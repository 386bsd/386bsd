
/*
 * Copyright (c) 1989 The Regents of the University of California.
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
 *	@(#)vfs_syscalls.c	7.74 (Berkeley) 6/21/91
 */

#include "param.h"
#include "systm.h"
#include "namei.h"
#include "filedesc.h"
#include "kernel.h"
#include "file.h"
#include "stat.h"
#include "vnode.h"
#include "mount.h"
#include "proc.h"
#include "uio.h"
#include "malloc.h"

/*
 * Virtual File System System Calls
 */

/*
 * Mount system call.
 */
/* ARGSUSED */
mount(p, uap, retval)
	struct proc *p;
	register struct args {
		int	type;
		char	*dir;
		int	flags;
		caddr_t	data;
	} *uap;
	int *retval;
{
	register struct nameidata *ndp;
	register struct vnode *vp;
	register struct mount *mp;
	int error, flag;
	struct nameidata nd;

	/*
	 * Must be super user
	 */
	if (error = suser(p->p_ucred, &p->p_acflag))
		return (error);
	/*
	 * Get vnode to be covered
	 */
	ndp = &nd;
	ndp->ni_nameiop = LOOKUP | FOLLOW | LOCKLEAF;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->dir;
	if (error = namei(ndp, p))
		return (error);
	vp = ndp->ni_vp;
	if (uap->flags & MNT_UPDATE) {
		if ((vp->v_flag & VROOT) == 0) {
			vput(vp);
			return (EINVAL);
		}
		mp = vp->v_mount;
		/*
		 * We allow going from read-only to read-write,
		 * but not from read-write to read-only.
		 */
		if ((mp->mnt_flag & MNT_RDONLY) == 0 &&
		    (uap->flags & MNT_RDONLY) != 0) {
			vput(vp);
			return (EOPNOTSUPP);	/* Needs translation */
		}
		flag = mp->mnt_flag;
		mp->mnt_flag |= MNT_UPDATE;
		VOP_UNLOCK(vp);
		goto update;
	}
	vinvalbuf(vp, 1);
	if (vp->v_usecount != 1) {
		vput(vp);
		return (EBUSY);
	}
	if (vp->v_type != VDIR) {
		vput(vp);
		return (ENOTDIR);
	}
	if ((unsigned long)uap->type > MOUNT_MAXTYPE ||
	    vfssw[uap->type] == (struct vfsops *)0) {
		vput(vp);
		return (ENODEV);
	}

	/*
	 * Allocate and initialize the file system.
	 */
	mp = (struct mount *)malloc((u_long)sizeof(struct mount),
		M_MOUNT, M_WAITOK);
	mp->mnt_op = vfssw[uap->type];
	mp->mnt_flag = 0;
	mp->mnt_exroot = 0;
	mp->mnt_mounth = NULLVP;
	if (error = vfs_lock(mp)) {
		free((caddr_t)mp, M_MOUNT);
		vput(vp);
		return (error);
	}
	if (vp->v_mountedhere != (struct mount *)0) {
		vfs_unlock(mp);
		free((caddr_t)mp, M_MOUNT);
		vput(vp);
		return (EBUSY);
	}
	vp->v_mountedhere = mp;
	mp->mnt_vnodecovered = vp;
update:
	/*
	 * Set the mount level flags.
	 */
	if (uap->flags & MNT_RDONLY)
		mp->mnt_flag |= MNT_RDONLY;
	else
		mp->mnt_flag &= ~MNT_RDONLY;
	if (uap->flags & MNT_NOSUID)
		mp->mnt_flag |= MNT_NOSUID;
	else
		mp->mnt_flag &= ~MNT_NOSUID;
	if (uap->flags & MNT_NOEXEC)
		mp->mnt_flag |= MNT_NOEXEC;
	else
		mp->mnt_flag &= ~MNT_NOEXEC;
	if (uap->flags & MNT_NODEV)
		mp->mnt_flag |= MNT_NODEV;
	else
		mp->mnt_flag &= ~MNT_NODEV;
	if (uap->flags & MNT_SYNCHRONOUS)
		mp->mnt_flag |= MNT_SYNCHRONOUS;
	else
		mp->mnt_flag &= ~MNT_SYNCHRONOUS;
	/*
	 * Mount the filesystem.
	 */
	error = VFS_MOUNT(mp, uap->dir, uap->data, ndp, p);
	if (mp->mnt_flag & MNT_UPDATE) {
		mp->mnt_flag &= ~MNT_UPDATE;
		vrele(vp);
		if (error)
			mp->mnt_flag = flag;
		return (error);
	}
	/*
	 * Put the new filesystem on the mount list after root.
	 */
	mp->mnt_next = rootfs->mnt_next;
	mp->mnt_prev = rootfs;
	rootfs->mnt_next = mp;
	mp->mnt_next->mnt_prev = mp;
	cache_purge(vp);
	if (!error) {
		VOP_UNLOCK(vp);
		vfs_unlock(mp);
		error = VFS_START(mp, 0, p);
	} else {
		vfs_remove(mp);
		free((caddr_t)mp, M_MOUNT);
		vput(vp);
	}
	return (error);
}

/*
 * Unmount system call.
 *
 * Note: unmount takes a path to the vnode mounted on as argument,
 * not special file (as before).
 */
/* ARGSUSED */
unmount(p, uap, retval)
	struct proc *p;
	register struct args {
		char	*pathp;
		int	flags;
	} *uap;
	int *retval;
{
	register struct vnode *vp;
	register struct nameidata *ndp;
	struct mount *mp;
	int error;
	struct nameidata nd;

	/*
	 * Must be super user
	 */
	if (error = suser(p->p_ucred, &p->p_acflag))
		return (error);

	ndp = &nd;
	ndp->ni_nameiop = LOOKUP | LOCKLEAF | FOLLOW;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->pathp;
	if (error = namei(ndp, p))
		return (error);
	vp = ndp->ni_vp;
	/*
	 * Must be the root of the filesystem
	 */
	if ((vp->v_flag & VROOT) == 0) {
		vput(vp);
		return (EINVAL);
	}
	mp = vp->v_mount;
	vput(vp);
	return (dounmount(mp, uap->flags, p));
}

