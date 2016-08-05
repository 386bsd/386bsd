
/*-
 * Copyright (c) 1990 William Jolitz.
 * Copyright (c) 1991 The Regents of the University of California.
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
 *	@(#)npx.c	7.2 (Berkeley) 5/12/91
 */
static char rcsid[] = "$Header: /usr/bill/working/sys/i386/isa/RCS/npx.c,v 1.2 92/01/21 14:34:27 william Exp $";
#include "npx.h"
#if NNPX > 0

#include "param.h"
#include "systm.h"
#include "conf.h"
#include "file.h"
#include "proc.h"
#include "machine/cpu.h"
#include "machine/pcb.h"
#include "machine/trap.h"
#include "ioctl.h"
#include "machine/specialreg.h"
#include "i386/isa/isa_device.h"
#include "icu.h"
/*
 * 387 and 287 Numeric Coprocessor Extension (NPX) Driver.
 */

int	npxprobe(), npxattach(), npxintr();
struct	isa_driver npxdriver = {
	npxprobe, npxattach, "npx",
};

struct proc *npxproc;	/* process who owns device, otherwise zero */
struct pcb *npxpcb;	/* owners context structure */
static npxexists;
extern long npx0mask;

/*
 * Probe routine - look device, otherwise set emulator bit
 */
npxprobe(dvp)
	struct isa_device *dvp;
{	static status, control;

#ifdef lint
	npxintr();
#endif

	/* insure EM bit off */
	load_cr0(rcr0() & ~CR0_EM);	/* stop emulating */
/*pg("init");*/
	asm("	fninit ");	/* put device in known state */

	/* check for a proper status of zero */
	status = 0x5a5a;	
	asm ("	fnstsw	%0 " : "=m" (status) : "m" (status) );

	if ((status&0xff) == 0) {

		/* good, now check for a proper control word */
		asm ("	fnstcw %0 " : "=m" (status) : "m" (status));

		if ((status&0x103f) == 0x3f) {
			/* then we have a numeric coprocessor */
		/* XXX should force an exception here to generate an intr */
			return (1);
		}
	}

/* insure EM bit on */
	load_cr0(rcr0() | CR0_EM);	/* start emulating */
	return (0);
}

/*
 * Attach routine - announce which it is, and wire into system
 */
npxattach(dvp)
	struct isa_device *dvp;
{

	npxinit(0x262);
	/*outb(0xb1,0);		/* reset processor */
	npxexists++;
	npx0mask = dvp->id_irq;
}

/*
 * Initialize floating point unit, usually after an error
 */
npxinit(control) {
static short wd;

	if (npxexists == 0) return;


wd = control;
	load_cr0(rcr0() & ~CR0_EM);	/* stop emulating */
#ifndef NOINTEL_COMPAT
	asm ("	fninit");
	asm("	fldcw %0" : : "g" (wd));
	asm("	fnsave %0 " : : "g" (curpcb->pcb_savefpu) );
#else
	asm("fninit");
	asm("	fnsave %0 " : : "g" (curpcb->pcb_savefpu) );
#endif
	load_cr0(rcr0() | CR0_EM);	/* start emulating */

}

/*
 * Load floating point context and record ownership to suite
 */
npxload() {

	if (npxproc) panic ("npxload");
	npxproc = curproc;
	npxpcb = curpcb;
	asm("	frstor %0 " : : "g" (curpcb->pcb_savefpu) );
}

/*
 * Unload floating point context and relinquish ownership
 */
npxunload() {

	if (npxproc == 0) panic ("npxunload");
	asm("	fsave %0 " : : "g" (npxpcb->pcb_savefpu) );
	npxproc = 0 ;
}

/*
 * Record information needed in processing an exception and clear status word
 */
npxintr(frame) struct intrframe frame; {
	struct trapframe tf;

/*pg("npxintr");*/
	outb(0xf0,0);		/* reset processor */

	/* sync state in process context structure, in advance of debugger/process looking for it */
	if (npxproc == 0 || npxexists == 0) panic ("npxintr");
	asm ("	fnsave %0 " : : "g" (npxpcb->pcb_savefpu) );

	/*
	 * Prepair a trap frame for our generic exception processing routine, trap()
	 */
	bcopy(&frame.if_es, &tf, sizeof(tf));
	tf.tf_trapno = T_ARITHTRAP;
#ifdef notyet
	/* encode the appropriate code for detailed information on this exception */
	tf.tf_err = ???;
#endif
	trap(tf);

	/*
	 * Restore with any changes to superior frame
	 */
	bcopy(&tf, &frame.if_es, sizeof(tf));

	/* clear the exception so we can catch others like it */
	asm ("	fnclex");
}

/*
 * Implement device not available (DNA) exception
 */
npxdna() {
/*pg("npxdna");*/


	if (npxexists == 0) return(0);
	load_cr0(rcr0() & ~CR0_EM);	/* stop emulating */
    	if (curpcb->pcb_flags & FP_NEEDSRESTORE)
		asm("	frstor %0 " : : "g" (curpcb->pcb_savefpu));
	curpcb->pcb_flags |= FP_WASUSED | FP_NEEDSSAVE;
	curpcb->pcb_flags &= ~FP_NEEDSRESTORE;
	npxproc = curproc;
	npxpcb = curpcb;
	return (1);
}
#endif










