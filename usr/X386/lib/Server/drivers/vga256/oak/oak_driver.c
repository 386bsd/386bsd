/*
 * Copyright 1993 by David Wexelblat <dwex@goblin.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of David Wexelblat not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  David Wexelblat makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * DAVID WEXELBLAT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL DAVID WEXELBLAT BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*************************************************************************/

/*
 * This is a oak SVGA driver for XFree86.  
 *
 *  Built from Xfree86 1.3 stub file.
 * 9/1/93 Initial version Steve Goldman  sgoldman@encore.com
 *
 * This one file can be used for both the color and monochrome servers.
 * Remember that the monochrome server is actually using a 16-color mode,
 * with only one bitplane active.  To distinguish between the two at
 * compile-time, use '#ifdef MONOVGA', etc.
 */

/*************************************************************************/

/* $XFree86: mit/server/ddx/x386/vga256/drivers/oak/oak_driver.c,v 2.1 1993/10/02 07:16:14 dawes Exp $ */

/*
 * These are X and server generic header files.
 */
#include "X.h"
#include "input.h"
#include "screenint.h"

/*
 * These are XFree86-specific header files
 */
#include "compiler.h"
#include "x386.h"
#include "x386Priv.h"
#include "xf86_OSlib.h"
#include "xf86_HWlib.h"
#include "vga.h"

/*
 * Driver data structures.
 */
typedef struct {
	/*
	 * This structure defines all of the register-level information
	 * that must be stored to define a video mode for this chipset.
	 * The 'vgaHWRec' member must be first, and contains all of the
	 * standard VGA register information, as well as saved text and
	 * font data.
	 */
	vgaHWRec std;               /* good old IBM VGA */
  	/* 
	 * Any other registers or other data that the new chipset needs
	 * to be saved should be defined here.  The Init/Save/Restore
	 * functions will manipulate theses fields.  Examples of things
	 * that would go here are registers that contain bank select
	 * registers, or extended clock select bits, or extensions to 
	 * the timing registers.  Use 'unsigned char' as the type for
	 * these registers.
	 */
	unsigned char oakMisc;    	/* Misc register */
	unsigned char oakOverflow;    	/* overflow register */
	unsigned char oakHsync2;    	/* Hsync/2 start register */
	unsigned char oakOverflow2;    	/* overflow2 register */
	unsigned char oakConfig;    	/* config 67 vs. 77 */
	unsigned char oakBCompat;    	/* backward compatibility */
	unsigned char oakBank;    	/* initial bank, debug only */
} vgaOAKRec, *vgaOAKPtr;

/*
 * Forward definitions for the functions that make up the driver.  See
 * the definitions of these functions for the real scoop.
 */
static Bool     OAKProbe();
static char *   OAKIdent();
static void     OAKClockSelect();
static void     OAKEnterLeave();
static Bool     OAKInit();
extern void *   OAKSave();
extern void     OAKRestore();
static void     OAKAdjust();
static void     OAKSaveScreen();
static void     OAKGetMode();
/*
 * These are the bank select functions.  There are defined in oak_bank.s
 */
extern void     OAKSetRead();
extern void     OAKSetWrite();
extern void     OAKSetReadWrite();

/*
 * This data structure defines the driver itself.  The data structure is
 * initialized with the functions that make up the driver and some data 
 * that defines how the driver operates.
 */
vgaVideoChipRec OAK = {
	/* 
	 * Function pointers
	 */
	OAKProbe,
	OAKIdent,
	OAKEnterLeave,
	OAKInit,
	OAKSave,
	OAKRestore,
	OAKAdjust,
	NoopDDA,
	NoopDDA,
	NoopDDA,
	OAKSetRead,
	OAKSetWrite,
	OAKSetReadWrite,
	/*
	 * This is the size of the mapped memory window, usually 64k.
	 */
	0x10000,		
	/*
	 * This is the size of a video memory bank for this chipset.
	 */
	0x10000,
	/*
	 * This is the number of bits by which an address is shifted
	 * right to determine the bank number for that address.
	 */
	16,
	/*
	 * This is the bitmask used to determine the address within a
	 * specific bank.
	 */
	0xFFFF,
	/*
	 * These are the bottom and top addresses for reads inside a
	 * given bank.
	 */
	0x00000, 0x10000,
	/*
	 * And corresponding limits for writes.
	 */
	0x00000, 0x10000,
	/*
	 * Whether this chipset supports a single bank register or
	 * separate read and write bank registers.  Almost all chipsets
	 * support two banks, and two banks are almost always faster
	 * (Trident 8900C and 9000 are odd exceptions).
	 */
	TRUE, /* two banks */
	/*
	 * If the chipset requires vertical timing numbers to be divided
	 * by two for interlaced modes, set this to VGA_DIVIDE_VERT.
	 */
	VGA_NO_DIVIDE_VERT,
	/*
	 * This is a dummy initialization for the set of vendor/option flags
	 * that this driver supports.  It gets filled in properly in the
	 * probe function, if the probe succeeds (assuming the driver
	 * supports any such flags).
	 */
	{0,},
	/*
	 * This specifies how the virtual width is to be rounded.  The
	 * virtual width will be rounded down the nearest multiple of
	 * this value
	 */
	16,
};

