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
 * $Id: pmap.c,v 1.1 94/10/19 17:40:03 bill Exp $
 */

/*
 * Derived from hp300 version by Mike Hibler, this version by William
 * Jolitz uses a recursive map [a pde points to the page directory] to
 * map the page tables using the pagetables themselves. This is done to
 * reduce the impact on kernel virtual memory for lots of sparse address
 * space, and to reduce the cost of memory to each process.
 *
 *	Derived from: hp300/@(#)pmap.c	7.1 (Berkeley) 12/5/90
 */

/*
 *	Reno i386 version, from Mike Hibler's hp300 version.
 */

/*
 *	Manages physical address maps.
 *
 *	In addition to hardware address maps, this
 *	module is called upon to provide software-use-only
 *	maps which may or may not be stored in the same
 *	form as hardware maps.  These pseudo-maps are
 *	used to store intermediate results from copy
 *	operations to and from address spaces.
 *
 *	Since the information managed by this module is
 *	also stored by the logical address mapping module,
 *	this module may throw away valid virtual-to-physical
 *	mappings at almost any time.  However, invalidations
 *	of virtual-to-physical mappings must be done as
 *	requested.
 *
 *	In order to cope with hardware architectures which
 *	make virtual-to-physical map invalidates expensive,
 *	this module may delay invalidate or reduced protection
 *	operations until such time as they are actually
 *	necessary.  This module is given full information as
 *	to which processors are currently using which maps,
 *	and to when physical maps must be made correct.
 */

#include "sys/param.h"
#include "proc.h"
#include "malloc.h"
#include "sys/user.h"
#include "resourcevar.h"

#include "vm.h"
#include "vmspace.h"
#include "kmem.h"
#include "vm_page.h"
#include "vm_pageout.h"

#include "machine/cpu.h"
/*#include "isa.h"*/
#include "specialreg.h"

#include "prototypes.h"
#define	DEBUG


#if defined(i486)
#define PMAP_INVTLB(pmap, va) \
	if (((pmap) == kernel_pmap || curproc && (pmap) == &curproc->p_vmspace->vm_pmap)) \
		asm volatile ("invlpg (%0) " : : "r" ((va))); \
	else	\
		(pmap)->pm_ptchanged = TRUE;
#else
#define PMAP_INVTLB(pmap, va) \
	if ((cpu_option & CPU_486_INVTLB) \
	    && ((pmap) == kernel_pmap || curproc && (pmap) == &curproc->p_vmspace->vm_pmap)) \
		asm volatile ("invlpg (%0) " : : "r" ((va))); \
	else	\
		(pmap)->pm_ptchanged = TRUE;
#endif
	
#define PMAP_ACTIVATE(pmap, pcb) \
	if ((pmap) != NULL && (pmap)->pm_ptchanged) {  \
		if ((pmap) == kernel_pmap) \
			tlbflush (); \
		else if ((curproc && (pmap) == &curproc->p_vmspace->vm_pmap)) \
			lcr(3, (pcb)->pcb_cr3); \
		(pmap)->pm_ptchanged = FALSE; \
	}

#define PMAP_DEACTIVATE(pmapp, pcbp)

void
tlbflush(void) {

	lcr(3, rcr(3));
}

/*
 * Allocate various and sundry SYSMAPs used in the days of old VM
 * and not yet converted.  XXX.
 */
#define BSDVM_COMPAT	1

extern vm_offset_t pager_sva, pager_eva;
long	atdevbase;	/* XXX */
extern struct vmspace kernspace;

#ifdef DEBUG
struct {
	int kernel;	/* entering kernel mapping */
	int user;	/* entering user mapping */
	int ptpneeded;	/* needed to allocate a PT page */
	int pwchange;	/* no mapping change, just wiring or protection */
	int wchange;	/* no mapping change, just wiring */
	int mchange;	/* was mapped but mapping to different page */
	int managed;	/* a managed page */
	int firstpv;	/* first mapping for this PA */
	int secondpv;	/* second mapping for this PA */
	int ci;		/* cache inhibited */
	int unmanaged;	/* not a managed page */
	int flushes;	/* cache flushes */
} enter_stats;
struct {
	int calls;
	int removes;
	int pvfirst;
	int pvsearch;
	int ptinvalid;
	int uflushes;
	int sflushes;
} remove_stats;

int debugmap = 0;
int pmapdebug = 0 /*xfff*/;
#define PDB_FOLLOW	0x0001
#define PDB_INIT	0x0002
#define PDB_ENTER	0x0004
#define PDB_REMOVE	0x0008
#define PDB_CREATE	0x0010
#define PDB_PTPAGE	0x0020
#define PDB_CACHE	0x0040
#define PDB_BITS	0x0080
#define PDB_COLLECT	0x0100
#define PDB_PROTECT	0x0200
#define PDB_PDRTAB	0x0400
#define PDB_PARANOIA	0x2000
#define PDB_WIRING	0x4000
#define PDB_PVDUMP	0x8000

int pmapvacflush = 0;
#define	PVF_ENTER	0x01
#define	PVF_REMOVE	0x02
#define	PVF_PROTECT	0x04
#define	PVF_TOTAL	0x80
#endif

/*
 * Get PDEs and PTEs for user/kernel address space
 */
#define	pmap_pde(m, v)	(&((m)->pm_pdir[((vm_offset_t)(v) >> PD_SHIFT)&1023]))

#define pmap_pte_pa(pte)	(*(int *)(pte) & PG_FRAME)

#define pmap_pde_v(pte)		((pte)->pd_v)
#define pmap_pte_w(pte)		((pte)->pg_w)
/* #define pmap_pte_ci(pte)	((pte)->pg_ci) */
#define pmap_pte_m(pte)		((pte)->pg_m)
#define pmap_pte_u(pte)		((pte)->pg_u)
#define pmap_pte_v(pte)		((pte)->pg_v)
#define pmap_pte_set_w(pte, v)		((pte)->pg_w = (v))
#define pmap_pte_set_wt(pte, v)		((pte)->pg_wt = (v))
#define pmap_pte_set_cd(pte, v)		((pte)->pg_cd = (v))
#define pmap_pte_set_prot(pte, v)	((pte)->pg_prot = (v))

/*
 * Given a map and a machine independent protection code,
 * convert to a vax protection code.
 */
#define pte_prot(m, p)	(protection_codes[p])
int	protection_codes[8];

pmap_t		kernel_pmap;

vm_offset_t    	avail_start;	/* PA of first available physical page */
vm_offset_t	avail_end;	/* PA of last available physical page */
vm_size_t	mem_size;	/* memory size in bytes */
vm_offset_t	virtual_avail;  /* VA of first avail page (after kernel bss)*/
vm_offset_t	virtual_end;	/* VA of last avail page (end of kernel AS) */
vm_offset_t	vm_first_phys;	/* PA of first managed page */
vm_offset_t	vm_last_phys;	/* PA just past last managed page */
int		i386pagesperpage;	/* PAGE_SIZE / I386_PAGE_SIZE */
boolean_t	pmap_initialized = FALSE;	/* Has pmap_init completed? */
char		*pmap_attributes;	/* reference and modify bits */

void pmap_clear_modify(vm_offset_t pa);
static void i386_protection_init();
/*static */  struct pte *pmap_pte(register struct pmap *pmap, vm_offset_t va);
void
pmap_ptalloc(struct pmap *pmap, pd_entry_t *pde);
static void
pmap_ptfree(struct pmap *pmap, pd_entry_t *pde);
void
pmap_activate(register struct pmap *pmap, struct pcb *pcbp);

#if BSDVM_COMPAT
#include "msgbuf.h"

/*
 * All those kernel PT submaps that BSD is so fond of
 */
