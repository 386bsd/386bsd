/*
 * Copyright (c) 1982, 1986, 1989, 1990, 1991 Regents of the University
 * of California.  All rights reserved.
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
 * $Id: cred.c,v 1.1 94/10/20 00:02:48 bill Exp $
 */

/*
 * System calls related to process protection
 */

#include "sys/param.h"
#include "sys/errno.h"
#include "proc.h"
#include "privilege.h"
#include "malloc.h"
#include "prototypes.h"

/* if process credentials are to be modified, check if they need to unshare */
struct pcred *
modpcred(struct proc *p) {
	struct pcred *pc = p->p_cred;

	/* if not uniquely used, generate unique copy of process credentials */
	if (pc->p_refcnt != 1) {
		MALLOC(pc, struct pcred *, sizeof(struct pcred),
			M_SUBPROC, M_WAITOK);
		*pc = *p->p_cred;
		pc->p_refcnt = 1;
		p->p_cred->p_refcnt--;
		p->p_cred = pc;

		/* user credentials now referenced by both */
		crhold(p->p_ucred);
	}

	/* need to generate an unshared copy of user credentials also ? */
	if (pc->pc_ucred->cr_ref != 1) {
		/* duplicate the old one */
		struct ucred *newcr = crdup(pc->pc_ucred);

		/* release the old reference */
		crfree(pc->pc_ucred);
		pc->pc_ucred = newcr;
	}

	return (pc);
}

/* POSIX get current processes real user ID */
int
getuid(p, uap, retval)
	struct proc *p;
	void *uap;
	int *retval;
{

	*retval = p->p_cred->p_ruid;
	return (0);
}

/* POSIX get current processes effective user ID */
int
geteuid(p, uap, retval)
	struct proc *p;
	void *uap;
	int *retval;
{

	*retval = p->p_ucred->cr_uid;
	return (0);
}

/* POSIX get current processes real group ID */
int
getgid(p, uap, retval)
	struct proc *p;
	void *uap;
	int *retval;
{

	*retval = p->p_cred->p_rgid;
	return (0);
}

/* POSIX get current processes effective group ID */
int
getegid(p, uap, retval)
	struct proc *p;
	void *uap;
	int *retval;
{

	*retval = p->p_ucred->cr_groups[0];
	return (0);
}

/* BSD get supplimentary groups */
int
getgroups(p, uap, retval)
	struct proc *p;
	struct	arg {
		u_int	gidsetsize;
		int	*gidset;		/* XXX not yet POSIX */
	} *uap;
	int *retval;
{
	struct pcred *pc = p->p_cred;
	gid_t *gp;
	int *lp;
	u_int ngrp;
	int groups[NGROUPS_MAX];
	int error;

	/* inquiry for number of groups present in this process group set */
	if ((ngrp = uap->gidsetsize) == 0) {
		*retval = pc->pc_ucred->cr_ngroups;
		return (0);
	}

	/* if the receiving buffer cannot contain all groups in full, fail */
	if (ngrp < pc->pc_ucred->cr_ngroups)
		return (EINVAL);

	/* collect supplemental groups into an integer array buffer */
	ngrp = pc->pc_ucred->cr_ngroups;
	for (gp = pc->pc_ucred->cr_groups, lp = groups; lp < &groups[ngrp]; )
		*lp++ = *gp++;

	/* pass the array buffer back to the user process */
	if (error = copyout(p, (caddr_t)groups, (caddr_t)uap->gidset,
	    ngrp * sizeof (groups[0])))
		return (error);

	/* return number of groups returned */
	*retval = ngrp;
	return (0);
}

/* POSIX set the real user id of the process */
int
setuid(p, uap, retval)
	struct proc *p;
	struct args {
		int	uid;
	} *uap;
	int *retval;
{
	struct pcred *pc = p->p_cred;
	uid_t uid = uap->uid;
	int error;

	/* privilege to change user id? */
	if (uid != pc->p_ruid &&
	    (error = use_priv(pc->pc_ucred, PRV_SETUID, p)))
		return (error);

	/* modify process and user credentials to new real user id */
	pc = modpcred(p);
	pc->pc_ucred->cr_uid = uid;
	pc->p_ruid = uid;
	pc->p_svuid = uid;

	return (0);
}

/* POSIX set the effective user id of the process */
int
seteuid(p, uap, retval)
	struct proc *p;
	struct args {
		int	euid;
	} *uap;
	int *retval;
{
	struct pcred *pc = p->p_cred;
	uid_t euid = uap->euid;
	int error;

	/* privilege to change just effective user id? */
	if (euid != pc->p_ruid && euid != pc->p_svuid &&
	    (error = use_priv(pc->pc_ucred, PRV_SETUID, p)))
		return (error);

	/* modify user credentials to new effective user id */
	pc = modpcred(p);
	pc->pc_ucred->cr_uid = euid;

	return (0);
}

