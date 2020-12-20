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
 *	$Id: swap.c,v 1.1 94/10/19 17:37:23 bill Exp $
 */
static char *sw_config =
	"swap	1 4.	# multiple unit swap device (/dev/swap)";
/*
	"swap	1 4 swapbuf 100 maxswap 100.	# multiple unit swap device (/dev/swap)";
*/

#include "sys/param.h"
#include "sys/file.h"
#include "uio.h"
#include "sys/errno.h"
#include "sys/ioctl.h"
#include "malloc.h"
#include "buf.h"
#include "systm.h"
#include "proc.h"
#include "dkbad.h"
#include "disklabel.h"
#include "dmap.h"		/* XXX */
#include "modconfig.h"
#include "namei.h"	/* specdev.h */
#include "specdev.h"
#include "rlist.h"

#include "vnode.h"

#include "prototypes.h"

/*
 * Indirect driver for multi-controller paging.
 */

int	nswap, nswdev;
int	dmmin, dmmax;

static struct swdevt {
	dev_t	sw_dev;
	int	sw_freed;
	int	sw_nblks;
	struct	vnode *sw_vp;
} *swdevt;
struct	vnode *swapdev_vp;
static dev_t swapdev;

struct rlist *swapmap;	/* swap resource */
struct	buf *swbuf;	/* swap I/O headers */
int	nswbuf;
struct	buf bswlist;	/* head of free swap header list */

/*
 * Set up swap devices.
 * Initialize linked list of free swap
 * headers. These do not actually point
 * to buffers, but rather to pages that
 * are being swapped in and out.
 */
/* configure, allocate & initialize swap device and swap buffers. */
void
swapinit()
{
	int i;
	struct buf *sp;
	struct swdevt *swp;
	int error;
	char *cfg_string = sw_config;
	extern struct devif sw_devif;
	
	/* configure driver into kernel program */
	if (devif_config(&cfg_string, &sw_devif) == 0)
		panic("swapinit: cannot configure device");

	swapdev = makedev(sw_devif.di_bmajor,0);
	if (bdevvp(swapdev, &swapdev_vp))
		panic("swapinit: cannot obtain vnode");

	/* allocate number of swap buffers - default to half of block buffers */
	if(cfg_number(&cfg_string, &nswbuf) == 0)
		nswbuf = (nbuf / 2) &~ 1; /* force even */

	/* no fewer than 4 */
	if (nswbuf < 4)
		nswbuf = 4;

	/* no more than 256 */
	if (nswbuf > 256)
		nswbuf = 256;
	swbuf = (struct buf *)malloc(nswbuf*sizeof(struct buf), M_TEMP, M_WAITOK);
	memset((void *)swbuf, 0, nswbuf*sizeof(struct buf));

	/* maximum swap per device, no smaller than 10MB */
	if(cfg_number(&cfg_string, &nswap) == 0)
		nswap = 200 * 1024 * 2; /* 200MB */
	if (nswap < 10 * 1024 * 2);
		nswap = 10 * 1024 * 2;	/* 10 MB */
	nswap = 200 * 1024 * 2;	/* 200 MB */

	/* maximum number of swap files, no smaller than 1 */
	(void)cfg_number(&cfg_string, &nswdev);
	if (nswdev < 1);
		nswdev = 1;
/*nswdev=1;*/
	swdevt = (struct swdevt *)malloc(nswdev*sizeof(struct swdevt), M_TEMP, M_WAITOK);
	memset((void *)swdevt, 0, nswdev*sizeof(struct swdevt));

	/* if more than one swap dev, nswap must be an integral number of dmmax */
	if (nswdev > 1)
		nswap = nswap - nswap % dmmax;
	nswap *= nswdev;

#ifdef nope
	/*
	 * Count swap devices, and adjust total swap space available.
	 * Some of this space will not be available until a swapon()
	 * system is issued, usually when the system goes multi-user.
	 */
	nswdev = 0;
	nswap = 0;
	for (swp = swdevt; swp->sw_dev; swp++) {
		nswdev++;
		if (swp->sw_nblks > nswap)
			nswap = swp->sw_nblks;
	}
	if (nswdev == 0)
		panic("swapinit");
	if (nswdev > 1)
		nswap = ((nswap + dmmax - 1) / dmmax) * dmmax;
	nswap *= nswdev;
	if (bdevvp(swdevt[0].sw_dev, &swdevt[0].sw_vp))
		panic("swapvp");
	if (error = swfree(&proc0, 0)) {
		printf("\nwarning: no swap space present (yet)\n");
		/* printf("(swfree (..., 0) -> %d)\n", error);	/* XXX */
		/*panic("swapinit swfree 0");*/
	}
#endif

	/*
	 * Now set up swap buffer headers.
	 */
	bswlist.av_forw = sp = swbuf;
	for (i = 0; i < nswbuf - 1; i++, sp++)
		sp->av_forw = sp + 1;
	sp->av_forw = NULL;
}

