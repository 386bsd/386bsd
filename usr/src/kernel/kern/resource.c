/*-
 * Copyright (c) 1982, 1986, 1991 The Regents of the University of California.
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
 *	$Id: resource.c,v 1.1 94/10/20 00:03:13 bill Exp $
 */

#include "sys/param.h"
#include "sys/errno.h"
#include "proc.h"
#include "privilege.h"
#include "resourcevar.h"
#include "malloc.h"
#include "vm.h"
#include "vmspace.h"
#include "prototypes.h"

/*
 * Resource controls and accounting.
 */

getpriority(curp, uap, retval)
	struct proc *curp;
	register struct args {
		int	which;
		int	who;
	} *uap;
	int *retval;
{
	register struct proc *p;
	register int low = PRIO_MAX + 1;

	switch (uap->which) {

	case PRIO_PROCESS:
		if (uap->who == 0)
			p = curp;
		else
			p = pfind(uap->who);
		if (p == 0)
			break;
		low = p->p_nice;
		break;

	case PRIO_PGRP: {
		register struct pgrp *pg;

		if (uap->who == 0)
			pg = curp->p_pgrp;
		else if ((pg = pgfind(uap->who)) == NULL)
			break;
		for (p = pg->pg_mem; p != NULL; p = p->p_pgrpnxt) {
			if (p->p_nice < low)
				low = p->p_nice;
		}
		break;
	}

	case PRIO_USER:
		if (uap->who == 0)
			uap->who = curp->p_ucred->cr_uid;
		for (p = allproc; p != NULL; p = p->p_nxt) {
			if (p->p_ucred->cr_uid == uap->who &&
			    p->p_nice < low)
				low = p->p_nice;
		}
		break;

	default:
		return (EINVAL);
	}
	if (low == PRIO_MAX + 1)
		return (ESRCH);
	*retval = low;
	return (0);
}

/* ARGSUSED */
setpriority(curp, uap, retval)
	struct proc *curp;
	register struct args {
		int	which;
		int	who;
		int	prio;
	} *uap;
	int *retval;
{
	register struct proc *p;
	int found = 0, error = 0;

	switch (uap->which) {

	case PRIO_PROCESS:
		if (uap->who == 0)
			p = curp;
		else
			p = pfind(uap->who);
		if (p == 0)
			break;
		error = donice(curp, p, uap->prio);
		found++;
		break;

	case PRIO_PGRP: {
		register struct pgrp *pg;
		 
		if (uap->who == 0)
			pg = curp->p_pgrp;
		else if ((pg = pgfind(uap->who)) == NULL)
			break;
		for (p = pg->pg_mem; p != NULL; p = p->p_pgrpnxt) {
			error = donice(curp, p, uap->prio);
			found++;
		}
		break;
	}

	case PRIO_USER:
		if (uap->who == 0)
			uap->who = curp->p_ucred->cr_uid;
		for (p = allproc; p != NULL; p = p->p_nxt)
			if (p->p_ucred->cr_uid == uap->who) {
				error = donice(curp, p, uap->prio);
				found++;
			}
		break;

	default:
		return (EINVAL);
	}
	if (found == 0)
		return (ESRCH);
	return (0);
}

donice(curp, chgp, n)
	register struct proc *curp, *chgp;
	register int n;
{
	register struct pcred *pcred = curp->p_cred;

	if (pcred->pc_ucred->cr_uid && pcred->p_ruid &&
	    pcred->pc_ucred->cr_uid != chgp->p_ucred->cr_uid &&
	    pcred->p_ruid != chgp->p_ucred->cr_uid)
		return (EPERM);
	if (n > PRIO_MAX)
		n = PRIO_MAX;
	if (n < PRIO_MIN)
		n = PRIO_MIN;

	/* need a priviledge to change priority? */
	if (n < chgp->p_nice &&
	    use_priv(pcred->pc_ucred, PRV_NICE, curp))
		return (EACCES);

	chgp->p_nice = n;
	(void) setpri(chgp);
	return (0);
}

/* priviledge associated with resource limit */
static cr_priv_t limit_priv[RLIM_NLIMITS] = {
PRV_RLIMIT_CPU, PRV_RLIMIT_FSIZE, PRV_RLIMIT_DATA, PRV_RLIMIT_STACK,
PRV_RLIMIT_CORE, PRV_RLIMIT_RSS, PRV_RLIMIT_MEMLOCK, PRV_RLIMIT_NPROC,
PRV_RLIMIT_OFILE,
};

