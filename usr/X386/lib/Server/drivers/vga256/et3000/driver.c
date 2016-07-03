/*
 * $XFree86: mit/server/ddx/x386/vga256/drivers/et3000/driver.c,v 2.10 1993/09/22 15:47:12 dawes Exp $
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
 *
 * $Header: /proj/X11/mit/server/ddx/x386/drivers/et3000/RCS/driver.c,v 1.2 1991/06/27 00:03:27 root Exp $
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
  unsigned char ExtStart;     /* Tseng ET3000 specials   CRTC 0x23/0x24/0x25 */
  unsigned char CRTCControl;
  unsigned char Overflow;
  unsigned char ZoomControl;    /* TS 6 & 7 */
  unsigned char AuxillaryMode;
  unsigned char Misc;           /* ATC 0x16 */
  unsigned char SegSel;
  } vgaET3000Rec, *vgaET3000Ptr;


static Bool     ET3000Probe();
static char *   ET3000Ident();
static Bool     ET3000ClockSelect();
static void     ET3000EnterLeave();
static Bool     ET3000Init();
static void *   ET3000Save();
static void     ET3000Restore();
static void     ET3000Adjust();
extern void     ET3000SetRead();
extern void     ET3000SetWrite();
extern void     ET3000SetReadWrite();

vgaVideoChipRec ET3000 = {
  ET3000Probe,
  ET3000Ident,
  ET3000EnterLeave,
  ET3000Init,
  ET3000Save,
  ET3000Restore,
  ET3000Adjust,
  NoopDDA,
  NoopDDA,
  NoopDDA,
  ET3000SetRead,
  ET3000SetWrite,
  ET3000SetReadWrite,
  0x10000,
  0x10000,
  16,
  0xFFFF,
  0x00000, 0x10000,
  0x00000, 0x10000,
  TRUE,                               /* Uses 2 banks */
  VGA_DIVIDE_VERT,
  {0,},
  16,
};

#define new ((vgaET3000Ptr)vgaNewVideoState)

static unsigned ET3000_ExtPorts[] = {0x3B8, 0x3BF, 0x3CD, 0x3D8};
static int Num_ET3000_ExtPorts = 
	(sizeof(ET3000_ExtPorts)/sizeof(ET3000_ExtPorts[0]));

/*
 * ET3000ClockSelect --
 *      select one of the possible clocks ...
 */

static Bool
ET3000ClockSelect(no)
     int no;
{
  static unsigned char save1, save2;
  unsigned char temp;

  switch(no)
  {
    case CLK_REG_SAVE:
      save1 = inb(0x3CC);
      outb(vgaIOBase + 4, 0x24); save2 = inb(vgaIOBase + 5);
      break;
    case CLK_REG_RESTORE:
      outb(0x3C2, save1);
      outw(vgaIOBase + 4, 0x24 | (save2 << 8));
      break;
    default:
      temp = inb(0x3CC);
      outb(0x3C2, ( temp & 0xf3) | ((no << 2) & 0x0C));
      outw(vgaIOBase+0x04, 0x24 | ((no & 0x04) << 7));
  }
  return(TRUE);
}


/*
 * ET3000Ident --
 */

static char *
ET3000Ident(n)
     int n;
{
  static char *chipsets[] = {"et3000"};

  if (n + 1 > sizeof(chipsets) / sizeof(char *))
    return(NULL);
  else
    return(chipsets[n]);
}


/*
 * ET3000Probe --
 *      check up whether a Et3000 based board is installed
 */

static Bool
ET3000Probe()
{
  /*
   * Set up I/O ports to be used by this card
   */
  xf86ClearIOPortList(vga256InfoRec.scrnIndex);
  xf86AddIOPorts(vga256InfoRec.scrnIndex, Num_VGA_IOPorts, VGA_IOPorts);
  xf86AddIOPorts(vga256InfoRec.scrnIndex, Num_ET3000_ExtPorts, ET3000_ExtPorts);

  if (vga256InfoRec.chipset)
    {
      if (StrCaseCmp(vga256InfoRec.chipset, ET3000Ident(0)))
	return (FALSE);
      else
	ET3000EnterLeave(ENTER);
    }
  else
    {
      unsigned char temp, origVal, newVal;

      ET3000EnterLeave(ENTER);
      origVal = inb(0x3CD);
      outb(0x3CD, origVal ^ 0x3F);
      newVal = inb(0x3CD);
      outb(0x3CD, origVal);
      if (newVal != (origVal ^ 0x3F))
	{
	  ET3000EnterLeave(LEAVE);
	  return(FALSE);
	}
      /*
       * At this stage, we think it's a Tseng, now make sure it's
       * an ET3000.
       */
      temp = inb(vgaIOBase+0x0A);
      outb(vgaIOBase+0x04, 0x1B);
      origVal = inb(vgaIOBase+0x05);
      outb(vgaIOBase+0x05, origVal ^ 0xFF);
      newVal = inb(vgaIOBase+0x05);
      outb(vgaIOBase+0x05, origVal);
      if (newVal != (origVal ^ 0xFF))
	{
	  ET3000EnterLeave(LEAVE);
	  return(FALSE);
	}
    }

  if (!vga256InfoRec.videoRam) vga256InfoRec.videoRam = 512;
  if (!vga256InfoRec.clocks) vgaGetClocks(8 , ET3000ClockSelect);

  vga256InfoRec.chipset = ET3000Ident(0);
  vga256InfoRec.bankedMono = TRUE;
  return(TRUE);
}



/*
 * ET3000EnterLeave --
 *      enable/disable io-mapping
 */

static void 
ET3000EnterLeave(enter)
     Bool enter;
{
  unsigned char temp;

  if (enter)
    {
      xf86EnableIOPorts(vga256InfoRec.scrnIndex);

      vgaIOBase = (inb(0x3CC) & 0x01) ? 0x3D0 : 0x3B0;
      outb(0x3BF, 0x03);                           /* unlock ET3000 special */
      outb(vgaIOBase + 8, 0xA0);
      outb(vgaIOBase + 4, 0x11); temp = inb(vgaIOBase + 5);
      outb(vgaIOBase + 5, temp & 0x7F);
    }
  else
    {
      outb(0x3BF, 0x01);                           /* relock ET3000 special */
      outb(vgaIOBase + 8, 0xA0);

      xf86DisableIOPorts(vga256InfoRec.scrnIndex);
    }
}



/*
 * ET3000Restore --
 *      restore a video mode
 */

static void 
ET3000Restore(restore)
  vgaET3000Ptr restore;
{
  unsigned char i;

  outb(0x3CD, restore->SegSel);

  outw(0x3C4, (restore->ZoomControl << 8)   | 0x06);
  outw(0x3C4, (restore->AuxillaryMode << 8) | 0x07);
  i = inb(vgaIOBase + 0x0A); /* reset flip-flop */
  outb(0x3C0, 0x36); outb(0x3C0, restore->Misc);
  outw(vgaIOBase + 4, (restore->ExtStart << 8)    | 0x23);
  if (restore->std.NoClock >= 0)
    outw(vgaIOBase + 4, (restore->CRTCControl << 8) | 0x24);
  outw(vgaIOBase + 4, (restore->Overflow << 8)    | 0x25);

  vgaHWRestore(restore);
}



/*
 * ET3000Save --
 *      save the current video mode
 */

static void *
ET3000Save(save)
     vgaET3000Ptr save;
{
  unsigned char             i;
  unsigned char             temp1, temp2;

  /*
   * we need this here , cause we MUST disable the ROM SYNC feature
   */
  vgaIOBase = (inb(0x3CC) & 0x01) ? 0x3D0 : 0x3B0;
  outb(vgaIOBase + 4, 0x24); temp1 = inb(vgaIOBase + 5);
  outb(vgaIOBase + 5, temp1 & 0x0f);
  temp2 = inb(0x3CD);

  outb(0x3CD,0x40); /* segment select */

  save = (vgaET3000Ptr)vgaHWSave(save, sizeof(vgaET3000Rec));
  save->CRTCControl = temp1;
  save->SegSel = temp2;

  outb(vgaIOBase + 4, 0x23); save->ExtStart = inb(vgaIOBase + 5);
  outb(vgaIOBase + 4, 0x25); save->Overflow = inb(vgaIOBase + 5);
  outb(0x3C4, 6); save->ZoomControl = inb(0x3C5);
  outb(0x3C4, 7); save->AuxillaryMode = inb(0x3C5);
  i = inb(vgaIOBase + 0x0A); /* reset flip-flop */
  outb(0x3C0, 0x36); save->Misc = inb(0x3C1); outb(0x3C0, save->Misc);
  
  return ((void *) save);
}



