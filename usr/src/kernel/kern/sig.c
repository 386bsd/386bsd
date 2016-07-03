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
 *	$Id: sig.c,v 1.1 94/10/20 00:03:14 bill Exp $
 */

#define	SIGPROP		/* include signal properties table */
#include "sys/param.h"
#include "sys/user.h"		/* for coredump */
#include "sys/file.h"
#include "sys/wait.h"
#include "sys/mman.h"
#include "resourcevar.h"
#include "signalvar.h"
#include "uio.h"
#include "kernel.h"
#ifdef	KTRACE
#include "sys/ktrace.h"
#endif /* KTRACE */

#include "sys/kinfo_proc.h"

#include "machine/cpu.h"

#include "namei.h"
#include "vnode.h"

#include "prototypes.h"

/* forward declarations of private functions as well as inline functions */

static void stop(struct proc *p, int swtchit, int sig);
static void volatile sigexit(struct proc *p, int sig);
static int killpg(struct proc *cp, int signo, int pgid, int all);

/*
 * Can process p send the signal signo to process q?
 */
extern inline
int cansignal(struct proc *p, struct proc *q, int signo)
{
	struct pcred *pc = p->p_cred, *qc = q->p_cred;

	if (pc->pc_ucred->cr_uid == 0 ||
	    pc->p_ruid == qc->p_ruid ||
	    pc->pc_ucred->cr_uid == qc->p_ruid ||
	    pc->p_ruid == q->p_ucred->cr_uid ||
	    pc->pc_ucred->cr_uid == q->p_ucred->cr_uid ||
	    (signo == SIGCONT && q->p_session == p->p_session))
		return 1;
	else
		return 0;
}

/* system call interfaces to the signal facility. */

/*
 * POSIX sigaction() signal interface.
 */
int
sigaction(p, uap, retval)
	struct proc *p;
	struct args {
		int	signo;
		struct	sigaction *nsa;
		struct	sigaction *osa;
	} *uap;
	int *retval;
{
	struct sigacts *ps = p->p_sigacts;
	struct sigaction vec;
	struct sigaction *sa = &vec;
	int sig = uap->signo, bit;

	/* have a valid signal to manipulate action of? */
	if (sig <= 0 || sig >= NSIG)
		return (EINVAL);
	bit = sigmask(sig);

	/* return the "current" signal's state? */
	if (uap->osa) {
		sa->sa_handler = ps->ps_sigact[sig];
		sa->sa_mask = ps->ps_catchmask[sig];

		/* assemble flags */
		sa->sa_flags = 0;
		if ((ps->ps_sigonstack & bit) != 0)
			sa->sa_flags |= SA_ONSTACK;
		if ((ps->ps_sigintr & bit) == 0)
			sa->sa_flags |= SA_RESTART;
		if (p->p_flag & SNOCLDSTOP)
			sa->sa_flags |= SA_NOCLDSTOP;

		if (copyout(p, (caddr_t)sa, (caddr_t)uap->osa, sizeof (vec)))
			return (EFAULT);
	}

	/* install a new state for the signal? */
	if (uap->nsa) {
		sig_t act;

		/* valid signal number? */
		if (bit & sigcantmask)
			return (EINVAL);

		/* can we obtain the action from the process? */
		if (copyin(p, (caddr_t)uap->nsa, (caddr_t)sa, sizeof (vec)))
			return (EFAULT);

		/* signal handler valid? */
		act = sa->sa_handler;
		if (act != SIG_DFL && act != SIG_IGN && act != BADSIG
	    	    && vmspace_access(p->p_vmspace, (caddr_t)act, sizeof(int), PROT_READ) == 0)
			return (EFAULT);

		/* install the signal atomically */
		(void) splhigh();
		ps->ps_sigact[sig] = act;
		ps->ps_catchmask[sig] = sa->sa_mask &~ sigcantmask;

		/* decode signal action flag into processes signal state */
		if ((sa->sa_flags & SA_RESTART) == 0)
			ps->ps_sigintr |= bit;
		else
			ps->ps_sigintr &= ~bit;
		if (sa->sa_flags & SA_ONSTACK)
			ps->ps_sigonstack |= bit;
		else
			ps->ps_sigonstack &= ~bit;

		/* deal with special case of SIGCHLD */
		if (sig == SIGCHLD) {
			if (sa->sa_flags & SA_NOCLDSTOP)
				p->p_flag |= SNOCLDSTOP;
			else
				p->p_flag &= ~SNOCLDSTOP;
		}

		/* mark signals that will be ignored or caught */
		if (act == SIG_IGN ||
		    (sigprop[sig] & SA_IGNORE && act == SIG_DFL)) {
			p->p_sig &= ~bit;

			/* don't ignore SIGCONT, allow psignal to un-stop it */
			if (sig != SIGCONT)
				p->p_sigignore |= bit;
			p->p_sigcatch &= ~bit;
		} else {
			p->p_sigignore &= ~bit;
			if (act == SIG_DFL)
				p->p_sigcatch &= ~bit;
			else
				p->p_sigcatch |= bit;
		}
		(void) splnone();
	}

	return (0);
}