struct pte	*CMAP1, *CMAP2, *mmap, *dumpISAmap;
struct pte	*msgbufmap;
caddr_t		CADDR1, CADDR2, vmmap, dumpISA;
struct msgbuf	*msgbufp;
#endif

/*
 *	Bootstrap the system enough to run with virtual memory.
 *	Map the kernel's code and data, and allocate the system page table.
 *
 *	On the I386 this is called after mapping has already been enabled
 *	and just syncs the pmap module with what has already been done.
 *	[We can't call it easily with mapping off since the kernel is not
 *	mapped with PA == VA, hence we would have to relocate every address
 *	from the linked base (virtual) address 0xFE000000 to the actual
 *	(physical) address starting relative to 0]
 */

void
pmap_bootstrap(vm_offset_t firstaddr, vm_offset_t loadaddr)
{
#if BSDVM_COMPAT
	vm_offset_t va;
	struct pte *pte;
#endif
	extern vm_offset_t maxmem, physmem;
	extern int VAKernelPTD;
	struct pte *fpte, *lpte;

	avail_start = firstaddr;
	avail_end = maxmem << PG_SHIFT;

	/* XXX: allow for msgbuf */
	avail_end -= i386_round_page(sizeof(struct msgbuf));

	mem_size = physmem << PG_SHIFT;
	virtual_avail = (vm_offset_t)firstaddr + VM_MIN_KERNEL_ADDRESS;
	virtual_end = VM_MAX_KERNEL_ADDRESS;
	i386pagesperpage = PAGE_SIZE / I386_PAGE_SIZE;

	/*
	 * Initialize protection array.
	 */
	i386_protection_init();

	/* initialize the kernel pmap from the sstatically allocate kernspace */
	kernel_pmap = &kernspace.vm_pmap;
	kernel_pmap->pm_pdir = (pd_entry_t *) VAKernelPTD;
	kernel_pmap->pm_count = 1;

	/* clear all kernel ptes */
	fpte = pmap_pte(kernel_pmap, virtual_avail),
	lpte = pmap_pte(kernel_pmap, virtual_end - NBPG);
	memset((void *)fpte, 0, (void *)lpte - (void *)fpte);


#if BSDVM_COMPAT
	/*
	 * Allocate all the submaps we need
	 */
#define	SYSMAP(c, p, v, n)	\
	v = (c)va; va += ((n)*I386_PAGE_SIZE); p = pte; pte += (n);

	va = virtual_avail;
	pte = pmap_pte(kernel_pmap, va);

	SYSMAP(caddr_t		,CMAP1		,CADDR1	   ,1		)
	SYSMAP(caddr_t		,CMAP2		,CADDR2	   ,1		)
	SYSMAP(caddr_t		,mmap		,vmmap	   ,1		)
	SYSMAP(struct msgbuf *	,msgbufmap	,msgbufp   ,1		)
	SYSMAP(caddr_t		,dumpISAmap	,dumpISA   ,16		)
	virtual_avail = va;
#endif
	/*
	 * reserve special hunk of memory for use by bus dma as a bounce
	 * buffer (contiguous virtual *and* physical memory). for now,
	 * assume vm does not use memory beneath hole, and we know that
	 * the bootstrap uses top 32k of base memory. -wfj
	 */
	{
		extern vm_offset_t isaphysmem;
	isaphysmem = va;

	virtual_avail = pmap_map(va, 0xa0000 - 32*1024, 0xa0000, VM_PROT_ALL);
	}

	*(int *)PTD = 0;
	tlbflush();

}

/*
 *	Initialize the pmap module.
 *	Called by vm_init, to initialize any structures that the pmap
 *	system needs to map virtual memory.
 */
void
pmap_init(void)
{
	vm_offset_t	addr, addr2, phys_start, phys_end;
	vm_size_t	npg, s;
	int		rv;
	extern int KPTphys;

	phys_start = avail_start;
	phys_end = avail_end;
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_init(%x, %x)\n", phys_start, phys_end);
#endif
	/*
	 * Now that kernel map has been allocated, we can mark as
	 * unavailable regions which we have mapped in locore.
	 */
	addr = atdevbase;
	(void) vm_map_insert(kernel_map, NULL, (vm_offset_t) 0,
			   addr, addr+(0x100000-0xa0000));

	addr = (vm_offset_t) KERNBASE+KPTphys;
	(void) vm_map_insert(kernel_map, kernel_object, addr - KERNBASE,
			   addr, addr+2*NBPG);

	/*
	 * Allocate memory for random pmap data structures.  Includes the
	 * pv_head_table and pmap_attributes.
	 */
	npg = atop(phys_end - phys_start);
	s = (vm_size_t) (sizeof(struct pv_entry) * npg + npg);
	s = round_page(s);
	addr = (vm_offset_t) kmem_alloc(kernel_map, s, M_ZERO_IT);
	/*(void) memset ((void *) addr, 0, s); /* sheer paranoia */
	pv_table = (pv_entry_t) addr;
	addr += sizeof(struct pv_entry) * npg;
	pmap_attributes = (char *) addr;
#ifdef DEBUG
	if (pmapdebug & PDB_INIT)
		printf("pmap_init: %x bytes (%x pgs): tbl %x attr %x\n",
		       s, npg, pv_table, pmap_attributes);
#endif

	/*
	 * Now it is safe to enable pv_table recording.
	 */
	vm_first_phys = phys_start;
	vm_last_phys = phys_end;
	pmap_initialized = TRUE;
}

/*
 *	Used to map a range of physical addresses into kernel
 *	virtual address space.
 *
 *	For now, VM is already on, we only need to map the
 *	specified memory.
 */
vm_offset_t
pmap_map(vm_offset_t virt, vm_offset_t start, vm_offset_t end, vm_prot_t prot)
{
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_map(%x, %x, %x, %x)\n", virt, start, end, prot);
#endif
	while (start < end) {
		pmap_enter(kernel_pmap, virt, start, prot, FALSE, AM_NONE);
		virt += PAGE_SIZE;
		start += PAGE_SIZE;
	}
	return(virt);
}

/*
 * Initialize a preallocated and zeroed pmap structure,
 * such as one in a vmspace structure.
 */
void
pmap_pinit(struct pmap *pmap)
{

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_CREATE))
		printf("pmap_pinit(%x)\n", pmap);
#endif
	pmap->pm_count = 1;
	pmap->pm_proc = 0;
}

/*
 *	Retire the given physical map from service.
 *	Should only be called if the map contains
 *	no valid mappings.
 */
void
pmap_destroy(register pmap_t pmap)
{
	int count;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_destroy(%x)\n", pmap);
#endif
	if (pmap == NULL)
		return;

	count = --pmap->pm_count;
	if (count == 0) {
		pmap_release(pmap);
		free((caddr_t)pmap, M_VMPMAP);
	}
}

/*
 * Release any resources held by the given physical map.
 * Called when a pmap initialized by pmap_pinit is being released.
 * Should only be called if the map contains no valid mappings.
 */
void
pmap_release(register struct pmap *pmap)
{

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_release(%x)\n", pmap);
#endif
#ifdef notdef /* DIAGNOSTIC */
	/* count would be 0 from pmap_destroy... */
	if (pmap->pm_count != 1)
		panic("pmap_release count");
#endif
	/* have we a page directory still? */
	if (pmap->pm_pdir)
		kmem_free(kernel_map, (vm_offset_t)pmap->pm_pdir, NBPG);
	
	/* have we any page tables? */
	if (pmap->pm_ptobject) {
		/* free any pages */
		pmap_ptfree(pmap, 0);
		pmap->pm_ptobject->paging_in_progress--;
		vm_object_terminate(pmap->pm_ptobject);
	}
}

