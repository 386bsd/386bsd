/*
 * Copyright (c) 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: skpc_i386.s,v 1.1 94/10/19 17:40:09 bill Exp $
 * VAX skpc instruction simulation.
 */

#include "asm.h"

ENTRY(skpc)
	push	%edi
	movl	16(%esp), %edi
	movl 	12(%esp), %ecx
	movb 	8(%esp), %al
	cld
	repe
	scasb
	je	1f
	incl	%ecx
1:	movl	%ecx, %eax
	popl	%edi
	ret
