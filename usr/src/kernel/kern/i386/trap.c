/*
 * Copyright (c) 1989, 1992, 1994 William F. Jolitz, TeleMuse
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
 *	$Id: trap.c,v 1.1 94/10/19 17:40:12 bill Exp Locker: bill $
 *
 * Exception trap and system call handlers.
 *
 * TODO:	Split into POSIX/kernel portions, dynamic syscall mech.
 */

#undef i486
#include "sys/param.h"
#include "systm.h"
#include "proc.h"
#include "resourcevar.h"
#include "sys/user.h"
#include "kernel.h"
#ifdef KTRACE
#include "sys/ktrace.h"
#endif
#include "prototypes.h"

#include "vm_param.h"
#include "pmap.h"
#include "vm_map.h"
#include "vmmeter.h"

#include "machine/cpu.h"
#include "machine/psl.h"
#include "machine/reg.h"
#include "machine/trap.h"

#include "specialreg.h"
#include "segments.h"

extern int npxlasterror;

extern iret, syscall_entry, syscall_lret;	/* see locore.s */
static void trapexcept(struct trapframe *tf, int *);
#ifdef DDB
extern int enterddb;
#endif

/*
 * trap(frame):
 *	Exception, fault, and trap interface to kernel. This
 * common code is called from assembly language IDT gate entry
 * routines that prepare a suitable stack frame, and restore this
 * frame after the exception has been processed. Note that the
 * effect is as if the arguments were passed call by reference.
 */

