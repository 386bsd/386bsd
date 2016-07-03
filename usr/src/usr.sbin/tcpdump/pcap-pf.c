/*
 * Copyright (c) 1990 The Regents of the University of California.
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
static  char rcsid[] =
    "@(#)$Header: pcap-pf.c,v 1.16 90/12/04 17:12:42 mccanne Exp $ (LBL)";
#endif

/*
 * packet filter subroutines for tcpdump
 *	Extraction/creation by Jeffrey Mogul, DECWRL
 *
 * Extracted from tcpdump.c.
 */

#include <stdio.h>
#include <netdb.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <net/pfilt.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>
#include <net/bpf.h>

#include "interface.h"

static	u_long	TotPkts = 0;		/* can't oflow for 79 hrs on ether */
static	u_long	TotDrops = 0;		/* count of dropped packets */
static	long	TotMissed = 0;		/* missed by i/f during this run */
static	long	OrigMissed = -1;	/* missed by i/f before this run */

void pfReadError();
static void PrintPFStats();

extern int errno;

/*
 * BUFSPACE is the size in bytes of the packet read buffer.  Most tcpdump
 * applications aren't going to need more than 200 bytes of packet header
 * and the read shouldn't return more packets than packetfilter's internal
 * queue limit (bounded at 256).
 */
#define BUFSPACE (200*256)

void
readloop(cnt, if_fd, fp, printit)
	int cnt;
	int if_fd;
	struct bpf_program *fp;
	void (*printit)();
{
	u_char *p;
	struct bpf_insn *fcode;
	int cc;
	/* This funny declaration forces buf to be properly aligned.
	   We really just want a u_char buffer that is BUFSPACE
	   bytes large. */
	struct enstamp buf[BUFSPACE / sizeof(struct enstamp)];


	fcode = fp->bf_insns;
	while (1) {
		register u_char *bp;
		int buflen, inc;
		struct enstamp stamp;

		if ((cc = read(if_fd, (char *)buf, sizeof(buf))) < 0) {
			pfReadError(if_fd, "reader");
		}
		/*
		 * Loop through each packet.
		 */
		bp = (u_char *)buf;
		while (cc > 0) {
			/* avoid alignment issues here */
			bcopy((char *)bp, (char *)&stamp, sizeof(stamp));

			if (stamp.ens_stamplen != sizeof(stamp)) {
				/* buffer is garbage, treat it as poison */
				break;
			}

			TotPkts++;
			TotDrops += stamp.ens_dropped;
			TotMissed = stamp.ens_ifoverflows;
			if (OrigMissed < 0)
				OrigMissed = TotMissed;

			p = bp + stamp.ens_stamplen;

			buflen = stamp.ens_count;
			if (buflen > snaplen)
				buflen = snaplen;

			if (bpf_filter(fcode, p, stamp.ens_count, buflen)) { 
				(*printit)(p, &stamp.ens_tstamp,
					   stamp.ens_count, buflen);
				if (cflag && --cnt == 0)
					goto out;
			}
			inc = ENALIGN(buflen + stamp.ens_stamplen);
			cc -= inc;
			bp += inc;
		}
	}
 out:
	wrapup(if_fd);
}

/* Call ONLY if read() has returned an error on packet filter */
void
pfReadError(fid, msg)
	int fid;
	char *msg;
{
	if (errno == EINVAL) {	/* read MAXINT bytes already! */
		if (lseek(fid, 0L, 0) < 0) {
			perror("pfReadError/lseek");
			exit(-1);
		}
		else
			return;
	}
	else {
		perror(msg);
		exit(-1);
	}
}

wrapup(fd)
	int fd;
{
	PrintPFStats();
	(void)close(fd);
}

static void
PrintPFStats()
{
	int missed = TotMissed - OrigMissed;
	
	(void)printf("%d packets", TotPkts);
	if (TotDrops)
		(void)printf(" + %d discarded by kernel", TotDrops);
	if (missed)
		(void)printf(" + %d discarded by interface", missed);
	(void)printf("\n");
}

int
initdevice(device, pflag, linktype)
	char *device;
	int pflag;
	int *linktype;
{
	struct timeval timeout;
	short enmode;
	int backlog = -1;	/* request the most */
	struct enfilter Filter;
	int if_fd;
	
	if_fd = pfopen(device, 0);
	if (if_fd < 0) {
		perror(device);
		error(
"your system may not be properly configured; see \"man packetfilter(4)\"");
	}

	/* set timeout */
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
	if (ioctl(if_fd, EIOCSRTIMEOUT, &timeout) < 0) {
		perror(device);
		exit(-1);
	}

	enmode = ENTSTAMP|ENBATCH|ENNONEXCL;
	/* set promiscuous mode if requested */
	if (pflag == 0) {
		enmode |= ENPROMISC;
	}
	if (ioctl(if_fd, EIOCMBIS, &enmode) < 0) {
		perror(device);
		exit(-1);
	}

#ifdef	ENCOPYALL
	/* Try to set COPYALL mode so that we see packets to ourself */
	enmode = ENCOPYALL;
	ioctl(if_fd, EIOCMBIS, &enmode);	/* OK if this fails */
#endif	ENCOPYALL

	/* set the backlog */
	if (ioctl(if_fd, EIOCSETW, &backlog) < 0) {
		perror(device);
		exit(-1);
	}

	/* set truncation */
	if (ioctl(if_fd, EIOCTRUNCATE, &snaplen) < 0) {
		perror(device);
		exit(-1);
	}
	/* accept all packets */
	Filter.enf_Priority = 37;	/* anything > 2 */
	Filter.enf_FilterLen = 0;	/* means "always true" */
	if (ioctl(if_fd, EIOCSETF, &Filter) < 0) {
		perror(device);
		exit(-1);
	}
	/*
	 * XXX
	 * Currently, the Ultrix packet filter supports only Ethernets.
	 * Eventually, support for FDDI and PPP (and possibly others) will
	 * be added.  At some point, we will have to recode this to do 
	 * the EIOCDEVP ioctl to get the ENDT_* type, and then convert 
	 * that to a DLT_* type.
	 */
	*linktype = DLT_EN10MB;

	return(if_fd);
}
