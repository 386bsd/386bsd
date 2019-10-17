/*
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Don Ahn.
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
 *	@(#)fd.c	7.4 (Berkeley) 5/25/91
 */

/* base driver adapted to use controller core driver code -wfj */

#include "sys/param.h"
#include "sys/errno.h"
#include "sys/file.h"
#include "sys/ioctl.h"
#include "buf.h"
#include "uio.h"
#include "isa_driver.h"
#include "nec765.h"
#include "fdreg.h"
#include "isa_irq.h"
#include "machine/icu.h"
#include "rtc.h"
#include "modconfig.h"
#include "prototypes.h"
#include "machine/inline/io.h"

#ifdef FDTEST
int fdtest = 1;
#endif

struct fd_type fd_types[] = {
 	{ 18,2,0xFF,0x1B,80,2880,1,0 },	/* 1.44 meg HD 3.5in floppy    */
	{ 15,2,0xFF,0x1B,80,2400,1,0 },	/* 1.2 meg HD 5.25in floppy    */
	{ 9,2,0xFF,0x20,80,1440,1,2  }, /* 720k floppy in HD drive     */
	{ 9,2,0xFF,0x23,40,720,2,1 },	/* 360k floppy in HD drive     */
	{ 9,2,0xFF,0x2A,40,720,1,1 },	/* 360k floppy in DD drive     */
};

/* drives - have state (heads, cylinders, transfers, seeks, buffers, on queues, errors, statistics */
struct fd_drive fd_dev_tab[2]; 		/* two drives per AT controller (NEC 765 has "room" for 4!) */
struct buf fdutab[2];

/* controllers - have queues of transfers for drives, commands, status/interrupts/dma/other */
#include "fdc.h"
extern struct fdc_stuff fdC[];
extern struct buf fdctab;

/* from kernel */
extern int hz;

/* function prototypes */
int fdustart(struct fd_drive *fdp);

int fdprobe(struct isa_device *);
void fdattach(struct isa_device *), fdintr(int);
int fd_turnoff(int);
void fd_turnon(int);
void nextstate(struct buf *dp);

static int lastunit = 0;
/*
 * probe for existance of controller
 */
int
fdprobe(struct isa_device *dev)
{
	int cyl, unit = dev->id_unit;
	struct fdc_stuff *fdcp;
	if (unit == '?')
		dev->id_unit = unit = lastunit++;
	fdC[unit].fdc_port = dev->id_iobase;
	fdcp = fdC + unit;
	fdcp->fdc_current_drive = fd_dev_tab + unit;
/*printf("\n unit %d status %x", unit, fdcinb(fdcp->fdc_port+fdsts));
printf(" motor ");*/
	set_motor(0,1);
	DELAY(10000);
	set_motor(unit,0);

DELAY(10000);
/*printf("\n Status %x", fdcinb(fdcp->fdc_port+fdsts));*/
		/* BIG SURPRISE! this is *manditory*. ST0 US sequences 0 1 2 3 all by itself! all are aborts!)
	{ int i; for (i = 0; i <4; i++) {
	fdcp->fdc_current_drive->fd_cmd[0] = NE7CMD_SENSEI;
	(void)fdc_to_from (fdcp, 1);
	if (fdcp->fdcs_status[FDCS_ST0] != 0xc0 + i)
		printf("%d. St0 %x ", i, fdcp->fdcs_status[FDCS_ST0]);
	}}
/*print_st0(fdcp, fdcp->fdcs_status[FDCS_ST0], unit, 0);*/
/*printf(" sTatus %x\n", fdcinb(fdcp->fdc_port+fdsts));*/
	/* Set transfer to 500kbps */
	fdcoutb(fdcp->fdc_port+fdctl,0);


	/* see if it can handle an SPECIFY (intialize parameters) command */
	fdcp->fdc_current_drive->fd_cmd[0] = NE7CMD_SPECIFY;
	fdcp->fdc_current_drive->fd_cmd[1] = 0xdf;
	fdcp->fdc_current_drive->fd_cmd[2] = 2;
	if (fdc_to_from (fdcp, 3) != 1)
		return 0;

	DELAY(10000);
	/* printf("\n SPECIFY statuS %x", fdcinb(fdcp->fdc_port+fdsts)); */

#ifdef OMIT
	/* find interrupt sense (if any) */
	fdcp->fdc_current_drive->fd_cmd[0] = NE7CMD_SENSEI;
	if (fdc_to_from (fdcp, 1) != 1)
		return 0;
	printf("St0 %x ", fdcp->fdcs_status[FDCS_ST0]);
	print_st0(fdcp, fdcp->fdcs_status[FDCS_ST0], unit, 0);
	printf(" stATus %x", fdcinb(fdcp->fdc_port+fdsts));

#ifdef FDTEST
	printf(" statUs %x", fdcinb(fdcp->fdc_port+fdsts));
fdcp->fdc_current_drive->fd_cmd[0] = NE7CMD_SENSED;
fdcp->fdc_current_drive->fd_cmd[1] = 0;
(void)fdc_to_from (fdcp, 2);
	printf(" status %x", fdcinb(fdcp->fdc_port+fdsts));
printf("st3 %x ", fdcp->fdc_st3);
#endif
#endif
printf("\n\n");
	return 1;
}

