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
 * $Id: descrip.c,v 1.1 94/10/20 00:02:49 bill Exp Locker: bill $
 *
 * File instance and file descriptor management and system calls.
 */

#include "sys/param.h"
#include "sys/stat.h"
#include "sys/ioctl.h"
#include "sys/fcntl.h"
#include "sys/syslog.h"
#include "sys/errno.h"
#include "filedesc.h"
#include "kernel.h"	/* time */
#include "malloc.h"
#include "modconfig.h"
#include "proc.h"
/*#include "socketvar.h"*/
#include "resourcevar.h"
#include "uio.h"
#ifdef KTRACE
#include "sys/ktrace.h"
#endif


#include "vnode.h"

#include "prototypes.h"

/*
 * Descriptor management.
 */
struct file *filehead;	/* head of list of open files */
int nfiles;		/* actual number of open files */
static int fdopen(dev_t dev, int mode, int type, struct proc *p);
/*int fdalloc(struct proc *p, int want, int *result);*/
static int selscan(struct proc *p, fd_set *ibits, fd_set *obits, int nfd, int *retval);





/*
 * Descriptor system calls, both BSD and POSIX.
 */

/* BSD file descriptor "table" size (number of descriptors */
int
getdtablesize(p, uap, retval)
	struct proc *p;
	void *uap;
	int *retval;
{

	*retval = p->p_rlimit[RLIMIT_OFILE].rlim_cur;
	return (0);
}



/* POSIX Duplicate a file descriptor.  */
int
dup(p, uap, retval)
	struct proc *p;
	struct args {
		int	i;
	} *uap;
	int *retval;
{
	register struct filedesc *fdp = p->p_fd;
	struct file *fp;
	int fd, error;

	if ((unsigned)uap->i >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[uap->i]) == NULL)
		return (EBADF);

	if (error = fdalloc(p, 0, &fd))
		return (error);

	fdp->fd_ofiles[fd] = fp;
	fdp->fd_ofileflags[fd] = fdp->fd_ofileflags[uap->i] &~ UF_EXCLOSE;
	fp->f_count++;
	if (fd > fdp->fd_lastfile)
		fdp->fd_lastfile = fd;
	*retval = fd;
	return (0);
}




/* POSIX Duplicate a file descriptor to a particular value.  */
int
dup2(p, uap, retval)
	struct proc *p;
	struct args {
		u_int	from;
		u_int	to;
	} *uap;
	int *retval;
{
	register struct filedesc *fdp = p->p_fd;
	register struct file *fp;
	register u_int old = uap->from, new = uap->to;
	int i, error;

	if (old >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[old]) == NULL ||
	    new >= p->p_rlimit[RLIMIT_OFILE].rlim_cur)
		return (EBADF);

	*retval = new;
	if (old == new)
		return (0);

	if (new >= fdp->fd_nfiles) {
		if (error = fdalloc(p, new, &i))
			return (error);
#ifdef DIAGNOSTIC
		if (new != i)
			panic("dup2: fdalloc");
#endif
	} else if (fdp->fd_ofiles[new]) {
		if (fdp->fd_ofileflags[new] & UF_MAPPED)
			(void) munmapfd(p, new);
		/* dup2() must succeed even if the close has an error.*/
		(void) closef(fdp->fd_ofiles[new], p);
	}

	fdp->fd_ofiles[new] = fp;
	fdp->fd_ofileflags[new] = fdp->fd_ofileflags[old] &~ UF_EXCLOSE;
	fp->f_count++;
	if (new > fdp->fd_lastfile)
		fdp->fd_lastfile = new;
	return (0);
}




