/*
 * Copyright (c) 1994 William F. Jolitz, TeleMuse
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
 *	This software is a component of "386BSD" developed by 
 *	William F. Jolitz, TeleMuse.
 * 4. Neither the name of the developer nor the name "386BSD"
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 5. Non-commercial distribution of this complete file in either source and/or
 *    binary form at no charge to the user (such as from an official Internet 
 *    archive site) is permitted. 
 * 6. Commercial distribution and sale of this complete file in either source
 *    and/or binary form on any media, including that of floppies, tape, or 
 *    CD-ROM, or through a per-charge download such as that of a BBS, is not 
 *    permitted without specific prior written permission.
 * 7. Non-commercial and/or commercial distribution of an incomplete, altered, 
 *    or otherwise modified file in either source and/or binary form is not 
 *    permitted.
 *
 * THIS SOFTWARE IS A COMPONENT OF 386BSD DEVELOPED BY WILLIAM F. JOLITZ 
 * AND IS INTENDED FOR RESEARCH AND EDUCATIONAL PURPOSES ONLY. THIS 
 * SOFTWARE SHOULD NOT BE CONSIDERED TO BE A COMMERCIAL PRODUCT. 
 * THE DEVELOPER URGES THAT USERS WHO REQUIRE A COMMERCIAL PRODUCT 
 * NOT MAKE USE OF THIS WORK.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPER ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPER BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: physio.c,v 1.1 94/10/20 00:03:05 bill Exp $
 *
 * Support for Un*x device driver physical I/O functionality.
 */

#include "sys/param.h"
#include "sys/file.h"
#include "sys/mman.h"
#include "sys/errno.h"
#include "buf.h"
#include "proc.h"
#include "uio.h"
#include "modconfig.h"
#include "vm.h"
#include "kmem.h"
#include "vmspace.h"
#include "vm_fault.h"

#include "vnode.h"
#include "specdev.h"

#include "prototypes.h"

static int physio(dev_t, int, int, caddr_t, int *, struct proc *);

/*
 * Driver interface to do "raw" I/O in the address space of a
 * user process directly for read and write operations..
 */

int
rawread(dev_t dev, struct uio *uio, int ioflag)
{
	return (uioapply(physio, dev, uio));
}

int
rawwrite(dev_t dev, struct uio *uio, int ioflag)
{
	return (uioapply(physio, dev, uio));
}

/*
 * Do an "iov" of raw I/O to a given device.
 */
static int
physio(dev_t dev, int off, int rw, caddr_t base, int *len, struct proc *p)
{
	struct buf buf;
	int amttodo = *len, error, amtdone = 0, amtthispass, s,  mtype;
	vm_prot_t ftype;
	vm_offset_t adr;

	/* create and build a buffer header for a physical i/o transfer */
	buf.b_flags = B_BUSY | B_PHYS;
	buf.b_proc = p;
	buf.b_dev = dev;
	buf.b_vp = 0;
	buf.b_error = 0;
	buf.b_blkno = off/DEV_BSIZE;

	/* transfer direction reverse of memory protection */
	if (rw == UIO_READ) {
		buf.b_flags |= B_READ;
		ftype = VM_PROT_WRITE;
		mtype = PROT_WRITE;
	} else {
		ftype = VM_PROT_READ;
		mtype = PROT_READ;
	}

	/* iteratively do I/O on as large a chunk as possible */
	do {
		buf.b_flags &= ~B_DONE;

		/* XXX limit transfer */
		buf.b_bcount = amtthispass = min (256*1024 /*devif_minphys(d)*/, amttodo);

		/* first, check if accessible */
		if (!vmspace_access(p->p_vmspace, base, amtthispass, mtype))
			return (EFAULT);

		/* force user virtual address region contents resident */
		vmspace_notpageable(p->p_vmspace, base, amtthispass);

		/* force user virtual address mapped XXX */
		if (vmspace_activate(p->p_vmspace, base, amtthispass)) {
			vmspace_pageable(p->p_vmspace, base, amtthispass);
			return (EFAULT);
		}

		/* force into kernel address space XXX */
		buf.b_un.b_addr = (caddr_t)kmem_mmap(phys_map,
		    (vm_offset_t)base, (vm_size_t)amtthispass);

		/* issue the transfer */
		(void)devif_strategy(CHRDEV, &buf);

		/* wait for conclusion */
		s = splbio();
		while ((buf.b_flags & B_DONE) == 0)
			(void) tsleep((caddr_t)&buf, PRIBIO, "physio", 0);
		splx(s);

		/* free kernel address space */
		kmem_free(phys_map, (vm_offset_t)buf.b_un.b_addr, amtthispass);

		/* release user address space resident restriction */
		vmspace_pageable(p->p_vmspace, base, amtthispass);

		/* reconcile residual */
		*len = (amttodo -= (amtdone = amtthispass - buf.b_resid));
		base += amtdone;
		buf.b_blkno += amtdone/DEV_BSIZE;

		/* make consistant error codes for inconsistant drivers */
		error = buf.b_error;
		if (error == 0 && (buf.b_flags & B_ERROR) != 0)
			error = EIO;

	} while (amttodo && error == 0 && amtdone > 0);

	return (error);
}
