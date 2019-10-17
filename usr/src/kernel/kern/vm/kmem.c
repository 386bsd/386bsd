/*
 * Copyright (c) 1994 William F. Jolitz, TeleMuse
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
 *	This software is a component of "386BSD" developed by 
 *	William F. Jolitz, TeleMuse.
 * 4. Neither the name of the developer nor the name "386BSD"
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 5. Non-commercial distribution of this complete file in either source and/or
 *    binary form at no charge to the user (such as from an official Internet 
 *    archive site) is permitted. 
 * 6. Commercial distribution and sale of this complete file in either source
 *    and/or binary form on any media, including that of floppies, tape, or 
 *    CD-ROM, or through a per-charge download such as that of a BBS, is not 
 *    permitted without specific prior written permission.
 * 7. Non-commercial and/or commercial distribution of an incomplete, altered, 
 *    or otherwise modified file in either source and/or binary form is not 
 *    permitted.
 *
 * THIS SOFTWARE IS A COMPONENT OF 386BSD DEVELOPED BY WILLIAM F. JOLITZ 
 * AND IS INTENDED FOR RESEARCH AND EDUCATIONAL PURPOSES ONLY. THIS 
 * SOFTWARE SHOULD NOT BE CONSIDERED TO BE A COMMERCIAL PRODUCT. 
 * THE DEVELOPER URGES THAT USERS WHO REQUIRE A COMMERCIAL PRODUCT 
 * NOT MAKE USE OF THIS WORK.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPER ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPER BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: kmem.c,v 1.1 94/10/19 17:37:15 bill Exp $
 *
 * Kernel memory / address space management.
 */

#include "sys/param.h"
#include "proc.h"
#include "vm.h"
#include "vmspace.h"
#include "kmem.h"
#include "vm_pageout.h"
#include "malloc.h"
#include "buf.h"
#include "modconfig.h"
#define	__NO_INLINES
#include "prototypes.h"

vm_map_t kernel_map, kmem_map, mb_map, buf_map, pager_map, phys_map;

/*
 * Bootstrap the virtual memory system by constructing the
 * kernel program's address space descriptor, and record the
 * presence of the initial contents of the kernel program in it.
 */
void
kmem_init(void)
{
	extern vm_offset_t virtual_avail, virtual_end;
	vm_offset_t	addr;

	addr = VM_MIN_KERNEL_ADDRESS;
	vm_map_init(&kernspace.vm_map, addr, virtual_end, FALSE);
	kernspace.vm_map.pmap = &kernspace.vm_pmap;	/* XXX */
	kernspace.vm_refcnt = 1;
	kernel_map = &kernspace.vm_map;			/* XXX */
	
	(void)vm_map_insert(kernel_map, NULL, (vm_offset_t) 0, addr,
	    virtual_avail);
}

/*
 * Kernel address space and memory are allocated by kmem_alloc(). This
 * allocator is used both by the kernel and the virtual memory system
 * to add resource to the kernel program address space at page granularity.
 * It may be used from interrupt level (M_NOWAIT) as well from process/thread
 * context. Both the core kernel allocator malloc() and packet allocator
 * m_get() obtain and subdivide memory resource from this allocator.
 */
