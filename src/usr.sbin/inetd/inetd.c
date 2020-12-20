/*
 * Copyright (c) 1983,1991 The Regents of the University of California.
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
char copyright[] =
"@(#) Copyright (c) 1983 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)inetd.c	5.30 (Berkeley) 6/3/91";
#endif /* not lint */

/*
 * Inetd - Internet super-server
 *
 * This program invokes all internet services as needed.
 * connection-oriented services are invoked each time a
 * connection is made, by creating a process.  This process
 * is passed the connection as file descriptor 0 and is
 * expected to do a getpeername to find out the source host
 * and port.
 *
 * Datagram oriented services are invoked when a datagram
 * arrives; a process is created and passed a pending message
 * on file descriptor 0.  Datagram servers may either connect
 * to their peer, freeing up the original socket for inetd
 * to receive further messages on, or ``take over the socket'',
 * processing all arriving datagrams and, eventually, timing
 * out.	 The first type of server is said to be ``multi-threaded'';
 * the second type of server ``single-threaded''. 
 *
 * Inetd uses a configuration file which is read at startup
 * and, possibly, at some later time in response to a hangup signal.
 * The configuration file is ``free format'' with fields given in the
 * order shown below.  Continuation lines for an entry must being with
 * a space or tab.  All fields must be present in each entry.
 *
 *	service name			must be in /etc/services
 *	socket type			stream/dgram/raw/rdm/seqpacket
 *	protocol			must be in /etc/protocols
 *	wait/nowait			single-threaded/multi-threaded
 *	user				user to run daemon as
 *	server program			full path name
 *	server program arguments	maximum of MAXARGS (20)
 *
 * Comment lines are indicated by a `#' in column 1.
 */
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <signal.h>
#include <netdb.h>
#include <syslog.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include "pathnames.h"

#define	TOOMANY		40		/* don't start more than TOOMANY */
#define	CNT_INTVL	60		/* servers in CNT_INTVL sec. */
#define	RETRYTIME	(60*10)		/* retry after bind or server fail */

#define	SIGBLOCK	(sigmask(SIGCHLD)|sigmask(SIGHUP)|sigmask(SIGALRM))

extern	int errno;

void	config(), reapchild(), retry();
char	*index();
char	*malloc();

int	debug = 0;
int	nsock, maxsock;
fd_set	allsock;
int	options;
int	timingout;
struct	servent *sp;

struct	servtab {
	char	*se_service;		/* name of service */
	int	se_socktype;		/* type of socket to use */
	char	*se_proto;		/* protocol used */
	short	se_wait;		/* single threaded server */
	short	se_checked;		/* looked at during merge */
	char	*se_user;		/* user name to run as */
	struct	biltin *se_bi;		/* if built-in, description */
	char	*se_server;		/* server program */
#define	MAXARGV 20
	char	*se_argv[MAXARGV+1];	/* program arguments */
	int	se_fd;			/* open descriptor */
	struct	sockaddr_in se_ctrladdr;/* bound address */
	int	se_count;		/* number started since se_time */
	struct	timeval se_time;	/* start of se_count */
	struct	servtab *se_next;
} *servtab;

int echo_stream(), discard_stream(), machtime_stream();
int daytime_stream(), chargen_stream();
int echo_dg(), discard_dg(), machtime_dg(), daytime_dg(), chargen_dg();

