/*
 * $XFree86: mit/server/ddx/x386/vga256/drivers/tvga8900/driver.c,v 2.10 1993/09/22 15:48:20 dawes Exp $
 * Copyright 1992 by Alan Hourihane, Wigan, England.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Alan Hourihane not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Alan Hourihane makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * ALAN HOURIHANE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL ALAN HOURIHANE BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Alan Hourihane, alanh@logitek.co.uk, version 0.1beta
 * 	    David Wexelblat, added ClockSelect logic. version 0.2.
 *	    Alan Hourihane, tweaked Init code (5 reg hack). version 0.2.1.
 *	    Alan Hourihane, removed ugly tweak code. version 0.3
 *          		    changed vgaHW.c to accomodate changes.
 * 	    Alan Hourihane, fix Restore called incorrectly. version 0.4
 *
 *	    Alan Hourihane, sent to x386beta team, version 1.0
 *
 *	    David Wexelblat, edit for comments.  Support 8900C only, dual
 *			     bank mode.  version 2.0
 *
 *	    Alan Hourihane, move vgaHW.c changes here for now. version 2.1
 *	    David Wexelblat, fix bank restoration. version 2.2
 *	    David Wexelblat, back to single bank mode.  version 2.3
 *	    Alan Hourihane, fix monochrome text restoration. version 2.4
 *	    Gertjan Akkerman, add TVGA9000 hacks.  version 2.5
 *
 *	    David Wexelblat, massive rewrite to support 8800CS, 8900B,
 *			     8900C, 8900CL, 9000.  Support 512k and 1M.
 *			     Version 3.0.
 */

#include "X.h"
#include "input.h"
#include "screenint.h"

#include "compiler.h"

#include "x386.h"
#include "x386Priv.h"
#include "xf86_OSlib.h"
#include "xf86_HWlib.h"
#define XCONFIG_FLAGS_ONLY
#include "xf86_Config.h"
#include "vga.h"

typedef struct {
	vgaHWRec std;          		/* std IBM VGA register 	*/
	unsigned char ConfPort;		/* For memory selection 	*/
	unsigned char OldMode2;		/* To enable memory banks 	*/
	unsigned char OldMode1;		/* For clock select		*/
	unsigned char NewMode2;		/* For clock select 		*/
	unsigned char NewMode1;		/* For write bank select 	*/
	unsigned char CRTCModuleTest;	/* For interlace mode 		*/
} vgaTVGA8900Rec, *vgaTVGA8900Ptr;

static Bool TVGA8900ClockSelect();
static char *TVGA8900Ident();
static Bool TVGA8900Probe();
static void TVGA8900EnterLeave();
static Bool TVGA8900Init();
static void *TVGA8900Save();
static void TVGA8900Restore();
static void TVGA8900Adjust();
extern void TVGA8900SetRead();
extern void TVGA8900SetWrite();
extern void TVGA8900SetReadWrite();

vgaVideoChipRec TVGA8900 = {
  TVGA8900Probe,
  TVGA8900Ident,
  TVGA8900EnterLeave,
  TVGA8900Init,
  TVGA8900Save,
  TVGA8900Restore,
  TVGA8900Adjust,
  NoopDDA,
  NoopDDA,
  NoopDDA,
  TVGA8900SetRead,
  TVGA8900SetWrite,
  TVGA8900SetReadWrite,
  0x10000,
  0x10000,
  16,
  0xffff,
  0x00000, 0x10000,
  0x00000, 0x10000,
  FALSE,                               /* Uses a single bank */
  VGA_DIVIDE_VERT,
  {0,},
  8,				/* Set to 16 for 512k cards in Probe() */
};

#define new ((vgaTVGA8900Ptr)vgaNewVideoState)

#define TVGA8800CS	0
#define TVGA8900B	1
#define TVGA8900C	2
#define TVGA8900CL	3
#define TVGA9000	4

static int TVGAchipset;

/*
 * TVGA8900Ident --
 */
static char *
TVGA8900Ident(n)
	int n;
{
	static char *chipsets[] = {"tvga8800cs", "tvga8900b", "tvga8900c", 
			     	   "tvga8900cl", "tvga9000"};

	if (n + 1 > sizeof(chipsets) / sizeof(char *))
		return(NULL);
	else
		return(chipsets[n]);
}

/*
 * TVGA8900ClockSelect --
 * 	select one of the possible clocks ...
 */