vm_offset_t
kmem_alloc(vm_map_t map, vm_size_t size, int flags)
{
	vm_offset_t offset, addr, i;
	vm_page_t m;
	vm_object_t object;
	vm_map_entry_t me;
	int pl;

	pl = splhigh();

	/* is request larger than can ever be satisfied with this map */
	if (size > map->max_offset - map->min_offset)
		panic("kmem_alloc: map smaller than allocation");

	/* locate a fresh portion of kernel address space for allocation */
	size = round_page(size);
	addr = vm_map_min(map);
	if (flags & M_NOWAIT) {
		if (lock_try_write(&map->lock) == FALSE)
			return(0);
		if (vm_map_find(map, &addr, size) != KERN_SUCCESS) {
			vm_map_unlock(map);
			splx(pl);
			return (0);
		}
	} else {
		vm_map_lock(map);
		while (vm_map_find(map, &addr, size) != KERN_SUCCESS) {
			vm_map_unlock(map);
			(void) tsleep((caddr_t)map, PVM, "kmemspc", 0);
			vm_map_lock(map);
		}
	}

	/* just reserve kernel address space or address space with appropriate object/offset allocation  */
	if (flags & M_SPACE_ONLY) {
		offset = 0;
		object = NULL;
	} else {
		if (map == kmem_map || map == mb_map || map == buf_map) {
			object = kmem_object;
			offset = addr - vm_map_min(kmem_map);
		} else {
			object = kernel_object;
			offset = addr - vm_map_min(kernel_map);
		}
		vm_object_reference(object);
	}
	vm_map_insert(map, object, offset, addr, addr + size);
	me = map->hint;
	vm_map_unlock(map);

	/*
	 * Allocate requested memory resources (if any).
	 */

	/* just want empty, default-pagable address space */
	if (flags & M_SPACE_ONLY) {
		splx(pl);
		return(addr);
	}

	for (i = 0; i < size; i += PAGE_SIZE) {

		/* allocate a page, either waiting for it or not */
		if (flags & M_NOWAIT)
			m = vm_page_alloc(object, offset + i,  flags&M_IO);
		else
			while ((m = vm_page_alloc(object, offset+i, flags&M_IO)) == NULL)
				vm_page_wait("kmempgs", atop(size - i));

		/* insufficient memory for request, free all and return */
		if (m == NULL) {
			vmspace_delete(&kernspace, (caddr_t)addr, size);
			wakeup((caddr_t)map);
			splx(pl);
			return(0);
		}

		/* initialize the memory to zero? */
		if (flags & M_ZERO_IT)
			pmap_zero_page(m->phys_addr);

		/* manually wire and map pages if part of kmem */
		m->busy = FALSE;
		if (map == kmem_map || map == mb_map ||  map == buf_map) {
			vm_page_wire(m);
			pmap_enter(map->pmap, addr + i, VM_PAGE_TO_PHYS(m),
			    VM_PROT_DEFAULT, TRUE, AM_NONE);
		}
	}

	/* wire map entry and unlock map */
	if (map == kmem_map || map == mb_map || map == buf_map)
		me->wired_count++;
	else
		(void) vmspace_notpageable(&kernspace, (caddr_t)addr, size);

	vm_map_simplify(map, addr);
	splx(pl);
	return(addr);
}

/*
 * return VA of mapped pages of spec_object:offset:size, with flags options, or 0 because can't
 * M_FIND - if all pages found and mapped, return VA otherwise 0 if fail, (then read or modify/write, kmem_free)
 * M_ALLOCATE - insure all pages allocated, return VA otherwise 0 because not enough space, (then do i/o) 
 */
vm_offset_t
buf_alloc(caddr_t *spec_object, vm_offset_t  offset, vm_size_t size, int flags)
{
	vm_map_t map = buf_map;
	vm_offset_t addr, i;
	vm_page_t m;
	vm_object_t object;
	vm_map_entry_t me;
	int pl;

	pl = splhigh();

	/* is request larger than can ever be satisfied with this map */
	if (size > map->max_offset - map->min_offset)
		panic("buf_alloc: map smaller than allocation");

	/* locate a fresh portion of kernel address space for address allocation */
	size = round_page(size);
	addr = vm_map_min(map);
	if (flags & M_NOWAIT) {
		if (lock_try_write(&map->lock) == FALSE)
			return(0);
		if (vm_map_find(map, &addr, size) != KERN_SUCCESS) {
			vm_map_unlock(map);
			splx(pl);
			return (0);
		}
	} else {
		vm_map_lock(map);
		while (vm_map_find(map, &addr, size) != KERN_SUCCESS) {
			vm_map_unlock(map);
			(void) tsleep((caddr_t)map, PVM, "bufspc", 0);
			vm_map_lock(map);
		}
	}

	if (*spec_object == 0)
		*(vm_object_t *)spec_object = vm_object_allocate(round_page(0xffff0000 /*spec.vattr.va_size*/));
	object = *(vm_object_t *)spec_object;
		
	/* just reserve kernel address space or address space with appropriate object/offset allocation  */
	if (flags & M_SPACE_ONLY) {
		offset = 0;
		object = NULL;
	} else	vm_object_reference(object);

	/* bind mapped address to object at offset */
	vm_map_insert(map, object, offset, addr, addr + size);
	me = map->hint;
	vm_map_unlock(map);

	/*
	 * Allocate requested memory resources (if any).
	 */

	/* just want empty, default-pagable address space */
	if (flags & M_SPACE_ONLY) {
		splx(pl);
		return(addr);
	}

	/* acquire physical memory pages */
	for (i = 0; i < size; i += PAGE_SIZE) {

		/* if page isn't present, allocate one */
		if ((m = vm_page_lookup(object, offset + i)) == NULL) {

			/* allocate a page, either waiting for it or not */
			if (flags & M_NOWAIT)
				m = vm_page_alloc(object, offset + i,  flags&M_IO);
			else
				while ((m = vm_page_alloc(object, offset+i, flags&M_IO)) == NULL)
					vm_page_wait("bufpgs", atop(size - i));
		}

		/* insufficient memory for request, return */
		if (m == NULL) {
#ifdef nope
			vmspace_delete(&kernspace, (caddr_t)addr, size);
			wakeup((caddr_t)map);
#endif
			splx(pl);
			return(0);
		}

		/* initialize the memory to zero? */
		if (flags & M_ZERO_IT)
			pmap_zero_page(m->phys_addr);

		/* manually wire and map pages if part of kmem */
		m->busy = FALSE;
		vm_page_wire(m);
		pmap_enter(map->pmap, addr + i, VM_PAGE_TO_PHYS(m), VM_PROT_DEFAULT, TRUE, AM_NONE);
	}

	/* wire map entry and unlock map */
	me->wired_count++;
	vm_map_simplify(map, addr);
	splx(pl);
	return(addr);
}

