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
 * $Id: proc.c,v 1.1 94/10/20 00:03:07 bill Exp $
 *
 * Process hierarchy and control.
 */

#include "sys/param.h"
#include "sys/wait.h"
#include "sys/file.h"
#include "sys/ioctl.h"
#include "sys/errno.h"
#include "tty.h"
#include "proc.h"
#include "malloc.h"
#include "uio.h"

/*#include "quota.h"*/

#include "prototypes.h"

struct proc *initproc, *pageproc, *zombproc, *allproc;
int whichqs;
struct prochd qs[NQS];

/*
 * Is p an inferior of the current process?
 */
int
inferior(struct proc *p)
{

	for (; p != curproc; p = p->p_pptr)
		if (p->p_pid == 0)
			return (0);
	return (1);
}

/*
 * Locate a process group by number
 */
struct pgrp *
pgfind(pid_t pgid)
{
	struct pgrp *pgrp = pgrphash[PIDHASH(pgid)];

	for (; pgrp; pgrp = pgrp->pg_hforw)
		if (pgrp->pg_id == pgid)
			return (pgrp);
	return ((struct pgrp *)0);
}

/*
 * Move p to a new or existing process group (and session)
 */
void
enterpgrp(struct proc *p, pid_t pgid, int mksess)
{
	struct pgrp *pgrp = pgfind(pgid);
	struct proc **pp;
	struct proc *cp;
	int n;

#ifdef DIAGNOSTIC
	if (pgrp && mksess)	/* firewalls */
		panic("enterpgrp: setsid into non-empty pgrp");
	if (SESS_LEADER(p))
		panic("enterpgrp: session leader attempted setpgrp");
#endif
	if (pgrp == NULL) {
		/*
		 * new process group
		 */
#ifdef DIAGNOSTIC
		if (p->p_pid != pgid)
			panic("enterpgrp: new pgrp and pid != pgid");
#endif
		MALLOC(pgrp, struct pgrp *, sizeof(struct pgrp), M_PGRP,
		       M_WAITOK);
		if (mksess) {
			register struct session *sess;

			/*
			 * new session
			 */
			MALLOC(sess, struct session *, sizeof(struct session),
				M_SESSION, M_WAITOK);
			sess->s_leader = p;
			sess->s_count = 1;
			sess->s_ttyvp = NULL;
			sess->s_ttyp = NULL;
			memcpy(sess->s_login, p->p_session->s_login,
			    sizeof(sess->s_login));
			p->p_flag &= ~SCTTY;
			pgrp->pg_session = sess;
#ifdef DIAGNOSTIC
			if (p != curproc)
				panic("enterpgrp: mksession and p != curproc");
#endif
		} else {
			pgrp->pg_session = p->p_session;
			pgrp->pg_session->s_count++;
		}
		pgrp->pg_id = pgid;
		pgrp->pg_hforw = pgrphash[n = PIDHASH(pgid)];
		pgrphash[n] = pgrp;
		pgrp->pg_jobc = 0;
		pgrp->pg_mem = NULL;
	} else if (pgrp == p->p_pgrp)
		return;

	/*
	 * Adjust eligibility of affected pgrps to participate in job control.
	 * Increment eligibility counts before decrementing, otherwise we
	 * could reach 0 spuriously during the first call.
	 */
	fixjobc(p, pgrp, 1);
	fixjobc(p, p->p_pgrp, 0);

	/*
	 * unlink p from old process group
	 */
	for (pp = &p->p_pgrp->pg_mem; *pp; pp = &(*pp)->p_pgrpnxt)
		if (*pp == p) {
			*pp = p->p_pgrpnxt;
			goto done;
		}
	panic("enterpgrp: can't find p on old pgrp");
done:
	/*
	 * delete old if empty
	 */
	if (p->p_pgrp->pg_mem == 0)
		pgdelete(p->p_pgrp);
	/*
	 * link into new one
	 */
	p->p_pgrp = pgrp;
	p->p_pgrpnxt = pgrp->pg_mem;
	pgrp->pg_mem = p;
}