struct biltin {
	char	*bi_service;		/* internally provided service name */
	int	bi_socktype;		/* type of socket supported */
	short	bi_fork;		/* 1 if should fork before call */
	short	bi_wait;		/* 1 if should wait for child */
	int	(*bi_fn)();		/* function which performs it */
} biltins[] = {
	/* Echo received data */
	"echo",		SOCK_STREAM,	1, 0,	echo_stream,
	"echo",		SOCK_DGRAM,	0, 0,	echo_dg,

	/* Internet /dev/null */
	"discard",	SOCK_STREAM,	1, 0,	discard_stream,
	"discard",	SOCK_DGRAM,	0, 0,	discard_dg,

	/* Return 32 bit time since 1970 */
	"time",		SOCK_STREAM,	0, 0,	machtime_stream,
	"time",		SOCK_DGRAM,	0, 0,	machtime_dg,

	/* Return human-readable time */
	"daytime",	SOCK_STREAM,	0, 0,	daytime_stream,
	"daytime",	SOCK_DGRAM,	0, 0,	daytime_dg,

	/* Familiar character generator */
	"chargen",	SOCK_STREAM,	1, 0,	chargen_stream,
	"chargen",	SOCK_DGRAM,	0, 0,	chargen_dg,
	0
};

#define NUMINT	(sizeof(intab) / sizeof(struct inent))
char	*CONFIG = _PATH_INETDCONF;
char	**Argv;
char 	*LastArg;

main(argc, argv, envp)
	int argc;
	char *argv[], *envp[];
{
	extern char *optarg;
	extern int optind;
	register struct servtab *sep;
	register struct passwd *pwd;
	register int tmpint;
	struct sigvec sv;
	int ch, pid, dofork;
	char buf[50];

	Argv = argv;
	if (envp == 0 || *envp == 0)
		envp = argv;
	while (*envp)
		envp++;
	LastArg = envp[-1] + strlen(envp[-1]);

	while ((ch = getopt(argc, argv, "d")) != EOF)
		switch(ch) {
		case 'd':
			debug = 1;
			options |= SO_DEBUG;
			break;
		case '?':
		default:
			fprintf(stderr, "usage: inetd [-d]");
			exit(1);
		}
	argc -= optind;
	argv += optind;

	if (argc > 0)
		CONFIG = argv[0];
	if (debug == 0)
		daemon(0, 0);
	openlog("inetd", LOG_PID | LOG_NOWAIT, LOG_DAEMON);
	bzero((char *)&sv, sizeof(sv));
	sv.sv_mask = SIGBLOCK;
	sv.sv_handler = retry;
	sigvec(SIGALRM, &sv, (struct sigvec *)0);
	config();
	sv.sv_handler = config;
	sigvec(SIGHUP, &sv, (struct sigvec *)0);
	sv.sv_handler = reapchild;
	sigvec(SIGCHLD, &sv, (struct sigvec *)0);

	{
		/* space for daemons to overwrite environment for ps */
#define	DUMMYSIZE	100
		char dummy[DUMMYSIZE];

		(void)memset(dummy, 'x', sizeof(DUMMYSIZE) - 1);
		dummy[DUMMYSIZE - 1] = '\0';
		(void)setenv("inetd_dummy", dummy, 1);
	}

	for (;;) {
	    int n, ctrl;
	    fd_set readable;

	    if (nsock == 0) {
		(void) sigblock(SIGBLOCK);
		while (nsock == 0)
		    sigpause(0L);
		(void) sigsetmask(0L);
	    }
	    readable = allsock;
	    if ((n = select(maxsock + 1, &readable, (fd_set *)0,
		(fd_set *)0, (struct timeval *)0)) <= 0) {
		    if (n < 0 && errno != EINTR)
			syslog(LOG_WARNING, "select: %m\n");
		    sleep(1);
		    continue;
	    }
	    for (sep = servtab; n && sep; sep = sep->se_next)
	        if (sep->se_fd != -1 && FD_ISSET(sep->se_fd, &readable)) {
		    n--;
		    if (debug)
			    fprintf(stderr, "someone wants %s\n",
				sep->se_service);
		    if (sep->se_socktype == SOCK_STREAM) {
			    ctrl = accept(sep->se_fd, (struct sockaddr *)0,
				(int *)0);
			    if (debug)
				    fprintf(stderr, "accept, ctrl %d\n", ctrl);
			    if (ctrl < 0) {
				    if (errno == EINTR)
					    continue;
				    syslog(LOG_WARNING, "accept (for %s): %m",
					    sep->se_service);
				    continue;
			    }
		    } else
			    ctrl = sep->se_fd;
		    (void) sigblock(SIGBLOCK);
		    pid = 0;
		    dofork = (sep->se_bi == 0 || sep->se_bi->bi_fork);
		    if (dofork) {
			    if (sep->se_count++ == 0)
				(void)gettimeofday(&sep->se_time,
				    (struct timezone *)0);
			    else if (sep->se_count >= TOOMANY) {
				struct timeval now;

				(void)gettimeofday(&now, (struct timezone *)0);
				if (now.tv_sec - sep->se_time.tv_sec >
				    CNT_INTVL) {
					sep->se_time = now;
					sep->se_count = 1;
				} else {
					syslog(LOG_ERR,
			"%s/%s server failing (looping), service terminated\n",
					    sep->se_service, sep->se_proto);
					FD_CLR(sep->se_fd, &allsock);
					(void) close(sep->se_fd);
					sep->se_fd = -1;
					sep->se_count = 0;
					nsock--;
					if (!timingout) {
						timingout = 1;
						alarm(RETRYTIME);
					}
				}
			    }
			    pid = fork();
		    }
		    if (pid < 0) {
			    syslog(LOG_ERR, "fork: %m");
			    if (sep->se_socktype == SOCK_STREAM)
				    close(ctrl);
			    sigsetmask(0L);
			    sleep(1);
			    continue;
		    }
		    if (pid && sep->se_wait) {
			    sep->se_wait = pid;
			    if (sep->se_fd >= 0) {
				FD_CLR(sep->se_fd, &allsock);
			        nsock--;
			    }
		    }
		    sigsetmask(0L);
		    if (pid == 0) {
			    if (debug && dofork)
				setsid();
			    if (dofork)
				for (tmpint = maxsock; --tmpint > 2; )
					if (tmpint != ctrl)
						close(tmpint);
			    if (sep->se_bi)
				(*sep->se_bi->bi_fn)(ctrl, sep);
			    else {
				if (debug)
					fprintf(stderr, "%d execl %s\n",
					    getpid(), sep->se_server);
				dup2(ctrl, 0);
				close(ctrl);
				dup2(0, 1);
				dup2(0, 2);
				if ((pwd = getpwnam(sep->se_user)) == NULL) {
					syslog(LOG_ERR,
					    "getpwnam: %s: No such user",
					    sep->se_user);
					if (sep->se_socktype != SOCK_STREAM)
						recv(0, buf, sizeof (buf), 0);
					_exit(1);
				}
				if (pwd->pw_uid) {
					(void) setgid((gid_t)pwd->pw_gid);
					initgroups(pwd->pw_name, pwd->pw_gid);
					(void) setuid((uid_t)pwd->pw_uid);
				}
				execv(sep->se_server, sep->se_argv);
				if (sep->se_socktype != SOCK_STREAM)
					recv(0, buf, sizeof (buf), 0);
				syslog(LOG_ERR, "execv %s: %m", sep->se_server);
				_exit(1);
			    }
		    }
		    if (sep->se_socktype == SOCK_STREAM)
			    close(ctrl);
		}
	}
}

