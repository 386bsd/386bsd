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
 *	$Id: malloc.c,v 1.1 94/10/20 00:03:04 bill Exp $
 */

#include "sys/param.h"
#include "sys/errno.h"
#include "proc.h"
#include "malloc.h"
#include "vm.h"
#include "kmem.h"

#include "prototypes.h"

struct kmembuckets bucket[MINBUCKET + 16];
struct kmemstats kmemstats[M_LAST];
struct kmemusage *kmemusage;
char *kmembase, *kmemlimit;
char *memname[] = INITKMEMNAMES;

/*
 * Allocate a block of memory
 */
void *
malloc(u_long size, int type, int flags)
{
	struct kmembuckets *kbp;
	struct kmemusage *kup;
	long indx, npg, alloc, allocsize;
	int s;
	caddr_t va, cp, savedlist;
#ifdef KMEMSTATS
	struct kmemstats *ksp = &kmemstats[type];

	if (((unsigned long)type) > M_LAST)
		panic("malloc - bogus type");
#endif

	indx = BUCKETINDX(size);
	kbp = &bucket[indx];
	s = splimp();
#ifdef KMEMSTATS
	while (ksp->ks_memuse >= ksp->ks_limit) {
		if (flags & M_NOWAIT) {
			splx(s);
			return ((void *) NULL);
		}
		if (ksp->ks_limblocks < 65535)
			ksp->ks_limblocks++;
		tsleep((caddr_t)ksp, PSWP+2, memname[type], 0);
	}
#endif
	if (kbp->kb_next == NULL) {
		if (size >= MAXALLOCSAVE)
			allocsize = roundup(size, CLBYTES);
		else
			allocsize = 1 << indx;
		npg = clrnd(btoc(allocsize));
		va = (caddr_t) kmem_alloc(kmem_map, (vm_size_t)ctob(npg),
					   flags);
		if (va == NULL) {
			splx(s);
			return ((void *) NULL);
		}
#ifdef KMEMSTATS
		kbp->kb_total += kbp->kb_elmpercl;
#endif
		kup = btokup(va);
		kup->ku_indx = indx;
		if (allocsize >= MAXALLOCSAVE) {
			if (npg > 65535)
				panic("malloc: allocation too large");
			kup->ku_pagecnt = npg;
#ifdef KMEMSTATS
			ksp->ks_memuse += allocsize;
#endif
			goto out;
		}
#ifdef KMEMSTATS
		kup->ku_freecnt = kbp->kb_elmpercl;
		kbp->kb_totalfree += kbp->kb_elmpercl;
#endif
		/*
		 * Just in case we blocked while allocating memory,
		 * and someone else also allocated memory for this
		 * bucket, don't assume the list is still empty.
		 */
		savedlist = kbp->kb_next;
		kbp->kb_next = va + (npg * NBPG) - allocsize;
		for (cp = kbp->kb_next; cp > va; cp -= allocsize)
			*(caddr_t *)cp = cp - allocsize;
		*(caddr_t *)cp = savedlist;
	}
	va = kbp->kb_next;
	kbp->kb_next = *(caddr_t *)va;
#ifdef KMEMSTATS
	kup = btokup(va);
	if (kup->ku_indx != indx)
		panic("malloc: wrong bucket");
	if (kup->ku_freecnt == 0)
		panic("malloc: lost data");
	kup->ku_freecnt--;
	kbp->kb_totalfree--;
	ksp->ks_memuse += 1 << indx;
out:
	kbp->kb_calls++;
	ksp->ks_inuse++;
	ksp->ks_calls++;
	if (ksp->ks_memuse > ksp->ks_maxused)
		ksp->ks_maxused = ksp->ks_memuse;
#else
out:
#endif /* KMEMSTATS */
	splx(s);

	/* clear memory? */
	if (flags & M_ZERO_IT)
		memset((void *)va, 0, size);

	return ((void *) va);
}

#ifdef DIAGNOSTIC
long addrmask[] = { 0x00000000,
	0x00000001, 0x00000003, 0x00000007, 0x0000000f,
	0x0000001f, 0x0000003f, 0x0000007f, 0x000000ff,
	0x000001ff, 0x000003ff, 0x000007ff, 0x00000fff,
	0x00001fff, 0x00003fff, 0x00007fff, 0x0000ffff,
};
#endif /* DIAGNOSTIC */

/*
 * Free a block of memory allocated by malloc.
 */
