/*
 * Copyright 1993 Hans Oey <hans@mo.hobby.nl>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Hans Oey not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Hans Oey makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * HANS OEY DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL HANS OEY BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 */

/* $XFree86: mit/server/ddx/x386/vga256/drivers/compaq/cpq_driver.c,v 2.10 1993/10/06 14:56:03 dawes Exp $ */

/*
  This XFree86 driver is intended to blow up your screen
  and crash your disks. Other damage to your system
  may occur. If the software fails this purpose it will
  try to make random changes to program and data files.
  It is provided "as is" without express or implied warranty
  by Hans Oey. On my Compaq LTE Lite/25c it failed completely
  and only gave me a boring X screen.

  The software was based on the other XFree86 drivers. I am
  very gratefull to Thomas Roell and the XFree86 team, but
  stacking up all copyright notices for the next twenty
  years seems a waste of disk space.
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

typedef struct {
	vgaHWRec std;               /* good old IBM VGA */
	unsigned char PageRegister0;
	unsigned char PageRegister1;
	unsigned char ControlRegister0;
	unsigned char EnvironmentReg;
} vgaCOMPAQRec, *vgaCOMPAQPtr;


static Bool     COMPAQProbe();
static char *   COMPAQIdent();
static Bool     COMPAQClockSelect();
static void     COMPAQEnterLeave();
static Bool     COMPAQInit();
static void *   COMPAQSave();
static void     COMPAQRestore();
static void     COMPAQAdjust();
static void     COMPAQSaveScreen();
extern void     COMPAQSetRead();
extern void     COMPAQSetWrite();
extern void     COMPAQSetReadWrite();

vgaVideoChipRec COMPAQ = {
	COMPAQProbe,
	COMPAQIdent,
	COMPAQEnterLeave,
	COMPAQInit,
	COMPAQSave,
	COMPAQRestore,
	COMPAQAdjust,
	COMPAQSaveScreen,
	NoopDDA,
	NoopDDA,
	COMPAQSetRead,
	COMPAQSetWrite,
	COMPAQSetReadWrite,
	0x10000,
	0x08000,
	15,
	0x7FFF,
	0x00000, 0x08000,
	0x08000, 0x10000,
	TRUE,                                 /* Uses 2 banks */
	VGA_NO_DIVIDE_VERT,
	{0,},
	8,
};

#define new ((vgaCOMPAQPtr)vgaNewVideoState)

char *COMPAQIdent(n)
int n;
{
	static char *chipsets[] = {"cpq_avga"};

	if (n + 1 > sizeof(chipsets) / sizeof(char *))
		return NULL;
	else
		return chipsets[n];
}


/* 
   COMPAQClockSelect --  select one of the possible clocks ...
*/
static Bool
COMPAQClockSelect(no)
int no;
{
	static unsigned char save1;
	unsigned char temp;

	switch(no) {
	case CLK_REG_SAVE:
		save1 = inb(0x3CC);
		break;
	case CLK_REG_RESTORE:
		outb(0x3C2, save1);
		break;
	default:
		temp = inb(0x3CC);
		outb(0x3C2, (temp & 0xf3) | ((no << 2) & 0x0C));
		break;
	}
	return(TRUE);
}


/*
   COMPAQProbe() checks for a Compaq VGC
   Returns TRUE or FALSE on exit.

   Makes sure the following are set in vga256InfoRec:
     chipset
     videoRam
     clocks
*/
#define VGABIOS_START vga256InfoRec.BIOSbase
#define SIGNATURE_LENGTH 6
#define ATI_SIGNATURE_LENGTH 9
#define BUFSIZE ATI_SIGNATURE_LENGTH	/* but at least 3 bytes */

