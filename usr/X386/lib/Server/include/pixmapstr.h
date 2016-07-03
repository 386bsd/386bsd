/* $XFree86: mit/server/include/pixmapstr.h,v 2.0 1993/07/24 07:15:21 dawes Exp $ */
/* $XConsortium: pixmapstr.h,v 5.0 89/06/09 15:00:35 keith Exp $ */
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

#ifndef PIXMAPSTRUCT_H
#define PIXMAPSTRUCT_H
#include "pixmap.h"
#include "screenint.h"
#include "miscstruct.h"

typedef struct _Drawable {
    unsigned char	type;	/* DRAWABLE_<type> */
    unsigned char	class;	/* specific to type */
    unsigned char	depth;
    unsigned char	bitsPerPixel;
    unsigned long	id;	/* resource id */
    short		x;	/* window: screen absolute, pixmap: 0 */
    short		y;	/* window: screen absolute, pixmap: 0 */
    unsigned short	width;
    unsigned short	height;
    ScreenPtr		pScreen;
    unsigned long	serialNumber;
} DrawableRec;

/*
 * PIXMAP -- device dependent 
 */

typedef struct _Pixmap {
    DrawableRec		drawable;
    int			slot;		/* Offscreen cache slot number */
    int			cacheId;	/* Pixmap id number */
    int			refcnt;
    int			devKind;
    DevUnion		devPrivate;
} PixmapRec;

#endif /* PIXMAPSTRUCT_H */
