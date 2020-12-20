/*-
 * Copyright (c) 1982, 1986, 1990 The Regents of the University of California.
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
 *	$Id: synch.c,v 1.1 94/10/20 00:03:15 bill Exp $
 */

#include "sys/param.h"
#include "sys/errno.h"
#include "systm.h"	/* panicstr */
#include "proc.h"
#include "vm.h"
#include "vmspace.h"
#include "kernel.h"	/* hz, avenrunnable, lbolt */
#include "signalvar.h"
#include "resourcevar.h"

#include "machine/cpu.h"

#include "prototypes.h"

u_char	curpri;			/* usrpri of curproc */

/* Force switch among equal priority processes every 100ms. */
void
roundrobin(void)
{
	need_resched();
	timeout((int (*)())roundrobin, (caddr_t)0, hz / 10);
}

/* calculations for digital decay to forget 90% of usage in 5*loadav sec */
#define	loadfactor(loadav)	(2 * (loadav))
#define	decay_cpu(loadfac, cpu)	(((loadfac) * (cpu)) / ((loadfac) + FSCALE))

/* decay 95% of `p_pctcpu' in 60 seconds; see CCPU_SHIFT before changing */
fixpt_t	ccpu = 0.95122942450071400909 * FSCALE;		/* exp(-1/20) */

/*
 * If `ccpu' is not equal to `exp(-1/20)' and you still want to use the
 * faster/more-accurate formula, you'll have to estimate CCPU_SHIFT below
 * and possibly adjust FSHIFT in "param.h" so that (FSHIFT >= CCPU_SHIFT).
 * To estimate CCPU_SHIFT for exp(-1/20), the following formula was used:
 *  1 - exp(-1/20) ~= 0.0487 ~= 0.0488 == 1/(2^11), thus 11 bits of fraction
 * in a fixed point number need to be represented.
 *
 * If you dont want to bother with the faster/more-accurate formula, you
 * can set CCPU_SHIFT to (FSHIFT + 1) which will use a slower/less-accurate
 * (more general) method of calculating the %age of CPU used by a process.
 */
#define	CCPU_SHIFT	11

/* Recompute process priorities, once a second. */
void
schedcpu(void)
{
	fixpt_t loadfac = loadfactor(averunnable[0]);
	struct proc *p;
	int s;
	unsigned int newcpu;

	wakeup((caddr_t)&lbolt);
	for (p = allproc; p != NULL; p = p->p_nxt) {
		/* Increment time and sleep time. Overflow is ignored. */
		p->p_time++;
		if (p->p_stat == SSLEEP || p->p_stat == SSTOP)
			p->p_slptime++;
#ifdef notyet
		/*
		 * If sleeping for 5 seconds and in loaded state,
		 * unmap all user address space, kernel stack and
		 * release all address translation state.
		 */
		if (p->p_slptime && (p->p_slptime % 5) && p->p_flag & SLOAD) {
			/* could process address space be deactivated? */
			if (vmspace_deactivate(p))
				p->p_flag &= ~SLOAD;
		}
#endif

		p->p_pctcpu = (p->p_pctcpu * ccpu) >> FSHIFT;
		/*
		 * If the process has slept the entire second,
		 * stop recalculating its priority until it wakes up.
		 */
		if (p->p_slptime > 1)
			continue;
		/*
		 * p_pctcpu is only for ps.
		 */
#if	(FSHIFT >= CCPU_SHIFT)
		p->p_pctcpu += (hz == 100)?
			((fixpt_t) p->p_cpticks) << (FSHIFT - CCPU_SHIFT):
                	100 * (((fixpt_t) p->p_cpticks)
				<< (FSHIFT - CCPU_SHIFT)) / hz;
#else
		p->p_pctcpu += ((FSCALE - ccpu) *
			(p->p_cpticks * FSCALE / hz)) >> FSHIFT;
#endif
		p->p_cpticks = 0;
		newcpu = (u_int) decay_cpu(loadfac, p->p_cpu) + p->p_nice;
		p->p_cpu = min(newcpu, UCHAR_MAX);
		setpri(p);
		s = splhigh();	/* prevent state changes */
		if (p->p_pri >= PUSER) {
			if ((p != curproc) &&
			    p->p_stat == SRUN &&
			    (p->p_flag & (SLOAD|SWEXIT)) == SLOAD &&
			    (p->p_pri / PPQ) != (p->p_usrpri / PPQ)) {
				remrq(p);
				p->p_pri = p->p_usrpri;
				setrq(p);
			} else
				p->p_pri = p->p_usrpri;
		}
		splx(s);
	}
	vmmeter();
	timeout((int (*)())schedcpu, (caddr_t)0, hz);
}

/*
 * Recalculate the priority of a process after it has slept for a while.
 * For all load averages >= 1 and max p_cpu of 255, sleeping for at least
 * six times the loadfactor will decay p_cpu to zero.
 */
