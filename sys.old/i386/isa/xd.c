#include "xd.h"
#if	NXD > 0

#include "param.h"
#include "dkbad.h"
#include "systm.h"
#include "conf.h"
#include "file.h"
#include "user.h"
#include "ioctl.h"
#include "machine/isa/disk.h"
#include "buf.h"
#include "vm.h"
#include "uio.h"
#include "machine/pte.h"
#include "machine/isa/device.h"
#include "machine/isa/xdreg.h"
#include "syslog.h"

#define	RETRIES		5	/* number of retries before giving up */
#define	MAXTRANSFER	32	/* max size of transfer in page clusters */

#define XDUNIT(dev)	((minor(dev) & 070) >> 3)

#define b_cylin	b_resid		/* cylinder number for doing IO to */
				/* shares an entry in the buf struct */

/*
 * Drive states.  Used for open and format operations.
 * States < OPEN (> 0) are transient, during an open operation.
 * OPENRAW is used for unlabeled disks, and for floppies, to inhibit
 * bad-sector forwarding.
 */
#define RAWDISK		8		/* raw disk operation, no translation*/
#define ISRAWSTATE(s)	(RAWDISK&(s))	/* are we in a raw state? */
#define DISKSTATE(s)	(~RAWDISK&(s))	/* are we in a given state regardless
					   of raw or cooked mode? */

#define	CLOSED		0		/* disk is closed. */
					/* "cooked" disk states */
#define	WANTOPEN	1		/* open requested, not started */
#define	RECAL		2		/* doing restore */
#define	RDLABEL		3		/* reading pack label */
#define	RDBADTBL	4		/* reading bad-sector table */
#define	OPEN		5		/* done with open */

#define	WANTOPENRAW	(WANTOPEN|RAWDISK)	/* raw WANTOPEN */
#define	RECALRAW	(RECAL|RAWDISK)	/* raw open, doing restore */
#define	OPENRAW		(OPEN|RAWDISK)	/* open, but unlabeled disk or floppy */


/*
 * The structure of a disk drive.
 */
struct	disk {
	struct disklabel dk_dd;			/* device configuration data */
	long	dk_bc;				/* byte count left */
	short	dk_skip;			/* blocks already transferred */
	char	dk_unit;			/* physical unit number */
	char	dk_sdh;				/* sdh prototype */
	char	dk_state;			/* control state */
	u_char	dk_status;			/* copy of status reg. */
	u_char	dk_error;			/* copy of error reg. */
	short	dk_open;			/* open/closed refcnt */
};

/*
 * This label is used as a default when initializing a new or raw disk.
 * It really only lets us access the first track until we know more.
 */
static struct disklabel dflt_sizes = {
	DISKMAGIC, DTYPE_ST506,
	{
		512,		/* sector size */
		36,		/* # of sectors per track */
		15,		/* # of tracks per cylinder */
		1224,		/* # of cylinders per unit */
		36*15,		/* # of sectors per cylinder */
		1224*15*36,	/* # of sectors per unit */
		0		/* write precomp cylinder (none) */
	},
	21600,	0,	/* A=root filesystem */
	21600,	40,
	660890, 0,	/* C=whole disk */
	216000,	80,
	0,	0,
	0,	0,
	0,	0,
	399600,	480
};

static	struct	dkbad	dkbad[NXD];
struct	disk	xddrives[NXD] = {0};	/* table of units */
struct	buf	xdtab = {0};
struct	buf	xdutab[NXD] = {0};	/* head of queue per drive */
struct	buf	rxdbuf[NXD] = {0};	/* buffers for raw IO */
long	xdxfer[NXD] = {0};		/* count of transfers */
static int	writeprotected[NXD] = { 0 };
int	xdprobe(), xdattach(), xdintr();
struct	isa_driver xddriver = {
	xdprobe, xdattach, "xd",
};
#include "dbg.h"
#define XDDEBUG
static xdc;

/*
 * Probe routine
 */
xdprobe(dvp)
	struct isa_device *dvp;
{
	xdc = dvp->id_iobase;

#ifdef lint
	xdintr(0);
#endif
	outb(xdc+xd_error, 0x5a) ;	/* error register not writable */
	/*xdp->xd_cyl_hi = 0xff ;/* only two bits of cylhi are implemented */
	outb(xdc+xd_cyl_lo, 0xa5) ;	/* but all of cyllo are implemented */
	if(inb(xdc+xd_error) != 0x5a /*&& xdp->xd_cyl_hi == 3*/
	   && inb(xdc+xd_cyl_lo) == 0xa5)
		return(1) ;
	return (0);
}

/*
 * attach each drive if possible.
 */
