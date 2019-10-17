/*#define LBA48	1*/
/* #define LBA */
/*#define CHS	1*/
/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 */

/* standard ISA/AT configuration: */
static char *wd_config =
	"wd 0 3 2 (0x1f0 14) (0x170 15).	# ide driver ";
#define	NWD 4	/* XXX dynamic */

#include "sys/param.h"
#include "sys/errno.h"
#include "sys/file.h"
#include "sys/stat.h"
#include "sys/ioctl.h"
#include "sys/syslog.h"
#include "dkbad.h"
#include "disklabel.h"
#include	"sys/uio.h"
#include	"sys/time.h"
#include	"sys/mount.h"
#include	"vnode.h"
#include "buf.h"
#include "uio.h"
#include "malloc.h"
#include "machine/cpu.h"
#include "isa_driver.h"
#include "isa_irq.h"
#include "machine/icu.h"
#include "wdreg.h"
#include "vm.h"
#include "modconfig.h"
#include "prototypes.h"
#include "machine/inline/io.h"	/* inline io port functions */

#define	RETRIES		5	/* number of retries before giving up */
#define	MAXTRANSFER	32	/* max size of transfer in page clusters */

#define wdnoreloc(dev)	(minor(dev) & 0x80)	/* ignore partition table */
#define wddospart(dev)	(minor(dev) & 0x40)	/* use dos partitions */
#define wdunit(dev)	((minor(dev) & 0x38) >> 3)
#define wdpart(dev)	(minor(dev) & 0x7)
#define makewddev(maj, unit, part)	(makedev(maj,((unit<<3)+part)))
#define WDRAW	3		/* 'd' partition isn't a partition! */

#define b_cylin	b_resid		/* cylinder number for doing IO to */
				/* shares an entry in the buf struct */

/*
 * Drive states.  Used to initialize drive.
 */

#define	CLOSED		0		/* disk is closed. */
#define	WANTOPEN	1		/* open requested, not started */
#define	RECAL		2		/* doing restore */
#define	OPEN		3		/* done with open */

/*
 * The structure of a disk drive.
 */
struct	disk {
	long	dk_bc;		/* byte count left */
	short	dk_skip;	/* blocks already transferred */
	char	dk_unit;	/* physical unit number */
	char	dk_state;	/* control state */
	u_char	dk_status;	/* copy of status reg. */
	u_char	dk_error;	/* copy of error reg. */
	short	dk_port;	/* i/o port base */
	
        u_long  dk_copenpart;   /* character units open on this drive */
        u_long  dk_bopenpart;   /* block units open on this drive */
        u_long  dk_openpart;    /* all units open on this drive */
	short	dk_wlabel;	/* label writable? */
	short	dk_flags;	/* drive characteistics found */
#define	DKFL_DOSPART	0x00001	 /* has DOS partition table */
#define	DKFL_QUIET	0x00002	 /* report errors back, but don't complain */
#define	DKFL_SINGLE	0x00004	 /* sector at a time mode */
#define	DKFL_ERROR	0x00008	 /* processing a disk error */
#define	DKFL_BSDLABEL	0x00010	 /* has a BSD disk label */
#define	DKFL_BADSECT	0x00020	 /* has a bad144 badsector table */
#define	DKFL_WRITEPROT	0x00040	 /* manual unit write protect */
#define	DKFL_LBA	0x00080	 /* LBA (implies DMA also) instead of CHS, e.g. ATA not (e)ide/"st506" */
#define	DKFL_LBA48	0x00100	 /* LBA48  (implies UltraDMA etc)*/
#define	DKFL_CHS(s)	(((s) & (DKFL_LBA|DKFL_LBA48)) == 0)	/* if not LBA then CHS */
	struct wdparams dk_params; /* ESDI/IDE drive/controller parameters */
	struct disklabel dk_dd;	/* device configuration data */
	struct	dos_partition
		dk_dospartitions[NDOSPART];	/* DOS view of disk */
	struct	dkbad	dk_bad;	/* bad sector table */
	int	dk_alive;
};

static struct	disk	*wddrives[NWD];		/* table of units */
static struct	buf	wdtab;
static struct	buf	wdutab[NWD];		/* head of queue per drive */
static struct	buf	rwdbuf[NWD];		/* buffers for raw IO */
static long	wdxfer[NWD];			/* count of transfers */
#ifdef	WDDEBUG
int	wddebug;
#endif

/* per driver interface structure */
struct	isa_driver wddriver = {
	wdprobe, wdattach, wdintr, "wd", &biomask
};


/* default per device */
static struct isa_device wd_default_devices[] = {
{ &wddriver,    0x1f0, IRQ14, -1,  0x00000,     0,  0 },
{ &wddriver,    0x170, IRQ15, -1,  0x00000,     0,  2 },
{ 0 }
};


static void wdustart(struct disk *);
static void wdstart(void);
static void wdfinished(struct buf *, struct buf *);
static int wdcommand(struct disk *, int, int);
static int wdselect(struct disk *, int, int);
static int Rwdselect(struct disk *, int, int);
static int wdcontrol(struct buf *);
static void wdsetctlr(dev_t, struct disk *);
static int wdgetctlr(int, struct disk *);


/*
 * Probe for controller.
 */
static int
wdprobe(struct isa_device *dvp)
{
	int unit = dvp->id_unit;
	struct disk *du;
	static int lastunit;
	int wdc, ok = 0;

	/* if wildcard unit number, use last to be allocated unit */
	if (unit == '?')
		dvp->id_unit = unit = lastunit;

	/* check for both drives/controllers */
	for (; unit <= dvp->id_unit+1; unit++) {

	if (unit > NWD)
		break;

	if ((du = wddrives[unit]) == 0) {
		du = wddrives[unit] = (struct disk *)
			malloc (sizeof(struct disk), M_TEMP, M_WAITOK);
		memset(du, 0, sizeof(struct disk));
		du->dk_unit = unit;
	}
	wdc = du->dk_port = dvp->id_iobase;
	
	/* unit to select? */
	if (wdselect(du, unit, 0) < 0)
		goto nodevice;

	/* is there a controller: execute a controller only command */
	if (wdcommand(du, WDCC_DIAGNOSE, 1) < 0)
		goto nodevice;

	/* is there a drive present: execute a disk only command */
	if (wdcommand(du, WDCC_RESTORE, 1) < 0)
		goto nodevice;

	(void) __inb(wdc+wd_error);	/* XXX! */
	__outb(wdc+wd_ctlr, WDCTL_4BIT);
	ok |= 1;

	/* XXX should we do a disklabel read as well? we then could report what media label this controller's drive it is? */
	continue;

nodevice:
	free(du, M_TEMP);
	wddrives[unit] = 0;
	}

	lastunit = unit;
	return (ok);
}

