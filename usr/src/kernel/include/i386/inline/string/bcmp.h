/*
 * Copyright (c) 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: bcmp.h,v 1.1 94/06/09 18:19:49 bill Exp Locker: bill $
 * BSD block comparision.
 */

__INLINE int
bcmp(const void *fromaddr, void *toaddr, size_t maxlength) {
	extern const int zero;			/* compiler bug workaround */
	const void *f = fromaddr + zero;	/* compiler bug workaround */

	asm volatile ("cld ; repe ; cmpsl ; jne 1f" :
	    "=D" (toaddr), "=S" (f) :
	    "0" (toaddr), "1" (f), "c" (maxlength / 4));
	asm volatile ("repe ; cmpsb ; jne 1f" :
	    "=D" (toaddr), "=S" (f) :
	    "0" (toaddr), "1" (f), "c" (maxlength & 3));
	
	return (0);	/* exact match */

	asm ("	1:");
	return (1);	/* failed match */
}
