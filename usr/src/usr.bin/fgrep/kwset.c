/* kwset.c - search for any of a set of keywords.
   Copyright 1989 Free Software Foundation
		  Written August 1989 by Mike Haertel.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 1, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   The author may be reached (Email) at the address mike@ai.mit.edu,
   or (US mail) as Mike Haertel c/o Free Software Foundation. */

#include "std.h"

/* The algorithm implemented by these routines bears a startling resemblence
   to one discovered by Beate Commentz-Walter, although it is not identical.
   See "A String Matching Algorithm Fast on the Average," Technical Report,
   IBM-Germany, Scientific Center Heidelberg, Tiergartenstrasse 15, D-6900
   Heidelberg, Germany.  See also Aho, A.V., and M. Corasick, "Efficient
   String Matching:  An Aid to Bibliographic Search," CACM June 1975,
   Vol. 18, No. 6, which describes the failure function used below. */

#include "kwset.h"
#include "obstack.h"

#define NCHAR (UCHAR_MAX + 1)
#define obstack_chunk_alloc malloc
#define obstack_chunk_free free

/* Balanced tree of edges and labels leaving a given trie node. */
struct tree
{
  struct tree *llink;		/* Left link; MUST be first field. */
  struct tree *rlink;		/* Right link (to larger labels). */
  struct trie *trie;		/* Trie node pointed to by this edge. */
  unsigned char label;		/* Label on this edge. */
  char balance;			/* Difference in depths of subtrees. */
};

/* Node of a trie representing a set of reversed keywords. */
struct trie
{
  unsigned int accepting;	/* Word index of accepted word, or zero. */
  struct tree *links;		/* Tree of edges leaving this node. */
  struct trie *parent;		/* Parent of this node. */
  struct trie *next;		/* List of all trie nodes in level order. */
  struct trie *fail;		/* Aho-Corasick failure function. */
  int depth;			/* Depth of this node from the root. */
  int shift;			/* Shift function for search failures. */
  int maxshift;			/* Max shift of self and descendents. */
};

/* Structure returned opaquely to the caller, containing everything. */
struct kwset
{
  struct obstack obstack;	/* Obstack for node allocation. */
  int words;			/* Number of words in the trie. */
  struct trie *trie;		/* The trie itself. */
  int mind;			/* Minimum depth of an accepting node. */
  int maxd;			/* Maximum depth of any node. */
  int delta[NCHAR];		/* Delta table for rapid search. */
  struct trie *next[NCHAR];	/* Table of children of the root. */
  const char *trans;		/* Character translation table. */
};

/* Allocate and initialize a keyword set object, returning an opaque
   pointer to it.  Return NULL if memory is not available. */
kwset_t
DEFUN(kwsalloc, (trans), const char *trans)
{
  struct kwset *kwset;

  kwset = (struct kwset *) malloc(sizeof (struct kwset));
  if (!kwset)
    return NULL;

  obstack_init(&kwset->obstack);
  kwset->words = 0;
  kwset->trie
    = (struct trie *) obstack_alloc(&kwset->obstack, sizeof (struct trie));
  if (!kwset->trie)
    {
      kwsfree((kwset_t) kwset);
      return NULL;
    }
  kwset->trie->accepting = 0;
  kwset->trie->links = NULL;
  kwset->trie->parent = NULL;
  kwset->trie->next = NULL;
  kwset->trie->fail = NULL;
  kwset->trie->depth = 0;
  kwset->trie->shift = 0;
  kwset->mind = INT_MAX;
  kwset->maxd = -1;
  kwset->trans = trans;

  return (kwset_t) kwset;
}

/* Add the given string to the contents of the keyword set.  Return NULL
   for success, an error message otherwise. */