/* POSIX file control system call.  */
int
fcntl(p, uap, retval)
	struct proc *p;
	struct args {
		int	fd;
		int	cmd;
		int	arg;
	} *uap;
	int *retval;
{
	struct filedesc *fdp = p->p_fd;
	struct file *fp;
	char *pop;
	struct vnode *vp;
	int cmd, i, tmp, error, flg = F_POSIX;
	struct flock fl;

	if ((unsigned)uap->fd >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[uap->fd]) == NULL)
		return (EBADF);

	pop = &fdp->fd_ofileflags[uap->fd];
	switch(uap->cmd) {
	case F_DUPFD:
		if ((unsigned)uap->arg >= p->p_rlimit[RLIMIT_OFILE].rlim_cur)
			return (EINVAL);
		if (error = fdalloc(p, uap->arg, &i))
			return (error);
		fdp->fd_ofiles[i] = fp;
		fdp->fd_ofileflags[i] = *pop &~ UF_EXCLOSE;
		fp->f_count++;
		if (i > fdp->fd_lastfile)
			fdp->fd_lastfile = i;
		*retval = i;
		return (0);

	case F_GETFD:
		*retval = *pop & UF_EXCLOSE;
		return (0);

	case F_SETFD:
		*pop = (*pop &~ UF_EXCLOSE) | (uap->arg & 1);
		return (0);

	case F_GETFL:
		*retval = OFLAGS(fp->f_flag);
		return (0);

	case F_SETFL:
		fp->f_flag &= ~FCNTLFLAGS;
		fp->f_flag |= FFLAGS(uap->arg) & FCNTLFLAGS;
		tmp = fp->f_flag & FNONBLOCK;
		error = (*fp->f_ops->fo_ioctl)(fp, FIONBIO, (caddr_t)&tmp, p);
		if (error)
			return (error);
		tmp = fp->f_flag & FASYNC;
		error = (*fp->f_ops->fo_ioctl)(fp, FIOASYNC, (caddr_t)&tmp, p);
		if (!error)
			return (0);
		fp->f_flag &= ~FNONBLOCK;
		tmp = 0;
		(void) (*fp->f_ops->fo_ioctl)(fp, FIONBIO, (caddr_t)&tmp, p);
		return (error);

	case F_GETOWN:
		if (fp->f_type == DTYPE_SOCKET)
			cmd = SIOCGPGRP;
		else
			cmd = TIOCGPGRP;
		error = (*fp->f_ops->fo_ioctl) (fp, cmd, (caddr_t)retval, p);
		if (fp->f_type != DTYPE_SOCKET)
			*retval = -*retval;
		return (error);

	case F_SETOWN:
		if (fp->f_type == DTYPE_SOCKET)
			cmd = SIOCSPGRP;
		else {
			cmd = TIOCSPGRP;
			if (uap->arg <= 0) {
				uap->arg = -uap->arg;
			} else {
				struct proc *p1 = pfind(uap->arg);
				if (p1 == 0)
					return (ESRCH);
				uap->arg = p1->p_pgrp->pg_id;
			}
		}
		return((*fp->f_ops->fo_ioctl)(fp, cmd, (caddr_t)&uap->arg, p)); 

	case F_SETLKW:
		flg |= F_WAIT;
		/* Fall into F_SETLK */

	case F_SETLK:
		if (fp->f_type != DTYPE_VNODE)
			return (EBADF);
		vp = (struct vnode *)fp->f_data;

		/* fetch the lock structure */
		error = copyin(p, (caddr_t)uap->arg, (caddr_t)&fl, sizeof (fl));
		if (error)
			return (error);

		if (fl.l_whence == SEEK_CUR)
			fl.l_start += fp->f_offset;

		switch (fl.l_type) {

		case F_RDLCK:
			if ((fp->f_flag & FREAD) == 0)
				return (EBADF);
			p->p_flag |= SADVLCK;
			return (VOP_ADVLOCK(vp, (caddr_t)p, F_SETLK, &fl, flg));

		case F_WRLCK:
			if ((fp->f_flag & FWRITE) == 0)
				return (EBADF);
			p->p_flag |= SADVLCK;
			return (VOP_ADVLOCK(vp, (caddr_t)p, F_SETLK, &fl, flg));

		case F_UNLCK:
			return (VOP_ADVLOCK(vp, (caddr_t)p, F_UNLCK, &fl,
				F_POSIX));

		default:
			return (EINVAL);
		}

	case F_GETLK:
		if (fp->f_type != DTYPE_VNODE)
			return (EBADF);
		vp = (struct vnode *)fp->f_data;

		/* fetch the lock structure */
		error = copyin(p, (caddr_t)uap->arg, (caddr_t)&fl, sizeof (fl));
		if (error)
			return (error);
		if (fl.l_whence == SEEK_CUR)
			fl.l_start += fp->f_offset;
		if (error = VOP_ADVLOCK(vp, (caddr_t)p, F_GETLK, &fl, F_POSIX))
			return (error);
		return (copyout(p, (caddr_t)&fl, (caddr_t)uap->arg, sizeof (fl)));

	default:
		return (EINVAL);
	}
	/* NOTREACHED */
}




/* POSIX close a file descriptor. */
int
close(p, uap, retval)
	struct proc *p;
	struct args {
		int	fd;
	} *uap;
	int *retval;
{
	struct filedesc *fdp = p->p_fd;
	struct file *fp;
	int fd = uap->fd;
	u_char *pf;

	if ((unsigned)fd >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[fd]) == NULL)
		return (EBADF);

	pf = (u_char *)&fdp->fd_ofileflags[fd];
	if (*pf & UF_MAPPED)
		(void) munmapfd(p, fd);

	fdp->fd_ofiles[fd] = NULL;
	while (fdp->fd_lastfile > 0 && fdp->fd_ofiles[fdp->fd_lastfile] == NULL)
		fdp->fd_lastfile--;
	if (fd < fdp->fd_freefile)
		fdp->fd_freefile = fd;
	*pf = 0;
	return (closef(fp, p));
}



/* POSIX: Return status information about a file descriptor. */
int
fstat(p, uap, retval)
	struct proc *p;
	struct args {
		int	fd;
		struct	stat *sb;
	} *uap;
	int *retval;
{
	struct filedesc *fdp = p->p_fd;
	struct file *fp;
	struct stat ub;
	int error;

	if ((unsigned)uap->fd >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[uap->fd]) == NULL)
		return (EBADF);

#ifdef foo
	switch (fp->f_type) {

	case DTYPE_VNODE:
		error = vn_stat((struct vnode *)fp->f_data, &ub, p);
		break;

	case DTYPE_SOCKET:
		error = soo_stat((struct socket *)fp->f_data, &ub);
		break;

	default:
		panic("fstat");
		/*NOTREACHED*/
	}
#endif
	error = (*fp->f_ops->fo_stat)(fp, &ub, p);

	if (error == 0)
		error = copyout(p, (caddr_t)&ub, (caddr_t)uap->sb, sizeof (ub));
	return (error);
}