/*
 * Do an unmount.
 */
dounmount(mp, flags, p)
	register struct mount *mp;
	int flags;
	struct proc *p;
{
	struct vnode *coveredvp;
	int error;

	coveredvp = mp->mnt_vnodecovered;
	if (vfs_busy(mp))
		return (EBUSY);
	mp->mnt_flag |= MNT_UNMOUNT;
	if (error = vfs_lock(mp))
		return (error);

	vnode_pager_umount(mp);	/* release cached vnodes */
	cache_purgevfs(mp);	/* remove cache entries for this file sys */
	if ((error = VFS_SYNC(mp, MNT_WAIT)) == 0 || (flags & MNT_FORCE))
		error = VFS_UNMOUNT(mp, flags, p);
	mp->mnt_flag &= ~MNT_UNMOUNT;
	vfs_unbusy(mp);
	if (error) {
		vfs_unlock(mp);
	} else {
		vrele(coveredvp);
		vfs_remove(mp);
		free((caddr_t)mp, M_MOUNT);
	}
	return (error);
}

/*
 * Sync system call.
 * Sync each mounted filesystem.
 */
/* ARGSUSED */
sync(p, uap, retval)
	struct proc *p;
	void *uap;
	int *retval;
{
	register struct mount *mp;
	struct mount *omp;

	mp = rootfs;
	do {
		/*
		 * The lock check below is to avoid races with mount
		 * and unmount.
		 */
		if ((mp->mnt_flag & (MNT_MLOCK|MNT_RDONLY|MNT_MPBUSY)) == 0 &&
		    !vfs_busy(mp)) {
			VFS_SYNC(mp, MNT_NOWAIT);
			omp = mp;
			mp = mp->mnt_next;
			vfs_unbusy(omp);
		} else
			mp = mp->mnt_next;
	} while (mp != rootfs);
	return (0);
}

/*
 * Operate on filesystem quotas.
 */
/* ARGSUSED */
quotactl(p, uap, retval)
	struct proc *p;
	register struct args {
		char *path;
		int cmd;
		int uid;
		caddr_t arg;
	} *uap;
	int *retval;
{
	register struct mount *mp;
	register struct nameidata *ndp;
	int error;
	struct nameidata nd;

	ndp = &nd;
	ndp->ni_nameiop = LOOKUP | FOLLOW;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->path;
	if (error = namei(ndp, p))
		return (error);
	mp = ndp->ni_vp->v_mount;
	vrele(ndp->ni_vp);
	return (VFS_QUOTACTL(mp, uap->cmd, uap->uid, uap->arg, p));
}

/*
 * Get filesystem statistics.
 */
/* ARGSUSED */
statfs(p, uap, retval)
	struct proc *p;
	register struct args {
		char *path;
		struct statfs *buf;
	} *uap;
	int *retval;
{
	register struct mount *mp;
	register struct nameidata *ndp;
	register struct statfs *sp;
	int error;
	struct nameidata nd;

	ndp = &nd;
	ndp->ni_nameiop = LOOKUP | FOLLOW;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->path;
	if (error = namei(ndp, p))
		return (error);
	mp = ndp->ni_vp->v_mount;
	sp = &mp->mnt_stat;
	vrele(ndp->ni_vp);
	if (error = VFS_STATFS(mp, sp, p))
		return (error);
	sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
	return (copyout((caddr_t)sp, (caddr_t)uap->buf, sizeof(*sp)));
}

/*
 * Get filesystem statistics.
 */
/* ARGSUSED */
fstatfs(p, uap, retval)
	struct proc *p;
	register struct args {
		int fd;
		struct statfs *buf;
	} *uap;
	int *retval;
{
	struct file *fp;
	struct mount *mp;
	register struct statfs *sp;
	int error;

	if (error = getvnode(p->p_fd, uap->fd, &fp))
		return (error);
	mp = ((struct vnode *)fp->f_data)->v_mount;
	sp = &mp->mnt_stat;
	if (error = VFS_STATFS(mp, sp, p))
		return (error);
	sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
	return (copyout((caddr_t)sp, (caddr_t)uap->buf, sizeof(*sp)));
}

/*
 * Get statistics on all filesystems.
 */
getfsstat(p, uap, retval)
	struct proc *p;
	register struct args {
		struct statfs *buf;
		long bufsize;
		int flags;
	} *uap;
	int *retval;
{
	register struct mount *mp;
	register struct statfs *sp;
	caddr_t sfsp;
	long count, maxcount, error;

	maxcount = uap->bufsize / sizeof(struct statfs);
	sfsp = (caddr_t)uap->buf;
	mp = rootfs;
	count = 0;
	do {
		if (sfsp && count < maxcount &&
		    ((mp->mnt_flag & MNT_MLOCK) == 0)) {
			sp = &mp->mnt_stat;
			/*
			 * If MNT_NOWAIT is specified, do not refresh the
			 * fsstat cache. MNT_WAIT overrides MNT_NOWAIT.
			 */
			if (((uap->flags & MNT_NOWAIT) == 0 ||
			    (uap->flags & MNT_WAIT)) &&
			    (error = VFS_STATFS(mp, sp, p))) {
				mp = mp->mnt_prev;
				continue;
			}
			sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
			if (error = copyout((caddr_t)sp, sfsp, sizeof(*sp)))
				return (error);
			sfsp += sizeof(*sp);
		}
		count++;
		mp = mp->mnt_prev;
	} while (mp != rootfs);
	if (sfsp && count > maxcount)
		*retval = maxcount;
	else
		*retval = count;
	return (0);
}

