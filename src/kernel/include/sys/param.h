/*-
 * Copyright (c) 1982, 1986, 1989 The Regents of the University of California.
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
 *	$Id: param.h,v 1.2 94/07/26 19:14:36 bill Exp Locker: bill $
 */

#define	__386BSDREL__	1	/* Release 1 */
#define	__386BSDREV__	0	/* Revision 0 (e.g. 1.0) */

#ifndef NULL
#define	NULL	0
#endif

#ifndef LOCORE
#include <sys/types.h>
#endif

#include <sys/syslimits.h>

#ifndef _POSIX_SOURCE
/*
 * BSD compatibility: machine-independent constants (some used in
 * following include files).
 *
 * MAXLOGNAME should be >= UT_NAMESIZE (see <utmp.h>)
 */

#define	MAXCOMLEN	16		/* max command name remembered */
#define	MAXLOGNAME	12		/* max login name length */
#define	NOGROUP		65535		/* marker for empty group set member */
#define MAXHOSTNAMELEN	256		/* max hostname size */

#ifdef nope
#if defined(_386BSD_ONLY) || defined(KERNEL)
/*
 * iBCS2 compatibility: Many of these define obselete portions of the
 * archaic UNIX implementation. Unfortunately, to be compliant the
 * standard calls them out, however, nothing in 386BSD depends on
 * them; the sole presence here is for minimal standards conformance.
 * See Intel iBCS 2 spec, Figure 6-23, page 6-44.
 */

#define MAXPID		30000		/* PID_MAX from proc.h */
#define MAXUID		SHRT_MAX
#define HZ		CLK_TCK
#define TICK		1000000		/* one microsecond resolution */
#define	NCARGS		ARG_MAX
#define NOFILES_MIN	20		/* thank you V7 */
#define NOFILES_MAX	OPEN_MAX
#endif	/* !_386BSD_ONLY */
#endif
#endif	/* !_POSIX_SOURCE */

/* default inclusion of signal definitions */
#include <sys/signal.h>

/* Machine type dependent parameters. */
#include <machine/param.h>
#include <machine/endian.h>
#include <machine/limits.h>

/*
 * Clustering of hardware pages on machines with ridiculously small
 * page sizes is done here.  The paging subsystem deals with units of
 * CLSIZE pte's describing NBPG (from machine/machparam.h) pages each.
 */
#define	CLBYTES		(CLSIZE*NBPG)
#define	CLOFSET		(CLSIZE*NBPG-1)	/* for clusters, like PGOFSET */
#define	claligned(x)	((((int)(x))&CLOFSET)==0)
#define	CLOFF		CLOFSET
#define	CLSHIFT		(PGSHIFT+CLSIZELOG2)

#if CLSIZE==1
#define	clbase(i)	(i)
#define	clrnd(i)	(i)
#else
/* Give the base virtual address (first of CLSIZE). */
#define	clbase(i)	((i) &~ (CLSIZE-1))
/* Round a number of clicks up to a whole cluster. */
#define	clrnd(i)	(((i) + (CLSIZE-1)) &~ (CLSIZE-1))
#endif

/*
 * File system parameters and macros.
 *
 * The file system is made out of blocks of at most MAXBSIZE units, with
 * smaller units (fragments) only in the last direct block.  MAXBSIZE
 * primarily determines the size of buffers in the buffer pool.  It may be
 * made larger without any effect on existing file systems; however making
 * it smaller make make some file systems unmountable.
 */
#define	MAXBSIZE	8192
#define MAXFRAG 	8

/*
 * MAXSYMLINKS defines the maximum number of symbolic links that may be
 * expanded in a path name. It should be set high enough to allow all
 * legitimate uses, but halt infinite loops reasonably quickly.
 */
#define MAXSYMLINKS	8

/* Macros for counting and rounding. */
#ifndef howmany
#define	howmany(x, y)	(((x)+((y)-1))/(y))
#endif
#define	roundup(x, y)	((((x)+((y)-1))/(y))*(y))
#define powerof2(x)	((((x)-1)&(x))==0)

/* Macros for fast min/max: with inline expansion, the "function" is faster. */
#ifdef KERNEL
/*#define	MIN(a,b) min((a), (b))
#define	MAX(a,b) max((a), (b))*/
#else
#define	MIN(a,b) (((a)<(b))?(a):(b))
#define	MAX(a,b) (((a)>(b))?(a):(b))
#endif

/*
 * Constants for setting the parameters of the kernel memory allocator.
 *
 * 2 ** MINBUCKET is the smallest unit of memory that will be
 * allocated. It must be at least large enough to hold a pointer.
 *
 * Units of memory less or equal to MAXALLOCSAVE will permanently
 * allocate physical memory; requests for these size pieces of
 * memory are quite fast. Allocations greater than MAXALLOCSAVE must
 * always allocate and free physical memory; requests for these
 * size allocations should be done infrequently as they will be slow.
 *
 * Constraints: CLBYTES <= MAXALLOCSAVE <= 2 ** (MINBUCKET + 14), and
 * MAXALLOCSIZE must be a power of two.
 */
#define MINBUCKET	4		/* 4 => min allocation of 16 bytes */
#define MAXALLOCSAVE	(2 * CLBYTES)

/*
 * Scale factor for scaled integers used to count %cpu time and load avgs.
 *
 * The number of CPU `tick's that map to a unique `%age' can be expressed
 * by the formula (1 / (2 ^ (FSHIFT - 11))).  The maximum load average that
 * can be calculated (assuming 32 bits) can be closely approximated using
 * the formula (2 ^ (2 * (16 - FSHIFT))) for (FSHIFT < 15).
 *
 * For the scheduler to maintain a 1:1 mapping of CPU `tick' to `%age',
 * FSHIFT must be at least 11; this gives us a maximum load avg of ~1024.
 */
#define	FSHIFT	11		/* bits to right of fixed binary point */
#define FSCALE	(1<<FSHIFT)
