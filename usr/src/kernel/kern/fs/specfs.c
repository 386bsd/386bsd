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
 * $Id: specfs.c,v 1.1 94/10/19 17:09:23 bill Exp Locker: bill $
 */

#include "sys/param.h"
#include "sys/file.h"
#include "sys/mount.h"
#include "sys/stat.h"
#include "sys/errno.h"
#include "sys/ioctl.h"
#include "uio.h"
#include "proc.h"
#include "systm.h"
#include "malloc.h"
#include "buf.h"
#include "modconfig.h"

#include "vnode.h"
#include "namei.h"
#include "specdev.h"
#include "dkbad.h"	/* XXX */
#include "disklabel.h"

#include "prototypes.h"

/* symbolic sleep message strings for devices */
char	devopn[] = "devopn";
char	devio[] = "devio";
char	devwait[] = "devwait";
char	devin[] = "devin";
char	devout[] = "devout";
char	devioc[] = "devioc";
char	devcls[] = "devcls";

struct vnodeops spec_vnodeops = {
	spec_lookup,		/* lookup */
	spec_create,		/* create */
	spec_mknod,		/* mknod */
	spec_open,		/* open */
	spec_close,		/* close */
	spec_access,		/* access */
	spec_getattr,		/* getattr */
	spec_setattr,		/* setattr */
	spec_read,		/* read */
	spec_write,		/* write */
	spec_ioctl,		/* ioctl */
	spec_select,		/* select */
	spec_mmap,		/* mmap */
	spec_fsync,		/* fsync */
	spec_seek,		/* seek */
	spec_remove,		/* remove */
	spec_link,		/* link */
	spec_rename,		/* rename */
	spec_mkdir,		/* mkdir */
	spec_rmdir,		/* rmdir */
	spec_symlink,		/* symlink */
	spec_readdir,		/* readdir */
	spec_readlink,		/* readlink */
	spec_abortop,		/* abortop */
	spec_inactive,		/* inactive */
	spec_reclaim,		/* reclaim */
	spec_lock,		/* lock */
	spec_unlock,		/* unlock */
	spec_bmap,		/* bmap */
	spec_strategy,		/* strategy */
	spec_print,		/* print */
	spec_islocked,		/* islocked */
	spec_advlock,		/* advlock */
};

struct vnode *speclisth[SPECHSZ];

/* XXX -- belongs in spec_node, spec_vfsops, etc, when moves to ./specfs */
/*
 * Create a vnode for a block device.
 * Used for root filesystem, argdev, and swap areas.
 * Also used for memory file system special devices.
 */
int
bdevvp(dev_t dev, struct vnode **vpp)
{
	register struct vnode *vp;
	struct vnode *nvp;
	int error;

	if (dev == BLK_NODEV)
		return (0);
	error = getnewvnode(VT_NON, (struct mount *)0, &spec_vnodeops, &nvp);
	if (error) {
		*vpp = 0;
		return (error);
	}
	vp = nvp;
	vp->v_type = VBLK;
	if (nvp = checkalias(vp, dev, (struct mount *)0)) {
		vput(vp);
		vp = nvp;
	}
	*vpp = vp;
	return (0);
}

/*
 * Check to see if the new vnode represents a special device
 * for which we already have a vnode (either because of
 * bdevvp() or because of a different vnode representing
 * the same block device). If such an alias exists, deallocate
 * the existing contents and return the aliased vnode. The
 * caller is responsible for filling it with its new contents.
 */
struct vnode *
checkalias(struct vnode *nvp, dev_t nvp_rdev, struct mount *mp)
{
	struct vnode *vp;
	struct vnode **vpp;

	if (nvp->v_type != VBLK && nvp->v_type != VCHR)
		return (NULLVP);

	vpp = &speclisth[SPECHASH(nvp_rdev)];
loop:
	for (vp = *vpp; vp; vp = vp->v_specnext) {
		if (nvp_rdev != vp->v_rdev || nvp->v_type != vp->v_type)
			continue;
		/*
		 * Alias, but not in use, so flush it out.
		 */
		if (vp->v_usecount == 0) {
			vgone(vp);
			goto loop;
		}
		if (vget(vp))
			goto loop;
		break;
	}
	if (vp == NULL || vp->v_tag != VT_NON) {
		MALLOC(nvp->v_specinfo, struct specinfo *,
			sizeof(struct specinfo), M_VNODE, M_WAITOK);
		nvp->v_rdev = nvp_rdev;
		nvp->v_hashchain = vpp;
		nvp->v_specnext = *vpp;
		nvp->v_specflags = 0;
		*vpp = nvp;
		if (vp != NULL) {
			nvp->v_flag |= VALIASED;
			vp->v_flag |= VALIASED;
			vput(vp);
		}
		return (NULLVP);
	}
	VOP_UNLOCK(vp);
	vclean(vp, 0);
	vp->v_op = nvp->v_op;
	vp->v_tag = nvp->v_tag;
	nvp->v_type = VNON;
	insmntque(vp, mp);
	return (vp);
}

