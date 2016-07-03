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

/* $XFree86: mit/server/ddx/x386/VGADriverDoc/stub_driver.c,v 2.6 1993/10/08 15:55:13 dawes Exp $ */

/*************************************************************************/

/*
 * This is a stub SVGA driver for XFree86.  To make an actual driver,
 * all instances of 'STUB' below should be replaced with the chipset
 * name (e.g. 'ET4000').
 *
 * This one file can be used for both the color and monochrome servers.
 * Remember that the monochrome server is actually using a 16-color mode,
 * with only one bitplane active.  To distinguish between the two at
 * compile-time, use '#ifdef MONOVGA', etc.
 */

/*************************************************************************/

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
 * This header is required for drivers that implement STUBFbInit().
 */
#ifndef MONOVGA
#include "cfbfuncs.h"
#endif

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
} vgaSTUBRec, *vgaSTUBPtr;

/*
 * Forward definitions for the functions that make up the driver.  See
 * the definitions of these functions for the real scoop.
 */
static Bool     STUBProbe();
static char *   STUBIdent();
static Bool     STUBClockSelect();
static void     STUBEnterLeave();
static Bool     STUBInit();
static void *   STUBSave();
static void     STUBRestore();
static void     STUBAdjust();
static void     STUBSaveScreen();
static void     STUBGetMode();
static void	STUBFbInit();
/*
 * These are the bank select functions.  There are defined in stub_bank.s
 */
static void     STUBSetRead();
static void     STUBSetWrite();
static void     STUBSetReadWrite();

/*
 * This data structure defines the driver itself.  The data structure is
 * initialized with the functions that make up the driver and some data 
 * that defines how the driver operates.
 */
vgaVideoChipRec STUB = {
	/* 
	 * Function pointers
	 */
	STUBProbe,
	STUBIdent,
	STUBEnterLeave,
	STUBInit,
	STUBSave,
	STUBRestore,
	STUBAdjust,
	STUBSaveScreen,
	STUBGetMode,
	STUBFbInit,
	STUBSetRead,
	STUBSetWrite,
	STUBSetReadWrite,
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
	 * seperate read and write bank registers.  Almost all chipsets
	 * support two banks, and two banks are almost always faster
	 * (Trident 8900C and 9000 are odd exceptions).
	 */
	TRUE,
	/*
	 * If the chipset requires vertical timing numbers to be divided
	 * by two for interlaced modes, set this to VGA_DIVIDE_VERT.
	 */
	VGA_NO_DIVIDE_VERT,
	/*
	 * This is a dummy initialization for the set of option flags
	 * that this driver supports.  It gets filled in properly in the
	 * probe function, if the probe succeeds (assuming the driver
	 * supports any such flags).
	 */
	{0,},
	/*
	 * This determines the multiple to which the virtual width of
	 * the display must be rounded for the 256-color server.  This
	 * will normally be 8, but may be 4 or 16 for some servers.
	 */
	8,
};

/*
 * This is a convenience macro, so that entries in the driver structure
 * can simply be dereferenced with 'new->xxx'.
 */
#define new ((vgaSTUBPtr)vgaNewVideoState)

/*
 * If your chipset uses non-standard I/O ports, you need to define an
 * array of ports, and an integer containing the array size.  The
 * generic VGA ports are defined in vgaHW.c.
 */
static unsigned STUB_ExtPorts[] = { };
static int Num_STUB_ExtPorts =
	(sizeof(Stub_ExtPorts)/sizeof(Stub_ExtPorts[0]));


/*
 * STUBIdent --
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
STUBIdent(n)
int n;
{
	static char *chipsets[] = {"sdc"};

	if (n + 1 > sizeof(chipsets) / sizeof(char *))
		return(NULL);
	else
		return(chipsets[n]);
}

/*
 * STUBClockSelect --
 * 
 * This function selects the dot-clock with index 'no'.  In most cases
 * this is done my setting the correct bits in various registers (generic
 * VGA uses two bits in the Miscellaneous Output Register to select from
 * 4 clocks).  Care must be taken to protect any other bits in these
 * registers by fetching their values and masking off the other bits.
 *
 * This function returns FALSE if the passed index is invalid or if the
 * clock can't be set for some reason.
 */
static Bool
STUBClockSelect(no)
int no;
{
	static unsigned char save1;
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
		break;
	case CLK_REG_RESTORE:
		/*
		 * Here all the previously saved registers are restored.
		 */
		outb(0x3C2, save1);
		/* Any extended registers would go here */
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
	 	 * masking off bits that won't be changing, then shifting and
	 	 * masking 'no' to set the bits as appropriate.
	 	 */
	}
	return(TRUE);
}


