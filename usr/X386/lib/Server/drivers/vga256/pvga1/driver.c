/*
 * $XFree86: mit/server/ddx/x386/vga256/drivers/pvga1/driver.c,v 2.18 1993/10/12 15:52:31 dawes Exp $
 * $XConsortium: driver.c,v 1.2 91/08/20 15:13:33 gildea Exp $
 *
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
 */

/*
 * Accelerated support for 90C31 added by Mike Tierney <floyd@eng.umd.edu>
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

#ifdef XF86VGA16
#define MONOVGA
#endif

#ifndef MONOVGA
#include "cfbfuncs.h"

extern void pvgacfbFillRectSolidCopy();
extern int pvgacfbDoBitbltCopy();
extern void pvgacfbFillBoxSolid();
extern void pvgaBitBlt();
#endif

typedef struct {
  vgaHWRec      std;          /* std IBM VGA register */
  unsigned char PR0A;           /* PVGA1A, WD90Cxx */
  unsigned char PR0B;
  unsigned char MemorySize;
  unsigned char VideoSelect;
  unsigned char CRTCCtrl;
  unsigned char VideoCtrl;
  unsigned char InterlaceStart; /* WD90Cxx */
  unsigned char InterlaceEnd;
  unsigned char MiscCtrl1;
  unsigned char MiscCtrl2;
  unsigned char InterfaceCtrl;  /* WD90C1x */
  unsigned char MemoryInterface; /* WD90C1x, WD90C3x */
  unsigned char FlatPanelCtrl; /* WD90C2x */
  unsigned short CursorCntl;   /* WD90C3x, 23C0<-2, 23C2, 0 */
} vgaPVGA1Rec, *vgaPVGA1Ptr;

static Bool  PVGA1Probe();
static char *PVGA1Ident();
static Bool  PVGA1ClockSelect();
static void  PVGA1EnterLeave();
static Bool  PVGA1Init();
static void *PVGA1Save();
static void  PVGA1Restore();
static void  PVGA1Adjust();
static void  PVGA1FbInit();
extern void  PVGA1SetRead();
extern void  PVGA1SetWrite();
extern void  PVGA1SetReadWrite();

vgaVideoChipRec PVGA1 = {
  PVGA1Probe,
  PVGA1Ident,
  PVGA1EnterLeave,
  PVGA1Init,
  PVGA1Save,
  PVGA1Restore,
  PVGA1Adjust,
  NoopDDA,
  NoopDDA,
  PVGA1FbInit,
  PVGA1SetRead,
  PVGA1SetWrite,
  PVGA1SetReadWrite,
  0x10000,
  0x08000,
  15,
  0x7FFF,
  0x08000, 0x10000,
  0x00000, 0x08000,
  TRUE,                                    /* Uses 2 banks */
  VGA_DIVIDE_VERT,
  {0,},
  8,
};

#define new ((vgaPVGA1Ptr)vgaNewVideoState)

#define MCLK	8		/* (VCLK == MCLK) is clock index 8 */

#define C_PVGA1	0		/* PVGA1 */
#define WD90C00	1		/* WD90C00 */
#define WD90C10	2		/* WD90C1x */
#define WD90C30	3		/* WD90C30 */
#define WD90C31	4		/* WD90C31 */
#define WD90C20 5		/* WD90C2x */
static int WDchipset;
static int MClk;
static unsigned char save_cs2 = 0;

#define IS_WD90C3X(x)	(((x) == WD90C30) || ((x) == WD90C31))

#undef DO_WD90C20

static unsigned PVGA1_ExtPorts[] = {            /* extra ports for WD90C31 */
             0x23C0, 0x23C1, 0x23C2, 0x23C3, 0x23C4, 0x23C5 };

static int NumPVGA1_ExtPorts =
             ( sizeof(PVGA1_ExtPorts) / sizeof(PVGA1_ExtPorts[0]) );


/*
 * PVGA1Ident
 */

