/*
 * Copyright (c) 1991, 1994 William F. Jolitz, TeleMuse
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
 *	This software is a component of "386BSD" developed by 
 *	William F. Jolitz, TeleMuse.
 * 4. Neither the name of the developer nor the name "386BSD"
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 5. Non-commercial distribution of this complete file in either source and/or
 *    binary form at no charge to the user (such as from an official Internet 
 *    archive site) is permitted. 
 * 6. Commercial distribution and sale of this complete file in either source
 *    and/or binary form on any media, including that of floppies, tape, or 
 *    CD-ROM, or through a per-charge download such as that of a BBS, is not 
 *    permitted without specific prior written permission.
 * 7. Non-commercial and/or commercial distribution of an incomplete, altered, 
 *    or otherwise modified file in either source and/or binary form is not 
 *    permitted.
 *
 * THIS SOFTWARE IS A COMPONENT OF 386BSD DEVELOPED BY WILLIAM F. JOLITZ 
 * AND IS INTENDED FOR RESEARCH AND EDUCATIONAL PURPOSES ONLY. THIS 
 * SOFTWARE SHOULD NOT BE CONSIDERED TO BE A COMMERCIAL PRODUCT. 
 * THE DEVELOPER URGES THAT USERS WHO REQUIRE A COMMERCIAL PRODUCT 
 * NOT MAKE USE OF THIS WORK.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPER ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPER BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: bio.c,v 1.1 94/10/19 17:09:15 bill Exp Locker: bill $
 *
 * Un*x styled block transfer buffer filesystem buffer cache.
 */

#include "sys/param.h"
#include "sys/file.h"
#include "sys/mount.h"
#include "sys/errno.h"
#include "proc.h"
#include "uio.h"
#include "buf.h"
#include "specdev.h"
#include "malloc.h"
#include "vm.h"
#include "kmem.h"
#include "resourcevar.h"
#ifdef KTRACE
#include "sys/ktrace.h"
#endif

#include "vnode.h"

#include "prototypes.h"

struct buf *getnewbuf(int);
extern	vm_map_t buffer_map;
int __dwell;
struct	bufhd bufhash[BUFHSZ];	/* heads of hash lists */
struct	buf bfreelist[BQUEUES];	/* heads of available lists */
struct	buf *buf;		/* the buffer pool itself */

/*
 * Initialize buffer headers and related structures.
 */
void
bufinit()
{
	struct bufhd *bh;
	struct buf *bp;

	/* first, make a null hash table */
	for(bh = bufhash; bh < bufhash + BUFHSZ; bh++) {
		bh->b_flags = 0;
		bh->b_forw = (struct buf *)bh;
		bh->b_back = (struct buf *)bh;
	}

	/* next, make a null set of free lists */
	for(bp = bfreelist; bp < bfreelist + BQUEUES; bp++) {
		bp->b_flags = 0;
		bp->av_forw = bp;
		bp->av_back = bp;
		bp->b_forw = bp;
		bp->b_back = bp;
	}

	/* finally, initialize each buffer header and stick on empty q */
	for(bp = buf; bp < buf + nbuf ; bp++) {
		bp->b_flags = B_HEAD | B_INVAL;	/* we're just an empty header */
		bp->b_dev = BLK_NODEV;
		bp->b_vp = 0;
		binstailfree(bp, bfreelist + BQ_EMPTY);
		binshash(bp, bfreelist + BQ_EMPTY);
	}
}

/*
 * Check to see if a block is currently memory resident.
 */
extern inline struct buf *
incore(struct vnode *vp, daddr_t blkno)
{
	struct buf *bh = BUFHASH(vp, blkno), *bp;

	/* search hash chain, looking for a hit. */
	for (bp = bh->b_forw; bp != bh ; bp = bp->b_forw)
		/* hit? */
		if (bp->b_lblkno == blkno && bp->b_vp == vp
		    && (bp->b_flags & B_INVAL) == 0)
			return (bp);
	/* nope, a miss */
	return(0);
}

/*
 * Find the block in the buffer pool.
 * If the buffer is not present, allocate a new buffer and load
 * its contents according to the filesystem fill routine.
 */