/* POSIX Read function system call handler.  */
int
read(p, uap, retval)
	struct proc *p;
	register struct args {
		int	fdes;
		char	*cbuf;
		unsigned count;
	} *uap;
	int *retval;
{
	register struct file *fp;
	register struct filedesc *fdp = p->p_fd;
	struct uio auio;
	struct iovec aiov;
	long cnt, error = 0;
#ifdef KTRACE
	struct iovec ktriov;
#endif

	if (((unsigned)uap->fdes) >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[uap->fdes]) == NULL ||
	    (fp->f_flag & FREAD) == 0)
		return (EBADF);

	aiov.iov_base = (caddr_t)uap->cbuf;
	aiov.iov_len = uap->count;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_resid = uap->count;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_procp = p;

#ifdef KTRACE
	/*
	 * if tracing, save a copy of iovec
	 */
	if (KTRPOINT(p, KTR_GENIO))
		ktriov = aiov;
#endif

	cnt = uap->count;
	if (error = (*fp->f_ops->fo_read)(fp, &auio, fp->f_cred))
		if (auio.uio_resid != cnt && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
	cnt -= auio.uio_resid;

#ifdef KTRACE
	if (KTRPOINT(p, KTR_GENIO) && error == 0)
		ktrgenio(p->p_tracep, uap->fdes, UIO_READ, &ktriov, cnt, error);
#endif

	*retval = cnt;
	return (error);
}



/* BSD scatter readv() system call handler. */
int
readv(p, uap, retval)
	struct proc *p;
	register struct args {
		int	fdes;
		struct	iovec *iovp;
		unsigned iovcnt;
	} *uap;
	int *retval;
{
	struct file *fp;
	struct filedesc *fdp = p->p_fd;
	struct uio auio;
	struct iovec *iov;
	struct iovec *saveiov;
	struct iovec aiov[UIO_SMALLIOV];
	long i, cnt, error = 0;
	unsigned iovlen;
#ifdef KTRACE
	struct iovec *ktriov = NULL;
#endif

	if (((unsigned)uap->fdes) >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[uap->fdes]) == NULL ||
	    (fp->f_flag & FREAD) == 0)
		return (EBADF);

	/* note: can't use iovlen until iovcnt is validated */
	iovlen = uap->iovcnt * sizeof (struct iovec);

	if (uap->iovcnt > UIO_SMALLIOV) {
		if (uap->iovcnt > UIO_MAXIOV)
			return (EINVAL);
		MALLOC(iov, struct iovec *, iovlen, M_IOV, M_WAITOK);
		saveiov = iov;
	} else
		iov = aiov;

	auio.uio_iov = iov;
	auio.uio_iovcnt = uap->iovcnt;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_procp = p;

	if (error = copyin(p, (caddr_t)uap->iovp, (caddr_t)iov, iovlen))
		goto done;

	auio.uio_resid = 0;
	for (i = 0; i < uap->iovcnt; i++) {
		if (iov->iov_len < 0) {
			error = EINVAL;
			goto done;
		}
		auio.uio_resid += iov->iov_len;
		if (auio.uio_resid < 0) {
			error = EINVAL;
			goto done;
		}
		iov++;
	}

#ifdef KTRACE
	/*
	 * if tracing, save a copy of iovec
	 */
	if (KTRPOINT(p, KTR_GENIO))  {
		MALLOC(ktriov, struct iovec *, iovlen, M_TEMP, M_WAITOK);
		(void) memcpy((caddr_t)ktriov, (caddr_t)auio.uio_iov, iovlen);
	}
#endif

	cnt = auio.uio_resid;
	if (error = (*fp->f_ops->fo_read)(fp, &auio, fp->f_cred))
		if (auio.uio_resid != cnt && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
	cnt -= auio.uio_resid;

#ifdef KTRACE
	if (ktriov != NULL) {
		if (error == 0)
			ktrgenio(p->p_tracep, uap->fdes, UIO_READ, ktriov,
			    cnt, error);
		FREE(ktriov, M_TEMP);
	}
#endif

	*retval = cnt;
done:
	if (uap->iovcnt > UIO_SMALLIOV)
		FREE(saveiov, M_IOV);

	return (error);
}



/* POSIX write() function system call handler */
int
write(p, uap, retval)
	struct proc *p;
	struct args {
		int	fdes;
		char	*cbuf;
		unsigned count;
	} *uap;
	int *retval;
{
	register struct file *fp;
	register struct filedesc *fdp = p->p_fd;
	struct uio auio;
	struct iovec aiov;
	long cnt, error = 0;
#ifdef KTRACE
	struct iovec ktriov;
#endif

	if (((unsigned)uap->fdes) >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[uap->fdes]) == NULL ||
	    (fp->f_flag & FWRITE) == 0)
		return (EBADF);

	aiov.iov_base = (caddr_t)uap->cbuf;
	aiov.iov_len = uap->count;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_resid = uap->count;
	auio.uio_rw = UIO_WRITE;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_procp = p;

#ifdef KTRACE
	/*
	 * if tracing, save a copy of iovec
	 */
	if (KTRPOINT(p, KTR_GENIO))
		ktriov = aiov;