void
updatepri(struct proc *p)
{
	unsigned int newcpu = p->p_cpu;
	fixpt_t loadfac = loadfactor(averunnable[0]);

	if (p->p_slptime > 5 * loadfac)
		p->p_cpu = 0;
	else {
		p->p_slptime--;	/* the first time was done in schedcpu */
		while (newcpu && --p->p_slptime)
			newcpu = (int) decay_cpu(loadfac, newcpu);
		p->p_cpu = min(newcpu, UCHAR_MAX);
	}
	setpri(p);
}

#define SQSIZE 0100	/* Must be power of 2 */
#define HASH(x)	(( (int) x >> 5) & (SQSIZE-1))
struct slpque {
	struct proc *sq_head;
	struct proc **sq_tailp;
} slpque[SQSIZE];

/*
 * During autoconfiguration or after a panic, a sleep will simply
 * lower the priority briefly to allow interrupts, then return.
 * The priority to be used (safepri) is machine-dependent, thus this
 * value is initialized and maintained in the machine-dependent layers.
 * This priority will typically be 0, or the lowest priority
 * that is safe for use on the interrupt stack; it can be made
 * higher to block network software interrupts after panics.
 */
int safepri;

/*
 * Timed sleep call.
 * Suspends current process until a wakeup is made on chan.
 * The process will then be made runnable with priority pri.
 * Sleeps at most timo/hz seconds (0 means no timeout).
 * If pri includes PCATCH flag, signals are checked
 * before and after sleeping, else signals are not checked.
 * Returns 0 if awakened, EWOULDBLOCK if the timeout expires.
 * If PCATCH is set and a signal needs to be delivered,
 * ERESTART is returned if the current system call should be restarted
 * if possible, and EINTR is returned if the system call should
 * be interrupted by the signal (return EINTR).
 */
int
tsleep(caddr_t chan, int pri, char *wmesg, int timo)
{
	struct proc *p = curproc;	/* XXX */
	struct slpque *qp;
	int s;
	int sig, catch = pri & PCATCH;
	extern int cold;
	static void endtsleep(struct proc *p);

	s = splhigh();
	if (cold || panicstr) {
		/*
		 * After a panic, or during autoconfiguration,
		 * just give interrupts a chance, then just return;
		 * don't run any other procs or panic below,
		 * in case this is the idle process and already asleep.
		 */
		splx(safepri);
		splx(s);
		return (0);
	}
#ifdef DIAGNOSTIC
	if (chan == 0 || p->p_stat != SRUN || p->p_rlink)
		panic("tsleep");
#endif
	p->p_wchan = chan;
	p->p_wmesg = wmesg;
	p->p_slptime = 0;
	p->p_pri = pri & PRIMASK;
	qp = &slpque[HASH(chan)];
	if (qp->sq_head == 0)
		qp->sq_head = p;
	else
		*qp->sq_tailp = p;
	*(qp->sq_tailp = &p->p_link) = 0;
	if (timo)
		timeout((int (*)())endtsleep, (caddr_t)p, timo);
	/*
	 * We put ourselves on the sleep queue and start our timeout
	 * before calling CURSIG, as we could stop there, and a wakeup
	 * or a SIGCONT (or both) could occur while we were stopped.
	 * A SIGCONT would cause us to be marked as SSLEEP
	 * without resuming us, thus we must be ready for sleep
	 * when CURSIG is called.  If the wakeup happens while we're
	 * stopped, p->p_wchan will be 0 upon return from CURSIG.
	 */
	if (catch) {
		p->p_flag |= SSINTR;
		if (sig = CURSIG(p)) {
			if (p->p_wchan)
				unsleep(p);
			p->p_stat = SRUN;
			goto resume;
		}
		if (p->p_wchan == 0) {
			catch = 0;
			goto resume;
		}
	}
	p->p_stat = SSLEEP;
	p->p_stats->p_ru.ru_nvcsw++;
	swtch();

	/* handy breakpoint location after process "wakes" */
#define	BP(s)	asm (".globl bp" # s " ; bp" # s ": ; ")
	BP(sleeprun);

resume:
	curpri = p->p_usrpri;
	splx(s);
	p->p_flag &= ~SSINTR;
	if (p->p_flag & STIMO) {
		p->p_flag &= ~STIMO;
		if (catch == 0 || sig == 0)
			return (EWOULDBLOCK);
	} else if (timo)
		untimeout((int (*)())endtsleep, (caddr_t)p);
	if (catch && (sig != 0 || (sig = CURSIG(p)))) {
		if (p->p_sigacts->ps_sigintr & sigmask(sig))
			return (EINTR);
		return (ERESTART);
	}
	return (0);
}

/*
 * Implement timeout for tsleep.
 * If process hasn't been awakened (wchan non-zero),
 * set timeout flag and undo the sleep.  If proc
 * is stopped, just unsleep so it will remain stopped.
 */
