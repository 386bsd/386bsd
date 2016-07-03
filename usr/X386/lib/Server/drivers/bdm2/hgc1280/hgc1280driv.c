/*
 * BDM2: Banked dumb monochrome driver
 * Pascal Haible 8/93, haible@izfm.uni-stuttgart.de
 *
 * bdm2/driver/hgc1280/hgc1280driv.c
 *
 * derived from:
 * hga2/*
 * Author:  Davor Matic, dmatic@athena.mit.edu
 * and
 * vga256/*
 * Copyright 1990,91 by Thomas Roell, Dinkelscherben, Germany.
 *
 * see bdm2/COPYRIGHT for copyright and disclaimers.
 */

/* $XFree86: mit/server/ddx/x386/bdm2/drivers/hgc1280/hgc1280driv.c,v 2.2 1993/10/02 09:50:46 dawes Exp $ */

#include "X.h"
#include "input.h"
#include "screenint.h"

#include "compiler.h"

#include "x386.h"
#include "x386Priv.h"
#include "xf86_OSlib.h"
#include "xf86_Config.h"
#include "bdm.h"
#include "hgc1280HW.h"

typedef struct {
  unsigned char reg[HGC_NUM_REGS];
  } hgc1280Rec, *hgc1280Ptr;

/*
 * Define the HGC I/O Ports
 */
unsigned HGC1280_IOPorts[] = { HGC_PORT_INDEX, HGC_PORT_DATA,
		HGC_PORT_CONTROL, HGC_PORT_CRT_STATUS, HGC_PORT_CONFIG };
int Num_HGC1280_IOPorts = (sizeof(HGC1280_IOPorts)/sizeof(HGC1280_IOPorts[0]));

char *	HGC1280Ident();
Bool	HGC1280Probe();
void	HGC1280EnterLeave();
void *	HGC1280Init();
void *	HGC1280Save();
void	HGC1280Restore();
void	HGC1280Adjust();
Bool	HGC1280SaveScreen();
void	HGC1280GetMode();

/* Assembler functions in hgc1280bank.s
 * - to be called by assembler functions only! */
extern void HGC1280SetRead();
extern void HGC1280SetWrite();
extern void HGC1280SetReadWrite();

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

bdmVideoChipRec HGC1280 = {
  /* Functions */
  HGC1280Ident,
  HGC1280Probe,
  HGC1280EnterLeave,
  HGC1280Init,
  HGC1280Save,
  HGC1280Restore,
  HGC1280Adjust,
  HGC1280SaveScreen,
  NoopDDA,			/* HGC1280GetMode */
  HGC1280SetRead,
  HGC1280SetWrite,
  HGC1280SetReadWrite,
  (void *)HGC_BANK1_BOTTOM,	/* ReadBottom */
  (void *)HGC_BANK1_TOP,	/* ReadTop */
  (void *)HGC_BANK2_BOTTOM,	/* WriteBottom */
  (void *)HGC_BANK2_TOP,	/* WriteTop */
  HGC_MAP_BASE,			/* MapBase */
  HGC_MAP_SIZE,			/* MapSize */
  HGC_SEGMENT_SIZE,		/* SegmentSize */
  HGC_SEGMENT_SHIFT,		/* SegmentShift */
  HGC_SEGMENT_MASK,		/* SegmentMask */
  TRUE,				/* Use2Banks */
  HGC_HDISPLAY,			/* HDisplay */
  HGC_VDISPLAY,			/* VDisplay */
  HGC_SCAN_LINE_WIDTH,		/* ScanLineWidth */
};

/*
 * HGC1280Ident
 */

char *
HGC1280Ident(n)
	int n;
{
static char *chipsets[] = {"hgc1280"};
if (n >= sizeof(chipsets) / sizeof(char *))
	return(NULL);
else return(chipsets[n]);
}

/*
 * HGC1280Probe --
 *      check whether an HGC1280 based board is installed
 */

