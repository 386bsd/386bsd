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
static char rcsid[] =
    "@(#) $Header: print-nfs.c,v 1.17 91/05/06 00:54:21 mccanne Exp $ (LBL)";
#endif

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>

#include <sys/time.h>
#include <errno.h>
#include <rpc/types.h>
#include <rpc/auth.h>
#include <rpc/auth_unix.h>
#include <rpc/svc.h>
#include <rpc/xdr.h>
#include <rpc/rpc_msg.h>

#include <ctype.h>

#include "interface.h"
/* These must come after interface.h for BSD. */
#if BSD >= 199006
#include <nfs/nfsv2.h>
#endif
#include <nfs/nfs.h>

#include "addrtoname.h"
#include "extract.h"

static void printfh(), nfs_printfn();

void
nfsreply_print(rp, length, ip)
	register struct rpc_msg *rp;
	int length;
	register struct ip *ip;
{
#if	BYTE_ORDER == LITTLE_ENDIAN
	bswap(rp, sizeof(*rp));
#endif
	if (!nflag)
		(void)printf("%s.nfs > %s.%x: reply %s %d",
			     ipaddr_string(&ip->ip_src),
			     ipaddr_string(&ip->ip_dst),
			     rp->rm_xid,
			     rp->rm_reply.rp_stat == MSG_ACCEPTED? "ok":"ERR",
			     length);
	else
		(void)printf("%s.%x > %s.%x: reply %s %d",
			     ipaddr_string(&ip->ip_src),
			     NFS_PORT,
			     ipaddr_string(&ip->ip_dst),
			     rp->rm_xid,
			     rp->rm_reply.rp_stat == MSG_ACCEPTED? "ok":"ERR",
			     length);
}

void
nfsreq_print(rp, length, ip)
	register struct rpc_msg *rp;
	int length;
	register struct ip *ip;
{
	register u_long *dp;
	register u_long *ep = (u_long *)snapend;

#if	BYTE_ORDER == LITTLE_ENDIAN
	bswap(rp, sizeof(*rp));
#endif

	if (!nflag)
		(void)printf("%s.%x > %s.nfs: %d",
			     ipaddr_string(&ip->ip_src),
			     rp->rm_xid,
			     ipaddr_string(&ip->ip_dst),
			     length);
	else
		(void)printf("%s.%x > %s.%x: %d",
			     ipaddr_string(&ip->ip_src),
			     rp->rm_xid,
			     ipaddr_string(&ip->ip_dst),
			     NFS_PORT,
			     length);


	/* find the start of the req data(if we captured it) */
	dp = (u_long *)(&rp->rm_call.cb_cred);
	if (dp < ep && dp[1] < length) {
		dp += (dp[1] + (2*sizeof(u_long) + 3)) / sizeof(u_long);
		if ((dp < ep) && (dp[1] < length)) {
			dp += (dp[1] + (2*sizeof(u_long) + 3)) / 
				sizeof(u_long);
			if (dp >= ep)
				dp = 0;
		} else
			dp = 0;
	} else
		dp = 0;

	switch (rp->rm_call.cb_proc) {
	case RFS_NULL:
		printf(" null");
		break;
	case RFS_GETATTR:
		printf(" getattr");
		if (dp)
			printfh(dp);
		break;
	case RFS_SETATTR:
		printf(" setattr");
		if (dp)
			printfh(dp);
		break;
	case RFS_ROOT:
		printf(" root");
		break;
	case RFS_LOOKUP:
		printf(" lookup");
		if (dp) {
			printfh(dp);
			putchar(' ');
			nfs_printfn(&dp[8]);
		}
		break;
	case RFS_READLINK:
		printf(" readlink");
		if (dp)
			printfh(dp);
		break;
	case RFS_READ:
		printf(" read");
		if (dp) {
			printfh(dp);
 			printf(" %d (%d) bytes @ %d",
 				ntohl(dp[9]), ntohl(dp[10]), ntohl(dp[8]));
		}
		break;
	case RFS_WRITECACHE:
		printf(" writecache");
		if (dp) {
			printfh(dp);
			printf(" %d (%d) bytes @ %d (%d)",
 				ntohl(dp[11]), ntohl(dp[10]),
 				ntohl(dp[9]), ntohl(dp[8]));
		}
		break;
	case RFS_WRITE:
		printf(" write");
		if (dp) {
			printfh(dp);
			printf(" %d (%d) bytes @ %d (%d)",
 				ntohl(dp[11]), ntohl(dp[10]),
 				ntohl(dp[9]), ntohl(dp[8]));
		}
		break;
	case RFS_CREATE:
		printf(" create");
		if (dp) {
			printfh(dp);
			putchar(' ');
			nfs_printfn(&dp[8]);
		}
		break;
	case RFS_REMOVE:
		printf(" remove");
		if (dp) {
			printfh(dp);
			putchar(' ');
			nfs_printfn(&dp[8]);
		}
		break;
	case RFS_RENAME:
		printf(" rename");
		if (dp) {
			printfh(dp);
			putchar(' ');
			nfs_printfn(&dp[8]);
		}
		break;
	case RFS_LINK:
		printf(" link");
		if (dp)
			printfh(dp);
		break;
	case RFS_SYMLINK:
		printf(" symlink");
		if (dp) {
			printfh(dp);
			putchar(' ');
			nfs_printfn(&dp[8]);
		}
		break;
	case RFS_MKDIR:
		printf(" mkdir");
		if (dp) {
			printfh(dp);
			putchar(' ');
			nfs_printfn(&dp[8]);
		}
		break;
	case RFS_RMDIR:
		printf(" rmdir");
		if (dp) {
			printfh(dp);
			putchar(' ');
			nfs_printfn(&dp[8]);
		}
		break;
	case RFS_READDIR:
		printf(" readdir");
		if (dp) {
			printfh(dp);
			printf(" %d bytes @ %d", ntohl(dp[9]), ntohl(dp[8]));
		}
		break;
	case RFS_STATFS:
		printf(" statfs");
		if (dp)
			printfh(dp);
		break;
	default:
		printf(" proc-%d", rp->rm_call.cb_proc);
		break;
	}
}