int
bread(struct vnode *vp, daddr_t blkno, int size, struct ucred *cred,
	struct buf **bpp)
{
	struct buf *bp;
	int rv = 0;

	bp = getblk (vp, blkno, size);

	/* if not found in cache, do some I/O */
	if ((bp->b_flags & B_CACHE) == 0 || (bp->b_flags & B_INVAL) != 0) {
		bp->b_flags |= B_READ;
		bp->b_flags &= ~(B_DONE|B_ERROR|B_INVAL);
		if (cred != NOCRED)
			crhold(cred);
		bp->b_rcred = cred;
		if (cred != NOCRED)
			crhold(cred);
		bp->b_wcred = cred;
#ifdef KTRACE
		if (curproc && KTRPOINT(curproc, KTR_BIO))
			ktrbio(curproc->p_tracep, KTBIO_READ, vp, blkno);
#endif
		if (curproc)
			curproc->p_stats->p_ru.ru_inblock++;
		VOP_STRATEGY(bp);
		rv = biowait (bp);
	}
	*bpp = bp;
	
	return (rv);
}

/*
 * Operates like bread, but also starts I/O on the specified
 * read-ahead block. [See page 55 of Bach's Book]
 */
int
breada(struct vnode *vp, daddr_t blkno, int size, daddr_t rablkno, int rabsize,
	struct ucred *cred, struct buf **bpp)
{
	struct buf *bp, *rabp;
	int rv = 0, needwait = 0, x;

	bp = getblk (vp, blkno, size);

	/* if not found in cache, do some I/O */
	if ((bp->b_flags & B_CACHE) == 0 || (bp->b_flags & B_INVAL) != 0) {
		bp->b_flags |= B_READ;
		bp->b_flags &= ~(B_DONE|B_ERROR|B_INVAL);
		if (cred != NOCRED)
			crhold(cred);
		bp->b_rcred = cred;
		if (cred != NOCRED)
			crhold(cred);
		bp->b_wcred = cred;
#ifdef KTRACE
		if (curproc && KTRPOINT(curproc, KTR_BIO))
			ktrbio(curproc->p_tracep, KTBIO_READA1, vp, blkno);
#endif
		if (curproc)
			curproc->p_stats->p_ru.ru_inblock++;
		VOP_STRATEGY(bp);
		needwait++;
	}
	
retry:
	/* if not found in cache, allocate and do some I/O (overlapped with first) */
	if ((rabp = incore(vp, rablkno)) == 0) {
		struct buf *bh;

		if ((rabp = getnewbuf(size)) == 0)
			goto retry;
		rabp->b_blkno = rabp->b_lblkno = rablkno;
		bgetvp(vp, rabp);
		x = splbio();
		bh = BUFHASH(vp, rablkno);
		binshash(rabp, bh);
		rabp->b_flags = B_BUSY;
		splx(x);

		/* age block since it has not proven it's effectiveness */
		rabp->b_flags |= B_READ | B_ASYNC | B_AGE;
		if (cred != NOCRED)
			crhold(cred);
		rabp->b_rcred = cred;
		if (cred != NOCRED)
			crhold(cred);
		rabp->b_wcred = cred;
#ifdef KTRACE
		if (curproc && KTRPOINT(curproc, KTR_BIO))
			ktrbio(curproc->p_tracep, KTBIO_READA2, vp, rablkno);
#endif
		if (curproc)
			curproc->p_stats->p_ru.ru_inblock++;
		VOP_STRATEGY(rabp);
	}
	/* block has proven it's reason to be in buffer cache */
	else
	if (rabp->b_flags & B_AGE) {
		/* put on the tail of the age queue */
		if ((rabp->b_flags & B_BUSY) == 0) {
			x = splbio();
			rabp->b_flags |= B_BUSY;
			bremfree(rabp);
			splx(x);
			brelse(rabp);
		} else
			rabp->b_flags &= ~B_AGE;
			
	}
	
	/* wait for original I/O */
	if (needwait)
		rv = biowait (bp);

	*bpp = bp;
	return (rv);
}

/*
 * Synchronous write.
 * Release buffer on completion.
 */
