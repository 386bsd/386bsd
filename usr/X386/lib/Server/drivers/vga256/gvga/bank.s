/* $XFree86: mit/server/ddx/x386/vga256/drivers/gvga/bank.s,v 1.11 1993/02/24 10:42:40 dawes Exp $ */
/*
 * Copyright 1990,91 by Thomas Roell, Dinkelscherben, Germany.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Thomas Roell not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Thomas Roell makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * THOMAS ROELL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THOMAS ROELL BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Thomas Roell, roell@informatik.tu-muenchen.de
 *
 * $Header: /proj/X11/mit/server/ddx/x386/drivers/gvga/RCS/bank.s,v 1.1 1991/06/02 22:37:11 root Exp $
 */


/*
 * These are here the very lowlevel VGA bankswitching routines.
 * The segment to switch to is passed via %eax. Only %eax and %edx my be used
 * without saving the original contents.
 *
 * WHY ASSEMBLY LANGUAGE ???
 *
 * These routines must be callable by other assembly routines. But I don't
 * want to have the overhead of pushing and poping the normal stack-frame.
 */

/*
 * first we have here a mirror for the segment register. That's because a
 * I/O read costs so much more time, that is better to keep the value of it
 * in memory.
 */

#include "assyntax.h"

	FILE("gvgabank.s")

	AS_BEGIN
	SEG_DATA
Segment:
	D_BYTE 0x40
 

	SEG_TEXT

	ALIGNTEXT4
	GLOBL	GLNAME(GVGASetRead)
GLNAME(GVGASetRead):
	MOV_B	(CONTENT(Segment),AH)
	AND_B	(CONST(0x78),AH)
	OR_B	(AL,AH)
	MOV_B	(AH,CONTENT(Segment))
	MOV_B	(CONST(6),AL)
	MOV_L	(CONST(0x3C4),EDX)
	OUT_W
	RET

        ALIGNTEXT4
	GLOBL	GLNAME(GVGASetWrite)
GLNAME(GVGASetWrite):
	MOV_B	(CONTENT(Segment),AH)
	AND_B	(CONST(0x47),AH)
	SHL_B	(CONST(3),AL)
	OR_B	(AL,AH)
	MOV_B	(AH,CONTENT(Segment))
	MOV_B	(CONST(6),AL)
	MOV_L	(CONST(0x3C4),EDX)
	OUT_W
	RET
	
	ALIGNTEXT4
	GLOBL	GLNAME(GVGASetReadWrite)
GLNAME(GVGASetReadWrite):
	MOV_B	(AL,AH)
	SHL_B	(CONST(3),AH)
	OR_B	(AL,AH)
	OR_B	(CONST(0x40),AH)
        MOV_B	(AH,CONTENT(Segment))
	MOV_B	(CONST(6),AL)
	MOV_L	(CONST(0x3C4),EDX)
	OUT_W
        RET