/*
 * wire controller into system, look for floppy units
 */
void
fdattach(struct isa_device *dev)
{
	int i, hdr;
	unsigned fdt, cyl; struct fdc_stuff *fdcp; 

	fdcp = fdC+0;
/* array of controller pointers to controller structures, each with an two or four unit array of drives */
	fdcp->fdc_dmachan = dev->id_drq;

	fdt = rtcin(RTC_FDISKETTE);
	hdr = 0;

	/* check for each floppy drive */
	for (i = 0; i < 1 /* 2 */; i++) {
		/* BIOS - is there a unit? */
		/*if ((fdt & 0xf0) == RTCFDT_NONE)
			continue;*/

		/* have controller select it and attempt to move head to track zero */
		fd_turnon(i);
#ifdef OMIT
		DELAY(10000);
fdcp->fdc_current_drive->fd_cmd[0] = NE7CMD_SEEK;
fdcp->fdc_current_drive->fd_cmd[1] = 0;
fdcp->fdc_current_drive->fd_cmd[2] = 0;
(void)fdc_to_from (fdcp, 3);
fdcp->fdc_current_drive->fd_cmd[0] = NE7CMD_SENSEI;
(void)fdc_to_from (fdcp, 1);
#endif
		DELAY(10000);
		fdcp->fdc_current_drive->fd_cmd[0] = NE7CMD_SENSED;
		fdcp->fdc_current_drive->fd_cmd[1] = i;
		(void)fdc_to_from (fdcp, 2);
printf("st3 %x ", fdcp->fdc_st3);
		if((fdcp->fdc_st3 & (NE7_ESIG | NE7_RDY | NE7_TRK0)) == (NE7_RDY | NE7_TRK0))
			goto foo;
/*if(fdcp->fdcs_status[FDCS_CYL]  != 0) {*/
		printf(" rEcal ");
		fdcp->fdc_current_drive->fd_cmd[0] = NE7CMD_RECAL;
		fdcp->fdc_current_drive->fd_cmd[1] = i;
		if (fdc_to_from (fdcp, 2) != 1)
			continue;
/*} */

	printf("msr %x ", fdcinb(fdcp->fdc_port+fdsts));
		DELAY(10000);
	printf("msr %x ", fdcinb(fdcp->fdc_port+fdsts));

		/* have controller sense if anything responding with appropriate sense */
		fdcp->fdc_current_drive->fd_cmd[0] = NE7CMD_SENSEI;
		if (fdc_to_from (fdcp, 1) != 1)
			continue;
/* reports unit 1 active?? */
		printf("cYl %d ST0 %x ", fdcp->fdcs_status[FDCS_CYL], fdcp->fdcs_status[FDCS_ST0]);
		print_st0(fdcp, fdcp->fdcs_status[FDCS_ST0], i, 0);
#ifdef OMIT
/* not working right. Reporting no seek end 0x10. logic should be: !(FD_ST0_COMMAND_FINISHED(st0) && FD_ST0_SEEK_DID(st0) && fdcp->fdcs_status[FDCS_CYL] == 0) */
		if (!FD_ST0_COMMAND_FINISHED(fdcp->fdcs_status[FDCS_ST0]) || FD_ST0_SEEK_DIDNT(fdcp->fdcs_status[FDCS_ST0]) || fdcp->fdcs_status[FDCS_CYL] != 0)
			continue;
#endif

	printf(" status %x ", fdcinb(fdcp->fdc_port+fdsts));

		/* have controller sense the drive itself */
		fdcp->fdc_current_drive->fd_cmd[0] = NE7CMD_SENSED;
		fdcp->fdc_current_drive->fd_cmd[1] = i;
		if (fdc_to_from (fdcp, 2) != 1 || (fdcp->fdc_st3 & 0xb8) != 0x38) {
/* printf("st3 %x! ", fdcp->fdc_st3);
			continue;*/
		}
printf("st3 %x ", fdcp->fdc_st3);
foo:
		/* yes, announce it */
		if (hdr)
			printf(", ");
		printf("fd%d: ", i);

		/* BIOS - assign likely "type" */
		if ((fdt & 0xf0) == RTCFDT_12M) {
			printf("1.2M");
 			fd_dev_tab[i].fd_type = 1;
		}
		if ((fdt & 0xf0) == RTCFDT_144M) {
			printf("1.44M");
 			fd_dev_tab[i].fd_type = 0;
		}

		fdt <<= 4;
		hdr = 1;

	}
printf("\n status %x", fdcinb(fdcp->fdc_port+fdsts));
#ifdef old
printf(" motor ");
	set_motor(0,1);
	DELAY(10000);
	set_motor(unit,0);

	/* see if it can handle a command */
	if (out_fdc(last_cmd = NE7CMD_SPECIFY) < 0) {
printf(" no spec ");
	}
	out_fdc(0xDF);
	out_fdc(2);
DELAY(10000);
printf("\n status %x", fdcinb(fdcp->fdc_port+fdsts));
printf(" ists? ");
		out_fdc(NE7CMD_SENSEI);
		st0 = in_fdc();
		cyl = in_fdc();
printf("ST0 %b ", st0, NE7_ST0BITS);
print_st0(fdcp, st0, unit, 0);
		/* sense the drive */
		out_fdc(NE7CMD_SENSED);
		out_fdc(0);
		st3 = in_fdc();
		printf(" ST3 %x ", st3);

		/* select it */
		DELAY(10000);
		out_fdc(last_cmd = NE7CMD_RECAL);	/* Recalibrate Function */
		out_fdc(0);
		DELAY(10000);
		printf(" recal ");

		/* anything responding */
		out_fdc(NE7CMD_SENSEI);
		st0 = in_fdc();
		cyl = in_fdc();
		printf("ST0 %b ", st0, NE7_ST0BITS);
		print_st0(fdcp, st0, 0, 0);
printf(" cyl %d ", cyl);

		/* sense the drive */
		out_fdc(NE7CMD_SENSED);
		out_fdc(0);
		st3 = in_fdc();
		printf(" ST3 %x ", st3);
#endif
	fdcp->fdc_current_drive->fd_cmd[0] = 0;
	/* Set transfer to 500kbps */
	fdcoutb(fdcp->fdc_port+fdctl,0);
	timeout(fd_turnoff, (caddr_t)0,10*hz);
}

