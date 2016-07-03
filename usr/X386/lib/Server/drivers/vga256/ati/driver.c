/*
 * This is the X11R5 x386 driver for ATI VGA WONDER video adapters.  At
 * present, this drive works best with ATI VGA WONDER PLUS and ATI VGA
 * WONDER XL cards with the ATI18810 dot clock and the ATI28800-5 chip.
 * ATI VGA WONDER cards with other chips revisions may not function as
 * desired.
 * 
 * Revised: Sat Feb 13 13:23:18 1993 by root@winter
 *
 */

/*
 * Copyright 1990,91 by Thomas Roell, Dinkelscherben, Germany.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Thomas Roell not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Thomas Roell makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * THOMAS ROELL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THOMAS ROELL BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Thomas Roell, roell@informatik.tu-muenchen.de
 *
 * $XFree86: mit/server/ddx/x386/vga256/drivers/ati/driver.c,v 2.12 1993/10/18 12:18:47 dawes Exp $
 */

/*
 * Hacked, to get it working by:
 *	Per Lindqvist, pgd@compuram.bbt.se
 *
 * Enhancements to support most VGA Wonder cards (including Plus and XL)
 * by Doug Evans, dje@sspiff.UUCP.
 * ALL DISCLAIMERS APPLY TO THESE ADDITIONS AS WELL.
 *
 *	I've come to believe the people who design these cards couldn't
 *	allow for future enhancements if their life depended on it!
 *	The register set architecture is a joke!
 *
 */

/*
 * Hacked for X11R5 by Rik Faith (faith@cs.unc.edu), Thu Aug  6 21:35:18 1992
 * ALL DISCLAIMERS APPLY TO MY ADDITIONS AS WELL.
 *
 * I wrote the ATIProbe and ATIEnterLeave routines from scratch.  I also
 * re-organized the ATISave and ATIRestore routines.  This was necessary,
 * since the vgaHWSave routine did not save the DAC values.  There also
 * appeared to be a timing problem associated with the 486-33, but this is
 * not well understood at this time.
 *
 * Change log, faith@cs.unc.edu:
 *  3Feb93: ER_B0 in extregPlusXLAndOrMasks changed from 0x28 to 0x31.
 *  3Feb93: ER_BE in extregPlusXLAndOrMasks changed from 0x08 to 0x18,
 *          but ONLY for chips BEFORE the 28800-5.  This required adding
 *          the ATIChipVersion variable.
 *  3Feb93: Added message stating correct clocks line.
 * 13Feb93: Fixed warning message for unsupported boards.
 * 13Feb93: Added comments and shortened lines over 80 characters long.
 * 13Feb93: Added the Graphics Ultra Plus \'a\' chip version
 */

/*
 * NOTES:
 *
 *   1) The ATI 18800/28800 use a special registers for their extended
 *      features. There is one index and one data register. Under MS-DOS this
 *      should be specified by reading C000:10. But since we cannot read this
 *      now, let's use the same fixed address as our unix kernel does:
 *      0x1CE/0x1CF. I also got these ports form a second source, thus it seems
 *      to be safe to use them.
 *
 *	This comment is no longer valid. We read C000:10.
 *
 *   2) The ATI 18800/28800 extended registers differ in their i/o behaviour
 *      from the normal ones:
 *
 *       write:   outw(0x1CE, (data << 8) | index);
 *       read:    outb(0x1CE, index); data = inb(0x1CF);
 *
 *      Two consecutive byte-writes are NOT allowed. Furthermore is a index
 *      written to 0x1CE only usable ONCE !!!
 *
 * More notes by dje ...
 *
 *   3) I've tried to allow for a future when this code drives all VGA Wonder
 *	cards. To do this I had decide what to do with the clock values. Boards
 *	prior to V5 use 4 crystals. Boards V5 and later use a clock generator
 *	chip. Just to complicate things a bit, V3 and V4 boards aren't
 *	compatible when it comes to choosing clock frequencies. :-(
 *
 *	V3/V4 Board Clock Frequencies
 *	R E G I S T E R S
 *	1CE(*)		3C2	3C2		Frequency
 *	B2h/BEh
 *	Bit 6/4		Bit 3	Bit 2
 *	---------	-------	-------		---------
 *	0		0	0		50.175
 *	0		0	1		56.644
 *	0		1	0		Spare 1
 *	0		1	1		44.900
 *	1		0	0		44.900
 *	1		0	1		50.175
 *	1		1	0		Spare 2
 *	1		1	1		36.000
 *
 *	(*): V3 uses Index B2h, bit 6; V4 uses Index BEh, bit 4.
 *
 *	V5,Plus,XL Board Clock Frequencies
 *	R E G I S T E R S
 *	1CE	1CE	3C2	3C2		Frequency
 *	BEh	B9h
 *	Bit 4	Bit 1	Bit 3	Bit 2
 *	-------	------- -------	-------		---------
 *	1	0	0	0		42.954
 *	1	0	0	1		48.771
 *	1	0	1	0		External 0 (16.657)
 *	1	0	1	1		36.000
 *	1	1	0	0		50.350
 *	1	1	0	1		56.640
 *	1	1	1	0		External 1 (28.322)
 *	1	1	1	1		44.900
 *	0	0	0	0		30.240
 *	0	0	0	1		32.000
 *	0	0	1	0		37.500
 *	0	0	1	1		39.000
 *	0	1	0	0		40.000
 *	0	1	0	1		56.644
 *	0	1	1	0		75.000
 *	0	1	1	1		65.000
 *
 *	For all of the above (V3,V4,V5,Plus,XL), these frequencies can be
 *	divided by 1, 2, 3, or 4:
 *
 *	Reg 1CE, Index B8h
 *	Bit 7	Bit 6
 *	0	0		Divide by 1
 *	0	1		Divide by 2
 *	1	0		Divide by 3
 *	1	1		Divide by 4
 *
 *	What I've done is the following. The clock values specified in Xconfig
 *	shall be:
 *
 *			18 22 25 28 36 44 50 56
 *			30 32 37 39 40 0  75 65		(duplicate 56: --> 0)
 *
 *	The first row is usable on all VGA Wonder cards. The second row is
 *	only usable on V5,Plus,XL cards. The code in ATIInit() will map these
 *	clock values into the appropriate bit settings for each of the cards.
 *	Some of these clock choices aren't really usable, think of these as
 *	place holders.
 *
 *   4) The ATI Programmer's Reference Manual lacked enough prose to explain
 *	what the various bits of all of the various registers do. It is
 *	entirely reasonable to believe some of the choices I've made are
 *	imperfect because I didn't understand what I was doing.
 *
 *   5) V3 board support needs a lot of work. I suspect it isn't worth it.
 */