Bool
HGC1280Probe()
{
  /*
   * Set up I/O ports to be used by this card
   */
  xf86ClearIOPortList(bdm2InfoRec.scrnIndex);
  xf86AddIOPorts(bdm2InfoRec.scrnIndex, Num_HGC1280_IOPorts, HGC1280_IOPorts);

  if (bdm2InfoRec.chipset) {
	/* Chipset preset */
	if (strcmp(bdm2InfoRec.chipset, HGC1280Ident(0)))
		/* desired chipset != this one */
		return (FALSE);
	else {
		HGC1280EnterLeave(ENTER);
		/* go on with videoram etc. below */
	}
  }
  else {
	unsigned char dsp, dsp_old;
	int i;
	int cnt=0;
	short val=0;
	Bool found=FALSE;

	HGC1280EnterLeave(ENTER);
	/*
	 * Checks if there is a HGC-1280 based board in the system.
	 *
	 *
	 */
	/*************************************************************
	 * If anybody has any 'real' infos about the HGC-1280 Hardware
	 * - Please please contact me!
	 *************************************************************/
	/* Check for Hercules-like HSYNC on BASE+0xA, Bit 1 */ 
#define HSYNC_MASK  0x01
#define MIN_COUNT 2000
	dsp_old = inb(HGC_PORT_CRT_STATUS) & HSYNC_MASK;
	for (i = 0; i < 5000; i++) {
		dsp = inb(HGC_PORT_CRT_STATUS) & HSYNC_MASK;
		if (dsp != dsp_old) cnt++;
		dsp_old = dsp;
	}
	/* This cnt is about 3500 to 4500 with a 'virgin' hardware,
	 * and might be exactly 5000 if the card had been switched
	 * to 'my' graphics mode before */
	if (cnt>MIN_COUNT) {
		bdm2InfoRec.chipset = HGC1280Ident(0);
		ErrorF("%s %s: %s: cnt=%d, ", XCONFIG_PROBED,
			bdm2InfoRec.name, bdm2InfoRec.chipset, cnt);
		/* Test a r/w register */
		HGC_SET_REG(HGC_PROBE_REG_RW,HGC_PROBE_VAL_WRITE);
		HGC_GET_REG(HGC_PROBE_REG_RW,&val);
		ErrorF("r/w-val=%d, ",val);
		found = (val==HGC_PROBE_VAL_READ);
		/* this register can't be set, depends on other registers */
		HGC_GET_REG(HGC_PROBE_REG_FIX,&val);
		ErrorF("fix-val1=%d, ",val);
		found = found && (val==HGC_PROBE_VAL_FIX1);
		HGC_SET_REG(HGC_PROBE_REG_RW,HGC_PROBE_VAL_RESET);
		HGC_GET_REG(HGC_PROBE_REG_FIX,&val);
		ErrorF("fix-val2=%d ",val);
		found = found && (val==HGC_PROBE_VAL_FIX2);
		if (found)
			ErrorF("==> found\n");
		else {
			ErrorF("==> not found\n");
			bdm2InfoRec.chipset = "\0";
		}
	}
	if ( !(cnt>MIN_COUNT) || !found ) {
	/* there is no HGC-1280 card */
		HGC1280EnterLeave(LEAVE);
		return(FALSE);
	}
  } /* else (bdm2InfoRec.chipset) -- bdm2InfoRec.chipset is already set */
  if (!bdm2InfoRec.videoRam) {
	/* videoram not given in Xconfig */
	bdm2InfoRec.videoRam=192;
  }
  /* We do 'virtual' handling here as it is highly chipset specific */
  /* Screen size (pixels) is fixed, virtual size can be larger up to
   * ChipMaxVirtualX and ChipMaxVirtualY */
  /* Real display size is given by HGC_HDISPLAY and HGC_VDISPLAY,
   * desired virtual size is bdm2InfoRec.virtualX and bdm2InfoRec.virtualY.
   * Think they can be -1 at this point.
   * Maximum virtual size as done by the driver is
   * HGC_MAX_VIRTUAL_X and ..._Y
   */
   if (bdm2InfoRec.virtualX < 0) {
	/* No virtual set in Xconfig */
	/* Set virtual to real size */
	bdm2InfoRec.virtualX = HGC_HDISPLAY;
	bdm2InfoRec.virtualY = HGC_VDISPLAY;
   } else {
	int Xrounding = 16;
        if (bdm2InfoRec.virtualX > HGC_MAX_VIRTUAL_X) {
		ErrorF("%s %s: %s: Virtual width too large, reset to %d\n",
			XCONFIG_PROBED,
			bdm2InfoRec.name, bdm2InfoRec.chipset,
			HGC_MAX_VIRTUAL_X);
		bdm2InfoRec.virtualX = HGC_MAX_VIRTUAL_X;
	} else if (bdm2InfoRec.virtualY > HGC_MAX_VIRTUAL_Y) {
		ErrorF("%s %s: %s: Virtual height too large, reset to %d\n",
			XCONFIG_PROBED,
			bdm2InfoRec.name, bdm2InfoRec.chipset,
			HGC_MAX_VIRTUAL_Y);
                bdm2InfoRec.virtualY = HGC_MAX_VIRTUAL_Y;
	} else if (bdm2InfoRec.virtualX < HGC_HDISPLAY) {
		ErrorF("%s %s: %s: Virtual width too small, reset to %d\n",
			XCONFIG_PROBED,
			bdm2InfoRec.name, bdm2InfoRec.chipset,
			HGC_HDISPLAY);
		bdm2InfoRec.virtualX = HGC_HDISPLAY;
	} else if (bdm2InfoRec.virtualY < HGC_VDISPLAY) {
		ErrorF("%s %s: %s: Virtual height too small, reset to %d\n",
			XCONFIG_PROBED,
			bdm2InfoRec.name, bdm2InfoRec.chipset,
			HGC_VDISPLAY);
		bdm2InfoRec.virtualY = HGC_VDISPLAY;
	};
	if (bdm2InfoRec.virtualX % Xrounding) {
		bdm2InfoRec.virtualX -= bdm2InfoRec.virtualX % Xrounding;
		ErrorF("%s: %s: Virtual width rounded down to %d\n",
			bdm2InfoRec.name, bdm2InfoRec.chipset,
			bdm2InfoRec.virtualX);
	}
   }
   /* Must return real display size */
   /* hardcoded in HGC1280 */
   return(TRUE);
}