/*
 *	Add a reference to the specified pmap.
 */
void
pmap_reference(register struct pmap *pmap)
{
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_reference(%x)", pmap);
#endif
	if (pmap != NULL) {
		pmap->pm_count++;
	}
}

/*
 * Reduce resident set size
 */
static void
pmap_rss(pv_entry_t pv, boolean_t shared) {
	if (pv->pv_pmap != kernel_pmap
	    && (pv->pv_pmap->pm_proc->p_flag & SWEXIT) == 0) {
		struct pstats *ps = pv->pv_pmap->pm_proc->p_stats;
#ifdef DIAGNOSTIC
		if (pfind(pv->pv_pmap->pm_proc->p_pid) != pv->pv_pmap->pm_proc)
			panic("pmap_rss");
#endif
/*		if (shared)
			ps->p_ru.ru_ilrss -= PAGE_SIZE/1024; */

		switch (pv->pv_am) {
		case AM_TEXT:
			ps->p_ru.ru_ixrss -= PAGE_SIZE/1024;
			break;
		case AM_DATA:
			ps->p_ru.ru_idrss -= PAGE_SIZE/1024;
			break;
		case AM_STACK:
			ps->p_ru.ru_isrss -= PAGE_SIZE/1024;
		}

		pv->pv_am = AM_NONE;
		pv->pv_pmap->pm_proc->p_vmspace->vm_rssize =
		    atop((ps->p_ru.ru_ixrss + ps->p_ru.ru_idrss
		        + ps->p_ru.ru_isrss /* - ps->p_ru.ru_ilrss */) * 1024);
	}
}

/*
 *	Remove the given range of addresses from the specified map.
 *
 *	It is assumed that the start and end are properly
 *	rounded to the page size.
 */
void
pmap_remove(struct pmap *pmap, vm_offset_t sva, vm_offset_t eva)
{
	register vm_offset_t pa, va;
	volatile register pt_entry_t *pte;
	register pv_entry_t pv, npv;
	register int ix;
	pd_entry_t *pde;
	int s, bits;
#ifdef DEBUG
	pt_entry_t opte;

	/* asm ("wbinvd");*/
	if (pmapdebug & (PDB_FOLLOW|PDB_REMOVE|PDB_PROTECT))
		printf("pmap_remove(%x, %x, %x)", pmap, sva, eva);
#endif

	if (pmap == NULL || pmap->pm_pdir == 0)
		return;

#ifdef DEBUG
	remove_stats.calls++;
#endif
	pte = 0;
	for (va = sva; va < eva; ) {
		/*
		 * Weed out invalid mappings.
		 * Note: we assume that the page directory table is
	 	 * always allocated, and in kernel virtual.
		 */
		pde = pmap_pde(pmap, va);
		if (!pmap_pde_v(pde)) {
			va += (1<<PD_SHIFT);
			va &= PD_MASK;
			pte = 0;
			continue;
		}

		if (pte == 0) {
			pte = pmap_pte(pmap, va);
			/* if (pte == 0) {
				va += PAGE_SIZE;
				continue;
			} */
		}

		/* look for "runs" of invalid ptes */
		pa = pmap_pte_pa(pte);
		if (pa == 0) {
			pte++;
			va += PAGE_SIZE;
			while (((int)pte & 0xfff) != 0 && *(int *)pte == 0) {
				pte++;
				va += PAGE_SIZE;
				if (va >= eva)
					goto doneearly;
			}
			continue;
		}
#ifdef DEBUG
		opte = *pte;
		remove_stats.removes++;
#endif
		/*
		 * Update statistics
		 */
		if (pmap_pte_w(pte))
			pmap->pm_stats.wired_count--;
		pmap->pm_stats.resident_count--;

		/*
		 * Invalidate the PTEs.
		 * XXX: should cluster them up and invalidate as many
		 * as possible at once.
		 */
#ifdef DEBUG
		if (pmapdebug & PDB_REMOVE)
			printf("remove: inv %x ptes at %x(%x) ",
			       i386pagesperpage, pte, *(int *)pte);
#endif
		bits = ix = 0;
		do {
			bits |= *(int *)pte & (PG_U|PG_M);
			*(int *)pte++ = 0;
			PMAP_INVTLB(pmap, va + ix * I386_PAGE_SIZE);
		} while (++ix != i386pagesperpage);

		/*
		 * Remove from the PV table (raise IPL since we
		 * may be called at interrupt time).
		 */
		if ((pa < vm_first_phys || pa > vm_last_phys)
		    || (va >= pager_sva && va < pager_eva)
		    || (va == (vm_offset_t) vmmap)
		    /*|| (va >= vm_map_min(kmem_map) && va < vm_map_max(kmem_map))*/) {
			va += PAGE_SIZE;
			continue;
		}

		pv = pa_to_pvh(pa);
		s = splimp();
		/*
		 * If it is the first entry on the list, it is actually
		 * in the header and we must copy the following entry up
		 * to the header.  Otherwise we must search the list for
		 * the entry.  In either case we free the now unused entry.
		 */
		if (pmap == pv->pv_pmap && va == pv->pv_va) {
			if (pv->pv_am != AM_NONE)
				pmap_rss(pv, pv->pv_next != NULL);
			npv = pv->pv_next;
			if (npv) {
				*pv = *npv;
#ifdef notyet
				/* if remaining ref of shared page, reduce rss */
				if (pv->pv_next == NULL && pv->pv_am != AM_NONE) {
				struct pstats *ps = pv->pv_pmap->pm_proc->p_stats;
					ps->p_ru.ru_ilrss -= PAGE_SIZE/1024;
				}
#endif
				free((caddr_t)npv, M_VMPVENT);
			} else
				pv->pv_pmap = NULL;
#ifdef DEBUG
			remove_stats.pvfirst++;
#endif
		} else {
			for (npv = pv->pv_next; npv; npv = npv->pv_next) {
#ifdef DEBUG
				remove_stats.pvsearch++;
#endif
				if (pmap == npv->pv_pmap && va == npv->pv_va)
					break;
				pv = npv;
			}
#ifdef DEBUG
			if (npv == NULL)
				panic("pmap_remove: PA not in pv_tab");
#endif

			if (npv->pv_am != AM_NONE)
				pmap_rss(npv, TRUE);
			pv->pv_next = npv->pv_next;
			free((caddr_t)npv, M_VMPVENT);
			pv = pa_to_pvh(pa);
		}

		/*
		 * Update saved attributes for managed page
		 */
		pmap_attributes[pa_index(pa)] |= bits;
		splx(s);
		va += PAGE_SIZE;
	}
doneearly:
	PMAP_ACTIVATE(pmap, (struct pcb *) curproc->p_addr);
#ifdef notdef
[cache flushing, if needed]
#endif
}

/*
 *	Routine:	pmap_remove_all
 *	Function:
 *		Removes this physical page from
 *		all physical maps in which it resides.
 *		Reflects back modify bits to the pager.
 */
void
pmap_remove_all(vm_offset_t pa)
{
	register pv_entry_t pv;
	int s;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_REMOVE|PDB_PROTECT))
		printf("pmap_remove_all(%x)", pa);
	/*pmap_pvdump(pa);*/
#endif
	/*
	 * Not one of ours
	 */
	if (pa < vm_first_phys || pa > vm_last_phys)
		return;

	pv = pa_to_pvh(pa);
	s = splimp();
	/*
	 * Do it the easy way for now
	 */
	while (pv->pv_pmap != NULL) {
#ifdef DEBUG
		if (!pmap_pde_v(pmap_pde(pv->pv_pmap, pv->pv_va)) ||
		    pmap_pte_pa(pmap_pte(pv->pv_pmap, pv->pv_va)) != pa)
			panic("pmap_remove_all: bad mapping");
#endif
		pmap_remove(pv->pv_pmap, pv->pv_va, pv->pv_va + PAGE_SIZE);
	}
	splx(s);
}