/*
 * Interface to access the process's signal mask. Used to
 * implement the POSIX sigprocmask() function, which is
 * completely implemented in the user program library that
 * makes use of this system call.
 */
int
sigprocmask(p, uap, retval)
	struct proc *p;
	struct args {
		int	how;
		sigset_t mask;
	} *uap;
	int *retval;
{
	/* return the current process signal mask before modification */
	*retval = p->p_sigmask;

	/* modify the mask as directed */
	switch (uap->how) {
	case SIG_BLOCK:
		p->p_sigmask |= uap->mask &~ sigcantmask;
		break;

	case SIG_UNBLOCK:
		p->p_sigmask &= ~uap->mask;
		break;

	case SIG_SETMASK:
		p->p_sigmask = uap->mask &~ sigcantmask;
		break;
	
	default:
		return(EINVAL);
		break;
	}

	return (0);
}

/*
 * Interface to obtain the process's pending signal set. Used to
 * implement the POSIX sigpending() function, which is
 * completely implemented in the user program library that
 * makes use of this system call.
 */
int
sigpending(p, uap, retval)
	struct proc *p;
	void *uap;
	int *retval;
{
	/* return the set of pending signals */
	*retval = p->p_sig;
	return (0);
}

/*
 * Interface to suspend a process pending the arrival of a signal.
 * Prior to suspension, the signal mask is set to the value of the
 * argument for the time suspended, reverting to its prior value
 * when the signal arrives. 
 */
int
sigsuspend(p, uap, retval)
	struct proc *p;
	struct args {
		sigset_t mask;
	} *uap;
	int *retval;
{
	struct sigacts *ps = p->p_sigacts;

	/* record mask in oldmask, and have it restored instead after signal */
	ps->ps_oldmask = p->p_sigmask;
	ps->ps_flags |= SA_OLDMASK;

	/* mask signals during the suspension */
	p->p_sigmask = uap->mask &~ sigcantmask;

	/* pause. if woken for any reason other than a signal, pause again. */
 	while (tsleep((caddr_t) ps, PPAUSE|PCATCH, "pause", 0) == 0)
		;

	return (EINTR);
}

/*
 * BSD interface to select a seperate stack to handle signal
 * transactions on instead of the process stack. This is an
 * extension of the POSIX signal implementation.
 */
