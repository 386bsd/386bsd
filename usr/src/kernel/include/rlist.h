/*
 * Copyright (c) 1992 William Jolitz. All rights reserved.
 * Written by William Jolitz 1/92
 *
 * Redistribution and use in source and binary forms are freely permitted
 * provided that the above copyright notice and attribution and date of work
 * and this paragraph are duplicated in all such forms.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Resource lists.
 * $Id$
 */

/* A resource list element. */
struct rlist {
	unsigned	rl_start;	/* boundaries of extent - inclusive */
	unsigned	rl_end;		/* boundaries of extent - inclusive */
	struct rlist	*rl_next;	/* next list entry, if present */
};

/* Functions to manipulate resource lists.  */
void rlist_free(struct rlist **, unsigned, unsigned);
int rlist_alloc(struct rlist **, unsigned, unsigned *);
void rlist_destroy(struct rlist **);