void
reapchild()
{
	int status;
	int pid;
	register struct servtab *sep;

	for (;;) {
		pid = wait3(&status, WNOHANG, (struct rusage *)0);
		if (pid <= 0)
			break;
		if (debug)
			fprintf(stderr, "%d reaped\n", pid);
		for (sep = servtab; sep; sep = sep->se_next)
			if (sep->se_wait == pid) {
				if (status)
					syslog(LOG_WARNING,
					    "%s: exit status 0x%x",
					    sep->se_server, status);
				if (debug)
					fprintf(stderr, "restored %s, fd %d\n",
					    sep->se_service, sep->se_fd);
				FD_SET(sep->se_fd, &allsock);
				nsock++;
				sep->se_wait = 1;
			}
	}
}

void
config()
{
	register struct servtab *sep, *cp, **sepp;
	struct servtab *getconfigent(), *enter();
	long omask;

	if (!setconfig()) {
		syslog(LOG_ERR, "%s: %m", CONFIG);
		return;
	}
	for (sep = servtab; sep; sep = sep->se_next)
		sep->se_checked = 0;
	while (cp = getconfigent()) {
		for (sep = servtab; sep; sep = sep->se_next)
			if (strcmp(sep->se_service, cp->se_service) == 0 &&
			    strcmp(sep->se_proto, cp->se_proto) == 0)
				break;
		if (sep != 0) {
			int i;

			omask = sigblock(SIGBLOCK);
			/*
			 * sep->se_wait may be holding the pid of a daemon
			 * that we're waiting for.  If so, don't overwrite
			 * it unless the config file explicitly says don't 
			 * wait.
			 */
			if (cp->se_bi == 0 && 
			    (sep->se_wait == 1 || cp->se_wait == 0))
				sep->se_wait = cp->se_wait;
#define SWAP(a, b) { char *c = a; a = b; b = c; }
			if (cp->se_user)
				SWAP(sep->se_user, cp->se_user);
			if (cp->se_server)
				SWAP(sep->se_server, cp->se_server);
			for (i = 0; i < MAXARGV; i++)
				SWAP(sep->se_argv[i], cp->se_argv[i]);
			sigsetmask(omask);
			freeconfig(cp);
			if (debug)
				print_service("REDO", sep);
		} else {
			sep = enter(cp);
			if (debug)
				print_service("ADD ", sep);
		}
		sep->se_checked = 1;
		sp = getservbyname(sep->se_service, sep->se_proto);
		if (sp == 0) {
			syslog(LOG_ERR, "%s/%s: unknown service",
			    sep->se_service, sep->se_proto);
			if (sep->se_fd != -1)
				(void) close(sep->se_fd);
			sep->se_fd = -1;
			continue;
		}
		if (sp->s_port != sep->se_ctrladdr.sin_port) {
			sep->se_ctrladdr.sin_port = sp->s_port;
			if (sep->se_fd != -1)
				(void) close(sep->se_fd);
			sep->se_fd = -1;
		}
		if (sep->se_fd == -1)
			setup(sep);
	}
	endconfig();
	/*
	 * Purge anything not looked at above.
	 */
	omask = sigblock(SIGBLOCK);
	sepp = &servtab;
	while (sep = *sepp) {
		if (sep->se_checked) {
			sepp = &sep->se_next;
			continue;
		}
		*sepp = sep->se_next;
		if (sep->se_fd != -1) {
			FD_CLR(sep->se_fd, &allsock);
			nsock--;
			(void) close(sep->se_fd);
		}
		if (debug)
			print_service("FREE", sep);
		freeconfig(sep);
		free((char *)sep);
	}
	(void) sigsetmask(omask);
}

