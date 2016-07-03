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
 *	$Id: page.c,v 1.1 94/10/19 17:37:20 bill Exp $
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

/* Physical memory page allocation via logical page abstraction.  */

#include "sys/param.h"
#include "sys/errno.h" 	/* for inlines */
#include "proc.h"
#ifdef	KTRACE_notyet
#include "sys/ktrace.h"
#endif
#include "vm.h"
#include "vm_pageout.h"
#include "prototypes.h"

/*
 * Each unit of physical memory has a logical page (vm_page) entry.
 */

/* allocate pages are located by logical "name" (obj/offset) via hash table */
queue_head_t	*vm_page_buckets;		/* hash bucket array */
int		vm_page_bucket_count = 0;	/* bucket array length */
int		vm_page_hash_mask;	/* address mask for hash function */

/* storage is allocated in units of logical pages, which are integral HW pages */
vm_size_t	page_size, page_mask;		/* logical page size */
int		page_shift;

/* pages are queued for use (active), reclaimation (inactive), or reuse (free) */
queue_head_t	vm_page_queue_free;
queue_head_t	vm_page_queue_active, vm_page_queue_inactive;

/* managed pages have logical page entries in an array, indexed by phys addr */
vm_page_t	vm_page_array;
long		first_page;
long		last_page;
vm_offset_t	first_phys_addr;
vm_offset_t	last_phys_addr;

/* each page queue has a count of pages contained within */
int	vm_page_free_count;
int	vm_page_free_io_count;	/* XXX */
int	vm_page_active_count;
int	vm_page_inactive_count;
int	vm_page_free_target = 0;
int	vm_page_free_min = 0;
int	vm_page_inactive_target = 0;

/* pages may be wired to keep them from being reclaimed */
int	vm_page_wire_count;

/*static void vm_page_insert(vm_page_t mem, vm_object_t object,
	vm_offset_t offset);*/
/* "internal" functions used externally by device_pager:
static void vm_page_remove(vm_page_t mem);
static void vm_page_init(vm_page_t mem, vm_object_t object, vm_offset_t offset);
*/

/* set logical page size, which is an integral number of hardware pages */
void
vm_set_page_size(int sz)
{
	page_size = 4096; /*sz;*/
	page_mask = page_size - 1;

	if ((page_mask & page_size) != 0)
		panic("vm_set_page_size: size not a power of two");

	if (page_size % NBPG != 0)
		panic("vm_set_page_size: size not an integral number of cpu pages ");

	for (page_shift = 0; ; page_shift++)
		if ((1 << page_shift) == page_size)
			break;
}

/* Initialize the array of logical pages. */
void
vm_page_startup(void /*vm_offset_t spa, vm_offset_t epa, vm_offset_t sva*/)
{
extern vm_offset_t avail_start, avail_end, virtual_avail;
	vm_offset_t spa = avail_start, epa = avail_end, sva = virtual_avail,
		mapped, new_start, pa;
	vm_page_t m;
	queue_t	bucket;
	vm_size_t npages;
	int i;

	/* Initialize the queue headers as being empty. */
	queue_init(&vm_page_queue_free);
	queue_init(&vm_page_queue_active);
	queue_init(&vm_page_queue_inactive);

	/*
	 * find the next power of 2 number of hash buckets to
	 * cover the range of managed pages and set hash table
	 * address mask.
	 */
	vm_page_buckets = (queue_t) sva;
	bucket = vm_page_buckets;
	if (vm_page_bucket_count == 0) {
		vm_page_bucket_count = 1;
		while (vm_page_bucket_count < atop(epa - spa))
			vm_page_bucket_count <<= 1;
	}
	vm_page_hash_mask = vm_page_bucket_count - 1;

	/* allocate and clear hash buckets from sva/spa */
	new_start = round_page(((queue_t)spa) + vm_page_bucket_count);
	mapped = sva;
	sva = pmap_map(mapped, spa, new_start,
			VM_PROT_READ|VM_PROT_WRITE);
	spa = new_start;
	memset((caddr_t) mapped, 0, sva - mapped);
	mapped = sva;

	/* initialize hash buckets each as empty queues */
	for (i = vm_page_bucket_count; i--;)
{ queue_init(bucket); bucket++; }	/* fix goddamn macros */

	/* find the number of logical pages to be managed */
	epa = trunc_page(epa);
	vm_page_free_count = npages =
	    howmany(epa - spa, PAGE_SIZE + sizeof(struct vm_page));

	/* find boundaries of logical page array and managed pages */
	m = vm_page_array = (vm_page_t) sva;
	first_page = spa;
	first_page += npages * sizeof(struct vm_page);
	first_page = atop(round_page(first_page));
	last_page  = first_page + npages - 1;
	first_phys_addr = ptoa(first_page);
	last_phys_addr  = ptoa(last_page) + page_mask;

	/* allocate & clear the logical page array from sva/spa */
	new_start = spa + (round_page(m + npages) - mapped);
	mapped = pmap_map(mapped, spa, new_start,
			VM_PROT_READ|VM_PROT_WRITE);
	spa = new_start;
	memset((caddr_t)m, 0, npages * sizeof(*m));

	/* initialize each logical page in the array */
	pa = first_phys_addr;
	while (npages--) {
		m->copy_on_write = FALSE;
		m->wanted = FALSE;
		m->inactive = FALSE;
		m->active = FALSE;
		m->busy = FALSE;
		m->object = NULL;
		m->phys_addr = pa;
#ifdef	i386
		m->io = (pa < 16*1024*1024);	/* XXX */
#else
		m->io = FALSE;			/* XXX */
#endif
		if (m->io)
			vm_page_free_io_count++;
		queue_enter(&vm_page_queue_free, m, vm_page_t, pageq);
		m++;
		pa += PAGE_SIZE;
	}

	virtual_avail = mapped;
}

