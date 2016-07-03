/*
 * BDM2: Banked dumb monochrome driver
 * Pascal Haible 9/93, haible@izfm.uni-stuttgart.de
 *
 * bdm2/driver/sigma/sigmadriv.c
 *
 * Parts derived from:
 * hga2/*
 * Author:  Davor Matic, dmatic@athena.mit.edu
 * and
 * vga256/*
 * Copyright 1990,91 by Thomas Roell, Dinkelscherben, Germany.
 *
 * see bdm2/COPYRIGHT for copyright and disclaimers.
 */

/* $XFree86: mit/server/ddx/x386/bdm2/drivers/sigma/sigmadriv.c,v 2.2 1993/10/02 09:50:52 dawes Exp $ */

#include "X.h"
#include "input.h"
#include "screenint.h"

#include "compiler.h"

#include "x386.h"
#include "x386Priv.h"
#include "xf86_OSlib.h"
#include "bdm.h"
#include "sigmaHW.h"

typedef struct {
  unsigned char en1, w16, w17, w18, blank, zoom, gr0, gr1,
		bank0, bank1, bank2, bank3, hires;
  } sigmaRec, *sigmaPtr;

sigmaRec sigmaRegsGraf1664x1152 = {
	/* en1 */	0x80,
	/* w16 */	((SIGMA_MEM_BASE_BIT16) ? 0 : 0x80),
	/* w17 */	((SIGMA_MEM_BASE_BIT17) ? 0 : 0x80),
	/* w18 */	((SIGMA_MEM_BASE_BIT18) ? 0 : 0x80),
	/* blank */	0x80,
	/* zoom */	0x80,
	/* gr0 */	0x80,
	/* gr1 */	0x80,
	/* Don't think the bank regs are really needed, but it certainly doesn't hurt */
	/* bank0 */	0,
	/* bank1 */	0,
	/* bank2 */	0,
	/* bank3 */	0,
	/* hires */	0x80
};

/*
 * Define the SIGMA I/O Ports
 */
unsigned SIGMA_IOPorts[] = { SLV_EN1, SLV_W16, SLV_W17, SLV_W18,
			SLV_BLANK, SLV_ZOOM, SLV_GR0, SLV_GR1,
			SLV_BANK0, SLV_BANK1, SLV_BANK2, SLV_BANK3,
			SLV_HIRES /* = MONOEN */, SLV_BOLD /* = WOB */ };
int Num_SIGMA_IOPorts = (sizeof(SIGMA_IOPorts)/sizeof(SIGMA_IOPorts[0]));

char *	SIGMAIdent();
Bool	SIGMAProbe();
void	SIGMAEnterLeave();
void *	SIGMAInit();
void *	SIGMASave();
void	SIGMARestore();
void	SIGMAAdjust();
Bool	SIGMASaveScreen();
void	SIGMAGetMode();

/* Assembler functions in sigmabank.s
 * - to be called by assembler functions only! */
extern void SIGMASetRead();
extern void SIGMASetWrite();
extern void SIGMASetReadWrite();

#if 0
/* From bdm.h -- see there, this here might not be up to date */
/*
 * structure for accessing the video chip`s functions
 *
 * We are doing a total encapsulation of the driver's functions.
 * Banking (bdmSetReadWrite(p) etc.) is done in bdmBank.c
 *   using the chip's function pointed to by
 *   bmpSetReadWriteFunc(bank) etc.
 */
typedef struct {
  char * (* ChipIdent)();
  Bool (* ChipProbe)();
  void (* ChipEnterLeave)();
  void * (* ChipInit)();
  void * (* ChipSave)();
  void (* ChipRestore)();
  void (* ChipAdjust)();
  Bool (* ChipSaveScreen)();
  void (* ChipGetMode)();
  /* These are the chip's banking functions:		*/
  /* they do the real switching to the desired bank	*/
  /* they 'become' bdmSetReadFunc() etc.		*/
  void (* ChipSetRead)();
  void (* ChipSetWrite)();
  void (* ChipSetReadWrite)();
  /* Bottom and top of the banking window (rel. to ChipMapBase)	*/
  /* Note: Top = highest accessable byte + 1			*/
  void *ChipReadBottom;
  void *ChipReadTop;
  void *ChipWriteBottom;
  void *ChipWriteTop;
  /* Memory to map      */
  int ChipMapBase;
  int ChipMapSize;      /* replaces MEMTOMAP */
  int ChipSegmentSize;
  int ChipSegmentShift;
  int ChipSegmentMask;
  Bool ChipUse2Banks;
  /* Display size is given by the driver */
  int ChipHDisplay;
  int ChipVDisplay;
  /* In case Scan Line in mfb is longer than HDisplay */
  int ChipScanLineWidth;
} bdmVideoChipRec, *bdmVideoChipPtr;
#endif /* 0 */

