/*
 * Copyright (c) 1987, 1991 The Regents of the University of California.
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
 *	from:	@(#)kern_malloc.c	7.25 (Berkeley) 5/8/91
 *	386BSD:	$Id
 */

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/time.h>
#include "malloc.h"
#include <stdio.h>

struct kmembuckets bucket[MINBUCKET + 28];
struct kmemusage *kmemusage;
caddr_t kmembase, kmemlimit;

static init(void);
static panic(char *s);

/*
 * Allocate a block of memory
 */
void *
malloc(unsigned long size)
{
	register struct kmembuckets *kbp;
	register struct kmemusage *kup;
	long indx, npg, alloc, allocsize;
	caddr_t va, cp, savedlist;
#ifdef MSTAT
	struct timeval tv, otv;
#endif MSTAT

	if (kmemusage == 0)
		init();
/* printf("malloc: size %d", size); */
size += 8; /* groan XXX */
#ifdef MSTAT
	gettimeofday(&otv, 0);
#endif
	indx = BUCKETINDX(size);
	kbp = &bucket[indx];
/* printf(" indx %d", indx); */
	if (kbp->kb_next == NULL) {
		if (size >= MAXALLOCSAVE)
			allocsize = roundup(size, CLBYTES);
		else
			allocsize = 1 << indx;
		npg = clrnd(btoc(allocsize));
/* printf(" npg %d", npg); */
		va = mmap(0, ctob(npg), (PROT_READ|PROT_WRITE),
			(MAP_ANON|MAP_INHERIT), -1, 0);

		if ((int) va == -1) {
/* printf(" fail\n"); */
			return ((void *) NULL);
		}
		if (va > kmemlimit)
			kmemlimit = va;
		kbp->kb_total += kbp->kb_elmpercl;
/* printf(" total %d", kbp->kb_total); */
		kup = btokup(va);
/* printf(" kup %x", kup); */
		if (kup->ku_indx != 0 || kup->ku_pagecnt != 0)
			panic ("bucket not free");
		kup->ku_indx = indx;
		if (allocsize >= MAXALLOCSAVE) {
			kup->ku_pagecnt = npg;
/* printf(" pagecnt %d", kup->ku_pagecnt); */
			goto out;
		}
		kup->ku_freecnt = kbp->kb_elmpercl;
/* printf(" freecnt %d", kup->ku_freecnt); */
		kbp->kb_totalfree += kbp->kb_elmpercl;
/* printf(" totalfree %d", kbp->kb_totalfree); */
		/*
		 * Just in case we blocked while allocating memory,
		 * and someone else also allocated memory for this
		 * bucket, don't assume the list is still empty.
		 */
		savedlist = kbp->kb_next;
		kbp->kb_next = va + (npg * NBPG) - allocsize;
		for (cp = kbp->kb_next; cp > va; cp -= allocsize)
{
			*(caddr_t *)cp = cp - allocsize;
/* printf(" list %x", *(int *)cp); */
}
		*(caddr_t *)cp = savedlist;
/*printf(" list %x", *(int *)cp);*/
	}
	va = kbp->kb_next;
	kbp->kb_next = *(caddr_t *)va;
(int) va += 8; /* groan XXX */
/* printf(" nxt %x", *(int *)va); */
	kup = btokup(va);
	if (kup->ku_indx != indx)
		panic("malloc: wrong bucket");
	if (kup->ku_freecnt == 0)
		panic("malloc: lost data");
	kup->ku_freecnt--;
/* printf(" freecnt %d", kup->ku_freecnt); */
	kbp->kb_totalfree--;
/* printf(" totalfree %d", kbp->kb_totalfree); */
out:
/* printf(" rtn %x\n", va); */
	kbp->kb_calls++;
#ifdef MSTAT
	gettimeofday(&tv, 0);
	kbp->kb_alloctime += (tv.tv_sec - otv.tv_sec) * 1000000
				+ tv.tv_usec - otv.tv_usec;
#endif

	return ((void *) va);
}

