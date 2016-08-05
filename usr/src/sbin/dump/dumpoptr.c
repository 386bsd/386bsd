/*-
 * Copyright (c) 1980, 1988 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char sccsid[] = "@(#)dumpoptr.c	5.8 (Berkeley) 3/7/91";
#endif /* not lint */

#include <sys/param.h>
#include <sys/wait.h>
#include <ufs/dir.h>
#include <signal.h>
#include <time.h>
#include <fstab.h>
#include <grp.h>
#include <varargs.h>
#include <utmp.h>
#include <tzfile.h>
#include <errno.h>
#include <stdio.h>
#ifdef __STDC__
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#endif
#include "dump.h"
#include "pathnames.h"

static void alarmcatch();
static void sendmes();

/*
 *	Query the operator; This previously-fascist piece of code
 *	no longer requires an exact response.
 *	It is intended to protect dump aborting by inquisitive
 *	people banging on the console terminal to see what is
 *	happening which might cause dump to croak, destroying
 *	a large number of hours of work.
 *
 *	Every 2 minutes we reprint the message, alerting others
 *	that dump needs attention.
 */
int	timeout;
char	*attnmessage;		/* attention message */

int
query(question)
	char	*question;
{
	char	replybuffer[64];
	int	back, errcount;
	FILE	*mytty;

	if ((mytty = fopen(_PATH_TTY, "r")) == NULL)
		quit("fopen on %s fails: %s\n", _PATH_TTY, strerror(errno));
	attnmessage = question;
	timeout = 0;
	alarmcatch();
	back = -1;
	errcount = 0;
	do {
		if (fgets(replybuffer, 63, mytty) == NULL) {
			clearerr(mytty);
			if (++errcount > 30)	/* XXX	ugly */
				quit("excessive operator query failures\n");
		} else if (replybuffer[0] == 'y' || replybuffer[0] == 'Y') {
			back = 1;
		} else if (replybuffer[0] == 'n' || replybuffer[0] == 'N') {
			back = 0;
		} else {
			(void) fprintf(stderr,
			    "  DUMP: \"Yes\" or \"No\"?\n");
			(void) fprintf(stderr,
			    "  DUMP: %s: (\"yes\" or \"no\") ", question);
		}
	} while (back < 0);

	/*
	 *	Turn off the alarm, and reset the signal to trap out..
	 */
	alarm(0);
	if (signal(SIGALRM, sigalrm) == SIG_IGN)
		signal(SIGALRM, SIG_IGN);
	fclose(mytty);
	return(back);
}

char lastmsg[100];

/*
 *	Alert the console operator, and enable the alarm clock to
 *	sleep for 2 minutes in case nobody comes to satisfy dump
 */
static void
alarmcatch()
{
	if (notify == 0) {
		if (timeout == 0)
			(void) fprintf(stderr,
			    "  DUMP: %s: (\"yes\" or \"no\") ",
			    attnmessage);
		else
			msgtail("\7\7");
	} else {
		if (timeout) {
			msgtail("\n");
			broadcast("");		/* just print last msg */
		}
		(void) fprintf(stderr,"  DUMP: %s: (\"yes\" or \"no\") ",
		    attnmessage);
	}
	signal(SIGALRM, alarmcatch);
	alarm(120);
	timeout = 1;
}

/*
 *	Here if an inquisitive operator interrupts the dump program
 */
void
interrupt()
{
	msg("Interrupt received.\n");
	if (query("Do you want to abort dump?"))
		dumpabort();
}

/*
 *	The following variables and routines manage alerting
 *	operators to the status of dump.
 *	This works much like wall(1) does.
 */
struct	group *gp;

/*
 *	Get the names from the group entry "operator" to notify.
 */	
void
set_operators()
{
	if (!notify)		/*not going to notify*/
		return;
	gp = getgrnam(OPGRENT);
	endgrent();
	if (gp == NULL) {
		msg("No group entry for %s.\n", OPGRENT);
		notify = 0;
		return;
	}
}

struct tm *localtime();
struct tm *localclock;

/*
 *	We fork a child to do the actual broadcasting, so
 *	that the process control groups are not messed up
 */