/*ARGSUSED*/
void
trap(struct trapframe frame)
{
	register struct proc *p = curproc;
	struct timeval syst;
	int ucode = 0, type, code, eva, signo = 0, onfault = 0, ins;
	extern int cold;

	frame.tf_eflags &= ~PSL_NT;	/* clear nested trap XXX */
	type = frame.tf_trapno;

	/* are we in an existing thread/process context? */
	if (p) {

		/* record trap evaluation start time */
		syst = p->p_stime;

		/* engage this instantiation of fault handling */
		onfault = (int)p->p_md.md_onfault;
		p->p_md.md_onfault = 0;

#ifdef DDB
		/* if an anticapated fault with a "handler" */
		if (onfault)
			switch (frame.tf_trapno) {

			case T_BPTFLT:	/* always pass to debugger regardless */
			case T_TRCTRAP:
			if (kdb_trap (frame.tf_trapno, frame.tf_err, &frame))
				return;
			break;
		}
#endif
	}

	/* check for botched cs register on return to user */
	if ((frame.tf_eip == (int)&iret || frame.tf_eip == (int)&syscall_lret)) {
		trapexcept(&frame, &frame.tf_esp);
		return;
	}
	
	/* was program executing in user space? */
	if (ISPL(frame.tf_cs) == SEL_UPL) {

#ifdef	DIAGNOSTIC
		if (p == 0)
			panic("trap: no process, yet from user mode");
#endif

		type |= T_USER;
		p->p_md.md_regs = (int *)&frame;
		p->p_addr->u_pcb.pcb_flags |= FM_TRAP;	/* XXX used by cpu_signal() */

#ifdef DIAGNOSTIC
		if (cpl)
			panic("trap: #1 user mode at interrupt level");
#endif
	}

	code = frame.tf_err;
	switch (type) {

		/*
		 * Unexpected kernel exception, employ typical Un*x
		 * error recovery * mechanism - panic! -or-
		 * 	"When in danger or in doubt,
		 *	 run in circles,
		 *	 scream and shout"
		 */
	default:
	we_re_toast:
#ifdef DDB
		/* always pass to debugger regardless */
		if ((type == T_BPTFLT || type == T_TRCTRAP || enterddb)
		    && kdb_trap(type, code, &frame))
			return;
#endif

		printf("trap %d code %x eip %x cs %x eflags %x ",
			frame.tf_trapno, frame.tf_err, frame.tf_eip,
			frame.tf_cs, frame.tf_eflags);
		if (p)
			printf("comm %s ", p->p_comm);
		printf("cr2 %x cpl %x\n", rcr(2), cpl);
		panic("trap");
		/*NOTREACHED*/

		/*
		 * X86 segment protection fault(s)
		 */
	case T_SEGNPFLT|T_USER:		/* segment not present fault */
	case T_STKFLT|T_USER:		/* stack protection fault */
	case T_PROTFLT|T_USER:		/* general protection fault */
	case T_FPOPFLT|T_USER:		/* coprocessor operand fault */
		ucode = code + BUS_SEGM_FAULT;
		signo = SIGSEGV;
		break;

		/*
		 * Unimplemented instructions
		 */
	case T_PRIVINFLT:
	case T_PRIVINFLT|T_USER:

		/*
		 * Attempt to emulate some 486 instructions on 386, to
		 * allow a 486-only kernel to at least function.
		 */

		/* check if a two byte 486-only instruction */
		ins = *(u_short *)frame.tf_eip;

		/* bswap %eax */
		if (ins == 0xc80f) {
			frame.tf_eax = htonl(frame.tf_eax);
			frame.tf_eip += 2;
			break;
		} else
		/* wbinvd, invd */
		if (ins == 0x090f || ins == 0x080f) {
			frame.tf_eip += 2;
			break;
		}

		/* 3 byte instructions: */
		ins = *(int *)frame.tf_eip & 0xffffff;
		/* invlpg (%eax) */
		if (ins == 0x38010f) {
			lcr(3, rcr(3));
			frame.tf_eip += 3;
			break;
		}

		/* not used: these instructions other address modes,
		 xadd, cmpxchg instructions */

		/* if from kernel, die */
		if ((type & T_USER) == 0)
			goto we_re_toast;

		ucode = type &~ T_USER;
		signo = SIGILL;
		break;

	case T_ASTFLT|T_USER:		/* Allow process switch */
	case T_ASTFLT:
		goto out;

	case T_DNA:
	case T_DNA|T_USER:

		/* hardware floating point context switch? */
		if (cpu_dna && (*cpu_dna)((void *)&frame))
			break;

		/* software floating point? */
		else if (cpu_dna_em) {
			if ((*cpu_dna_em)((void *)&frame) == 0)
				break;
		}
		signo = SIGFPE;
		ucode = FPE_FPU_NP_TRAP;
		break;

	case T_BOUND|T_USER:
		/* ucode = SEGV_BOUND_TRAP; */
		signo = SIGSEGV;
		break;

	case T_OFLOW|T_USER:
		/* ucode = SEGV_OFLOW_TRAP; */
		signo = SIGSEGV;
		break;

	/* case T_ALIGN|T_USER:
		signo = SIGBUS;
		break; */

	case T_DIVIDE|T_USER:
		ucode = FPE_INTDIV_TRAP;
		signo = SIGFPE;
		break;

	case T_ARITHTRAP|T_USER:
		ucode = npxerror();
		signo = SIGFPE;
		break;

	case T_PAGEFLT:			/* kernel page fault */
	case T_PAGEFLT|T_USER:		/* user page fault */
	    {
		vm_offset_t va;
		struct vmspace *vm = 0;
		vm_map_t map;
		extern vm_map_t kernel_map;
		unsigned nss = 0;

		eva = rcr(2);
		va = trunc_page((vm_offset_t)eva);

		/* if reference to kernel space from kernel, use kernel map */
		if (type == T_PAGEFLT && va >= KERNBASE)
			map = kernel_map;

		/* otherwise, either a user address or page table fault */
		else {
			if (p == 0 || (vm = p->p_vmspace) == 0)
				panic ("trap: invalid user space fault");
			map = &vm->vm_map;

			/* are we a page table reference */
			if (type == T_PAGEFLT && va >= PT_MIN_ADDRESS) {
				pmap_ptalloc(&vm->vm_pmap, vm->vm_pmap.pm_pdir
					+ i386_btop(va - PT_MIN_ADDRESS));
				return;
			}
		}

		/* does a user stack grow and limit check need to be done? */
		if (vm && vm->vm_ssize && (caddr_t)va >= vm->vm_maxsaddr) {

			/* has the stack exceeded the limit bounds? */
			nss = clrnd(btoc((unsigned)vm->vm_maxsaddr
				+ MAXSSIZ - (unsigned)va));
			if (nss > btoc(p->p_rlimit[RLIMIT_STACK].rlim_cur))
				goto nogo;

			/* does more stack need to be allocated? */
			if (nss > vm->vm_ssize) {
				vm_offset_t oldtospage = (vm_offset_t)
				    vm->vm_maxsaddr + MAXSSIZ - ctob(vm->vm_ssize);
				vm_offset_t sz =  oldtospage - va;

				/* fail if the idiot mapped it already */
				if (vmspace_allocate(vm, &va, sz, FALSE)
					!= KERN_SUCCESS)
					goto nogo;

				vm->vm_ssize = nss;
			}
		}

		/* if fault cannot be handled transparently, it is terminal */
		if (vm_fault(map, va, (code & PGEX_W) ?
		    VM_PROT_READ | VM_PROT_WRITE : VM_PROT_READ,
		    FALSE) != KERN_SUCCESS) {
		nogo:
			/* handle via prearranged kernel handler */
			if (onfault) {
				frame.tf_eip = onfault;
				return;
			}

			/* kernel fault panic */
			if (type == T_PAGEFLT) {
				printf("MMU %s fault at 0x%x\n",
				    (code & PGEX_W) ? "write" : "read", eva);
				goto we_re_toast;
			}
			signo = SIGSEGV;
		}
	    }
		break;

	case T_TRCTRAP:

		/* trace trap -- someone single stepping lcall's */
		if (frame.tf_eip == (int)&syscall_entry) {
			frame.tf_eflags &= ~PSL_T;
			p->p_md.md_flags |= MDP_SSTEP;
		} else
			goto we_re_toast;
		break;
	
	case T_BPTFLT|T_USER:		/* bpt instruction fault */
	case T_TRCTRAP|T_USER:		/* trace trap */
		frame.tf_eflags &= ~PSL_T;
		signo = SIGTRAP;
		break;

#ifdef nope
#include "isa.h"
#if	NISA > 0
	case T_NMI:
	case T_NMI|T_USER:
#ifdef	FAILSAFE
		if (inb(0x61) & 0x40) {
			extern int splstart;
			failsafe_cancel();
			printf("|%x %x:%x. ", splstart,
				frame.tf_eip, frame.tf_cs & 0xffff);
			return;
		}
#endif
#ifdef DDB
		/*
		 * NMI can be hooked up to a pushbutton for debugging
		 * (in a pinch, a meduim flat-blade screwdriver can be
		 *  used to short NMI accross to ground in an ISA slot
		 *  -- be very careful! -wfj)
		 */
		printf("NMI ... going to debugger\n");
		if (kdb_trap(type, code, &frame))
			return;
#endif
		/* machine/parity/power fail/"kitchen sink" faults */
		if(isa_nmi(code) == 0)
			return;
		else
			goto we_re_toast;
#endif
#endif
	}

	/* if no process, we are finished */
	if (p == 0)
		return;

	/* pass signal, compensating for asynch npx exception */
trap:
	p->p_md.md_flags |= MDP_SIGPROC;
	if (signo)
		trapsignal(p, signo, ucode);
	p->p_md.md_flags &= ~MDP_SIGPROC;
npxerr:
	if (p->p_md.md_flags & MDP_NPXCOLL) {
		p->p_md.md_flags &= ~MDP_NPXCOLL;
		trapsignal(p, SIGFPE, npxlasterror);
	}

	if ((type & T_USER) == 0)
		return;
out:
	/* any signals for this process? again take care with npx */
	p->p_md.md_flags |= MDP_SIGPROC;
	p->p_md.md_flags &= ~MDP_AST;
	while (signo = CURSIG(p))
		psig(signo);
	p->p_md.md_flags &= ~MDP_SIGPROC;
	if (p->p_md.md_flags & MDP_NPXCOLL)
		goto npxerr;

	/* priority restored to a user process priority only here */
	p->p_pri = p->p_usrpri;

	/* need to reschedule self with an involuntary context switch? */
	if (p->p_md.md_flags & MDP_RESCHED) {
		p->p_stats->p_ru.ru_nivcsw++;
		qswtch();
		splnone();

		/* look for signals, if any, taking care for npx */
		if (p->p_md.md_flags & MDP_AST) {
		 trapsigloop:
			p->p_md.md_flags &= ~MDP_AST;
			p->p_md.md_flags |= MDP_SIGPROC;
			while (signo = CURSIG(p))
				psig(signo);
			p->p_md.md_flags &= ~MDP_SIGPROC;
			if (p->p_md.md_flags & MDP_NPXCOLL) {
				p->p_md.md_flags &= ~MDP_NPXCOLL;
				trapsignal(p, SIGFPE, npxlasterror);
				goto trapsigloop;
			}
		}
	}

	/* need to talley any profiling ticks? */
	if (p->p_stats->p_prof.pr_scale) {
		int ticks;
		struct timeval *tv = &p->p_stime;

		ticks = ((tv->tv_sec - syst.tv_sec) * 1000 +
			(tv->tv_usec - syst.tv_usec) / 1000) / (tick / 1000);
		if (ticks)
			addupc(frame.tf_eip, &p->p_stats->p_prof, ticks);
	}

	/* update process priority */
	curpri = p->p_pri;
	p->p_addr->u_pcb.pcb_flags &= ~FM_TRAP;	/* XXX used by cpu_signal() */
}

