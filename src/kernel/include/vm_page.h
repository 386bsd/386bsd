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
 *	$Id: vm_page.h,v 1.2 93/02/04 20:16:08 bill Exp $
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

/* Logical memory page abstraction used to allocate primary store. */

#ifndef	_VM_PAGE_
#define	_VM_PAGE_

/*
 * An array of logical memory page abstraction structures indexed by
 * the corresponding page frame number of the managed page of RAM. Thus,
 * the reverse lookup of the physical address to logical memory structure
 * is possible (via the macro PHYS_TO_VMPAGE()).
 *
 * Pages can only be allocated to objects at specific page offsets to
 * represent a portion of the objects resident memory. Allocated pages can
 * be located by name (object/offset) using a hash table of queues that
 * each is linked on (using vm_page_lookup()). Object hold pages by
 * incorporating them in the objects queue of resident pages.
 *
 * Allocated pages can be made eligible for reclaimation by placing them
 * on the active queue when they are added to the address translation maps.
 * The pageout daemon will monitor them, deactivating candidates for
 * reclaimation by removing them from the address translation maps and placing
 * them on the inactive queue. Inactive pages may be eventually reclaimed,
 * their associated object being used to locate the pager to save any changed
 * contents on secondary backing store prior to reuse. Wired pages suspend
 * reclaimation until they become unwired. 
 */

struct vm_page {
	/* queues */
	queue_chain_t	pageq;		/* page queue(active, inactive, free) */
	queue_chain_t	hashq;		/* allocated page lookup hash table */
	queue_chain_t	listq;		/* allocated object (O)*/
	/* allocated page "name" */
	vm_object_t	object;		/* page holder */
	vm_offset_t	offset;		/* page position within object */
	/* page status */
	unsigned int	wire_count:16,	/* wired map entry reference count */
			inactive:1,	/* queued inactive */
			active:1,	/* queued active */
			laundry:1,	/* must be written back before reclaim*/
			clean:1,	/* has not been modified */
			busy:1,		/* is in exclusive use */
			wanted:1,	/* another waits for exclusive use */
			tabled:1,	/* allocated to object */
			copy_on_write:1,/* clone before write access allowed */
			fictitious:1,	/* entry not part of managed array */
			fake:1,		/* contents are invalid */
			io:1;		/* can be used for I/O */

	vm_offset_t	phys_addr;	/* physical address of page */
	vm_prot_t	page_lock;	/* restricted access implied */
	vm_prot_t	unlock_request;	/* unlock request on restriction */
};

typedef struct vm_page	*vm_page_t;

#ifdef	KERNEL
extern queue_head_t	vm_page_queue_free;	/* memory free queue */
extern queue_head_t	vm_page_queue_active;	/* active memory queue */
extern queue_head_t	vm_page_queue_inactive;	/* inactive memory queue */

extern vm_page_t	vm_page_array;
extern long		first_page;	/* first physical page number */
					/* ... represented in vm_page_array */
extern long		last_page;	/* last physical page number */
					/* ... represented in vm_page_array */
					/* [INCLUSIVE] */
extern vm_offset_t	first_phys_addr; /* physical address for first_page */
extern vm_offset_t	last_phys_addr;	/* physical address for last_page */

extern int	vm_page_free_count;	/* How many pages are free? */
extern int	vm_page_active_count;	/* How many pages are active? */
extern int	vm_page_inactive_count;	/* How many pages are inactive? */
extern int	vm_page_wire_count;	/* How many pages are wired? */
extern int	vm_page_free_target;	/* How many do we want free? */
extern int	vm_page_free_min;	/* When to wakeup pageout */
extern int	vm_page_inactive_target;/* How many do we want inactive? */

#define VM_PAGE_TO_PHYS(entry)	((entry)->phys_addr)

#define IS_VM_PHYSADDR(pa) \
		((pa) >= first_phys_addr && (pa) <= last_phys_addr)

#define PHYS_TO_VM_PAGE(pa) \
		(&vm_page_array[atop(pa) - first_page ])

void	vm_set_page_size();
void /* vm_offset_t*/
	vm_page_startup(void /*vm_offset_t start, vm_offset_t end, vm_offset_t vaddr*/);
vm_page_t
	vm_page_lookup(vm_object_t object, vm_offset_t offset);
void	vm_page_rename(vm_page_t mem, vm_object_t new_object,
		vm_offset_t new_offset);
vm_page_t
	vm_page_alloc(vm_object_t object, vm_offset_t offset, boolean_t io);
void	vm_page_free(vm_page_t mem);
void	vm_page_wire(vm_page_t mem);
void	vm_page_unwire(vm_page_t mem);
void	vm_page_deactivate(vm_page_t m);
void	vm_page_activate(vm_page_t m);
boolean_t
	vm_page_zero_fill(vm_page_t m);
void	vm_page_copy(vm_page_t src_m, vm_page_t dest_m);
/* "internal" functions used externally by device_pager: */
void	vm_page_remove(vm_page_t mem);
void	vm_page_init(vm_page_t mem, vm_object_t object, vm_offset_t offset);
void	vm_page_insert(vm_page_t mem, vm_object_t object, vm_offset_t offset);

/* set logical page to default state on allocation. */
extern inline void
vm_page_init(vm_page_t mem, vm_object_t object, vm_offset_t offset)
{
	mem->busy = TRUE;
	mem->tabled = FALSE;
	vm_page_insert(mem, object, offset);
	mem->fictitious = FALSE;
	mem->page_lock = VM_PROT_NONE;
	mem->unlock_request = VM_PROT_NONE;
	mem->laundry = FALSE;
	mem->active = FALSE;
	mem->inactive = FALSE;
	mem->wire_count = 0;
	mem->clean = TRUE;
	mem->copy_on_write = FALSE;
	mem->fake = TRUE;
}

/*
 *	Functions implemented as macros
 */

#define PAGE_WAIT(m, s, interruptible)	{ \
	(m)->wanted = TRUE; \
	(void) tsleep((caddr_t)m, PVM, s, 0); \
}

#define PAGE_WAKEUP(m)	{ \
	(m)->busy = FALSE; \
	if ((m)->wanted) { \
		(m)->wanted = FALSE; \
		wakeup((caddr_t) (m)); \
	} \
}

#define vm_page_set_modified(m)	{ (m)->clean = FALSE; }
#endif	KERNEL
#endif	_VM_PAGE_
