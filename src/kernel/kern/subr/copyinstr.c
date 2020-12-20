/*
 * Copyright (c) 1989, 1991, 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: copyinstr.c,v 1.1 94/10/19 18:33:13 bill Exp $
 * Portable version of function, uses copyin_().
 */

#include "sys/types.h"
#include "sys/errno.h"
#include "proc.h"
#include "prototypes.h"

int
copyinstr(struct proc *p, void *fromaddr, void *toaddr, u_int maxlength,
	u_int *lencopied) {
	int rv, c, tally;

	c = tally = 0;
	while (maxlength--) {

		/* obtain a byte if able */
		rv = copyin_(p, fromaddr++, &c, 1);
		if (rv) {
			if (lencopied)
				*lencopied = tally;
			return(rv);
		}

		/* stash in kernel */
		tally++;
		*(char *)toaddr++ = (char) c;

		/* terminating null? */
		if (c == 0) {
			if(lencopied)
				*lencopied = (u_int)tally;
			return(0);
		}
	}

	/* consumed entire buffer contents */
	if(lencopied)
		*lencopied = (u_int)tally;
	return(ENAMETOOLONG);
}
