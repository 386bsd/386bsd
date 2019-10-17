/*~
 * Copyright (c) 1995, 2001, 2005 William Jolitz
 * All rights reserved.
 *
 * This code is derived from software contributed to The Regents of the
 * University of California by William Jolitz in 1990.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Intent to obscure or deny the origins of this code in unattributed
 *    derivations is sufficient cause to breach the above conditions.
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
 */

/* floppy disk controller core driver for NEC uPD 765 (and derivatives) */

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
#include "fdc.h"
#include "nec765.h"

#ifdef	FDTEST
extern int fdtest;
#endif

/* standard default ISA/AT configuration: */
static char *fdc_config =
	"fd	2 9 1 (0x3f0 6 2 0 0 0).	# floppy diskette controller ";

#define NFDC 1
struct fdc_stuff fdC[NFDC];	/* controller state */
struct buf fdctab;	/* controller transfer queue */

extern struct buf fdutab[];	/* drive transfer queues */
extern struct fd_type fd_types[];	/* drive types */
extern int fdopenf;

int fdprobe(struct isa_device *);
void fdattach(struct isa_device *), fdcintr(int);

struct	isa_driver fdcdriver = {
	fdprobe, fdattach, fdcintr, "fd", &biomask
};

/* default per device */
static struct isa_device fdc_default_devices[] = {
	{ &fdcdriver,    0x3f0,  IRQ6,  2,  0x00000,     0,  0 },
	{ 0 }
};

int fdopen(dev_t, int, int, struct proc *);
int fdclose(dev_t, int, int, struct proc *);
int fdstrategy(struct buf *);
int fdioctl(dev_t, int, caddr_t, int, struct proc *);
int fd_turnoff(int);
void fd_turnon(int);
void nextstate(struct buf *dp);


/* top level, called from base driver to work controller */

void
fdcstart()
{
	register struct buf *dp,*bp;
	int s, cmd_len;
	struct fdc_stuff *fdcp; 
	fdcp = fdC+0;
loop:
	/* is there a drive for the controller to do a transfer with? */
	dp = fdctab.b_actf;
	if (dp == NULL)
		return;

	/* is there a transfer to this drive ? if so, link it on
	   the controller's queue */
	bp = dp->b_actf;
	if (bp == NULL) {
		fdctab.b_actf = dp->b_forw;
		goto loop;
	}

	fdctab.b_active = 1;

	s = splbio();
	fdcp->fdc_skip = 0;
{
	int read,head,trac,sec,i,s,sectrac;
	unsigned long blknum;
	struct fd_type *ft;
struct fdc_stuff *fdcp = fdC+0;
 		ft = &fd_types[fdcp->fdc_current_drive->fd_type];
		if (bp->b_cylin != fdcp->fdcs_status[FDCS_CYL]) {
#ifdef FDDEBUG
if(fdtest)printf("Seek from %d to %d \n", fdcp->fdcs_status[FDCS_CYL], bp->b_cylin);
#endif
		/* Seek necessary, never quite sure where head is at! */
		fdcp->fdc_current_drive->fd_cmd[0] = NE7CMD_SEEK;
		cmd_len = 3;
		} else {
reissue:
		isa_dmastart(bp->b_flags, bp->b_un.b_addr+fdcp->fdc_skip,
			FDBLK, fdcp->fdc_dmachan);
		read = bp->b_flags & B_READ;
		fdcp->fdc_current_drive->fd_cmd[0] = read ? NE7CMD_READ : NE7CMD_WRITE;
		cmd_len = 9;
		}
		blknum = (unsigned long)bp->b_blkno*DEV_BSIZE/FDBLK
			+ fdcp->fdc_skip/FDBLK;
		sectrac = ft->sectrac;
		sec = blknum %  (sectrac * 2);
		head = sec / sectrac;
		sec = sec % sectrac;

		fdcp->fdc_interrupt_flags |= FDC_FLAG_INTR_SEEK;
		fdcp->fdc_current_drive->fd_cmd[1] = head << 2 | fdcp->fdc_current_drive->fd_unit;	/* head & drive */
		fdcp->fdc_current_drive->fd_cmd[2] = bp->b_cylin;		/* track */
		if (cmd_len != 3) {
#ifdef FDTEST
if(fdtest)printf("Iocmd %d/%d/%d ", bp->b_cylin, head, sec);
#endif
		fdcp->fdc_current_drive->fd_cmd[3] = head;
		fdcp->fdc_current_drive->fd_cmd[4] = sec+1;
		fdcp->fdc_current_drive->fd_cmd[5] = ft->secsize;
		fdcp->fdc_current_drive->fd_cmd[6] = sectrac;
		fdcp->fdc_current_drive->fd_cmd[7] = ft->gap;
		fdcp->fdc_current_drive->fd_cmd[8] = ft->datalen;

		/* if last sector of track (regardless head) flag for consecutive transfer */
		if (sectrac == sec+1)
			fdcp->fdc_interrupt_flags |= FDC_FLAG_INTR_ENDSECTOR;
		else
			fdcp->fdc_interrupt_flags &= ~FDC_FLAG_INTR_ENDSECTOR;
		}

		if (fdc_to_from (fdcp, cmd_len) != 1)
			pg("start transferseek failed");
		else if (cmd_len == 3 && fdcinb(fdcp->fdc_port+fdsts) == 0x80) {
			fdcp->fdcs_status[FDCS_CYL] = bp->b_cylin;
			goto reissue;
		}
		
		fdcp->fdc_interrupt_flags &= ~FDC_FLAG_INTR_SEEK;
	}
	splx(s);
}
/* bottom level, called from interrupt entry point managed by bus */