int
bwrite(register struct buf *bp)
{
	int rv;

	if(bp->b_flags & B_INVAL) {
		brelse(bp);
		return (0);
	} else {
		int wasdelayed;

		if(!(bp->b_flags & B_BUSY))
			panic("bwrite: not busy");

		wasdelayed = bp->b_flags & B_DELWRI;
		bp->b_flags &= ~(B_READ|B_DONE|B_ERROR|B_ASYNC|B_DELWRI);
		if(wasdelayed) {
			reassignbuf(bp, bp->b_vp);
			bp->b_flags |= B_AGE;
		} else if (curproc)
			curproc->p_stats->p_ru.ru_oublock++;

		bp->b_flags |= B_DIRTY;
		bp->b_vp->v_numoutput++;
#ifdef KTRACE
		if (curproc && KTRPOINT(curproc, KTR_BIO))
			ktrbio(curproc->p_tracep, KTBIO_WRITE,
				bp->b_vp, bp->b_lblkno);
#endif
		VOP_STRATEGY(bp);
		rv = biowait(bp);
		brelse(bp);
		return (rv);
	}
}

/*
 * Delayed write.
 *
 * The buffer is marked dirty, but is not queued for I/O.
 * This routine should be used when the buffer is expected
 * to be modified again soon, typically a small write that
 * partially fills a buffer.
 *
 * NB: magnetic tapes cannot be delayed; they must be
 * written in the order that the writes are requested.
 */
void
bdwrite(struct buf *bp, struct vnode *vp)
{

	if(!(bp->b_flags & B_BUSY))
		panic("bdwrite: not busy");

	if(bp->b_flags & B_INVAL) {
		brelse(bp);
		return;
	}
	if(bp->b_flags & B_TAPE) {
		bwrite(bp);
		return;
	}

	bp->b_flags &= ~(B_READ|B_DONE);

	/* if first delayed write, account for it */
	if ((bp->b_flags & B_DELWRI) == 0 && curproc) {
		curproc->p_stats->p_ru.ru_oublock++;
		bp->b_flags |= B_DIRTY|B_DELWRI;
		reassignbuf(bp, vp);
	}
	brelse(bp);

#ifdef __DWELL
	/* if there are any delayed writes outstanding on this
	 * vnode, and it isn't just us, force them out. We only
	 * allow a single outstanding delayed write per vnode.
	 */
	if (bp->b_blockb && vp->v_dirtyblkhd &&
		(bp != vp->v_dirtyblkhd || bp->b_blockf != 0))
		/* eventually, should bawrite all of them and tsleep */
		vflushbuf(vp, 0, bp);
#endif /* __DWELL */
}

/*
 * Asynchronous write.
 * Start I/O on a buffer, but do not wait for it to complete.
 * The buffer is released when the I/O completes.
 */
void
bawrite(register struct buf *bp)
{

	if(!(bp->b_flags & B_BUSY))
		panic("bawrite: not busy");

	if(bp->b_flags & B_INVAL)
		brelse(bp);
	else {
		int wasdelayed;

		wasdelayed = bp->b_flags & B_DELWRI;
		bp->b_flags &= ~(B_READ|B_DONE|B_ERROR|B_DELWRI);
		if(wasdelayed) {
			reassignbuf(bp, bp->b_vp);
			bp->b_flags |= B_AGE;
		}
		else if (curproc)
			curproc->p_stats->p_ru.ru_oublock++;

		bp->b_flags |= B_DIRTY | B_ASYNC;
		bp->b_vp->v_numoutput++;
#ifdef KTRACE
		if (curproc && KTRPOINT(curproc, KTR_BIO))
			ktrbio(curproc->p_tracep,
			    wasdelayed ? KTBIO_DTOA: KTBIO_AWRITE,
			    bp->b_vp, bp->b_lblkno);
#endif
		VOP_STRATEGY(bp);
	}
}

/*
 * Release a buffer.
 * Even if the buffer is dirty, no I/O is started.
 */
