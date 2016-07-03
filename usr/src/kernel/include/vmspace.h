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
 *	$Id: vmspace.h,v 1.2 93/02/04 20:16:56 bill Exp $
 */

#ifndef	VMSPACE_H
#define	VMSPACE_H
/*
 * Shareable process virtual address space.
 * May eventually be merged with vm_map.
 * Several fields are temporary (text, data stuff).
 */
struct vmspace {
	struct	vm_map vm_map;	/* VM address map */
	struct	pmap vm_pmap;	/* private physical map */
	int	vm_refcnt;	/* number of references */
	caddr_t	vm_shm;		/* SYS5 shared memory private data XXX */
/* we copy from vm_startcopy to the end of the structure on fork */
#define vm_startcopy vm_rssize
	segsz_t vm_rssize; 	/* current resident set size in pages */
	segsz_t vm_swrss;	/* resident set size before last swap */
	segsz_t vm_tsize;	/* text size (pages) XXX */
	segsz_t vm_dsize;	/* data size (pages) XXX */
	segsz_t vm_ssize;	/* stack size (pages) */
	caddr_t	vm_taddr;	/* user virtual address of text XXX */
	caddr_t	vm_daddr;	/* user virtual address of data XXX */
	caddr_t vm_maxsaddr;	/* user VA at max stack growth */
};

extern struct vmspace kernspace;

__BEGIN_DECLS
struct vmspace *vmspace_fork(struct vmspace *, struct proc *);
void vmspace_free(struct vmspace *);
int vmspace_allocate(struct vmspace *vs, vm_offset_t *addr, vm_size_t size,
    int anywhere);
void vmspace_delete(struct vmspace *vs, caddr_t va, unsigned sz);
int vm_mmap(struct vmspace *vs, vm_offset_t *addr, vm_size_t size,
    vm_prot_t prot, int flags, caddr_t handle, vm_offset_t foff);
int vmspace_protect(struct vmspace *vs, caddr_t va, unsigned sz, int set_max,
    vm_prot_t new_prot);
int vmspace_access(struct vmspace *vs, caddr_t va, unsigned sz, int prot);
void vmspace_pageable(struct vmspace *vs, caddr_t va, unsigned sz);
void vmspace_notpageable(struct vmspace *vs, caddr_t va, unsigned sz);
int vmspace_inherit(struct vmspace *vs, caddr_t va, unsigned sz,
    vm_inherit_t new_inheritance);
int vmspace_activate(struct vmspace *vs, caddr_t va, unsigned sz);
__END_DECLS

#endif	VMSPACE_H
