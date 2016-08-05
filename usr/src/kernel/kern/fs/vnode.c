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
 * $Id: vnode.c,v 1.1 94/10/19 17:09:26 bill Exp Locker: bill $
 *
 * Virtual filesystem vnode abstraction implementation.
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
/*#include "namei.h"*/

#include "prototypes.h"

u_long nextvnodeid;

/*static*/ void insmntque(struct vnode *vp, struct mount *mp);
#if defined(DEBUG) || defined(DIAGNOSTIC)
void vprint(char *label, struct vnode *vp);
void printlockedvnodes(void);
#endif

#ifdef notyet
/* compose a partially implemented vnode ops with parts from another */
void
incorporateops(struct vnodeops *srcvnops, struct vnodeops *dstvnops) {

	...
}
#endif

/*
 * Set vnode attributes to VNOVAL
 */
void
vattr_null(struct vattr *vap)
{

	vap->va_type = VNON;
	vap->va_mode = vap->va_nlink = vap->va_uid = vap->va_gid =
		vap->va_fsid = vap->va_fileid = vap->va_size =
		vap->va_size_rsv = vap->va_blocksize = vap->va_rdev =
		vap->va_bytes = vap->va_bytes_rsv =
		vap->va_atime.tv_sec = vap->va_atime.tv_usec =
		vap->va_mtime.tv_sec = vap->va_mtime.tv_usec =
		vap->va_ctime.tv_sec = vap->va_ctime.tv_usec =
		vap->va_flags = vap->va_gen = VNOVAL;
}

/*
 * Routines having to do with the management of the vnode table.
 */
struct vnode *vfreeh, **vfreet;
extern struct vnodeops dead_vnodeops, spec_vnodeops;
struct vattr va_null;
long numvnodes;

/*
 * Return the next vnode from the free list.
 */
int
getnewvnode(enum vtagtype tag, struct mount *mp, struct vnodeops *vops,
	struct vnode **vpp)
{
	struct vnode *vp, *vq;

	if (numvnodes < desiredvnodes) {
		vp = (struct vnode *)malloc((u_long)sizeof *vp,
		    M_VNODE, M_WAITOK);
		(void) memset((char *)vp, 0, sizeof *vp);
		numvnodes++;
	} else {
		if ((vp = vfreeh) == NULL) {
			tablefull("vnode");
			*vpp = 0;
			return (ENFILE);
		}
		if (vp->v_usecount)
			panic("free vnode isn't");
		if (vq = vp->v_freef)
			vq->v_freeb = &vfreeh;
		else
			vfreet = &vfreeh;
		vfreeh = vq;
		vp->v_freef = NULL;
		vp->v_freeb = NULL;
		if (vp->v_type != VBAD)
			vgone(vp);
		vp->v_flag = 0;
		vp->v_lastr = -1;	/* necessary for first read-ahead */
		vp->v_socket = 0;
		memset(vp->v_name, 0, 16);
	}
	vp->v_type = VNON;
	cache_purge(vp);
	vp->v_tag = tag;
	vp->v_op = vops;
	insmntque(vp, mp);
	VREF(vp);
	*vpp = vp;
	return (0);
}

/*
 * Flush all dirty buffers associated with a vnode.
 */
