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
 *	$Id: map.c,v 1.1 94/10/19 17:37:16 bill Exp $
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

/*
 * Virtual address management layer.
 */

#include "sys/param.h"
#include "uio.h"
#include "sys/errno.h"
#include "sys/mman.h"
#include "malloc.h"
#include "vm.h"
#include "vm_fault.h"
#include "proc.h"
#include "vmspace.h"
#ifdef	KTRACE
#include "sys/ktrace.h"
#endif

#include "prototypes.h"

/* internal function prototypes */

static vm_map_entry_t vm_map_entry_create(vm_map_t map);
static void vm_map_entry_dispose(vm_map_entry_t entry);
static void _vm_map_clip_start(vm_map_t map, vm_map_entry_t entry,
	vm_offset_t start);
static void _vm_map_clip_end(vm_map_t map, vm_map_entry_t entry,
	vm_offset_t end);
static void vm_map_entry_recycle(vm_map_t map, vm_map_entry_t entry);
static vm_map_entry_t vm_map_entry_delete(vm_map_t map, vm_map_entry_t entry);
static void vm_map_copy_entry(vm_map_t src_map, vm_map_t dst_map,
	vm_map_entry_t src_entry, vm_map_entry_t dst_entry);
/* "internal" routines currently used externally (vm_kern,vm_mmap):
static int vm_map_submap(vm_map_t map, vm_offset_t start, vm_offset_t end,
	vm_map_t submap);
static int vm_map_delete(vm_map_t map, vm_offset_t start, vm_offset_t end);
static int vm_map_insert(vm_map_t map, vm_object_t object,
	vm_offset_t offset, vm_offset_t start, vm_offset_t end);
static boolean_t vm_map_lookup_entry(vm_map_t map, vm_offset_t address,
	vm_map_entry_t *entry); */

/*
 *  Map and entry structures are allocated from the general
 *  purpose memory pool with some exceptions:
 *
 *  - The kernel map is allocated statically (in kern/main.c).
 *  - The kernel map's submaps are allocated statically, during kernel
 *    initialization.
 *  - For any of the statically allocated maps(IS_STATIC_MAP()), map
 *    entries are similarly allocated out of a static pool of entries.
 *
 *  These restrictions are necessary to avoid deadlocks with
 *  dynamic allocation, since the kernel memory allocator malloc()
 *  itself uses kernel maps and requires map entries.
 */

extern vm_map_t kernel_map;
static struct vm_map __kmap[MAX_KMAP], *kmap_free = __kmap;
#define	IS_STATIC_MAP(mp) \
  ((mp) == kernel_map || ((mp) >= __kmap && (mp) < __kmap + MAX_KMAP))

static struct vm_map_entry __kmap_entry[MAX_KMAPENT], *kentry_free;
#define	IS_STATIC_MAPENT(me) \
  ((me) >= __kmap_entry && (me) < __kmap_entry + MAX_KMAPENT)

/*
 *  Initialize the vm_map/vmspace layer.  Must be called before
 *  any other vm_map routines.
 */
void vm_map_startup(void)
{
	int i;
	vm_map_entry_t mep;

	/* Form a free list of statically allocated kernel map entries. */
	kentry_free = mep = __kmap_entry;
	for (i = MAX_KMAPENT; --i > 0; mep++)
		mep->next = mep + 1;
	mep->next = NULL;
}

/*
 * Create and return a new empty VM map with the given physical
 * map structure, and having the given lower and upper address bounds.
 */
vm_map_t
vm_map_create(pmap_t pmap, vm_offset_t min, vm_offset_t max,
    boolean_t pageable)
{
	vm_map_t result;
	extern vm_map_t	kmem_map;

	/* last static map allocated? */
	if (kmem_map == NULL) {
		result = kmap_free++;
		if (result > __kmap + MAX_KMAP)
			panic("vm_map_create: out of maps");
	} else
		MALLOC(result, vm_map_t, sizeof(struct vm_map),
		       M_VMMAP, M_WAITOK);

	vm_map_init(result, min, max, pageable);
	result->pmap = pmap;
	return(result);
}

/*
 * Initialize an existing vm_map structure such as that in the vmspace
 * structure. The pmap is set elsewhere.
 */
void
vm_map_init(vm_map_t map, vm_offset_t min, vm_offset_t max,
	boolean_t pageable)
{
	map->header.next = map->header.prev = &map->header;
	map->nentries = 0;
	map->size = 0;
	map->ref_count = 1;
	map->is_main_map = TRUE;
	map->min_offset = min;
	map->max_offset = max;
	map->entries_pageable = pageable;
	map->first_free = &map->header;
	map->hint = &map->header;
	map->timestamp = 0;
	map->pmap = (pmap_t)0;
	lock_init(&map->lock);
}

/* Allocate a static or dynamic VM map entry for insertion. */
extern inline vm_map_entry_t
vm_map_entry_create(vm_map_t map)
{
	vm_map_entry_t	entry;

	if (IS_STATIC_MAP(map)) {
		if (entry = kentry_free)
			kentry_free = kentry_free->next;
	} else
		MALLOC(entry, vm_map_entry_t, sizeof(struct vm_map_entry),
		       M_VMMAPENT, M_WAITOK);
#ifdef	DIAGNOSTIC
	if (entry == NULL)
		panic("vm_map_entry_create: out of map entries");
#endif

	return(entry);
}

/* Inverse of vm_map_entry_create. */
extern inline void
vm_map_entry_dispose(vm_map_entry_t entry)
{
	if (IS_STATIC_MAPENT(entry)) {
		entry->next = kentry_free;
		kentry_free = entry;
	} else
		FREE(entry, M_VMMAPENT);
}

/* Insert a new map entry into a map's queue of entries after a entry.*/
extern inline void
vm_map_entry_link(vm_map_t map, vm_map_entry_t after_where,
	vm_map_entry_t entry) {

	map->nentries++;
	entry->prev = after_where;
	entry->next = after_where->next;
	entry->prev->next = entry;
	entry->next->prev = entry;
}

/* Remove an entry from a map. */
extern inline void
vm_map_entry_unlink(vm_map_t map, vm_map_entry_t entry) {

	map->nentries--;
	entry->next->prev = entry->prev;
	entry->prev->next = entry->next;
}

/* Account for another reference to the map. */
void
vm_map_reference(vm_map_t map)
{
	if (map == NULL)
		return;

	map->ref_count++;
}

/*
 * Removes a reference from the specified map, destroying it if no
 * references remain. The map should not be locked.
 */
void
vm_map_deallocate(vm_map_t map)
{
	/* if non-existant or still has a reference, we are done. */
	if (map == NULL || --map->ref_count > 0)
		return;

	/* lock the map, to wait out all other references to it. */
	vm_map_lock(map);
	(void) vm_map_delete(map, map->min_offset, map->max_offset);
	pmap_destroy(map->pmap);

	if (!IS_STATIC_MAP(map))
		FREE(map, M_VMMAP);
#ifdef	DIAGNOSTIC
	else
		panic("vm_map_deallocate: static map free");
#endif
}

/*
 * Insert a given VM object into the target map at the specified address
 * region. The portion of the object associated with region must exist,
 * and the map must be locked.
 */