/*
 * Attach each drive if possible.
 */
static void
wdattach(struct isa_device *dvp)
{
	int unit = dvp->id_unit;
	struct disk *du;

	for (; unit <= dvp->id_unit + 1; unit++) {

	if ((du = wddrives[unit]) == 0)
		continue;
printf("wd%d", unit);


	if(wdgetctlr(unit, du) == 0)  {
		int i, blank;
		char c;
printf("%d/%d/%d ", du->dk_params.wdp_fixedcyl,  du->dk_params.wdp_heads, du->dk_params.wdp_sectors);
		printf(" <");
		for (i = blank = 0 ; i < sizeof(du->dk_params.wdp_model); i++) {
			char c = du->dk_params.wdp_model[i];

			if (blank && c == ' ') continue;
			if (blank && c != ' ') {
				printf(" %c", c);
				blank = 0;
				continue;
			} 
			if (c == ' ')
				blank = 1;
			else
				printf("%c", c);
		}
		printf("> ");
	}
/* check for index pulses from each drive. if present, report and
   allocate a bios drive position to it, which will be used by read disklabel */
	du->dk_unit = unit;
	du->dk_alive = 1;
	}
}

/* Read/write routine for a buffer.  Finds the proper unit, range checks
 * arguments, and schedules the transfer.  Does not wait for the transfer
 * to complete.  Multi-page transfers are supported.  All I/O requests must
 * be a multiple of a sector in length.
 */
static int
wdstrategy(register struct buf *bp)
{
	register struct buf *dp;
	struct disklabel *lp;
	register struct partition *p;
	struct disk *du;	/* Disk unit to do the IO.	*/
	long maxsz, sz;
	int	unit = wdunit(bp->b_dev);
	int	s;

	/* valid unit, controller, and request?  */
	if (unit > NWD || bp->b_blkno < 0 || (du = wddrives[unit]) == 0) {
		bp->b_error = EINVAL;
		bp->b_flags |= B_ERROR;
		goto done;
	}

	/* "soft" write protect check */
	if ((du->dk_flags & DKFL_WRITEPROT) && (bp->b_flags & B_READ) == 0) {
		bp->b_error = EROFS;
		bp->b_flags |= B_ERROR;
		goto done;
	}

	/* have partitions and want to use them? */
	if ((du->dk_flags & DKFL_BSDLABEL) != 0 && wdpart(bp->b_dev) != WDRAW) {

		/*
		 * do bounds checking, adjust transfer. if error, process.
		 * if end of partition, just return
		 */
		if (bounds_check_with_label(bp, &du->dk_dd, du->dk_wlabel) <= 0)
			goto done;
		/* otherwise, process transfer request */
	}

#ifdef SPECIALDEBUG

{ int blknum = bp->b_blkno; 
	lp = &du->dk_dd;
	if ((du->dk_flags & DKFL_BSDLABEL) != 0 && wdpart(bp->b_dev) != WDRAW)
		blknum += lp->d_partitions[wdpart(bp->b_dev)].p_offset;
if(blknum > 65312 &&  blknum < 398944 && *(int *)(bp->b_un.b_addr) == 0
	&& (bp->b_flags & B_READ) == 0 && bp->b_lblkno == 0) {
printf("%s:\n", bp->b_vp->v_name);
	Debugger();
}
}
#endif
q:
	/* queue transfer on drive, activate drive and controller if idle */
	dp = &wdutab[unit];
	s = splbio();
	disksort(dp, bp);
	if (dp->b_active == 0)
		wdustart(du);		/* start drive */
	if (wdtab.b_active == 0)
		wdstart();		/* start controller */
	splx(s);
	return;

done:
	/* toss transfer, we're done early */
	biodone(bp);
}

/*
 * Routine to queue a command to the controller.  The unit's
 * request is linked into the active list for the controller.
 * If the controller is idle, the transfer is started.
 */
static void
wdustart(register struct disk *du)
{
	register struct buf *bp, *dp = &wdutab[du->dk_unit];

	/* unit already active? */
	if (dp->b_active)
		return;

	/* anything to start? */
	bp = dp->b_actf;
	if (bp == NULL)
		return;	

	/* link onto controller queue */
	dp->b_forw = NULL;
	if (wdtab.b_actf  == NULL)
		wdtab.b_actf = dp;
	else
		wdtab.b_actl->b_forw = dp;
	wdtab.b_actl = dp;

	/* mark the drive unit as busy */
	dp->b_active = 1;
}

/*
 * Controller startup routine.  This does the calculation, and starts
 * a single-sector read or write operation.  Called to start a transfer,
 * or from the interrupt routine to continue a multi-sector transfer.
 * RESTRICTIONS:
 * 1.	The transfer length must be an exact multiple of the sector size.
 */

static void
wdstart(void)
{
	register struct disk *du;	/* disk unit for IO */
	register struct buf *bp;
	struct disklabel *lp;
	struct buf *dp;
	register struct bt_bad *bt_ptr;
	long	blknum, pagcnt, cylin, head, sector;
	long	secpertrk, secpercyl, i;
	int	unit, s, wdc;
	caddr_t addr;

loop:
	/* is there a drive for the controller to do a transfer with? */
	dp = wdtab.b_actf;
	if (dp == NULL)
		return;

	/* is there a transfer to this drive ? if so, link it on
	   the controller's queue */
	bp = dp->b_actf;
	if (bp == NULL) {
		wdtab.b_actf = dp->b_forw;
		goto loop;
	}

	/* obtain controller and drive information */
	unit = wdunit(bp->b_dev);
	du = wddrives[unit];

	/* if not really a transfer, do control operations specially */
	if (du->dk_state < OPEN) {
		(void) wdcontrol(bp);
		return;
	}

	/* calculate transfer details */
	blknum = bp->b_blkno + du->dk_skip;
/*if(wddebug)printf("bn%d ", blknum);*/
#ifdef	WDDEBUG
	if (du->dk_skip == 0)
		printf("\nwdstart %d: %s %d@%d; map ", unit,
			(bp->b_flags & B_READ) ? "read" : "write",
			bp->b_bcount, blknum);
	else
		printf(" %d)%x", du->dk_skip, __inb(wdc+wd_altsts));
#endif
	addr = bp->b_un.b_addr;
	if (du->dk_skip == 0) {
		du->dk_bc = min(bp->b_bcount, MAXTRANSFER * NBPG);
		bp->b_resid = bp->b_bcount - du->dk_bc;
	}

	lp = &du->dk_dd;
	secpertrk = lp->d_nsectors;
	secpercyl = lp->d_secpercyl;
	if ((du->dk_flags & DKFL_BSDLABEL) != 0 && wdpart(bp->b_dev) != WDRAW)
		blknum += lp->d_partitions[wdpart(bp->b_dev)].p_offset;

	if (DKFL_CHS(du->dk_flags)) {
		cylin = blknum / secpercyl;
		head = (blknum % secpercyl) / secpertrk;
		sector = blknum % secpertrk;
	} else { /* LBA */
		sector = blknum & 0xff;
		cylin = (blknum >> 8) & 0xffff;
		head = (blknum & 0xFF000000) >> 24;
	}

#ifdef forget_it
	/* 
	 * See if the current block is in the bad block list.
	 * (If we have one, and not formatting.)
	 */
	if ((du->dk_flags & (DKFL_SINGLE|DKFL_BADSECT))
		== (DKFL_SINGLE|DKFL_BADSECT))
	    for (bt_ptr = du->dk_bad.bt_bad; bt_ptr->bt_cyl != 0xffff; bt_ptr++) {
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
#ifdef	WDDEBUG
			    printf("--- badblock code -> Old = %d; ",
				blknum);
#endif
			blknum = lp->d_secperunit - lp->d_nsectors
				- (bt_ptr - du->dk_bad.bt_bad) - 1;
			if (DKFL_CHS(du->dk_flags)) {
				cylin = blknum / secpercyl;
				head = (blknum % secpercyl) / secpertrk;
				sector = blknum % secpertrk;
			} else { /* LBA */
				sector = blknum & 0xff;
				cylin = (blknum >> 8) & 0xffff;
				head = (blknum & 0xFF000000) >> 24;
			}
#ifdef	WDDEBUG
			    printf("new = %d\n", blknum);
#endif
			break;
		}
	}
