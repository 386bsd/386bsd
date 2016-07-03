/*
   Copyright 1989 by the Massachusetts Institute of Technology

   Permission to use, copy, modify, and distribute this software and its
   documentation for any purpose and without fee is hereby granted,
   provided that the above copyright notice appear in all copies and that
   both that copyright notice and this permission notice appear in
   supporting documentation, and that the name of M.I.T. not be used in
   advertising or publicity pertaining to distribution of the software
   without specific, written prior permission.  M.I.T. makes no
   representations about the suitability of this software for any
   purpose.  It is provided "as is" without express or implied warranty.

   Author: Keith Packard

   Modified for the 8514/A by Kevin E. Martin (martin@cs.unc.edu)

   KEVIN E. MARTIN DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
   INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
   EVENT SHALL KEVIN E. MARTIN BE LIABLE FOR ANY SPECIAL, INDIRECT OR
   CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
   USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
   OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
   PERFORMANCE OF THIS SOFTWARE.

*/
  
/* $XFree86: mit/server/ddx/x386/vga256/drivers/cirrus/cir_blt.c,v 2.0 1993/09/21 15:24:25 dawes Exp $ */
/* $XConsortium: cfbbitblt.c,v 5.39 91/05/24 16:33:25 keith Exp $ */

/*
 * Modified by Amancio Hasty and Jon Tombs
 * Hacked for Cirrus by Bill Reynolds
 *
 * Reworked by: Simon P. Cooper, <scooper@vizlab.rutgers.edu>
 *
 * Id: cir_blt.c,v 0.7 1993/09/16 01:07:25 scooper Exp
 */

#include "X.h"
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
#include "fastblt.h"
#include "compiler.h"

#include "misc.h"
#include "x386.h"
#include "vgaBank.h"

extern pointer vgaBase;
void CirrusFindOrdering ();
void CirrusImageRead ();
void CirrusImageWrite ();