bdmVideoChipRec SIGMA = {
  /* Functions */
  SIGMAIdent,
  SIGMAProbe,
  SIGMAEnterLeave,
  SIGMAInit,
  SIGMASave,
  SIGMARestore,
  SIGMAAdjust,
  SIGMASaveScreen,
  NoopDDA,			/* SIGMAGetMode */
  SIGMASetRead,
  SIGMASetWrite,
  SIGMASetReadWrite,
  (void *)SIGMA_BANK1_BOTTOM,	/* ReadBottom */
  (void *)SIGMA_BANK1_TOP,	/* ReadTop */
  (void *)SIGMA_BANK2_BOTTOM,	/* WriteBottom */
  (void *)SIGMA_BANK2_TOP,	/* WriteTop */
  SIGMA_MAP_BASE,		/* MapBase */
  SIGMA_MAP_SIZE,		/* MapSize */
  SIGMA_SEGMENT_SIZE,		/* SegmentSize */
  SIGMA_SEGMENT_SHIFT,		/* SegmentShift */
  SIGMA_SEGMENT_MASK,		/* SegmentMask */
  TRUE,				/* Use2Banks */
  SIGMA_HDISPLAY,		/* HDisplay */
  SIGMA_VDISPLAY,		/* VDisplay */
  SIGMA_SCAN_LINE_WIDTH,	/* ScanLineWidth */
};

/*
 * SIGMAIdent
 */

char *
SIGMAIdent(n)
	int n;
{
/* "Sigma L-View (LVA-PC-00SP0)", "Sigma LaserView PLUS (LDA-1200)" */
/* But this must be the chipset name optionally preset (lowercase!?)*/
/* using only one as I don't know how to tell them apart */
static char *chipsets[] = {"sigmalview"};
if (n >= sizeof(chipsets) / sizeof(char *))
	return(NULL);
else return(chipsets[n]);
}

/*
 * SIGMAProbe --
 *      check whether an SIGMA based board is installed
 */

Bool
SIGMAProbe()
{
  /*
   * Set up I/O ports to be used by this card
   */
  xf86ClearIOPortList(bdm2InfoRec.scrnIndex);
  xf86AddIOPorts(bdm2InfoRec.scrnIndex, Num_SIGMA_IOPorts, SIGMA_IOPorts);

  if (bdm2InfoRec.chipset) {
	/* Chipset preset */
	if (strcmp(bdm2InfoRec.chipset, SIGMAIdent(0)))
		/* desired chipset != this one */
		return (FALSE);
	else {
		SIGMAEnterLeave(ENTER);
		/* go on with videoram etc. below */
	}
  }
  else {
	Bool found=FALSE;
	unsigned char zoom_save;

	/* Enable I/O Ports */
	SIGMAEnterLeave(ENTER);

	/*
	 * Check if there is a Sigma L-View or LaserView PLUS board
	 * in the system.
	 */

	/* Test if ZOOM bit (bit 7 on extended port 0x4649) is r/w */
	/* save it first */
	zoom_save = inb(SLV_ZOOM);
	outb(SLV_ZOOM,0);
	found=(inb(SLV_ZOOM)==0);
	outb(SLV_ZOOM,0x80);
	found=found && ((inb(SLV_ZOOM)&0x80)==0x80);
	/* write back */
	if (found)
		outb(SLV_ZOOM, zoom_save);

	/* There seems to be no easy way to tell if it is an PLUS or not
	 * (apart perhaps from writing to both planes) */
	if (found) {
		bdm2InfoRec.chipset = SIGMAIdent(0);
		ErrorF("%s: %s detected\n", bdm2InfoRec.name,
				bdm2InfoRec.chipset);
	} else {
	/* there is no Sigma L-View card */
		SIGMAEnterLeave(LEAVE);
		return(FALSE);
	}
  } /* else (bdm2InfoRec.chipset) -- bdm2InfoRec.chipset is already set */
  if (!bdm2InfoRec.videoRam) {
	/* videoram not given in Xconfig */
	bdm2InfoRec.videoRam=256;
  }
  /* We do 'virtual' handling here as it is highly chipset specific */
  /* Screen size (pixels) is fixed, virtual size can be larger up to
   * ChipMaxVirtualX and ChipMaxVirtualY */
  /* Real display size is given by SIGMA_HDISPLAY and SIGMA_VDISPLAY,
   * desired virtual size is bdm2InfoRec.virtualX and bdm2InfoRec.virtualY.
   * Think they can be -1 at this point.
   * Maximum virtual size as done by the driver is
   * SIGMA_MAX_VIRTUAL_X and ..._Y
   */
   if (!(bdm2InfoRec.virtualX < 0)) {
	/* virtual set in Xconfig */
	ErrorF("%s: %s: Virtual not allowed for Sigma LaserView\n",
		       bdm2InfoRec.name, bdm2InfoRec.chipset);
   }
   /* Set virtual to real size */
   bdm2InfoRec.virtualX = SIGMA_HDISPLAY;
   bdm2InfoRec.virtualY = SIGMA_VDISPLAY;
   /* Must return real display size */
   /* hardcoded in SIGMA */
   return(TRUE);
}