int
sigstack(p, uap, retval)
	struct proc *p;
	struct args {
		struct	sigstack *nss;
		struct	sigstack *oss;
	} *uap;
	int *retval;
{
	struct sigstack ss;
	int error = 0;

	/* return signal stack feature state if requested */
	if (uap->oss && (error = copyout(p, (caddr_t)&p->p_sigacts->ps_sigstack,
	    (caddr_t)uap->oss, sizeof (struct sigstack))))
		return (error);

	/* install signal stack feature state if requested */
	if (uap->nss && (error = copyin(p, (caddr_t)uap->nss, (caddr_t)&ss,
	    sizeof (ss))) == 0)
		p->p_sigacts->ps_sigstack = ss;

	return (error);
}

/*
 * POSIX kill() interface, used to generate signals to a process or
 * set of processes.
 */
int
kill(cp, uap, retval)
	struct proc *cp;
	struct args {
		int	pid;
		int	signo;
	} *uap;
	void *retval;
{
	struct proc *p;

	/* bounds check the signal to be sent */
	if ((unsigned) uap->signo >= NSIG)
		return (EINVAL);

	/* signal a single process? ... */
	if (uap->pid > 0) {

		/* locate the process */
		p = pfind(uap->pid);
		if (p == 0)
			return (ESRCH);

		/* do we have permission to send the signal? */
		if (!cansignal(cp, p, uap->signo))
			return (EPERM);

		/* do we have a signal to send? */
		if (uap->signo)
			psignal(p, uap->signo);

		return (0);
	}

	/* ... or to a set of processes? */
	switch (uap->pid) {
	case -1:		/* broadcast signal */
		return (killpg(cp, uap->signo, 0, 1));
	case 0:			/* signal own process group */
		return (killpg(cp, uap->signo, 0, 0));
	default:		/* negative explicit process group */
		return (killpg(cp, uap->signo, -uap->pid, 0));
	}
}

/*
 * Common code for kill process group/broadcast kill.
 * cp is calling process.
 */
static int
killpg(struct proc *cp, int signo, int pgid, int all)
{
	struct proc *p;
	struct pgrp *pgrp;
	int nfound = 0;
	
	/* send to all, or just a process group */
	if (all)	
		/* 
		 * broadcast - walk the list of all processes,
		 * selecting candidates not system processes, init, or
		 * self.
		 */
		for (p = allproc; p != NULL; p = p->p_nxt) {
			if (p->p_pid == 1 || p->p_flag & SSYS || 
			    p == cp || !cansignal(cp, p, signo))
				continue;
			nfound++;
			if (signo)
				psignal(p, signo);
		}
	else {

		/* zero pgid means send to my process group. */
		if (pgid == 0)		
			pgrp = cp->p_pgrp;
		else {
			pgrp = pgfind(pgid);
			if (pgrp == NULL)
				return (ESRCH);
		}

		/* walk the process group list looking for candidates */
		for (p = pgrp->pg_mem; p != NULL; p = p->p_pgrpnxt) {
			if (p->p_pid == 1 || p->p_flag & SSYS ||
			    p->p_stat == SZOMB || !cansignal(cp, p, signo))
				continue;
			nfound++;
			if (signo)
				psignal(p, signo);
		}
	}

	return (nfound ? 0 : ESRCH);
}

/*
 * This is a hidden system call, an artifact of the signal
 * implementation that is used to cleanup state after a signal
 * has been taken.  It accesses a machine-dependant function that
 * resets the signal mask and stack state from context left on the stack
 * by delivering a signal.
 */
int
sigreturn(p, uap, retval)
	struct proc *p;
	void *uap;
	int *retval;
{
	return (cpu_signalreturn(p));
}




/*
 * Nonexistent system call-- signal process (may want to handle it).
 * Flag error in case process won't see signal immediately (blocked or ignored).
 */
int
nosys(p, args, retval)
	struct proc *p;
	void *args;
	int *retval;
{

	psignal(p, SIGSYS);
	return (ENOSYS);
}

/* POSIX signal mechanism implementation */