/*
 * Change current working directory to a given file descriptor.
 */
/* ARGSUSED */
fchdir(p, uap, retval)
	struct proc *p;
	struct args {
		int	fd;
	} *uap;
	int *retval;
{
	register struct filedesc *fdp = p->p_fd;
	register struct vnode *vp;
	struct file *fp;
	int error;

	if (error = getvnode(fdp, uap->fd, &fp))
		return (error);
	vp = (struct vnode *)fp->f_data;
	VOP_LOCK(vp);
	if (vp->v_type != VDIR)
		error = ENOTDIR;
	else
		error = VOP_ACCESS(vp, VEXEC, p->p_ucred, p);
	VOP_UNLOCK(vp);
	if (error)
		return (error);
	VREF(vp);
	vrele(fdp->fd_cdir);
	fdp->fd_cdir = vp;
	return (0);
}

/*
 * Change current working directory (``.'').
 */
/* ARGSUSED */
chdir(p, uap, retval)
	struct proc *p;
	struct args {
		char	*fname;
	} *uap;
	int *retval;
{
	register struct nameidata *ndp;
	register struct filedesc *fdp = p->p_fd;
	int error;
	struct nameidata nd;

	ndp = &nd;
	ndp->ni_nameiop = LOOKUP | FOLLOW | LOCKLEAF;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->fname;
	if (error = chdirec(ndp, p))
		return (error);
	vrele(fdp->fd_cdir);
	fdp->fd_cdir = ndp->ni_vp;
	return (0);
}

/*
 * Change notion of root (``/'') directory.
 */
/* ARGSUSED */
chroot(p, uap, retval)
	struct proc *p;
	struct args {
		char	*fname;
	} *uap;
	int *retval;
{
	register struct nameidata *ndp;
	register struct filedesc *fdp = p->p_fd;
	int error;
	struct nameidata nd;

	if (error = suser(p->p_ucred, &p->p_acflag))
		return (error);
	ndp = &nd;
	ndp->ni_nameiop = LOOKUP | FOLLOW | LOCKLEAF;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->fname;
	if (error = chdirec(ndp, p))
		return (error);
	if (fdp->fd_rdir != NULL)
		vrele(fdp->fd_rdir);
	fdp->fd_rdir = ndp->ni_vp;
	return (0);
}

/*
 * Common routine for chroot and chdir.
 */
chdirec(ndp, p)
	struct nameidata *ndp;
	struct proc *p;
{
	struct vnode *vp;
	int error;

	if (error = namei(ndp, p))
		return (error);
	vp = ndp->ni_vp;
	if (vp->v_type != VDIR)
		error = ENOTDIR;
	else
		error = VOP_ACCESS(vp, VEXEC, p->p_ucred, p);
	VOP_UNLOCK(vp);
	if (error)
		vrele(vp);
	return (error);
}

/*
 * Open system call.
 * Check permissions, allocate an open file structure,
 * and call the device open routine if any.
 */
open(p, uap, retval)
	struct proc *p;
	register struct args {
		char	*fname;
		int	mode;
		int	crtmode;
	} *uap;
	int *retval;
{
	struct nameidata *ndp;
	register struct filedesc *fdp = p->p_fd;
	register struct file *fp;
	register struct vnode *vp;
	int fmode, cmode;
	struct file *nfp;
	int type, indx, error;
	struct flock lf;
	struct nameidata nd;
	extern struct fileops vnops;

	if (error = falloc(p, &nfp, &indx))
		return (error);
	fp = nfp;
	fmode = FFLAGS(uap->mode);
	cmode = ((uap->crtmode &~ fdp->fd_cmask) & 07777) &~ S_ISVTX;
	ndp = &nd;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->fname;
	p->p_dupfd = -indx - 1;			/* XXX check for fdopen */
	if (error = vn_open(ndp, p, fmode, cmode)) {
		ffree(fp);
		if (error == ENODEV &&		/* XXX from fdopen */
		    p->p_dupfd >= 0 &&
		    (error = dupfdopen(fdp, indx, p->p_dupfd, fmode)) == 0) {
			*retval = indx;
			return (0);
		}
		if (error == ERESTART)
			error = EINTR;
		fdp->fd_ofiles[indx] = NULL;
		return (error);
	}
	vp = ndp->ni_vp;
	fp->f_flag = fmode & FMASK;
	if (fmode & (O_EXLOCK | O_SHLOCK)) {
		lf.l_whence = SEEK_SET;
		lf.l_start = 0;
		lf.l_len = 0;
		if (fmode & O_EXLOCK)
			lf.l_type = F_WRLCK;
		else
			lf.l_type = F_RDLCK;
		type = F_FLOCK;
		if ((fmode & FNONBLOCK) == 0)
			type |= F_WAIT;
		if (error = VOP_ADVLOCK(vp, (caddr_t)fp, F_SETLK, &lf, type)) {
			VOP_UNLOCK(vp);
			(void) vn_close(vp, fp->f_flag, fp->f_cred, p);
			ffree(fp);
			fdp->fd_ofiles[indx] = NULL;
			return (error);
		}
		fp->f_flag |= FHASLOCK;
	}
	VOP_UNLOCK(vp);
	fp->f_type = DTYPE_VNODE;
	fp->f_ops = &vnops;
	fp->f_data = (caddr_t)vp;
	*retval = indx;
	return (0);
}

#ifdef COMPAT_43
/*
 * Creat system call.
 */
