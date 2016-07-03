/*
 * regs3.h
 * 
 * Written by Jake Richter Copyright (c) 1989, 1990 Panacea Inc., Londonderry,
 * NH - All Rights Reserved
 * 
 * This code may be freely incorporated in any program without royalty, as long
 * as the copyright notice stays intact.
 * 
 * Additions by Kevin E. Martin (martin@cs.unc.edu)
 * 
 * KEVIN E. MARTIN DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEVIN E. MARTIN BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 * 
 */

/* $XFree86: mit/server/ddx/x386/accel/s3/regs3.h,v 2.9 1993/09/22 15:43:18 dawes Exp $ */

#ifndef _S3_H
#define _S3_H

/* for OUT instructions */
#include "compiler.h"

/* S3 chipset definitions */

/*
 * Modified by Amancio Hasty and Jon Tombs
 * 
 * Id: reg8514.h,v 2.2 1993/06/22 20:54:09 jon Exp jon
 */

/*
 * S3 chipset definitions Provided by Jim Nichols <nichols@felix.cmd.usf.edu>
 * intended use: if (S3_801_928_SERIES(chip_id)) { <S3 801 and 928 specific
 * code here> } Note:  The S3 801 and 928 chipsets are very similar.  The
 * primary difference is that the 801 is DRAM based and the 928 is VRAM
 * based.  If you add code for the 801, chances are it will also work fine
 * with the 928, so please use S3_801_928_SERIES and have someone with a 928
 * test it. The 801 and the 805 appear to be indistinguishable from each
 * other using just the chip_id byte.  The 805 is the localbus and EISA
 * version of the 801. S3_911_SERIES will match both the 911 and 924 (as well
 * as any future versions of the 911/924 chips, but not the 801 or 928).
 */

/*
 * The early 928's have a nasty bug where memory access can alter io registers.
 * By locking them hopefully this won't happen.
 */

#define UNLOCK_SYS_REGS	          { \
				   outb(vgaCRIndex, 0x39); \
				   outb(vgaCRReg, 0xa5); }

/*
 * I haven't had time to put the lock and unlocks in all the right places, for
 * now things will need to stay locked.
 */
#define LOCK_SYS_REGS	        /**/
#ifdef notyet
				 { \
				   outb(vgaCRIndex, 0x39); \
				   outb(vgaCRReg, 0x50); }
#endif				   

#define S3_911_ONLY(chip)     (chip==0x81)
#define S3_924_ONLY(chip)     (chip==0x82)
#if 0
/* XXXX These are NOT valid. These will only detect certains steps */
#define S3_801_ONLY(chip)       (chip==0xa0)
#define S3_928_ONLY(chip)       (chip==0x90)
#endif
#define S3_911_SERIES(chip)     ((chip&0xf0)==0x80)
#define S3_801_SERIES(chip)     ((chip&0xf0)==0xa0)
#define S3_928_SERIES(chip)     (((chip&0xf0)==0x90)||((chip&0xf0)==0xb0))
#define S3_801_928_SERIES(chip) (S3_801_SERIES(chip)||S3_928_SERIES(chip))
#define S3_8XX_9XX_SERIES(chip) (S3_911_SERIES(chip)||S3_801_928_SERIES(chip))
#define S3_ANY_SERIES(chip)     (S3_8XX_9XX_SERIES(chip))

