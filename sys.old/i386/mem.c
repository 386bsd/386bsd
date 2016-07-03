/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that: (1) source distributions retain this entire copyright
 * notice and comment, and (2) distributions including binaries display
 * the following acknowledgement:  ``This product includes software
 * developed by the University of California, Berkeley and its contributors''
 * in the documentation or other materials provided with the distribution
 * and in all advertising materials mentioning features or use of this
 * software. Neither the name of the University nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * from: Utah $Hdr: mem.c 1.13 89/10/08$
 *
 *	@(#)mem.c	7.1 (Berkeley) 5/8/90
 */

/*
 * Memory special file
 */

#include "machine/pte.h"

#include "param.h"
#include "user.h"
#include "conf.h"
#include "buf.h"
#include "systm.h"
#include "vm.h"
#include "cmap.h"
#include "uio.h"
#include "malloc.h"

#include "machine/cpu.h"

/*ARGSUSED*/
mmrw(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
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
			*(int *)mmap = (v & PG_FRAME) | PG_V |
				(uio->uio_rw == UIO_READ ? PG_KR : PG_KW);
			load_cr3(u.u_pcb.pcb_cr3);
			o = (int)uio->uio_offset & PGOFSET;
			c = (u_int)(NBPG - ((int)iov->iov_base & PGOFSET));
			c = MIN(c, (u_int)(NBPG - o));
			c = MIN(c, (u_int)iov->iov_len);
			error = uiomove((caddr_t)&vmmap[o], (int)c, uio);
			continue;

/* minor device 1 is kernel memory */
		case 1:
			c = iov->iov_len;
			if (!kernacc((caddr_t)uio->uio_offset, c,
			    uio->uio_rw == UIO_READ ? B_READ : B_WRITE))
				goto fault;
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
				bzero(zbuf, CLBYTES);
			}
			c = MIN(iov->iov_len, CLBYTES);
			error = uiomove(zbuf, (int)c, uio);
			continue;

/* minor device 16 (/dev/ioportb) accesses ioports byte wide */
		case 16:
			c = iov->iov_len;
			if(c == sizeof(char)) {
				int val;

				if (uio->uio_rw == UIO_WRITE) {
					val = fubyte(iov->iov_base);
					if (val < 0) return(EFAULT);
					outb(uio->uio_offset, val);
				} else {
					val = subyte(iov->iov_base,
						inb(uio->uio_offset));
					if (val < 0) return(EFAULT);
				}
			} else {
				if (!useracc((caddr_t)iov->iov_base, c,
				  uio->uio_rw == UIO_READ ? B_READ : B_WRITE))
					return(EFAULT);
				if (uio->uio_rw == UIO_WRITE)
					outsb(uio->uio_offset, iov->iov_base,c);
				else
					insb(uio->uio_offset, iov->iov_base, c);
			}
			break;

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
fault:
	return (EFAULT);
}