/*
 * Process a user context reload exception that manifests itself
 * as a kernel exception. Must reorganize trap frame to process a signal,
 * rewrite frame to allow safe return to signal, and do trap signal
 * processing to recover and potentially kill a forever looping thread.
 */
void
trapexcept(struct trapframe *tf, int *sp) {
	struct proc *p = curproc;
	volatile auto int eip, eflags;
	int i;

	/* save state */
	eip = tf->tf_eip;
	eflags = tf->tf_eflags;

	/* create frame for signal generation */
	tf->tf_eip = *sp++;
	tf->tf_cs = *sp++;
	if (eip == (int)&iret)
		tf->tf_eflags = *sp++;
	tf->tf_esp = *sp++;
	tf->tf_ss = *sp;

	/* force signal. If the signal is ignored, will loop */
	p->p_md.md_flags |= MDP_SIGPROC;
	trapsignal(p, SIGSEGV, 0);

	/* any signals for this process? again take care with npx */
	p->p_md.md_flags &= ~MDP_AST;
	while (i = CURSIG(p))
		psig(i);
	p->p_md.md_flags &= ~MDP_SIGPROC;
	if (p->p_md.md_flags & MDP_NPXCOLL)
		goto npxerr;

	/* restore priority to a user priority */
	p->p_pri = p->p_usrpri;

	/* need to reschedule self? */
	if (p->p_md.md_flags & MDP_RESCHED) {
		p->p_stats->p_ru.ru_nivcsw++;
		qswtch();
		splnone();

		/* look for signals, if any, taking care for npx */
		if (p->p_md.md_flags & MDP_AST) {
		 trapsigloop:
			p->p_md.md_flags &= ~MDP_AST;
			p->p_md.md_flags |= MDP_SIGPROC;
			while (i = CURSIG(p))
				psig(i);
			p->p_md.md_flags &= ~MDP_SIGPROC;
			if (p->p_md.md_flags & MDP_NPXCOLL) {
		 npxerr:
				p->p_md.md_flags &= ~MDP_NPXCOLL;
				trapsignal(p, SIGFPE, npxlasterror);
				goto trapsigloop;
			}
		}
	}

	/* reconstruct trap frame in place */
	*sp = _udatasel;	/* force a sane value */
	*--sp = tf->tf_esp;
	if (eip == (int)&iret) {
		*--sp = tf->tf_eflags;
		tf->tf_eflags = eflags;
	}
	*--sp = _ucodesel;	/* force a sane value */
	*--sp = tf->tf_eip;

	tf->tf_eip = eip;
	tf->tf_cs = KCSEL;
	tf->tf_es = tf->tf_ds = _udatasel;
}

