/*
 * Copyright (c) 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: scanc_i386.s,v 1.1 94/10/19 17:40:04 bill Exp $
 * VAX scanc instruction simulation (used in UFS).
 */

#include "asm.h"

ENTRY(scanc)
	push	%esi
	push	%ebx
	movl 	12(%esp), %ecx
	movl	16(%esp), %esi
	movl	20(%esp), %ebx
	movb 	24(%esp), %ah
	cld
1:	lodsb
	xlatb
	testb	%ah, %al
	loope	1b
	je	1f
	incl	%ecx
1:	movl	%ecx, %eax
	popl	%ebx
	popl	%esi
	ret
