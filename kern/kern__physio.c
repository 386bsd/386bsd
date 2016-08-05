
/*
 * Copyright (c) 1989, 1990, 1991, 1992 William F. Jolitz, TeleMuse
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
	William F. Jolitz, TeleMuse.
 * 4. Neither the name of the developer nor the name "386BSD"
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS A COMPONENT OF 386BSD DEVELOPED BY WILLIAM F. JOLITZ 
 * AND IS INTENDED FOR RESEARCH AND EDUCATIONAL PURPOSES ONLY. THIS 
 * SOFTWARE SHOULD NOT BE CONSIDERED TO BE A COMMERCIAL PRODUCT. 
 * THE DEVELOPER URGES THAT USERS WHO REQUIRE A COMMERCIAL PRODUCT 
 * NOT MAKE USE THIS WORK.
 *
 * FOR USERS WHO WISH TO UNDERSTAND THE 386BSD SYSTEM DEVELOPED
 * BY WILLIAM F. JOLITZ, WE RECOMMEND THE USER STUDY WRITTEN 
 * REFERENCES SUCH AS THE  "PORTING UNIX TO THE 386" SERIES 
 * (BEGINNING JANUARY 1991 "DR. DOBBS JOURNAL", USA AND BEGINNING 
 * JUNE 1991 "UNIX MAGAZIN", GERMANY) BY WILLIAM F. JOLITZ AND 
 * LYNNE GREER JOLITZ, AS WELL AS OTHER BOOKS ON UNIX AND THE 
 * ON-LINE 386BSD USER MANUAL BEFORE USE. A BOOK DISCUSSING THE INTERNALS 
 * OF 386BSD ENTITLED "386BSD FROM THE INSIDE OUT" WILL BE AVAILABLE LATE 1992.
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
 */
static char rcsid[] = "$Header: /usr/bill/working/sys/kern/RCS/kern__physio.c,v 1.3 92/01/21 21:29:06 william Exp $";

#include "param.h"
#include "systm.h"
#include "buf.h"
#include "conf.h"
#include "proc.h"
#include "malloc.h"
#include "vnode.h"
#include "specdev.h"

static physio(int (*)(), int, int, int, caddr_t, int *, struct proc *);

/*
 * Driver interface to do "raw" I/O in the address space of a
 * user process directly for read and write operations..
 */

rawread(dev, uio)
	dev_t dev; struct uio *uio;
{
	return (uioapply(physio, cdevsw[major(dev)].d_strategy, dev, uio));
}

rawwrite(dev, uio)
	dev_t dev; struct uio *uio;
{
	return (uioapply(physio, cdevsw[major(dev)].d_strategy, dev, uio));
}

static physio(strat, dev, off, rw, base, len, p)
	int (*strat)(); 
	dev_t dev;
	int rw, off;
	caddr_t base;
	int *len;
	struct proc *p;
{
	register struct buf *bp;
	int resid = *len, error;

	rw = rw == UIO_READ ? B_READ : 0;

	/* create and build a buffer header for a transfer */
	bp = (struct buf *)malloc(sizeof(*bp), M_TEMP, M_NOWAIT);
	bp->b_flags = B_BUSY | B_PHYS | rw;
	bp->b_proc = p;
	bp->b_dev = dev;
	bp->b_error = 0;
	bp->b_blkno = off/DEV_BSIZE;

	/* iteratively do I/O on as large a chunk as possible */
	do {
		bp->b_flags &= ~B_DONE;
		bp->b_un.b_addr = base;
		/* DMA controller limit */
		bp->b_bcount = min (64*1024, resid);
		resid -= bp->b_bcount;
		off += bp->b_bcount;

		/* first, check if accessible */
		if (rw == B_READ && !useracc(base, bp->b_bcount, B_WRITE))
			return (EFAULT);
		if (rw == B_WRITE && !useracc(base, bp->b_bcount, B_READ))
			return (EFAULT);

		/* lock in core */
		vslock (base, bp->b_bcount);

		/* perform transfer */
		physstrat(bp, strat, PRIBIO);

		/* unlock */
		vsunlock (base, bp->b_bcount, 0);
		resid += bp->b_resid;
		bp->b_blkno = off/DEV_BSIZE;
	} while (resid && (bp->b_flags & B_ERROR) == 0 && bp->b_resid == 0);

	error = bp->b_error;
	free(bp, M_TEMP);
	*len = resid;
	return (error);
}