void
brelse(register struct buf *bp)
{
	int x, flags = bp->b_flags;

#ifdef KTRACE
	if (curproc && KTRPOINT(curproc, KTR_BIO))
		ktrbio(curproc->p_tracep,
		    (bp->b_flags & B_INVAL) ? KTBIO_INVALID :
			((bp->b_flags & B_DELWRI) ? KTBIO_MODIFY :
			    KTBIO_RELEASE),
		    bp->b_vp, bp->b_lblkno);
#endif
	/* anyone need a "free" block? */
	x=splbio();
	if ((bfreelist + BQ_AGE)->b_flags & B_WANTED) {
		(bfreelist + BQ_AGE) ->b_flags &= ~B_WANTED;
		wakeup((caddr_t)bfreelist);
	}
	/* anyone need this very block? */
	if (bp->b_flags & B_WANTED) {
		bp->b_flags &= ~B_WANTED;
		wakeup((caddr_t)bp);
	}

	if (bp->b_flags & (B_INVAL|B_ERROR|B_NOCACHE)) {
		if(bp->b_vp)
			brelvp(bp);
		bp->b_flags = B_INVAL;
		flags = B_INVAL;
	}

	bp->b_flags &= ~(B_CACHE|B_NOCACHE|B_AGE);
	/* enqueue */
	/* just an empty buffer head ... */
	/*if(flags & B_HEAD)
		binsheadfree(bp, bfreelist + BQ_EMPTY)*/
	/* buffers with junk contents */
	/*else*/ if(flags & (B_ERROR|B_INVAL|B_NOCACHE))
		binsheadfree(bp, bfreelist + BQ_AGE)
	/* buffers with stale but valid contents */
	else if((flags & (B_CACHE|B_AGE)) == B_AGE) {
		binstailfree(bp, bfreelist + BQ_AGE)
		bp->b_flags |= B_AGE;
	/* buffers with valid and quite potentially reuseable contents */
	}
	else
		binstailfree(bp, bfreelist + BQ_LRU)

	/* unlock */
	bp->b_flags &= ~B_BUSY;
	splx(x);

}

int freebufspace;
int allocbufspace;

/*
 * Find a buffer which is available for use.
 * If free memory for buffer space and an empty header from the empty list,
 * use that. Otherwise, select something from a free list.
 * Preference is to AGE list, then LRU list.
 */
extern caddr_t bufspace_addr; /* base address of buffer space */
/*static*/ struct buf *
getnewbuf(int sz)
{
	struct buf *bp;
	int x;

	x = splbio();
start:
	/* can we constitute a new buffer? */
	if (/*freebufspace > sz
		&&*/ bfreelist[BQ_EMPTY].av_forw != (struct buf *)bfreelist+BQ_EMPTY) {
		caddr_t addr;

		bp = bfreelist[BQ_EMPTY].av_forw;
		if ((sz % NBPG) == 0)
			addr = kmem_alloc(buf_map, sz, M_NOWAIT);
		else
			addr = malloc(sz, M_TEMP, M_NOWAIT);

		if (addr == 0)
			goto tryfree;
		/*freebufspace -= sz;
		allocbufspace += sz;*/


		bp->b_flags = B_BUSY | B_INVAL;
		bremfree(bp);
		bp->b_un.b_addr = addr;
		bp->b_bufsize = sz;	/* 20 Aug 92*/
		goto fillin;
	}

tryfree:
	if (bfreelist[BQ_AGE].av_forw != (struct buf *)bfreelist+BQ_AGE) {
		bp = bfreelist[BQ_AGE].av_forw;
		bremfree(bp);
	} else if (bfreelist[BQ_LRU].av_forw != (struct buf *)bfreelist+BQ_LRU) {
		bp = bfreelist[BQ_LRU].av_forw;
		bremfree(bp);
	} else	{
		/* wait for a free buffer of any kind */
		(bfreelist + BQ_AGE)->b_flags |= B_WANTED;
		tsleep((caddr_t)bfreelist, PRIBIO, "getnewbuf", 0);
		/*splx(x);
		return (0);*/
		goto start;
	}

	/* if we are a delayed write, convert to an async write! */
	if (bp->b_flags & B_DELWRI) {
		bp->b_flags |= B_BUSY;
		bawrite (bp);
		goto start;
	}

#ifdef KTRACE
	if (curproc && KTRPOINT(curproc, KTR_BIO))
		ktrbio(curproc->p_tracep, KTBIO_RECYCLE, bp->b_vp,
			bp->b_lblkno);
#endif

	if(bp->b_vp)
		brelvp(bp);

	/* we are not free, nor do we contain interesting data */
	if (bp->b_rcred != NOCRED)
		crfree(bp->b_rcred);
	if (bp->b_wcred != NOCRED)
		crfree(bp->b_wcred);
	bp->b_flags = B_INVAL|B_BUSY;
fillin:
	bremhash(bp);
	splx(x);
	bp->b_dev = BLK_NODEV;
	bp->b_vp = NULL;
	bp->b_blkno = bp->b_lblkno = 0;
	bp->b_iodone = 0;
	bp->b_error = 0;
	bp->b_wcred = bp->b_rcred = NOCRED;
	if (bp->b_bufsize != sz)
		allocbuf(bp, sz);
	bp->b_bcount = bp->b_bufsize = sz;
	bp->b_dirtyoff = bp->b_dirtyend = 0;
	bp->b_blockb = 0;
	bp->b_blockf = 0;
	return (bp);
}

