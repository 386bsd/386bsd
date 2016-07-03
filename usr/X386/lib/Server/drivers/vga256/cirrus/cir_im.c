/*
 * $XFree86: mit/server/ddx/x386/vga256/drivers/cirrus/cir_im.c,v 2.2 1993/10/02 16:09:18 dawes Exp $
 *
 * Copyright 1993 by Bill Reynolds, Santa Fe, New Mexico
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Bill Reynolds not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Bill Reynolds makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * BILL REYNOLDS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL BILL REYNOLDS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Bill Reynolds, bill@goshawk.lanl.gov
 *
 * Reworked by: Simon P. Cooper, <scooper@vizlab.rutgers.edu>
 *
 * Id: cir_im.c,v 0.7 1993/09/16 01:07:25 scooper Exp
 */

#include "misc.h"
#include "x386.h"
#include "X.h"
#include "Xos.h"
#include "Xmd.h"
#include "Xproto.h"
#include "gcstruct.h"
#include "windowstr.h"
#include "scrnintstr.h"
#include "pixmapstr.h"
#include "regionstr.h"
#include "cfb.h"
#include "cfbmskbits.h"
#include "cfb8bit.h"

#include "mergerop.h"
#include "vgaBank.h"
#include "compiler.h"
#include "os.h"		/* For FatalError */

#include "cir_driver.h"

extern pointer vgaBase;

#define BLITADDRESS (vgaBase+4)

extern void vgaImageWrite ();
extern void vgaImageRead ();
extern void SpeedUpBitBlt ();

CirrusBltLine (dstAddr, srcAddr, dstPitch, srcPitch, w, h, dir)
     unsigned int dstAddr, srcAddr;
     unsigned int dstPitch, srcPitch;
     unsigned int w, h;
     int dir;			/* >0, increase adrresses, <0, decrease */
{
  volatile unsigned char tmpreg;

  /* Set the SrcAddress */

  outw (0x3CE, ((srcAddr & 0x000000FF) << 8) | 0x2C);
  outw (0x3CE, ((srcAddr & 0x0000FF00)) | 0x2D);
  outw (0x3CE, ((srcAddr & 0x001F0000) >> 8) | 0x2E);

  /* Set the Src Pitch */

  outw (0x3CE, ((srcPitch & 0x000000FF) << 8) | 0x26);
  outw (0x3CE, ((srcPitch & 0x00000F00)) | 0x27);

  /* Set the DstAddress */

  outw (0x3CE, ((dstAddr & 0x000000FF) << 8) | 0x28);
  outw (0x3CE, ((dstAddr & 0x0000FF00)) | 0x29);
  outw (0x3CE, ((dstAddr & 0x001F0000) >> 8) | 0x2A);

  /* Set the Dest Pitch */

  outw (0x3CE, ((dstPitch & 0x000000FF) << 8) | 0x24);
  outw (0x3CE, ((dstPitch & 0x00000F00)) | 0x25);

  /* Set the Width */

  w--;
  outw (0x3CE, ((w & 0x000000FF) << 8) | 0x20);
  outw (0x3CE, ((w & 0x00000700)) | 0x21);

  /* Set the Height */

  h--;
  outw (0x3CE, ((h & 0x000000FF) << 8) | 0x22);
  outw (0x3CE, ((h & 0x00000300)) | 0x23);

  /* Set the direction */
  if (dir > 0)
    {
      outw (0x3CE, (0x00 << 8) | 0x30);
    }
  else
    {
      outw (0x3CE, (0x01 << 8) | 0x30);
    }

  /* Set the ROP: Copy = 0x0D */
  outw (0x3CE, (CROP_SRC << 8) | 0x32);

  /* Ok, we're all loaded up, let's do it */
  outw (0x3CE, (0x02 << 8) | 0x31);

  do
    {
      outb (0x3CE, 0x31);
      tmpreg = inb (0x3CF);
    }
  while (tmpreg & 0x01);

}

