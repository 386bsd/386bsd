/*
 * at.c : Put file into atrun queue
 * Copyright (C) 1993  Thomas Koenig
 *
 * Atrun & Atq modifications
 * Copyright (C) 1993  David Parsons
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author(s) may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define _USE_BSD 1

/* System Headers */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Local headers */
#include "at.h"
#include "panic.h"
#include "parsetime.h"
#include "pathnames.h"
#define MAIN
#include "privs.h"

/* Macros */
#define ALARMC 10		/* Number of seconds to wait for timeout */

#define SIZE 255
#define TIMESIZE 50

/* File scope variables */
static char rcsid[] = "$Id: at.c,v 1.1 94/07/29 19:58:02 root Exp $";
char *no_export[] =
{
	"TERM", "TERMCAP", "DISPLAY", "_"
};
static send_mail = 0;

/* External variables */
extern char **environ;
int fcreated;
char *namep;
char atfile[FILENAME_MAX];

char *atinput = (char *) 0;	/* where to get input from */
char atqueue = 0;		/* which queue to examine for jobs (atq) */
char atverify = 0;		/* verify time instead of queuing job */

/* Function declarations */
static void sigc	__P((int signo));
static void alarmc	__P((int signo));
static char *cwdname	__P((void));
static void writefile	__P((time_t runtimer, char queue));
static void list_jobs	__P((void));

/* Signal catching functions */

static void 
sigc(signo)
	int signo;
{
/* If the user presses ^C, remove the spool file and exit
 */
	if (fcreated) {
		PRIV_START
		unlink(atfile);
		PRIV_END
	}

	exit(EXIT_FAILURE);
}

static void 
alarmc(signo)
	int signo;
{
/* Time out after some seconds
 */
	panic("File locking timed out");
}

/* Local functions */

static char *
cwdname()
{
/* Read in the current directory; the name will be overwritten on
 * subsequent calls.
 */
	static char *ptr = NULL;
	static size_t size = SIZE;

	if (ptr == NULL)
		ptr = (char *) malloc(size);

	while (1) {
		if (ptr == NULL)
			panic("Out of memory");

		if (getcwd(ptr, size - 1) != NULL)
			return ptr;

		if (errno != ERANGE)
			perr("Cannot get directory");

		free(ptr);
		size += SIZE;
		ptr = (char *) malloc(size);
	}
}