void
free(void *addr, int type)
{
	register struct kmembuckets *kbp;
	register struct kmemusage *kup;
	long alloc, size;
	int s;
	extern int end;
#ifdef KMEMSTATS
	register struct kmemstats *ksp = &kmemstats[type];
#endif /* KMEMSTATS */

	if (addr < (void *)&end ||
	   (vm_offset_t)addr < vm_map_min(kmem_map) ||
	   (vm_offset_t)addr > vm_map_max(kmem_map)) {
		panic("free: outside arena");
	}

	kup = btokup(addr);
	size = 1 << kup->ku_indx;
#ifdef DIAGNOSTIC
	if (size > NBPG * CLSIZE)
		alloc = addrmask[BUCKETINDX(NBPG * CLSIZE)];
	else
		alloc = addrmask[kup->ku_indx];
	if (((u_long)addr & alloc) != 0)
		panic("free: unaligned addr");
#endif /* DIAGNOSTIC */
	kbp = &bucket[kup->ku_indx];
	s = splimp();
	if (size >= MAXALLOCSAVE) {
		kmem_free(kmem_map, (vm_offset_t)addr, ctob(kup->ku_pagecnt));
#ifdef KMEMSTATS
		size = kup->ku_pagecnt << PGSHIFT;
		ksp->ks_memuse -= size;
		kup->ku_indx = 0;
		kup->ku_pagecnt = 0;
		if (ksp->ks_memuse + size >= ksp->ks_limit &&
		    ksp->ks_memuse < ksp->ks_limit)
			wakeup((caddr_t)ksp);
		ksp->ks_inuse--;
		kbp->kb_total -= 1;
#endif /* KMEMSTATS */
		splx(s);
		return;
	}
#ifdef KMEMSTATS
	kbp->kb_totalfree++;
	kup->ku_freecnt++;
	if (kup->ku_freecnt >= kbp->kb_elmpercl)
		if (kup->ku_freecnt > kbp->kb_elmpercl)
			panic("free: multiple frees");
		else if (kbp->kb_totalfree > kbp->kb_highwat && type != M_MBUF) {
			caddr_t freepage = kuptob(kup);
			register caddr_t *cpp, *nxt;
			int freesize = kup->ku_freecnt * size;

			ksp->ks_memuse -= size;
			kup->ku_indx = 0;
			if (ksp->ks_memuse + size >= ksp->ks_limit &&
				ksp->ks_memuse < ksp->ks_limit)
				wakeup((caddr_t)ksp);
			ksp->ks_inuse--;
			kbp->kb_total -= kbp->kb_elmpercl;
			kbp->kb_totalfree -= kbp->kb_elmpercl;

			if (--kup->ku_freecnt != 0) {

			/* walk bucket list deleting entries in page freed */
			for (cpp = &kbp->kb_next; *cpp != NULL ; ) {
				nxt = *(caddr_t **)cpp;
				if ((caddr_t)nxt >= freepage
				    && (caddr_t)nxt < freepage + freesize) {
					while (*nxt >= freepage
					    && *nxt < freepage + freesize) {
						nxt = *(caddr_t **)nxt;
						kup->ku_freecnt--;
					}
					*cpp = *(caddr_t *) nxt;
					kup->ku_freecnt--;
				} else if (nxt == NULL)
					break;
				else
					cpp = nxt;
			}
			if (kup->ku_freecnt != 0)
				panic("free: missing a bucket");
			}
			kmem_free(kmem_map, (vm_offset_t)freepage, freesize);
			splx(s);
			return;
		}
	ksp->ks_memuse -= size;
	if (ksp->ks_memuse + size >= ksp->ks_limit &&
	    ksp->ks_memuse < ksp->ks_limit)
		wakeup((caddr_t)ksp);
	ksp->ks_inuse--;
#endif	/* KMEMSTATS */
	*(caddr_t *)addr = kbp->kb_next;
	kbp->kb_next = addr;
	splx(s);
}

/*
 * Initialize the kernel memory allocator
 */
void
kmeminit(void)
{
	long indx;
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
	npg = VM_KMEM_SIZE / NBPG;
	kmemusage = (struct kmemusage *) kmem_alloc(kernel_map,
		(vm_size_t)(npg * sizeof(struct kmemusage)), 0);
	memset((caddr_t)kmemusage, 0, npg * sizeof(struct kmemusage));
	kmem_map = kmem_suballoc(/*kernel_map, (vm_offset_t *)&kmembase,
		(vm_offset_t *)&kmemlimit,*/ (vm_size_t)(npg * NBPG), FALSE);
	kmembase = (caddr_t) vm_map_min(kmem_map);
	kmemlimit = (caddr_t) vm_map_max(kmem_map);
#ifdef KMEMSTATS
	for (indx = 0; indx < MINBUCKET + 16; indx++) {
		if (1 << indx >= CLBYTES)
			bucket[indx].kb_elmpercl = 1;
		else
			bucket[indx].kb_elmpercl = CLBYTES / (1 << indx);
		bucket[indx].kb_highwat = 5 * bucket[indx].kb_elmpercl;
	}
	for (indx = 0; indx < M_LAST; indx++)
		kmemstats[indx].ks_limit = npg * NBPG * 6 / 10;
#endif
}
