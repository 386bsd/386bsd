/*
 * Copyright (c) 1989, 1990, 1991, 1992, 1993, 1994 William F. Jolitz, TeleMuse
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
 * $Id: cpu.c,v 1.1 94/10/19 17:39:55 bill Exp $
 *
 * This file contains functions that implement the machine-dependant
 * portions of the kernel's internal facilities.
 */

#include "sys/param.h"
#include "sys/user.h"
#include "signalvar.h"
/* #include "malloc.h" */
#include "proc.h"
#include "kmem.h"
#include "prototypes.h"


#include "machine/cpu.h"
#define IPCREG
#include "machine/reg.h"
#include "machine/psl.h"

#include "segments.h"
#include "specialreg.h"
#include "sigframe.h"

/*
 * Implement the innermost part of a fork() operation, by building
 * a new kernel execution thread (consisting of a process control block
 * and new kernel stack). When the new thread runs, it's kernel context will
 * appear to be identical to it's copy, except that in the copy's thread
 * cpu_tfork() will return the value of 0, and in the new thread it will
 * return the value of 1.
 */
int
cpu_tfork(struct proc *p1, register struct proc *p2)
{
 	int diff;
	struct user *up;
	unsigned *fp, nfp;

#ifdef DIAGNOSTICx	/*XXX proc0 not initialized */
	if (p2->p_stksz < sizeof(struct pcb))
		panic("invalid kernel stack size");
#endif
	p2->p_addr = up = (struct user *) kmem_alloc(kernel_map, ctob(UPAGES), 0);
	p2->p_stksz = ctob(UPAGES);	/* XXX */
	p1->p_stksz = ctob(UPAGES);	/* XXX */
 	diff = ((caddr_t)p2->p_addr + p2->p_stksz)
		- ((caddr_t)p1->p_addr + p1->p_stksz);

	/* copy the pcb to the new process. */
	up->u_pcb = p1->p_addr->u_pcb;

	/* update the new pcb to reflect this process current status. */
	/* if (curproc == p1) { */
		asm volatile ("movl %%esp, %0" : "=m" (up->u_pcb.pcb_tss.tss_esp));
		asm volatile ("movl %%ebp, %0" : "=m" (up->u_pcb.pcb_tss.tss_ebp));
	/* } */

	up->u_pcb.pcb_tss.tss_esp += diff;
	up->u_pcb.pcb_tss.tss_ebp += diff;

	/* copy the kernel stack */
	asm volatile ("  cld ; repe ; movsl " : :
	    "D" ((caddr_t)up->u_pcb.pcb_tss.tss_esp /*+ diff*/),
	    "S" ((caddr_t)up->u_pcb.pcb_tss.tss_esp -  diff),
	    "c" (((unsigned)p1->p_addr + p1->p_stksz
		- (up->u_pcb.pcb_tss.tss_esp- diff) ) / sizeof(int)));

	/* relocate the stack and frame pointers to the new kernel stack. */
	/*up->u_pcb.pcb_tss.tss_esp += diff;
	up->u_pcb.pcb_tss.tss_ebp += diff;*/

	/* relocate the frame pointers in the new kernel stack */
	fp = (unsigned *) up->u_pcb.pcb_tss.tss_ebp;
	while ((nfp = *fp) >= (unsigned) fp - (unsigned) diff) {
		nfp += diff;
		*fp = nfp;
		fp = (unsigned *)nfp;
	}

	/* relocate md_reg pointer. */
	(int)p2->p_md.md_regs = (int) p1->p_md.md_regs + diff;
	p2->p_md.md_flags = 0;

	/* allocate a TSS for this thread. */
	alloctss(p2);

	/* set the stack's starting location on entry into kernel mode. */
	up->u_pcb.pcb_tss.tss_esp0 = (unsigned) up + p2->p_stksz;

	/* inherit only the shared kernel address space. */
	up->u_pcb.pcb_ptd = KernelPTD;

	/* update the new pcb to reflect this process current status. */
	/* if (curproc == p1) { */
		asm volatile ("movl %%ebx, %0" : "=m" (up->u_pcb.pcb_tss.tss_ebx));
		asm volatile ("movl %%edi, %0" : "=m" (up->u_pcb.pcb_tss.tss_edi));
		asm volatile ("movl %%esi, %0" : "=m" (up->u_pcb.pcb_tss.tss_esi));
	/* } */

	/* set location of the new thread's first instruction. */
	asm volatile ("movl $1f, %0; 1:" : "=m" (up->u_pcb.pcb_tss.tss_eip));

	/* are we the new thread? */
	if (curproc == 0) {
asm(".globl _tfork_child ; _tfork_child: ");
		curproc = p2;
		splnone();
		return (1);	/* child */
	}
	return (0);		/* parent */
}