xdattach(dvp)
	struct isa_device *dvp;
{
	int unit = dvp->id_unit;

	outb(0x376,12);
	DELAY(1000);
	outb(0x376,8);
}

/* Read/write routine for a buffer.  Finds the proper unit, range checks
 * arguments, and schedules the transfer.  Does not wait for the transfer
 * to complete.  Multi-page transfers are supported.  All I/O requests must
 * be a multiple of a sector in length.
 */
xdstrategy(bp)
	register struct buf *bp;	/* IO operation to perform */
{
	register struct buf *dp;
	register struct disk *du;	/* Disk unit to do the IO.	*/
	long nblocks, cyloff, blknum;
	int	unit = XDUNIT(bp->b_dev), xunit = minor(bp->b_dev) & 7;
	int	s;

	if ((unit >= NXD) || (bp->b_blkno < 0)) {
		printf("xdstrat: unit = %d, blkno = %d, bcount = %d\n",
			unit, bp->b_blkno, bp->b_bcount);
		pg("xd:error in xdstrategy");
		bp->b_flags |= B_ERROR;
		goto bad;
	}
	if (writeprotected[unit] && (bp->b_flags & B_READ) == 0) {
		printf("xd%d: write protected\n", unit);
		goto bad;
	}
	du = &xddrives[unit];
	if (DISKSTATE(du->dk_state) != OPEN)
		goto q;
	/*
	 * Convert DEV_BSIZE "blocks" to sectors.
	 * Note: doing the conversions this way limits the partition size
	 * to about 8 million sectors (1-8 Gb).
	 */
	blknum = (unsigned long) bp->b_blkno * DEV_BSIZE / du->dk_dd.dk_secsize;
	if (((u_long) bp->b_blkno * DEV_BSIZE % du->dk_dd.dk_secsize != 0) ||
	    bp->b_bcount >= MAXTRANSFER * CLBYTES) {
		bp->b_flags |= B_ERROR;
		goto bad;
	}
	nblocks = du->dk_dd.dk_partition[xunit].nblocks;
	cyloff = du->dk_dd.dk_partition[xunit].cyloff;
	if (blknum + (bp->b_bcount / du->dk_dd.dk_secsize) > nblocks) {
		if (blknum == nblocks)
			bp->b_resid = bp->b_bcount;
		else
			bp->b_flags |= B_ERROR;
		goto bad;
	}
	bp->b_cylin = blknum / du->dk_dd.dk_secpercyl + cyloff;
q:
	dp = &xdutab[unit];
	s = splbio();
	disksort(dp, bp);
	if (dp->b_active == 0)
		xdustart(du);		/* start drive if idle */
	if (xdtab.b_active == 0)
		xdstart(s);		/* start IO if controller idle */
	splx(s);
	return;

bad:
	bp->b_error = EINVAL;
	biodone(bp);
}

/* Routine to queue a read or write command to the controller.  The request is
 * linked into the active list for the controller.  If the controller is idle,
 * the transfer is started.
 */
xdustart(du)
	register struct disk *du;
{
	register struct buf *bp, *dp;

	dp = &xdutab[du->dk_unit];
	if (dp->b_active)
		return;
	bp = dp->b_actf;
	if (bp == NULL)
		return;	
	dp->b_forw = NULL;
	if (xdtab.b_actf  == NULL)		/* link unit into active list */
		xdtab.b_actf = dp;
	else
		xdtab.b_actl->b_forw = dp;
	xdtab.b_actl = dp;
	dp->b_active = 1;		/* mark the drive as busy */
}

/*
 * Controller startup routine.  This does the calculation, and starts
 * a single-sector read or write operation.  Called to start a transfer,
 * or from the interrupt routine to continue a multi-sector transfer.
 * RESTRICTIONS:
 * 1.	The transfer length must be an exact multiple of the sector size.
 */

static xd_sebyse;

