/*
 * Copyright (c) 1991 Regents of the University of California.
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
 *	$Id: vm.h,v 1.2 93/02/04 20:15:18 bill Exp $
 */

#ifndef VM_H
#define VM_H
#include "vm_param.h"
#include "lock.h"
#include "queue.h"
#include "vm_prot.h"
#include "vm_inherit.h"
#include "vm_object.h"
#include "vm_page.h"
#include "vm_pager.h"
#include "vm_statistics.h"
#include "pmap.h"
#include "vm_map.h"

/* where do these prototypes belong? */
void vm_mem_init();

/* from mmap.c: */
void munmapfd(struct proc *p, int fd);

	/* caddr_t handle; XXX should be vp */
int vm_region(vm_map_t map, vm_offset_t *addr, vm_size_t *size,
    vm_prot_t *prot, vm_prot_t *max_prot, vm_inherit_t *inheritance,
    boolean_t *shared, vm_object_t *object, vm_offset_t *objoff);
int vm_allocate_with_pager(vm_map_t map, vm_offset_t *addr, vm_size_t size,
    boolean_t fitit, vm_pager_t pager, vm_offset_t poffset, boolean_t internal);
boolean_t vm_map_is_allocated(vm_map_t map, vm_offset_t start, vm_offset_t end,
    boolean_t single_entry);

/* from pageout.c: */
void	vm_init_limits(struct proc *p);

#endif /* VM_H */