void
spec_anonymous(struct vnode *vp) {
	vclean(vp, 0);
	vp->v_op = &spec_vnodeops;
	insmntque(vp, (struct mount *)0);
}

/* remove any aliases of the device */
void
spec_remove_aliases(struct vnode *vp) {
	struct vnode *vq;

	while (vp->v_flag & VALIASED) {
		for (vq = *vp->v_hashchain; vq; vq = vq->v_specnext) {
			if (vq->v_rdev != vp->v_rdev ||
			    vq->v_type != vp->v_type || vp == vq)
				continue;
			vgone(vq);
			break;
		}
	}
}

/* remove special device vnode from alias list. */
void
spec_remove_(struct vnode *vp)
{
	struct vnode *vq, *vx;
	long count;

	/* remove device */
	if (*vp->v_hashchain == vp) {
		*vp->v_hashchain = vp->v_specnext;
	} else {
		for (vq = *vp->v_hashchain; vq; vq = vq->v_specnext) {
			if (vq->v_specnext != vp)
				continue;
			vq->v_specnext = vp->v_specnext;
			break;
		}
		/* device not found in hash */
		if (vq == NULL)
			panic("spec_remove: device");
	}

	/* remove device's aliases */
	if (vp->v_flag & VALIASED) {
		count = 0;
		for (vq = *vp->v_hashchain; vq; vq = vq->v_specnext) {
			if (vq->v_rdev != vp->v_rdev ||
			    vq->v_type != vp->v_type)
				continue;
			count++;
			vx = vq;
		}
		/* device's alias is missing */
		if (count == 0)
			panic("spec_remove: alias");
		if (count == 1)
			vx->v_flag &= ~VALIASED;
		vp->v_flag &= ~VALIASED;
	}
	FREE(vp->v_specinfo, M_VNODE);
	vp->v_specinfo = NULL;
}

/* locate a vnode in the special device list by device number and type. */
int
spec_find(dev_t dev, enum vtype type, struct vnode **vpp)
{
	struct vnode *vp;

	for (vp = speclisth[SPECHASH(dev)]; vp; vp = vp->v_specnext) {
		if (dev != vp->v_rdev || type != vp->v_type)
			continue;
		*vpp = vp;
		return (0);
	}
	return (1);
}

/*
 * Calculate the total number of references to a special device.
 */
int
spec_count(struct vnode *vp)
{
	struct vnode *vq;
	int count;

	if ((vp->v_flag & VALIASED) == 0)
		return (vp->v_usecount);
loop:
	for (count = 0, vq = *vp->v_hashchain; vq; vq = vq->v_specnext) {
		if (vq->v_rdev != vp->v_rdev || vq->v_type != vp->v_type)
			continue;
		/*
		 * Alias, but not in use, so flush it out.
		 */
		if (vq->v_usecount == 0) {
			vgone(vq);
			goto loop;
		}
		count += vq->v_usecount;
	}
	return (count);
}

/* Check to see if a filesystem is mounted on a block device. */
int
mountedon(struct vnode *vp)
{
	struct vnode *vq;

	if (vp->v_specflags & SI_MOUNTEDON)
		return (EBUSY);
	if (vp->v_flag & VALIASED) {
		for (vq = *vp->v_hashchain; vq; vq = vq->v_specnext) {
			if (vq->v_rdev != vp->v_rdev ||
			    vq->v_type != vp->v_type)
				continue;
			if (vq->v_specflags & SI_MOUNTEDON)
				return (EBUSY);
		}
	}
	return (0);
}