static Bool
TVGA8900ClockSelect(no)
	int no;
{
	static unsigned char save1, save2, save3;
	unsigned char temp;

	/*
	 * CS0 and CS1 are in MiscOutReg
	 *
	 * For 8900B, 8900C, 8900CL and 9000, CS2 is bit 0 of
	 * New Mode Control Register 2.
	 *
	 * For 8900CL, CS3 is bit 4 of Old Mode Control Register 1.
	 *
	 * For 9000, CS3 is bit 6 of New Mode Control Register 2.
	 */
	switch(no)
	{
	case CLK_REG_SAVE:
		save1 = inb(0x3CC);
		if (TVGAchipset != TVGA8800CS)
		{
			outw(0x3C4, 0x000B);	/* Switch to Old Mode */
			inb(0x3C5);		/* Now to New Mode */
			outb(0x3C4, 0x0D); save2 = inb(0x3C5);
			if ((TVGAchipset == TVGA8900CL) ||
			    (OFLG_ISSET(OPTION_16CLKS,&vga256InfoRec.options)))
			{
				outw(0x3C4, 0x000B);	/* Switch to Old Mode */
				outb(0x3C4, 0x0E); save3 = inb(0x3C5);
			}
		}
		break;
	case CLK_REG_RESTORE:
		outb(0x3C2, save1);
		if (TVGAchipset != TVGA8800CS)
		{
			outw(0x3C4, 0x000B);	/* Switch to Old Mode */
			inb(0x3C5);		/* Now to New Mode */
			outw(0x3C4, 0x0D | (save2 << 8)); 
			if ((TVGAchipset == TVGA8900CL) ||
			    (OFLG_ISSET(OPTION_16CLKS,&vga256InfoRec.options)))
			{
				outw(0x3C4, 0x000B);	/* Switch to Old Mode */
				outw(0x3C4, 0x0E | (save3 << 8));
			}
		}
		break;
	default:
		/*
		 * Do CS0 and CS1
		 */
		temp = inb(0x3CC);
		outb(0x3C2, (temp & 0xF3) | ((no << 2) & 0x0C));

		if (TVGAchipset != TVGA8800CS)
		{
			/* 
		 	 * Go to New Mode for CS2 and TVGA9000 CS3.
		 	 */
			outw(0x3C4, 0x000B);	/* Switch to Old Mode */
			inb(0x3C5);		/* Now to New Mode */
			outb(0x3C4, 0x0D);
			/*
			 * Bits 1 & 2 are dividers - set to 0 to get no
			 * clock division.
			 */
			temp = inb(0x3C5) & 0xF8;
			temp |= (no & 0x04) >> 2;
			if (TVGAchipset == TVGA9000)
			{
				temp &= ~0x40;
				temp |= (no & 0x08) << 3;
			}
			outb(0x3C5, temp);
			if ((TVGAchipset == TVGA8900CL) ||
			    (OFLG_ISSET(OPTION_16CLKS,&vga256InfoRec.options)))
			{
				/* 
				 * Go to Old Mode for CS3.
				 */
				outw(0x3C4, 0x000B);	/* Switch to Old Mode */
				outb(0x3C4, 0x0E);
				temp = inb(0x3C5) & 0xEF;
				temp |= (no & 0x08) << 1;
				outb(0x3C5, temp);
			}
		}
	}
	return(TRUE);
}

/* 
 * TVGA8900Probe --
 * 	check up whether a Trident based board is installed
 */
