/* Copyright 1988,1990,1993,1994 by Paul Vixie
 * All rights reserved
 *
 * Distribute freely, except: don't remove my name from the source or
 * documentation (don't take credit for my work), mark your changes (don't
 * get me blamed for your possible bugs), don't alter or remove this
 * notice.  May be sold if buildable source is provided to buyer.  No
 * warrantee of any kind, express or implied, is included with this
 * software; use at your own risk, responsibility for damages (if any) to
 * anyone resulting from the use of this software rests entirely with the
 * user.
 *
 * Send bug reports, bug fixes, enhancements, requests, flames, etc., and
 * I'll try to keep a version up to date.  I can be reached as follows:
 * Paul Vixie          <paul@vix.com>          uunet!decwrl!vixie!paul
 */

#if !defined(lint) && !defined(LINT)
static char rcsid[] = "$Id: do_command.c,v 2.12 1994/01/15 20:43:43 vixie Exp $";
#endif


#include "cron.h"
#include <sys/signal.h>
#if defined(sequent)
# include <sys/universe.h>
#endif
#if defined(SYSLOG)
# include <syslog.h>
#endif


static void		child_process __P((entry *, user *)),
			do_univ __P((user *));


void
do_command(e, u)
	entry	*e;
	user	*u;
{
	Debug(DPROC, ("[%d] do_command(%s, (%s,%d,%d))\n",
		getpid(), e->cmd, u->name, e->uid, e->gid))

	/* fork to become asynchronous -- parent process is done immediately,
	 * and continues to run the normal cron code, which means return to
	 * tick().  the child and grandchild don't leave this function, alive.
	 *
	 * vfork() is unsuitable, since we have much to do, and the parent
	 * needs to be able to run off and fork other processes.
	 */
	switch (fork()) {
	case -1:
		log_it("CRON",getpid(),"error","can't fork");
		break;
	case 0:
		/* child process */
		acquire_daemonlock(1);
		child_process(e, u);
		Debug(DPROC, ("[%d] child process done, exiting\n", getpid()))
		_exit(OK_EXIT);
		break;
	default:
		/* parent process */
		break;
	}
	Debug(DPROC, ("[%d] main process returning to work\n", getpid()))
}


