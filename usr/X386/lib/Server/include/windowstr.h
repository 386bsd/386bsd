/* $XConsortium: windowstr.h,v 5.13 89/10/04 10:27:15 rws Exp $ */
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

#ifndef WINDOWSTRUCT_H
#define WINDOWSTRUCT_H

#include "window.h"
#include "pixmapstr.h"
#include "regionstr.h"
#include "cursor.h"
#include "property.h"
#include "resource.h"	/* for ROOT_WINDOW_ID_BASE */
#include "dix.h"
#include "miscstruct.h"
#include "Xprotostr.h"

#define GuaranteeNothing	0
#define GuaranteeVisBack	1

#define SameBackground(as, a, bs, b)				\
    ((as) == (bs) && ((as) == None ||				\
		      (as) == ParentRelative ||			\
 		      SamePixUnion(a,b,as == BackgroundPixel)))

#define SameBorder(as, a, bs, b)				\
    EqualPixUnion(as, a, bs, b)

typedef struct _WindowOpt {
    VisualID		visual;		   /* default: same as parent */
    CursorPtr		cursor;		   /* default: window.cursorNone */
    Colormap		colormap;	   /* default: same as parent */
    Mask		dontPropagateMask; /* default: window.dontPropagate */
    Mask		otherEventMasks;   /* default: 0 */
    struct _OtherClients *otherClients;	   /* default: NULL */
    struct _GrabRec	*passiveGrabs;	   /* default: NULL */
    PropertyPtr		userProps;	   /* default: NULL */
    unsigned long	backingBitPlanes;  /* default: ~0L */
    unsigned long	backingPixel;	   /* default: 0 */
#ifdef SHAPE
    RegionPtr		boundingShape;	   /* default: NULL */
    RegionPtr		clipShape;	   /* default: NULL */
#endif
#ifdef XINPUT
    struct _OtherInputMasks *inputMasks;   /* default: NULL */
#endif
} WindowOptRec, *WindowOptPtr;

#define BackgroundPixel	    2L
#define BackgroundPixmap    3L

typedef struct _Window {
    DrawableRec		drawable;
    WindowPtr		parent;		/* ancestor chain */
    WindowPtr		nextSib;	/* next lower sibling */
    WindowPtr		prevSib;	/* next higher sibling */
    WindowPtr		firstChild;	/* top-most child */
    WindowPtr		lastChild;	/* bottom-most child */
    RegionRec		clipList;	/* clipping rectangle for output */
    RegionRec		borderClip;	/* NotClippedByChildren + border */
    union _Validate	*valdata;
    RegionRec		winSize;
    RegionRec		borderSize;
    DDXPointRec		origin;		/* position relative to parent */
    unsigned short	borderWidth;
    unsigned short	deliverableEvents;
    Mask		eventMask;
    PixUnion		background;
    PixUnion		border;
    pointer		backStorage;	/* null when BS disabled */
    WindowOptPtr	optional;
    unsigned		backgroundState:2; /* None, Relative, Pixel, Pixmap */
    unsigned		borderIsPixel:1;
    unsigned		cursorIsNone:1;	/* else same as parent */
    unsigned		backingStore:2;
    unsigned		saveUnder:1;
    unsigned		DIXsaveUnder:1;
    unsigned		bitGravity:4;
    unsigned		winGravity:4;
    unsigned		overrideRedirect:1;
    unsigned		visibility:2;
    unsigned		mapped:1;
    unsigned		realized:1;	/* ancestors are all mapped */
    unsigned		viewable:1;	/* realized && InputOutput */
    unsigned		dontPropagate:3;/* index into DontPropagateMasks */
    DevUnion		*devPrivates;
} WindowRec;

/*
 * Ok, a bunch of macros for accessing the optional record
 * fields (or filling the appropriate default value)
 */

extern WindowPtr    FindWindowWithOptional();
extern Mask	    DontPropagateMasks[];

#define wTrackParent(w,field)	((w)->optional ? \
				    w->optional->field \
 				 : FindWindowWithOptional(w)->optional->field)
#define wUseDefault(w,field,def)	((w)->optional ? \
				    w->optional->field \
				 : def)

#define wVisual(w)		wTrackParent(w, visual)
#define wCursor(w)		((w)->cursorIsNone ? None : wTrackParent(w, cursor))
#define wColormap(w)		((w)->drawable.class == InputOnly ? None : wTrackParent(w, colormap))
#define wDontPropagateMask(w)	wUseDefault(w, dontPropagateMask, DontPropagateMasks[(w)->dontPropagate])
#define wOtherEventMasks(w)	wUseDefault(w, otherEventMasks, 0)
#define wOtherClients(w)	wUseDefault(w, otherClients, NULL)
#ifdef XINPUT
#define wOtherInputMasks(w)	wUseDefault(w, inputMasks, NULL)
#else
#define wOtherInputMasks(w)	NULL
#endif
#define wPassiveGrabs(w)	wUseDefault(w, passiveGrabs, NULL)
#define wUserProps(w)		wUseDefault(w, userProps, NULL)
#define wBackingBitPlanes(w)	wUseDefault(w, backingBitPlanes, ~0L)
#define wBackingPixel(w)	wUseDefault(w, backingPixel, 0)
#ifdef SHAPE
#define wBoundingShape(w)	wUseDefault(w, boundingShape, NULL)
#define wClipShape(w)		wUseDefault(w, clipShape, NULL)
#endif
#define wClient(w)		(clients[CLIENT_ID((w)->drawable.id)])
#define wBorderWidth(w)		((int) (w)->borderWidth)

/* true when w needs a border drawn. */

#ifdef SHAPE
#define HasBorder(w)	((w)->borderWidth || wClipShape(w))
#else
#define HasBorder(w)	((w)->borderWidth)
#endif

extern int DeleteWindow();
extern int ChangeWindowAttributes();
extern int WalkTree();
extern Bool CreateRootWindow();
extern WindowPtr CreateWindow();
extern int DeleteWindow();
extern int DestroySubwindows();
extern int ChangeWindowAttributes();
extern int GetWindowAttributes();
extern int ConfigureWindow();
extern int ReparentWindow();
extern int MapWindow();
extern int MapSubwindow();
extern int UnmapWindow();
extern int UnmapSubwindow();
extern RegionPtr NotClippedByChildren();
extern void SendVisibilityNotify();

#endif /* WINDOWSTRUCT_H */

