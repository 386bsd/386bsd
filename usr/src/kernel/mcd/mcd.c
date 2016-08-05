/*
 * Copyright 1993 by Holger Veit (data part)
 * Copyright 1993 by Brian Moore (audio part)
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
 *	This software was developed by Holger Veit and Brian Moore
 *      for use with "386BSD" and similar operating systems.
 *    "Similar operating systems" includes mainly non-profit oriented
 *    systems for research and education, including but not restricted to
 *    "NetBSD", "FreeBSD", "Mach" (by CMU).
 * 4. Neither the name of the developer(s) nor the name "386BSD"
 *    may be used to endorse or promote products derived from this 
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPER(S) ``AS IS'' AND ANY 
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPER(S) BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, 
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	@(#) $RCSfile: mcd.c,v $	$Revision: 1.8 $ $Date: 95/01/09 14:15:05 $
 */
static char COPYRIGHT[] = "mcd-driver (C)1993 by H.Veit & B.Moore";
static char *mcd_conf =
	"mcd 5 14 (0x300 10).	# mitsumi CDROM $Revision: 1.8 $";

#define NMCD 1
#include "sys/types.h"
#include "sys/param.h"
#include "systm.h"
#include "sys/file.h"
#include "proc.h"
#include "buf.h"
#include "sys/stat.h"
#include "uio.h"
#include "sys/ioctl.h"
#include "cdio.h"
#include "sys/errno.h"
#include "dkbad.h"
#include "disklabel.h"
#include "isa_irq.h"
#include "machine/icu.h"
#include "isa_driver.h"
#include "mcdreg.h"
#include "kernel.h" /* for lbolt */
#include "modconfig.h"
#include "prototypes.h"
#include "machine/inline/io.h"	/* inline i/o port functions */

/* user definable options */
/*#define MCD_TO_WARNING_ON*/	/* define to get timeout messages */
/*#define MCDMINI*/ 		/* define for a mini configuration for boot kernel */


#ifdef MCDMINI
#define MCD_TRACE(fmt,a,b,c,d)
#ifdef MCD_TO_WARNING_ON
#undef MCD_TO_WARNING_ON
#endif
#else
#define MCD_TRACE(fmt,a,b,c,d)	{if (mcd_data[unit].debug) {printf("mcd%d st=%02x: ",unit,mcd_data[unit].status); printf(fmt,a,b,c,d);}}
#endif

#define mcd_part(dev)	((minor(dev)) & 7)
#define mcd_unit(dev)	(((minor(dev)) & 0x38) >> 3)
#define mcd_phys(dev)	(((minor(dev)) & 0x40) >> 6)
#define RAW_PART	3

/* flags */
#define MCDOPEN		0x0001	/* device opened */
#define MCDVALID	0x0002	/* parameters loaded */
#define MCDINIT		0x0004	/* device is init'd */
#define MCDWAIT		0x0008	/* waiting for something */
#define MCDLABEL	0x0010	/* label is read */
#define	MCDPROBING	0x0020	/* probing */
#define	MCDREADRAW	0x0040	/* read raw mode (2352 bytes) */
#define	MCDVOLINFO	0x0080	/* already read volinfo */
#define	MCDTOC		0x0100	/* already read toc */
#define	MCDMBXBSY	0x0200	/* local mbx is busy */
#define	MCDMBXRD	0x0400	/* local mbx is in doread */
#define	MCDDS		0x0800	/* double speed drive */
#define	MCDMBXTIMEO	0x1000	/* timeout */
#define	MCDMBXRTRY	0x2000	/* retrying */
#define	MCDMBXSTUCK	0x4000	/* possibly stuck */

/* status */
#define	MCDAUDIOBSY	MCD_ST_AUDIOBSY		/* playing audio */
#define MCDDSKCHNG	MCD_ST_DSKCHNG		/* sensed change of disk */
#define MCDDSKIN	MCD_ST_DSKIN		/* sensed disk in drive */
#define MCDDOOROPEN	MCD_ST_DOOROPEN		/* sensed door open */

/* toc */
#define MCD_MAXTOCS	104	/* from the Linux driver */
#define MCD_LASTPLUS1   170     /* special toc entry */

struct mcd_mbx {
	short		unit;
	short		port;
	short		retry;
	short		nblk;
	int		sz;
	u_long		skip;
	struct buf	*bp;
	int		p_offset;
	short		count;
};

struct mcd_data {
	short	config;
	short	flags;
	short	status;
	int	blksize;
	u_long	disksize;
	int	iobase;
	struct disklabel dlabel;
	int	partflags[MAXPARTITIONS];
	int	openflags;
	struct mcd_volinfo volinfo;
#ifndef MCDMINI
	struct mcd_qchninfo toc[MCD_MAXTOCS];
	short	audio_status;
	struct mcd_read2 lastpb;
#endif
	short	debug;
	struct buf head;		/* head of buf queue */
	struct mcd_mbx mbx;
} mcd_data[NMCD];

/* reader state machine */
#define MCD_S_BEGIN	0
#define MCD_S_BEGIN1	1
#define MCD_S_WAITSTAT	2
#define MCD_S_WAITMODE	3
#define MCD_S_WAITREAD	4

/* prototypes */
static int mcdopen(dev_t dev, int flags, int fmt, struct proc *p);
static int mcdclose(dev_t dev, int flags, int fmt, struct proc *p);
static int	mcdstrategy(struct buf *bp);
static int	mcdioctl(dev_t dev, int cmd, caddr_t addr, int flags, struct proc *p);
static	void	mcd_done(struct mcd_mbx *mbx);
static	void	mcd_start(int unit);
static int	mcdsize(dev_t dev);
static	int	mcd_getdisklabel(int unit);
static	void	mcd_configure(struct mcd_data *cd);
static	int	mcd_get(int unit, char *buf, int nmax);
static	void	mcd_setflags(int unit,struct mcd_data *cd);
static	int	mcd_getstat(int unit,int sflg);
static	int	mcd_send(int unit, int cmd,int nretrys);
static	int	bcd2bin(bcd_t b);
static	bcd_t	bin2bcd(int b);
static	void	hsg2msf(int hsg, bcd_t *msf);
static	int	msf2hsg(bcd_t *msf);
static	int	mcd_volinfo(int unit);
static	int	mcd_waitrdy(int port,int dly);
static 	void	mcd_doread(int state, struct mcd_mbx *mbxin);
#ifndef MCDMINI
static	int 	mcd_setmode(int unit, int mode);
static	int	mcd_getqchan(int unit, struct mcd_qchninfo *q);
static	int	mcd_subchan(int unit, struct ioc_read_subchannel *sc);
static	int	mcd_toc_header(int unit, struct ioc_toc_header *th);
static	int	mcd_read_toc(int unit);
static	int	mcd_toc_entry(int unit, struct ioc_read_toc_entry *te);
static	int	mcd_stop(int unit);
static	int	mcd_playtracks(int unit, struct ioc_play_track *pt);
static	int	mcd_play(int unit, struct mcd_read2 *pb);
static	int	mcd_pause(int unit);
static	int	mcd_resume(int unit);
#endif
static void mcd_setport(int unit, int port, u_char data);
static u_char mcd_getport(int unit, int port);

