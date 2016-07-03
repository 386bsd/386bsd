/*
 * Copyright (c) 1991, 1992, 1994 William F. Jolitz, TeleMuse
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This software is a component of "386BSD" developed by 
 *	William F. Jolitz, TeleMuse.
 * 4. Neither the name of the developer nor the name "386BSD"
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 5. Non-commercial distribution of this complete file in either source and/or
 *    binary form at no charge to the user (such as from an official Internet 
 *    archive site) is permitted. 
 * 6. Commercial distribution and sale of this complete file in either source
 *    and/or binary form on any media, including that of floppies, tape, or 
 *    CD-ROM, or through a per-charge download such as that of a BBS, is not 
 *    permitted without specific prior written permission.
 * 7. Non-commercial and/or commercial distribution of an incomplete, altered, 
 *    or otherwise modified file in either source and/or binary form is not 
 *    permitted.
 *
 * THIS SOFTWARE IS A COMPONENT OF 386BSD DEVELOPED BY WILLIAM F. JOLITZ 
 * AND IS INTENDED FOR RESEARCH AND EDUCATIONAL PURPOSES ONLY. THIS 
 * SOFTWARE SHOULD NOT BE CONSIDERED TO BE A COMMERCIAL PRODUCT. 
 * THE DEVELOPER URGES THAT USERS WHO REQUIRE A COMMERCIAL PRODUCT 
 * NOT MAKE USE OF THIS WORK.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPER ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPER BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: rlist.c,v 1.1 94/10/19 18:33:28 bill Exp $
 *
 * Resource lists.
 */

#include "sys/param.h"
#include "malloc.h"
#include "rlist.h"

/*
 * Add space to a resource list. Used to either
 * initialize a list or return free space to it.
 */
void
rlist_free (register struct rlist **rlp, unsigned start, unsigned end) {
	struct rlist *head;
	register struct rlist *olp = 0;

	head = *rlp;

loop:
	/* if nothing here, insert (tail of list) */
	if (*rlp == 0) {
		*rlp = (struct rlist *)malloc(sizeof(**rlp), M_TEMP, M_NOWAIT);
		(*rlp)->rl_start = start;
		(*rlp)->rl_end = end;
		(*rlp)->rl_next = 0;
		return;
	}

	/* if new region overlaps something currently present, panic */
	if (start >= (*rlp)->rl_start && start <= (*rlp)->rl_end)  {
		printf("Frag %d:%d, ent %d:%d ", start, end,
			(*rlp)->rl_start, (*rlp)->rl_end);
		panic("overlapping front rlist_free: freed twice?");
	}
	if (end >= (*rlp)->rl_start && end <= (*rlp)->rl_end) {
		printf("Frag %d:%d, ent %d:%d ", start, end,
			(*rlp)->rl_start, (*rlp)->rl_end);
		panic("overlapping tail rlist_free: freed twice?");
	}

	/* are we adjacent to this element? (in front) */
	if (end+1 == (*rlp)->rl_start) {
		/* coalesce */
		(*rlp)->rl_start = start;
		goto scan;
	}

	/* are we before this element? */
	if (end < (*rlp)->rl_start) {
		register struct rlist *nlp;

		nlp = (struct rlist *)malloc(sizeof(*nlp), M_TEMP, M_WAITOK);
		nlp->rl_start = start;
		nlp->rl_end = end;
		nlp->rl_next = *rlp;

		/* If the new element is in front of the list,
		 * adjust *rlp, else don't.
		 */
		if (olp) 
			olp->rl_next = nlp;
		else
			*rlp = nlp;
		return;
	}

	/* are we adjacent to this element? (at tail) */
	if ((*rlp)->rl_end + 1 == start) {
		/* coalesce */
		(*rlp)->rl_end = end;
		goto scan;
	}

	/* are we after this element */
	if (start  > (*rlp)->rl_end) {
		olp = *rlp;
		rlp = &((*rlp)->rl_next);
		goto loop;
	} else
		panic("rlist_free: can't happen");

scan:
	/* can we coalesce list now that we've filled a void? */
	{
		register struct rlist *lp, *lpn;

		for (lp = head; lp->rl_next ;) { 
			lpn = lp->rl_next;

			/* coalesce ? */
			if (lp->rl_end + 1 == lpn->rl_start) {
				lp->rl_end = lpn->rl_end;
				lp->rl_next = lpn->rl_next;
				free(lpn, M_TEMP);
			} else
				lp = lp->rl_next;
		}
	}
}

/*
 * Obtain a region of desired size from a resource list.
 * If nothing available of that size, return 0. Otherwise,
 * return a value of 1 and set resource start location with
 * "*loc". (Note: loc can be zero if we don't wish the value)
 */
int
rlist_alloc (struct rlist **rlp, unsigned size, unsigned *loc) {
	struct rlist *lp, *olp = 0;

	/* walk list, allocating first thing that's big enough (first fit) */
	for (; *rlp; rlp = &((*rlp)->rl_next))
		if(size <= (*rlp)->rl_end - (*rlp)->rl_start + 1) {

			/* hand it to the caller */
			if (loc) *loc = (*rlp)->rl_start;
			(*rlp)->rl_start += size;

			/* did we eat this element entirely? */
			if ((*rlp)->rl_start > (*rlp)->rl_end) {
				lp = (*rlp)->rl_next;
				free (*rlp, M_TEMP);
				/* If the deleted element was the front 
				 * of the list, adjust *rlp, else don't.
				 */
				if (olp) 
					olp->rl_next = lp;
				else 
					*rlp = lp;
			}

			return (1);
		} else
			olp = *rlp;

	/* nothing in list that's big enough */
	return (0);
}

/*
 * Finished with this resource list, reclaim all space and
 * mark it as being empty.
 */
void
rlist_destroy (struct rlist **rlp)
{
	struct rlist *lp, *nlp;

	lp = *rlp;
	*rlp = 0;
	for (; lp; lp = nlp) {
		nlp = lp->rl_next;
		free (lp, M_TEMP);
	}
}