static char *
PVGA1Ident(n)
     int n;
{
#ifdef DO_WD90C20
  static char *chipsets[] = {"pvga1","wd90c00","wd90c10",
			     "wd90c30","wd90c31","wd90c20"};
#else
  static char *chipsets[] = {"pvga1","wd90c00","wd90c10","wd90c30","wd90c31"};
#endif

  if (n + 1 > sizeof(chipsets) / sizeof(char *))
    return(NULL);
  else
    return(chipsets[n]);
}


/*
 * PVGA1ClockSelect --
 *      select one of the possible clocks ...
 */

static Bool
PVGA1ClockSelect(no)
     int no;
{
  static unsigned char save1, save2, save3;
  unsigned char temp;

  switch(no)
  {
    case CLK_REG_SAVE:
      save1 = inb(0x3CC);
      if (vga256InfoRec.clocks > 4)
      {
        outb(0x3CE, 0x0C); save2 = inb(0x3CF);
      }
      if (WDchipset != C_PVGA1)
      {
        outb(vgaIOBase + 4, 0x2E); save3 = inb(vgaIOBase + 5);
      }
      break;
    case CLK_REG_RESTORE:
      outb(0x3C2, save1);
      if (vga256InfoRec.clocks > 4)
      {
        outw(0x3CE, 0x0C | (save2 << 8));
      }
      if (WDchipset != C_PVGA1)
      {
        outw(vgaIOBase + 4, 0x2E | (save3 << 8));
      }
      break;
    case MCLK:
      /*
       * On all of the chipsets after PVGA1, you can feed MCLK as VCLK.  Hence
       * a 9th clock.
       */
      outb(vgaIOBase+4, 0x2E);
      temp = inb(vgaIOBase+5);
      outb(vgaIOBase+5, temp | 0x10);
      break;
    default:
      /*
       * Disable feeding MCLK to VCLK
       */
      if (WDchipset != C_PVGA1)
      {
          outb(vgaIOBase+4, 0x2E);
          temp = inb(vgaIOBase+5);
          outb(vgaIOBase+5, temp & 0xEF);
      }
      temp = inb(0x3CC);
      outb(0x3C2, ( temp & 0xf3) | ((no << 2) & 0x0C));
      if (vga256InfoRec.clocks > 4)
      {
        outw(0x3CE, 0x0C | ((((no & 0x04) >> 1) ^ save_cs2) << 8));
      }
  }
  return(TRUE);
}



/*
 * PVGA1Probe --
 *      check up whether a PVGA1 based board is installed
 */

