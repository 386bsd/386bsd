/*
 * Copyright (c) 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: $
 * Long minimum function.
 */
__INLINE long
lmin(long l1, long l2)
{
	return (l1 < l2 ? l1 : l2);
}
