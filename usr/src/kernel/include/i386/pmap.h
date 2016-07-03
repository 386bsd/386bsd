/* 
 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and William Jolitz of UUNET Technologies Inc.
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
 *	from:	@(#)pmap.h	7.4 (Berkeley) 5/12/91
 *	386BSD:	$Id: pmap.h,v 1.2 93/08/05 17:03:26 bill Exp Locker: root $
 */

/*
 * Derived from hp300 version by Mike Hibler, this version by William
 * Jolitz uses a recursive map [a pde points to the page directory] to
 * map the page tables using the pagetables themselves. This is done to
 * reduce the impact on kernel virtual memory for lots of sparse address
 * space, and to reduce the cost of memory to each process.
 *
 * from hp300:	@(#)pmap.h	7.2 (Berkeley) 12/16/90
 */

#ifndef	_PMAP_MACHINE_
#define	_PMAP_MACHINE_

/*
 * 386 page table entry and page table directory
 * W.Jolitz, 8/89
 */

#ifndef LOCORE
struct pde
{
unsigned int	
		pd_v:1,			/* valid bit */
		pd_prot:2,		/* access control */
		pd_mbz1:2,		/* reserved, must be zero */
		pd_u:1,			/* hardware maintained 'used' bit */
		:1,			/* not used */
		pd_mbz2:2,		/* reserved, must be zero */
		:3,			/* reserved for software */
		pd_pfnum:20;		/* physical page frame number of pte's*/
};
#endif

#define	PD_MASK		0xffc00000	/* page directory address bits */
#define	PT_MASK		0x003ff000	/* page table address bits */
#define	PD_SHIFT	22		/* page directory address shift */
#define	PG_SHIFT	12		/* page table address shift */

#ifndef LOCORE
struct pte
{
unsigned int	
		pg_v:1,			/* valid bit */
		pg_prot:2,		/* access control */
		/*pg_mbz1:1,		/* reserved, must be zero */
		pg_wt:1,		/* 'write thru' bit */
		pg_cd:1,		/* 'uncacheable page' bit */
		/*pg_mbz1:2,		/* reserved, must be zero */
		pg_u:1,			/* hardware maintained 'used' bit */
		pg_m:1,			/* hardware maintained modified bit */
		pg_mbz2:2,		/* reserved, must be zero */
		pg_w:1,			/* software, wired down page */
		:1,			/* software (unused) */
		/* pg_nc:1,		/* 'uncacheable page' bit */
		:1,			/* software (unused) */
		pg_pfnum:20;		/* physical page frame number */
};
#endif

#define	PG_V		0x00000001
#define	PG_RO		0x00000000
#define	PG_RW		0x00000002
#define	PG_u		0x00000004
#define	PG_PROT		0x00000006 /* all protection bits . */
#define	PG_W		0x00000200
/*#define PG_N		0x00000800 /* Non-cacheable */
#define PG_CD		0x00000010 /* Non-cacheable */
#define PG_WT		0x00000008 /* write thru */
#define	PG_M		0x00000040
#define PG_U		0x00000020
#define	PG_FRAME	0xfffff000

#define	PG_NOACC	0
#define	PG_KR		0x00000000
#define	PG_KW		0x00000002
#define	PG_URKR		0x00000004
#define	PG_URKW		0x00000004
#define	PG_UW		0x00000006

/* Garbage for current bastardized pager that assumes a hp300 */
#define	PG_NV	0
#define	PG_CI	0
/*
 * Page Protection Exception bits
 */

#define PGEX_P		0x01	/* Protection violation vs. not present */
#define PGEX_W		0x02	/* during a Write cycle */
#define PGEX_U		0x04	/* access from User mode (UPL) */

#ifndef LOCORE
typedef struct pde	pd_entry_t;	/* page directory entry */
typedef struct pte	pt_entry_t;	/* page table entry */
#endif

/*
 * One page directory, shared between
 * kernel and user modes.
 */
