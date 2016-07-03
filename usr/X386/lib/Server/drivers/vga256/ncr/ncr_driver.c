/* Copyright 1992 NCR Corporation - Dayton, Ohio, USA */

/* $XFree86: mit/server/ddx/x386/vga256/drivers/ncr/ncr_driver.c,v 2.9 1993/09/22 15:47:40 dawes Exp $ */

/*
 * Copyright 1992,1993 NCR Corporation, Inc.
 *
 * Permission to use, copy, modify, and distribute this software and
 * it's documentation for any purpose and without fee is hereby
 * granted, provided that the above copyright notice appear in all
 * copies and that both that copyright notice and this permission
 * notice appear in supporting documentation, and that the name of
 * NCR not be used in advertising or publicity
 * pertaining to distribution of the software without specific,
 * written prior permission.  NCR makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * NCR DISCLAIMs ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL NCR BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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
  vgaHWRec      std;		/* std IBM VGA register */
  unsigned char PHostOffsetH;	/* High Byte */
  unsigned char PHostOffsetL;	/* Low  Byte - Usually 0 */
  unsigned char SHostOffsetH;	/* High Byte */
  unsigned char SHostOffsetL;	/* Low  Byte - Usually 0 */
  unsigned char ExtModes;
  unsigned char CurForeColor;
  unsigned char CurBackColor;
  unsigned char CurControl;
  unsigned char CurXLocH;
  unsigned char CurXLocL;
  unsigned char CurYLocH;
  unsigned char CurYLocL;
  unsigned char CurXIndex;
  unsigned char CurYIndex;
  unsigned char CurStoreH;
  unsigned char CurStoreL;
  unsigned char CurStOffH;
  unsigned char CurStOffL;
  unsigned char CurMask;
  unsigned char DispOffH;
  unsigned char DispOffL;
  unsigned char MemEnable;
  unsigned char ExtClock;
  unsigned char ExtVMemAddr;
  unsigned char ExtPixel;
  unsigned char BusWidth;
  unsigned char ExtHTime;
  unsigned char ExtDispPos;
} vgaNCRRec, *vgaNCRPtr;

static Bool  NCRProbe();
static char *NCRIdent();
static Bool  NCRClockSelect();
static void  NCREnterLeave();
static Bool  NCRInit();
static void *NCRSave();
static void  NCRRestore();
static void  NCRAdjust();
#ifndef MONOVGA
extern void  NCRSetRead();
extern void  NCRSetWrite();
extern void  NCRSetReadWrite();
#endif


vgaVideoChipRec NCR = {
  NCRProbe,
  NCRIdent,
  NCREnterLeave,
  NCRInit,
  NCRSave,
  NCRRestore,
  NCRAdjust,
  NoopDDA,
  NoopDDA,
  NoopDDA,
#ifdef MONOVGA
  NoopDDA,
  NoopDDA,
  NoopDDA,
#else
  NCRSetRead,
  NCRSetWrite,
  NCRSetReadWrite,
#endif
  0x20000,	/* ChipMapSize */
  0x20000,	/* ChipSegmentSize */
  17,		/* ChipSegmentShift */
  0x1FFFF,	/* ChipSegmentMask */
  0x00000, 0x20000,	/* ChipReadBottom, ChipReadTop */
  0x00000, 0x20000,	/* ChipWriteBottom, ChipWriteTop */
  TRUE,		/* Uses 2 banks */
  VGA_DIVIDE_VERT,	/* Interlace (77C22E,E+ only) requires divide-by-2 */
  {0,},
  8,
};

#define new ((vgaNCRPtr)vgaNewVideoState)

#define NCR77C22	0
#define NCR77C22E	1
static unsigned char NCRchipset;

/*
 * NCRIdent
 */
static char *
NCRIdent(n)
     int n;
{
  static char *chipsets[] = {"ncr77c22", "ncr77c22e"};

  if (n + 1 > sizeof(chipsets) / sizeof(char *))
    return(NULL);
  else
    return(chipsets[n]);
}

/*
 * NCRClockSelect --
 *      select one of the possible clocks ...
 */