xdstart()
{
	register struct disk *du;	/* disk unit for IO */
	register struct buf *bp;
	struct buf *dp;
	register struct bt_bad *bt_ptr;
	long	blknum, pagcnt, cylin, head, sector;
	long	secpertrk, secpercyl, addr, i;
	int	minor_dev, unit, s;

loop:
	dp = xdtab.b_actf;
	if (dp == NULL)
		return;
	bp = dp->b_actf;
	if (bp == NULL) {
		xdtab.b_actf = dp->b_forw;
		goto loop;
	}
	unit = XDUNIT(bp->b_dev);
	du = &xddrives[unit];
	if (DISKSTATE(du->dk_state) <= RDLABEL) {
		if (xdcontrol(bp)) {
			dp->b_actf = bp->av_forw;
			goto loop;	/* done */
		}
		return;
	}
	minor_dev = minor(bp->b_dev) & 7;
	secpertrk = du->dk_dd.dk_nsectors;
	secpercyl = du->dk_dd.dk_secpercyl;
	/*
	 * Convert DEV_BSIZE "blocks" to sectors.
	 */
	blknum = (unsigned long) bp->b_blkno * DEV_BSIZE / du->dk_dd.dk_secsize
		+ du->dk_skip;
#ifdef	XDDEBUG
	if (du->dk_skip == 0) {
		dprintf(DDSK,"\nxdstart %d: %s %d@%d; map ", unit,
			(bp->b_flags & B_READ) ? "read" : "write",
			bp->b_bcount, blknum);
	} else {
		dprintf(DDSK," %d)%x", du->dk_skip, inb(xdc+xd_status));
	}
#endif

	addr = (int) bp->b_un.b_addr;
	if(du->dk_skip==0) du->dk_bc = bp->b_bcount;
	cylin = blknum / secpercyl;
	head = (blknum % secpercyl) / secpertrk;
	sector = blknum % secpertrk;
	if (DISKSTATE(du->dk_state) == OPEN)
		cylin += du->dk_dd.dk_partition[minor_dev].cyloff;

	/* 
	 * See if the current block is in the bad block list.
	 * (If we have one, and not formatting.)
	 */
	if (DISKSTATE(du->dk_state) == OPEN && xd_sebyse)
	    for (bt_ptr = dkbad[unit].bt_bad; bt_ptr->bt_cyl != -1; bt_ptr++) {
		if (bt_ptr->bt_cyl > cylin)
			/* Sorted list, and we passed our cylinder. quit. */
			break;
		if (bt_ptr->bt_cyl == cylin &&
				bt_ptr->bt_trksec == (head << 8) + sector) {
			/*
			 * Found bad block.  Calculate new block addr.
			 * This starts at the end of the disk (skip the
			 * last track which is used for the bad block list),
			 * and works backwards to the front of the disk.
			 */
#ifdef	XDDEBUG
			    dprintf(DDSK,"--- badblock code -> Old = %d; ",
				blknum);
#endif
			blknum = du->dk_dd.dk_secperunit - du->dk_dd.dk_nsectors
				- (bt_ptr - dkbad[unit].bt_bad) - 1;
			cylin = blknum / secpercyl;
			head = (blknum % secpercyl) / secpertrk;
			sector = blknum % secpertrk;
#ifdef	XDDEBUG
			    dprintf(DDSK, "new = %d\n", blknum);
#endif
			break;
		}
	}
	sector += 1;	/* sectors begin with 1, not 0 */

	xdtab.b_active = 1;		/* mark controller active */

	if(du->dk_skip==0 || xd_sebyse) {
	if(xdtab.b_errcnt && (bp->b_flags & B_READ) == 0) du->dk_bc += 512;
	while ((inb(xdc+xd_status) & XDCS_BUSY) != 0) ;
	/*while ((inb(xdc+xd_status) & XDCS_DRQ)) inb(xdc+xd_data);*/
	outb(xdc+xd_precomp, 0xff);
	/*wr(xdc+xd_precomp, du->dk_dd.dk_precompcyl / 4);*/
	/*if (bp->b_flags & B_FORMAT) {
		wr(xdc+xd_sector, du->dk_dd.dk_gap3);
		wr(xdc+xd_seccnt, du->dk_dd.dk_nsectors);
	} else {*/
	if(xd_sebyse)
		outb(xdc+xd_seccnt, 1);
	else
		outb(xdc+xd_seccnt, ((du->dk_bc +511) / 512));
	outb(xdc+xd_sector, sector);

	outb(xdc+xd_cyl_lo, cylin);
	outb(xdc+xd_cyl_hi, cylin >> 8);

	/* Set up the SDH register (select drive).     */
	outb(xdc+xd_sdh, XDSD_IBM | (unit<<4) | (head & 0xf));
	while ((inb(xdc+xd_status) & XDCS_READY) == 0) ;

	/*if (bp->b_flags & B_FORMAT)
		wr(xdc+xd_command, XDCC_FORMAT);
	else*/
		outb(xdc+xd_command,
			(bp->b_flags & B_READ)? XDCC_READ : XDCC_WRITE);
#ifdef	XDDEBUG
	dprintf(DDSK,"sector %d cylin %d head %d addr %x sts %x\n",
	    sector, cylin, head, addr, inb(xdc+xd_altsts));
#endif
}
		
	/* If this is a read operation, just go away until it's done.	*/
	if (bp->b_flags & B_READ) return;

	/* Ready to send data?	*/
	while ((inb(xdc+xd_status) & XDCS_DRQ) == 0)
		nulldev();		/* So compiler won't optimize out */

	/* ASSUMES CONTIGUOUS MEMORY */
	outsw (xdc+xd_data, addr+du->dk_skip*512, 256);
	du->dk_bc -= 512;
}