#endif

	cnt = uap->count;
	if (error = (*fp->f_ops->fo_write)(fp, &auio, fp->f_cred)) {
		if (auio.uio_resid != cnt && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
		if (error == EPIPE)
			psignal(p, SIGPIPE);
	}
	cnt -= auio.uio_resid;

#ifdef KTRACE
	if (KTRPOINT(p, KTR_GENIO) && error == 0)
		ktrgenio(p->p_tracep, uap->fdes, UIO_WRITE,
		    &ktriov, cnt, error);
#endif

	*retval = cnt;
	return (error);
}



/* BSD gather write writev() function system call handler */
int
writev(p, uap, retval)
	struct proc *p;
	register struct args {
		int	fdes;
		struct	iovec *iovp;
		unsigned iovcnt;
	} *uap;
	int *retval;
{
	register struct file *fp;
	register struct filedesc *fdp = p->p_fd;
	struct uio auio;
	register struct iovec *iov;
	struct iovec *saveiov;
	struct iovec aiov[UIO_SMALLIOV];
	long i, cnt, error = 0;
	unsigned iovlen;
#ifdef KTRACE
	struct iovec *ktriov = NULL;
#endif

	if (((unsigned)uap->fdes) >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[uap->fdes]) == NULL ||
	    (fp->f_flag & FWRITE) == 0)
		return (EBADF);

	/* note: can't use iovlen until iovcnt is validated */
	iovlen = uap->iovcnt * sizeof (struct iovec);
	if (uap->iovcnt > UIO_SMALLIOV) {
		if (uap->iovcnt > UIO_MAXIOV)
			return (EINVAL);
		MALLOC(iov, struct iovec *, iovlen, M_IOV, M_WAITOK);
		saveiov = iov;
	} else
		iov = aiov;

	auio.uio_iov = iov;
	auio.uio_iovcnt = uap->iovcnt;
	auio.uio_rw = UIO_WRITE;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_procp = p;

	if (error = copyin(p, (caddr_t)uap->iovp, (caddr_t)iov, iovlen))
		goto done;

	auio.uio_resid = 0;
	for (i = 0; i < uap->iovcnt; i++) {
		if (iov->iov_len < 0) {
			error = EINVAL;
			goto done;
		}
		auio.uio_resid += iov->iov_len;
		if (auio.uio_resid < 0) {
			error = EINVAL;
			goto done;
		}
		iov++;
	}

#ifdef KTRACE
	/*
	 * if tracing, save a copy of iovec
	 */
	if (KTRPOINT(p, KTR_GENIO))  {
		MALLOC(ktriov, struct iovec *, iovlen, M_TEMP, M_WAITOK);
		(void) memcpy((caddr_t)ktriov, (caddr_t)auio.uio_iov, iovlen);
	}
#endif

	cnt = auio.uio_resid;
	if (error = (*fp->f_ops->fo_write)(fp, &auio, fp->f_cred)) {
		if (auio.uio_resid != cnt && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
		if (error == EPIPE)
			psignal(p, SIGPIPE);
	}
	cnt -= auio.uio_resid;

#ifdef KTRACE
	if (ktriov != NULL) {
		if (error == 0)
			ktrgenio(p->p_tracep, uap->fdes, UIO_WRITE,
				ktriov, cnt, error);
		FREE(ktriov, M_TEMP);
	}
#endif

	*retval = cnt;
done:
	if (uap->iovcnt > UIO_SMALLIOV)
		FREE(saveiov, M_IOV);

	return (error);
}