void
fdcintr(controller)
{
	register struct buf *dp,*bp;
	struct buf *dpother;
	int rv, read,sec,i,s,sectrac;
	int head, cyl;
	unsigned long blknum;
	struct fd_type *ft;
	struct fdc_stuff *fdcp = fdC+controller;
unsigned short sum;
	int cmd_len;

	if (!fdctab.b_active) {
		if (fdcp->fdc_interrupt_flags  & FDC_FLAG_INTR_RESET) {
			/* four drives, four calls */
			printf("fdc%d: controller reset interrupt ", controller);
			fdcp->fdc_current_drive->fd_cmd[0] = NE7CMD_SENSEI;
			for (i = 0; i < 4 ; i++) {
				(void)fdc_to_from (fdcp, 1);
				printf("St0 %x ", fdcp->fdcs_status[FDCS_ST0]);
				print_st0(fdcp, fdcp->fdcs_status[FDCS_ST0], i, 0);
			}
			fdcp->fdc_interrupt_flags  &= ~FDC_FLAG_INTR_RESET;
		} else
			printf("fd: spurious interrupt\n");
		return;
	}
			fdcp->fdc_interrupt_flags  &= ~FDC_FLAG_INTR_RESET;
 	ft = &fd_types[fdcp->fdc_current_drive->fd_type];
#ifdef FDTEST
if(fdtest)printf("\n intr - skip %d, typ %d sts %x|",fdcp->fdc_skip,fdcp->fdc_current_drive->fd_type, fdcinb(fdcp->fdc_port+fdsts));
DELAY(100000);
#endif

#ifdef notyet
	if software interrupt, hardware interrupt, nested
	   just hw check status, if aborted command
#endif

#ifdef FDC_DEBUG
	/* what do we do if the drive is in motion? just report it now */
	if (fdcinb(fdcp->fdc_port+fdsts) &0x80) {
		if (fdcinb(fdcp->fdc_port+fdsts) & 0xf && fdtest)
/*  its drive presence - when you select the drive, the active bit is a "bounce" to tell that something is there */
/* in fdattach?? perhaps in sense to see if drive is unplugged? */
			printf(" dim%x ", fdcp->fdc_current_drive->fd_cmd[0]);
	}
#endif
	rv = fdc_sense(fdcp);
	if (rv == 0) return;

	/* if drive isn't open, we're done! */
	if (!fdopenf) return;

	/* get the transfer off the controller's queue */
	dp = &fdutab[fdcp->fdc_current_drive->fd_unit];
	bp = dp->b_actf;

 	/*ft = &fd_types[fdcp->fdc_current_drive->fd_type];*/

#ifdef FDTEST
printf("%d/%d/%d ST0 %x ", fdcp->fdcs_status[FDCS_CYL], fdcp->fdcs_status[FDCS_HEAD], fdcp->fdcs_status[FDCS_SECTOR]-1, fdcp->fdcs_status[FDCS_ST0]);
print_st0(fdcp, fdcp->fdcs_status[FDCS_ST0], i, fdcp->fdcs_status[FDCS_HEAD]);
#endif
	/* seek/recal */
	if (rv == 1) {
		if (fdcp->fdcs_status[FDCS_CYL] != bp->b_cylin)
			printf("fd%d: Seek to cyl %d failed; am at cyl %d\n",
			fdcp->fdc_current_drive->fd_unit,
			bp->b_cylin, fdcp->fdcs_status[FDCS_CYL]);
	}
	/* xfer */
	if (rv == 2) {
		/* error? */
		if (FD_ST0_COMMAND_DIDNT_FINISH(fdcp->fdcs_status[FDCS_ST0])) {
printf("ST1 %x %x\n", fdcp->fdcs_status[1], fdcp->fdcs_status[2]);
			isa_dmadone(bp->b_flags, bp->b_un.b_addr+fdcp->fdc_skip,
				FDBLK, fdcp->fdc_dmachan);
			badtrans(dp, bp);
			fdctab.b_active = 0;
			return;
		}
		/*if (fdcp->fdcs_status[0]&0xd8) */
		if (FD_ST0_SEEK_DIDNT(fdcp->fdcs_status[FDCS_ST0])) {
pg("iocmd status0 err %d:",fdcp->fdcs_status[FDCS_ST0]);
			/*goto retry;*/
		}
		
		/* sucessful transfer "moves" reported cylinder ahead not head */
		fdcp->fdcs_status[FDCS_CYL] = bp->b_cylin;
#ifdef FDTEST
sum = 0;
{ unsigned char *cp = (unsigned char *) bp->b_un.b_addr + fdcp->fdc_skip; int cnt = FDBLK;
do { 
	sum += *cp++;
} while (--cnt);
}
printf("%x: %x ", (unsigned char *) bp->b_un.b_addr + fdcp->fdc_skip, sum);
#endif
		/* All OK */
		isa_dmadone(bp->b_flags, bp->b_un.b_addr+fdcp->fdc_skip,
			FDBLK, fdcp->fdc_dmachan);
		fdcp->fdc_skip += FDBLK;
		fd_resched_turnoff((caddr_t) FDUNIT(minor(dp->b_actf->b_dev)));
		if (fdcp->fdc_skip >= bp->b_bcount) {
#ifdef FDTEST
printf("DONE %d ", bp->b_blkno);
#endif
			/* ALL DONE */
			fdcp->fdc_skip = 0;
			bp->b_resid = 0;
			dp->b_actf = bp->av_forw;
			biodone(bp);
			nextstate(dp);
      /* controller idle */
        fdctab.b_active = 0;

        /* anything more on drive queue? */
        if (dp->b_actf)
                (void)fdustart(fdcp->fdc_current_drive);
        /* anything more for controller to do? */
        if (fdctab.b_actf)
                fdcstart();
	return;
		} else {
#ifdef FDTEST
printf("n| ");
#endif
			/* set up next transfer */
			blknum = (unsigned long)bp->b_blkno*DEV_BSIZE/FDBLK
				+ fdcp->fdc_skip/FDBLK;
			bp->b_cylin = (blknum / (ft->sectrac * 2));
		}

	}

#ifdef OMIT
	/* complain if we're not in the right place already - perhaps because transfer wrapped across boundary? */
	if (fdcp->fdcs_status[FDCS_CYL] != bp->b_cylin)
		printf("fdc on cyl %d NOT on transfer's cylinder %d! ", fdcp->fdcs_status[FDCS_CYL],  bp->b_cylin);
#endif

	read = bp->b_flags & B_READ;

	blknum = (unsigned long)bp->b_blkno*DEV_BSIZE/FDBLK + fdcp->fdc_skip/FDBLK;
 	cyl = blknum / (ft->sectrac * 2);
	sectrac = ft->sectrac;
	sec = blknum %  (sectrac * 2);
	head = sec / sectrac;
	sec = sec % sectrac;


	/* is head positioned for transfer? or must we also force a seek because transfer's implied seek won't work */
	if (
		(fdcp->fdc_interrupt_flags &
			(FDC_FLAG_INTR_ENDSECTOR|FDC_FLAG_INTR_CYLNXT)
		) == (FDC_FLAG_INTR_ENDSECTOR|FDC_FLAG_INTR_CYLNXT)
		||
		fdcp->fdcs_status[FDCS_CYL] != cyl) {
#ifdef OMIT
		if (fdcp->fdcs_status[FDCS_CYL] != cyl)
			printf("fd%d: Seek to cyl %d failed; am at cyl %d\n",
			fdcp->fdc_current_drive->fd_unit,
			bp->b_cylin, fdcp->fdcs_status[FDCS_CYL]);

#ifdef FDC_DIAGNOSTIC
		/*if (
		(fdcp->fdc_interrupt_flags &
			(FDC_FLAG_INTR_ENDSECTOR|FDC_FLAG_INTR_CYLNXT)
		) == (FDC_FLAG_INTR_ENDSECTOR|FDC_FLAG_INTR_CYLNXT)) */
		else
			printf ("forced seek ");
#endif
#endif
		/* clear forced seek to bypass implied seek issue */
		fdcp->fdc_interrupt_flags &= ~(FDC_FLAG_INTR_ENDSECTOR|FDC_FLAG_INTR_CYLNXT);

		/* beginning of fdc_transfer_seek() */
		/* Seek necessary with interrupt */
		fdcp->fdc_current_drive->fd_cmd[0] = NE7CMD_SEEK;
		cmd_len = 3;
	} else {
reissue:
		isa_dmastart(bp->b_flags, bp->b_un.b_addr+fdcp->fdc_skip,
			FDBLK, fdcp->fdc_dmachan);
#ifdef FDTEST
if(fdtest)printf("iocmd %d/%d/%d ", bp->b_cylin, head, sec);
#endif
		fdcp->fdc_current_drive->fd_cmd[0] = read ? NE7CMD_READ : NE7CMD_WRITE;
		cmd_len = 9;
	}
	fdcp->fdc_interrupt_flags |= FDC_FLAG_INTR_SEEK;
	fdcp->fdc_current_drive->fd_cmd[1] = head << 2 | fdcp->fdc_current_drive->fd_unit;	/* head & drive */
	fdcp->fdc_current_drive->fd_cmd[2] = bp->b_cylin;		/* track */
	if (cmd_len != 3) {
		fdcp->fdc_current_drive->fd_cmd[3] = head;
		fdcp->fdc_current_drive->fd_cmd[4] = sec+1;
		fdcp->fdc_current_drive->fd_cmd[5] = ft->secsize;
		fdcp->fdc_current_drive->fd_cmd[6] = sectrac;
		fdcp->fdc_current_drive->fd_cmd[7] = ft->gap;
		fdcp->fdc_current_drive->fd_cmd[8] = ft->datalen;

		/* if last sector of track (regardless head) flag for consecutive transfer */
		if (sectrac == sec+1)
			fdcp->fdc_interrupt_flags |= FDC_FLAG_INTR_ENDSECTOR;
		else
			fdcp->fdc_interrupt_flags &= ~FDC_FLAG_INTR_ENDSECTOR;
	}
	if (fdc_to_from (fdcp, cmd_len) != 1)
		pg("transferseek failed");
	else if (cmd_len == 3 && fdcinb(fdcp->fdc_port+fdsts) == 0x80) {
		fdcp->fdcs_status[FDCS_CYL] = cyl;
		goto reissue;
	}
	fdcp->fdc_interrupt_flags &= ~FDC_FLAG_INTR_SEEK;
	/* end of fdc_transfer_seek */
}