/*
 * these are globally defined so they can be found
 * by the debugger easily in the case of a system crash
 */
daddr_t xd_errsector;
daddr_t xd_errbn;
unsigned char xd_errstat;

/* Interrupt routine for the controller.  Acknowledge the interrupt, check for
 * errors on the current operation, mark it done if necessary, and start
 * the next request.  Also check for a partially done transfer, and
 * continue with the next chunk if so.
 */
xdintr()
{
	register struct	disk *du;
	register struct buf *bp, *dp;
	int status;
	char partch ;
static shit[32];
static xd_haderror;
	/* Shouldn't need this, but it may be a slow controller.	*/
	while ((status = inb(xdc+xd_status)) & XDCS_BUSY)
		nulldev();
	if (!xdtab.b_active) {
		printf("xd: extra interrupt\n");
		return;
	}

#ifdef	XDDEBUG
	dprintf(DDSK,"I ");
#endif
	dp = xdtab.b_actf;
	bp = dp->b_actf;
	du = &xddrives[XDUNIT(bp->b_dev)];
	partch = "abcdefgh"[minor(bp->b_dev)&7] ;
	if (DISKSTATE(du->dk_state) <= RDLABEL) {
		if (xdcontrol(bp))
			goto done;
		return;
	}
	if (status & (XDCS_ERR | XDCS_ECCCOR)) {
		xd_errstat = inb(xdc+xd_error);		/* save error status */
/* outb(xdc+xd_command, XDCC_RESTORE);
while (inb(xdc+xd_status)&XDCS_BUSY); */
#ifdef	XDDEBUG
		printf("status %x error %x\n", status, xd_errstat);
#endif
		if(xd_sebyse == 0) {
			xd_haderror = 1;
			goto outt;
		}
		/*if (bp->b_flags & B_FORMAT) {
			du->dk_status = status;
			du->dk_error = xdp->xd_error;
			bp->b_flags |= B_ERROR;
			goto done;
		}*/
		
		xd_errsector = (bp->b_cylin * du->dk_dd.dk_secpercyl) +
			(((unsigned long) bp->b_blkno * DEV_BSIZE /
			    du->dk_dd.dk_secsize) % du->dk_dd.dk_secpercyl) +
			du->dk_skip;
		xd_errbn = bp->b_blkno
			+ du->dk_skip * du->dk_dd.dk_secsize / DEV_BSIZE ;
		if (status & XDCS_ERR) {
			if (++xdtab.b_errcnt < RETRIES) {
				xdtab.b_active = 0;
				/*while ((inb(xdc+xd_status) & XDCS_DRQ))
				insw(xdc+xd_data, &shit, sizeof(shit)/2);*/
			} else {
				printf("xd%d%c: ", du->dk_unit, partch);
				printf(
				"hard %s error, sn %d bn %d status %b error %b\n",
					(bp->b_flags & B_READ)? "read":"write",
					xd_errsector, xd_errbn, status, XDCS_BITS,
					xd_errstat, XDERR_BITS);
				bp->b_flags |= B_ERROR;	/* flag the error */
			}
		} else
			log(LOG_WARNING,"xd%d%c: soft ecc sn %d bn %d\n",
				du->dk_unit, partch, xd_errsector,
				xd_errbn);
	}
outt:

	/*
	 * If this was a successful read operation, fetch the data.
	 */
	if (((bp->b_flags & (B_READ | B_ERROR)) == B_READ) && xdtab.b_active) {
		int chk, dummy;

		chk = min(256,du->dk_bc/2);
		/* Ready to receive data?	*/
		while ((inb(xdc+xd_status) & XDCS_DRQ) == 0)
			nulldev();

/*dprintf(DDSK,"addr %x\n", (int)bp->b_un.b_addr + du->dk_skip * 512);*/
		insw(xdc+xd_data,(int)bp->b_un.b_addr + du->dk_skip * 512 ,chk);
		du->dk_bc -= 2*chk;
		while (chk++ < 256) insw (xdc+xd_data,&dummy,1);
	}

	xdxfer[du->dk_unit]++;
	if (xdtab.b_active) {
		if ((bp->b_flags & B_ERROR) == 0) {
			du->dk_skip++;		/* Add to successful sectors. */
			if (xdtab.b_errcnt) {
				log(LOG_WARNING, "xd%d%c: ",
						du->dk_unit, partch);
				log(LOG_WARNING,
			"soft %s error, sn %d bn %d error %b retries %d\n",
				    (bp->b_flags & B_READ) ? "read" : "write",
				    xd_errsector, xd_errbn, xd_errstat,
				    XDERR_BITS, xdtab.b_errcnt);
			}
			xdtab.b_errcnt = 0;

			/* see if more to transfer */
			/*if (du->dk_skip < (bp->b_bcount + 511) / 512) {*/
			if (du->dk_bc > 0 && xd_haderror == 0) {
				xdstart();
				return;		/* next chunk is started */
			} else if (xd_haderror && xd_sebyse == 0) {
				du->dk_skip = 0;
				xd_haderror = 0;
				xd_sebyse = 1;
				xdstart();
				return;		/* redo xfer sector by sector */
			}
		}

done:
		xd_sebyse = 0;
		/* done with this transfer, with or without error */
		xdtab.b_actf = dp->b_forw;
		xdtab.b_errcnt = 0;
		du->dk_skip = 0;
		dp->b_active = 0;
		dp->b_actf = bp->av_forw;
		dp->b_errcnt = 0;
		bp->b_resid = 0;
		biodone(bp);
	}
	xdtab.b_active = 0;
	if (dp->b_actf)
		xdustart(du);		/* requeue disk if more io to do */
	if (xdtab.b_actf)
		xdstart();		/* start IO on next drive */
}