/*
 * This is a convenience macro, so that entries in the driver structure
 * can simply be dereferenced with 'new->xxx'.
 */
#define new ((vgaOAKPtr)vgaNewVideoState)

/*
    A bunch of defines that match Oak's register names
    so we don't use a bunch of hardcoded constants in the code.
*/
#define OTI_INDEX 0x3DE			/* Oak extended index register */
#define OTI_R_W 0x3DF			/* Oak extended r/w register */
#define OTI_CRT_CNTL 0xC		/* Oak CRT COntrol Register */
#define OTI_MISC  0xD			/* Oak Misc register */
#define OTI_BCOMPAT  0xE		/* Oak Back compat register */
#define OTI_SEGMENT  0x11		/* Oak segment register */
#define OTI_CONFIG  0x12		/* Oak config register */
#define OTI_OVERFLOW  0x14		/* Oak overflow register */
#define OTI_HSYNC2  0x15		/* Oak hsync/2 start register */
#define OTI_OVERFLOW2  0x16		/* Oak overflow2 register */


#define OTI67 0   /* same index as ident function */
#define OTI77 1   /* same index as ident function */

static int OTI_chipset;

static unsigned OAK_ExtPorts[] = { OTI_INDEX, OTI_R_W };
static int Num_OAK_ExtPorts = (sizeof(OAK_ExtPorts)/sizeof(OAK_ExtPorts[0]));

/*
 * OAKIdent --
 *
 * Returns the string name for supported chipset 'n'.  Most drivers only
 * support one chipset, but multiple version may require that the driver
 * identify them individually (e.g. the Trident driver).  The Ident function
 * should return a string if 'n' is valid, or NULL otherwise.  The
 * server will call this function when listing supported chipsets, with 'n' 
 * incrementing from 0, until the function returns NULL.  The 'Probe'
 * function should call this function to get the string name for a chipset
 * and when comparing against an Xconfig-supplied chipset value.  This
 * cuts down on the number of places errors can creep in.
 */
static char *
OAKIdent(n)
int n;
{
	static char *chipsets[] = {"oti067","oti077"};

	if (n + 1 > sizeof(chipsets) / sizeof(char *))
		return(NULL);
	else
		return(chipsets[n]);
}

/*
 * OAKClockSelect --
 * 
 * This function selects the dot-clock with index 'no'.  In most cases
 * this is done my setting the correct bits in various registers (generic
 * VGA uses two bits in the Miscellaneous Output Register to select from
 * 4 clocks).  Care must be taken to protect any other bits in these
 * registers by fetching their values and masking off the other bits.
 */
static void
OAKClockSelect(no)
int no;
{
	static unsigned char save1,save2;
	unsigned char temp;

	switch(no)
	{
	case CLK_REG_SAVE:
		/*
		 * Here all of the registers that can be affected by
		 * clock setting should be saved into static variables.
		 */
		save1 = inb(0x3CC);
		/* Any extended registers would go here */
		outb(OTI_INDEX, OTI_MISC);
		save2 = inb(OTI_R_W);
		break;
	case CLK_REG_RESTORE:
		/*
		 * Here all the previously saved registers are restored.
		 */
		outb(0x3C2, save1);
		/* Any extended registers would go here */
		/* all the examples seem to just blast the old data
	           in what about any other bit changes?? */
		outw(OTI_INDEX, OTI_MISC | (save2 << 8));
		break;
	default:
		/* 
	 	 * These are the generic two low-order bits of the clock select 
		 */
		temp = inb(0x3CC);
		outb(0x3C2, ( temp & 0xF3) | ((no << 2) & 0x0C));
		/* 
	 	 * Here is where the high order bit(s) supported by the chipset 
	 	 * are set.  This is done by fetching the appropriate register,
	 	 * masking off bits that will be changing, then shifting and
	 	 * masking 'no' to set the bits as appropriate.
	 	 */
		outb(OTI_INDEX, OTI_MISC);
		temp = inb(OTI_R_W);
		outw(OTI_INDEX, OTI_MISC | 
                                ((( temp & 0xDF ) | (( no & 4) << 3)) << 8));

	}
}