static Bool
TVGA8900Probe()
{
  	int numClocks;
  	unsigned char temp;

	/*
         * Set up I/O ports to be used by this card
	 */
	xf86ClearIOPortList(vga256InfoRec.scrnIndex);
	xf86AddIOPorts(vga256InfoRec.scrnIndex, Num_VGA_IOPorts, VGA_IOPorts);

  	if (vga256InfoRec.chipset)
    	{
		/*
		 * If chipset from Xconfig matches...
		 */
		if (!StrCaseCmp(vga256InfoRec.chipset, "tvga8900"))
		{
			ErrorF("\ntvga8900 is no longer valid.  Use one of\n");
			ErrorF("the names listed by the -showconfig option\n");
			return(FALSE);
		}
		if (!StrCaseCmp(vga256InfoRec.chipset, TVGA8900Ident(0)))
			TVGAchipset = TVGA8800CS;
		else if (!StrCaseCmp(vga256InfoRec.chipset, TVGA8900Ident(1)))
			TVGAchipset = TVGA8900B;
		else if (!StrCaseCmp(vga256InfoRec.chipset, TVGA8900Ident(2)))
			TVGAchipset = TVGA8900C;
		else if (!StrCaseCmp(vga256InfoRec.chipset, TVGA8900Ident(3)))
			TVGAchipset = TVGA8900CL;
		else if (!StrCaseCmp(vga256InfoRec.chipset, TVGA8900Ident(4)))
			TVGAchipset = TVGA9000;
		else
			return(FALSE);
		TVGA8900EnterLeave(ENTER);
    	}
  	else
    	{
      		unsigned char origVal, newVal;
      		char *TVGAName;
	
      		TVGA8900EnterLeave(ENTER);

      		/* 
       		 * Check first that we have a Trident card.
       		 */
		outw(0x3C4, 0x000B);	/* Switch to Old Mode */
		inb(0x3C5);		/* Now to New Mode */
      		outb(0x3C4, 0x0E);
      		origVal = inb(0x3C5);
      		outb(0x3C5, 0x00);
      		newVal = inb(0x3C5) & 0x0F;
      		outb(0x3C5, (origVal ^ 0x02));

		/* 
		 * Is it a Trident card ?? 
		 */
      		if (newVal != 2)
		{
			/*
			 * Nope, so quit
			 */
	  		TVGA8900EnterLeave(LEAVE);
	  		return(FALSE);
		}

      		/* 
		 * We've found a Trident card, now check the version.
		 */
		TVGAchipset = -1;
      		outw(0x3C4, 0x000B);
      		temp = inb(0x3C5);
      		switch (temp)
      		{
		case 0x01:
			TVGAName = "TVGA8800BR";
			break;
      		case 0x02:
			TVGAchipset = TVGA8800CS;
      			TVGAName = "TVGA8800CS";
      			break;
      		case 0x03:
			TVGAchipset = TVGA8900B;
      			TVGAName = "TVGA8900B";
      			break;
      		case 0x04:
      		case 0x13:
			TVGAchipset = TVGA8900C;
      			TVGAName = "TVGA8900C";
      			break;
      		case 0x33:
			TVGAchipset = TVGA8900CL;
      			TVGAName = "TVGA8900CL";
      			break;
      		case 0x23:
			TVGAchipset = TVGA9000;
      			TVGAName = "TVGA9000";
      			break;
      		default:
      			TVGAName = "UNKNOWN";
      		}
      		ErrorF("%s Trident chipset version: 0x%02x (%s)\n", 
                       XCONFIG_PROBED, temp, TVGAName);
		if (TVGAchipset == -1)
		{
			if (temp == 0x01)
			{
				ErrorF("Cannot support 8800BR, sorry\n");
			}
			else
			{
				ErrorF("Unknown Trident chipset.\n");
			}
	  		TVGA8900EnterLeave(LEAVE);
	  		return(FALSE);
		}
    	}
 
 	/* 
	 * How much Video Ram have we got?
	 */
    	if (!vga256InfoRec.videoRam)
    	{
		unsigned char temp;

		outb(vgaIOBase + 4, 0x1F); 
		temp = inb(vgaIOBase + 5);

		switch (temp & 0x03) 
		{
		case 0: 
			vga256InfoRec.videoRam = 256; 
			break;
		case 1: 
			vga256InfoRec.videoRam = 512; 
			break;
		case 2: 
			vga256InfoRec.videoRam = 768; 
			break;
		case 3: 
			vga256InfoRec.videoRam = 1024; 
			break;
		}
     	}

	if (vga256InfoRec.videoRam != 1024)
		TVGA8900.ChipRounding = 16;

	/*
	 * If clocks are not specified in Xconfig file, probe for them
	 */
    	if (!vga256InfoRec.clocks) 
	{
		if (TVGAchipset == TVGA8800CS)
		{
			numClocks = 4;
		}
		else if ((TVGAchipset == TVGA8900B) || 
			 (TVGAchipset == TVGA8900C))
		{
			if (OFLG_ISSET(OPTION_16CLKS,&vga256InfoRec.options))
			{
				numClocks = 16;
			}
			else
			{
				numClocks = 8;
			}
		}
		else
		{
			/* 8900CL or 9000 */
			numClocks = 16;
		}
		vgaGetClocks(numClocks, TVGA8900ClockSelect); 
	}
	
	/*
	 * Get to New Mode.
	 */
      	outw(0x3C4, 0x000B);
      	inb(0x3C5);	

	vga256InfoRec.chipset = TVGA8900Ident(TVGAchipset);
	vga256InfoRec.bankedMono = TRUE;

	/* Initialize option flags allowed for this driver */
	if ((TVGAchipset == TVGA8900B) || (TVGAchipset == TVGA8900C))
	{
		OFLG_SET(OPTION_16CLKS, &TVGA8900.ChipOptionFlags);
	}
    	return(TRUE);
}