/*
 * Get a block of requested size that is associated with
 * a given vnode and block offset. If it is found in the
 * block cache, mark it as having been found, make it busy
 * and return it. Otherwise, return an empty block of the
 * correct size. It is up to the caller to insure that the
 * cached blocks be of the correct size.
 */
struct buf *
getblk(register struct vnode *vp, daddr_t blkno, int size)
{
	struct buf *bp, *bh;
	int x;

	for (;;) {
		if (bp = incore(vp, blkno)) {
			x = splbio();
			if (bp->b_flags & B_BUSY) {
#ifdef KTRACE
				if (curproc && KTRPOINT(curproc, KTR_BIO))
					ktrbio(curproc->p_tracep, KTBIO_BUSY,
					    vp, blkno);
#endif
				bp->b_flags &= ~B_AGE;
				bp->b_flags |= B_WANTED | B_CACHE;
				tsleep ((caddr_t)bp, PRIBIO, "getblk", 0);
				splx(x);
				continue;
			}
#ifdef KTRACE
			if (curproc && KTRPOINT(curproc, KTR_BIO))
				ktrbio(curproc->p_tracep, KTBIO_REFERENCE,
				    vp, blkno);
#endif
			bp->b_flags &= ~B_AGE;
			bp->b_flags |= B_BUSY | B_CACHE;
			bremfree(bp);
			if (bp->b_bufsize != size)
				allocbuf(bp, size);
		} else {

			if ((bp = getnewbuf(size)) == 0)
				continue;
			bp->b_blkno = bp->b_lblkno = blkno;
			bgetvp(vp, bp);
			x = splbio();
			bh = BUFHASH(vp, blkno);
			binshash(bp, bh);
			bp->b_flags = B_BUSY;
		}
		splx(x);
		return (bp);
	}
}



/*
 * Get an empty, disassociated buffer of given size.
 */
struct buf *
geteblk(int size)
{
	struct buf *bp;
	int x;

	while ((bp = getnewbuf(size)) == 0)
		;
	x = splbio();
	binshash(bp, bfreelist + BQ_AGE);
	splx(x);

	return (bp);
}

/*
 * Exchange a buffer's underlying buffer storage for one of different
 * size, taking care to maintain contents appropriately. When buffer
 * increases in size, caller is responsible for filling out additional
 * contents. When buffer shrinks in size, data is lost, so caller must
 * first return it to backing store before shrinking the buffer, as
 * no implied I/O will be done.
 *
 * Expanded buffer is returned as value.
 */
void
allocbuf(register struct buf *bp, int size)
{
	caddr_t newcontents;

	/* get new memory buffer */
	if (size % NBPG != 0)
		newcontents = (caddr_t) malloc (size, M_TEMP, M_WAITOK);
	else
		newcontents = kmem_alloc(buf_map, size, 0);

	if (bp->b_un.b_addr != newcontents)
		/* copy the old into the new, up to the maximum that will fit */
		(void) memcpy (newcontents, bp->b_un.b_addr, min(bp->b_bufsize, size));

	/* return old contents to free heap */
	if (bp->b_bufsize % NBPG != 0)
		free (bp->b_un.b_addr, M_TEMP);
	else
		kmem_free(buf_map, bp->b_un.b_addr, bp->b_bufsize);

	/* adjust buffer cache's idea of memory allocated to buffer contents */
	/*freebufspace -= size - bp->b_bufsize;
	allocbufspace += size - bp->b_bufsize;*/

	/* update buffer header */
	bp->b_un.b_addr = newcontents;
	bp->b_bcount = bp->b_bufsize = size;
}

