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
 *	$Id: object.c,v 1.1 94/10/19 17:37:18 bill Exp $
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

/* Resident contents of virtual address space via memory object abstraction. */

#include "sys/param.h"
#include "proc.h"
#include "malloc.h"
#include "vm.h"
#define __NO_INLINES
#include "prototypes.h"

/*
 * Virtual memory objects maintain the actual data associated with
 * allocated virtual memory, as a variable length segment of sparse memory pages.
 * These pages each correspond to a unique offset within the logically ordered
 * segment, which always starts at zero and ends at a maximum size.
 * Pages are managed by a seperate mechanism that allocates them onto
 * an object. At any time, a memory page may be uniquely identified by its
 * object/offset pair.
 *
 * Objects underly map entries, and are allocated by reference to faulted
 * pages. An object is only deallocated when all "references" are given up,
 * as when a map is deallocated. Objects may have a "pager" routine to provide
 * access to non-resident pages in a backing store by means of kernel I/O to
 * make them resident. The fault and pageout mechanisms use this to exchange
 * pages between memory/secondary storage. Objects can also use other objects
 * to hold to a subset of the segment contents; this can be used to "share"
 * common contents of many objects, or to uniquely record consecutive changed
 * sets of pages used in implementing a form of sharing known as copy-on-write.
 *
 */

/* statically allocated contents of kernel and kmem objects */
struct vm_object kernel_object_store, kmem_object_store;

#define	VM_OBJECT_HASH_COUNT	157

int		vm_cache_max = 100;	/* can patch if necessary */
queue_head_t	vm_object_hashtable[VM_OBJECT_HASH_COUNT];

long	object_collapses;
long	object_bypasses;

static void _vm_object_allocate(vm_size_t size, vm_object_t object);
static void vm_object_cache_trim(void);
/* static */ void vm_object_cache_clear();
/*static */ void vm_object_deactivate_pages(vm_object_t object);

/* initialize the memory object abstraction layer. */
void
vm_object_init(void)
{
	int	i;

	/* initialize empty cached object list */
	queue_init(&vm_object_cached_list);
	vm_object_count = 0;

	/* clear hash table of queue headers for vm_object_lookup() */
	for (i = 0; i < VM_OBJECT_HASH_COUNT; i++)
		queue_init(&vm_object_hashtable[i]);

	/* allocate & initialize an empty object to hold kernel pages */
	kernel_object = &kernel_object_store;
	_vm_object_allocate(VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS,
			kernel_object);
	kernel_object->ref_count = 0;

	/* allocate & initialize an empty object to hold kernel memory allocator pages */
	kmem_object = &kmem_object_store;
	_vm_object_allocate(VM_KMEM_SIZE + VM_MBUF_SIZE + /*VM_BUF_SIZE*/ (4*1024*1024), kmem_object);
	kmem_object->ref_count = 0;
}

/* dynamically allocate a new object to contain a desired sized segment */
vm_object_t
vm_object_allocate(vm_size_t size)
{
	vm_object_t	result;

	result = (vm_object_t)
		malloc((u_long)sizeof *result, M_VMOBJ, M_WAITOK);
	_vm_object_allocate(size, result);
	return(result);
}

/* initialize the object with default paramters */
static void
_vm_object_allocate(vm_size_t size, register vm_object_t object)
{
	/* empty termporary object */
	queue_init(&object->memq);
	object->ref_count = 1;
	object->resident_page_count = 0;
	object->size = size;
	object->can_persist = FALSE;
	object->paging_in_progress = 0;
	object->copy = NULL;

	/* no external contents */
	object->pager = NULL;
	object->internal = TRUE;
	object->paging_offset = 0;
	object->shadow = NULL;
	object->shadow_offset = (vm_offset_t) 0;

	vm_object_count++;
}

/* increase reference count on an object */
void
vm_object_reference(vm_object_t object)
{
	if (object == NULL)
		return;
	object->ref_count++;
}

/* release a reference on an object, deallocating if no more references */
void
vm_object_deallocate(vm_object_t object)
{
	vm_object_t	temp;

	/* walk the list of allocated objects */
	while (object != NULL) {

		/* reduce the reference count */
		if (--(object->ref_count) != 0)
			return;

		/* if object persists, cache and deactivate pages */
		if (object->can_persist) {
			queue_enter(&vm_object_cached_list, object,
				vm_object_t, cached_list);
			vm_object_cached++;
			vm_object_deactivate_pages(object);

			/* check for cache overflow */
			vm_object_cache_trim();
			return;
		}

		/* isolate from pagers to avoid new references to obj */
		vm_object_remove(object->pager);

		/* terminate this object, and then deallocate shadow */
		temp = object->shadow;
		vm_object_terminate(object);
		object = temp;
	}
}

