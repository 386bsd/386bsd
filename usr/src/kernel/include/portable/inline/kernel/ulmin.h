/*
 * Copyright (c) 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: $
 * Unsigned long minimum function.
 */

__INLINE u_long
ulmin(u_long u1, u_long u2)
{
	return (u1 < u2 ? u1 : u2);
}