/* construct a hash of the low-order bits of the page offset/obj pair */
#define vm_page_hash(object, offset) \
    (((unsigned)object + (unsigned)atop(offset)) & vm_page_hash_mask)

/* insert a page into an object at an offset */
void
vm_page_insert(register vm_page_t mem, vm_object_t object, vm_offset_t offset)
{
	queue_t	bucket;
	int spl;

#ifdef	DIAGNOSTIC
	if (mem->tabled)
		panic("vm_page_insert: already inserted");
#endif

	/* assign page to object at an offset */
	mem->object = object;
	mem->offset = offset;

	/* enter the page into the hash table queue */
	bucket = &vm_page_buckets[vm_page_hash(object, offset)];
	spl = splimp();
	queue_enter(bucket, mem, vm_page_t, hashq);

	/* enter the page into the object's queue of pages */
	queue_enter(&object->memq, mem, vm_page_t, listq);
	(void) splx(spl);
	mem->tabled = TRUE;
	object->resident_page_count++;
}

/* remove page from an object */
/* static */ void
vm_page_remove(vm_page_t mem)
{
	queue_t bucket;
	int spl;

	/* if not in an object, don't need to remove from an object */
	if (!mem->tabled)
		return;

	/* remove page from lookup hash table */
	bucket = &vm_page_buckets[vm_page_hash(mem->object, mem->offset)];
	spl = splimp();
	queue_remove(bucket, mem, vm_page_t, hashq);

	/* remove page from object's queue of pages */
	queue_remove(&mem->object->memq, mem, vm_page_t, listq);
	(void) splx(spl);
	mem->object->resident_page_count--;
	mem->tabled = FALSE;
}

/* locate a page by object/offset using hash table queues */
vm_page_t
vm_page_lookup(vm_object_t object, vm_offset_t offset)
{
	vm_page_t mem;
	queue_t	bucket;
	int spl;

	/* find the bucket queue header for this object/offset */
	bucket = &vm_page_buckets[vm_page_hash(object, offset)];

	/* linear search of the queue for the desired page */
	spl = splimp();
	mem = (vm_page_t) queue_first(bucket);
	while (!queue_end(bucket, (queue_entry_t) mem)) {
		if ((mem->object == object) && (mem->offset == offset)) {
			splx(spl);
			return(mem);
		}
		mem = (vm_page_t) queue_next(&mem->hashq);
	}
	splx(spl);
	return(NULL);
}


/* associate page with new object/offset pair */
void
vm_page_rename(vm_page_t mem, vm_object_t new_object, vm_offset_t new_offset)
{
	if (mem->object == new_object)
		return;

    	vm_page_remove(mem);
	vm_page_insert(mem, new_object, new_offset);
}

