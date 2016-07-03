/*
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
 * /proj/X11/mit/server/ddx/at386/vga/RCS/vgaBitBlt.c,v 1.5 91/02/10 16:44:40 root Exp
 */

/* $XFree86: mit/server/ddx/x386/vga256/drivers/pvga1/pvgaBitBlt.c,v 2.1 1993/09/22 15:48:11 dawes Exp $ */

/* 90C31 accel code: Mike Tierney <floyd@eng.umd.edu> */

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


void
pvgaBitBlt(pdstBase, psrcBase, widthSrc, widthDst, x, y,
	    x1, y1, w, h, xdir, ydir, alu, planemask)
     unsigned char *psrcBase, *pdstBase;  /* start of src and dst bitmaps */
     int    widthSrc, widthDst;
     int    x, y, x1, y1, w, h;
     int    xdir, ydir;
     int    alu;
     unsigned long  planemask;

{
  int psrc, pdst;
  int blit_direct;

   if (xdir == 1 && ydir == 1)    /* left to right */ /* top to bottom */
   {
      psrc = (y*widthSrc)+x;
      pdst = (y1*widthDst)+x1;
      blit_direct = 0x0;
   }
   else                           /* right to left */ /* bottom to top */
   {
      psrc = ((y+h-1)*widthSrc)+x+w;
      pdst = ((y1+h-1)*widthDst)+x1+w;
      blit_direct = BLT_DIRECT;
   }

   WAIT_BLIT;
   SET_BLT_SRC_LOW ((psrc & 0xFFF));
   SET_BLT_SRC_HGH ((psrc >> 12));
   SET_BLT_DST_LOW ((pdst & 0xFFF));
   SET_BLT_DST_HGH ((pdst >> 12));
   SET_BLT_ROW_PTCH (widthDst);
   SET_BLT_DIM_X    (w);
   SET_BLT_DIM_Y    (h);
   SET_BLT_MASK     ((planemask & 0xFF));
   SET_BLT_RAS_OP   ((alu << 8));
   SET_BLT_CNTRL2   (0x00);
   SET_BLT_CNTRL1 (BLT_ACT_STAT | BLT_PACKED | BLT_SRC_COLR | blit_direct);

   WAIT_BLIT; /* must wait, since memory writes can mess up as well */
}