/*
 * Compensate for 386 brain damage (missing URKR), by being called
 * from portions of the kernel when the kernel detects a write is
 * about to occur to a read-only page of a user process. If the
 * address space cannot upgrade the page to read/write protection,
 * a false value is returned. This routine should never be called
 * in the case the processor is a 486 or better, since those processors
 * have the ability to read-protect from kernel mode.
 */
int
trapwrite(unsigned addr) {
	vm_offset_t va;

	va = trunc_page((vm_offset_t)addr);
	if (va > VM_MAX_ADDRESS)
		return(1);
	if (vm_fault(&curproc->p_vmspace->vm_map, va,
	    VM_PROT_READ | VM_PROT_WRITE, FALSE) == KERN_SUCCESS)
		return(0);
	else
		return(1);
}

/*
 * Most system calls use three words of arguments. So, lets
 * do an inline call to grab 3 and call copyin() otherwise.
 * We get minimal overhead for maximum effect.
 */
extern inline int
copyin3(struct proc *p, void *fromaddr, void *toaddr, u_int sz) {
	int rv;

	if (sz > 3 * sizeof(int)) {
		return (copyin(p, fromaddr, toaddr, sz));
	} else {
		/* set fault vector */
		asm (" movl	$4f, %0" : : "m" (p->p_md.md_onfault));

		/* copy the arguments */
		*((int *) toaddr)++ = *((int *) fromaddr)++;
		*((int *) toaddr)++ = *((int *) fromaddr)++;
		*((int *) toaddr)++ = *((int *) fromaddr)++;

		/* catch the possible fault */
		asm ("\
			xorl %0, %0 ;		\
			jmp 5f ;		\
		4:	movl %1, %0 ;		\
		5:				"
			: "=r" (rv)
			: "I" (EFAULT));

		/* clear the fault vector */
		p->p_md.md_onfault = 0;

		return (rv);
	}
}

