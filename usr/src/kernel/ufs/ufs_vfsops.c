/*
 * Copyright (c) 1989, 1991 The Regents of the University of California.
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
 *	$Id: ufs_vfsops.c,v 1.1 94/10/20 10:56:46 root Exp $
 */

static char *ufs_config = "ufs 1.";

#include "sys/param.h"
#include "sys/file.h"
#include "sys/mount.h"
#include "privilege.h"
#include "sys/ioctl.h"
#include "uio.h"
#include "sys/errno.h"
#include "systm.h"	/* nblkdev */
#include "proc.h"
#include "specdev.h"
#include "buf.h"
#include "malloc.h"
#include "modconfig.h"

#include "machine/cpu.h"	/* inittodr() */

#include "dkbad.h"	/* XXX */
#include "disklabel.h"

#include "namei.h"
#include "vnode.h"
#include "ufs_quota.h"
#include "ufs.h"
#include "ufs_mount.h"
#include "ufs_inode.h"
 
#include "prototypes.h"

static int ufs_mountroot(void);
static int ufs_mount(struct mount *mp, char *path, caddr_t data,
	struct nameidata *ndp, struct proc *p);
/* used by mfs until mfs independant of ufs */
/*static*/ int mountfs(struct vnode *devvp, struct mount *mp, struct proc *p);

static int ufs_mount(struct mount *mp, char *path, caddr_t data,
	struct nameidata *ndp, struct proc *p);
static int ufs_start(struct mount *mp, int flags, struct proc *p);
static int ufs_unmount(struct mount *mp, int mntflags, struct proc *p);
static int ufs_root(struct mount *mp, struct vnode **vpp);
static int ufs_quotactl(struct mount *mp, int cmds, uid_t uid,
	caddr_t arg, struct proc *p);
static int ufs_statfs(struct mount *mp, struct statfs *sbp, struct proc *p);
static int ufs_sync(struct mount *mp, int waitfor);
static int ufs_fhtovp(struct mount *mp, struct fid *fhp, struct vnode **vpp);
static int ufs_vptofh(struct vnode *vp, struct fid *fhp);
int ufs_init(void);

struct vfsops ufs_vfsops = {
	"ufs", 0, 0,
	ufs_mount,
	ufs_start,
	ufs_unmount,
	ufs_root,
	ufs_quotactl,
	ufs_statfs,
	ufs_sync,
	ufs_fhtovp,
	ufs_vptofh,
	ufs_init,
	ufs_mountroot
};

FILESYSTEM_MODCONFIG() {
	char *cfg_string = ufs_config;

	/* is this filesystem to be configured? */
	if (config_scan(ufs_config, &cfg_string) == 0)
		return;

	/* what type should be assigned to it */
	if (cfg_number(&cfg_string, &ufs_vfsops.vfs_type) == 0)
		return;

	addvfs(&ufs_vfsops);
}

/*
 * Called by vfs_mountroot when ufs is going to be mounted as root.
 *
 * Name is updated by mount(8) after booting.
 */
#define ROOTNAME	"root_device"

static int
ufs_mountroot(void)
{
	register struct mount *mp;
	extern struct vnode *rootvp;
	struct proc *p = curproc;	/* XXX */
	struct ufsmount *ump;
	register struct fs *fs;
	u_int size;
	int error;

	mp = (struct mount *)malloc((u_long)sizeof(struct mount),
		M_MOUNT, M_WAITOK);
	mp->mnt_op = &ufs_vfsops;
	mp->mnt_flag = MNT_RDONLY;
	mp->mnt_exroot = 0;
	mp->mnt_mounth = NULLVP;
	error = mountfs(rootvp, mp, p);
	if (error) {
		free((caddr_t)mp, M_MOUNT);
		return (error);
	}
	if (error = vfs_lock(mp)) {
		(void)ufs_unmount(mp, 0, p);
		free((caddr_t)mp, M_MOUNT);
		return (error);
	}
	rootfs = mp;
	mp->mnt_next = mp;
	mp->mnt_prev = mp;
	mp->mnt_vnodecovered = NULLVP;
	ump = VFSTOUFS(mp);
	fs = ump->um_fs;
	memset(fs->fs_fsmnt, 0, sizeof(fs->fs_fsmnt));
	fs->fs_fsmnt[0] = '/';
	memcpy((caddr_t)mp->mnt_stat.f_mntonname, (caddr_t)fs->fs_fsmnt,
	    MNAMELEN);
	(void) copystr(ROOTNAME, mp->mnt_stat.f_mntfromname, MNAMELEN - 1,
	    &size);
	memset(mp->mnt_stat.f_mntfromname + size, 0, MNAMELEN - size);
	(void) ufs_statfs(mp, &mp->mnt_stat, p);
	vfs_unlock(mp);
	inittodr(fs->fs_time);
	return (0);
}