static void
writefile(runtimer, queue)
	time_t runtimer;
	char queue;
{
	/*
	 * This does most of the work if at or batch are invoked for
	 * writing a job.
	 */
	int i;
	char *ap, *ppos, *mailname;
	struct passwd *pass_entry;
	struct stat statbuf;
	int fdes, lockdes, fd2;
	FILE *fp, *fpin;
	struct sigaction act;
	char **atenv;
	int ch;
	mode_t cmask;
	struct flock lock;

	/*
	 * Install the signal handler for SIGINT; terminate after removing the
	 * spool file if necessary
	 */
	act.sa_handler = sigc;
	sigemptyset(&(act.sa_mask));
	act.sa_flags = 0;

	sigaction(SIGINT, &act, NULL);

	strcpy(atfile, _PATH_ATJOBS);
	ppos = atfile + strlen(_PATH_ATJOBS);

	/*
	 * Loop over all possible file names for running something at this
	 *  particular time, see if a file is there; the first empty slot at
	 *  any particular time is used.  Lock the file _PATH_LOCKFILE first
	 *  to make sure we're alone when doing this.
	 */

	PRIV_START

	if ((lockdes = open(_PATH_LOCKFILE, O_WRONLY | O_CREAT, 0600)) < 0)
		perr2("Cannot open lockfile ", _PATH_LOCKFILE);

	lock.l_type = F_WRLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 0;

	act.sa_handler = alarmc;
	sigemptyset(&(act.sa_mask));
	act.sa_flags = 0;

	/*
	 * Set an alarm so a timeout occurs after ALARMC seconds, in case
	 * something is seriously broken.
	 */
	sigaction(SIGALRM, &act, NULL);
	alarm(ALARMC);
	fcntl(lockdes, F_SETLKW, &lock);
	alarm(0);

	for (i = 0; i < AT_MAXJOBS; i++) {
		sprintf(ppos, "%c%8lx.%3x", queue,
		    (unsigned long) (runtimer / 60), i);
		for (ap = ppos; *ap != '\0'; ap++)
			if (*ap == ' ')
				*ap = '0';

		if (stat(atfile, &statbuf) != 0) {
			if (errno == ENOENT)
				break;
			else
				perr2("Cannot access ", _PATH_ATJOBS);
		}
	}			/* for */

	if (i >= AT_MAXJOBS)
		panic("Too many jobs already");

	/*
	 * Create the file. The x bit is only going to be set after it has
	 * been completely written out, to make sure it is not executed in
	 * the meantime.  To make sure they do not get deleted, turn off
	 * their r bit.  Yes, this is a kluge.
	 */
	cmask = umask(S_IRUSR | S_IWUSR | S_IXUSR);
	if ((fdes = creat(atfile, O_WRONLY)) == -1)
		perr("Cannot create atjob file");

	if ((fd2 = dup(fdes)) < 0)
		perr("Error in dup() of job file");

	if (fchown(fd2, real_uid, -1) != 0)
		perr("Cannot give away file");

	PRIV_END

	/*
	 * We no longer need suid root; now we just need to be able to
	 * write to the directory, if necessary.
	 */

	    REDUCE_PRIV(0);

	/*
	 * We've successfully created the file; let's set the flag so it
	 * gets removed in case of an interrupt or error.
	 */
	fcreated = 1;

	/* Now we can release the lock, so other people can access it */
	lock.l_type = F_UNLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 0;
	fcntl(lockdes, F_SETLKW, &lock);
	close(lockdes);

	if ((fp = fdopen(fdes, "w")) == NULL)
		panic("Cannot reopen atjob file");

	/*
	 * Get the userid to mail to, first by trying getlogin(), which
	 * reads /etc/utmp, then from LOGNAME, finally from getpwuid().
	 */
	mailname = getlogin();
	if (mailname == NULL)
		mailname = getenv("LOGNAME");

	if ((mailname == NULL) || (mailname[0] == '\0')
	    || (strlen(mailname) > 8)) {
		pass_entry = getpwuid(getuid());
		if (pass_entry != NULL)
			mailname = pass_entry->pw_name;
	}

	if (atinput != (char *) NULL) {
		fpin = freopen(atinput, "r", stdin);
		if (fpin == NULL)
			perr("Cannot open input file");
	}
	fprintf(fp, "#! /bin/sh\n# mail %8s %d\n", mailname, send_mail);

	/* Write out the umask at the time of invocation */
	fprintf(fp, "umask %lo\n", (unsigned long) cmask);

	/*
	 * Write out the environment. Anything that may look like a special
	 * character to the shell is quoted, except for \n, which is done
	 * with a pair of "'s.  Dont't export the no_export list (such as
	 * TERM or DISPLAY) because we don't want these.
	 */
	for (atenv = environ; *atenv != NULL; atenv++) {
		int export = 1;
		char *eqp;

		eqp = strchr(*atenv, '=');
		if (ap == NULL)
			eqp = *atenv;
		else {
			int i;

			for (i = 0;i < sizeof(no_export) /
			    sizeof(no_export[0]); i++) {
				export = export
				    && (strncmp(*atenv, no_export[i],
					(size_t) (eqp - *atenv)) != 0);
			}
			eqp++;
		}

		if (export) {
			fwrite(*atenv, sizeof(char), eqp - *atenv, fp);
			for (ap = eqp; *ap != '\0'; ap++) {
				if (*ap == '\n')
					fprintf(fp, "\"\n\"");
				else {
					if (!isalnum(*ap))
						fputc('\\', fp);

					fputc(*ap, fp);
				}
			}
			fputs("; export ", fp);
			fwrite(*atenv, sizeof(char), eqp - *atenv - 1, fp);
			fputc('\n', fp);

		}
	}
	/*
	 * Cd to the directory at the time and write out all the commands
	 * the user supplies from stdin.
	 */
	fprintf(fp, "cd %s\n", cwdname());

	while ((ch = getchar()) != EOF)
		fputc(ch, fp);

	fprintf(fp, "\n");
	if (ferror(fp))
		panic("Output error");

	if (ferror(stdin))
		panic("Input error");

	fclose(fp);

	/*
	 * Set the x bit so that we're ready to start executing
	 */
	if (fchmod(fd2, S_IRUSR | S_IWUSR | S_IXUSR) < 0)
		perr("Cannot give away file");

	close(fd2);
	fprintf(stderr, "Job %s will be executed using /bin/sh\n", ppos);
}