const char *
DEFUN(kwsincr, (kws, text, len),
      kwset_t kws AND const char *text AND size_t len)
{
  struct kwset *kwset;
  register struct trie *trie;
  register unsigned char label;
  register struct tree *link;
  register int depth;
  struct tree *links[12];
  enum { L, R } dirs[12];
  struct tree *t, *r, *l, *rl, *lr;

  kwset = (struct kwset *) kws;
  trie = kwset->trie;
  text += len;

  /* Descend the trie (built of reversed keywords) character-by-character,
     installing new nodes when necessary. */
  while (len--)
    {
      label = kwset->trans ? kwset->trans[(unsigned char) *--text] : *--text;

      /* Descend the tree of outgoing links for this trie node,
	 looking for the current character and keeping track
	 of the path followed. */
      link = trie->links;
      links[0] = (struct tree *) &trie->links;
      dirs[0] = L;
      depth = 1;

      while (link && label != link->label)
	{
	  links[depth] = link;
	  if (label < link->label)
	    dirs[depth++] = L, link = link->llink;
	  else
	    dirs[depth++] = R, link = link->rlink;
	}

      /* The current character doesn't have an outgoing link at
	 this trie node, so build a new trie node and install
	 a link in the current trie node's tree. */
      if (!link)
	{
	  link = (struct tree *) obstack_alloc(&kwset->obstack,
					       sizeof (struct tree));
	  if (!link)
	    return "memory exhausted";
	  link->llink = NULL;
	  link->rlink = NULL;
	  link->trie = (struct trie *) obstack_alloc(&kwset->obstack,
						     sizeof (struct trie));
	  if (!link->trie)
	    return "memory exhausted";
	  link->trie->accepting = 0;
	  link->trie->links = NULL;
	  link->trie->parent = trie;
	  link->trie->next = NULL;
	  link->trie->fail = NULL;
	  link->trie->depth = trie->depth + 1;
	  link->trie->shift = 0;
	  link->label = label;
	  link->balance = 0;

	  /* Install the new tree node in its parent. */
	  if (dirs[--depth] == L)
	    links[depth]->llink = link;
	  else
	    links[depth]->rlink = link;

	  /* Back up the tree fixing the balance flags. */
	  while (depth && !links[depth]->balance)
	    {
	      if (dirs[depth] == L)
		--links[depth]->balance;
	      else
		++links[depth]->balance;
	      --depth;
	    }

	  /* Rebalance the tree by pointer rotations if necessary. */
	  if (depth && (dirs[depth] == L && --links[depth]->balance
			|| dirs[depth] == R && ++links[depth]->balance))
	    {
	      switch (links[depth]->balance)
		{
		case (char) -2:
		  switch (dirs[depth + 1])
		    {
		    case L:
		      r = links[depth], t = r->llink, rl = t->rlink;
		      t->rlink = r, r->llink = rl;
		      t->balance = r->balance = 0;
		      break;
		    case R:
		      r = links[depth], l = r->llink, t = l->rlink;
		      rl = t->rlink, lr = t->llink;
		      t->llink = l, l->rlink = lr, t->rlink = r, r->llink = rl;
		      l->balance = t->balance != 1 ? 0 : -1;
		      r->balance = t->balance != (char) -1 ? 0 : 1;
		      t->balance = 0;
		      break;
		    }
		  break;
		case 2:
		  switch (dirs[depth + 1])
		    {
		    case R:
		      l = links[depth], t = l->rlink, lr = t->llink;
		      t->llink = l, l->rlink = lr;
		      t->balance = l->balance = 0;
		      break;
		    case L:
		      l = links[depth], r = l->rlink, t = r->llink;
		      lr = t->llink, rl = t->rlink;
		      t->llink = l, l->rlink = lr, t->rlink = r, r->llink = rl;
		      l->balance = t->balance != 1 ? 0 : -1;
		      r->balance = t->balance != (char) -1 ? 0 : 1;
		      t->balance = 0;
		      break;
		    }
		  break;
		}

	      if (dirs[depth - 1] == L)
		links[depth - 1]->llink = t;
	      else
		links[depth - 1]->rlink = t;
	    }
	}

      trie = link->trie;
    }

  /* Mark the node we finally reached as accepting, encoding the
     index number of this word in the keyword set so far. */
  if (!trie->accepting)
    trie->accepting = 1 + 2 * kwset->words;
  ++kwset->words;

  /* Keep track of the longest and shortest string of the keyword set. */
  if (trie->depth < kwset->mind)
    kwset->mind = trie->depth;
  if (trie->depth > kwset->maxd)
    kwset->maxd = trie->depth;

  return NULL;
}

/* Enqueue the trie nodes referenced from the given tree in the
   given queue. */
static void
DEFUN(enqueue, (tree, last), struct tree *tree AND struct trie **last)
{
  if (!tree)
    return;
  enqueue(tree->llink, last);
  enqueue(tree->rlink, last);
  (*last) = (*last)->next = tree->trie;
}

/* Compute the Aho-Corasick failure function for the trie nodes referenced
   from the given tree, given the failure function for their parent as
   well as a last resort failure node. */