int
fdsize(dev_t dev)
{
	return(0);
}

/****************************************************************************/
/*                               fdstrategy                                 */
/****************************************************************************/
fdstrategy(struct buf *bp)
{
	register struct buf *dp,*dp0,*dp1;
	long nblocks,blknum;
 	int	unit, type, s;
	struct fdc_stuff *fdcp; 

 	unit = FDUNIT(minor(bp->b_dev));
 	type = fd_dev_tab[unit].fd_type;
if (type != 0) printf("type changed");
 	type = fd_dev_tab[unit].fd_type = 0;

	fdcp = fdC+0;
fdcp->fdc_current_drive = fd_dev_tab + unit;
#ifdef FDTEST
if(fdtest) printf("fdstrat%d, blk = %d, bcount = %d, addr = %x type %d|",
	unit, bp->b_blkno, bp->b_bcount,bp->b_un.b_addr, type);
#endif
	if ((unit >= 2) || (bp->b_blkno < 0)) {
		printf("fdstrat: unit = %d, blkno = %d, bcount = %d\n",
			unit, bp->b_blkno, bp->b_bcount);
		pg("fd:error in fdstrategy");
		bp->b_error = EINVAL;
		bp->b_flags |= B_ERROR;
		goto bad;
	}
	/*
	 * Set up block calculations.
	 */
	blknum = (unsigned long) bp->b_blkno * DEV_BSIZE / FDBLK;
 	nblocks = fd_types[type].size;
	if (blknum + (bp->b_bcount / FDBLK) > nblocks) {
		if (blknum == nblocks) {
			bp->b_resid = bp->b_bcount;
		} else {
			bp->b_error = ENOSPC;
			bp->b_flags |= B_ERROR;
		}
		goto bad;
	}
 	bp->b_cylin = blknum / (fd_types[type].sectrac * 2);
	dp = &fdutab[unit];

        /* queue transfer on drive, activate drive and controller if idle */
        s = splbio();
        disksort(dp, bp);

	/* if drive is active start controller otherwise wait for drive's motor to start */
        if (dp->b_active == 0 && fdustart(&fd_dev_tab[unit])) {
		/* if controller inactive, start controller */
        	if (fdctab.b_active == 0)
                	fdcstart();
	}
	splx(s);
	return;

bad:
	biodone(bp);
}

/****************************************************************************/
/*                            motor control stuff                           */
/****************************************************************************/
set_motor(unit, reset)
int unit,reset;
{
	int m0,m1;
	struct fdc_stuff *fdcp; 
	fdcp = fdC+0;
	m0 = fd_dev_tab[0].motor;
	m1 = fd_dev_tab[1].motor;

	if (reset) fdC[unit].fdc_interrupt_flags  |= FDC_FLAG_INTR_RESET;
	fdcoutb(fdcp->fdc_port+fdout, (unit&FDO_FDSEL)
		| (reset ? 0 : (FDO_FRST|FDO_FDMAEN))
		| (m0 ? FDO_MOEN0 : 0)
		| (m1 ? FDO_MOEN1 : 0));
}