void
vflushbuf(struct vnode *vp, int flags, struct buf *xbp)
{
	struct buf *bp, *nbp;
	int s;

loop:
	s = splbio();
	for (bp = vp->v_dirtyblkhd; bp; bp = nbp) {
		nbp = bp->b_blockf;
		if ((bp->b_flags & B_BUSY))
			continue;
		if ((bp->b_flags & B_DELWRI) == 0)
			panic("vflushbuf: not dirty");
		/* if flushing all but xbp, pass on this buffer */
		if (xbp && bp == xbp)
			continue;
		/* if an indirect block, don't bother */
		if (xbp && bp->b_vp != vp)
			continue;
		bremfree(bp);
		bp->b_flags |= B_BUSY;
		splx(s);
		/*
		 * Wait for I/O associated with indirect blocks to complete,
		 * since there is no way to quickly wait for them below.
		 * NB: This is really specific to ufs, but is done here
		 * as it is easier and quicker.
		 */
		if (bp->b_vp == vp || (flags & B_SYNC) == 0)
			(void) bawrite(bp);
		else
			(void) bwrite(bp);
		goto loop;
	}
	splx(s);
	if ((flags & B_SYNC) == 0)
		return;
	s = splbio();
	while (vp->v_numoutput) {
		vp->v_flag |= VBWAIT;
		tsleep((caddr_t)&vp->v_numoutput, PRIBIO + 1, "vflushbuf", 0);
	}
	splx(s);
	if (vp->v_dirtyblkhd) {
#ifdef DEBUG
		vprint("vflushbuf: dirty", vp);
#endif
		goto loop;
	}
}

/*
 * Update outstanding I/O count and do wakeup if requested.
 */
void
vwakeup(struct buf *bp)
{
	struct vnode *vp;

	bp->b_dirtyoff = bp->b_dirtyend = 0;
	if (vp = bp->b_vp) {
		vp->v_numoutput--;
		if ((vp->v_flag & VBWAIT) && vp->v_numoutput <= 0) {
			if (vp->v_numoutput < 0)
				panic("vwakeup: neg numoutput");
			vp->v_flag &= ~VBWAIT;
			wakeup((caddr_t)&vp->v_numoutput);
		}
	}
}

/*
 * Flush out and invalidate all buffers associated with a vnode.
 * Called with the underlying object locked.
 */
int
vinvalbuf(struct vnode *vp, int save)
{
	struct buf *bp, *nbp, *blist;
	int s, dirty = 0;

	for (;;) {
		if (blist = vp->v_dirtyblkhd)
			/* void */;
		else if (blist = vp->v_cleanblkhd)
			/* void */;
		else
			break;
		for (bp = blist; bp; bp = nbp) {
			nbp = bp->b_blockf;
			s = splbio();
			if (bp->b_flags & B_BUSY) {
				bp->b_flags |= B_WANTED;
				tsleep((caddr_t)bp, PRIBIO + 1, "vinvalbuf", 0);
				splx(s);
				break;
			}
			bremfree(bp);
			bp->b_flags |= B_BUSY;
			splx(s);
			if (save && (bp->b_flags & B_DELWRI)) {
				dirty++;
				(void) bwrite(bp);
				break;
			}
			if (bp->b_vp != vp)
				reassignbuf(bp, bp->b_vp);
			else
				bp->b_flags |= B_INVAL;
			brelse(bp);
		}
	}
	if (vp->v_dirtyblkhd || vp->v_cleanblkhd)
		panic("vinvalbuf: flush failed");
	return (dirty);
}

#ifdef notyet
/*
 * Age the buffers of a vnode.
 */
void
vagebuf(struct vnode *vp)
{
	struct buf *bp = buf;
	int s, i;

	for (i = 0; i < nbuf; i++,bp++) {
		if (bp->b_vp != vp)
			continue;
		if (bp->b_flags & B_AGE)
			continue;
		s = splbio();
		bp->b_flags |= B_AGE;
		if (bp->b_flags & B_BUSY) {
			splx(s);
			continue;
		}
		bremfree(bp);
		bp->b_flags |= B_BUSY;
		splx(s);
		brelse(bp);
	}
}

/*
 * Un-age the buffers of a vnode.
 */
void
vunagebuf(struct vnode *vp)
{
	struct buf *bp = buf;
	int s, i;

	for (i = 0; i < nbuf; i++,bp++) {
		if (bp->b_vp != vp)
			continue;
		if ((bp->b_flags & B_AGE) == 0)
			continue;
		s = splbio();
		bp->b_flags &= ~B_AGE;
		if (bp->b_flags & B_BUSY) {
			splx(s);
			continue;
		}
		bremfree(bp);
		bp->b_flags |= B_BUSY;
		splx(s);
		brelse(bp);
	}
}
#endif


