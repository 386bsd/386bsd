/*
* $XConsortium: VendorP.h,v 1.20 89/10/04 12:22:55 swick Exp $
* $oHeader: VendorP.h,v 1.2 88/08/18 15:56:48 asente Exp $
*/

/***********************************************************
Copyright 1987, 1988 by Digital Equipment Corporation, Maynard, Massachusetts,
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

/* 
 * VendorP.h - Private definitions for VendorShell widget
 * 
 * Author:	Paul Asente
 * 		Digital Equipment Corporation
 * 		Western Software Laboratory
 * Date:	Thu Dec 3, 1987
 */

/***********************************************************************
 *
 * VendorShell Widget Private Data
 *
 ***********************************************************************/

#ifndef  _XtVendorPrivate_h
#define _XtVendorPrivate_h

#include <X11/Vendor.h>

/* New fields for the VendorShell widget class record */

typedef struct {
    XtPointer       extension;          /* pointer to extension record      */
} VendorShellClassPart;

typedef struct _VendorShellClassRec {
  	CoreClassPart      core_class;
	CompositeClassPart composite_class;
	ShellClassPart  shell_class;
	WMShellClassPart   wm_shell_class;
	VendorShellClassPart vendor_shell_class;
} VendorShellClassRec;

externalref VendorShellClassRec vendorShellClassRec;

/* New fields for the vendor shell widget. */

typedef struct {
	int		vendor_specific;
} VendorShellPart;

typedef  struct {
	CorePart 	core;
	CompositePart 	composite;
	ShellPart 	shell;
	WMShellPart	wm;
	VendorShellPart	vendor;
} VendorShellRec, *VendorShellWidget;

#endif  /* _XtVendorPrivate_h */