/* reclaim an object, releasing all underlying abstractions */
void
vm_object_terminate(vm_object_t object)
{
	vm_page_t p;
	vm_object_t shadow_object;

	/* if this is the copy object for the shadow, clear */
	if ((shadow_object = object->shadow) != NULL) {
		if (shadow_object->copy == object)
			shadow_object->copy = NULL;
	}

	/* any I/O references outstanding, if so wait for conclusion */
	while (object->paging_in_progress != 0)
		(void) tsleep((caddr_t)object, PVM, "objterm", 0);

	/* isolate pages, making them "off queue" prior to free */
	p = (vm_page_t) queue_first(&object->memq);
	while (!queue_end(&object->memq, (queue_entry_t) p)) {

		/* release from active queue */
		if (p->active) {
			queue_remove(&vm_page_queue_active, p, vm_page_t,
						pageq);
			p->active = FALSE;
			vm_page_active_count--;
		}

		/* release from inactive queue */
		if (p->inactive) {
			queue_remove(&vm_page_queue_inactive, p, vm_page_t,
						pageq);
			p->inactive = FALSE;
			vm_page_inactive_count--;
		}

		p = (vm_page_t) queue_next(&p->listq);
	}
				
#ifdef	DIAGNOSTIC
	if (object->paging_in_progress != 0)
		panic("vm_object_deallocate: pageout in progress");
#endif

	/* force last update of modified pages to external storage */
	if (!object->internal)
		vm_object_page_clean(object, 0, 0);

	/* free objects pages */
	while (!queue_empty(&object->memq)) {
		p = (vm_page_t) queue_first(&object->memq);
		vm_page_free(p);
	}

	/* free pager (if any) */
	if (object->pager != NULL)
		vm_pager_deallocate(object->pager);
	object->pager = NULL;

	/* free object */
	free((caddr_t)object, M_VMOBJ);
	vm_object_count--;
}

/* find dirty pages and force them back to external storage */
void
vm_object_page_clean(vm_object_t object, vm_offset_t start, vm_offset_t end)
{
	vm_page_t p;

	/* no pager, nothing to do. */
	if (object->pager == NULL)
		return;

	/* walk objects queue of pages, looking for dirty pages */
again:
	p = (vm_page_t) queue_first(&object->memq);
	while (!queue_end(&object->memq, (queue_entry_t) p)) {

		/* page in clean region of objects logical segment */
		if (start == end ||
		    p->offset >= start && p->offset < end) {

			/* update page dirty and force unmapped */
			if (p->clean && pmap_is_modified(VM_PAGE_TO_PHYS(p)))
				p->clean = FALSE;
			pmap_page_protect(VM_PAGE_TO_PHYS(p), VM_PROT_NONE);

			/* if dirty, synchronously write the page */
			if (!p->clean) {
				p->busy = TRUE;
				object->paging_in_progress++;
				(void) vm_pager_put(object->pager, p, TRUE);
				object->paging_in_progress--;
				wakeup((caddr_t) object);
				p->busy = FALSE;
				PAGE_WAKEUP(p);

				/* re examine from start of list */
				goto again;
			}
		}
		p = (vm_page_t) queue_next(&p->listq);
	}
}

/* force objects pages to be reclaimable */
void
vm_object_deactivate_pages(vm_object_t object)
{
	vm_page_t p, next;

	if (object == NULL)
		return;

	/* walk objects page queue, deactivating pages. */
	p = (vm_page_t) queue_first(&object->memq);
	while (!queue_end(&object->memq, (queue_entry_t) p)) {
		next = (vm_page_t) queue_next(&p->listq);
		vm_page_deactivate(p);
		p = next;
	}
}

/* reduce the object cache if oversized */
static void
vm_object_cache_trim(void)
{
	vm_object_t	object;

	/* if cache is too large ... */
	while (vm_object_cached > vm_cache_max) {
		object = (vm_object_t) queue_first(&vm_object_cached_list);

#ifdef DIAGNOSTIC
		if (object != vm_object_lookup(object->pager))
			panic("vm_object_cache_trim: object not in cache");
#endif

		/* force object to lose persistance and be deallocated */
		pager_cache(object, FALSE);
	}
}

