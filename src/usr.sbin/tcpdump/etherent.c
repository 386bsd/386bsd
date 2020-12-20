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
    "@(#) $Header: etherent.c,v 1.2 90/09/20 23:16:06 mccanne Exp $ (LBL)";
#endif

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include "interface.h"

#ifndef ETHER_SERVICE

#include "etherent.h"

/* Hex digit to integer. */
static inline int
xdtoi(c)
{
	if (isdigit(c))
		return c - '0';
	else if (islower(c))
		return c - 'a' + 10;
	else
		return c - 'A' + 10;
}

static inline int
skip_space(f)
	FILE *f;
{
	int c;

	do {
		c = getc(f);
	} while (isspace(c) && c != '\n');

	return c;
}

static inline int
skip_line(f)
	FILE *f;
{
	int c;

	do
		c = getc(f);
	while (c != '\n' && c != EOF);

	return c;
}

struct etherent *
next_etherent(fp)
	FILE *fp;
{
	register int c, d, i;
	char *bp;
	static struct etherent e;
	static int nline = 1;
 top:
	while (nline) {
		/* Find addr */
		c = skip_space(fp);
		if (c == '\n')
			continue;
		/* If this is a comment, or first thing on line
		   cannot be etehrnet address, skip the line. */
		else if (!isxdigit(c))
			c = skip_line(fp);
		else {
			/* must be the start of an address */
			for (i = 0; i < 6; i += 1) {
				d = xdtoi(c);
				c = getc(fp);
				if (c != ':') {
					d <<= 4;
					d |= xdtoi(c);
					c = getc(fp);
				}
				e.addr[i] = d;
				if (c != ':')
					break;
				c = getc(fp);
			}
			nline = 0;
		}
		if (c == EOF)
			return 0;
	}
	
	/* If we started a new line, 'c' holds the char past the ether addr,
	   which we assume is white space.  If we are continuning a line,
	   'c' is garbage.  In either case, we can throw it away. */
	   
	c = skip_space(fp);
	if (c == '\n') {
		nline = 1;
		goto top;
	}
	else if (c == '#') {
		(void)skip_line(fp);
		nline = 1;
		goto top;
	}
	else if (c == EOF)
		return 0;
	
	/* Must be a name. */
	bp = e.name;
	/* Use 'd' to prevent buffer overflow. */
	d = sizeof(e.name) - 1;
	do {
		*bp++ = c;
		c = getc(fp);
	} while (!isspace(c) && c != EOF && --d > 0);
	*bp = '\0';
	if (c == '\n')
		nline = 1;

	return &e;
}

#endif