/*
 * OAKProbe --
 *
 * This is the function that makes a yes/no decision about whether or not
 * a chipset supported by this driver is present or not.  The server will
 * call each driver's probe function in sequence, until one returns TRUE
 * or they all fail.
 *
 * Pretty much any mechanism can be used to determine the presence of the
 * chipset.  If there is a BIOS signature (e.g. ATI, GVGA), it can be read
 * via /dev/mem on most OSs, but some OSs (e.g. Mach) require special
 * handling, and others (e.g. Amoeba) don't allow reading  the BIOS at
 * all.  Hence, this mechanism is discouraged, if other mechanisms can be
 * found.  If the BIOS-reading mechanism must be used, examine the ATI and
 * GVGA drivers for the special code that is needed.  Note that the BIOS 
 * base should not be assumed to be at 0xC0000 (although most are).  Use
 * 'vga256InfoRec.BIOSbase', which will pick up any changes the user may
 * have specified in the Xconfig file.
 *
 * The preferred mechanism for doing this is via register identification.
 * It is important not only the chipset is detected, but also to
 * ensure that other chipsets will not be falsely detected by the probe
 * (this is difficult, but something that the developer should strive for).  
 * For testing registers, there are a set of utility functions in the 
 * "compiler.h" header file.  A good place to find example probing code is
 * in the SuperProbe program, which uses algorithms from the "vgadoc2.zip"
 * package (available on most PC/vga FTP mirror sites, like ftp.uu.net and
 * wuarchive.wustl.edu).
 *
 * Once the chipset has been successfully detected, then the developer needs 
 * to do some other work to find memory, and clocks, etc, and do any other
 * driver-level data-structure initialization may need to be done.
 */