/*
 * HGC1280EnterLeave --
 *      enable/disable io permissions
 */

void 
HGC1280EnterLeave(enter)
     Bool enter;
{
  if (enter)
	xf86EnableIOPorts(bdm2InfoRec.scrnIndex);
  else
	xf86DisableIOPorts(bdm2InfoRec.scrnIndex);
}

/*
 * HGC1280Init --
 *      Handle the initialization of the HGCs registers
 */

void *
HGC1280Init(mode)
     DisplayModePtr mode;
{
/* this is a r/w copy of the initial graph mode */
static hgc1280Ptr hgcInitVideoMode = NULL;

if (!hgcInitVideoMode)
	hgcInitVideoMode = (hgc1280Ptr)Xalloc(sizeof(hgc1280Rec));
/* memcpy(dest,source,size) */
memcpy((void *)hgcInitVideoMode, (void *)hgcRegsGraf1280x1024,
       sizeof(hgc1280Rec));
return((void *)hgcInitVideoMode);
}

/*
 * HGC1280Save --
 *      save the current video mode
 */

void *
HGC1280Save(save)
     hgc1280Ptr save;
{
unsigned char i, val;

if (save==NULL)
	save=(hgc1280Ptr)Xalloc(sizeof(hgc1280Rec));
for (i = 0; i < HGC_NUM_REGS; i++) {
	HGC_GET_REG(i,&val);
	save->reg[i] = val;
}
return((void *)save);
}

/*
 * HGC1280Restore --
 *      restore a video mode
 */

void
HGC1280Restore(restore)
     hgc1280Ptr restore;
{
  unsigned char i;
  if (restore!=NULL) /* better be shure */ {
	for (i = 0; i < HGC_NUM_REGS; i++) {
		HGC_SET_REG(i,restore->reg[i]);
	}
  }
  else ErrorF("Warning: HGC1280Restore called with arg==NULL\n");
}

/*
 * HGC1280SaveScreen();
 *	Disable the video on the frame buffer (screensaver)
 */

Bool
HGC1280SaveScreen(pScreen,on)
	ScreenPtr pScreen;
	Bool      on;
{
if (on == SCREEN_SAVER_FORCER)
	SetTimeSinceLastInputEvent();
if (x386VTSema) {
	/* Kind of hack: to get the screen dark, I set the
	 * "begin [end] of display within scanline" to middle of the screen
	 */
	if (on) { /* Grrr! SaveScreen(on=TRUE) means turn ScreenSaver off */
		HGC_SET_REG(HGC_REG_LEFT_BORDER,138);
		HGC_SET_REG(HGC_REG_RIGHT_BORDER,90);
	} else {
		HGC_SET_REG(HGC_REG_LEFT_BORDER,170);
		HGC_SET_REG(HGC_REG_RIGHT_BORDER,50);
	}
} /* if we are not on the active VT, don't do anything - the screen
   * will be visible as soon as we switch back anyway (?) */
return(TRUE);
}

/* HGC1280Adjust --
 *      adjust the current video frame to display the mousecursor
 *      (x,y) is the upper left corner to be displayed.
 *      This needs some special handling with hgc1280: the card can only pan
 *      in steps of 16 pixels.
 */
void
HGC1280Adjust(x,y)
	int x, y;
{
static int oldx=0;
int val;
#if 0
ErrorF("(%d,%d)",x,y);
if (x>=192)
	ErrorF("hgc1280: HGC1280Adjust(%d,%d) ",x,y);
#endif
/* make shure the pointer is always on the screen */
if (x>oldx)
	/* right side */
	val=16-((x+15)>>4);
else
	/* left side */
	val=16-(x>>4);
oldx=x;
HGC_SET_REG(HGC_REG_SHIFT_DISPLAY,val);
}