static void
printfh(dp)
	register u_long *dp;
{
	/*
	 * take a wild guess at the structure of file handles.
	 * On sun 3s, there are 2 longs of fsid, a short
	 * len == 8, a long of inode & a long of generation number.
	 * On sun 4s, the len == 10 & there are 2 bytes of
	 * padding immediately following it.
	 */
	if (dp[2] == 0xa0000) {
		if (dp[1])
			(void) printf(" fh %d.%d.%d", dp[0], dp[1], dp[3]);
		else
			(void) printf(" fh %d.%d", dp[0], dp[3]);
	} else if ((dp[2] >> 16) == 8)
		/*
		 * 'dp' is longword aligned, so we must use the extract
		 * macros below for dp+10 which cannot possibly be aligned.
		 */
		if (dp[1])
			(void) printf(" fh %d.%d.%d", dp[0], dp[1],
				      EXTRACT_LONG((u_char *)dp + 10));
		else
			(void) printf(" fh %d.%d", dp[0],
				      EXTRACT_LONG((u_char *)dp + 10));
	/* On Ultrix pre-4.0, three longs: fsid, fno, fgen and then zeros */
	else if (dp[3] == 0) {
		(void)printf(" fh %d,%d/%d.%d", major(dp[0]), minor(dp[0]),
			     dp[1], dp[2]);
	}
	/*
	 * On Ultrix 4.0, 
	 * five longs: fsid, fno, fgen, eno, egen and then zeros
	 */
	else if (dp[5] == 0) {
		(void)printf(" fh %d,%d/%d.%d", major(dp[0]), minor(dp[0]),
			     dp[1], dp[2]);
		if (vflag) {
			/* print additional info */
			(void)printf("[%d.%d]", dp[3], dp[4]);
		}
	}
	else
		(void) printf(" fh %x.%x.%x.%x", dp[0], dp[1], dp[2], dp[3]);
}

static void
nfs_printfn(dp)
	u_long *dp;
{
	register u_int i = *dp++;
	register u_char *cp = (u_char *)dp;
	register char c;

	i = ntohl(i);
	if (i == 0) {	/* otherwise we decrement to MAXINT before testing i */
	    putchar('"');
    	    putchar('"');
	    return;
	}
	if (i < 64) {	/* sanity */
		putchar('"');
		do {
			c = toascii(*cp++);
			if (!isascii(c)) {
				c = toascii(c);
				putchar('M');
				putchar('-');
			}
			if (!isprint(c)) {
				c ^= 0x40; /* DEL to ?, others to alpha */
				putchar('^');
			}
			putchar(c);
		} while (--i);
		putchar('"');
	}
}

#if	BYTE_ORDER == LITTLE_ENDIAN
/* assumes input is long-aligned and N*sizeof(long) bytes */
static bswap(bp, len)
long *bp;
int len;
{
	int i;
	len = len/sizeof(long);

	for (i = 0; i < len; i++)
		bp[i] = ntohl(bp[i]);
}
#endif