static Bool
COMPAQProbe()
{
	unsigned char SetReset;        /* Set/Reset Data */
        unsigned char Rotate;
        unsigned char EnvironmentReg;
        unsigned char BLTConf;
        unsigned char temp;

	/*
	 * Set up I/O ports to be used by this card
	 */
	xf86ClearIOPortList(vga256InfoRec.scrnIndex);
	xf86AddIOPorts(vga256InfoRec.scrnIndex, Num_VGA_IOPorts, VGA_IOPorts);

	if (vga256InfoRec.chipset) {
		if (StrCaseCmp(vga256InfoRec.chipset, COMPAQIdent(0)))
			return(FALSE);
		COMPAQEnterLeave(ENTER);
	} 
	else {
		char buf[BUFSIZE];
		char *signature = "COMPAQ";
		char *ati_signature = "761295520";

    		/* check for COMPAQ VGC */
		if (xf86ReadBIOS(VGABIOS_START, 0, (unsigned char *)buf,
				 BUFSIZE) != BUFSIZE)
			return(FALSE);
		if ((buf[0] != (char)0x55) || (buf[1] != (char)0xAA))
			return(FALSE);
		if (xf86ReadBIOS(VGABIOS_START, (buf[2] * 512) - 0x16,
				 (unsigned char *)buf,
				 SIGNATURE_LENGTH) != SIGNATURE_LENGTH)
			return(FALSE);
		if (strncmp(signature, buf, SIGNATURE_LENGTH))
			return FALSE;

		/*
		 * Now make sure it isn't an ATI card with a COMPAQ signature
		 * in the BIOS
		 */

		if (xf86ReadBIOS(VGABIOS_START, 0x31, (unsigned char *)buf,
				 ATI_SIGNATURE_LENGTH) != ATI_SIGNATURE_LENGTH)
			return(FALSE);
		if (!strncmp(ati_signature, buf, ATI_SIGNATURE_LENGTH))
			return(FALSE);

    		COMPAQEnterLeave(ENTER);

		/* at least it is some COMPAQ :-) */
		/* check for seperate SR and BLTConf registers. */
		outb(0x3ce, 0x00); SetReset = inb(0x3cf);
		outb(0x3ce, 0x03); Rotate = inb(0x3cf);
		outb(0x3ce, 0x0f); EnvironmentReg = inb(0x3cf);
		outb(0x3cf, 0x05); /* unlock */
		outb(0x3ce, 0x10); BLTConf = inb(0x3cf);
	
		if ((BLTConf & 0x0f) == SetReset) {
			/* try another pattern */
			temp = ~SetReset & 0xff;
			outw(0x3ce, temp << 8 | 0x00);	/*change Set/Rst Data*/
			outb(0x3ce, 0x10);		/*read BLTConf reg   */
			if ((inb(0x3cf) & 0x0f) == temp) {
				/* restore changed registers */
				outw(0x3ce, SetReset << 8 | 0x00);
				outw(0x3ce, EnvironmentReg << 8 | 0x0f);
				COMPAQEnterLeave(LEAVE);
				return FALSE;
			}
			/* restore */
			outw(0x3ce, SetReset << 8 | 0x00);
		}
		outw(0x3ce, EnvironmentReg << 8 | 0x0f);
	}
    
	/* Detect how much memory is installed, that's easy :-) */
	if (!vga256InfoRec.videoRam)
		vga256InfoRec.videoRam = 512;

	if (!vga256InfoRec.clocks)
		vgaGetClocks(4, COMPAQClockSelect);  /* 4? clocks available */

	vga256InfoRec.chipset = COMPAQIdent(0);
	vga256InfoRec.bankedMono = FALSE;     /* who cares ;-) */

	return TRUE;
}

/*
   COMPAQEnterLeave() -- enable/disable io-mapping

   This routine is used when entering or leaving X (i.e., when starting or
   exiting an X session, or when switching to or from a vt which does not
   have an X session running.
*/
static void COMPAQEnterLeave(enter)
Bool enter;
{
	if (enter) {
		xf86EnableIOPorts(vga256InfoRec.scrnIndex);
		vgaIOBase = (inb(0x3CC) & 0x01) ? 0x3D0 : 0x3B0; 
	} 
	else {
		xf86DisableIOPorts(vga256InfoRec.scrnIndex);
	}
}

/*
   COMPAQRestore -- restore a video mode
*/