/*
 *	Routine:	pmap_copy_on_write
 *	Function:
 *		Remove write privileges from all
 *		physical maps for this physical page.
 */
void
pmap_copy_on_write(vm_offset_t pa)
{
#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_PROTECT))
		printf("pmap_copy_on_write(%x)", pa);
#endif
	pmap_changebit(pa, PG_RO, TRUE);
}

/*
 *	Set the physical protection on the
 *	specified range of this map as requested.
 */
void
pmap_protect(register struct pmap *pmap, vm_offset_t sva, vm_offset_t eva, vm_prot_t prot)
{
	register pt_entry_t *pte;
	register vm_offset_t va;
	register int ix;
	int i386prot;
	boolean_t firstpage = TRUE;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_PROTECT))
		printf("pmap_protect(%x, %x, %x, %x)", pmap, sva, eva, prot);
#endif
	if (pmap == NULL || pmap->pm_pdir == 0)
		return;

	if ((prot & VM_PROT_READ) == VM_PROT_NONE) {
		pmap_remove(pmap, sva, eva);
		return;
	}
	if (prot & VM_PROT_WRITE)
		return;

	pte = 0;
	for (va = sva; va < eva; va += PAGE_SIZE) {
		/*
		 * Page table page is not allocated.
		 * Skip it, we don't want to force allocation
		 * of unnecessary PTE pages just to set the protection.
		 */
		if (!pmap_pde_v(pmap_pde(pmap, va))) {
			/* XXX: avoid address wrap around */
			if (va >= i386_trunc_pdr((vm_offset_t)-1))
				break;
			va = i386_round_pdr(va + PAGE_SIZE) - PAGE_SIZE;
			pte = 0;
			continue;
		}

		if (pte == 0)
			pte = pmap_pte(pmap, va);
		if (pte == 0) panic("pmap_protect: cannot happen");

		/*
		 * Page not valid.  Again, skip it.
		 * Should we do this?  Or set protection anyway?
		 */
		if (!pmap_pte_v(pte)) {
			pte++;
			continue;
		}

		ix = 0;
		i386prot = pte_prot(pmap, prot);
		if(va < PT_MAX_ADDRESS)
			i386prot |= 2 /*PG_u*/;
		do {
			/* clear VAC here if PG_RO? */
			pmap_pte_set_prot(pte++, i386prot);
			/*TBIS(va + ix * I386_PAGE_SIZE);*/
			PMAP_INVTLB(pmap, va + ix * I386_PAGE_SIZE);
		} while (++ix != i386pagesperpage);
	}
	/* if (curproc && pmap == &curproc->p_vmspace->vm_pmap)
		pmap_activate(pmap, (struct pcb *)curproc->p_addr); */
	PMAP_ACTIVATE(pmap, (struct pcb *) curproc->p_addr);
}

/*
 * (Re)Allocate a unique page directory and page table object to hold
 * any page table pages. pmap_collect() can snatch these if process
 * goes inactive, and any subsequent fault may force them restored on
 * demand. N.B.the kernel stack is independantly managed by the kernel,
 * and wired/unwired before entering or after leaving run queues.
 */
void
pmap_pdiralloc(struct pmap *pmap)
{
	/* do we currently have a page directory ? */
	if (pmap->pm_pdir == 0) {
		pmap->pm_pdir = (pd_entry_t *) kmem_alloc(kernel_map, NBPG, 0);

		/* wire in kernel global address entries, clear user entries */
		(void) memset(pmap->pm_pdir, 0, KPTDI_FIRST*4);
		(void) memcpy(pmap->pm_pdir+KPTDI_FIRST, PTD+KPTDI_FIRST, 
			(KPTDI_LAST-KPTDI_FIRST+1)*4);

		/* install self-referential address mapping entry */
		pmap->pm_proc->p_addr->u_pcb.pcb_ptd = *(int *)(pmap->pm_pdir+PTDPTDI) =
			(int)pmap_extract(kernel_pmap, (vm_offset_t) pmap->pm_pdir) | PG_V | PG_KW;

		/* make it current */
		pmap->pm_ptchanged = TRUE;
		/* pmap->pm_proc = curproc; */
		(void)pmap_activate(pmap, (struct pcb *)curproc->p_addr);
	}

	/* do we have a page table object ? */
	if (pmap->pm_ptobject == 0) {

		/* allocate a object to hold page table pages for user */
		pmap->pm_ptobject =
			vm_object_allocate(PT_MAX_ADDRESS - PT_MIN_ADDRESS);

		/* tell pageout not to mess with this object */
		pmap->pm_ptobject->paging_in_progress++;
	}
}

/*
 *	Insert the given physical page (p) at
 *	the specified virtual address (v) in the
 *	target physical map with the protection requested.
 *
 *	If specified, the page will be wired down, meaning
 *	that the related pte can not be reclaimed.
 *
 *	NB:  This is the only routine which MAY NOT lazy-evaluate
 *	or lose information.  That is, this routine must actually
 *	insert this page into the given map NOW.
 */
void
pmap_enter(register struct pmap *pmap, vm_offset_t va,
	register vm_offset_t pa, vm_prot_t prot, boolean_t wired, int am)
{
	register pt_entry_t *pte;
	register int npte, ix;
	vm_offset_t opa;
	boolean_t cacheable = TRUE;
	boolean_t checkpv = TRUE;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_ENTER))
		printf("pmap_enter(%x, %x, %x, %x, %x)",
		       pmap, va, pa, prot, wired);
#endif
	if (pmap == NULL)
		return;
	/* am = AM_NONE;*/

	/* do we currently have a page directory and page table object ? */
	if (pmap != kernel_pmap && (pmap->pm_pdir == 0 || pmap->pm_ptobject == 0))
		pmap_pdiralloc(pmap);
#ifdef nope
	/* do we currently have a page directory ? */
	if (pmap->pm_pdir == 0) {
		pmap->pm_pdir = (pd_entry_t *) kmem_alloc(kernel_map, NBPG, 0);

		/* wire in kernel global address entries, clear user entries */
		(void) memset(pmap->pm_pdir, 0, KPTDI_FIRST*4);
		(void) memcpy(pmap->pm_pdir+KPTDI_FIRST, PTD+KPTDI_FIRST, 
			(KPTDI_LAST-KPTDI_FIRST+1)*4);

		/* install self-referential address mapping entry */
		pmap->pm_proc->p_addr->u_pcb.pcb_ptd = *(int *)(pmap->pm_pdir+PTDPTDI) =
			(int)pmap_extract(kernel_pmap, (vm_offset_t) pmap->pm_pdir) | PG_V | PG_KW;

		/* make it current */
		pmap->pm_ptchanged = TRUE;
		/* pmap->pm_proc = curproc; */
		(void)pmap_activate(pmap, (struct pcb *)curproc->p_addr);
	}

	/* do we have a page table object ? */
	if (pmap != kernel_pmap && pmap->pm_ptobject == 0) {
		/* allocate a object to hold page table pages for user */
		pmap->pm_ptobject =
			vm_object_allocate(PT_MAX_ADDRESS - PT_MIN_ADDRESS);

		/* tell pageout not to mess with this object */
		pmap->pm_ptobject->paging_in_progress++;
	}
#endif

	/* are we in a valid region? */
	if(va > VM_MAX_KERNEL_ADDRESS || (va >= PT_MAX_ADDRESS && va < VM_MIN_KERNEL_ADDRESS))
		panic("pmap_enter: invalid address range");
	/* if (va >= PT_MIN_ADDRESS && va < PT_MAX_ADDRESS)
		panic("pmap_enter: pt address"); */