static Bool
OAKProbe()
{
	unsigned char save, temp1;

	xf86ClearIOPortList(vga256InfoRec.scrnIndex);
	xf86AddIOPorts(vga256InfoRec.scrnIndex, Num_VGA_IOPorts, VGA_IOPorts);
	xf86AddIOPorts(vga256InfoRec.scrnIndex, Num_OAK_ExtPorts, OAK_ExtPorts);

	/*
	 * First we attempt to figure out if one of the supported chipsets
	 * is present.
	 */
	if (vga256InfoRec.chipset)
	{
		/*
		 * This is the easy case.  The user has specified the
		 * chipset in the Xconfig file.  All we need to do here
		 * is a string comparison against each of the supported
		 * names available from the Ident() function.  If this
		 * driver supports more than one chipset, there would be
		 * nested conditionals here (see the Trident and WD drivers
		 * for examples).
		 */
		if (!StrCaseCmp(vga256InfoRec.chipset, OAKIdent(0))) {
			OTI_chipset = OTI67;
		} else if (!StrCaseCmp(vga256InfoRec.chipset, OAKIdent(1))) {
			OTI_chipset = OTI77;
      		} else
			return (FALSE);
		OAKEnterLeave(ENTER);
    	}
  	else
	{
		/*
		 * OK.  We have to actually test the hardware.  The
		 * EnterLeave() function (described below) unlocks access
		 * to registers that may be locked, and for OSs that require
		 * it, enables I/O access.  So we do this before we probe,
		 * even though we don't know for sure that this chipset
		 * is present.
		 */
		OAKEnterLeave(ENTER);

		/*
		 * Here is where all of the probing code should be placed.  
		 * The best advice is to look at what the other drivers are 
		 * doing.  If you are lucky, the chipset reference will tell 
		 * how to do this.  Other resources include SuperProbe/vgadoc2,
		 * and the Ferraro book.
		 */
		
		/* First we see if the segment register is present */
		outb(OTI_INDEX, OTI_SEGMENT);
		save = inb(OTI_R_W);
		/* I assume that one I set the index I can r/w/r/w to
                   my hearts content */
                outb(OTI_R_W, save ^ 0x11);
		temp1 = inb(OTI_R_W);
		outb(OTI_R_W, save);
		if (temp1 != ( save ^ 0x11 )) {
			/*
			 * Turn things back off if the probe is going to fail.
			 * Returning FALSE implies failure, and the server
			 * will go on to the next driver.
			 */
	  		OAKEnterLeave(LEAVE);
	  		return(FALSE);
		}
		/* figure out which chipset */
		temp1 = inb(OTI_INDEX);
		temp1 &= 0xE0;
		switch (temp1) {
		    case 0xE0 : /* oti 57 don't know it */
		        ErrorF("OAK driver: OTI-57 unsupported\n");
	  		OAKEnterLeave(LEAVE);
	  		return(FALSE);
		    case 0x40 : /* oti 67 */
			OTI_chipset = OTI67;
			break;
		    case 0xA0 : /* oti 77 */
			OTI_chipset = OTI77;
			break;
		    default : /* don't know it */
		        ErrorF("OAK driver: unknown chipset\n");
	  		OAKEnterLeave(LEAVE);
	  		return(FALSE);
		}
    	}

	/*
	 * If the user has specified the amount of memory in the Xconfig
	 * file, we respect that setting.
	 */
  	if (!vga256InfoRec.videoRam) {
		/*
		 * Otherwise, do whatever chipset-specific things are 
		 * necessary to figure out how much memory (in kBytes) is 
		 * available.
		 */
		outb(OTI_INDEX, OTI_MISC);
		temp1 = inb(OTI_R_W);
		temp1 &= 0xC0;
		if (temp1 == 0xC0 )
      		    vga256InfoRec.videoRam = 1024;
		else if (temp1 == 0x80 )
      		    vga256InfoRec.videoRam = 512;
		else if (temp1 == 0x00 )
      		    vga256InfoRec.videoRam = 256;
		else {
		    ErrorF("OAK driver: unknown video memory\n");
	  	    OAKEnterLeave(LEAVE);
	  	    return(FALSE);
		}
    	}

	/*
	 * Again, if the user has specified the clock values in the Xconfig
	 * file, we respect those choices.
	 */
  	if (!vga256InfoRec.clocks)
    	{
		/*
		 * This utility function will probe for the clock values.
		 * It is passed the number of supported clocks, and a
		 * pointer to the clock-select function.
		 */
      		vgaGetClocks(8, OAKClockSelect);
    	}

	/*
	 * Last we fill in the remaining data structures.  We specify
	 * the chipset name, using the Ident() function and an appropriate
	 * index.  We set a boolean for whether or not this driver supports
	 * banking for the Monochrome server.  And we set up a list of all
	 * the vendor flags that this driver can make use of.
	 */
  	vga256InfoRec.chipset = OAKIdent(OTI_chipset);
  	vga256InfoRec.bankedMono = TRUE;
  	return(TRUE);
}

/*
 * OAKEnterLeave --
 *
 * This function is called when the virtual terminal on which the server
 * is running is entered or left, as well as when the server starts up
 * and is shut down.  Its function is to obtain and relinquish I/O 
 * permissions for the SVGA device.  This includes unlocking access to
 * any registers that may be protected on the chipset, and locking those
 * registers again on exit.
 */
