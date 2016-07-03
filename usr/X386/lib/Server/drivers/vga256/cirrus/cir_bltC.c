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

*/

/* $XFree86: mit/server/ddx/x386/vga256/drivers/cirrus/cir_bltC.c,v 2.0 1993/09/21 15:24:27 dawes Exp $ */

/*
 * Author:  Bill Reynolds, bill@goshawk.lanl.gov
 *
 * Reworked by: Simon P. Cooper, <scooper@vizlab.rutgers.edu>
 *
 * Id: cir_bltC.c,v 0.7 1993/09/16 01:07:25 scooper Exp
 */


#include	"X.h"
#include	"Xmd.h"
#include	"Xproto.h"
#include	"gcstruct.h"
#include	"windowstr.h"
#include	"scrnintstr.h"
#include	"pixmapstr.h"
#include	"regionstr.h"
#include	"cfb.h"
#include	"cfbmskbits.h"
#include	"cfb8bit.h"
#include	"fastblt.h"
#include        "vgaBank.h"

extern void (*ourvgaBitBlt)();


CirrusDoBitbltCopy(pSrc, pDst, alu, prgnDst, pptSrc, planemask)
    DrawablePtr	    pSrc, pDst;
    int		    alu;
    RegionPtr	    prgnDst;
    DDXPointPtr	    pptSrc;
    unsigned long   planemask;
{
  unsigned char *psrcBase, *pdstBase;	
  /* start of src and dst bitmaps */
  int widthSrc, widthDst;	/* add to get to same position in next line */
  register void (*fnp)();
  int NoCirrus = 0;
  BoxPtr pbox;
  int nbox;
  int readwrite=0;		/* DBG */
  
  
  BoxPtr pboxTmp, pboxNext, pboxBase, pboxNew1, pboxNew2;
                                  /* temporaries for shuffling rectangles */
  DDXPointPtr pptTmp, pptNew1, pptNew2;
                                  /* shuffling boxes entails shuffling the
                                     source points too */

  int xdir;			/* 1 = left right, -1 = right left/ */
  int ydir;			/* 1 = top down, -1 = bottom up */
  int careful;

  void fastBitBltCopy();
  void vgaImageRead();
  void vgaImageWrite();
  void CirrusImageWrite();
  void CirrusImageRead();
  void CirrusBitBlt();
  void vgaPixBitBlt();

  if (pSrc->type == DRAWABLE_WINDOW)
    {
      psrcBase = (unsigned char *)
	(((PixmapPtr)(pSrc->pScreen->devPrivate))->devPrivate.ptr);
      widthSrc = (int)((PixmapPtr)(pSrc->pScreen->devPrivate))->devKind;
    }
  else
    {
      psrcBase = (unsigned char *)(((PixmapPtr)pSrc)->devPrivate.ptr);
      widthSrc = (int)(((PixmapPtr)pSrc)->devKind);
    }
  
  if (pDst->type == DRAWABLE_WINDOW)
    {
      pdstBase = (unsigned char *)
	(((PixmapPtr)(pDst->pScreen->devPrivate))->devPrivate.ptr);
      widthDst = (int)
	((PixmapPtr)(pDst->pScreen->devPrivate))->devKind;
    }
  else
    {
      pdstBase = (unsigned char *)(((PixmapPtr)pDst)->devPrivate.ptr);
      widthDst = (int)(((PixmapPtr)pDst)->devKind);
    }


  /* XXX we have to err on the side of safety when both are windows,
   * because we don't know if IncludeInferiors is being used.
   */
  careful = ((pSrc == pDst) ||
	     ((pSrc->type == DRAWABLE_WINDOW) &&
	      (pDst->type == DRAWABLE_WINDOW)));
  
  pbox = REGION_RECTS(prgnDst);
  nbox = REGION_NUM_RECTS(prgnDst);
  
  if (careful && (pptSrc->y < pbox->y1)) NoCirrus = 1;
  if (careful && (pptSrc->x < pbox->x1)) NoCirrus = 1;
  
  if (CHECKSCREEN(psrcBase))
       {
       if (CHECKSCREEN(pdstBase)) /* Screen -> Screen */
	    {
	    fnp = CirrusBitBlt;	/* This works fine */
	    }
       else			/* Screen -> Mem */
	    {
	    if(NoCirrus)
		 {
		 fnp = vgaImageRead;
		 }
	    else 
		 {
		 fnp = CirrusImageRead;
		 }
	    }
       }
  else 
       if (CHECKSCREEN(pdstBase)) /* Mem -> Screen */
	    {
	    if(NoCirrus) 
		 {
		 fnp = vgaImageWrite;
		 }
	    else fnp = CirrusImageWrite;
	    }
       else fnp = vgaPixBitBlt;	/* Don't need to change: Mem -> Mem */
       
       
  pboxNew1 = NULL;
  pptNew1 = NULL;
  pboxNew2 = NULL;
  pptNew2 = NULL;
  if (careful && (pptSrc->y < pbox->y1))
    {
      /* walk source botttom to top */
      ydir = -1;
      widthSrc = -widthSrc;
      widthDst = -widthDst;
      
      if (nbox > 1)
	{
	  /* keep ordering in each band, reverse order of bands */
	  pboxNew1 = (BoxPtr)ALLOCATE_LOCAL(sizeof(BoxRec) * nbox);
	  if(!pboxNew1)
	    return;
	  pptNew1 = (DDXPointPtr)ALLOCATE_LOCAL(sizeof(DDXPointRec) * nbox);
	  if(!pptNew1)
	    {
	      DEALLOCATE_LOCAL(pboxNew1);
	      return;
	    }
	  pboxBase = pboxNext = pbox+nbox-1;
	  while (pboxBase >= pbox)
	    {
	      while ((pboxNext >= pbox) &&
		     (pboxBase->y1 == pboxNext->y1))
		pboxNext--;
	      pboxTmp = pboxNext+1;
	      pptTmp = pptSrc + (pboxTmp - pbox);
	      while (pboxTmp <= pboxBase)
	        {
		  *pboxNew1++ = *pboxTmp++;
		  *pptNew1++ = *pptTmp++;
	        }
	      pboxBase = pboxNext;
	    }
	  pboxNew1 -= nbox;
	  pbox = pboxNew1;
	  pptNew1 -= nbox;
	  pptSrc = pptNew1;
        }
    }
  else
    {
      /* walk source top to bottom */
      ydir = 1;
    }
  
  if (careful && (pptSrc->x < pbox->x1))
    {
      /* walk source right to left */
      xdir = -1;
      
      if (nbox > 1)
	{
	  /* reverse order of rects in each band */
	  pboxNew2 = (BoxPtr)ALLOCATE_LOCAL(sizeof(BoxRec) * nbox);
	  pptNew2 = (DDXPointPtr)ALLOCATE_LOCAL(sizeof(DDXPointRec) * nbox);
	  if(!pboxNew2 || !pptNew2)
	    {
	      if (pptNew2) DEALLOCATE_LOCAL(pptNew2);
	      if (pboxNew2) DEALLOCATE_LOCAL(pboxNew2);
	      if (pboxNew1)
		{
		  DEALLOCATE_LOCAL(pptNew1);
		  DEALLOCATE_LOCAL(pboxNew1);
		}
	      return;
	    }
	  pboxBase = pboxNext = pbox;
	  while (pboxBase < pbox+nbox)
	    {
	      while ((pboxNext < pbox+nbox) &&
		     (pboxNext->y1 == pboxBase->y1))
		pboxNext++;
	      pboxTmp = pboxNext;
	      pptTmp = pptSrc + (pboxTmp - pbox);
	      while (pboxTmp != pboxBase)
	        {
		  *pboxNew2++ = *--pboxTmp;
		  *pptNew2++ = *--pptTmp;
	        }
	      pboxBase = pboxNext;
	    }
	  pboxNew2 -= nbox;
	  pbox = pboxNew2;
	  pptNew2 -= nbox;
	  pptSrc = pptNew2;
	}
    }
  else
    {
      /* walk source left to right */
      xdir = 1;
    }
  
  while(nbox--) {
    (*fnp)(pdstBase, psrcBase,widthSrc,widthDst,
	   pptSrc->x, pptSrc->y, pbox->x1, pbox->y1,
	   pbox->x2 - pbox->x1, pbox->y2 - pbox->y1,
	   xdir, ydir, alu, planemask);
    pbox++;
    pptSrc++;
  }
  
  /* free up stuff */
  if (pboxNew2)
    {
      DEALLOCATE_LOCAL(pptNew2);
      DEALLOCATE_LOCAL(pboxNew2);
    }
  if (pboxNew1)
    {
      DEALLOCATE_LOCAL(pptNew1);
      DEALLOCATE_LOCAL(pboxNew1);
    }
}


