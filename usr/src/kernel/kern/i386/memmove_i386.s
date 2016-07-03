/*
 * Copyright (c) 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: memmove_i386.s,v 1.1 94/10/19 17:40:01 bill Exp $
 * POSIX (overlapping) block memory move.
 */

#include "asm.h"
#include "assym.s"

ENTRY(memmove)
	pushl	%esi
	pushl	%edi
	movl	12(%esp),%edi
	movl	16(%esp),%esi
	movl	20(%esp),%ecx
	cmpl	%esi,%edi	/* potentially overlapping? */
	jnb	1f
	cld			/* nope, copy forwards. */
	shrl	$2,%ecx		/* copy by words */
	rep
	movsl
	movl	20(%esp),%ecx
	andl	$3,%ecx		/* any bytes left? */
	rep
	movsb
	popl	%edi
	popl	%esi
	xorl	%eax,%eax
	ret
1:
	addl	%ecx,%edi	/* copy backwards. */
	addl	%ecx,%esi
	std
	andl	$3,%ecx		/* any fractional bytes? */
	decl	%edi
	decl	%esi
	rep
	movsb
	movl	20(%esp),%ecx	/* copy remainder by words */
	shrl	$2,%ecx
	subl	$3,%esi
	subl	$3,%edi
	rep
	movsl
	popl	%edi
	popl	%esi
	xorl	%eax,%eax
	cld
	ret
