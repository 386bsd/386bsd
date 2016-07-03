/*
 * $XFree86: mit/server/ddx/x386/vga256/drivers/gvga/driver.c,v 2.9 1993/09/22 15:47:29 dawes Exp $
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
 * $Header: /proj/X11/mit/server/ddx/x386/drivers/gvga/RCS/driver.c,v 1.2 1991/06/27 00:03:49 root Exp $
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
  vgaHWRec      std;          /* good old IBM VGA */
  unsigned char ExtCtrlReg1;
  unsigned char ExtCtrlReg2;
  unsigned char ExtCtrlReg3;
  unsigned char ExtCtrlReg4;
  unsigned char ExtCtrlReg5;
  unsigned char MemSegReg;
  } vgaGVGARec, *vgaGVGAPtr;


static Bool     GVGAProbe();
static char *   GVGAIdent();
static Bool     GVGAClockSelect();
static void     GVGAEnterLeave();
static Bool     GVGAInit();
static void *   GVGASave();
static void     GVGARestore();
static void     GVGAAdjust();
extern void     GVGASetRead();
extern void     GVGASetWrite();
extern void     GVGASetReadWrite();

vgaVideoChipRec GVGA = {
  GVGAProbe,
  GVGAIdent,
  GVGAEnterLeave,
  GVGAInit,
  GVGASave,
  GVGARestore,
  GVGAAdjust,
  NoopDDA,
  NoopDDA,
  NoopDDA,
  GVGASetRead,
  GVGASetWrite,
  GVGASetReadWrite,
  0x10000,
  0x10000,
  16,
  0xFFFF,
  0x00000, 0x10000,
  0x00000, 0x10000,
  TRUE,                                 /* Uses 2 banks */
  VGA_NO_DIVIDE_VERT,
  {0,},
  16,
};


#define new ((vgaGVGAPtr)vgaNewVideoState)


/*
 * GVGAIdent
 */

static char *
GVGAIdent(n)
     int n;
{
  static char *chipsets[] = {"gvga"};

  if (n + 1 > sizeof(chipsets) / sizeof(char *))
    return(NULL);
  else
    return(chipsets[n]);
}


/*
 * GVGAClockSelect --
 *      select one of the possible clocks ...
 */

static Bool
GVGAClockSelect(no)
     int no;
{
  static unsigned char save1, save2;
  unsigned char temp;

  switch(no)
  {
    case CLK_REG_SAVE:
      save1 = inb(0x3CC);
      outb(0x3C4, 0x07); save2 = inb(0x3C5);
      break;
    case CLK_REG_RESTORE:
      outb(0x3C2, save1);
      outw(0x3C4, 0x07 | (save2 << 8));
      break;
    default:
      temp = inb(0x3CC);
      outb(0x3C2, ( temp & 0xf3) | ((no << 2) & 0x0C));
      outb(0x3C4, 0x07);
      temp = inb(0x3C5);
      outb(0x3C5, (temp & 0xFE) | ((no & 0x04) >> 2));
  }
  return(TRUE);
}



/*
 * GVGAProbe --
 *      check up whether a GVGA based board is installed
 */

static Bool
GVGAProbe()
{
  /*
   * Set up I/O ports to be used by this card
   */
  xf86ClearIOPortList(vga256InfoRec.scrnIndex);
  xf86AddIOPorts(vga256InfoRec.scrnIndex, Num_VGA_IOPorts, VGA_IOPorts);

  if (vga256InfoRec.chipset)
    {
      if (StrCaseCmp(vga256InfoRec.chipset, GVGAIdent(0)))
	return (FALSE);

      if (!vga256InfoRec.videoRam) vga256InfoRec.videoRam = 512;
    }
  else
    {
      unsigned char offset, signature[4];

      if (xf86ReadBIOS(vga256InfoRec.BIOSbase, 0x37, &offset, 1) != 1)
	return(FALSE);
      if (xf86ReadBIOS(vga256InfoRec.BIOSbase, offset, signature, 4) != 4)
	return(FALSE);

      if ((signature[0] != 0x77) ||
	  ((signature[1] == 0x33) || (signature[1] == 0x55)) ||
	  ((signature[2] != 0x66) && (signature[2] != 0x99)) ||
	  ((signature[3] != 0x99) && (signature[3] != 0x66)))
	return(FALSE);

      if (!vga256InfoRec.videoRam)

	switch(signature[1]) {
	case 0x00:
	case 0x22: 
	  vga256InfoRec.videoRam = 256;
	  break;

	case 0x11:
	default:  
	  vga256InfoRec.videoRam = 512;
	  break;
	}
    }

  GVGAEnterLeave(ENTER);

  if (!vga256InfoRec.clocks) vgaGetClocks(8, GVGAClockSelect);

  vga256InfoRec.chipset = GVGAIdent(0);
  vga256InfoRec.bankedMono = TRUE;
  return(TRUE);
}


