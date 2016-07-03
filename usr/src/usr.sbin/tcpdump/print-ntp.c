/*
 * Copyright (c) 1988-1990 The Regents of the University of California.
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
 *
 * Format and print ntp packets.
 *	By Jeffrey Mogul/DECWRL
 *	loosely based on print-bootp.c
 */

#ifndef lint
static char rcsid[] =
    "@(#) $Header: print-ntp.c,v 1.5 91/06/09 17:17:17 van Exp $ (LBL)";
#endif

#include <stdio.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <strings.h>
#include <ctype.h>

#include "interface.h"
#include "addrtoname.h"
#include "ntp.h"

/*
 * Print ntp requests
 */
void
ntp_print(bp, length)
	register struct ntpdata *bp;
	int length;
{
	u_char *ep;
	int mode, version, leapind;
	static char rclock[5];

#define TCHECK(var, l) if ((u_char *)&(var) > ep - l) goto trunc

	/* Note funny sized packets */
	if (length != sizeof(struct ntpdata))
		(void)printf(" [len=%d]", length);

	/* 'ep' points to the end of avaible data. */
	ep = (u_char *)snapend;

	TCHECK(bp->status, sizeof(bp->status));

	version = (bp->status & VERSIONMASK) >> 3;
	printf(" v%d", version);

	leapind = bp->status & LEAPMASK;
	switch (leapind) {
	case NO_WARNING:
		break;
	case PLUS_SEC:
		printf(" +1s");
		break;
	case MINUS_SEC:
		printf(" -1s");
		break;
	}

	mode = bp->status & MODEMASK;
	switch (mode) {
	case MODE_UNSPEC:	/* unspecified */
		printf(" unspec");
		break;
	case MODE_SYM_ACT:	/* symmetric active */
		printf(" sym_act");
		break;
	case MODE_SYM_PAS:	/* symmetric passive */
		printf(" sym_pas");
		break;
	case MODE_CLIENT:	/* client */
		printf(" client");
		break;
	case MODE_SERVER:	/* server */
		printf(" server");
		break;
	case MODE_BROADCAST:	/* broadcast */
		printf(" bcast");
		break;
	case MODE_RES1:		/* reserved */
		printf(" res1");
		break;
	case MODE_RES2:		/* reserved */
		printf(" res2");
		break;
	}

	TCHECK(bp->stratum, sizeof(bp->stratum));
	printf(" strat %d", bp->stratum);
	
	TCHECK(bp->ppoll, sizeof(bp->ppoll));
	printf(" poll %d", bp->ppoll);

/*	TCHECK(bp->precision, sizeof(bp->precision)); */ /* not legal */
	printf(" prec %d", bp->precision);

	if (! vflag)
		return;

	TCHECK(bp->distance, sizeof(bp->distance));
	printf(" dist ");
	p_sfix(&bp->distance);

	TCHECK(bp->dispersion, sizeof(bp->dispersion));
	printf(" disp ");
	p_sfix(&bp->dispersion);

	TCHECK(bp->refid, sizeof(bp->refid));
	printf(" ref ");
	/* Interpretation depends on stratum */
	switch (bp->stratum) {
	case UNSPECIFIED:
	case PRIM_REF:
	     strncpy(rclock, (char *)&(bp->refid), 4);
	     rclock[4] = 0;
	     printf(rclock);
	     break;
	case INFO_QUERY:
	     printf("%s INFO_QUERY", ipaddr_string((&(bp->refid))));
	     /* this doesn't have more content */
	     return;
	case INFO_REPLY:
	     printf("%s INFO_REPLY", ipaddr_string((&(bp->refid))));
	     /* this is too complex to be worth printing */
	     return;
	default:
	     printf("%s", ipaddr_string((&(bp->refid))));
	     break;
	}

	TCHECK(bp->reftime, sizeof(bp->reftime));
	printf("@");
	p_ntp_time(&(bp->reftime));

	TCHECK(bp->org, sizeof(bp->org));
	printf(" orig ");
	p_ntp_time(&(bp->org));

	TCHECK(bp->rec, sizeof(bp->rec));
	printf(" rec ");
	p_ntp_delta(&(bp->org), &(bp->rec));

	TCHECK(bp->xmt, sizeof(bp->xmt));
	printf(" xmt ");
	p_ntp_delta(&(bp->org), &(bp->xmt));

	return;

trunc:
	fputs(" [|ntp]", stdout);
#undef TCHECK
}

p_sfix(sfp)
register struct s_fixedpt *sfp;
{
	register int i;
	register int f;
	register float ff;

	i = ntohs(sfp->int_part);
	f = ntohs(sfp->fraction);
	ff = (f / 65536.0);	/* shift radix point by 16 bits */
	f = (ff * 1000000.0);	/* Treat fraction as parts per million */
	printf("%d.%06d", i, f);
}

#define	FMAXINT	(4294967296.0)	/* floating point rep. of MAXINT */

p_ntp_time(lfp)
register struct l_fixedpt *lfp;
{
	register long i;
	register unsigned long uf;
	register unsigned long f;
	register float ff;

	i = ntohl(lfp->int_part);
	uf = ntohl(lfp->fraction);
	ff = uf;
	if (ff < 0.0)		/* some compilers are buggy */
		ff += FMAXINT;
	ff = ff / FMAXINT;	/* shift radix point by 32 bits */
	f = ff * 1000000000.0;	/* treat fraction as parts per billion */
	printf("%lu.%09d", i, f);
}

/* Prints time difference between *lfp and *olfp */
p_ntp_delta(olfp, lfp)
register struct l_fixedpt *olfp;
register struct l_fixedpt *lfp;
{
	register long i;
	register unsigned long uf;
	register unsigned long ouf;
	register unsigned long f;
	register float ff;
	int signbit;

	i = ntohl(lfp->int_part) - ntohl(olfp->int_part);

	uf = ntohl(lfp->fraction);
	ouf = ntohl(olfp->fraction);

	if (i > 0) {	/* new is definitely greater than old */
	    signbit = 0;
	    f = uf - ouf;
	    if (ouf > uf) {	/* must borrow from high-order bits */
		i -= 1;
	    }
	}
	else if (i < 0) {	/* new is definitely less than old */
	    signbit = 1;
	    f = ouf - uf;
	    if (uf > ouf) {	/* must carry into the high-order bits */
	    	i += 1;
	    }
	    i = -i;
	}
	else {			/* int_part is zero */
	    if (uf > ouf) {
		signbit = 0;
		f = uf - ouf;
	    }
	    else {
		signbit = 1;
		f = ouf - uf;
	    }
	}

	ff = f;
	if (ff < 0.0)		/* some compilers are buggy */
		ff += FMAXINT;
	ff = ff / FMAXINT;	/* shift radix point by 32 bits */
	f = ff * 1000000000.0;	/* treat fraction as parts per billion */
	if (signbit)
	    putchar('-');
	else
	    putchar('+');
	printf("%d.%09d", i, f);
}