static long addrmask[] = { 0x00000000,
	0x00000001, 0x00000003, 0x00000007, 0x0000000f,
	0x0000001f, 0x0000003f, 0x0000007f, 0x000000ff,
	0x000001ff, 0x000003ff, 0x000007ff, 0x00000fff,
	0x00001fff, 0x00003fff, 0x00007fff, 0x0000ffff,
	0x0001ffff, 0x0003ffff, 0x0007ffff, 0x000fffff,
	0x001fffff, 0x003fffff, 0x007fffff, 0x00ffffff,
	0x01ffffff, 0x03ffffff, 0x07ffffff, 0x0fffffff,
	0x1fffffff, 0x3fffffff, 0x7fffffff, 0xffffffff,
};

/*
 * Free a block of memory allocated by malloc.
 */
void
free(addr)
	void *addr;
{
	register struct kmembuckets *kbp;
	register struct kmemusage *kup;
	long alloc, size;
#ifdef	MSTAT
	struct timeval tv, otv;
#endif

/* printf("free: %x", addr); */
	if (addr == NULL)
		return;
(int) addr -= 8; /* groan XXX */
	/* are we memory allocated by the malloc() allocator? */
	if ((caddr_t)addr <  kmembase || (caddr_t)addr > kmemlimit)
		return;
	kup = btokup(addr);
/* printf(" kup %x", kup); */
	size = 1 << kup->ku_indx;
/* printf(" size %d", size); */
	if (size > NBPG * CLSIZE)
		alloc = addrmask[BUCKETINDX(NBPG * CLSIZE)];
	else
		alloc = addrmask[kup->ku_indx];
	if (((u_long)addr & alloc) != 0) {
#ifdef DIAGNOSTIC
		fprintf(stderr, "free: unaligned addr 0x%x, size %d, mask %d\n",
			addr, size, alloc);
		panic("free: unaligned addr");
#else
		return;
#endif /* DIAGNOSTIC */
	}

	/* looks valid, lets free it */
#ifdef	MSTAT
	gettimeofday(&otv, 0);
#endif
	kbp = &bucket[kup->ku_indx];
/* printf(" kbp %x", kbp); */

	/* integral page cluster bucket deallocation */
	if (size >= MAXALLOCSAVE) {
		if (kup->ku_pagecnt == 0)
			panic("already freed");
/* printf(" pagecnt %d unmap", kup->ku_pagecnt); */
		if (munmap((caddr_t) addr, (size_t) ctob(kup->ku_pagecnt)) < 0)
			panic("can't free");
		size = ctob(kup->ku_pagecnt);
/* printf(" size %d", size); */
		kup->ku_indx = 0;
		kup->ku_pagecnt = 0;
		kbp->kb_total--;
/* printf(" total %d\n", kbp->kb_total); */
#ifdef	MSTAT
		gettimeofday(&tv, 0);
		kbp->kb_freetime += (tv.tv_sec - otv.tv_sec) * 1000000
			+ tv.tv_usec - otv.tv_usec;
#endif
		return;
	}

	/* fractional page bucket deallocation */
	kbp->kb_totalfree++;
/* printf(" totalfree %d", kbp->kb_totalfree); */
	kup->ku_freecnt++;
/* printf(" freecnt %d", kup->ku_freecnt); */

	/* all fractions of a bucket deallocated? */
	if (kup->ku_freecnt >= kbp->kb_elmpercl)
		if (kup->ku_freecnt > kbp->kb_elmpercl)
			panic("free: multiple frees");
		/* should we return space to the kernel? */
		else if (kbp->kb_totalfree > kbp->kb_highwat) {
			caddr_t freepage = kuptob(kup);
			register caddr_t *cpp, *nxt;
			int freesize = kup->ku_freecnt * size;

			kup->ku_indx = 0;
			kbp->kb_total -= kbp->kb_elmpercl;
/* printf(" total %d", kbp->kb_total); */
			kbp->kb_totalfree -= kbp->kb_elmpercl;
/* printf(" totalfree %d", kbp->kb_totalfree); */

			/* are there other fragments on the bucket's free list */
			if (--kup->ku_freecnt != 0) {

/* printf(" freecnt:release %d", kup->ku_freecnt); */
			/*
			 * Hard part: walk the bucket's free list deleting
			 * the entries in the page freed
			 */
			for (cpp = &kbp->kb_next; *cpp != NULL ; ) {
				nxt = *(caddr_t **)cpp;
				/* is the nxt one a free bucket? */
				if ((caddr_t)nxt >= freepage
					&& (caddr_t)nxt < freepage + freesize) {
					/* do we have adjacent free bkts? */
					while (*nxt >= freepage
						&& *nxt < freepage + freesize) {
						nxt = *(caddr_t **)nxt;
						kup->ku_freecnt--;
					}
					/* update list pointer in prev bkt */
					*cpp = *(caddr_t *) nxt;
					kup->ku_freecnt--;
				}
				/* or are we at the end of the list? */
				else if (nxt == NULL)
					break;
				/* otherwise, just cdr down the list */
				else
					cpp = nxt;
			}
			if (kup->ku_freecnt != 0) {
				panic("free: missing a bucket");
			}
			}
/* printf(" unmap %x %d\n", freepage, freesize); */
			if (munmap((caddr_t) freepage, (size_t) freesize) < 0)
				panic("can't free");
#ifdef	MSTAT
			gettimeofday(&tv, 0);
			kbp->kb_freetime += (tv.tv_sec - otv.tv_sec) * 1000000
				+ tv.tv_usec - otv.tv_usec;
#endif
			return;
		}
/* printf(" tail %x\n", kbp->kb_next); */
	*(caddr_t *)addr = kbp->kb_next;
	kbp->kb_next = addr;
#ifdef MSTAT
	gettimeofday(&tv, 0);
	kbp->kb_freetime += (tv.tv_sec - otv.tv_sec) * 1000000
				+ tv.tv_usec - otv.tv_usec;
#endif
}

