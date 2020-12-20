/*
 * Copyright (c) 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: memmove.h,v 1.1 94/06/09 18:20:01 bill Exp Locker: bill $
 * POSIX (overlapping) block memory copy.
 */

__INLINE void *
memmove(void *to, const void *from, size_t len)
{
	void *rv =  to;
	extern const int zero;		/* compiler bug workaround */
	const void *f = from + zero;	/* compiler bug workaround */

	/* possibly overlapping? */
	if ((u_long) to < (u_long) f) {
		asm volatile ("cld ; repe ; movsl" :
		    "=D" (to), "=S" (f) : "0" (to), "1" (f), "c" (len / 4));

		asm volatile ("repe ; movsb" :
		    "=D" (to), "=S" (f) : "0" (to), "1" (f), "c" (len & 3));
	} else {
		to += len - 1;
		f += len - 1;
		asm volatile ("std ; repe ; movsb" :
		    "=D" (to), "=S" (f) : "0" (to), "1" (f), "c" (len & 3));

		to -= 3;
		f -= 3;
		asm volatile ("repe ; movsl" :
		    "=D" (to), "=S" (f) : "0" (to), "1" (f), "c" (len / 4));

		asm volatile ("cld");
	}

	return (rv);
}