static void 
OAKEnterLeave(enter)
Bool enter;
{
	static unsigned char save;
	unsigned char temp;

  	if (enter)
    	{
		xf86EnableIOPorts(vga256InfoRec.scrnIndex);

		/* 
		 * This is a global.  The CRTC base address depends on
		 * whether the VGA is functioning in color or mono mode.
		 * This is just a convenient place to initialize this
		 * variable.
		 */
      		vgaIOBase = (inb(0x3CC) & 0x01) ? 0x3D0 : 0x3B0;

		/*
		 * Here we deal with register-level access locks.  This
		 * is a generic VGA protection; most SVGA chipsets have
		 * similar register locks for their extended registers
		 * as well.
		 */

      		/* Unprotect CRTC[0-7] */
      		outb(vgaIOBase + 4, 0x11); temp = inb(vgaIOBase + 5);
      		outb(vgaIOBase + 5, temp & 0x7F);
		outb(OTI_INDEX, OTI_CRT_CNTL);
		temp = inb(OTI_R_W);
		outb(OTI_R_W, temp & 0xF0);
                save = temp;
    	}
  	else
    	{
		/*
		 * Here undo what was done above.
		 */
		outb(OTI_INDEX, OTI_CRT_CNTL);
		/* don't set the i/o write test bit even though
                   we cleared it on entry */
		outb(OTI_R_W, (save & 0xF7) );

      		/* Protect CRTC[0-7] */
      		outb(vgaIOBase + 4, 0x11); temp = inb(vgaIOBase + 5);
      		outb(vgaIOBase + 5, (temp & 0x7F) | 0x80);

		xf86DisableIOPorts(vga256InfoRec.scrnIndex);
    	}
}

/*
 * OAKRestore --
 *
 * This function restores a video mode.  It basically writes out all of
 * the registers that have previously been saved in the vgaOAKRec data 
 * structure.
 *
 * Note that "Restore" is a little bit incorrect.  This function is also
 * used when the server enters/changes video modes.  The mode definitions 
 * have previously been initialized by the Init() function, below.
 */
extern void 
OAKRestore(restore)
vgaOAKPtr restore;
{
	unsigned char temp;

	/*
	 * Whatever code is needed to get things back to bank zero should be
	 * placed here.  Things should be in the same state as when the
	 * Save/Init was done.
	 */
	/* put the segment regs back to zero */
	outw(OTI_INDEX, OTI_SEGMENT);

        /*
           Set the OTI-Misc register. We must be sure that we
           aren't in one of the extended graphics modes when
           we are leaving X and about to call vgaHWRestore for
           the last time. If we don't text mode is completely
           fouled up.
        */
	outb(OTI_INDEX, OTI_MISC);
	temp = inb(OTI_R_W) & 0x20; /* get the clock bit */
        temp |= (restore->oakMisc & 0xDF);
	outb(OTI_R_W, temp);

	/*
	 * This function handles restoring the generic VGA registers.
	 */
	vgaHWRestore(restore);

	/*
	 * Code to restore any SVGA registers that have been saved/modified
	 * goes here.  Note that it is allowable, and often correct, to 
	 * only modify certain bits in a register by a read/modify/write cycle.
	 *
	 * A special case - when using an external clock-setting program,
	 * this function must not change bits associated with the clock
	 * selection.  This condition can be checked by the condition:
	 *
	 *	if (restore->std.NoClock >= 0)
	 *		restore clock-select bits.
	 */

	outb(OTI_INDEX, OTI_SEGMENT);
        outb(OTI_R_W, restore->oakBank);

	if (restore->std.NoClock >= 0) {
		/* restore clock-select bits. */
	        outw(OTI_INDEX, OTI_MISC + (restore->oakMisc << 8));
	} else {
		/* don't restore clock-select bits. */
	        outb(OTI_INDEX, OTI_MISC);
		temp = inb(OTI_R_W) & 0x20; /* get the clock bit */
		temp |= (restore->oakMisc & 0xDF);
	        outb(OTI_R_W, temp);
	}

	outb(OTI_INDEX, OTI_BCOMPAT);
	temp = inb(OTI_R_W);
	temp &= 0xF9;
	temp |= (restore->oakBCompat & 0x6);
	outb(OTI_INDEX, OTI_CONFIG);
	temp = inb(OTI_R_W);
	temp &= 0xF7;
	temp |= (restore->oakConfig & 0x8);
	outb(OTI_R_W, temp);
	outw(OTI_INDEX, OTI_OVERFLOW + (restore->oakOverflow << 8));
	outw(OTI_INDEX, OTI_HSYNC2 + (restore->oakHsync2 << 8));

        if ( OTI_chipset == OTI77)
	    outw(OTI_INDEX, OTI_OVERFLOW2 + (restore->oakOverflow2 << 8));

  	outw(0x3C4, 0x0300); /* now reenable the timing sequencer */

}

/*
 * OAKSave --
 *
 * This function saves the video state.  It reads all of the SVGA registers
 * into the vgaOAKRec data structure.  There is in general no need to
 * mask out bits here - just read the registers.
 */