int
vm_map_insert(vm_map_t map, vm_object_t object, vm_offset_t offset,
	vm_offset_t start, vm_offset_t end)  
{
	vm_map_entry_t	new_entry, prev_entry;

	/* Check that the start and end points are not bogus. */
	if ((start < map->min_offset) || (end > map->max_offset) ||
			(start >= end))
		return(KERN_INVALID_ADDRESS);

	/*
	 * Find the entry prior to the proposed starting address;
	 * if it's part of an existing entry, this range is bogus.
	 */
	if (vm_map_lookup_entry(map, start, &prev_entry))
		return(KERN_NO_SPACE);

	/* Assert that the next entry doesn't overlap the end point.*/
	if ((prev_entry->next != &map->header) &&
			(prev_entry->next->start < end))
		return(KERN_NO_SPACE);

	/*
	 * See if we can avoid creating a new entry by extending
	 * one of our neighbors.
	 */
	if (object == NULL && (prev_entry != &map->header) &&
	    (prev_entry->end == start) && (map->is_main_map) &&
	    (prev_entry->is_a_map == FALSE) &&
	    (prev_entry->is_sub_map == FALSE) &&
	    (prev_entry->inheritance == VM_INHERIT_DEFAULT) &&
	    (prev_entry->protection == VM_PROT_DEFAULT) &&
	    (prev_entry->max_protection == VM_PROT_DEFAULT) &&
	    (prev_entry->wired_count == 0)) {

		if (vm_object_coalesce(prev_entry->object.vm_object,
		    NULL, prev_entry->offset, (vm_offset_t) 0,
		    (vm_size_t)(prev_entry->end - prev_entry->start),
		    (vm_size_t)(end - prev_entry->end))) {
			/*
			 * Coalesced the two objects - can extend
			 * the previous map entry to include the
			 * new range.
			 */
			map->size += (end - prev_entry->end);
			prev_entry->end = end;
			return(KERN_SUCCESS);
		}
	}

	/* Create a new entry */
	new_entry = vm_map_entry_create(map);
	new_entry->start = start;
	new_entry->end = end;
	new_entry->is_a_map = FALSE;
	new_entry->is_sub_map = FALSE;
	new_entry->object.vm_object = object;
	new_entry->offset = offset;
	new_entry->copy_on_write = FALSE;
	new_entry->needs_copy = FALSE;

	if (map->is_main_map) {
		new_entry->inheritance = VM_INHERIT_DEFAULT;
		new_entry->protection = VM_PROT_DEFAULT;
		new_entry->max_protection = VM_PROT_DEFAULT;
		new_entry->wired_count = 0;
	}

	/* Insert the new entry into the list */
	vm_map_entry_link(map, prev_entry, new_entry);
	map->size += new_entry->end - new_entry->start;

	/* Update the free space hint */
	if ((map->first_free == prev_entry)
	    && (prev_entry->end >= new_entry->start))
		map->first_free = new_entry;

	return(KERN_SUCCESS);
}

/*
 * Find the map entry containing (or immediately preceding) the
 * specified address in the given map; the entry is returned in the
 * "entry" parameter. The boolean result indicates whether the address
 * is actually contained in the map.
 * [n.b. used by kern_malloc to check for unique vm_map entry generation,
 *  as well as vm_mmap to find any mapped regions. -wfj]
 */
/* static */ boolean_t
vm_map_lookup_entry(vm_map_t map, vm_offset_t address, vm_map_entry_t *entry)
{
	vm_map_entry_t	cur, last = &map->header;

	/* check hint, and if no hint try first entry. */
	if ((cur = map->hint) == last)
		cur = cur->next;

	/* is this an empty map ? */
	if (cur == last) {
		*entry = cur;
		return(FALSE);
	}

	/* is the address in or above this entry? */
	if (address >= cur->start) {
		/* is this first entry checked the entry we wanted? */
		if (cur->end > address) {
			*entry = cur;
			return(TRUE);
		}
		/* above: search from this entry to end */
	} else
		/* below: search from beginning to end */ 
		cur = map->header.next;

	/* walk the ordered list for an entry containing the address */
	for (; cur != last; cur = cur->next)
		if (cur->end > address) {
			if (address >= cur->start) {

				/* found, return entry and update hint */
				*entry = cur;
				map->hint = cur;
				return(TRUE);
			}
			break;
		}

	/* not found, return previous entry and record it as the hint */
	map->hint = *entry = cur->prev;
	return(FALSE);
}

/*
 * Find an unallocated region in the target address map with the given
 * length. The search is defined to be first-fit from the specified
 * address; the region found is returned in the same parameter.
 */
int
vm_map_find(vm_map_t map, vm_offset_t *addr, vm_size_t length)
{
	vm_map_entry_t	entry;
	vm_offset_t	start = *addr, end;

	/* find address to start search from */
	if (start < map->min_offset)
		start = map->min_offset;

	/* check if requested area starts within the bounds of the map */
	if (start > map->max_offset)
		return (KERN_NO_SPACE);

	/*
	 * check free space hint if starting at beginning, otherwise
	 * start from the end of the first adjacent entry. If no hint,
	 * start from first location of map (no entries in map).
	 */
	if (start == map->min_offset) {
		if ((entry = map->first_free) != &map->header)
			start = entry->end;
	} else
		if (vm_map_lookup_entry(map, start, &entry))
			start = entry->end;

	/* check map entries for a valid hole of "length" bytes. */
	while (TRUE) {
		vm_map_entry_t	next;

		/* would the hole overlap or wrap around the end of the map? */
		end = start + length;
		if ((end > map->max_offset) || (end < start))
			return (KERN_NO_SPACE);

		/* if there is no adjacent entry, hole is found. */
		next = entry->next;
		if (next == &map->header)
			break;

		/* if adjacent entry starts above hole, hole is found. */
		if (next->start >= end)
			break;

		entry = next;
		start = entry->end;
	}
	*addr = start;
	
	map->hint = entry;
	return(KERN_SUCCESS);
}

/*
 * Force entry to start at the desired address. If it doesn't, clip off
 * the starting fragment and insert it in a new adjacent entry ahead of
 * this one so that this one does.
 */
extern inline void
vm_map_clip_start(vm_map_t map, vm_map_entry_t entry, vm_offset_t startaddr)
{
	if (startaddr > entry->start)
		_vm_map_clip_start(map, entry, startaddr);
}

/* Create, split, and insert map entry ahead of current one. */
static void
_vm_map_clip_start(vm_map_t map, vm_map_entry_t entry, vm_offset_t start)
{
	vm_map_entry_t	new_entry;

	/* clone a new entry from the old, holding the leading portion */
	new_entry = vm_map_entry_create(map);
	*new_entry = *entry;
	new_entry->end = start;

	/* relocate current entry logical contents to new starting address */
	entry->offset += (start - entry->start);
	entry->start = start;

	/* insert new entry immediately ahead of current shortened entry */
	vm_map_entry_link(map, entry->prev, new_entry);

	/* bump reference counts up to account for new entry creation */
	if (entry->is_a_map || entry->is_sub_map)
	 	vm_map_reference(new_entry->object.share_map);
	else
		vm_object_reference(new_entry->object.vm_object);
}

/*
 * Force entry to end at the desired address. If it doesn't, clip off
 * the trailing fragment and insert it in a new adjacent entry after 
 * this one so that this one does.
 */
extern inline void
vm_map_clip_end(vm_map_t map, vm_map_entry_t entry, vm_offset_t endaddr)
{
	if (endaddr < entry->end)
		_vm_map_clip_end(map, entry, endaddr);
}