/*
 * VFS Operations.
 *
 * mount system call
 */
static int
ufs_mount(struct mount *mp, char *path, caddr_t data, struct nameidata *ndp,
	struct proc *p)
{
	struct vnode *devvp;
	struct ufs_args args;
	struct ufsmount *ump;
	struct fs *fs;
	u_int size;
	int error;

	if (error = copyin(p, data, (caddr_t)&args, sizeof (struct ufs_args)))
		return (error);

	/* if mount request or filesystem is exported, update mount flag */
	if ((args.exflags & MNT_EXPORTED) || (mp->mnt_flag & MNT_EXPORTED)) {
		if (args.exflags & MNT_EXPORTED)
			mp->mnt_flag |= MNT_EXPORTED;
		else
			mp->mnt_flag &= ~MNT_EXPORTED;
		if (args.exflags & MNT_EXRDONLY)
			mp->mnt_flag |= MNT_EXRDONLY;
		else
			mp->mnt_flag &= ~MNT_EXRDONLY;
		mp->mnt_exroot = args.exroot;
	}

	/*
	 * If updating, check whether changing from read-only to
	 * read/write; if there is no device name, that's all we do.
	 */
	if (mp->mnt_flag & MNT_UPDATE) {
		ump = VFSTOUFS(mp);
		fs = ump->um_fs;
		if (fs->fs_ronly && (mp->mnt_flag & MNT_RDONLY) == 0)
			fs->fs_ronly = 0;
		if (args.fspec == 0)
			return (0);
	}

	/*
	 * Not an update, or updating the name: look up the name
	 * and verify that it refers to a sensible block device.
	 */
	ndp->ni_nameiop = LOOKUP | FOLLOW;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = args.fspec;
	if (error = namei(ndp, p))
		return (error);
	devvp = ndp->ni_vp;
	if (devvp->v_type != VBLK) {
		vrele(devvp);
		return (ENOTBLK);
	}
	if ((mp->mnt_flag & MNT_UPDATE) == 0)
		error = mountfs(devvp, mp, p);
	else {
		if (devvp != ump->um_devvp)
			error = EINVAL;	/* needs translation */
		else
			vrele(devvp);
	}
	if (error) {
		vrele(devvp);
		return (error);
	}
	ump = VFSTOUFS(mp);
	fs = ump->um_fs;
	(void) copyinstr(p, path, fs->fs_fsmnt, sizeof(fs->fs_fsmnt) - 1, &size);
	memset(fs->fs_fsmnt + size, 0, sizeof(fs->fs_fsmnt) - size);
	memcpy((caddr_t)mp->mnt_stat.f_mntonname, (caddr_t)fs->fs_fsmnt,
	    MNAMELEN);
	(void) copyinstr(p, args.fspec, mp->mnt_stat.f_mntfromname, MNAMELEN - 1, 
	    &size);
	memset(mp->mnt_stat.f_mntfromname + size, 0, MNAMELEN - size);
	(void) ufs_statfs(mp, &mp->mnt_stat, p);
	return (0);
}

/*
 * Common code for mount and mountroot (also used by mfs)
 */