static void
endtsleep(struct proc *p)
{
	int s = splhigh();

	if (p->p_wchan) {
		if (p->p_stat == SSLEEP)
			setrun(p);
		else
			unsleep(p);
		p->p_flag |= STIMO;
	}
	splx(s);
}

/*
 * Remove a process from its wait queue
 */
void
unsleep(struct proc *p)
{
	struct slpque *qp;
	struct proc **hp;
	int s;

	s = splhigh();
	if (p->p_wchan) {
		hp = &(qp = &slpque[HASH(p->p_wchan)])->sq_head;
		while (*hp != p)
			hp = &(*hp)->p_link;
		*hp = p->p_link;
		if (qp->sq_tailp == &p->p_link)
			qp->sq_tailp = hp;
		p->p_wchan = 0;
	}
	splx(s);
}

/*
 * Wakeup on "chan"; set all processes
 * sleeping on chan to run state.
 */
void
wakeup(caddr_t chan)
{
	struct slpque *qp;
	struct proc *p, **q;
	int s;

	s = splhigh();
	qp = &slpque[HASH(chan)];
restart:
	for (q = &qp->sq_head; p = *q; ) {
#ifdef DIAGNOSTIC
		if (p->p_rlink || p->p_stat != SSLEEP && p->p_stat != SSTOP)
			panic("wakeup");
#endif
		if (p->p_wchan == chan) {
			p->p_wchan = 0;
			*q = p->p_link;
			if (qp->sq_tailp == &p->p_link)
				qp->sq_tailp = q;
			if (p->p_stat == SSLEEP) {
				/* OPTIMIZED INLINE EXPANSION OF setrun(p) */
				if (p->p_slptime > 1)
					updatepri(p);
				p->p_slptime = 0;
				p->p_stat = SRUN;
				if (p->p_flag & SLOAD)
					setrq(p);
				/*
				 * Since curpri is a usrpri,
				 * p->p_pri is always better than curpri.
				 */
				/* if ((p->p_flag&SLOAD) == 0)
					vmspace_reactivate(p);
				else */
					need_resched();
				/* END INLINE EXPANSION */
				goto restart;
			}
		} else
			q = &p->p_link;
	}
	splx(s);
}

/*
 * Initialize the (doubly-linked) run queues
 * to be empty.
 */
void
rqinit(void)
{
	int i;

	for (i = 0; i < NQS; i++)
		qs[i].ph_link = qs[i].ph_rlink = (struct proc *)&qs[i];
}

/*
 * Change process state to be runnable,
 * placing it on the run queue if it is in memory,
 * and awakening the swapper if it isn't in memory.
 */
void
setrun(struct proc *p)
{
	int s;

	s = splclock();
	switch (p->p_stat) {

	case 0:
	case SWAIT:
	case SRUN:
	case SZOMB:
	default:
		panic("setrun");

	case SSTOP:
	case SSLEEP:
		unsleep(p);		/* e.g. when sending signals */
		break;

	case SIDL:
		break;
	}
	p->p_stat = SRUN;
	if (p->p_flag & SLOAD)
		setrq(p);
	splx(s);
	if (p->p_slptime > 1)
		updatepri(p);
	p->p_slptime = 0;
	/* if ((p->p_flag&SLOAD) == 0)
		vmspace_reactivate(p);
	else  */ if (p->p_pri < curpri)
		need_resched();
}

/*
 * Compute priority of process when running in user mode.
 * Arrange to reschedule if the resulting priority
 * is better than that of the current process.
 */
void
setpri(struct proc *p)
{
	unsigned int newpri;

	newpri = PUSER + p->p_cpu / 4 + 2 * p->p_nice;
	newpri = min(newpri, MAXPRI);
	p->p_usrpri = newpri;
	if (newpri < curpri)
		need_resched();
}

#ifdef DDB
#define	DDBFUNC(s)	ddb_##s
DDBFUNC(ps) () {
	int np;
	struct proc *ap, *p, *pp;
	np = nprocs;
	p = ap = allproc;
    printf("  pid  proc    addr     uid  ppid  pgrp  flag stat comm        wchan\n");
    while (--np >= 0) {
	pp = p->p_pptr;
	if (pp == 0)
		pp = p;
	if (p->p_stat) {
	    printf((curproc == p) ? "*" : " ");
	    printf("%4d %06x %06x %3d %5d %5d  %06x  %d  %s   ",
		   p->p_pid, ap, p->p_addr, p->p_cred->p_ruid, pp->p_pid, 
		   p->p_pgrp->pg_id, p->p_flag, p->p_stat,
		   p->p_comm);
	    if (p->p_wchan) {
		if (p->p_wmesg)
		    printf("%s ", p->p_wmesg);
		printf("%x", p->p_wchan);
	    }
	    printf("\n");
	}
	ap = p->p_nxt;
	if (ap == 0 && np > 0)
		ap = zombproc;
	p = ap;
    }
}
#endif