/*
 * TVGA8900EnterLeave --
 * 	enable/disable io-mapping
 */
static void
TVGA8900EnterLeave(enter)
	Bool enter;
{
  	unsigned char temp;

  	if (enter)
    	{
      		xf86EnableIOPorts(vga256InfoRec.scrnIndex);
		vgaIOBase = (inb(0x3CC) & 0x01) ? 0x3D0 : 0x3B0;
      		outb(vgaIOBase + 4, 0x11); temp = inb(vgaIOBase + 5);
      		outb(vgaIOBase + 5, temp & 0x7F);
    	}
  	else
    	{
      		xf86DisableIOPorts(vga256InfoRec.scrnIndex);
    	}
}

/*
 * TVGA8900Restore --
 *      restore a video mode
 */
static void
TVGA8900Restore(restore)
     	vgaTVGA8900Ptr restore;
{
	unsigned char temp;
	int i;

	/*
	 * Go to Old Mode.
	 */
	outw(0x3C4, 0x000B);
	
	/*
	 * Restore Old Mode Control Registers 1 & 2.
	 */
	if ((TVGAchipset == TVGA8900CL) ||
	    (OFLG_ISSET(OPTION_16CLKS,&vga256InfoRec.options)))
	{
		if (restore->std.NoClock >= 0)
		{
			outb(0x3C4, 0x0E);
			temp = inb(0x3C5) & 0xEF;
			outb(0x3C5, temp | (restore->OldMode1 & 0x10));
		}
	}
	outw(0x3C4, ((restore->OldMode2) << 8) | 0x0D); 

	/*
	 * Now go to New Mode
	 */
	outb(0x3C4, 0x0B);
	inb(0x3C5);

	/*
	 * Unlock Configuration Port Register, then restore:
	 *	Configuration Port Register 1
	 *	New Mode Control Register 2
	 *	New Mode Control Register 1
	 *	CRTC Module Testing Register
	 */
	outw(0x3C4, 0x820E);
	outb(0x3C4, 0x0C);
	temp = inb(0x3C5) & 0xDF;
	outb(0x3C5, temp | (restore->ConfPort & 0x20));
	outw(0x3C4, ((restore->NewMode1 ^ 0x02) << 8) | 0x0E);
        if (restore->std.NoClock >= 0)
		outw(0x3C4, ((restore->NewMode2) << 8) | 0x0D);
	outw(vgaIOBase + 4, ((restore->CRTCModuleTest) << 8) | 0x1E);

	/*
	 * Now restore generic VGA Registers
	 */
	vgaHWRestore(restore);
}

/*
 * TVGA8900Save --
 *      save the current video mode
 */
static void *
TVGA8900Save(save)
     	vgaTVGA8900Ptr save;
{
	unsigned char temp;
	int i;
	
	/*
	 * Get current bank
	 */
	outb(0x3C4, 0x0e); temp = inb(0x3C5);

	/*
	 * Save generic VGA registers
	 */

  	save = (vgaTVGA8900Ptr)vgaHWSave(save, sizeof(vgaTVGA8900Rec));

	/*
	 * Go to Old Mode.
	 */
	outw(0x3C4, 0x000B);

	/*
	 * Save Old Mode Control Registers 1 & 2.
	 */
	if ((TVGAchipset == TVGA8900CL) ||
	    (OFLG_ISSET(OPTION_16CLKS,&vga256InfoRec.options)))
	{
		outb(0x3C4, 0x0E); save->OldMode1 = inb(0x3C5); 
	}
	outb(0x3C4, 0x0D); save->OldMode2 = inb(0x3C5); 
	
	/*
	 * Now go to New Mode
	 */
	outb(0x3C4, 0x0B);
	inb(0x3C5);

	/*
	 * Unlock Configuration Port Register, then save:
	 *	Configuration Port Register 1
	 *	New Mode Control Register 2
	 *	New Mode Control Register 1
	 *	CRTC Module Testing Register
	 */
	outw(0x3C4, 0x820E);
	outb(0x3C4, 0x0C); save->ConfPort = inb(0x3C5);
	save->NewMode1 = temp;
	outb(0x3C4, 0x0D); save->NewMode2 = inb(0x3C5);
	outb(vgaIOBase + 4, 0x1E); save->CRTCModuleTest = inb(vgaIOBase + 5);

  	return ((void *) save);
}