/* POSIX set the real group id of the process */
int
setgid(p, uap, retval)
	struct proc *p;
	struct args {
		int	gid;
	} *uap;
	int *retval;
{
	struct pcred *pc = p->p_cred;
	gid_t gid = uap->gid;
	int error;

	/* privilege to change group id? */
	if (gid != pc->p_rgid &&
	    (error = use_priv(pc->pc_ucred, PRV_SETGID, p)))
		return (error);

	/* modify user and process credentials to new group id */
	pc = modpcred(p);
	pc->pc_ucred->cr_groups[0] = gid;
	pc->p_rgid = gid;
	pc->p_svgid = gid;

	return (0);
}

/* POSIX set effective group id of the current process */
int
setegid(p, uap, retval)
	struct proc *p;
	struct args {
		int	egid;
	} *uap;
	int *retval;
{
	struct pcred *pc = p->p_cred;
	gid_t egid = uap->egid;
	int error;

	/* privilege to change just effective group id? */
	if (egid != pc->p_rgid && egid != pc->p_svgid &&
	    (error = use_priv(pc->pc_ucred, PRV_SETGID, p)))
		return (error);

	/* modify user credentials to new effective group id */
	pc = modpcred(p);
	pc->pc_ucred->cr_groups[0] = egid;

	return (0);
}

/* BSD set supplementary group set */
int
setgroups(p, uap, retval)
	struct proc *p;
	struct args {
		u_int	gidsetsize;
		int	*gidset;
	} *uap;
	int *retval;
{
	struct pcred *pc = p->p_cred;
	gid_t *gp;
	u_int ngrp;
	int *lp, error, groups[NGROUPS_MAX];

	/* privilege to set groups */
	if (error = use_priv(pc->pc_ucred, PRV_SETGID, p))
		return (error);

	/* more groups than implemented? */
	if ((ngrp = uap->gidsetsize) > NGROUPS_MAX)
		return (EINVAL);

	/* copy into temporary buffer array from user process */
	if (error = copyin(p, (caddr_t)uap->gidset, (caddr_t)groups,
	    ngrp * sizeof (groups[0])))
		return (error);

	/* modify process credentials to suit */
	pc = modpcred(p);
	pc->pc_ucred->cr_ngroups = ngrp;

	/* convert from int's to gid_t's */
	for (gp = pc->pc_ucred->cr_groups, lp = groups; ngrp--; )
		*gp++ = *lp++;

	return (0);
}

/* check if gid is a member of this user credential. */
int
groupmember(gid_t gid, const struct ucred *cred)
{
	gid_t *gp, *egp = &(cred->cr_groups[cred->cr_ngroups]);

	/* sift through supplimental group set array in user credentials */
	for (gp = cred->cr_groups; gp < egp; gp++)
		if (*gp == gid)
			return (1);

	return (0);
}

/* allocate a fresh, zeroed set of credentials */
struct ucred *
crget()
{
	struct ucred *cr;

	MALLOC(cr, struct ucred *, sizeof(struct ucred), M_CRED, M_WAITOK);
	(void) memset((caddr_t)cr, 0, sizeof(*cr));
	cr->cr_ref = 1;
	return (cr);
}

/* release a reference on a user credential set, and free when unreferenced */
void
crfree(struct ucred *cr)
{

	if (--cr->cr_ref != 0)
		return;
	FREE((caddr_t)cr, M_CRED);
}

/* replicate a user credential set */
struct ucred *
crdup(const struct ucred *cr)
{
	struct ucred *newcr;

	newcr = crget();
	*newcr = *cr;
	newcr->cr_ref = 1;
	return (newcr);
}

/* BSD get login name. */
int
getlogin(p, uap, retval)
	struct proc *p;
	struct args {
		char	*namebuf;
		u_int	namelen;
	} *uap;
	const int *retval;
{

	/* bounds limit string buffer to login buffer */
	if (uap->namelen > sizeof (p->p_pgrp->pg_session->s_login))
		uap->namelen = sizeof (p->p_pgrp->pg_session->s_login);

	/* return string buffer, partial or all */
	return (copyout(p, (caddr_t) p->p_pgrp->pg_session->s_login,
	    (caddr_t) uap->namebuf, uap->namelen));
}

/* BSD set login name. */
int
setlogin(p, uap, retval)
	struct proc *p;
	struct args {
		char	*namebuf;
		/* int	role; */
	} *uap;
	int *retval;
{
	int error;

	/* privilege to set login id? */
	if (error = use_priv(p->p_ucred, PRV_SETLOGIN, p))
		return (error);

	/* copy in variable length string into fixed length field */
	error = copyinstr(p, (caddr_t) uap->namebuf,
	    (caddr_t) p->p_pgrp->pg_session->s_login,
	    sizeof (p->p_pgrp->pg_session->s_login) - 1, (u_int *)0);
	if (error == ENAMETOOLONG)
		error = EINVAL;

	/* if successful, modify process credentials to assign role */
	if (error == 0) {
		struct pcred *pc = modpcred(p);

		pc->pc_ucred->cr_role = 0 /*role */;
	}
	return (error);
}