/*
 * Send the specified signal to the specified process.
 * If the signal has an action, the action is usually performed
 * by the target process rather than the caller; we simply add
 * the signal to the set of pending signals for the process.
 * Exceptions:
 *   o When a stop signal is sent to a sleeping process that takes the default
 *     action, the process is stopped without awakening it.
 *   o SIGCONT restarts stopped processes (or puts them back to sleep)
 *     regardless of the signal action (eg, blocked or ignored).
 * Other ignored signals are discarded immediately.
 */
void
psignal(struct proc *p, int sig)
{
	int s, prop = sigprop[sig], bit = sigmask(sig);
	sig_t action;

#ifdef	DIAGNOSTIC
	if ((unsigned)sig >= NSIG || sig == 0)
		panic("psignal sig");
#endif

	/*
	 * Traced process signals are sequenced through the default case
	 * so that the signal will be noticed first by the parent process
	 * (debugger) before its action is activated in the context of
	 * the child process.
	 */
	if (p->p_flag & STRC)
		action = SIG_DFL;
	else {
		/* abandon ignored signals, except for SIGCONT case */
		if (p->p_sigignore & bit)
			return;

		/* hold blocked signals, catch ones with handlers */
		if (p->p_sigmask & bit)
			action = SIG_HOLD;
		else if (p->p_sigcatch & bit)
			action = SIG_CATCH;
		else
			action = SIG_DFL;	/* SIGCONT only */
	}

	/* if process is "niced" and terminating, promote it's priority */
	if (p->p_nice > NZERO && (sig == SIGKILL ||
	    sig == SIGTERM && !(p->p_flag&STRC || action != SIG_DFL)))
		p->p_nice = NZERO;

	/* if continue property, remove any stop signals pending */
	if (prop & SA_CONT)
		p->p_sig &= ~stopsigmask;

	/* if stop property, clear any pending continue signals */
	if (prop & SA_STOP) {
		/* terminal stops don't affect orphaned process groups */
		if (prop & SA_TTYSTOP && p->p_pgrp->pg_jobc == 0 &&
		    action == SIG_DFL)
		        return;
		p->p_sig &= ~contsigmask;
	}

	/* post pending signal */
	p->p_sig |= bit;

	/*
	 * Defer further processing for signals which are held,
	 * except that stopped processes must be continued by SIGCONT.
	 */
	if (action == SIG_HOLD && ((prop & SA_CONT) == 0 || p->p_stat != SSTOP))
		return;

	/* alter process state in reaction to processing the signal */
	s = splhigh();
	switch (p->p_stat) {

	case SSLEEP:
		/* process not interruptable, wait till return to user */
		if ((p->p_flag & SSINTR) == 0)
			goto out;

		/* process is traced, run it so it can notice signal */
		if (p->p_flag&STRC)
			goto run;

		/* process is being default continued, just clear pending */
 		if ((prop & SA_CONT) && action == SIG_DFL) {
 			p->p_sig &= ~bit;
 			goto out;
 		}

		/* stop default action sleeping processes here, otherwise run */ 
		if ((prop & SA_STOP) != 0 && action == SIG_DFL) {
			p->p_sig &= ~bit;
			stop(p, 0, sig);
			goto out;
		} else
			goto runfast;

	case SSTOP:
		/* kill signal always sets processes running */
		if (sig == SIGKILL)
			goto runfast;

		/* signal a traced process: no action needed */
		if (p->p_flag&STRC)
			goto out;

		/* continue a stopped process? */
		if (prop & SA_CONT) {
			/* if default or ignored, cancel pending */ 
			if (action == SIG_DFL)
				p->p_sig &= ~bit;
			/* otherwise, handle it in the context of the process */
			if (action == SIG_CATCH)
				goto runfast;
			/* if it was running before, run it again */
			if (p->p_wchan == 0)
				goto run;
			/* otherwise, return it to sleeping state */
			p->p_stat = SSLEEP;
			goto out;
		}

		/* stop a stopped process? if so, cancel pending signal */
		if (prop & SA_STOP) {
			p->p_sig &= ~bit;
			goto out;
		}

		/* process was sleeping interruptable before, make it again */
		if (p->p_wchan && p->p_flag & SSINTR)
			unsleep(p);
		goto out;

	default:
		/* force signal evaluation */
		cpu_signotify(p);
		goto out;
	}

runfast:
	/* give priority to deliver a signal inside a process */
	if (p->p_pri > PUSER)
		p->p_pri = PUSER;
run:
	setrun(p);
out:
	splx(s);
}