/*
 * Session leader exits..
 */
void
exitsessleader(struct proc *p)
{
	struct session *sp = p->p_session;

	/* is there a controlling terminal? */
	if (sp->s_ttyvp) {

		/* if we are the controlling process */
		if (sp->s_ttyp->t_session == sp) {

			/* signal any foreground processes */
			if (sp->s_ttyp->t_pgrp)
				pgsignal(sp->s_ttyp->t_pgrp, SIGHUP, 1);

			/* wait for output to drain */
			(void) ttywait(sp->s_ttyp);

			/* revoke further access to terminal */
			vgoneall(sp->s_ttyvp);
		}
		vrele(sp->s_ttyvp);
		sp->s_ttyvp = NULL;
		/*
		 * s_ttyp is not zero'd; we use this to indicate
		 * that the session once had a controlling terminal.
		 * (for logging and informational purposes)
		 */
	}
	sp->s_leader = NULL;
}


/*
 * remove process from process group
 */
void
leavepgrp(struct proc *p)
{
	struct proc **pp = &p->p_pgrp->pg_mem;

	for (; *pp; pp = &(*pp)->p_pgrpnxt)
		if (*pp == p) {
			*pp = p->p_pgrpnxt;
			goto done;
		}
	panic("leavepgrp: can't find p in pgrp");
done:
	if (!p->p_pgrp->pg_mem)
		pgdelete(p->p_pgrp);
	p->p_pgrp = 0;
}

/*
 * delete a process group
 */
void
pgdelete(struct pgrp *pgrp)
{
	struct pgrp **pgp = &pgrphash[PIDHASH(pgrp->pg_id)];

	if (pgrp->pg_session->s_ttyp != NULL && 
	    pgrp->pg_session->s_ttyp->t_pgrp == pgrp)
		pgrp->pg_session->s_ttyp->t_pgrp = NULL;
	for (; *pgp; pgp = &(*pgp)->pg_hforw)
		if (*pgp == pgrp) {
			*pgp = pgrp->pg_hforw;
			goto done;
		}
	panic("pgdelete: can't find pgrp on hash chain");
done:
	if (--pgrp->pg_session->s_count == 0)
		FREE(pgrp->pg_session, M_SESSION);
	FREE(pgrp, M_PGRP);
}

static void orphanpg(struct pgrp *pg);

/*
 * Adjust pgrp jobc counters when specified process changes process group.
 * We count the number of processes in each process group that "qualify"
 * the group for terminal job control (those with a parent in a different
 * process group of the same session).  If that count reaches zero, the
 * process group becomes orphaned.  Check both the specified process'
 * process group and that of its children.
 * entering == 0 => p is leaving specified group.
 * entering == 1 => p is entering specified group.
 */
void
fixjobc(struct proc *p, struct pgrp *pgrp, int entering)
{
	struct pgrp *hispgrp;
	struct session *mysession = pgrp->pg_session;

	/*
	 * Check p's parent to see whether p qualifies its own process
	 * group; if so, adjust count for p's process group.
	 */
	if ((hispgrp = p->p_pptr->p_pgrp) != pgrp &&
	    hispgrp->pg_session == mysession)
		if (entering)
			pgrp->pg_jobc++;
		else if (--pgrp->pg_jobc == 0)
			orphanpg(pgrp);

	/*
	 * Check this process' children to see whether they qualify
	 * their process groups; if so, adjust counts for children's
	 * process groups.
	 */
	for (p = p->p_cptr; p; p = p->p_osptr)
		if ((hispgrp = p->p_pgrp) != pgrp &&
		    hispgrp->pg_session == mysession &&
		    p->p_stat != SZOMB)
			if (entering)
				hispgrp->pg_jobc++;
			else if (--hispgrp->pg_jobc == 0)
				orphanpg(hispgrp);
}