/*
 * syscall(frame):
 *	System call request from POSIX system call gate interface to kernel.
 * Like trap(), argument is call by reference.
 */
/*ARGSUSED*/
void
syscall(volatile struct syscframe frame)
{
	caddr_t params;
	register int i;
	register struct sysent *callp;
	register struct proc *p = curproc;
	struct timeval syst;
	int error, idx, args[8], rval[2];

#ifdef DIAGNOSTIC
	if (ISPL(frame.sf_cs) != SEL_UPL || cpl) {
		pg("cpl %x call %d ", cpl, frame.sf_eax);
		pg("ss %x ", frame.sf_ss);
		pg("cs %x call %d\n", frame.sf_cs, frame.sf_eax);
		pg("eip %x call %d\n", frame.sf_eip, frame.sf_eax);
		panic("syscall");
	}
	if (p == 0)
		panic("syscall2");
#endif

	/* prepare for system call */
	syst = p->p_stime;
	idx = frame.sf_eax;
	p->p_addr->u_pcb.pcb_flags &= ~FM_TRAP;	/* XXX used by cpu_signal() */
	p->p_md.md_regs = (int *)&frame;
	params = (caddr_t)frame.sf_esp + sizeof (int) ;

	/* resume single stepping following the lcall instruction -see trap */
	if (p->p_md.md_flags & MDP_SSTEP) {
		p->p_md.md_flags &= ~MDP_SSTEP;
		frame.sf_eflags |= PSL_T;
	}

	/* locate the associated system call handler */
	callp = (idx >= nsysent) ? &sysent[0] : &sysent[idx];

	/* implement the indirect system call handler inline */
	if (callp == sysent) {
		if (error = copyin_(p, (void *)params, (void *)&idx,
		    sizeof(idx)))
			goto err;
		params += sizeof (int);
		callp = (idx >= nsysent) ? &sysent[0] : &sysent[idx];
	}

	/* copy any arguments into the kernel address space */
	if ((i = callp->sy_narg * sizeof (int)) &&
	    (error = copyin3(p, params, (caddr_t)args, (u_int)i))) {
	err:
		frame.sf_eax = error;
		frame.sf_eflags |= PSL_C;	/* carry bit */
#ifdef KTRACE
		if (KTRPOINT(p, KTR_SYSCALL))
			ktrsyscall(p->p_tracep, idx, callp->sy_narg, &args);
#endif
		goto done;
	}

	/* call system call handler */
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSCALL))
		ktrsyscall(p->p_tracep, idx, callp->sy_narg, &args);
