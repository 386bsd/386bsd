/* $XConsortium: scrnintstr.h,v 5.12 91/05/13 23:44:14 rws Exp $ */
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
#ifndef SCREENINTSTRUCT_H
#define SCREENINTSTRUCT_H

#include "screenint.h"
#include "miscstruct.h"
#include "region.h"
#include "pixmap.h"
#include "gc.h"
#include "colormap.h"


typedef struct _PixmapFormat {
    unsigned char	depth;
    unsigned char	bitsPerPixel;
    unsigned char	scanlinePad;
    } PixmapFormatRec;
    
typedef struct _Visual {
    unsigned long	vid;
    short		class;
    short		bitsPerRGBValue;
    short		ColormapEntries;
    short		nplanes;/* = log2 (ColormapEntries). This does not
				 * imply that the screen has this many planes.
				 * it may have more or fewer */
    unsigned long	redMask, greenMask, blueMask;
    int			offsetRed, offsetGreen, offsetBlue;
  } VisualRec;

typedef struct _Depth {
    unsigned char	depth;
    short		numVids;
    unsigned long	*vids;    /* block of visual ids for this depth */
  } DepthRec;

typedef struct _Screen {
    int			myNum;	/* index of this instance in Screens[] */
    ATOM		id;
    short		width, height;
    short		mmWidth, mmHeight;
    short		numDepths;
    unsigned char      	rootDepth;
    DepthPtr       	allowedDepths;
    unsigned long      	rootVisual;
    unsigned long	defColormap;
    short		minInstalledCmaps, maxInstalledCmaps;
    char                backingStoreSupport, saveUnderSupport;
    unsigned long	whitePixel, blackPixel;
    unsigned long	rgf;	/* array of flags; she's -- HUNGARIAN */
    GCPtr		GCperDepth[MAXFORMATS+1];
			/* next field is a stipple to use as default in
			   a GC.  we don't build default tiles of all depths
			   because they are likely to be of a color
			   different from the default fg pixel, so
			   we don't win anything by building
			   a standard one.
			*/
    PixmapPtr		PixmapPerDepth[1];
    pointer		devPrivate;
    short       	numVisuals;
    VisualPtr		visuals;
    int			WindowPrivateLen;
    unsigned		*WindowPrivateSizes;
    unsigned		totalWindowSize;
    int			GCPrivateLen;
    unsigned		*GCPrivateSizes;
    unsigned		totalGCSize;

    /* Random screen procedures */

    Bool (* CloseScreen)();		/* index, pScreen */
    void (* QueryBestSize)();		/* class, pwidth, pheight, pScreen */
    Bool (* SaveScreen)();		/* pScreen, on */
    void (* GetImage)();		/* pDrawable, sx, sy, w, h, format, 
					 * planemask, pdestbits */
    void (* GetSpans)();		/* pDrawable, wMax, ppt, pwidth,
					 * nspans, pdstbits */
    void (* PointerNonInterestBox)();	/* pScr, BoxPtr */

    void (* SourceValidate)();		/* pDrawable, x, y, w, h */

    /* Window Procedures */

    Bool (* CreateWindow)();		/* pWin */
    Bool (* DestroyWindow)();		/* pWin */
    Bool (* PositionWindow)();		/* pWin, x, y */
    Bool (* ChangeWindowAttributes)();	/* pWin, mask */
    Bool (* RealizeWindow)();		/* pWin */
    Bool (* UnrealizeWindow)();		/* pWin */
    int  (* ValidateTree)();		/* pParent, pChild, kind */
    void (* PostValidateTree)();	/* pParent, pChild, kind */
    void (* WindowExposures)();       /* pWin: WindowPtr, pRegion: RegionPtr */
    void (* PaintWindowBackground)();	/* pWin, pRgn, which */
    void (* PaintWindowBorder)();	/* pWin, pRgn, which */
    void (* CopyWindow)();		/* pWin, oldPt, pOldRegion */
    void (* ClearToBackground)();	/* pWin, x,y,w,h, sendExpose */
    void (* ClipNotify)();              /* pWin, dx, dy */

    /* Pixmap procedures */

    PixmapPtr (* CreatePixmap)(); 	/* pScreen, width, height, depth */
    Bool (* DestroyPixmap)();		/* pPixmap */

    /* Backing store procedures */

    void (* SaveDoomedAreas)();		/* pWin, pRegion, dx, dy */
    RegionPtr (* RestoreAreas)();	/* pWin, pRegion */
    void (* ExposeCopy)();		/* pSrc, pDst, pGC, pRegion, */
					/* srcx, srcy, dstx, dsty, plane */
    RegionPtr (* TranslateBackingStore)();/* pWin, dx, dy, pOldClip */
    RegionPtr (* ClearBackingStore)();	/* pWin, x, y, w, h, sendExpose */
    void (* DrawGuarantee)();		/* pWin, pGC, guarantee */
    
    /* Font procedures */

    Bool (* RealizeFont)();		/* pScr, pFont */
    Bool (* UnrealizeFont)();		/* pScr, pFont */

    /* Cursor Procedures */
    void (* ConstrainCursor)();   	/* pScr, BoxPtr */
    void (* CursorLimits)();		/* pScr, pCurs, BoxPtr, BoxPtr */
    Bool (* DisplayCursor)();		/* pScr, pCurs */
    Bool (* RealizeCursor)();		/* pScr, pCurs */
    Bool (* UnrealizeCursor)();		/* pScr, pCurs */
    void (* RecolorCursor)();		/* pScr, pCurs, displayed */
    Bool (* SetCursorPosition)();	/* pScr, x, y */

    /* GC procedures */

    Bool (* CreateGC)();		/* pGC */

    /* Colormap procedures */

    Bool (* CreateColormap)();		/* pcmap */
    void (* DestroyColormap)();		/* pcmap */
    void (* InstallColormap)();		/* pcmap */
    void (* UninstallColormap)();	/* pcmap */
    int (* ListInstalledColormaps) (); 	/* pScreen, pmaps */
    void (* StoreColors)();		/* pmap, ndef, pdef */
    void (* ResolveColor)();		/* preg, pgreen, pblue */

    /* Region procedures */

    RegionPtr (* RegionCreate)(); 	/* rect, size */
    void (* RegionInit)();		/* pRegion, rect, size */
    Bool (* RegionCopy)();		/* dstrgn, srcrgn */
    void (* RegionDestroy)();		/* pRegion */
    void (* RegionUninit)();		/* pRegion */
    Bool (* Intersect)();		/* newReg, reg1, reg2 */
    Bool (* Union)();			/* newReg, reg1, reg2 */
    Bool (* Subtract)();		/* regD, regM, regS */
    Bool (* Inverse)();			/* newReg, reg1, invRect */
    void (* RegionReset)();		/* pRegion, pBox */
    void (* TranslateRegion)();		/* pRegion, x, y */
    int (* RectIn)();			/* pRegion, pRect */
    Bool (* PointInRegion)();		/* pRegion, x, y, pBox */
    Bool (* RegionNotEmpty)();      	/* pRegion: RegionPtr */
    void (* RegionEmpty)();        	/* pRegion: RegionPtr */
    BoxPtr (* RegionExtents)(); 	/* pRegion: RegionPtr */
    Bool (* RegionAppend)();		/* pRegion, pRegion */
    Bool (* RegionValidate)();		/* pRegion, pOverlap */
    RegionPtr (* BitmapToRegion)();	/* PixmapPtr */
    RegionPtr (* RectsToRegion)();	/* nrects, pRects, ordering */
    void (* SendGraphicsExpose)();	/* client, rgn, draw, major, minor */

    /* os layer procedures */
    void (* BlockHandler)();		/* data: pointer */
    void (* WakeupHandler)();		/* data: pointer */
    pointer blockData;
    pointer wakeupData;

    /* anybody can get a piece of this array */
    DevUnion	*devPrivates;
} ScreenRec;

typedef struct _ScreenInfo {
    int		imageByteOrder;
    int		bitmapScanlineUnit;
    int		bitmapScanlinePad;
    int		bitmapBitOrder;
    int		numPixmapFormats;
    PixmapFormatRec
		formats[MAXFORMATS];
    int		arraySize;
    int		numScreens;
    ScreenPtr	screens[MAXSCREENS];
} ScreenInfo;

extern ScreenInfo screenInfo;

extern int AllocateWindowPrivateIndex(), AllocateGCPrivateIndex();
extern Bool AllocateWindowPrivate(), AllocateGCPrivate();

#endif /* SCREENINTSTRUCT_H */
