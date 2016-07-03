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
	"@(#)$Header: print-ppp.c,v 1.5 91/04/19 10:46:44 mccanne Exp $ (LBL)";
#endif

#ifdef PPP
#include <stdio.h>
#include <netdb.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
/*#include <sys/timeb.h>*/
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/mbuf.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#include <net/bpf.h>

#include "interface.h"
#include "addrtoname.h"

/* XXX This goes somewhere else. */
#define PPP_HDRLEN 4

void
ppp_if_print(p, tvp, length, caplen)
	u_char *p;
	struct timeval *tvp;
	int length;
	int caplen;
{
	struct ip *ip;

	if (tflag > 0) {
		int i = (tvp->tv_sec + thiszone) % 86400;
		(void)printf(timestamp_fmt,
			     i / 3600, (i % 3600) / 60, i % 60,
			     tvp->tv_usec / timestamp_scale);
	} else if (tflag < 0)
		printf("%d.%06d ", tvp->tv_sec, tvp->tv_usec);

	if (caplen < PPP_HDRLEN) {
		printf("[|ppp]");
		goto out;
	}

	/*
	 * Some printers want to get back at the link level addresses,
	 * and/or check that they're not walking off the end of the packet.
	 * Rather than pass them all the way down, we set these globals.
	 */
	packetp = (u_char *)p;
	snapend = (u_char *)p + caplen;

	length -= PPP_HDRLEN;

	ip = (struct ip *)(p + PPP_HDRLEN);

	if (eflag)
		printf("%c %02x %04x: ", p[0] ? 'O' : 'I', p[1], 
		       ntohs(*(u_short *)&p[2]));

	ip_print(ip, length);

	if (xflag)
		default_print((u_short *)ip, caplen - PPP_HDRLEN);
 out:
	putchar('\n');
}
#else
#include <stdio.h>
void
ppp_if_print()
{
	void error();

	error("not configured for ppp");
	/* NOTREACHED */
}
#endif