/* allocate a page (if available) to the object at the offset */
vm_page_t
vm_page_alloc(vm_object_t object, vm_offset_t offset, boolean_t io)
{
	vm_page_t mem;
	int spl;

#ifdef KTRACE_notyet
	if (curproc && KTRPOINT(curproc, KTR_VM))
		ktrvm(curproc->p_tracep, KTVM_PAGE, object, offset, 0, 0);
#endif
	spl = splimp();				/* XXX */
	if (queue_empty(&vm_page_queue_free)) {
	none:
mem = NULL;
goto jab;
		/*splx(spl);
		return(NULL);*/
	}

	mem = (vm_page_t) queue_first(&vm_page_queue_free);
	while (!queue_end(&vm_page_queue_free, (queue_entry_t) mem)) {
if (mem->tabled)
panic("vm_page_alloc");
		if (!io || mem->io)
			goto found;
		mem = (vm_page_t) queue_next(&mem->pageq);
	}
	goto none;

found:
	queue_remove(&vm_page_queue_free, mem, vm_page_t, pageq);
	vm_page_free_count--;
	if (mem->io)
		vm_page_free_io_count--;
	vm_page_init(mem, object, offset);
jab:

	/* schedule pageout if too few free (or soon to be free) pages */
	if (vm_pages_needed || (vm_page_free_count < vm_page_free_min) ||
	    (vm_page_free_io_count < vm_page_free_min) ||
	    ((vm_page_free_count < vm_page_free_target) &&
	    (vm_page_inactive_count < vm_page_inactive_target)))
		wakeup((caddr_t)&proc0);

	splx(spl);
	return(mem);
}

/* release page to free memory queue */
void
vm_page_free(register vm_page_t mem)
{
	/* remove from object/offset */
	vm_page_remove(mem);

	/* remove from active queue */
	if (mem->active) {
		queue_remove(&vm_page_queue_active, mem, vm_page_t, pageq);
		mem->active = FALSE;
		vm_page_active_count--;
	}

	/* remove from inactive queue */
	if (mem->inactive) {
		queue_remove(&vm_page_queue_inactive, mem, vm_page_t, pageq);
		mem->inactive = FALSE;
		vm_page_inactive_count--;
	}

	/* if a "real" memory page, add to free queue */
	if (!mem->fictitious) {
		int spl = splimp();

		queue_enter(&vm_page_queue_free, mem, vm_page_t, pageq);
		vm_page_free_count++;
		if (vm_pages_needed && vm_pages_needed > vm_page_free_count)
			wakeup((caddr_t) &vm_page_free_count);
		if (mem->io)
			vm_page_free_io_count++;
		splx(spl);
	}
}

/* increase map entry wiring reference count on page. */
void
vm_page_wire(register vm_page_t mem)
{
	/* if this is the first wiring, isolate from pageout daemon */
	if (mem->wire_count++ == 0) {

		/* remove from active queue */
		if (mem->active) {
			queue_remove(&vm_page_queue_active, mem, vm_page_t,
						pageq);
			vm_page_active_count--;
			mem->active = FALSE;
		}

		/* remove from inactive queue */
		if (mem->inactive) {
			queue_remove(&vm_page_queue_inactive, mem, vm_page_t,
						pageq);
			vm_page_inactive_count--;
			mem->inactive = FALSE;
		}

		vm_page_wire_count++;
	}
}

/* decrease map entry wiring reference count on page. */
void
vm_page_unwire(register vm_page_t mem)
{
	/* if page becomes unwired, activate the page */
	if (--mem->wire_count == 0) {
		queue_enter(&vm_page_queue_active, mem, vm_page_t, pageq);
		vm_page_active_count++;
		mem->active = TRUE;
		vm_page_wire_count--;
	}
}