/* Create, split, and insert map entry after current one. */
static void
_vm_map_clip_end(vm_map_t map, vm_map_entry_t entry, vm_offset_t end)
{
	vm_map_entry_t	new_entry;

	/* clone a new entry from the old, holding the trailing portion */
	new_entry = vm_map_entry_create(map);
	*new_entry = *entry;
	new_entry->start = end;

	/* relocate new entry logical contents */
	new_entry->offset += (end - entry->start);

	/* truncate current entry */
	entry->end = end;

	/* insert new entry immediately after current shortened entry */
	vm_map_entry_link(map, entry, new_entry);

	/* bump reference counts up to account for new entry creation */
	if (entry->is_a_map || entry->is_sub_map)
	 	vm_map_reference(new_entry->object.share_map);
	else
		vm_object_reference(new_entry->object.vm_object);
}

/* Unify the map entry of the address with its preceeding one of like kind. */
void
vm_map_simplify(vm_map_t map, vm_offset_t start)
{
	vm_map_entry_t	this_entry, prev_entry;

	if((vm_map_lookup_entry(map, start, &this_entry)) &&
	    ((prev_entry = this_entry->prev) != &map->header) &&
	    (prev_entry->end == start) && (map->is_main_map) &&
	    (prev_entry->is_a_map == FALSE) &&
	    (prev_entry->is_sub_map == FALSE) &&
	    (this_entry->is_a_map == FALSE) &&
	    (this_entry->is_sub_map == FALSE) &&
	    (prev_entry->inheritance == this_entry->inheritance) &&
	    (prev_entry->protection == this_entry->protection) &&
	    (prev_entry->max_protection == this_entry->max_protection) &&
	    (prev_entry->wired_count == this_entry->wired_count) &&
	    (prev_entry->copy_on_write == this_entry->copy_on_write) &&
	    (prev_entry->needs_copy == this_entry->needs_copy) &&
	    (prev_entry->object.vm_object == this_entry->object.vm_object) &&
	    ((prev_entry->offset + (prev_entry->end - prev_entry->start))
		     == this_entry->offset) ) {
		if (map->first_free == this_entry)
			map->first_free = prev_entry;

		map->hint = prev_entry;
		vm_map_entry_unlink(map, this_entry);
		prev_entry->end = this_entry->end;
	 	vm_object_deallocate(this_entry->object.vm_object);
		vm_map_entry_dispose(this_entry);
	}
}

/*
 * Common code for vm_map_<operate> kernel functions, which forces
 * region arguements to sane values for map, locks map, finds and
 * clips the associated map entry for the starting address region.
 * All of the vm_map_<operate> functions perform the same operations
 * on a set of discrete map entries.
 */
#define	VM_MAP_LOCK_CLIP_START					\
	/* force within bounds the region arguements */		\
	if (start < vm_map_min(map)) 				\
		start = vm_map_min(map);			\
	if (end > vm_map_max(map))				\
		end = vm_map_max(map);				\
	if (start > end)					\
		start = end;					\
								\
	/* lock the map to alter entries exclusively */		\
	vm_map_lock(map);					\
								\
	/* if starting address has a map entry, clip it */	\
	if (vm_map_lookup_entry(map, start, &entry))		\
		vm_map_clip_start(map, entry, start);		\
								\
	/* otherwise use succeeding entry */			\
	else							\
		entry = entry->next;

/* Convert a discrete region of the kernel map into a submap. */
/* static */ int
vm_map_submap(vm_map_t map, vm_offset_t start, vm_offset_t end, vm_map_t submap)
{
	vm_map_entry_t	entry;
	int		result = KERN_INVALID_ARGUMENT;

	/* isolate single map entry describing region specified. */
	VM_MAP_LOCK_CLIP_START;
	vm_map_clip_end(map, entry, end);

	/* insure region is a single empty entry used for anonymous memory.*/
	if ((entry->start == start) && (entry->end == end) &&
	    (!entry->is_a_map) && (entry->object.vm_object == NULL) &&
	    (!entry->copy_on_write)) {

		/* convert entry to point at submap. */
		entry->is_a_map = FALSE;
		entry->is_sub_map = TRUE;
		entry->object.sub_map = submap;
		result = KERN_SUCCESS;
	}

	vm_map_unlock(map);
	return(result);
}

#define	VM_MAP_LOOPENT(c) \
 for(; ((c) != &map->header) && ((c)->start < end); (c) = (c)->next)

/*
 * Set the protection of the specified address region in the target space.
 * If "set_max" is specified, the maximum protection is to be set;
 * otherwise, only the current protection is affected.
 */
int
vmspace_protect(struct vmspace *vs, caddr_t va, unsigned sz,
	int set_max, vm_prot_t new_prot)
{
	vm_map_t map = &vs->vm_map;
	vm_offset_t start = trunc_page(va), end = round_page(va+sz-1);
	vm_map_entry_t	current, entry;

	VM_MAP_LOCK_CLIP_START;

	/* check region to see if new protection falls within maximum limit. */
	current = entry;
	VM_MAP_LOOPENT(current) {
		if (current->is_sub_map)
			return(KERN_INVALID_ARGUMENT);
		if ((new_prot & current->max_protection) != new_prot) {
			vm_map_unlock(map);
			return(KERN_PROTECTION_FAILURE);
		}
	}

	/* change protection of region */
	current = entry;
	VM_MAP_LOOPENT(current) {
		vm_prot_t	old_prot;

		vm_map_clip_end(map, current, end);

		/* change protection of this entry */
		old_prot = current->protection;
		if (set_max)
			current->protection =
				(current->max_protection = new_prot) &
					old_prot;
		else
			current->protection = new_prot;

		/* update memory managment protection level */
		if (current->protection != old_prot) {

#define MASK(entry)	((entry)->copy_on_write ? ~VM_PROT_WRITE : \
							VM_PROT_ALL)
			/* if entry indirects through a share map... */
			if (current->is_a_map) {
				vm_map_entry_t	share_entry;
				vm_offset_t	share_end;

				/* ...find starting shared entry and... */
				vm_map_lock(current->object.share_map);
				(void) vm_map_lookup_entry(
						current->object.share_map,
						current->offset,
						&share_entry);
				share_end = current->offset +
					(current->end - current->start);

				/* ... update shared entries memory protection. */
				while ((share_entry != 
					&current->object.share_map->header) &&
					(share_entry->start < share_end)) {

					pmap_protect(map->pmap,
						(max(share_entry->start,
							current->offset) -
							current->offset +
							current->start),
						min(share_entry->end,
							share_end) -
						current->offset +
						current->start,
						current->protection &
							MASK(share_entry));

					share_entry = share_entry->next;
				}
				vm_map_unlock(current->object.share_map);
			}
			/* ... otherwise, just update private entries. */
			else
			 	pmap_protect(map->pmap, current->start,
					current->end,
					current->protection & MASK(entry));
#undef	MASK
		}
	}

	vm_map_unlock(map);
	return(KERN_SUCCESS);
}

/*
 * Set the inheritance of the specified address range in the target
 * address space. Inheritance affects how the map will be shared with
 * child maps at the time of vmspace_fork().
 */
int
vmspace_inherit(struct vmspace *vs, caddr_t va, unsigned sz,
	vm_inherit_t new_inheritance)
{
	vm_map_t map = &vs->vm_map;
	vm_offset_t start = trunc_page(va), end = round_page(va+sz-1);
	vm_map_entry_t	entry;

	switch (new_inheritance) {
	case VM_INHERIT_NONE:
	case VM_INHERIT_COPY:
	case VM_INHERIT_SHARE:
		break;
	default:
		return(KERN_INVALID_ARGUMENT);
	}

	VM_MAP_LOCK_CLIP_START;

	VM_MAP_LOOPENT(entry) {
		vm_map_clip_end(map, entry, end);
		entry->inheritance = new_inheritance;
	}

	vm_map_unlock(map);
	return(KERN_SUCCESS);
}

