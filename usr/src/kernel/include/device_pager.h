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
 *	$Id: device_pager.h,v 1.3 93/12/11 15:21:17 bill Exp Locker: bill $
 */

#ifndef	_DEVICE_PAGER_
#define	_DEVICE_PAGER_	1

/*
 * Device pager private data.
 */
struct devpager {
	queue_head_t	devp_list;	/* list of managed devices */
	dev_t		devp_dev;	/* devno of device */
/* this needs to be a sparse representation, like a pageq */
	vm_page_t	devp_pages;	/* page structs for device */
	int		devp_npages;	/* size of device in pages */
	int		devp_count;	/* reference count */
	int		devp_nprot;	/* BSD dev prot */
	int		(*devp_mapfunc)(); /* BSD dev mapping function */
	vm_offset_t	devp_base;	/* -1 if uninit, value if relbase */
	vm_object_t	devp_object;	/* object representing this device */
};
typedef struct devpager	*dev_pager_t;

#define DEV_PAGER_NULL	((dev_pager_t)0)

#ifdef KERNEL

void	dev_pager_init(void);
vm_pager_t
	dev_pager_alloc(caddr_t handle, vm_size_t size, vm_prot_t prot);
void	dev_pager_dealloc(vm_pager_t pager);
int	dev_pager_getpage(vm_pager_t pager, vm_page_t m, boolean_t sync);
int	dev_pager_putpage(vm_pager_t pager, vm_page_t m, boolean_t sync);
boolean_t
	dev_pager_haspage(vm_pager_t pager, vm_offset_t offset);

struct pagerops devicepagerops = {
	dev_pager_init,
	dev_pager_alloc,
	dev_pager_dealloc,
	dev_pager_getpage,
	dev_pager_putpage,
	dev_pager_haspage
};

#endif

#endif	/* _DEVICE_PAGER_ */
