/*
 * Copyright (c) 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: ntohs.h,v 1.1 94/06/08 12:38:57 bill Exp Locker: bill $
 * Network to Host Short byte order conversion.
 */

__INLINE unsigned short
ntohs(unsigned short wd)
{	unsigned short rv;

	asm ("xchgb %b1, %h0" : "=q" (rv) : "0" (wd));
	return (rv);
}