static void COMPAQRestore(restore)
vgaCOMPAQPtr restore;
{
	if (restore->EnvironmentReg == 0x0f)
		outw(0x3ce, 0 << 8 | 0x0f);		/* lock */
	else
		outw(0x3ce, 0x05 << 8 | 0x0f);		/* unlock */

	vgaHWRestore(restore);

	/* Compaq doesn't like the sequencer reset in vgaHWRestore */
	outw(0x3ce, restore->ControlRegister0 << 8 | 0x40);
	outw(0x3ce, restore->PageRegister0 << 8 | 0x45);
	outw(0x3ce, restore->PageRegister1 << 8 | 0x46);

	/*
	 * Don't need to do any clock stuff - the bits are in MiscOutReg,
	 * which is handled by vgaHWRestore.
	 */
}

/*
  COMPAQSave -- save the current video mode
*/
static void *COMPAQSave(save)
vgaCOMPAQPtr save;
{
	unsigned char temp0, temp1, temp2, temp3;

	outb(0x3ce, 0x0f); temp0 = inb(0x3cf);       /* Environment Register */
	outb(0x3ce, 0x40); temp1 = inb(0x3cf);       /* Control Register 0 */
	outb(0x3ce, 0x45); temp2 = inb(0x3cf);       /* Page Register 0 */
	outb(0x3ce, 0x46); temp3 = inb(0x3cf);       /* Page Register 1 */

	save = (vgaCOMPAQPtr)vgaHWSave(save, sizeof(vgaCOMPAQRec));

	save->EnvironmentReg = temp0;
	save->ControlRegister0 = temp1;
	save->PageRegister0 = temp2;
	save->PageRegister1 = temp3;

	return ((void *) save);
}

/*
  COMPAQInit -- Handle the initialization of the VGAs registers
*/
static Bool COMPAQInit(mode)
DisplayModePtr mode;
{
#ifndef MONOVGA
	/* Double horizontal timings. */
	mode->HTotal <<= 1;
	mode->HDisplay <<= 1;
	mode->HSyncStart <<= 1;
	mode->HSyncEnd <<= 1;
#endif

	if (!vgaHWInit(mode,sizeof(vgaCOMPAQRec)))
		return(FALSE);
#ifndef MONOVGA
	/* Restore them, they are used elsewhere */
	mode->HTotal >>= 1;
	mode->HDisplay >>= 1;
	mode->HSyncStart >>= 1;
	mode->HSyncEnd >>= 1;
#endif

#ifndef MONOVGA
	new->std.Sequencer[0x02] = 0xff; /* write plane mask for 256 colors */
	new->std.CRTC[0x13] = vga256InfoRec.virtualX >> 3;
	new->std.CRTC[0x14] = 0x40;
#endif

#ifdef MONOVGA
	new->ControlRegister0 = 0x00;
#else
	new->ControlRegister0 = 0x01;
#endif

	new->EnvironmentReg = 0x05;
	new->PageRegister0 = 0x0;
	new->PageRegister1 = 0x0;

	return(TRUE);
}

/*
 * COMPAQAdjust --
 *      adjust the current video frame to display the mousecursor
 */

static void 
COMPAQAdjust(x, y)
     int x, y;
{
#ifdef MONOVGA
	int Base = (y * vga256InfoRec.virtualX + x + 3) >> 3;
#else
	int Base = (y * vga256InfoRec.virtualX + x + 1) >> 2;
#endif

	outw(vgaIOBase + 4, (Base & 0x00FF00) | 0x0C);
	outw(vgaIOBase + 4, ((Base & 0x00FF) << 8) | 0x0D);
}

/*
 * COMPAQSaveScreen --
 *	Save registers that can be disrupted by a synchronous reset
 */
static void
COMPAQSaveScreen(mode)
int mode;
{
	static unsigned char save1, save2, save3;

	if (mode == SS_START)
	{
		outb(0x3ce, 0x45); save1 = inb(0x3cf);  /* Page Register 0 */
		outb(0x3ce, 0x46); save2 = inb(0x3cf);  /* Page Register 1 */
		outb(0x3ce, 0x40); save3 = inb(0x3cf);  /* Control Register 0 */
	}
	else
	{
		outw(0x3ce, save3 << 8 | 0x40);
		outw(0x3ce, save2 << 8 | 0x46);
		outw(0x3ce, save1 << 8 | 0x45);
	}
}
