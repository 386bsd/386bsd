/*
 * cfb copy area
 */

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

/* WD90C31 code: Mike Tierney <floyd@eng.umd.edu> */

/* $XFree86: mit/server/ddx/x386/vga256/drivers/pvga1/pvgablt.c,v 2.1 1993/09/22 15:48:12 dawes Exp $ */
/* $XConsortium: cfbblt.c,v 1.7 91/05/06 15:13:21 rws Exp $ */

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
#include	"mergerop.h"
#include        "vgaBank.h"

#include "compiler.h"
#include "paradise.h"


pvgacfbDoBitbltCopy(pSrc, pDst, alu, prgnDst, pptSrc, planemask)
    DrawablePtr	    pSrc, pDst;
    int		    alu;
    RegionPtr	    prgnDst;
    DDXPointPtr	    pptSrc;
    unsigned long   planemask;
{
    unsigned long *psrcBase, *pdstBase;	
				/* start of src and dst bitmaps */
    int widthSrc, widthDst;	/* add to get to same position in next line */

    BoxPtr pbox;
    int nbox;

    BoxPtr pboxTmp, pboxNext, pboxBase, pboxNew1, pboxNew2;
				/* temporaries for shuffling rectangles */
    DDXPointPtr pptTmp, pptNew1, pptNew2;
				/* shuffling boxes entails shuffling the
				   source points too */
    int w, h;
    int xdir;			/* 1 = left right, -1 = right left/ */
    int ydir;			/* 1 = top down, -1 = bottom up */
    int blit_dir;

    unsigned long *psrcLine, *pdstLine;	
				/* pointers to line with current src and dst */
    register unsigned char *psrc;/* pointer to current src longword */
    register unsigned char *pdst;/* pointer to current dst longword */

    MROP_DECLARE_REG()

				/* following used for looping through a line */
    unsigned long startmask, endmask;	/* masks for writing ends of dst */
    int nlMiddle;		/* whole longwords in dst */
    int xoffSrc, xoffDst;
    register int leftShift, rightShift;
    register unsigned long bits;
    register unsigned long bits1;
    register int nl;		/* temp copy of nlMiddle */

				/* place to store full source word */
    int nstart;			/* number of ragged bits at start of dst */
    int nend;			/* number of ragged bits at end of dst */
    int srcStartOver;		/* pulling nstart bits from src
				   overflows into the next word? */
    int careful;
    int tmpSrc;
    int srcaddr, dstaddr;


    MROP_INITIALIZE(alu,planemask);

    cfbGetByteWidthAndPointer (pSrc, widthSrc, (unsigned char *)psrcBase)

    cfbGetByteWidthAndPointer (pDst, widthDst, (unsigned char *)pdstBase)

    BANK_FLAG_BOTH(psrcBase,pdstBase)

    /* XXX we have to err on the side of safety when both are windows,
     * because we don't know if IncludeInferiors is being used.
     */
    careful = ((pSrc == pDst) ||
	       ((pSrc->type == DRAWABLE_WINDOW) &&
		(pDst->type == DRAWABLE_WINDOW)));

    if (!CHECKSCREEN(psrcBase) || !CHECKSCREEN(pdstBase))
    {
       pvga1_stdcfbDoBitbltCopy (pSrc, pDst, alu, prgnDst, pptSrc, planemask);
       return;
    }
    

    pbox = REGION_RECTS(prgnDst);
    nbox = REGION_NUM_RECTS(prgnDst);

    pboxNew1 = NULL;
    pptNew1 = NULL;
    pboxNew2 = NULL;
    pptNew2 = NULL;
    if (careful && (pptSrc->y < pbox->y1))
    {
        /* walk source botttom to top */
	ydir = -1;

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

    if (careful && (pptSrc->x < pbox->x1) && (pptSrc->y <= pbox->y1))
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

    blit_dir = ((xdir == -1 || ydir == -1) ? BLT_DIRECT : 0);

    while(nbox--)
    {
	w = pbox->x2 - pbox->x1;
	h = pbox->y2 - pbox->y1;

        if (xdir == -1 || ydir == -1)
        {
           dstaddr = (pbox->y1+h-1)*widthSrc + pbox->x1+w-1;
           srcaddr = (pptSrc->y+h-1)*widthDst + pptSrc->x+w-1;
        }
        else
        {
           dstaddr = pbox->y1*widthDst + pbox->x1;
           srcaddr = pptSrc->y*widthSrc + pptSrc->x;
        }

       /** handle the blit, this could actually handle and ROP **/
        WAIT_BLIT;
        SET_BLT_SRC_LOW ((srcaddr & 0xFFF));
        SET_BLT_SRC_HGH (((srcaddr >> 12) & 0xFF));
        SET_BLT_DST_LOW ((dstaddr & 0xFFF));
        SET_BLT_DST_HGH (((dstaddr >> 12) & 0xFF));
        SET_BLT_ROW_PTCH (widthDst);
        SET_BLT_DIM_X   (w);
        SET_BLT_DIM_Y   (h);
        SET_BLT_MASK    ((planemask & 0xFF));
        SET_BLT_RAS_OP  (ROP_SRC);
        SET_BLT_CNTRL2  (0x00);
        SET_BLT_CNTRL1  ((BLT_ACT_STAT | BLT_PACKED | BLT_SRC_COLR | blit_dir));

        pbox++;
        pptSrc++;
    }

    WAIT_BLIT; /* must wait, since memory writes can mess up as well */

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