static Bool
NCRClockSelect(no)
     int no;
{
  static unsigned char save1, save2;
  unsigned char temp;

  switch(no)
  {
    case CLK_REG_SAVE:
      save1 = inb(0x3CC);
      if (NCRchipset == NCR77C22E)
      {
        outb(0x3C4, 0x1F);
	save2 = inb(0x3C5);
      }
      break;
    case CLK_REG_RESTORE:
      outb(0x3C2, save1);
      if (NCRchipset == NCR77C22E)
      {
	outw(0x3C4, (save2 << 8) | 0x1F);
      }
      break;
    default:
      temp = inb(0x3CC);
      outb(0x3C2, (temp & 0xF3) | ((no << 2) & 0x0C));
      if (NCRchipset == NCR77C22E)
      {
	outb(0x3C4, 0x1F);
	temp = inb(0x3C5);
	outb(0x3C5, (temp & 0xBF) | ((no << 4) & 0x40));
      }
  }
  return(TRUE);
}


/*
 * NCRProbe --
 *      check whether a 77C22 Chip is installed
 */
static	Bool
NCRProbe()
{
    unsigned char temp;
    int	version;
    int numClocks;
    int flag = FALSE;

    /*
     * Set up I/O ports to be used by this card
     */
    xf86ClearIOPortList(vga256InfoRec.scrnIndex);
    xf86AddIOPorts(vga256InfoRec.scrnIndex, Num_VGA_IOPorts, VGA_IOPorts);

    if (vga256InfoRec.chipset)
    {
	if (!StrCaseCmp(vga256InfoRec.chipset, "ncrvga"))
	{
	    ErrorF("\n'ncrvga' is obsolete.  Please use a chipset name\n");
	    ErrorF("shown by running the server with '-showconfig'\n");
	    return(FALSE);
	}
        if (!StrCaseCmp(vga256InfoRec.chipset, NCRIdent(0)))
	{
	    NCRchipset = NCR77C22;
	}
	else if (!StrCaseCmp(vga256InfoRec.chipset, NCRIdent(1)))
	{
	    NCRchipset = NCR77C22E;
	}
	else
	{
	    return(FALSE);
	}
        NCREnterLeave(ENTER);
    }
    else
    {
        NCREnterLeave(ENTER);
        if (testinx2(0x3C4,0x05,0x05))
	{
	    wrinx(0x3C4, 0x05, 0x00);		/* Disable extended regs */
	    if (!testinx(0x3C4, 0x10))
	    {
		wrinx(0x3C4, 0x05, 0x01);	/* Enable extended regs */
		if (testinx(0x3C4, 0x10))
		{
		    /*
		     * It's NCR.  Let's figure out which one.
		     */
		    temp = rdinx(0x3C4, 0x08);
		    switch (temp >> 4)
		    {
		    case 0:
		        flag = TRUE;
			NCRchipset = NCR77C22;
			break;
		    case 2:
			/*
			 * If it's ever necessary to distinguish the 22E from
			 * the 22E+, ((temp & 0x0F) >= 0x08) implies 22E+.
			 */
			flag = TRUE;
			NCRchipset = NCR77C22E;
			break;
		    case 1:
		    default:
			/*
			 * 1 is NCR77C21, whatever that is.  Others are
			 * undefined.
			 */
			break;
		    }
		}
	    }
	}
	if (!flag)
	{
            NCREnterLeave(LEAVE);
            return(FALSE);
	}
    }

    version = rdinx(0x3C4, 0x08); 
    ErrorF("NCR 77C22 Type %x, Version %x\n", (version&0xF0)>>4, version&0x0F);

    /*
     * Detect how much memory is installed
     */
    if (!vga256InfoRec.videoRam)
    {
        unsigned char config;

	config = rdinx(0x3C4, 0x1E);
        outb(0x3C4, 0x1E); config = inb(0x3C5);
      
        switch(config & 0x03) 
	{
        case 0x00:
	    vga256InfoRec.videoRam = 256;
	    break;
        case 0x01:
        case 0x03:
	    vga256InfoRec.videoRam = 1024;
	    break;
        case 0x02:
            Error("Invalid Memeory Configuration");
            ErrorF(" (this shouldn't happen on the 77C22)\n");
    	    NCREnterLeave(LEAVE);
	    return(FALSE);
        }
    }

    numClocks = (NCRchipset == NCR77C22) ? 4 : 8;

    if (!vga256InfoRec.clocks) vgaGetClocks(numClocks, NCRClockSelect);

    vga256InfoRec.chipset = NCRIdent(NCRchipset);
    vga256InfoRec.bankedMono = FALSE;
    return(TRUE);
}