/*if(wddebug)pg("c%d h%d s%d ", cylin, head, sector);*/
#endif

	wdtab.b_active = 1;		/* mark controller active */
	wdc = du->dk_port;

	/* if starting a multisector transfer, or doing single transfers */
	if (du->dk_skip == 0 || (du->dk_flags & DKFL_SINGLE)) {
		if (wdtab.b_errcnt && (bp->b_flags & B_READ) == 0)
			du->dk_bc += DEV_BSIZE;

		/* controller idle? */
		while (__inb(wdc+wd_status) & WDCS_BUSY)
			;

#ifdef notdef
		/* stuff the task file */
		__outb(wdc+wd_precomp, lp->d_precompcyl / 4);
#endif
#ifdef	B_FORMAT
		if (bp->b_flags & B_FORMAT) {
			__outb(wdc+wd_sector, lp->d_gap3);
			__outb(wdc+wd_seccnt, lp->d_nsectors);
		} else {
#endif
		/* grab the drive */
		if (Rwdselect(du, unit, head) < 0) {
			bp->b_flags |= B_ERROR;
			bp->b_error = EIO;	/* XXX needs translation */
			wdfinished(dp, bp);
			goto loop;
		}


		if (DKFL_CHS(du->dk_flags))
			__outb(wdc+wd_sector, sector + 1);	/* CHS - origin 1 */
		else {
			if (du->dk_flags & DKFL_LBA48) {
				__outb(wdc+wd_sector, head >> 24);
				__outb(wdc+wd_cyl_lo, 0);
				__outb(wdc+wd_cyl_hi, 0);
				__outb(wdc+wd_seccnt, 0);
			}
			__outb(wdc+wd_sector, sector);
		}
#ifdef	B_FORMAT
		}
#endif

		__outb(wdc+wd_cyl_lo, cylin);
		__outb(wdc+wd_cyl_hi, cylin >> 8);
		if (du->dk_flags & DKFL_SINGLE)
			__outb(wdc+wd_seccnt, 1);
		else
			__outb(wdc+wd_seccnt, howmany(du->dk_bc, DEV_BSIZE));


		/* initiate command! */
#ifdef	B_FORMAT
		if (bp->b_flags & B_FORMAT)
			__outb(wdc+wd_command, WDCC_FORMAT);
		else
#endif
		if (wdcommand(du,
		    (bp->b_flags & B_READ) ?
			((du->dk_flags & DKFL_LBA48)? 0x24 : WDCC_READ) :
			((du->dk_flags & DKFL_LBA48)? 0x34 : WDCC_WRITE)
		    , 0) < 0) {
#ifdef notdef
#ifdef LBA48
		    (bp->b_flags & B_READ)? 0x24 : 0x34, 0) < 0) {
#else
		    (bp->b_flags & B_READ)? WDCC_READ : WDCC_WRITE, 0) < 0) {
#endif
#endif
			bp->b_flags |= B_ERROR;
			bp->b_error = EIO;	/* XXX needs translation */
			wdfinished(dp, bp);
			goto loop;
		}
#ifdef	WDDEBUG
		printf("sector %d cylin %d head %d addr %x sts %x\n",
	    		sector, cylin, head, addr, __inb(wdc+wd_altsts));
#endif
	}

	/* if this is a read operation, just go away until it's done.	*/
	if (bp->b_flags & B_READ)
		return;

	/* ready to send data?	*/
	while ((__inb(wdc+wd_status) & WDCS_DRQ) == 0)
		;

	/* then send it! */
	outsw (wdc+wd_data, addr+du->dk_skip * DEV_BSIZE,
		DEV_BSIZE/sizeof(short));
	du->dk_bc -= DEV_BSIZE;
}

/* Interrupt routine for the controller.  Acknowledge the interrupt, check for
 * errors on the current operation, mark it done if necessary, and start
 * the next request.  Also check for a partially done transfer, and
 * continue with the next chunk if so.
 */