extern	int	hz;
static	int	mcd_probe(struct isa_device *dev);
static	void	mcd_attach(struct isa_device *dev);
static	void	mcdintr(int unit);
struct	isa_driver	mcddriver = { mcd_probe, mcd_attach, mcdintr, "mcd", &biomask};

static int mcd_drv_rdy(int unit);
#define mcd_put(port,byte)	outb(port,byte)

#define MCD_RETRYS	5
#define MCD_RDRETRYS	8

#define MCDBLK	2048	/* for cooked mode */
#define MCDRBLK	2352	/* for raw mode */

/* several delays */
#define RDELAY_WAITSTAT	30
#define RDELAY_WAITMODE	30
#define RDELAY_WAITREAD	40

#define DELAY_STATUS	10000l		/* 10000 * 1us */
#define DELAY_GETREPLY	200000l		/* 200000 * 2us */
#define DELAY_SEEKREAD	20000l		/* 20000 * 1us */
#define mcd_delay	DELAY

static void
mcd_attach(struct isa_device *dev)
{
	struct mcd_data *cd = mcd_data + dev->id_unit;
	int i, port;
	char vers[2];
	
	port = cd->iobase = dev->id_iobase;
	cd->flags = MCDINIT;
	cd->openflags = 0;
	for (i=0; i<MAXPARTITIONS; i++) cd->partflags[i] = 0;

#ifdef nope
	(void)mcd_getstat(dev->id_unit, 1);
	/* kind of drive */
	if (mcd_send(dev->id_unit, MCD_CMDGETVERS, 6) < 0 ||
	    mcd_get(dev->id_unit, vers, sizeof vers) != sizeof vers)
		printf("can't get drive version\n");
	/* presume future versions are at least double speed ... */
	else {
		/* if (vers[0] != MCD_VER_LU && vers[0] != MCD_VER_FX)*/
		if (vers[0] == MCD_VER_FXD)
			cd->flags |= MCDDS;
		printf("version %c rev %x", vers[0], (unsigned)vers[1]);
	}
#endif
		
	/* seek retrys */
	outb(port+mcd_Data, MCD_SETPARAMS);
	outb(port+mcd_Data, MCD_SP_SRETRYS);
	outb(port+mcd_Data, 3);
	(void)mcd_getstat(dev->id_unit, 0);

	/* non dma mode */
	outb(port+mcd_Data, MCD_SETPARAMS);
	outb(port+mcd_Data, MCD_SP_MODE);
	outb(port+mcd_Data, MCDSP_PIO);	/* ??? */
	(void)mcd_getstat(dev->id_unit, 0);

	/* ask for interrupt before transfer instead of after */
	outb(port+mcd_Data, MCD_SETPARAMS);
	outb(port+mcd_Data, MCD_SP_INTR);
	outb(port+mcd_Data, MCDSP_PRE);
	(void)mcd_getstat(dev->id_unit, 0);

#ifdef notyet
	/* XXX: set data length 2+ bytes bigger than sector */
	outb(port+mcd_Data, MCD_SETPARAMS);
	outb(port+mcd_Data, MCD_SP_DLEN);
#ifdef notyet
	outb(port+mcd_Data, 0x8);
	outb(port+mcd_Data, 0x1);
#else
	outb(port+mcd_Data, 0x7);
	outb(port+mcd_Data, 0xff);
#endif
	(void)mcd_getstat(dev->id_unit, 0);
#endif

	/* set timeout to 36ms */
	outb(port+mcd_Data, MCD_SETPARAMS);
	outb(port+mcd_Data, MCD_SP_TIMEO);
	outb(port+mcd_Data, 18);
	(void)mcd_getstat(dev->id_unit, 0);

#ifdef NOTYET
	/* wire controller for interrupts and dma */
	mcd_configure(cd);
#endif
}

static void mcd_monitor(void);

static int
mcdopen(dev_t dev, int flags, int fmt, struct proc *p)
{
	int unit,part,phys;
	struct mcd_data *cd;
	int rv;
	static int monitor;
	
	unit = mcd_unit(dev);
	if (unit >= NMCD)
		return ENXIO;

	cd = mcd_data + unit;
	part = mcd_part(dev);
	phys = mcd_phys(dev);
	
	/* not initialized*/
	if (!(cd->flags & MCDINIT))
		return ENXIO;

	/* invalidated in the meantime? mark all open part's invalid */
	if (!(cd->flags & MCDVALID) && cd->openflags)
		return ENXIO;

	if (monitor == 0) {
		monitor++;
		timeout((int (*)())mcd_monitor, (caddr_t)0, hz/2);
	}

	/* mountable block device presumes non-audio media is present */
	if (fmt == S_IFBLK)
{ int i, sts, error;
	while ((sts = mcd_getstat(unit, 1)) < 0 ||
	    (sts & (MCD_ST_DOOROPEN | MCD_ST_DSKIN | MCD_ST_DSKCHNG | MCD_ST_AUDIOBSY))
		!= MCD_ST_DSKIN) {
		if (error = tsleep((caddr_t)&lbolt, PRIBIO|PCATCH, "mcdopen", 0))
			return (error);
	}
}

	/* XXX get a default disklabel */
	mcd_getdisklabel(unit);
{ int i;
for (i = 1 ; i < 20; i++) {
	if ((rv = mcdsize(dev))>= 0)
		break;
	DELAY((1<<i) * 1000);
	printf("%d", i);
}
}
	if (rv < 0) {
		printf("mcd%d: failed to get disk size\n",unit);
		return ENXIO;
	} else
		cd->flags |= MCDVALID;

MCD_TRACE("open: partition=%d, disksize = %d, blksize=%d\n",
	part,cd->disksize,cd->blksize,0);

	if (part == RAW_PART || 
	  (part < cd->dlabel.d_npartitions &&
	   cd->dlabel.d_partitions[part].p_fstype != FS_UNUSED)) {
		cd->partflags[part] |= MCDOPEN;
		cd->openflags |= (1<<part);
		if (part == RAW_PART && phys != 0)
			cd->partflags[part] |= MCDREADRAW;
/*cd->debug=0;*/
		/* mountable block device presumes media won't change */
		if (fmt == S_IFBLK) {
			int port = cd->iobase;	

			mcd_put(port+mcd_command, MCD_CMDLOCKDRV);
			mcd_put(port+mcd_command, MCD_LK_LOCK);
			(void)mcd_getstat(unit, 0);
		}
		return 0;
	}
	
	return ENXIO;
}