/*
 * If the current process has a signal to process (should be caught
 * or cause termination, should interrupt current syscall),
 * return the signal number.  Stop signals with default action
 * are processed immediately, then cleared; they aren't returned.
 * This is checked after each entry to the system for a syscall
 * or trap (though this can usually be done without actually calling
 * issig by checking the pending signal masks in the CURSIG macro.)
 * The normal call sequence is
 *
 *	while (sig = CURSIG(curproc)) {
 *		x = splhigh();
 *		psig(sig);
 *		splx(x);
 *	}
 */
int
issig(struct proc *p)
{
	int sig, mask, bit, prop;

	for (;;) {
		mask = p->p_sig &~ p->p_sigmask;
		if (p->p_flag & SPPWAIT)
			mask &= ~stopsigmask;
		if (mask == 0)	 	/* no signal to send */
			return (0);
		sig = ffs((long)mask);
		bit = sigmask(sig);
		prop = sigprop[sig];

		/*
		 * We should see pending but ignored signals
		 * only if STRC was on when they were posted.
		 */
		if (bit & p->p_sigignore && (p->p_flag & STRC) == 0) {
			p->p_sig &= ~bit;
			continue;
		}

		if ((p->p_flag & (STRC|SPPWAIT)) == STRC) {
			/*
			 * If traced, always stop, and stay
			 * stopped until released by the parent.
			 */
			psignal(p->p_pptr, SIGCHLD);
			do
			    stop(p, 1, sig);
			while (!procxmt(p)
			    && (p->p_flag & (STRC|SPPWAIT)) == STRC);

			/*
			 * If the traced bit got turned off,
			 * go back up to the top to rescan signals.
			 * This ensures that p_sig* and ps_sigact
			 * are consistent.
			 */
			if ((p->p_flag & (STRC|SPPWAIT)) != STRC)
				continue;

			/*
			 * If parent wants us to take the signal,
			 * then it will leave it in p->p_sigacts->ps_stopsig;
			 * otherwise we just look for signals again.
			 */
			p->p_sig &= ~bit;	/* clear the old signal */
			sig = p->p_sigacts->ps_stopsig;
			if (sig == 0)
				continue;

			/*
			 * Put the new signal into p_sig.
			 * If signal is being masked,
			 * look for other signals.
			 */
			bit = sigmask(sig);
			p->p_sig |= bit;
			if (p->p_sigmask & bit)
				continue;
		}

		/*
		 * Decide whether the signal should be returned.
		 * Return the signal's number, or fall through
		 * to clear it from the pending mask.
		 */
		switch ((int)p->p_sigacts->ps_sigact[sig]) {

		case SIG_DFL:
			/*
			 * Don't take default actions on system processes.
			 */
			if (p->p_flag & SSYS)
				break;		/* == ignore */

			/*
			 * If there is a pending stop signal to process
			 * with default action, stop here,
			 * then clear the signal.  However,
			 * if process is member of an orphaned
			 * process group, ignore tty stop signals.
			 */
			if (prop & SA_STOP) {
				if (p->p_flag & STRC ||
		    		    (p->p_pgrp->pg_jobc == 0 &&
				    prop & SA_TTYSTOP))
					break;	/* == ignore */
				stop(p, 1, sig);
				break;
			} else if (prop & SA_IGNORE) {
				/*
				 * Except for SIGCONT, shouldn't get here.
				 * Default action is to ignore; drop it.
				 */
				break;		/* == ignore */
			} else
				return (sig);
			/*NOTREACHED*/

		case SIG_IGN:
#ifdef DIAGNOSTIC
			/*
			 * Masking above should prevent us ever trying
			 * to take action on an ignored signal other
			 * than SIGCONT, unless process is traced.
			 */
			if ((prop & SA_CONT) == 0 && (p->p_flag&STRC) == 0)
				printf("issig\n");
#endif
			break;		/* == ignore */

		default:
			/*
			 * This signal has an action, let
			 * psig process it.
			 */
			return (sig);
		}
		p->p_sig &= ~bit;		/* take the signal! */
	}
	/* NOTREACHED */
}