void
wdintr(int dev)
{
	register struct	disk *du;
	register struct buf *bp, *dp;
	int status, wdc;
	char partch ;

	if (!wdtab.b_active) {
#ifdef nyet
		printf("wd: extra interrupt\n");
#endif
		return;
	}

	dp = wdtab.b_actf;
	bp = dp->b_actf;
	du = wddrives[wdunit(bp->b_dev)];
	wdc = du->dk_port;

#ifdef	WDDEBUG
	printf("I ");
#endif

	/* controller still busy? */
	while ((status = __inb(wdc+wd_status)) & WDCS_BUSY) ;

	/* is it not a transfer, but a control operation? */
	if (du->dk_state < OPEN) {
		if (wdcontrol(bp))
			wdstart();
		return;
	}

	/* have we an error? */
	if (status & (WDCS_ERR | WDCS_ECCCOR)) {

		du->dk_status = status;
		du->dk_error = __inb(wdc + wd_error);
#ifdef	WDDEBUG
		printf("status %x error %x\n", status, du->dk_error);
#endif
		if((du->dk_flags & DKFL_SINGLE) == 0) {
			du->dk_flags |=  DKFL_ERROR;
			goto outt;
		}
#ifdef B_FORMAT
		if (bp->b_flags & B_FORMAT) {
			bp->b_flags |= B_ERROR;
			bp->b_error = EIO;
			goto done;
		}
#endif
		
		/* error or error correction? */
		if (status & WDCS_ERR) {
			if (++wdtab.b_errcnt < RETRIES) {
				wdtab.b_active = 0;
			} else {
				if((du->dk_flags&DKFL_QUIET) == 0) {
					diskerr(bp, "wd", "hard error",
						LOG_PRINTF, du->dk_skip,
						&du->dk_dd);
#ifdef WDDEBUG
					printf( "status %b error %b\n",
						status, WDCS_BITS,
						__inb(wdc+wd_error), WDERR_BITS);
#endif
				}
				bp->b_flags |= B_ERROR;	/* flag the error */
				bp->b_error = EIO;
			}
		} else if((du->dk_flags&DKFL_QUIET) == 0) {
				diskerr(bp, "wd", "soft ecc", 0,
					du->dk_skip, &du->dk_dd);
		}
	}
outt:

	/*
	 * If this was a successful read operation, fetch the data.
	 */
	if (((bp->b_flags & (B_READ | B_ERROR)) == B_READ) && wdtab.b_active) {
		int chk, dummy;

		chk = min(DEV_BSIZE / sizeof(short), du->dk_bc / sizeof(short));

		/* ready to receive data? */
		while ((__inb(wdc+wd_status) & WDCS_DRQ) == 0)
			;

		/* suck in data */
		insw (wdc+wd_data,
			bp->b_un.b_addr + du->dk_skip * DEV_BSIZE, chk);
		du->dk_bc -= chk * sizeof(short);

		/* for obselete fractional sector reads */
		while (chk++ < 256) insw (wdc+wd_data, (caddr_t) &dummy, 1);
	}

	wdxfer[du->dk_unit]++;
	if (wdtab.b_active) {
		if ((bp->b_flags & B_ERROR) == 0) {
			du->dk_skip++;		/* Add to successful sectors. */
			if (wdtab.b_errcnt && (du->dk_flags & DKFL_QUIET) == 0)
				diskerr(bp, "wd", "soft error", 0,
					du->dk_skip, &du->dk_dd);
			wdtab.b_errcnt = 0;

			/* see if more to transfer */
			if (du->dk_bc > 0 && (du->dk_flags & DKFL_ERROR) == 0) {
				wdstart();
				return;		/* next chunk is started */
			} else if ((du->dk_flags & (DKFL_SINGLE|DKFL_ERROR))
					== DKFL_ERROR) {
				du->dk_skip = 0;
				du->dk_flags &= ~DKFL_ERROR;
				du->dk_flags |=  DKFL_SINGLE;
				wdstart();
				return;		/* redo xfer sector by sector */
			}
		}

done:
		/* done with this transfer, with or without error */
		du->dk_flags &= ~DKFL_SINGLE;
		wdtab.b_actf = dp->b_forw;
		wdtab.b_errcnt = 0;
		du->dk_skip = 0;
		dp->b_active = 0;
		dp->b_actf = bp->av_forw;
		dp->b_errcnt = 0;
		/* bp->b_resid = 0; */
		biodone(bp);
	}

	/* controller idle */
	wdtab.b_active = 0;

	/* anything more on drive queue? */
	if (dp->b_actf)
		wdustart(du);
	/* anything more for controller to do? */
	if (wdtab.b_actf)
		wdstart();
}

static void
wdfinished(struct buf *dp, struct buf *bp) {

	wdtab.b_actf = dp->b_forw;
	wdtab.b_errcnt = 0;
	dp->b_active = 0;
	dp->b_actf = bp->av_forw;
	dp->b_errcnt = 0;
	biodone(bp);
}

/*
 * Initialize a drive.
 */
static int
wdopen(dev_t dev, int flags, int fmt, struct proc *p)
{
	register unsigned int unit;
	register struct disk *du;
        int part = wdpart(dev), mask = 1 << part;
        struct partition *pp;
	struct dkbad *db;
	int i, error = 0;
	char *msg;

	unit = wdunit(dev);
	if (unit > NWD) return (ENXIO) ;

	du = wddrives[unit];
	if (du == 0 || du->dk_alive == 0) return (ENXIO) ;

	if ((du->dk_flags & DKFL_BSDLABEL) == 0) {
		/* du->dk_flags |= DKFL_WRITEPROT; */
		wdutab[unit].b_actf = NULL;

		/*
		 * Use the default sizes until we've read the label,
		 * or longer if there isn't one there.
		 */
		(void)memset(&du->dk_dd, 0, sizeof(du->dk_dd));
		du->dk_dd.d_type = DTYPE_ST506;
		du->dk_dd.d_ncylinders = 1024;
		du->dk_dd.d_secsize = DEV_BSIZE;
		du->dk_dd.d_ntracks = 16; /*8;*/
		du->dk_dd.d_nsectors = 63; /*17;*/
		du->dk_dd.d_secpercyl = 63*16; /*17*8;*/
		du->dk_state = WANTOPEN;
		du->dk_unit = unit;
		if (part == WDRAW)
			du->dk_flags |= DKFL_QUIET;
		else
			du->dk_flags &= ~DKFL_QUIET;

		/* read label using "c" partition */
		if (msg = readdisklabel(makewddev(major(dev), wdunit(dev), WDRAW),
				wdstrategy, &du->dk_dd, du->dk_dospartitions,
				&du->dk_bad, 0)) {
			if((du->dk_flags&DKFL_QUIET) == 0) {
				log(LOG_WARNING, "wd%d: cannot find label (%s)\n",
					unit, msg);
				error = EINVAL;		/* XXX needs translation */
			} else printf("quiet ");
			goto done;
		} else {

			wdsetctlr(dev, du);
			du->dk_flags |= DKFL_BSDLABEL;
			du->dk_flags &= ~DKFL_WRITEPROT;
			if (du->dk_dd.d_flags & D_BADSECT)
				du->dk_flags |= DKFL_BADSECT;
		}

done:
		if (error)
			return(error);

	}
        /*
         * Warn if a partion is opened
         * that overlaps another partition which is open
         * unless one is the "raw" partition (whole disk).
         */
        if ((du->dk_openpart & mask) == 0 /*&& part != RAWPART*/ && part != WDRAW) {
		int	start, end;

                pp = &du->dk_dd.d_partitions[part];
                start = pp->p_offset;
                end = pp->p_offset + pp->p_size;
                for (pp = du->dk_dd.d_partitions;
                     pp < &du->dk_dd.d_partitions[du->dk_dd.d_npartitions];
			pp++) {
                        if (pp->p_offset + pp->p_size <= start ||
                            pp->p_offset >= end)
                                continue;
                        /*if (pp - du->dk_dd.d_partitions == RAWPART)
                                continue; */
                        if (pp - du->dk_dd.d_partitions == WDRAW)
                                continue;
                        if (du->dk_openpart & (1 << (pp -
					du->dk_dd.d_partitions)))
                                log(LOG_WARNING,
                                    "wd%d%c: overlaps open partition (%c)\n",
                                    unit, part + 'a',
                                    pp - du->dk_dd.d_partitions + 'a');
                }
        }
        if (part >= du->dk_dd.d_npartitions && part != WDRAW)
                return (ENXIO);

	/* insure only one open at a time */
        du->dk_openpart |= mask;
        switch (fmt) {
        case S_IFCHR:
                du->dk_copenpart |= mask;
                break;
        case S_IFBLK:
                du->dk_bopenpart |= mask;
                break;
        }
	return (0);
}

