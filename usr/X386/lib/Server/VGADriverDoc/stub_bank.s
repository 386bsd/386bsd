/*
 * Copyright 1993 by David Wexelblat <dwex@goblin.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of David Wexelblat not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  David Wexelblat makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * DAVID WEXELBLAT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL DAVID WEXELBLAT BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $XFree86: mit/server/ddx/x386/VGADriverDoc/stub_bank.s,v 1.2 1993/05/04 10:17:41 dawes Exp $ */

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
 * All XFree86 assembler-language files are written in a macroized assembler
 * format.  This is done to allow a single .s file to be assembled under
 * the USL-syntax assemblers with SVR3/4, the GAS-syntax used by 386BSD,
 * Linux, and Mach, and the Intel-syntax used by Amoeba and Minix.
 *
 * All of the 386 instructions, addressing modes, and appropriate assembler
 * pseudo-ops are definined in the "assyntax.h" header file.
 */
#include "assyntax.h"

/*
 * Three entry points must be defined in this file:
 *
 *	STUBSetRead      - Set the read-bank pointer
 *	STUBSetWrite     - Set the write-bank pointer
 *	STUBSetReadWrite - Set both bank pointers to the same bank
 *
 * Most SVGA chipsets have two bank pointers - a read bank and a write bank.
 * In general, the server sets the read and write banks to the same bank,
 * via the SetReadWrite function.  For BitBlt operations, it is much more
 * efficient to use two banks independently.
 *
 * If the new chipset only has one bank pointer, then all three entry
 * points must still be defined, but they will be all the same function.
 * In this case, make sure the ChipUse2Banks field of the STUBChipRec is
 * set to FALSE in the stub_driver.c file.
 */

	FILE("stub_bank.s")	/* Define the file name for the .o file */

	AS_BEGIN		/* This macro does all generic setup */

/*
 * Some chipsets maintain both bank pointers in a single register.  To
 * avoid having to do a read/modify/write cycle on the register, it is
 * best to maintain a copy of the register in memory, modify the 
 * appropriate part of it, and then do a single 'OUT_B' to set it.
 *
 *	SEG_DATA		/* Switch to the data segment */
 *Segment:			/* Storage for efficiency */
 *	D_BYTE 0
 */

	SEG_TEXT		/* Switch to the text segment */

/* 
 * The SetReadWrite function sets both bank pointers.  The bank will be
 * passed in AL.  As an example, this stub assumes that the read bank 
 * register is register 'base', index 'idx_r', and the write bank register
 * is index 'idx_w'.
 */
	ALIGNTEXT4		
	GLOBL	GLNAME(STUBSetReadWrite)
GLNAME(STUBSetReadWrite):
	MOV_B	(AL, AH)		/* Move bank to high half */
	MOV_B	(CONST(idx_r),AL)	/* Put read index in low byte */
	MOV_L	(CONST(base),EDX)	/* Store base register */
	OUT_W				/* Output read bank */
	MOV_B	(CONST(idx_w),AL)	/* Put write index in low byte */
	OUT_W				/* Output write bank */
	RET

/* 
 * The SetWrite function sets just the write bank pointer
 */
	ALIGNTEXT4
	GLOBL	GLNAME(STUBSetWrite)
GLNAME(STUBSetWrite):
	MOV_B	(AL, AH)		/* Move bank to high half */
	MOV_L	(CONST(base),EDX)	/* Store base register */
	MOV_B	(CONST(idx_w),AL)	/* Put write index in low byte */
	OUT_W				/* Output write bank */
	RET

/* 
 * The SetRead function sets just the read bank pointer
 */
	ALIGNTEXT4
	GLOBL	GLNAME(STUBSetRead)
GLNAME(STUBSetRead):
	MOV_B	(AL, AH)		/* Move bank to high half */
	MOV_B	(CONST(idx_r),AL)	/* Put read index in low byte */
	MOV_L	(CONST(base),EDX)	/* Store base register */
	OUT_W				/* Output read bank */
	RET
