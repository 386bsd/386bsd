/*
 * Copyright (c) 1982, 1986, 1989, 1991 Regents of the University of California.
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
 * $Id: exit.c,v 1.1 94/10/20 00:02:52 bill Exp $
 */

#include "sys/param.h"
#include "sys/wait.h"
#include "sys/errno.h"
#include "kernel.h"
#include "proc.h"
#include "malloc.h"
#include "signalvar.h"
#include "resourcevar.h"

#include "machine/cpu.h"

#include "prototypes.h"

/*
 * POSIX exit() function system call handler. Force process to exit,
 * returning argument as termination status to parent.
 */
int
rexit(p, uap, retval)
	struct proc *p;
	struct args {
		int	rval;
	} *uap;
	int *retval;
{

	exit(p, W_EXITCODE(uap->rval, 0));
}

static void reclaimproc(struct proc *p);

/*
 * Terminate process, reclaiming process resources and leaving
 * unschedulable zombie process to hold process statistics and
 * termination status, awaiting notification of next of kin(e.g.
 * wait4() by parent). Deal pragmatically with child processes
 * that are being orphaned. Never returns.
 */
void volatile
exit(struct proc *p, int rv)
{
	struct proc *q, *nq;

#ifdef	DIAGNOSTIC
	if (p->p_pid == 1)
		panic("init died");
#endif

	/*
	 * First step: insure that process won't be interfered
	 * with as it is incrementally disassembled.
	 */

	/* cancel trace if present, and indicate that process is exitting */
	p->p_flag &= ~STRC;
	p->p_flag |= SWEXIT | SSYS;

	/* cancel any pending signals, ignore all of them */
	p->p_sigignore = ~0;
	p->p_sig = 0;

	/* clear interval timer (if present) */
	untimeout((int (*)())realitexpire, (caddr_t)p);

	/*
	 * Second step: jettision application interface related state
	 * while the process is still operational (can block and hold
	 * resources).
	 */

	/* close open files and release open-file table. */
	if (p->p_fd)
		fdfree(p);

	/* process is starting to leave the process group */
	if (p->p_pgrp) {
		/* if a POSIX session leader process, abandon session. */
		if (p->p_session && SESS_LEADER(p))
			exitsessleader(p);
		fixjobc(p, p->p_pgrp, 0);
	}

#ifdef KTRACE
	/* if process was being ktraced, detach reference to trace file */
	if (p->p_tracep)
		ktrace_exit(p);
#endif

	/* if process is being accounted for, account for exit
	if (p->p_acflag)
		acct_exit(p, rv); */

	/* release user address space, return process to kernel space only */
	if (p->p_vmspace) {
		vmspace_free(p->p_vmspace);
		p->p_vmspace = 0;
	}

	/*
	 * Third step: Process only has the kernel concurrent object
	 * (proc, pcb and kernel stack) and the resources (memory, pid)
	 * to be released (process cannot block now). N.B. any process
	 * state referenced by utilities like ps must hang around as
	 * long as the zombie is unclaimed (like credentials and stats).
	 */

	/* make process invisable and unschedulable */
	curproc = NULL;
	leavepidhash(p);
	if (*p->p_prev = p->p_nxt)
		p->p_nxt->p_prev = p->p_prev;

	/* check if process limits are unreferenced and need to be reclaimed */
	if (p->p_limit && --p->p_limit->p_refcnt == 0)
		FREE(p->p_limit, M_SUBPROC);

	/* statistics to deliver? */
	if (p->p_stats) {
		/* record exit status in zombie process */
		p->p_stats->p_status = rv;

		/* make process a zombie */
		if (p->p_nxt = zombproc)
			p->p_nxt->p_prev = &p->p_nxt;
		p->p_prev = &zombproc;
		zombproc = p;
		p->p_stat = SZOMB;
		p->p_pptr->p_flag |= SZCHILD;
	}

	/* if process terminating with zombie children, let init find out */
	if ((p->p_flag & SZCHILD) != 0 && p->p_cptr) {
		wakeup((caddr_t) initproc);
		p->p_flag &= ~SZCHILD;
	}

	/* scan queue of child processes we are orphaning */
	for (q = p->p_cptr; q != NULL; q = nq) {
		nq = q->p_osptr;
		if (nq != NULL)
			nq->p_ysptr = NULL;

		/* init inherits them as additional children */
		if (initproc->p_cptr)
			initproc->p_cptr->p_ysptr = q;
		q->p_osptr = initproc->p_cptr;
		q->p_ysptr = NULL;
		initproc->p_cptr = q;
		q->p_pptr = initproc;

		/* kill off any traced processes unconditionally */
		if (q->p_flag&STRC) {
			q->p_flag &= ~STRC;
			psignal(q, SIGKILL);
		}
	}
	p->p_cptr = NULL;

	/*
	 * Fourth step: alert POSIX parent process of new corpse, or
	 * if not a POSIX process, reclaim the process slot immediately.
	 * Then ask machine-dependant layer to terminate this running thread.
	 */

	/* if stats, signal parent that there is a child to be reclaimed. */
	if (p->p_stats) {
		p->p_flag &= ~SPPWAIT;
		psignal(p->p_pptr, SIGCHLD);
		wakeup((caddr_t)p->p_pptr);
	} else
		reclaimproc(p);

	/* terminate this kernel thread, reclaiming concurrent object state */
	cpu_texit(p);
}