/* XXX */

/*
 * Trivial lookup routine that always fails.
 */
spec_lookup(vp, ndp, p)
	struct vnode *vp;
	struct nameidata *ndp;
	struct proc *p;
{

	ndp->ni_dvp = vp;
	ndp->ni_vp = NULL;
	return (ENOTDIR);
}

/*
 * Open a special file: Don't allow open if fs is mounted -nodev,
 * and don't allow opens of block devices that are currently mounted.
 * Otherwise, call device driver open function.
 */
/* ARGSUSED */
spec_open(vp, mode, cred, p)
	register struct vnode *vp;
	int mode;
	struct ucred *cred;
	struct proc *p;
{
	dev_t dev = (dev_t)vp->v_rdev;
	/* register int maj = major(dev); */
	int error;

	if (vp->v_mount && (vp->v_mount->mnt_flag & MNT_NODEV))
		return (ENXIO);

	switch (vp->v_type) {

	case VCHR:
		/* if ((u_int)maj >= nchrdev)
			return (ENXIO); */
		VOP_UNLOCK(vp);
		error = devif_open(dev, CHRDEV, mode, p);
		/* error = (*cdevsw[maj].d_open)(dev, mode, S_IFCHR, p);*/
		VOP_LOCK(vp);
		return (error);

	case VBLK:
		/* if ((u_int)maj >= nblkdev)
			return (ENXIO); */
		if (error = mountedon(vp))
			return (error);

		/* if a mount, must be exclusive use and invalidate cache. */
		if ((mode & FMOUNT) != 0) {
			if (spec_count(vp) > 1)
				return (EBUSY);
			vinvalbuf(vp, 1);
		}

		return (devif_open(dev, BLKDEV, mode, p));
		/*return ((*bdevsw[maj].d_open)(dev, mode, S_IFBLK, p));*/
	}
	return (0);
}

/*
 * Vnode op for read
 */
/* ARGSUSED */
spec_read(vp, uio, ioflag, cred)
	register struct vnode *vp;
	register struct uio *uio;
	int ioflag;
	struct ucred *cred;
{
	struct proc *p = uio->uio_procp;
	struct buf *bp;
	daddr_t bn;
	long bsize, bscale;
	struct partinfo dpart;
	register int n, on;
	int error = 0;
	extern int mem_no;

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_READ)
		panic("spec_read mode");
	if (uio->uio_segflg == UIO_USERSPACE && uio->uio_procp != curproc)
		panic("spec_read proc");
#endif
	if (uio->uio_resid == 0)
		return (0);

	switch (vp->v_type) {

	case VCHR:
		/*
		 * Negative offsets allowed only for /dev/kmem
		 */
		if (uio->uio_offset < 0 && major(vp->v_rdev) != mem_no)
			return (EINVAL);
		VOP_UNLOCK(vp);
		error = devif_read(vp->v_rdev, CHRDEV, uio, ioflag);
		/*error = (*cdevsw[major(vp->v_rdev)].d_read)
			(vp->v_rdev, uio, ioflag);*/
		VOP_LOCK(vp);
		return (error);

	case VBLK:
		if (uio->uio_offset < 0)
			return (EINVAL);
		bsize = BLKDEV_IOSIZE;
		if (devif_ioctl(vp->v_rdev, BLKDEV, DIOCGPART,
		    (caddr_t)&dpart, FREAD, p) == 0) {
		/* if ((*bdevsw[major(vp->v_rdev)].d_ioctl)(vp->v_rdev, DIOCGPART,
		    (caddr_t)&dpart, FREAD, p) == 0) { */
			if (dpart.part->p_fstype == FS_BSDFFS &&
			    dpart.part->p_frag != 0 && dpart.part->p_fsize != 0)
				bsize = dpart.part->p_frag *
				    dpart.part->p_fsize;
		}
		bscale = bsize / DEV_BSIZE;
		do {
			bn = (uio->uio_offset / DEV_BSIZE) &~ (bscale - 1);
			on = uio->uio_offset % bsize;
			n = min((unsigned)(bsize - on), uio->uio_resid);
			if (vp->v_lastr + bscale == bn)
				error = breada(vp, bn, (int)bsize, bn + bscale,
					(int)bsize, NOCRED, &bp);
			else
				error = bread(vp, bn, (int)bsize, NOCRED, &bp);
			vp->v_lastr = bn;
			n = min(n, bsize - bp->b_resid);
			if (error) {
				brelse(bp);
				return (error);
			}
			error = uiomove(bp->b_un.b_addr + on, n, uio);
			if (n + on == bsize)
				bp->b_flags |= B_AGE;
			brelse(bp);
		} while (error == 0 && uio->uio_resid > 0 && n != 0);
		return (error);

	default:
		panic("spec_read type");
	}
	/* NOTREACHED */
}