static int
mcdclose(dev_t dev, int flags, int fmt, struct proc *p)
{
	int unit,part,phys;
	struct mcd_data *cd;
	
	unit = mcd_unit(dev);
	if (unit >= NMCD)
		return ENXIO;

	cd = mcd_data + unit;
	part = mcd_part(dev);
	phys = mcd_phys(dev);
	
	if (!(cd->flags & MCDINIT))
		return ENXIO;

	/* mountable block device presumes media won't change */
	if (fmt == S_IFBLK) {
		int port = cd->iobase;	

		mcd_put(port+mcd_command, MCD_CMDLOCKDRV);
		mcd_put(port+mcd_command, MCD_LK_UNLOCK);
		(void)mcd_getstat(unit, 0);
	}

	(void) mcd_getstat(unit,1);	/* get status */

	/* close channel */
	cd->partflags[part] &= ~(MCDOPEN|MCDREADRAW);
	cd->openflags &= ~(1<<part);
	MCD_TRACE("close: partition=%d\n",part,0,0,0);

	return 0;
}

static int
mcdstrategy(struct buf *bp) 
{
	struct mcd_data *cd;
	struct buf *qp;
	int s;
	
	int unit = mcd_unit(bp->b_dev);

	cd = mcd_data + unit;

	/* test validity */
MCD_TRACE("strategy: buf=0x%lx, unit=%ld, block#=%ld bcount=%ld\n",
	bp,unit,bp->b_blkno,bp->b_bcount);
	if (unit >= NMCD || bp->b_blkno < 0) {
		printf("mcdstrategy: unit = %d, blkno = %d, bcount = %d\n",
			unit, bp->b_blkno, bp->b_bcount);
		printf("mcd: mcdstratregy failure");
		bp->b_error = EINVAL;
		bp->b_flags |= B_ERROR;
		goto bad;
	}

	/* if device invalidated (e.g. media change, door open), error */
	if (!(cd->flags & MCDVALID)) {
MCD_TRACE("strategy: drive not valid\n",0,0,0,0);
		bp->b_error = EIO;
		goto bad;
	}

	/* read only */
	if (!(bp->b_flags & B_READ)) {
		bp->b_error = EROFS;
		goto bad;
	}
	
	/* no data to read */
	if (bp->b_bcount == 0)
		goto done;
	
	/* for non raw access, check partition limits */
	if (mcd_part(bp->b_dev) != RAW_PART) {
		if (!(cd->flags & MCDLABEL)) {
			bp->b_error = EIO;
			goto bad;
		}
		/* adjust transfer if necessary */
		if (bounds_check_with_label(bp,&cd->dlabel,1) <= 0) {
			goto done;
		}
	}
	
	/* queue it */
	qp = &cd->head;
	s = splbio();
	disksort(qp,bp);
	splx(s);
	
	/* now check whether we can perform processing */
	mcd_start(unit);
	return;

bad:
	bp->b_flags |= B_ERROR;
done:
	bp->b_resid = bp->b_bcount;
	biodone(bp);
	return;
}

static mcd_expire(int unit);
static mcd_pollintr(int unit);
static int mcd_getreply(int unit,int dly);
static void
mcd_start(int unit)
{
	struct mcd_data *cd = mcd_data + unit;
	struct buf *bp, *qp = &cd->head;
	struct partition *p;
	int part;
	int s = splbio();
	struct mcd_mbx *mbx = &cd->mbx;
	int port;

	int	rm,i,k, x, sts;
	struct mcd_read2 rbuf;
	int	blknum;
	caddr_t	addr;

restart:	
	/* transfer in progress */
	if (cd->flags & MCDMBXBSY) {
		splx(s);
		return;
	}

	/* issue a retry */
	if (cd->flags & MCDMBXRTRY) {
printf("retry ");
		cd->flags |= MCDMBXBSY;
		bp = mbx->bp;
		port = mbx->port;
		goto retry;
	}

	if ((bp = qp->b_actf) != 0) {
		/* block found to process, dequeue */
		/*MCD_TRACE("mcd_start: found block bp=0x%x\n",bp,0,0,0);*/
		qp->b_actf = bp->av_forw;
		splx(s);
	} else {
		/* nothing to do */
		splx(s);
		return;
	}

	cd->flags |= MCDMBXBSY;

	/* changed media? */
	if (!(cd->flags	& MCDVALID)) {
		MCD_TRACE("mcd_start: drive not valid\n",0,0,0,0);
readerr:
		bp->b_flags |= B_ERROR;
		bp->b_resid = bp->b_bcount;
		biodone(bp);
		cd->flags &= ~MCDMBXBSY;
		splx(s);
		goto restart;
	}

	p = cd->dlabel.d_partitions + mcd_part(bp->b_dev);

	cd->mbx.unit = unit;
	port = cd->mbx.port = cd->iobase;
	cd->mbx.retry = MCD_RDRETRYS;
	cd->mbx.bp  = bp;
	cd->mbx.p_offset = p->p_offset;
	cd->mbx.skip = 0;

retry:
	mbx->count = RDELAY_WAITSTAT;

	/* check drive state */
	for(i=1 ; i <5; i++)
		if ((i = mcd_getstat(unit, 1)) > 0)
			break;

	if((i&(MCD_ST_DOOROPEN|MCD_ST_DSKIN|MCD_ST_DSKSPIN)) == MCD_ST_DSKIN) {
		if (mcd_send(unit, MCD_CMDSTOP, MCD_RETRYS) < 0)
			printf("can't stop ");
		else if (mcd_send(unit, MCD_CMDSTOPAUDIO, MCD_RETRYS) < 0)
			printf("can't pause ");
		else	printf("cha-cha");
		printf("%x ", i = mcd_getstat(unit,1));
	}
	/*mcd_setflags(unit,cd);*/

	/* reject, if audio active */
	if (cd->status & MCDAUDIOBSY) {
		printf("mcd%d: audio is active %x\n",unit, cd->status);
		/* printf("%x ", i = mcd_getstat(unit,1));
			goto readerr;*/
	}

	/*
	 * select drive mode and transfer size
	 */

	/* to check for raw/cooked mode */
	if (cd->flags & MCDREADRAW) {
		rm = MCD_MD_RAW;
		mbx->sz = MCDRBLK;
	} else {
		rm = MCD_MD_COOKED;
		mbx->sz = cd->blksize;
	}

	mcd_put(port+mcd_command, MCD_CMDSETMODE);
	mcd_put(port+mcd_command, rm);

	if (mcd_getreply(unit,DELAY_GETREPLY) < 0) {
		printf("sm");
		if (mcd_getreply(unit,DELAY_GETREPLY) < 0)
		printf("2");
	}

	mcd_setflags(unit,cd);

	/*
	 * calculate sector address and other command paramters
	 */

	/* for first sector in the block */
	if (mbx->skip == 0) 
		mbx->nblk = (bp->b_bcount + (mbx->sz-1)) / mbx->sz;
	blknum = (bp->b_blkno / (mbx->sz/DEV_BSIZE)) + mbx->p_offset + mbx->skip/mbx->sz;

	MCD_TRACE("mcdstart: read blknum=%d for bp=0x%x\n",blknum,bp,0,0);

	/* build parameter block */
	hsg2msf(blknum, rbuf.start_msf);

	/* k = inb(port+mcd_xfer);
	if((k&0xf) != 0xf)
		failcmd = 1;
	else
		failcmd = 0;
 char mcd_readcmd[7] = { MCD_CMDREAD2, 0,0,0, 0,0,1 } ;
	insb(port+mcd_command, rbuf.start_msf, */
	/* send the read command */
	if (cd->flags & MCDDS)
		mcd_put(port+mcd_command, MCD_CMDREAD2D);
	else
		mcd_put(port+mcd_command, MCD_CMDREAD2);
	mcd_put(port+mcd_command,rbuf.start_msf[0]);
	mcd_put(port+mcd_command,rbuf.start_msf[1]);
	mcd_put(port+mcd_command,rbuf.start_msf[2]);
	mcd_put(port+mcd_command,0);
	mcd_put(port+mcd_command,0);
	mcd_put(port+mcd_command,1);
		
	/*k = inb(port+mcd_xfer);
	if ((k & MCD_STEN)==0) {
		int j;

		printf("rb ");
		do {
			printf("%2x ", (j = inb(port+mcd_rdata)));
		} while(((k = inb(port+mcd_Status)) & MCD_STEN) == 0); 
		goto nextblock;
	}*/
	
	/* fire off a interrupt poll or lost interrupt monitor */
	/* if (irq) { */
		/* timeout((int (*)())mcd_expire, (caddr_t)unit, hz*2); */
	/* } else {
		mbx->count = hz*2;
		timeout((int (*)())mcd_pollintr, (caddr_t)unit, 1);
	} */
	splx(s);
	return;
}

