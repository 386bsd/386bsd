/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)SYS.h	5.5 (Berkeley) 5/7/91
 */

#include <syscall.h>

#ifdef PROF
#define	ENTRY(x)	.globl _/**/x; \
			.data; 1:; .long 0; .text; .align 2; _/**/x: \
			movl $1b,%eax; call mcount
#else
#define	ENTRY(x)	.globl _/**/x; .text; .align 2; _/**/x: 
#endif PROF
#define	SYSCALL(x)	2: jmp cerror; ENTRY(x); lea SYS_/**/x,%eax; LCALL(7,0); jb 2b
#define	RSYSCALL(x)	SYSCALL(x); ret
#define	PSEUDO(x,y)	ENTRY(x); lea SYS_/**/y, %eax; ; LCALL(7,0); ret
#define	CALL(x,y)	call _/**/y; addl $4*x,%esp
/* gas fucks up offset -- although we don't currently need it, do for BCS */
#define	LCALL(x,y)	.byte 0x9a ; .long y; .word x

#define	ASMSTR		.asciz

	.globl	cerror
