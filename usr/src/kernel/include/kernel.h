/*-
 * Copyright (c) 1990 The Regents of the University of California.
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
 *
 *	$Id$
 */

/* Global variables for the kernel. */

/* 1.1 */
extern char hostname[MAXHOSTNAMELEN];
extern int hostnamelen;

/* 1.2 */
extern struct timeval boottime;
extern struct timeval time;
extern struct timezone tz;			/* XXX */

/*extern int hz;					/* clock frequency */
extern int phz;				/* alternate clock's frequency */
extern int tick;

extern fixpt_t	averunnable[3];

#ifdef GPROF
extern u_long s_textsize;
extern int profiling;
extern u_short *kcount;
extern char *s_lowpc;
#endif

/* interface symbols */
#define	__ISYM_VERSION__ "1"	/* XXX RCS major revision number of hdr file */
#include "isym.h"		/* this header has interface symbols */

/* global variables used in core kernel and other modules */
__ISYM__(int, hz,)		/* clock frequency */
__ISYM__(int, lbolt,)		/* once a second sleep address */
__ISYM__(long, hostid,)

#undef __ISYM__
#undef __ISYM_ALIAS__
#undef __ISYM_VERSION__