/*
 * Patiently await operations to complete on this buffer.
 * When they do, extract error value and return it.
 * Extract and return any errors associated with the I/O.
 * If an invalid block, force it off the lookup hash chains.
 */
int
biowait(register struct buf *bp)
{
	int x;

	x = splbio();
	while ((bp->b_flags & B_DONE) == 0) {
		char *msg;

		if (curproc->p_flag & SPAGE)
			msg = "biowaitpg";
		else
			msg = "biowait";

		tsleep((caddr_t)bp, PRIBIO, msg, 0);
	}
#ifdef KTRACE
	if (curproc && KTRPOINT(curproc, KTR_BIO))
		ktrbio(curproc->p_tracep, KTBIO_WAIT, bp->b_vp,
			bp->b_lblkno);
#endif
	if((bp->b_flags & B_ERROR) || bp->b_error) {
		if ((bp->b_flags & B_INVAL) == 0) {
			bp->b_flags |= B_INVAL;
			bremhash(bp);
			binshash(bp, bfreelist + BQ_AGE);
		}
		if (!bp->b_error)
			bp->b_error = EIO;
		else
			bp->b_flags |= B_ERROR;
		splx(x);
		return (bp->b_error);
	} else {
		splx(x);
		return (0);
	}
}

/*
 * Finish up operations on a buffer, calling an optional
 * function (if requested), and releasing the buffer if
 * marked asynchronous. Then mark this buffer done so that
 * others biowait()'ing for it will notice when they are
 * woken up from sleep().
 */
void
biodone(register struct buf *bp)
{
	int x;

	x = splbio();
	if (bp->b_flags & B_CALL) {
		(*bp->b_iodone)(bp);
		bp->b_flags &= ~B_CALL;
	}
	if ((bp->b_flags & (B_READ|B_DIRTY)) == B_DIRTY)  {
		bp->b_flags &= ~B_DIRTY;
		vwakeup(bp);
	}
	if (bp->b_flags & B_ASYNC)
		brelse(bp);
	bp->b_flags &= ~(B_ASYNC|B_ORDER);
	bp->b_flags |= B_DONE;
	wakeup((caddr_t)bp);
	splx(x);
}

#ifdef DEBUG
printbp(struct buf *b) {

	if(b->b_flags & B_NOCACHE) printf("NO_CACHE ");
	if(b->b_flags & B_RAW) printf("RAW ");
	if(b->b_flags & B_CALL) printf("CALL ");
	if(b->b_flags & B_BAD) printf("BAD ");
	if(b->b_flags & B_HEAD) printf("HEAD ");
	if(b->b_flags & B_LOCKED) printf("LOCKED ");
	if(b->b_flags & B_INVAL) printf("INVAL ");
	if(b->b_flags & B_CACHE) printf("CACHE ");
	if(b->b_flags & B_PGIN) printf("PGIN ");
	if(b->b_flags & B_DIRTY) printf("DIRTY ");
	if(b->b_flags & B_MALLOC) printf("MALLOC ");
	if(b->b_flags & B_VMPAGE) printf("VMPAGE ");
	if(b->b_flags & B_TAPE) printf("TAPE ");
	if(b->b_flags & B_DELWRI) printf("DELWRI ");
	if(b->b_flags & B_ASYNC) printf("ASYNC ");
	if(b->b_flags & B_AGE) printf("AGE ");
	if(b->b_flags & B_WANTED) printf("WANTED ");
	if(b->b_flags & B_PHYS) printf("PHYS ");
	if(b->b_flags & B_BUSY) printf("BUSY ");
	if(b->b_flags & B_ERROR) printf("ERROR ");
	if((b->b_flags & B_DONE)) printf("DONE ");
	if(b->b_flags & B_READ) printf("READ "); else printf("WRITE ");
	printf("\t%d %x %d %d 0x%x\n", 
		b->b_bcount, b->b_dev, b->b_lblkno, b->b_blkno, b->b_vp);
}
#endif