/* POSIX ioctl() function system call handler */
int
ioctl(p, uap, retval)
	struct proc *p;
	register struct args {
		int	fdes;
		int	cmd;
		caddr_t	cmarg;
	} *uap;
	int *retval;
{
	struct file *fp;
	struct filedesc *fdp = p->p_fd;
	int com, cmd, error;
	u_int size;
	caddr_t memp = 0;
#define STK_PARAMS	128
	char stkbuf[STK_PARAMS];
	caddr_t data = stkbuf;
	int tmp;

	if ((unsigned)uap->fdes >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[uap->fdes]) == NULL)
		return (EBADF);
	if ((fp->f_flag & (FREAD|FWRITE)) == 0)
		return (EBADF);

	com = uap->cmd;
	if (com == FIOCLEX) {
		fdp->fd_ofileflags[uap->fdes] |= UF_EXCLOSE;
		return (0);
	}
	if (com == FIONCLEX) {
		fdp->fd_ofileflags[uap->fdes] &= ~UF_EXCLOSE;
		return (0);
	}

	/*
	 * Interpret high order word to find
	 * amount of data to be copied to/from the
	 * user's address space.
	 */
	size = IOCPARM_LEN(com);
	if (size > IOCPARM_MAX)
		return (ENOTTY);
	if (size > sizeof (stkbuf)) {
		memp = (caddr_t)malloc((u_long)size, M_IOCTLOPS, M_WAITOK);
		data = memp;
	}
	if (com&IOC_IN) {
		if (size) {
			error = copyin(p, uap->cmarg, data, (u_int)size);
			if (error) {
				if (memp)
					free(memp, M_IOCTLOPS);
				return (error);
			}
		} else
			*(caddr_t *)data = uap->cmarg;
	} else if ((com&IOC_OUT) && size)
		/*
		 * Zero the buffer so the user always
		 * gets back something deterministic.
		 */
		(void) memset(data, 0, size);
	else if (com&IOC_VOID)
		*(caddr_t *)data = uap->cmarg;

	switch (com) {

	case FIONBIO:
		if (tmp = *(int *)data)
			fp->f_flag |= FNONBLOCK;
		else
			fp->f_flag &= ~FNONBLOCK;
		error = (*fp->f_ops->fo_ioctl)(fp, FIONBIO, (caddr_t)&tmp, p);
		break;

	case FIOASYNC:
		if (tmp = *(int *)data)
			fp->f_flag |= FASYNC;
		else
			fp->f_flag &= ~FASYNC;
		error = (*fp->f_ops->fo_ioctl)(fp, FIOASYNC, (caddr_t)&tmp, p);
		break;

	case FIOSETOWN:
		tmp = *(int *)data;
		if (fp->f_type == DTYPE_SOCKET)
			cmd = SIOCSPGRP;
		else {
			cmd = TIOCSPGRP;
			if (tmp <= 0) {
				tmp = -tmp;
			} else {
				struct proc *p1 = pfind(tmp);
				if (p1 == 0) {
					error = ESRCH;
					break;
				}
				tmp = p1->p_pgrp->pg_id;
			}
		}
		error = (*fp->f_ops->fo_ioctl) (fp, cmd, (caddr_t)&tmp, p);
		break;

	case FIOGETOWN:
		if (fp->f_type == DTYPE_SOCKET)
			cmd = SIOCGPGRP;
		else
			cmd = TIOCGPGRP;
		error = (*fp->f_ops->fo_ioctl)(fp, cmd, data, p);
		if (fp->f_type != DTYPE_SOCKET)
			*(int *)data = -*(int *)data;
		break;

	default:
		error = (*fp->f_ops->fo_ioctl)(fp, com, data, p);
		if (error == 0 && (com&IOC_OUT) && size)
			error = copyout(p, data, uap->cmarg, (u_int)size);
		break;
	}

	if (memp)
		free(memp, M_IOCTLOPS);
	return (error);
}

int	selwait, nselcoll;



/* BSD select() function system call handler.  */
int
select(p, uap, retval)
	register struct proc *p;
	register struct args {
		int	nd;
		fd_set	*in, *ou, *ex;
		struct	timeval *tv;
	} *uap;
	int *retval;
{
	fd_set ibits[3], obits[3];
	struct timeval atv;
	int ncoll, ni, error = 0, timo;

	(void) memset((caddr_t)ibits, 0, sizeof(ibits));
	(void) memset((caddr_t)obits, 0, sizeof(obits));
	if (uap->nd > p->p_fd->fd_nfiles)
		uap->nd = p->p_fd->fd_nfiles;
	ni = howmany(uap->nd, NFDBITS);

#define	getbits(name, x) \
	if (uap->name) { \
		error = copyin(p, (caddr_t)uap->name, (caddr_t)&ibits[x], \
		    (unsigned)(ni * sizeof(fd_mask))); \
		if (error) \
			goto done; \
	}
	getbits(in, 0);
	getbits(ou, 1);
	getbits(ex, 2);
#undef	getbits

	if (uap->tv) {
		error = copyin(p, (caddr_t)uap->tv, (caddr_t)&atv,
			sizeof (atv));
		if (error)
			goto done;
		if (itimerfix(&atv)) {
			error = EINVAL;
			goto done;
		}
		splclock(); timevaladd(&atv, &time); splnone();
		timo = hzto(&atv);
	} else
		timo = 0;

retry:
	ncoll = nselcoll;
	p->p_flag |= SSEL;
	error = selscan(p, ibits, obits, uap->nd, retval);
	if (error || *retval)
		goto done;

	splhigh();
	/* this should be timercmp(&time, &atv, >=) */
	if (uap->tv && (time.tv_sec > atv.tv_sec ||
	    time.tv_sec == atv.tv_sec && time.tv_usec >= atv.tv_usec)) {
		p->p_flag &= ~SSEL;
		/* error = EWOULDBLOCK; */
		splnone();
		goto done;
	}
	if ((p->p_flag & SSEL) == 0 || nselcoll != ncoll) {
		splnone();
		goto retry;
	}
	p->p_flag &= ~SSEL;
	error = tsleep((caddr_t)&selwait, PSOCK | PCATCH, "select", timo);
	splnone();

	if (error == 0)
		goto retry;
done:

	/* select is not restarted after signals... */
	if (error == ERESTART)
		error = EINTR;
	if (error == EWOULDBLOCK)
		error = 0;

#define	putbits(name, x) \
	if (uap->name) { \
		int error2 = copyout(p, (caddr_t)&obits[x], (caddr_t)uap->name, \
		    (unsigned)(ni * sizeof(fd_mask))); \
		if (error2) \
			error = error2; \
	}
	if (error == 0) {
		putbits(in, 0);
		putbits(ou, 1);
		putbits(ex, 2);
	}
#undef putbits

	return (error);
}

