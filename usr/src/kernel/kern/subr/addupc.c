/*
 * Copyright (c) 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: addupc.c,v 1.1 94/10/19 18:33:12 bill Exp $
 * Talley time ticks against appropriate counter if profiling.
 * family.
 */

#include "sys/param.h"
#include "sys/time.h"
#include "resourcevar.h"

void
addupc(int pc, struct uprof *up, int ticks)
{
	unsigned idx;

	/* is pc in range? */
	pc -= up->pr_off;
	if (pc < 0)
		return;

	/* scale pc to locate counter */
	idx = (pc >> 1) * up->pr_scale;
	idx >>= 16;

	/* is buffer long enough? */
	if (idx > up->pr_size)
		return;

	/* talley count */
	up->pr_base[idx] += ticks; 
	
fault:
	up->pr_scale = 0;
}