/*static */int
mountfs(struct vnode *devvp, struct mount *mp, struct proc *p)
{
	register struct ufsmount *ump = (struct ufsmount *)0;
	struct buf *bp = NULL;
	register struct fs *fs;
	dev_t dev = devvp->v_rdev;
	struct partinfo dpart;
	caddr_t base, space;
	int havepart = 0, blks;
	int error, i, size;
	int needclose = 0;
	int ronly = (mp->mnt_flag & MNT_RDONLY) != 0;
	extern struct vnode *rootvp;

	if (error = VOP_OPEN(devvp, (ronly ? FREAD : FREAD|FWRITE) | FMOUNT,
	    NOCRED, p))
		return (error);
	needclose = 1;
	if (VOP_IOCTL(devvp, DIOCGPART, (caddr_t)&dpart, FREAD, NOCRED, p) != 0)
		size = DEV_BSIZE;
	else {
		havepart = 1;
		size = dpart.disklab->d_secsize;
	}
	if (error = bread(devvp, SBLOCK, SBSIZE, NOCRED, &bp))
		goto out;
	fs = bp->b_un.b_fs;
	if (fs->fs_magic != FS_MAGIC || fs->fs_bsize > MAXBSIZE ||
	    fs->fs_bsize < sizeof(struct fs)) {
		error = EINVAL;		/* XXX needs translation */
		goto out;
	}
	ump = (struct ufsmount *)malloc(sizeof *ump, M_UFSMNT, M_WAITOK);
	ump->um_fs = (struct fs *)malloc((u_long)fs->fs_sbsize, M_SUPERBLK,
	    M_WAITOK);
	memcpy((caddr_t)ump->um_fs, (caddr_t)bp->b_un.b_addr,
	   (u_int)fs->fs_sbsize);
	if (fs->fs_sbsize < SBSIZE)
		bp->b_flags |= B_INVAL;
	brelse(bp);
	bp = NULL;
	fs = ump->um_fs;
	fs->fs_ronly = ronly;
	if (ronly == 0)
		fs->fs_fmod = 1;
	if (havepart) {
		dpart.part->p_fstype = FS_BSDFFS;
		dpart.part->p_fsize = fs->fs_fsize;
		dpart.part->p_frag = fs->fs_frag;
		dpart.part->p_cpg = fs->fs_cpg;
	}
	blks = howmany(fs->fs_cssize, fs->fs_fsize);
	base = space = (caddr_t)malloc((u_long)fs->fs_cssize, M_SUPERBLK,
	    M_WAITOK);
	for (i = 0; i < blks; i += fs->fs_frag) {
		size = fs->fs_bsize;
		if (i + fs->fs_frag > blks)
			size = (blks - i) * fs->fs_fsize;
		error = bread(devvp, fsbtodb(fs, fs->fs_csaddr + i), size,
			NOCRED, &bp);
		if (error) {
			free((caddr_t)base, M_SUPERBLK);
			goto out;
		}
		memcpy(space, (caddr_t)bp->b_un.b_addr, (u_int)size);
		fs->fs_csp[fragstoblks(fs, i)] = (struct csum *)space;
		space += size;
		brelse(bp);
		bp = NULL;
	}
	mp->mnt_data = (qaddr_t)ump;
	mp->mnt_stat.f_fsid.val[0] = (long)dev;
	mp->mnt_stat.f_fsid.val[1] = MOUNT_UFS;
	mp->mnt_flag |= MNT_LOCAL;
	ump->um_mountp = mp;
	ump->um_dev = dev;
	ump->um_devvp = devvp;
	for (i = 0; i < MAXQUOTAS; i++)
		ump->um_quotas[i] = NULLVP;
	devvp->v_specflags |= SI_MOUNTEDON;

	/* Sanity checks for old file systems.			   XXX */
	fs->fs_npsect = max(fs->fs_npsect, fs->fs_nsect);	/* XXX */
	fs->fs_interleave = max(fs->fs_interleave, 1);		/* XXX */
	if (fs->fs_postblformat == FS_42POSTBLFMT)		/* XXX */
		fs->fs_nrpos = 8;				/* XXX */
	return (0);
out:
	if (bp)
		brelse(bp);
	if (needclose)
		(void)VOP_CLOSE(devvp, ronly ? FREAD : FREAD|FWRITE, NOCRED, p);
	if (ump) {
		free((caddr_t)ump->um_fs, M_SUPERBLK);
		free((caddr_t)ump, M_UFSMNT);
		mp->mnt_data = (qaddr_t)0;
	}
	return (error);
}