void
retry()
{
	register struct servtab *sep;

	timingout = 0;
	for (sep = servtab; sep; sep = sep->se_next)
		if (sep->se_fd == -1)
			setup(sep);
}

setup(sep)
	register struct servtab *sep;
{
	int on = 1;

	if ((sep->se_fd = socket(AF_INET, sep->se_socktype, 0)) < 0) {
		syslog(LOG_ERR, "%s/%s: socket: %m",
		    sep->se_service, sep->se_proto);
		return;
	}
#define	turnon(fd, opt) \
setsockopt(fd, SOL_SOCKET, opt, (char *)&on, sizeof (on))
	if (strcmp(sep->se_proto, "tcp") == 0 && (options & SO_DEBUG) &&
	    turnon(sep->se_fd, SO_DEBUG) < 0)
		syslog(LOG_ERR, "setsockopt (SO_DEBUG): %m");
	if (turnon(sep->se_fd, SO_REUSEADDR) < 0)
		syslog(LOG_ERR, "setsockopt (SO_REUSEADDR): %m");
#undef turnon
	if (bind(sep->se_fd, (struct sockaddr *)&sep->se_ctrladdr,
	    sizeof (sep->se_ctrladdr)) < 0) {
		syslog(LOG_ERR, "%s/%s: bind: %m",
		    sep->se_service, sep->se_proto);
		(void) close(sep->se_fd);
		sep->se_fd = -1;
		if (!timingout) {
			timingout = 1;
			alarm(RETRYTIME);
		}
		return;
	}
	if (sep->se_socktype == SOCK_STREAM)
		listen(sep->se_fd, 10);
	FD_SET(sep->se_fd, &allsock);
	nsock++;
	if (sep->se_fd > maxsock)
		maxsock = sep->se_fd;
}