#ifdef DEBUG
	if (pmap == kernel_pmap)
		enter_stats.kernel++;
	else
		enter_stats.user++;
#endif

	/*
	 * Page Directory table entry not valid, we need a new PT page
	 */
	if (!pmap_pde_v(pmap_pde(pmap, va)))
		pmap_ptalloc(pmap, pmap_pde(pmap, va));

	pte = pmap_pte(pmap, va);
	opa = pmap_pte_pa(pte);
#ifdef DEBUG
	if (pmapdebug & PDB_ENTER)
		printf("enter: pte %x, *pte %x ", pte, *(int *)pte);
#endif

	/*
	 * Mapping has not changed, must be protection or wiring change.
	 */
	if (opa == pa) {
#ifdef DEBUG
		enter_stats.pwchange++;
#endif
		/*
		 * Wiring change, just update stats.
		 * We don't worry about wiring PT pages as they remain
		 * resident as long as there are valid mappings in them.
		 * Hence, if a user page is wired, the PT page will be also.
		 */
		if (wired && !pmap_pte_w(pte) || !wired && pmap_pte_w(pte)) {
#ifdef DEBUG
			if (pmapdebug & PDB_ENTER)
				printf("enter: wiring change -> %x ", wired);
#endif
			if (wired)
				pmap->pm_stats.wired_count++;
			else
				pmap->pm_stats.wired_count--;
#ifdef DEBUG
			enter_stats.wchange++;
#endif
		}
		goto validate;
	}

	/*
	 * Mapping has changed, invalidate old range and fall through to
	 * handle validating new mapping.
	 */
	if (opa) {
#ifdef DEBUG
		if (pmapdebug & PDB_ENTER)
			printf("enter: removing old mapping %x pa %x ", va, opa);
#endif
		pmap_remove(pmap, va, va + PAGE_SIZE);
#ifdef DEBUG
		enter_stats.mchange++;
#endif
	}

	/*
	 * Enter on the PV list if part of our managed memory
	 * Note that we raise IPL while manipulating pv_table
	 * since pmap_enter can be called at interrupt time.
	 */
	/* XXX don't insert pager mappings into pvtable */
	if ((pa >= vm_first_phys && pa <= vm_last_phys)
	    && (va < pager_sva || va > pager_eva)
	    && (va != (vm_offset_t)vmmap)
	    /*&& (va < vm_map_min(kmem_map) || va > vm_map_max(kmem_map))*/) {
		register pv_entry_t pv, npv;
		int s;

#ifdef DEBUG
		enter_stats.managed++;
#endif
		pv = pa_to_pvh(pa);
		s = splimp();
#ifdef DEBUG
		if (pmapdebug & PDB_ENTER)
			printf("enter: pv at %x: %x/%x/%x ",
			       pv, pv->pv_va, pv->pv_pmap, pv->pv_next);
#endif

		/* track rss changes for this process */
		if (pmap != kernel_pmap && pmap->pm_proc) {
			struct pstats *ps = pmap->pm_proc->p_stats;

#ifdef notdef
			/* more than one mapping? */
			if (pv->pv_pmap != NULL && am != AM_NONE)
				ps->p_ru.ru_ilrss += PAGE_SIZE/1024;
#endif

			switch (am) {
			case AM_TEXT:
				ps->p_ru.ru_ixrss += PAGE_SIZE/1024;
				break;
			case AM_DATA:
				ps->p_ru.ru_idrss += PAGE_SIZE/1024;
				break;
			case AM_STACK:
				ps->p_ru.ru_isrss += PAGE_SIZE/1024;
			}

			pmap->pm_proc->p_vmspace->vm_rssize =
		    	    atop((ps->p_ru.ru_ixrss + ps->p_ru.ru_idrss
		    		+ ps->p_ru.ru_isrss /* - ps->p_ru.ru_ilrss */) * 1024);
		}

		/*
		 * No entries yet, use header as the first entry
		 */
		if (pv->pv_pmap == NULL) {
#ifdef DEBUG
			enter_stats.firstpv++;
#endif
			pv->pv_va = va;
			pv->pv_pmap = pmap;
			pv->pv_next = NULL;
			pv->pv_am = am;
		}
		/*
		 * There is at least one other VA mapping this page.
		 * Place this entry after the header.
		 */
		else {
			int mflags;
#ifdef notyet
			/* on second mapping of text, change unshared to shared */
			if (pv->pv_next == NULL && pv->pv_am == AM_TEXT) {
				struct pstats *ps =
					pv->pv_pmap->pm_proc->p_stats;
				ps->p_ru.ru_ixrss -= PAGE_SIZE/1024;
				/* ps->p_ru.ru_ilrss += PAGE_SIZE/1024; */
			}
#endif
				
			for (npv = pv; npv; npv = npv->pv_next)
{
				if (pmap == npv->pv_pmap && va == npv->pv_va)
					panic("pmap_enter: pvtable");
	   if (npv->pv_va >= vm_map_min(kmem_map) && npv->pv_va < vm_map_max(kmem_map))
			panic("kmem: in pv list ");
}
	   if (va >= vm_map_min(kmem_map) && va < vm_map_max(kmem_map))
			panic("kmem: into pv list");
			mflags = pmap == kernel_pmap? M_NOWAIT : M_WAITOK;
			npv = (pv_entry_t)
				malloc(sizeof *npv, M_VMPVENT, mflags);
			npv->pv_va = va;
			npv->pv_pmap = pmap;
			npv->pv_am = am;
			npv->pv_next = pv->pv_next;
			pv->pv_next = npv;
#ifdef DEBUG
			if (!npv->pv_next)
				enter_stats.secondpv++;
#endif
		}
		splx(s);
	}
/*if (pa < vm_first_phys || pa > vm_last_phys)
{
		printf("ent %x", pa);
Debugger();
}*/
	/*
	 * Assumption: if it is not part of our managed memory
	 * then it must be device memory which may be volatile.
	 */
	if (pmap_initialized) {
		checkpv = cacheable = FALSE;
#ifdef DEBUG
		enter_stats.unmanaged++;
#endif
	}

	/*
	 * Increment counters
	 */
	pmap->pm_stats.resident_count++;
	if (wired)
		pmap->pm_stats.wired_count++;

validate:
	/*
	 * Now validate mapping with desired protection/wiring.
	 * Assume uniform modified and referenced status for all
	 * I386 pages in a MACH page.
	 */
	npte = (pa & PG_FRAME) | pte_prot(pmap, prot) | PG_V;
	npte |= (*(int *)pte & (PG_M|PG_U));
	if (wired)
		npte |= PG_W | /* PG_CD | */ PG_WT;
	/* if(va < PT_MIN_ADDRESS) */
		npte |= PG_u;
	/* else if(va < PT_MAX_ADDRESS)
		npte |= PG_u | PG_RW; */
#ifdef DEBUG
	if (pmapdebug & PDB_ENTER)
		printf("enter: new pte value %x ", npte);
#endif
	ix = 0;
	do {
		/* if pte is invalid, don't bother with invalidate */
		if (*(int *)pte == 0)
			*(int *)pte++ = npte;
		else {
			*(int *)pte++ = npte;
			PMAP_INVTLB(pmap, va);
		}
		npte += I386_PAGE_SIZE;
		va += I386_PAGE_SIZE;
	} while (++ix != i386pagesperpage);
#ifdef DEBUGx
cache, tlb flushes
#endif
	/* *should not flush on initial mapping, *should flush on removing/
	changing mapping, should do page by page on 486, all at once on
	386, I/O ranges */
	if (!pte)
		return;
	PMAP_ACTIVATE(pmap, (struct pcb *)curproc->p_addr);
}