/*
 * Take the action for the specified signal
 * from the current set of pending signals.
 * Called with interrupts blocked.
 */
void
psig(int sig)
{
	struct proc *p = curproc;
	struct sigacts *ps = p->p_sigacts;
	sig_t action = ps->ps_sigact[sig];
	int bit = sigmask(sig), returnmask;

#ifdef DIAGNOSTIC
	if (sig == 0)
		panic("psig");
#endif

	p->p_sig &= ~bit;

#ifdef KTRACE
	if (KTRPOINT(p, KTR_PSIG))
		ktrpsig(p->p_tracep, sig, action, ps->ps_flags & SA_OLDMASK ?
		    ps->ps_oldmask : p->p_sigmask, 0);
#endif

	/* If the signal action is the default, terminate the process. */
	if (action == SIG_DFL)
		sigexit(p, sig);

	/* Otherwise the action is to process a caught signal. */
	else {
#ifdef DIAGNOSTIC
		if (action == SIG_IGN || (p->p_sigmask & bit))
			panic("psig action");
#endif
		/*
		 * Set the new mask value and also defer further
		 * occurences of this signal.
		 *
		 * Special case: user has done a sigpending.  Here the
		 * current mask is not of interest, but rather the
		 * mask from before the sigpending is what we want
		 * restored after the signal processing is completed.
		 */
		if (ps->ps_flags & SA_OLDMASK) {
			returnmask = ps->ps_oldmask;
			ps->ps_flags &= ~SA_OLDMASK;
		} else
			returnmask = p->p_sigmask;
		p->p_sigmask |= ps->ps_catchmask[sig] | bit;
		p->p_stats->p_ru.ru_nsignals++;

		/* deliver signal, if any error, terminate process */
		if (cpu_signal(p, sig, returnmask))
			sigexit(p, SIGILL);
		ps->ps_code = 0;	/* XXX for core dump/debugger */
	}
}

/*
 * Create a core dump due to abnormal process termination.
 * The file name is "core:progname".
 * Core dumps are not created if the process is setuid.
 */
