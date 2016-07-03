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
 *	from:		@(#)vnode_pager.h	7.1 (Berkeley) 12/5/90
 *	386BSD:		$Id: vnode_pager.h,v 1.2 93/02/04 20:16:59 bill Exp $
 */

#ifndef	_VNODE_PAGER_
#define	_VNODE_PAGER_	1

/*
 * VNODE pager private data.
 */
struct vnpager {
	int		vnp_flags;	/* flags */
	struct vnode	*vnp_vp;	/* vnode */
	vm_size_t	vnp_size;	/* vnode current size */
	struct exec	vnp_hdr;	/* execve() file cache */
	struct vattr	vnp_attr;
};
typedef struct vnpager	*vn_pager_t;

#define VN_PAGER_NULL	((vn_pager_t)0)

#define	VNP_PAGING	0x01		/* vnode used for pageout */
#define VNP_CACHED	0x02		/* vnode is cached */
#define VNP_EXECVE	0x04		/* vnode has execve() info */

#ifdef KERNEL

void	vnode_pager_init();
vm_pager_t
	vnode_pager_alloc(caddr_t handle, vm_size_t size, vm_prot_t prot);
void	vnode_pager_dealloc(vm_pager_t pager);
int	vnode_pager_getpage(vm_pager_t pager, vm_page_t m, boolean_t sync);
int	vnode_pager_putpage(vm_pager_t pager, vm_page_t m, boolean_t sync);
boolean_t
	vnode_pager_haspage(vm_pager_t pager, vm_offset_t offset);

struct pagerops vnodepagerops = {
	vnode_pager_init,
	vnode_pager_alloc,
	vnode_pager_dealloc,
	vnode_pager_getpage,
	vnode_pager_putpage,
	vnode_pager_haspage
};

#endif

#endif	/* _VNODE_PAGER_ */
