/*
 * Copyright (c) 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: copystr.h,v 1.1 95/01/21 16:03:49 bill Exp Locker: bill $
 * Copy kernel to kernel variable length strings from finite buffers.
 */

__INLINE int
copystr(void *from, void *to, u_int maxlength, u_int *lencopied)
{
	extern const int zero;		/* compiler bug workaround */
	/* const void *f = from + zero;	/* compiler bug workaround */
	u_int req /* = maxlength */;
	int rv;

	lencopied += zero;
	from += zero;
	maxlength += zero;
	req = maxlength;

	/* copy the string */
	asm volatile (" \
		cld ;			\
	1:	cmpb	$0, (%1) ;	\
		movsb ;			\
		loopne	1b ;		\
		jne     3f ;		"
		: "=D" (to), "=S" (from), "=c" (maxlength)
		: "0" (to), "1" (from), "2" (maxlength));

	/* set the return value, and catch the possible fault */
	asm volatile ("\
	2:	xorl %0, %0 ;		\
		jmp 5f ;		\
	3:	movl %1, %0 ;		\
		jmp 5f ;		\
	5:				"
		: "=r" (rv)
		: "i" (ENAMETOOLONG));

	/* was the return of the length found desired? */
	if (lencopied)
		*lencopied = req - maxlength;

	return (rv);
}