ocreat(p, uap, retval)
	struct proc *p;
	register struct args {
		char	*fname;
		int	fmode;
	} *uap;
	int *retval;
{
	struct args {
		char	*fname;
		int	mode;
		int	crtmode;
	} openuap;

	openuap.fname = uap->fname;
	openuap.crtmode = uap->fmode;
	openuap.mode = O_WRONLY | O_CREAT | O_TRUNC;
	return (open(p, &openuap, retval));
}
#endif /* COMPAT_43 */

/*
 * Mknod system call.
 */
/* ARGSUSED */
mknod(p, uap, retval)
	struct proc *p;
	register struct args {
		char	*fname;
		int	fmode;
		int	dev;
	} *uap;
	int *retval;
{
	register struct nameidata *ndp;
	register struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;

	if (error = suser(p->p_ucred, &p->p_acflag))
		return (error);
	ndp = &nd;
	ndp->ni_nameiop = CREATE | LOCKPARENT;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->fname;
	if (error = namei(ndp, p))
		return (error);
	vp = ndp->ni_vp;
	if (vp != NULL) {
		error = EEXIST;
		goto out;
	}
	VATTR_NULL(&vattr);
	switch (uap->fmode & S_IFMT) {

	case S_IFMT:	/* used by badsect to flag bad sectors */
		vattr.va_type = VBAD;
		break;
	case S_IFCHR:
		vattr.va_type = VCHR;
		break;
	case S_IFBLK:
		vattr.va_type = VBLK;
		break;
	default:
		error = EINVAL;
		goto out;
	}
	vattr.va_mode = (uap->fmode & 07777) &~ p->p_fd->fd_cmask;
	vattr.va_rdev = uap->dev;
out:
	if (!error) {
		error = VOP_MKNOD(ndp, &vattr, p->p_ucred, p);
	} else {
		VOP_ABORTOP(ndp);
		if (ndp->ni_dvp == vp)
			vrele(ndp->ni_dvp);
		else
			vput(ndp->ni_dvp);
		if (vp)
			vrele(vp);
	}
	return (error);
}

/*
 * Mkfifo system call.
 */
/* ARGSUSED */
mkfifo(p, uap, retval)
	struct proc *p;
	register struct args {
		char	*fname;
		int	fmode;
	} *uap;
	int *retval;
{
	register struct nameidata *ndp;
	struct vattr vattr;
	int error;
	struct nameidata nd;

#ifndef FIFO
	return (EOPNOTSUPP);
#else
	ndp = &nd;
	ndp->ni_nameiop = CREATE | LOCKPARENT;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->fname;
	if (error = namei(ndp, p))
		return (error);
	if (ndp->ni_vp != NULL) {
		VOP_ABORTOP(ndp);
		if (ndp->ni_dvp == ndp->ni_vp)
			vrele(ndp->ni_dvp);
		else
			vput(ndp->ni_dvp);
		vrele(ndp->ni_vp);
		return (EEXIST);
	}
	VATTR_NULL(&vattr);
	vattr.va_type = VFIFO;
	vattr.va_mode = (uap->fmode & 07777) &~ p->p_fd->fd_cmask;
	return (VOP_MKNOD(ndp, &vattr, p->p_ucred, p));
#endif /* FIFO */
}

/*
 * Link system call.
 */
/* ARGSUSED */
link(p, uap, retval)
	struct proc *p;
	register struct args {
		char	*target;
		char	*linkname;
	} *uap;
	int *retval;
{
	register struct nameidata *ndp;
	register struct vnode *vp, *xp;
	int error;
	struct nameidata nd;

	ndp = &nd;
	ndp->ni_nameiop = LOOKUP | FOLLOW;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->target;
	if (error = namei(ndp, p))
		return (error);
	vp = ndp->ni_vp;
	if (vp->v_type == VDIR &&
	    (error = suser(p->p_ucred, &p->p_acflag)))
		goto out1;
	ndp->ni_nameiop = CREATE | LOCKPARENT;
	ndp->ni_dirp = (caddr_t)uap->linkname;
	if (error = namei(ndp, p))
		goto out1;
	xp = ndp->ni_vp;
	if (xp != NULL) {
		error = EEXIST;
		goto out;
	}
	xp = ndp->ni_dvp;
	if (vp->v_mount != xp->v_mount)
		error = EXDEV;
out:
	if (!error) {
		error = VOP_LINK(vp, ndp, p);
	} else {
		VOP_ABORTOP(ndp);
		if (ndp->ni_dvp == ndp->ni_vp)
			vrele(ndp->ni_dvp);
		else
			vput(ndp->ni_dvp);
		if (ndp->ni_vp)
			vrele(ndp->ni_vp);
	}
out1:
	vrele(vp);
	return (error);
}

/*
 * Make a symbolic link.
 */
/* ARGSUSED */
symlink(p, uap, retval)
	struct proc *p;
	register struct args {
		char	*target;
		char	*linkname;
	} *uap;
	int *retval;
{
	register struct nameidata *ndp;
	struct vattr vattr;
	char *target;
	int error;
	struct nameidata nd;

	ndp = &nd;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->linkname;
	MALLOC(target, char *, MAXPATHLEN, M_NAMEI, M_WAITOK);
	if (error = copyinstr(uap->target, target, MAXPATHLEN, (u_int *)0))
		goto out;
	ndp->ni_nameiop = CREATE | LOCKPARENT;
	if (error = namei(ndp, p))
		goto out;
	if (ndp->ni_vp) {
		VOP_ABORTOP(ndp);
		if (ndp->ni_dvp == ndp->ni_vp)
			vrele(ndp->ni_dvp);
		else
			vput(ndp->ni_dvp);
		vrele(ndp->ni_vp);
		error = EEXIST;
		goto out;
	}
	VATTR_NULL(&vattr);
	vattr.va_mode = 0777 &~ p->p_fd->fd_cmask;
	error = VOP_SYMLINK(ndp, &vattr, target, p);
out:
	FREE(target, M_NAMEI);
	return (error);
}