/*
 * Release any remaining resources of this thread and pass control
 * to next process or thread.
 */
volatile void
cpu_texit(register struct proc *p)
{
	extern int Exit_stack;

	/* release coprocessor if we have it */
	if (p == npxproc)
		npxproc = 0;

	/* release the tss, and cut over to a temporary static tss(exit_tss) */
	freetss((sel_t)p->p_md.md_tsel);

	/* change our stack to temporary exit stack. */
	asm ("movl $%0, %%esp" : : "m"(Exit_stack));

	/* drop pcb and kernel stack. No more automatic references allowed. */
	/* if (p->p_stksz >= NBPG) */
		kmem_free(kernel_map, (vm_offset_t)p->p_addr, p->p_stksz);
	/* else
		free(p->p_addr, M_TEMP); */

	/* context switch, never to return */
	swtch();
#ifdef	DIAGNOSTIC
	panic("cpu_exit");
#endif
}

/*
 * Setup processes registers for execve()
 */
void
cpu_execsetregs(struct proc *p, caddr_t eip, caddr_t esp)
{

	p->p_md.md_regs[sESP] = (int) esp;
	p->p_md.md_regs[sEBP] = 0;	/* bottom of the fp chain */
	p->p_md.md_regs[sEIP] = (int) eip;

	p->p_addr->u_pcb.pcb_flags = 0;	/* no fp at all */
}

/*
 * Send an POSIX signal to the program in the process indicated.
 *
 * [In this version, the process *must* be the current running process. -wfj]
 *
 * This function alters the state of the program by adding a special
 * stack frame to the stack, which simulates a hardware interrupt to
 * the program. When the program is restarted, it will continue execution
 * in the signal handler associated with this signal. If the signal
 * handler returns, it will be compelled by the sigcode(see locore.s)
 * present in the user program to execute a sigreturn() (see below) to
 * reload the register state information saved in the special stack frame.
 *
 * If frame cannot be created, return an error code.
 * N.B. no checks are made for copy-on-write "stacks".
 */
int
cpu_signal(struct proc *p, int sig, int mask)
{
	sig_t catcher;
	int *regs;
	struct sigframe *fp;
	struct sigacts *ps = p->p_sigacts;
	int oonstack, frmtrap;
	extern caddr_t sigcode;
	extern int szsigcode;

#ifdef	DIAGNOSTIC
	if (curproc != p)
		panic("cpu_signal(): not current process");
#endif
	catcher = ps->ps_sigact[sig];
	regs = p->p_md.md_regs;
        oonstack = ps->ps_onstack;
	frmtrap = p->p_addr->u_pcb.pcb_flags & FM_TRAP;

	/*
	 * Find the base of the special signal stack frame. If a
	 * signal uses the special signal stack, and it's available,
	 * use that. Otherwise, use the program's stack.
	 */
        if (ps->ps_onstack == 0 && (ps->ps_sigonstack & sigmask(sig))) {
		fp = (struct sigframe *)
		    (ps->ps_sigsp - sizeof(struct sigframe));
                ps->ps_onstack++;
	} else {
		/* could be in one of two places */
		if (frmtrap)
			fp = (struct sigframe *)(regs[tESP]
				- sizeof(struct sigframe));
		else
			fp = (struct sigframe *)(regs[sESP]
				- sizeof(struct sigframe));
	}

	/* is signal the gauranteed bad signal, or is the new stack
	   frame inside the user program address space ? */ 
	if (catcher == BADSIG
	   || (unsigned) fp + sizeof(struct sigframe) >  USRSTACK) {
		asm("cpu_signal_err:");
		p->p_md.md_onfault = 0;
		return(EFAULT);
	}

	/*
	 * point the fault vector at the error return, so that any
	 * failed user process address space reference will cause
	 * a terminal error.
	 */
	asm ("movl $cpu_signal_err, %0 "
		: "=o" (p->p_md.md_onfault));

	/* construct the program visable portion of the signal frame. */
	fp->sf_signum = sig;
	fp->sf_code = ps->ps_code;
	fp->sf_scp = &fp->sf_sc;
	fp->sf_handler = catcher;

	/* save scratch registers in the opaque portion of the signal frame. */
	if(frmtrap) {
		fp->sf_eax = regs[tEAX];
		fp->sf_edx = regs[tEDX];
		fp->sf_ecx = regs[tECX];
	} else {
		fp->sf_eax = regs[sEAX];
		fp->sf_edx = regs[sEDX];
		fp->sf_ecx = regs[sECX];
	}

	/* record previous program state so sigreturn() can restore it. */
	fp->sf_sc.sc_onstack = oonstack;
	fp->sf_sc.sc_mask = mask;
	memcpy((caddr_t)fp->sf_sigcode, &sigcode, szsigcode);
	if(frmtrap) {
		fp->sf_sc.sc_sp = regs[tESP];
		fp->sf_sc.sc_fp = regs[tEBP];
		fp->sf_sc.sc_pc = regs[tEIP];
		fp->sf_sc.sc_ps = regs[tEFLAGS];
		regs[tESP] = (int)fp;
		regs[tEIP] = (int)fp->sf_sigcode;
	} else {
		fp->sf_sc.sc_sp = regs[sESP];
		fp->sf_sc.sc_fp = regs[sEBP];
		fp->sf_sc.sc_pc = regs[sEIP];
		fp->sf_sc.sc_ps = regs[sEFLAGS];
		regs[sESP] = (int)fp;
		regs[sEIP] = (int)fp->sf_sigcode;
	}

	/* clear fault vector, return success */
	p->p_md.md_onfault = 0;
	return(0);
}

