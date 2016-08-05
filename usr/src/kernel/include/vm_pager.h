/*
 * Copyright (c) 1990 University of Utah.
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *	$Id: vm_pager.h,v 1.3 93/11/28 19:25:41 bill Exp $
 */

/*
 * Pager routine interface definition.
 * For BSD we use a cleaner version of the internal pager interface.
 */

#ifndef	_VM_PAGER_
#define	_VM_PAGER_

struct	pager_struct {
	queue_head_t	pg_list;	/* links for list management */
	caddr_t		pg_handle;	/* external handle (vp, dev, fp) */
	int		pg_type;	/* type of pager */
	struct pagerops	*pg_ops;	/* pager operations */
/*	vm_map_t	pg_shmap;	/* shared map (if shared) */
	caddr_t		pg_data;	/* private pager data */
};
typedef	struct pager_struct *vm_pager_t;

/* pager types */
#define PG_DFLT		-1
#define	PG_SWAP		0
#define	PG_VNODE	1
#define PG_DEVICE	2

struct	pagerops {
	void		(*pgo_init)();		/* initialize pager */
	vm_pager_t	(*pgo_alloc)();		/* allocate pager */
	void		(*pgo_dealloc)();	/* disassociate */
	int		(*pgo_getpage)();	/* get (read) page */
	int		(*pgo_putpage)();	/* put (write) page */
	boolean_t  	(*pgo_haspage)();	/* does pager have page? */
};

/*
 * get/put return values
 * OK	operation was successful
 * BAD	specified data was out of the accepted range
 * FAIL	specified data was in range, but doesn't exist
 * PEND	operations was initiated but not completed
 */
#define	VM_PAGER_OK	0
#define	VM_PAGER_BAD	1
#define	VM_PAGER_FAIL	2
#define	VM_PAGER_PEND	3

#define	VM_PAGER_ALLOC(h, s, p)		(*(pg)->pg_ops->pgo_alloc)(h, s, p)
#define	VM_PAGER_DEALLOC(pg)		(*(pg)->pg_ops->pgo_dealloc)(pg)
#define	VM_PAGER_GET(pg, m, s)		(*(pg)->pg_ops->pgo_getpage)(pg, m, s)
#define	VM_PAGER_PUT(pg, m, s)		(*(pg)->pg_ops->pgo_putpage)(pg, m, s)
#define	VM_PAGER_HASPAGE(pg, o)		(*(pg)->pg_ops->pgo_haspage)(pg, o)

#ifdef KERNEL
vm_pager_t
	vm_pager_allocate(int type, caddr_t handle, vm_size_t size,
		vm_prot_t prot);
void	vm_pager_deallocate(vm_pager_t pager);
int	vm_pager_get(vm_pager_t pager, vm_page_t m, boolean_t sync);
int	vm_pager_put(vm_pager_t pager, vm_page_t m, boolean_t sync);
boolean_t
	vm_pager_has_page(vm_pager_t pager, vm_offset_t offset);

vm_offset_t
	vm_pager_map_page(vm_page_t m);
void	vm_pager_unmap_page(vm_offset_t kva);
vm_pager_t
	vm_pager_lookup(queue_head_t *list, caddr_t handle);
void	vm_pager_sync();
int	pager_cache(vm_object_t object, boolean_t should_cache);

extern struct pagerops *dfltpagerops;
#endif

#endif	/* _VM_PAGER_ */
