/* $XConsortium: window.h,v 1.4 89/07/04 16:09:11 rws Exp $ */
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

#ifndef WINDOW_H
#define WINDOW_H
#define TOTALLY_OBSCURED 0
#define UNOBSCURED 1
#define OBSCURED 2

#define VisibilityNotViewable	3

/* return values for tree-walking callback procedures */
#define WT_STOPWALKING		0
#define WT_WALKCHILDREN		1
#define WT_DONTWALKCHILDREN	2
#define WT_NOMATCH 3
#define NullWindow ((WindowPtr) 0)

typedef struct _BackingStore *BackingStorePtr;
typedef struct _Window *WindowPtr;

#endif /* WINDOW_H */