extern void *
OAKSave(save)
vgaOAKPtr save;
{
        unsigned char temp;
	/*
	 * Whatever code is needed to get back to bank zero goes here.
	 */
	outb(OTI_INDEX, OTI_SEGMENT);
        temp = inb(OTI_R_W);
        /* put segment register to zero */
        outb(OTI_R_W, 0);

	/*
	 * This function will handle creating the data structure and filling
	 * in the generic VGA portion.
	 */
	save = (vgaOAKPtr)vgaHWSave(save, sizeof(vgaOAKRec));

	/*
	 * The port I/O code necessary to read in the extended registers 
	 * into the fields of the vgaOAKRec structure goes here.
	 */

	save->oakBank = temp; /* this seems silly, leftover from textmode
	                         problems */

	outb(OTI_INDEX, OTI_MISC);
	save->oakMisc = inb(OTI_R_W);
	outb(OTI_INDEX, OTI_CONFIG);
	save->oakConfig = inb(OTI_R_W);
	outb(OTI_INDEX, OTI_BCOMPAT);
	save->oakBCompat = inb(OTI_R_W);
	outb(OTI_INDEX, OTI_OVERFLOW);
	save->oakOverflow = inb(OTI_R_W);
	outb(OTI_INDEX, OTI_HSYNC2);
	save->oakHsync2 = inb(OTI_R_W);
        if ( OTI_chipset == OTI77) {
	    outb(OTI_INDEX, OTI_OVERFLOW2);
	    save->oakOverflow2 = inb(OTI_R_W);
	}

  	return ((void *) save);
}

/*
 * OAKInit --
 *
 * This is the most important function (after the Probe) function.  This
 * function fills in the vgaOAKRec with all of the register values needed
 * to enable either a 256-color mode (for the color server) or a 16-color
 * mode (for the monochrome server).
 *
 * The 'mode' parameter describes the video mode.  The 'mode' structure 
 * as well as the 'vga256InfoRec' structure can be dereferenced for
 * information that is needed to initialize the mode.  The 'new' macro
 * (see definition above) is used to simply fill in the structure.
 */
static Bool
OAKInit(mode)
DisplayModePtr mode;
{
	if (mode->Flags & V_INTERLACE ) {
            /*
		When in interlace mode cut the vertical numbers in half.
		(Try and find that in the manual!)
		Could just set VGA_DIVIDE_VERT but we'd have to
		test it and do the divides here anyway.
            */ 
            mode->VTotal >>= 1;
            mode->VDisplay >>= 1;
            mode->VSyncStart >>= 1;
            mode->VSyncEnd >>= 1;
        }
	/*
	 * This will allocate the datastructure and initialize all of the
	 * generic VGA registers.
	 */
	if (!vgaHWInit(mode,sizeof(vgaOAKRec)))
		return(FALSE);

	/*
	 * Here all of the other fields of 'new' get filled in, to
	 * handle the SVGA extended registers.  It is also allowable
	 * to override generic registers whenever necessary.
	 *
	 * A special case - when using an external clock-setting program,
	 * this function must not change bits associated with the clock
	 * selection.  This condition can be checked by the condition:
	 *
	 *	if (new->std.NoClock >= 0)
	 *		initialize clock-select bits.
	 */
#ifndef MONOVGA
	/* new->std.CRTC[19] = vga256InfoRec.virtualX >> 3; /* 3 in byte mode */
	/* much clearer as 0x01 than 0x41, seems odd though... */
	new->std.Attribute[16] = 0x01; 

        /*
            We set the fifo depth to maximum since it seems to
            remove screen interference at high resolution. This
            could probably be set to some other value for better
            performance.
        */
        if ( new->std.NoClock >= 0 ) {
	    new->oakMisc = 0x0F | ((new->std.NoClock & 0x04) << 3);
	} else
	    new->oakMisc = 0x0F; /*  high res mode, deep fifo */
#else
        if ( new->std.NoClock >= 0 ) {
	    new->oakMisc = 0x18 | ((new->std.NoClock & 0x04) << 3);
	} else
	    new->oakMisc = 0x18; /*  16 color high res mode */
#endif
	new->oakBank = 0; 
	new->oakBCompat = 0x80; /* vga mode */
	new->oakConfig = (OTI_chipset == OTI77 ? 0x8 : 0 );
	/* set number of ram chips! */                    /* 40 */
	new->oakMisc |= (vga256InfoRec.videoRam == 1024 ? 0x40 : 0x00 );
	new->oakMisc |= (vga256InfoRec.videoRam >= 512 ? 0x80 : 0x00 );
	if (mode->Flags & V_INTERLACE ) {
	    new->oakOverflow = 0x80 |
            /* V-retrace-start */  (((mode->VSyncStart ) & 0x400) >> 8 ) |
            /* V-blank-start */    ((((mode->VDisplay-1) ) & 0x400) >> 9 ) |
            /* V-total */          ((((mode->VTotal-2) ) & 0x400) >> 10 ) ;
	    /* can set overflow2 no matter what here since restore will
               do the right thing */
	    new->oakOverflow2 = 0;
            /* Doc. says this is when vertical retrace will start in 
               every odd frame in interlaced mode in characters. Hmm??? */
	    new->oakHsync2 =  (mode->VTotal-2) >> 3; 
	} else {
	    new->oakOverflow = (mode->Flags & V_INTERLACE ? 0x80 : 0x00) |
            /* V-retrace-start */  ((mode->VSyncStart & 0x400) >> 8 ) |
            /* V-blank-start */    (((mode->VDisplay-1) & 0x400) >> 9 ) |
            /* V-total */          (((mode->VTotal-2) & 0x400) >> 10 ) ;
	    /* can set overflow2 no matter what here since restore will
               do the right thing */
	    new->oakOverflow2 = 0;
	    new->oakHsync2 = 0;
	}

        /*
             Put vertical numbers back so virtual screen doesn't
             get fooled.
        */
	if (mode->Flags & V_INTERLACE ) {
            mode->VTotal <<= 1;
            mode->VDisplay <<= 1;
            mode->VSyncStart <<= 1;
            mode->VSyncEnd <<= 1;
        }


	return(TRUE);
}