/*
 * GVGAEnterLeave --
 *      enable/disable io-mapping
 */

static void 
GVGAEnterLeave(enter)
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
    xf86DisableIOPorts(vga256InfoRec.scrnIndex);
}



/*
 * GVGARestore --
 *      restore a video mode
 */

static void 
GVGARestore(restore)
  vgaGVGAPtr restore;
{
  outw(0x3C4, 0x0006);  /* segment select */

  vgaHWRestore(restore);

  outw(vgaIOBase + 4, (restore->ExtCtrlReg1 << 8) | 0x2F);
  outw(0x3C4, (restore->MemSegReg << 8)   | 0x06);
  if (restore->std.NoClock >= 0)
    outw(0x3C4, (restore->ExtCtrlReg2 << 8) | 0x07);
  outw(0x3C4, (restore->ExtCtrlReg3 << 8) | 0x08);
  outw(0x3C4, (restore->ExtCtrlReg4 << 8) | 0x10);
  outw(0x3CE, (restore->ExtCtrlReg5 << 8) | 0x09);
}



/*
 * GVGASave --
 *      save the current video mode
 */

static void *
GVGASave(save)
     vgaGVGAPtr save;
{
  unsigned char             temp;

  outb(0x3C4, 0x06); temp = inb(0x3C5); outb(0x3C5, 0x00); /* segment select */

  save = (vgaGVGAPtr)vgaHWSave(save, sizeof(vgaGVGARec));
  save->MemSegReg = temp;

  outb(vgaIOBase + 4, 0x2f); save->ExtCtrlReg1 = inb(vgaIOBase + 5);
  outb(0x3C4, 0x07); save->ExtCtrlReg2 = inb(0x3C5);
  outb(0x3C4, 0x08); save->ExtCtrlReg3 = inb(0x3C5);
  outb(0x3C4, 0x10); save->ExtCtrlReg4 = inb(0x3C5);
  outb(0x3CE, 0x09); save->ExtCtrlReg5 = inb(0x3CF);
  
  return ((void *) save);
}



/*
 * GVGAInit --
 *      Handle the initialization, etc. of a screen.
 */

static Bool
GVGAInit(mode)
     DisplayModePtr mode;
{
  if (!vgaHWInit(mode,sizeof(vgaGVGARec)))
    return(FALSE);

#ifndef MONOVGA
  new->std.Sequencer[4] = 0x06;  /* use the FAST 256 Color Mode */
  new->std.Attribute[16] = 0x01;
  new->ExtCtrlReg1 = 0x02;
#else
  new->ExtCtrlReg1 = 0x00;  /* Not sure about this (DHD) */
#endif
  new->ExtCtrlReg2 = 0x06 |
      (new->std.NoClock >= 0) ? ((int)(new->std.NoClock & 0x04) >> 2) : 0;
  new->ExtCtrlReg3 = 0x20;
  new->ExtCtrlReg4 = 0x24;
  new->ExtCtrlReg5 = 0x08;
  return(TRUE);
}



/*
 * GVGAAdjust --
 *      adjust the current video frame to display the mousecursor
 */

static void 
GVGAAdjust(x, y)
     int x, y;
{
  int Base =(y * vga256InfoRec.virtualX + x + 3) >> 3;

  outw(vgaIOBase + 4, (Base & 0x00FF00)      | 0x0C);
  outw(vgaIOBase + 4, ((Base & 0x00FF) << 8) | 0x0D);
}