/* check for stuck drive or lost interrupts */
static void
mcd_monitor(void) {
	int port, ists;
	int i, unit;

	i = splbio();
	for (unit = 0; unit < NMCD; unit++) {
		port = mcd_data[unit].iobase;
		if (port == 0)
			continue;
		ists = inb(port+mcd_Status);
		if ((ists & (MCD_STEN | MCD_DTEN)) != (MCD_STEN | MCD_DTEN)) {
			if (mcd_data[unit].flags & MCDMBXSTUCK) {
				printf("monitor intr ");
				mcd_data[unit].flags |= MCDMBXTIMEO;
				mcdintr(unit);
			} else
				mcd_data[unit].flags |= MCDMBXSTUCK;
		} else
			mcd_data[unit].flags &= ~MCDMBXSTUCK;
	}
	splx(i);
	timeout((int (*)())mcd_monitor, (caddr_t)0, hz/2);
}

/* lost interrupt catcher */
mcd_expire(int unit) {
	int i;

	i = splbio();
printf("expire %d\n", unit);
	mcd_data[unit].flags |= MCDMBXTIMEO;
	mcdintr(unit);
	splx(i);
}

/* simulate interrupt with a polling timeout */
mcd_pollintr(unit) {
	int port =  mcd_data[unit].iobase, ists;
	struct mcd_data *cd = mcd_data + unit;
	
	/* if data or status is present, call interrupt */
	ists = inb(port+mcd_Status);
	if ((ists & (MCD_STEN | MCD_DTEN)) != (MCD_STEN | MCD_DTEN))
		mcdintr(unit);
	else {
		/* if operation has not yet timed out, set poll */
		if (mcd_data[unit].mbx.count-- > 0)
			timeout((int (*)())mcd_pollintr, (caddr_t)unit, 1);

		/* timed out, signal a retry on timeout */
		else {
			cd->flags |= MCDMBXTIMEO;
			mcdintr(unit);
		}
	}
}

static int
mcdioctl(dev_t dev, int cmd, caddr_t addr, int flags, struct proc *p)
{
	struct mcd_data *cd;
	int unit,part;
	
	unit = mcd_unit(dev);
	part = mcd_part(dev);
	cd = mcd_data + unit;

#ifdef MCDMINI
	return ENOTTY;
#else
	if (!(cd->flags & MCDVALID))
		return EIO;
MCD_TRACE("ioctl called 0x%x\n",cmd,0,0,0);

	switch (cmd) {
	case DIOCSBAD:
		return EINVAL;
	case DIOCGDINFO:
	case DIOCGPART:
	case DIOCWDINFO:
	case DIOCSDINFO:
	case DIOCWLABEL:
		return ENOTTY;
	case CDIOCPLAYTRACKS:
		return mcd_playtracks(unit, (struct ioc_play_track *) addr);
	case CDIOCPLAYBLOCKS:
		return mcd_play(unit, (struct mcd_read2 *) addr);
	case CDIOCREADSUBCHANNEL:
		return mcd_subchan(unit, (struct ioc_read_subchannel *) addr);
	case CDIOREADTOCHEADER:
		return mcd_toc_header(unit, (struct ioc_toc_header *) addr);
	case CDIOREADTOCENTRYS:
		return mcd_toc_entry(unit, (struct ioc_read_toc_entry *) addr);
	case CDIOCSETPATCH:
	case CDIOCGETVOL:
	case CDIOCSETVOL:
	case CDIOCSETMONO:
	case CDIOCSETSTERIO:
	case CDIOCSETMUTE:
	case CDIOCSETLEFT:
	case CDIOCSETRIGHT:
		return EINVAL;
	case CDIOCRESUME:
		return mcd_resume(unit);
	case CDIOCPAUSE:
		return mcd_pause(unit);
	case CDIOCSTART:
		return EINVAL;
	case CDIOCSTOP:
		return mcd_stop(unit);
	case CDIOCEJECT:
		return EINVAL;
	case CDIOCSETDEBUG:
		cd->debug = 1;
		return 0;
	case CDIOCCLRDEBUG:
		cd->debug = 0;
		return 0;
	case CDIOCRESET:
		return EINVAL;
	default:
		return ENOTTY;
	}
	/*NOTREACHED*/
#endif /*!MCDMINI*/
}

/* this could have been taken from scsi/cd.c, but it is not clear
 * whether the scsi cd driver is linked in
 */
