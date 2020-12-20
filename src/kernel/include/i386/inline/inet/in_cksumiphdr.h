/*
 * Copyright (c) 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: in_cksumiphdr.h,v 1.1 94/06/08 12:38:48 bill Exp Locker: bill $
 * Calculate 16 bit ones compliment checksum for minimal ip header(no options)
 */

__INLINE u_short
in_cksumiphdr(void *ip) {
	u_long val, acc;

	asm ("\
		movl	  (%2), %0 ;	\
		addl	 4(%2), %0 ;	\
		adcl	 8(%2), %0 ;	\
		adcl	12(%2), %0 ;	\
		adcl	16(%2), %0 ;	\
		adcl	$0, %0 ;	\
		movl	%0, %1 ;	\
		roll	$16, %0 ;	\
		addw	%w0, %w1 ;	\
		adcw	$0, %w1 ;	"
		: "=r" (acc), "=r" (val) : "r" (ip));
	return (~val);
}