/*
 * Initialize a drive.
 */
xdopen(dev, flags)
	dev_t	dev;
	int	flags;
{
	register unsigned int unit;
	register struct buf *bp;
	register struct disk *du;
	struct dkbad *db;
	int i, error = 0;

	unit = XDUNIT(dev);
	if (unit >= NXD) return (ENXIO) ;
	du = &xddrives[unit];
	if (du->dk_open){
		du->dk_open++ ;
		return(0);	/* already is open, don't mess with it */
	}
#ifdef THE_BUG
	if (du->dk_state && DISKSTATE(du->dk_state) <= OPEN)
		return(0);
#endif
	du->dk_unit = unit;
	xdutab[unit].b_actf = NULL;
	/*if (flags & O_NDELAY)
		du->dk_state = WANTOPENRAW;
	else*/
		du->dk_state = WANTOPEN;
	/*
	 * Use the default sizes until we've read the label,
	 * or longer if there isn't one there.
	 */
	du->dk_dd = dflt_sizes;

	/*
	 * Recal, read of disk label will be done in xdcontrol
	 * during first read operation.
	 */
	bp = geteblk(512);
	bp->b_dev = dev & 0xff00;
	bp->b_blkno = bp->b_bcount = 0;
	bp->b_flags = B_READ;
	xdstrategy(bp);
	biowait(bp);
	if (bp->b_flags & B_ERROR) {
		error = ENXIO;
		du->dk_state = CLOSED;
		goto done;
	}
	if (du->dk_state == OPENRAW) {
		du->dk_state = OPENRAW;
		goto done;
	}
	/*
	 * Read bad sector table into memory.
	 */
	i = 0;
	do {
		bp->b_flags = B_BUSY | B_READ;
		bp->b_blkno = du->dk_dd.dk_secperunit - du->dk_dd.dk_nsectors
			+ i;
		if (du->dk_dd.dk_secsize > DEV_BSIZE)
			bp->b_blkno *= du->dk_dd.dk_secsize / DEV_BSIZE;
		else
			bp->b_blkno /= DEV_BSIZE / du->dk_dd.dk_secsize;
		bp->b_bcount = du->dk_dd.dk_secsize;
		bp->b_cylin = du->dk_dd.dk_ncylinders - 1;
		xdstrategy(bp);
		biowait(bp);
	} while ((bp->b_flags & B_ERROR) && (i += 2) < 10 &&
		i < du->dk_dd.dk_nsectors);
	db = (struct dkbad *)(bp->b_un.b_addr);
#define DKBAD_MAGIC 0x4321
	if ((bp->b_flags & B_ERROR) == 0 && db->bt_mbz == 0 &&
	    db->bt_flag == DKBAD_MAGIC) {
		dkbad[unit] = *db;
		du->dk_state = OPEN;
	} else {
		printf("xd%d: %s bad-sector file\n", unit,
		    (bp->b_flags & B_ERROR) ? "can't read" : "format error in");
		error = ENXIO ;
		du->dk_state = OPENRAW;
	}
done:
	bp->b_flags = B_INVAL | B_AGE;
	brelse(bp);
	if (error == 0)
		du->dk_open = 1;
	return (error);
}

/*
 * Implement operations other than read/write.
 * Called from xdstart or xdintr during opens and formats.
 * Uses finite-state-machine to track progress of operation in progress.
 * Returns 0 if operation still in progress, 1 if completed.
 */