static void
child_process(e, u)
	entry	*e;
	user	*u;
{
	int		stdin_pipe[2], stdout_pipe[2];
	register char	*input_data;
	char		*usernm, *mailto;
	int		children = 0;

	Debug(DPROC, ("[%d] child_process('%s')\n", getpid(), e->cmd))

	/* mark ourselves as different to PS command watchers by upshifting
	 * our program name.  This has no effect on some kernels.
	 */
	/*local*/{
		register char	*pch;

		for (pch = ProgramName;  *pch;  pch++)
			*pch = MkUpper(*pch);
	}

	/* discover some useful and important environment settings
	 */
	usernm = env_get("LOGNAME", e->envp);
	mailto = env_get("MAILTO", e->envp);

#ifdef USE_SIGCHLD
	/* our parent is watching for our death by catching SIGCHLD.  we
	 * do not care to watch for our children's deaths this way -- we
	 * use wait() explictly.  so we have to disable the signal (which
	 * was inherited from the parent).
	 */
	(void) signal(SIGCHLD, SIG_IGN);
#else
	/* on system-V systems, we are ignoring SIGCLD.  we have to stop
	 * ignoring it now or the wait() in cron_pclose() won't work.
	 * because of this, we have to wait() for our children here, as well.
	 */
	(void) signal(SIGCLD, SIG_DFL);
#endif /*BSD*/

	/* create some pipes to talk to our future child
	 */
	pipe(stdin_pipe);	/* child's stdin */
	pipe(stdout_pipe);	/* child's stdout */
	
	/* since we are a forked process, we can diddle the command string
	 * we were passed -- nobody else is going to use it again, right?
	 *
	 * if a % is present in the command, previous characters are the
	 * command, and subsequent characters are the additional input to
	 * the command.  Subsequent %'s will be transformed into newlines,
	 * but that happens later.
	 */
	/*local*/{
		register int escaped = FALSE;
		register int ch;

		for (input_data = e->cmd;  ch = *input_data;  input_data++) {
			if (escaped) {
				escaped = FALSE;
				continue;
			}
			if (ch == '\\') {
				escaped = TRUE;
				continue;
			}
			if (ch == '%') {
				*input_data++ = '\0';
				break;
			}
		}
	}

	/* fork again, this time so we can exec the user's command.
	 */
	switch (vfork()) {
	case -1:
		log_it("CRON",getpid(),"error","can't vfork");
		exit(ERROR_EXIT);
		/*NOTREACHED*/
	case 0:
		Debug(DPROC, ("[%d] grandchild process Vfork()'ed\n",
			      getpid()))

		/* write a log message.  we've waited this long to do it
		 * because it was not until now that we knew the PID that
		 * the actual user command shell was going to get and the
		 * PID is part of the log message.
		 */
		/*local*/{
			char *x = mkprints((u_char *)e->cmd, strlen(e->cmd));

			log_it(usernm, getpid(), "CMD", x);
			free(x);
		}

		/* that's the last thing we'll log.  close the log files.
		 */
#ifdef SYSLOG
		closelog();
#endif

		/* get new pgrp, void tty, etc.
		 */
		(void) setsid();

		/* close the pipe ends that we won't use.  this doesn't affect
		 * the parent, who has to read and write them; it keeps the
		 * kernel from recording us as a potential client TWICE --
		 * which would keep it from sending SIGPIPE in otherwise
		 * appropriate circumstances.
		 */
		close(stdin_pipe[WRITE_PIPE]);
		close(stdout_pipe[READ_PIPE]);

		/* grandchild process.  make std{in,out} be the ends of
		 * pipes opened by our daddy; make stderr go to stdout.
		 */
		close(STDIN);	dup2(stdin_pipe[READ_PIPE], STDIN);
		close(STDOUT);	dup2(stdout_pipe[WRITE_PIPE], STDOUT);
		close(STDERR);	dup2(STDOUT, STDERR);

		/* close the pipes we just dup'ed.  The resources will remain.
		 */
		close(stdin_pipe[READ_PIPE]);
		close(stdout_pipe[WRITE_PIPE]);

		/* set our login universe.  Do this in the grandchild
		 * so that the child can invoke /usr/lib/sendmail
		 * without surprises.
		 */
		do_univ(u);

		/* set our directory, uid and gid.  Set gid first, since once
		 * we set uid, we've lost root privledges.
		 */
		setgid(e->gid);
# if defined(BSD)
		initgroups(env_get("LOGNAME", e->envp), e->gid);
# endif
		setuid(e->uid);		/* we aren't root after this... */
		chdir(env_get("HOME", e->envp));

		/* exec the command.
		 */
		{
			char	*shell = env_get("SHELL", e->envp);

# if DEBUGGING
			if (DebugFlags & DTEST) {
				fprintf(stderr,
				"debug DTEST is on, not exec'ing command.\n");
				fprintf(stderr,
				"\tcmd='%s' shell='%s'\n", e->cmd, shell);
				_exit(OK_EXIT);
			}
# endif /*DEBUGGING*/
			execle(shell, shell, "-c", e->cmd, (char *)0, e->envp);
			fprintf(stderr, "execl: couldn't exec `%s'\n", shell);
			perror("execl");
			_exit(ERROR_EXIT);
		}
		break;
	default:
		/* parent process */
		break;
	}

	children++;

	/* middle process, child of original cron, parent of process running
	 * the user's command.
	 */

	Debug(DPROC, ("[%d] child continues, closing pipes\n", getpid()))

	/* close the ends of the pipe that will only be referenced in the
	 * grandchild process...
	 */
	close(stdin_pipe[READ_PIPE]);
	close(stdout_pipe[WRITE_PIPE]);

	/*
	 * write, to the pipe connected to child's stdin, any input specified
	 * after a % in the crontab entry.  while we copy, convert any
	 * additional %'s to newlines.  when done, if some characters were
	 * written and the last one wasn't a newline, write a newline.
	 *
	 * Note that if the input data won't fit into one pipe buffer (2K
	 * or 4K on most BSD systems), and the child doesn't read its stdin,
	 * we would block here.  thus we must fork again.
	 */

	if (*input_data && fork() == 0) {
		register FILE	*out = fdopen(stdin_pipe[WRITE_PIPE], "w");
		register int	need_newline = FALSE;
		register int	escaped = FALSE;
		register int	ch;

		Debug(DPROC, ("[%d] child2 sending data to grandchild\n", getpid()))

		/* close the pipe we don't use, since we inherited it and
		 * are part of its reference count now.
		 */
		close(stdout_pipe[READ_PIPE]);

		/* translation:
		 *	\% -> %
		 *	%  -> \n
		 *	\x -> \x	for all x != %
		 */
		while (ch = *input_data++) {
			if (escaped) {
				if (ch != '%')
					putc('\\', out);
			} else {
				if (ch == '%')
					ch = '\n';
			}

			if (!(escaped = (ch == '\\'))) {
				putc(ch, out);
				need_newline = (ch != '\n');
			}
		}
		if (escaped)
			putc('\\', out);
		if (need_newline)
			putc('\n', out);

		/* close the pipe, causing an EOF condition.  fclose causes
		 * stdin_pipe[WRITE_PIPE] to be closed, too.
		 */
		fclose(out);

		Debug(DPROC, ("[%d] child2 done sending to grandchild\n", getpid()))
		exit(0);
	}

	/* close the pipe to the grandkiddie's stdin, since its wicked uncle
	 * ernie back there has it open and will close it when he's done.
	 */
	close(stdin_pipe[WRITE_PIPE]);

	children++;

	/*
	 * read output from the grandchild.  it's stderr has been redirected to
	 * it's stdout, which has been redirected to our pipe.  if there is any
	 * output, we'll be mailing it to the user whose crontab this is...
	 * when the grandchild exits, we'll get EOF.
	 */

	Debug(DPROC, ("[%d] child reading output from grandchild\n", getpid()))

	/*local*/{
		register FILE	*in = fdopen(stdout_pipe[READ_PIPE], "r");
		register int	ch = getc(in);

		if (ch != EOF) {
			register FILE	*mail;
			register int	bytes = 1;
			int		status = 0;

			Debug(DPROC|DEXT,
				("[%d] got data (%x:%c) from grandchild\n",
					getpid(), ch, ch))

			/* get name of recipient.  this is MAILTO if set to a
			 * valid local username; USER otherwise.
			 */
			if (mailto) {
				/* MAILTO was present in the environment
				 */
				if (!*mailto) {
					/* ... but it's empty. set to NULL
					 */
					mailto = NULL;
				}
			} else {
				/* MAILTO not present, set to USER.
				 */
				mailto = usernm;
			}
		
			/* if we are supposed to be mailing, MAILTO will
			 * be non-NULL.  only in this case should we set
			 * up the mail command and subjects and stuff...
			 */

			if (mailto) {
				register char	**env;
				auto char	mailcmd[MAX_COMMAND];
				auto char	hostname[MAXHOSTNAMELEN];

				(void) gethostname(hostname, MAXHOSTNAMELEN);
				(void) sprintf(mailcmd, MAILARGS,
					       MAILCMD, mailto);
				if (!(mail = cron_popen(mailcmd, "w"))) {
					perror(MAILCMD);
					(void) _exit(ERROR_EXIT);
				}
				fprintf(mail, "From: root (Cron Daemon)\n");
				fprintf(mail, "To: %s\n", mailto);
				fprintf(mail, "Subject: Cron <%s@%s> %s\n",
					usernm, first_word(hostname, "."),
					e->cmd);
# if defined(MAIL_DATE)
				fprintf(mail, "Date: %s\n",
					arpadate(&TargetTime));
# endif /* MAIL_DATE */
				for (env = e->envp;  *env;  env++)
					fprintf(mail, "X-Cron-Env: <%s>\n",
						*env);
				fprintf(mail, "\n");

				/* this was the first char from the pipe
				 */
				putc(ch, mail);
			}

			/* we have to read the input pipe no matter whether
			 * we mail or not, but obviously we only write to
			 * mail pipe if we ARE mailing.
			 */

			while (EOF != (ch = getc(in))) {
				bytes++;
				if (mailto)
					putc(ch, mail);
			}

			/* only close pipe if we opened it -- i.e., we're
			 * mailing...
			 */

			if (mailto) {
				Debug(DPROC, ("[%d] closing pipe to mail\n",
					getpid()))
				/* Note: the pclose will probably see
				 * the termination of the grandchild
				 * in addition to the mail process, since
				 * it (the grandchild) is likely to exit
				 * after closing its stdout.
				 */
				status = cron_pclose(mail);
			}

			/* if there was output and we could not mail it,
			 * log the facts so the poor user can figure out
			 * what's going on.
			 */
			if (mailto && status) {
				char buf[MAX_TEMPSTR];

				sprintf(buf,
			"mailed %d byte%s of output but got status 0x%04x\n",
					bytes, (bytes==1)?"":"s",
					status);
				log_it(usernm, getpid(), "MAIL", buf);
			}

		} /*if data from grandchild*/

		Debug(DPROC, ("[%d] got EOF from grandchild\n", getpid()))

		fclose(in);	/* also closes stdout_pipe[READ_PIPE] */
	}

	/* wait for children to die.
	 */
	for (;  children > 0;  children--)
	{
		WAIT_T		waiter;
		PID_T		pid;

		Debug(DPROC, ("[%d] waiting for grandchild #%d to finish\n",
			getpid(), children))
		pid = wait(&waiter);
		if (pid < OK) {
			Debug(DPROC, ("[%d] no more grandchildren--mail written?\n",
				getpid()))
			break;
		}
		Debug(DPROC, ("[%d] grandchild #%d finished, status=%04x",
			getpid(), pid, WEXITSTATUS(waiter)))
		if (WIFSIGNALED(waiter) && WCOREDUMP(waiter))
			Debug(DPROC, (", dumped core"))
		Debug(DPROC, ("\n"))
	}
}


static void
do_univ(u)
	user	*u;
{
#if defined(sequent)
/* Dynix (Sequent) hack to put the user associated with
 * the passed user structure into the ATT universe if
 * necessary.  We have to dig the gecos info out of
 * the user's password entry to see if the magic
 * "universe(att)" string is present.
 */

	struct	passwd	*p;
	char	*s;
	int	i;

	p = getpwuid(u->uid);
	(void) endpwent();

	if (p == NULL)
		return;

	s = p->pw_gecos;

	for (i = 0; i < 4; i++)
	{
		if ((s = strchr(s, ',')) == NULL)
			return;
		s++;
	}
	if (strcmp(s, "universe(att)"))
		return;

	(void) universe(U_ATT);
#endif
}
