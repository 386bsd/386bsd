/*
 * Copyright (c) 1988, 1990 The Regents of the University of California.
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
    "@(#)$Header: pcap-nit.c,v 1.12 90/12/04 17:13:24 mccanne Exp $ (LBL)";
#endif

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
#include <net/nit.h>

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

/*
 * The chunk size for NIT.  This is the amount of buffering
 * done for read calls.
 */
#define CHUNKSIZE (2*1024)

/*
 * The total buffer space used by NIT.
 */
#define BUFSPACE (4*CHUNKSIZE)

void
readloop(cnt, if_fd, fp, printit)
	int cnt;
	int if_fd;
	struct bpf_program *fp;
	void (*printit)();
{
	u_char buf[CHUNKSIZE];
	struct bpf_insn *fcode;
	int cc, i;

	fcode = fp->bf_insns;
	i = 0;

	while ((cc = read(if_fd, (char *)buf, sizeof(buf))) > 0) {
		register u_char *bp, *bstop;
		register u_char *sp;
		register struct nit_hdr *nh;
		register int datalen = 0;
		int caplen;
		
		/*
		 * Loop through each packet.  The increment expression
		 * rounds up to the next int boundary past the end of
		 * the previous packet.
		 */
		bstop = buf + cc;
		for (bp = buf; bp < bstop;
		     bp += ((sizeof(struct nit_hdr)+datalen+sizeof(int)-1)
			    & ~(sizeof(int)-1))) {

			nh = (struct nit_hdr *)bp;
			sp = bp + sizeof(*nh);
			
			switch (nh->nh_state) {
			case NIT_CATCH:
				datalen = nh->nh_datalen;
				break;
			case NIT_SEQNO:
			case NIT_NOMBUF:
			case NIT_NOCLUSTER:
			case NIT_NOSPACE:
				datalen = 0;
				continue;
			default:
				(void)fprintf(stderr,
				      "bad nit state %d\n", nh->nh_state);
				exit(1);
			}
			caplen = nh->nh_wirelen;
			if (caplen > snaplen)
				caplen = snaplen;
			if (bpf_filter(fcode, sp, nh->nh_wirelen, caplen)) {
				(*printit)(sp, &nh->nh_timestamp, 
					   nh->nh_wirelen, caplen);
				if (cflag && ++i >= cnt) {
					wrapup(if_fd);
					return;
				}
			}
		}
	}
	perror("read");
	exit(-1);
}

wrapup(fd)
	int fd;
{
	close(fd);
}

int
initdevice(device, pflag, linktype)
	char *device;
	int pflag;
	int *linktype;
{
	struct sockaddr_nit snit;
	struct nit_ioc nioc;
	int if_fd;

	if (snaplen < 96) 
		/*
		 * NIT requires a snapshot length of at least 96.
		 */
		snaplen = 96;

	if_fd = socket(AF_NIT, SOCK_RAW, NITPROTO_RAW);

	if (if_fd < 0) {
		perror("nit socket");
		exit(-1);
	}

	snit.snit_family = AF_NIT;
	(void)strncpy(snit.snit_ifname, device, NITIFSIZ);

	if (bind(if_fd, (struct sockaddr *)&snit, sizeof(snit))) {
		perror(snit.snit_ifname);
		exit(1);
	}

	bzero((char *)&nioc, sizeof(nioc));
	nioc.nioc_bufspace = BUFSPACE;
	nioc.nioc_chunksize = CHUNKSIZE;
	nioc.nioc_typetomatch = NT_ALLTYPES;
	nioc.nioc_snaplen = snaplen;
	nioc.nioc_bufalign = sizeof(int);
	nioc.nioc_bufoffset = 0;
	nioc.nioc_flags = pflag ? NF_TIMEOUT : NF_PROMISC|NF_TIMEOUT;
	nioc.nioc_timeout.tv_sec = 1;
	nioc.nioc_timeout.tv_usec = 0;

	if (ioctl(if_fd, SIOCSNIT, &nioc) != 0) {
		perror("nit ioctl");
		exit(1);
	}
	/*
	 * NIT supports only ethernets.
	 */
	*linktype = DLT_EN10MB;

	return if_fd;
}