#include "X.h"
#include "input.h"
#include "screenint.h"

#include "compiler.h"

#include "x386.h"
#include "x386Priv.h"
#include "xf86_OSlib.h"
#include "xf86_HWlib.h"
#include "vga.h"
#include <sys/types.h>

#define DEBUG       0

#if DEBUG
#define TRACE(a)    ErrorF a	/* Enable TRACE statements */
#define DEBUG_PROBE 0		/* Debug the ATIProbe() routine */
#else
#define TRACE(a)		/* Disable TRACE statements */
#endif

typedef struct {
	vgaHWRec std;		/* good old IBM VGA */
	unsigned char ATIExtRegBank[11]; /* ATI Registers B0,B1,B2,B3,B5,B6,B8,B9,BE,A6,A7 */
} vgaATIRec, *vgaATIPtr;

#define ATI_BOARD_V3	0	/* which ATI board does the user have? */
#define ATI_BOARD_V4	1	/* keep these chronologically increasing */
#define ATI_BOARD_V5	2
#define ATI_BOARD_PLUS	3
#define ATI_BOARD_XL	4

#define ER_B0	0		/* Extended Register indices (ATIExtRegBank) */
#define ER_B1	1
#define ER_B2	2
#define ER_B3	3
#define ER_B5	4
#define ER_B6	5
#define ER_B8	6
#define ER_B9	7
#define ER_BE	8
#define ER_A6	9
#define ER_A7	10

#define ATIReg0	ATIExtRegBank[ER_B0]	/* simplifies dje's additions */
#define ATIReg1	ATIExtRegBank[ER_B1]
#define ATIReg2	ATIExtRegBank[ER_B2]
#define ATIReg3	ATIExtRegBank[ER_B3]
#define ATIReg5	ATIExtRegBank[ER_B5]
#define ATIReg6	ATIExtRegBank[ER_B6]
#define ATIReg8	ATIExtRegBank[ER_B8]
#define ATIReg9	ATIExtRegBank[ER_B9]
#define ATIRegE	ATIExtRegBank[ER_BE]
#define ATIRegA6 ATIExtRegBank[ER_A6]
#define ATIRegA7 ATIExtRegBank[ER_A7]

static Bool ATIProbe();
static char *ATIIdent();
static void ATIEnterLeave();
static Bool ATIInit();
static void *ATISave();
static void ATIRestore();
static void ATIAdjust();
extern void ATISetRead();
extern void ATISetWrite();
extern void ATISetReadWrite();
static int inATI();

vgaVideoChipRec ATI = {
	ATIProbe,
	ATIIdent,
	ATIEnterLeave,
	ATIInit,
	ATISave,
	ATIRestore,
	ATIAdjust,
	NoopDDA,
	NoopDDA,
	NoopDDA,
	ATISetRead,
	ATISetWrite,
	ATISetReadWrite,
	0x10000,
	0x10000,
	16,
	0xFFFF,
	0x00000, 0x10000,
	0x00000, 0x10000,
	TRUE,                    /* uses 2 banks */
	VGA_DIVIDE_VERT,
	{0,},
	16,
};

short ATIExtReg = 0x1ce;	/* used by bank.s (must be short!) */

static int ATIBoard;		/* one of ATI_BOARD_XXX */
static int ATIChipVersion;	/* chip version number */

/*
 * ATIIdent --
 */
static char *
ATIIdent(n)
int n;
{
	static char *chipsets[] = {"ati"};

	if (n + 1 > sizeof(chipsets) / sizeof(char *))
		return(NULL);
	else
		return(chipsets[n]);
}

/*
 * ATIRestore --
 *      restore a video mode
 *
 * Results:
 *      nope.
 *
 * Side Effects:
 *      the display enters a new graphics mode.
 */