/*
 * Mark address space, memory, and address translation in the
 * specified address range in the target address space as
 * pagable (e.g. can become non-resident).
 */
void
vmspace_pageable(struct vmspace *vs, caddr_t va, unsigned sz)
{
	vm_map_entry_t	entry, temp_entry;
	vm_offset_t	start = trunc_page(va), end = round_page(va+sz-1);
	vm_map_t	map = &vs->vm_map;

	VM_MAP_LOCK_CLIP_START;
	temp_entry = entry;

	/* check that all of the region is wired, otherwise don't unwire. */
	VM_MAP_LOOPENT(entry) {
	    if (entry->wired_count == 0)
		goto finished;
	}

	/* reduce wiring count on all entries in region. */
	lock_set_recursive(&map->lock);
	entry = temp_entry;
	VM_MAP_LOOPENT(entry) {
	    vm_map_clip_end(map, entry, end);

	    /* on last wiring reference, force unwiring fault on entry */
	    if (--entry->wired_count == 0)
		vm_fault_unwire(map, entry->start, entry->end);
	}
	lock_clear_recursive(&map->lock);

finished:
	vm_map_simplify(map, start);
	vm_map_unlock(map);
}

/*
 * Mark address space, memory, and address translation in the
 * specified address range in the target address space as
 * not pagable (e.g. must be held resident).
 */
void
vmspace_notpageable(struct vmspace *vs, caddr_t va, unsigned sz)
{
	vm_map_entry_t	entry, temp_entry;
	vm_offset_t	start = trunc_page(va), end = round_page(va+sz-1);
	vm_map_t	map = &vs->vm_map;

	VM_MAP_LOCK_CLIP_START;
	temp_entry = entry;

	/*
	 * Perform operations in advance on a region's entries to prevent
	 * the wiring fault(s) from write-locking the map (contradiction).
	 */
	entry = temp_entry;
	VM_MAP_LOOPENT(entry) {
	    vm_map_clip_end(map, entry, end);

	    /* bump wiring ref count. if this is the first, do the work. */
	    entry->wired_count++;
	    if (entry->wired_count == 1) {

		/*
		 * Perform actions of vm_map_lookup() that need the write
		 * lock on the map: create a shadow object for a
		 * copy-on-write region, or an object for a zero-fill region.
		 *
		 * We don't have to do this for entries that point to sharing
		 * maps, because we won't hold the lock on the sharing map.
		 */
		if (!entry->is_a_map) {
		    if (entry->needs_copy &&
			((entry->protection & VM_PROT_WRITE) != 0)) {

			vm_object_shadow(&entry->object.vm_object,
					&entry->offset,
					(vm_size_t)(entry->end
						- entry->start));
			entry->needs_copy = FALSE;
		    }
		    else if (entry->object.vm_object == NULL) {
			entry->object.vm_object =
			    vm_object_allocate((vm_size_t)(entry->end
			    			- entry->start));
			entry->offset = (vm_offset_t)0;
		    }
		}
	    }
	}

	/* downgrade to read lock to avoid deadlocks during vm_fault_wire */
	lock_set_recursive(&map->lock);
	lock_write_to_read(&map->lock);

	/* perform initial wirings of any entries in the region */
	entry = temp_entry;
	VM_MAP_LOOPENT(entry)
	    if (entry->wired_count == 1)
		vm_fault_wire(map, entry->start, entry->end);

	/* drop locks and attempt any map structure reduction */
	lock_clear_recursive(&map->lock);
	vm_map_simplify(map, start);
	vm_map_unlock(map);
}

/* Prepare a map entries contents for deletion/reuse. */
static void
vm_map_entry_recycle(vm_map_t map, vm_map_entry_t entry)
{
	vm_offset_t start = entry->start, end = entry->end;
	vm_object_t object = entry->object.vm_object;

	/*
	 * Unwire before removing addresses from the pmap; otherwise,
	 * unwiring will put the entries back in the pmap.
	 */
	if (entry->wired_count) {
		vm_fault_unwire(map, start, end);
		entry->wired_count = 0;
	}

	/*
	 * If this is a sharing map, we must remove *all* references to
	 * this data, since we can't find all of the physical maps which
	 * are sharing it.
	 */
	if (object == kernel_object || object == kmem_object)
		vm_object_page_remove(object, entry->offset,
				entry->offset + (end - start));
	else if (!map->is_main_map)
		vm_object_pmap_remove(object,
				 entry->offset,
				 entry->offset + (end - start));
	else
		pmap_remove(map->pmap, start, end);
}

/* Delete a map entry from a map. Returns next entry after deleted one. */
static vm_map_entry_t
vm_map_entry_delete(vm_map_t map, vm_map_entry_t entry)
{
	vm_map_entry_t next;

	/* prepare entry mapped contents for deletion */
	vm_map_entry_recycle(map, entry);
		
	/* detach entry from map, and reduce mapped size total */
	vm_map_entry_unlink(map, entry);
	map->size -= entry->end - entry->start;

	/* reduce underlying reference counts */
	if (entry->is_a_map || entry->is_sub_map)
		vm_map_deallocate(entry->object.share_map);
	else
	 	vm_object_deallocate(entry->object.vm_object);

	/* recycle dead map entry and return successive entry. */
	next = entry->next;
	vm_map_entry_dispose(entry);
	return(next);
}

/* Deallocates the given address region from the target map. */
/* static */ void
vm_map_delete(vm_map_t map, vm_offset_t start, vm_offset_t end)
{
	vm_map_entry_t entry;

	/* find starting entry. if it is in map, force it to start of region.*/
	if (!vm_map_lookup_entry(map, start, &entry))
		entry = entry->next;
	else {
		entry = entry;
		vm_map_clip_start(map, entry, start);

		/* hint will be deleted. set hint to next lower entry. */
		map->hint = entry->prev;
	}

	/* update free space hint */
	if (map->first_free->start >= start)
		map->first_free = entry->prev;

	/* delete map entries in the region. */
	while((entry != &map->header) && (entry->start < end)) {
		vm_map_clip_end(map, entry, end);

		entry = vm_map_entry_delete(map, entry);
	}
}

/* Allocate a virtual address space region */
int
vmspace_allocate(struct vmspace *vs, vm_offset_t *va, vm_size_t sz, int anywhere)
{
	vm_map_t	map = &vs->vm_map;
	int rv;

#ifdef KTRACE
	/* record allocation of space by a user process */
	if (curproc && KTRPOINT(curproc, KTR_VM) && curproc->p_vmspace == vs)
		ktrvm(curproc->p_tracep, KTVM_ALLOC, *va, sz, anywhere, 0);
#endif
	/*
	 * If we cannot allocate any pages of memory, don't
	 * allow any additional address space allocations.
	 */
	if (curproc && curproc->p_vmspace == vs && chk4space(atop(sz)) == 0)
		return(KERN_NO_SPACE);

	if (sz == 0)
		return(0);

	vm_map_lock(map);
	if (anywhere) {
		*va = vm_map_min(map);
		if((rv = vm_map_find(map, va, round_page(sz-1)))
			!= KERN_SUCCESS) {
			vm_map_unlock(map);
			return (rv);
		}
	} else
		*va = trunc_page(*va);

	rv = vm_map_insert(map, NULL, (vm_offset_t)0, *va, round_page(*va+sz-1));
	vm_map_unlock(map);
	return(rv);
}