/*
 * OAKAdjust --
 *
 * This function is used to initialize the SVGA Start Address - the first
 * displayed location in the video memory.  This is used to implement the
 * virtual window.
 */
static void 
OAKAdjust(x, y)
int x, y;
{
        int temp;
	/*
	 * The calculation for Base works as follows:
	 *
	 *	(y * virtX) + x ==> the linear starting pixel
	 *
	 * This number is divided by 8 for the monochrome server, because
	 * there are 8 pixels per byte.
	 *
	 * For the color server, it's a bit more complex.  There is 1 pixel
	 * per byte.  In general, the 256-color modes are in word-mode 
	 * (16-bit words).  Word-mode vs byte-mode is will vary based on
	 * the chipset - refer to the chipset databook.  So the pixel address 
	 * must be divided by 2 to get a word address.  In 256-color modes, 
	 * the 4 planes are interleaved (i.e. pixels 0,3,7, etc are adjacent 
	 * on plane 0). The starting address needs to be as an offset into 
	 * plane 0, so the Base address is divided by 4.
	 *
	 * So:
	 *    Monochrome: Base is divided by 8
	 *    Color:
	 *	if in word mode, Base is divided by 8
	 *	if in byte mode, Base is divided by 4
	 *
	 * The generic VGA only supports 16 bits for the Starting Address.
	 * But this is not enough for the extended memory.  SVGA chipsets
	 * will have additional bits in their extended registers, which
	 * must also be set.
	 */
	int Base = (y * vga256InfoRec.virtualX + x + 3) >> 3;

	/*
	 * These are the generic starting address registers.
	 */
	outw(vgaIOBase + 4, (Base & 0x00FF00) | 0x0C);
  	outw(vgaIOBase + 4, ((Base & 0x00FF) << 8) | 0x0D);

	/*
	 * Here the high-order bits are masked and shifted, and put into
	 * the appropriate extended registers.
	 */
  	outb(OTI_INDEX, OTI_OVERFLOW);
	temp = inb(OTI_R_W);
        temp &= 0xF7;
  	temp |= ((Base & 0x10000) >> (5+8));
  	outb(OTI_R_W, temp);
        if ( OTI_chipset == OTI77) {
  	    outb(OTI_INDEX, OTI_OVERFLOW2);
	    temp = inb(OTI_R_W);
            temp &= 0xF7;
  	    temp |= ((Base & 0x20000) >> (6 + 8));
  	    outb(OTI_R_W, temp);
	}
}
