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
 * $Id: fork.c,v 1.1 94/10/20 00:02:54 bill Exp $
 */

#include "sys/param.h"
#include "sys/user.h"	/* only for signals */
#include "privilege.h"
#include "uio.h"
#include "filedesc.h"
#include "resourcevar.h"
#include "kernel.h"
#include "malloc.h"
#include "vnode.h"

#include "prototypes.h"


fork(p, uap, retval)
	struct proc *p;
	void *uap;
	int retval[];
{

	return (fork1(p, 0, retval));
}

vfork(p, uap, retval)
	struct proc *p;
	void *uap;
	int retval[];
{

	return (fork1(p, 1, retval));
}

int
fork1(struct proc *p1, int isvfork, int *retval)
{
	struct proc *p2;
	struct pstats *ps;
	int mypid, haspriv = 0;
	volatile static int nextpid, pidchecked = 0;

	/*
	 * Enforce per-user and global limits on the number of processes.
	 */

	/* if too many processes, or last one and not privileged, pass error */
redo:

	if (nprocs >= maxproc ||
	    (haspriv = use_priv(p1->p_ucred, PRV_NO_UPROCLIMIT, p1)) &&
	    nprocs >= maxproc + 1) {
		tablefull("proc");
		return (EAGAIN);
	}

	/* find number of processes already allocated to this user id */
	if (haspriv == 0) {
		uid_t uid = p1->p_ucred->cr_uid;
		int count = 0;

		for (p2 = allproc; p2; p2 = p2->p_nxt)
			if (p2->p_ucred->cr_uid == uid)
				count++;
		for (p2 = zombproc; p2; p2 = p2->p_nxt)
			if (p2->p_ucred->cr_uid == uid)
				count++;

		/* if too many processes for process limit, return error */
		if (count > p1->p_rlimit[RLIMIT_NPROC].rlim_cur)
			return (EAGAIN);
	}

	/*
	 * Assign an unused process ID. A run of pids from nextpid+1
	 * through pidchecked-1 are unused and may be allocated immediately.
	 */
	nextpid++;
retry:
	/* process id wrapped around */
	if (nextpid >= PID_MAX) {
		/* restart assuming low numbered pids are immortal */
		nextpid = 100;
		pidchecked = 0;
	}

	/* no more unused pids, scan active & zombie queues for another run */
	if (nextpid >= pidchecked) {
		int doingzomb = 0;

		/* scan active process queue to find if pid is used. */
		pidchecked = PID_MAX;
		p2 = allproc;
again:
		for (; p2 != NULL; p2 = p2->p_nxt) {
			pid_t pid = p2->p_pid, pgid = p2->p_pgrp->pg_id;

			if (pid == nextpid || pgid == nextpid) {
				nextpid++;
				if (nextpid >= pidchecked)
					goto retry;
			}

			/* find first pid above the next run of unused pids */
			if (pid > nextpid && pidchecked > pid)
				pidchecked = pid;
			if (pgid > nextpid &&  pidchecked > pgid)
				pidchecked = pgid;
		}
		if (!doingzomb) {
			/* scan zombie process queue to find if pid is used. */
			doingzomb = 1;
			p2 = zombproc;
			goto again;
		}
	}

	/*
	 * Construct a new embryonic process entry.
	 */

	/* allocate a new proc entry */
	mypid = nextpid;
	MALLOC(p2, struct proc *, sizeof(struct proc), M_PROC, M_WAITOK);
	if (mypid != nextpid) {
		FREE(p2, M_PROC);
		goto redo;
	}

	/* process marked under construction until fully assembled */
	p2->p_stat = SIDL;
	p2->p_flag = SSYS;
	p2->p_pid = mypid;

	/* assemble minimal process state, from parent or from 0 */
	p1->p_cred->p_refcnt++;
	(void) memset(&p2->p_startzero, 0,
	    (unsigned) ((caddr_t)&p2->p_endzero - (caddr_t)&p2->p_startzero));
	(void) memcpy(&p2->p_startcopy, &p1->p_startcopy,
	    (unsigned) ((caddr_t)&p2->p_endcopy - (caddr_t)&p2->p_startcopy));

	/* process exists, make it visable to kernel to reserve PID */
	p2->p_nxt = allproc;
	p2->p_nxt->p_prev = &p2->p_nxt;		/* allproc is never NULL */
	p2->p_prev = &allproc;
	allproc = p2;
	nprocs++;

	/* child: pass parent pid & child(1) indication in this kernel stack */
	retval[0] = p1->p_pid;
	retval[1] = 1;
	if (cpu_tfork(p1, p2)) {
		/* record start time and begin child process context */
		p2->p_stats->p_start = time;
		return (0);
	}

	/* link the process onto its queues and process groups */
	p1->p_pgrpnxt = p2;
	p2->p_pptr = p1;
	p2->p_osptr = p1->p_cptr;
	if (p1->p_cptr)
		p1->p_cptr->p_ysptr = p2;
	p1->p_cptr = p2;

	/*
	 * Complete process replication by amending the substructures of
	 * the proc entry, mediating access to shared objects.
	 */

	/* replicate any open file descriptors in new process entry */
	p2->p_fd = fdcopy(p1);

	/* was sharing modification of limits and are no longer, make a copy */
	/* if (p1->p_limit->p_lflags & PL_SHAREMOD)
		p2->p_limit = limcopy(p1->p_limit);
	else { */
		/* otherwise, share same limits */
		p2->p_limit = p1->p_limit;
		p2->p_limit->p_refcnt++;
	
		/* if creating a shared thread, mark shared modification */
		/* if (uthread)
			p2->p_limit->p_lflags |= PL_SHAREMOD;
	} */

	/* replicate statistics */
	MALLOC(ps, struct pstats *, sizeof(struct pstats), M_ZOMBIE, M_WAITOK);
	memset(&ps->pstat_startzero, 0, (unsigned)
	    ((caddr_t)&ps->pstat_endzero - (caddr_t)&ps->pstat_startzero));
	(void)memcpy(&ps->pstat_startcopy, &p1->p_stats->pstat_startcopy,
	    ((caddr_t)&ps->pstat_endcopy - (caddr_t)&ps->pstat_startcopy));
	p2->p_stats = ps;

	/* replicate address space (put vfork() pass address to child here) */
	p2->p_vmspace = vmspace_fork(p1->p_vmspace, p2);

#ifdef KTRACE
	/* replicate ktrace if implemented */
	if (p1->p_traceflag)
		ktrace_fork(p1, p2);
#endif

	/* position signal actions in kernel stack and copy them */
	p2->p_sigacts = &p2->p_addr->u_sigacts;
	*p2->p_sigacts = *p1->p_sigacts;

	/* set process flags. */
	p2->p_flag = SLOAD;
	if (p1->p_session->s_ttyvp != NULL && p1->p_flag & SCTTY)
		p2->p_flag |= SCTTY;
	if (isvfork)
		p2->p_flag |= SPPWAIT;

	/*
	 * Process is completely copied. Enable its context for
	 * execution in the kernel, and return to parent process.
	 */

	/* make process pfind()able in the kernel */
	enterpidhash(p2);

	/* make child runnable and add to run queue. */
	(void) splclock();
	p2->p_stat = SRUN;
	p2->p_rlink = 0;
	setrq(p2);
	(void) splnone();

	/* if simulating vfork(), wait for child to release parent */
	if (isvfork) {
		while (p2->p_flag & SPPWAIT)
			(void) tsleep((caddr_t)p1, PWAIT, "ppwait", 0);
		/* put address space recovery from child here */
	}

	/* parent: indicate child pid and that this is the parent(0) */
	retval[0] = p2->p_pid;
	retval[1] = 0;
	return (0);
}