static Bool
PVGA1Probe()
{
    int numclocks;

    /*
     * Set up I/O ports to be used by this card
     */
    xf86ClearIOPortList(vga256InfoRec.scrnIndex);
    xf86AddIOPorts(vga256InfoRec.scrnIndex, Num_VGA_IOPorts, VGA_IOPorts);

    if (vga256InfoRec.chipset) {
        if (!StrCaseCmp(vga256InfoRec.chipset, PVGA1Ident(0)))
	    WDchipset = C_PVGA1;
        else if (!StrCaseCmp(vga256InfoRec.chipset, PVGA1Ident(1)))
	    WDchipset = WD90C00;
        else if (!StrCaseCmp(vga256InfoRec.chipset, PVGA1Ident(2)))
	    WDchipset = WD90C10;
        else if (!StrCaseCmp(vga256InfoRec.chipset, PVGA1Ident(3)))
	    WDchipset = WD90C30;
	else if (!StrCaseCmp(vga256InfoRec.chipset, PVGA1Ident(4)))
	    WDchipset = WD90C31;
#ifdef DO_WD90C20
        else if (!StrCaseCmp(vga256InfoRec.chipset, PVGA1Ident(5)))
	    WDchipset = WD90C20;
#endif
        else
	    return (FALSE);

        PVGA1EnterLeave(ENTER);
    }
    else {
	char ident[4];
	unsigned char tmp;
	unsigned char sig[2];

	if (xf86ReadBIOS(vga256InfoRec.BIOSbase, 0x7D, (unsigned char *)ident,
			 4) != 4)
	    return(FALSE);
        if (strncmp(ident, "VGA=",4)) 
	    return(FALSE);

        /*
         * OK.  We know it's a Paradise/WD chip.  Now to figure out which one.
         */
        PVGA1EnterLeave(ENTER);
        if (!testinx(vgaIOBase+0x04, 0x2B))
	    WDchipset = C_PVGA1;
        else {
	    tmp = rdinx(0x3C4, 0x06);
	    wrinx(0x3C4, 0x06, 0x48);
	    if (!testinx2(0x3C4, 0x07, 0xF0))
	        WDchipset = WD90C00;
	    else if (!testinx(0x3C4, 0x10)) {
		/* WD90C2x */
	        WDchipset = WD90C20;	
		wrinx(vgaIOBase+0x04, 0x34, 0xA6);
		if ((rdinx(vgaIOBase+0x04,0x32) & 0x20) != 0)
		    wrinx(vgaIOBase+0x04, 0x34, 0x00);
#ifndef DO_WD90C20
        	PVGA1EnterLeave(LEAVE);
		return(FALSE);
#endif
	    }
	    else if (testinx2(0x3C4, 0x14, 0x0F)) {
		/* 
		 * WD90C3x - these registers MAY hold the numeric signature;
		 * they're not documented.  We'll see.  Odds are that this
		 * check will get turned around into != '0', to handle the
		 * later WD chipsets.
		 */
		sig[0] = rdinx(vgaIOBase+0x04, 0x36);
		sig[1] = rdinx(vgaIOBase+0x04, 0x37);
    		ErrorF("%s %s: WD: Signature for WD90C3X=[0x%02x 0x%02x]\n", 
           	       XCONFIG_PROBED, vga256InfoRec.name, sig[0], sig[1]);
		if (sig[1] == '1')
		    WDchipset = WD90C31;
		else
		    WDchipset = WD90C30;
	    }
	    else {
		/* WD90C1x */
		WDchipset = WD90C10;
	    }
	    wrinx(0x3C4, 0x06, tmp);
	}
    }
    vga256InfoRec.chipset = PVGA1Ident(WDchipset);

    if (WDchipset == WD90C31)  /* enable extra hardware accel registers */
    {
        xf86AddIOPorts(vga256InfoRec.scrnIndex,
                       NumPVGA1_ExtPorts, PVGA1_ExtPorts);
      	PVGA1EnterLeave(LEAVE);   /* force update of IO ports enable */
      	PVGA1EnterLeave(ENTER);
    }


    /*
     * Detect how much memory is installed
     */
    if (!vga256InfoRec.videoRam) {
        unsigned char config;

        outb(0x3CE, 0x0B); config = inb(0x3CF);
      
        switch(config & 0xC0) {
        case 0x00:
        case 0x40:
	    vga256InfoRec.videoRam = 256;
	    break;
        case 0x80:
	    vga256InfoRec.videoRam = 512;
	    break;
        case 0xC0:
	    vga256InfoRec.videoRam = 1024;
	    break;
        }
    }

    /*
     * The 90C1x, 90C2x, and 90C3x seem to have this bit set for the first 
     * 4 clocks (perhaps only with ICS90C6x clock chip?), so save the initial 
     * state to get the correct clock ordering.  If the user requested that 
     * the bit be inverted, do it.
     */
    if ((WDchipset == WD90C10) || (WDchipset == WD90C20) || 
	(IS_WD90C3X(WDchipset))) {
        if (OFLG_ISSET(OPTION_SWAP_HIBIT, &vga256InfoRec.options))
	    save_cs2 = 0;
	else
	    save_cs2 = 2;
    }
    else {
        if (OFLG_ISSET(OPTION_SWAP_HIBIT, &vga256InfoRec.options))
	    save_cs2 = 2;
	else
	    save_cs2 = 0;
    }

    /*
     * All of the chips later than the PVGA1 can feed MCLK as VCLK, yielding
     * an extra clock select.
     */
    if (WDchipset == C_PVGA1)
	if (OFLG_ISSET(OPTION_8CLKS, &vga256InfoRec.options))
	    numclocks = 8;
	else
	    numclocks = 4;
    else
	numclocks = 9;

    if (!vga256InfoRec.clocks) vgaGetClocks(numclocks, PVGA1ClockSelect);
    if (WDchipset != C_PVGA1) {
	if (vga256InfoRec.clocks != 9) {
	    ErrorF("%s %s: %s: No MCLK specified in 'Clocks' line.  %s\n",
		   XCONFIG_GIVEN, vga256InfoRec.name, 
		   vga256InfoRec.chipset, "Using default 45");
	    MClk = 45000;
	}
	else
	    MClk = vga256InfoRec.clock[MCLK];
    }

    vga256InfoRec.bankedMono = TRUE;

    /* Initialize allowed option flags based on chipset */
    OFLG_SET(OPTION_SWAP_HIBIT, &PVGA1.ChipOptionFlags);
    if (WDchipset == C_PVGA1)
	OFLG_SET(OPTION_8CLKS, &PVGA1.ChipOptionFlags);
    if (WDchipset == WD90C20) {
	OFLG_SET(OPTION_INTERN_DISP, &PVGA1.ChipOptionFlags);
	OFLG_SET(OPTION_EXTERN_DISP, &PVGA1.ChipOptionFlags);
    }
    if (WDchipset == WD90C31)
	OFLG_SET(OPTION_NOACCEL, &PVGA1.ChipOptionFlags);
    
    return(TRUE);
}