int
coredump(struct proc *p)
{
	struct vnode *vp;
	struct pcred *pcred = p->p_cred;
	struct ucred *cred = pcred->pc_ucred;
	struct vmspace *vm = p->p_vmspace;
	struct vattr vattr;
	int error, error1;
	struct nameidata nd;
	char name[MAXCOMLEN+6];	/* <progname>.core */

	/* if setuid/setgid, don't bother for security purposes */
	if (pcred->p_svuid != pcred->p_ruid ||
	    pcred->p_svgid != pcred->p_rgid)
		return (EPERM);

	/* if core would exceed size of allowable core file, don't bother */
	if (ctob(UPAGES + vm->vm_dsize + vm->vm_ssize) >=
	    p->p_rlimit[RLIMIT_CORE].rlim_cur)
		return (EFAULT);

	/* open the core file */
	sprintf(name, "%s.core", p->p_comm);
	nd.ni_dirp = name;
	nd.ni_segflg = UIO_SYSSPACE;
	if (error = vn_open(&nd, p, O_CREAT|FWRITE, 0644))
		return (error);

	/* if the file is not suitable for use, don't bother */
	vp = nd.ni_vp;
	if (vp->v_type != VREG || VOP_GETATTR(vp, &vattr, cred, p) ||
	    vattr.va_nlink != 1) {
		error = EFAULT;
		goto out;
	}

	/* update file attributes in case we don't succeed in writing file */
	VATTR_NULL(&vattr);
	vattr.va_size = 0;
	VOP_SETATTR(vp, &vattr, cred, p);

	/* touch up process, save details in eproc for debugger in per-process */
	/* if (p && p->p_acflag)
		acct_core(p); */
	memcpy(&p->p_addr->u_kproc.kp_proc, p, sizeof(struct proc));
	fill_eproc(p, &p->p_addr->u_kproc.kp_eproc);

	/* write kernel stack, which now contains per-process, first */
	error = vn_rdwr(UIO_WRITE, vp, (caddr_t) p->p_addr, ctob(UPAGES),
	    (off_t)0, UIO_SYSSPACE, IO_NODELOCKED|IO_UNIT, cred, (int *) NULL,
	    p);
	if (error)
		goto out;

	/* write data segment XXX (what about shared libraries) */
	error = vn_rdwr(UIO_WRITE, vp, vm->vm_daddr,
	    (int)ctob(vm->vm_dsize), (off_t)ctob(UPAGES), UIO_USERSPACE,
	    IO_NODELOCKED|IO_UNIT, cred, (int *) NULL, p);
	if (error)
		goto out;

	/* write stack region */
	error = vn_rdwr(UIO_WRITE, vp,
	    (caddr_t) trunc_page(vm->vm_maxsaddr + MAXSSIZ
		- ctob(vm->vm_ssize)),
	    round_page(ctob(vm->vm_ssize)),
	    (off_t)ctob(UPAGES) + ctob(vm->vm_dsize), UIO_USERSPACE,
	    IO_NODELOCKED|IO_UNIT, cred, (int *) NULL, p);
out:
	/* release the file */
	VOP_UNLOCK(vp);
	error1 = vn_close(vp, FWRITE, cred, p);
	if (error == 0)
		error = error1;
	return (error);
}

/* internal kernel interfaces to the signal implementation */

/*
 * Send a signal caused by an processor exception trap to the current process.
 * If it will be caught immediately, deliver it with correct code.
 * If recurring while processing an outstanding signal, force termination.
 * Otherwise, post it normally.
 */
void
trapsignal(struct proc *p, int sig, unsigned code)
{
	struct sigacts *ps = p->p_sigacts;
	int bit = sigmask(sig);

	ps->ps_code = code;	/* XXX for core dump/debugger */

#ifdef	DIAGNOSTIC
	/* terminal error in pageout process? */
	if (p == pageproc)
		panic("trapsignal");
#endif

	/*
	 * If this is a signal due to an invalid memory reference,
	 * assure that the signal can be delivered to its handler,
	 * otherwise terminate the process.
	 */
	if (p == curproc  && sig == SIGSEGV
	    && vmspace_access(p->p_vmspace, (caddr_t)ps->ps_sigact[SIGSEGV], sizeof(int), PROT_READ)
	    == 0)
		sigexit(p, sig);

	/*
	 * If current process ready to catch signal without ptrace()
	 * facility in place to intercept, deliver signal directly
	 */
	if (p == curproc && (p->p_flag & STRC) == 0 &&
	    (p->p_sigcatch & bit) != 0) {

		/* if signal not already blocked, pass to user program ... */
		if ((p->p_sigmask & bit) == 0) {
			p->p_stats->p_ru.ru_nsignals++;
#ifdef KTRACE
			if (KTRPOINT(p, KTR_PSIG))
				ktrpsig(p->p_tracep, sig, ps->ps_sigact[sig], 
					p->p_sigmask, code);
#endif
			/* if failed to deliver, force process termination */
			if (cpu_signal(p, sig, p->p_sigmask))
				sigexit(p, sig);

			ps->ps_code = 0;	/* XXX for core dump/debugger */
			p->p_sigmask |= ps->ps_catchmask[sig] | bit;
		}
		/* ... otherwise force process termination. */
		else
			sigexit(p, sig);
	}
	/* process signal as if recieved from another process */
	else
		psignal(p, sig);
}

