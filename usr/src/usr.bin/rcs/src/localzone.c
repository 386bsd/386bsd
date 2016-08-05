/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
static char sccsid[] = "@(#)localzone.c	5.1 (Berkeley) 8/15/89";
#endif /* not lint */


#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>

static int lclzon;
localzone()
{
	struct timezone tz;

	if (!lclzon) {
		(void)gettimeofday((struct timeval *)NULL, &tz);
		lclzon = tz.tz_minuteswest;
	}
	return(lclzon);
}