/*
 * PVGA1FbInit --
 *      initialize the cfb SpeedUp functions
 */
/* block of current settings for speedup registers */
int pvga1_blt_cntrl2 = -1;
int pvga1_blt_src_low = -1;
int pvga1_blt_src_hgh = -1;
int pvga1_blt_dim_x = -1;
int pvga1_blt_dim_y = -1;
int pvga1_blt_row_pitch = -1;
int pvga1_blt_rop = -1;
int pvga1_blt_for_color = -1;
int pvga1_blt_bck_color = -1;
int pvga1_blt_trns_color = -1;
int pvga1_blt_trns_mask = -1;
int pvga1_blt_planemask = -1;

void (*pvga1_stdcfbFillRectSolidCopy)();
int (*pvga1_stdcfbDoBitbltCopy)();
void (*pvga1_stdcfbBitblt)();
void (*pvga1_stdcfbFillBoxSolid)();


static void
PVGA1FbInit()
{
#ifndef MONOVGA
  int useSpeedUp;

  if (WDchipset != WD90C31 ||
      OFLG_ISSET(OPTION_NOACCEL, &vga256InfoRec.options))
     return;

  useSpeedUp = vga256InfoRec.speedup & SPEEDUP_ANYWIDTH;
  if (useSpeedUp && x386Verbose)
  {
    ErrorF("%s %s: PVGA1: SpeedUps selected (Flags=0x%x)\n", 
           OFLG_ISSET(XCONFIG_SPEEDUP,&vga256InfoRec.xconfigFlag) ?
           XCONFIG_GIVEN : XCONFIG_PROBED,
           vga256InfoRec.name, useSpeedUp);
  }
  if (useSpeedUp & SPEEDUP_FILLRECT)
  {
   /** must save default because if not screen to screen must use default **/
    pvga1_stdcfbFillRectSolidCopy = cfbLowlevFuncs.fillRectSolidCopy;
    cfbLowlevFuncs.fillRectSolidCopy = pvgacfbFillRectSolidCopy;
  }
  if (useSpeedUp & SPEEDUP_BITBLT)
  {
    pvga1_stdcfbDoBitbltCopy = cfbLowlevFuncs.doBitbltCopy;
    cfbLowlevFuncs.doBitbltCopy = pvgacfbDoBitbltCopy;
    pvga1_stdcfbBitblt = cfbLowlevFuncs.vgaBitblt;
    cfbLowlevFuncs.vgaBitblt = pvgaBitBlt;
  }
  if (useSpeedUp & SPEEDUP_LINE)
  {
    ;
  }
  if (useSpeedUp & SPEEDUP_FILLBOX)
  {
    pvga1_stdcfbFillBoxSolid = cfbLowlevFuncs.fillBoxSolid;
    cfbLowlevFuncs.fillBoxSolid = pvgacfbFillBoxSolid;
  }
#endif /* MONOVGA */
}



