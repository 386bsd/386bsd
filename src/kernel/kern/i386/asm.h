/*
 * Copyright (c) 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: asm.h,v 1.1 94/10/19 17:39:54 bill Exp $
 * Assembly macros.
 */

#define	ENTRY(name) \
	.globl _/**/name; .align 4;  _/**/name:
#define	ALTENTRY(name) \
	.globl _/**/name; _/**/name:

