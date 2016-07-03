/*
 * $XFree86: mit/server/ddx/x386/vga256/drivers/et4000/driver.c,v 2.17 1993/10/03 14:57:40 dawes Exp $
 * $XConsortium: driver.c,v 1.2 91/08/20 15:11:50 gildea Exp $
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

extern void speedupcfbFillRectSolidCopy();
extern int speedupcfbDoBitbltCopy();
extern void speedupcfbLineSS();
extern void speedupcfbSegmentSS();
extern void speedupcfbFillBoxSolid();
#endif

typedef struct {
  vgaHWRec std;               /* good old IBM VGA */
  unsigned char ExtStart;     /* Tseng ET4000 specials   CRTC 0x33/0x34/0x35 */
  unsigned char Compatibility;
  unsigned char OverflowHigh;
  unsigned char StateControl;    /* TS 6 & 7 */
  unsigned char AuxillaryMode;
  unsigned char Misc;           /* ATC 0x16 */
  unsigned char SegSel;
#ifndef MONOVGA
  unsigned char RCConf;       /* CRTC 0x32 */
#endif
  } vgaET4000Rec, *vgaET4000Ptr;


static Bool     ET4000Probe();
static char *   ET4000Ident();
static Bool     ET4000ClockSelect();
static Bool     LegendClockSelect();
static void     ET4000EnterLeave();
static Bool     ET4000Init();
static void *   ET4000Save();
static void     ET4000Restore();
static void     ET4000Adjust();
static void     ET4000FbInit();
extern void     ET4000SetRead();
extern void     ET4000SetWrite();
extern void     ET4000SetReadWrite();

static unsigned char 	save_divide = 0;
#ifndef MONOVGA
static unsigned char    initialRCConf = 0x70;
#endif

vgaVideoChipRec ET4000 = {
  ET4000Probe,
  ET4000Ident,
  ET4000EnterLeave,
  ET4000Init,
  ET4000Save,
  ET4000Restore,
  ET4000Adjust,
  NoopDDA,
  NoopDDA,
  ET4000FbInit,
  ET4000SetRead,
  ET4000SetWrite,
  ET4000SetReadWrite,
  0x10000,
  0x10000,
  16,
  0xFFFF,
  0x00000, 0x10000,
  0x00000, 0x10000,
  TRUE,                                 /* Uses 2 banks */
  VGA_NO_DIVIDE_VERT,
  {0,},
  8,
};

#define new ((vgaET4000Ptr)vgaNewVideoState)

Bool (*ClockSelect)();

static unsigned ET4000_ExtPorts[] = {0x3B8, 0x3BF, 0x3CD, 0x3D8};
static int Num_ET4000_ExtPorts = 
	(sizeof(ET4000_ExtPorts)/sizeof(ET4000_ExtPorts[0]));

/*
 * ET4000Ident
 */

static char *
ET4000Ident(n)
     int n;
{
  static char *chipsets[] = {"et4000"};

  if (n + 1 > sizeof(chipsets) / sizeof(char *))
    return(NULL);
  else
    return(chipsets[n]);
}


/*
 * ET4000ClockSelect --
 *      select one of the possible clocks ...
 */

static Bool
ET4000ClockSelect(no)
     int no;
{
  static unsigned char save1, save2, save3;
  unsigned char temp;

  switch(no)
  {
    case CLK_REG_SAVE:
      save1 = inb(0x3CC);
      outb(vgaIOBase + 4, 0x34); save2 = inb(vgaIOBase + 5);
      outb(0x3C4, 7); save3 = inb(0x3C5);
      break;
    case CLK_REG_RESTORE:
      outb(0x3C2, save1);
      outw(vgaIOBase + 4, 0x34 | (save2 << 8));
      outw(0x3C4, 7 | (save3 << 8));
      break;
    default:
      temp = inb(0x3CC);
      outb(0x3C2, ( temp & 0xf3) | ((no << 2) & 0x0C));
      outw(vgaIOBase + 4, 0x34 | ((no & 0x04) << 7));

      outb(0x3C4, 7); temp = inb(0x3C5);
      outb(0x3C5, (save_divide ^ ((no & 0x08) << 3)) | (temp & 0xBF));
  }
  return(TRUE);
}



/*
 * LegendClockSelect --
 *      select one of the possible clocks ...
 */

static Bool
LegendClockSelect(no)
     int no;
{
  /*
   * Sigma Legend special handling
   *
   * The Legend uses an ICS 1394-046 clock generator.  This can generate 32
   * different frequencies.  The Legend can use all 32.  Here's how:
   *
   * There are two flip/flops used to latch two inputs into the ICS clock
   * generator.  The five inputs to the ICS are then
   *
   * ICS     ET-4000
   * ---     ---
   * FS0     CS0
   * FS1     CS1
   * FS2     ff0     flip/flop 0 output
   * FS3     CS2
   * FS4     ff1     flip/flop 1 output
   *
   * The flip/flops are loaded from CS0 and CS1.  The flip/flops are
   * latched by CS2, on the rising edge. After CS2 is set low, and then high,
   * it is then set to its final value.
   *
   */
  static unsigned char save1, save2;
  unsigned char temp;

  switch(no)
  {
    case CLK_REG_SAVE:
      save1 = inb(0x3CC);
      outb(vgaIOBase + 4, 0x34); save2 = inb(vgaIOBase + 5);
      break;
    case CLK_REG_RESTORE:
      outb(0x3C2, save1);
      outw(vgaIOBase + 4, 0x34 | (save2 << 8));
      break;
    default:
      temp = inb(0x3CC);
      outb(0x3C2, (temp & 0xF3) | ((no & 0x10) >> 1) | (no & 0x04));
      outw(vgaIOBase + 4, 0x0034);
      outw(vgaIOBase + 4, 0x0234);
      outw(vgaIOBase + 4, ((no & 0x08) << 6) | 0x34);
      outb(0x3C2, (temp & 0xF3) | ((no << 2) & 0x0C));
  }
  return(TRUE);
}



/*
 * ET4000Probe --
 *      check up whether a Et4000 based board is installed
 */

static Bool
ET4000Probe()
{
  int numClocks;

  /*
   * Set up I/O ports to be used by this card
   */
  xf86ClearIOPortList(vga256InfoRec.scrnIndex);
  xf86AddIOPorts(vga256InfoRec.scrnIndex, Num_VGA_IOPorts, VGA_IOPorts);
  xf86AddIOPorts(vga256InfoRec.scrnIndex, Num_ET4000_ExtPorts, ET4000_ExtPorts);

  if (vga256InfoRec.chipset)
    {
      if (StrCaseCmp(vga256InfoRec.chipset, ET4000Ident(0)))
	return (FALSE);
      else
	ET4000EnterLeave(ENTER);
    }
  else
    {
      unsigned char temp, origVal, newVal;

      ET4000EnterLeave(ENTER);
      /*
       * Check first that there is a ATC[16] register and then look at
       * CRTC[33]. If both are R/W correctly it's a ET4000 !
       */
      temp = inb(vgaIOBase+0x0A); 
      outb(0x3C0, 0x16 | 0x20); origVal = inb(0x3C1);
      outb(0x3C0, origVal ^ 0x10);
      outb(0x3C0, 0x16 | 0x20); newVal = inb(0x3C1);
      outb(0x3C0, origVal);
      if (newVal != (origVal ^ 0x10))
	{
	  ET4000EnterLeave(LEAVE);
	  return(FALSE);
	}

      outb(vgaIOBase+0x04, 0x33);          origVal = inb(vgaIOBase+0x05);
      outb(vgaIOBase+0x05, origVal ^ 0x0F); newVal = inb(vgaIOBase+0x05);
      outb(vgaIOBase+0x05, origVal);
      if (newVal != (origVal ^ 0x0F))
	{
	  ET4000EnterLeave(LEAVE);
	  return(FALSE);
	}
    }

  /*
   * Detect how much memory is installed
   */
  if (!vga256InfoRec.videoRam)
    {
      unsigned char config;
      
      outb(vgaIOBase+0x04, 0x37); config = inb(vgaIOBase+0x05);
      
      switch(config & 0x03) {
      case 1: vga256InfoRec.videoRam = 256; break;
      case 2: vga256InfoRec.videoRam = 512; break;
      case 3: vga256InfoRec.videoRam = 1024; break;
      }

      if (config & 0x80) vga256InfoRec.videoRam <<= 1;
    }

  if (OFLG_ISSET(OPTION_LEGEND, &vga256InfoRec.options))
    {
      ClockSelect = LegendClockSelect;
      numClocks   = 32;
    }
  else
    {
      ClockSelect = ET4000ClockSelect;
      numClocks   = 16;
    }
  
  if (OFLG_ISSET(OPTION_HIBIT_HIGH, &vga256InfoRec.options))
    {
      if (OFLG_ISSET(OPTION_HIBIT_LOW, &vga256InfoRec.options))
        {
          ET4000EnterLeave(LEAVE);
          FatalError(
             "\nOptions \"hibit_high\" and \"hibit_low\" are incompatible\n");
        }
      save_divide = 0x40;
    }
  else if (OFLG_ISSET(OPTION_HIBIT_LOW, &vga256InfoRec.options))
    save_divide = 0;
  else
    {
      /* Check for initial state of divide flag */
      outb(0x3C4, 7);
      save_divide = inb(0x3C5) & 0x40;
      ErrorF("%s %s: ET4000: Initial hibit state: %s\n", XCONFIG_PROBED,
             vga256InfoRec.name, save_divide & 0x40 ? "high" : "low");
    }

#ifndef MONOVGA
    /* Save initial RCConf value */
    outb(vgaIOBase + 4, 0x32); initialRCConf = inb(vgaIOBase + 5);
#endif

  if (!vga256InfoRec.clocks) vgaGetClocks(numClocks, ClockSelect);

  vga256InfoRec.chipset = ET4000Ident(0);
  vga256InfoRec.bankedMono = TRUE;

  /* Initialize option flags allowed for this driver */
  OFLG_SET(OPTION_LEGEND, &ET4000.ChipOptionFlags);
  OFLG_SET(OPTION_HIBIT_HIGH, &ET4000.ChipOptionFlags);
  OFLG_SET(OPTION_HIBIT_LOW, &ET4000.ChipOptionFlags);
#ifndef MONOVGA
  OFLG_SET(OPTION_FAST_DRAM, &ET4000.ChipOptionFlags);
#endif

  return(TRUE);
}


/*
 * ET4000FbInit --
 *	initialise the cfb SpeedUp functions
 */

static void
ET4000FbInit()
{
#ifndef MONOVGA
  int useSpeedUp;

  useSpeedUp = vga256InfoRec.speedup & SPEEDUP_ANYWIDTH;
  if (useSpeedUp && x386Verbose)
  {
    ErrorF("%s %s: ET4000: SpeedUps selected (Flags=0x%x)\n", 
	   OFLG_ISSET(XCONFIG_SPEEDUP,&vga256InfoRec.xconfigFlag) ?
           XCONFIG_GIVEN : XCONFIG_PROBED,
           vga256InfoRec.name, useSpeedUp);
  }
  if (useSpeedUp & SPEEDUP_FILLRECT)
  {
    cfbLowlevFuncs.fillRectSolidCopy = speedupcfbFillRectSolidCopy;
  }
  if (useSpeedUp & SPEEDUP_BITBLT)
  {
    cfbLowlevFuncs.doBitbltCopy = speedupcfbDoBitbltCopy;
  }
  if (useSpeedUp & SPEEDUP_LINE)
  {
    cfbLowlevFuncs.lineSS = speedupcfbLineSS;
    cfbLowlevFuncs.segmentSS = speedupcfbSegmentSS;
    cfbTEOps1Rect.Polylines = speedupcfbLineSS;
    cfbTEOps1Rect.PolySegment = speedupcfbSegmentSS;
    cfbTEOps.Polylines = speedupcfbLineSS;
    cfbTEOps.PolySegment = speedupcfbSegmentSS;
    cfbNonTEOps1Rect.Polylines = speedupcfbLineSS;
    cfbNonTEOps1Rect.PolySegment = speedupcfbSegmentSS;
    cfbNonTEOps.Polylines = speedupcfbLineSS;
    cfbNonTEOps.PolySegment = speedupcfbSegmentSS;
  }
  if (useSpeedUp & SPEEDUP_FILLBOX)
  {
    cfbLowlevFuncs.fillBoxSolid = speedupcfbFillBoxSolid;
  }
#endif /* MONOVGA */
}


/*
 * ET4000EnterLeave --
 *      enable/disable io-mapping
 */

static void 
ET4000EnterLeave(enter)
     Bool enter;
{
  unsigned char temp;

  if (enter)
    {
      xf86EnableIOPorts(vga256InfoRec.scrnIndex);

      vgaIOBase = (inb(0x3CC) & 0x01) ? 0x3D0 : 0x3B0;
      outb(0x3BF, 0x03);                           /* unlock ET4000 special */
      outb(vgaIOBase + 8, 0xA0);
      outb(vgaIOBase + 4, 0x11); temp = inb(vgaIOBase + 5);
      outb(vgaIOBase + 5, temp & 0x7F);
    }
  else
    {
      outb(vgaIOBase + 4, 0x11); temp = inb(vgaIOBase + 5);
      outb(vgaIOBase + 5, temp | 0x80);
      outb(vgaIOBase + 8, 0x00);
      outb(0x3D8, 0x29);
      outb(0x3BF, 0x01);                           /* relock ET4000 special */

      xf86DisableIOPorts(vga256InfoRec.scrnIndex);
    }
}





/*
 * ET4000Restore --
 *      restore a video mode
 */

static void 
ET4000Restore(restore)
  vgaET4000Ptr restore;
{
  unsigned char i;

  outb(0x3CD, 0x00); /* segment select */

  vgaHWRestore(restore);

  outw(0x3C4, (restore->StateControl << 8)  | 0x06);
  outw(0x3C4, (restore->AuxillaryMode << 8) | 0x07);
  i = inb(vgaIOBase + 0x0A); /* reset flip-flop */
  outb(0x3C0, 0x36); outb(0x3C0, restore->Misc);
  outw(vgaIOBase + 4, (restore->ExtStart << 8)      | 0x33);
  if (restore->std.NoClock >= 0)
    outw(vgaIOBase + 4, (restore->Compatibility << 8) | 0x34);
  outw(vgaIOBase + 4, (restore->OverflowHigh << 8)  | 0x35);
#ifndef MONOVGA
  if (OFLG_ISSET(OPTION_FAST_DRAM, &vga256InfoRec.options))
    outw(vgaIOBase + 4, (restore->RCConf << 8)  | 0x32);
#endif
  outb(0x3CD, restore->SegSel);

  /*
   * This might be required for the Legend clock setting method, but
   * should not be used for the "normal" case because the high order
   * bits are not set in NoClock when returning to text mode.
   */
  if (OFLG_ISSET(OPTION_LEGEND, &vga256InfoRec.options))
    if (restore->std.NoClock >= 0)
      {
	vgaProtect(TRUE);
        (ClockSelect)(restore->std.NoClock);
	vgaProtect(FALSE);
      }
}



/*
 * ET4000Save --
 *      save the current video mode
 */