static void
list_jobs()
{
	/*
	 * List all a user's jobs in the queue, by looping through
	 * _PATH_ATJOBS, or everybody's if we are root
	 */
	struct passwd *pw;
	DIR *spool;
	struct dirent *dirent;
	struct stat buf;
	struct tm runtime;
	unsigned long ctm;
	char queue;
	time_t runtimer;
	char timestr[TIMESIZE];
	int first = 1;

	PRIV_START

	    if (chdir(_PATH_ATJOBS) != 0)
		perr2("Cannot change to ", _PATH_ATJOBS);

	if ((spool = opendir(".")) == NULL)
		perr2("Cannot open ", _PATH_ATJOBS);

	/* Loop over every file in the directory */
	while ((dirent = readdir(spool)) != NULL) {
		if (stat(dirent->d_name, &buf) != 0)
			perr2("Cannot stat in ", _PATH_ATJOBS);

		/*
		 * See it's a regular file and has its x bit turned on and
		 * is the user's
		 */
		if (!S_ISREG(buf.st_mode)
		    || ((buf.st_uid != real_uid) && !(real_uid == 0))
		    || !(S_IXUSR & buf.st_mode || atverify))
			continue;

		if (sscanf(dirent->d_name, "%c%8lx", &queue, &ctm) != 2)
			continue;

		if (atqueue && (queue != atqueue))
			continue;

		runtimer = 60 * (time_t) ctm;
		runtime = *localtime(&runtimer);
		strftime(timestr, TIMESIZE, "%X %x", &runtime);
		if (first) {
			printf("Date\t\t\tOwner\tQueue\tJob#\n");
			first = 0;
		}
		pw = getpwuid(buf.st_uid);

		printf("%s\t%s\t%c%s\t%s\n",
		    timestr,
		    pw ? pw->pw_name : "???",
		    queue,
		    (S_IXUSR & buf.st_mode) ? "" : "(done)",
		    dirent->d_name);
	}
	PRIV_END
}

static void
delete_jobs(argc, argv)
	int argc;
	char **argv;
{
	/* Delete every argument (job - ID) given */
	int i;
	struct stat buf;

	PRIV_START

	    if (chdir(_PATH_ATJOBS) != 0)
		perr2("Cannot change to ", _PATH_ATJOBS);

	for (i = optind; i < argc; i++) {
		if (stat(argv[i], &buf) != 0)
			perr(argv[i]);
		if ((buf.st_uid != real_uid) && !(real_uid == 0)) {
			fprintf(stderr, "%s: Not owner\n", argv[i]);
			exit(EXIT_FAILURE);
		}
		if (unlink(argv[i]) != 0)
			perr(argv[i]);
	}
	PRIV_END
}				/* delete_jobs */

/* Global functions */

int
main(argc, argv)
	int argc;
	char **argv;
{
	int c;
	char queue = 'a';
	char *pgm;

	enum {
		ATQ, ATRM, AT, BATCH
	};			/* what program we want to run */
	int program = AT;	/* our default program */
	char *options = "q:f:mv";	/* default options for at */
	time_t timer;

	RELINQUISH_PRIVS

	/* Eat any leading paths */
	if ((pgm = strrchr(argv[0], '/')) == NULL)
		pgm = argv[0];
	else
		pgm++;

	namep = pgm;

	/* find out what this program is supposed to do */
	if (strcmp(pgm, "atq") == 0) {
		program = ATQ;
		options = "q:v";
	} else if (strcmp(pgm, "atrm") == 0) {
		program = ATRM;
		options = "";
	} else if (strcmp(pgm, "batch") == 0) {
		program = BATCH;
		options = "f:mv";
	}

	/* process whatever options we can process */
	opterr = 1;
	while ((c = getopt(argc, argv, options)) != EOF)
		switch (c) {
		case 'v':	/* verify time settings */
			atverify = 1;
			break;

		case 'm':	/* send mail when job is complete */
			send_mail = 1;
			break;

		case 'f':
			atinput = optarg;
			break;

		case 'q':	/* specify queue */
			if (strlen(optarg) > 1)
				usage();

			atqueue = queue = *optarg;
			if ((!islower(queue)) || (queue > 'l'))
				usage();
			break;

		default:
			usage();
			break;
		}
	/* end of options eating */

	/* select our program */
	switch (program) {
	case ATQ:

		REDUCE_PRIV(0);

		list_jobs();
		break;

	case ATRM:

		REDUCE_PRIV(0);

		delete_jobs(argc, argv);
		break;

	case AT:
		timer = parsetime(argc, argv);
		if (atverify) {
			struct tm *tm = localtime(&timer);

			fprintf(stderr, "%s\n", asctime(tm));
		}
		writefile(timer, queue);
		break;

	case BATCH:
		writefile(time(NULL), 'b');
		break;

	default:
		panic("Internal error");
		break;
	}
	exit(EXIT_SUCCESS);
}