/*
 *	pmap_ptalloc:
 *
 *	Allocate a page table page for a specific page directory entry.
 *	Called from pmap_enter when adding a new translation, or from
 *	trap to restore a page table page reaped by pmap_collect.
 *	Not used by kernel_pmap.
 */
void
pmap_ptalloc(struct pmap *pmap, pd_entry_t *pde)
{
	vm_object_t	object;
	vm_offset_t	offset = i386_ptob(pde - pmap->pm_pdir);
	vm_page_t	mem;

	/* the kernel has everything statically allocated */
	if (pmap == kernel_pmap)
		panic("pmap_ptalloc: kernel pmap");

	/* do we currently have a page directory and page table object ? */
	if (pmap->pm_pdir == 0 || pmap->pm_ptobject == 0)
		pmap_pdiralloc(pmap);
		
	/* grab object */
	object = pmap->pm_ptobject;

	/* stick a page into it */
	while ((mem = vm_page_alloc(object, offset, 0)) == NULL)
		vm_page_wait("ptpage", 1);

	/* clear the page (fills with invalid entries) */
	pmap_zero_page(mem->phys_addr);
	mem->busy = FALSE;
	mem->wire_count = 0;

	/* stick page into page directory table */
	*(int *)pde = (mem->phys_addr & PG_FRAME) | PG_UW | PG_V;
}

/*
 *	pmap_ptfree():
 *
 *	Free page table page(s). Either free the specified pde, or
 *	if pde is NULL, all pde's in the object.
 */
static void
pmap_ptfree(struct pmap *pmap, pd_entry_t *pde)
{
	vm_object_t	object = pmap->pm_ptobject;
	vm_offset_t	start, end;
	vm_page_t	mem;

	/* do one or all? */
	if (pde) {
		start = i386_ptob(pde - pmap->pm_pdir);
		end = start + NBPG;
	} else {
		start = 0;
		end = PT_MIN_ADDRESS - PT_MAX_ADDRESS;
	}

	/* remove the pages */
	vm_object_page_remove(object, start, end);
}

/*
 *      pmap_page_protect:
 *
 *      Lower the permission for all mappings to a given page.
 */
void
pmap_page_protect(vm_offset_t phys, vm_prot_t prot)
{
        switch (prot) {
        case VM_PROT_READ:
        case VM_PROT_READ|VM_PROT_EXECUTE:
                pmap_copy_on_write(phys);
                break;
        case VM_PROT_ALL:
                break;
        default:
                pmap_remove_all(phys);
                break;
        }
}

/*
 *	Routine:	pmap_change_wiring
 *	Function:	Change the wiring attribute for a map/virtual-address
 *			pair.
 *	In/out conditions:
 *			The mapping must already exist in the pmap.
 */
void
pmap_change_wiring(register struct pmap *pmap, vm_offset_t va, boolean_t wired)
{
	register pt_entry_t *pte;
	register int ix;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_change_wiring(%x, %x, %x)", pmap, va, wired);
#endif
	if (pmap == NULL || pmap->pm_pdir == 0)
		return;

	pte = pmap_pte(pmap, va);
#ifdef DEBUG
	/*
	 * Page table page is not allocated.
	 * Should this ever happen?  Ignore it for now,
	 * we don't want to force allocation of unnecessary PTE pages.
	 */
	if (!pmap_pde_v(pmap_pde(pmap, va))) {
		if (pmapdebug & PDB_PARANOIA)
			printf("pmap_change_wiring: invalid PDE for %x ", va);
		return;
	}
	/*
	 * Page not valid.  Should this ever happen?
	 * Just continue and change wiring anyway.
	 */
	if (!pmap_pte_v(pte)) {
		if (pmapdebug & PDB_PARANOIA)
			printf("pmap_change_wiring: invalid PTE for %x ", va);
	}
#endif
	if (wired && !pmap_pte_w(pte) || !wired && pmap_pte_w(pte)) {
		if (wired)
			pmap->pm_stats.wired_count++;
		else
			pmap->pm_stats.wired_count--;
	}
	/*
	 * Wiring is not a hardware characteristic so there is no need
	 * to invalidate TLB.
	 */
	ix = 0;
	do {
		pmap_pte_set_wt(pte, wired);
		/* pmap_pte_set_cd(pte, wired); */
		pmap_pte_set_w(pte++, wired);
	} while (++ix != i386pagesperpage);
}

/*
 *	Routine:	pmap_pte
 *	Function:
 *		Extract the page table entry associated
 *		with the given map/virtual_address pair.
 * [ what about induced faults -wfj]
 */
/* db_something() calls pmap_pte()! */
/* static */ struct pte *
pmap_pte(register struct pmap *pmap, vm_offset_t va)
{

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_pte(%x, %x) ->\n", pmap, va);
#endif
	if (pmap && pmap->pm_pdir && pmap_pde_v(pmap_pde(pmap, va))) {

		/* are we current address space or kernel? */
		if (pmap->pm_pdir[PTDPTDI].pd_pfnum == PTDpde.pd_pfnum
			|| pmap == kernel_pmap)
			return ((struct pte *) vtopte(va));

		/* otherwise, we are alternate address space */
		else {
			if (pmap->pm_pdir[PTDPTDI].pd_pfnum
				!= APTDpde.pd_pfnum) {
				APTDpde = pmap->pm_pdir[PTDPTDI];
				lcr(3, rcr(3));
			}
			return((struct pte *) avtopte(va));
		}
	}
	return(0);
}

/*
 *	Routine:	pmap_extract
 *	Function:
 *		Extract the physical page address associated
 *		with the given map/virtual_address pair.
 */

vm_offset_t
pmap_extract(register struct pmap *pmap, vm_offset_t va)
{
	register vm_offset_t pa;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_extract(%x, %x) -> ", pmap, va);
#endif
	pa = 0;
	if (pmap && pmap_pde_v(pmap_pde(pmap, va))) {
		pa = *(int *) pmap_pte(pmap, va);
	}
	if (pa)
		pa = (pa & PG_FRAME) | (va & ~PG_FRAME);

/*if (pa < vm_first_phys || pa > vm_last_phys) {
		printf("ext %x", pa);
Debugger();
}*/
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("%x\n", pa);
#endif
	return(pa);
}

/*
 *	Copy the range specified by src_addr/len
 *	from the source map to the range dst_addr/len
 *	in the destination map.
 *
 *	This routine is only advisory and need not do anything.
 */
void
pmap_copy(struct pmap *dst_pmap, struct pmap *src_pmap,
	vm_offset_t dst_addr, vm_size_t len, vm_offset_t src_addr)
{
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_copy(%x, %x, %x, %x, %x)",
		       dst_pmap, src_pmap, dst_addr, len, src_addr);
#endif
}

/*
 *	Require that all active physical maps contain no
 *	incorrect entries NOW.  [This update includes
 *	forcing updates of any address map caching.]
 *
 *	Generally used to insure that a thread about
 *	to run will see a semantically correct world.
 */
void
pmap_update()
{
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_update()");
#endif
	tlbflush();
}

/*
 *	Routine:	pmap_collect
 *	Function:
 *		Garbage collects the physical map system for
 *		pages which are no longer used.
 *		Success need not be guaranteed -- that is, there
 *		may well be pages which are not referenced, but
 *		others may be collected.
 *	Usage:
 *		Called by the pageout daemon when pages are scarce.
 * [ needs to be written -wfj ]
 */