/*
 * Implement operations other than read/write.
 * Called from wdstart or wdintr during opens and formats.
 * Uses finite-state-machine to track progress of operation in progress.
 * Returns 0 if operation still in progress, 1 if completed.
 */
static int
wdcontrol(register struct buf *bp)
{
	register struct disk *du;
	int unit;
	unsigned char  stat;
	int s, cnt;
	extern int bootdev;
	int cyl, trk, sec, i, wdc;

	du = wddrives[wdunit(bp->b_dev)];
	unit = du->dk_unit;
	wdc = du->dk_port;
	
	switch (du->dk_state) {

	tryagainrecal:
	case WANTOPEN:			/* set SDH, step rate, do restore */
#ifdef	WDDEBUG
		printf("wd%d: recal ", unit);
#endif
		s = splbio();		/* not called from intr level ... */
		wdgetctlr(unit, du);

		if (wdselect(du, unit, 0) < 0)
			goto badopen;
		wdtab.b_active = 1;
		if (wdcommand(du, WDCC_RESTORE | WD_STEP, 0) < 0)
			goto badopen;
		du->dk_state++;
		splx(s);
		return(0);

	case RECAL:
		if ((stat = __inb(wdc+wd_status)) & WDCS_ERR) {
			if ((du->dk_flags & DKFL_QUIET) == 0) {
				printf("wd%d: recal", du->dk_unit);
				printf(": status %b error %b\n",
					stat, WDCS_BITS, __inb(wdc+wd_error),
					WDERR_BITS);
			}
			if (++wdtab.b_errcnt < RETRIES)
				goto tryagainrecal;
			goto badopen;
		}

		/* some controllers require this ... */
		wdsetctlr(bp->b_dev, du);

		wdtab.b_errcnt = 0;
		du->dk_state = OPEN;
		/*
		 * The rest of the initialization can be done
		 * by normal means.
		 */
		return(1);

	default:
		panic("wdcontrol");
	}
	/* NOTREACHED */

badopen:
	if ((du->dk_flags & DKFL_QUIET) == 0) 
		printf(": status %b error %b\n",
			stat, WDCS_BITS, __inb(wdc + wd_error), WDERR_BITS);
	bp->b_flags |= B_ERROR;
	bp->b_error = ENXIO;	/* XXX needs translation */
	return(1);
}

/*
 * send a command and optionally wait uninterruptibly until controller
 * is finished.
 * return -1 if controller busy for too long, otherwise
 * return status. intended for brief controller commands at critical points.
 * assumes interrupts are blocked if wait requested.
 */
static int
wdcommand(struct disk *du, int cmd, int wait) {
	int timeout = 1000000, stat, wdc;

	/* controller ready for command? */
	wdc = du->dk_port;
	while (((stat = __inb(wdc + wd_status)) & WDCS_BUSY) && timeout > 0) {
		DELAY(10);
		timeout--;
	}
	if (timeout <= 0)
		return(-1);

	/* send command, await results */
	__outb(wdc+wd_command, cmd);
	if (wait == 0)
		return (0);
	while (((stat = __inb(wdc+wd_status)) & WDCS_BUSY) && timeout > 0) {
		DELAY(10);
		timeout--;
	}
	if (timeout <= 0)
		return(-1);
	if (cmd != WDCC_READP && cmd != WDCC_READ
	    && cmd != WDCC_WRITE && cmd != WDCC_FORMAT)
		return (0);

	/* is controller ready to return data? */
	while (((stat = __inb(wdc+wd_status)) & (WDCS_ERR|WDCS_DRQ)) == 0 &&
		timeout > 0) {
		DELAY(10);
		timeout--;
	}
	if (timeout <= 0)
		return(-1);

	return ((stat & WDCS_ERR) ? -2 : 0);
}

/*
 * select a drive and wait for the drive to come ready.
 * return -1 if drive does not come ready in time after selection,
 * otherwise return 0.
 */
static int
wdselect(struct disk *du, int unit, int head) {
	int timeout = 100000, wdc;

	/* is controller idle so we can switch unit and/or heads? */
	wdc = du->dk_port;
	while ((__inb(wdc + wd_status) & WDCS_BUSY) && timeout > 0) {
		DELAY(10);
		timeout--;
	}
	if (timeout <= 0)
		return(-1);

	/* select drive */
	/* XXX if slave flip drive selects. why?? */ if (unit & 2) __outb(wdc+wd_sdh, WDSD_IBM | ((unit^1)<<4) | (head & 0xf)); else
	__outb(wdc+wd_sdh, WDSD_IBM | (unit<<4) | (head & 0xf));
#ifdef notdef
	/* XXX if slave flip drive selects. why?? */ if (unit & 2) __outb(wdc+wd_sdh, 0xe0 | ((unit^1)<<4) | (head & 0xf)); else
	__outb(wdc+wd_sdh, 0xe0 | (unit<<4) | (head & 0xf));
#endif

	/* has drive come ready for a command? */
	while ((__inb(wdc+wd_status) & (WDCS_READY|WDCS_BUSY)) != WDCS_READY && timeout > 0) {
		DELAY(10);
		timeout--;
	}
	if (timeout <= 0)
		return(-1);

	return (0);
}

