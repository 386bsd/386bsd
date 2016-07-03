/*
 * Copyright 1992 by Kevin E. Martin, Chapel Hill, North Carolina.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Kevin E. Martin not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Kevin E. Martin makes no
 * representations about the suitability of this software for any purpose.
 * It is provided "as is" without express or implied warranty.
 *
 * KEVIN E. MARTIN DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEVIN E. MARTIN BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 */

/*
 * Modified by Amancio Hasty and Jon Tombs
 *
 * Id: s3.h,v 2.2 1993/06/22 20:54:09 jon Exp jon
*/

/* $XFree86: mit/server/ddx/x386/accel/s3/s3.h,v 2.22 1993/09/29 11:10:56 dawes Exp $ */

#ifndef S3_H
#define S3_H

#define S3_PATCHLEVEL "0"

#ifndef LINKKIT
#include "s3name.h"

#include "X.h"
#include "Xmd.h"
#include "input.h"
#include "scrnintstr.h"
#include "mipointer.h"
#include "cursorstr.h"
#include "compiler.h"
#include "misc.h"
#include "x386.h"
#include "regionstr.h"
#include "xf86_OSlib.h"
#include "x386Procs.h"

#include "s3Cursor.h"

#include <X11/Xfuncproto.h>

#if defined(S3_MMIO) && (defined(__STDC__) || defined(__GNUC__))
# define S3_OUTW(p,n) *(volatile unsigned short *)(vgaBase+(p)) = \
			(unsigned short)(n)
#else
# define S3_OUTW(p,n) outw(p, n)
#endif

#else /* !LINKKIT */
#include "X.h"
#include "input.h"
#include "misc.h"
#include "x386.h"
#include "xf86_OSlib.h"
#endif /* !LINKKIT */

#if !defined(__GNUC__) || defined(NO_INLINE)
#define __inline__ /**/
#endif

typedef struct {
   Bool (*ChipProbe)();
   char *(*ChipIdent)();
   void (*ChipEnterLeaveVT)();
   Bool (*ChipInitialize)();
   void (*ChipAdjustFrame)();
   Bool (*ChipSwitchMode)();
} s3VideoChipRec, *s3VideoChipPtr;

extern ScrnInfoRec s3InfoRec;

#ifndef LINKKIT
_XFUNCPROTOBEGIN

extern void s3WarpCursor();
extern void s3RestoreCursor();
extern void s3QueryBestSize();
extern void (*s3ImageReadFunc)();
extern void (*s3ImageWriteFunc)();
extern void (*s3ImageFillFunc)();
extern Bool s3ScreenInit();
extern void vgaGetClocks();
extern Bool s3CursorInit();
extern Bool s3Probe();
extern Bool s3Initialize();
extern void s3EnterLeaveVT();
extern void s3Dline();
extern void s3Dsegment();
extern Bool s3SaveScreen();
extern Bool s3CloseScreen();
extern void s3RestoreDACvalues();
extern int s3GetInstalledColormaps();
extern int s3ListInstalledColormaps();
extern void s3StoreColors();
extern void s3InstallColormap();
extern void s3UninstallColormap();
extern void s3RestoreColor0();
extern void s3AdjustFrame(
#if NeedFunctionPrototypes
	int x, int y
#endif
);
extern Bool s3SwitchMode(
#if NeedFunctionPrototypes
	DisplayModePtr mode
#endif
);
extern Bool s3RealizeFont();
extern Bool s3UnrealizeFont();
extern Bool s3Init(
#if NeedFunctionPrototypes
	DisplayModePtr mode
#endif
);
extern void InitEnvironment(
#if NeedFunctionPrototypes
	void
#endif
);
extern void s3CleanUp(
#if NeedFunctionPrototypes
	void
#endif
);
extern void *s3Save();
extern void s3Unlock();
extern void s3Restore();

extern void s3ImageStipple();
extern void s3ImageOpStipple(
#if NeedFunctionPrototypes
	int, int, int, int, unsigned char *, int, int,
	int, int, int, int, int, short, short
#endif
);

extern int s3PcachOverflow();
extern void s3CacheInit();
extern void s3FontCache8Init();
extern void s3ImageInit();

extern int s3CacheTile();
extern int s3CacheStipple();
extern int s3CacheOpStipple();
extern int s3CImageText8();
extern void s3CImageFill();
extern void s3CImageStipple();
extern void s3CImageOpStipple();
extern void s3CacheFreeSlot();


extern int s3CPolyText8();
extern void s3UnCacheFont8();
extern void s3BitCache8Init();

extern void s3PolyPoint();
extern void s3Line();
extern void s3Segment();

extern void s3SetSpans();
extern void s3GetSpans();

extern void s3SolidFSpans();
extern void s3TiledFSpans();
extern void s3StipFSpans();
extern void s3OStipFSpans();

extern void s3PolyFillRect();

extern int s3PolyText8();
extern void s3ImageText8();
extern int s3PolyText16();
extern void s3ImageText16();

extern void s3FindOrdering();
extern RegionPtr s3CopyArea();
extern RegionPtr s3CopyPlane();
extern void s3CopyWindow();
extern void s3GetImage();

extern void s3SaveAreas();
extern void s3RestoreAreas();

extern Bool s3CreateGC();

extern int s3DisplayWidth;
extern int s3ScissB;
extern short s3alu[];
extern pointer s3VideoMem;
extern pointer vgaBase;
extern ScreenPtr s3savepScreen;

/* also needed in s3 somebody fix these headers please */
extern Bool mfbRegisterCopyPlaneProc();
extern int cfbReduceRasterOp();
extern int miFindMaxBand();
extern int miClipSpans();
extern void DoChangeGC();
extern void QueryGlyphExtents();
extern int cfbExpandDirectColors();
extern int QueryColors();
extern int miScreenInit();
extern int mfbClipLine();
extern int miPolyText8();
extern int miPolyText16();

extern int vgaCRIndex;
extern int vgaCRReg;

extern int s3ValidTokens[];
_XFUNCPROTOEND

#endif /* !LINKKIT */
#endif /* S3_H */