static void
ATIRestore(restore)
vgaATIPtr restore;
{
        int i;
	
	TRACE(("ATIRestore(restore=0x%x)\n", restore));

	if (vgaIOBase == 0x3B0)
		restore->std.MiscOutReg &= 0xFE;
	else
		restore->std.MiscOutReg |= 0x01;

	/* Disable video */
	(void) inb(vgaIOBase + 0x0A);	/* reset flip-flop */
	outb(0x3C0, 0x00);

	/* Unlock ATI specials */
	outw(ATIExtReg, ((inATI(0xb8) & 0xC0) << 8) | 0xb8);

	/* Load Miscellaneous Output External Register */
	outb(0x3C2, restore->std.MiscOutReg);

	outw(ATIExtReg, 0x00b2);	/* segment select 0 */

	/* For text modes, download Character Generator into Plane 2 */
	if (restore->std.FontInfo1 || restore->std.FontInfo2 ||
            restore->std.TextInfo) {
		/*
		 * here we switch temporary to 16 color-plane-mode, to simply
		 * copy the font-info and saved text
		 *
		 * BUGALLERT:
		 * The vga's segment-select register MUST be set appropriate !
		 */

		inb(vgaIOBase + 0x0A); /* reset flip-flop */
		outb(0x3C0,0x10); outb(0x3C0, 0x01); /* graphics mode */

#if 0
		outw(0x3c3, 0x0001);
#endif
		
		if (restore->std.FontInfo1) {
			outw(0x3C4, 0x0402);    /* write to plane 2 */
			outw(0x3C4, 0x0604);    /* enable plane graphics */
			outw(0x3CE, 0x0204);    /* read plane 2 */
			outw(0x3CE, 0x0005);    /* write mode 0, read mode 0 */
			outw(0x3CE, 0x0506);    /* set graphics */

			bcopy(restore->std.FontInfo1, vgaBase, 8192);
		}

		if (restore->std.FontInfo2) {
			outw(0x3C4, 0x0802);	/* write to plane 3 */
			outw(0x3C4, 0x0604);	/* enable plane grp */
			outw(0x3CE, 0x0304);	/* read plane 3 */
			outw(0x3CE, 0x0005);	/* read/write mode 0 */
			outw(0x3CE, 0x0506);	/* set graphics */
			bcopy(restore->std.FontInfo2, vgaBase, 8192);
		}

		if (restore->std.TextInfo) {
			outw(0x3C4, 0x0202);	/* write to plane 1 */
			outw(0x3C4, 0x0604);	/* enable plane grp */
			outw(0x3CE, 0x0104);	/* read plane 1 */
			outw(0x3CE, 0x0005);	/* read/write mode 0 */
			outw(0x3CE, 0x0506);	/* set graphics */
			bcopy(restore->std.TextInfo, vgaBase, 4096);

			outw(0x3C4, 0x0102);	/* write to plane 0 */
			outw(0x3C4, 0x0604);	/* enable plane grp */
			outw(0x3CE, 0x0004);	/* read plane 0 */
			outw(0x3CE, 0x0005);	/* read/write mode 0 */
			outw(0x3CE, 0x0506);	/* set graphics */
			bcopy(restore->std.TextInfo + 4096, vgaBase, 4096);
		}
	}
	
	/* This sequence is from the "VGA Wonder Programmer's
	   Reference Manual."  I assume it is correct :-)
	   faith@cs.unc.edu (7Aug92) */

	/* Place Sequencer into Reset condition using its Reset Register */
	outw(0x3C4, 0x0100);

	/* Load ATI Extended Registers */
	outw(ATIExtReg, (restore->ATIReg0 << 8) | 0xb0);
	outw(ATIExtReg, (restore->ATIReg1 << 8) | 0xb1);
	outw(ATIExtReg, (restore->ATIReg2 << 8) | 0xb2);
	outw(ATIExtReg, (restore->ATIReg5 << 8) | 0xb5);
	outw(ATIExtReg, (restore->ATIReg6 << 8) | 0xb6);
	outw(ATIExtReg, (restore->ATIRegE << 8) | 0xbe);
	outw(ATIExtReg, (restore->ATIReg3 << 8) | 0xb3);
	outw(ATIExtReg, (restore->ATIReg8 << 8) | 0xb8);
	if (ATIBoard >= ATI_BOARD_PLUS) {
		outw(ATIExtReg, (restore->ATIReg9 << 8) | 0xb9);
		outw(ATIExtReg, (restore->ATIRegA6 << 8) | 0xa6);
		outw(ATIExtReg, (restore->ATIRegA7 << 8) | 0xa7);
	}

	/* Load Miscellaneous Output External Register */
	outb(0x3C2, restore->std.MiscOutReg);

	/* Load Sequence Registers 1 through 4 */
  	for (i=1; i<5;  i++) outw(0x3C4, (restore->std.Sequencer[i] << 8) | i);

	/* Restart Sequencer using Reset Register */
	outw(0x3C4, 0x0300);

	/* Load all 25 CRT Control Registers */
	/* But first, unlock CRTC registers 0 to 7 */
	outw(vgaIOBase + 4, ((restore->std.CRTC[0x11] & 0x7F) << 8) | 0x11);
	for (i=0; i<25; i++)
	      outw(vgaIOBase + 4,(restore->std.CRTC[i] << 8) | i);

	/* Reset Attribute Controller address/data flip-flop */
	(void) inb(vgaIOBase + 0x0A);

	/* Load all 20 Attribute Controller Registers */
	for (i=0; i<21; i++) {
	   (void) inb(vgaIOBase + 0x0A);
	   outb(0x3C0,i);
	   outb(0x3C0, restore->std.Attribute[i]);
	}

	/* Load all 9 Graphics Controller Registers */
	for (i=0; i<9; i++) outw(0x3CE, (restore->std.Graphics[i] << 8) | i);

	/* Load all 768 DAC Registers */
	outb(0x3C8,0x00);
	for (i=0; i<768; i++) outb(0x3C9, restore->std.DAC[i]);

	/* Reset Attribute Controller address/data flip-flop */
	(void) inb(vgaIOBase + 0x0A);

	/* Turn Attribute Controller on */
	outb(0x3C0, 0x20);
}

/*
 * ATISave --
 *      save the current video mode
 *
 * Results:
 *      pointer to the current mode record.
 *
 * Side Effects:
 *      None.
 */

