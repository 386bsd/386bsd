/*
 * Copyright (c) 1991 Regents of the University of California.
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
 *	@(#)signalvar.h	7.1 (Berkeley) 5/9/91
 */

#ifndef	_SIGNALVAR_H_		/* tmp for user.h */
#define	_SIGNALVAR_H_

/*
 * Kernel signal definitions and data structures,
 * not exported to user programs.
 */

/*
 * Process signal actions and state, needed only within the process
 * (not necessarily resident).
 */
struct	sigacts {
	sig_t	ps_sigact[NSIG];	/* disposition of signals */
	sigset_t ps_catchmask[NSIG];	/* signals to be blocked */
	sigset_t ps_sigonstack;		/* signals to take on sigstack */
	sigset_t ps_sigintr;		/* signals that interrupt syscalls */
	sigset_t ps_oldmask;		/* saved mask from before sigpause */
	int	ps_flags;		/* signal flags, below */
	struct	sigstack ps_sigstack;	/* sp & on stack state variable */
	int	ps_sig;			/* for core dump/debugger XXX */
	int	ps_code;		/* for core dump/debugger XXX */
};

#define	ps_onstack	ps_sigstack.ss_onstack
#define	ps_sigsp	ps_sigstack.ss_sp

/* signal flags */
#define	SA_OLDMASK	0x01		/* need to restore mask before pause */

/* additional signal action values, used only temporarily/internally */
#define	SIG_CATCH	(void (*)())2
#define	SIG_HOLD	(void (*)())3

/*
 * get signal action for process and signal; currently only for current process
 */
#define SIGACTION(p, sig)	(p->p_sigacts->ps_sigact[(sig)])

/*
 * Determine signal that should be delivered to process p, the current process,
 * 0 if none.  If there is a pending stop signal with default action,
 * the process stops in issig().
 */
#define	CURSIG(p) \
	(((p)->p_sig == 0 || \
	    ((p)->p_flag&STRC) == 0 && ((p)->p_sig &~ (p)->p_sigmask) == 0) ? \
	    0 : issig(p))

/*
 * Clear a pending signal from a process.
 */
#define	CLRSIG(p, sig)	{ (p)->p_sig &= ~sigmask(sig); }

/*
 * Signal properties and actions.
 * The array below categorizes the signals and their default actions
 * according to the following properties:
 */
#define	SA_KILL		0x01		/* terminates process by default */
#define	SA_CORE		0x02		/* ditto and coredumps */
#define	SA_STOP		0x04		/* suspend process */
#define	SA_TTYSTOP	(0x08|SA_STOP)	/* ditto, from tty */
#define	SA_IGNORE	0x10		/* ignore by default */
#define	SA_CONT		0x20		/* continue if suspended */
#define	SA_CANTMASK	0x40		/* non-maskable, catchable */

#ifdef	SIGPROP
int sigprop[NSIG + 1] = {
	0,			/* unused */
	SA_KILL,		/* SIGHUP */
	SA_KILL,		/* SIGINT */
	SA_KILL|SA_CORE,	/* SIGQUIT */
	SA_KILL|SA_CORE,	/* SIGILL */
	SA_KILL|SA_CORE,	/* SIGTRAP */
	SA_KILL|SA_CORE,	/* SIGABRT */
	SA_KILL|SA_CORE,	/* SIGEMT */
	SA_KILL|SA_CORE,	/* SIGFPE */
	SA_KILL,		/* SIGKILL */
	SA_KILL|SA_CORE,	/* SIGBUS */
	SA_KILL|SA_CORE,	/* SIGSEGV */
	SA_KILL|SA_CORE,	/* SIGSYS */
	SA_KILL,		/* SIGPIPE */
	SA_KILL,		/* SIGALRM */
	SA_KILL,		/* SIGTERM */
	SA_IGNORE,		/* SIGURG */
	SA_STOP,		/* SIGSTOP */
	SA_TTYSTOP,		/* SIGTSTP */
	SA_IGNORE|SA_CONT,	/* SIGCONT */
	SA_IGNORE,		/* SIGCHLD */
	SA_TTYSTOP,		/* SIGTTIN */
	SA_TTYSTOP,		/* SIGTTOU */
	SA_IGNORE,		/* SIGIO */
	SA_KILL,		/* SIGXCPU */
	SA_KILL,		/* SIGXFSZ */
	SA_KILL,		/* SIGVTALRM */
	SA_KILL,		/* SIGPROF */
	SA_IGNORE,		/* SIGWINCH  */
	SA_IGNORE,		/* SIGINFO */
	SA_KILL,		/* SIGUSR1 */
	SA_KILL,		/* SIGUSR2 */
};

#define	stopsigmask	(sigmask(SIGSTOP)|sigmask(SIGTSTP)|\
			 sigmask(SIGTTIN)|sigmask(SIGTTOU))
#define	contsigmask	(sigmask(SIGCONT))

#endif /* SIGPROP */

#define	sigcantmask	(sigmask(SIGKILL)|sigmask(SIGSTOP))

#ifdef KERNEL
/*
 * Machine-independent functions:
 */
void	siginit __P((struct proc *p));
void	execsigs __P((struct proc *p));
void	gsignal __P((int pgid, int sig));
void	pgsignal __P((struct pgrp *pgrp, int sig, int checkctty));
void	trapsignal __P((struct proc *p, int sig, unsigned code));
void	psignal __P((struct proc *p, int sig));
int	issig __P((struct proc *p));
void	psig __P((int sig));
int	coredump __P((struct proc *p));

/*
 * Machine-dependent functions:
 */
void	sendsig __P((sig_t action, int sig, int returnmask, unsigned code));
#endif	/* KERNEL */
#endif	/* !_SIGNALVAR_H_ */