/*
 * Vnode op for write
 */
/* ARGSUSED */
spec_write(vp, uio, ioflag, cred)
	register struct vnode *vp;
	register struct uio *uio;
	int ioflag;
	struct ucred *cred;
{
	struct proc *p = uio->uio_procp;
	struct buf *bp;
	daddr_t bn;
	int bsize, blkmask;
	struct partinfo dpart;
	register int n, on;
	int error = 0;
	extern int mem_no;

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_WRITE)
		panic("spec_write mode");
	if (uio->uio_segflg == UIO_USERSPACE && uio->uio_procp != curproc)
		panic("spec_write proc");
#endif

	switch (vp->v_type) {

	case VCHR:
		/*
		 * Negative offsets allowed only for /dev/kmem
		 */
		if (uio->uio_offset < 0 && major(vp->v_rdev) != mem_no)
			return (EINVAL);
		VOP_UNLOCK(vp);
		error = devif_write(vp->v_rdev, CHRDEV, uio, ioflag);
		/* error = (*cdevsw[major(vp->v_rdev)].d_write)
			(vp->v_rdev, uio, ioflag); */
		VOP_LOCK(vp);
		return (error);

	case VBLK:
		if (uio->uio_resid == 0)
			return (0);
		if (uio->uio_offset < 0)
			return (EINVAL);
		bsize = BLKDEV_IOSIZE;
		if (devif_ioctl(vp->v_rdev, BLKDEV, DIOCGPART,
		    (caddr_t)&dpart, FREAD, p) == 0) {
		/* if ((*bdevsw[major(vp->v_rdev)].d_ioctl)(vp->v_rdev, DIOCGPART,
		    (caddr_t)&dpart, FREAD, p) == 0) { */
			if (dpart.part->p_fstype == FS_BSDFFS &&
			    dpart.part->p_frag != 0 && dpart.part->p_fsize != 0)
				bsize = dpart.part->p_frag *
				    dpart.part->p_fsize;
		}
		blkmask = (bsize / DEV_BSIZE) - 1;
		do {
			bn = (uio->uio_offset / DEV_BSIZE) &~ blkmask;
			on = uio->uio_offset % bsize;
			n = min((unsigned)(bsize - on), uio->uio_resid);
			if (n == bsize)
				bp = getblk(vp, bn, bsize);
			else
				error = bread(vp, bn, bsize, NOCRED, &bp);
			n = min(n, bsize - bp->b_resid);
			if (error) {
				brelse(bp);
				return (error);
			}
			error = uiomove(bp->b_un.b_addr + on, n, uio);
			if (n + on == bsize) {
				bp->b_flags |= B_AGE;
				bawrite(bp);
			} else
				bdwrite(bp, vp);
		} while (error == 0 && uio->uio_resid > 0 && n != 0);
		return (error);

	default:
		panic("spec_write type");
	}
	/* NOTREACHED */
}

/*
 * Device ioctl operation.
 */
/* ARGSUSED */
spec_ioctl(vp, com, data, fflag, cred, p)
	struct vnode *vp;
	int com;
	caddr_t data;
	int fflag;
	struct ucred *cred;
	struct proc *p;
{
	dev_t dev = vp->v_rdev;

	switch (vp->v_type) {

	case VCHR:
		return (devif_ioctl(dev, CHRDEV, com, data, fflag, p));
		/*return ((*cdevsw[major(dev)].d_ioctl)(dev, com, data,
		    fflag, p));*/

	case VBLK:
		/* internal "ioctl" to sense flags to discover if a tape */
		/* if (com == 0 && (int)data == B_TAPE)
			if (bdevsw[major(dev)].d_flags & B_TAPE)
				return (0);
			else
				return (1); */
		return (devif_ioctl(dev, BLKDEV, com, data, fflag, p));
		/* return ((*bdevsw[major(dev)].d_ioctl)(dev, com, data,
		   fflag, p)); */

	default:
		panic("spec_ioctl");
		/* NOTREACHED */
	}
}