static int
mcd_getdisklabel(int unit)
{
	struct mcd_data *cd = mcd_data + unit;
	
	if (cd->flags & MCDLABEL)
		return -1;
	
	memset(&cd->dlabel, 0, sizeof(struct disklabel));
	strncpy(cd->dlabel.d_typename,"Mitsumi CD ROM ",16);
	strncpy(cd->dlabel.d_packname,"unknown        ",16);
	cd->dlabel.d_secsize 	= cd->blksize;
	cd->dlabel.d_nsectors	= 100;
	cd->dlabel.d_ntracks	= 1;
	cd->dlabel.d_ncylinders	= (cd->disksize/100)+1;
	cd->dlabel.d_secpercyl	= 100;
	cd->dlabel.d_secperunit	= cd->disksize;
	cd->dlabel.d_rpm	= 300;
	cd->dlabel.d_interleave	= 1;
	cd->dlabel.d_flags	= D_REMOVABLE;
	cd->dlabel.d_npartitions= 1;
	cd->dlabel.d_partitions[0].p_offset = 0;
	cd->dlabel.d_partitions[0].p_size = cd->disksize;
	cd->dlabel.d_partitions[0].p_fstype = 9;
	cd->dlabel.d_magic	= DISKMAGIC;
	cd->dlabel.d_magic2	= DISKMAGIC;
	cd->dlabel.d_checksum	= dkcksum(&cd->dlabel);
	
	cd->flags |= MCDLABEL;
	return 0;
}

static int
mcdsize(dev_t dev) 
{
	int size;
	int unit = mcd_unit(dev);
	struct mcd_data *cd = mcd_data + unit;

	if (mcd_volinfo(unit) >= 0) {
		cd->blksize = MCDBLK;
		size = msf2hsg(cd->volinfo.vol_msf);
		cd->disksize = size * (MCDBLK/DEV_BSIZE);
		return 0;
	}
	return -1;
}

/***************************************************************
 * lower level of driver starts here
 **************************************************************/

#ifdef NOTDEF
static char irqs[] = {
	0x00,0x00,0x10,0x20,0x00,0x30,0x00,0x00,
	0x00,0x10,0x40,0x50,0x00,0x00,0x00,0x00
};

static char drqs[] = {
	0x00,0x01,0x00,0x03,0x00,0x05,0x06,0x07,
};
#endif

static void mcd_configure(struct mcd_data *cd)
{
	outb(cd->iobase+mcd_config,cd->config);
}

/* check if there is a cdrom */
int mcd_probe(struct isa_device *dev)
{
	struct mcd_data *cd;
	int port = dev->id_iobase;	
	static lastunit;
	int unit;
	int st;
	int i;
	char vers[2];

	if (dev->id_unit == '?')
		dev->id_unit = lastunit;

	unit = dev->id_unit;
	cd = mcd_data + dev->id_unit;
	cd->iobase = port;	
	mcd_data[unit].flags = MCDPROBING;

#ifdef NOTDEF
	/* get irq/drq configuration word */
	mcd_data[unit].config = irqs[dev->id_irq]; /* | drqs[dev->id_drq];*/
#else
	mcd_data[unit].config = 0;
#endif

	/* attempt a soft reset */
	/* outb(port+mcd_config, 2);  /* LU version only */
printf("soft reset ");
	mcd_setport(unit, mcd_Data, MCD_SOFTRESET);

	/* if drive still does not respond, do a hard reset */
	if (mcd_drv_rdy(unit) < 0 ||
	    mcd_data[unit].status & (MCD_ST_DOOROPEN | MCD_ST_DSKIN) == 0) {
		int i;

printf("hard reset ");
		/* send a reset */
		mcd_setport(unit, mcd_reset, 0);
		for (i = 0 ; i < 4; i++)
			if (mcd_getstat(unit, 1) > 0)
				break;
		/* If not resettable, no drive */
		if (i >= 3)
			return (0);
	}
	/* XXX: obtain drive version and firmware revision */

printf("get status ");
	/* get status */
	st = mcd_getstat(unit, 1);
	mcd_data[unit].flags = 0;
	if (st <= 0)
		return (0);

printf("get drive type ");
	/* kind of drive */
	if (mcd_send(unit, MCD_CMDGETVERS, 4) < 0 ||
	    mcd_get(dev->id_unit, vers, sizeof vers) != sizeof vers)
		return (0);
		/* printf("can't get drive version\n");*/
	else {
		cd->flags = MCDINIT;
		/* double speed drive uses different read command */
		if (vers[0] == MCD_VER_FXD)
			cd->flags |= MCDDS;
		/* printf("version %c rev %x", vers[0], (unsigned)vers[1]); */
	}
	lastunit++;
	return (1);
}

/* obtain drive status */
static int
mcd_drv_rdy(int unit) {
	struct	mcd_data *cd = mcd_data + unit;
	int	i, port = cd->iobase;
	unsigned sts;

	/* wait until drive signals that status is available */
	for (i = 0; i < 3000*4; i++) {
		if ((mcd_getport(unit, mcd_Status) & MCD_STEN) == 0)
			goto status_is_available;
		mcd_delay(250);
	}
	return (-1);

status_is_available:
	sts =  mcd_getport(unit, mcd_Data);

	/* if door is open, diskette state is meaningless */
	if (sts & MCD_ST_DOOROPEN)
		sts &= ~(MCD_ST_DSKIN | MCD_ST_DSKCHNG | MCD_ST_DSKSPIN);

	cd->status = sts;
	return (sts & 0xff);
}

static int mcd_waitrdy(int port,int dly) 
{
	int i;

	/* wait until xfer port senses data ready */
	for (i=0; i<dly; i++) {
		if ((inb(port+mcd_xfer) & MCD_ST_BUSY)==0)
			return 0;
		mcd_delay(1);
	}
	return -1;
}

static int mcd_getreply(int unit,int dly)
{
	int	i;
	struct	mcd_data *cd = mcd_data + unit;
	int	port = cd->iobase;

	/* wait data to become ready */
	if (mcd_waitrdy(port,dly)<0) {
#ifdef MCD_TO_WARNING_ON
		printf("mcd%d: timeout getreply\n",unit);
#endif
		return -1;
	}

	/* get the data */
	return inb(port+mcd_status) & 0xFF;
}

static int mcd_getstat(int unit, int sflg)
{
	int	i;
	struct	mcd_data *cd = mcd_data + unit;
	int	port = cd->iobase;

	/* get the status */
	if (sflg)
		outb(port+mcd_command, MCD_CMDGETSTAT);
#ifdef was
	i = mcd_getreply(unit,DELAY_GETREPLY);
	if (i<0) return -1;

	cd->status = i;
	mcd_setflags(unit,cd);
	return cd->status;
#else
	i = mcd_drv_rdy(unit);
	mcd_setflags(unit, cd);
	return i;
#endif
}