#endif
	rval[0] = 0;
	rval[1] = frame.sf_edx;
	error = (*callp->sy_call)(p, args, rval);

	/* process any errors (or pseudo-errors) */
	if (error == ERESTART)
		frame.sf_eip -= 7;	/* backup pc, size of lcall $0x7,0 */
	else if (error != EJUSTRETURN) {
		if (error) {
			frame.sf_eax = error;
			frame.sf_eflags |= PSL_C;	/* carry bit */
		} else {
			frame.sf_eax = rval[0];
			frame.sf_edx = rval[1];
			frame.sf_eflags &= ~PSL_C;	/* carry bit */
		}
	}	/* otherwise, don't modify return register state */
	
	/* reload potentially stale process pointer (if child of fork) */
	p = curproc;

done:
	/* any signals for this process? again take care with npx */
	p->p_md.md_flags |= MDP_SIGPROC;
	p->p_md.md_flags &= ~MDP_AST;
	while (i = CURSIG(p))
		psig(i);
	p->p_md.md_flags &= ~MDP_SIGPROC;

	/* restore priority to a user priority  this time only */
	p->p_pri = p->p_usrpri;

	do {

		/* any signals for this process? again take care with npx */
		if (p->p_md.md_flags & MDP_AST) {
			p->p_md.md_flags |= MDP_SIGPROC;
			p->p_md.md_flags &= ~MDP_AST;
			while (i = CURSIG(p))
				psig(i);
			p->p_md.md_flags &= ~MDP_SIGPROC;
		}

		/* race with npx */
		if (p->p_md.md_flags & MDP_NPXCOLL) {
			p->p_md.md_flags &= ~MDP_NPXCOLL;
			trapsignal(p, SIGFPE, npxlasterror);
			continue;
		}


		/* need to reschedule self with an involuntary switch? */
		if (p->p_md.md_flags & MDP_RESCHED) {
			p->p_stats->p_ru.ru_nivcsw++;
			qswtch();
			splnone();
		}
	} while (p->p_md.md_flags & (MDP_AST|MDP_NPXCOLL|MDP_RESCHED));

	/* need to talley any profiling ticks? */
	if (p->p_stats->p_prof.pr_scale) {
		int ticks;
		struct timeval *tv = &p->p_stime;

		ticks = ((tv->tv_sec - syst.tv_sec) * 1000 +
			(tv->tv_usec - syst.tv_usec) / 1000) / (tick / 1000);
		if (ticks)
			addupc(frame.sf_eip, &p->p_stats->p_prof, ticks);
	}

#ifdef KTRACE
	if(KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p->p_tracep, idx, error, rval[0]);
#endif

	/* update current process priority */
	curpri = p->p_pri;

#ifdef DIAGNOSTIC
	if (cpl)
		panic("user mode at interrupt level3");
#endif
}

/*
 * Process a late AST found prior to system call return
 */
void
systrap(volatile struct syscframe frame) {
	register struct proc *p = curproc;
	struct timeval syst;
	int i;

	syst = p->p_stime;

	do {
		/* any signals for this process? again take care with npx */
		if (p->p_md.md_flags & MDP_AST) {
			p->p_md.md_flags |= MDP_SIGPROC;
			p->p_md.md_flags &= ~MDP_AST;
			while (i = CURSIG(p))
				psig(i);
			p->p_md.md_flags &= ~MDP_SIGPROC;
		}

		/*  */
		if (p->p_md.md_flags & MDP_NPXCOLL) {
			p->p_md.md_flags &= ~MDP_NPXCOLL;
			trapsignal(p, SIGFPE, npxlasterror);
			continue;
		}

		/* need to reschedule self? */
		if (p->p_md.md_flags & MDP_RESCHED) {
			p->p_stats->p_ru.ru_nivcsw++;
			qswtch();
			splnone();
		}
	} while (p->p_md.md_flags & (MDP_AST|MDP_NPXCOLL|MDP_RESCHED));

	/* need to talley any profiling ticks? */
	if (p->p_stats->p_prof.pr_scale) {
		int ticks;
		struct timeval *tv = &p->p_stime;

		ticks = ((tv->tv_sec - syst.tv_sec) * 1000 +
			(tv->tv_usec - syst.tv_usec) / 1000) / (tick / 1000);
		if (ticks)
			addupc(frame.sf_eip, &p->p_stats->p_prof, ticks);
	}

	/* update process priority */
	curpri = p->p_pri;
}