struct servtab *
enter(cp)
	struct servtab *cp;
{
	register struct servtab *sep;
	long omask;

	sep = (struct servtab *)malloc(sizeof (*sep));
	if (sep == (struct servtab *)0) {
		syslog(LOG_ERR, "Out of memory.");
		exit(-1);
	}
	*sep = *cp;
	sep->se_fd = -1;
	omask = sigblock(SIGBLOCK);
	sep->se_next = servtab;
	servtab = sep;
	sigsetmask(omask);
	return (sep);
}

FILE	*fconfig = NULL;
struct	servtab serv;
char	line[256];
char	*skip(), *nextline();

setconfig()
{

	if (fconfig != NULL) {
		fseek(fconfig, 0L, L_SET);
		return (1);
	}
	fconfig = fopen(CONFIG, "r");
	return (fconfig != NULL);
}

endconfig()
{
	if (fconfig) {
		(void) fclose(fconfig);
		fconfig = NULL;
	}
}

struct servtab *
getconfigent()
{
	register struct servtab *sep = &serv;
	int argc;
	char *cp, *arg, *newstr();

more:
	while ((cp = nextline(fconfig)) && *cp == '#')
		;
	if (cp == NULL)
		return ((struct servtab *)0);
	sep->se_service = newstr(skip(&cp));
	arg = skip(&cp);
	if (strcmp(arg, "stream") == 0)
		sep->se_socktype = SOCK_STREAM;
	else if (strcmp(arg, "dgram") == 0)
		sep->se_socktype = SOCK_DGRAM;
	else if (strcmp(arg, "rdm") == 0)
		sep->se_socktype = SOCK_RDM;
	else if (strcmp(arg, "seqpacket") == 0)
		sep->se_socktype = SOCK_SEQPACKET;
	else if (strcmp(arg, "raw") == 0)
		sep->se_socktype = SOCK_RAW;
	else
		sep->se_socktype = -1;
	sep->se_proto = newstr(skip(&cp));
	arg = skip(&cp);
	sep->se_wait = strcmp(arg, "wait") == 0;
	sep->se_user = newstr(skip(&cp));
	sep->se_server = newstr(skip(&cp));
	if (strcmp(sep->se_server, "internal") == 0) {
		register struct biltin *bi;

		for (bi = biltins; bi->bi_service; bi++)
			if (bi->bi_socktype == sep->se_socktype &&
			    strcmp(bi->bi_service, sep->se_service) == 0)
				break;
		if (bi->bi_service == 0) {
			syslog(LOG_ERR, "internal service %s unknown\n",
				sep->se_service);
			goto more;
		}
		sep->se_bi = bi;
		sep->se_wait = bi->bi_wait;
	} else
		sep->se_bi = NULL;
	argc = 0;
	for (arg = skip(&cp); cp; arg = skip(&cp))
		if (argc < MAXARGV)
			sep->se_argv[argc++] = newstr(arg);
	while (argc <= MAXARGV)
		sep->se_argv[argc++] = NULL;
	return (sep);
}

freeconfig(cp)
	register struct servtab *cp;
{
	int i;

	if (cp->se_service)
		free(cp->se_service);
	if (cp->se_proto)
		free(cp->se_proto);
	if (cp->se_user)
		free(cp->se_user);
	if (cp->se_server)
		free(cp->se_server);
	for (i = 0; i < MAXARGV; i++)
		if (cp->se_argv[i])
			free(cp->se_argv[i]);
}

