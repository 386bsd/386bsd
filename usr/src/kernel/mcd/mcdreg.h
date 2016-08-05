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

 *	@(#) $RCSfile: mcdreg.h,v $	$Revision: 1.4 $ $Date: 95/01/08 16:51:37 $
 *
 * This file contains definitions for some cdrom control commands
 * and status codes. This info was "inherited" from the DOS MTMCDE.SYS
 * driver, and is thus not complete (and may even be wrong). Some day
 * the manufacturer or anyone else might provide better documentation,
 * so this file (and the driver) will then have a better quality.
 */
#ifndef MCD_H
#define MCD_H

#ifdef __GNUC__
#if __GNUC__ >= 2
#pragma pack(1)
#endif
#endif

typedef unsigned char	bcd_t;
#define M_msf(msf) msf[0]
#define S_msf(msf) msf[1]
#define F_msf(msf) msf[2]

/*
 * Decoder Card ports and port bit definitions
 */
#define	mcd_command	0
#define mcd_status	0
#define mcd_rdata	0
#define mcd_Data	0		/* Drive data port (R/W) */

#define mcd_reset	1		/* Drive Reset signal (W) */
#define mcd_Status	1		/* Drive status signals (R) */
/*#define		MCD_CMDRESET	0x00*/
#define mcd_xfer	1
#define		MCD_ST_BUSY	0x04
#define		MCD_STEN	0x04	/* status enabled */
#define		MCD_DTEN	0x02	/* data enabled */
		/* XXX Bit 3,1 used, but for what? */
#define mcd_ctl2	2 /* XXX Is this right? */
#define mcd_hostcfg	2		/* Host Bus adapter configuration */
#define		MCD_16BUFEN	0x41	/* enable low and hi byte */
#define		MCD_16DIR	0x80	/* read PIO instead of DMA (override) */
#define		MCDH_DTS	0x08	/* read PIO instead of DMA (override) */
/*#define		MCD_???	0x04	/* ?? */

#define mcd_config	3
#define		MCD_DECODER	0x80	/* OTI decoder instead of Sanyo */
#define		MCD_MASK_IRQ	0x70	/* bits 6-4 = INT number */
					/* 000 = *no interrupt* */
					/* 001 = int 2,9 */
					/* 010 = int 3 */
					/* 011 = int 5 */
					/* 100 = int 10 */
					/* 101 = int 11 */
#define 	MCD_PIO16	0x10	/* 16 vs 8 bit programmed I/O */
#define 	MCD_MASK_DMA	0x07	/* bits 2-0 = DMA channel */
					/* 000 = *no DMA*, PIO */

/*
 * CDROM drive commands, command options, and status bits
 */

/* commands known by the controller */
#define MCD_CMDGETVOLINFO	0x10	/* gets mcd_volinfo  GET TOC*/
#define MCD_CMDGETSESSION	0x11	/* get session info */
					/* 0=single / 1=multisession, */
					/* mm, hh, ff of last session */

#define MCD_CMDGETQCHN	0x20	/* gets mcd_qchninfo */
#define MCD_CMDGETSTAT	0x40	/* gets a byte  of status */
#define		MCD_ST_DOOROPEN	0x80	/* drive door is open */
#define		MCD_ST_DSKIN	0x40	/* disk is inside drive */
#define		MCD_ST_DSKCHNG	0x20	/* disk has been changed */
#define		MCD_ST_DSKSPIN	0x10	/* disk is spinning */
#define		MCD_ST_AUDIOTRK	0x08	/* audio or data track */
#define		MCD_ST_READERR	0x04	/* read error */
#define		MCD_ST_AUDIOBSY	0x02	/* drive playing audio */
#define		MCD_ST_INVCMD	0x01	/* invalid command */

#define	MCD_CMDSETMODE	0x50	/* set transmission mode, needs byte */
#define	 MCD_MD_RAW		 0x60
#define	 MCD_MD_UPC		 0x00
#define	 MCD_MD_COOKED		 0x01
#define	 MCD_MD_TOC		 0x05

#define MCD_SOFTRESET		0x60
#define MCD_CMDSTOPAUDIO	0x70	/* actually pause HOLD */
#define MCD_CMDINACTIVE		0x80	/* inactivity timeout */

#define MCD_CMDGETVOLUME	0x8E	/* gets mcd_volume */

	/* XXX 0x90 used, 1,2,or 3 args */
#define	MCD_SETPARAMS	0x90	/* set drive parameters */
#define	 MCD_SP_DLEN	 0x01	 /* data length selection, takes two bytes */
				  /* MSB, then LSB */
#define	 MCD_SP_MODE	 0x02	 /* mode selection, takes a byte: */
#define	  MCDSP_DMA	  0x01	  /* DMA mode */
#define	  MCDSP_PIO	  0x00	  /* PIO mode */
#define	 MCD_SP_SYSMODE	 0x04	 /* system mode selection, takes a byte: */
#define	  MCDSP_UPC	  0x01	  /* enter UPC mode */
#define	  MCDSP_NORMAL	  0x00	  /* leave UPC mode, resume normal mode */
#define	 MCD_SP_TIMEO	 0x08	 /* timeout selection, takes a byte(2MS units) */
#define	 MCD_SP_INTR	 0x10	 /* interrupt selection, takes a byte: */
#define	  MCDSP_PRE	  0x05	  /* interrupt before xxx */
#define	  MCDSP_POST	  0x06	  /* interrupt after xxx */
#define	 MCD_SP_SRETRYS	 0x20	 /* seek retrys selection, takes a byte */

	/* XXX 0xA0 used, 1 arg */
#define	MCD_SETDSKMODE	0xa0	/* set diskette mode, takes a byte */
				 /* MODE# (either 0x01 or 0x02 */
#define MCD_CMDGETUPC	0xA2	/* put upc into subchannel data */

#define MCD_CMDSETVOLUME	0xAE	/* sets mcd_volume */
#define MCD_CMDREAD1	0xB0	/* read n sectors of audio subchannel */
#define MCD_CMDREAD2	0xC0	/* read from-to */
#define MCD_CMDREAD2D	0xC1	/* read from-to (double speed) */
	/* XXX 0xDC used, gets 2 bytes */
#define MCD_CMDGETVERS	0xDC	/* get drive model & firmware version */
#define	 MCD_VER_LU	 'M'	 /* LU model */
#define	 MCD_VER_FX	 'F'	 /* FX model */
#define	 MCD_VER_FXD	 'D'	 /* FXxxxD model */
#define MCD_CMDSTOP		0xF0	/* stop CD from spinning */
#define MCD_CMDEJECTDISK	0xF6
#define	MCD_CMDCLOSETRAY	0xF8
#define MCD_CMDLOCKDRV	0xFE	/* needs byte */
#define		MCD_LK_UNLOCK	0x00
#define		MCD_LK_LOCK	0x01
#define		MCD_LK_TEST	0x02

struct mcd_volinfo {
	bcd_t	trk_low;
	bcd_t	trk_high;
	bcd_t	vol_msf[3];
	bcd_t	trk1_msf[3];
};

struct mcd_qchninfo {
	u_char	ctrl_adr;
	u_char	trk_no;
	u_char	idx_no;
	bcd_t	trk_size_msf[3];
	u_char	:8;
	bcd_t	hd_pos_msf[3];
};

struct mcd_volume {
	u_char	v0l;
	u_char	v0rs;
	u_char	v0r;
	u_char	v0ls;
};

struct mcd_read1 {
	bcd_t	start_msf[3];
	u_char	nsec[3];
};

struct mcd_read2 {
	bcd_t	start_msf[3];
	bcd_t	end_msf[3];
};


#endif /* MCD_H */
