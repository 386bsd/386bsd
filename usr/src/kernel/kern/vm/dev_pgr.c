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
 *	$Id: dev_pgr.c,v 1.1 94/10/19 17:37:10 bill Exp $
 *
 * Bypass lower-level of virtual memory system to map private device memory
 * independent of the RAM memory pool.
 */

#include "sys/param.h"
#include "sys/mman.h"
#include "sys/errno.h"
#include "malloc.h"
#include "vm.h"
#include "kmem.h"
#include "device_pager.h"
#include "modconfig.h"
#include "prototypes.h"

queue_head_t	dev_pager_list;	/* list of managed devices */

#ifdef DEBUG
int	dpagerdebug = 0;
#define	DDB_FOLLOW	0x01
#define DDB_INIT	0x02
#define DDB_ALLOC	0x04
#define DDB_FAIL	0x08
#endif

void
dev_pager_init(void)
{
#ifdef DEBUG
	if (dpagerdebug & DDB_FOLLOW)
		printf("dev_pager_init()\n");
#endif
	queue_init(&dev_pager_list);
}

/*
 * Allocate pager, and, if first time, empty fictitous pages to cover
 * object, as well as object. Relys on fact that dev_pager_haspage() wil
 * be used to validate offsets into device mapped by pager.
 */
vm_pager_t
dev_pager_alloc(caddr_t handle, vm_size_t size, vm_prot_t prot)
{
	dev_t dev;
	vm_pager_t pager;
	int (*mapfunc)(), nprot;
	vm_object_t object;
	vm_page_t page;
	dev_pager_t devp;
	int npages, off;
	extern int nullop(), enodev();


#ifdef DEBUG
	if (dpagerdebug & DDB_FOLLOW)
		printf("dev_pager_alloc(%x, %x, %x)\n", handle, size, prot);
#endif

#ifdef	DIAGNOSTIC
	/* attempt to allocate pager from pageout daemon */
	if (handle == NULL)
		panic("dev_pager_alloc called");
#endif

	/* can we locate any existing pager instance for this handle? */
	pager = vm_pager_lookup(&dev_pager_list, handle);
	if (pager == NULL) {
		int xxx = (int) handle - 1; /* XXX */
		
		/* nope, allocate a new one for this device */
		dev = (dev_t)xxx;

		/* convert access protection type to BSD driver type */
		nprot = 0;
		if (prot & VM_PROT_READ)
			nprot |= PROT_READ;
		if (prot & VM_PROT_WRITE)
			nprot |= PROT_WRITE;
		if (prot & VM_PROT_EXECUTE)
			nprot |= PROT_EXEC;

		/* allocate and initialize pager structures for device reference */
		pager = (vm_pager_t)malloc(sizeof *pager, M_VMPAGER, M_WAITOK);
		if (pager == NULL)
			return(NULL);
		devp = (dev_pager_t)malloc(sizeof *devp, M_VMPGDATA, M_WAITOK);
		if (devp == NULL) {
			free((caddr_t)pager, M_VMPAGER);
			return(NULL);
		}

		devp->devp_dev = dev;
		devp->devp_nprot = nprot;
		devp->devp_base = -1;
		devp->devp_npages = atop(round_page(size));
		pager->pg_handle = handle;
		pager->pg_ops = &devicepagerops;
		pager->pg_type = PG_DEVICE;
		pager->pg_data = (caddr_t)devp;

		/* allocate object and vm_page structures to describe memory */
		npages = devp->devp_npages;
		object = devp->devp_object = vm_object_allocate(ptoa(npages));
		object->pager = pager; vm_object_enter(object);
		devp->devp_pages = (vm_page_t)
		    kmem_alloc(kernel_map, npages*sizeof(struct vm_page), 0);
		memset(devp->devp_pages, 0, npages*sizeof(struct vm_page));

		/* put it on the managed list so other can find it. */
		queue_enter(&dev_pager_list, pager, vm_pager_t, pg_list);
#ifdef DEBUG
		if (dpagerdebug & DDB_ALLOC)
			printf("dev_pager_alloc: pages %d@%x\n",
			       devp->devp_npages, devp->devp_pages);
#endif
	} else {
		/*
		 * yup, add a reference, possibly recovering object from
		 * the persistant object cache.
		 * [XXX devices should not be cached - wfj]
		 */
		devp = (dev_pager_t)pager->pg_data;
		if (vm_object_lookup(pager) != devp->devp_object)
			panic("dev_pager_setup: bad object");
	}
#ifdef DEBUG
	if (dpagerdebug & DDB_ALLOC) {
		printf("dev_pager_alloc: pager %x devp %x object %x\n",
		       pager, devp, object);
		vm_object_print(object, FALSE);
	}
#endif
	return(pager);

}

