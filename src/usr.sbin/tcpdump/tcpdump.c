/*
 * Copyright (c) 1987-1990 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
#ifndef lint
char copyright[] =
    "@(#) Copyright (c) 1987-1990 The Regents of the University of California.\nAll rights reserved.\n";
static  char rcsid[] =
    "@(#)$Header: tcpdump.c,v 1.55 91/05/06 02:10:34 mccanne Exp $ (LBL)";
#endif

/*
 * tcpdump - monitor tcp/ip traffic on an ethernet.
 *
 * First written in 1987 by Van Jacobson, Lawrence Berkeley Laboratory.
 * Mercilessly hacked and occasionally improved since then via the
 * combined efforts of Van, Steve McCanne and Craig Leres of LBL.
 */

#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
/*#include <sys/timeb.h> */
#include <netinet/in.h>

#include <net/bpf.h>

#include "interface.h"
#include "savefile.h"
#include "addrtoname.h"

int fflag;			/* don't translate "foreign" IP address */
int nflag;			/* leave addresses as numbers */
int Nflag;			/* remove domains from printed host names */
int pflag;			/* don't go promiscuous */
int qflag;			/* quick (shorter) output */
int cflag;			/* only print 'cnt' packets */
int tflag = 1;			/* print packet arrival time */
int eflag;			/* print ethernet header */
int vflag;			/* verbose */
int xflag;			/* print packet in hex */
int Oflag = 1;			/* run filter code optimizer */
int Sflag;			/* print raw TCP sequence numbers */

int dflag;			/* print filter code */

long thiszone;			/* gmt to local correction */

static void cleanup();

/* Length of saved portion of packet. */
int snaplen = DEFAULT_SNAPLEN;

static int if_fd = -1;

struct printer {
	void (*f)();
	int type;
};

static struct printer printers[] = {
	{ ether_if_print, DLT_EN10MB },
	{ sl_if_print, DLT_SLIP },
	{ ppp_if_print, DLT_PPP },
	{ 0, 0 },
};

void
(*lookup_printer(type))()
	int type;
{
	struct printer *p;

	for (p = printers; p->f; ++p)
		if (type == p->type)
			return p->f;

	error("unknown data link type %x", type);
	/* NOTREACHED */
}

void
main(argc, argv)
	int argc;
	char **argv;
{
	struct bpf_program *parse();
	void bpf_dump();

	int cnt, i;
/*	struct timeb zt;*/
	struct bpf_program *fcode;
	int op;
	void (*printit)();
	char *infile = 0;
	char *cmdbuf;
	int linktype;
	int err;

	char *RFileName = 0;	/* -r argument */
	char *WFileName = 0;	/* -w argument */

	char *device = 0;

	extern char *optarg;
	extern int optind, opterr;

	opterr = 0;
	while ((op = getopt(argc, argv, "c:defF:i:lnNOpqr:s:Stvw:xY")) != EOF)
		switch (op) {
		case 'c':
			++cflag;
			cnt = atoi(optarg);
			break;

		case 'd':
			++dflag;
			break;

		case 'e':
			++eflag;
			break;

		case 'f':
			++fflag;
			break;

		case 'F':
			infile = optarg;
			break;

		case 'i':
			device = optarg;
			break;

		case 'l':
			setlinebuf(stdout);
			break;

		case 'n':
			++nflag;
			break;

		case 'N':
			++Nflag;
			break;

		case 'O':
			Oflag = 0;
			break;

		case 'p':
			++pflag;
			break;

		case 'q':
			++qflag;
			break;

		case 'r':
 			RFileName = optarg;
 			break;

		case 's':
			snaplen = atoi(optarg);
			break;

		case 'S':
			++Sflag;
			break;

		case 't':
			--tflag;
			break;

		case 'v':
			++vflag;
			break;

		case 'w':
 			WFileName = optarg;
 			break;
#ifdef YYDEBUG
		case 'Y':
			{
			extern int yydebug;
			yydebug = 1;
			}
			break;
#endif
		case 'x':
			++xflag;
			break;

		default:
			usage();
			/* NOTREACHED */
		}

	if (tflag > 0) {
		struct timeval now;
		struct timezone tz;

		if (gettimeofday(&now, &tz) < 0) {
			perror("gettimeofday");
			exit(1);
		}
		thiszone = tz.tz_minuteswest * -60;
		if (localtime(&now.tv_sec)->tm_isdst)
			thiszone += 3600;

		/* Machine specific timestamp initialization */
		timestampinit(clock_sigfigs());
	}

	if (RFileName) {
		/*
		 * We don't need network access, so set back the user id.
		 */
		setuid(getuid());

		err = sf_read_init(RFileName, &linktype);
		if (err)
			sf_err(err);
	}
	else {
		if (device == 0) {
			device = lookup_device();
			if (device == 0)
				error("can't find any interfaces");
		}
		if_fd = initdevice(device, pflag, &linktype);

		/*
		 * Let user own process after socket has been opened.
		 */
		setuid(getuid());
	}

	if (infile) 
		cmdbuf = read_infile(infile);
	else
		cmdbuf = copy_argv(&argv[optind]);

	fcode = parse(cmdbuf, Oflag, linktype);
	if (dflag) {
		bpf_dump(fcode, dflag);
		exit(0);
	}

	init_addrtoname(device, fflag);

	(void)signal(SIGTERM, cleanup);
	(void)signal(SIGINT, cleanup);
	(void)signal(SIGHUP, cleanup);

	printit = lookup_printer(linktype);

	if (WFileName) {
		sf_write_init(WFileName, linktype);
		printit = sf_write;
	}
	if (RFileName) {
		err = sf_read(fcode, cnt, printit);
		if (err)
			sf_err(err);
	} else {
		fprintf(stderr, "tcpdump: listening on %s\n", device);
		fflush(stderr);
		readloop(cnt, if_fd, fcode, printit);
	}
	exit(0);
}

/* make a clean exit on interrupts */
static void
cleanup()
{
	if (if_fd >= 0) {
		putc('\n', stderr);
		wrapup(if_fd);
	}
	exit(0);
}

void
default_print(sp, length)
	register u_short *sp;
	register int length;
{
	register u_int i;
	register int nshorts;

	nshorts = (unsigned) length / sizeof(u_short);
	i = 0;
	while (--nshorts >= 0) {
		if ((i++ % 8) == 0)
			(void)printf("\n\t\t\t");
		(void)printf(" %04x", ntohs(*sp++));
	}
	if (length & 1) {
		if ((i % 8) == 0)
			(void)printf("\n\t\t\t");
		(void)printf(" %02x", *(u_char *)sp);
	}
}

void
usage()
{
	(void)fprintf(stderr, "Version %d.%d\n", VERSION_MAJOR, VERSION_MINOR);
	(void)fprintf(stderr,
"Usage: tcpdump [-deflnOpqtvx] [-c count] [-i interface]\n");
	(void)fprintf(stderr,
"\t\t[-r filename] [-w filename] [expr]\n");
	exit(-1);
}