void
pmap_collect(register struct pmap *pmap)
{
	register vm_offset_t pa;
	register pv_entry_t pv;
	register int *pte;
	vm_offset_t kpa;
	int s;

#ifdef DEBUG
	int *pde;
	int opmapdebug;
	if (pmapdebug & PDB_COLLECT)
		printf("pmap_collect(%x) ", pmap);
#endif
	if (pmap == kernel_pmap)
		return;
	pmapdebug=0xffff;

	/* undone: snapshot rss */
	

	/* go back to global kernel address space */
	pmap->pm_proc->p_addr->u_pcb.pcb_ptd = KernelPTD;

	/* make it current */
	/*pmap->pm_ptchanged = TRUE;
	(void)pmap_activate(pmap, (struct pcb *)curproc->p_addr);*/
	tlbflush();

	(void) memset(pmap->pm_pdir + PTDPTDI, 0, (KPTDI_LAST-PTDPTDI+1)*4);

	/* need to pmap_remove entire process mapping first */
	pmap_remove(pmap, VM_MIN_ADDRESS, VM_MAX_ADDRESS);

	/* free page table pages and object that holds them */
	if (pmap->pm_ptobject) {
		pmap_ptfree(pmap, 0);
		pmap->pm_ptobject->paging_in_progress--;
		vm_object_terminate(pmap->pm_ptobject);
		pmap->pm_ptobject = 0;
	}

	/* remove processes page directory */
	if (pmap->pm_pdir) {
		kmem_free(kernel_map, (vm_offset_t)pmap->pm_pdir, NBPG);
		pmap->pm_pdir = 0;
	}

	/* process address translation memory is freed (until next pmap_enter) */
	pmapdebug=0;
}

void
pmap_activate(register struct pmap *pmap, struct pcb *pcbp)
{
int x;
#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_PDRTAB))
		printf("pmap_activate(%x, %x) ", pmap, pcbp);
#endif
	PMAP_ACTIVATE(pmap, pcbp);
/*tlbflush();*/
/*printf("pde ");
for(x=0x3f6; x < 0x3fA; x++)
	printf("%x ", pmap->pm_pdir[x]);*/
/*pads(pmap);*/
/*pg(" pcb_cr3 %x", pcbp->pcb_cr3);*/
}

/*
 *	Routine:	pmap_kernel
 *	Function:
 *		Returns the physical map handle for the kernel.
 */
pmap_t
pmap_kernel()
{
    	/* return (kernel_pmap); */
    	return (kernel_pmap);
}

/*
 *	pmap_zero_page zeros the specified (machine independent)
 *	page by mapping the page into virtual memory and using
 *	memset() to clear its contents, one machine dependent page
 *	at a time.
 */
void
pmap_zero_page(vm_offset_t phys)
{
	register int ix;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_zero_page(%x)", phys);
#endif
	phys >>= PG_SHIFT;
	ix = 0;
	do {
		*(int *)CMAP2 = PG_V | PG_KW | ctob(phys++);
		tlbflush();
		/* asm volatile ("invlpg %0 " : : "m" (CADDR2)); */
		(void) memset(CADDR2, 0, NBPG);
		*(int *) CMAP2 = 0;
	} while (++ix != i386pagesperpage);
}


/*
 * copy a page of physical memory
 * specified in relocation units (NBPG bytes)
 */
physcopyseg(frm, to) {

	*(int *)CMAP1 = PG_V | PG_KW | ctob(frm);
	*(int *)CMAP2 = PG_V | PG_KW | ctob(to);
	tlbflush();
	/* asm volatile ("invlpg %0 " : : "m" (CADDR1));
	asm volatile ("invlpg %0 " : : "m" (CADDR2)); */
	memcpy(CADDR2, CADDR1, NBPG);
	*(int *) CMAP1 = 0;
	*(int *) CMAP2 = 0;
}
/*
 *	pmap_copy_page copies the specified (machine independent)
 *	page by mapping the page into virtual memory and using
 *	memcpy() to copy the page, one machine dependent page at a
 *	time.
 */
void
pmap_copy_page(vm_offset_t src, vm_offset_t dst)
{
	register int ix;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_copy_page(%x, %x)", src, dst);
#endif
	src >>= PG_SHIFT;
	dst >>= PG_SHIFT;
	ix = 0;
	do {
		*(int *)CMAP1 = PG_V | PG_KW | ctob(src++);
		*(int *)CMAP2 = PG_V | PG_KW | ctob(dst++);
		tlbflush();
		/* asm volatile ("invlpg %0 " : : "m" (CADDR1));
		asm volatile ("invlpg %0 " : : "m" (CADDR2)); */
		memcpy(CADDR2, CADDR1, NBPG);
		*(int *) CMAP1 = 0;
		*(int *) CMAP2 = 0;
	} while (++ix != i386pagesperpage);
}


/*
 *	Routine:	pmap_pageable
 *	Function:
 *		Make the specified pages (by pmap, offset)
 *		pageable (or not) as requested.
 *
 *		A page which is not pageable may not take
 *		a fault; therefore, its page table entry
 *		must remain valid for the duration.
 *
 *		This routine is merely advisory; pmap_enter
 *		will specify that these pages are to be wired
 *		down (or not) as appropriate.
 */
void
pmap_pageable(register struct pmap *pmap, vm_offset_t sva, vm_offset_t eva, boolean_t pageable)
{
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_pageable(%x, %x, %x, %x)",
		       pmap, sva, eva, pageable);
#endif
	/*
	 * If we are making a PT page pageable then all valid
	 * mappings must be gone from that page.  Hence it should
	 * be all zeros and there is no need to clean it.
	 * Assumptions:
	 *	- we are called with only one page at a time
	 *	- PT pages have only one pv_table entry
	 */
	if (pmap == kernel_pmap && pageable && sva + PAGE_SIZE == eva) {
		register pv_entry_t pv;
		register vm_offset_t pa;

#ifdef DEBUG
		if ((pmapdebug & (PDB_FOLLOW|PDB_PTPAGE)) == PDB_PTPAGE)
			printf("pmap_pageable(%x, %x, %x, %x)",
			       pmap, sva, eva, pageable);
#endif
		/*if (!pmap_pde_v(pmap_pde(pmap, sva)))
			return;*/
		if(pmap_pte(pmap, sva) == 0)
			return;
		pa = pmap_pte_pa(pmap_pte(pmap, sva));
		if (pa < vm_first_phys || pa > vm_last_phys)
			return;
		pv = pa_to_pvh(pa);
		/*if (!ispt(pv->pv_va))
			return;*/
#ifdef DEBUG
		if (pv->pv_va != sva || pv->pv_next) {
			printf("pmap_pageable: bad PT page va %x next %x\n",
			       pv->pv_va, pv->pv_next);
			return;
		}
#endif
		/*
		 * Mark it unmodified to avoid pageout
		 */
		pmap_clear_modify(pa);
#ifdef needsomethinglikethis
		if (pmapdebug & PDB_PTPAGE)
			printf("pmap_pageable: PT page %x(%x) unmodified\n",
			       sva, *(int *)pmap_pte(pmap, sva));
		if (pmapdebug & PDB_WIRING)
			pmap_check_wiring("pageable", sva);
#endif
	}
}

/*
 *	Clear the modify bits on the specified physical page.
 */

void
pmap_clear_modify(vm_offset_t pa)
{
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_clear_modify(%x)", pa);
#endif
	pmap_changebit(pa, PG_M, FALSE);
}

/*
 *	pmap_clear_reference:
 *
 *	Clear the reference bit on the specified physical page.
 */

void
pmap_clear_reference(vm_offset_t pa)
{
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_clear_reference(%x)", pa);
#endif
	pmap_changebit(pa, PG_U, FALSE);
}

/*
 *	pmap_is_referenced:
 *
 *	Return whether or not the specified physical page is referenced
 *	by any physical maps.
 */

