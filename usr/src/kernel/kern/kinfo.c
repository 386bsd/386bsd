/*
 * Copyright (c) 1982, 1986, 1989 Regents of the University of California.
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
 * $Id: kinfo.c,v 1.1 94/10/20 00:02:57 bill Exp $
 */

#include "sys/param.h"
#include "sys/kinfo.h"
#include "sys/ioctl.h"
#include "sys/file.h"
#include "sys/mman.h"
#include "sys/errno.h"
#include "proc.h"
#include "resourcevar.h"
#include "tty.h"
#include "vm.h"
#include "vmspace.h"

#include "sys/kinfo_proc.h"

#include "modconfig.h"
#include "prototypes.h"

#define snderr(e) { error = (e); goto release;}
static int
kinfo_doproc(int op, char *where, int *acopysize, int arg, int *aneeded);
static int
kinfo_file(int op, char *where, int *acopysize, int arg, int *aneeded);
struct kinfo_lock kinfo_lock;
struct kinfoif *kinfo_servers;

/* ARGSUSED */
int
getkerninfo(p, uap, retval)
	struct proc *p;
	register struct args {
		int	op;
		char	*where;
		int	*size;
		int	arg;
	} *uap;
	int *retval;
{
	int bufsize;		/* max size of users buffer */
	int needed, locked, (*server)(), error = 0, typ;
	struct kinfoif *kif;

	if (error = copyin(p, (caddr_t)uap->size, (caddr_t)&bufsize,
	    sizeof (bufsize)))
		goto done;

	for (kif = kinfo_servers; kif && kif->ki_idx != ki_type(uap->op);
	    kif = kif->ki_next) 
		;
	if (kif == NULL) {
		error = EINVAL;
		goto done;
	}
	
	if (uap->where == NULL || uap->size == NULL) {
		error = (kif->ki_srvr)(uap->op, NULL, NULL, uap->arg, &needed);
		goto done;
	}

	while (kinfo_lock.kl_lock) {
		kinfo_lock.kl_want++;
		(void) tsleep((caddr_t)&kinfo_lock, PRIBIO+1, "kinfo", 0);
		kinfo_lock.kl_want--;
		kinfo_lock.kl_locked++;
	}
	kinfo_lock.kl_lock++;

	if (!vmspace_access(p->p_vmspace, (caddr_t)uap->where, (unsigned)bufsize, PROT_WRITE))
		snderr(EFAULT);
	locked = bufsize;
	error = (kif->ki_srvr)(uap->op, uap->where, &bufsize, uap->arg, &needed);
	if (error == 0)
		error = copyout(p, (caddr_t)&bufsize,
				(caddr_t)uap->size, sizeof (bufsize));
release:
	kinfo_lock.kl_lock--;
	if (kinfo_lock.kl_want)
		wakeup((caddr_t)&kinfo_lock);
done:
	if (!error)
		*retval = needed;
	return (error);
}

/* 
 * try over estimating by 5 procs 
 */
#define KINFO_PROCSLOP	(5 * sizeof (struct kinfo_proc))

static int
kinfo_doproc(int op, char *where, int *acopysize, int arg, int *aneeded)
{
	register struct proc *p;
	register struct kinfo_proc *dp = (struct kinfo_proc *)where;
	register needed = 0;
	int buflen;
	int doingzomb;
	struct eproc eproc;
	int error = 0;

	if (where != NULL)
		buflen = *acopysize;

	p = allproc;
	doingzomb = 0;
again:
	for (; p != NULL; p = p->p_nxt) {
		/* 
		 * TODO - make more efficient (see notes below).
		 * do by session. 
		 */
		switch (ki_op(op)) {

		case KINFO_PROC_PID:
			/* could do this with just a lookup */
			if (p->p_pid != (pid_t)arg)
				continue;
			break;

		case KINFO_PROC_PGRP:
			/* could do this by traversing pgrp */
			if (p->p_pgrp->pg_id != (pid_t)arg)
				continue;
			break;

		case KINFO_PROC_TTY:
			if ((p->p_flag&SCTTY) == 0 || 
			    p->p_session->s_ttyp == NULL ||
			    p->p_session->s_ttyp->t_dev != (dev_t)arg)
				continue;
			break;

		case KINFO_PROC_UID:
			if (p->p_ucred->cr_uid != (uid_t)arg)
				continue;
			break;

		case KINFO_PROC_RUID:
			if (p->p_cred->p_ruid != (uid_t)arg)
				continue;
			break;
		}
		if (where != NULL && buflen >= sizeof (struct kinfo_proc)) {
			fill_eproc(p, &eproc);
			if (error = copyout(curproc, (caddr_t)p, &dp->kp_proc, 
			    sizeof (struct proc)))
				return (error);
			if (error = copyout(curproc, (caddr_t)&eproc, &dp->kp_eproc, 
			    sizeof (eproc)))
				return (error);
			dp++;
			buflen -= sizeof (struct kinfo_proc);
		}
		needed += sizeof (struct kinfo_proc);
	}
	if (doingzomb == 0) {
		p = zombproc;
		doingzomb++;
		goto again;
	}
	if (where != NULL)
		*acopysize = (caddr_t)dp - where;
	else
		needed += KINFO_PROCSLOP;
	*aneeded = needed;

	return (0);
}

