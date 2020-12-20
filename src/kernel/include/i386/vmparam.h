/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	@(#)vmparam.h	5.9 (Berkeley) 5/12/91
 */

/*
 * Virtual memory implementation specific constants for [34]86/Pentium.
 */

/*
 * Virtual address space assignment:
 * On the 386 architecture, there is a single linear address space that
 * is shared by user and supervisory programs during operation.
 *
 * Within this single space we have divided up portions for use by user
 * and kernel programs. By industry convention (iBCS), the user program
 * is the lower, larger portion, and the kernel is in the remaining upper
 * portion.
 *
 * In our implementation, the boundary between these is redefinable on
 * 4MB boundaries (n.b pentium allows 4MB pages, which may be supported
 * in future versions of this code), and is set relative to the base of
 * the kernel's address space (which is *not* the end of the user program's
 * space).
 */
#if (KERNBASE & (1<<22)) != 0	/* insure boundary is sane */
#error KERNBASE is not a multiple of 4MB
#endif

/*
 * The user program's portion runs from the start of the address space
 * to the start of the pagetable space. N.B. the boundaries are for
 * reserving address space for use in the virtual memory system, and
 * are not * intended to define how the user program arranges or
 * allocates space inside this region of virtual address space.
 */
#define VM_MIN_ADDRESS		((vm_offset_t) 0)		/* user */
#define VM_MAX_ADDRESS		((vm_offset_t) (KERNBASE - (1 << 22)))

/*
 * Immediately above the user program's portion of the address space is
 * the pagetable address space, which allows direct access to the 386's
 * address translation tables of this process.
 */
#define PT_MIN_ADDRESS		VM_MAX_ADDRESS			/* page table */
#define PT_MAX_ADDRESS		((vm_offset_t) PT_MIN_ADDRESS | (PT_MIN_ADDRESS>>10))

/*
 * Above the pagetable space is the kernel address space, which contains
 * the kernel program and it's data structures, among which is the
 * per-process and kernel stack(s). Note that the kernel's address space
 * actually starts below KERNBASE, since it includes the address space
 * associated with the kernel's own static page tables, which are present
 * in each process's address space identically.
 */
#define VM_MIN_KERNEL_ADDRESS	((vm_offset_t) PT_MAX_ADDRESS) /* kernel */
#define VM_MAX_KERNEL_ADDRESS	((vm_offset_t) 0xFFC00000)

/*
 * Finally, following the kernel is the auxillary pagetable space, which
 * can be pointed at other processes address translation tables for locating
 * the physical pages associated in that process's address space.
 */
#define APT_MIN_ADDRESS		VM_MAX_KERNEL_ADDRESS		/* auxillary */
#define APT_MAX_ADDRESS		((vm_offset_t) 0xFFFFF000)

/*
 * User program space assignment:
 * The user program space is currently split into a instruction/data
 * portion that starts at USRTEXT, and a stack portion that ends at
 * USRSTACK. N.B. these constants set the semantics of the user programs
 * address space, and need not obey any address granularity restrictions.
 */
#define	USRTEXT		0
#ifndef USRSTACK
#define USRSTACK	(KERNBASE - (1 << 22))
#else
#if USRSTACK > KERNBASE - (1 << 22)
#error USRSTACK set too large for KERNBASE
#endif
#endif

/*
 * Within the user address space, maximum and default administrative
 * limits are set for program instructions(or text), data, and stack.
 */
#define	MAXTSIZ		(6*1024*1024)		/* max text size */
#ifndef DFLDSIZ
#define	DFLDSIZ		(6*1024*1024)		/* initial data size limit */
#endif
#ifndef MAXDSIZ
#define	MAXDSIZ		(32*1024*1024)		/* max data size */
#endif
#ifndef	DFLSSIZ
#define	DFLSSIZ		(512*1024)		/* initial stack size limit */
#endif
#ifndef	MAXSSIZ
#define	MAXSSIZ		MAXDSIZ			/* max stack size */
#endif

/*
 * Secondary or backing store (swap) allocation:
 * Default sizes of swap allocation chunks (see dmap.h).
 * The actual values may be changed in vminit() based on MAXDSIZ.
 * With MAXDSIZ of 16Mb and NDMAP of 38, dmmax will be 1024.
 */
#define	DMMIN	8 /*32*/		/* smallest swap allocation */
#define	DMMAX	4096			/* largest potential swap allocation */
#define	DMTEXT	1024			/* swap allocation for text */

/*
 * The time for a process to be blocked before being very swappable.
 * This is a number of seconds which the system takes as being a non-trivial
 * amount of real time.  You probably shouldn't change this;
 * it is used in subtle ways (fractions and multiples of it are, that is, like
 * half of a ``long time'', almost a long time, etc.)
 * It is related to human patience and other factors which don't really
 * change over time.
 */
#define	MAXSLP 		20		/* time considered a long term sleeper */

/*
 * A swapped in process is given a small amount of core without being bothered
 * by the page replacement algorithm.  Basically this says that if you are
 * swapped in you deserve some resources.  We protect the last SAFERSS
 * pages against paging and will just swap you out rather than paging you.
 * Note that each process has at least UPAGES+CLSIZE pages which are not
 * paged anyways (this is currently 8+2=10 pages or 5k bytes), so this
 * number just means a swapped in process is given around 25k bytes.
 * Just for fun: current memory prices are 4600$ a megabyte on VAX (4/22/81),
 * so we loan each swapped in process memory worth 100$, or just admit
 * that we don't consider it worthwhile and swap it out to disk which costs
 * $30/mb or about $0.75.
 * { wfj 6/16/89: Retail AT memory expansion $800/megabyte, loan of $17
 *   on disk costing $7/mb or $0.18 (in memory still 100:1 in cost!) }
 * { wfj 7/20/93: Retail ISA memory is $30/megabyte, loan of $1 on disk
 *   costing $1/mb -- no difference in cost savings now.}
 */
#define	SAFERSS		8		/* nominal ``small'' resident set size
					   protected against replacement */

/*
 * Paging thresholds (see vm_sched.c).
 * Strategy of 1/19/85:
 *	lotsfree is 512k bytes, but at most 1/4 of memory
 *	desfree is 200k bytes, but at most 1/8 of memory
 *	minfree is 64k bytes, but at most 1/2 of desfree
 */
#define	LOTSFREE	(512 * 1024)
#define	LOTSFREEFRACT	4
#define	DESFREE		(200 * 1024)
#define	DESFREEFRACT	8
#define	MINFREE		(64 * 1024)
#define	MINFREEFRACT	2

/* virtual sizes (bytes) for various kernel submaps */
#define VM_MBUF_SIZE		(NMBCLUSTERS*MCLBYTES)
#define VM_KMEM_SIZE		(NKMEMCLUSTERS*CLBYTES)
#define VM_PHYS_SIZE		(USRIOSIZE*CLBYTES)
