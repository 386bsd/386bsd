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
 *	$Id: vm_map.h,v 1.2 93/02/04 20:15:43 bill Exp Locker: bill $
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

/*
 *	Virtual memory map module definitions.
 */

#ifndef	_VM_MAP_
#define	_VM_MAP_

/*
 *	Types defined:
 *
 *	vm_map_t		the high-level address map data structure.
 *	vm_map_entry_t		an entry in an address map.
 *	vm_map_version_t	a timestamp of a map, for use with vm_map_lookup
 */

/*
 *	Objects which live in maps may be either VM objects, or
 *	another map (called a "sharing map") which denotes read-write
 *	sharing with other maps.
 */

union vm_map_object {
	struct vm_object	*vm_object;	/* object object */
	struct vm_map		*share_map;	/* share map */
	struct vm_map		*sub_map;	/* belongs to another map */
};

typedef union vm_map_object	vm_map_object_t;

/*
 *	Address map entries consist of start and end addresses,
 *	a VM object (or sharing map) and offset into that object,
 *	and user-exported inheritance and protection information.
 *	Also included is control information for virtual copy operations.
 */
struct vm_map_entry {
	struct vm_map_entry	*prev;		/* previous entry */
	struct vm_map_entry	*next;		/* next entry */
	vm_offset_t		start;		/* start address */
	vm_offset_t		end;		/* end address */
	union vm_map_object	object;		/* object I point to */
	vm_offset_t		offset;		/* offset into object */
	unsigned int
				is_a_map:1,	/* Is "object" a map? */
				is_sub_map:1,	/* Is "object" a submap? */
		/* Only in sharing maps: */
				copy_on_write:1,/* is data copy-on-write */
				needs_copy:1,	/* does object need to be copied */
		/* Only in task maps: */
				canwire:1;	/* wire on fault */
	vm_prot_t		protection;	/* protection code */
	vm_prot_t		max_protection;	/* maximum protection */
	vm_inherit_t		inheritance;	/* inheritance */
	int			wired_count;	/* can be paged if = 0 */
};

typedef struct vm_map_entry	*vm_map_entry_t;

/*
 *	Maps are doubly-linked lists of map entries, kept sorted
 *	by address.  A single hint is provided to start
 *	searches again from the last successful search,
 *	insertion, or removal.
 */
struct vm_map {
	struct pmap *		pmap;		/* Physical map */
	lock_data_t		lock;		/* Lock for map data */
	struct vm_map_entry	header;		/* List of entries */
	int			nentries;	/* Number of entries */
	vm_size_t		size;		/* virtual size */
	boolean_t		is_main_map;	/* Am I a main map? */
	int			ref_count;	/* Reference count */
	vm_map_entry_t		hint;		/* hint for quick lookups */
	vm_map_entry_t		first_free;	/* First free space hint */
	boolean_t		entries_pageable; /* map entries pageable?? */
	unsigned int		timestamp;	/* Version number */
#define	min_offset		header.start
#define max_offset		header.end
};

typedef	struct vm_map	*vm_map_t;

/*
 *	Map versions are used to validate a previous lookup attempt.
 *
 *	Since lookup operations may involve both a main map and
 *	a sharing map, it is necessary to have a timestamp from each.
 *	[If the main map timestamp has changed, the share_map and
 *	associated timestamp are no longer valid; the map version
 *	does not include a reference for the imbedded share_map.]
 */
typedef struct {
	int		main_timestamp;
	vm_map_t	share_map;
	int		share_timestamp;
} vm_map_version_t;

/*
 *	Macros:		vm_map_lock, etc.
 *	Function:
 *		Perform locking on the data portion of a map.
 */

#define		vm_map_lock(map)	{ lock_write(&(map)->lock); (map)->timestamp++; }
#define		vm_map_unlock(map)	lock_write_done(&(map)->lock)
#define		vm_map_lock_read(map)	lock_read(&(map)->lock)
#define		vm_map_unlock_read(map)	lock_read_done(&(map)->lock)

/*
 *	Exported procedures that operate on vm_map_t.
 */

void	vm_map_startup(void);
vm_map_t
	vm_map_create(pmap_t pmap, vm_offset_t min, vm_offset_t max,
		boolean_t pageable);
void	vm_map_init(struct vm_map *map, vm_offset_t min, vm_offset_t max,
		boolean_t pageable);
void	vm_map_reference(vm_map_t map);
void	vm_map_deallocate(vm_map_t map);
int	vm_map_find(vm_map_t map, vm_offset_t *addr, vm_size_t length);
int	vm_map_protect(vm_map_t map, vm_offset_t start, vm_offset_t end,
		vm_prot_t new_prot, boolean_t set_max);
int	vm_map_inherit(vm_map_t map, vm_offset_t start, vm_offset_t end,
		vm_inherit_t new_inheritance);
int	vm_map_pageable(vm_map_t map, vm_offset_t start, vm_offset_t end,
		boolean_t new_pageable);
int	vm_map_remove(vm_map_t map, vm_offset_t start, vm_offset_t end);
boolean_t
	vm_map_check_protection(vm_map_t map, vm_offset_t start,
		vm_offset_t end, vm_prot_t protection);
int	vm_map_copy(vm_map_t dst_map, vm_map_t src_map, vm_offset_t dst_addr,
		vm_size_t len, vm_offset_t src_addr, boolean_t dst_alloc,
		boolean_t src_destroy);
int	vm_map_lookup(vm_map_t *var_map, vm_offset_t vaddr,
		vm_prot_t fault_type, vm_map_entry_t *out_entry,
		vm_object_t *object, vm_offset_t *offset, vm_prot_t *out_prot,
		boolean_t *wired, boolean_t *single_use);
void	vm_map_lookup_done(vm_map_t map, vm_map_entry_t entry);
void	vm_map_simplify(vm_map_t map, vm_offset_t start);

/*  "internal" routines currently used externally (vm_kern,vm_mmap): */
int	vm_map_submap(vm_map_t map, vm_offset_t start, vm_offset_t end,
		vm_map_t submap);
void	vm_map_delete(vm_map_t map, vm_offset_t start, vm_offset_t end);
int	vm_map_insert(vm_map_t map, vm_object_t object, vm_offset_t offset,
		vm_offset_t start, vm_offset_t end);
boolean_t
	vm_map_lookup_entry(vm_map_t map, vm_offset_t address,
		vm_map_entry_t *entry);

void vm_map_print(vm_map_t map, boolean_t full);

/*
 *	Functions implemented as macros
 */
#define		vm_map_min(map)		((map)->min_offset)
#define		vm_map_max(map)		((map)->max_offset)
#define		vm_map_pmap(map)	((map)->pmap)

/* XXX: number of kernel maps and entries to statically allocate */
#define MAX_KMAP	10
#define	MAX_KMAPENT	500

#endif	_VM_MAP_