/* sense diagnostic */
void
print_st0(struct fdc_stuff *fdcp, int st0, int drv, int hd) {

	printf ("cmd %x: ", fdcp->fdc_current_drive->fd_cmd[0]);
	if (FD_ST0_COMMAND_FINISHED(st0))
		printf("fini ");
	if (FD_ST0_COMMAND_DIDNT_FINISH(st0))
		printf("didnt ");
	if (FD_ST0_INVALID_COMMAND(st0))
		printf("inv ");
	if (FD_ST0_COMMAND_DRIVE_ABORT(st0))
		printf("abt ");
	if (FD_ST0_SEEK_DIDNT(st0))
		printf("noseek0 ");
	else {
		if (FD_ST0_SEEK_DID(st0))
			printf("seeked ");
	}
	if (!FD_ST0_DRIVE_READY(st0))
		printf("drvnrdy ");
	if (!FD_ST0_THIS_DRIVE(st0, drv, hd)) {
		if (((st0 >>2)&1) != hd) printf("hd%d not%d ", ((st0 >>2)&1), hd);
		if ((st0 & 3) != drv) printf("drv%d not%d ", (st0 & 3), drv);
	}
}

/* controller has indicated sense should be ready, extract and evaluate it */
/* rv  = 0 return, 1 = did seek/recal, 2 = did i/o */
int
fdc_sense(struct fdc_stuff *fdcp)
{	int rv = 0;

#ifdef	FDC_DIAGNOSTICSx
	/* if there was a command, we'd better have had drive  active */
	if (fdcp->fdc_current_drive->fd_cmd[0] ...)
		printf("drive's not active? ");
#endif

	if (fdcp->fdc_current_drive->fd_cmd[0] == NE7CMD_SEEK || fdcp->fdc_current_drive->fd_cmd[0] == NE7CMD_RECAL) {

		fdcp->fdc_current_drive->fd_cmd[0] = NE7CMD_SENSEI;
		if (fdc_to_from (fdcp, 1) != 1)
			pg("sensei failed in fdcintr?");

#ifdef	FDC_DIAGNOSTICS
		if (!(fdcp->fdcs_status[FDCS_ST0] & 0x20))
			printf("no SE! ");
#endif
/* interrupt but status has already been extracted, ignore */
if(fdcinb(fdcp->fdc_port+fdsts) & 0x80)
#ifdef	FDC_DIAGNOSTICS
/* head in ST0 always 0? */
	/*if(fdtest)*/print_st0(fdcp, fdcp->fdcs_status[FDCS_ST0], fdcp->fdc_current_drive->fd_unit, 0 /*head*/);
	/*if(fdtest)*/printf("Cyl %d| ", fdcp->fdcs_status[FDCS_CYL]);
#endif
		rv = 1;
	}

	/* extract transfer sense that's already present */
	if (fdcp->fdc_current_drive->fd_cmd[0] == NE7CMD_READ || fdcp->fdc_current_drive->fd_cmd[0] == NE7CMD_WRITE) {
		int oldcyl = fdcp->fdcs_status[FDCS_CYL], oldhead = fdcp->fdcs_status[FDCS_HEAD], oldsector = fdcp->fdcs_status[FDCS_SECTOR]-1;
		/* MRQ|DIO|BUSY and drive active all must be ready at this point */
		if (fdc_to_from (fdcp, 0) != 1)
			pg("transfer sense failed in fdcintr?");
		/* some controllers don't switch heads while incrementing to next track, workaround */
		if (oldcyl + 1 == fdcp->fdcs_status[FDCS_CYL])
			fdcp->fdc_interrupt_flags |= FDC_FLAG_INTR_CYLNXT;
		else
			fdcp->fdc_interrupt_flags &= ~FDC_FLAG_INTR_CYLNXT;
#ifdef	FDC_DIAGNOSTICSx
		printf(" iocmd sts ");
{ int i;
		for(i=0;i<7;i++)
			 printf("%x ", fdcp->fdcs_status[i]);
}
		/*print_st0(fdcp, fdcp->fdcs_status[0], fdcp->fdc_current_drive->fd_unit, fdcp->fdcs_status[FDCS_HEAD]);*/
#endif
#ifdef	FDC_DIAGNOSTICS
		if (fdcp->fdcs_status[FDCS_ST0] & 0x20)
			printf("fdsense: xfer SE %d/%d/%d -> %d/%d/%d ", oldcyl, oldhead, oldsector, fdcp->fdcs_status[FDCS_CYL], fdcp->fdcs_status[FDCS_HEAD],  fdcp->fdcs_status[FDCS_SECTOR]-1);
		if (fdcp->fdcs_status[FDCS_ST2] & NE7_WCYL) {
			printf("WCYL");
			if (oldcyl != fdcp->fdcs_status[FDCS_CYL])
				printf(": %d/%d/%d -> %d/%d/%d ", oldcyl, oldhead, oldsector, fdcp->fdcs_status[FDCS_CYL], fdcp->fdcs_status[FDCS_HEAD],  fdcp->fdcs_status[FDCS_SECTOR]-1);
		}
#endif
#define NE7_SERR	0x4
		/* nope if (fdcp->fdcs_status[FDCS_ST2] ==  (NE7_WCYL|NE7_SERR))*/
		if (fdcp->fdcs_status[FDCS_ST2] & NE7_WCYL){
			fdcp->fdc_current_drive->fd_cmd[0] = NE7CMD_RECAL;
			fdcp->fdcs_status[FDCS_CYL] = -1;
			/*fdcp->fdc_current_drive->fd_cmd[1] = i;*/
			(void)fdc_to_from (fdcp, 2);
			printf("R ");
			return(0);
		}
		
		rv = 2;
	}

#ifdef	FDC_DIAGNOSTICS
	/* drive should not be busy, have data to send etc. Should be awaiting command */
	if ((fdcinb(fdcp->fdc_port+fdsts) & 0xf0) != 0x80)
		printf("fdsense: after sense  sts %x! ", fdcinb(fdcp->fdc_port+fdsts));
#endif
	fdcp->fdc_current_drive->fd_cmd[0] = 0; /* no cmd */
	return rv;

#ifdef where_does_this_go
	/* get drive sense */
	fdcp->fdc_current_drive->fd_cmd[0] = NE7CMD_SENSED;
	fdcp->fdc_current_drive->fd_cmd[1] = head<<2 + fdcp->fdc_current_drive->fd_unit;
	if (fdc_to_from (fdcp, 2) != 1);

	if(fdtest)printf("st3 %x ", fdcp->fdc_st3);
	/*drive error/rdy for can/isn't working (check after seek?)
        write protect key on floppy media sense (ioctl?)
        on track 0 drive head positioner sense (check after movement to cyl 0?)
	drive is capabile of double sided media (probe? on open?)
        drive head and unit selected by controller (extract each time to confirm same as thought?)
*/
#endif
}