/*
 * NCREnterLeave --
 *      enable/disable io-mapping
 */

static void 
NCREnterLeave(enter)
     Bool enter;
{
  unsigned char temp;

  if (enter)
    {
      xf86EnableIOPorts(vga256InfoRec.scrnIndex);

      outw(0x3C4,0x0105); /* Enable extended Registers */
      vgaIOBase = (inb(0x3CC) & 0x01) ? 0x3D0 : 0x3B0;
      outb(vgaIOBase + 4, 0x11); temp = inb(vgaIOBase + 5);
      outb(vgaIOBase + 5, temp & 0x7F);
    }
  else
    {
      outw(0x3C4,0x0005); /* Disable extended Registers */
      xf86DisableIOPorts(vga256InfoRec.scrnIndex);
    }
}



/*
 * NCRRestore --
 *      restore a video mode
 */

static void
NCRRestore(restore)
     vgaNCRPtr restore;
{
  unsigned char temp;

  /*
   * First Enable the extended registers
   */
  outw(0x3C4,0x0105);	/* Enable extended Registers */

  outw(0x3C4,0x0019);	/* Clear low byte of host offsets */
  outw(0x3C4,0x001D);

  outw(0x3C4,0x0018);	/* Force host offsets to first segment */
  outw(0x3C4,0x001C);

  vgaHWRestore(restore);

  outw(0x3C4, 0x0A|(restore->CurForeColor<<8));
  outw(0x3C4, 0x0B|(restore->CurBackColor<<8));
  outw(0x3C4, 0x0C|(restore->CurControl<<8));
  outw(0x3C4, 0x0D|(restore->CurXLocH<<8));
  outw(0x3C4, 0x0E|(restore->CurXLocL<<8));
  outw(0x3C4, 0x0F|(restore->CurYLocH<<8));
  outw(0x3C4, 0x10|(restore->CurYLocL<<8));
  outw(0x3C4, 0x11|(restore->CurXIndex<<8));
  outw(0x3C4, 0x12|(restore->CurYIndex<<8));
  outw(0x3C4, 0x13|(restore->CurStoreH<<8));
  outw(0x3C4, 0x14|(restore->CurStoreL<<8));
  outw(0x3C4, 0x15|(restore->CurStOffH<<8));
  outw(0x3C4, 0x16|(restore->CurStOffL<<8));
  outw(0x3C4, 0x17|(restore->CurMask<<8));
  outw(0x3C4, 0x18|(restore->PHostOffsetH<<8));
  outw(0x3C4, 0x19|(restore->PHostOffsetL<<8));
  outw(0x3C4, 0x1A|(restore->DispOffH<<8));
  outw(0x3C4, 0x1B|(restore->DispOffL<<8));
  outw(0x3C4, 0x1C|(restore->SHostOffsetH<<8));
  outw(0x3C4, 0x1D|(restore->SHostOffsetL<<8));
  outw(0x3C4, 0x1E|(restore->MemEnable<<8));
  /*
   * Extended Clocking register has 3rd clock select bit for 77C22E, so we
   * have to be very careful here.  Also, bits 5 & 7 must be protected.
   */
  outb(0x3C4, 0x1F);
  temp = inb(0x3C5);
  temp = (temp & 0xE0) | (restore->ExtClock & 0x1F);
  if (NCRchipset == NCR77C22E)
  {
    if (restore->std.NoClock >= 0)
    {
      temp = (temp & 0xBF) | (restore->ExtClock & 0x40);
    }
  }
  outb(0x3C5, temp);
  outw(0x3C4, 0x20|(restore->ExtVMemAddr<<8));
  outw(0x3C4, 0x21|(restore->ExtPixel<<8));
  outw(0x3C4, 0x22|(restore->BusWidth<<8));

  outw(vgaIOBase+4, 0x30|(restore->ExtHTime<<8));
  outw(vgaIOBase+4, 0x31|(restore->ExtDispPos<<8));

  outw(0x3C4, 0x05|(restore->ExtModes<<8)); /* restore this last because it */
					    /* may lock out the extended    */
					    /* registers when set.          */
}