RegionPtr
CirrusCopyArea (pSrcDrawable, pDstDrawable,
		pGC, srcx, srcy, width, height, dstx, dsty)
     register DrawablePtr pSrcDrawable;
     register DrawablePtr pDstDrawable;
     GC *pGC;
     int srcx, srcy;
     int width, height;
     int dstx, dsty;
{
  RegionPtr prgnSrcClip;	/* may be a new region, or just a copy */
  Bool freeSrcClip = FALSE;

  RegionPtr prgnExposed;
  RegionRec rgnDst;
  register BoxPtr pbox;
  int i;
  register int dx;
  register int dy;
  xRectangle origSource;
  DDXPointRec origDest;
  int numRects;
  BoxRec fastBox;
  int fastClip = 0;		/* for fast clipping with pixmap source */
  int fastExpose = 0;		/* for fast exposures with pixmap source */
  int xdir, ydir;
  xdir = 1, ydir = 1;


  origSource.x = srcx;
  origSource.y = srcy;
  origSource.width = width;
  origSource.height = height;
  origDest.x = dstx;
  origDest.y = dsty;

  if ((pSrcDrawable != pDstDrawable) &&
      pSrcDrawable->pScreen->SourceValidate)
    {
      (*pSrcDrawable->pScreen->SourceValidate) (pSrcDrawable, srcx, srcy, width, height);
    }

  if (pGC->alu != GXcopy || (pGC->planemask & PMSK) != PMSK)
    {
      return cfbCopyArea (pSrcDrawable, pDstDrawable,
			  pGC, srcx, srcy, width, height, dstx, dsty);
    }

  srcx += pSrcDrawable->x;
  srcy += pSrcDrawable->y;

  /* clip the source */

  if (pSrcDrawable->type == DRAWABLE_PIXMAP)
    {
      if ((pSrcDrawable == pDstDrawable) &&
	  (pGC->clientClipType == CT_NONE))
	{
	  prgnSrcClip = ((cfbPrivGC *) (pGC->devPrivates[cfbGCPrivateIndex].ptr))->pCompositeClip;
	}
      else
	{
	  fastClip = 1;
	}
    }
  else
    {
      if (pGC->subWindowMode == IncludeInferiors)
	{
	  if (!((WindowPtr) pSrcDrawable)->parent)
	    {
	      /*
	         * special case bitblt from root window in
	         * IncludeInferiors mode; just like from a pixmap
	       */
	      fastClip = 1;
	    }
	  else if ((pSrcDrawable == pDstDrawable) &&
		   (pGC->clientClipType == CT_NONE))
	    {
	      prgnSrcClip = ((cfbPrivGC *) (pGC->devPrivates[cfbGCPrivateIndex].ptr))->pCompositeClip;
	    }
	  else
	    {
	      prgnSrcClip = NotClippedByChildren ((WindowPtr) pSrcDrawable);
	      freeSrcClip = TRUE;
	    }
	}
      else
	{
	  prgnSrcClip = &((WindowPtr) pSrcDrawable)->clipList;
	}
    }

  fastBox.x1 = srcx;
  fastBox.y1 = srcy;
  fastBox.x2 = srcx + width;
  fastBox.y2 = srcy + height;

  /* Don't create a source region if we are doing a fast clip */
  if (fastClip)
    {

      fastExpose = 1;
      /*
         * clip the source; if regions extend beyond the source size,
         * make sure exposure events get sent
       */
      if (fastBox.x1 < pSrcDrawable->x)
	{
	  fastBox.x1 = pSrcDrawable->x;
	  fastExpose = 0;
	}
      if (fastBox.y1 < pSrcDrawable->y)
	{
	  fastBox.y1 = pSrcDrawable->y;
	  fastExpose = 0;
	}
      if (fastBox.x2 > pSrcDrawable->x + (int) pSrcDrawable->width)
	{
	  fastBox.x2 = pSrcDrawable->x + (int) pSrcDrawable->width;
	  fastExpose = 0;
	}
      if (fastBox.y2 > pSrcDrawable->y + (int) pSrcDrawable->height)
	{
	  fastBox.y2 = pSrcDrawable->y + (int) pSrcDrawable->height;
	  fastExpose = 0;
	}
    }
  else
    {
      (*pGC->pScreen->RegionInit) (&rgnDst, &fastBox, 1);
      (*pGC->pScreen->Intersect) (&rgnDst, &rgnDst, prgnSrcClip);
    }

  dstx += pDstDrawable->x;
  dsty += pDstDrawable->y;

  if (pDstDrawable->type == DRAWABLE_WINDOW)
    {
      if (!((WindowPtr) pDstDrawable)->realized)
	{
	  if (!fastClip)
	    (*pGC->pScreen->RegionUninit) (&rgnDst);
	  if (freeSrcClip)
	    (*pGC->pScreen->RegionDestroy) (prgnSrcClip);
	  return NULL;
	}
    }

  dx = srcx - dstx;
  dy = srcy - dsty;



  /* Translate and clip the dst to the destination composite clip */
  if (fastClip)
    {
      RegionPtr cclip;

      /* Translate the region directly */
      fastBox.x1 -= dx;
      fastBox.x2 -= dx;
      fastBox.y1 -= dy;
      fastBox.y2 -= dy;

      /* If the destination composite clip is one rectangle we can
         do the clip directly.  Otherwise we have to create a full
         blown region and call intersect */

      /* XXX because CopyPlane uses this routine for 8-to-1 bit
         * copies, this next line *must* also correctly fetch the
         * composite clip from an mfb gc
       */

      cclip = ((cfbPrivGC *) (pGC->devPrivates[cfbGCPrivateIndex].ptr))->pCompositeClip;
      if (REGION_NUM_RECTS (cclip) == 1)
	{
	  BoxPtr pBox = REGION_RECTS (cclip);

	  if (fastBox.x1 < pBox->x1)
	    fastBox.x1 = pBox->x1;
	  if (fastBox.x2 > pBox->x2)
	    fastBox.x2 = pBox->x2;
	  if (fastBox.y1 < pBox->y1)
	    fastBox.y1 = pBox->y1;
	  if (fastBox.y2 > pBox->y2)
	    fastBox.y2 = pBox->y2;

	  /* Check to see if the region is empty */
	  if (fastBox.x1 >= fastBox.x2 || fastBox.y1 >= fastBox.y2)
	    (*pGC->pScreen->RegionInit) (&rgnDst, NullBox, 0);
	  else
	    (*pGC->pScreen->RegionInit) (&rgnDst, &fastBox, 1);
	}
      else
	{
	  /* We must turn off fastClip now, since we must create
	     a full blown region.  It is intersected with the
	     composite clip below. */
	  fastClip = 0;
	  (*pGC->pScreen->RegionInit) (&rgnDst, &fastBox, 1);
	}
    }
  else
    {
      (*pGC->pScreen->TranslateRegion) (&rgnDst, -dx, -dy);
    }

  if (!fastClip)
    {
      (*pGC->pScreen->Intersect) (&rgnDst,
				  &rgnDst,
				  ((cfbPrivGC *) (pGC->devPrivates[cfbGCPrivateIndex].ptr))->pCompositeClip);
    }

  /* Do bit blitting */
  numRects = REGION_NUM_RECTS (&rgnDst);
  if (numRects && width && height)
    {
      int swidth = pGC->pScreen->width;

      pbox = REGION_RECTS (&rgnDst);


      if (pSrcDrawable->type == DRAWABLE_WINDOW
	  && pDstDrawable->type == DRAWABLE_WINDOW)
	{
	  /* Window --> Window */
	  unsigned int *ordering;
	  BoxPtr prect;

	  ordering = (unsigned int *) ALLOCATE_LOCAL (numRects *
						      sizeof (unsigned int));
/*            ErrorF("Cirrus WWcopy\n"); */

	  if (!ordering)
	    {
	      DEALLOCATE_LOCAL (ordering);
	      return (RegionPtr) NULL;
	    }

	  CirrusFindOrdering (pSrcDrawable, pDstDrawable,
			      pGC, numRects, pbox, srcx, srcy, dstx, dsty,
			      ordering);


	  /* As I understand it, we now have a list */
	  /* of boxes, pbox[i], which should be */
	  /* copied, in the order given in ordering[i] */
	  /* to the box given by pbox->x + dx, */
	  /* pbox->y + dy (since every point in the */
	  /* dest box is the same point in the source */
	  /* box with the translational shift, dx,dy) */


	  for (i = 0; i < numRects; i++)
	    {
	      unsigned int srcAddr, dstAddr, boxwidth, boxheight, status;
	      volatile unsigned char tmpreg;
	      
	      prect = &pbox[ordering[i]];

	      dstAddr = prect->x1 + prect->y1 * swidth;
	      srcAddr = (prect->x1 + dx) + (prect->y1 + dy) * swidth;

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

	      outw (0x3CE, ((swidth & 0x000000FF) << 8) | 0x24);
	      outw (0x3CE, ((swidth & 0x00000F00)) | 0x25);

	      /* Set the Src Pitch */

	      outw (0x3CE, ((swidth & 0x000000FF) << 8) | 0x26);
	      outw (0x3CE, ((swidth & 0x00000F00)) | 0x27);

	      /* Set the Width */

	      boxwidth = prect->x2 - prect->x1;
	      boxheight = prect->y2 - prect->y1;

	      boxwidth--;
	      outw (0x3CE, ((boxwidth & 0x000000FF) << 8) | 0x20);
	      outw (0x3CE, ((boxwidth & 0x00000700)) | 0x21);

	      /* Set the Height */

	      boxheight--;
	      outw (0x3CE, ((boxheight & 0x000000FF) << 8) | 0x22);
	      outw (0x3CE, ((boxheight & 0x00000300)) | 0x23);

	      /* Set the direction */

	      outw (0x3CE, (0x00 << 8) | 0x30);

	      /* Set the ROP: Copy = 0x0D */
	      outw (0x3CE, (0x0D << 8) | 0x32);

	      /* Ok, we're all loaded up, let's do it */
	      outw (0x3CE, (0x02 << 8) | 0x31);

	      do
		{
		  outb (0x3CE, 0x31);
		  tmpreg = inb (0x3CF);
		}
	      while (tmpreg & 0x01);
	      
	    }
	}
      else if (pSrcDrawable->type == DRAWABLE_WINDOW
	       && pDstDrawable->type != DRAWABLE_WINDOW)
	{
	  char *pdstBase, *psrcBase;
	  int widthSrc, widthDst;	/* add to get to same position in */
	  /* next line */

	  /* Window --> Pixmap */
	  int pixWidth = PixmapBytePad (pDstDrawable->width,
					pDstDrawable->depth);
	  unsigned char *pdst = ((PixmapPtr) pDstDrawable)->devPrivate.ptr;

	  pdstBase = (unsigned char *)
	    (((PixmapPtr) pDstDrawable)->devPrivate.ptr);
	  psrcBase = (unsigned char *)
	    (((PixmapPtr) (pSrcDrawable->pScreen->devPrivate))
	     ->devPrivate.ptr);

	  widthDst = (int) (((PixmapPtr) pDstDrawable)->devKind);
	  widthSrc = (int) ((PixmapPtr)
			    (pSrcDrawable->pScreen->devPrivate))->devKind;

	  for (i = numRects; --i >= 0; pbox++)
	    {
	      CirrusImageRead(pdstBase, widthSrc, widthDst, 
			      pbox->x1 + dx, pbox->y1 + dy, 
			      pbox->x1, pbox->y1, 
			      pbox->x2 - pbox->x1,  pbox->y2 - pbox->y1, 
			      swidth);
	    }

	}
      else if (pSrcDrawable->type != DRAWABLE_WINDOW
	       && pDstDrawable->type == DRAWABLE_WINDOW)
	{
	  /* Pixmap --> Window */
	  unsigned char *psrcBase, *pdstBase;
	  int widthSrc, widthDst;	/* add to get to same position in */
	  /* next line */
	  int pixWidth = PixmapBytePad (pSrcDrawable->width,
					pSrcDrawable->depth);

	  unsigned char *psrc = ((PixmapPtr) pSrcDrawable)->devPrivate.ptr;

	  psrcBase = (unsigned char *)
	    (((PixmapPtr) pSrcDrawable)->devPrivate.ptr);
	  pdstBase = (unsigned char *)
	    (((PixmapPtr) (pDstDrawable->pScreen->devPrivate))
	     ->devPrivate.ptr);


	  widthDst = (int) ((PixmapPtr)
			    (pDstDrawable->pScreen->devPrivate))->devKind;

	  widthSrc = (int) (((PixmapPtr) pSrcDrawable)->devKind);

	  for (i = numRects; --i >= 0; pbox++)
	    {
	      int h = pbox->y2 - pbox->y1;
	      int w = pbox->x2 - pbox->x1;

	      CirrusImageWrite (psrcBase, pixWidth, widthDst,
				pbox->x1 + dx, pbox->y1 + dy,
				pbox->x1, pbox->y1, w, h);
	    }

	}
      else
	{
	  /* Pixmap --> Pixmap */
	  unsigned char *psrcBase, *pdstBase;
	  int widthSrc, widthDst;	/* add to get to same position in */
	  /* next line */
	  int pixWidth = PixmapBytePad (pSrcDrawable->width,
					pSrcDrawable->depth);

	  unsigned char *psrc = ((PixmapPtr) pSrcDrawable)->devPrivate.ptr;

	  psrcBase = (unsigned char *)
	    (((PixmapPtr) pSrcDrawable)->devPrivate.ptr);

	  pdstBase = (unsigned char *)
	    (((PixmapPtr) pDstDrawable)->devPrivate.ptr);


	  widthDst = (int) (((PixmapPtr) pDstDrawable)->devKind);

	  widthSrc = (int) (((PixmapPtr) pSrcDrawable)->devKind);

	  for (i = numRects; --i >= 0; pbox++)
	    {
	      int h = pbox->y2 - pbox->y1;
	      int w = pbox->x2 - pbox->x1;
	      vgaPixBitBlt (pdstBase, psrcBase, widthSrc, widthDst,
			    pSrcDrawable->x, pSrcDrawable->y,
			    pbox->x1, pbox->y1, w, h, xdir, ydir,
			    pGC->alu, pGC->planemask);
	    }

	}

    }

  prgnExposed = NULL;
  if (((cfbPrivGC *) (pGC->devPrivates[cfbGCPrivateIndex].ptr))->fExpose)
    {
      extern RegionPtr miHandleExposures ();

      /* Pixmap sources generate a NoExposed (we return NULL to do this) */
      if (!fastExpose)
	prgnExposed =
	  miHandleExposures (pSrcDrawable, pDstDrawable, pGC,
			     origSource.x, origSource.y,
			     (int) origSource.width,
			     (int) origSource.height,
			     origDest.x, origDest.y, 0);
    }
  (*pGC->pScreen->RegionUninit) (&rgnDst);
  if (freeSrcClip)
    (*pGC->pScreen->RegionDestroy) (prgnSrcClip);
  return prgnExposed;
}

