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
 *	$Id: vm_object.h,v 1.3 93/11/28 19:28:59 bill Exp Locker: bill $
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

/* Holder of resident memory pages */

#ifndef	_VM_OBJECT_
#define	_VM_OBJECT_

struct pager_struct;

/*
 */

struct vm_object {
	queue_chain_t		memq;		/* Resident memory */
	queue_chain_t		object_list;	/* list of all objects */
	int			ref_count;	/* How many refs?? */
	vm_size_t		size;		/* Object size */
	short			resident_page_count;
						/* number of resident pages */
	struct vm_object	*copy;		/* Object that holds copies of
						   my changed pages */
	struct pager_struct	*pager;		/* Where to get data */
	struct vm_object	*shadow;	/* My shadow */
	vm_offset_t		shadow_offset;	/* Offset in shadow */
	vm_offset_t		paging_offset;	/* Offset in pager */
	unsigned int
				paging_in_progress:16,
						/* Paging (in or out) - don't
						   collapse or destroy */
	/* boolean_t */		can_persist:1,	/* allow to persist */
	/* boolean_t */		internal:1;	/* internally created object */
	queue_chain_t		cached_list;	/* for persistence */
};

typedef struct vm_object	*vm_object_t;

struct vm_object_hash_entry {
	queue_chain_t		hash_links;	/* hash chain links */
	vm_object_t		object;		/* object we represent */
};

typedef struct vm_object_hash_entry	*vm_object_hash_entry_t;

#ifdef	KERNEL
queue_head_t	vm_object_cached_list;	/* list of objects persisting */
int		vm_object_cached;	/* size of cached list */

long		vm_object_count;	/* count of all objects */
					/* lock for object list and count */

vm_object_t	kernel_object;		/* the single kernel object */
vm_object_t	kmem_object;

#endif	KERNEL

/*
 *	Declare procedures that operate on VM objects.
 */

void	vm_object_init();
vm_object_t
	vm_object_allocate(vm_size_t size);
void	vm_object_reference(vm_object_t object);
void	vm_object_deallocate(vm_object_t object);
void	vm_object_terminate(vm_object_t object);
void	vm_object_page_clean(vm_object_t object, vm_offset_t start,
		vm_offset_t end);
void	vm_object_pmap_copy(vm_object_t object, vm_offset_t start,
		vm_offset_t end);
void	vm_object_pmap_remove(vm_object_t object, vm_offset_t start,
		vm_offset_t end);
void	vm_object_page_remove(vm_object_t object, vm_offset_t start,
		vm_offset_t end);
void	vm_object_copy(vm_object_t src_object, vm_offset_t src_offset,
		vm_size_t size, vm_object_t *dst_object,
		vm_offset_t *dst_offset, boolean_t *src_needs_copy);
void	vm_object_shadow(vm_object_t *object, vm_offset_t *offset,
		vm_size_t length);
/*void	vm_object_setpager(vm_object_t object, struct pager_struct *pager,
		vm_offset_t paging_offset, boolean_t read_only); */
vm_object_t
	vm_object_lookup(struct pager_struct *pager);
/*void	vm_object_enter(vm_object_t object, struct pager_struct *pager);*/
void	vm_object_enter(vm_object_t object);
void	vm_object_remove(struct pager_struct *pager);
void	vm_object_collapse(vm_object_t object);
boolean_t
	vm_object_coalesce(vm_object_t prev_object, vm_object_t next_object,
		vm_offset_t prev_offset, vm_offset_t next_offset,
		vm_size_t prev_size, vm_size_t next_size);

#define	vm_object_cache(pager)	pager_cache(vm_object_lookup(pager), TRUE)
#define	vm_object_uncache(pager) pager_cache(vm_object_lookup(pager), FALSE)

/* void		vm_object_cache_clear(); */
void	vm_object_print(vm_object_t object, boolean_t full);

#define	vm_object_sleep(event, object, interruptible) \
	thread_sleep((event), &(object)->Lock, (interruptible))
#endif	_VM_OBJECT_