extern void domem();
/*
 * Initialize the kernel memory allocator
 */
static
init(void)
{
	register long indx;
	int npg;

#if	((MAXALLOCSAVE & (MAXALLOCSAVE - 1)) != 0)
		ERROR!_kmeminit:_MAXALLOCSAVE_not_power_of_2
#endif
#if	(MAXALLOCSAVE > MINALLOCSIZE * 32768)
		ERROR!_kmeminit:_MAXALLOCSAVE_too_big
#endif
#if	(MAXALLOCSAVE < CLBYTES)
		ERROR!_kmeminit:_MAXALLOCSAVE_too_small
#endif
/* determine max size of malloc by obtaining rlimit datasize
(q: do we care if data limit is raised/lowered), or from
rss size (should we jettision brk/sbrk, or "emulate" it by
turning it into a malloc of a fake break region -- fucks badly
gnuemacs and anything that uses unexec(). to unexec you'd
need multiple segments at different addresses and sizes
(malloc could emulate brk by allocating all malloc va space
possible, locating brk there, and unallocating malloc space
all done at crt0 time if brk/sbrk symbol present. could also
leave "old" malloc/break in a -lcompat library.

N.B. current execve allocates a 6MB reserved data segment, so
mmap(0)'s fall after this, outside of data limit.

txt | data | kmem_usage | <max malloc> | break
			 (sized by maxrss)
*/

/* atexit(domem); */
	/* kmemusage = (struct kmemusage *) kmem_alloc(
		npg * sizeof(struct kmemusage))); */
	npg = 1000 ; /* XXX */
	npg = clrnd(btoc(npg * sizeof(struct kmemusage) ));
	kmemusage = (struct kmemusage *)
		mmap(0, ctob(npg), (PROT_READ|PROT_WRITE),
			(MAP_ANON|MAP_INHERIT), -1, 0);
	kmembase = (caddr_t) kmemusage + ctob(npg);
	for (indx = 0; indx < MINBUCKET + 28; indx++) {
		if (1 << indx >= CLBYTES)
			bucket[indx].kb_elmpercl = 1;
		else
			bucket[indx].kb_elmpercl = CLBYTES / (1 << indx);
		bucket[indx].kb_highwat = 5 * bucket[indx].kb_elmpercl;
	}
/* setvbuf(stdout, 0, _IONBF, 0); */
}

static
panic(char *s) {
	write(2, "panic: ", 7);
	write(2, s, strlen(s));
	write(2, "\n", 1);
	abort();
}