#define I386_PAGE_SIZE	NBPG
#define I386_PDR_SIZE	NBPDR

#define	KPTDI_LAST	0x3ff		/* last of kernel virtual pde's */
#define	KPTDI_FIRST	(KERNBASE >> PD_SHIFT) /* start of kernel virtual pde's */
#define	PTDPTDI		(KPTDI_FIRST - 1) /* ptd entry that points to ptd! */

/*
 * Current and alternate address space page table maps
 * and directories. These are defined as absolute addresses in locore.
 */
#ifndef LOCORE
#ifdef KERNEL
extern struct pte	PTmap[], APTmap[];
extern struct pde	PTD[], APTD[], PTDpde, APTDpde;

extern int	KernelPTD;	/* physical address of "Idle" state directory */
#endif

/*
 * Virtual address to page table entry and to physical address.
 * Likewise for alternate address space.
 * N.B.: these work recursively, thus vtopte of a pte will give
 * the corresponding pde that in turn maps it.
 */
#define	vtopte(va)	(PTmap + i386_btop(va))
#define	kvtopte(va)	vtopte(va)
#define	ptetov(pt)	(i386_ptob(pt - PTmap)) 
#define	vtophys(va)  (i386_ptob(vtopte(va)->pg_pfnum) | ((int)(va) & PGOFSET))

#define	avtopte(va)	(APTmap + i386_btop(va))
#define	ptetoav(pt)	(i386_ptob(pt - APTmap)) 
#define	avtophys(va)  (i386_ptob(avtopte(va)->pg_pfnum) | ((int)(va) & PGOFSET))

/*
 * Macros to generate page directory/table indicies
 */

#define	pdei(va)	(((va)&PD_MASK)>>PD_SHIFT)
#define	ptei(va)	(((va)&PT_MASK)>>PG_SHIFT)

/*
 * Pmap instance definition.
 */

struct pmap {
	pd_entry_t		*pm_pdir;	/* KVA of page directory */
	boolean_t		pm_ptchanged;	/* page tables changed */
	short			pm_dref;	/* page directory ref count */
	short			pm_count;	/* pmap reference count */
	struct pmap_statistics	pm_stats;	/* pmap statistics */
	long			pm_ptpages;	/* more stats: PT pages */
	vm_object_t		pm_ptobject;	/* pagetable pages in here */
	struct proc		*pm_proc;	/* associated process XXX */
};

typedef struct pmap	*pmap_t;

#ifdef KERNEL
extern pmap_t		kernel_pmap;
#endif

/*
 * Each logical page (vm_page_t) has an entry in the physical to virtual table,
 * pv_table, even if the page is not mapped to a virtual address. If mapped to
 * more than one virtual address or in other virtual address space(s), the
 * entry is the head of a list of address spaces and virtual addresses to
 * which the page is mapped. This is used to locate all instances of address
 * translation mappings of a page so that the can be altered if the state of
 * the page changes (i.e. if reclaimed, or if contents are affected).
 */
typedef struct pv_entry {
	struct pv_entry	*pv_next;	/* next pv_entry */
	pmap_t		pv_pmap;	/* pmap where mapping lies */
	vm_offset_t	pv_va;		/* virtual address for mapping */
	int		pv_flags;	/* flags */
	int		pv_am;		/* address mapping class */
} *pv_entry_t;

#define	PV_ENTRY_NULL	((pv_entry_t) 0)

#define	PV_CI		0x01	/* all entries must be cache inhibited */
#define PV_PTPAGE	0x02	/* entry maps a page table page */

#ifdef	KERNEL

pv_entry_t	pv_table;		/* array of entries, one per page */

#define pa_index(pa)		atop(pa - vm_first_phys)
#define pa_to_pvh(pa)		(&pv_table[pa_index(pa)])

#define	pmap_resident_count(pmap)	((pmap)->pm_stats.resident_count)

#endif	KERNEL
#endif	LOCORE

#endif	_PMAP_MACHINE_
