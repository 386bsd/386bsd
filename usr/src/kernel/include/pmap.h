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
 *	$Id: pmap.h,v 1.2 93/02/04 20:15:10 bill Exp Locker: bill $
 *
 *
 * Copyright (c) 1987, 1990 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Avadis Tevanian, Jr.
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
 *
 *	Machine address mapping definitions -- machine-independent
 *	section.  [For machine-dependent section, see "machine/pmap.h".]
 */

#ifndef	_PMAP_VM_
#define	_PMAP_VM_

/*
 * Kinds of address mappings -- corresponds to application use.
 */
#define	AM_NONE		0x0	/* no special meaning */
#define	AM_TEXT		0x1	/* sharable object mapping */
#define	AM_DATA		0x2	/* unshared mapping */
#define	AM_STACK	0x3	/* unshared stack mapping */

#include <machine/pmap.h>

#ifdef KERNEL
void	pmap_bootstrap(vm_offset_t firstaddr, vm_offset_t loadaddr);
void	pmap_init(void /*vm_offset_t phys_start, vm_offset_t phys_end*/);
struct pmap
	*pmap_create(vm_size_t size);
void	pmap_destroy(pmap_t pmap);
void	pmap_pinit(struct pmap *pmap);
void	pmap_release(struct pmap *pmap);
vm_offset_t
	pmap_map(vm_offset_t virt, vm_offset_t start, vm_offset_t end,
		vm_prot_t prot);
void	pmap_reference(struct pmap *pmap);
void	pmap_remove(struct pmap *pmap, vm_offset_t sva, vm_offset_t eva);
void	pmap_protect(struct pmap *pmap, vm_offset_t sva, vm_offset_t eva,
		vm_prot_t prot);
void	pmap_enter(struct pmap *pmap, vm_offset_t va, vm_offset_t pa,
		vm_prot_t prot, boolean_t wired, int am);
void	pmap_page_protect(vm_offset_t phys, vm_prot_t prot);
vm_offset_t
	pmap_extract(struct pmap *pmap, vm_offset_t va);
void	pmap_update();
void	pmap_collect(struct pmap *pmap);
/* needs forward declaration of struct pcb:
void	pmap_activate(struct pmap *pmap, struct pcb *pcbp); */
void	pmap_copy(struct pmap *dst_pmap, struct pmap *src_pmap,
		vm_offset_t dst_addr, vm_size_t len, vm_offset_t src_addr);
#ifndef pmap_kernel
pmap_t	pmap_kernel();
#endif
void	pmap_pageable(struct pmap *pmap, vm_offset_t sva, vm_offset_t eva,
		boolean_t pageable);
void	pmap_change_wiring(struct pmap *pmap, vm_offset_t va, boolean_t wired);
void	pmap_zero_page(vm_offset_t phys);
void	pmap_copy_page(vm_offset_t src, vm_offset_t dst);
void	pmap_clear_modify(vm_offset_t pa);
void	pmap_clear_reference(vm_offset_t pa);
boolean_t
	pmap_is_referenced(vm_offset_t pa);
boolean_t
	pmap_is_modified(vm_offset_t pa);
vm_offset_t
	pmap_phys_address(int ppn);
void	pmap_changebit(vm_offset_t pa, int bit, boolean_t setem);

/* current functions not global could be static: */
boolean_t
	pmap_testbit(vm_offset_t pa, int bit);
void	pmap_remove_all(vm_offset_t pa);
void	pmap_copy_on_write(vm_offset_t pa);

/* current functions global should be static: */
/* db_something() calls pmap_pte()! */
/* static */ struct pte *pmap_pte(struct pmap *pmap, vm_offset_t va);

/* not implemented:
void	pmap_redzone();
boolean_t
	pmap_access();
void	pmap_statistics();
void	pmap_deactivate();
*/

extern pmap_t	kernel_pmap;
#endif

#endif	_PMAP_VM_