static void mcd_setflags(int unit, struct mcd_data *cd) 
{
	/* check flags */
	if (cd->status & (MCDDSKCHNG|MCDDOOROPEN)) {
		MCD_TRACE("getstat: sensed DSKCHNG or DOOROPEN\n",0,0,0,0);
		cd->flags &= ~(MCDVALID | MCDTOC);
	}

#ifndef MCDMINI
	if (cd->status & MCDAUDIOBSY)
		cd->audio_status = CD_AS_PLAY_IN_PROGRESS;
	else if (cd->audio_status == CD_AS_PLAY_IN_PROGRESS)
		cd->audio_status = CD_AS_PLAY_COMPLETED;
#endif
}

static int mcd_get(int unit, char *buf, int nmax)
{
	int port = mcd_data[unit].iobase;
	int i,k;
	
	for (i=0; i<nmax; i++) {

		/* wait for data */
		if ((k = mcd_getreply(unit,DELAY_GETREPLY)) < 0) {
#ifdef MCD_TO_WARNING_ON
			printf("mcd%d: timeout mcd_get\n",unit);
#endif
			return -1;
		}
		buf[i] = k;
	}
	return i;
}

static int mcd_send(int unit, int cmd,int nretrys)
{
	int i,k;
	int port = mcd_data[unit].iobase;
	
MCD_TRACE("mcd_send: command = 0x%x\n",cmd,0,0,0);
	for (i=0; i<nretrys; i++) {
		outb(port+mcd_command, cmd);
		if ((k=mcd_getstat(unit,0)) != -1)
			break;
	}
	if (i == nretrys) {
		printf("mcd%d: mcd_send cmd %x retry cnt exceeded\n",unit, cmd);
		return -1;
	}
MCD_TRACE("mcd_send: status = 0x%x\n",k,0,0,0);
	return 0;
}

static int mcd_Send(int unit, char *cmdp, int ncmd, int nretrys)
{
	int i, sts;
	int port = mcd_data[unit].iobase;
	
MCD_TRACE("mcd_send: command = 0x%x\n", *cmdp,0,0,0);
	for (i = 0; i < nretrys; i++) {
		outsb (port+mcd_command, cmdp, ncmd);
		if ((sts = mcd_getstat(unit, 0)) != -1)
			break;
	}
	if (i == nretrys) {
		printf("mcd%d: mcd_send cmd %x retry cnt exceeded\n", unit, *cmdp);
		return -1;
	}
MCD_TRACE("mcd_send: status = 0x%x\n", sts, 0, 0, 0);
	return 0;
}

static int bcd2bin(bcd_t b)
{
	return (b >> 4) * 10 + (b & 15);
}

static bcd_t bin2bcd(int b)
{
	return ((b / 10) << 4) | (b % 10);
}

static void hsg2msf(int hsg, bcd_t *msf)
{
	hsg += 2*75;
	M_msf(msf) = bin2bcd(hsg / (60*75));
	hsg %= 60*75;
	S_msf(msf) = bin2bcd(hsg / 75);
	F_msf(msf) = bin2bcd(hsg % 75);
}

static int msf2hsg(bcd_t *msf)
{
	return (bcd2bin(M_msf(msf)) * 60 +
		bcd2bin(S_msf(msf))) * 75 +
		bcd2bin(F_msf(msf)) - (2*75);
}

static int mcd_volinfo(int unit)
{
	struct mcd_data *cd = mcd_data + unit;
	int i;

/*MCD_TRACE("mcd_volinfo: enter\n",0,0,0,0);*/

	/* Get the status, in case the disc has been changed */
	if (mcd_getstat(unit, 1) < 0) return EIO;

	/* Just return if we already have it */
	if (cd->flags & MCDVOLINFO) return 0;

	/* send volume info command */
	if (mcd_send(unit, MCD_CMDGETVOLINFO, MCD_RETRYS) < 0)
		return -1;

	/* get data */
	if (mcd_get(unit,(char*) &cd->volinfo,sizeof(struct mcd_volinfo)) < 0) {
		printf("mcd%d: mcd_volinfo: error read data\n",unit);
		return -1;
	}

	if (cd->volinfo.trk_low != 0 || cd->volinfo.trk_high != 0) {
		cd->flags |= MCDVOLINFO;   /* volinfo is OK */
		return 0;
	}

	return -1;
}

static void
mcdintr(unit)
{
	int port = mcd_data[unit].iobase;
	struct	mcd_data *cd = mcd_data + unit;
	int i, ists, sts;
	struct mcd_mbx *mbx = &cd->mbx;
	struct buf *bp;
	caddr_t	addr;
	
	MCD_TRACE("interrupt ists 0x%x\n",inb(port+mcd_Status),0,0,0);

	bp = mbx->bp;
	ists = inb(port+mcd_Status);
	if ((cd->flags & MCDMBXBSY) == 0) {
		/* non-transfer interrupt XXX */
		printf("I%2x ", ists);
		
		for (sts = -1;
		    (ists & (MCD_STEN | MCD_DTEN)) != (MCD_STEN | MCD_DTEN); )
		{
			int flg = 0, data;

			if ((ists & MCD_STEN) == 0) {
				sts = inb(port+mcd_rdata);
				if (flg != MCD_STEN)
					printf("sts ");
				printf("%2x ", sts);
				flg = MCD_STEN;
			}
			if ((ists & MCD_DTEN) == 0) {
				data = inb(port+mcd_rdata);
				/* if (flg != MCD_DTEN)
					printf("data ");
				printf("%2x ", data); */
				flg = MCD_DTEN;
			}
			ists = inb(port+mcd_Status);
		}
		return;
	}

	/* transfer interrupt */
	/* kill timeout */
	/* untimeout((int (*)())mcd_expire,(caddr_t)unit); */
	mcd_data[unit].flags &= ~MCDMBXSTUCK;
	if (ists != 0xed) printf("i%2x ", ists);

	/* wait til status or data become available */
	do
		ists = inb(port+mcd_Status);
	while ((ists & (MCD_STEN|MCD_DTEN)) == (MCD_STEN|MCD_DTEN));

	/* status, transfer has failed */
	if ((ists & MCD_STEN) == 0) {
		sts = inb(port+mcd_rdata);
		printf("sb %2x", sts);
		/*if (sts & MCD_ST_INVCMD)
			goto nextblock;
		if (sts & MCD_ST_READERR)*/
			goto readerr;
	}

	/* data, transfer is successful */
	if ((ists & MCD_DTEN) == 0) {
		addr = bp->b_un.b_addr + mbx->skip;
#ifndef eight
		outb(port+mcd_ctl2,0x00);	/* XXX */
		insb (port+mcd_rdata, addr, mbx->sz);
		outb(port+mcd_ctl2,0x08);	/* XXX */
#else
		outb(port+mcd_ctl2,0xc1);	/* XXX */
		insw (port+mcd_rdata, addr, mbx->sz/2);
		outb(port+mcd_ctl2,0x41);	/* XXX */
#endif
#ifdef dontbother
		/* transfer dribbled? */
		ists = inb(port+mcd_Status);
		if ((ists & MCD_DTEN) ==0) {
#ifndef eight
			outb(port+mcd_ctl2,0x00);	/* XXX */
			(void)inb(port+mcd_rdata);
			(void)inb(port+mcd_rdata);
			outb(port+mcd_ctl2,0x08);	/* XXX */
#else
			outb(port+mcd_ctl2,0xc1);	/* XXX */
			(void)inw(port+mcd_rdata);
			outb(port+mcd_ctl2,0x41);	/* XXX */
#endif
			ists = inb(port+mcd_Status);
			printf("F");
		}
		if ((ists & MCD_DTEN)==0)
			printf("G");
		if ((ists & MCD_STEN)==0) {
			printf("sa ");
			do printf("%2x ", inb(port+mcd_rdata));
			while(((ists = inb(port+mcd_Status)) & MCD_STEN) == 0); 
				goto readerr;
			}
#endif

		/* this must be here!! */
		/* check drive status to insure correct state */
		if ((sts = mcd_getstat(unit, 1)) != (MCD_ST_DSKIN | MCD_ST_DSKSPIN)) {
			printf("S%x ", sts);
			goto readerr;
		}

		if (--mbx->nblk > 0) {
			mbx->skip += mbx->sz;
		} else {
			/* return buffer */
			bp->b_resid = 0;
			biodone(bp);
		}

		cd->flags &= ~(MCDMBXBSY | MCDMBXRTRY);
		mcd_start(mbx->unit);
		return;

		/*if ((k & MCD_STEN)==0) {
			sts = inb(port+mcd_rdata);
			printf("srt %2x", sts);
			goto readerr;
		}
		if(failcmd) printf("fail"); */
	} else {

readerr:
		if (mbx->retry-- > 0) {
#ifdef MCD_TO_WARNING_ON
			printf("mcd%d: retry %d\n", unit, mbx->retry);
#endif
printf("r%d ", mbx->retry);
			mbx->skip = 0;
			cd->flags |= MCDMBXRTRY;
		} else {

printf("failed ");
			/* invalidate the buffer */
			bp->b_flags |= B_ERROR;
			bp->b_resid = bp->b_bcount;
			biodone(bp);
		}
		cd->flags &= ~MCDMBXBSY;
		mcd_start(mbx->unit);
	}
}