/* ARGSUSED */
setrlimit(p, uap, retval)
	struct proc *p;
	register struct args {
		u_int	which;
		struct	rlimit *lim;
	} *uap;
	int *retval;
{
	struct rlimit alim;
	register struct rlimit *alimp;
	int error;

	/* within bounds of currently implemented resource limits ? */
	if (uap->which >= RLIM_NLIMITS)
		return (EINVAL);

	/* fetch new limit entry into temp buffer to examine it */
	alimp = &p->p_rlimit[uap->which];
	if (error =
	    copyin(p, (caddr_t)uap->lim, (caddr_t)&alim, sizeof (struct rlimit)))
		return (error);

	/* if exceeding maxium limit, check for priviledge to do so */
	if (alim.rlim_cur > alimp->rlim_max || alim.rlim_max > alimp->rlim_max)
		if (error =
		    use_priv(p->p_ucred, limit_priv[uap->which], p))
			return (error);

	/* need to unshare limits ? */
	if (p->p_limit->p_refcnt > 1 &&
	    (p->p_limit->p_lflags & PL_SHAREMOD) == 0) {
		p->p_limit->p_refcnt--;
		p->p_limit = limcopy(p->p_limit);
	}

	switch (uap->which) {

	case RLIMIT_RSS:
		if (alim.rlim_cur > proc0.p_stats->p_ru.ru_maxrss)
			alim.rlim_cur = proc0.p_stats->p_ru.ru_maxrss;
		if (alim.rlim_max > proc0.p_stats->p_ru.ru_maxrss)
			alim.rlim_max = proc0.p_stats->p_ru.ru_maxrss;
		p->p_stats->p_ru.ru_maxrss = alim.rlim_cur;
		break;

	case RLIMIT_DATA:
		if (alim.rlim_cur > MAXDSIZ)
			alim.rlim_cur = MAXDSIZ;
		if (alim.rlim_max > MAXDSIZ)
			alim.rlim_max = MAXDSIZ;
		break;

	case RLIMIT_STACK:
		if (alim.rlim_cur > MAXDSIZ)
			alim.rlim_cur = MAXDSIZ;
		if (alim.rlim_max > MAXDSIZ)
			alim.rlim_max = MAXDSIZ;
		/*
		 * Stack is allocated to the max at exec time with only
		 * "rlim_cur" bytes accessible.  If stack limit is going
		 * up make more accessible, if going down make inaccessible.
		 */
		if (alim.rlim_cur != alimp->rlim_cur) {
			vm_offset_t addr;
			vm_size_t size;
			vm_prot_t prot;
			struct vmspace *vm = p->p_vmspace;

			addr = (unsigned) vm->vm_maxsaddr + MAXSSIZ;
			if (alim.rlim_cur > alimp->rlim_cur) {
				prot = VM_PROT_ALL;
				size = alim.rlim_cur - alimp->rlim_cur;
				addr -= alim.rlim_cur;
			} else {
				prot = VM_PROT_NONE;
				size = alimp->rlim_cur - alim.rlim_cur;
				addr -= alimp->rlim_cur;
			}
			(void) vmspace_protect(p->p_vmspace,
			      (caddr_t)addr, size, prot, FALSE);
		}
		break;
	}
	p->p_rlimit[uap->which] = alim;
	return (0);
}

/* ARGSUSED */
getrlimit(p, uap, retval)
	struct proc *p;
	register struct args {
		u_int	which;
		struct	rlimit *rlp;
	} *uap;
	int *retval;
{

	if (uap->which >= RLIM_NLIMITS)
		return (EINVAL);
	return (copyout(p, (caddr_t)&p->p_rlimit[uap->which], (caddr_t)uap->rlp,
	    sizeof (struct rlimit)));
}

/* ARGSUSED */
getrusage(p, uap, retval)
	register struct proc *p;
	register struct args {
		int	who;
		struct	rusage *rusage;
	} *uap;
	int *retval;
{
	register struct rusage *rup;

	switch (uap->who) {

	case RUSAGE_SELF: {
		int s;

		rup = &p->p_stats->p_ru;
		s = splclock();
		rup->ru_stime = p->p_stime;
		rup->ru_utime = p->p_utime;
		splx(s);
		break;
	}

	case RUSAGE_CHILDREN:
		rup = &p->p_stats->p_cru;
		break;

	default:
		return (EINVAL);
	}
	return (copyout(p, (caddr_t)rup, (caddr_t)uap->rusage,
	    sizeof (struct rusage)));
}

void
ruadd(struct rusage *ru, struct rusage *ru2)
{
	long *ip, *ip2;
	int i;

	timevaladd(&ru->ru_utime, &ru2->ru_utime);
	timevaladd(&ru->ru_stime, &ru2->ru_stime);
	if (ru->ru_maxrss < ru2->ru_maxrss)
		ru->ru_maxrss = ru2->ru_maxrss;
	ip = &ru->ru_first; ip2 = &ru2->ru_first;
	for (i = &ru->ru_last - &ru->ru_first; i >= 0; i--)
		*ip++ += *ip2++;
}

/*
 * Make a copy of the plimit structure.
 * We share these structures copy-on-write after fork,
 * and copy when a limit is changed.
 */
struct plimit *
limcopy(lim)
	struct plimit *lim;
{
	register struct plimit *copy;

	MALLOC(copy, struct plimit *, sizeof(struct plimit),
	    M_SUBPROC, M_WAITOK);
	memcpy(copy->pl_rlimit, lim->pl_rlimit,
	    sizeof(struct rlimit) * RLIM_NLIMITS);
	copy->p_lflags = 0;
	copy->p_refcnt = 1;
	return (copy);
}