static int
Rwdselect(struct disk *du, int unit, int head) {
	int timeout = 100000, wdc, select;

	/* is controller idle so we can switch unit and/or heads? */
	wdc = du->dk_port;
	while ((__inb(wdc + wd_status) & WDCS_BUSY) && timeout > 0) {
		DELAY(10);
		timeout--;
	}
	if (timeout <= 0)
		return(-1);

	/* select drive */
	if (DKFL_CHS(du->dk_flags))
		select = WDSD_IBM;
	else if (du->dk_flags& DKFL_LBA)
		select = 0xe0;
	else if (du->dk_flags& DKFL_LBA48) {
		head = 0;
		select = 0x40;
	}
	/* XXX if slave flip drive selects. why?? */ if (unit & 2) __outb(wdc+wd_sdh, select | ((unit^1)<<4) | (head & 0xf)); else
	__outb(wdc+wd_sdh, select | (unit<<4) | (head & 0xf));
#ifdef notdef
#ifdef CHS
	/* XXX if slave flip drive selects. why?? */ if (unit & 2) __outb(wdc+wd_sdh, WDSD_IBM | ((unit^1)<<4) | (head & 0xf)); else
	__outb(wdc+wd_sdh, WDSD_IBM | (unit<<4) | (head & 0xf));
#else
#ifdef LBA48
	/* XXX if slave flip drive selects. why?? */ if (unit & 2) __outb(wdc+wd_sdh, 0x40 | ((unit^1)<<4)); else
	__outb(wdc+wd_sdh, 0x40 | (unit<<4));
#else
	/* XXX if slave flip drive selects. why?? */ if (unit & 2) __outb(wdc+wd_sdh, 0xe0 | ((unit^1)<<4) | (head & 0xf)); else
	__outb(wdc+wd_sdh, 0xe0 | (unit<<4) | (head & 0xf));
#endif
#endif
#endif

	/* has drive come ready for a command? */
	while ((__inb(wdc+wd_status) & (WDCS_READY|WDCS_BUSY)) != WDCS_READY && timeout > 0) {
		DELAY(10);
		timeout--;
	}
	if (timeout <= 0)
		return(-1);

	return (0);
}

/*
 * issue IDC to drive to tell it just what geometry it is to be.
 */
static void
wdsetctlr(dev_t dev, struct disk *du) {
	int stat, x, wdc;

/*printf("C%dH%dS%d ", du->dk_dd.d_ncylinders, du->dk_dd.d_ntracks,
	du->dk_dd.d_nsectors);*/
if (DKFL_CHS(du->dk_flags)) {
	x = splbio();
	wdc = du->dk_port;
	__outb(wdc+wd_cyl_lo, du->dk_dd.d_ncylinders+1);
	__outb(wdc+wd_cyl_hi, (du->dk_dd.d_ncylinders+1)>>8);
	/* XXX if slave flip drive selects. why?? */ if (wdunit(dev) & 2) __outb(wdc+wd_sdh, WDSD_IBM | ((wdunit(dev)^1)<<4) | ((du->dk_dd.d_ntracks-1) & 0xf)); else
	__outb(wdc+wd_sdh, WDSD_IBM | (wdunit(dev) << 4) + du->dk_dd.d_ntracks-1);
	/* lba __outb(wdc+wd_sdh, 0xe0 | (wdunit(dev) << 4) + du->dk_dd.d_ntracks-1); */

	__outb(wdc+wd_seccnt, du->dk_dd.d_nsectors);
	stat = wdcommand(du, WDCC_IDC, 1);

	if (stat < 0) {
		splx(x);
		return;
	}
	if (stat & WDCS_ERR)
		printf("wdsetctlr: status %b error %b\n",
			stat, WDCS_BITS, __inb(wdc+wd_error), WDERR_BITS);
	splx(x);
}
}

/*
 * issue READP to drive to ask it what it is.
 */
static int
wdgetctlr(int u, struct disk *du) {
	int stat, x, i, wdc, err;
	char tb[DEV_BSIZE];
	struct wdparams *wp;
unsigned short Signature, Capabilities, ObsoleteCapabilities; unsigned long CommandSets, Size;

	x = splbio();		/* not called from intr level ... */
	wdc = du->dk_port;
	/* XXX if slave flip drive selects. why?? */ if (u & 2) __outb(wdc+wd_sdh, WDSD_IBM | ((u^1)<<4)); else
	__outb(wdc+wd_sdh, WDSD_IBM | (u << 4));
	stat = wdcommand(du, WDCC_READP, 1);

	if (stat < 0) {
		splx(x);
		return(stat);
	}
	if (stat & WDCS_ERR) {
		err = inb(wdc+wd_error);
		splx(x);
		return(err);
	}

	if (stat < 0) {
		splx(x);
		return(stat);
	}

	/* obtain parameters */
	wp = &du->dk_params;
	insw(wdc+wd_data, (caddr_t) tb, sizeof(tb)/sizeof(short));
	(void)memcpy(wp, tb, sizeof(struct wdparams));
printf(" perintsect %x ", wp->wdp_nsecperint);
#define ATA_IDENT_DEVICETYPE   0
#define ATA_IDENT_CYLINDERS    2
#define ATA_IDENT_HEADS        6
#define ATA_IDENT_SECTORS      12
#define ATA_IDENT_SERIAL       20
#define ATA_CAPABILITIES	49
#define ATA_IDENT_MODEL        54
#define ATA_IDENT_CAPABILITIES 98
#define ATA_IDENT_FIELDVALID   106
#define ATA_IDENT_MAX_LBA      120
#define ATA_IDENT_COMMANDSETS  164
#define ATA_IDENT_MAX_LBA_EXT  200
Signature    = *((unsigned short *)(tb + ATA_IDENT_DEVICETYPE));
Capabilities = *((unsigned short *)(tb + ATA_IDENT_CAPABILITIES));
ObsoleteCapabilities = *((unsigned short *)(tb + ATA_CAPABILITIES));
CommandSets  = *((unsigned long *)(tb + ATA_IDENT_COMMANDSETS));
printf("obsoletecaps: %x commandsets: %x ",  ObsoleteCapabilities, CommandSets);
#ifdef LBA48	/* force LBA48 */
	du->dk_flags |= DKFL_LBA48;
	printf("lba48 ");
#endif
#ifdef LBA	/* force LBA */
	du->dk_flags |= DKFL_LBA;
	printf("lba dma ");
#endif
#ifdef CHS	/* force CHS */
	printf("chs ");
#endif
#if !defined(LBA) && !defined(LBA48) && !defined(CHS)
         if (CommandSets & (1 << 26)) {
            /* Device uses 48-Bit Addressing:*/
            Size   = *((unsigned long *)(tb + ATA_IDENT_MAX_LBA_EXT));
printf("48bit ");
	du->dk_flags |= DKFL_LBA48;
         } else {
            /* Device uses CHS or LBA: 28-bit Addressing:*/
            Size   = *((unsigned long *)(tb + ATA_IDENT_MAX_LBA));
	    /*if ((ObsoleteCapabilities & 0x300) == 0x300) { standard isn't "standard"*/
	    if (
		*((unsigned short *)(tb + ATA_IDENT_HEADS)) == 16 &&
		*((unsigned short *)(tb + ATA_IDENT_SECTORS)) == 63) {
		du->dk_flags |= DKFL_LBA;
		printf("lba dma ");
	    } else
		printf("chs ");
	}
printf("size %dMB ", Size/(2*1024));
#endif

	/* shuffle string byte order */
	for (i=0; i < sizeof(wp->wdp_model) ;i+=2) {
		u_short *p;
		p = (u_short *) (wp->wdp_model + i);
		*p = ntohs(*p);
	}
/*printf("gc %x cyl %d trk %d sec %d type %d sz %d model %s\n", wp->wdp_config,
wp->wdp_fixedcyl+wp->wdp_removcyl, wp->wdp_heads, wp->wdp_sectors,
wp->wdp_cntype, wp->wdp_cnsbsz, wp->wdp_model);*/

	/* update disklabel given drive information */
	du->dk_dd.d_ncylinders = wp->wdp_fixedcyl + wp->wdp_removcyl /*+- 1*/;
	du->dk_dd.d_ntracks = wp->wdp_heads;
	du->dk_dd.d_nsectors = wp->wdp_sectors;
	du->dk_dd.d_secpercyl = du->dk_dd.d_ntracks * du->dk_dd.d_nsectors;
	du->dk_dd.d_partitions[1].p_size = du->dk_dd.d_secpercyl *
			wp->wdp_sectors;
	du->dk_dd.d_partitions[1].p_offset = 0;
	/* dubious ... */
	(void)memcpy(du->dk_dd.d_typename, "ESDI/IDE", 9);
	(void)memcpy(du->dk_dd.d_packname, wp->wdp_model+20, 14-1);
	/* better ... */
	du->dk_dd.d_type = DTYPE_ESDI;
	du->dk_dd.d_subtype |= DSTYPE_GEOMETRY;

	/* XXX sometimes possibly needed */
	(void) __inb(wdc+wd_status);
	splx(x);
	return (0);
}