/*
 * NCRSave --
 *      save the current video mode
 */

static void *
NCRSave(save)
     vgaNCRPtr save;
{
  unsigned char PHostOffsetH, PHostOffsetL, SHostOffsetH, SHostOffsetL, temp;

  outb(0x3C4,0x18); PHostOffsetH = inb(0x3C5); outb(0x3C5, 0x00);
  outb(0x3C4,0x19); PHostOffsetL = inb(0x3C5); outb(0x3C5, 0x00);
  outb(0x3C4,0x1C); SHostOffsetH = inb(0x3C5); outb(0x3C5, 0x00);
  outb(0x3C4,0x1D); SHostOffsetL = inb(0x3C5); outb(0x3C5, 0x00);

  save = (vgaNCRPtr)vgaHWSave(save, sizeof(vgaNCRRec));

  outb(0x3C4, 0x05); save->ExtModes = inb(0x3C5);
  save->PHostOffsetH = PHostOffsetH;
  save->PHostOffsetL = PHostOffsetL;
  save->SHostOffsetH = SHostOffsetH;
  save->SHostOffsetL = SHostOffsetL;
  outb(0x3C4, 0x0A); save->CurForeColor = inb(0x3C5);
  outb(0x3C4, 0x0B); save->CurBackColor = inb(0x3C5);
  outb(0x3C4, 0x0C); save->CurControl = inb(0x3C5);
  outb(0x3C4, 0x0D); save->CurXLocH = inb(0x3C5);
  outb(0x3C4, 0x0E); save->CurXLocL = inb(0x3C5);
  outb(0x3C4, 0x0F); save->CurYLocH = inb(0x3C5);
  outb(0x3C4, 0x10); save->CurYLocL = inb(0x3C5);
  outb(0x3C4, 0x11); save->CurXIndex = inb(0x3C5);
  outb(0x3C4, 0x12); save->CurYIndex = inb(0x3C5);
  outb(0x3C4, 0x13); save->CurStoreH = inb(0x3C5);
  outb(0x3C4, 0x14); save->CurStoreL = inb(0x3C5);
  outb(0x3C4, 0x15); save->CurStOffH = inb(0x3C5);
  outb(0x3C4, 0x16); save->CurStOffL = inb(0x3C5);
  outb(0x3C4, 0x17); save->CurMask = inb(0x3C5);
  outb(0x3C4, 0x1A); save->DispOffH = inb(0x3C5);
  outb(0x3C4, 0x1B); save->DispOffL = inb(0x3C5);
  outb(0x3C4, 0x1E); save->MemEnable = inb(0x3C5);
  outb(0x3C4, 0x1F); save->ExtClock = inb(0x3C5);
  outb(0x3C4, 0x20); save->ExtVMemAddr = inb(0x3C5);
  outb(0x3C4, 0x21); save->ExtPixel = inb(0x3C5);
  outb(0x3C4, 0x22); save->BusWidth = inb(0x3C5);

  outb(vgaIOBase+4, 0x30); save->ExtHTime = inb(vgaIOBase+5);
  outb(vgaIOBase+4, 0x31); save->ExtDispPos = inb(vgaIOBase+5);

  return ((void *) save);
}



/*
 * NCRInit --
 *      Handle the initialization, etc. of a screen.
 */