/*
 * Associate a buffer with a vnode.
 */
void
bgetvp(struct vnode *vp, struct buf *bp)
{
	struct vnode *vq;
	struct buf *bq;

	if (bp->b_vp)
		panic("bgetvp: not free");
	VHOLD(vp);
	bp->b_vp = vp;
	if (vp->v_type == VBLK || vp->v_type == VCHR)
		bp->b_dev = vp->v_rdev;
	else
		bp->b_dev = BLK_NODEV;
	/*
	 * Insert onto list for new vnode.
	 */
	if (bq = vp->v_cleanblkhd)
		bq->b_blockb = &bp->b_blockf;
	bp->b_blockf = bq;
	bp->b_blockb = &vp->v_cleanblkhd;
	vp->v_cleanblkhd = bp;
}

/*
 * Disassociate a buffer from a vnode.
 */
void
brelvp(struct buf *bp)
{
	struct buf *bq;
	struct vnode *vp;

	if (bp->b_vp == (struct vnode *) 0)
		panic("brelvp: NULL");
	/*
	 * Delete from old vnode list, if on one.
	 */
	if (bp->b_blockb) {
		if (bq = bp->b_blockf)
			bq->b_blockb = bp->b_blockb;
		*bp->b_blockb = bq;
		bp->b_blockf = NULL;
		bp->b_blockb = NULL;
	}
	vp = bp->b_vp;
	bp->b_vp = (struct vnode *) 0;
	HOLDRELE(vp);
}

/*
 * Reassign a buffer from one vnode to another.
 * Used to assign file specific control information
 * (indirect blocks) to the vnode to which they belong.
 */
void
reassignbuf(struct buf *bp, struct vnode *newvp)
{
	struct buf *bq, **listheadp;

	if (newvp == NULL)
		panic("reassignbuf: NULL");
	/*
	 * Delete from old vnode list, if on one.
	 */
	if (bp->b_blockb) {
		if (bq = bp->b_blockf)
			bq->b_blockb = bp->b_blockb;
		*bp->b_blockb = bq;
	}
	/*
	 * If dirty, put on list of dirty buffers;
	 * otherwise insert onto list of clean buffers.
	 */
	if (bp->b_flags & B_DELWRI)
		listheadp = &newvp->v_dirtyblkhd;
	else
		listheadp = &newvp->v_cleanblkhd;
	if (bq = *listheadp)
		bq->b_blockb = &bp->b_blockf;
	bp->b_blockf = bq;
	bp->b_blockb = listheadp;
	*listheadp = bp;
}

/*
 * Grab a particular vnode from the free list, increment its
 * reference count and lock it. The vnode lock bit is set the
 * vnode is being eliminated in vgone. The process is awakened
 * when the transition is completed, and an error returned to
 * indicate that the vnode is no longer usable (possibly having
 * been changed to a new file system type).
 */
int
vget(struct vnode *vp)
{
	struct vnode *vq;

	if (vp->v_flag & VXLOCK) {
		vp->v_flag |= VXWANT;
		tsleep((caddr_t)vp, PINOD, "vget", 0);
		return (1);
	}
	if (vp->v_usecount == 0) {
		if (vq = vp->v_freef)
			vq->v_freeb = vp->v_freeb;
		else
			vfreet = vp->v_freeb;
		*vp->v_freeb = vq;
		vp->v_freef = NULL;
		vp->v_freeb = NULL;
	}
	VREF(vp);
	VOP_LOCK(vp);
	/* if (vp->v_usecount == 1)
		vunagebuf(vp); */
	return (0);
}

/*
 * Vnode reference, just increment the count
 */
void
vref(struct vnode *vp)
{

	vp->v_usecount++;
}

/*
 * vput(), just unlock and vrele()
 */
void
vput(struct vnode *vp)
{
	VOP_UNLOCK(vp);
	vrele(vp);
}

/*
 * Vnode release.
 * If count drops to zero, call inactive routine and return to freelist.
 */