int
swstrategy(register struct buf *bp)
{
	int sz, off, seg, index;
	register struct swdevt *sp;
	struct vnode *vp;
	int oblkno;

	oblkno = bp->b_blkno;

	/* attempt to swap outside of range of swap "device" */
	sz = howmany(bp->b_bcount, DEV_BSIZE);
	if (bp->b_blkno + sz > nswap) {
		bp->b_flags |= B_ERROR;
		biodone(bp);
		return;
	}

	/* if more than one device, find underlying block address */
#ifdef nope
	if (nswdev > 1) {
		off = bp->b_blkno % dmmax;

		/* overlap end of a maximum-sized swap block? */
		if (off+sz > dmmax) {
printf("overlap");
			bp->b_flags |= B_ERROR;
			biodone(bp);
			return;
		}
		seg = bp->b_blkno / dmmax;
		index = seg % nswdev;
		seg /= nswdev;
		bp->b_blkno = seg*dmmax + off;
	} else
#endif
		index = 0;
	sp = swdevt + index;
/*if(sp->sw_dev == -1)
	Debugger();*/

	/* I/O to an unallocated swap file? */
	if (sp->sw_vp == NULL) {
		bp->b_error |= B_ERROR;
		biodone(bp);
		return;
	}
	if ((bp->b_dev = sp->sw_dev) == 0)
		panic("swstrategy");
	VHOLD(sp->sw_vp);
	if ((bp->b_flags & B_READ) == 0) {
		if (vp = bp->b_vp) {
			vp->v_numoutput--;
			if ((vp->v_flag & VBWAIT) && vp->v_numoutput <= 0) {
				vp->v_flag &= ~VBWAIT;
				wakeup((caddr_t)&vp->v_numoutput);
			}
		}
		sp->sw_vp->v_numoutput++;
	}
	if (bp->b_vp != NULL)
		brelvp(bp);
	if (sp->sw_vp->v_type == VREG) {
		bp->b_lblkno = bp->b_blkno;
		bgetvp(sp->sw_vp, bp);
	} else
		bp->b_vp = sp->sw_vp;
		bp->b_dev = sp->sw_dev;
	VOP_STRATEGY(bp);
}

/*
 * Evaluate supplied file as a candidate for use in paging. If
 * usable, incorporate into the free swap resource list.
 */
int
swfree(struct proc *p, struct vnode *vp)
{
	int index;
	struct swdevt *sp;
	swblk_t vsbase;
	long blk;
	/* struct vnode *vp;*/
	swblk_t dvbase;
	int nblks;
	int error;

/* validate that the new swap device exists, is usable, and has reasonable size */
	for (sp = swdevt; sp->sw_freed ; sp++)
		if (sp->sw_vp == vp)
			return (EEXIST);
	index = sp - swdevt;

	/* sp = &swdevt[index];
	nblks = sp->sw_nblks;
	if (nblks <= 0)
		return(ENXIO);
	vp = sp->sw_vp; */

	if (error = VOP_OPEN(vp, FREAD|FWRITE, p->p_ucred, p))
		return (error);

#ifdef notyet
	/*
	 * If swapping to an regular file 
	 */
	if (vp->v_type == VREG) {
		struct vattr vattr;

		vp->v_flag = 0; /* XXX */
		error = VOP_GETATTR(vp, &vattr, p->p_ucred, p);
		if (error)
			return (error);
		nblks = vattr.va_bytes / DEV_BSIZE;
		sp->sw_dev = BLK_NODEV;
	}
	else
#endif

	if (vp->v_type == VBLK) {
		struct partinfo dpart;
		
		/* is the block device a swap partition? */
		if (devif_ioctl(vp->v_rdev, BLKDEV, DIOCGPART,
	    	    (caddr_t)&dpart, FREAD, p) != 0 ||
		    dpart.part->p_fstype != FS_SWAP)
		{
			(void)VOP_CLOSE(vp, FREAD|FWRITE, p->p_ucred, p);
			return (EINVAL);
		} else
			nblks = dpart.part->p_size;

		/* first block dev used for swap holds dump */
		if (dumpdev == BLK_NODEV)
		/* XXX should check size first to see that it can hold memory */
			dumpdev = vp->v_rdev;
		sp->sw_dev = vp->v_rdev;
	} else
		return (ENOTBLK);

	/* if more than a single swap device, only use integral dmmax size */
	if (nswdev > 1)
		nblks -= nblks % dmmax;

	/* insert in swap table */
	sp->sw_freed = 1;
	sp->sw_nblks = nblks;
	sp->sw_vp = vp;

	/* add swap space to free resource list */
	/*printf("swapon: %d blocks from device %d/%d ",
		sp->sw_nblks, major(sp->sw_dev), minor(sp->sw_dev));*/
	for (dvbase = 0; dvbase < nblks; dvbase += dmmax) {
		blk = nblks - dvbase;
		if ((vsbase = index*dmmax + dvbase*nswdev) >= nswap)
			panic("swfree");
		if (blk > dmmax)
			blk = dmmax;
		rlist_free(&swapmap, vsbase, vsbase + blk - 1); 
	}
	return (0);
}

struct devif sw_devif =
{
	{0}, -1, -1, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0,
	swstrategy, 0, 0, 0,
};