static void *
ET4000Save(save)
     vgaET4000Ptr save;
{
  unsigned char             i;
  unsigned char             temp1, temp2;

  /*
   * we need this here , cause we MUST disable the ROM SYNC feature
   */
  outb(vgaIOBase + 4, 0x34); temp1 = inb(vgaIOBase + 5);
  outb(vgaIOBase + 5, temp1 & 0x0F);
  temp2 = inb(0x3CD); outb(0x3CD, 0x00); /* segment select */

  save = (vgaET4000Ptr)vgaHWSave(save, sizeof(vgaET4000Rec));
  save->Compatibility = temp1;
  save->SegSel = temp2;

  outb(vgaIOBase + 4, 0x33); save->ExtStart     = inb(vgaIOBase + 5);
  outb(vgaIOBase + 4, 0x35); save->OverflowHigh = inb(vgaIOBase + 5);
#ifndef MONOVGA
  if (OFLG_ISSET(OPTION_FAST_DRAM, &vga256InfoRec.options))
    outb(vgaIOBase + 4, 0x32); save->RCConf = inb(vgaIOBase + 5);
#endif
  outb(0x3C4, 6); save->StateControl  = inb(0x3C5);
  outb(0x3C4, 7); save->AuxillaryMode = inb(0x3C5);
  save->AuxillaryMode |= 0x14;
  i = inb(vgaIOBase + 0x0A); /* reset flip-flop */
  outb(0x3C0,0x36); save->Misc = inb(0x3C1); outb(0x3C0, save->Misc);

  return ((void *) save);
}



/*
 * ET4000Init --
 *      Handle the initialization of the VGAs registers
 */

static Bool
ET4000Init(mode)
     DisplayModePtr mode;
{
  if (!vgaHWInit(mode,sizeof(vgaET4000Rec)))
    return(FALSE);

#ifndef MONOVGA
  new->std.Attribute[16] = 0x01;  /* use the FAST 256 Color Mode */
  new->std.CRTC[19] = vga256InfoRec.virtualX >> 3;
#endif
  new->std.CRTC[20] = 0x60;
  new->std.CRTC[23] = 0xAB;
  new->StateControl = 0x00; 
  new->AuxillaryMode = 0xBC;
  new->ExtStart = 0x00;

  new->OverflowHigh = (mode->Flags & V_INTERLACE ? 0x80 : 0x00)
    | 0x10
      | ((mode->VSyncStart & 0x400) >> 7 )
	| (((mode->VDisplay -1) & 0x400) >> 8 )
	  | (((mode->VTotal -2) & 0x400) >> 9 )
	    | (((mode->VSyncStart) & 0x400) >> 10 );

#ifdef MONOVGA
  new->Misc = 0x00;
#else
  new->Misc = 0x80;
#endif

#ifndef MONOVGA
  if (OFLG_ISSET(OPTION_FAST_DRAM, &vga256InfoRec.options))
  {
    /*
     *  make sure Trsp is no more than 75ns
     *            Tcsw is 25ns
     *            Tcsp is 25ns
     *            Trcd is no more than 50ns
     * Timings assume SCLK = 40MHz
     *
     * Note, this is experimental, but works for me (DHD)
     */
    new->RCConf = initialRCConf;
    /* Tcsw, Tcsp, Trsp */
    new->RCConf &= ~0x1F;
    if (initialRCConf & 0x18)
      new->RCConf |= 0x08;
    /* Trcd */
    new->RCConf &= ~0x20;
  }
#endif
    
  /* Set clock-related registers when not Legend */
  if (!OFLG_ISSET(OPTION_LEGEND, &vga256InfoRec.options))
    if (new->std.NoClock >= 0)
    {
      new->AuxillaryMode = (save_divide ^ ((new->std.NoClock & 8) << 3)) |
                           (new->AuxillaryMode & 0xBF);
      new->Compatibility = (new->std.NoClock & 0x04) >> 1;
    }

  return(TRUE);
}



/*
 * ET4000Adjust --
 *      adjust the current video frame to display the mousecursor
 */

static void 
ET4000Adjust(x, y)
     int x, y;
{
#ifdef MONOVGA
  int Base = (y * vga256InfoRec.virtualX + x + 3) >> 3;
#else
  int Base = (y * vga256InfoRec.virtualX + x + 1) >> 2;
#endif

  outw(vgaIOBase + 4, (Base & 0x00FF00) | 0x0C);
  outw(vgaIOBase + 4, ((Base & 0x00FF) << 8) | 0x0D);
  outw(vgaIOBase + 4, ((Base & 0x030000) >> 8) | 0x33);
}



