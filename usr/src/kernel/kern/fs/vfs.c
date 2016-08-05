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
 * $Id: vfs.c,v 1.1 94/10/19 17:09:24 bill Exp Locker: bill $
 *
 * Virtual filesystem interface routines and implementation.
 */

#include "sys/param.h"
#include "sys/mount.h"
#include "sys/time.h"
#include "sys/file.h"
#include "sys/errno.h"
#include "sys/kinfo.h"

#include "proc.h"
#include "uio.h"
#include "specdev.h"
#include "buf.h"
#include "malloc.h"
#include "modconfig.h"

#include "vnode.h"
#include "namei.h"

#include "prototypes.h"

struct mount *rootfs;
struct vfsops *vfs;

/*static*/ void insmntque(struct vnode *vp, struct mount *mp);

/*
 * Remove a mount point from the list of mounted filesystems.
 * Unmount of the root is illegal.
 */
void
vfs_remove(struct mount *mp)
{

	if (mp == rootfs)
		panic("vfs_remove: unmounting root");
	mp->mnt_prev->mnt_next = mp->mnt_next;
	mp->mnt_next->mnt_prev = mp->mnt_prev;
	mp->mnt_vnodecovered->v_mountedhere = (struct mount *)0;
	vfs_unlock(mp);
}

/*
 * Lock a filesystem.
 * Used to prevent access to it while mounting and unmounting.
 */
int
vfs_lock(struct mount *mp)
{

	while(mp->mnt_flag & MNT_MLOCK) {
		mp->mnt_flag |= MNT_MWAIT;
		tsleep((caddr_t)mp, PVFS, "vfslock", 0);
	}
	mp->mnt_flag |= MNT_MLOCK;
	return (0);
}

/*
 * Unlock a locked filesystem.
 * Panic if filesystem is not locked.
 */
void
vfs_unlock(struct mount *mp)
{

	if ((mp->mnt_flag & MNT_MLOCK) == 0)
		panic("vfs_unlock: not locked");
	mp->mnt_flag &= ~MNT_MLOCK;
	if (mp->mnt_flag & MNT_MWAIT) {
		mp->mnt_flag &= ~MNT_MWAIT;
		wakeup((caddr_t)mp);
	}
}

/*
 * Mark a mount point as busy.
 * Used to synchronize access and to delay unmounting.
 */
int
vfs_busy(struct mount *mp)
{

	while(mp->mnt_flag & MNT_MPBUSY) {
		mp->mnt_flag |= MNT_MPWANT;
		tsleep((caddr_t)&mp->mnt_flag, PVFS, "vfsbusy", 0);
	}
	if (mp->mnt_flag & MNT_UNMOUNT)
		return (1);
	mp->mnt_flag |= MNT_MPBUSY;
	return (0);
}

/*
 * Free a busy filesystem.
 * Panic if filesystem is not busy.
 */
void
vfs_unbusy(struct mount *mp)
{

	if ((mp->mnt_flag & MNT_MPBUSY) == 0)
		panic("vfs_unbusy: not busy");
	mp->mnt_flag &= ~MNT_MPBUSY;
	if (mp->mnt_flag & MNT_MPWANT) {
		mp->mnt_flag &= ~MNT_MPWANT;
		wakeup((caddr_t)&mp->mnt_flag);
	}
}

/*
 * Lookup a mount point by filesystem identifier.
 */
struct mount *
getvfs(fsid_t *fsid)
{
	struct mount *mp;

	mp = rootfs;
	do {
		if (mp->mnt_stat.f_fsid.val[0] == fsid->val[0] &&
		    mp->mnt_stat.f_fsid.val[1] == fsid->val[1]) {
			return (mp);
		}
		mp = mp->mnt_next;
	} while (mp != rootfs);
	return ((struct mount *)0);
}

/* add a filesystem type to the system. */
void
addvfs(struct vfsops *vfsp) {
	struct vfsops *ovfsp = vfs;

	vfsp->vfs_next = ovfsp;
	vfs = vfsp;
}

/* find a filesystem type in the system. */
struct vfsops *
findvfs(int type) {
	struct vfsops *vfsp;

	/* search list */
	for(vfsp = vfs; vfsp ; vfsp = vfsp->vfs_next)
		if (vfsp->vfs_type == type)
			return (vfsp);
	return (0);
}

/* compose a partially implemented filesystem with parts from another */
void
incorporatefs(struct vfsops *srcfs, struct vfsops *dstfs) {

	if (dstfs->vfs_mount == 0)
		dstfs->vfs_mount = srcfs->vfs_mount;
	if (dstfs->vfs_start == 0)
		dstfs->vfs_start = srcfs->vfs_start;
	if (dstfs->vfs_unmount == 0)
		dstfs->vfs_unmount = srcfs->vfs_unmount;
	if (dstfs->vfs_root == 0)
		dstfs->vfs_root = srcfs->vfs_root;
	if (dstfs->vfs_quotactl == 0)
		dstfs->vfs_quotactl = srcfs->vfs_quotactl;
	if (dstfs->vfs_statfs == 0)
		dstfs->vfs_statfs = srcfs->vfs_statfs;
	if (dstfs->vfs_sync == 0)
		dstfs->vfs_sync = srcfs->vfs_sync;
	if (dstfs->vfs_fhtovp == 0)
		dstfs->vfs_fhtovp = srcfs->vfs_fhtovp;
	if (dstfs->vfs_vptofh == 0)
		dstfs->vfs_vptofh = srcfs->vfs_vptofh;
	if (dstfs->vfs_init == 0)
		dstfs->vfs_init = srcfs->vfs_init;
	if (dstfs->vfs_mountroot == 0)
		dstfs->vfs_mountroot = srcfs->vfs_mountroot;
}