/* ARGSUSED */
static int
wdclose(dev_t dev, int flags, int fmt, struct proc *p)
{
	register struct disk *du;
        int part = wdpart(dev), mask = 1 << part;

	du = wddrives[wdunit(dev)];

	/* insure only one open at a time */
        du->dk_openpart &= ~mask;
        switch (fmt) {
        case S_IFCHR:
                du->dk_copenpart &= ~mask;
                break;
        case S_IFBLK:
                du->dk_bopenpart &= ~mask;
                break;
        }
	return(0);
}

static int
wdioctl(dev_t dev, int cmd, caddr_t addr, int flag, struct proc *p)
{
	int unit = wdunit(dev);
	register struct disk *du;
	int error = 0;
	struct uio auio;
	struct iovec aiov;

	du = wddrives[unit];

	switch (cmd) {

	case DIOCSBAD:
                if ((flag & FWRITE) == 0)
                        error = EBADF;
		else
			du->dk_bad = *(struct dkbad *)addr;
		break;

	case DIOCGDINFO:
		*(struct disklabel *)addr = du->dk_dd;
		break;

        case DIOCGPART:
                ((struct partinfo *)addr)->disklab = &du->dk_dd;
                ((struct partinfo *)addr)->part =
                    &du->dk_dd.d_partitions[wdpart(dev)];
                break;

        case DIOCSDINFO:
                if ((flag & FWRITE) == 0)
                        error = EBADF;
                else
                        error = setdisklabel(&du->dk_dd,
					(struct disklabel *)addr,
                         /*(du->dk_flags & DKFL_BSDLABEL) ? du->dk_openpart : */0,
				du->dk_dospartitions);
                if (error == 0) {
			du->dk_flags |= DKFL_BSDLABEL;
			wdsetctlr(dev, du);
		}
                break;

        case DIOCWLABEL:
		du->dk_flags &= ~DKFL_WRITEPROT;
                if ((flag & FWRITE) == 0)
                        error = EBADF;
                else
                        du->dk_wlabel = *(int *)addr;
                break;

        case DIOCWDINFO:
		du->dk_flags &= ~DKFL_WRITEPROT;
                if ((flag & FWRITE) == 0)
                        error = EBADF;
                else if ((error = setdisklabel(&du->dk_dd, (struct disklabel *)addr,
                         /*(du->dk_flags & DKFL_BSDLABEL) ? du->dk_openpart :*/ 0,
				du->dk_dospartitions)) == 0) {
                        int wlab;

			du->dk_flags |= DKFL_BSDLABEL;
			wdsetctlr(dev, du);

                        /* simulate opening partition 0 so write succeeds */
                        du->dk_openpart |= (1 << 0);            /* XXX */
                        wlab = du->dk_wlabel;
                        du->dk_wlabel = 1;
                        error = writedisklabel(dev, wdstrategy,
				&du->dk_dd, du->dk_dospartitions);
                        du->dk_openpart = du->dk_copenpart | du->dk_bopenpart;
                        du->dk_wlabel = wlab;
                }
                break;

#ifdef notyet
	case DIOCGDINFOP:
		*(struct disklabel **)addr = &(du->dk_dd);
		break;

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
				fop->df_startblk * du->dk_dd.d_secsize;
			error = physio(wdformat, &rwdbuf[unit], dev, B_WRITE,
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

#ifdef	B_FORMAT
int
wdformat(struct buf *bp)
{

	bp->b_flags |= B_FORMAT;
	return (wdstrategy(bp));
}
#endif

int
wdsize(dev_t dev)
{
	int unit = wdunit(dev), part = wdpart(dev), val;
	struct disk *du;

	if (unit > NWD)
		return(-1);

	du = wddrives[unit];
	if (du == 0 || du->dk_state == 0)
		val = wdopen (makewddev(major(dev), unit, WDRAW), FREAD, S_IFBLK, 0);
	if (du == 0 || val != 0 || du->dk_flags & DKFL_WRITEPROT)
		return (-1);

	return((int)du->dk_dd.d_partitions[part].p_size);
}

extern        char *vmmap;            /* poor name! */

int
wddump(dev_t dev)			/* dump core after a system crash */
{
	register struct disk *du;	/* disk unit to do the IO */
	register struct bt_bad *bt_ptr;
	long	num;			/* number of sectors to write */
	int	unit, part, wdc;
	long	blkoff, blknum, blkcnt;
	long	cylin, head, sector, stat;
	long	secpertrk, secpercyl, nblocks, i;
	char	*addr;
	extern	int maxmem;
	static  int wddoingadump = 0 ;
	extern	caddr_t CADDR1;

	addr = (char *) 0;		/* starting address */

	/* toss any characters present prior to dump */
	while (sgetc(1))
		;

	/* size of memory to dump */
	num = maxmem;
	unit = wdunit(dev);		/* eventually support floppies? */
	part = wdpart(dev);		/* file system */
	/* check for acceptable drive number */
	if (unit >= NWD) return(ENXIO);

	du = wddrives[unit];
	if (du == 0) return(ENXIO);
	/* was it ever initialized ? */
	if (du->dk_state < OPEN) return (ENXIO) ;
	if (du->dk_flags & DKFL_WRITEPROT) return(ENXIO);
	wdc = du->dk_port;

	/* Convert to disk sectors */
	num = (u_long) num * NBPG / du->dk_dd.d_secsize;

	/* check if controller active */
	/*if (wdtab.b_active) return(EFAULT); */
	if (wddoingadump) return(EFAULT);

	secpertrk = du->dk_dd.d_nsectors;
	secpercyl = du->dk_dd.d_secpercyl;
	nblocks = du->dk_dd.d_partitions[part].p_size;
	blkoff = du->dk_dd.d_partitions[part].p_offset;

/*pg("xunit %x, nblocks %d, dumplo %d num %d\n", part,nblocks,dumplo,num);*/
	/* check transfer bounds against partition size */
	if (num > nblocks)
		return(EINVAL);

	/*wdtab.b_active = 1;		/* mark controller active for if we
					   panic during the dump */
	wddoingadump = 1;
	if (wdselect(du, unit, 0) < 0)
		return(EIO);
	if (wdcommand(du, WDCC_RESTORE | WD_STEP, 1) < 0)
		return(EIO);

	/* some controllers require this ... */
	wdsetctlr(dev, du);
	
	blknum = blkoff;
	while (num > 0) {
#ifdef notdef
		if (blkcnt > MAXTRANSFER) blkcnt = MAXTRANSFER;
		if ((blknum + blkcnt - 1) / secpercyl != blknum / secpercyl)
			blkcnt = secpercyl - (blknum % secpercyl);
			    /* keep transfer within current cylinder */
#endif
		pmap_enter(kernel_pmap, (vm_offset_t)CADDR1, trunc_page(addr),
			VM_PROT_READ, TRUE, AM_NONE);

		/* compute disk address */
		/* CHS */
		cylin = blknum / secpercyl;
		head = (blknum % secpercyl) / secpertrk;
		sector = blknum % secpertrk;
		/* LBA */
		sector = blknum & 0xff;
		cylin = (blknum >> 8) & 0xffff;
		head = (blknum & 0xF000000) >> 24;

#ifdef notyet
		/* 
		 * See if the current block is in the bad block list.
		 * (If we have one.)
		 */
	    		for (bt_ptr = du->dk_bad.bt_bad;
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
				blknum = (du->dk_dd.d_secperunit)
					- du->dk_dd.d_nsectors
					- (bt_ptr - du->dk_bad.bt_bad) - 1;
				cylin = blknum / secpercyl;
				head = (blknum % secpercyl) / secpertrk;
				sector = blknum % secpertrk;
				break;
			}

#endif

		/* select drive.     */
		if (wdselect(du, unit, head) < 0)
			return (EIO);

		/* transfer some blocks */
		/* __outb(wdc+wd_sector, sector + 1);	/* CHS  - origin 1 */
		__outb(wdc+wd_sector, sector);
		__outb(wdc+wd_seccnt,1);
		__outb(wdc+wd_cyl_lo, cylin);
		__outb(wdc+wd_cyl_hi, cylin >> 8);
#ifdef notdef
		/* lets just talk about this first...*/
		pg ("sdh 0%o sector %d cyl %d addr 0x%x",
			__inb(wdc+wd_sdh), __inb(wdc+wd_sector),
			__inb(wdc+wd_cyl_hi)*256+inb(wdc+wd_cyl_lo), addr) ;
#endif
		if (wdcommand(du, WDCC_WRITE, 1) < 0)
			return(EIO);
		
		outsw (wdc+wd_data, CADDR1+((int)addr&(NBPG-1)), 256);

		if (__inb(wdc+wd_status) & WDCS_ERR) return(EIO) ;
		/* Check data request (should be done).         */
		if (__inb(wdc+wd_status) & WDCS_DRQ) return(EIO) ;

		/* wait for completion */
		for ( i = 1000000 ; __inb(wdc+wd_status) & WDCS_BUSY ; i--) {
				if (i < 0) return (EIO) ;
		}
		/* error check the xfer */
		if (__inb(wdc+wd_status) & WDCS_ERR) return(EIO) ;

		if ((unsigned)addr % (1024*1024) == 0) printf("%d ", num/2048) ;
		/* update block count */
		num--;
		blknum++ ;
		(int) addr += 512;

		/* operator aborting dump? */
		if (sgetc(1))
			return(EINTR);
	}
	return(0);
}


static struct devif wd_devif =
{
	{0}, -1, -1, 0x38, 3, 7, 0, 0, 0,
	wdopen,	wdclose, wdioctl, 0, 0, 0, 0,
	wdstrategy,	0, wddump, wdsize,
};

/*static struct bdevsw wd_bdevsw = 
	{ wdopen,	wdclose,	wdstrategy,	wdioctl,
	  wddump,	wdsize,		NULL };

int spec_config(char **cfg, struct bdevsw *bdp, struct cdevsw *cdp1, struct cdevsw *cdp2);
*/

DRIVER_MODCONFIG() {
	int nctl;
	char *cfg_string = wd_config;
	
#ifdef nope
	/* find configuration string. */
	if (!config_scan(wd_config, &cfg_string)) 
		return;

	/* configure driver into kernel program */
	if (!spec_config(&cfg_string, &wd_bdevsw, (struct cdevsw *) -1, (struct cdevsw *)0))
		return;
#else

	if (devif_config(&cfg_string, &wd_devif) == 0)
		return;
#endif

	/* allocate driver resources for controllers */
	nctl = 1;
	(void)cfg_number(&cfg_string, &nctl);
	if (nctl*2 > NWD)
		printf("too many controllers/disk, recompile with larger NWD\n");
#ifdef notyet
	/*  how to handle arbitrary controllers ? = malloc(vec[2]*?); */

	/* how to scale dkn statistics structures */

	/* root device needs to be configured to work, should others be postponed till used? hotswap? */
	if ( ... != bdev.bd_major)
#endif

	/* probe for hardware */
	new_isa_configure(&cfg_string, &wddriver);
}