/* Deallocate a region from an address space. */
void
vmspace_delete(struct vmspace *vs, caddr_t va, unsigned sz)
{
	vm_map_t	map = &vs->vm_map;
	vm_offset_t	start = trunc_page(va), end = round_page(va+sz-1);

#ifdef KTRACE
	/* record region deletions of traced processes */
	if (curproc && KTRPOINT(curproc, KTR_VM) && curproc->p_vmspace == vs)
		ktrvm(curproc->p_tracep, KTVM_REMOVE, map, start, end, 0);
#endif

	/* lock map and deallocate region */
	vm_map_lock(map);
	vm_map_delete(map, start, end);
	vm_map_unlock(map);
}

/* Simulate an access of a kind(rw) on a region. */
int
vmspace_access(struct vmspace *vs, caddr_t addr, unsigned len, int rw)
{
	boolean_t rv;
	vm_prot_t prot = rw == PROT_READ ? VM_PROT_READ : VM_PROT_WRITE;

	rv = vm_map_check_protection(&vs->vm_map,
	    trunc_page(addr), round_page(addr+len-1), prot);
	return(rv == TRUE);
}

#ifndef nope
/* static */
boolean_t
vm_map_check_protection(vm_map_t map, vm_offset_t start,
		vm_offset_t end, vm_prot_t protection) {
	vm_map_entry_t	entry;

	/* obtain the entry containing the starting address (if present) */
	if (!vm_map_lookup_entry(map, start, &entry))
		return(FALSE);

	/* examine entries for access in covering the segment */
	for (; start < end; entry = entry->next) {

		/* no more entries, yet still part of segment? */
		if (entry == &map->header)
			return(FALSE);

		/* is this entry not adjacent? */
		if (start < entry->start)
			return(FALSE);
		start = entry->end;

		/* does this entry allow for an access of this kind? */
		if ((entry->protection & protection) != protection)
			return(FALSE);
	}
	return(TRUE);
}
#else
/*
 *	vm_map_check_protection:
 *
 *	Assert that the target map allows the specified
 *	privilege on the entire address region given.
 *	The entire region must be allocated.
 */
/* static */ boolean_t
vm_map_check_protection(register vm_map_t map, vm_offset_t start,
	vm_offset_t end, vm_prot_t protection)
{
	vm_map_entry_t	entry;
	vm_map_entry_t		tmp_entry;

	if (!vm_map_lookup_entry(map, start, &tmp_entry)) {
		return(FALSE);
	}

	entry = tmp_entry;

	while (start < end) {
		if (entry == &map->header) {
			return(FALSE);
		}

		/*
		 *	No holes allowed!
		 */

		if (start < entry->start) {
			return(FALSE);
		}

		/*
		 * Check protection associated with entry.
		 */

		if ((entry->protection & protection) != protection) {
			return(FALSE);
		}

		/* go to next entry */

		start = entry->end;
		entry = entry->next;
	}
	return(TRUE);
}
#endif

/*
 * Perform a virtual copy of the region of address space described by an
 * map entry.
 *	entry.  The entries *must* be aligned properly.
 */
static void
vm_map_copy_entry(vm_map_t src_map, vm_map_t dst_map,
	vm_map_entry_t src_entry, vm_map_entry_t dst_entry)
{
	vm_object_t	temp_object;

	/* can't copy submaps */
	if (src_entry->is_sub_map || dst_entry->is_sub_map)
		panic("vm_map_copy_entry: submap");

#ifdef DIAGNOSTIC
	if (dst_entry->object.vm_object != NULL &&
	    !dst_entry->object.vm_object->internal)
		panic("vm_map_copy_entry: copying over permanent data!\n");
#endif

	/* recycle destination entry prior to use with new contents */
	vm_map_entry_recycle(dst_map, dst_entry);

	/* if any wiring references, must do actual copy of map entry. */
	if (src_entry->wired_count)
		/* simulate fault forcing the copy and unwire the new copy. */
		vm_fault_copy_entry(dst_map, src_map, dst_entry, src_entry);

	/* otherwise, postpone copy until a fault occurs in either entry. */
	else {
		boolean_t	src_needs_copy;

		/* Write protect the source entry if it has not already been.*/
		if (!src_entry->needs_copy) {
			boolean_t su = src_map->is_main_map ? 1 :
				(src_map->ref_count == 1);

			/*
			 * If the source entry has only one mapping,
			 * we can just protect the virtual address range.
			 */
			if (su)
				pmap_protect(src_map->pmap,
				    src_entry->start, src_entry->end,
				    src_entry->protection & ~VM_PROT_WRITE);
			else
				vm_object_pmap_copy(src_entry->object.vm_object,
				    src_entry->offset,
				    src_entry->offset + (src_entry->end
					-src_entry->start));
		}

		/* Copy the source object into a new destination object. */
		temp_object = dst_entry->object.vm_object;
		vm_object_copy(src_entry->object.vm_object,
				src_entry->offset,
				(vm_size_t)(src_entry->end -
					    src_entry->start),
				&dst_entry->object.vm_object,
				&dst_entry->offset,
				&src_needs_copy);
		vm_object_deallocate(temp_object);

		/*
		 * Mark the map entries to indicate that the copy object
		 * allocation has been postponed till a fault.
		 */
		if (src_needs_copy)
			src_entry->needs_copy = TRUE;
		dst_entry->needs_copy = TRUE;

		/* Make both entries copy-on-write since they are shared. */
		src_entry->copy_on_write = dst_entry->copy_on_write = TRUE;

		/* inform pmap of the copy operation */
		pmap_copy(dst_map->pmap, src_map->pmap, dst_entry->start,
			dst_entry->end - dst_entry->start, src_entry->start);
	}
}

/*
 * Perform a virtual memory copy from the source
 * address map/range to the destination map/range.
 *
 * If src_destroy or dst_alloc is requested,
 * the source and destination regions should be
 * disjoint, not only in the top-level map, but
 * in the sharing maps as well.  [The best way
 * to guarantee this is to use a new intermediate
 * map to make copies.  This also reduces map
 * fragmentation.]
 */
