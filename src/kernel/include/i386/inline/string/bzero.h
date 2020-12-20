/*
 * Copyright (c) 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: bzero.h,v 1.1 94/06/09 18:19:54 bill Exp Locker: bill $
 * BSD block zero.
 */

__INLINE void
bzero(void *toaddr, size_t maxlength) {

	asm volatile ("cld ; repe ; stosl" :
	    "=D" (toaddr) :
	    "0" (toaddr), "c" (maxlength / 4), "a" (0));
	asm volatile ("  repe ; stosb " :
	    "=D" (toaddr) :
	    "0" (toaddr), "c" (maxlength & 3), "a" (0));
}