/* force read-only access to pages in range & mark pages copy-on_write. */ 
void
vm_object_pmap_copy(vm_object_t object, vm_offset_t start, vm_offset_t end)
{
	vm_page_t p;

	if (object == NULL)
		return;

	p = (vm_page_t) queue_first(&object->memq);
	while (!queue_end(&object->memq, (queue_entry_t) p)) {
		if ((start <= p->offset) && (p->offset < end)) {
			pmap_page_protect(VM_PAGE_TO_PHYS(p), VM_PROT_READ);
			p->copy_on_write = TRUE;
		}
		p = (vm_page_t) queue_next(&p->listq);
	}
}

/* remove access to pages in range. */
void
vm_object_pmap_remove(vm_object_t object, vm_offset_t start, vm_offset_t end)
{
	vm_page_t p;

	if (object == NULL)
		return;

	p = (vm_page_t) queue_first(&object->memq);
	while (!queue_end(&object->memq, (queue_entry_t) p)) {
		if ((start <= p->offset) && (p->offset < end))
			pmap_page_protect(VM_PAGE_TO_PHYS(p), VM_PROT_NONE);
		p = (vm_page_t) queue_next(&p->listq);
	}
}

/*
 * copy a range of the object to be used by another map entry by
 * increasing the references on an existing object. a object is
 * preceded by a copy object that holds modified pages seperate
 * from its read-only contents. if not present, the copy object is
 * created and inserted in front of the supplied object whose pages
 * are made read-only. only the copy object is shared by new copies.
 */
void
vm_object_copy(vm_object_t src_object, vm_offset_t src_offset, vm_size_t size,
  vm_object_t *dst_object, vm_offset_t *dst_offset, boolean_t *src_needs_copy)
{
	vm_object_t new_copy, old_copy;
	vm_offset_t new_start, new_end;
	vm_page_t p;

	/* no object yet exists to copy */
	if (src_object == NULL) {
		*dst_object = NULL;
		*dst_offset = 0;
		*src_needs_copy = FALSE;
		return;
	}

	/* postpone object generation since no pager or default pager? */
	if (src_object->pager == NULL || src_object->internal) {

		/* pages must be uniquely cloned if written */
		for (p = (vm_page_t) queue_first(&src_object->memq);
		     !queue_end(&src_object->memq, (queue_entry_t)p);
		     p = (vm_page_t) queue_next(&p->listq)) {
			if (src_offset <= p->offset &&
			    p->offset < src_offset + size)
				p->copy_on_write = TRUE;
		}

		/* share source object (until either modify, then shadow) */
		*dst_object = src_object;
		*dst_offset = src_offset;
		src_object->ref_count++;
		*src_needs_copy = TRUE;
		return;
	}

	/* collapse object chain before increasing it */
	vm_object_collapse(src_object);

	/* if no modified/external pages in existing copy object, use it. */
	if ((old_copy = src_object->copy) != NULL &&
	    old_copy->resident_page_count == 0 && old_copy->pager == NULL) {
		old_copy->ref_count++;
		*dst_object = old_copy;
		*dst_offset = src_offset;
		*src_needs_copy = FALSE;
		return;
	}

	/* allocate a copy object to entirely shadow current object */
	new_copy = vm_object_allocate(src_object->size);
	new_start = (vm_offset_t) 0;
	new_end   = (vm_offset_t) new_copy->size;

	/* check if copy object already inserted in src */
	if ((old_copy = src_object->copy) != NULL) {
#ifdef DIAGNOSTIC
		if (old_copy->shadow != src_object ||
		    old_copy->shadow_offset != (vm_offset_t) 0)
			panic("vm_object_copy: copy/shadow inconsistency");
#endif
		/* chain new one in front of old one. new one sees changes. */
		src_object->ref_count--;
		old_copy->shadow = new_copy;
		new_copy->ref_count++;
	}

	/* incorporate copy object with original that it entirely shadows */
	new_copy->shadow = src_object;
	new_copy->shadow_offset = new_start;
	src_object->ref_count++;
	src_object->copy = new_copy;

	/* make original object pages read-only, cloned on write into copy object */
	p = (vm_page_t) queue_first(&src_object->memq);
	while (!queue_end(&src_object->memq, (queue_entry_t) p)) {
		if ((new_start <= p->offset) && (p->offset < new_end))
			p->copy_on_write = TRUE;
		p = (vm_page_t) queue_next(&p->listq);
	}

	/* pass new copy object back as copy */
	*dst_object = new_copy;
	*dst_offset = src_offset - new_start;
	*src_needs_copy = FALSE;
}

