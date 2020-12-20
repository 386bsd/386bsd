/*
 * Copyright (c) 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: ntohl.h,v 1.1 94/06/08 12:38:56 bill Exp Locker: bill $
 * Network to Host Long byte order conversion.
 */

__INLINE unsigned long
ntohl(unsigned long wd)
{	unsigned long rv;

#if defined(i486)
	/* note: constrain to only eax register to simplify "emulation" on 386 */
	asm ("bswap %0" : /*"=r"*/ "=a" (rv) : "0" (wd));
#else
	asm ("xchgb %b0, %h0 ; roll $16, %0 ; xchgb %b0, %h0"
		: "=q" (rv) : "0" (wd));
#endif
	return (rv);
}
