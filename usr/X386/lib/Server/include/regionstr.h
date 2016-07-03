/* $XConsortium: regionstr.h,v 1.5 89/07/09 15:34:24 rws Exp $ */
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
#ifndef REGIONSTRUCT_H
#define REGIONSTRUCT_H

#include "region.h"
#include "miscstruct.h"
/* 
 *   clip region
 */

typedef struct _RegData {
    long	size;
    long 	numRects;
/*  BoxRec	rects[size];   in memory but not explicitly declared */
} RegDataRec, *RegDataPtr;

typedef struct _Region {
    BoxRec 	extents;
    RegDataPtr	data;
} RegionRec;

#define REGION_NIL(reg) ((reg)->data && !(reg)->data->numRects)
#define REGION_NUM_RECTS(reg) ((reg)->data ? (reg)->data->numRects : 1)
#define REGION_SIZE(reg) ((reg)->data ? (reg)->data->size : 0)
#define REGION_RECTS(reg) ((reg)->data ? (BoxPtr)((reg)->data + 1) \
			               : &(reg)->extents)
#define REGION_BOXPTR(reg) ((BoxPtr)((reg)->data + 1))
#define REGION_BOX(reg,i) (&REGION_BOXPTR(reg)[i])
#define REGION_TOP(reg) REGION_BOX(reg, (reg)->data->numRects)
#define REGION_END(reg) REGION_BOX(reg, (reg)->data->numRects - 1)
#define REGION_SZOF(n) (sizeof(RegDataRec) + ((n) * sizeof(BoxRec)))

#endif /* REGIONSTRUCT_H */