/*
 * Make a filesystem operational.
 * Nothing to do at the moment.
 */
/* ARGSUSED */
static int
ufs_start(struct mount *mp, int flags, struct proc *p)
{

	return (0);
}

/* unmount filesystem operation */
static int
ufs_unmount(struct mount *mp, int mntflags, struct proc *p)
{
	struct ufsmount *ump;
	struct fs *fs;
	int i, error, ronly, flags = 0;

	if (mntflags & MNT_FORCE) {
		if (mp == rootfs)
			return (EINVAL);
		flags |= FORCECLOSE;
	}
	mntflushbuf(mp, 0);
	if (mntinvalbuf(mp))
		return (EBUSY);
	ump = VFSTOUFS(mp);
#ifdef QUOTA
	if (mp->mnt_flag & MNT_QUOTA) {
		if (error = vflush(mp, NULLVP, SKIPSYSTEM|flags))
			return (error);
		for (i = 0; i < MAXQUOTAS; i++) {
			if (ump->um_quotas[i] == NULLVP)
				continue;
			quotaoff(p, mp, i);
		}
		/*
		 * Here we fall through to vflush again to ensure
		 * that we have gotten rid of all the system vnodes.
		 */
	}
#endif
	if (error = vflush(mp, NULLVP, flags))
		return (error);
	fs = ump->um_fs;
	ronly = !fs->fs_ronly;
	ump->um_devvp->v_specflags &= ~SI_MOUNTEDON;
	error = VOP_CLOSE(ump->um_devvp, ronly ? FREAD : FREAD|FWRITE,
		NOCRED, p);
	vrele(ump->um_devvp);
	free((caddr_t)fs->fs_csp[0], M_SUPERBLK);
	free((caddr_t)fs, M_SUPERBLK);
	free((caddr_t)ump, M_UFSMNT);
	mp->mnt_data = (qaddr_t)0;
	mp->mnt_flag &= ~MNT_LOCAL;
	return (error);
}

/* Return root of a filesystem */
static int
ufs_root(struct mount *mp, struct vnode **vpp)
{
	register struct inode *ip;
	struct inode *nip;
	struct vnode tvp;
	int error;

	tvp.v_mount = mp;
	ip = VTOI(&tvp);
	ip->i_vnode = &tvp;
	ip->i_dev = VFSTOUFS(mp)->um_dev;
	error = iget(ip, (ino_t)ROOTINO, &nip);
	if (error)
		return (error);
	*vpp = ITOV(nip);
	return (0);
}

/* Implement quota controls in this filesystem */
static int
ufs_quotactl(struct mount *mp, int cmds, uid_t uid, caddr_t arg, struct proc *p)
{
	struct ufsmount *ump = VFSTOUFS(mp);
	int cmd, type, error;

#ifndef QUOTA
	return (EOPNOTSUPP);
#else
	if (uid == -1)
		uid = p->p_cred->p_ruid;
	cmd = cmds >> SUBCMDSHIFT;

	switch (cmd) {
	case Q_GETQUOTA:
	case Q_SYNC:
		if (uid == p->p_cred->p_ruid)
			break;
		/* fall through */
	default:
		if (error = use_priv(p->p_ucred, PRV_UFS_QUOTA_CHANGE, p))
			return (error);
	}

	type = cmd & SUBCMDMASK;
	if ((u_int)type >= MAXQUOTAS)
		return (EINVAL);

	switch (cmd) {

	case Q_QUOTAON:
		return (quotaon(p, mp, type, arg));

	case Q_QUOTAOFF:
		if (vfs_busy(mp))
			return (0);
		error = quotaoff(p, mp, type);
		vfs_unbusy(mp);
		return (error);

	case Q_SETQUOTA:
		return (setquota(mp, uid, type, arg));

	case Q_SETUSE:
		return (setuse(mp, uid, type, arg));

	case Q_GETQUOTA:
		return (getquota(mp, uid, type, arg));

	case Q_SYNC:
		if (vfs_busy(mp))
			return (0);
		error = qsync(mp);
		vfs_unbusy(mp);
		return (error);

	default:
		return (EINVAL);
	}
	/* NOTREACHED */
#endif
}

