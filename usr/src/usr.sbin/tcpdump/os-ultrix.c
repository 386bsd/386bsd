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
    "@(#) $Header: os-ultrix.c,v 1.2 90/09/21 02:12:07 mccanne Exp $ (LBL)";
#endif

#include <sys/types.h>

#include "os.h"

#ifdef	ETHER_SERVICE

u_char *
ETHER_hostton(name)
	char *name;
{
	u_char *ep;

	ep = (u_char *)malloc(6);
	if (ether_hostton(name, ep) == 0)
		return ep;
	free((char *)ep);
	return 0;
}

char *
ETHER_ntohost(ep)
	u_char *ep;
{
	char buf[128], *cp;

	if (ether_ntohost(buf, ep) == 0) {
		cp = (char *)malloc(strlen(buf) + 1);
		strcpy(cp, buf);
		return cp;
	}
	return 0;
}

#endif	ETHER_SERVICE