/*
 * SIGMAEnterLeave --
 *      enable/disable io permissions
 */

void 
SIGMAEnterLeave(enter)
     Bool enter;
{
  if (enter)
	xf86EnableIOPorts(bdm2InfoRec.scrnIndex);
  else
	xf86DisableIOPorts(bdm2InfoRec.scrnIndex);
}

/*
 * SIGMAInit --
 *      Handle the initialization of the SIGMAs registers
 */

void *
SIGMAInit(mode)
     DisplayModePtr mode;
{
/* this is a r/w copy of the initial graph mode */
static sigmaPtr sigmaInitVideoMode = NULL;

if (!sigmaInitVideoMode)
	sigmaInitVideoMode = (sigmaPtr)Xalloc(sizeof(sigmaRec));
/* memcpy(dest,source,size) */
memcpy((void *)sigmaInitVideoMode, (void *)&sigmaRegsGraf1664x1152,
       sizeof(sigmaRec));
return((void *)sigmaInitVideoMode);
}

/*
 * SIGMASave --
 *      save the current video mode
 */

void *
SIGMASave(save)
     sigmaPtr save;
{
unsigned char i, val;

if (save==NULL)
	save=(sigmaPtr)Xalloc(sizeof(sigmaRec));
save->en1=inb(SLV_EN1);
save->w16=inb(SLV_W16);
save->w17=inb(SLV_W17);
save->w18=inb(SLV_W18);
save->blank=inb(SLV_BLANK);
save->zoom=inb(SLV_ZOOM);
save->gr0=inb(SLV_GR0);
save->gr1=inb(SLV_GR1);
save->bank0=inb(SLV_BANK0);
save->bank1=inb(SLV_BANK1);
save->bank2=inb(SLV_BANK2);
save->bank3=inb(SLV_BANK3);
save->hires=inb(SLV_HIRES);

return((void *)save);
}

/*
 * SIGMARestore --
 *      restore a video mode
 */

void
SIGMARestore(restore)
     sigmaPtr restore;
{
  unsigned char i;
  if (restore!=NULL) /* better be shure */ {
	/* Blank the screen to black */
	outb(SLV_GR0,0);
	outb(SLV_GR1,0);
	outb(SLV_BLANK,0);
	/* Disable adapter memory */
	outb(SLV_EN1,0);

	/* Restore original values */
	outb(SLV_W16,restore->w16);
	outb(SLV_W17,restore->w17);
	outb(SLV_W18,restore->w18);
	outb(SLV_ZOOM,restore->zoom);
	outb(SLV_BANK0,restore->bank0);
	outb(SLV_BANK1,restore->bank1);
	outb(SLV_BANK2,restore->bank2);
	outb(SLV_BANK3,restore->bank3);
	outb(SLV_HIRES,restore->hires);
	/* Restore screensaver values */
	outb(SLV_GR0,restore->gr0);
	outb(SLV_GR1,restore->gr1);
	outb(SLV_BLANK,restore->blank);
	/* Restore enable state (may be disabled) */
	outb(SLV_EN1,restore->en1);
  }
  else ErrorF("Warning: SIGMARestore called with arg==NULL\n");
}

/*
 * SIGMASaveScreen();
 *	Disable the video on the frame buffer (screensaver)
 */

Bool
SIGMASaveScreen(pScreen,on)
	ScreenPtr pScreen;
	Bool      on;
{
if (on == SCREEN_SAVER_FORCER)
	SetTimeSinceLastInputEvent();
if (x386VTSema) {
	if (on) { /* Grrr! SaveScreen(on=TRUE) means turn ScreenSaver off */
		/* Unblank to 4 gray levels */
		outb(SLV_BLANK,0x80);
		outb(SLV_GR0,0x80);
		outb(SLV_GR1,0x80);
	} else {
		/* Blank to black */
		outb(SLV_BLANK,0);
		outb(SLV_GR0,0);
		outb(SLV_GR1,0);
	}
} /* if we are not on the active VT, don't do anything - the screen
   * will be visible as soon as we switch back anyway (?) */
return(TRUE);
}

/* SIGMAAdjust --
 *      adjust the current video frame to display the mousecursor
 *      (x,y) is the upper left corner to be displayed.
 *      The Sigma L-View / LaserView PLUS can't pan.
 */
void
SIGMAAdjust(x,y)
	int x, y;
{
}
