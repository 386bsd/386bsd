/*
 * $XConsortium: mergerop.h,v 1.7 91/07/18 22:54:58 keith Exp $
 *
 * Copyright 1989 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of M.I.T. not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  M.I.T. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * M.I.T. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL M.I.T.
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Keith Packard, MIT X Consortium
 */

#ifndef _MERGEROP_H_
#define _MERGEROP_H_

#ifndef GXcopy
#include "X.h"
#endif

typedef struct _mergeRopBits {
    unsigned long   ca1, cx1, ca2, cx2;
} mergeRopRec, *mergeRopPtr;

extern mergeRopRec	mergeRopBits[16];

#define DeclareMergeRop() unsigned long   _ca1, _cx1, _ca2, _cx2;
#define DeclarePrebuiltMergeRop()	unsigned long	_cca, _ccx;

#if PPW != 32	/* cfb */
#define InitializeMergeRop(alu,pm) {\
    unsigned long   _pm; \
    mergeRopPtr  _bits; \
    _pm = PFILL(pm); \
    _bits = &mergeRopBits[alu]; \
    _ca1 = _bits->ca1 &  _pm; \
    _cx1 = _bits->cx1 | ~_pm; \
    _ca2 = _bits->ca2 &  _pm; \
    _cx2 = _bits->cx2 &  _pm; \
}
#endif

#if PPW == 32	/* mfb */
#define InitializeMergeRop(alu,pm) {\
    mergeRopPtr  _bits; \
    _bits = &mergeRopBits[alu]; \
    _ca1 = _bits->ca1; \
    _cx1 = _bits->cx1; \
    _ca2 = _bits->ca2; \
    _cx2 = _bits->cx2; \
}
#endif

/* AND has higher precedence than XOR */

#define DoMergeRop(src, dst) \
    ((dst) & ((src) & _ca1 ^ _cx1) ^ ((src) & _ca2 ^ _cx2))

#define DoPrebuiltMergeRop(dst) ((dst) & _cca ^ _ccx)

#define DoMaskPrebuiltMergeRop(dst,mask) \
    ((dst) & (_cca | ~(mask)) ^ (_ccx & (mask)))

#define PrebuildMergeRop(src) ((_cca = (src) & _ca1 ^ _cx1), \
			       (_ccx = (src) & _ca2 ^ _cx2))

#define DoMaskMergeRop(src, dst, mask) \
    ((dst) & (((src) & _ca1 ^ _cx1) | ~(mask)) ^ (((src) & _ca2 ^ _cx2) & (mask)))

#ifndef MROP
#define MROP 0
#endif

#define Mclear		(1<<GXclear)
#define Mand		(1<<GXand)
#define MandReverse	(1<<GXandReverse)
#define Mcopy		(1<<GXcopy)
#define MandInverted	(1<<GXandInverted)
#define Mnoop		(1<<GXnoop)
#define Mxor		(1<<GXxor)
#define Mor		(1<<GXor)
#define Mnor		(1<<GXnor)
#define Mequiv		(1<<GXequiv)
#define Minvert		(1<<GXinvert)
#define MorReverse	(1<<GXorReverse)
#define McopyInverted	(1<<GXcopyInverted)
#define MorInverted	(1<<GXorInverted)
#define Mnand		(1<<GXnand)
#define Mset		(1<<GXset)

#if (MROP) == Mcopy
#define MROP_DECLARE()
#define MROP_DECLARE_REG()
#define MROP_INITIALIZE(alu,pm)
#define MROP_SOLID(src,dst)	(src)
#define MROP_MASK(src,dst,mask)	((dst) & ~(mask) | (src) & (mask))
#define MROP_NAME(prefix)	MROP_NAME_CAT(prefix,Copy)
#endif