/*
 * Initialize the vnode structures and initialize each file system type.
 */
void
vfsinit(void)
{
	struct vfsops *vfsp;

	/* initialize the name cache */
	nchinit();

	/* initalize the vnode layer */
	vattr_null(&va_null);

	/* initialize each filesystem */
	for (vfsp = vfs; vfsp ; vfsp = vfsp->vfs_next)
		(*vfsp->vfs_init)();
}

/*
 * Move a vnode from one mount queue to another.
 */
/*static*/ void
insmntque(struct vnode *vp, struct mount *mp)
{
	struct vnode *vq;

	/*
	 * Delete from old mount point vnode list, if on one.
	 */
	if (vp->v_mountb) {
		if (vq = vp->v_mountf)
			vq->v_mountb = vp->v_mountb;
		*vp->v_mountb = vq;
	}
	/*
	 * Insert into list of vnodes for the new mount point, if available.
	 */
	vp->v_mount = mp;
	if (mp == NULL) {
		vp->v_mountf = NULL;
		vp->v_mountb = NULL;
		return;
	}
	if (vq = mp->mnt_mounth)
		vq->v_mountb = &vp->v_mountf;
	vp->v_mountf = vq;
	vp->v_mountb = &mp->mnt_mounth;
	mp->mnt_mounth = vp;
}

/*
 * Make sure all write-behind blocks associated
 * with mount point are flushed out (from sync).
 */
void
mntflushbuf(struct mount *mountp, int flags)
{
	struct vnode *vp;

	if ((mountp->mnt_flag & MNT_MPBUSY) == 0)
		panic("mntflushbuf: not busy");
loop:
	for (vp = mountp->mnt_mounth; vp; vp = vp->v_mountf) {
		if (VOP_ISLOCKED(vp))
			continue;
		if (vget(vp))
			goto loop;
		vflushbuf(vp, flags, 0);
		vput(vp);
		if (vp->v_mount != mountp)
			goto loop;
	}
}

/*
 * Invalidate in core blocks belonging to closed or unmounted filesystem
 *
 * Go through the list of vnodes associated with the file system;
 * for each vnode invalidate any buffers that it holds. Normally
 * this routine is preceeded by a bflush call, so that on a quiescent
 * filesystem there will be no dirty buffers when we are done. Binval
 * returns the count of dirty buffers when it is finished.
 */
int
mntinvalbuf(struct mount *mountp)
{
	register struct vnode *vp;
	int dirty = 0;

	if ((mountp->mnt_flag & MNT_MPBUSY) == 0)
		panic("mntinvalbuf: not busy");
loop:
	for (vp = mountp->mnt_mounth; vp; vp = vp->v_mountf) {
		if (vget(vp))
			goto loop;
		dirty += vinvalbuf(vp, 1);
		vput(vp);
		if (vp->v_mount != mountp)
			goto loop;
	}
	return (dirty);
}

/*
 * Remove any vnodes in the vnode table belonging to mount point mp.
 *
 * If MNT_NOFORCE is specified, there should not be any active ones,
 * return error if any are found (nb: this is a user error, not a
 * system error). If MNT_FORCE is specified, detach any active vnodes
 * that are found.
 */
#ifdef	DEBUG
int busyprt = 0;	/* patch to print out busy vnodes */
#endif

int
vflush(struct mount *mp, struct vnode *skipvp, int flags)
{
	register struct vnode *vp, *nvp;
	int busy = 0;

	if ((mp->mnt_flag & MNT_MPBUSY) == 0)
		panic("vflush: not busy");
loop:
	for (vp = mp->mnt_mounth; vp; vp = nvp) {
		if (vp->v_mount != mp)
			goto loop;
		nvp = vp->v_mountf;
		/*
		 * Skip over a selected vnode.
		 */
		if (vp == skipvp)
			continue;
		/*
		 * Skip over a vnodes marked VSYSTEM.
		 */
		if ((flags & SKIPSYSTEM) && (vp->v_flag & VSYSTEM))
			continue;
		/*
		 * With v_usecount == 0, all we need to do is clear
		 * out the vnode data structures and we are done.
		 */
		if (vp->v_usecount == 0) {
			vgone(vp);
			continue;
		}
		/*
		 * For block or character devices, revert to an
		 * anonymous device. For all other files, just kill them.
		 */
		if (flags & FORCECLOSE) {
			if (vp->v_type != VBLK && vp->v_type != VCHR) {
				vgone(vp);
			} else {
				spec_anonymous(vp);
				/*vclean(vp, 0);
				vp->v_op = &spec_vnodeops;
				insmntque(vp, (struct mount *)0);*/
			}
			continue;
		}
#ifdef DEBUG
		if (busyprt)
			vprint("vflush: busy vnode", vp);
#endif
		busy++;
	}
	if (busy)
		return (EBUSY);
	return (0);
}
