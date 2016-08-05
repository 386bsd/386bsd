/* Copyright (C) 1989 Aladdin Enterprises.  All rights reserved.
   Distributed by Free Software Foundation, Inc.

This file is part of Ghostscript.

Ghostscript is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility
to anyone for the consequences of using it or for whether it serves any
particular purpose or works at all, unless he says so in writing.  Refer
to the Ghostscript General Public License for full details.

Everyone is granted permission to copy, modify and redistribute
Ghostscript, but only under the conditions described in the Ghostscript
General Public License.  A copy of this license is supposed to have been
given to you along with Ghostscript so you can know your rights and
responsibilities.  It should be in a file named COPYING.  Among other
things, the copyright notice and this notice must be preserved on all
copies.  */

/* gzdevice.h */
/* Private structures for describing devices for GhostScript */
/* requires gsmatrix.h */
#include "gxdevice.h"

/* Structure for a device in the graphics state */
typedef struct device_s {
	gx_device *info;		/* points to actual device info */
	int is_band_device;		/* if true, must render in pieces */
	gx_color_index white, black;	/* device color codes */
} device;