static void *
ATISave(save)
vgaATIPtr save;
{
	unsigned char b2_save,b8_save;
	int i;

	TRACE(("ATISave(save=0x%x)\n", save));

	vgaIOBase = (inb(0x3cc) & 0x01) ? 0x3D0 : 0x3B0;
	
	/* Disable video */
	inb(vgaIOBase + 0x0A); /* reset flip-flop */
	outb( 0x3c0, 0x00 );

	/* Unlock ATI specials */
	outw(ATIExtReg, (((b8_save = inATI(0xb8)) & 0xC0) << 8) | 0xb8);

	if (save == NULL) {
	   save = (vgaATIPtr)Xcalloc(sizeof(vgaATIRec));
	   TRACE(("ATISave: save = 0x%x\n",save));
	}

	/*
	 * now get the fuck'in register
	 */
	save->std.MiscOutReg = inb(0x3CC);


	b2_save = inATI(0xb2);
	outw(ATIExtReg, 0x00b2);	/* segment select 0 */

	save->ATIReg0 = inATI(0xb0);
	save->ATIReg1 = inATI(0xb1);
	save->ATIReg2 = b2_save;
	save->ATIReg3 = inATI(0xb3);
	save->ATIReg5 = inATI(0xb5);
	save->ATIReg6 = inATI(0xb6);
	save->ATIReg8 = b8_save;
	save->ATIRegE = inATI(0xbe);
	if (ATIBoard >= ATI_BOARD_PLUS) {
		save->ATIReg9 = inATI(0xb9);
		save->ATIRegA6 = inATI(0xa6);
		save->ATIRegA7 = inATI(0xa7);
	}

	for (i=0; i<25; i++) {
	   outb(vgaIOBase + 4,i);
	   save->std.CRTC[i] = inb(vgaIOBase + 5);
	}
	
	for (i=0; i<21; i++) {
	   inb(vgaIOBase + 0x0A); /* reset flip-flop */
	   outb(0x3C0,i);
	   save->std.Attribute[i] = inb(0x3C1);
	}

	for (i=0; i<9;  i++) {
	   outb(0x3CE,i); save->std.Graphics[i]  = inb(0x3CF);
	}

	for (i=0; i<5;  i++) {
	   outb(0x3C4,i); save->std.Sequencer[i] = inb(0x3C5);
	}
	
	/*			 
	 * save the colorlookuptable 
	 */
	outb(0x3C6,0xFF);
	outb(0x3C7,0x00);
	for (i=0; i<768; i++) save->std.DAC[i] = inb(0x3C9); 

	/*
	 * get the character set of the first non-graphics application
	 */
	if (((save->std.Attribute[0x10] & 0x01) == 0) &&
	    (save->std.FontInfo1 == NULL)) {
	   /*
	    * Here we switch temporary to 16 color-plane-mode, to simply
	    * copy the font-info
	    *
	    * BUGALLERT: The vga's segment-select register
	    *            MUST be set appropriate !
	    */
	   save->std.FontInfo1 = (pointer)xalloc(8192);
	   inb(vgaIOBase + 0x0A); /* reset flip-flop */
	   outb(0x3C0,0x10); outb(0x3C0, 0x01); /* graphics mode */
	   outw(0x3C4, 0x0402);    /* write to plane 2 */
	   outw(0x3C4, 0x0604);    /* enable plane graphics */
	   outw(0x3CE, 0x0204);    /* read plane 2 */
	   outw(0x3CE, 0x0005);    /* write mode 0, read mode 0 */
	   outw(0x3CE, 0x0506);    /* set graphics */

	   bcopy(vgaBase, save->std.FontInfo1, 8192);

#ifndef HAS_USL_VTS
	   if (save->std.FontInfo2 == NULL) {
		save->std.FontInfo2 = (pointer)xalloc(8192);
		/* plane 3 */
		outw(0x3C4, 0x0802);
		outw(0x3C4, 0x0604);
		outw(0x3CE, 0x0304);
		outw(0x3CE, 0x0005);
		outw(0x3CE, 0x0506);
		bcopy(vgaBase, save->std.FontInfo2, 8192);
	   }

	   if (save->std.TextInfo == NULL) {
		save->std.TextInfo = (pointer)xalloc(8192);
		/* plane 1 */
		outw(0x3C4, 0x0202);
		outw(0x3C4, 0x0604);
		outw(0x3CE, 0x0104);
		outw(0x3CE, 0x0005);
		outw(0x3CE, 0x0506);
		bcopy(vgaBase, save->std.TextInfo, 4096);
		
		/* plane 0 */
		outw(0x3C4, 0x0102);
		outw(0x3C4, 0x0604);
		outw(0x3CE, 0x0004);
		outw(0x3CE, 0x0005);
		outw(0x3CE, 0x0506);
		bcopy(vgaBase, save->std.TextInfo + 4096, 4096);
	   }
#endif /* HAS_USL_VTS */
	}

	/* Enable video */
	inb(vgaIOBase + 0x0A); /* reset flip-flop */
	outb( 0x3c0, 0x20 );

	return ((void *) save);
}

/*
 * ATIInit --
 *      Handle the initialization, etc. of a screen.
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *
 */

#define TABLE_END	255	/* denotes end of extended register table */

/*
 * Various bit values for mapping the clock values in Xconfig to bit masks for
 * the various registers.
 * WARNING: The bits are carefully chosen. They must not collide.
 *
 * See the discussion at the beginning of this file.
 */

#define CLOCK_0		(0 << 2)	/* values for Reg 3C2, bits 2,3 */
#define CLOCK_1		(1 << 2)
#define CLOCK_2		(2 << 2)
#define CLOCK_3		(3 << 2)
#define CLOCK_MASK	0x0c

#define DIVIDE_BY_2	0x40	/* divide freq. by 2 */
#define DIVIDE_MASK	0xc0	/* for all of Reg 1CE, Index B8h, bits 6,7 */

#define V3V4_BIT64	0x10	/* V3 Reg 1CE, Index B2h, Bit 6; and
				   V4 Reg 1CE, Index BEh, Bit 4 */

#define V5_BE_BIT4	0x10	/* V5,Plus,XL Reg 1CE, Index BEh, bit 4 */
#define V5_B9_BIT1	0x02	/* V5,Plus,XL Reg 1CE, Index B9h, bit 1 */