/* Get file system statistics. */
int
ufs_statfs(struct mount *mp, struct statfs *sbp, struct proc *p)
{
	struct ufsmount *ump;
	struct fs *fs;

	ump = VFSTOUFS(mp);
	fs = ump->um_fs;
	if (fs->fs_magic != FS_MAGIC)
		panic("ufs_statfs");
	sbp->f_type = MOUNT_UFS;
	sbp->f_fsize = fs->fs_fsize;
	sbp->f_bsize = fs->fs_bsize;
	sbp->f_blocks = fs->fs_dsize;
	sbp->f_bfree = fs->fs_cstotal.cs_nbfree * fs->fs_frag +
		fs->fs_cstotal.cs_nffree;
	sbp->f_bavail = (fs->fs_dsize * (100 - fs->fs_minfree) / 100) -
		(fs->fs_dsize - sbp->f_bfree);
	sbp->f_files =  fs->fs_ncg * fs->fs_ipg - ROOTINO;
	sbp->f_ffree = fs->fs_cstotal.cs_nifree;
	if (sbp != &mp->mnt_stat) {
		memcpy((caddr_t)&sbp->f_mntonname[0],
			(caddr_t)mp->mnt_stat.f_mntonname, MNAMELEN);
		memcpy((caddr_t)&sbp->f_mntfromname[0],
			(caddr_t)mp->mnt_stat.f_mntfromname, MNAMELEN);
	}
	return (0);
}

int	syncprt = 0;

/*
 * Go through the disk queues to initiate sandbagged IO;
 * go through the inodes to write those that have been modified;
 * initiate the writing of the super block if it has been modified.
 *
 * Note: we are always called with the filesystem marked `MPBUSY'.
 */
static int
ufs_sync(struct mount *mp, int waitfor)
{
	register struct vnode *vp;
	register struct inode *ip;
	register struct ufsmount *ump = VFSTOUFS(mp);
	register struct fs *fs;
	int error, allerror = 0;

	/*if (syncprt)
		bufstats();*/
	fs = ump->um_fs;
	/*
	 * Write back modified superblock.
	 * Consistency check that the superblock
	 * is still in the buffer cache.
	 */
	if (fs->fs_fmod != 0) {
		if (fs->fs_ronly != 0) {		/* XXX */
			printf("fs = %s\n", fs->fs_fsmnt);
			panic("update: rofs mod");
		}
		fs->fs_fmod = 0;
		fs->fs_time = time.tv_sec;
		allerror = sbupdate(ump, waitfor);
	}

	/*
	 * Write back each (modified) inode.
	 */
loop:
	for (vp = mp->mnt_mounth; vp; vp = vp->v_mountf) {
		/*
		 * If the vnode that we are about to sync is no longer
		 * associated with this mount point, start over.
		 */
		if (vp->v_mount != mp)
			goto loop;
		if (VOP_ISLOCKED(vp))
			continue;
		ip = VTOI(vp);
		if ((ip->i_flag & (IMOD|IACC|IUPD|ICHG)) == 0 &&
		    vp->v_dirtyblkhd == NULL)
			continue;
		if (vget(vp))
			goto loop;
		if (vp->v_dirtyblkhd)
			vflushbuf(vp, 0, 0);
		if ((ip->i_flag & (IMOD|IACC|IUPD|ICHG)) &&
		    (error = iupdat(ip, &time, &time, 0)))
			allerror = error;
		vput(vp);
	}
	/*
	 * Force stale file system control information to be flushed.
	 */
	vflushbuf(ump->um_devvp, waitfor == MNT_WAIT ? B_SYNC : 0, 0);
#ifdef QUOTA
	qsync(mp);
#endif
	return (allerror);
}

/*
 * Write a superblock and associated information back to disk.
 */