/*
 * PVGA1EnterLeave --
 *      enable/disable io-mapping
 */

static void 
PVGA1EnterLeave(enter)
     Bool enter;
{
  unsigned char temp;

  if (enter)
    {
      xf86EnableIOPorts(vga256InfoRec.scrnIndex);

      vgaIOBase = (inb(0x3CC) & 0x01) ? 0x3D0 : 0x3B0;
      outw(0x3CE, 0x050F);           /* unlock PVGA1 Register Bank 1 */
      outw(vgaIOBase + 4, 0x8529);   /* unlock PVGA1 Register Bank 2 */
      outb(vgaIOBase + 4, 0x11); temp = inb(vgaIOBase + 5);
      outb(vgaIOBase + 5, temp & 0x7F);
    }
  else
    {
      xf86DisableIOPorts(vga256InfoRec.scrnIndex);
    }
}



/*
 * PVGA1Restore --
 *      restore a video mode
 */

static void
PVGA1Restore(restore)
     vgaPVGA1Ptr restore;
{
  unsigned char temp;

  /*
   * First unlock all these special registers ...
   * NOTE: Locking will not be fully renabled !!!
   */
  outb(0x3CE, 0x0D); temp = inb(0x3CF); outb(0x3CF, temp & 0x1C);
  outb(0x3CE, 0x0E); temp = inb(0x3CF); outb(0x3CF, temp & 0xFB);

  outb(vgaIOBase + 4, 0x2A); temp = inb(vgaIOBase + 5);
  outb(vgaIOBase + 5, temp & 0xF8);

  /* Unlock sequencer extended registers */
  outb(0x3C4, 0x06); temp = inb (0x3C5);
  outb(0x3C5, temp | 0x48);

  outw(0x3CE, 0x0009);   /* segment select A */
  outw(0x3CE, 0x000A);   /* segment select B */

  vgaHWRestore(restore);

  outw(0x3CE, (restore->PR0A << 8) | 0x09);
  outw(0x3CE, (restore->PR0B << 8) | 0x0A);

  outb(0x3CE, 0x0B); temp = inb(0x3CF);          /* banking mode ... */
  outb(0x3CF, (temp & 0xF7) | (restore->MemorySize & 0x08));
       
  if (new->std.NoClock >= 0)
    outw(0x3CE, (restore->VideoSelect << 8) | 0x0C);
#ifndef MONOVGA
  outw(0x3CE, (restore->CRTCCtrl << 8)    | 0x0D);
  outw(0x3CE, (restore->VideoCtrl << 8)   | 0x0E);
#endif
  
  /*
   * Now the WD90Cxx specials (Register Bank 2)
   */
  outw(vgaIOBase + 4, (restore->InterlaceStart << 8) | 0x2C);
  outw(vgaIOBase + 4, (restore->InterlaceEnd << 8)   | 0x2D);
  outw(vgaIOBase + 4, (restore->MiscCtrl2 << 8)      | 0x2F);

  /*
   * For the WD90C10 & WD90C11 we have to select segment mapping.
   */
  outb(0x3C4, 0x11); temp = inb(0x3C5);
  outb(0x3C5, (temp & 0x03) | (restore->InterfaceCtrl & 0xFC));

  outb(vgaIOBase + 4, 0x2E);

  /* Note, only selected bits of MiscCtrl1 are restored */
  temp = inb(vgaIOBase+5);
  if (WDchipset != C_PVGA1)
    {
      /*
       * Deal with 9th clock select if not PVGA1.
       */
      if (new->std.NoClock >= 0)
        temp = (temp & 0xEF) | (restore->MiscCtrl1 & 0x10);
      if (IS_WD90C3X(WDchipset))
        temp = (temp & 0xBF) | (restore->MiscCtrl1 & 0x40);
      outb(vgaIOBase + 5, temp);
    }
  if ((IS_WD90C3X(WDchipset)) || (WDchipset == WD90C10))
    {
      unsigned char mask = (IS_WD90C3X(WDchipset) ? 0xCF : 0x0F);

      outb(0x3C4, 0x10);
      temp = inb(0x3C5);
      outb(0x3C5, (restore->MemoryInterface & mask) | (temp & ~mask));
    }
  if (WDchipset == WD90C20)
    {
      /* Note, only selected bits of FlagPanelCtrl are restored */
      outb(vgaIOBase + 4, 0x32);
      temp = inb(vgaIOBase + 5);
      outb(vgaIOBase + 5, (temp & 0xCF) | (restore->FlatPanelCtrl & 0x30));
    }

  if (WDchipset == WD90C31)    /* set hardware cursor register */
  {
     outw (0x23C0, 2);
     outw (0x23C2, restore->CursorCntl);
  }
}