/*
 * Deallocate pager and fictitious pages, following discard of reference
 */
void
dev_pager_dealloc(vm_pager_t pager)
{
	dev_pager_t devp = (dev_pager_t)pager->pg_data;
	register vm_object_t object;

#ifdef DEBUG
	if (dpagerdebug & DDB_FOLLOW)
		printf("dev_pager_dealloc(%x)\n", pager);
#endif
	/* make pager instance unavailable for reference  */
	queue_remove(&dev_pager_list, pager, vm_pager_t, pg_list);

	/* remove ficitious pages prior to deallocation */
	object = devp->devp_object;
#ifdef DEBUG
	if (dpagerdebug & DDB_ALLOC)
		printf("dev_pager_dealloc: devp %x object %x pages %d@%x\n",
		       devp, object, devp->devp_npages, devp->devp_pages);
#endif
	while (!queue_empty(&object->memq))
		vm_page_remove((vm_page_t)queue_first(&object->memq));

	/* free ficitious page array storage */
	kmem_free(kernel_map, (vm_offset_t)devp->devp_pages,
		  devp->devp_npages * sizeof(struct vm_page));

	/* free pager and associated data */
	free((caddr_t)devp, M_VMPGDATA);
	free((caddr_t)pager, M_VMPAGER);
}

/*
 * Get/put page, should never happen, as pages are force mapped on
 * initial reference, and are never a part of page reclaimation.
 */
int
dev_pager_getpage(vm_pager_t pager, vm_page_t m, boolean_t sync)
{
#ifdef DEBUG
	if (dpagerdebug & DDB_FOLLOW)
		printf("dev_pager_getpage(%x, %x)\n", pager, m);
#endif
	return(VM_PAGER_BAD);
}

int
dev_pager_putpage(vm_pager_t pager, vm_page_t m, boolean_t sync)
{
#ifdef DEBUG
	if (dpagerdebug & DDB_FOLLOW)
		printf("dev_pager_putpage(%x, %x)\n", pager, m);
#endif
	if (pager == NULL)
		return(VM_PAGER_OK);	/* [does this ever happen? -wfj] */
	panic("dev_pager_putpage called");
	return(VM_PAGER_BAD);
}

/*
 * Check/set association between device memory (offset), protection of
 * mapping(devp_nprot) with a device's mmap interface(devif_mmap).
 */
boolean_t
dev_pager_haspage(vm_pager_t pager, vm_offset_t offset)
{
	dev_pager_t devp = (dev_pager_t)pager->pg_data;
	int off;
	vm_page_t m;

#ifdef DEBUG
	if (dpagerdebug & DDB_FOLLOW)
		printf("dev_pager_haspage(%x, %x)\n", pager, offset);
#endif

	/* does the device allow this portion to be mapped? */
	if (devif_mmap(devp->devp_dev, CHRDEV, offset, devp->devp_nprot) == -1)
		return(FALSE);

	/* have we learned our base addr? */
	if (devp->devp_base == -1)
		devp->devp_base = offset;

	/* are we within the range to be mapped? */
	off = atop(offset - devp->devp_base);
	if (off >= devp->devp_npages)
		return (FALSE);

	/* if initialized, are we the same. if not initialized, do so. */
	m = devp->devp_pages + off;
	if (m->fictitious) {
		return (m->phys_addr == pmap_phys_address(
		    devif_mmap(devp->devp_dev, CHRDEV, offset,
			devp->devp_nprot)));
	} else {
		vm_page_init(m, devp->devp_object, offset);
		m->phys_addr = pmap_phys_address(
		    devif_mmap(devp->devp_dev, CHRDEV, offset,
			devp->devp_nprot));
		m->wire_count = 1;
		m->fictitious = TRUE;
		PAGE_WAKEUP(m);
	}
	return (TRUE);
}