/* insert an object in front of the current one, which it now shadows a portion of */
void
vm_object_shadow(vm_object_t *object, vm_offset_t *offset, vm_size_t length)
{
	vm_object_t source = *object, result;

	/* allocate a object to shadow original */
	result = vm_object_allocate(length);
#ifdef	DIAGNOSTIC
	if (result == NULL)
		panic("vm_object_shadow: no object for shadowing");
#endif

	/* insert shadow ahead of current entry, at desired offset into current*/
	result->shadow = source;
	result->shadow_offset = *offset;
	*offset = 0;
	*object = result;
}

/* hash object lookup on pager instance address */
#define vm_object_hash(pager) \
	(((unsigned)pager)%VM_OBJECT_HASH_COUNT)

/* locate an allocated object (with a pager) by its pager instance */
vm_object_t
vm_object_lookup(vm_pager_t pager)
{
	queue_t bucket;
	vm_object_hash_entry_t entry;
	vm_object_t object;

	/* find associated queue head for this pager */
	bucket = &vm_object_hashtable[vm_object_hash(pager)];

	/* search queue for an object with this pager */
	entry = (vm_object_hash_entry_t) queue_first(bucket);
	while (!queue_end(bucket, (queue_entry_t) entry)) {
		object = entry->object;

		/* found object? */
		if (object->pager == pager) {

			/* in cache? remove and assert a reference */
			if (object->ref_count == 0) {
				queue_remove(&vm_object_cached_list, object,
				    vm_object_t, cached_list);
				vm_object_cached--;
			}
			object->ref_count++;
			return(object);
		}
		entry = (vm_object_hash_entry_t) queue_next(&entry->hash_links);
	}

	return(NULL);
}


/* insert object into hash table so lookup can find it by pager. */
void
vm_object_enter(vm_object_t object)
{
	queue_t	 bucket;
	vm_object_hash_entry_t	entry;

	/* ignore attempts to cache null objects or pagers */
	if (object == NULL || object->pager == NULL)
		return;

	/* find hash bucket queue header for pager */
	bucket = &vm_object_hashtable[vm_object_hash(object->pager)];

	/* allocate and insert hash entry in cache */
	entry = (vm_object_hash_entry_t)
		malloc((u_long)sizeof *entry, M_VMOBJHASH, M_WAITOK);
	entry->object = object;
	object->can_persist = TRUE;
	object->paging_offset = 0;
	queue_enter(bucket, entry, vm_object_hash_entry_t, hash_links);
}


/* remove object associated with pager instance from lookup hash table */
void
vm_object_remove(vm_pager_t pager)
{
	queue_t bucket;
	vm_object_hash_entry_t entry;
	vm_object_t object;

	bucket = &vm_object_hashtable[vm_object_hash(pager)];

	entry = (vm_object_hash_entry_t) queue_first(bucket);
	while (!queue_end(bucket, (queue_entry_t) entry)) {
		object = entry->object;
		if (object->pager == pager) {
			queue_remove(bucket, entry, vm_object_hash_entry_t,
					hash_links);
			free((caddr_t)entry, M_VMOBJHASH);
			break;
		}
		entry = (vm_object_hash_entry_t) queue_next(&entry->hash_links);
	}
}

