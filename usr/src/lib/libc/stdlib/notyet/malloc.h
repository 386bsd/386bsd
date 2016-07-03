/*
 * Copyright (c) 1987 Regents of the University of California.
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
 *	from:	@(#)malloc.h	7.25 (Berkeley) 5/15/91
 *	386BSD:	$Id$
 */

#ifndef _MALLOC_H_
#define	_MALLOC_H_

#include <sys/param.h>

/*
 * Array of descriptors that describe the contents of each page
 */
struct kmemusage {
	short ku_indx;		/* bucket index */
	union {
		u_short freecnt;/* for small allocations, free pieces in page */
		u_short pagecnt;/* for large allocations, pages alloced */
	} ku_un;
};
#define ku_freecnt ku_un.freecnt
#define ku_pagecnt ku_un.pagecnt

/*
 * Set of buckets for each size of memory block that is retained
 */
struct kmembuckets {
	caddr_t kb_next;	/* list of free blocks */
	long	kb_calls;	/* total calls to allocate this size */
	long	kb_total;	/* total number of blocks allocated */
	long	kb_totalfree;	/* # of free elements in this bucket */
	long	kb_elmpercl;	/* # of elements in this sized allocation */
	long	kb_highwat;	/* high water mark */
	long	kb_alloctime;	/* microseconds to do an malloc() request */
	long	kb_freetime;	/* microseconds to do an free() request */
};

#define	MINALLOCSIZE	(1 << MINBUCKET)
#define BUCKETINDX(size) \
	(size) <= (MINALLOCSIZE * (1<<15)) \
		? (size) <= (MINALLOCSIZE * (1<<7)) \
			? (size) <= (MINALLOCSIZE * (1<<3)) \
				? (size) <= (MINALLOCSIZE * (1<<1)) \
					? (size) <= (MINALLOCSIZE * (1<<0)) \
						? (MINBUCKET + 0) \
						: (MINBUCKET + 1) \
					: (size) <= (MINALLOCSIZE * (1<<2)) \
						? (MINBUCKET + 2) \
						: (MINBUCKET + 3) \
				: (size) <= (MINALLOCSIZE * (1<<5)) \
					? (size) <= (MINALLOCSIZE * (1<<4)) \
						? (MINBUCKET + 4) \
						: (MINBUCKET + 5) \
					: (size) <= (MINALLOCSIZE * (1<<6)) \
						? (MINBUCKET + 6) \
						: (MINBUCKET + 7) \
			: (size) <= (MINALLOCSIZE * (1<<11)) \
				? (size) <= (MINALLOCSIZE * (1<<9)) \
					? (size) <= (MINALLOCSIZE * (1<<8)) \
						? (MINBUCKET + 8) \
						: (MINBUCKET + 9) \
					: (size) <= (MINALLOCSIZE * (1<<10)) \
						? (MINBUCKET + 10) \
						: (MINBUCKET + 11) \
				: (size) <= (MINALLOCSIZE * (1<<13)) \
					? (size) <= (MINALLOCSIZE * (1<<12)) \
						? (MINBUCKET + 12) \
						: (MINBUCKET + 13) \
					: (size) <= (MINALLOCSIZE * (1<<14)) \
						? (MINBUCKET + 14) \
						: (MINBUCKET + 15) \
		: /* (size) <= (MINALLOCSIZE * (1<<31)) */ 1 == 1 \
			? (size) <= (MINALLOCSIZE * (1<<23)) \
				? (size) <= (MINALLOCSIZE * (1<<17)) \
					? (size) <= (MINALLOCSIZE * (1<<16)) \
						? (MINBUCKET + 16) \
						: (MINBUCKET + 17) \
					: (size) <= (MINALLOCSIZE * (1<<18)) \
						? (MINBUCKET + 18) \
						: (MINBUCKET + 19) \
				: (size) <= (MINALLOCSIZE * (1<<21)) \
					? (size) <= (MINALLOCSIZE * (1<<20)) \
						? (MINBUCKET + 20) \
						: (MINBUCKET + 21) \
					: (size) <= (MINALLOCSIZE * (1<<22)) \
						? (MINBUCKET + 22) \
						: (MINBUCKET + 23) \
			: /* (size) <= (MINALLOCSIZE * (1<<27)) */ 1 == 1 \
				? (size) <= (MINALLOCSIZE * (1<<25)) \
					? (size) <= (MINALLOCSIZE * (1<<24)) \
						? (MINBUCKET + 24) \
						: (MINBUCKET + 25) \
					: (size) <= (MINALLOCSIZE * (1<<26)) \
						? (MINBUCKET + 26) \
						: (MINBUCKET + 27) \
				: 0
#ifdef whenweget64bits
				: (size) <= (MINALLOCSIZE * (1<<29)) \
					? (size) <= (MINALLOCSIZE * (1<<28)) \
						? (MINBUCKET + 28) \
						: (MINBUCKET + 29) \
					: (size) <= (MINALLOCSIZE * (1<<30)) \
						? (MINBUCKET + 30) \
						: (MINBUCKET + 31)
#endif
/*
 * Turn virtual addresses into kmem map indicies
 */
#define kmemxtob(alloc)	(kmembase + (alloc) * NBPG)
#define btokmemx(addr)	(((caddr_t)(addr) - kmembase) / NBPG)
/* #define btokup(addr)	(&kmemusage[((caddr_t)(addr) - kmembase) >> CLSHIFT]) */
#define btokup(addr)	(kmemusage + (((caddr_t)(addr) - kmembase) >> CLSHIFT))
#define kuptob(kup)	(kmembase + (((kup) - kmemusage) << CLSHIFT))

extern struct kmemusage *kmemusage;
extern caddr_t kmembase;
extern struct kmembuckets bucket[];
extern void *malloc __P((unsigned long size));
extern void free __P((void *addr));
#endif /* !_MALLOC_H_ */
