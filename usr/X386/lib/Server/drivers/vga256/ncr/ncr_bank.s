/*	Copyright 1992 NCR Corporation - Dayton, Ohio, USA */

/* $XFree86: mit/server/ddx/x386/vga256/drivers/ncr/ncr_bank.s,v 1.1 1993/03/08 12:09:02 dawes Exp $ */

/*
 * Copyright 1992,1993 NCR Corporation, Inc.
 *
 * Permission to use, copy, modify, and distribute this software and
 * it's documentation for any purpose and without fee is hereby
 * granted, provided that the above copyright notice appear in all
 * copies and that both that copyright notice and this permission
 * notice appear in supporting documentation, and that the name of
 * NCR not be used in advertising or publicity
 * pertaining to distribution of the software without specific,
 * written prior permission.  NCR makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * NCR DISCLAIMs ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL NCR BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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
 * what happens really here ?
 *
 * Primary Host Offset and Secondary Host Offset are segmentpointers to
 * two segments.  One for Read (Secondary) and one for Write (Primary).
 *
 * Window size of 128K is being used. This results in a shift of 13 bits
 * to put the segment number in the higest 3 bits of the Offset register.
 * Because we are also using Extended chain 4 mode (CHX4), there is already
 * a two bit shift in place. This means that we only have to shift 11 bit
 * to get everything right.
 */

#include "assyntax.h"

	FILE("ncr_bank.s")

	AS_BEGIN

	SEG_TEXT

/* 
 * for Read operations, we are using Secondary offset address as write pointer
 * to a 128k window.
 * for Write operations, we are using Primary offset address as write pointer
 * to a 128k window.
 */
	ALIGNTEXT4
	GLOBL GLNAME(NCRSetReadWrite)
GLNAME(NCRSetReadWrite):
	SHL_L	(CONST(11),EAX)       /* combined %al*32 & movb %al,%ah */
	MOV_B	(CONST(0x18),AL)
	MOV_L	(CONST(0x3C4),EDX)
	OUT_W
	MOV_B	(CONST(0x1C),AL)
	MOV_L	(CONST(0x3C4),EDX)
	OUT_W
	RET

/* 
 * for Write operations, we are using Primary offset address as write pointer
 * to a 128k window.
 */
	ALIGNTEXT4
	GLOBL GLNAME(NCRSetWrite)
GLNAME(NCRSetWrite):
	SHL_L	(CONST(11),EAX)
	MOV_B	(CONST(0x18),AL)
	MOV_L	(CONST(0x3C4),EDX)
	OUT_W
	RET

/* 
 * for Read operations, we are using Secondary offset address as write pointer
 * to a 128k window.
 */
	ALIGNTEXT4
	GLOBL GLNAME(NCRSetRead)
GLNAME(NCRSetRead):
	SHL_L	(CONST(11),EAX)
	MOV_B	(CONST(0x1C),AL)
	MOV_L	(CONST(0x3C4),EDX)
	OUT_W
	RET
