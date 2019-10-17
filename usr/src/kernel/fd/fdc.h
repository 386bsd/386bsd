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

struct fdc_stuff {
	int	fdc_port;	/* i/o port base */
	int	fdc_dmachan;	/* isa dma channel */
	int	fdc_interrupt_flags;	/* timeout/retry/nested/hardware/settle state bits on interrupt */
#define FDC_FLAG_INTR_SEEK	1	/* issued seek command generates interrupt */
#define FDC_FLAG_INTR_TRANSFER	2	/* issued transfer command generates interrupt */
#define FDC_FLAG_INTR_ENDSECTOR	4	/* attempting transfer accross track boundary */
#define FDC_FLAG_INTR_CYLNXT	8	/* attempting transfer accross track boundary */
#define FDC_FLAG_INTR_RESET	0x10	/* interrupt on reset of controller */

	int	fdc_cmd[9];	/* last command to controller */
#define	FDCC_COMMAND	0		/* command code */
#define	FDCC_HDDS	1		/* head and drive select */
#define	FDCC_CYL	2		/* cylinder in drive */
#define	FDCC_HEAD	3		/* head in drive's cylinder */
#define	FDCC_SECTOR	4		/* sector under head in drive's cylinder */

	int 	fdc_state;	/* controller state */
	int	fdcs_status[7];	/* last status */
#define	FDCS_ST0	0		/* status register 0 */
#define	FDCS_ST1	1		/* status register 1 */
#define	FDCS_ST2	2		/* status register 2 */
#define	FDCS_CYL	3		/* cylinder in drive */
#define	FDCS_HEAD	4		/* head in drive's cylinder */
#define	FDCS_SECTOR	5		/* sector under head in drive's cylinder */
#define	FDCS_SECTORSZ	6		/* sector's size code */

	int 	fdc_st3;	/* last status */
	struct	fd_drive *fdc_current_drive;	/* current drive */
	int	fdc_skip;	/* sub transfer counter (in bytes transferred) */
};

/* Common formatted bitfields in command/sense */
#define	FD_HDDRV(drv, hd)	 		((hd)<<2)|(drv) 		/* head and drive */

/* Sense register macros to make understandable controller responses */

/* command conclusion */
#define	FD_ST0_COMMAND_FINISHED(st0)		(((st0) & 0xc0) == 0)		/* command finished normally */
#define	FD_ST0_COMMAND_DIDNT_FINISH(st0)	(((st0) & 0xc0) == 0x40)	/* command did not finish */
#define	FD_ST0_INVALID_COMMAND(st0)		(((st0) & 0xc0) == 0x80)	/* invalid command */
#define	FD_ST0_COMMAND_DRIVE_ABORT(st0)		(((st0) & 0xc0) == 0xc0)	/* command aborts because of drive losing ready signal */

/* drive recal */
#define	FD_ST0_SEEK_DID(st0)			(((st0) & 0x30) == 0x20)	/* seek to cylinder 0 made it to cylinder 0 */
#define	FD_ST0_SEEK_DIDNT(st0)			(((st0) & 0x30) == 0x30)	/* seek to cylinder 0 did not make it to cylinder 0 */

/* drive ready */
#define	FD_ST0_DRIVE_READY(st0)			(((st0) & 8) != 8)		/* drive ready */

/* this drive */
#define	FD_ST0_THIS_DRIVE(st0, drv, hd)	(((st0) & 7) == FD_HDDRV(drv, hd))	/* confirm this is the correct drive and head */

void print_st0(struct fdc_stuff *fdcp, int st0, int drv, int hd);
int fdc_to_from(struct fdc_stuff *fdcp, unsigned len);
int fdc_sense(struct fdc_stuff *fdcp);
int fdcinb(int port);
void fdcoutb(int port, int value);
void fdcstart();
