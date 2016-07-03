/***********************************************************
Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts,
and the Massachusetts Institute of Technology, Cambridge, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the names of Digital or MIT not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/

/* $XFree86: mit/server/ddx/x386/vga256/drivers/pvga1/pvgapntwin.c,v 2.1 1993/09/22 15:48:13 dawes Exp $ */

/* WD90C31 code: Mike Tierney <floyd@eng.umd.edu> */


#include "X.h"

#include "windowstr.h"
#include "regionstr.h"
#include "pixmapstr.h"
#include "scrnintstr.h"

#include "cfb.h"
#include "cfbmskbits.h"
#include "vgaBank.h"

#include "compiler.h"
#include "paradise.h"



void
pvgacfbFillBoxSolid (pDrawable, nBox, pBox, pixel1, pixel2, alu)
    DrawablePtr	    pDrawable;
    int		    nBox;
    BoxPtr	    pBox;
    unsigned long   pixel1;
    unsigned long   pixel2;
    int	            alu;
{
    unsigned char   *pdstBase;
    unsigned long   pdst;
    unsigned long    widthDst;
    unsigned long    h;
    unsigned long   fill1;
    unsigned long   w;

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

    if (!CHECKSCREEN(pdstBase))
    {
        pvga1_stdcfbFillBoxSolid (pDrawable, nBox, pBox, pixel1, pixel2, alu);
        return;
    }

    fill1 = PFILL(pixel1);

    for (; nBox; nBox--, pBox++)
    {
      /** wait for last blit to finish **/
       WAIT_BLIT;
       SET_BLT_CNTRL2  (0x0);
       SET_BLT_RAS_OP  ((alu << 8));
       SET_BLT_MASK    (0XFF);
       SET_BLT_SRC_LOW (0x00);
       SET_BLT_SRC_HGH (0x00);

       pdst = pBox->y1 * widthDst + pBox->x1;
       SET_BLT_DST_LOW ((pdst & 0xFFF));
       SET_BLT_DST_HGH ((pdst >> 12));
       SET_BLT_ROW_PTCH (widthDst);

       h = pBox->y2 - pBox->y1;
       w = pBox->x2 - pBox->x1;
       SET_BLT_DIM_X (w);
       SET_BLT_DIM_Y (h);

       SET_BLT_FOR_COLR (fill1 & 0xFF);
       SET_BLT_CNTRL1  (BLT_ACT_STAT | BLT_PACKED | BLT_SRC_FCOL);
    }
    WAIT_BLIT; /* must wait since memory writes can mess up as well */
}