#ifndef MCDMINI
static int mcd_setmode(int unit, int mode)
{
	struct mcd_data *cd = mcd_data + unit;
	int port = cd->iobase;
	int retry;

	printf("mcd%d: setting mode to %d\n", unit, mode);
	for(retry=0; retry<MCD_RETRYS; retry++)
	{
		outb(port+mcd_command, MCD_CMDSETMODE);
		outb(port+mcd_command, mode);
		if (mcd_getstat(unit, 0) != -1) return 0;
	}

	return -1;
}

static int mcd_toc_header(int unit, struct ioc_toc_header *th)
{
	struct mcd_data *cd = mcd_data + unit;

	if (mcd_volinfo(unit) < 0)
		return ENXIO;

	th->len = msf2hsg(cd->volinfo.vol_msf);
	th->starting_track = bcd2bin(cd->volinfo.trk_low);
	th->ending_track = bcd2bin(cd->volinfo.trk_high);

	return 0;
}

static int mcd_read_toc(int unit)
{
	struct mcd_data *cd = mcd_data + unit;
	struct ioc_toc_header th;
	struct mcd_qchninfo q;
	int rc, trk, idx, retry;

	/* Only read TOC if needed */
	if (cd->flags & MCDTOC) return 0;

	printf("mcd%d: reading toc header\n", unit);
	if (mcd_toc_header(unit, &th) != 0)
		return ENXIO;

	printf("mcd%d: stopping play\n", unit);
	if ((rc = mcd_stop(unit)) != 0)
		return rc;

	/* try setting the mode twice */
	if (mcd_setmode(unit, MCD_MD_TOC) != 0)
		return EIO;
	if (mcd_setmode(unit, MCD_MD_TOC) != 0)
		return EIO;

	printf("mcd%d: get_toc reading qchannel info\n",unit);	
	for(trk=th.starting_track; trk<=th.ending_track; trk++)
		cd->toc[trk].idx_no = 0;
	trk = th.ending_track - th.starting_track + 1;
	for(retry=0; retry<300 && trk>0; retry++)
	{
		if (mcd_getqchan(unit, &q) < 0) break;
		idx = bcd2bin(q.idx_no);
		if (idx>0 && idx < MCD_MAXTOCS && q.trk_no==0)
			if (cd->toc[idx].idx_no == 0)
			{
				cd->toc[idx] = q;
				trk--;
			}
	}

	if (mcd_setmode(unit, MCD_MD_COOKED) != 0)
		return EIO;

	if (trk != 0) return ENXIO;

	/* add a fake last+1 */
	idx = th.ending_track + 1;
	cd->toc[idx].ctrl_adr = cd->toc[idx-1].ctrl_adr;
	cd->toc[idx].trk_no = 0;
	cd->toc[idx].idx_no = 0xAA;
	cd->toc[idx].hd_pos_msf[0] = cd->volinfo.vol_msf[0];
	cd->toc[idx].hd_pos_msf[1] = cd->volinfo.vol_msf[1];
	cd->toc[idx].hd_pos_msf[2] = cd->volinfo.vol_msf[2];

	cd->flags |= MCDTOC;

	return 0;
}

static int mcd_toc_entry(int unit, struct ioc_read_toc_entry *te)
{
	struct mcd_data *cd = mcd_data + unit;
	struct ret_toc
	{
		struct ioc_toc_header th;
		struct cd_toc_entry rt;
	} ret_toc;
	struct ioc_toc_header th;
	int rc, i;

	/* Make sure we have a valid toc */
	if ((rc=mcd_read_toc(unit)) != 0)
		return rc;

	/* find the toc to copy*/
	i = te->starting_track;
	if (i == MCD_LASTPLUS1)
		i = bcd2bin(cd->volinfo.trk_high) + 1;
	
	/* verify starting track */
	if (i < bcd2bin(cd->volinfo.trk_low) ||
		i > bcd2bin(cd->volinfo.trk_high)+1)
		return EINVAL;

	/* do we have room */
	if (te->data_len < sizeof(struct ioc_toc_header) +
		sizeof(struct cd_toc_entry)) return EINVAL;

	/* Copy the toc header */
	if (mcd_toc_header(unit, &th) < 0) return EIO;
	ret_toc.th = th;

	/* copy the toc data */
	ret_toc.rt.control = cd->toc[i].ctrl_adr;
	ret_toc.rt.addr_type = te->address_format;
	ret_toc.rt.track = i;
	if (te->address_format == CD_MSF_FORMAT)
	{
		ret_toc.rt.addr[1] = cd->toc[i].hd_pos_msf[0];
		ret_toc.rt.addr[2] = cd->toc[i].hd_pos_msf[1];
		ret_toc.rt.addr[3] = cd->toc[i].hd_pos_msf[2];
	}

	/* copy the data back */
	copyout(curproc, &ret_toc, te->data, sizeof(struct cd_toc_entry)
		+ sizeof(struct ioc_toc_header));

	return 0;
}