int
vm_map_copy(vm_map_t dst_map, vm_map_t src_map, vm_offset_t dst_addr,
	vm_size_t len, vm_offset_t src_addr, boolean_t dst_alloc,
	boolean_t src_destroy)
{
	vm_map_entry_t	src_entry, dst_entry, tmp_entry;
	vm_offset_t	src_start, src_end, dst_start, dst_end,
			src_clip, dst_clip;
	int		result;
	boolean_t	old_src_destroy;

	/*
	 *	XXX While we figure out why src_destroy screws up,
	 *	we'll do it by explicitly vm_map_delete'ing at the end.
	 */

	old_src_destroy = src_destroy;
	src_destroy = FALSE;

	/*
	 *	Compute start and end of region in both maps
	 */

	src_start = src_addr;
	src_end = src_start + len;
	dst_start = dst_addr;
	dst_end = dst_start + len;

	/*
	 *	Check that the region can exist in both source
	 *	and destination.
	 */

	if ((dst_end < dst_start) || (src_end < src_start))
		return(KERN_NO_SPACE);

	/*
	 *	Lock the maps in question -- we avoid deadlock
	 *	by ordering lock acquisition by map value
	 */

	if (src_map == dst_map) {
		vm_map_lock(src_map);
	}
	else if ((int) src_map < (int) dst_map) {
	 	vm_map_lock(src_map);
		vm_map_lock(dst_map);
	} else {
		vm_map_lock(dst_map);
	 	vm_map_lock(src_map);
	}

	result = KERN_SUCCESS;

	/*
	 *	Check protections... source must be completely readable and
	 *	destination must be completely writable.  [Note that if we're
	 *	allocating the destination region, we don't have to worry
	 *	about protection, but instead about whether the region
	 *	exists.]
	 */

	if (src_map->is_main_map && dst_map->is_main_map) {
		if (!vm_map_check_protection(src_map, src_start, src_end,
					VM_PROT_READ)) {
			result = KERN_PROTECTION_FAILURE;
			goto Return;
		}

		if (dst_alloc) {
			if ((result = vm_map_insert(dst_map, NULL,
					(vm_offset_t) 0, dst_start, dst_end)) != KERN_SUCCESS)
				goto Return;
		}
		else if (!vm_map_check_protection(dst_map, dst_start, dst_end,
					VM_PROT_WRITE)) {
			result = KERN_PROTECTION_FAILURE;
			goto Return;
		}
	}

	/*
	 *	Find the start entries and clip.
	 *
	 *	Note that checking protection asserts that the
	 *	lookup cannot fail.
	 *
	 *	Also note that we wait to do the second lookup
	 *	until we have done the first clip, as the clip
	 *	may affect which entry we get!
	 */

	(void) vm_map_lookup_entry(src_map, src_addr, &tmp_entry);
	src_entry = tmp_entry;
	vm_map_clip_start(src_map, src_entry, src_start);

	(void) vm_map_lookup_entry(dst_map, dst_addr, &tmp_entry);
	dst_entry = tmp_entry;
	vm_map_clip_start(dst_map, dst_entry, dst_start);

	/*
	 *	If both source and destination entries are the same,
	 *	retry the first lookup, as it may have changed.
	 */

	if (src_entry == dst_entry) {
		(void) vm_map_lookup_entry(src_map, src_addr, &tmp_entry);
		src_entry = tmp_entry;
	}

	/*
	 *	If source and destination entries are still the same,
	 *	a null copy is being performed.
	 */

	if (src_entry == dst_entry)
		goto Return;

	/*
	 *	Go through entries until we get to the end of the
	 *	region.
	 */

	while (src_start < src_end) {
		/*
		 *	Clip the entries to the endpoint of the entire region.
		 */

		vm_map_clip_end(src_map, src_entry, src_end);
		vm_map_clip_end(dst_map, dst_entry, dst_end);

		/*
		 *	Clip each entry to the endpoint of the other entry.
		 */

		src_clip = src_entry->start + (dst_entry->end - dst_entry->start);
		vm_map_clip_end(src_map, src_entry, src_clip);

		dst_clip = dst_entry->start + (src_entry->end - src_entry->start);
		vm_map_clip_end(dst_map, dst_entry, dst_clip);

		/*
		 *	Both entries now match in size and relative endpoints.
		 *
		 *	If both entries refer to a VM object, we can
		 *	deal with them now.
		 */

		if (!src_entry->is_a_map && !dst_entry->is_a_map) {
			vm_map_copy_entry(src_map, dst_map, src_entry,
						dst_entry);
		}
		else {
			register vm_map_t	new_dst_map;
			vm_offset_t		new_dst_start;
			vm_size_t		new_size;
			vm_map_t		new_src_map;
			vm_offset_t		new_src_start;

			/*
			 *	We have to follow at least one sharing map.
			 */

			new_size = (dst_entry->end - dst_entry->start);

			if (src_entry->is_a_map) {
				new_src_map = src_entry->object.share_map;
				new_src_start = src_entry->offset;
			}
			else {
			 	new_src_map = src_map;
				new_src_start = src_entry->start;
				lock_set_recursive(&src_map->lock);
			}

			if (dst_entry->is_a_map) {
			    	vm_offset_t	new_dst_end;

				new_dst_map = dst_entry->object.share_map;
				new_dst_start = dst_entry->offset;

				/*
				 *	Since the destination sharing entries
				 *	will be merely deallocated, we can
				 *	do that now, and replace the region
				 *	with a null object.  [This prevents
				 *	splitting the source map to match
				 *	the form of the destination map.]
				 *	Note that we can only do so if the
				 *	source and destination do not overlap.
				 */

				new_dst_end = new_dst_start + new_size;

				if (new_dst_map != new_src_map) {
					vm_map_lock(new_dst_map);
					(void) vm_map_delete(new_dst_map,
							new_dst_start,
							new_dst_end);
					(void) vm_map_insert(new_dst_map,
							NULL,
							(vm_offset_t) 0,
							new_dst_start,
							new_dst_end);
					vm_map_unlock(new_dst_map);
				}
			}
			else {
			 	new_dst_map = dst_map;
				new_dst_start = dst_entry->start;
				lock_set_recursive(&dst_map->lock);
			}

			/*
			 *	Recursively copy the sharing map.
			 */

			(void) vm_map_copy(new_dst_map, new_src_map,
				new_dst_start, new_size, new_src_start,
				FALSE, FALSE);

			if (dst_map == new_dst_map)
				lock_clear_recursive(&dst_map->lock);
			if (src_map == new_src_map)
				lock_clear_recursive(&src_map->lock);
		}

		/*
		 *	Update variables for next pass through the loop.
		 */

		src_start = src_entry->end;
		src_entry = src_entry->next;
		dst_start = dst_entry->end;
		dst_entry = dst_entry->next;

		/*
		 *	If the source is to be destroyed, here is the
		 *	place to do it.
		 */

		if (src_destroy && src_map->is_main_map &&
						dst_map->is_main_map)
			(void)vm_map_entry_delete(src_map, src_entry->prev);
	}

	/*
	 *	Update the physical maps as appropriate
	 */

	if (src_map->is_main_map && dst_map->is_main_map) {
		if (src_destroy)
			pmap_remove(src_map->pmap, src_addr, src_addr + len);
	}

	/*
	 *	Unlock the maps
	 */

	Return: ;

	if (old_src_destroy)
		vm_map_delete(src_map, src_addr, src_addr + len);

	vm_map_unlock(src_map);
	if (src_map != dst_map)
		vm_map_unlock(dst_map);

	return(result);
}

/*
 * Create a new process vmspace structure and vm_map based on those of an
 * existing process. The new map is based on the old map, according to the
 * inheritance values on the regions in that map.
 */
