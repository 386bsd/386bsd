/*
 * Copyright (c) 1989, 1991, 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: copyoutstr.c,v 1.1 94/10/19 18:33:14 bill Exp $
 * Portable version of function, uses copyout_().
 */

#include "sys/types.h"
#include "sys/errno.h"
#include "proc.h"
#include "prototypes.h"

int
copyoutstr (struct proc *p, void *fromaddr, void *toaddr, u_int maxlength,
	u_int *lencopied) {
	int rv, c, tally = 0;

	for (c = *(char *)fromaddr; maxlength--; c = *(char *)++fromaddr) {

		/* shove byte at user process if it will accept */
		rv = copyout_(p, &c, toaddr++, 1);
		if (rv) {
			if (lencopied)
				*lencopied = tally;
			return(rv);
		}

		/* terminating null? */
		tally++;
		if (c == 0) {
			if (lencopied)
				*lencopied = tally;
			return(0);
		}
	}

	/* exceeded buffer */
	if (lencopied)
		*lencopied = tally;
	return(ENAMETOOLONG);
}
