/*
 * cfbfuncs.h
 */

/* $XFree86: mit/server/ddx/x386/vga256/cfb.banked/cfbfuncs.h,v 2.2 1993/10/03 14:57:20 dawes Exp $ */

#ifndef _CFBFUNCS_H
#define _CFBFUNCS_H

#include "gcstruct.h"
extern GCOps cfbTEOps1Rect, cfbTEOps, cfbNonTEOps1Rect, cfbNonTEOps;

typedef struct _Cfbfunc{
    void (*vgaBitblt)();
    int (*doBitbltCopy)();
    void (*fillRectSolidCopy)();
    void (*fillRectTransparentStippled32)();
    void (*fillRectOpaqueStippled32)();
    void (*segmentSS)();
    void (*lineSS)();
    void (*fillBoxSolid)();
    void (*teGlyphBlt8)();
} CfbfuncRec, *CfbfuncPtr;

extern CfbfuncRec cfbLowlevFuncs;

#endif /* _CFBFUNCS_H */