/*
 * ET3000Init --
 *      Handle the initialization, etc. of a screen.
 */

static Bool
ET3000Init(mode)
     DisplayModePtr mode;
{
#ifdef MONOVGA
  Bool first_time = (!new);
  int i;
#endif
   
#ifdef MONOVGA
  /* weird mode, halve the horizontal timings */
  mode->HTotal /= 2;
  mode->HDisplay /= 2;
  mode->HSyncStart /= 2;
  mode->HSyncEnd /= 2;
#endif
  if (!vgaHWInit(mode,sizeof(vgaET3000Rec)))
    return(FALSE);
#ifdef MONOVGA
  /* restore... */
  mode->HTotal *= 2;
  mode->HDisplay *= 2;
  mode->HSyncStart *= 2;
  mode->HSyncEnd *= 2;
#endif

#ifndef MONOVGA
  new->std.Sequencer[4] = 0x06;  /* use the FAST 256 Color Mode */
#endif
  new->ZoomControl   = 0x00; 
  new->Misc          = 0x10;
  new->ExtStart      = 0x00;
  if (new->std.NoClock >= 0)
    new->CRTCControl   = (int)(new->std.NoClock & 0x04) >> 1 ;
  new->Overflow      = 0x10
    | ((mode->VSyncStart & 0x400) >> 7 )
      | (((mode->VDisplay -1) & 0x400) >> 8 )
	| (((mode->VTotal -2) & 0x400) >> 9 )
	  | ((mode->VSyncStart & 0x400) >> 10 )
	    | (mode->Flags & V_INTERLACE ? 0x80 : 0);

#ifdef MONOVGA
  new->AuxillaryMode = 0xc8;	/* MCLK / 2, devil knows what's that */
  new->std.CRTC[5] |= 0x60;	/* Hsync skew */
  new->std.CRTC[19] = vga256InfoRec.virtualX >> 5;
  /* This weird mode uses the DAC in an unusual way */
  if (first_time)
  {
    for (i = 0; i < 768; i+=3)
      if ((i/3) & (1 << BIT_PLANE))
      {
        new->std.DAC[i] = vga256InfoRec.whiteColour.red;
        new->std.DAC[i + 1] = vga256InfoRec.whiteColour.green;
        new->std.DAC[i + 2] = vga256InfoRec.whiteColour.blue;
      }
      else
      {
        new->std.DAC[i] = vga256InfoRec.blackColour.red;
        new->std.DAC[i + 1] = vga256InfoRec.blackColour.green;
        new->std.DAC[i + 2] = vga256InfoRec.blackColour.blue;
      }
  }
  new->std.Attribute[17] = 0x00;
   
#else /* !MONOVGA */
  new->AuxillaryMode = 0x88;
#endif
  return(TRUE);
}


/*
 * ET3000Adjust --
 *      adjust the current video frame to display the mousecursor
 */

static void 
ET3000Adjust(x, y)
     int x, y;
{
#ifdef USE_PAN
#ifdef MONOVGA
  int Base = (y * vga256InfoRec.virtualX + x + 3) >> 4;
  int wants_pan = (y * vga256InfoRec.virtualX + x + 3) & 8;
#else
  int Base = (y * vga256InfoRec.virtualX + x + 1) >> 3;
  int wants_pan = (y * vga256InfoRec.virtualX + x + 1) & 4;
#endif
#else
#ifdef MONOVGA
  int Base = (y * vga256InfoRec.virtualX + x + 7) >> 4;
#else
  int Base = (y * vga256InfoRec.virtualX + x + 3) >> 3;
#endif
#endif

#ifdef USE_PAN
  /* Wait for vertical retrace */
  {
    unsigned char tmp;

    while (tmp = inb(vgaIOBase + 0x0A) & 0x80)
      ;
  }
#endif

  outw(vgaIOBase + 4, (Base & 0x00FF00)        | 0x0C);
  outw(vgaIOBase + 4, ((Base & 0x00FF) << 8)   | 0x0D);
  outw(vgaIOBase + 4, ((Base & 0x010000) >> 7) | 0x23);

#ifdef USE_PAN
  /* set horizontal panning register */
  (void)inb(vgaIOBase + 0x0A);
  outb(0x3C0, 0x33);
  outb(0x3C0, wants_pan ? 3 : 0);
#endif
}



