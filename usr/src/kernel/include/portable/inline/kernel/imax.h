/*
 * Copyright (c) 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: $
 * Integer maximum function.
 */
__INLINE int
imax(int a, int b)
{
	return ((a > b) ? a : b);
}
