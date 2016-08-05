/*-
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department, and code derived from software contributed to
 * Berkeley by William Jolitz.
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
 *	$Id: mem.c,v 1.3 94/07/07 21:05:52 bill Exp Locker: bill $
 */

/*
 * Memory special file
 */
static char *mem_config =
	"mem 2.	 # mem device /dev/mem $Revision: 1.3 $";

#include "sys/param.h"
#include "sys/errno.h"
#include "sys/mman.h"
#include "malloc.h"
#include "uio.h"
#include "modconfig.h"
#include "prototypes.h"

#include "machine/cpu.h"

#include "vm.h"
#include "vmspace.h"

extern        char *vmmap;            /* poor name! */

int
mmrw(dev_t dev, struct uio *uio, int flags)
{
	register int o;
	register u_int c, v;
	register struct iovec *iov;
	int error = 0;
	caddr_t zbuf = NULL;

	while (uio->uio_resid > 0 && error == 0) {
		iov = uio->uio_iov;
		if (iov->iov_len == 0) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			if (uio->uio_iovcnt < 0)
				panic("mmrw");
			continue;
		}
		switch (minor(dev)) {

/* minor device 0 is physical memory */
		case 0:
			v = uio->uio_offset;
			pmap_enter(pmap_kernel(), (vm_offset_t) vmmap, v,
				uio->uio_rw == UIO_READ ? VM_PROT_READ : VM_PROT_WRITE,
				TRUE, AM_NONE);
			o = (int)uio->uio_offset & PGOFSET;
			c = (u_int)(NBPG - ((int)iov->iov_base & PGOFSET));
			c = min(c, (u_int)(NBPG - o));
			c = min(c, (u_int)iov->iov_len);
			error = uiomove((caddr_t)&vmmap[o], (int)c, uio);
			pmap_remove(pmap_kernel(), (vm_offset_t) vmmap, (vm_offset_t)vmmap+NBPG);
			continue;

/* minor device 1 is kernel memory */
		case 1:
			c = iov->iov_len;
			if (!vmspace_access(&kernspace, (caddr_t)uio->uio_offset, c,
			    uio->uio_rw == UIO_READ ? PROT_READ : PROT_WRITE))
				return(EFAULT);
			error = uiomove((caddr_t)uio->uio_offset, (int)c, uio);
			continue;

/* minor device 2 is EOF/RATHOLE */
		case 2:
			if (uio->uio_rw == UIO_READ)
				return (0);
			c = iov->iov_len;
			break;

/* minor device 12 (/dev/zero) is source of nulls on read, rathole on write */
		case 12:
			if (uio->uio_rw == UIO_WRITE) {
				c = iov->iov_len;
				break;
			}
			if (zbuf == NULL) {
				zbuf = (caddr_t)
				    malloc(CLBYTES, M_TEMP, M_WAITOK);
				memset(zbuf, 0, CLBYTES);
			}
			c = min(iov->iov_len, CLBYTES);
			error = uiomove(zbuf, (int)c, uio);
			continue;

#ifdef notyet
#ifdef i386
/* 386 I/O address space (/dev/ioport[bwl]) is a read/write access to seperate
   i/o device address bus, different than memory bus. Semantics here are
   very different than ordinary read/write, as if iov_len is a multiple
   an implied string move from a single port will be done. Note that lseek
   must be used to set the port number reliably. */
		case 14:
			if (iov->iov_len == 1) {
				u_char tmp;
				tmp = inb(uio->uio_offset);
				error = uiomove (&tmp, iov->iov_len, uio);
			} else {
				if (!useracc((caddr_t)iov->iov_base,
					iov->iov_len, uio->uio_rw))
					return (EFAULT);
				insb(uio->uio_offset, iov->iov_base,
					iov->iov_len);
			}
			break;
		case 15:
			if (iov->iov_len == sizeof (short)) {
				u_short tmp;
				tmp = inw(uio->uio_offset);
				error = uiomove (&tmp, iov->iov_len, uio);
			} else {
				if (!useracc((caddr_t)iov->iov_base,
					iov->iov_len, uio->uio_rw))
					return (EFAULT);
				insw(uio->uio_offset, iov->iov_base,
					iov->iov_len/ sizeof (short));
			}
			break;
		case 16:
			if (iov->iov_len == sizeof (long)) {
				u_long tmp;
				tmp = inl(uio->uio_offset);
				error = uiomove (&tmp, iov->iov_len, uio);
			} else {
				if (!useracc((caddr_t)iov->iov_base,
					iov->iov_len, uio->uio_rw))
					return (EFAULT);
				insl(uio->uio_offset, iov->iov_base,
					iov->iov_len/ sizeof (long));
			}
			break;
#endif
#endif

		default:
			return (ENXIO);
		}
		if (error)
			break;
		iov->iov_base += c;
		iov->iov_len -= c;
		uio->uio_offset += c;
		uio->uio_resid -= c;
	}
	if (zbuf)
		free(zbuf, M_TEMP);
	return (error);
}

/*
 * XXX experimental
 * (there is no way for dynamicly remapped regions in the kernel
 *  to shoot down mmap aliases to the kernel pages, as the association
 *  is busted by the mapping -wfj)
 */
int
mmmmap(dev_t dev, int offset, int nprot)
{

	switch (minor(dev)) {

	case 0:
		break;
	case 1:
		/* is it in kernel map? */
		if (!vmspace_access(&kernspace, (caddr_t)offset, NBPG, nprot))
			return(-1);
		/* what physical address are we at? */
		 offset = (int) pmap_extract(pmap_kernel(),
					(vm_offset_t) offset);
		/* XXX pmap_extract should return -1, not 0 for non-existant */
		/* if (offset == 0)
			return (-1); */
		break;
	default:
		return (-1);
	}
	return i386_btop(offset);	/* XXX */
}

static struct devif mem_devif =
{
	{0}, -1, -2, 0, 0, 0, 0, 0, 0,
	0, 0, 0, mmrw, mmrw, 0, 0,
	0, 0, 0, 0,
};

int mem_no;

DRIVER_MODCONFIG() {
	char *cfg_string = mem_config;
	
	/* configure device? */
	if (devif_config(&cfg_string, &mem_devif) == 0)
		return;

	/* XXX trivial kernel dependancy: kernel knows driver state. */
	mem_no = mem_devif.di_cmajor;	/* should this be a di_flags bit? */
}
