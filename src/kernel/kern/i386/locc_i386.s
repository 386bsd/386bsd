/*
 * Copyright (c) 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: locc_i386.s,v 1.1 94/10/19 17:39:57 bill Exp $
 * VAX locc instruction simulation (UFS).
 */

#include "asm.h"

ENTRY(locc)
	push	%edi
	movl	16(%esp), %edi
	movl 	12(%esp), %ecx
	movb 	8(%esp), %al
	cld
	repne
	scasb
	jne	1f
	incl	%ecx
1:	movl	%ecx, %eax
	popl	%edi
	ret