/* ARGSUSED */
spec_select(vp, which, fflags, cred, p)
	struct vnode *vp;
	int which, fflags;
	struct ucred *cred;
	struct proc *p;
{
	register dev_t dev;

	switch (vp->v_type) {

	default:
		return (1);		/* XXX */

	case VCHR:
		dev = vp->v_rdev;
		return (devif_select(dev, CHRDEV, which, p));
		/* return (*cdevsw[major(dev)].d_select)(dev, which, p);*/
	}
}

/*
 * Just call the device strategy routine
 */
spec_strategy(bp)
	register struct buf *bp;
{

	(void)devif_strategy(BLKDEV, bp);
	/* (*bdevsw[major(bp->b_dev)].d_strategy)(bp); */
	return (0);
}

/*
 * This is a noop, simply returning what one has been given.
 */
spec_bmap(vp, bn, vpp, bnp)
	struct vnode *vp;
	daddr_t bn;
	struct vnode **vpp;
	daddr_t *bnp;
{

	if (vpp != NULL)
		*vpp = vp;
	if (bnp != NULL)
		*bnp = bn;
	return (0);
}

/*
 * At the moment we do not do any locking.
 */
/* ARGSUSED */
spec_lock(vp)
	struct vnode *vp;
{

	return (0);
}

/* ARGSUSED */
spec_unlock(vp)
	struct vnode *vp;
{

	return (0);
}

/*
 * Device close routine
 */
/* ARGSUSED */
spec_close(vp, flag, cred, p)
	register struct vnode *vp;
	int flag;
	struct ucred *cred;
	struct proc *p;
{
	dev_t dev = vp->v_rdev;
	/*int (*devclose) __P((dev_t, int, int, struct proc *));*/
	devif_type_t typ;
	int mode;

	switch (vp->v_type) {

	case VCHR:
		/*
		 * If the vnode is locked, then we are in the midst
		 * of forcably closing the device, otherwise we only
		 * close on last reference.
		 */
		if (spec_count(vp) > 1 && (vp->v_flag & VXLOCK) == 0)
			return (0);
		/* devclose = cdevsw[major(dev)].d_close; */
		mode = S_IFCHR;
		typ = CHRDEV;
		break;

	case VBLK:
		/*
		 * On last close of a block device (that isn't mounted)
		 * we must invalidate any in core blocks, so that
		 * we can, for instance, change floppy disks.
		 */
		vflushbuf(vp, 0, 0);
		if (vinvalbuf(vp, 1))
			return (0);
		/*
		 * We do not want to really close the device if it
		 * is still in use unless we are trying to close it
		 * forcibly. Since every use (buffer, vnode, swap, cmap)
		 * holds a reference to the vnode, and because we mark
		 * any other vnodes that alias this device, when the
		 * sum of the reference counts on all the aliased
		 * vnodes descends to one, we are on last close.
		 */
		if (spec_count(vp) > 1 && (vp->v_flag & VXLOCK) == 0)
			return (0);
		/* devclose = bdevsw[major(dev)].d_close; */
		mode = S_IFBLK;
		typ = BLKDEV;
		break;

	default:
		panic("spec_close: not special");
	}

	return (devif_close(dev, typ, flag, p));
	/* return ((*devclose)(dev, flag, mode, p)); */
}

/*
 * Print out the contents of a special device vnode.
 */
spec_print(vp)
	struct vnode *vp;
{

	printf("tag VT_NON, dev %d, %d\n", major(vp->v_rdev),
		minor(vp->v_rdev));
}

/*
 * Special device advisory byte-level locks.
 */
/* ARGSUSED */
spec_advlock(vp, id, op, fl, flags)
	struct vnode *vp;
	caddr_t id;
	int op;
	struct flock *fl;
	int flags;
{

	return (EOPNOTSUPP);
}

/*
 * Special device failed operation
 */
spec_ebadf()
{

	return (EBADF);
}

/*
 * Special device bad operation
 */
spec_badop()
{

	panic("spec_badop called");
	/* NOTREACHED */
}
