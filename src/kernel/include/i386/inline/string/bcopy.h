/*
 * Copyright (c) 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: bcopy.h,v 1.1 94/06/09 18:19:53 bill Exp Locker: bill $
 * BSD block copy.
 */

__INLINE void
bcopy(const void *fromaddr, void *toaddr, size_t maxlength)
{
	extern const int zero;			/* compiler bug workaround */
	const void *f = fromaddr + zero;	/* compiler bug workaround */

	/* possibly overlapping? */
	if ((u_long) toaddr < (u_long) fromaddr) {
		asm volatile ("cld ; repe ; movsl" :
		    "=D" (toaddr), "=S" (fromaddr) :
		    "0" (toaddr), "1" (fromaddr), "c" (maxlength / 4));
		asm volatile ("repe ; movsb" :
		    "=D" (toaddr), "=S" (fromaddr) :
		    "0" (toaddr), "1" (fromaddr), "c" (maxlength & 3));
	} else {
		toaddr += maxlength - 1;
		fromaddr += maxlength - 1;
		asm volatile ("std ; repe ; movsb" :
		    "=D" (toaddr), "=S" (fromaddr) :
		    "0" (toaddr), "1" (fromaddr), "c" (maxlength & 3));
		toaddr -= 3;
		fromaddr -= 3;
		asm volatile ("repe ; movsl ; cld" :
		    "=D" (toaddr), "=S" (fromaddr) :
		    "0" (toaddr), "1" (fromaddr), "c" (maxlength / 4));
	}
}
