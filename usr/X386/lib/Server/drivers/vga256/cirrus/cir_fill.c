/*
 * $XFree86: mit/server/ddx/x386/vga256/drivers/cirrus/cir_fill.c,v 2.1 1993/09/24 17:09:13 dawes Exp $
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
 * Id: cir_fill.c,v 0.7 1993/09/16 01:07:25 scooper Exp
 */

#include "X.h"
#include "Xmd.h"
#include "servermd.h"
#include "gcstruct.h"
#include "window.h"
#include "pixmapstr.h"
#include "scrnintstr.h"
#include "windowstr.h"

#include "cfb.h"
#include "cfbmskbits.h"
#include "cfbrrop.h"
#include "mergerop.h"
#include "vgaBank.h"

#include "compiler.h"

#include "cir_driver.h"

extern void (*ourcfbFillBoxSolid)();

void CirrusFillBoxSolid();

void
CirrusFillSolid(dstAddr,pdstBase,pat,fillHeight,fillWidth,dstPitch,rop)
     unsigned int dstAddr;
     unsigned char *pdstBase;
     unsigned long pat;
     int fillHeight,fillWidth,dstPitch;
     int rop;
{
  volatile unsigned char tmpreg;
  unsigned int srcAddr;
  int i;
  pointer pDst;
  extern int CirrusMemTop;
  extern pointer vgaBase;

  /* Write out the initial 8x8 pattern, */
  /* We'll write it above the displayable */
  /* board memory */
     
  srcAddr = CirrusMemTop + 4;
  pDst = pdstBase + srcAddr;

  BANK_FLAG(pDst);
  SETW(pDst);
  for(i=0;i<16;i++)
    {
      *((*(int **)&pDst)++) = pat;
      CHECKWO(pDst);
    }
     
  /* BLTBIT screen -> screen copy */

  /* Set the DstAddress */

  outw (0x3CE, ((dstAddr & 0x000000FF) << 8) | 0x28);
  outw (0x3CE, ((dstAddr & 0x0000FF00)) | 0x29);
  outw (0x3CE, ((dstAddr & 0x001F0000) >> 8) | 0x2A);

  /* Set the SrcAddress */

  outw (0x3CE, ((srcAddr & 0x000000FF) << 8) | 0x2C);
  outw (0x3CE, ((srcAddr & 0x0000FF00)) | 0x2D);
  outw (0x3CE, ((srcAddr & 0x001F0000) >> 8) | 0x2E);

  /* Set the Dest Pitch */

  outw (0x3CE, ((dstPitch & 0x000000FF) << 8) | 0x24);
  outw (0x3CE, ((dstPitch & 0x00000F00)) | 0x25);
  
  fillWidth--;
  outw (0x3CE, ((fillWidth & 0x000000FF) << 8) | 0x20);
  outw (0x3CE, ((fillWidth & 0x00000700)) | 0x21);

  /* Set the Height */

  fillHeight--;
  outw (0x3CE, ((fillHeight & 0x000000FF) << 8) | 0x22);
  outw (0x3CE, ((fillHeight & 0x00000300)) | 0x23);

  /* Set: 8x8 Pattern Copy, Screen <-> screen blt, forwards */

  outw (0x3CE, (0x40 << 8) | 0x30);

  /* Set the ROP: Copy = 0x0D */
  outw (0x3CE, (rop << 8) | 0x32);

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
CirrusFillRectSolidCopy (pDrawable, pGC, nBox, pBox)
    DrawablePtr	    pDrawable;
    GCPtr	    pGC;
    int		    nBox;
    BoxPtr	    pBox;
{
  unsigned long rrop_xor,rrop_and;
  RROP_FETCH_GC(pGC);
  CirrusFillBoxSolid (pDrawable, nBox, pBox, rrop_xor, 0, GXcopy);
}

void
CirrusFillBoxSolid (pDrawable, nBox, pBox, pixel1, pixel2, alu)
    DrawablePtr	    pDrawable;
    int		    nBox;
    BoxPtr	    pBox;
    unsigned long   pixel1;
    unsigned long   pixel2;
    int	            alu;
{
  unsigned char   *pdstBase;
  unsigned long   fill2;
  unsigned char   *pdst;
  register int    hcount, vcount, count;
  int             widthPitch;
  Bool            flag;
  int		    widthDst;
  int             h;
  unsigned long   fill1;
  int             m;
  int		    w;

  if (pDrawable->type == DRAWABLE_WINDOW)
    {
      pdstBase = (unsigned char *)
	(((PixmapPtr)(pDrawable->pScreen->devPrivate))->devPrivate.ptr);
      widthDst = (int)
	(((PixmapPtr)(pDrawable->pScreen->devPrivate))->devKind);
    }
  else
    {
      pdstBase = (unsigned char *)(((PixmapPtr)pDrawable)->devPrivate.ptr);
      widthDst = (int)(((PixmapPtr)pDrawable)->devKind);
    }

  flag = CHECKSCREEN(pdstBase);
  fill1 = PFILL(pixel1);
  fill2 = PFILL(pixel2);
    
  if(flag)			/* i.e. We're filling a box on the screen */
    {
      for (; nBox; nBox--, pBox++)
	{
	  unsigned int dstAddr;
	      
	  dstAddr = pBox->y1 * widthDst + pBox->x1;
	  h = pBox->y2 - pBox->y1;
	  w = pBox->x2 - pBox->x1;
	  
	  switch (alu)
	    {
	    case GXcopy:
	      CirrusFillSolid(dstAddr,pdstBase,fill1,h,w,widthDst,CROP_SRC);
	      break;

	    case GXor:
	      CirrusFillSolid(dstAddr,pdstBase,fill1,h,w,widthDst,CROP_OR);
	      break;

	    case GXand:
	      CirrusFillSolid(dstAddr,pdstBase,fill1,h,w,widthDst,CROP_AND);
	      break;

	    case GXxor:
      	      CirrusFillSolid(dstAddr,pdstBase,fill1,h,w,widthDst,CROP_XOR);
	      break;

	    case GXset:
	      /* This is a hack, but it should be doing the right thing */

      	      CirrusFillSolid(dstAddr,pdstBase,fill1,h,w,widthDst,CROP_AND);
      	      CirrusFillSolid(dstAddr,pdstBase,fill2,h,w,widthDst,CROP_XOR);
	      break;
	      
	    default: 
	      return; 
	      break;
	    }
	}
    }
  else cfbFillBoxSolid (pDrawable, nBox, pBox, pixel1, pixel2, alu);

}