void
CirrusBitBlt (pdstBase, psrcBase, widthSrc, widthDst, x, y,
	      x1, y1, w, h, xdir, ydir, alu, planemask)
     pointer pdstBase, psrcBase;	/* start of src bitmap */
     int widthSrc, widthDst;
     int x, y, x1, y1, w, h;	/* Src x,y; Dst x1,y1; Dst (w)idth,(h)eight */
     int xdir, ydir;
     int alu;
     unsigned long planemask;
{
  unsigned int psrc, pdst, ppdst;
  int i;

  if (widthSrc < 0)
    widthSrc *= -1;
  if (widthDst < 0)
    widthDst *= -1;

  if (alu == GXcopy && (planemask & 0xFF) == 0xFF)
    {
      if (xdir == 1)		/* left to right */
	{
	  if (ydir == 1)	/* top to bottom */
	    {
	      psrc = (y * widthSrc) + x;
	      pdst = (y1 * widthDst) + x1;
	    }
	  else
	    /* bottom to top */
	    {
	      psrc = ((y + h - 1) * widthSrc) + x;
	      pdst = ((y1 + h - 1) * widthDst) + x1;
	    }
	}
      else
	/* right to left */
	{
	  if (ydir == 1)	/* top to bottom */
	    {
	      psrc = (y * widthSrc) + x + w - 1;
	      pdst = (y1 * widthDst) + x1 + w - 1;
	    }
	  else
	    /* bottom to top */
	    {
	      psrc = ((y + h - 1) * widthSrc) + x + w - 1;
	      pdst = ((y1 + h - 1) * widthDst) + x1 + w - 1;
	    }
	}

      /* I could probably do the line by line */
      /* blits a little faster by breaking the */
      /* blit regions into rectangles */
      /* and blitting those, making sure I don't */
      /* overwrite stuff. However, the */
      /* difference between the line by line */
      /* and block blits isn't noticable to */
      /* me, so I think I'll blow it off. */

      if (xdir == 1)
	{
	  if (ydir == 1)
	    {			/* Nothing special, straight blit */
	      CirrusBltLine (pdst, psrc, widthDst, widthSrc, w, h, 1);
	    }
	  else
	    /* Line by line, going up. */
	    {
	      for (i = 0; i < h; i++)
		{
		  CirrusBltLine (pdst, psrc, widthDst, widthSrc, w, 1, 1);
		  psrc -= widthSrc;
		  pdst -= widthDst;
		}
	    }
	}
      else
	{

	  if (ydir == 1)	/* Line by line, going down and to the left */
	    {
	      for (i = 0; i < h; i++)
		{
		  CirrusBltLine (pdst, psrc, widthDst, widthSrc, w, 1, -1);
		  psrc += widthSrc;
		  pdst += widthDst;
		}
	    }
	  else
	    /* Another stock blit, albeit backwards */
	    {
	      CirrusBltLine (pdst, psrc, widthDst, widthSrc, w, h, -1);
	    }
	}
    }
}

