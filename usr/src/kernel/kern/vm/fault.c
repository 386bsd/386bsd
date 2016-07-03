/* 
 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * The Mach Operating System project at Carnegie-Mellon University.
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
 *	$Id: fault.c,v 1.1 94/10/19 17:37:12 bill Exp $
 *
 *
 * Copyright (c) 1987, 1990 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Authors: Avadis Tevanian, Jr., Michael Wayne Young
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/* Address space fault handler */

#include "sys/param.h"
#include "uio.h"
#include "proc.h"
#include "resourcevar.h"
#include "vmmeter.h"
#ifdef	KTRACE
#include "sys/ktrace.h"
#endif
#include "vm.h"
#include "vmspace.h"
#include "vm_pageout.h"
#include "vm_fault.h"
#define	__NO_INLINES
#include "prototypes.h"

/*
 * Evaluate a faulting reference at a given protection on an address space
 * and alter the contents of the address space to alleviate the condition
 * causing the fault. If required, allocate a page, load it, and add it to the
 * address translation maps for the associated process. Return the success
 * or failure of the operation with an error code.
 */
int
vm_fault(vm_map_t map, vm_offset_t vaddr, vm_prot_t fault_type,
    boolean_t change_wiring)
{
	vm_object_t first_object, object, next_object, copy_object;
	vm_offset_t first_offset, offset;
	vm_map_entry_t entry;
	vm_page_t first_m, m, old_m;
	vm_prot_t prot;
	int result, am;
	boolean_t wired, su, lookup_still_valid, page_exists;

	/* record fault occurance */
	cnt.v_faults++;
	if(curproc)
		curproc->p_flag |= SPAGE;

	/* fault retry recovery operations */
#define	FREE_PAGE(m)	{				\
	PAGE_WAKEUP(m);					\
	vm_page_free(m);				\
}

#define	RELEASE_PAGE(m)	{				\
	PAGE_WAKEUP(m);					\
	vm_page_activate(m);				\
}

#define	UNLOCK_MAP	{				\
	if (lookup_still_valid) {			\
		vm_map_lookup_done(map, entry);		\
		lookup_still_valid = FALSE;		\
	}						\
}

#define	UNLOCK_THINGS	{				\
	object->paging_in_progress--;			\
	if (object != first_object) {			\
		FREE_PAGE(first_m);			\
		first_object->paging_in_progress--;	\
	}						\
	UNLOCK_MAP;					\
}

#define	UNLOCK_AND_DEALLOCATE	{			\
	UNLOCK_THINGS;					\
	vm_object_deallocate(first_object);		\
}

    RetryFault: ;

	/* locate the vm elements associated with this fault */
	if ((result = vm_map_lookup(&map, vaddr, fault_type, &entry,
			&first_object, &first_offset,
			&prot, &wired, &su)) != KERN_SUCCESS) {
		if(curproc)
			curproc->p_flag &= ~SPAGE;
		return(result);
	}
	lookup_still_valid = TRUE;

#ifdef KTRACE
	if (curproc && KTRPOINT(curproc, KTR_VM) && &curproc->p_vmspace->vm_map == map)
		ktrvm(curproc->p_tracep, KTVM_FAULT, vaddr, fault_type, change_wiring, prot);