/*
 * Delete a name from the filesystem.
 */
/* ARGSUSED */
unlink(p, uap, retval)
	struct proc *p;
	struct args {
		char	*fname;
	} *uap;
	int *retval;
{
	register struct nameidata *ndp;
	register struct vnode *vp;
	int error;
	struct nameidata nd;

	ndp = &nd;
	ndp->ni_nameiop = DELETE | LOCKPARENT | LOCKLEAF;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->fname;
	if (error = namei(ndp, p))
		return (error);
	vp = ndp->ni_vp;
	if (vp->v_type == VDIR &&
	    (error = suser(p->p_ucred, &p->p_acflag)))
		goto out;
	/*
	 * The root of a mounted filesystem cannot be deleted.
	 */
	if (vp->v_flag & VROOT) {
		error = EBUSY;
		goto out;
	}
	(void) vnode_pager_uncache(vp);
out:
	if (!error) {
		error = VOP_REMOVE(ndp, p);
	} else {
		VOP_ABORTOP(ndp);
		if (ndp->ni_dvp == vp)
			vrele(ndp->ni_dvp);
		else
			vput(ndp->ni_dvp);
		vput(vp);
	}
	return (error);
}

/*
 * Seek system call.
 */
lseek(p, uap, retval)
	struct proc *p;
	register struct args {
		int	fdes;
		off_t	off;
		int	sbase;
	} *uap;
	off_t *retval;
{
	struct ucred *cred = p->p_ucred;
	register struct filedesc *fdp = p->p_fd;
	register struct file *fp;
	struct vattr vattr;
	int error;

	if ((unsigned)uap->fdes >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[uap->fdes]) == NULL)
		return (EBADF);
	if (fp->f_type != DTYPE_VNODE)
		return (ESPIPE);
	switch (uap->sbase) {

	case L_INCR:
		fp->f_offset += uap->off;
		break;

	case L_XTND:
		if (error = VOP_GETATTR((struct vnode *)fp->f_data,
		    &vattr, cred, p))
			return (error);
		fp->f_offset = uap->off + vattr.va_size;
		break;

	case L_SET:
		fp->f_offset = uap->off;
		break;

	default:
		return (EINVAL);
	}
	*retval = fp->f_offset;
	return (0);
}

/*
 * Check access permissions.
 */
/* ARGSUSED */
saccess(p, uap, retval)
	struct proc *p;
	register struct args {
		char	*fname;
		int	fmode;
	} *uap;
	int *retval;
{
	register struct nameidata *ndp;
	register struct ucred *cred = p->p_ucred;
	register struct vnode *vp;
	int error, mode, svuid, svgid;
	struct nameidata nd;

	ndp = &nd;
	svuid = cred->cr_uid;
	svgid = cred->cr_groups[0];
	cred->cr_uid = p->p_cred->p_ruid;
	cred->cr_groups[0] = p->p_cred->p_rgid;
	ndp->ni_nameiop = LOOKUP | FOLLOW | LOCKLEAF;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->fname;
	if (error = namei(ndp, p))
		goto out1;
	vp = ndp->ni_vp;
	/*
	 * fmode == 0 means only check for exist
	 */
	if (uap->fmode) {
		mode = 0;
		if (uap->fmode & R_OK)
			mode |= VREAD;
		if (uap->fmode & W_OK)
			mode |= VWRITE;
		if (uap->fmode & X_OK)
			mode |= VEXEC;
		if ((mode & VWRITE) == 0 || (error = vn_writechk(vp)) == 0)
			error = VOP_ACCESS(vp, mode, cred, p);
	}
	vput(vp);
out1:
	cred->cr_uid = svuid;
	cred->cr_groups[0] = svgid;
	return (error);
}

/*
 * Stat system call.
 * This version follows links.
 */
/* ARGSUSED */
stat(p, uap, retval)
	struct proc *p;
	register struct args {
		char	*fname;
		struct stat *ub;
	} *uap;
	int *retval;
{
	register struct nameidata *ndp;
	struct stat sb;
	int error;
	struct nameidata nd;

	ndp = &nd;
	ndp->ni_nameiop = LOOKUP | LOCKLEAF | FOLLOW;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->fname;
	if (error = namei(ndp, p))
		return (error);
	error = vn_stat(ndp->ni_vp, &sb, p);
	vput(ndp->ni_vp);
	if (error)
		return (error);
	error = copyout((caddr_t)&sb, (caddr_t)uap->ub, sizeof (sb));
	return (error);
}

/*
 * Lstat system call.
 * This version does not follow links.
 */
/* ARGSUSED */
lstat(p, uap, retval)
	struct proc *p;
	register struct args {
		char	*fname;
		struct stat *ub;
	} *uap;
	int *retval;
{
	register struct nameidata *ndp;
	struct stat sb;
	int error;
	struct nameidata nd;

	ndp = &nd;
	ndp->ni_nameiop = LOOKUP | LOCKLEAF | NOFOLLOW;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->fname;
	if (error = namei(ndp, p))
		return (error);
	error = vn_stat(ndp->ni_vp, &sb, p);
	vput(ndp->ni_vp);
	if (error)
		return (error);
	error = copyout((caddr_t)&sb, (caddr_t)uap->ub, sizeof (sb));
	return (error);
}

/*
 * Return target name of a symbolic link.
 */