/*
 * PVGA1Save --
 *      save the current video mode
 */

static void *
PVGA1Save(save)
     vgaPVGA1Ptr save;
{
  unsigned char PR0A, PR0B, temp;

  vgaIOBase = (inb(0x3CC) & 0x01) ? 0x3D0 : 0x3B0;
  outb(0x3CE, 0x0D); temp = inb(0x3CF); outb(0x3CF, temp & 0x1C);
  outb(0x3CE, 0x0E); temp = inb(0x3CF); outb(0x3CF, temp & 0xFD);

  outb(vgaIOBase + 4, 0x2A); temp = inb(vgaIOBase + 5);
  outb(vgaIOBase + 5, temp & 0xF8);

  /* Unlock sequencer extended registers */
  outb(0x3C4, 0x06); temp = inb (0x3C5);
  outb(0x3C5, temp | 0x48);

  outb(0X3CE, 0x09); PR0A = inb(0x3CF); outb(0x3CF, 0x00);
  outb(0X3CE, 0x0A); PR0B = inb(0x3CF); outb(0x3CF, 0x00);

  save = (vgaPVGA1Ptr)vgaHWSave(save, sizeof(vgaPVGA1Rec));

  save->PR0A = PR0A;
  save->PR0B = PR0B;
  outb(0x3CE, 0x0B); save->MemorySize  = inb(0x3CF);
  outb(0x3CE, 0x0C); save->VideoSelect = inb(0x3CF);
#ifndef MONOVGA
  outb(0x3CE, 0x0D); save->CRTCCtrl    = inb(0x3CF);
  outb(0x3CE, 0x0E); save->VideoCtrl   = inb(0x3CF);
#endif

  /* WD90Cxx */
  outb(vgaIOBase + 4, 0x2C); save->InterlaceStart = inb(vgaIOBase+5);
  outb(vgaIOBase + 4, 0x2D); save->InterlaceEnd   = inb(vgaIOBase+5);
  outb(vgaIOBase + 4, 0x2E); save->MiscCtrl1      = inb(vgaIOBase+5);
  outb(vgaIOBase + 4, 0x2F); save->MiscCtrl2      = inb(vgaIOBase+5);

  /* WD90C1x */
  outb(0x3C4, 0x11); save->InterfaceCtrl = inb(0x3C5);

  /* WD90C3x */
  if (IS_WD90C3X(WDchipset) || WDchipset == WD90C10)
    {
      outb(0x3C4, 0x10); save->MemoryInterface = inb(0x3C5);
    }
  if (WDchipset == WD90C20)
    {
      outb(vgaIOBase + 4, 0x32); save->FlatPanelCtrl = inb(vgaIOBase + 5);
    }

  if (WDchipset == WD90C31)   /* save hardware cursor register */
  {
     outw (0x23C0, 2);
     save->CursorCntl = inw(0x23C2);
  }

  return ((void *) save);
}



/*
 * PVGA1Init --
 *      Handle the initialization, etc. of a screen.
 */