#endif

	if (wired)
		fault_type = prot;

	first_m = NULL;

	/* add a reference for vm_fault() and paging i/o to top object */
	first_object->ref_count++;
	first_object->paging_in_progress++;

	/* search obj shadow list at object/offset supplied, get resident page */
	object = first_object;
	offset = first_offset;

	while (TRUE) {

		/* resident page is already present? */
		m = vm_page_lookup(object, offset);
		if (m != NULL) {

			/* present, and in use, wait for it and retry. */
			if (m->busy) {
				cnt.v_intrans++;
				UNLOCK_THINGS;
				PAGE_WAIT(m, "fltpgbusy", !change_wiring);
				vm_object_deallocate(first_object);
				goto RetryFault;
			}

			/* if page locked for access, request unlock */
			if (fault_type & m->page_lock) {
#ifdef	DIAGNOSTIC
				if ((fault_type & m->unlock_request) != fault_type)
					panic("vm_fault: pager_data_unlock");
#endif
				UNLOCK_THINGS;
				PAGE_WAIT(m, "fltpgunlck", !change_wiring);
				vm_object_deallocate(first_object);
				goto RetryFault;
			}

			/* take page "off-queue" so pageout can't get it */
			if (m->inactive) {
				queue_remove(&vm_page_queue_inactive, m,
						vm_page_t, pageq);
				m->inactive = FALSE;
				vm_page_inactive_count--;
				cnt.v_pgfrec++;
			} 
			if (m->active) {
				queue_remove(&vm_page_queue_active, m,
						vm_page_t, pageq);
				m->active = FALSE;
				vm_page_active_count--;
			}

			/* exclusive access to this page, others will block */
			m->busy = TRUE;
			cnt.v_pgrec++;
			if (curproc && &curproc->p_vmspace->vm_map == map)
				curproc->p_stats->p_ru.ru_minflt++;
			break;
		}

		/* object with pager or first object needs a page */
		if (((object->pager != NULL) && (!change_wiring || wired))
		    || (object == first_object)) {

			/* have any pages for a user process? */
			if (curproc && &curproc->p_vmspace->vm_map == map
			    && chk4space(1) == 0) {
				UNLOCK_AND_DEALLOCATE;
				uprintf("out of memory; aborted page fault\n");
				if(curproc)
					curproc->p_flag &= ~SPAGE;
				return(KERN_NO_SPACE);
			}

			/* allocate a page into object/offset */
			m = vm_page_alloc(object, offset, wired);
			if (m == NULL) {
				UNLOCK_AND_DEALLOCATE;
				vm_page_wait("fault", 1);
				goto RetryFault;
			}
		}

		/* allocated page needs to be filled from pager? */
		if ((object->pager != NULL) && (!change_wiring || wired)) {
			int rv;

			/* have page, release map */
			UNLOCK_MAP;

			/* obtain pages external contents */
			rv = vm_pager_get(object->pager, m, TRUE);
			if (rv == VM_PAGER_OK) {

				/* see if page was exchanged for another */
				m = vm_page_lookup(object, offset);

				/* validate contents, and finish */
				m->fake = FALSE;
				pmap_clear_modify(VM_PAGE_TO_PHYS(m));
				cnt.v_pgin++;
				cnt.v_pgpgin++;
				if (curproc && &curproc->p_vmspace->vm_map == map)
					curproc->p_stats->p_ru.ru_majflt++;
				break;
			}

			/* leave page in top object anyways */
			if (object != first_object) {

				/* page cannot be used (error or out of range) */
				FREE_PAGE(m);
				if (rv == VM_PAGER_BAD) {
					UNLOCK_AND_DEALLOCATE;
					if (curproc)
						curproc->p_flag &= ~SPAGE;
					return(KERN_PROTECTION_FAILURE); /* XXX */
				}
			}
		}

		/* remember first page */
		if (object == first_object)
			first_m = m;

		/* follow object shadow chain, relocating offset */
		offset += object->shadow_offset;
		next_object = object->shadow;

		/* if end of shadow chain, zero fill top object page */
		if (next_object == NULL) {

			/* if not on first object, refer to first object */
			if (object != first_object) {
				object->paging_in_progress--;

				object = first_object;
				offset = first_offset;
				m = first_m;
			}

			first_m = NULL;
			pmap_zero_page(m->phys_addr);
			cnt.v_zfod++;
			m->fake = FALSE;
			break;
		}

		/* exchange paging reference from current to next object */
		else {
			if (object != first_object)
				object->paging_in_progress--;
			object = next_object;
			object->paging_in_progress++;
		}
	}
#ifdef DIAGNOSTIC
	if (m == NULL || m->active || m->inactive || !m->busy)
		panic("vm_fault: no page, active, inactive or not busy after main loop");
#endif

	/* save page that would be copied */
	old_m = m;

	/* deal with private shadow. if page isn't in the top object ... */
	if (object != first_object) {

		/* ... and we are writing the page, copy it into the top object */
	    	if (fault_type & VM_PROT_WRITE) {

			/* copy into blocking page prior to removing it  */
			pmap_copy_page(m->phys_addr, first_m->phys_addr);
			first_m->fake = FALSE;

			/* lose all references to the "old" page & discard */
			vm_page_deactivate(m);
			pmap_page_protect(VM_PAGE_TO_PHYS(m), VM_PROT_NONE);
			PAGE_WAKEUP(m);
			object->paging_in_progress--;

			/* replace old page with "new" page in top object */
			vm_stat.cow_faults++;
			m = first_m;
			object = first_object;
			offset = first_offset;

			/* collapse object list */
			object->paging_in_progress--;
			vm_object_collapse(object);
			object->paging_in_progress++;
		}
		/* if not writing, restrict access on entry */
		else
		    	prot &= ~VM_PROT_WRITE;
	}
#ifdef	DIAGNOSTIC
	if (m->active || m->inactive)
		panic("vm_fault: active or inactive before copy object handling");
