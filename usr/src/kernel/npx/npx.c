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
 *	$Id: npx.c,v 1.1 94/06/26 15:16:40 bill Exp Locker: bill $
 */

/* standard AT configuration: (will always be configured if loaded) */
static	char *npx_config =
	"npx (0 13).	# hardware floating point $Revision$";

#include "sys/param.h"
#include "systm.h"
#include "sys/file.h"
#include "proc.h"
#include "sys/user.h"
#include "prototypes.h"
#include "machine/cpu.h"
#include "machine/trap.h"
#include "machine/inline/io.h"
#include "sys/ioctl.h"
#include "../kern/i386/specialreg.h" /* XXX */
#include "modconfig.h"
#include "isa_driver.h"
#include "isa_irq.h"
#include "machine/icu.h"

/*
 * 387 and 287 Numeric Coprocessor Extension (NPX) Driver.
 */

int	npxprobe(), npxattach();
void	npxintr();
struct	isa_driver npxdriver = {
	npxprobe, npxattach, npxintr, "npx", 0
};

struct proc *npxproc;	/* process who owns device, otherwise zero */
int npxexists, npxlasterror;


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
	lcr(0, rcr(0) & ~CR0_EM);	/* stop emulating */
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
	lcr(0, rcr(0) | CR0_EM);	/* start emulating */
	return (0);
}

/*
 * Attach routine - announce which it is, and wire into system
 */
npxattach(dvp)
	struct isa_device *dvp;
{

printf("npx:");
	/* npxinit(__INITIAL_NPXCW__); */
	npxexists++;
	/* lock out npx exception during interrupt processing */
	ttymask |= dvp->id_irq;
	biomask |= dvp->id_irq;
	netmask |= dvp->id_irq;
	lcr(0, (rcr(0) & ~CR0_EM) | CR0_MP);
}

/*
 * Initialize floating point unit.
 */
npxinit(control) {
	static short wd;

	if (npxexists == 0) return;


	wd = control;
	wd = 0x272;
	asm ("	clts; fninit");
	asm("	fldcw %0" : : "m" (wd));
	if (curproc) {
		struct pcb *pcbp = &curproc->p_addr->u_pcb;

		asm("	fnsave %0 " : : "m" (pcbp->pcb_savefpu));
		pcbp->pcb_flags |= FP_NEEDSRESTORE;
	}
	outb(0xf1,0);			/* reset coprocessor */
}

/*
 * Load floating point context and record ownership to suite
 */
npxload() {

	if (npxproc) panic ("npxload");
	npxproc = curproc;
	asm("	frstor %0 " : : "m" (curproc->p_addr->u_pcb.pcb_savefpu));
}

/*
 * Unload floating point context and relinquish ownership
 */
npxunload() {

	if (npxproc == 0)
		panic ("npxunload");
	asm("	fsave %0 " : : "m" (curproc->p_addr->u_pcb.pcb_savefpu));
	npxproc = 0 ;
}

/*
 * Handle npx error if passed as an interrupt
 */
void
npxintr(frame) struct intrframe frame; {

/*pg("npxintr"); */

	outb(0xf0,0);		/* clear BUSY# latch */

	/* is there a npx, and a process using it */
	if (npxexists == 0)
		panic ("npxerror");
	if (npxproc == 0)
		return;
	
	/*
	 * The npx signals exceptions asynchronously to the main
	 * processor, which means that the exception can come at
	 * inconvienent times. To compensate for this, we go through
	 * great pains to compensate for this in this implementation.
	 * Three situations are possible:
	 *
	 * interrupt from usermode:
	 *  In this case, we simulate a trap (FM_TRAP, md_regs).
	 * interrupt from process entered via syscall, trap (before/after psig):
	 *  In this case, just trapsignal().
	 *  N.B. race with PSIG stuff, intr during sendsig
	 * interrupt during psig/trapsignal, nested interrupt:
	 *  postpone trap till after (see trap()).
	 */
	
	/* if directly from user mode, simulate trap() */
	if (npxproc == curproc && ISPL(frame.if_cs) == SEL_UPL) {
		/* lie about frame */
		curproc->p_md.md_regs = (int *)&frame.if_es;
		curproc->p_addr->u_pcb.pcb_flags |= FM_TRAP;	/* used by sendsig */
	}
	
	/* if was processing signals, mark the flags so trap() will finish */
	if (curproc->p_md.md_flags & MDP_SIGPROC) {
		npxlasterror = npxerror();
		curproc->p_md.md_flags |= MDP_NPXCOLL;
	} else
	/* pass exception to process, which may not be the current one */
		trapsignal(npxproc, SIGFPE, npxerror());

	/* should not be needed */
	if (npxproc == curproc && ISPL(frame.if_cs) == SEL_UPL)
		curproc->p_addr->u_pcb.pcb_flags &= ~FM_TRAP;	/* used by sendsig */
}

/*
 * Record information needed in processing an exception and clear status word
 */
int
npxerror() {
	int cr0 = rcr(0), code;
	static status;

	/* emulating instructions? */
	/* if (cr0 & CR0_EM)
		lcr(0, cr0 & ~CR0_EM);	/* stop emulating */

	asm ("	fnstsw	%0 " : "=m" (status) : "m" (status) );

	/* sync state in process context structure, in advance of debugger/process looking for it */
	asm ("	fnsave %0 " : : "m" (npxproc->p_addr->u_pcb.pcb_savefpu) );
	npxproc->p_addr->u_pcb.pcb_flags |= FP_NEEDSRESTORE;

	/* clear the exception so we can catch others like it */
	asm ("	fnclex");

	/* encode the appropriate code for detailed information on this exception */
	code = status & 0xff;

	/* restore if emulating */
	/* lcr(0, cr0); */

	return(code);
}

/*
 * Implement device not available (DNA) exception
 */
int
npxdna() {
	static short wd;
/*pg("npxdna");*/

	if (npxexists == 0) return(0);

	asm("clts");
	if (npxproc) {
		if (curproc == npxproc) {
			return(1);
		}
		asm("fnsave %0" : : "m" (npxproc->p_addr->u_pcb.pcb_savefpu));
	} else
    		npxproc = curproc;

    	if (npxproc->p_addr->u_pcb.pcb_flags & FP_NEEDSRESTORE)
		asm("frstor %0" : : "m" (npxproc->p_addr->u_pcb.pcb_savefpu));
	else {
/*printf("rcr(0) %x ", rcr(0));
		lcr(0, (rcr(0) & ~CR0_EM) | CR0_MP); */
		asm("clts; fninit");
		wd = 0x272;
		asm("fldcw %0" : : "m" (wd));
	}

    	npxproc->p_addr->u_pcb.pcb_flags |= FP_NEEDSRESTORE;
	asm("clts");
	return (1);
}

DRIVER_MODCONFIG() {
	char *cfg_string;
	
	/* find configuration string. */
	if (!config_scan(npx_config, &cfg_string)) 
		return;

	/* configure driver into kernel program */
	(int *)cpu_dna = (int *)npxdna;

	/* probe for hardware */
	new_isa_configure(&cfg_string, &npxdriver);
}