/*
 * Map an (already wired) region of a process into kernel memory. XXX
 */
vm_offset_t
kmem_mmap(vm_map_t map, vm_offset_t addr, vm_size_t size)
{
	vm_offset_t sva = trunc_page(addr), eva = round_page(addr + size - 1);
	vm_offset_t uva, kva, pa;

#ifdef DIAGNOSTIC
	if (curproc == 0)
		panic("kmem_map: no process");
#endif

	/* grab some kernel address space */
	if ((kva = kmem_alloc(map, eva - sva, M_SPACE_ONLY)) == 0)
		panic("kmem_map: map too small");

	/* force user pages into the kernel's address space XXX! */
	for (uva = sva; uva < eva ; uva += PAGE_SIZE) {
		if ((pa = pmap_extract(&curproc->p_vmspace->vm_pmap, uva)) == 0)
			panic("kmem_map: no physical address");
		pmap_enter(kernel_pmap, kva + (uva - sva), trunc_page(pa),
			   VM_PROT_READ|VM_PROT_WRITE, TRUE, AM_NONE);
	}

	/* return corresponding address in kernel virtual address space */
	return (kva + addr - sva);
}

/*
 * Free a portion of the kernel's virtual address space to reclaim
 * its resources.
 */
void
kmem_free(vm_map_t map, vm_offset_t addr, vm_size_t size)
{
	int pl = splhigh();

	/* free kernel address space and mem./addr. trans., if any */
	vm_map_lock(map);
	(void) vm_map_delete(map, trunc_page(addr), round_page(addr + size - 1));

	/* if any address space needed, wakeup any waiters for kernel space */
	wakeup((caddr_t)map);

	/* simplify map after deletion */
	vm_map_unlock(map);
	vm_map_simplify(map, trunc_page(addr));
	splx(pl);
}

/*
 * Isolate a region of kernel address space for use via a seperate map.  XXX
 */
vm_map_t
kmem_suballoc(vm_size_t size, boolean_t pageable)
{
	vm_map_t map;
	vm_offset_t min, max;

	/* locate a region of kernel address space */
	size = round_page(size);
	min = (vm_offset_t) vm_map_min(kernel_map);
	vm_map_lock(kernel_map);
	if (vm_map_find(kernel_map, &min, size) != KERN_SUCCESS)
		panic("kmem_suballoc");

	/* allocate the region of address space */
	max = min + size;
	(void)vm_map_insert(kernel_map, NULL, (vm_offset_t) 0, min, max);
	vm_map_unlock(kernel_map);
	pmap_reference(kernel_pmap);

	/* create a new submap */
	if ((map = vm_map_create(kernel_pmap, min, max, pageable)) == NULL)
		panic("kmem_suballoc: cannot create a map");
	if (vm_map_submap(kernel_map, min, max, map) != KERN_SUCCESS)
		panic("kmem_suballoc: unable to assign region to the map");

	return (map);
}

/*
 * Convert kernel virtual address (KVA) to physical address (PA) XXX
 */
vm_offset_t
kmem_phys(vm_offset_t kva)
{
	vm_offset_t pa;

	/* valid KVA ? */
	if (kva < vm_map_min(kernel_map) || kva > vm_map_max(kernel_map))
		panic("kmem_phys: not a kernel virtual address");

	/* has a page? */
	pa = pmap_extract(kernel_pmap, kva);
	if (pa == 0)
		panic("kmem_phys: no physical address");

	return (pa);
}