/* ARGSUSED */
readlink(p, uap, retval)
	struct proc *p;
	register struct args {
		char	*name;
		char	*buf;
		int	count;
	} *uap;
	int *retval;
{
	register struct nameidata *ndp;
	register struct vnode *vp;
	struct iovec aiov;
	struct uio auio;
	int error;
	struct nameidata nd;

	ndp = &nd;
	ndp->ni_nameiop = LOOKUP | LOCKLEAF;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->name;
	if (error = namei(ndp, p))
		return (error);
	vp = ndp->ni_vp;
	if (vp->v_type != VLNK) {
		error = EINVAL;
		goto out;
	}
	aiov.iov_base = uap->buf;
	aiov.iov_len = uap->count;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = 0;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_procp = p;
	auio.uio_resid = uap->count;
	error = VOP_READLINK(vp, &auio, p->p_ucred);
out:
	vput(vp);
	*retval = uap->count - auio.uio_resid;
	return (error);
}

/*
 * Change flags of a file given path name.
 */
/* ARGSUSED */
chflags(p, uap, retval)
	struct proc *p;
	register struct args {
		char	*fname;
		int	flags;
	} *uap;
	int *retval;
{
	register struct nameidata *ndp;
	register struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;

	ndp = &nd;
	ndp->ni_nameiop = LOOKUP | FOLLOW | LOCKLEAF;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->fname;
	if (error = namei(ndp, p))
		return (error);
	vp = ndp->ni_vp;
	if (vp->v_mount->mnt_flag & MNT_RDONLY) {
		error = EROFS;
		goto out;
	}
	VATTR_NULL(&vattr);
	vattr.va_flags = uap->flags;
	error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
out:
	vput(vp);
	return (error);
}

/*
 * Change flags of a file given a file descriptor.
 */
/* ARGSUSED */
fchflags(p, uap, retval)
	struct proc *p;
	register struct args {
		int	fd;
		int	flags;
	} *uap;
	int *retval;
{
	struct vattr vattr;
	struct vnode *vp;
	struct file *fp;
	int error;

	if (error = getvnode(p->p_fd, uap->fd, &fp))
		return (error);
	vp = (struct vnode *)fp->f_data;
	VOP_LOCK(vp);
	if (vp->v_mount->mnt_flag & MNT_RDONLY) {
		error = EROFS;
		goto out;
	}
	VATTR_NULL(&vattr);
	vattr.va_flags = uap->flags;
	error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
out:
	VOP_UNLOCK(vp);
	return (error);
}

/*
 * Change mode of a file given path name.
 */
/* ARGSUSED */
chmod(p, uap, retval)
	struct proc *p;
	register struct args {
		char	*fname;
		int	fmode;
	} *uap;
	int *retval;
{
	register struct nameidata *ndp;
	register struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;

	ndp = &nd;
	ndp->ni_nameiop = LOOKUP | FOLLOW | LOCKLEAF;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->fname;
	if (error = namei(ndp, p))
		return (error);
	vp = ndp->ni_vp;
	if (vp->v_mount->mnt_flag & MNT_RDONLY) {
		error = EROFS;
		goto out;
	}
	VATTR_NULL(&vattr);
	vattr.va_mode = uap->fmode & 07777;
	error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
out:
	vput(vp);
	return (error);
}

/*
 * Change mode of a file given a file descriptor.
 */
/* ARGSUSED */
fchmod(p, uap, retval)
	struct proc *p;
	register struct args {
		int	fd;
		int	fmode;
	} *uap;
	int *retval;
{
	struct vattr vattr;
	struct vnode *vp;
	struct file *fp;
	int error;

	if (error = getvnode(p->p_fd, uap->fd, &fp))
		return (error);
	vp = (struct vnode *)fp->f_data;
	VOP_LOCK(vp);
	if (vp->v_mount->mnt_flag & MNT_RDONLY) {
		error = EROFS;
		goto out;
	}
	VATTR_NULL(&vattr);
	vattr.va_mode = uap->fmode & 07777;
	error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
out:
	VOP_UNLOCK(vp);
	return (error);
}

/*
 * Set ownership given a path name.
 */
/* ARGSUSED */
chown(p, uap, retval)
	struct proc *p;
	register struct args {
		char	*fname;
		int	uid;
		int	gid;
	} *uap;
	int *retval;
{
	register struct nameidata *ndp;
	register struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;

	ndp = &nd;
	ndp->ni_nameiop = LOOKUP | NOFOLLOW | LOCKLEAF;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->fname;
	if (error = namei(ndp, p))
		return (error);
	vp = ndp->ni_vp;
	if (vp->v_mount->mnt_flag & MNT_RDONLY) {
		error = EROFS;
		goto out;
	}
	VATTR_NULL(&vattr);
	vattr.va_uid = uap->uid;
	vattr.va_gid = uap->gid;
	error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
out:
	vput(vp);
	return (error);
}

/*
 * Set ownership given a file descriptor.
 */
/* ARGSUSED */
fchown(p, uap, retval)
	struct proc *p;
	register struct args {
		int	fd;
		int	uid;
		int	gid;
	} *uap;
	int *retval;
{
	struct vattr vattr;
	struct vnode *vp;
	struct file *fp;
	int error;

	if (error = getvnode(p->p_fd, uap->fd, &fp))
		return (error);
	vp = (struct vnode *)fp->f_data;
	VOP_LOCK(vp);
	if (vp->v_mount->mnt_flag & MNT_RDONLY) {
		error = EROFS;
		goto out;
	}
	VATTR_NULL(&vattr);
	vattr.va_uid = uap->uid;
	vattr.va_gid = uap->gid;
	error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
out:
	VOP_UNLOCK(vp);
	return (error);
}

/*
 * Set the access and modification times of a file.
 */