/* 
 * A process group has become orphaned;
 * if there are any stopped processes in the group,
 * hang-up all process in that group.
 */
static void
orphanpg(struct pgrp *pg)
{
	register struct proc *p;

	for (p = pg->pg_mem; p; p = p->p_pgrpnxt) {
		if (p->p_stat == SSTOP) {
			for (p = pg->pg_mem; p; p = p->p_pgrpnxt) {
				psignal(p, SIGHUP);
				psignal(p, SIGCONT);
			}
			return;
		}
	}
}


#ifdef DEBUG
void
pgrpdump(void)
{
	register struct pgrp *pgrp;
	register struct proc *p;
	register i;

	for (i=0; i<pidhashmask+1; i++) {
		if (pgrphash[i]) {
		  printf("\tindx %d\n", i);
		  for (pgrp=pgrphash[i]; pgrp; pgrp=pgrp->pg_hforw) {
		    printf("\tpgrp %x, pgid %d, sess %x, sesscnt %d, mem %x\n",
			pgrp, pgrp->pg_id, pgrp->pg_session,
			pgrp->pg_session->s_count, pgrp->pg_mem);
		    for (p=pgrp->pg_mem; p; p=p->p_pgrpnxt) {
			printf("\t\tpid %d addr %x pgrp %x\n", 
				p->p_pid, p, p->p_pgrp);
		    }
		  }

		}
	}
}
#endif /* DEBUG */

/* POSIX set session ID */
setsid(p, uap, retval)
	register struct proc *p;
	void *uap;
	int *retval;
{

	if (p->p_pgid == p->p_pid || pgfind(p->p_pid)) {
		return (EPERM);
	} else {
		enterpgrp(p, p->p_pid, 1);
		*retval = p->p_pid;
		return (0);
	}
}

/* POSIX insert a specified process into a specified process group */
setpgid(curp, uap, retval)
	struct proc *curp;
	struct args {
		int	pid;	/* target process id */
		int	pgid;	/* target pgrp id */
	} *uap;
	int *retval;
{
	struct proc *targp;		/* target process */
	struct pgrp *pgrp;		/* target pgrp */

	/* if target is not this process, check if it is an acceptable child */
	if (uap->pid != 0 && uap->pid != curp->p_pid) {

		/* does the process exist, and is it a child of the current? */
		if ((targp = pfind(uap->pid)) == 0 || !inferior(targp))
			return (ESRCH);

		/* is the target process in the same session as the current? */
		if (targp->p_session != curp->p_session)
			return (EPERM);

		/* has the process done an execve()? */
		if (targp->p_flag&SEXEC)
			return (EACCES);
	} else
		targp = curp;

	/* is the target process the session leader */
	if (SESS_LEADER(targp))
		return (EPERM);

	/* use current processes process group, or one in current session */
	if (uap->pgid == 0)
		uap->pgid = targp->p_pid;
	else if (uap->pgid != targp->p_pid)
		if ((pgrp = pgfind(uap->pgid)) == 0 ||
	            pgrp->pg_session != curp->p_session)
			return (EPERM);

	/* put target process into requested process group */
	enterpgrp(targp, uap->pgid, 0);
	return (0);
}

/* Get process group ID; note that POSIX getpgrp takes no parameter */
getpgrp(p, uap, retval)
	struct proc *p;
	void *uap;
	int *retval;
{

	*retval = p->p_pgrp->pg_id;
	return (0);
}

/* POSIX get current processes process ID */
getpid(p, uap, retval)
	struct proc *p;
	void *uap;
	int *retval;
{

	*retval = p->p_pid;
	return (0);
}

/* POSIX get parent processes process ID */
getppid(p, uap, retval)
	struct proc *p;
	void *uap;
	int *retval;
{

	*retval = p->p_pptr->p_pid;
	return (0);
}
