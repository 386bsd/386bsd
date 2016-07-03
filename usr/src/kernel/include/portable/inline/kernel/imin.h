/*
 * Copyright (c) 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: $
 * Integer minimum function.
 */
__INLINE int
imin(int i1, int i2)
{
	return ((i1 < i2) ? i1 : i2);
}

