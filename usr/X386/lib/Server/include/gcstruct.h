/* $XConsortium: gcstruct.h,v 5.3 91/02/21 18:55:33 rws Exp $ */
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

#ifndef GCSTRUCT_H
#define GCSTRUCT_H

#include "gc.h"

#include "miscstruct.h"
#include "region.h"
#include "pixmap.h"
#include "screenint.h"

/*
 * functions which modify the state of the GC
 */

typedef struct _GCFuncs {
    void	(* ValidateGC)();   /* pGC, pDrawable */
    void	(* ChangeGC)();	    /* pGC, mask */
    void	(* CopyGC)();	    /* pGCSrc, mask, pGCDst */
    void	(* DestroyGC)();    /* pGC */
    void	(* ChangeClip)();   /* pGC, clipType, pointer, nrects */
    void	(* DestroyClip)();  /* pGC */
    void	(* CopyClip)();	    /* pgcDst, pgcSrc */
    DevUnion	devPrivate;
} GCFuncs;

/*
 * graphics operations invoked through a GC
 */

typedef struct _GCOps {
    void	(* FillSpans)();
    void	(* SetSpans)();
    void	(* PutImage)();
    RegionPtr	(* CopyArea)();
    RegionPtr	(* CopyPlane)();
    void	(* PolyPoint)();
    void	(* Polylines)();
    void	(* PolySegment)();
    void	(* PolyRectangle)();
    void	(* PolyArc)();
    void	(* FillPolygon)();
    void	(* PolyFillRect)();
    void	(* PolyFillArc)();
    int		(* PolyText8)();
    int		(* PolyText16)();
    void	(* ImageText8)();
    void	(* ImageText16)();
    void	(* ImageGlyphBlt)();
    void	(* PolyGlyphBlt)();
    void	(* PushPixels)();
    void	(* LineHelper)();
    DevUnion	devPrivate;
} GCOps;

/* there is padding in the bit fields because the Sun compiler doesn't
 * force alignment to 32-bit boundaries.  losers.
 */
typedef struct _GC {
    ScreenPtr		pScreen;		
    unsigned char	depth;    
    unsigned char	alu;
    unsigned short	lineWidth;          
    unsigned short	dashOffset;
    unsigned short	numInDashList;
    unsigned char	*dash;
    unsigned int	lineStyle : 2;
    unsigned int	capStyle : 2;
    unsigned int	joinStyle : 2;
    unsigned int	fillStyle : 2;
    unsigned int	fillRule : 1;
    unsigned int 	arcMode : 1;
    unsigned int	subWindowMode : 1;
    unsigned int	graphicsExposures : 1;
    unsigned int	clientClipType : 2; /* CT_<kind> */
    unsigned int	miTranslate:1; /* should mi things translate? */
    unsigned int	tileIsPixel:1; /* tile is solid pixel */
    unsigned int	unused:16; /* see comment above */
    unsigned long	planemask;
    unsigned long	fgPixel;
    unsigned long	bgPixel;
    /*
     * alas -- both tile and stipple must be here as they
     * are independently specifiable
     */
    PixUnion		tile;
    PixmapPtr		stipple;
    DDXPointRec		patOrg;		/* origin for (tile, stipple) */
    struct _Font	*font;
    DDXPointRec		clipOrg;
    DDXPointRec		lastWinOrg;	/* position of window last validated */
    pointer		clientClip;
    unsigned long	stateChanges;	/* masked with GC_<kind> */
    unsigned long       serialNumber;
    GCFuncs		*funcs;
    GCOps		*ops;
    DevUnion		*devPrivates;
} GC;

#endif /* GCSTRUCT_H */