void
vrele(struct vnode *vp)
{
	struct proc *p = curproc;		/* XXX */

#ifdef DIAGNOSTIC
	if (vp == NULL)
		panic("vrele: null vp");
#endif
	vp->v_usecount--;
	if (vp->v_usecount > 0)
		return;
	/* vagebuf(vp); */
#if defined(DIAGNOSTIC) || defined(DEBUG)
	if (vp->v_usecount != 0 || vp->v_writecount != 0) {
		vprint("vrele: bad ref count", vp);
		panic("vrele: ref cnt");
	}
#endif
	if (vfreeh == NULLVP) {
		/*
		 * insert into empty list
		 */
		vfreeh = vp;
		vp->v_freeb = &vfreeh;
	} else {
		/*
		 * insert at tail of list
		 */
		*vfreet = vp;
		vp->v_freeb = vfreet;
	}
	vp->v_freef = NULL;
	vfreet = &vp->v_freef;
	VOP_INACTIVE(vp, p);
}

/*
 * Page or buffer structure gets a reference.
 */
void
vhold(struct vnode *vp)
{

	vp->v_holdcnt++;
}

/*
 * Page or buffer structure frees a reference.
 */
void
holdrele(struct vnode *vp)
{

	if (vp->v_holdcnt <= 0)
		panic("holdrele: holdcnt");
	vp->v_holdcnt--;
}

/*
 * Disassociate the underlying file system from a vnode.
pass in newops?
 */
void
vclean(struct vnode *vp, int flags)
{
	struct vnodeops *origops;
	int active;
	struct proc *p = curproc;	/* XXX */

	/*
	 * Check to see if the vnode is in use.
	 * If so we have to reference it before we clean it out
	 * so that its count cannot fall to zero and generate a
	 * race against ourselves to recycle it.
	 */
	if (active = vp->v_usecount)
		VREF(vp);
	/*
	 * Prevent the vnode from being recycled or
	 * brought into use while we clean it out.
	 */
	if (vp->v_flag & VXLOCK)
		panic("vclean: deadlock");
	vp->v_flag |= VXLOCK;
	/*
	 * Even if the count is zero, the VOP_INACTIVE routine may still
	 * have the object locked while it cleans it out. The VOP_LOCK
	 * ensures that the VOP_INACTIVE routine is done with its work.
	 * For active vnodes, it ensures that no other activity can
	 * occur while the buffer list is being cleaned out.
	 */
	VOP_LOCK(vp);
	if (flags & DOCLOSE)
		vinvalbuf(vp, 1);
	/*
	 * Prevent any further operations on the vnode from
	 * being passed through to the old file system.
	 */
	origops = vp->v_op;
	vp->v_op = &dead_vnodeops;
	vp->v_tag = VT_NON;
	/*
	 * If purging an active vnode, it must be unlocked, closed,
	 * and deactivated before being reclaimed.
	 */
	(*(origops->vop_unlock))(vp);
	if (active) {
		if (flags & DOCLOSE)
			(*(origops->vop_close))(vp, IO_NDELAY, NOCRED, p);
		(*(origops->vop_inactive))(vp, p);
	}
	/*
	 * Reclaim the vnode.
	 */
	if ((*(origops->vop_reclaim))(vp))
		panic("vclean: cannot reclaim");
/* cache_purge(vp); ? */
	if (active)
		vrele(vp);
	/*
	 * Done with purge, notify sleepers in vget of the grim news.
	 */
	vp->v_flag &= ~VXLOCK;
	if (vp->v_flag & VXWANT) {
		vp->v_flag &= ~VXWANT;
		wakeup((caddr_t)vp);
	}
}

/*
 * Eliminate all activity associated with  the requested vnode
 * and with all vnodes aliased to the requested vnode.
 */