static Bool
PVGA1Init(mode)
     DisplayModePtr mode;
{


  if (!vgaHWInit(mode,sizeof(vgaPVGA1Rec)))
    return(FALSE);

#ifndef MONOVGA
  new->std.CRTC[19] = vga256InfoRec.virtualX >> 3; /* we are in byte-mode */
  new->std.CRTC[20] = 0x40;
  new->std.CRTC[23] = 0xE3; /* thats what the man says */
#endif

  new->PR0A = 0x00;
  new->PR0B = 0x00;
  /*************************************
   * Try setting this to 0x0C to play with 8bit vs 16 bit stuff
   *************************************
   */
  new->MemorySize = 0x08;
  if (new->std.NoClock >= 0)
    new->VideoSelect = ((new->std.NoClock & 0x4) >> 1) ^ save_cs2;
#ifndef MONOVGA
  new->CRTCCtrl = 0x00;
  new->VideoCtrl = 0x01;
#endif

  /* WD90Cxx */
  if (mode->Flags & V_INTERLACE) 
    {
      new->InterlaceStart = (mode->HSyncStart >> 3) - (mode->HTotal >> 4);
      new->InterlaceEnd = 0x20 |
                          ((mode->HSyncEnd >> 3) - (mode->HTotal >> 4)) & 0x1F;
    }
  else
    {
      new->InterlaceStart = 0x00;
      new->InterlaceEnd = 0x00;
    }
  /* 9th clock select */
  new->MiscCtrl1 = 0x00;
  if ((new->std.NoClock == MCLK) && (WDchipset != C_PVGA1))
    new->MiscCtrl1 |= 0x10;
  new->MiscCtrl2 = 0x00;

  /* WD90C1x */
  if ((WDchipset != C_PVGA1)&&(WDchipset != WD90C00)&&(WDchipset != WD90C20))
    new->InterfaceCtrl = 0x5C;
  else
    new->InterfaceCtrl = 0x00;

  if (WDchipset == WD90C10)
#ifdef MONOVGA
    new->MemoryInterface = 0x0B;
#else
    new->MemoryInterface = 0x09;
#endif

  if (IS_WD90C3X(WDchipset))
    {
      new->MemoryInterface = 0xC1;
      if (vga256InfoRec.clock[new->std.NoClock] > MClk)
        new->MiscCtrl1 |= 0x40;

      if (WDchipset == WD90C31)
	{
/*** MJT ***/
/**** not yet
          new->CursorCntl = 
	    (0 << 12) | (0 << 11) | (1 << 9) | (0 << 8) | (1 << 5);
****/
        }
    }
  
  if (WDchipset == WD90C20)
    {
      /*
       * Select internal display, external display or both, depending
       * on user option-flag settings.  If neither option flag is set,
       * default to the internal display for monochrome, and external
       * display for color.
       */
      new->FlatPanelCtrl = 0x00;
      if (OFLG_ISSET(OPTION_INTERN_DISP, &vga256InfoRec.options))
	new->FlatPanelCtrl |= 0x10;
      if (OFLG_ISSET(OPTION_EXTERN_DISP, &vga256InfoRec.options))
	new->FlatPanelCtrl |= 0x20;
      if (new->FlatPanelCtrl == 0x00)
#ifdef MONOVGA
	new->FlatPanelCtrl = 0x10;
#else
	new->FlatPanelCtrl = 0x20;
#endif
    }

  return(TRUE);
}
	


/*
 * PVGA1Adjust --
 *      adjust the current video frame to display the mousecursor
 */

static void 
PVGA1Adjust(x, y)
     int x, y;
{
#ifdef MONOVGA
  int           Base = (y * vga256InfoRec.virtualX + x + 3) >> 3;
#else
  int           Base = (y * vga256InfoRec.virtualX + x + 1) >> 2;
#endif
  unsigned char temp;

  outw(vgaIOBase + 4, (Base & 0x00FF00) | 0x0C);
  outw(vgaIOBase + 4, ((Base & 0x00FF) << 8) | 0x0D);
  outb(0x3CE, 0x0D); temp=inb(0x3CF); 
  outb(0x3CF, ((Base & 0x030000) >> 13) | (temp & 0xE7));
}