#endif

	/* if we have a copy object, propogate page to copy before writing */
	while ((copy_object = first_object->copy) != NULL) {
		vm_offset_t copy_offset;
		vm_page_t copy_m;

		/* don't bother for read access, primary RO object works */
		if ((fault_type & VM_PROT_WRITE) == 0) {
			prot &= ~VM_PROT_WRITE;
			break;
		}

		/* generate a unique page copy for modifications */
		else {

			/* hold a reference on the copy object to prevent it from being deallocated */
			copy_object->ref_count++;

			/* is there a corresponding page in the copy object? */
			copy_offset = first_offset - copy_object->shadow_offset;
			copy_m = vm_page_lookup(copy_object, copy_offset);
			if (page_exists = (copy_m != NULL)) {

				/* if page is busy by another process... */
				if (copy_m->busy) {
					/* release the page, loose the reference */
					RELEASE_PAGE(m);
					cnt.v_intrans++;
					copy_object->ref_count--;

					/* unlock & wait for page, then retry */
					UNLOCK_THINGS;
					PAGE_WAIT(copy_m, "fltcpypgbsy", !change_wiring);
					vm_object_deallocate(first_object);
					goto RetryFault;
				}
			}

			/* if page not resident & pager, check for ext page */ 
			if (!page_exists) {

				/* insert place holder page in copy object */
				copy_m = vm_page_alloc(copy_object,
				    copy_offset, 0);

				/* no page available, back out & retry. */
				if (copy_m == NULL) {
					RELEASE_PAGE(m);
					copy_object->ref_count--;
					UNLOCK_AND_DEALLOCATE;
					vm_page_wait("faultcpy", 1);
					goto RetryFault;
				}

				/* if a pager see if the page exists externally */
			 	if (copy_object->pager != NULL) {
					UNLOCK_MAP;

					/* external page exists? */
					page_exists =
					  vm_pager_has_page(copy_object->pager,
					    (copy_offset + copy_object->paging_offset));

					/* copy object different or unreferenced, retry */
					if (copy_object->shadow != object ||
					    copy_object->ref_count == 1) {
						/* drop page & object reference */
						FREE_PAGE(copy_m);
						vm_object_deallocate(copy_object);
						continue;
					}

					/* page exists, discard placeholder */
					if (page_exists) {
						FREE_PAGE(copy_m);
					}
				}
			}

			/* if page does not exist, must copy into the copy object */
			if (!page_exists) {
				pmap_copy_page(m->phys_addr, copy_m->phys_addr);

				/* loose all refs to prior page */
				pmap_page_protect(VM_PAGE_TO_PHYS(old_m),
				    VM_PROT_NONE);

				/* page valid & arrange to write external page */
				copy_m->clean = FALSE;
				copy_m->fake = FALSE;

				/* XXX must be active to become deactivated */
				vm_page_activate(copy_m);
				PAGE_WAKEUP(copy_m);
			}
			/* loose accquired reference as we are done with copy object */
			copy_object->ref_count--;
		}
		break;
	}
#ifdef DIAGNOSTIC
	if (m->active || m->inactive)
		panic("vm_fault: active or inactive before retrying lookup");
#endif

	/* if we had to lose map's lock accross a block, relookup */
	if (!lookup_still_valid) {
		vm_object_t	retry_object;
		vm_offset_t	retry_offset;
		vm_prot_t	retry_prot;

		/* only ask for read protection to avoid write lock for wiring*/
		result = vm_map_lookup(&map, vaddr,
		   fault_type & ~VM_PROT_WRITE, &entry, &retry_object,
		   &retry_offset, &retry_prot, &wired, &su);

		/* lookup fails, abandon the page and fail */
		if (result != KERN_SUCCESS) {
			RELEASE_PAGE(m);
			UNLOCK_AND_DEALLOCATE;
			if (curproc)
				curproc->p_flag &= ~SPAGE;
			return(result);
		}
		lookup_still_valid = TRUE;

		/* map entry has different object/offset pair, retry */
		if ((retry_object != first_object) ||
		    (retry_offset != first_offset)) {
			RELEASE_PAGE(m);
			UNLOCK_AND_DEALLOCATE;
			goto RetryFault;
		}

		/* restrict protection as the minimum of current & last lookup */
		prot &= retry_prot;
	}

	/* XXX determine class of address mapping. */
	am = AM_NONE;
	if (curproc && &curproc->p_vmspace->vm_map == map) {
		if (object->can_persist)
			am = AM_TEXT;
		else if (vaddr >= (vm_offset_t)curproc->p_vmspace->vm_maxsaddr)
			am = AM_STACK;
		else
			am = AM_DATA;
	}