/* VESA Approved Register Definitions */
#define	DAC_MASK	0x03c6
#define	DAC_R_INDEX	0x03c7
#define	DAC_W_INDEX	0x03c8
#define	DAC_DATA	0x03c9
#define	DISP_STAT	0x02e8
#define	H_TOTAL		0x02e8
#define	H_DISP		0x06e8
#define	H_SYNC_STRT	0x0ae8
#define	H_SYNC_WID	0x0ee8
#define	V_TOTAL		0x12e8
#define	V_DISP		0x16e8
#define	V_SYNC_STRT	0x1ae8
#define	V_SYNC_WID	0x1ee8
#define	DISP_CNTL	0x22e8
#define	ADVFUNC_CNTL	0x4ae8
#define	SUBSYS_STAT	0x42e8
#define	SUBSYS_CNTL	0x42e8
#define	ROM_PAGE_SEL	0x46e8
#define	CUR_Y		0x82e8
#define	CUR_X		0x86e8
#define	DESTY_AXSTP	0x8ae8
#define	DESTX_DIASTP	0x8ee8
#define	ERR_TERM	0x92e8
#define	MAJ_AXIS_PCNT	0x96e8
#define	GP_STAT		0x9ae8
#define	CMD		0x9ae8
#define	SHORT_STROKE	0x9ee8
#define	BKGD_COLOR	0xa2e8
#define	FRGD_COLOR	0xa6e8
#define	WRT_MASK	0xaae8
#define	RD_MASK		0xaee8
#define	COLOR_CMP	0xb2e8
#define	BKGD_MIX	0xb6e8
#define	FRGD_MIX	0xbae8
#define	MULTIFUNC_CNTL	0xbee8
#define	MIN_AXIS_PCNT	0x0000
#define	SCISSORS_T	0x1000
#define	SCISSORS_L	0x2000
#define	SCISSORS_B	0x3000
#define	SCISSORS_R	0x4000
#define	MEM_CNTL	0x5000
#define	PATTERN_L	0x8000
#define	PATTERN_H	0x9000
#define	PIX_CNTL	0xa000
#define	PIX_TRANS	0xe2e8

/* Display Status Bit Fields */
#define	HORTOG		0x0004
#define	VBLANK		0x0002
#define	SENSE		0x0001

/* Horizontal Sync Width Bit Field */
#define HSYNCPOL_NEG	0x0020
#define	HSYNCPOL_POS	0x0000

/* Vertical Sync Width Bit Field */
#define	VSYNCPOL_NEG	0x0020
#define	VSYNCPOL_POS	0x0000

/* Display Control Bit Field */
#define	DISPEN_NC	0x0000
#define	DISPEN_DISAB	0x0040
#define	DISPEN_ENAB	0x0020
#define	INTERLACE	0x0010
#define	DBLSCAN		0x0008
#define	MEMCFG_2	0x0000
#define	MEMCFG_4	0x0002
#define	MEMCFG_6	0x0004
#define	MEMCFG_8	0x0006
#define	ODDBNKENAB	0x0001

/* Subsystem Status Register */
#define	_8PLANE		0x0080
#define	MONITORID_8503	0x0050
#define	MONITORID_8507	0x0010
#define	MONITORID_8512	0x0060
#define	MONITORID_8513	0x0060
#define	MONITORID_8514	0x0020
#define	MONITORID_NONE	0x0070
#define	MONITORID_MASK	0x0070
#define	GPIDLE		0x0008
#define	INVALIDIO	0x0004
#define	PICKFLAG	0x0002
#define	VBLNKFLG	0x0001

/* Subsystem Control Register */
#define	GPCTRL_NC	0x0000
#define	GPCTRL_ENAB	0x4000
#define	GPCTRL_RESET	0x8000
#define CHPTEST_NC	0x0000
#define CHPTEST_NORMAL	0x1000
#define CHPTEST_ENAB	0x2000
#define	IGPIDLE		0x0800
#define	IINVALIDIO	0x0400
#define	IPICKFLAG	0x0200
#define	IVBLNKFLG	0x0100
#define	RGPIDLE		0x0008
#define	RINVALIDIO	0x0004
#define	RPICKFLAG	0x0002
#define	RVBLNKFLG	0x0001

/* Current X, Y & Dest X, Y Mask */
#define	COORD_MASK	0x07ff

#ifdef CLKSEL
#undef CLKSEL
#endif

/* Advanced Function Control Regsiter */
#define	CLKSEL		0x0004
#define	DISABPASSTHRU	0x0001

/* Graphics Processor Status Register */
#define	GPBUSY		0x0200
#define	DATDRDY		0x0100

/* Command Register */
#define	CMD_NOP		0x0000
#define	CMD_LINE	0x2000
#define	CMD_RECT	0x4000
#define	CMD_RECTV1	0x6000
#define	CMD_RECTV2	0x8000
#define	CMD_LINEAF	0xa000
#define	CMD_BITBLT	0xc000
#define	CMD_OP_MSK	0xf000
#define	BYTSEQ		0x1000
#define	_16BIT		0x0200
#define	PCDATA		0x0100
#define	INC_Y		0x0080
#define	YMAJAXIS	0x0040
#define	INC_X		0x0020
#define	DRAW		0x0010
#define	LINETYPE	0x0008
#define	LASTPIX		0x0004
#define	PLANAR		0x0002
#define	WRTDATA		0x0001