xdcontrol(bp)
	register struct buf *bp;
{
	register struct disk *du;
	register unit;
	unsigned char  stat;
	int s, cnt;
	extern int bootdev, cyloffset;

	du = &xddrives[XDUNIT(bp->b_dev)];
	unit = du->dk_unit;
	switch (DISKSTATE(du->dk_state)) {

	tryagainrecal:
	case WANTOPEN:			/* set SDH, step rate, do restore */
#ifdef	XDDEBUG
		dprintf(DDSK,"xd%d: recal ", unit);
#endif
		s = splbio();		/* not called from intr level ... */
		outb(xdc+xd_sdh, XDSD_IBM | (unit << 4) 
			+ du->dk_dd.dk_ntracks-1);
		outb(xdc+xd_seccnt, du->dk_dd.dk_nsectors);
		outb(xdc+xd_command, 0x91);
		while ((stat = inb(xdc+xd_status)) & XDCS_BUSY) nulldev();

		outb(xdc+xd_sdh, XDSD_IBM | (unit << 4));
		xdtab.b_active = 1;
		outb(xdc+xd_command, XDCC_RESTORE | XD_STEP);
		du->dk_state++;
		splx(s);
		return(0);

	case RECAL:
		if ((stat = inb(xdc+xd_status)) & XDCS_ERR) {
			printf("xd%d: recal", du->dk_unit);
			if (unit == 0) {
				printf(": status %b error %b\n",
					stat, XDCS_BITS,
					inb(xdc+xd_error), XDERR_BITS);
				if (++xdtab.b_errcnt < RETRIES)
					goto tryagainrecal;
			}
			goto badopen;
		}
		xdtab.b_errcnt = 0;
		if (ISRAWSTATE(du->dk_state)) {
			du->dk_state = OPENRAW;
			return(1);
		}
retry:
#ifdef	XDDEBUG
		dprintf(DDSK,"rdlabel ");
#endif
cyloffset=0;
		/*
		 * Read in sector 0 to get the pack label and geometry.
		 */
		outb(xdc+xd_precomp, 0xff);/* sometimes this is head bit 3 */
		outb(xdc+xd_seccnt, 1);
		outb(xdc+xd_sector, 1);
		/*if (bp->b_dev == bootdev) {
			(xdc+xd_cyl_lo = cyloffset & 0xff;
			(xdc+xd_cyl_hi = cyloffset >> 8;
		} else {
			(xdc+xd_cyl_lo = 0;
			(xdc+xd_cyl_hi = 0;
		}*/
		outb(xdc+xd_cyl_lo, (cyloffset & 0xff));
		outb(xdc+xd_cyl_hi, (cyloffset >> 8));
		outb(xdc+xd_sdh, XDSD_IBM | (unit << 4));
		outb(xdc+xd_command, XDCC_READ);
		du->dk_state = RDLABEL;
		return(0);

	case RDLABEL:
		if ((stat = inb(xdc+xd_status)) & XDCS_ERR) {
			if (++xdtab.b_errcnt < RETRIES)
				goto retry;
			printf("xd%d: read label", unit);
			goto badopen;
		}

		insw(xdc+xd_data, bp->b_un.b_addr, 256);

		if (((struct disklabel *)
		    (bp->b_un.b_addr + LABELOFFSET))->dk_magic == DISKMAGIC) {
		       du->dk_dd =
			 * (struct disklabel *) (bp->b_un.b_addr + LABELOFFSET);
		} else {
			printf("xd%d: bad disk label\n", du->dk_unit);
			du->dk_state = OPENRAW;
		}
		s = splbio();		/* not called from intr level ... */
		outb(xdc+xd_sdh, XDSD_IBM | (unit << 4) 
			+ du->dk_dd.dk_ntracks-1);
		outb(xdc+xd_seccnt, du->dk_dd.dk_nsectors);
		outb(xdc+xd_command, 0x91);
		while ((stat = inb(xdc+xd_status)) & XDCS_BUSY) nulldev();
		splx(s);

		if (du->dk_state == RDLABEL)
			du->dk_state = RDBADTBL;
		/*
		 * The rest of the initialization can be done
		 * by normal means.
		 */
		return(1);

	default:
		panic("xdcontrol %x", du->dk_state );
	}
	/* NOTREACHED */

badopen:
	printf(": status %b error %b\n",
		stat, XDCS_BITS, inb(xdc+xd_error), XDERR_BITS);
	du->dk_state = OPENRAW;
	return(1);
}

xdclose(dev)
	dev_t dev;
{	struct disk *du;

	du = &xddrives[XDUNIT(dev)];
	du->dk_open-- ;
	/*if (du->dk_open == 0) du->dk_state = CLOSED ; does not work */
}