void *
realloc(void *p, unsigned long nsize) {
	void *n;
	register struct kmemusage *kup;
	long alloc, osize;

	if (p) {
		/* are we memory allocated by the malloc() allocator? */
		if ((caddr_t)p <  kmembase || (caddr_t)p > kmemlimit)
			goto skip;
		kup = btokup(p);
		osize = 1 << kup->ku_indx;
		if (osize > NBPG * CLSIZE)
			alloc = addrmask[BUCKETINDX(NBPG * CLSIZE)];
		else
			alloc = addrmask[kup->ku_indx];
		if (((u_long)p & alloc) != 0) {
#ifdef DIAGNOSTIC
			fprintf(stderr, "realloc: unaligned addr 0x%x, size %d, mask %d\n",
				p, osize, alloc);
			panic("free: unaligned addr");
#else
			goto skip;
#endif /* DIAGNOSTIC */
		}

		/* integral page cluster bucket? */
		if (osize >= MAXALLOCSAVE) {
			if (kup->ku_pagecnt == 0)
				panic("already freed");
			osize = ctob(kup->ku_pagecnt);
		}

		/* if there would be no change in bucket size, don't bother */
		if (nsize <= osize && nsize > osize/2)
			return (p);
	}

skip:
	if (nsize)
		n = malloc(nsize);
	if (p && n && nsize)
		bcopy (p, n, osize > nsize ? nsize : osize);
	if (p)
		free(p);
	return (n);
}

#ifdef MSTAT
void
domem()
{
	register struct kmembuckets *kp;
	/* register struct kmemstats *ks; */
	register int i;
	int size;
	long totuse = 0, totfree = 0, totreq = 0, tottime = 0, totftime = 0;
	/* struct kmemstats kmemstats[M_LAST];
	struct kmembuckets buckets[MINBUCKET + 16]; */
	struct timeval otv, tv;
	long tovh;

	gettimeofday(&otv, 0);
	gettimeofday(&tv, 0);
	tovh = (tv.tv_sec - otv.tv_sec) * 1000000 + tv.tv_usec - otv.tv_usec;

	(void)fprintf(stderr, "Memory statistics by bucket size\n");
	(void)fprintf(stderr,
	    "    Size   In Use   Free   Requests  HighWater  Allocation time(usec)\n");
	for (i = MINBUCKET, kp = &bucket[i]; i < MINBUCKET + 16; i++, kp++) {
		if (kp->kb_calls == 0)
			continue;
		size = 1 << i;
		(void)fprintf(stderr, "%8d %8ld %6ld %10ld %7ld %10ld", size, 
			kp->kb_total - kp->kb_totalfree,
			kp->kb_totalfree, kp->kb_calls,
			kp->kb_highwat,
			kp->kb_alloctime/kp->kb_calls - tovh);
		if (kp->kb_calls - (kp->kb_total - kp->kb_totalfree) > 0)
			(void)fprintf(stderr, " %10ld\n",
			kp->kb_freetime / (kp->kb_calls - (kp->kb_total - kp->kb_totalfree)) - tovh);
		else
			(void)fprintf(stderr, "\n" );
		totfree += size * kp->kb_totalfree;
		totuse += size * (kp->kb_total - kp->kb_totalfree);
		totreq += kp->kb_calls;
		tottime += kp->kb_alloctime/kp->kb_calls - tovh;
	}

/*	(void)printf("\nMemory statistics by type\n");
	(void)printf(
"      Type  In Use  MemUse   HighUse  Limit Requests  TypeLimit KernLimit\n");
	for (i = 0, ks = &kmemstats[0]; i < M_LAST; i++, ks++) {
		if (ks->ks_calls == 0)
			continue;
		(void)printf("%10s %6ld %7ldK %8ldK %5ldK %8ld %6u %9u\n",
		    kmemnames[i] ? kmemnames[i] : "undefined",
		    ks->ks_inuse, (ks->ks_memuse + 1023) / 1024,
		    (ks->ks_maxused + 1023) / 1024,
		    (ks->ks_limit + 1023) / 1024, ks->ks_calls,
		    ks->ks_limblocks, ks->ks_mapblocks);
		totuse += ks->ks_memuse;
		totreq += ks->ks_calls;
	} */
	(void)fprintf(stderr, "\nMemory Totals:  In Use    Free    Requests     Allocation time(usec)\n");
	(void)fprintf(stderr, "              %7ldK %6ldK    %8ld   %8ld\n",
	     (totuse + 1023) / 1024, (totfree + 1023) / 1024, totreq, tottime);
}
#endif