/*
 * Short Stroke Vector Transfer Register (The angular Defs also apply to the
 * Command Register
 */
#define	VECDIR_000	0x0000
#define	VECDIR_045	0x0020
#define	VECDIR_090	0x0040
#define	VECDIR_135	0x0060
#define	VECDIR_180	0x0080
#define	VECDIR_225	0x00a0
#define	VECDIR_270	0x00c0
#define	VECDIR_315	0x00e0
#define	SSVDRAW		0x0010

/* Background Mix Register */
#define	BSS_BKGDCOL	0x0000
#define	BSS_FRGDCOL	0x0020
#define	BSS_PCDATA	0x0040
#define	BSS_BITBLT	0x0060

/* Foreground Mix Register */
#define	FSS_BKGDCOL	0x0000
#define	FSS_FRGDCOL	0x0020
#define	FSS_PCDATA	0x0040
#define	FSS_BITBLT	0x0060

/* The Mixes */
#define	MIX_MASK			0x001f

#define	MIX_NOT_DST			0x0000
#define	MIX_0				0x0001
#define	MIX_1				0x0002
#define	MIX_DST				0x0003
#define	MIX_NOT_SRC			0x0004
#define	MIX_XOR				0x0005
#define	MIX_XNOR			0x0006
#define	MIX_SRC				0x0007
#define	MIX_NAND			0x0008
#define	MIX_NOT_SRC_OR_DST		0x0009
#define	MIX_SRC_OR_NOT_DST		0x000a
#define	MIX_OR				0x000b
#define	MIX_AND				0x000c
#define	MIX_SRC_AND_NOT_DST		0x000d
#define	MIX_NOT_SRC_AND_DST		0x000e
#define	MIX_NOR				0x000f

#define	MIX_MIN				0x0010
#define	MIX_DST_MINUS_SRC		0x0011
#define	MIX_SRC_MINUS_DST		0x0012
#define	MIX_PLUS			0x0013
#define	MIX_MAX				0x0014
#define	MIX_HALF__DST_MINUS_SRC		0x0015
#define	MIX_HALF__SRC_MINUS_DST		0x0016
#define	MIX_AVERAGE			0x0017
#define	MIX_DST_MINUS_SRC_SAT		0x0018
#define	MIX_SRC_MINUS_DST_SAT		0x001a
#define	MIX_HALF__DST_MINUS_SRC_SAT	0x001c
#define	MIX_HALF__SRC_MINUS_DST_SAT	0x001e
#define	MIX_AVERAGE_SAT			0x001f

/* Memory Control Register */
#define	BUFSWP		0x0010
#define	VRTCFG_2	0x0000
#define	VRTCFG_4	0x0004
#define	VRTCFG_6	0x0008
#define	VRTCFG_8	0x000C
#define	HORCFG_4	0x0000
#define	HORCFG_5	0x0001
#define	HORCFG_8	0x0002
#define	HORCFG_10	0x0003

/* Pixel Control Register */
#define	MIXSEL_FRGDMIX	0x0000
#define	MIXSEL_PATT	0x0040
#define	MIXSEL_EXPPC	0x0080
#define	MIXSEL_EXPBLT	0x00c0
#define COLCMPOP_F	0x0000
#define COLCMPOP_T	0x0008
#define COLCMPOP_GE	0x0010
#define COLCMPOP_LT	0x0018
#define COLCMPOP_NE	0x0020
#define COLCMPOP_EQ	0x0028
#define COLCMPOP_LE	0x0030
#define COLCMPOP_GT	0x0038
#define	PLANEMODE	0x0004

  typedef struct {
     unsigned char r, g, b;
  }
LUTENTRY;

/* Wait until "v" queue entries are free */
#define	WaitQueue(v)	{ while (inb(GP_STAT) & (0x0100 >> (v))); }

/* Wait until GP is idle and queue is empty */
#define	WaitIdleEmpty() \
   { while (inw(GP_STAT) & (GPBUSY | 1)); }

/* Wait until GP is idle */
#define WaitIdle() { while (inw(GP_STAT) & GPBUSY) ; }

#define	MODE_800	1
#define	MODE_1024	2
#define	MODE_1280	3
#define MODE_1152	4	/* 801/805 C-step, 928 E-step */
#define MODE_1600	5	/* 928 E-step */

#ifndef NULL
#define NULL	0
#endif

#endif /* _S3_H */