struct vmspace *
vmspace_fork(struct vmspace *vm1, struct proc *p2)
{
	struct vmspace *vm2;
	vm_map_t	old_map = &vm1->vm_map, new_map;
	vm_map_entry_t	old_entry, new_entry;
	pmap_t		new_pmap;

	vm_map_lock(old_map);

	/*
	 * Allocate a vmspace structure, including a vm_map and pmap,
	 * and initialize those structures.  The refcnt is set to 1.
	 * The remaining fields must be initialized by the caller.
	 */
	MALLOC(vm2, struct vmspace *, sizeof(struct vmspace), M_VMMAP, M_WAITOK);
	memset(vm2, 0, (caddr_t) &vm2->vm_startcopy - (caddr_t) vm2);
	vm_map_init(&vm2->vm_map, old_map->min_offset, old_map->max_offset,
		TRUE);
	pmap_pinit(&vm2->vm_pmap);
	vm2->vm_map.pmap = &vm2->vm_pmap;	/* XXX */
	vm2->vm_refcnt = 1;

	memcpy(&vm2->vm_startcopy, &vm1->vm_startcopy,
	    (caddr_t) (vm1 + 1) - (caddr_t) &vm1->vm_startcopy);
	new_pmap = &vm2->vm_pmap;		/* XXX */
	new_pmap->pm_proc = p2;			/* XXX */
	p2->p_vmspace = vm2;			/* XXX */
	new_map = &vm2->vm_map;			/* XXX */

	/* replicate map entries for generating the new image. */
	for (old_entry = old_map->header.next; old_entry != &old_map->header;
	    old_entry = old_entry->next)

	    switch (old_entry->inheritance) {
	    case VM_INHERIT_NONE: /* child does not inherit the entry. */
			break;

	    case VM_INHERIT_SHARE: /* child shares the entry with the parent. */

		/* if entry doesn't use a sharing map, make it */
		if (!old_entry->is_a_map) {
		 	vm_map_t	new_share_map;
			vm_map_entry_t	new_share_entry;
				
			/* create a sharing map. */
			new_share_map = vm_map_create(NULL, old_entry->start,
			    old_entry->end, TRUE);
			new_share_map->is_main_map = FALSE;

			/* clone parent entry, move into share map. */
			new_share_entry = vm_map_entry_create(new_share_map);
			*new_share_entry = *old_entry;
			vm_map_entry_link(new_share_map,
			    new_share_map->header.prev, new_share_entry);

			/* point original parent entry to share map. */
			old_entry->is_a_map = TRUE;
			old_entry->object.share_map = new_share_map;
			old_entry->offset = old_entry->start;
		}

		/* clone parent entry, bump reference, insert into child map. */
		new_entry = vm_map_entry_create(new_map);
		*new_entry = *old_entry;
		vm_map_reference(new_entry->object.share_map);
		vm_map_entry_link(new_map, new_map->header.prev, new_entry);

		/* inform pmap of replication of the entry's region */
		pmap_copy(new_map->pmap, old_map->pmap, new_entry->start,
		    (old_entry->end - old_entry->start), old_entry->start);
		break;

	    case VM_INHERIT_COPY: /* child copies the entry from the parent. */

		/* allocate & link new empty placeholder entry into child */
		new_entry = vm_map_entry_create(new_map);
		*new_entry = *old_entry;
		new_entry->wired_count = 0;
		new_entry->object.vm_object = NULL;
		new_entry->is_a_map = FALSE;
		vm_map_entry_link(new_map, new_map->header.prev, new_entry);

		/* if parent entry references a sharing map ... */
		if (old_entry->is_a_map)
			/* evaluate indirection and copy entry */
			(void) vm_map_copy(new_map, old_entry->object.share_map,
			    new_entry->start, (vm_size_t)(new_entry->end -
			    new_entry->start), old_entry->offset, FALSE, FALSE);
		else
			vm_map_copy_entry(old_map, new_map, old_entry,
			    new_entry);
		break;
	    }

	new_map->size = old_map->size;
	vm_map_unlock(old_map);
	return(vm2);
}

/* Release a reference on a vmspace, and free it if no references. */
void
vmspace_free(struct vmspace *vm)
{

	if (--vm->vm_refcnt > 0)
		return;

	/*
	 * Lock the map, to wait out all other references to it.
	 * Delete all of the mappings and pages they hold,
	 * then call the pmap module to reclaim anything left.
	 */
	vm_map_lock(&vm->vm_map);
	(void) vm_map_delete(&vm->vm_map, vm->vm_map.min_offset,
	    vm->vm_map.max_offset);
	pmap_release(&vm->vm_pmap);
	FREE(vm, M_VMMAP);
}

/*
 * Find the VM object, offset, and protection for a given virtual
 * address in the specified map, assuming a page fault of the
 * type specified. If successful, locks the map until
 * vm_map_lookup_done is called with a supplied map handle. If
 * object and shadow object creation have been postponed, they
 * will be done now.
 */
int
vm_map_lookup(vm_map_t *var_map, vm_offset_t vaddr, vm_prot_t fault_type,
	vm_map_entry_t *out_entry, vm_object_t *object, vm_offset_t *offset,
	vm_prot_t *out_prot, boolean_t *wired, boolean_t *single_use)
{
	vm_map_t		share_map;
	vm_offset_t		share_offset;
	vm_map_entry_t		entry;
	vm_map_t		map = *var_map;
	vm_prot_t		prot;
	boolean_t		su;

RetryLookup:
	vm_map_lock_read(map);

	/* check hint before resorting to vm_map_lookup_entry() */
	entry = map->hint;
	if ((entry == &map->header) ||
	    (vaddr < entry->start) || (vaddr >= entry->end)) {

		/*
		 * Entry was either not a valid hint, or the vaddr
		 * was not contained in the entry, so do a full lookup.
		 */
		if (!vm_map_lookup_entry(map, vaddr, &entry)) {
			vm_map_unlock_read(map);
			return(KERN_INVALID_ADDRESS);
		}
	}
	*out_entry = entry;

	/* indirect through submap and retry lookup */
	if (entry->is_sub_map) {
		vm_map_t	old_map = map;

		*var_map = map = entry->object.sub_map;
		vm_map_unlock_read(old_map);
		goto RetryLookup;
	}
		
	/* is entry protection adequate for this fault? */
	prot = entry->protection;
	if ((fault_type & (prot)) != fault_type) {
		vm_map_unlock_read(map);
		return(KERN_PROTECTION_FAILURE);
	}

	/* if entry is wired, insure that all access is maintained */
	if (*wired = (entry->wired_count != 0))
		fault_type = entry->protection;

	/* if the entry refers to a share map, indirect through it */
	su = !entry->is_a_map;
	if (su == 0) {
		vm_map_entry_t	share_entry;

		/* find sharing map and locate offset within the map */
		share_map = entry->object.share_map;
		share_offset = (vaddr - entry->start) + entry->offset;

		/* lock map and find associated share entry */
		vm_map_lock_read(share_map);
		if (!vm_map_lookup_entry(share_map, share_offset,
					&share_entry)) {

			/* no entry, fail lookup */
			vm_map_unlock_read(share_map);
			vm_map_unlock_read(map);
			return(KERN_INVALID_ADDRESS);
		}
		entry = share_entry;
	} else {
		/* indicate no share map by setting to map value */
	 	share_map = map;
		share_offset = vaddr;
	}

	/* this entry requires a copy object to be created */
	if (entry->needs_copy) {

		/* if a write fault, must create copy object */
		if (fault_type & VM_PROT_WRITE) {

			/* upgrade read to write lock */
			if (lock_read_to_write(&share_map->lock)) {

				/* failed, back out and retry locks */
				if (share_map != map)
					vm_map_unlock_read(map);
				goto RetryLookup;
			}

			/* insert shadow object to hold modifications */
			vm_object_shadow(
				&entry->object.vm_object,
				&entry->offset,
				(vm_size_t) (entry->end - entry->start));
				
			/* clear copy object request, lose write lock */
			entry->needs_copy = FALSE;
			lock_write_to_read(&share_map->lock);
		}

		/* restrict map entry protection to not allow writes */
		else
			prot &= ~VM_PROT_WRITE;
	}

	/* if no object to hold pages for anonymous memory, make one. */
	if (entry->object.vm_object == NULL) {

		/* upgrade read lock to write lock */
		if (lock_read_to_write(&share_map->lock)) {

			/* failed, back out and retry locks */
			if (share_map != map)
				vm_map_unlock_read(map);
			goto RetryLookup;
		}

		/* allocate object to cover entry and lose write lock */
		entry->object.vm_object = vm_object_allocate(
			(vm_size_t)(entry->end - entry->start));
		entry->offset = 0;
		lock_write_to_read(&share_map->lock);
	}

	/* return object/offset pair for this fault */
	*offset = (share_offset - entry->start) + entry->offset;
	*object = entry->object.vm_object;

	/* if a share map, return indication if this is only map */
	if (!su)
		su = (share_map->ref_count == 1);

	*out_prot = prot;
	*single_use = su;
	return(KERN_SUCCESS);
}