void
vgoneall(struct vnode *vp)
{
	struct vnode *vq;

	if (vp->v_flag & VALIASED) {
		/*
		 * If a vgone (or vclean) is already in progress,
		 * wait until it is done and return.
		 */
		if (vp->v_flag & VXLOCK) {
			vp->v_flag |= VXWANT;
			tsleep((caddr_t)vp, PINOD, "vgoneall", 0);
			return;
		}
		/*
		 * Ensure that vp will not be vgone'd while we
		 * are eliminating its aliases.
		 */
		vp->v_flag |= VXLOCK;
		spec_remove_aliases(vp);
		/*
		 * Remove the lock so that vgone below will
		 * really eliminate the vnode after which time
		 * vgone will awaken any sleepers.
		 */
		vp->v_flag &= ~VXLOCK;
	}
	vgone(vp);
}

/*
 * Eliminate all activity associated with a vnode
 * in preparation for reuse.
 */
void
vgone(struct vnode *vp)
{
	struct vnode *vq, *vx;
	long count;

	/* another process is reclaiming/cleaning this vnode, wait for it. */
	if (vp->v_flag & VXLOCK) {
		vp->v_flag |= VXWANT;
		tsleep((caddr_t)vp, PINOD, "vgone", 0);
		return;
	}

	/* clean out the filesystem specific data. */
	vclean(vp, DOCLOSE);

	/* delete from old mount point vnode list, if on one. */
	if (vp->v_mountb) {
		if (vq = vp->v_mountf)
			vq->v_mountb = vp->v_mountb;
		*vp->v_mountb = vq;
		vp->v_mountf = NULL;
		vp->v_mountb = NULL;
	}

	/* if a special device, remove it from special device alias list.  */
	if (vp->v_type == VBLK || vp->v_type == VCHR)
		spec_remove_(vp);

	/* if vnode is on the freelist, move it to the head of the list. */
	if (vp->v_freeb) {
		if (vq = vp->v_freef)
			vq->v_freeb = vp->v_freeb;
		else
			vfreet = vp->v_freeb;
		*vp->v_freeb = vq;
		vp->v_freef = vfreeh;
		vp->v_freeb = &vfreeh;
		vfreeh->v_freeb = &vp->v_freef;
		vfreeh = vp;
	}
	vp->v_type = VBAD;
}

#if	defined(DEBUG) || defined(DIAGNOSTIC)
int prtactive = 0;	/* patch to print out reclaim of active vnodes */

/*
 * Print out a description of a vnode.
 */
static char *typename[] =
   { "VNON", "VREG", "VDIR", "VBLK", "VCHR", "VLNK", "VSOCK", "VFIFO", "VBAD" };

void
vprint(char *label, struct vnode *vp)
{
	char buf[64];

	if (label != NULL)
		printf("%s: ", label);
	printf("type %s, usecount %d, writecount %d, refcount %d,",
		typename[vp->v_type], vp->v_usecount, vp->v_writecount,
		vp->v_holdcnt);
	buf[0] = '\0';
	if (vp->v_flag & VROOT)
		strcat(buf, "|VROOT");
	if (vp->v_flag & VTEXT)
		strcat(buf, "|VTEXT");
	if (vp->v_flag & VSYSTEM)
		strcat(buf, "|VSYSTEM");
	if (vp->v_flag & VXLOCK)
		strcat(buf, "|VXLOCK");
	if (vp->v_flag & VXWANT)
		strcat(buf, "|VXWANT");
	if (vp->v_flag & VBWAIT)
		strcat(buf, "|VBWAIT");
	if (vp->v_flag & VALIASED)
		strcat(buf, "|VALIASED");
	if (buf[0] != '\0')
		printf(" flags (%s)", &buf[1]);
	printf("\n\t");
	VOP_PRINT(vp);
}

/*
 * List all of the locked vnodes in the system.
 * Called when debugging the kernel.
 */
void
printlockedvnodes(void)
{
	struct mount *mp;
	struct vnode *vp;

	printf("Locked vnodes\n");
	mp = rootfs;
	do {
		for (vp = mp->mnt_mounth; vp; vp = vp->v_mountf)
			if (VOP_ISLOCKED(vp))
				vprint((char *)0, vp);
		mp = mp->mnt_next;
	} while (mp != rootfs);
}
#endif