void
broadcast(message)
	char	*message;
{
	time_t		clock;
	FILE	*f_utmp;
	struct	utmp	utmp;
	int	nusers;
	char	**np;
	int	pid, s;

	if (!notify || gp == NULL)
		return;

	switch (pid = fork()) {
	case -1:
		return;
	case 0:
		break;
	default:
		while (wait(&s) != pid)
			continue;
		return;
	}

	clock = time(0);
	localclock = localtime(&clock);

	if ((f_utmp = fopen(_PATH_UTMP, "r")) == NULL) {
		msg("Cannot open %s: %s\n", _PATH_UTMP, strerror(errno));
		return;
	}

	nusers = 0;
	while (!feof(f_utmp)) {
		if (fread(&utmp, sizeof (struct utmp), 1, f_utmp) != 1)
			break;
		if (utmp.ut_name[0] == 0)
			continue;
		nusers++;
		for (np = gp->gr_mem; *np; np++) {
			if (strncmp(*np, utmp.ut_name, sizeof(utmp.ut_name)) != 0)
				continue;
			/*
			 *	Do not send messages to operators on dialups
			 */
			if (strncmp(utmp.ut_line, DIALUP, strlen(DIALUP)) == 0)
				continue;
#ifdef DEBUG
			msg("Message to %s at %s\n", *np, utmp.ut_line);
#endif
			sendmes(utmp.ut_line, message);
		}
	}
	(void) fclose(f_utmp);
	Exit(0);	/* the wait in this same routine will catch this */
	/* NOTREACHED */
}

static void
sendmes(tty, message)
	char *tty, *message;
{
	char t[50], buf[BUFSIZ];
	register char *cp;
	int lmsg = 1;
	FILE *f_tty;

	(void) strcpy(t, _PATH_DEV);
	(void) strcat(t, tty);

	if ((f_tty = fopen(t, "w")) != NULL) {
		setbuf(f_tty, buf);
		(void) fprintf(f_tty,
		    "\n\
\7\7\7Message from the dump program to all operators at %d:%02d ...\r\n\n\
DUMP: NEEDS ATTENTION: ",
		    localclock->tm_hour, localclock->tm_min);
		for (cp = lastmsg; ; cp++) {
			if (*cp == '\0') {
				if (lmsg) {
					cp = message;
					if (*cp == '\0')
						break;
					lmsg = 0;
				} else
					break;
			}
			if (*cp == '\n')
				(void) putc('\r', f_tty);
			(void) putc(*cp, f_tty);
		}
		(void) fclose(f_tty);
	}
}

/*
 *	print out an estimate of the amount of time left to do the dump
 */

time_t	tschedule = 0;

void
timeest()
{
	time_t	tnow, deltat;

	time (&tnow);
	if (tnow >= tschedule) {
		tschedule = tnow + 300;
		if (blockswritten < 500)
			return;	
		deltat = tstart_writing - tnow +
			(1.0 * (tnow - tstart_writing))
			/ blockswritten * tapesize;
		msg("%3.2f%% done, finished in %d:%02d\n",
			(blockswritten * 100.0) / tapesize,
			deltat / 3600, (deltat % 3600) / 60);
	}
}

/*
 *	tapesize: total number of blocks estimated over all reels
 *	blockswritten:	blocks actually written, over all reels
 *	etapes:	estimated number of tapes to write
 *
 *	tsize:	blocks can write on this reel
 *	asize:	blocks written on this reel
 *	tapeno:	number of tapes written so far
 */
int
blocksontape()
{
	if (tapeno == etapes)
		return (tapesize - (etapes - 1) * tsize);
	return (tsize);
}

#ifdef lint

/* VARARGS1 */
void msg(fmt) char *fmt; { strcpy(lastmsg, fmt); }

/* VARARGS1 */
void msgtail(fmt) char *fmt; { fmt = fmt; }

void quit(fmt) char *fmt; { msg(fmt); dumpabort(); }

#else /* lint */

void
msg(va_alist)
	va_dcl
{
	va_list ap;
	char *fmt;

	(void) fprintf(stderr,"  DUMP: ");
#ifdef TDEBUG
	(void) fprintf(stderr, "pid=%d ", getpid());
#endif
	va_start(ap);
	fmt = va_arg(ap, char *);
	(void) vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void) fflush(stdout);
	(void) fflush(stderr);
	va_start(ap);
	fmt = va_arg(ap, char *);
	(void) vsprintf(lastmsg, fmt, ap);
	va_end(ap);
}

void
msgtail(va_alist)
	va_dcl
{
	va_list ap;
	char *fmt;

	va_start(ap);
	fmt = va_arg(ap, char *);
	(void) vfprintf(stderr, fmt, ap);
	va_end(ap);
}

void
quit(va_alist)
	va_dcl
{
	va_list ap;
	char *fmt;

	(void) fprintf(stderr,"  DUMP: ");
#ifdef TDEBUG
	(void) fprintf(stderr, "pid=%d ", getpid());
#endif
	va_start(ap);
	fmt = va_arg(ap, char *);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void) fflush(stdout);
	(void) fflush(stderr);
	dumpabort();
}

#endif /* lint */

/*
 *	Tell the operator what has to be done;
 *	we don't actually do it
 */