char *
skip(cpp)
	char **cpp;
{
	register char *cp = *cpp;
	char *start;

again:
	while (*cp == ' ' || *cp == '\t')
		cp++;
	if (*cp == '\0') {
		int c;

		c = getc(fconfig);
		(void) ungetc(c, fconfig);
		if (c == ' ' || c == '\t')
			if (cp = nextline(fconfig))
				goto again;
		*cpp = (char *)0;
		return ((char *)0);
	}
	start = cp;
	while (*cp && *cp != ' ' && *cp != '\t')
		cp++;
	if (*cp != '\0')
		*cp++ = '\0';
	*cpp = cp;
	return (start);
}

char *
nextline(fd)
	FILE *fd;
{
	char *cp;

	if (fgets(line, sizeof (line), fd) == NULL)
		return ((char *)0);
	cp = index(line, '\n');
	if (cp)
		*cp = '\0';
	return (line);
}

char *
newstr(cp)
	char *cp;
{
	if (cp = strdup(cp ? cp : ""))
		return(cp);
	syslog(LOG_ERR, "strdup: %m");
	exit(-1);
}

setproctitle(a, s)
	char *a;
	int s;
{
	int size;
	register char *cp;
	struct sockaddr_in sin;
	char buf[80];

	cp = Argv[0];
	size = sizeof(sin);
	if (getpeername(s, (struct sockaddr *)&sin, &size) == 0)
		(void) sprintf(buf, "-%s [%s]", a, inet_ntoa(sin.sin_addr)); 
	else
		(void) sprintf(buf, "-%s", a); 
	strncpy(cp, buf, LastArg - cp);
	cp += strlen(cp);
	while (cp < LastArg)
		*cp++ = ' ';
}

/*
 * Internet services provided internally by inetd:
 */
#define	BUFSIZE	8192

/* ARGSUSED */
echo_stream(s, sep)		/* Echo service -- echo data back */
	int s;
	struct servtab *sep;
{
	char buffer[BUFSIZE];
	int i;

	setproctitle(sep->se_service, s);
	while ((i = read(s, buffer, sizeof(buffer))) > 0 &&
	    write(s, buffer, i) > 0)
		;
	exit(0);
}

/* ARGSUSED */
echo_dg(s, sep)			/* Echo service -- echo data back */
	int s;
	struct servtab *sep;
{
	char buffer[BUFSIZE];
	int i, size;
	struct sockaddr sa;

	size = sizeof(sa);
	if ((i = recvfrom(s, buffer, sizeof(buffer), 0, &sa, &size)) < 0)
		return;
	(void) sendto(s, buffer, i, 0, &sa, sizeof(sa));
}

/* ARGSUSED */
discard_stream(s, sep)		/* Discard service -- ignore data */
	int s;
	struct servtab *sep;
{
	int ret;
	char buffer[BUFSIZE];

	setproctitle(sep->se_service, s);
	while (1) {
		while ((ret = read(s, buffer, sizeof(buffer))) > 0)
			;
		if (ret == 0 || errno != EINTR)
			break;
	}
	exit(0);
}

/* ARGSUSED */
discard_dg(s, sep)		/* Discard service -- ignore data */
	int s;
	struct servtab *sep;
{
	char buffer[BUFSIZE];

	(void) read(s, buffer, sizeof(buffer));
}

#include <ctype.h>
#define LINESIZ 72
char ring[128];
char *endring;

initring()
{
	register int i;

	endring = ring;

	for (i = 0; i <= 128; ++i)
		if (isprint(i))
			*endring++ = i;
}

/* ARGSUSED */
chargen_stream(s, sep)		/* Character generator */
	int s;
	struct servtab *sep;
{
	register char *rs;
	int len;
	char text[LINESIZ+2];

	setproctitle(sep->se_service, s);

	if (!endring) {
		initring();
		rs = ring;
	}