sbupdate(mp, waitfor)
	struct ufsmount *mp;
	int waitfor;
{
	register struct fs *fs = mp->um_fs;
	register struct buf *bp;
	int blks;
	caddr_t space;
	int i, size, error = 0;

	bp = getblk(mp->um_devvp, SBLOCK, (int)fs->fs_sbsize);
	memcpy(bp->b_un.b_addr, (caddr_t)fs, (u_int)fs->fs_sbsize);
	/* Restore compatibility to old file systems.		   XXX */
	if (fs->fs_postblformat == FS_42POSTBLFMT)		/* XXX */
		bp->b_un.b_fs->fs_nrpos = -1;			/* XXX */
	if (waitfor == MNT_WAIT)
		error = bwrite(bp);
	else
		bawrite(bp);
	blks = howmany(fs->fs_cssize, fs->fs_fsize);
	space = (caddr_t)fs->fs_csp[0];
	for (i = 0; i < blks; i += fs->fs_frag) {
		size = fs->fs_bsize;
		if (i + fs->fs_frag > blks)
			size = (blks - i) * fs->fs_fsize;
		bp = getblk(mp->um_devvp, fsbtodb(fs, fs->fs_csaddr + i), size);
		memcpy(bp->b_un.b_addr, space, (u_int)size);
		space += size;
		if (waitfor == MNT_WAIT)
			error = bwrite(bp);
		else
			bawrite(bp);
	}
	return (error);
}

#ifdef nope
/*
 * Print out statistics on the current allocation of the buffer pool.
 * Can be enabled to print out on every ``sync'' by setting "syncprt"
 * above.
 */
bufstats()
{
	int s, i, j, count;
	register struct buf *bp, *dp;
	int counts[MAXBSIZE/CLBYTES+1];
	static char *bname[BQUEUES] = { "LOCKED", "LRU", "AGE", "EMPTY" };

	for (bp = bfreelist, i = 0; bp < &bfreelist[BQUEUES]; bp++, i++) {
		count = 0;
		for (j = 0; j <= MAXBSIZE/CLBYTES; j++)
			counts[j] = 0;
		s = splbio();
		for (dp = bp->av_forw; dp != bp; dp = dp->av_forw) {
			counts[dp->b_bufsize/CLBYTES]++;
			count++;
		}
		splx(s);
		printf("%s: total-%d", bname[i], count);
		for (j = 0; j <= MAXBSIZE/CLBYTES; j++)
			if (counts[j] != 0)
				printf(", %d-%d", j * CLBYTES, counts[j]);
		printf("\n");
	}
}
#endif

/*
 * File handle to vnode
 *
 * Have to be really careful about stale file handles:
 * - check that the inode number is in range
 * - call iget() to get the locked inode
 * - check for an unallocated inode (i_mode == 0)
 * - check that the generation number matches
 */
static int
ufs_fhtovp(struct mount *mp, struct fid *fhp, struct vnode **vpp)
{
	struct ufid *ufhp;
	struct fs *fs;
	struct inode *ip, *nip;
	struct vnode tvp;
	int error;

	ufhp = (struct ufid *)fhp;
	fs = VFSTOUFS(mp)->um_fs;
	if (ufhp->ufid_ino < ROOTINO ||
	    ufhp->ufid_ino >= fs->fs_ncg * fs->fs_ipg) {
		*vpp = NULLVP;
		return (EINVAL);
	}
	tvp.v_mount = mp;
	ip = VTOI(&tvp);
	ip->i_vnode = &tvp;
	ip->i_dev = VFSTOUFS(mp)->um_dev;
	if (error = iget(ip, ufhp->ufid_ino, &nip)) {
		*vpp = NULLVP;
		return (error);
	}
	ip = nip;
	if (ip->i_mode == 0) {
		iput(ip);
		*vpp = NULLVP;
		return (EINVAL);
	}
	if (ip->i_gen != ufhp->ufid_gen) {
		iput(ip);
		*vpp = NULLVP;
		return (EINVAL);
	}
	*vpp = ITOV(ip);
	return (0);
}

/*
 * Vnode pointer to File handle
 */
int
ufs_vptofh(struct vnode *vp, struct fid *fhp)
{
	struct inode *ip = VTOI(vp);
	struct ufid *ufhp;

	ufhp = (struct ufid *)fhp;
	ufhp->ufid_len = sizeof(struct ufid);
	ufhp->ufid_ino = ip->i_number;
	ufhp->ufid_gen = ip->i_gen;
	return (0);
}