static int
selscan(struct proc *p, fd_set *ibits, fd_set *obits, int nfd, int *retval)
{
	struct filedesc *fdp = p->p_fd;
	int which, i, j;
	fd_mask bits;
	int flag;
	struct file *fp;
	int error = 0, n = 0;

	for (which = 0; which < 3; which++) {
		switch (which) {

		case 0:
			flag = FREAD; break;

		case 1:
			flag = FWRITE; break;

		case 2:
			flag = 0; break;
		}
		for (i = 0; i < nfd; i += NFDBITS) {
			bits = ibits[which].fds_bits[i/NFDBITS];
			while ((j = ffs(bits)) && i + --j < nfd) {
				bits &= ~(1 << j);
				fp = fdp->fd_ofiles[i + j];
				if (fp == NULL) {
					error = EBADF;
					break;
				}
				if ((*fp->f_ops->fo_select)(fp, flag, p)) {
					FD_SET(i + j, &obits[which]);
					n++;
				}
			}
		}
	}
	*retval = n;
	return (error);
}

/*ARGSUSED*/
int
seltrue(dev_t dev, int which, struct proc *p)
{

	return (1);
}


void
selwakeup(pid_t pid, int coll)
{
	register struct proc *p;

	if (coll) {
		nselcoll++;
		wakeup((caddr_t)&selwait);
	}

	if (pid && (p = pfind(pid))) {
		int s = splhigh();

		if (p->p_wchan == (caddr_t)&selwait) {
			if (p->p_stat == SSLEEP)
				setrun(p);
			else
				unsleep(p);
		} else if (p->p_flag & SSEL)
			p->p_flag &= ~SSEL;
		splx(s);
	}
}

/*
 * Allocate a file descriptor for the process.
 */
int
fdalloc(struct proc *p, int want, int *result)
{
	register struct filedesc *fdp = p->p_fd;
	int i;
	int lim, last, nfiles;
	struct file **newofile;
	char *newofileflags;

	/*
	 * Search for a free descriptor starting at the higher
	 * of want or fd_freefile.  If that fails, consider
	 * expanding the ofile array.
	 */
	lim = p->p_rlimit[RLIMIT_OFILE].rlim_cur;
	for (;;) {
		last = min(fdp->fd_nfiles, lim);
		if ((i = want) < fdp->fd_freefile)
			i = fdp->fd_freefile;
		for (; i < last; i++) {
			if (fdp->fd_ofiles[i] == NULL) {
				fdp->fd_ofileflags[i] = 0;
				if (i > fdp->fd_lastfile)
					fdp->fd_lastfile = i;
				if (want <= fdp->fd_freefile)
					fdp->fd_freefile = i;
				*result = i;
				return (0);
			}
		}

		/* no space in current array.  expand it? */
		if (fdp->fd_nfiles >= lim)
			return (EMFILE);
		if (fdp->fd_nfiles < NDEXTENT)
			nfiles = NDEXTENT;
		else
			nfiles = (2 * fdp->fd_nfiles) % lim;

		MALLOC(newofile, struct file **, nfiles * OFILESIZE,
		    M_FILEDESC, M_WAITOK);
		newofileflags = (char *) &newofile[nfiles];

		/*
		 * Copy the existing ofile and ofileflags arrays
		 * and zero the new portion of each array.
		 */
		(void) memcpy(newofile, fdp->fd_ofiles,
			(i = sizeof(struct file *) * fdp->fd_nfiles));
		(void) memset((char *)newofile + i, 0,
			nfiles * sizeof(struct file *) - i);
		(void) memcpy(newofileflags, fdp->fd_ofileflags,
			(i = sizeof(char) * fdp->fd_nfiles));
		(void) memset(newofileflags + i, 0, nfiles * sizeof(char) - i);

		if (fdp->fd_nfiles > NDFILE)
			FREE(fdp->fd_ofiles, M_FILEDESC);

		fdp->fd_ofiles = newofile;
		fdp->fd_ofileflags = newofileflags;
		fdp->fd_nfiles = nfiles;
	}
}

/*
 * Check to see whether n user file descriptors
 * are available to the process p.
 */
int
fdavail(struct proc *p, int n)
{
	struct filedesc *fdp = p->p_fd;
	struct file **fpp;
	int i;

	if ((i = p->p_rlimit[RLIMIT_OFILE].rlim_cur - fdp->fd_nfiles) > 0 &&
	    (n -= i) <= 0)
		return (1);

	fpp = &fdp->fd_ofiles[fdp->fd_freefile];
	for (i = fdp->fd_nfiles - fdp->fd_freefile; --i >= 0; fpp++)
		if (*fpp == NULL && --n <= 0)
			return (1);
	return (0);
}

/*
 * Create a new open file structure and allocate
 * a file descriptor for the process that refers to it.
 */
int
falloc(struct proc *p, struct file **resultfp, int *resultfd)
{
	struct file *fp, *fq, **fpp;
	int error, i;

	if (error = fdalloc(p, 0, &i))
		return (error);
	if (nfiles >= maxfiles) {
		tablefull("file");
		return (ENFILE);
	}

	/*
	 * Allocate a new file descriptor.
	 * If the process has file descriptor zero open, add to the list
	 * of open files at that point, otherwise put it at the front of
	 * the list of open files.
	 */
	nfiles++;
	MALLOC(fp, struct file *, sizeof(struct file), M_FILE, M_WAITOK);
	if (fq = p->p_fd->fd_ofiles[0])
		fpp = &fq->f_filef;
	else
		fpp = &filehead;
	p->p_fd->fd_ofiles[i] = fp;
	if (fq = *fpp)
		fq->f_fileb = &fp->f_filef;
	fp->f_filef = fq;
	fp->f_fileb = fpp;
	*fpp = fp;

	fp->f_count = 1;
	fp->f_msgcount = 0;
	fp->f_offset = 0;
	fp->f_cred = p->p_ucred;
	crhold(fp->f_cred);

	if (resultfp)
		*resultfp = fp;
	if (resultfd)
		*resultfd = i;
	return (0);
}