/*
 * Poll child processes to see if any have exited, or stopped
 * under trace, or (optionally) stopped by a signal. Pass back
 * exit or signal status and deallocate any zombie child process
 * state left over to hold information for potential use by wait4().
 */
int
wait4(q, uap, retval)
	struct proc *q;
	struct args {
		int	pid;
		int	*status;
		int	options;
		struct	rusage *rusage;
	} *uap;
	int retval[];
{
	struct proc *p;
	struct rusage ru;
	int nfound, status, error, pid = uap->pid;

	/* look for our process group id number? */
	if (pid == 0)
		pid = -q->p_pgid;

	/* legal option for POSIX wait4()? */
	if (uap->options &~ (WUNTRACED|WNOHANG))
		return (EINVAL);

	/* walk list of child processes, following "oldest sibling" */
loop:
	nfound = 0;
	for (p = q->p_cptr; p; p = p->p_osptr) {

		/* if the child doesn't match pattern skip it, or count it */
		if (pid != WAIT_ANY && p->p_pid != pid && p->p_pgid != -pid)
			continue;

		/* if "dead" process, get termination state and reclaim slot */
		if (p->p_stat == SZOMB) {
			retval[0] = p->p_pid;

			/* update and sum current and prior statistics */
			ru = p->p_stats->p_ru;
			ru.ru_stime = p->p_stime;
			ru.ru_utime = p->p_utime;
			ruadd(&ru, &p->p_stats->p_cru);

			/* if status return vector, return exit status */
			if (uap->status && (error = copyout(q,
			    (caddr_t)&p->p_stats->p_status,
			    (caddr_t) uap->status, sizeof(status))))
				return (error);

			/* if resource usage vector, return it */
			if (uap->rusage && (error = copyout(q, (caddr_t)&ru,
			    (caddr_t)uap->rusage, sizeof (struct rusage))))
				return (error);

			/* accumulate child into total child stats and free */
			ruadd(&q->p_stats->p_cru, &ru);
			FREE(p->p_stats, M_ZOMBIE);

			/* reclaim this process, unlinking from zombproc */
			if (*p->p_prev = p->p_nxt)
				p->p_nxt->p_prev = p->p_prev;
			reclaimproc(p);
			return (0);
		}

		/* is it a stopped or traced process, not yet waited for? */
		if (p->p_stat == SSTOP && (p->p_flag & SWTED) == 0 &&
		    (p->p_flag & STRC || uap->options & WUNTRACED)) {

			/* save stop status indication */
			p->p_stats->p_status =
				W_STOPCODE(p->p_sigacts->ps_stopsig);

			/* if status return vector, return stop status */
			if (uap->status && (error = copyout(q,
			    (caddr_t)&p->p_stats->p_status,
			    (caddr_t)uap->status, sizeof(status))))
				return (error);

			/* if resource usage vector, return it */
			if (uap->rusage) {
				ru = p->p_stats->p_ru;
				ru.ru_stime = p->p_stime;
				ru.ru_utime = p->p_utime;
				ruadd(&ru, &p->p_stats->p_cru);

				if (error = copyout(q, (caddr_t)&ru,
			    	    (caddr_t)uap->rusage,
				    sizeof (struct rusage)))
					return (error);
			}

			/* return id and mark it as having been waited for */
			p->p_flag |= SWTED;
			retval[0] = p->p_pid;
			return (0);
		}
		nfound++;
	}

	/* wait with no children to wait for */
	if (nfound == 0)
		return (ECHILD);

	/* return from poll with no child processes stopped or exitted */
	if (uap->options & WNOHANG) {
		retval[0] = 0;
		return (0);
	}

	/* wait for a child process to exit, signal or wakeup this parent */
	if (error = tsleep((caddr_t)q, PWAIT | PCATCH, "wait", 0))
		return (error);
	goto loop;
}

/* reclaim process entry */
static void
reclaimproc(struct proc *p)
{
	struct proc *q;

	/* reclaim outstanding process subsidery structures */
	if (--p->p_cred->p_refcnt == 0) {
		crfree(p->p_cred->pc_ucred);
		FREE(p->p_cred, M_SUBPROC);
	}

	/* process has finally left the process group */
	leavepgrp(p);

	/* unlink process from its related queues */
	if (q = p->p_ysptr)
		q->p_osptr = p->p_osptr;
	if (q = p->p_osptr)
		q->p_ysptr = p->p_ysptr;
	if ((q = p->p_pptr)->p_cptr == p)
		q->p_cptr = p->p_osptr;

	/* free process entry */
	FREE(p, M_PROC);
	nprocs--;
}
