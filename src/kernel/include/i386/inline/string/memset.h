/*
 * Copyright (c) 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: memset.h,v 1.1 94/06/09 18:20:02 bill Exp Locker: bill $
 * POSIX block memory fill.
 */

__INLINE void *
memset(void *toaddr, int pat, size_t maxlength) {
	void *rv = toaddr, *t = toaddr;

	/* construct pattern for possible word fill */
	if (pat) {
		pat &= 0xff;
		pat |= (pat<<8);
		pat |= (pat<<16);
	}

	/* fill by words first, then any remaining bytes */
	asm volatile ("cld ; repe ; stosl" :
	    "=D" (t) : "0" (t), "c" (maxlength / 4), "a" (pat));

	asm volatile ("repe ; stosb" :
	    "=D" (t) : "0" (t), "c" (maxlength & 3), "a" (pat));

	return(rv);
}