static Bool
ATIInit(mode)
DisplayModePtr mode;
{
	/*
	 * We use a table lookup method for setting the extended registers.
	 * These values are taken from the Programmer's Ref. Manual and are
	 * based on Mode 62h. Support for higher resolutions on earlier boards
	 * is not complete (but I have documented what's missing).
	 */
	static unsigned char extregV3AndOrMasks[][3] = {
		{ ER_B0, 0xc1, 0x30 },	/* 30 is 38 for mode 63h */
		{ ER_B1, 0x87, 0x00 },
		{ ER_B2, 0xbe, 0x00 },	/* 00 is 40 for mode 63h */
		{ ER_B5, 0x7f, 0x00 },
		{ ER_B6, 0xe7, 0x00 },
		{ ER_B8, 0x3f, 0x00 },
		{ TABLE_END }
	};
	static unsigned char extregV4AndOrMasks[][3] = {
		{ ER_B0, 0xc1, 0x30 },	/* FIXME: 30 is 38 for mode 63h */
		{ ER_B1, 0x87, 0x00 },
		{ ER_B3, 0xaf, 0x00 },
		{ ER_B5, 0x7f, 0x00 },
		{ ER_B6, 0xe7, 0x00 },
		{ ER_B8, 0x3f, 0x00 },
		{ ER_BE, 0xe5, 0x08 },
		{ TABLE_END }
	};
	static unsigned char extregV5AndOrMasks[][3] = {
		{ ER_B0, 0xc1, 0x30 },	/* FIXME: 30 is 38 for mode 63h */
		{ ER_B1, 0x87, 0x00 },
		{ ER_B3, 0xaf, 0x00 },
		{ ER_B5, 0x7f, 0x00 },
		{ ER_B6, 0xe7, 0x00 },
		{ ER_B8, 0x3f, 0x00 },
		{ ER_B9, 0xfd, 0x00 },
		{ ER_BE, 0xe5, 0x08 },
		{ TABLE_END }
	};
	/*
	 * Plus and XL Extended Registers ...
	 *
	 * The Programmer's Reference Manual has lots of contradictions. Sigh.
	 * ER_B0: Page 16-5 says AND mask is 0xd8. Page 16-33 has 0xd1.
	 * ER_B8: Page 16-5 has an AND mask of 0x7f which is wrong.
	 * ER_B9: Page 16-5 has an AND mask of 0xff which is wrong.
	 * ER_BE: Page 16-5 has an AND mask of 0xf5 which is wrong.
	 */
	static unsigned char extregPlusXLAndOrMasks[][3] = {
	   /* Changed ER_B0 entry from 0x28 to 0x31, Wed Feb  3 09:22:13 1993,
	      based on patch by ruhtra@turing.toronto.edu (Arthur Tateishi). */
		{ ER_B0, 0xd9, 0x31 },	/* Experiment: 28 was 20 */
		{ ER_B1, 0x87, 0x00 },
		{ ER_B3, 0xaf, 0x00 },
		{ ER_B5, 0x7f, 0x00 },
		{ ER_B6, 0xe2, 0x04 },
		{ ER_B8, 0x3f, 0x00 },
		{ ER_B9, 0xfd, 0x00 },
		{ ER_BE, 0xe5, 0x08 },
		{ ER_A6, 0xfe, 0x01 },	/* Experiment */
		{ ER_A7, 0xf4, 0x08 },
		{ TABLE_END }
	};
	static unsigned char extregIndices[11] = {
		/* These values must coincide with the ER_x values. */
		0xb0, 0xb1, 0xb2, 0xb3, 0xb5, 0xb6, 0xb8, 0xb9, 0xbe,
		0xa6, 0xa7
	};
	static unsigned char clockV3V4Table[] = {
		/* clock values: 18 22 25 28 36 44 50 56 */
		V3V4_BIT64 | CLOCK_3 | DIVIDE_BY_2, /*18*/
		0          | CLOCK_3 | DIVIDE_BY_2, /*22*/
		0          | CLOCK_0 | DIVIDE_BY_2, /*25*/
		0          | CLOCK_1 | DIVIDE_BY_2, /*28*/
		V3V4_BIT64 | CLOCK_3,               /*36*/
		0          | CLOCK_3,               /*44*/
		0          | CLOCK_0,               /*50*/
		0          | CLOCK_1                /*56*/
	};
	static unsigned char clockV5PlusXLTable[] = {
		/* clock values: 18 22 25 28 36 44 50 56
		                 30 32 37 39 40 56 75 65 */
		V5_BE_BIT4 | 0          | CLOCK_3 | DIVIDE_BY_2, /*18*/
		V5_BE_BIT4 | V5_B9_BIT1 | CLOCK_3 | DIVIDE_BY_2, /*22*/
		V5_BE_BIT4 | V5_B9_BIT1 | CLOCK_0 | DIVIDE_BY_2, /*25*/
		V5_BE_BIT4 | V5_B9_BIT1 | CLOCK_1 | DIVIDE_BY_2, /*28*/
		V5_BE_BIT4 | 0          | CLOCK_3,               /*36*/
		V5_BE_BIT4 | V5_B9_BIT1 | CLOCK_3,               /*44*/
		V5_BE_BIT4 | V5_B9_BIT1 | CLOCK_0,               /*50*/
		V5_BE_BIT4 | V5_B9_BIT1 | CLOCK_1,               /*56*/
		0          | 0          | CLOCK_0,               /*30*/
		0          | 0          | CLOCK_1,               /*32*/
		0          | 0          | CLOCK_2,               /*37*/
		0          | 0          | CLOCK_3,               /*39*/
		0          | V5_B9_BIT1 | CLOCK_0,               /*40*/
		0          | V5_B9_BIT1 | CLOCK_1,               /*56*/
		0          | V5_B9_BIT1 | CLOCK_2,               /*75*/
		0          | V5_B9_BIT1 | CLOCK_3                /*65*/
	};
	int i,fd;
	unsigned char byte_42,byte_43,byte_44;
	unsigned char (*extregTabPtr)[3];
	unsigned char *clockTabPtr;
	vgaATIPtr new;	/* was: #define new ((vgaATIPtr)vgaNewVideoState)
				- made a variable to ease debugging */

	TRACE(("ATIInit(mode=0x%x)\n", mode));

	/*
	 * Set the default values ...
	 */

	if (!vgaHWInit(mode, sizeof(vgaATIRec)))
	  return(FALSE);

	new = (vgaATIPtr) vgaNewVideoState;	/* must do after vgaHWInit() */

	/*
	 * On with ATI specific stuff ...
	 */

	switch (ATIBoard) {
	case ATI_BOARD_V3 :
		extregTabPtr = extregV3AndOrMasks;
		clockTabPtr = clockV3V4Table;
		break;
	case ATI_BOARD_V4 :
		extregTabPtr = extregV4AndOrMasks;
		clockTabPtr = clockV3V4Table;
		break;
	case ATI_BOARD_V5 :
		extregTabPtr = extregV5AndOrMasks;
		clockTabPtr = clockV5PlusXLTable;
		break;
	case ATI_BOARD_PLUS :
	case ATI_BOARD_XL :
	default :
		extregTabPtr = extregPlusXLAndOrMasks;
		clockTabPtr = clockV5PlusXLTable;
		break;
	}

	/*
	 * The VGA Wonder/XL has a bit that says "multiply vertical values by
	 * 2". This applies to CRTC[8,10h,12h,15h,18h]. At least that's what
	 * the manual says. I suspect it applies elsewhere, and that even a few
	 * of these values may be wrong. Unfortunately, their 1024x768 examples
	 * don't set this bit, so what is going on?
	 *
	 * We use this feature when the vertical display size is > 480. This
	 * means that odd values in the vertical timing field in Xconfig are
	 * truncated (modulo 2).
	 *
	 * I've duplicated all of vgaHWInit()'s setting of CRTC for
	 * completeness. Down the road we may have to add more stuff, so I've
	 * brought it all in here just in case.
	 */

	if (mode->VDisplay > 480) {
		int shift = 1 + ((ATIBoard > ATI_BOARD_V3) &&
				 (mode->Flags & V_INTERLACE));

		mode->VDisplay >>= shift;
		mode->VSyncStart >>= shift;
		mode->VSyncEnd >>= shift;
		mode->VTotal >>= shift;

		new->std.CRTC[0]  = (mode->HTotal >> 3) - 5;
		new->std.CRTC[1]  = (mode->HDisplay >> 3) - 1;
		new->std.CRTC[2]  = (mode->HSyncStart >> 3) - 1;
		new->std.CRTC[3]  = ((mode->HSyncEnd >> 3) & 0x1f) | 0x80;
		new->std.CRTC[4]  = (mode->HSyncStart >> 3);
		new->std.CRTC[5]  = (((mode->HSyncEnd >> 3) & 0x20) << 2)
			| (((mode->HSyncEnd >> 3)) & 0x1f);
		new->std.CRTC[6]  = (mode->VTotal - 2) & 0xff;
		new->std.CRTC[7]  = (((mode->VTotal - 2) & 0x100) >> 8)
			| (((mode->VDisplay - 1) & 0x100) >> 7)
			| ((mode->VSyncStart & 0x100) >> 6)
			| (((mode->VSyncStart) & 0x100) >> 5)
			| 0x10
			| (((mode->VTotal - 2) & 0x200) >> 4)
			| (((mode->VDisplay - 1) & 0x200) >> 3)
			| ((mode->VSyncStart & 0x200) >> 2);
		new->std.CRTC[8]  = 0x00;
		new->std.CRTC[9]  = ((mode->VSyncStart & 0x200) >> 4 ) | 0x40;
		new->std.CRTC[10] = 0x00;
		new->std.CRTC[11] = 0x00;
		new->std.CRTC[12] = 0x00;
		new->std.CRTC[13] = 0x00;
		new->std.CRTC[14] = 0x00;
		new->std.CRTC[15] = 0x00;
		new->std.CRTC[16] = mode->VSyncStart & 0xff;
		new->std.CRTC[17] = (mode->VSyncEnd & 0x0f) | 0x20;
		new->std.CRTC[18] = (mode->VDisplay -1) & 0xff;
		new->std.CRTC[19] = vga256InfoRec.virtualX >> 4;
		new->std.CRTC[20] = 0x00;
		new->std.CRTC[21] = mode->VSyncStart & 0xff; 
		new->std.CRTC[22] = (mode->VSyncStart + 1) & 0xff;
		new->std.CRTC[23] = 0xe7; /* bit 0x04 sets "*2 vert. values" */
		new->std.CRTC[24] = 0xff;

		mode->VDisplay <<= shift;
		mode->VSyncStart <<= shift;
		mode->VSyncEnd <<= shift;
		mode->VTotal <<= shift;
	} else {
		new->std.CRTC[23] = 0xe3;	/* ATI uses an even/odd bank
							256 color mode */
	}

	new->std.Sequencer[4] = 0x0a;
	new->std.Graphics[5] = 0x00;
	new->std.Attribute[16] = 0x01;

	/*
	 * Experiment. It looks like MiscOutReg is a little different for
	 * the high resolution modes. I've duplicated the code from
	 * vgaHWInit() because it looks like they have different handling
	 * if the syncs are explicitly mentioned, and I want to preserve
	 * this.

	if ((mode->Flags & (V_PHSYNC | V_NHSYNC))
		&& (mode->Flags & (V_PVSYNC | V_NVSYNC)))
	{
		new->MiscOutReg = 0x23;
		if (mode->Flags & V_NHSYNC) new->MiscOutReg |= 0x40;
		if (mode->Flags & V_NVSYNC) new->MiscOutReg |= 0x80;
	} else {
		if      (mode->VDisplay < 400) new->MiscOutReg = 0xA3;
		else if (mode->VDisplay < 480) new->MiscOutReg = 0x63;
		else if (mode->VDisplay < 768) new->MiscOutReg = 0xE3;
		else                           new->MiscOutReg = 0xE3;
	}

	/*
	 * Get the extended register values ...
	 */

	bzero(new->ATIExtRegBank, sizeof(new->ATIExtRegBank));
	for (i = 0; extregTabPtr[i][0] != TABLE_END; i++) {
		new->ATIExtRegBank[extregTabPtr[i][0]] =
			(inATI(extregIndices[extregTabPtr[i][0]]) &
				extregTabPtr[i][1]) |
					extregTabPtr[i][2];
	}

	if ((ATIBoard > ATI_BOARD_V3) && (mode->Flags & V_INTERLACE))
		new->ATIRegE |= 0x02;	/* FIXME: doesn't work */

	/*
	 * Handle the oddballs ...
	 *
	 * It appears that bit 0 of extended register 0xb6 enables the upper
	 * 512KB. ATI's MSWindows driver sets this bit to 1, and it appears
	 * to work here as well (previously I was getting duplicate images:
	 * the lower 512KB being used twice).
	 */

	switch (ATIBoard) {
	case ATI_BOARD_V3 :
	case ATI_BOARD_V4 :
	case ATI_BOARD_V5 :
		break;
	case ATI_BOARD_PLUS :
	case ATI_BOARD_XL :
	default :
		if (ATIChipVersion < '5' || ATIChipVersion >= 'a') {
		   /* Changed ER_BE entry from 0x08 to 0x18,
		      Wed Feb  3 12:11:27 1993, based on patch by
		      aal@broue.rot.qc.ca (Alain Hebert). */
		   new->ATIExtRegBank[ER_BE] |= 0x10;
		}

		if (vga256InfoRec.videoRam == 1024)
			new->ATIReg6 |= 0x01;
		break;
	}

	/*
	 * Get the clock value ...
	 * See the discussion at the beginning of this file.
	 */

	new->std.MiscOutReg &= ~CLOCK_MASK;
	new->std.MiscOutReg |= clockTabPtr[mode->Clock] & CLOCK_MASK;

	if (ATIBoard == ATI_BOARD_V3) {
		if (clockTabPtr[mode->Clock] & V3V4_BIT64)
			new->ATIReg2 |= 0x40;
	} else if (ATIBoard == ATI_BOARD_V4) {
		if (clockTabPtr[mode->Clock] & V3V4_BIT64)
			new->ATIRegE |= 0x10;
	} else {
		if (clockTabPtr[mode->Clock] & V5_BE_BIT4)
			new->ATIRegE |= 0x10;
		if (clockTabPtr[mode->Clock] & V5_B9_BIT1)
			new->ATIReg9 |= 0x02;
	}

	new->ATIReg8 |= clockTabPtr[mode->Clock] & DIVIDE_MASK;
	return(TRUE);
}

