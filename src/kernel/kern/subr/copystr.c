/*
 * Copyright (c) 1989, 1991, 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: copystr.c,v 1.1 94/10/19 18:33:16 bill Exp $
 * Portable version of function.
 */

#include "sys/types.h"
#include "sys/errno.h"
#include "proc.h"
#include "prototypes.h"

int
copystr(void *fromaddr, void *toaddr, u_int maxlength, u_int *lencopied) {
	u_int tally;

	tally = 0;
	while (maxlength--) {

		/* copy kernel to kernel */
		*(u_char *)toaddr = *(u_char *)fromaddr++;
		tally++;

		/* terminating null? */
		if (*(u_char *)toaddr++ == 0) {
			if (lencopied)
				*lencopied = tally;
			return(0);
		}
	}

	/* buffer longer than contents */
	if (lencopied)
		*lencopied = tally;
	return(ENAMETOOLONG);
}