int kinfo_vdebug = 1;
int kinfo_vgetfailed;
#define KINFO_VNODESLOP	10

/*
 * Dump vnode list (via kinfo).
 * Copyout address of vnode followed by vnode.
 */
/* ARGSUSED */
int
kinfo_vnode(int op, char *where, int *acopysize, int arg, int *aneeded)
{
	struct mount *mp = rootfs;
	struct mount *omp;
	struct vnode *vp;
	char *bp = where, *savebp;
	char *ewhere = where + *acopysize;
	int error;

#define VPTRSZ	sizeof (struct vnode *)
#define VNODESZ	sizeof (struct vnode)
	if (where == NULL) {
		*aneeded = (numvnodes + KINFO_VNODESLOP) * (VPTRSZ + VNODESZ);
		return (0);
	}
		
	do {
		if (vfs_busy(mp)) {
			mp = mp->mnt_next;
			continue;
		}
		savebp = bp;
again:
		for (vp = mp->mnt_mounth; vp; vp = vp->v_mountf) {
			/*
			 * Check that the vp is still associated with
			 * this filesystem.  RACE: could have been
			 * recycled onto the same filesystem.
			 */
			if (vp->v_mount != mp) {
				if (kinfo_vdebug)
					printf("kinfo: vp changed\n");
				bp = savebp;
				goto again;
			}
			if ((bp + VPTRSZ + VNODESZ <= ewhere) && 
			    ((error = copyout(curproc, (caddr_t)&vp, bp, VPTRSZ)) ||
			     (error = copyout(curproc, (caddr_t)vp, bp + VPTRSZ, 
			      VNODESZ))))
				return (error);
			bp += VPTRSZ + VNODESZ;
		}
		omp = mp;
		mp = mp->mnt_next;
		vfs_unbusy(omp);
	} while (mp != rootfs);

	*aneeded = bp - where;
	if (bp > ewhere)
		*acopysize = ewhere - where;
	else
		*acopysize = bp - where;
	return (0);
}

static struct kinfoif
	kinfo_vnode_kif = { "vnode", KINFO_VNODE, kinfo_vnode };

/* configure servers */
KERNEL_MODCONFIG() {

	kinfo_addserver(&kinfo_vnode_kif);
}

/*
 * Locate vnode operations for a type not implemented by a filesystem.
 * If implemented, reassign the vnode operations vector to suite.
 * Otherwise, indicate that the type is not implemented on this system.
 * If type implies use of an aliased vnode, return the alias.
 */
int
vt_reassign(struct vnode *vp, dev_t dev, struct vnode **aliasvp)
{
	static struct vnodeops sv;
	
	/* XXX need types, per filesystem */
	if (sv.vop_open == 0) {
		struct vnodeops *v = vp->v_op;

		sv = spec_vnodeops;
		sv.vop_access = v->vop_access;
		sv.vop_getattr = v->vop_getattr;
		sv.vop_setattr = v->vop_setattr;
		sv.vop_inactive = v->vop_inactive;
		sv.vop_reclaim = v->vop_reclaim;
		sv.vop_lock = v->vop_lock;
		sv.vop_unlock = v->vop_unlock;
		sv.vop_print = v->vop_print;
		sv.vop_islocked = v->vop_islocked;
	}

	/* fifo vnode type implemented? */
	if (vp->v_type == VFIFO) {

#ifdef FIFO
		extern struct vnodeops fifo_nfsv2nodeops;
		vp->v_op = &fifo_nfsv2nodeops;
#else
		return (EOPNOTSUPP);
#endif /* FIFO */
	}

	/* special device type implemented? */
	if (vp->v_type == VCHR || vp->v_type == VBLK) {

		vp->v_op = &sv;
		*aliasvp = checkalias(vp, dev, vp->v_mount);
	} else
		*aliasvp = 0;
}
