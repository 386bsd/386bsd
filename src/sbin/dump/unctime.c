/*-
 * Copyright (c) 1980 The Regents of the University of California.
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
static char sccsid[] = "@(#)unctime.c	5.4 (Berkeley) 3/7/91";
#endif /* not lint */

#include <sys/types.h>
#include <time.h>
#include <stdio.h>
#ifdef __STDC__
#include <stdlib.h>
#include <string.h>
#endif

/*
 * Convert a ctime(3) format string into a system format date.
 * Return the date thus calculated.
 *
 * Return -1 if the string is not in ctime format.
 */

/*
 * Offsets into the ctime string to various parts.
 */

#define	E_MONTH		4
#define	E_DAY		8
#define	E_HOUR		11
#define	E_MINUTE	14
#define	E_SECOND	17
#define	E_YEAR		20

static int lookup();

time_t
unctime(str)
	char *str;
{
	struct tm then;
	char dbuf[30];
	time_t emitl();

	if (strlen(str) != 25)
		str[25] = 0;
	strcpy(dbuf, str);
	dbuf[E_MONTH+3] = 0;
	if ((then.tm_mon = lookup(&dbuf[E_MONTH])) < 0)
		return (-1);
	then.tm_mday = atoi(&dbuf[E_DAY]);
	then.tm_hour = atoi(&dbuf[E_HOUR]);
	then.tm_min = atoi(&dbuf[E_MINUTE]);
	then.tm_sec = atoi(&dbuf[E_SECOND]);
	then.tm_year = atoi(&dbuf[E_YEAR]) - 1900;
	return(emitl(&then));
}

static char months[] =
	"JanFebMarAprMayJunJulAugSepOctNovDec";

static int
lookup(str)
	char *str;
{
	register char *cp, *cp2;

	for (cp = months, cp2 = str; *cp != 0; cp += 3)
		if (strncmp(cp, cp2, 3) == 0)
			return((cp-months) / 3);
	return(-1);
}
/*
 * Routine to convert a localtime(3) format date back into
 * a system format date.
 *
 *	Use a binary search.
 */

struct tm *localtime();
static int dcmp();

time_t
emitl(dp)
	struct tm *dp;
{
	time_t conv;
	register int i, bit;
	struct tm dcopy;

	dcopy = *dp;
	dp = &dcopy;
	conv = 0;
	for (i = 30; i >= 0; i--) {
		bit = 1 << i;
		conv |= bit;
		if (dcmp(localtime(&conv), dp) > 0)
			conv &= ~bit;
	}
	return(conv);
}

/*
 * Compare two localtime dates, return result.
 */

#define DECIDE(a) \
	if (dp->a > dp2->a) \
		return(1); \
	if (dp->a < dp2->a) \
		return(-1)

static int
dcmp(dp, dp2)
	register struct tm *dp, *dp2;
{

	DECIDE(tm_year);
	DECIDE(tm_mon);
	DECIDE(tm_mday);
	DECIDE(tm_hour);
	DECIDE(tm_min);
	DECIDE(tm_sec);
	return(0);
}
