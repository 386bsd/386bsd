/*
 *  exception.h
 *
 *
 * Copyright (C) 1992, 1993  W. Metzenthen, 22 Parker St, Ormond,
 *                           Vic 3163, Australia.
 *                           E-mail apm233m@vaxc.cc.monash.edu.au
 * All rights reserved.
 *
 * This copyright notice covers the redistribution and use of the
 * FPU emulator developed by W. Metzenthen. It covers only its use
 * in the 386BSD operating system. Any other use is not permitted
 * under this copyright.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must include information specifying
 *    that source code for the emulator is freely available and include
 *    either:
 *      a) an offer to provide the source code for a nominal distribution
 *         fee, or
 *      b) list at least two alternative methods whereby the source
 *         can be obtained, e.g. a publically accessible bulletin board
 *         and an anonymous ftp site from which the software can be
 *         downloaded.
 * 3. All advertising materials specifically mentioning features or use of
 *    this emulator must acknowledge that it was developed by W. Metzenthen.
 * 4. The name of W. Metzenthen may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * W. METZENTHEN BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef _EXCEPTION_H_
#define _EXCEPTION_H_


#ifdef __ASSEMBLER__
#define	Const_(x)	$##x
#else
#define	Const_(x)	x
#endif

#ifndef SW_C1
#include "fpu_emu.h"
#endif SW_C1

#define FPU_BUSY        Const_(0x8000)   /* FPU busy bit (8087 compatibility) */
#define EX_ErrorSummary Const_(0x0080)   /* Error summary status */
/* Special exceptions: */
#define	EX_INTERNAL	Const_(0x8000)	/* Internal error in wm-FPU-emu */
#define EX_StackOver	Const_(0x0041|SW_C1)	/* stack overflow */
#define EX_StackUnder	Const_(0x0041)	/* stack underflow */
/* Exception flags: */
#define EX_Precision	Const_(0x0020)	/* loss of precision */
#define EX_Underflow	Const_(0x0010)	/* underflow */
#define EX_Overflow	Const_(0x0008)	/* overflow */
#define EX_ZeroDiv	Const_(0x0004)	/* divide by zero */
#define EX_Denormal	Const_(0x0002)	/* denormalized operand */
#define EX_Invalid	Const_(0x0001)	/* invalid operation */


#ifndef __ASSEMBLER__

#ifdef DEBUG
#define	EXCEPTION(x)	{ printk("exception in %s at line %d\n", \
	__FILE__, __LINE__); exception(x); }
#else
#define	EXCEPTION(x)	exception(x)
#endif

#endif __ASSEMBLER__

#endif _EXCEPTION_H_