static void
DEFUN(treefails, (tree, fail, recourse),
      register struct tree *tree
      AND struct trie *fail AND struct trie *recourse)
{
  register struct tree *link;

  if (!tree)
    return;

  treefails(tree->llink, fail, recourse);
  treefails(tree->rlink, fail, recourse);

  /* Find, in the chain of fails going back to the root, the first
     node that has a descendent on the current label. */
  while (fail)
    {
      link = fail->links;
      while (link && tree->label != link->label)
	if (tree->label < link->label)
	  link = link->llink;
	else
	  link = link->rlink;
      if (link)
	{
	  tree->trie->fail = link->trie;
	  return;
	}
      fail = fail->fail;
    }

  tree->trie->fail = recourse;
}

/* Set delta entries for the links of the given tree such that
   the preexisting delta value is larger than the current depth. */
static void
DEFUN(treedelta, (tree, depth, delta),
      register struct tree *tree AND register int depth AND int delta[])
{
  if (!tree)
    return;
  treedelta(tree->llink, depth, delta);
  treedelta(tree->rlink, depth, delta);
  if (depth < delta[tree->label])
    delta[tree->label] = depth;
}

/* Return true if A has every label in B. */
static int
DEFUN(hasevery, (a, b), register struct tree *a AND register struct tree *b)
{
  if (!b)
    return 1;
  if (!hasevery(a, b->llink))
    return 0;
  if (!hasevery(a, b->rlink))
    return 0;
  while (a && b->label != a->label)
    if (b->label < a->label)
      a = a->llink;
    else
      a = a->rlink;
  return !!a;
}

/* Compute a vector, indexed by character code, of the trie nodes
   referenced from the given tree. */
static void
DEFUN(treenext, (tree, next), struct tree *tree AND struct trie *next[])
{
  if (!tree)
    return;
  treenext(tree->llink, next);
  treenext(tree->rlink, next);
  next[tree->label] = tree->trie;
}

/* Compute the shift for each trie node, as well as the delta
   table and next cache for the given keyword set. */
const char *
DEFUN(kwsprep, (kws), kwset_t kws)
{
  register struct kwset *kwset;
  register int i;
  register struct trie *curr, *fail;
  register const char *trans;
  int delta[NCHAR];
  struct trie *last, *next[NCHAR];

  kwset = (struct kwset *) kws;

  /* Initial values for the delta table; will be changed later.  The
     delta entry for a given character is the smallest depth of any
     node at which an outgoing edge is labeled by that character. */
  for (i = 0; i < NCHAR; ++i)
    delta[i] = kwset->mind;

  /* Traverse the nodes of the trie in level order, simultaneously
     computing the delta table, failure function, and shift function. */
  for (curr = last = kwset->trie; curr; curr = curr->next)
    {
      /* Enqueue the immediate descendents in the level order queue. */
      enqueue(curr->links, &last);

      curr->shift = kwset->mind;
      curr->maxshift = kwset->mind;

      /* Update the delta table for the descendents of this node. */
      treedelta(curr->links, curr->depth, delta);

      /* Compute the failure function for the decendents of this node. */
      treefails(curr->links, curr->fail, kwset->trie);

      /* Update the shifts at each node in the current node's chain
	 of fails back to the root. */
      for (fail = curr->fail; fail; fail = fail->fail)
	{
	  /* If the current node has some outgoing edge that the fail
	     doesn't, then the shift at the fail should be no larger
	     than the difference of their depths. */
	  if (!hasevery(fail->links, curr->links))
	    if (curr->depth - fail->depth < fail->shift)
	      fail->shift = curr->depth - fail->depth;

	  /* If the current node is accepting then the shift at the
	     fail and its descendents should be no larger than the
	     difference of their depths. */
	  if (curr->accepting && fail->maxshift > curr->depth - fail->depth)
	    fail->maxshift = curr->depth - fail->depth;
	}
    }

  /* Traverse the trie in level order again, fixing up all nodes whose
     shift exceeds their inherited maxshift. */
  for (curr = kwset->trie->next; curr; curr = curr->next)
    {
      if (curr->maxshift > curr->parent->maxshift)
	curr->maxshift = curr->parent->maxshift;
      if (curr->shift > curr->maxshift)
	curr->shift = curr->maxshift;
    }

  /* Create a vector, indexed by character code, of the outgoing links
     from the root node. */
  for (i = 0; i < NCHAR; ++i)
    next[i] = NULL;
  treenext(kwset->trie->links, next);

  /* Fix things up for any translation table. */
  if (trans = kwset->trans)
    for (i = 0; i < NCHAR; ++i)
      {
	kwset->delta[i] = delta[(unsigned char) trans[i]];
	kwset->next[i] = next[(unsigned char) trans[i]];
      }
  else
    for (i = 0; i < NCHAR; ++i)
      {
	kwset->delta[i] = delta[i];
	kwset->next[i] = next[i];
      }

  return NULL;
}

