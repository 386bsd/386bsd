/*
 * Copyright (c) 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: strlen.h,v 1.1 94/06/09 18:20:04 bill Exp Locker: bill $
 * Bell V7 string length.
 */

__INLINE int
strlen(const char *str) {
	int rv;
	const char *strcp = str;
	char zb = 0;	/* pattern to scan for */
	int len = 0;	/* length of string (all of address space) */

	asm volatile ("cld ; repne ; scasb"
	    : "=D" (str)
	    : "0" (str), "a" (zb), "c" (len));
	return (str - strcp - 1);
}