/*
 * ATIAdjust --
 *      adjust the current video frame to display the mousecursor
 *
 * Results:
 *      nope.
 *
 * Side Effects:
 *      the display scrolls
 */

static void
ATIAdjust(x, y)
     int x, y;
{
	int Base;

	/**
	 ** TODO: Adjust the shift-count here !!!
	 **	Ferraro writes, that there may be no shift at all. But this
	 **	would kill our panning feature.
	 **/

	Base = (y * vga256InfoRec.virtualX + x + 3) >> 3;

	outw(vgaIOBase + 4, (Base & 0x00FF00) | 0x0C);
	outw(vgaIOBase + 4, ((Base & 0x00FF) << 8) | 0x0D);
}

static int 
inATI(index)
{
	outb(ATIExtReg, index);
	return inb(ATIExtReg + 1);
}

/*
 * ATIProbe() checks for an ATI card.
 *
 * Returns TRUE or FALSE on exit.
 *
 * Makes sure the following are set in vga256InofRec:
 *    chipset
 *    videoRam
 *    clocks
 *
 * Also sets:
 *    ATIBoard
 *    ATIExtREG
 *
 * References:
 *   "VGA WONDER Programmer's Reference,"
 *      ATI Technologies, 1991.
 *      Release 1.2 -- Reference #PRG28800
 *      (Part No. 10709B0412)
 *      Chapter 7: Identification and Configuration
 *
 *   George Sutty and Steve Blair's "Advanced Programmer's
 *      Guide to SuperVGAs," Brady/Simon & Schuster, 1990.
 *      Chapter 11: ATI 18800 ATI VGAWONDER
 *
 *   Some random document from the ATI BBS dated 3Jun91,
 *      which may have been called PROGINGO.DOC.
 *
 * Written by:
 * Rik Faith (faith@cs.unc.edu), Sun Aug  9 09:27:40 1992
 *
 */