static Bool
NCRInit(mode)
     DisplayModePtr mode;
{
#if 0
  vgaIOBase = (inb(0x3CC) & 0x01) ? 0x3D0 : 0x3B0;
#endif

  if (!vgaHWInit(mode,sizeof(vgaNCRRec)))
    return(FALSE);

	/* These value have to be doubled to compensate for the 4 bit
	   character clock */
#ifndef MONOVGA
  new->ExtHTime = ((((mode->HTotal>>2)-5)&0x100)>>8) |
		  ((((mode->HDisplay>>2)-1)&0x100)>>7) |
		  ((((mode->HSyncStart>>2)-1)&0x100)>>6) |
		   (((mode->HSyncStart>>2)&0x100)>>5);
  new->std.CRTC[0]  = (mode->HTotal >> 2) - 5;
  new->std.CRTC[1]  = (mode->HDisplay >> 2) - 1;
  new->std.CRTC[2]  = (mode->HSyncStart >> 2) -1;
  new->std.CRTC[3]  = ((mode->HSyncEnd >> 2) & 0x1F) | 0x80;
  new->std.CRTC[4]  = (mode->HSyncStart >> 2);
  new->std.CRTC[5]  = (((mode->HSyncEnd >> 2) & 0x20 ) << 2 )
    | (((mode->HSyncEnd >> 2)) & 0x1F);
  new->std.CRTC[19] = vga256InfoRec.virtualX >> 3; /* we are in byte-mode */
  new->std.CRTC[23] = 0xE3;		/* Countbytwo=0 */
#else
  new->ExtHTime = 0x00;
#endif
  if (NCRchipset == NCR77C22E)
  {
    if (mode->Flags & V_INTERLACE)
    {
      new->ExtHTime |= 0x10;
    }
  }
  new->std.Graphics[6]  = 0x01;		/* graphics, Map all 128k */
  new->std.Attribute[16] = 0x01;	/* Put in graphics mode */

  new->ExtModes = 0x01;		/* Enable  Extended registers */
  new->CurForeColor = 0;
  new->CurBackColor = 0;
  new->CurControl = 0;
  new->CurXLocH = 0;
  new->CurXLocL = 0;
  new->CurYLocH = 0;
  new->CurYLocL = 0;
  new->CurXIndex = 0;
  new->CurYIndex = 0;
  new->CurStoreH = 0;
  new->CurStoreL = 0;
  new->CurStOffH = 0;
  new->CurStOffL = 0;
  new->CurMask = 0;
  new->PHostOffsetH = 0;
  new->PHostOffsetL = 0;
  new->DispOffH = 0;
  new->DispOffL = 0;
  new->SHostOffsetH = 0;
  new->SHostOffsetL = 0;
#ifdef MONOVGA
  new->MemEnable = 0x10;
  new->ExtClock = 0x00;
  new->ExtVMemAddr = 0x01;	/* ADD16 */
  new->ExtPixel = 0x00;
  new->BusWidth = 0x01;
  new->ExtDispPos = 0;
#else
  new->MemEnable = 0x14;
  new->ExtClock = 0x10;		/* Set Font width to 4 */
  new->ExtVMemAddr = 0x03;	/* Chain-4X, ADD16 */
  new->ExtPixel = 0x03;		/* Packed/Nibble Pixel Format, Graphics Byte */
  new->BusWidth = 0x01;
  new->ExtDispPos = 0;
#endif
  if (NCRchipset == NCR77C22E)
  {
    if (new->std.NoClock >= 0)
    {
      new->ExtClock |= (new->std.NoClock & 0x04) << 4;
    }
  }
  return(TRUE);
}
	


/*
 * NCRAdjust --
 *      adjust the current video frame to display the mousecursor
 */

static void 
NCRAdjust(x, y)
     int x, y;
{
  unsigned char	temp;
#ifdef MONOVGA
  unsigned long	Base = (y * vga256InfoRec.virtualX + x + 3) >> 3;
#else
  unsigned long	Base = (y * vga256InfoRec.virtualX + x + 1) >> 2;
#endif

  outw(vgaIOBase + 4, (Base & 0x00FF00) | 0x0C);
  outw(vgaIOBase + 4, ((Base & 0x00FF) << 8) | 0x0D);
  outb(vgaIOBase+4,0x31); temp=inb(vgaIOBase+5);	/* Extended Display Position Register */
  outb(vgaIOBase+5, ((Base&0x0F0000)>>16)|(temp&0xF0));
}