/* reduce number of redundant shadow objects in place */
void
vm_object_collapse(vm_object_t object)
{
	vm_object_t backing_object;
	vm_offset_t backing_offset, new_offset;
	vm_size_t size;
	vm_page_t p, pp;

	/* attempt to sucessive collapse/bypass shadows into current object */
	while (TRUE) {
		/* an idle object doesn't exist without external contents */
		if (object == NULL || object->paging_in_progress != 0
		    || object->pager != NULL)
			return;

		/* the object doesn't shadow an internal idle object */
		if ((backing_object = object->shadow) == NULL ||
		    !backing_object->internal ||
		    backing_object->paging_in_progress != 0)
			return;
	
		/* the shadowed object is the copy object */
		if (backing_object->shadow != NULL &&
		    backing_object->shadow->copy != NULL)
			return;

		backing_offset = object->shadow_offset;
		size = object->size;

		/* collapse shadow into current object */
		if (backing_object->ref_count == 1) {

			/* merge shadow's pages into current object */
			while (!queue_empty(&backing_object->memq)) {

				p = (vm_page_t)
					queue_first(&backing_object->memq);

				new_offset = (p->offset - backing_offset);

				/* free shadow page that exceeds boundaries of current object */
				if (p->offset < backing_offset ||
				    new_offset >= size)
					vm_page_free(p);
				else {
				    /* if valid page exists in current object, free shadow page */
				    pp = vm_page_lookup(object, new_offset);
				    if (pp != NULL && !pp->fake)
					vm_page_free(p);
				    else {
					/* free invalid page in current object */
					if (pp) {
					    /* may be someone waiting for it */
					    PAGE_WAKEUP(pp);
					    vm_page_free(pp);
					}
					/* otherwise, move page into current object */
					vm_page_rename(p, object, new_offset);
				    }
				}
			}

			/* relocate pager (if any) into current object */
			object->pager = backing_object->pager;
			object->paging_offset += backing_offset;
			backing_object->pager = NULL;

			/* current object now shadows shadows' shadow */
			object->shadow = backing_object->shadow;
			object->shadow_offset += backing_object->shadow_offset;
#ifdef	DIAGNOSTIC
			if (object->shadow != NULL &&
			    object->shadow->copy != NULL) {
				panic("vm_object_collapse: we collapsed a copy-object!");
			}
#endif

			/* old shadow object reclaimed inline */
			vm_object_count--;
			free((caddr_t)backing_object, M_VMOBJ);
			object_collapses++;
		}

		/* bypass unused shadow to release reference and allow other collapse */
		else {
			/* no external page state */
			if (backing_object->pager != NULL)
				return;

			/* scan shadow to find any dependant entries */
			p = (vm_page_t) queue_first(&backing_object->memq);
			while (!queue_end(&backing_object->memq,
					  (queue_entry_t) p)) {

				new_offset = (p->offset - backing_offset);

				/* shadow has dependant page, failed bypass */
				if (p->offset >= backing_offset &&
				    new_offset <= size &&
				    ((pp = vm_page_lookup(object, new_offset))
				      == NULL ||
				     pp->fake))
					return;

				p = (vm_page_t) queue_next(&p->listq);
			}

			/* current object shadows shadows' shadow instead of shadow */
			vm_object_reference(object->shadow = backing_object->shadow);
			object->shadow_offset += backing_object->shadow_offset;

			/* no longer reference shadow */
			backing_object->ref_count--;
			object_bypasses ++;
		}
	}
}

/* remove access to pages in range and free them. */
void
vm_object_page_remove(vm_object_t object, vm_offset_t start, vm_offset_t end)
{
	vm_page_t p, next;

	if (object == NULL)
		return;

	p = (vm_page_t) queue_first(&object->memq);
	while (!queue_end(&object->memq, (queue_entry_t) p)) {
		next = (vm_page_t) queue_next(&p->listq);
		if ((start <= p->offset) && (p->offset < end)) {
			pmap_page_protect(VM_PAGE_TO_PHYS(p), VM_PROT_NONE);
			vm_page_free(p);
		}
		p = next;
	}
}

/* combine two like, adjacent anonymous objects into a single object */
boolean_t
vm_object_coalesce(vm_object_t prev_object, vm_object_t next_object,
    vm_offset_t prev_offset, vm_offset_t next_offset, vm_size_t prev_size,
    vm_size_t next_size)
{
	vm_size_t newsize;
	vm_offset_t newoffset = prev_offset + prev_size;

	/* trivial coalesce */
	if (prev_object == NULL && next_object == NULL)
		return(TRUE);

	/* reduce object shadow list to canonicalize object */
	vm_object_collapse(prev_object);

	/*
	 * can't work if either object multiply referenced, shadows another,
	 * or has a copy object (can't tell if isomorphic/adjacent).
	 */
	if (prev_object->ref_count != 1 || prev_object->pager ||
	    prev_object->shadow || prev_object->copy)
		return(FALSE);
	if (next_object != NULL && (next_object->ref_count != 1 ||
	    next_object->pager || next_object->shadow || next_object->copy))
		return(FALSE);

	/* clear extended region of object */
	vm_object_page_remove(prev_object, newoffset, newoffset + next_size);

	/* extend object size if new combination exceeds existing size */
	newsize = newoffset + next_size;
	if (newsize > prev_object->size)
		prev_object->size = newsize;

	/* if next object has contents, merge them and discard object */
	if (next_object != NULL) {
		vm_page_t p;

		/* move pages to previous object */
		while (!queue_empty(&next_object->memq)) {
			p = (vm_page_t)queue_first(&next_object->memq);
			if (p->offset < next_size)
				vm_page_rename(p, prev_object,
				    p->offset + newoffset);
			else
				vm_page_free(p);
		}

		/* release object */
		vm_object_count--;
		free((caddr_t)next_object, M_VMOBJ);
	}
	return(TRUE);
}