int
fd_turnoff(int unit)
{
int x;

#ifdef FDC_DIAGNOSTICS
if(fd_dev_tab[unit].fd_cmd[0]) printf("fd_turnoff: cmd %x ", fd_dev_tab[unit].fd_cmd[0]);
if(fd_dev_tab[unit].fd_cmd[0]) {
	x = splbio();
	fdcintr(0);
	splx(x);
}
#endif
	fd_dev_tab[unit].motor = 0;
	if (unit) set_motor(0, 0);
	else set_motor(1, 0);
}

void
fd_turnon(int unit)
{
	fd_dev_tab[unit].motor = 1;
	set_motor(unit,0);
}

void
fd_resched_turnoff(caddr_t arg)
{
	untimeout(fd_turnoff, arg);
	timeout(fd_turnoff, arg, 10*hz);
}


int fdopenf;
/****************************************************************************/
/*                           fdopen/fdclose                                 */
/****************************************************************************/
int
fdopen(dev_t dev, int flags, int fmt, struct proc *p)
{
 	int unit = FDUNIT(minor(dev));
 	int type;
	int s;

/* work through state machine to insure controller has been reset, initialized, there is a drive and the drive functions */
/* if media change bit on open, clear. if media changes, force all transactions as errors until close */
/* if drives says write protect and open for writing, deny open */
	fdopenf = 1;
	/* check bounds */
	if (unit >= 2) return(ENXIO);

	/* Set proper disk type, only allow one type */
        type = FDTYPE(unit, minor(dev));
	fd_dev_tab[unit].fd_type = type;
	fd_dev_tab[unit].fd_unit = unit;
printf("fdopen: dev %x unit %d type %d ", dev, unit, type);

	return 0;
}

int
fdclose(dev_t dev, int flags, int fmt, struct proc *p)
{
	fdopenf = 0;
	return(0);
}

/*
 * Routine to queue a command to the controller.  The unit's
 * request is linked into the active list for the controller.
 * If the controller is idle, the transfer is started.
 * If the media isn't rotating, it's started and a delayed callback
 * initiates the transfer instead.
 */
int
fdustart(register struct fd_drive *fdp)
{
	register struct buf *bp, *dp = &fdutab[fdp->fd_unit];

	/* unit already active? */
	if (dp->b_active)
		return 0;

	/* motor not running? */
        if (!fdp->motor) {
                fd_turnon(fdp->fd_unit);
		fdp->fd_flags |= FD_FLAG_SPINNINGUP;

		/* Wait for 0.1 sec - 300RPM drive,  a half rotation */
		timeout(fdustart, (caddr_t)fdp, hz/10);
		/*return 0;*/
	} else {
		fdp->fd_flags &= ~FD_FLAG_SPINNINGUP;
        	if (fdctab.b_active == 0)
                	fdcstart();
	}

	/* anything to start? */
	bp = dp->b_actf;
	if (bp == NULL) 
		return 0;	

	/* link onto controller queue */
	dp->b_forw = NULL;
	if (fdctab.b_actf  == NULL)
		fdctab.b_actf = dp;
	else
		fdctab.b_actl->b_forw = dp;
	fdctab.b_actl = dp;

	/* mark the drive unit as busy */
	dp->b_active = 1;
	return 1;
}

badtrans(dp,bp)
struct buf *dp,*bp;
{
	struct fdc_stuff *fdcp; 
	fdcp = fdC+0;

	bp->b_flags |= B_ERROR;
	bp->b_error = EIO;
	bp->b_resid = bp->b_bcount - fdcp->fdc_skip;
	dp->b_actf = bp->av_forw;
	fdcp->fdc_skip = 0;
	biodone(bp);
	nextstate(dp);

}

/*
	nextstate : After a transfer is done, continue processing
	requests on the current drive queue.  If empty, go to
	the other drives queue.  If that is empty too, timeout
	to turn off the current drive in 5 seconds, and go
	to state 0 (not expecting any interrupts).
*/

void
nextstate(struct buf *dp)
{
	if (!dp->b_actf) {
	struct fdc_stuff *fdcp; 
	fdcp = fdC+0;
		fdcp->fdc_current_drive->fd_cmd[0] = 0;
		fd_resched_turnoff((caddr_t)0 /*FDUNIT(minor(dp->b_actf->b_dev))*/);
		dp->b_active = 0;
	}
}

int
fdioctl(dev_t dev, int cmd, caddr_t addr, int flag, struct proc *p)
{
	return(ENODEV);
}

int
fddump(dev_t dev)
{
	return(ENODEV);
}

