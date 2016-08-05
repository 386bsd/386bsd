
/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	@(#)isa.c	7.2 (Berkeley) 5/13/91
 */
static char rcsid[] = "$Header: /usr/src/sys.386bsd/i386/isa/RCS/isa.c,v 1.2 92/01/21 14:34:23 william Exp Locker: root $";

/*
 * code to manage AT bus
 */

#include "param.h"
#include "systm.h"
#include "conf.h"
#include "file.h"
#include "buf.h"
#include "uio.h"
#include "syslog.h"
#include "machine/segments.h"
#include "i386/isa/isa_device.h"
#include "i386/isa/icu.h"

/*
 * Configure all ISA devices
 */
isa_configure() {
	struct isa_device *dvp;
	struct isa_driver *dp;

	splhigh();
	INTREN(IRQ_SLAVE);
	for (dvp = isa_devtab_tty; config_isadev(dvp,&ttymask); dvp++);
	for (dvp = isa_devtab_bio; config_isadev(dvp,&biomask); dvp++);
	for (dvp = isa_devtab_net; config_isadev(dvp,&netmask); dvp++);
	for (dvp = isa_devtab_null; config_isadev(dvp,0); dvp++);
#include "sl.h"
#if NSL > 0
	netmask |= ttymask;
	ttymask |= netmask;
#endif
	/* biomask |= ttymask ;  can some tty devices use buffers? */
	/*printf("biomask %x ttymask %x netmask %x\n", biomask, ttymask, netmask);*/
	splnone();
}

/*
 * Configure an ISA device.
 */
config_isadev(isdp, mp)
	struct isa_device *isdp;
	int *mp;
{
	struct isa_driver *dp;
 
	if (dp = isdp->id_driver) {
		if (isdp->id_maddr) {
			extern int atdevbase;

			isdp->id_maddr -= 0xa0000;
			isdp->id_maddr += atdevbase;
		}
		isdp->id_alive = (*dp->probe)(isdp);
		if (isdp->id_alive) {
			printf("%s%d", dp->name, isdp->id_unit);
			(*dp->attach)(isdp);
			printf(" at 0x%x ", isdp->id_iobase);
			if(isdp->id_irq) {
				int intrno;

				intrno = ffs(isdp->id_irq)-1;
				printf("irq %d ", intrno);
				INTREN(isdp->id_irq);
				if(mp)INTRMASK(*mp,isdp->id_irq);
				setidt(ICU_OFFSET+intrno, isdp->id_intr,
					 SDT_SYS386IGT, SEL_KPL);
			}
			if (isdp->id_drq != -1) printf("drq %d ", isdp->id_drq);
			printf("on isa\n");
		}
		return (1);
	} else	return(0);
}

#define	IDTVEC(name)	__CONCAT(X,name)
/* default interrupt vector table */
extern	IDTVEC(intr0), IDTVEC(intr1), IDTVEC(intr2), IDTVEC(intr3),
	IDTVEC(intr4), IDTVEC(intr5), IDTVEC(intr6), IDTVEC(intr7),
	IDTVEC(intr8), IDTVEC(intr9), IDTVEC(intr10), IDTVEC(intr11),
	IDTVEC(intr12), IDTVEC(intr13), IDTVEC(intr14), IDTVEC(intr15);
	
/*
 * Fill in default interrupt table (in case of spuruious interrupt
 * during configuration of kernel, setup interrupt control unit
 */
isa_defaultirq() {

/* first icu */
	setidt(32, &IDTVEC(intr0),  SDT_SYS386IGT, SEL_KPL);
	setidt(33, &IDTVEC(intr1),  SDT_SYS386IGT, SEL_KPL);
	setidt(34, &IDTVEC(intr2),  SDT_SYS386IGT, SEL_KPL);
	setidt(35, &IDTVEC(intr3),  SDT_SYS386IGT, SEL_KPL);
	setidt(36, &IDTVEC(intr4),  SDT_SYS386IGT, SEL_KPL);
	setidt(37, &IDTVEC(intr5),  SDT_SYS386IGT, SEL_KPL);
	setidt(38, &IDTVEC(intr6),  SDT_SYS386IGT, SEL_KPL);
	setidt(39, &IDTVEC(intr7),  SDT_SYS386IGT, SEL_KPL);

/* second icu */
	setidt(40, &IDTVEC(intr8),  SDT_SYS386IGT, SEL_KPL);
	setidt(41, &IDTVEC(intr9),  SDT_SYS386IGT, SEL_KPL);
	setidt(42, &IDTVEC(intr10),  SDT_SYS386IGT, SEL_KPL);
	setidt(43, &IDTVEC(intr11),  SDT_SYS386IGT, SEL_KPL);
	setidt(44, &IDTVEC(intr12),  SDT_SYS386IGT, SEL_KPL);
	setidt(45, &IDTVEC(intr13),  SDT_SYS386IGT, SEL_KPL);
	setidt(46, &IDTVEC(intr14),  SDT_SYS386IGT, SEL_KPL);
	setidt(47, &IDTVEC(intr15),  SDT_SYS386IGT, SEL_KPL);

	/* initialize 8259's */
	outb(0xf1,0);
	outb(0x20,0x11);
	outb(0x21,32);
	outb(0x21,4);
	outb(0x21,1);
	outb(0x21,0xff);

	outb(0xa0,0x11);
	outb(0xa1,40);
	outb(0xa1,2);
	outb(0xa1,1);
	outb(0xa1,0xff);
}

/* stuff needed for virtual to physical calculations */

struct buf *dma_bounce[8];
#define MAXDMASZ 512

/* XXX temporary crud */
kernel_space(x)
unsigned long x;
{
	if (x >= KERNBASE) return 1;
	else return 0;
}


/****************************************************************************/
/*                                 at_dma                                   */
/* set up DMA read/write operation and virtual address addr for nbytes      */
/****************************************************************************/
at_dma(read,addr,nbytes, chan)
int read;
unsigned long addr;
int nbytes;
{
	unsigned long phys;
	int s,raw;
	caddr_t bounce;

	if (kernel_space(addr)) raw = 0;
	else raw = 1;

	if(raw) {
		if (dma_bounce[chan] == 0)
			dma_bounce[chan] = geteblk(MAXDMASZ);
		bounce = dma_bounce[chan]->b_un.b_addr;
	}

	/* copy bounce buffer on write */
	if (raw && !read) bcopy(addr,bounce,nbytes);

	/* Set read/write bytes */
	if (read) {
		outb(0xC,0x46); outb(0xB,0x46);
	} else {
		outb(0xC,0x4A); outb(0xB,0x4A);
	}
	/* Send start address */
	if (raw) phys = (unsigned long) bounce;
	else phys = addr;
	/* translate to physical */
	phys = pmap_extract(pmap_kernel(), (vm_offset_t)phys);
	outb(0x4,phys & 0xFF);
	outb(0x4,(phys>>8) & 0xFF);
	outb(0x81,(phys>>16) & 0xFF);
	/* Send count */
	nbytes--;
	outb(0x5,nbytes & 0xFF);
	outb(0x5,(nbytes>>8) & 0xFF);
	/* set channel 2 */
	outb(0x0A,chan);
}

/*
 * Handle a NMI, possibly a machine check.
 * return true to panic system, false to ignore.
 */
isa_nmi(cd) {

	log(LOG_CRIT, "\nNMI port 61 %x, port 70 %x\n", inb(0x61), inb(0x70));
	return(0);
}

/*
 * Caught a stray interrupt, notify
 */
isa_strayintr(d) {

	/* for some reason, we get bursts of intr #7, even if not enabled! */
	log(LOG_ERR,"ISA strayintr %d", ffs(d)-1);
}










