/* $XFree86: mit/server/ddx/x386/vga256/drivers/ati/bank.s,v 1.5 1993/02/24 10:52:41 dawes Exp $ */
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
 * These are here the very lowlevel VGA bankswitching routines.
 * The segment to switch to is passed via %eax. Only %eax and %edx my be used
 * without saving the original contents.
 *
 * WHY ASSEMBLY LANGUAGE ???
 *
 * These routines must be callable by other assembly routines. But I don't
 * want to have the overhead of pushing and poping the normal stack-frame.
 *
 * Enhancements to support most VGA Wonder cards (including Plus and XL)
 * by Doug Evans, dje@sspiff.UUCP.
 * ALL DISCLAIMERS APPLY TO MY ADDITIONS AS WELL.
 *
 *	I've come to believe the people who design these cards couldn't
 *	allow for future enhancements if their life depended on it!
 *	The register set architecture is a joke!
 *
 * Boards V4 and V5 have the 18800-1 chip. Boards Plus and XL have the 28800
 * chip. Page selection is done with Extended Register 1CE, Index B2.
 * The format is:
 *
 * D7-D5 = Read page select bits 2-0
 * D4    = Reserved (18800-1)
 * D4    = Read page select bit 3 (28800)	Arrggghhhh!!!!
 * D3-D1 = Page select bits 2-0
 * D0    = Reserved (18800-1)
 * D0    = Page select bit 3 (28800)		Arrggghhhh!!!!
 *
 * Actually, it's even worse than this. The manual can't make up it's mind
 * whether its:
 *		R2 R1 R0 R3 W2 W1 W0 W3
 * or
 *		R2 R1 R0 W3 W2 W1 W0 R3
 *
 * It appears that the format is: R2 R1 R0 W3 W2 W1 W0 R3.
 */

#include "assyntax.h"

	FILE("atibank.s")
	AS_BEGIN

/**
 ** Please read the notes in driver.c !!!
 **/

	SEG_DATA

/*
 * We have a mirror for the segment register because an I/O read costs so much
 * more time, that is better to keep the value of it in memory.
 */

Segment:
	D_BYTE 0
 
/*
 * The functions ...
 */

	SEG_TEXT


	ALIGNTEXT4
	GLOBL	GLNAME(ATISetRead)
GLNAME(ATISetRead):
	AND_L	(CONST(0x0F),EAX)			/* FIXME: necessary? */
	MOV_B	(CONTENT(Segment),AH)
	AND_B	(CONST(0x1E),AH)
	ROR_B	(CONST(3),AL)
	OR_B	(AL,AH)
	MOV_B	(AH,CONTENT(Segment))
	MOV_W	(CONTENT(GLNAME(ATIExtReg)),DX)
  	MOV_B	(CONST(0xB2),AL)
	OUT1_W	(DX)
	RET

        ALIGNTEXT4
	GLOBL	GLNAME(ATISetWrite)
GLNAME(ATISetWrite):
	AND_L	(CONST(0x0F),EAX)			/* FIXME: necessary? */
	MOV_B	(CONTENT(Segment),AH)
	AND_B	(CONST(0xE1),AH)
	SHL_B	(CONST(1),AL)
	OR_B	(AL,AH)
	MOV_B	(AH,CONTENT(Segment))
	MOV_W	(CONTENT(GLNAME(ATIExtReg)),DX)
  	MOV_B	(CONST(0xB2),AL)
	OUT1_W	(DX)
	RET
	
	ALIGNTEXT4
	GLOBL	GLNAME(ATISetReadWrite)
GLNAME(ATISetReadWrite):
	AND_L	(CONST(0x0F),EAX)			/* FIXME: necessary? */
	MOV_B	(AL,AH)
	SHL_B	(CONST(1),AH)
	ROR_B	(CONST(3),AL)
	OR_B	(AL,AH)
        MOV_B   (AH,CONTENT(Segment))
	MOV_W	(CONTENT(GLNAME(ATIExtReg)),DX)
  	MOV_B	(CONST(0xB2),AL)
	OUT1_W	(DX)
	RET