/*
 * Initialize signal state for the initial ancestor process, which
 * will be inherited by its progeny as the default. Signal actions
 * are set either to default termination or to be ignored based on
 * the property table.
 */
void
siginit(struct proc *p)
{
	int i;

	for (i = 0; i < NSIG; i++)
		if (sigprop[i] & SA_IGNORE && i != SIGCONT)
			p->p_sigignore |= sigmask(i);
}

/*
 * Reset signals for an execve() of the specified process.
 */
void
execsigs(struct proc *p)
{
	struct sigacts *ps = p->p_sigacts;
	int signo, bit;

	/*
	 * Reset caught signals.  Held signals remain held
	 * through p_sigmask (unless they were caught,
	 * and are now ignored by default).
	 */
	while (p->p_sigcatch) {
		signo = ffs((long)p->p_sigcatch);
		bit = sigmask(signo);
		p->p_sigcatch &= ~bit;
		if (sigprop[signo] & SA_IGNORE) {
			if (signo != SIGCONT)
				p->p_sigignore |= bit;
			p->p_sig &= ~bit;
		}
		ps->ps_sigact[signo] = SIG_DFL;
	}
	/*
	 * Reset stack state to the user stack.
	 * Clear set of signals caught on the signal stack.
	 */
	ps->ps_onstack = 0;
	ps->ps_sigsp = 0;
	ps->ps_sigonstack = 0;
	ps->ps_stopsig = 0;
}

/*
 * Send sig to every member of a process group.
 * If checktty is 1, limit to members which have a controlling
 * terminal.
 */
void
pgsignal(struct pgrp *pgrp, int sig, int checkctty)
{
	struct proc *p;

	if (pgrp)
		for (p = pgrp->pg_mem; p != NULL; p = p->p_pgrpnxt)
			if (checkctty == 0 || p->p_flag & SCTTY)
				psignal(p, sig);
}

/* private functions used in this signal implementation */

/*
 * Terminate this process unconditionally as a result of a signal.
 * If the signal requires a core dump, do one prior to termination.
 * We bypass the normal tests for masked and caught signals, allowing
 * unrecoverable failures to terminate the process without changing
 * signal state.
 */
static volatile void
sigexit(struct proc *p, int sig)
{
	/* does this signal require a core dump? */
	if (sigprop[sig] & SA_CORE) {

 		/* If dumping core, save the signal number for the debugger. */
		p->p_sigacts->ps_sig = sig;

		/* if successful, record for exit code that a core was made. */
		if (coredump(p) == 0)
			sig |= WCOREFLAG;
	}

	/* exit with appropriate status, never to return */
	exit(p, W_EXITCODE(0, sig));
}

/*
 * Stop the process p. If a signal is present, filter it via the parent
 * first. If requested, abandon the processor.
 */
static void
stop(struct proc *p, int swtchit, int sig)
{

	/* don't stop if we would deadlock */
	if (p->p_flag&SPPWAIT)
		return;

	/* if process is not traced and no SNOCLDSTOP option, signal parent */
	if ((p->p_flag & STRC) == 0 && (p->p_pptr->p_flag & SNOCLDSTOP) == 0)
		psignal(p->p_pptr, SIGCHLD);

	/* put the process into the stop state and wakeup its parent */
	p->p_stat = SSTOP;
	wakeup((caddr_t)p->p_pptr);

	/* record stop signal and indicate it has not been waited for yet */
	p->p_sigacts->ps_stopsig = sig;
	p->p_flag &= ~SWTED;

	/* if asked, give up processor */
	if (swtchit) {
		swtch();
		(void) splnone();
	}
}