xdioctl(dev,cmd,addr,flag)
	dev_t dev;
	caddr_t addr;
{
	int unit = XDUNIT(dev);
	register struct disk *du;
	int error = 0;
	struct uio auio;
	struct iovec aiov;
	/*int xdformat();*/

	du = &xddrives[unit];

	switch (cmd) {

	case DIOCGDINFO:
		*(struct disklabel *)addr = du->dk_dd;
		break;

	case DIOCGDINFOP:
		*(struct disklabel **)addr = &(du->dk_dd);
		break;

#ifdef notyet
	case DIOCWFORMAT:
		if ((flag & FWRITE) == 0)
			error = EBADF;
		else {
			register struct format_op *fop;

			fop = (struct format_op *)addr;
			aiov.iov_base = fop->df_buf;
			aiov.iov_len = fop->df_count;
			auio.uio_iov = &aiov;
			auio.uio_iovcnt = 1;
			auio.uio_resid = fop->df_count;
			auio.uio_segflg = 0;
			auio.uio_offset =
				fop->df_startblk * du->dk_dd.dk_secsize;
			error = physio(xdformat, &rxdbuf[unit], dev, B_WRITE,
				minphys, &auio);
			fop->df_count -= auio.uio_resid;
			fop->df_reg[0] = du->dk_status;
			fop->df_reg[1] = du->dk_error;
		}
		break;
#endif

	default:
		error = ENOTTY;
		break;
	}
	return (error);
}

/*xdformat(bp)
	struct buf *bp;
{

	bp->b_flags |= B_FORMAT;
	return (xdstrategy(bp));
}*/

/*
 * Routines to do raw IO for a unit.
 */
xdread(dev, uio)			/* character read routine */
	dev_t dev;
	struct uio *uio;
{
	int unit = XDUNIT(dev) ;

	if (unit >= NXD) return(ENXIO);
	return(physio(xdstrategy, &rxdbuf[unit], dev, B_READ, minphys, uio));
}


xdwrite(dev, uio)			/* character write routine */
	dev_t dev;
	struct uio *uio;
{
	int unit = XDUNIT(dev) ;

	if (unit >= NXD) return(ENXIO);
	return(physio(xdstrategy, &rxdbuf[unit], dev, B_WRITE, minphys, uio));
}

xdsize(dev)
	dev_t dev;
{
	register unit = XDUNIT(dev) ;
	register xunit = minor(dev) & 07;
	register struct disk *du;
	register val ;

	return(21600);
#ifdef notdef
	if (unit >= NXD) return(-1);
	if (xddrives[unit].dk_state == 0) /*{
		val = xdopen (dev, 0) ;
		if (val < 0) return (val) ;
	}*/	return (-1) ;
	du = &xddrives[unit];
	return((int)((u_long)du->dk_dd.dk_partition[xunit].nblocks *
		du->dk_dd.dk_secsize / 512));
#endif
}