#if defined(DEBUG) || defined(DDB)
/*
 *	vm_object_print:	[ debug ]
 */
void
vm_object_print(vm_object_t object, boolean_t full)
{
	register vm_page_t	p;
	extern indent;

	register int count;

	if (object == NULL)
		return;

	iprintf("Object 0x%x: size=0x%x res=%d ref=%d persist %d int %d ",
		(int) object, (int) object->size,
		object->resident_page_count, object->ref_count,
		object->can_persist, object->internal);
	printf("pager=0x%x+0x%x, shadow=(0x%x)+0x%x\n",
	       (int) object->pager, (int) object->paging_offset,
	       (int) object->shadow, (int) object->shadow_offset);
	printf("cache: next=0x%x, prev=0x%x\n",
	       object->cached_list.next, object->cached_list.prev);

	if (!full)
		return;

	indent += 2;
	count = 0;
	p = (vm_page_t) queue_first(&object->memq);
	while (!queue_end(&object->memq, (queue_entry_t) p)) {
		if (count == 0)
			iprintf("memory:=");
		else if (count == 6) {
			pg("\n");
			iprintf(" ...");
			count = 0;
		} else
			printf(",");
		count++;

		printf("(m=%x off=0x%x,page=0x%x)", p, p->offset, VM_PAGE_TO_PHYS(p));
		p = (vm_page_t) queue_next(&p->listq);
	}
	if (count != 0)
		printf("\n");
	indent -= 2;
}

#ifndef nope
queue_check(queue_chain_t *q) {

	vm_page_t f, l, n, p, m;
	int count = 100000;
	int nelmt = 1, npanic = 0, ppanic = 0;

	/* walk forward links, checking reverse */
	m = f = (vm_page_t) queue_first(q);
	l = (vm_page_t) queue_last(q);
/*printf("f %x l %x ", f, l); */
	for (m = f; !queue_end(q, (queue_entry_t) m) && npanic == 0 ; m = n) {

		if (m->object != (vm_object_t) q) {
			printf("object %x\n", m->object);
			npanic++;
			break;
		}

		n = (vm_page_t) queue_next(&m->listq);
		p = (vm_page_t) queue_prev(&m->listq);
/*printf("m %x (p %x n %x) ", m, p, n); */
		if (m == n) {
			printf("%d.%x: next(m) == m\n", nelmt, m);
			npanic++;
		} else if (m == l && queue_end(q, (queue_entry_t)n) == 0) {
			printf("%d.%x: last link not end\n", nelmt, l);
			npanic++;
		} else if (queue_end(q, (queue_entry_t) n) == 0 &&
		    m != (vm_page_t) queue_prev(&n->listq)) {
			printf("%d.%x: prev(next(m)) != m (%x != %x)\n",
				nelmt+1, n, m, queue_prev(&n->listq));
			vm_page_print(n);
			npanic++;
		}
		if (m == p) {
			printf("%d.%x: prev(m) == m \n", nelmt, m);
			npanic++;
		} else if (m == f && queue_end(q, (queue_entry_t)p) == 0) {
			printf("%d.%x: first link\n", nelmt, p);
			npanic++;
		} else if (queue_end(q, (queue_entry_t) p) == 0 &&
		    m != (vm_page_t) queue_next(&p->listq)) {
			printf("%d.%x: next(prev(m)) != m (%x != %x)\n",
				nelmt-1, p, m, queue_next(&p->listq));
			vm_page_print(p);
			npanic++;
		}
		if (vm_Page_check(m))
			nelmt++;
		else
			npanic++;
		if (count-- < 0) {
			printf("circular queue(n)\n");
			npanic++;
		}
		/*m = n;*/
	}
	if (npanic)
		vm_page_print(m);

	if (npanic || ppanic)
		panic("queue");
}
#endif
#endif