#ifdef DIAGNOSTIC
	if (m->active || m->inactive)
		panic("vm_fault: active or inactive before pmap_enter");
#endif

	/* insert new address translation entry now */
	pmap_enter(map->pmap, vaddr, VM_PAGE_TO_PHYS(m), 
			prot & ~(m->page_lock), wired, am);

	/* if wiring change, adjust wiring for this map entry */
	if (change_wiring) {
		if (wired)
			vm_page_wire(m);
		else
			vm_page_unwire(m);
	}
	/* otherwise, activate page and let pageout manage it.  */
	else
		vm_page_activate(m);

	/* unlock and return success */
	PAGE_WAKEUP(m);
	UNLOCK_AND_DEALLOCATE;
	if(curproc)
		curproc->p_flag &= ~SPAGE;
	return(KERN_SUCCESS);
}

/* Wire down a range of virtual addresses in a map. */
void
vm_fault_wire(vm_map_t map, vm_offset_t start, vm_offset_t end)
{
	vm_offset_t va;

	/* tell pmap layer that an address region is to be wired */
	pmap_pageable(vm_map_pmap(map), start, end, FALSE);

	/* simulate the fault accross region by faulting pages */
	for (va = start; va < end; va += PAGE_SIZE)
		(void) vm_fault(map, va, VM_PROT_NONE, TRUE);
}

/* Unwire a range of virtual addresses in a map.  */
void
vm_fault_unwire(vm_map_t map, vm_offset_t start, vm_offset_t end)
{
	vm_offset_t va, pa;
	pmap_t pmap = vm_map_pmap(map);

	/* clear wiring from pages and address translation maps */
	for (va = start; va < end; va += PAGE_SIZE) {

		/* find physical address of page of virtual address */
		pa = pmap_extract(pmap, va);
		if (pa == (vm_offset_t) 0)
			panic("unwire: page not in pmap");

		/* unwire page in address translation map and page */
		pmap_change_wiring(pmap, va, FALSE);
		vm_page_unwire(PHYS_TO_VM_PAGE(pa));
	}

	/* tell pmap layer to remove wiring restriction on space */
	pmap_pageable(pmap, start, end, TRUE);
}

/* create dest. copy of a wired-down entry as part of a virtual copy */
void
vm_fault_copy_entry(vm_map_t dst_map, vm_map_t src_map,
    vm_map_entry_t dst_entry, vm_map_entry_t src_entry)
{
	vm_object_t dst_object, src_object;
	vm_offset_t dst_offset, src_offset, vaddr;
	vm_prot_t prot;
	vm_page_t dst_m, src_m;
	int am;

	src_object = src_entry->object.vm_object;
	src_offset = src_entry->offset;

	/* allocate a new destination object to hold pages */
	dst_object = vm_object_allocate(
	    (vm_size_t) (dst_entry->end - dst_entry->start));
	dst_entry->object.vm_object = dst_object;
	dst_entry->offset = 0;
	prot  = dst_entry->max_protection;

	/* clone the pages of the source into the destination object. */
	for (vaddr = dst_entry->start, dst_offset = 0;
	     vaddr < dst_entry->end;
	     vaddr += PAGE_SIZE, dst_offset += PAGE_SIZE) {

		/* allocate a unwired destination page */
		do {
			dst_m = vm_page_alloc(dst_object, dst_offset, 0);
			if (dst_m == NULL) {
				vm_page_wait("faultcpyent", 1);
			}
		} while (dst_m == NULL);

		/* copy a wired source page into the destination */
		src_m = vm_page_lookup(src_object, dst_offset + src_offset);
		if (src_m == NULL)
			panic("vm_fault_copy_wired: page missing");
		pmap_copy_page(src_m->phys_addr, dst_m->phys_addr);

		/* determine class of address mapping. */
		am = AM_NONE;
		if (curproc && &curproc->p_vmspace->vm_map == dst_map) {
			if (dst_object->can_persist)
				am = AM_TEXT;
			else if (vaddr >=
			    (vm_offset_t)curproc->p_vmspace->vm_maxsaddr)
				am = AM_STACK;
			else
				am = AM_DATA;
		}

		/* insert a non-wired address translation mapping */
		pmap_enter(dst_map->pmap, vaddr, VM_PAGE_TO_PHYS(dst_m),
		    prot, FALSE, am);

		/* activate and release page */
		vm_page_activate(dst_m);
		PAGE_WAKEUP(dst_m);
	}
}