/* deactivate a page by removing it from reuse. */
void
vm_page_deactivate(register vm_page_t m)
{
	/* ignore already inactive or wired pages */
	if (!m->inactive && m->wire_count == 0) {

		/* loose reference bit and any adr. trans. to detect reuse */
                pmap_clear_reference(VM_PAGE_TO_PHYS(m));
		pmap_page_protect(VM_PAGE_TO_PHYS(m), VM_PROT_NONE);

		/* remove from active queue */
		if (m->active) {
			queue_remove(&vm_page_queue_active, m, vm_page_t, pageq);
			m->active = FALSE;
			vm_page_active_count--;
		}

		/* insert in inactive queue */
		queue_enter(&vm_page_queue_inactive, m, vm_page_t, pageq);
		m->active = FALSE;
		m->inactive = TRUE;
		vm_page_inactive_count++;

		/* has page been modified? */
		if (pmap_is_modified(VM_PAGE_TO_PHYS(m)))
			m->clean = FALSE;
		m->laundry = !m->clean;
	}
}

/* activate an inactive or off-queue page */
void
vm_page_activate(register vm_page_t m)
{
	/* remove from inactive queue */
	if (m->inactive) {
		queue_remove(&vm_page_queue_inactive, m, vm_page_t, pageq);
		vm_page_inactive_count--;
		m->inactive = FALSE;
	}

	/* ignore wired pages */
	if (m->wire_count)
		return;

#ifdef	DIAGNOSTIC
	if (m->active)
		panic("vm_page_activate: already active");
#endif

	/* enter active queue */
	queue_enter(&vm_page_queue_active, m, vm_page_t, pageq);
	m->active = TRUE;
	vm_page_active_count++;
}

#if	defined(DEBUG) || defined(DDB)
/*vm_page_array_check() {
	vm_page_t p;

	for(p = vm_page_array ; p < vm_page_array + (last_page - first_page); p++) {
*/
vm_Page_check(vm_page_t p) {
	if ( (((unsigned int) p) < ((unsigned int) &vm_page_array[0])) ||
	     (((unsigned int) p) > ((unsigned int) &vm_page_array[last_page-first_page])))
{
vm_page_print(p);
	return(0);
}
return(1);
}
#undef vm_page_check
vm_page_check(vm_page_t p) {
	if ( (((unsigned int) p) < ((unsigned int) &vm_page_array[0])) ||
	     (((unsigned int) p) > ((unsigned int) &vm_page_array[last_page-first_page])))
{
vm_page_print(p);
		/*panic("not vm_page");*/
Debugger();
}
}

vm_page_print(vm_page_t p) {
	if ( (((unsigned int) p) < ((unsigned int) &vm_page_array[0])) ||
	     (((unsigned int) p) > ((unsigned int) &vm_page_array[last_page-first_page]))){
		printf("Page %x: not in page array\n ");
return;
}
	printf("Page %x: obj/off %x/%x list %x/%x phys %x ",
	    p, p->object, p->offset, p->listq.next, p->listq.prev,
	    p->phys_addr);
/*pageq
hashq
listq*/
	/* booleans first */
	if (p->busy) printf("busy ");
	if (p->tabled) printf("tabled ");
	if (p->fictitious) printf("ficitious ");
	if (p->laundry) printf("laundry ");
	if (p->active) printf("active ");
	if (p->inactive) printf("inactive ");
	if (p->clean) printf("clean ");
	if (p->fake) printf("fake ");
	if (p->copy_on_write) printf("copy_on_write ");

	/* decimal fields */
	if (p->page_lock) printf("page_lock %d ", p->page_lock);
	if (p->unlock_request) printf("unlock_request %d ", p->unlock_request);
	if (p->wire_count) printf("wire_count %d ", p->wire_count);

	printf("\n");
}

vm_page_queues_print() {
	
	printf("free (%d):\n", vm_page_free_count);
	vm_page_queue_print(&vm_page_queue_free);
	printf("inactive(%d):\n", vm_page_inactive_count);
	vm_page_queue_print(&vm_page_queue_inactive);
	printf("active(%d):\n", vm_page_active_count);
	vm_page_queue_print(&vm_page_queue_active);
}

vm_page_queue_print(queue_t q) {
	int cnt=0;
	vm_page_t mem;

	mem = (vm_page_t) queue_first(q);
	while (!queue_end(q, (queue_entry_t) mem)) {
		vm_page_print(mem);
		mem = (vm_page_t) queue_next(&mem->pageq);
		if (++cnt > 10) {
			if(pg("hit return for more") != '\n')
				return;
			cnt = 0;
		}
	}
}
#endif