/*
 * send to controller command/parameters and recieve back status
no CB at start if either immediate command or perhaps seek? CB if transfer command interrupt yeilding sense, then released
 */
/* 1 -> nominal command/sense, -1 -> t/o busy, 0 -> no command/sense */
int fdc_to_from (struct fdc_stuff *fdcp, unsigned len)
{
	int timeo = 100000, sts;
	int to_from, from_fdc = (NE7_DIO|NE7_RQM|NE7_CB), to_fdc = (NE7_RQM|NE7_CB),  to_cnt = 0, from_cnt = 0;
	
	/* either called */
	if (len) {
		/* send command to make controller busy, wait to come busy, fail if it doesn't */
		fdcoutb(fdcp->fdc_port+fddata, fdcp->fdc_current_drive->fd_cmd[to_cnt++]); len--;
		while((fdcinb(fdcp->fdc_port+fdsts) & NE7_CB) == 0 && timeo--);
		if (timeo == 0)
			return (-1);
	}
	DELAY(1000);

	/* send parameters then receive sense while busy, fail if it doesn't */
	do {
		to_from = (sts = fdcinb(fdcp->fdc_port+fdsts)) & from_fdc; 

		/* send a command parameter? */
		if (to_from  == to_fdc && len > 0) {
		 	fdcoutb(fdcp->fdc_port+fddata, fdcp->fdc_current_drive->fd_cmd[to_cnt++]); len--;
			if (len == 0 && fdcp->fdc_interrupt_flags & (FDC_FLAG_INTR_SEEK | FDC_FLAG_INTR_TRANSFER))
				break;
		}

		/* receive sense? */
		if (to_from  == from_fdc) {
			/* sense asserted during a command sequence? */
			if (len > 0) {
				printf(" fdc supplying sense while %d more commands parameters to be sent  ", len);
				break;
			}
			/* shuffle sense to appropriate places */
			else {
				if (fdcp->fdc_current_drive->fd_cmd[0] == NE7CMD_SENSED) {
					if (from_cnt == 0)
						fdcp->fdc_st3 = fdcinb(fdcp->fdc_port+fddata);
				}
				else if (fdcp->fdc_current_drive->fd_cmd[0] == NE7CMD_SENSEI) {
					if (from_cnt == 0)
						fdcp->fdcs_status[FDCS_ST0] = fdcinb(fdcp->fdc_port+fddata);
					if (from_cnt == 1)
						fdcp->fdcs_status[FDCS_CYL] = fdcinb(fdcp->fdc_port+fddata);
				} else
					fdcp->fdcs_status[from_cnt] = fdcinb(fdcp->fdc_port+fddata);
				from_cnt++;
			}
		}
	} while ((sts & NE7_CB) && timeo--);

	if (timeo == 0 || len != 0)
		return -1;
	if (to_cnt || from_cnt)
		return 1;
	return 0;
}