/* Release the outstanding lookup request. */
void
vm_map_lookup_done(vm_map_t map, vm_map_entry_t entry)
{
	/* unlock the share map if used. */
	if (entry->is_a_map)
		vm_map_unlock_read(entry->object.share_map);

	/* unlock the original map. */
	vm_map_unlock_read(map);
}

/*
 * Force a region of address space to be actively mapped and ready for use.
 */
int
vmspace_activate(struct vmspace *vs, caddr_t va, unsigned sz)
{
	vm_map_entry_t	entry;
	vm_offset_t	start = trunc_page(va), end = round_page(va+sz-1), adr;
	vm_map_t	map = &vs->vm_map;
	int result;

	VM_MAP_LOCK_CLIP_START;

	/* force address space mapped in the region at greatest level allowed */
	lock_set_recursive(&map->lock);
	VM_MAP_LOOPENT(entry) {
	    vm_map_clip_end(map, entry, end);

		/* force mapped by simulating fault on all pages */
		for (adr = entry->start ; adr < entry->end; adr += PAGE_SIZE)
			if ((result = vm_fault(map, adr, entry->max_protection,
			    FALSE)) != KERN_SUCCESS)
				goto abort;
	}
abort:
	lock_clear_recursive(&map->lock);

	vm_map_unlock(map);
	return (result);
}

#ifdef notyet
/*
 * Force a region of address space to not be mapped (but still be allocated).
 */
void
vmspace_deactivate(struct vmspace *vs, caddr_t va, unsigned sz)
{
	vm_map_entry_t	entry;
	vm_offset_t	start = trunc_page(va), end = round_page(va+sz-1);
	vm_map_t	map = &vs->vm_map;
	int result;

	VM_MAP_LOCK_CLIP_START;

	/* force any objects in the region to deactivate pages with the range */
	lock_set_recursive(&map->lock);
	VM_MAP_LOOPENT(entry) {
	    vm_map_clip_end(map, entry, end);

		/* if entry indirects through a share map... */
		if (entry->is_a_map) {
			vm_map_entry_t	share_entry;
			vm_offset_t	share_end;

			/* ...find starting shared entry and... */
			vm_map_lock(entry->object.share_map);
			(void) vm_map_lookup_entry(entry->object.share_map,
			    entry->offset, &share_entry);
			share_end = entry->offset +
			    (entry->end - entry->start);

			/* ... deactivate shared entries */
			while ((share_entry != 
			    &entry->object.share_map->header) &&
				(share_entry->start < share_end)) {

				vm_object_page_deactivate(share_entry->object,
				    (max(share_entry->start, entry->offset) -
					entry->offset + entry->start),
				    min(share_entry->end, share_end) -
					entry->offset + entry->start);

					share_entry = share_entry->next;
			}
			vm_map_unlock(entry->object.share_map);
		}
		/* ... otherwise, just update private entries. */
		else
		 	vm_object_page_deactivate(entry->object, entry->offset,
				entry->offset + entry->end - entry->start);
	}
abort:
	lock_clear_recursive(&map->lock);

	vm_map_unlock(map);
}

/*
 * Move/copy by reference the portion of address space containing the
 * source segment to another address space, returning the address in the
 * new address space if successful (otherwise return the zero value).
 */
vm_offset_t
vmspace_move(struct vmspace *src, vm_offset_t src_addr, struct vmspace *dst,
	vm_size_t num_bytes, boolean_t src_dealloc)
{
	vm_map_t src_map = src->vm_map, dst_map = dst->vm_map;
	vm_offset_t	src_start;	/* Beginning of region */
	vm_size_t	src_size;	/* Size of rounded region */
	vm_offset_t	dst_start;	/* destination address */
	int	result;

	/* Page-align the source region */
	src_start = trunc_page(src_addr);
	src_size = round_page(src_addr + num_bytes) - src_start;

	/* moving to nowhere means deletion of the source region */
	if (dst_map == NULL) {
		if (src_dealloc)
			vmspace_delete(src_map, src_start, src_size);
		return(0);
	}

	/* attempt to allocate address space in target */
	dst_start = vm_map_min(dst_map);
	if (vmspace_allocate(dst, &dst_start, src_size, TRUE) != KERN_SUCCESS)
		return (0);

	/* attempt to move or copy by reference source to destionation */
	if (vm_map_copy(dst_map, src_map, dst_start, src_size,
	    src_start, FALSE, src_dealloc) != KERN_SUCCESS);
		return (0);

	return(dst_start + (src_addr - src_start));
}
#endif
#if defined(DEBUG) || defined(DDB)
/*
 *	vm_map_print:	[ debug ]
 */
void
vm_map_print(vm_map_t map, boolean_t full)
{
	register vm_map_entry_t	entry;
	extern int indent;

	iprintf("%s map 0x%x: pmap=0x%x,ref=%d,nentries=%d,version=%d\n",
		(map->is_main_map ? "Task" : "Share"),
 		(int) map, (int) (map->pmap), map->ref_count, map->nentries,
		map->timestamp);

	if (!full && indent)
		return;

	indent += 2;
	for (entry = map->header.next; entry != &map->header;
				entry = entry->next) {
		iprintf("map entry 0x%x: start=0x%x, end=0x%x, ",
			(int) entry, (int) entry->start, (int) entry->end);
		if (map->is_main_map) {
		     	static char *inheritance_name[4] =
				{ "share", "copy", "none", "donate_copy"};
			printf("prot=%x/%x/%s, ",
				entry->protection,
				entry->max_protection,
				inheritance_name[entry->inheritance]);
			if (entry->wired_count != 0)
				printf("wired, ");
		}

		if (entry->is_a_map || entry->is_sub_map) {
		 	printf("share=0x%x, offset=0x%x\n",
				(int) entry->object.share_map,
				(int) entry->offset);
			if ((entry->prev == &map->header) ||
			    (!entry->prev->is_a_map) ||
			    (entry->prev->object.share_map !=
			     entry->object.share_map)) {
				indent += 2;
				vm_map_print(entry->object.share_map, full);
				indent -= 2;
			}
				
		}
		else {
			printf("object=0x%x, offset=0x%x",
				(int) entry->object.vm_object,
				(int) entry->offset);
			if (entry->copy_on_write)
				printf(", copy (%s)",
				       entry->needs_copy ? "needed" : "done");
			printf("\n");

			if ((entry->prev == &map->header) ||
			    (entry->prev->is_a_map) ||
			    (entry->prev->object.vm_object !=
			     entry->object.vm_object)) {
				indent += 2;
				vm_object_print(entry->object.vm_object, full);
				indent -= 2;
			}
			pg("");
		}
	}
	indent -= 2;
}
#endif