	text[LINESIZ] = '\r';
	text[LINESIZ + 1] = '\n';
	for (rs = ring;;) {
		if ((len = endring - rs) >= LINESIZ)
			bcopy(rs, text, LINESIZ);
		else {
			bcopy(rs, text, len);
			bcopy(ring, text + len, LINESIZ - len);
		}
		if (++rs == endring)
			rs = ring;
		if (write(s, text, sizeof(text)) != sizeof(text))
			break;
	}
	exit(0);
}

/* ARGSUSED */
chargen_dg(s, sep)		/* Character generator */
	int s;
	struct servtab *sep;
{
	struct sockaddr sa;
	static char *rs;
	int len, size;
	char text[LINESIZ+2];

	if (endring == 0) {
		initring();
		rs = ring;
	}

	size = sizeof(sa);
	if (recvfrom(s, text, sizeof(text), 0, &sa, &size) < 0)
		return;

	if ((len = endring - rs) >= LINESIZ)
		bcopy(rs, text, LINESIZ);
	else {
		bcopy(rs, text, len);
		bcopy(ring, text + len, LINESIZ - len);
	}
	if (++rs == endring)
		rs = ring;
	text[LINESIZ] = '\r';
	text[LINESIZ + 1] = '\n';
	(void) sendto(s, text, sizeof(text), 0, &sa, sizeof(sa));
}

/*
 * Return a machine readable date and time, in the form of the
 * number of seconds since midnight, Jan 1, 1900.  Since gettimeofday
 * returns the number of seconds since midnight, Jan 1, 1970,
 * we must add 2208988800 seconds to this figure to make up for
 * some seventy years Bell Labs was asleep.
 */

long
machtime()
{
	struct timeval tv;

	if (gettimeofday(&tv, (struct timezone *)0) < 0) {
		fprintf(stderr, "Unable to get time of day\n");
		return (0L);
	}
	return (htonl((long)tv.tv_sec + 2208988800));
}

/* ARGSUSED */
machtime_stream(s, sep)
	int s;
	struct servtab *sep;
{
	long result;

	result = machtime();
	(void) write(s, (char *) &result, sizeof(result));
}

/* ARGSUSED */
machtime_dg(s, sep)
	int s;
	struct servtab *sep;
{
	long result;
	struct sockaddr sa;
	int size;

	size = sizeof(sa);
	if (recvfrom(s, (char *)&result, sizeof(result), 0, &sa, &size) < 0)
		return;
	result = machtime();
	(void) sendto(s, (char *) &result, sizeof(result), 0, &sa, sizeof(sa));
}

/* ARGSUSED */
daytime_stream(s, sep)		/* Return human-readable time of day */
	int s;
	struct servtab *sep;
{
	char buffer[256];
	time_t time(), clock;
	char *ctime();

	clock = time((time_t *) 0);

	(void) sprintf(buffer, "%.24s\r\n", ctime(&clock));
	(void) write(s, buffer, strlen(buffer));
}

/* ARGSUSED */
daytime_dg(s, sep)		/* Return human-readable time of day */
	int s;
	struct servtab *sep;
{
	char buffer[256];
	time_t time(), clock;
	struct sockaddr sa;
	int size;
	char *ctime();

	clock = time((time_t *) 0);

	size = sizeof(sa);
	if (recvfrom(s, buffer, sizeof(buffer), 0, &sa, &size) < 0)
		return;
	(void) sprintf(buffer, "%.24s\r\n", ctime(&clock));
	(void) sendto(s, buffer, strlen(buffer), 0, &sa, sizeof(sa));
}

/*
 * print_service:
 *	Dump relevant information to stderr
 */
print_service(action, sep)
	char *action;
	struct servtab *sep;
{
	fprintf(stderr,
	    "%s: %s proto=%s, wait=%d, user=%s builtin=%x server=%s\n",
	    action, sep->se_service, sep->se_proto,
	    sep->se_wait, sep->se_user, (int)sep->se_bi, sep->se_server);
}