void
CirrusImageWrite (pdstBase, psrcBase, widthSrc, widthDst, x, y,
		  x1, y1, w, h, xdir, ydir, alu, planemask)
     pointer pdstBase, psrcBase;	/* start of src bitmap */
     int widthSrc, widthDst;
     int x, y, x1, y1, w, h;	/* Src x,y; Dst x1,y1; Dst (w)idth,(h)eight */
     int xdir, ydir;
     int alu;
     unsigned long planemask;
{
  unsigned long *plSrc;
  volatile unsigned char status;
  volatile unsigned long *pDst;
  pointer psrc;
  unsigned int dstAddr;
  unsigned int word_count, word_rem;
  int i, j;

  if (alu == GXcopy && (planemask & 0xFF) == 0xFF)
    {
      int width, height;

      psrc = psrcBase + (y * widthSrc) + x;
      dstAddr = (y1 * widthDst) + x1;

      /* Set the DstAddress */

      outw (0x3CE, ((dstAddr & 0x000000FF) << 8) | 0x28);
      outw (0x3CE, ((dstAddr & 0x0000FF00)) | 0x29);
      outw (0x3CE, ((dstAddr & 0x001F0000) >> 8) | 0x2A);

      /* Set the Dest Pitch */

      outw (0x3CE, ((widthDst & 0x000000FF) << 8) | 0x24);
      outw (0x3CE, ((widthDst & 0x00000F00)) | 0x25);

      /* Set the Width */

      width = w - 1;
      outw (0x3CE, ((width & 0x000000FF) << 8) | 0x20);
      outw (0x3CE, ((width & 0x00000700)) | 0x21);

      /* Set the Height */

      height = h - 1;
      outw (0x3CE, ((height & 0x000000FF) << 8) | 0x22);
      outw (0x3CE, ((height & 0x00000300)) | 0x23);

      /* Set the direction and source (System Memory) */

      outw (0x3CE, (0x04 << 8) | 0x30);

      /* Set the ROP: Copy = 0x0D */
      outw (0x3CE, (0x0D << 8) | 0x32);

      /* Lets play DMA controller ... */
      outw (0x3CE, (0x02 << 8) | 0x31);

      /*
       * We must transfer 4 bytes per blit line.  This is cautious code and I
       * do not read from outside of the pixmap... The 386/486 allows
       * unaligned memory acceses, and has little endian word ordering.  This
       * is used to our advantage when dealing with the 3 byte remainder.
       * Don't try this on your Sparc :-)
       */

      pDst = (unsigned long *) BLITADDRESS;

      word_count = w >> 2;
      word_rem = w & 0x3;

      switch (word_rem)
	{

	case 0:
	  for (i = 0; i < h; i++)
	    {
	      plSrc = (unsigned long *) psrc;
	      for (j = 0; j < word_count; j++)
		*pDst = *plSrc++;
	      psrc += widthSrc;
	    }
	  break;

	case 1:		/* One byte extra */
	  for (i = 0; i < h; i++)
	    {
	      plSrc = (unsigned long *) psrc;
	      for (j = 0; j < word_count; j++)
		*pDst = *plSrc++;

	      *pDst = (unsigned long) (*(unsigned char *) plSrc);

	      psrc += widthSrc;
	    }
	  break;

	case 2:		/* Two bytes extra */
	  for (i = 0; i < h; i++)
	    {
	      plSrc = (unsigned long *) psrc;
	      for (j = 0; j < word_count; j++)
		*pDst = *plSrc++;

	      *pDst = (unsigned long) (*(unsigned short *) plSrc);

	      psrc += widthSrc;
	    }
	  break;

	case 3:

	  for (i = 0; i < h; i++)
	    {
	      plSrc = (unsigned long *) psrc;
	      for (j = 0; j < word_count; j++)
		*pDst = *plSrc++;

	      (*(unsigned char **)&plSrc)--;
	      *pDst = (*plSrc) >> 8;

	      psrc += widthSrc;
	    }
	  break;

	}

      do
	{
	  outb (0x3CE, 0x31);
	  status = inb (0x3CF);
	}
      while (status & 0x01);
    }
  else
    {
      vgaImageWrite(pdstBase, psrcBase, widthSrc, widthDst, x, y,
		    x1, y1, w, h, xdir, ydir, alu, planemask);
    }
  
}