/*
 * TVGA8900Init --
 *      Handle the initialization, etc. of a screen.
 */
static Bool
TVGA8900Init(mode)
    	DisplayModePtr mode;
{

#ifndef MONOVGA
	/*
	 * In 256-color mode, with less than 1M memory, the horizontal
	 * timings and the dot-clock must be doubled.  We can (and
	 * should) do the former here.  The latter must, unfortunately,
	 * be handled by the user in the Xconfig file.
	 */
	if (vga256InfoRec.videoRam != 1024)
	{
		/*
		 * Double horizontal timings.
		 */
		mode->HTotal <<= 1;
		mode->HDisplay <<= 1;
		mode->HSyncStart <<= 1;
		mode->HSyncEnd <<= 1;
		/*
		 * Initialize generic VGA registers.
		 */
		vgaHWInit(mode, sizeof(vgaTVGA8900Rec));
		/*
		 * Put horizontal numbers back, so the virtual-screen
		 * stuff doesn't get screwed up.
		 */
		mode->HTotal >>= 1;
		mode->HDisplay >>= 1;
		mode->HSyncStart >>= 1;
		mode->HSyncEnd >>= 1;
  
		/*
		 * Now do Trident-specific stuff.  This one is also
		 * affected by the x2 requirement.
		 */
		new->std.CRTC[19] = vga256InfoRec.virtualX >> 
			(mode->Flags & V_INTERLACE ? 2 : 3);
	} else {
#endif
		/*
		 * Initialize generic VGA Registers
		 */
		vgaHWInit(mode, sizeof(vgaTVGA8900Rec));

		/*
		 * Now do Trident-specific stuff.
		 */
		new->std.CRTC[19] = vga256InfoRec.virtualX >> 
			(mode->Flags & V_INTERLACE ? 3 : 4);
#ifndef MONOVGA
	}
	new->std.CRTC[20] = 0x40;
	new->std.CRTC[23] = 0xA3;
	if (vga256InfoRec.videoRam > 512)
	{
		new->OldMode2 = 0x10;
	}
	else
	{
		new->OldMode2 = 0x00;
	}
	new->NewMode1 = 0x02;
#endif

	new->CRTCModuleTest = (mode->Flags & V_INTERLACE ? 0x84 : 0x80); 
	if (vga256InfoRec.videoRam > 512)
	{
		new->ConfPort = 0x20;
	}
	else
	{
		new->ConfPort = 0x00;
	}
        if (new->std.NoClock >= 0)
	{
	  	new->NewMode2 = (new->std.NoClock & 0x04) >> 2;
		if (TVGAchipset == TVGA9000)
		{
			new->NewMode2 |= (new->std.NoClock & 0x08) << 3;
		}
		if ((TVGAchipset == TVGA8900CL) ||
	    	    (OFLG_ISSET(OPTION_16CLKS,&vga256InfoRec.options)))
		{
			new->OldMode1 = (new->std.NoClock & 0x08) << 1;
		}
	}
        return(TRUE);
}

/*
 * TVGA8900Adjust --
 *      adjust the current video frame to display the mousecursor
 */

static void 
TVGA8900Adjust(x, y)
	int x, y;
{
	unsigned char temp;
	int base;

#ifndef MONOVGA
	/* 
	 * Go see the comments in the Init function.
	 */
	if (vga256InfoRec.videoRam != 1024)
		base = (y * vga256InfoRec.virtualX + x + 1) >> 2;
	else
#endif
		base = (y * vga256InfoRec.virtualX + x + 3) >> 3;

  	outw(vgaIOBase + 4, (base & 0x00FF00) | 0x0C);
	outw(vgaIOBase + 4, ((base & 0x00FF) << 8) | 0x0D);
	outb(vgaIOBase + 4, 0x1E);
	temp = inb(vgaIOBase + 5) & 0xDF;
	if (base > 0xFFFF)
		temp |= 0x20;
	outb(vgaIOBase + 5, temp);
}