/*
 * Free a file instance, recovering resources.
 */
void
ffree(struct file *fp)
{
	struct file *fq;

	/* remove from global file list */
	if (fq = fp->f_filef)
		fq->f_fileb = fp->f_fileb;
	*fp->f_fileb = fq;

#ifdef DIAGNOSTIC
	fp->f_filef = NULL;
	fp->f_fileb = NULL;
	fp->f_count = 0;
#endif

	crfree(fp->f_cred);
	nfiles--;
	FREE(fp, M_FILE);
}

/*
 * Copy a filedesc structure.
 */
struct filedesc *
fdcopy(struct proc *p)
{
	struct filedesc *newfdp, *fdp = p->p_fd;
	struct file **fpp;
	int i;

	MALLOC(newfdp, struct filedesc *, sizeof(struct filedesc0),
	    M_FILEDESC, M_WAITOK);
	(void) memcpy(newfdp, fdp, sizeof(struct filedesc));
	newfdp->fd_refcnt = 1;

	VREF(newfdp->fd_cdir);
	if (newfdp->fd_rdir)
		VREF(newfdp->fd_rdir);

	/*
	 * If the number of open files fits in the internal arrays
	 * of the open file structure, use them, otherwise allocate
	 * additional memory for the number of descriptors currently
	 * in use.
	 */
	if (newfdp->fd_lastfile < NDFILE) {
		newfdp->fd_ofiles = ((struct filedesc0 *) newfdp)->fd_dfiles;
		newfdp->fd_ofileflags =
		    ((struct filedesc0 *) newfdp)->fd_dfileflags;
		i = NDFILE;
	} else {
		/*
		 * Compute the smallest multiple of NDEXTENT needed
		 * for the file descriptors currently in use,
		 * allowing the table to shrink.
		 */
		i = newfdp->fd_nfiles;
		while (i > 2 * NDEXTENT && i >= newfdp->fd_lastfile * 2)
			i /= 2;
		MALLOC(newfdp->fd_ofiles, struct file **, i * OFILESIZE,
		    M_FILEDESC, M_WAITOK);
		newfdp->fd_ofileflags = (char *) &newfdp->fd_ofiles[i];
	}

	newfdp->fd_nfiles = i;
	(void) memcpy(newfdp->fd_ofiles, fdp->fd_ofiles, i * sizeof(struct file **));
	(void) memcpy(newfdp->fd_ofileflags, fdp->fd_ofileflags, i * sizeof(char));
	fpp = newfdp->fd_ofiles;
	for (i = newfdp->fd_lastfile; i-- >= 0; fpp++)
		if (*fpp != NULL)
			(*fpp)->f_count++;

	return (newfdp);
}

/*
 * Release a filedesc structure.
 */
void
fdfree(struct proc *p)
{
	register struct filedesc *fdp = p->p_fd;
	struct file **fpp;
	char *fdfp;
	register int i;

	if (--fdp->fd_refcnt > 0)
		return;

	fpp = fdp->fd_ofiles;
	fdfp = fdp->fd_ofileflags;
	for (i = 0; i <= fdp->fd_lastfile; i++, fpp++, fdfp++)
		if (*fpp != NULL) {
			if (*fdfp & UF_MAPPED)
				(void) munmapfd(p, i);
			(void) closef(*fpp, p);
		}

	if (fdp->fd_nfiles > NDFILE)
		FREE(fdp->fd_ofiles, M_FILEDESC);

	vrele(fdp->fd_cdir);
	if (fdp->fd_rdir)
		vrele(fdp->fd_rdir);

	FREE(fdp, M_FILEDESC);
}

/* Close any files on execve()? */
void
fdcloseexec(struct proc *p)
{
	struct filedesc *fdp = p->p_fd;
	struct file **fpp;
	char *fdfp;
	register int i;

	fpp = fdp->fd_ofiles;
	fdfp = fdp->fd_ofileflags;
	for (i = 0; i <= fdp->fd_lastfile; i++, fpp++, fdfp++)
		if (*fpp != NULL && (*fdfp & UF_EXCLOSE)) {
			if (*fdfp & UF_MAPPED)
				(void) munmapfd(p, i);
			(void) closef(*fpp, p);
			*fpp = NULL;
			*fdfp = 0;
			if (i < fdp->fd_freefile)
				fdp->fd_freefile = i;
		}

	while (fdp->fd_lastfile > 0 && fdp->fd_ofiles[fdp->fd_lastfile] == NULL)
		fdp->fd_lastfile--;
}

/*
 * Private file close function.
 * Decrement reference count on file structure.
 */