/* configuration into kernel */

struct devif fdc_devif =
{
	{0}, -1, -1, 0x8, 3, 3, 0, 0, 0,
	fdopen,	fdclose, fdioctl, 0, 0, 0, 0,
	fdstrategy,	0, 0, 0,
};

/*
static struct bdevsw fd_bdevsw = 
	{ fdopen,	fdclose,	fdstrategy,	fdioctl,
	  fddump,	fdsize,		NULL };

void mkraw(struct bdevsw *bdp, struct cdevsw *cdp);*/

DRIVER_MODCONFIG() {
	int vec[3], nvec = 3, nctl;	/* (bdev, cdev, nctl) */
	char *cfg_string = fdc_config;
#ifdef	nope
	struct cdevsw fd_cdevsw;
	
	vec[2] = 1;	/* by default, one controller */
	if (!config_scan(fd_config, &cfg_string)) 
		return /*(EINVAL)*/;

	/* no more controllers than 2 */
	if (vec[2] > 2)
		vec[2] = 2;

	/* bdevsw[vec[0]] = fd_bdevsw;
	mkraw(&fd_bdevsw, &fd_cdevsw);
	cdevsw[vec[1]] = fd_cdevsw; */

	if (!spec_config(&cfg_string, &fd_bdevsw, (struct cdevsw *) -1, 0))
		return;
#else
	if (devif_config(&cfg_string, &fdc_devif) == 0)
		return;
#endif
	
	(void)cfg_number(&cfg_string, &nctl);

	/* malloc(vec[2]*controllerresources) ... */

	new_isa_configure(&cfg_string, &fdcdriver);

	/*return (0);*/
}


int
fdcinb(int port) {
	int i, rv = inb(port);
	for (i = 0; i < 20; i++)
		(void)inb_(0x84);
	return rv;
}

void
fdcoutb(int port, int value) {
	int i;
	(void)inb_(0x84);
	(void)inb_(0x84);
	outb(port, value);
	for (i = 0; i < 20; i++)
		(void)inb_(0x84);
}


/*
 * Errata:
 * on real hw can't probe correctly, can't get rootfs.
 * jams on umount/sync after mount
 *
 * Timeouts:
 *    * missed interrupt? reissue command  from top level (not interrupt level), set reissue?
 *    * pauses in operation? warm up/settle
 *    * reset sequencing from jammed to getting sense/recal
 *
 * Two kinds of interrupt:
 *    * head movement
 *    * transfer
 *
 * Controllers either are capable of sequencing commmands or are in the process of reset
 * Drives either capabile of error free movement within seconds or are in the process of retry
 * Media is either capabile of error free transfer on cylinder or are in the process of retry
 * 
 */
