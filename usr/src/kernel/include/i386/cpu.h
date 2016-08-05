/*-
 * Copyright (c) 1990 The Regents of the University of California.
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
 *	@(#)cpu.h	5.4 (Berkeley) 5/9/91
 */

#ifndef _CPU_H_
#define _CPU_H_
/*
 * Definitions unique to i386 cpu support.
 */
#include "machine/frame.h"

/*
 * Selector priority, used by hardclock() to discover user/kernel mode.
 */
#define	ISPL(s)	((s) & 3)	/* what is the priority level of a selector */
#define	SEL_KPL	0		/* kernel priority level */	
#define	SEL_UPL	3		/* user priority level */	

/*
 * definitions of cpu-dependent requirements
 * referenced in generic code
 */

/* Thread create/destroy */
int cpu_tfork(struct proc *, struct proc *);
volatile void cpu_texit(struct proc *);

/* POSIX specific cpu-dependant functions */
#define cpu_wait(p)			/* recover resources after exit() */
void cpu_execsetregs(struct proc *, caddr_t, caddr_t);	/* start program */
int cpu_signal(struct proc *, int, int);		/* issue a signal */
int cpu_signalreturn(struct proc *);			/* remove a signal */
int *cpu_ptracereg(struct proc *, int);	/* return address of a register */

/* implementation dependant */
void cpu_startup(void);
void cpu_reset(void);
#define	inittodr(s)	/* sorry, no GMT todr, only localtime cmos clock */
extern int (*cpu_dna)(void *);
extern int (*cpu_dna_em)(void *);

/*
 * Arguments to hardclock, softclock and gatherstats
 * encapsulate the previous machine state in an opaque
 * clockframe.
 */
struct clock_frame {
	int cf_ipm;	/* interrupt priority mask when clock interrupted */
	int cf_eip;	/* location in program when clock interrupted */
	int cf_cs;	/* code selector when clock interrupted */
};
typedef struct clock_frame clockframe;

#define	CLKF_BASEPRI(fp)	((fp)->cf_ipm == 0)
#define	CLKF_USERMODE(fp)	(ISPL((fp)->cf_cs) == SEL_UPL)
#define	CLKF_PC(fp)		((fp)->cf_eip)
#define setsoftclock()		sclkpending++
#define setsoftnet()		netpending++

#define	resettodr()			/* no todr to set */

/*
 * If in a process, force the process to reschedule and thus be
 * preempted.
 */
#define	need_resched() \
	if (curproc) curproc->p_md.md_flags |= MDP_AST | MDP_RESCHED

/*
 * Give a profiling tick to the current process from the softclock
 * interrupt.
 */
#define	profile_tick(p, fp) addupc((fp)->cf_eip, &(p)->p_stats->p_prof, 1)

/*
 * Notify the current process (p) that it has a signal pending,
 * process as soon as possible.
 */
#define	cpu_signotify(p)	(p)->p_md.md_flags |= MDP_AST

extern int	sclkpending;	/* need to do a softclock() on return to basepri */
extern int	netpending;	/* need to do a netintr() on return to basepri */
extern volatile int	cpl;	/* current priority level(mask) of interrupt controller */

/* global bit vector of options */
extern int	cpu_option;

/* various processor dependant options, referenced during execution */
#define	CPU_386_KR	0x00000001	/* simulate kernel read protection */
#define	CPU_486_INVC	0x00000002	/* use instructions to invalidate cache on I/O */
#define	CPU_486_INVTLB	0x00000004	/* invalidate TLB cache by pages */
#define	CPU_486_NPXEXCP	0x00000008	/* npx exception instead of interrupt */
#define	CPU_486_ALIGN	0x00000010	/* align exception */
#define	CPU_PENT_WBACK	0x00000020	/* pentium write back cache */
#define	CPU_PENT_4MBPG	0x00000040	/* pentium 4MB pages */

/* by default, a 386 should ... */
#define	CPU_386_DEFAULT		(CPU_386_KR)

/* by default, a 486 should ... */
#define	CPU_486_DEFAULT		(CPU_486_NPXEXCP)

/* by default, a pentium should ... */
#define	CPU_PENT_DEFAULT	(CPU_PENT_WBACK)

/* which process context holds the coprocessor(NPX), if any */
extern struct	proc	*npxproc;

#endif