#define SIGNATURE_LENGTH	9
#define BIOS_DATA_SIZE		0x50
static Bool
ATIProbe()
{
   char *signature = "761295520";
/*   const char signature[] = "761295520"; */
   const char signature_length = SIGNATURE_LENGTH;
   const int  bios_data_size = BIOS_DATA_SIZE;
   char bios_signature[SIGNATURE_LENGTH];
   char bios_data[BIOS_DATA_SIZE];
   unsigned ports[2];
   int  fd;
	 
   TRACE(("ATIProbe()\n"));

   /*
    * Set up I/O ports to be used by this card
    */
   xf86ClearIOPortList(vga256InfoRec.scrnIndex);
   xf86AddIOPorts(vga256InfoRec.scrnIndex, Num_VGA_IOPorts, VGA_IOPorts);
   
   if (vga256InfoRec.chipset) {
      if (StrCaseCmp(vga256InfoRec.chipset, ATIIdent(0)))
         return FALSE;
   } else {
      if (xf86ReadBIOS(vga256InfoRec.BIOSbase, 0x31,
			(unsigned char *)bios_signature,
			signature_length) != signature_length)
	 return FALSE;
      if (strncmp( signature, bios_signature, signature_length )) {
	 TRACE(("ATIProbe: Incorrect Signature\n"));
	 return FALSE;
      }
   }

   /* Assume an ATI card. */
   vga256InfoRec.chipset = ATIIdent(0);

   /* Now look in the BIOS for magic numbers. */
   if (xf86ReadBIOS(vga256InfoRec.BIOSbase, 0x00, (unsigned char *)bios_data, 
		     bios_data_size) != bios_data_size)
      return FALSE;

   ErrorF( "ATI BIOS Information Block:\n" );
   ErrorF( "   Signature code:                %c%c = ",
	   bios_data[ 0x40 ], bios_data[ 0x41 ] );
   if (bios_data[ 0x40 ] != '3') ErrorF( "Unknown\n" );
   else switch( bios_data[ 0x41 ] ) {
   case '1':
      ErrorF( "VGA WONDER\n" );
      break;
   case '2':
      ErrorF( "EGA WONDER800+\n" );
      break;
   case '3':
      ErrorF( "VGA BASIC-16\n" );
      break;
   default:
      ErrorF( "Unknown\n" );
      break;
   }
   ErrorF( "   Chip version:                  %c =  ",
	   bios_data[ 0x43 ] );
   switch (bios_data[ 0x43 ] ) {
   case '1':
      ErrorF( "ATI 18800\n" );
      break;
   case '2':
      ErrorF( "ATI 18800-1\n" );
      break;
   case '3':
      ErrorF( "ATI 28800-2\n" );
      break;
   case '4': case '5':
      ErrorF( "ATI 28800-%c\n", bios_data[ 0x43 ] );
      break;
   case 'a':
      ErrorF( "Mach-32\n" );
      break;
   default:
      ErrorF( "Unknown\n" );
      break;
   }
   ErrorF( "   BIOS version:                  %d.%d\n",
	   bios_data[ 0x4c ], bios_data[ 0x4d ] );


#if DEBUG_PROBE
   ErrorF( "\n" );
#endif
   
   ErrorF( "   Byte at offset 0x42 =          0x%02x\n",
	   bios_data[ 0x42 ] );

#if DEBUG_PROBE
   ErrorF( "   8 and 16 bit ROM supported:    %s\n",
	   bios_data[ 0x42 ] & 0x01 ? "Yes" : "No" );
   ErrorF( "   Mouse chip present:            %s\n",
	   bios_data[ 0x42 ] & 0x02 ? "Yes" : "No" );
   ErrorF( "   Inport compatible mouse port : %s\n",
	   bios_data[ 0x42 ] & 0x04 ? "Yes" : "No" );
   ErrorF( "   Micro Channel supported:       %s\n",
	   bios_data[ 0x42 ] & 0x08 ? "Yes" : "No" );
   ErrorF( "   Clock chip present:            %s\n",
	   bios_data[ 0x42 ] & 0x10 ? "Yes" : "No" );
   ErrorF( "   Uses c000:0 to d000:ffff:      %s\n",
	   bios_data[ 0x42 ] & 0x80 ? "Yes" : "No" );
   ErrorF( "\n" );
#endif
   
   ErrorF( "   Byte at offset 0x44 =          0x%02x\n",
	  bios_data[ 0x44 ] );

#if DEBUG_PROBE
   ErrorF( "   Supports 70Hz non-interlaced:  %s\n",
	   bios_data[ 0x44 ] & 0x01 ? "No" : "Yes" );
   ErrorF( "   Supports Korean characters:    %s\n",
	   bios_data[ 0x44 ] & 0x02 ? "Yes" : "No" );
   ErrorF( "   Uses 45Mhz memory clock:       %s\n",
	   bios_data[ 0x44 ] & 0x04 ? "Yes" : "No" );
   ErrorF( "   Supports zero wait states:     %s\n",
	   bios_data[ 0x44 ] & 0x08 ? "Yes" : "No" );
   ErrorF( "   Uses paged ROMS:               %s\n",
	   bios_data[ 0x44 ] & 0x10 ? "Yes" : "No" );
   ErrorF( "   8514/A hardware on board:      %s\n",
	   bios_data[ 0x44 ] & 0x40 ? "No" : "Yes" );
   ErrorF( "   32K color DAC on board:        %s\n",
	   bios_data[ 0x44 ] & 0x80 ? "Yes" : "No" );
#endif

   ATIExtReg = *((short int *)bios_data + 0x08);
   ports[0] = ATIExtReg;
   ports[1] = ATIExtReg+1;
   xf86AddIOPorts(vga256InfoRec.scrnIndex, 2, ports);

   ATIChipVersion = bios_data[ 0x43 ];
   
   switch( bios_data[ 0x43 ] ) {
   case '1':
      ATIBoard = ATI_BOARD_V3;
      break;
   case '2':
      if (bios_data[ 0x42 ] & 0x10) ATIBoard = ATI_BOARD_V5;
      else ATIBoard = ATI_BOARD_V4;
      break;
   case '3':
   case '4':
      ATIBoard = ATI_BOARD_PLUS;
      break;
   case '5':
   case 'a':			/* Graphics Ultra Plus */
   default:
      if (bios_data[ 0x44 ] & 0x80) ATIBoard = ATI_BOARD_XL;
      else ATIBoard = ATI_BOARD_PLUS;
   }

   ErrorF( "\n  This video adapter is a:        " );
   switch (ATIBoard) {
   case ATI_BOARD_V3:   ErrorF( "VGA WONDER V3\n" ); break;
   case ATI_BOARD_V4:   ErrorF( "VGA WONDER V4\n" ); break;
   case ATI_BOARD_V5:   ErrorF( "VGA WONDER V5\n" ); break;
   case ATI_BOARD_PLUS: ErrorF( "VGA WONDER PLUS (V6)\n" ); break;
   case ATI_BOARD_XL:   ErrorF( "VGA WONDER XL (V7)\n" ); break;
   default:             ErrorF( "Unknown\n" ); break;
   }
      
   ATIEnterLeave( ENTER );

   if (!vga256InfoRec.videoRam) {
      if (bios_data[ 0x43 ] <= '2') {
	 outb( ATIExtReg, 0xbb );
	 vga256InfoRec.videoRam = (inb( ATIExtReg + 1 ) & 0x20) ? 512 : 256;
      } else {
	 outb( ATIExtReg, 0xb0 );
	 switch (inb( ATIExtReg + 1 ) & 0x18) {
	 case 0x00:
	    vga256InfoRec.videoRam = 256;
	    break;
	 case 0x10:
	    vga256InfoRec.videoRam = 512;
	    break;
	 case 0x08:
	    vga256InfoRec.videoRam = 1024;
	    break;
	 }
      }
   }

   ErrorF( "  Amount of RAM on video adapter: %dk\n",
	   vga256InfoRec.videoRam );
   if (bios_data[ 0x43 ] < '4') {
      ErrorF( "  WARNING: Driver may not work correctly with your board!\n" );
   }
   ErrorF( "\n" );
   
#if DEBUG_PROBE
   ErrorF( "\nThe extended registers (ATIExtREG) are at 0x%x\n", ATIExtReg );
   ErrorF( "The x386 ATI driver will set ATIBoard =   %d\n", ATIBoard );
#endif

   if (vga256InfoRec.videoRam < 512) {
      ErrorF( "ATI driver requires at least 512k video RAM!\n" );
      ATIEnterLeave( LEAVE );
      return FALSE;
   }

   if (!vga256InfoRec.clocks) {
      ErrorF( "ATI driver requires \"clocks\" to be set in Xconfig!\n" );
      ErrorF( "Clocks 18 22 25 28 36 44 50 56\n" );
      ErrorF( "       30 32 37 39 40  0 75 65\n" );
      ATIEnterLeave( LEAVE );
      return FALSE;
   }

   vga256InfoRec.bankedMono = FALSE;
   return TRUE;
}

/*
 * ATIEnterLeave()
 *
 * This routine is used when entering or leaving X (i.e., when starting or
 * exiting an X session, or when switching to or from a vt which does not
 * have an X session running.
 *
 * Assumptions:
 *   ATIProbe() has been called first, so that ATIExtReg is valid.
 *
 * Written by:
 * Rik Faith (faith@cs.unc.edu), Sun Aug  9 13:50:33 1992
 *
 */

static void
ATIEnterLeave( enter )
   Bool enter;
{
  unsigned char temp;

  TRACE(("ATIEnterLeave(%d)\n",enter));

  if (enter)
    {
      xf86EnableIOPorts(vga256InfoRec.scrnIndex);
      vgaIOBase = (inb(0x3CC) & 0x01) ? 0x3D0 : 0x3B0;
    }
  else
    {
      xf86DisableIOPorts(vga256InfoRec.scrnIndex);
    }
}