int
closef(struct file *fp, struct proc *p)
{
	struct vnode *vp;
	struct flock lf;
	int error;

	if (fp == NULL)
		return (0);
	/*
	 * POSIX record locking dictates that any close releases ALL
	 * locks owned by this process.  This is handled by setting
	 * a flag in the unlock to free ONLY locks obeying POSIX
	 * semantics, and not to free BSD-style file locks.
	 */
	if ((p->p_flag & SADVLCK) && fp->f_type == DTYPE_VNODE) {
		lf.l_whence = SEEK_SET;
		lf.l_start = 0;
		lf.l_len = 0;
		lf.l_type = F_UNLCK;
		vp = (struct vnode *)fp->f_data;
		(void) VOP_ADVLOCK(vp, (caddr_t)p, F_UNLCK, &lf, F_POSIX);
	}
	if (--fp->f_count > 0)
		return (0);

#ifdef	DIAGNOSTIC
	if (fp->f_count < 0)
		panic("closef: count < 0");
#endif
	if ((fp->f_flag & FHASLOCK) && fp->f_type == DTYPE_VNODE) {
		lf.l_whence = SEEK_SET;
		lf.l_start = 0;
		lf.l_len = 0;
		lf.l_type = F_UNLCK;
		vp = (struct vnode *)fp->f_data;
		(void) VOP_ADVLOCK(vp, (caddr_t)fp, F_UNLCK, &lf, F_FLOCK);
	}

	error = (*fp->f_ops->fo_close)(fp, p);
	ffree(fp);
	return (error);
}

/*
 * Apply an advisory lock on a file descriptor.
 *
 * Just attempt to get a record lock of the requested type on
 * the entire file (l_whence = SEEK_SET, l_start = 0, l_len = 0).
 */

/* ARGSUSED */
int
flock(p, uap, retval)
	struct proc *p;
	struct args {
		int	fd;
		int	how;
	} *uap;
	int *retval;
{
	register struct filedesc *fdp = p->p_fd;
	register struct file *fp;
	struct vnode *vp;
	struct flock lf;
	int error;

	if ((unsigned)uap->fd >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[uap->fd]) == NULL)
		return (EBADF);
	if (fp->f_type != DTYPE_VNODE)
		return (EOPNOTSUPP);

	vp = (struct vnode *)fp->f_data;
	lf.l_whence = SEEK_SET;
	lf.l_start = 0;
	lf.l_len = 0;

	if (uap->how & LOCK_UN) {
		lf.l_type = F_UNLCK;
		fp->f_flag &= ~FHASLOCK;
		return (VOP_ADVLOCK(vp, (caddr_t)fp, F_UNLCK, &lf, F_FLOCK));
	}

	if (uap->how & LOCK_EX)
		lf.l_type = F_WRLCK;
	else if (uap->how & LOCK_SH)
		lf.l_type = F_RDLCK;
	else
		return (EBADF);

	fp->f_flag |= FHASLOCK;
	if (uap->how & LOCK_NB)
		return (VOP_ADVLOCK(vp, (caddr_t)fp, F_SETLK, &lf, F_FLOCK));

	return (VOP_ADVLOCK(vp, (caddr_t)fp, F_SETLK, &lf, F_FLOCK|F_WAIT));
}

/*
 * File Descriptor pseudo-device driver (/dev/fd/).
 *
 * Opening minor device N dup()s the file (if any) connected to file
 * descriptor N belonging to the calling process.  Note that this driver
 * consists of only the ``open()'' routine, because all subsequent
 * references to this file will be directed to the other file.
 */
/* ARGSUSED */
static int
fdopen(dev_t dev, int mode, int type, struct proc *p)
{

	/*
	 * XXX Kludge: set curproc->p_dupfd to contain the value of the
	 * the file descriptor being sought for duplication. The error 
	 * return ensures that the vnode for this device will be released
	 * by vn_open. Open will detect this special error and take the
	 * actions in dupfdopen below. Other callers of vn_open or VOP_OPEN
	 * will simply report the error.
	 */
	p->p_dupfd = minor(dev);		/* XXX */
	return (ENODEV);
}

static struct devif fd_devif =
{
	{0}, -1, -2, 0, 0, 0, 0, 0, 0,
	fdopen, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0,
	0,  0, 
};

DRIVER_MODCONFIG() {
	char *cfg_string = "fdesc 53.";
	
	if (devif_config(&cfg_string, &fd_devif) == 0)
		return;
}

/*
 * Duplicate the specified descriptor to a free descriptor.
 */
int
dupfdopen(struct filedesc *fdp, int indx, int dfd, int mode)
{
	register struct file *wfp;
	struct file *fp;
	
	/*
	 * If the to-be-dup'd fd number is greater than the allowed number
	 * of file descriptors, or the fd to be dup'd has already been
	 * closed, reject.  Note, check for new == old is necessary as
	 * falloc could allocate an already closed to-be-dup'd descriptor
	 * as the new descriptor.
	 */
	fp = fdp->fd_ofiles[indx];
	if ((u_int)dfd >= fdp->fd_nfiles ||
	    (wfp = fdp->fd_ofiles[dfd]) == NULL || fp == wfp)
		return (EBADF);

	/*
	 * Check that the mode the file is being opened for is a subset 
	 * of the mode of the existing descriptor.
	 */
	if (((mode & (FREAD|FWRITE)) | wfp->f_flag) != wfp->f_flag)
		return (EACCES);
	fdp->fd_ofiles[indx] = wfp;
	fdp->fd_ofileflags[indx] = fdp->fd_ofileflags[dfd];
	wfp->f_count++;
	if (indx > fdp->fd_lastfile)
		fdp->fd_lastfile = indx;
	return (0);
}