void
CirrusFindOrdering (pSrcDrawable, pDstDrawable, pGC, numRects, boxes,
		    srcx, srcy, dstx, dsty, ordering)
     DrawablePtr pSrcDrawable;
     DrawablePtr pDstDrawable;
     GC *pGC;
     int numRects;
     BoxPtr boxes;
     int srcx;
     int srcy;
     int dstx;
     int dsty;
     unsigned int *ordering;
{
  int i, j, y;
  int xMax, yMin, yMax;

  /* If not the same drawable then order of move doesn't matter.
     Following assumes that boxes are sorted from top
     to bottom and left to right.
   */

  if ((pSrcDrawable != pDstDrawable) &&
      ((pGC->subWindowMode != IncludeInferiors) ||
       (pSrcDrawable->type == DRAWABLE_PIXMAP) ||
       (pDstDrawable->type == DRAWABLE_PIXMAP)))
    {
      for (i = 0; i < numRects; i++)
	ordering[i] = i;
    }
  /* within same drawable, must sequence */

  /* Scroll up or stationary vertical. */
  /* Vertical order OK */

  else
    /* moves carefully! */
    {

      /* Scroll left or stationary horizontal. */
      /* Horizontal order OK as well */
      if (dsty <= srcy)
	{

	  if (dstx <= srcx)
	    {
	      for (i = 0; i < numRects; i++)
		ordering[i] = i;
	    }
	  /* scroll right. must reverse horizontal */
	  /* banding of rects. */
	  else
	    {
	      for (i = 0, j = 1, xMax = 0; i < numRects; j = i + 1, xMax = i)
		{
		  /* find extent of current horizontal band */

		  /* band has this y coordinate */
		  y = boxes[i].y1;

		  while ((j < numRects) && (boxes[j].y1 == y))
		    j++;

		  /* reverse the horizontal band in the output */
		  /* ordering */

		  for (j--; j >= xMax; j--, i++)
		    ordering[i] = j;
		}
	    }
	}
      /* Scroll down. Must reverse vertical */
      /* banding. */
      else
	{
	  /* Scroll left. Horizontal order OK. */
	  if (dstx < srcx)
	    {
	      for (i = numRects - 1, j = i - 1, yMin = i, yMax = 0; i >= 0;
		   j = i - 1, yMin = i)
		{
		  /* find extent of current horizontal band */

		  y = boxes[i].y1;	/* band has this y coordinate */
		  while ((j >= 0) && (boxes[j].y1 == y))
		    j--;

		  /* reverse the horizontal band in the output */
		  /* ordering */

		  for (j++; j <= yMin; j++, i--, yMax++)
		    ordering[yMax] = j;
		}
	    }
	  else
	    {
	      /* Scroll right or horizontal stationary. */
	      /* Reverse horizontal order as well (if */
	      /* stationary, horizontal order can be */
	      /* swapped without penalty and this is */
	      /* faster to compute). */

	      for (i = 0, j = numRects - 1; i < numRects; i++, j--)
		ordering[i] = j;
	    }
	}
    }
}
