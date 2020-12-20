/*
 * Copyright (c) 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: ffs.h,v 1.1 94/06/09 18:19:56 bill Exp Locker: bill $
 * BSD find first set bit.
 */

__INLINE int
ffs(int bits) {
	int rv;

	asm volatile ("bsfl %1, %0 ; je 1f" :  "=r" (rv) : "m" (bits));
	return (rv + 1);

	asm ("1:");
	return (0);
}