/*
 * STUBProbe --
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
STUBProbe()
{
	/*
	 * Set up I/O ports to be used by this card.  Only do the second
	 * xf86AddIOPorts() if there are non-standard ports for this
	 * chipset.
	 */
	xf86ClearIOPortList(vga256InforRec.scrnIndex);
	xf86AddIOPorts(vga256InfoRec.scrnIndex, Num_VGA_IOPorts, VGA_IOPorts);
	xf86AddIOPorts(vga256InfoRec.scrnIndex, 
		       Num_STUB_ExtPorts, STUB_ExtPorts);

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
		if (StrCaseCmp(vga256InfoRec.chipset, STUBIdent(0)))
			return (FALSE);
      		else
			STUBEnterLeave(ENTER);
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
		STUBEnterLeave(ENTER);

		/*
		 * Here is where all of the probing code should be placed.  
		 * The best advice is to look at what the other drivers are 
		 * doing.  If you are lucky, the chipset reference will tell 
		 * how to do this.  Other resources include SuperProbe/vgadoc2,
		 * and the Ferraro book.
		 */

      		if (failed)
		{
			/*
			 * Turn things back off if the probe is going to fail.
			 * Returning FALSE implies failure, and the server
			 * will go on to the next driver.
			 */
	  		STUBEnterLeave(LEAVE);
	  		return(FALSE);
		}
    	}

	/*
	 * If the user has specified the amount of memory in the Xconfig
	 * file, we respect that setting.
	 */
  	if (!vga256InfoRec.videoRam)
    	{
		/*
		 * Otherwise, do whatever chipset-specific things are 
		 * necessary to figure out how much memory (in kBytes) is 
		 * available.
		 */
      		vga256InfoRec.videoRam = XXX;
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
      		vgaGetClocks(NUM, STUBClockSelect);
    	}

	/*
	 * It is recommended that you fill in the maximum allowable dot-clock
	 * rate for your chipset.  If you don't do this, the default of
	 * 90MHz will be used; this is likely too high for many chipsets.
	 * This is specified in KHz, so 90Mhz would be 90000 for this
	 * setting.
	 */
	vga256InfoRec.maxClock = STUB_MAX_CLOCK_IN_KHZ;

	/*
	 * Last we fill in the remaining data structures.  We specify
	 * the chipset name, using the Ident() function and an appropriate
	 * index.  We set a boolean for whether or not this driver supports
	 * banking for the Monochrome server.  And we set up a list of all
	 * the option flags that this driver can make use of.
	 */
  	vga256InfoRec.chipset = STUBIdent(0);
  	vga256InfoRec.bankedMono = FALSE;
	OFLG_SET(OPTION_FLG1, &STUB.ChipOptionFlags);
  	return(TRUE);
}

/*
 * STUBEnterLeave --
 *
 * This function is called when the virtual terminal on which the server
 * is running is entered or left, as well as when the server starts up
 * and is shut down.  Its function is to obtain and relinquish I/O 
 * permissions for the SVGA device.  This includes unlocking access to
 * any registers that may be protected on the chipset, and locking those
 * registers again on exit.
 */
static void 
STUBEnterLeave(enter)
Bool enter;
{
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
    	}
  	else
    	{
		/*
		 * Here undo what was done above.
		 */
		xf86DisableIOPorts(vga256InfoRec.scrnIndex);

      		/* Protect CRTC[0-7] */
      		outb(vgaIOBase + 4, 0x11); temp = inb(vgaIOBase + 5);
      		outb(vgaIOBase + 5, (temp & 0x7F) | 0x80);
    	}
}

/*
 * STUBRestore --
 *
 * This function restores a video mode.  It basically writes out all of
 * the registers that have previously been saved in the vgaSTUBRec data 
 * structure.
 *
 * Note that "Restore" is a little bit incorrect.  This function is also
 * used when the server enters/changes video modes.  The mode definitions 
 * have previously been initialized by the Init() function, below.
 */
static void 
STUBRestore(restore)
vgaSTUBPtr restore;
{
	/*
	 * Whatever code is needed to get things back to bank zero should be
	 * placed here.  Things should be in the same state as when the
	 * Save/Init was done.
	 */

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
}

/*
 * STUBSave --
 *
 * This function saves the video state.  It reads all of the SVGA registers
 * into the vgaSTUBRec data structure.  There is in general no need to
 * mask out bits here - just read the registers.
 */
static void *
STUBSave(save)
vgaSTUBPtr save;
{
	/*
	 * Whatever code is needed to get back to bank zero goes here.
	 */

	/*
	 * This function will handle creating the data structure and filling
	 * in the generic VGA portion.
	 */
	save = (vgaSTUBPtr)vgaHWSave(save, sizeof(vgaSTUBRec));

	/*
	 * The port I/O code necessary to read in the extended registers 
	 * into the fields of the vgaSTUBRec structure goes here.
	 */

  	return ((void *) save);
}

/*
 * STUBInit --
 *
 * This is the most important function (after the Probe) function.  This
 * function fills in the vgaSTUBRec with all of the register values needed
 * to enable either a 256-color mode (for the color server) or a 16-color
 * mode (for the monochrome server).
 *
 * The 'mode' parameter describes the video mode.  The 'mode' structure 
 * as well as the 'vga256InfoRec' structure can be dereferenced for
 * information that is needed to initialize the mode.  The 'new' macro
 * (see definition above) is used to simply fill in the structure.
 */
static Bool
STUBInit(mode)
DisplayModePtr mode;
{
	/*
	 * This will allocate the datastructure and initialize all of the
	 * generic VGA registers.
	 */
	if (!vgaHWInit(mode,sizeof(vgaSTUBRec)))
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

	return(TRUE);
}

/*
 * STUBAdjust --
 *
 * This function is used to initialize the SVGA Start Address - the first
 * displayed location in the video memory.  This is used to implement the
 * virtual window.
 */
static void 
STUBAdjust(x, y)
int x, y;
{
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
	int Base = (y * vga256InfoRec.virtualX + x) >> 3;

	/*
	 * These are the generic starting address registers.
	 */
	outw(vgaIOBase + 4, (Base & 0x00FF00) | 0x0C);
  	outw(vgaIOBase + 4, ((Base & 0x00FF) << 8) | 0x0D);

	/*
	 * Here the high-order bits are masked and shifted, and put into
	 * the appropriate extended registers.
	 */
}

/*
 * STUBSaveScreen --
 *
 * This function gets called before and after a synchronous reset is
 * performed on the SVGA chipset during a mode-changing operation.  Some
 * chipsets will reset registers that should not be changed during this.
 * If your function is one of these, then you can use this function to
 * save and restore the registers.
 *
 * Most chipsets do not require this function, and instead put 'NoopDDA'
 * in the vgaVideoChipRec structure (NoopDDA is an empty function for
 * generic use).
 */
static void
STUBSaveScreen(mode)
int mode;
{
	if (mode == SS_START)
	{
		/*
		 * Save an registers that will be destroyed by the reset
		 * into static variables.
		 */
	}
	else
	{
		/*
		 * Now restore those registers.
		 */
	}
}

/*
 * STUBGetMode --
 *
 * This function will read the current SVGA register settings and produce
 * a filled-in DisplayModeRec containing the current mode.
 *
 * Note that the is function is NOT used in XFree86 1.3, hence in a real
 * driver you should put 'NoopDDA' in the vgaVideoChipRec structure.  At
 * some point in the future, this function will be used to implement
 * interactive mode setting, and drivers will be required to supply it.
 */
static void
STUBGetMode(mode)
DisplayModePtr mode;
{
	/*
	 * Fill in the 'mode' stucture based on current register settings.
	 */
}

/*
 * STUBFbInit --
 *
 * This function is used to initialise chip-specific graphics functions.
 * It can be used to make use of the accelerated features of some chipsets.
 * For most drivers, this function is not required, and 'NoopDDA' is put
 * in the vgaVideoChipRec structure.
 */
static void
STUBFbInit()
{

	/*
	 * Fill in the fields of cfbLowlevFuncs for which there are
	 * accelerated versions.  This struct is defined in
	 * mit/server/ddx/x386/vga256/cfb.banked/cfbfuncs.h.
	 */
	cfbLowlevFuncs.fillRectSolidCopy = STUBFillRectSolidCopy;
	cfbLowlevFuncs.doBitbltCopy = STUBDoBitbltCopy;

	/*
	 * Some functions (eg, line drawing) are initialised via the
	 * cfbTEOps, cfbTEOps1Rect, cfbNonTEOps, cfbNonTEOps1Rect
	 * structs as well as in cfbLowlevFuncs.  These are of type
	 * 'struct GCFuncs' which is defined in mit/server/include/gcstruct.h.
	 */
	cfbLowlevFuncs.lineSS = STUBLineSS;
	cfbTEOps1Rect.Polylines = STUBLineSS;
	cfbTEOps.Polylines = STUBLineSS;
	cfbNonTEOps1Rect.Polylines = STUBLineSS;
	cfbNonTEOps.Polylines = STUBLineSS;

	/*
	 * If hardware cursor is supported, the vgaHWCursor struct should
	 * be filled in here.
	 */
	vgaHWCursor.Initialized = TRUE;
	vgaHWCursor.Init = STUBCursorInit;
	vgaHWCursor.Restore = STUBCursorRestore;
	vgaHWCursor.Warp = STUBCursorWarp;
	vgaHWCursor.QueryBestSize = STUBQueryBestSize;
	
}