#if (MROP) == McopyInverted
#define MROP_DECLARE()
#define MROP_DECLARE_REG()
#define MROP_INITIALIZE(alu,pm)
#define MROP_SOLID(src,dst)	(~(src))
#define MROP_MASK(src,dst,mask)	((dst) & ~(mask) | (~(src)) & (mask))
#define MROP_NAME(prefix)	MROP_NAME_CAT(prefix,CopyInverted)
#endif

#if (MROP) == Mxor
#define MROP_DECLARE()
#define MROP_DECLARE_REG()
#define MROP_INITIALIZE(alu,pm)
#define MROP_SOLID(src,dst)	((src) ^ (dst))
#define MROP_MASK(src,dst,mask)	(((src) & (mask)) ^ (dst))
#define MROP_NAME(prefix)	MROP_NAME_CAT(prefix,Xor)
#endif

#if (MROP) == Mor
#define MROP_DECLARE()
#define MROP_DECLARE_REG()
#define MROP_INITIALIZE(alu,pm)
#define MROP_SOLID(src,dst)	((src) | (dst))
#define MROP_MASK(src,dst,mask)	(((src) & (mask)) | (dst))
#define MROP_NAME(prefix)	MROP_NAME_CAT(prefix,Or)
#endif

#if (MROP) == (Mcopy|Mxor|MandReverse|Mor)
#define MROP_DECLARE()	unsigned long _ca1, _cx1;
#define MROP_DECLARE_REG()	register MROP_DECLARE()
#define MROP_INITIALIZE(alu,pm)	{ \
    mergeRopPtr  _bits; \
    _bits = &mergeRopBits[alu]; \
    _ca1 = _bits->ca1; \
    _cx1 = _bits->cx1; \
}
#define MROP_SOLID(src,dst) \
    ((dst) & ((src) & _ca1 ^ _cx1) ^ (src))
#define MROP_MASK(src,dst,mask)	\
    ((dst) & (((src) & _ca1 ^ _cx1) | ~(mask)) ^ ((src) & (mask)))
#define MROP_NAME(prefix)	MROP_NAME_CAT(prefix,CopyXorAndReverseOr)
#define MROP_PREBUILD(src)	PrebuildMergeRop(src)
#define MROP_PREBUILT_DECLARE()	DeclarePrebuiltMergeRop()
#define MROP_PREBUILT_SOLID(src,dst)	DoPrebuiltMergeRop(dst)
#define MROP_PREBUILT_MASK(src,dst,mask)    DoMaskPrebuiltMergeRop(dst,mask)
#endif

#if (MROP) == 0
#define MROP_DECLARE()	DeclareMergeRop()
#define MROP_DECLARE_REG()	register DeclareMergeRop()
#define MROP_INITIALIZE(alu,pm)	InitializeMergeRop(alu,pm)
#define MROP_SOLID(src,dst)	DoMergeRop(src,dst)
#define MROP_MASK(src,dst,mask)	DoMaskMergeRop(src, dst, mask)
#define MROP_NAME(prefix)	MROP_NAME_CAT(prefix,General)
#define MROP_PREBUILD(src)	PrebuildMergeRop(src)
#define MROP_PREBUILT_DECLARE()	DeclarePrebuiltMergeRop()
#define MROP_PREBUILT_SOLID(src,dst)	DoPrebuiltMergeRop(dst)
#define MROP_PREBUILT_MASK(src,dst,mask)    DoMaskPrebuiltMergeRop(dst,mask)
#endif

#ifndef MROP_PREBUILD
#define MROP_PREBUILD(src)
#define MROP_PREBUILT_DECLARE()
#define MROP_PREBUILT_SOLID(src,dst)	MROP_SOLID(src,dst)
#define MROP_PREBUILT_MASK(src,dst,mask)    MROP_MASK(src,dst,mask)
#endif

#if __STDC__ && !defined(UNIXCPP)
#define MROP_NAME_CAT(prefix,suffix)	prefix##suffix
#else
#define MROP_NAME_CAT(prefix,suffix)	prefix/**/suffix
#endif

#endif