struct fstab *
allocfsent(fs)
	register struct fstab *fs;
{
	register struct fstab *new;

	new = (struct fstab *)malloc(sizeof (*fs));
	if (new == NULL ||
	    (new->fs_file = strdup(fs->fs_file)) == NULL ||
	    (new->fs_type = strdup(fs->fs_type)) == NULL ||
	    (new->fs_spec = strdup(fs->fs_spec)) == NULL)
		quit("%s\n", strerror(errno));
	new->fs_passno = fs->fs_passno;
	new->fs_freq = fs->fs_freq;
	return (new);
}

struct	pfstab {
	struct	pfstab *pf_next;
	struct	fstab *pf_fstab;
};

static	struct pfstab *table;

void
getfstab()
{
	register struct fstab *fs;
	register struct pfstab *pf;

	if (setfsent() == 0) {
		msg("Can't open %s for dump table information: %s\n",
		    _PATH_FSTAB, strerror(errno));
		return;
	}
	while (fs = getfsent()) {
		if (strcmp(fs->fs_type, FSTAB_RW) &&
		    strcmp(fs->fs_type, FSTAB_RO) &&
		    strcmp(fs->fs_type, FSTAB_RQ))
			continue;
		fs = allocfsent(fs);
		if ((pf = (struct pfstab *)malloc(sizeof (*pf))) == NULL)
			quit("%s\n", strerror(errno));
		pf->pf_fstab = fs;
		pf->pf_next = table;
		table = pf;
	}
	endfsent();
}

/*
 * Search in the fstab for a file name.
 * This file name can be either the special or the path file name.
 *
 * The entries in the fstab are the BLOCK special names, not the
 * character special names.
 * The caller of fstabsearch assures that the character device
 * is dumped (that is much faster)
 *
 * The file name can omit the leading '/'.
 */
struct fstab *
fstabsearch(key)
	char *key;
{
	register struct pfstab *pf;
	register struct fstab *fs;
	char *rawname();

	for (pf = table; pf != NULL; pf = pf->pf_next) {
		fs = pf->pf_fstab;
		if (strcmp(fs->fs_file, key) == 0 ||
		    strcmp(fs->fs_spec, key) == 0 ||
		    strcmp(rawname(fs->fs_spec), key) == 0)
			return (fs);
		if (key[0] != '/') {
			if (*fs->fs_spec == '/' &&
			    strcmp(fs->fs_spec + 1, key) == 0)
				return (fs);
			if (*fs->fs_file == '/' &&
			    strcmp(fs->fs_file + 1, key) == 0)
				return (fs);
		}
	}
	return (NULL);
}

/*
 *	Tell the operator what to do
 */
void
lastdump(arg)
	char	arg;	/* w ==> just what to do; W ==> most recent dumps */
{
	register int i;
	register struct fstab *dt;
	register struct dumpdates *dtwalk;
	char *lastname, *date;
	int dumpme, datesort();
	time_t tnow;

	time(&tnow);
	getfstab();		/* /etc/fstab input */
	initdumptimes();	/* /etc/dumpdates input */
	qsort(ddatev, nddates, sizeof(struct dumpdates *), datesort);

	if (arg == 'w')
		(void) printf("Dump these file systems:\n");
	else
		(void) printf("Last dump(s) done (Dump '>' file systems):\n");
	lastname = "??";
	ITITERATE(i, dtwalk) {
		if (strncmp(lastname, dtwalk->dd_name,
		    sizeof(dtwalk->dd_name)) == 0)
			continue;
		date = (char *)ctime(&dtwalk->dd_ddate);
		date[16] = '\0';	/* blast away seconds and year */
		lastname = dtwalk->dd_name;
		dt = fstabsearch(dtwalk->dd_name);
		dumpme = (dt != NULL &&
		    dt->fs_freq != 0 &&
		    dtwalk->dd_ddate < tnow - (dt->fs_freq * SECSPERDAY));
		if (arg != 'w' || dumpme)
			(void) printf(
			    "%c %8s\t(%6s) Last dump: Level %c, Date %s\n",
			    dumpme && (arg != 'w') ? '>' : ' ',
			    dtwalk->dd_name,
			    dt ? dt->fs_file : "",
			    dtwalk->dd_level,
			    date);
	}
}

int
datesort(a1, a2)
	void *a1, *a2;
{
	struct dumpdates *d1 = *(struct dumpdates **)a1;
	struct dumpdates *d2 = *(struct dumpdates **)a2;
	int diff;

	diff = strncmp(d1->dd_name, d2->dd_name, sizeof(d1->dd_name));
	if (diff == 0)
		return (d2->dd_ddate - d1->dd_ddate);
	return (diff);
}

int max(a, b)
	int a, b;
{
	return (a > b ? a : b);
}
int min(a, b)
	int a, b;
{
	return (a < b ? a : b);
}