xddump(dev)			/* dump core after a system crash */
	dev_t dev;
{
#ifdef notyet
	register struct disk *du;	/* disk unit to do the IO */
	register struct xd1010 *xdp = (struct xd1010 *) VA_XD;
	register struct bt_bad *bt_ptr;
	long	num;			/* number of sectors to write */
	int	unit, xunit;
	long	cyloff, blknum, blkcnt;
	long	cylin, head, sector;
	long	secpertrk, secpercyl, nblocks, i;
	register char *addr;
	char	*end;
	extern	int dumplo, totalclusters;
	static  xddoingadump = 0 ;

	addr = (char *) PA_RAM;		/* starting address */
	/* size of memory to dump */
	num = totalclusters * CLSIZE - PA_RAM / PGSIZE;
	unit = XDUNIT(dev) ;		/* eventually support floppies? */
	xunit = minor(dev) & 7;		/* file system */
	/* check for acceptable drive number */
	if (unit >= NXD) return(ENXIO);

	du = &xddrives[unit];
	/* was it ever initialized ? */
	if (du->dk_state < OPEN) return (ENXIO) ;

	/* Convert to disk sectors */
	num = (u_long) num * PGSIZE / du->dk_dd.dk_secsize;

	/* check if controller active */
	/*if (xdtab.b_active) return(EFAULT); */
	if (xddoingadump) return(EFAULT);

	secpertrk = du->dk_dd.dk_nsectors;
	secpercyl = du->dk_dd.dk_secpercyl;
	nblocks = du->dk_dd.dk_partition[xunit].nblocks;
	cyloff = du->dk_dd.dk_partition[xunit].cyloff;

	/* check transfer bounds against partition size */
	if ((dumplo < 0) || ((dumplo + num) >= nblocks))
		return(EINVAL);

	/*xdtab.b_active = 1;		/* mark controller active for if we
					   panic during the dump */
	xddoingadump = 1  ;  i = 100000 ;
	while ((xdp->xd_status & XDCS_BUSY) && (i-- > 0)) nulldev() ;
	inb(xdc+xd_sdh = du->dk_sdh ;
	inb(xdc+xd_command = XDCC_RESTORE | XD_STEP;
	while (inb(xdc+xd_status & XDCS_BUSY) nulldev() ;
	
	blknum = dumplo;
	while (num > 0) {
#ifdef notdef
		if (blkcnt > MAXTRANSFER) blkcnt = MAXTRANSFER;
		if ((blknum + blkcnt - 1) / secpercyl != blknum / secpercyl)
			blkcnt = secpercyl - (blknum % secpercyl);
			    /* keep transfer within current cylinder */
#endif

		/* compute disk address */
		cylin = blknum / secpercyl;
		head = (blknum % secpercyl) / secpertrk;
		sector = blknum % secpertrk;
		sector++;		/* origin 1 */
		cylin += cyloff;

		/* 
		 * See if the current block is in the bad block list.
		 * (If we have one.)
		 */
	    		for (bt_ptr = dkbad[unit].bt_bad;
				bt_ptr->bt_cyl != -1; bt_ptr++) {
			if (bt_ptr->bt_cyl > cylin)
				/* Sorted list, and we passed our cylinder.
					quit. */
				break;
			if (bt_ptr->bt_cyl == cylin &&
				bt_ptr->bt_trksec == (head << 8) + sector) {
			/*
			 * Found bad block.  Calculate new block addr.
			 * This starts at the end of the disk (skip the
			 * last track which is used for the bad block list),
			 * and works backwards to the front of the disk.
			 */
				blknum = (du->dk_dd.dk_secperunit)
					- du->dk_dd.dk_nsectors
					- (bt_ptr - dkbad[unit].bt_bad) - 1;
				cylin = blknum / secpercyl;
				head = (blknum % secpercyl) / secpertrk;
				sector = blknum % secpertrk;
				break;
			}

		/* select drive.     */
		inb(xdc+xd_sdh = du->dk_sdh | (head&07);
		while ((inb(xdc+xd_status & XDCS_READY) == 0) nulldev();

		/* transfer some blocks */
		inb(xdc+xd_sector = sector;
		inb(xdc+xd_seccnt = 1;
		inb(xdc+xd_cyl_lo = cylin;
		if (du->dk_dd.dk_ntracks > 8) { 
			if (head > 7)
				inb(xdc+xd_precomp = 0;	/* set 3rd head bit */
			else
				inb(xdc+xd_precomp = 0xff;	/* set 3rd head bit */
		}
		inb(xdc+xd_cyl_hi = cylin >> 8;
#ifdef notdef
		/* lets just talk about this first...*/
		printf ("sdh 0%o sector %d cyl %d addr 0x%x\n",
			xdp->xd_sdh, xdp->xd_sector,
			xdp->xd_cyl_hi*256+xdp->xd_cyl_lo, addr) ;
		for (i=10000; i > 0 ; i--)
			;
		continue;
#endif
		inb(xdc+xd_command = XDCC_WRITE;
		
		/* Ready to send data?	*/
		while ((inb(xdc+xd_status & XDCS_DRQ) == 0) nulldev();
		if (inb(xdc+xd_status & XDCS_ERR) return(EIO) ;

		end = (char *)addr + du->dk_dd.dk_secsize;
		for (; addr < end; addr += 8) {
			xdp->xd_data = addr[0];
			xdp->xd_data = addr[1];
			xdp->xd_data = addr[2];
			xdp->xd_data = addr[3];
			xdp->xd_data = addr[4];
			xdp->xd_data = addr[5];
			xdp->xd_data = addr[6];
			xdp->xd_data = addr[7];
		}
		if (inb(xdc+xd_status & XDCS_ERR) return(EIO) ;
		/* Check data request (should be done).         */
		if (inb(xdc+xd_status & XDCS_DRQ) return(EIO) ;

		/* wait for completion */
		for ( i = 1000000 ; inb(xdc+xd_status & XDCS_BUSY ; i--) {
				if (i < 0) return (EIO) ;
				nulldev () ;
		}
		/* error check the xfer */
		if (inb(xdc+xd_status & XDCS_ERR) return(EIO) ;
		/* update block count */
		num--;
		blknum++ ;
#ifdef	XDDEBUG
if (num % 100 == 0) printf(".") ;
#endif
	}
	return(0);
#endif
}
#endif