/*
 * Function to restore signal context prior to signal.
 * This function is just an implementation detail to clean up
 * the special stack frame installed by cpu_signal(). To avoid
 * being spoofed, the contents of the frame are "sanity checked"
 * to insure that the kernel program is not comprimised.
 * This function is normally called from a hidden system call.
 */
int
cpu_signalreturn(struct proc *p)
{	struct sigcontext *scp;
	struct sigframe *fp;
	int *regs = p->p_md.md_regs;

	/* signal state is current top of stack contents */
	fp = (struct sigframe *) regs[sESP] ;

	/* is new stack frame inside the user program address space ? */ 
	if ((unsigned) fp + sizeof(struct sigframe) >  USRSTACK) {
		asm("cpu_signalreturn_err:");
		p->p_md.md_onfault = 0;
		return(EFAULT);
	}

	/*
	 * point the fault vector at the error return, so that any
	 * failed user process address space reference will cause
	 * a terminal error.
	 */
	asm ("movl $cpu_signalreturn_err, %0 " : "=o" (p->p_md.md_onfault));

	/* check to see if this is a valid context generated by cpu_signal() */
	if (fp->sf_scp != &fp->sf_sc) {
		p->p_md.md_onfault = 0;
		return(EINVAL);
	}
	scp = fp->sf_scp;
	if ((scp->sc_ps & PSL_MBZ) != 0 || (scp->sc_ps & PSL_MBO) != PSL_MBO) {
		p->p_md.md_onfault = 0;
		return(EINVAL);
	}

	/* restore previous program state and signal state */
        p->p_sigacts->ps_onstack = scp->sc_onstack & 1;
	p->p_sigmask = scp->sc_mask & ~sigcantmask;
	regs[sEBP] = scp->sc_fp;
	regs[sESP] = scp->sc_sp;
	regs[sEIP] = scp->sc_pc;
	regs[sEFLAGS] = (scp->sc_ps | PSL_USERSET) & ~PSL_USERCLR;

	/* restore scratch registers */
	regs[sEAX] = fp->sf_eax;
	regs[sEDX] = fp->sf_edx;
	regs[sECX] = fp->sf_ecx;

	p->p_md.md_onfault = 0;
	return(EJUSTRETURN);
}

/*
 * Force reset the processor by invalidating the entire address space!
 */
void
cpu_reset(void) {

	/* force a shutdown by unmapping entire address space ! */
	(void)memset((caddr_t) PTD, 0, NBPG);

	/* "good night, sweet prince .... <THUNK!>" */
	tlbflush(); 

	asm("	movl	$0, %esp ");

	/* NOTREACHED */
}

static int sipcreg[NIPCREG] =
  { 0,0,sEDI,sESI,sEBP,sEBX,sEDX,sECX,sEAX,sEIP,sCS,sEFLAGS,sESP,sSS };
static int ipcreg[NIPCREG] =
  { tES,tDS,tEDI,tESI,tEBP,tEBX,tEDX,tECX,tEAX,tEIP,tCS,tEFLAGS,tESP,tSS };

/*
 * Return address of desired register for ptrace() XXX
 */
int *
cpu_ptracereg(struct proc *p, int reg) {

	/*if (p->p_addr->u_pcb.pcb_flags & FM_TRAP)
		return (p->p_md.md_regs + ipcreg[reg]);
	else
		return (p->p_md.md_regs + sipcreg[reg]); */
	return (p->p_md.md_regs + reg);
}