/* ARGSUSED */
utimes(p, uap, retval)
	struct proc *p;
	register struct args {
		char	*fname;
		struct	timeval *tptr;
	} *uap;
	int *retval;
{
	register struct nameidata *ndp;
	register struct vnode *vp;
	struct timeval tv[2];
	struct vattr vattr;
	int error;
	struct nameidata nd;

	if (error = copyin((caddr_t)uap->tptr, (caddr_t)tv, sizeof (tv)))
		return (error);
	ndp = &nd;
	ndp->ni_nameiop = LOOKUP | FOLLOW | LOCKLEAF;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->fname;
	if (error = namei(ndp, p))
		return (error);
	vp = ndp->ni_vp;
	if (vp->v_mount->mnt_flag & MNT_RDONLY) {
		error = EROFS;
		goto out;
	}
	VATTR_NULL(&vattr);
	vattr.va_atime = tv[0];
	vattr.va_mtime = tv[1];
	error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
out:
	vput(vp);
	return (error);
}

/*
 * Truncate a file given its path name.
 */
/* ARGSUSED */
truncate(p, uap, retval)
	struct proc *p;
	register struct args {
		char	*fname;
		off_t	length;
	} *uap;
	int *retval;
{
	register struct nameidata *ndp;
	register struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;

	ndp = &nd;
	ndp->ni_nameiop = LOOKUP | FOLLOW | LOCKLEAF;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->fname;
	if (error = namei(ndp, p))
		return (error);
	vp = ndp->ni_vp;
	if (vp->v_type == VDIR) {
		error = EISDIR;
		goto out;
	}
	if ((error = vn_writechk(vp)) ||
	    (error = VOP_ACCESS(vp, VWRITE, p->p_ucred, p)))
		goto out;
	VATTR_NULL(&vattr);
	vattr.va_size = uap->length;
	error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
out:
	vput(vp);
	return (error);
}

/*
 * Truncate a file given a file descriptor.
 */
/* ARGSUSED */
ftruncate(p, uap, retval)
	struct proc *p;
	register struct args {
		int	fd;
		off_t	length;
	} *uap;
	int *retval;
{
	struct vattr vattr;
	struct vnode *vp;
	struct file *fp;
	int error;

	if (error = getvnode(p->p_fd, uap->fd, &fp))
		return (error);
	if ((fp->f_flag & FWRITE) == 0)
		return (EINVAL);
	vp = (struct vnode *)fp->f_data;
	VOP_LOCK(vp);
	if (vp->v_type == VDIR) {
		error = EISDIR;
		goto out;
	}
	if (error = vn_writechk(vp))
		goto out;
	VATTR_NULL(&vattr);
	vattr.va_size = uap->length;
	error = VOP_SETATTR(vp, &vattr, fp->f_cred, p);
out:
	VOP_UNLOCK(vp);
	return (error);
}

/*
 * Synch an open file.
 */
/* ARGSUSED */
fsync(p, uap, retval)
	struct proc *p;
	struct args {
		int	fd;
	} *uap;
	int *retval;
{
	register struct vnode *vp;
	struct file *fp;
	int error;

	if (error = getvnode(p->p_fd, uap->fd, &fp))
		return (error);
	vp = (struct vnode *)fp->f_data;
	VOP_LOCK(vp);
	error = VOP_FSYNC(vp, fp->f_flag, fp->f_cred, MNT_WAIT, p);
	VOP_UNLOCK(vp);
	return (error);
}

/*
 * Rename system call.
 *
 * Source and destination must either both be directories, or both
 * not be directories.  If target is a directory, it must be empty.
 */
/* ARGSUSED */
rename(p, uap, retval)
	struct proc *p;
	register struct args {
		char	*from;
		char	*to;
	} *uap;
	int *retval;
{
	register struct vnode *tvp, *fvp, *tdvp;
	struct nameidata fromnd, tond;
	int error;

	fromnd.ni_nameiop = DELETE | WANTPARENT | SAVESTART;
	fromnd.ni_segflg = UIO_USERSPACE;
	fromnd.ni_dirp = uap->from;
	if (error = namei(&fromnd, p))
		return (error);
	fvp = fromnd.ni_vp;
	tond.ni_nameiop = RENAME | LOCKPARENT | LOCKLEAF | NOCACHE | SAVESTART;
	tond.ni_segflg = UIO_USERSPACE;
	tond.ni_dirp = uap->to;
	if (error = namei(&tond, p)) {
		VOP_ABORTOP(&fromnd);
		vrele(fromnd.ni_dvp);
		vrele(fvp);
		goto out1;
	}
	tdvp = tond.ni_dvp;
	tvp = tond.ni_vp;
	if (tvp != NULL) {
		if (fvp->v_type == VDIR && tvp->v_type != VDIR) {
			error = ENOTDIR;
			goto out;
		} else if (fvp->v_type != VDIR && tvp->v_type == VDIR) {
			error = EISDIR;
			goto out;
		}
		if (fvp->v_mount != tvp->v_mount) {
			error = EXDEV;
			goto out;
		}
	}
	if (fvp->v_mount != tdvp->v_mount) {
		error = EXDEV;
		goto out;
	}
	if (fvp == tdvp)
		error = EINVAL;
	/*
	 * If source is the same as the destination (that is the
	 * same inode number with the same name in the same directory),
	 * then there is nothing to do.
	 */
	if (fvp == tvp && fromnd.ni_dvp == tdvp &&
	    fromnd.ni_namelen == tond.ni_namelen &&
	    !bcmp(fromnd.ni_ptr, tond.ni_ptr, fromnd.ni_namelen))
		error = -1;
out:
	if (!error) {
		error = VOP_RENAME(&fromnd, &tond, p);
	} else {
		VOP_ABORTOP(&tond);
		if (tdvp == tvp)
			vrele(tdvp);
		else
			vput(tdvp);
		if (tvp)
			vput(tvp);
		VOP_ABORTOP(&fromnd);
		vrele(fromnd.ni_dvp);
		vrele(fvp);
	}
	vrele(tond.ni_startdir);
	FREE(tond.ni_pnbuf, M_NAMEI);
out1:
	vrele(fromnd.ni_startdir);
	FREE(fromnd.ni_pnbuf, M_NAMEI);
	if (error == -1)
		return (0);
	return (error);
}