/*
 * Fill in an eproc structure for the specified process.
 */
void
fill_eproc(p, ep)
	register struct proc *p;
	register struct eproc *ep;
{
	register struct tty *tp;

	ep->e_paddr = p;
	ep->e_sess = p->p_pgrp->pg_session;
	ep->e_pcred = *p->p_cred;
	ep->e_ucred = *p->p_ucred;
	if (p->p_vmspace)
		ep->e_vm = *p->p_vmspace;
	if (p->p_pptr)
		ep->e_ppid = p->p_pptr->p_pid;
	else
		ep->e_ppid = 0;
	ep->e_pgid = p->p_pgrp->pg_id;
	ep->e_jobc = p->p_pgrp->pg_jobc;
	if ((p->p_flag&SCTTY) && 
	     (tp = ep->e_sess->s_ttyp)) {
		ep->e_tdev = tp->t_dev;
		ep->e_tpgid = tp->t_pgrp ? tp->t_pgrp->pg_id : NO_PID;
		ep->e_tsess = tp->t_session;
	} else
		ep->e_tdev = TTY_NODEV;
	ep->e_flag = ep->e_sess->s_ttyvp ? EPROC_CTTY : 0;
	if (SESS_LEADER(p))
		ep->e_flag |= EPROC_SLEADER;
	if (p->p_wmesg)
		strncpy(ep->e_wmesg, p->p_wmesg, WMESGLEN);
	if (p->p_vmspace) {
		ep->e_xsize = p->p_vmspace->vm_tsize;
		ep->e_xrssize = (p->p_stats->p_ru.ru_ixrss * 1024) / NBPG;
	}
}

/*
 * Get file structures.
 */
static int
kinfo_file(int op, char *where, int *acopysize, int arg, int *aneeded)
{
	int buflen, needed, error;
	struct file *fp;
	char *start = where;

	if (where == NULL) {
		/*
		 * overestimate by 10 files
		 */
		*aneeded = sizeof (filehead) + 
			(nfiles + 10) * sizeof (struct file);
		return (0);
	}
	buflen = *acopysize;
	needed = 0;

	/*
	 * first copyout filehead
	 */
	if (buflen > sizeof (filehead)) {
		if (error = copyout(curproc, (caddr_t)&filehead, where,
		    sizeof (filehead)))
			return (error);
		buflen -= sizeof (filehead);
		where += sizeof (filehead);
	}
	needed += sizeof (filehead);

	/*
	 * followed by an array of file structures
	 */
	for (fp = filehead; fp != NULL; fp = fp->f_filef) {
		if (buflen > sizeof (struct file)) {
			if (error = copyout(curproc, (caddr_t)fp, where,
			    sizeof (struct file)))
				return (error);
			buflen -= sizeof (struct file);
			where += sizeof (struct file);
		}
		needed += sizeof (struct file);
	}
	*acopysize = where - start;
	*aneeded = needed;

	return (0);
}

void
kinfo_addserver(struct kinfoif *kif) {
	struct kinfoif *okif = kinfo_servers;

	kif->ki_next = okif;
	kinfo_servers = kif;
}

static struct kinfoif
	kinfo_proc_kif = { "proc", KINFO_PROC, kinfo_doproc },
	kinfo_file_kif = { "file", KINFO_FILE, kinfo_file };

/* configure servers */
KERNEL_MODCONFIG() {

	kinfo_addserver(&kinfo_proc_kif);
	kinfo_addserver(&kinfo_file_kif);
}