void
CirrusImageRead (pdstBase, psrcBase, widthSrc, widthDst, x, y,
		 x1, y1, w, h, xdir, ydir, alu, planemask)
     pointer pdstBase, psrcBase;	/* start of src bitmap */
     int widthSrc, widthDst;
     int x, y, x1, y1, w, h;	/* Src x,y; Dst x1,y1; Dst (w)idth,(h)eight */
     int xdir, ydir;
     int alu;
     unsigned long planemask;
{
  unsigned long *plDst;
  volatile unsigned char status;
  volatile unsigned long *pSrc;
  pointer pdst;
  unsigned int srcAddr;
  unsigned int word_count, word_rem;
  int i, j;

  if (alu == GXcopy && (planemask & 0xFF) == 0xFF)
    {
      int width, height;

      pdst = pdstBase + (y1 * widthDst) + x1;
      srcAddr = (y * widthSrc) + x;

      /* Set the SrcAddress */

      outw (0x3CE, ((srcAddr & 0x000000FF) << 8) | 0x2C);
      outw (0x3CE, ((srcAddr & 0x0000FF00)) | 0x2D);
      outw (0x3CE, ((srcAddr & 0x001F0000) >> 8) | 0x2E);
      
      /* Set the Src Pitch */

      outw (0x3CE, ((widthSrc & 0x000000FF) << 8) | 0x26);
      outw (0x3CE, ((widthSrc & 0x00000F00)) | 0x27);

      /* Set the Width */

      width = w - 1;
      outw (0x3CE, ((width & 0x000000FF) << 8) | 0x20);
      outw (0x3CE, ((width & 0x00000700)) | 0x21);

      /* Set the Height */

      height = h - 1;
      outw (0x3CE, ((height & 0x000000FF) << 8) | 0x22);
      outw (0x3CE, ((height & 0x00000300)) | 0x23);

      /* Set the direction and destination (System Memory) */

      outw (0x3CE, (0x02 << 8) | 0x30);

      /* Set the ROP: Copy = 0x0D */
      outw (0x3CE, (0x0D << 8) | 0x32);

      /* Lets play DMA controller ... */
      outw (0x3CE, (0x02 << 8) | 0x31);

      /*
       * We must transfer 4 bytes per blit line.  This is cautious code and I
       * do not read from outside of the pixmap... The 386/486 allows
       * unaligned memory acceses, and has little endian word ordering.  This
       * is used to our advantage when dealing with the 3 byte remainder.
       * Don't try this on your Sparc :-)
       */

      pSrc = (unsigned long *) BLITADDRESS;

      word_count = w >> 2;
      word_rem = w & 0x3;

      switch (word_rem)
	{

	case 0:
	  for (i = 0; i < h; i++)
	    {
	      plDst = (unsigned long *) pdst;
	      for (j = 0; j < word_count; j++)
		*plDst++ = *pSrc;
	      pdst += widthDst;
	    }
	  break;

	case 1:		/* One byte extra */
	  for (i = 0; i < h; i++)
	    {
	      plDst = (unsigned long *) pdst;
	      for (j = 0; j < word_count; j++)
		*plDst++ = *pSrc;

	      *(unsigned char *)plDst = (unsigned char) *pSrc;

	      pdst += widthDst;
	    }
	  break;

	case 2:		/* Two bytes extra */
	  for (i = 0; i < h; i++)
	    {
	      plDst = (unsigned long *) pdst;
	      for (j = 0; j < word_count; j++)
		*plDst++ = *pSrc;

	      *(unsigned short *)plDst = (unsigned short) *pSrc;

	      pdst += widthDst;
	    }
	  break;

	case 3:

	  for (i = 0; i < h; i++)
	    { unsigned long tpix;

	      plDst = (unsigned long *) pdst;
	      for (j = 0; j < word_count; j++)
		*plDst++ = *pSrc;

	      tpix = *pSrc;

	      *(*(unsigned short **)&plDst)++ = (unsigned short) tpix;
	      *(unsigned char  *)plDst = (unsigned char)(tpix >>16);
	      
	      pdst += widthDst;
	    }
	  break;

	}

      do
	{
	  outb (0x3CE, 0x31);
	  status = inb (0x3CF);
	}
      while (status & 0x01);
    }
  else
    {
      vgaImageRead(pdstBase, psrcBase, widthSrc, widthDst, x, y,
		   x1, y1, w, h, xdir, ydir, alu, planemask);
    }
}