/*
 * Mkdir system call.
 */
/* ARGSUSED */
mkdir(p, uap, retval)
	struct proc *p;
	register struct args {
		char	*name;
		int	dmode;
	} *uap;
	int *retval;
{
	register struct nameidata *ndp;
	register struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;

	ndp = &nd;
	ndp->ni_nameiop = CREATE | LOCKPARENT;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->name;
	if (error = namei(ndp, p))
		return (error);
	vp = ndp->ni_vp;
	if (vp != NULL) {
		VOP_ABORTOP(ndp);
		if (ndp->ni_dvp == vp)
			vrele(ndp->ni_dvp);
		else
			vput(ndp->ni_dvp);
		vrele(vp);
		return (EEXIST);
	}
	VATTR_NULL(&vattr);
	vattr.va_type = VDIR;
	vattr.va_mode = (uap->dmode & 0777) &~ p->p_fd->fd_cmask;
	error = VOP_MKDIR(ndp, &vattr, p);
	if (!error)
		vput(ndp->ni_vp);
	return (error);
}

/*
 * Rmdir system call.
 */
/* ARGSUSED */
rmdir(p, uap, retval)
	struct proc *p;
	struct args {
		char	*name;
	} *uap;
	int *retval;
{
	register struct nameidata *ndp;
	register struct vnode *vp;
	int error;
	struct nameidata nd;

	ndp = &nd;
	ndp->ni_nameiop = DELETE | LOCKPARENT | LOCKLEAF;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->name;
	if (error = namei(ndp, p))
		return (error);
	vp = ndp->ni_vp;
	if (vp->v_type != VDIR) {
		error = ENOTDIR;
		goto out;
	}
	/*
	 * No rmdir "." please.
	 */
	if (ndp->ni_dvp == vp) {
		error = EINVAL;
		goto out;
	}
	/*
	 * The root of a mounted filesystem cannot be deleted.
	 */
	if (vp->v_flag & VROOT)
		error = EBUSY;
out:
	if (!error) {
		error = VOP_RMDIR(ndp, p);
	} else {
		VOP_ABORTOP(ndp);
		if (ndp->ni_dvp == vp)
			vrele(ndp->ni_dvp);
		else
			vput(ndp->ni_dvp);
		vput(vp);
	}
	return (error);
}

/*
 * Read a block of directory entries in a file system independent format.
 */
getdirentries(p, uap, retval)
	struct proc *p;
	register struct args {
		int	fd;
		char	*buf;
		unsigned count;
		long	*basep;
	} *uap;
	int *retval;
{
	register struct vnode *vp;
	struct file *fp;
	struct uio auio;
	struct iovec aiov;
	off_t off;
	int error, eofflag;

	if (error = getvnode(p->p_fd, uap->fd, &fp))
		return (error);
	if ((fp->f_flag & FREAD) == 0)
		return (EBADF);
	vp = (struct vnode *)fp->f_data;
	if (vp->v_type != VDIR)
		return (EINVAL);
	aiov.iov_base = uap->buf;
	aiov.iov_len = uap->count;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_procp = p;
	auio.uio_resid = uap->count;
	VOP_LOCK(vp);
	auio.uio_offset = off = fp->f_offset;
	error = VOP_READDIR(vp, &auio, fp->f_cred, &eofflag);
	fp->f_offset = auio.uio_offset;
	VOP_UNLOCK(vp);
	if (error)
		return (error);
	error = copyout((caddr_t)&off, (caddr_t)uap->basep, sizeof(long));
	*retval = uap->count - auio.uio_resid;
	return (error);
}

/*
 * Set the mode mask for creation of filesystem nodes.
 */
mode_t
umask(p, uap, retval)
	struct proc *p;
	struct args {
		int	mask;
	} *uap;
	int *retval;
{
	register struct filedesc *fdp = p->p_fd;

	*retval = fdp->fd_cmask;
	fdp->fd_cmask = uap->mask & 07777;
	return (0);
}

/*
 * Void all references to file by ripping underlying filesystem
 * away from vnode.
 */
/* ARGSUSED */
revoke(p, uap, retval)
	struct proc *p;
	register struct args {
		char	*fname;
	} *uap;
	int *retval;
{
	register struct nameidata *ndp;
	register struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;

	ndp = &nd;
	ndp->ni_nameiop = LOOKUP | FOLLOW;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->fname;
	if (error = namei(ndp, p))
		return (error);
	vp = ndp->ni_vp;
	if (vp->v_type != VCHR && vp->v_type != VBLK) {
		error = EINVAL;
		goto out;
	}
	if (error = VOP_GETATTR(vp, &vattr, p->p_ucred, p))
		goto out;
	if (p->p_ucred->cr_uid != vattr.va_uid &&
	    (error = suser(p->p_ucred, &p->p_acflag)))
		goto out;
	if (vp->v_usecount > 1 || (vp->v_flag & VALIASED))
		vgoneall(vp);
out:
	vrele(vp);
	return (error);
}

/*
 * Convert a user file descriptor to a kernel file entry.
 */
getvnode(fdp, fdes, fpp)
	struct filedesc *fdp;
	struct file **fpp;
	int fdes;
{
	struct file *fp;

	if ((unsigned)fdes >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[fdes]) == NULL)
		return (EBADF);
	if (fp->f_type != DTYPE_VNODE)
		return (EINVAL);
	*fpp = fp;
	return (0);
}