static int mcd_stop(int unit)
{
	struct mcd_data *cd = mcd_data + unit;

	if (mcd_send(unit, MCD_CMDSTOPAUDIO, MCD_RETRYS) < 0)
		return ENXIO;
	cd->audio_status = CD_AS_PLAY_COMPLETED;
	return 0;
}

static int mcd_getqchan(int unit, struct mcd_qchninfo *q)
{
	struct mcd_data *cd = mcd_data + unit;

	if (mcd_send(unit, MCD_CMDGETQCHN, MCD_RETRYS) < 0)
		return -1;
	if (mcd_get(unit, (char *) q, sizeof(struct mcd_qchninfo)) < 0)
		return -1;
	if (cd->debug)
	printf("mcd%d: qchannel ctl=%d, t=%d, i=%d, ttm=%d:%d.%d dtm=%d:%d.%d\n",
		unit,
		q->ctrl_adr, q->trk_no, q->idx_no,
		q->trk_size_msf[0], q->trk_size_msf[1], q->trk_size_msf[2],
		q->trk_size_msf[0], q->trk_size_msf[1], q->trk_size_msf[2]);
	return 0;
}

static int mcd_subchan(int unit, struct ioc_read_subchannel *sc)
{
	struct mcd_data *cd = mcd_data + unit;
        struct mcd_qchninfo q;
        struct cd_sub_channel_info data;

        printf("mcd%d: subchan af=%d, df=%d\n", unit,
        	sc->address_format,
                sc->data_format);
        if (sc->address_format != CD_MSF_FORMAT) return EIO;
        if (sc->data_format != CD_CURRENT_POSITION) return EIO;

        if (mcd_getqchan(unit, &q) < 0) return EIO;

        data.header.audio_status = cd->audio_status;
        data.what.position.data_format = CD_MSF_FORMAT;
        data.what.position.track_number = bcd2bin(q.trk_no);

        if (copyout(curproc, &data, sc->data, sizeof(struct cd_sub_channel_info))!=0)
                return EFAULT;
        return 0;
}

static int mcd_playtracks(int unit, struct ioc_play_track *pt)
{
	struct mcd_data *cd = mcd_data + unit;
	struct mcd_read2 pb;
	int a = pt->start_track;
	int z = pt->end_track;
	int rc;

	if ((rc = mcd_read_toc(unit)) != 0) return rc;

	printf("mcd%d: playtracks from %d:%d to %d:%d\n", unit,
		a, pt->start_index, z, pt->end_index);

	if (a < cd->volinfo.trk_low || a > cd->volinfo.trk_high || a > z ||
		z < cd->volinfo.trk_low || z > cd->volinfo.trk_high)
		return EINVAL;

	pb.start_msf[0] = cd->toc[a].hd_pos_msf[0];
	pb.start_msf[1] = cd->toc[a].hd_pos_msf[1];
	pb.start_msf[2] = cd->toc[a].hd_pos_msf[2];
	pb.end_msf[0] = cd->toc[z+1].hd_pos_msf[0];
	pb.end_msf[1] = cd->toc[z+1].hd_pos_msf[1];
	pb.end_msf[2] = cd->toc[z+1].hd_pos_msf[2];

	return mcd_play(unit, &pb);
}

static int mcd_play(int unit, struct mcd_read2 *pb)
{
	struct mcd_data *cd = mcd_data + unit;
	int port = cd->iobase;
	int retry, st;

	cd->lastpb = *pb;
	for(retry=0; retry<MCD_RETRYS; retry++)
	{
		outb(port+mcd_command, MCD_CMDREAD2);
		outb(port+mcd_command, pb->start_msf[0]);
		outb(port+mcd_command, pb->start_msf[1]);
		outb(port+mcd_command, pb->start_msf[2]);
		outb(port+mcd_command, pb->end_msf[0]);
		outb(port+mcd_command, pb->end_msf[1]);
		outb(port+mcd_command, pb->end_msf[2]);
		if ((st=mcd_getstat(unit, 0)) != -1) break;
	}

	if (cd->debug)
	printf("mcd%d: mcd_play retry=%d, status=%d\n", unit, retry, st);
	if (st == -1) return ENXIO;
	cd->audio_status = CD_AS_PLAY_IN_PROGRESS;
	return 0;
}

static int mcd_pause(int unit)
{
	struct mcd_data *cd = mcd_data + unit;
        struct mcd_qchninfo q;
        int rc;

        /* Verify current status */
        if (cd->audio_status != CD_AS_PLAY_IN_PROGRESS)
	{
		printf("mcd%d: pause attempted when not playing\n", unit);
		return EINVAL;
	}

        /* Get the current position */
        if (mcd_getqchan(unit, &q) < 0) return EIO;

        /* Copy it into lastpb */
        cd->lastpb.start_msf[0] = q.hd_pos_msf[0];
        cd->lastpb.start_msf[1] = q.hd_pos_msf[1];
        cd->lastpb.start_msf[2] = q.hd_pos_msf[2];

        /* Stop playing */
        if ((rc=mcd_stop(unit)) != 0) return rc;

        /* Set the proper status and exit */
        cd->audio_status = CD_AS_PLAY_PAUSED;
        return 0;
}

static int mcd_resume(int unit)
{
	struct mcd_data *cd = mcd_data + unit;

	if (cd->audio_status != CD_AS_PLAY_PAUSED) return EINVAL;
	return mcd_play(unit, &cd->lastpb);
}
#endif /*!MCDMINI*/

static struct devif mcd_devif =
{
	{0}, -1, -1, 0x38, 3, 7, 0, 0, 0,
	mcdopen, mcdclose, mcdioctl, 0, 0, 0, 0,
	mcdstrategy,	0, 0, 0,
};

DRIVER_MODCONFIG() {
	char *cfg_string = mcd_conf;
	
	if (devif_config(&cfg_string, &mcd_devif) == 0)
		return;

	/* probe for hardware */
	new_isa_configure(&cfg_string, &mcddriver);
}

static void
mcd_setport(int unit, int port, u_char data) {
	struct mcd_data *cd = mcd_data + unit;

	outb(cd->iobase + port, data);
}

static u_char
mcd_getport(int unit, int port) {
	struct mcd_data *cd = mcd_data + unit;

	return (inb(cd->iobase + port));
}
