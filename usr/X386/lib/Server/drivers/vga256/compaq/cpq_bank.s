/*
 * Copyright 1993 Hans Oey <hans@mo.hobby.nl>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Hans Oey not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Hans Oey makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * HANS OEY DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL HANS OEY BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 */

/* $XFree86: mit/server/ddx/x386/vga256/drivers/compaq/cpq_bank.s,v 1.1 1993/05/04 10:19:20 dawes Exp $ */

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

#include "assyntax.h"

 	FILE("cpq_bank.s")

	AS_BEGIN

	SEG_TEXT

	ALIGNTEXT4
	GLOBL	GLNAME(COMPAQSetRead)
GLNAME(COMPAQSetRead):
/* We map 32k segments with a 4k granularity. */
	SHL_L	(CONST(11),EAX)	/* multiply by 8; mov al, ah */
	MOV_B	(CONST(0x45),AL)
	MOV_L	(CONST(0x3CE),EDX)
        OUT_W
        RET

	ALIGNTEXT4
	GLOBL	GLNAME(COMPAQSetWrite)
	GLOBL	GLNAME(COMPAQSetReadWrite)
GLNAME(COMPAQSetWrite):
GLNAME(COMPAQSetReadWrite):
	SHL_L	(CONST(11),EAX)
	MOV_B	(CONST(0x46),AL)
	MOV_L	(CONST(0x3CE),EDX)
        OUT_W
        RET

