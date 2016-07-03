/*
 * Copyright (c) 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: htons.h,v 1.1 94/06/08 12:38:45 bill Exp Locker: bill $
 * Host to Network Short byte order conversion.
 */

__INLINE unsigned short
htons(unsigned short wd)
{	unsigned short rv;

	asm ("xchgb %b0, %h0" : "=q" (rv) : "0" (wd));
	return (rv);
}