/* Search through the given text for a match of any member of the
   given keyword set.  Return a pointer to the first character of
   the matching substring, or NULL if no match is found.  If FOUNDLEN
   is non-NULL store in the referenced location the length of the
   matching substring.  Similarly, if FOUNDIDX is non-NULL, store
   in the referenced location the index number of the particular
   keyword matched. */
char *
DEFUN(kwsexec, (kws, text, len, kwsmatch),
      kwset_t kws AND char *text AND size_t len AND struct kwsmatch *kwsmatch)
{
  struct kwset *kwset;
  struct trie **next, *trie, *accept;
  char *beg, *lim, *mch, *lmch;
  register unsigned char c;
  register int *delta, d;
  register char *end, *qlim;
  register struct tree *tree;
  register const char *trans;

  /* Initialize register copies and look for easy ways out. */
  kwset = (struct kwset *) kws;
  if (len < kwset->mind)
    return NULL;
  next = kwset->next;
  delta = kwset->delta;
  trans = kwset->trans;
  lim = text + len;
  end = text;
  if (d = kwset->mind)
    mch = NULL;
  else
    {
      mch = text, accept = kwset->trie;
      goto match;
    }

  if (len >= 4 * kwset->mind)
    qlim = lim - 4 * kwset->mind;
  else
    qlim = NULL;

  while (lim - end >= d)
    {
      if (qlim && end <= qlim)
	{
	  end += d - 1;
	  while ((d = delta[c = *end]) && end < qlim)
	    {
	      end += d;
	      end += delta[(unsigned char) *end];
	      end += delta[(unsigned char) *end];
	    }
	  ++end;
	}
      else
	d = delta[c = (end += d)[-1]];
      if (d)
	continue;
      beg = end - 1;
      trie = next[c];
      if (trie->accepting)
	{
	  mch = beg;
	  accept = trie;
	}
      d = trie->shift;
      while (beg > text)
	{
	  c = trans ? trans[(unsigned char) *--beg] : *--beg;
	  tree = trie->links;
	  while (tree && c != tree->label)
	    if (c < tree->label)
	      tree = tree->llink;
	    else
	      tree = tree->rlink;
	  if (tree)
	    {
	      trie = tree->trie;
	      if (trie->accepting)
		{
		  mch = beg;
		  accept = trie;
		}
	    }
	  else
	    break;
	  d = trie->shift;
	}
      if (mch)
	goto match;
    }
  return NULL;

 match:
  /* Given a known match, find the longest possible match anchored
     at or before its starting point.  This is nearly a verbatim
     copy of the preceding main search loops. */
  if (lim - mch > kwset->maxd)
    lim = mch + kwset->maxd;
  lmch = NULL;
  d = 1;
  while (lim - end >= d)
    {
      if (d = delta[c = (end += d)[-1]])
	continue;
      beg = end - 1;
      if (!(trie = next[c]))
	{
	  d = 1;
	  continue;
	}
      if (trie->accepting && beg <= mch)
	{
	  lmch = beg;
	  accept = trie;
	}
      d = trie->shift;
      while (beg > text)
	{
	  c = trans ? trans[(unsigned char) *--beg] : *--beg;
	  tree = trie->links;
	  while (tree && c != tree->label)
	    if (c < tree->label)
	      tree = tree->llink;
	    else
	      tree = tree->rlink;
	  if (tree)
	    {
	      trie = tree->trie;
	      if (trie->accepting && beg <= mch)
		{
		  lmch = beg;
		  accept = trie;
		}
	    }
	  else
	    break;
	  d = trie->shift;
	}
      if (lmch)
	{
	  mch = lmch;
	  goto match;
	}
      if (!d)
	d = 1;
    }

  if (kwsmatch)
    {
      kwsmatch->index = accept->accepting / 2;
      kwsmatch->beg[0] = mch;
      kwsmatch->size[0] = accept->depth;
    }
  return mch;
}
  
/* Free the components of the given keyword set. */
void
DEFUN(kwsfree, (kws), kwset_t kws)
{
  struct kwset *kwset;

  kwset = (struct kwset *) kws;
  obstack_free(&kwset->obstack, (PTR) NULL);
  free((PTR) kws);
}
