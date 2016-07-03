/*
 * $XFree86: mit/server/ddx/x386/vga256/drivers/cirrus/cir_driver.h,v 2.0 1993/09/21 15:24:30 dawes Exp $
 *
 * Copyright 1993 by Simon P. Cooper, New Brunswick, New Jersey, USA.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Simon P. Cooper not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Simon P. Cooper makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * SIMON P. COOPER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL SIMON P. COOPER BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Simon P. Cooper, <scooper@vizlab.rutgers.edu>
 *
 * Id: cir_driver.h,v 0.7 1993/09/16 01:07:25 scooper Exp
 */

extern void CirrusFillBoxSolid();
extern void CirrusFillRectSolidCopy();
extern int  CirrusDoBitbltCopy();

#define CROP_0			0x00	/*     0 */
#define CROP_1			0x0E	/*     1 */
#define CROP_SRC		0x0D	/*     S */
#define CROP_DST		0x06	/*     D */
#define CROP_NOT_SRC		0xD0	/*    ~S */
#define CROP_NOT_DST		0x0B	/*    ~D */
#define CROP_AND		0x05	/*   S.D */
#define CROP_SRC_AND_NOT_DST	0x09	/*  S.~D */
#define CROP_NOT_SRC_AND_DST	0x50	/*  ~S.D */
#define CROP_NOR		0x90	/* ~S.~D */
#define CROP_OR			0x6D	/*   S+D */
#define CROP_SRC_OR_NOT_DST	0xAD	/*  S+~D */
#define CROP_NOT_SRC_OR_DST	0xD6	/*  ~S+D */
#define CROP_NAND		0xDA	/* ~S+~D */
#define CROP_XOR		0x59	/*  S~=D */
#define CROP_XNOR		0x95	/*   S=D */