boolean_t
pmap_is_referenced(vm_offset_t pa)
{
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW) {
		boolean_t rv = pmap_testbit(pa, PG_U);
		printf("pmap_is_referenced(%x) -> %c", pa, "FT"[rv]);
		return(rv);
	}
#endif
	return(pmap_testbit(pa, PG_U));
}

/*
 *	pmap_is_modified:
 *
 *	Return whether or not the specified physical page is modified
 *	by any physical maps.
 */

boolean_t
pmap_is_modified(vm_offset_t pa)
{
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW) {
		boolean_t rv = pmap_testbit(pa, PG_M);
		printf("pmap_is_modified(%x) -> %c", pa, "FT"[rv]);
		return(rv);
	}
#endif
	return(pmap_testbit(pa, PG_M));
}

vm_offset_t
pmap_phys_address(int ppn)
{
	return(i386_ptob(ppn));
}

/*
 * Miscellaneous support routines follow
 */

static void
i386_protection_init()
{
	register int *kp, prot;

	kp = protection_codes;
	for (prot = 0; prot < 8; prot++) {
		switch (prot) {
		case VM_PROT_NONE | VM_PROT_NONE | VM_PROT_NONE:
			*kp++ = 0;
			break;
		case VM_PROT_READ | VM_PROT_NONE | VM_PROT_NONE:
		case VM_PROT_READ | VM_PROT_NONE | VM_PROT_EXECUTE:
		case VM_PROT_NONE | VM_PROT_NONE | VM_PROT_EXECUTE:
			*kp++ = PG_RO;
			break;
		case VM_PROT_NONE | VM_PROT_WRITE | VM_PROT_NONE:
		case VM_PROT_NONE | VM_PROT_WRITE | VM_PROT_EXECUTE:
		case VM_PROT_READ | VM_PROT_WRITE | VM_PROT_NONE:
		case VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE:
			*kp++ = PG_RW;
			break;
		}
	}
}

boolean_t
pmap_testbit(register vm_offset_t pa, int bit)
{
	register pv_entry_t pv;
	register int *pte, ix;
	int s;

	if (pa < vm_first_phys || pa > vm_last_phys)
		return(FALSE);

	pv = pa_to_pvh(pa);
	s = splimp();
	/*
	 * Check saved info first
	 */
	if (pmap_attributes[pa_index(pa)] & bit) {
		splx(s);
		return(TRUE);
	}
	/*
	 * Not found, check current mappings returning
	 * immediately if found.
	 */
	if (pv->pv_pmap != NULL) {
		for (; pv; pv = pv->pv_next) {
			if (pv->pv_va >= pager_sva && pv->pv_va < pager_eva)
				continue;
	    		/*if (pv->pv_va >= vm_map_min(kmem_map) &&
			    pv->pv_va < vm_map_max(kmem_map))
				continue;*/
			pte = (int *) pmap_pte(pv->pv_pmap, pv->pv_va);
			ix = 0;
			do {
				if (*pte++ & bit) {
					splx(s);
					return(TRUE);
				}
			} while (++ix != i386pagesperpage);
		}
	}
	splx(s);
	return(FALSE);
}

void
pmap_changebit(register vm_offset_t pa, int bit, boolean_t setem)
{
	register pv_entry_t pv;
	register int *pte, npte, ix;
	vm_offset_t va;
	int s;
	boolean_t firstpage = TRUE;

#ifdef DEBUG
	if (pmapdebug & PDB_BITS)
		printf("pmap_changebit(%x, %x, %s)",
		       pa, bit, setem ? "set" : "clear");
#endif
	if (pa < vm_first_phys || pa > vm_last_phys)
		return;

	pv = pa_to_pvh(pa);
	s = splimp();
	/*
	 * Clear saved attributes (modify, reference)
	 */
	if (!setem)
		pmap_attributes[pa_index(pa)] &= ~bit;
	/*
	 * Loop over all current mappings setting/clearing as appropos
	 * If setting RO do we need to clear the VAC?
	 */
	if (pv->pv_pmap != NULL) {
#ifdef DEBUG
		int toflush = 0;
#endif
		for (; pv; pv = pv->pv_next) {
#ifdef DEBUG
			toflush |= (pv->pv_pmap == kernel_pmap) ? 2 : 1;
#endif
			va = pv->pv_va;

                        /*
                         * XXX don't write protect pager mappings
                         */
                        if (bit == PG_RO) {
                                extern vm_offset_t pager_sva, pager_eva;

                                if (va >= pager_sva && va < pager_eva)
                                        continue;
	    			/*if (va >= vm_map_min(kmem_map) &&
				    va < vm_map_max(kmem_map))
					continue;*/
                        }

			pte = (int *) pmap_pte(pv->pv_pmap, va);
if (pte == 0) {
printf("va %x ", va);
panic("chg");
}
			ix = 0;
			do {
				if (setem)
					npte = *pte | bit;
				else
					npte = *pte & ~bit;
				if (*pte != npte) {
					*pte = npte;
					PMAP_INVTLB(pv->pv_pmap, va);
				}
				va += I386_PAGE_SIZE;
				pte++;
			} while (++ix != i386pagesperpage);

			PMAP_ACTIVATE(pv->pv_pmap, (struct pcb *) curproc->p_addr);
			/* if (curproc && pv->pv_pmap == &curproc->p_vmspace->vm_pmap)
				pmap_activate(pv->pv_pmap, (struct pcb *)curproc->p_addr); */
		}
#ifdef somethinglikethis
		if (setem && bit == PG_RO && (pmapvacflush & PVF_PROTECT)) {
			if ((pmapvacflush & PVF_TOTAL) || toflush == 3)
				DCIA();
			else if (toflush == 2)
				DCIS();
			else
				DCIU();
		}
#endif
	}
	splx(s);
}

#ifdef DEBUG
pmap_pvdump(vm_offset_t pa)
{
	register pv_entry_t pv;

	printf("pa %x", pa);
	for (pv = pa_to_pvh(pa); pv; pv = pv->pv_next) {
		printf(" -> pmap %x, va %x, am %x",
		       pv->pv_pmap, pv->pv_va, pv->pv_am);
		pads(pv->pv_pmap);
	}
	printf(" ");
}

#ifdef notyet
pmap_check_wiring(char *str, vm_offset_t va)
{
	vm_map_entry_t entry;
	register int count, *pte;

	va = trunc_page(va);
	if (!pmap_pde_v(pmap_pde(kernel_pmap, va)) ||
	    !pmap_pte_v(pmap_pte(kernel_pmap, va)))
		return;

	if (!vm_map_lookup_entry(pt_map, va, &entry)) {
		printf("wired_check: entry for %x not found\n", va);
		return;
	}
	count = 0;
	for (pte = (int *)va; pte < (int *)(va+PAGE_SIZE); pte++)
		if (*pte)
			count++;
	if (entry->wired_count != count)
		printf("*%s*: %x: w%d/a%d\n",
		       str, va, entry->wired_count, count);
}
#endif

/* print address space of pmap*/
pads(register struct pmap *pm) {
	unsigned va, i, j;
	struct pte *ptep;

	if(pm == kernel_pmap) return;
	for (i = 0; i < 1024; i++) 
		if(pm->pm_pdir[i].pd_v)
			for (j = 0; j < 1024 ; j++) {
				va = (i<<22)+(j<<12);
				if (pm == kernel_pmap && va < KERNBASE)
						continue;
				if (pm != kernel_pmap && va > PT_MAX_ADDRESS)
						continue;
				ptep = pmap_pte(pm, va);
				if(pmap_pte_v(ptep)) 
					printf("%x:%x ", va, *(int *)ptep); 
			} ;
				
}
#endif
