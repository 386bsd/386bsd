/*
 * $XFree86: mit/server/ddx/x386/vga256/drivers/tvga8900/bank.s,v 2.0 1993/10/08 15:59:26 dawes Exp $
 * Copyright 1992 by Alan Hourihane, Wigan, England.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Alan Hourihane not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Alan Hourihane makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * ALAN HOURIHANE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL ALAN HOURIHANE BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Alan Hourihane, alanh@logitek.co.uk, version 0.1beta
 *
 *	    David Wexelblat, dwex@goblin.org, version 0.2beta
 *		(dual banks)
 *	    David Wexelblat, version 0.3beta (back to 1 bank)
 */


/*
 * These are here the very lowlevel VGA bankswitching routines.
 * The segment to switch to is passed via %eax. Only %eax and %edx my be used
 * without saving the original contents.
 */

#include "assyntax.h"

	FILE("tvga8900bank.s")
	AS_BEGIN

	SEG_DATA

	SEG_TEXT

	ALIGNTEXT4
	GLOBL GLNAME(TVGA8900SetReadWrite)
	GLOBL GLNAME(TVGA8900SetRead)
	GLOBL GLNAME(TVGA8900SetWrite)
GLNAME(TVGA8900SetReadWrite):
GLNAME(TVGA8900SetRead):
GLNAME(TVGA8900SetWrite):
	MOV_B	(AL,AH)
	MOV_B	(CONST(0x0E),AL)
	XOR_B	(CONST(0x02),AH)
	MOV_L	(CONST(0x3C4),EDX)
	OUT1_W	(DX)
	RET
