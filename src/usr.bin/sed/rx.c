/*	Copyright (C) 1992, 1993 Free Software Foundation, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this software; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* NOTE!!!  AIX requires this to be the first thing in the file.
 * Do not put ANYTHING before it!  
 */
#if !defined (__GNUC__) && defined (_AIX)
 #pragma alloca
#endif

static char rx_version_string[] = "GNU Rx version 0.03";

			/* ``Too hard!''
			 *	    -- anon.
			 */

/* N.B.:
 *
 * I think Joe Keane thought of the clever name `superstate'.
 */


#include <stdio.h>
#include <ctype.h>
#ifndef isgraph
#define isgraph(c) (isprint (c) && !isspace (c))
#endif
#ifndef isblank
#define isblank(c) ((c) == ' ' || (c) == '\t')
#endif

#include <sys/types.h>
#include <stdio.h>
#include "rx.h"

#undef MAX
#undef MIN
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

typedef char boolean;
#define false 0
#define true 1


/* This page is decls to the interesting subsystems and lower layers
 * of rx.  Everything which doesn't have a public counterpart in 
 * regex.c is declared here.
 * 
 * A useful (i hope) system is obtained by removing all or part of the regex.c
 * reimplementation and making these all extern.  I think this package
 * could be used to implement on-line lexers and parsers and who knows what 
 * else.
 */
/* In the definitions, these functions are qualified by `RX_DECL' */
#define RX_DECL static

#ifdef __STDC__

RX_DECL int rx_bitset_is_subset (int size, rx_Bitset a, rx_Bitset b);
RX_DECL void rx_bitset_null (int size, rx_Bitset b);
RX_DECL void rx_bitset_universe (int size, rx_Bitset b);
RX_DECL void rx_bitset_complement (int size, rx_Bitset b);
RX_DECL void rx_bitset_assign (int size, rx_Bitset a, rx_Bitset b);
RX_DECL void rx_bitset_union (int size, rx_Bitset a, rx_Bitset b);
RX_DECL void rx_bitset_intersection (int size,
				     rx_Bitset a, rx_Bitset b);
RX_DECL void rx_bitset_difference (int size, rx_Bitset a, rx_Bitset b);
RX_DECL unsigned long rx_bitset_hash (int size, rx_Bitset b);
RX_DECL struct rx_hash_item * rx_hash_find (struct rx_hash * table,
					    unsigned long hash,
					    void * value,
					    struct rx_hash_rules * rules);
RX_DECL struct rx_hash_item * rx_hash_store (struct rx_hash * table,
					     unsigned long hash,
					     void * value,
					     struct rx_hash_rules * rules);
RX_DECL void rx_hash_free (struct rx_hash_item * it,
			   struct rx_hash_rules * rules);
RX_DECL rx_Bitset rx_cset (struct rx *rx);
RX_DECL rx_Bitset rx_copy_cset (struct rx *rx, rx_Bitset a);
RX_DECL void rx_free_cset (struct rx * rx, rx_Bitset c);
RX_DECL struct rexp_node * rexp_node (struct rx *rx,
				      enum rexp_node_type type);
RX_DECL struct rexp_node * rx_mk_r_cset (struct rx * rx,
					 rx_Bitset b);
RX_DECL struct rexp_node * rx_mk_r_concat (struct rx * rx,
					   struct rexp_node * a,
					   struct rexp_node * b);
RX_DECL struct rexp_node * rx_mk_r_alternate (struct rx * rx,
					      struct rexp_node * a,
					      struct rexp_node * b);
RX_DECL struct rexp_node * rx_mk_r_opt (struct rx * rx,
					struct rexp_node * a);
RX_DECL struct rexp_node * rx_mk_r_star (struct rx * rx,
					 struct rexp_node * a);
RX_DECL struct rexp_node * rx_mk_r_2phase_star (struct rx * rx,
						struct rexp_node * a,
						struct rexp_node * b);
RX_DECL struct rexp_node * rx_mk_r_side_effect (struct rx * rx,
						rx_side_effect a);
RX_DECL struct rexp_node * rx_mk_r_data  (struct rx * rx,
					  void * a);
RX_DECL void rx_free_rexp (struct rx * rx, struct rexp_node * node);
RX_DECL struct rexp_node * rx_copy_rexp (struct rx *rx,
					 struct rexp_node *node);
RX_DECL struct rx_nfa_state * rx_nfa_state (struct rx *rx);
RX_DECL void rx_free_nfa_state (struct rx_nfa_state * n);
RX_DECL struct rx_nfa_state * rx_id_to_nfa_state (struct rx * rx,
						  int id);
RX_DECL struct rx_nfa_edge * rx_nfa_edge (struct rx *rx,
					  enum rx_nfa_etype type,
					  struct rx_nfa_state *start,
					  struct rx_nfa_state *dest);
RX_DECL void rx_free_nfa_edge (struct rx_nfa_edge * e);
RX_DECL void rx_free_nfa (struct rx *rx);
RX_DECL int rx_build_nfa (struct rx *rx,
			  struct rexp_node *rexp,
			  struct rx_nfa_state **start,
			  struct rx_nfa_state **end);
RX_DECL void rx_name_nfa_states (struct rx *rx);
RX_DECL int rx_eclose_nfa (struct rx *rx);
RX_DECL void rx_delete_epsilon_transitions (struct rx *rx);
RX_DECL int rx_compactify_nfa (struct rx *rx,
			       void **mem, unsigned long *size);
RX_DECL struct rx_superset * rx_superstate_eclosure_union
  (struct rx * rx, struct rx_superset *set, struct rx_nfa_state_set *ecl) ;
RX_DECL void rx_release_superset (struct rx *rx,
				  struct rx_superset *set);
RX_DECL struct rx_superstate * rx_superstate (struct rx *rx,
					      struct rx_superset *set);
RX_DECL struct rx_inx * rx_handle_cache_miss
  (struct rx *rx, struct rx_superstate *super, unsigned char chr, void *data) ;

#else /* ndef __STDC__ */
RX_DECL int rx_bitset_is_subset ();
RX_DECL void rx_bitset_null ();
RX_DECL void rx_bitset_universe ();
RX_DECL void rx_bitset_complement ();
RX_DECL void rx_bitset_assign ();
RX_DECL void rx_bitset_union ();
RX_DECL void rx_bitset_intersection ();
RX_DECL void rx_bitset_difference ();
RX_DECL unsigned long rx_bitset_hash ();
RX_DECL struct rx_hash_item * rx_hash_find ();
RX_DECL struct rx_hash_item * rx_hash_store ();
RX_DECL void rx_hash_free ();
RX_DECL rx_Bitset rx_cset ();
RX_DECL rx_Bitset rx_copy_cset ();
RX_DECL void rx_free_cset ();
RX_DECL struct rexp_node * rexp_node ();
RX_DECL struct rexp_node * rx_mk_r_cset ();
RX_DECL struct rexp_node * rx_mk_r_concat ();
RX_DECL struct rexp_node * rx_mk_r_alternate ();
RX_DECL struct rexp_node * rx_mk_r_opt ();
RX_DECL struct rexp_node * rx_mk_r_star ();
RX_DECL struct rexp_node * rx_mk_r_2phase_star ();
RX_DECL struct rexp_node * rx_mk_r_side_effect ();
RX_DECL struct rexp_node * rx_mk_r_data  ();
RX_DECL void rx_free_rexp ();
RX_DECL struct rexp_node * rx_copy_rexp ();
RX_DECL struct rx_nfa_state * rx_nfa_state ();
RX_DECL void rx_free_nfa_state ();
RX_DECL struct rx_nfa_state * rx_id_to_nfa_state ();
RX_DECL struct rx_nfa_edge * rx_nfa_edge ();
RX_DECL void rx_free_nfa_edge ();
RX_DECL void rx_free_nfa ();
RX_DECL int rx_build_nfa ();
RX_DECL void rx_name_nfa_states ();
RX_DECL int rx_eclose_nfa ();
RX_DECL void rx_delete_epsilon_transitions ();
RX_DECL int rx_compactify_nfa ();
RX_DECL struct rx_superset * rx_superstate_eclosure_union ();
RX_DECL void rx_release_superset ();
RX_DECL struct rx_superstate * rx_superstate ();
RX_DECL struct rx_inx * rx_handle_cache_miss ();
  
#endif /* ndef __STDC__ */



/* Emacs already defines alloca, sometimes.  */
#ifndef alloca

/* Make alloca work the best possible way.  */
#ifdef __GNUC__
#define alloca __builtin_alloca
#else /* not __GNUC__ */
#if HAVE_ALLOCA_H
#include <alloca.h>
#else /* not __GNUC__ or HAVE_ALLOCA_H */
#ifndef _AIX /* Already did AIX, up at the top.  */
char *alloca ();
#endif /* not _AIX */
#endif /* not HAVE_ALLOCA_H */ 
#endif /* not __GNUC__ */

#endif /* not alloca */


/* Should we use malloc or alloca?  If REGEX_MALLOC is not defined, we
 * use `alloca' instead of `malloc' for the backtracking stack.
 *
 * Emacs will die miserably if we don't do this.
 */

#ifdef REGEX_MALLOC

#define REGEX_ALLOCATE malloc

#else /* not REGEX_MALLOC  */

#define REGEX_ALLOCATE alloca

#endif /* not REGEX_MALLOC */




/* Memory management and stuff for emacs. */

#define BYTEWIDTH 8 /* In bits.  */

/* (Re)Allocate N items of type T using malloc.  */
#define TALLOC(n, t) ((t *) malloc ((n) * sizeof (t)))
#define RETALLOC(addr, n, t) ((addr) = (t *) realloc (addr, (n) * sizeof (t)))

#define remalloc(M, S) (M ? realloc (M, S) : malloc (S))

#ifdef emacs
/* The `emacs' switch turns on certain matching commands
 * that make sense only in Emacs. 
 */

#include "config.h"
#include "lisp.h"
#include "buffer.h"
#include "syntax.h"

/* Emacs uses `NULL' as a predicate.  */
#undef NULL
#else  /* not emacs */

/* Setting RX_MEMDBUG is useful if you have dbmalloc.  Maybe with similar
 * packages too.
 */
#ifdef RX_MEMDBUG
#include <malloc.h>
#else /* not RX_RX_MEMDBUG */

/* We used to test for `BSTRING' here, but only GCC and Emacs define
 * `BSTRING', as far as I know, and neither of them use this code.  
 */
#if HAVE_STRING_H || STDC_HEADERS
#include <string.h>
#ifndef bcmp
#define bcmp(s1, s2, n)	memcmp ((s1), (s2), (n))
#endif
#ifndef bcopy
#define bcopy(s, d, n)	memcpy ((d), (s), (n))
#endif
#ifndef bzero
#define bzero(s, n)	memset ((s), 0, (n))
#endif
#else
#include <strings.h>
#endif

#ifdef STDC_HEADERS
#include <stdlib.h>
#else /* not STDC_HEADERS */

char *malloc ();
char *realloc ();
#endif /* not STDC_HEADERS */

#endif /* not RX_RX_MEMDBUG */



/* Define the syntax basics for \<, \>, etc.
 * This must be nonzero for the wordchar and notwordchar pattern
 * commands in re_match_2.
 */
#ifndef Sword 
#define Sword 1
#endif

#ifdef SYNTAX_TABLE
extern char *re_syntax_table;
#else /* not SYNTAX_TABLE */

/* How many characters in the character set.  */
#define CHAR_SET_SIZE (1 << BYTEWIDTH)
static char re_syntax_table[CHAR_SET_SIZE];

#ifdef __STDC__
static void
init_syntax_once (void)
#else
static void
init_syntax_once ()
#endif
{
   register int c;
   static int done = 0;

   if (done)
     return;

   bzero (re_syntax_table, sizeof re_syntax_table);

   for (c = 'a'; c <= 'z'; c++)
     re_syntax_table[c] = Sword;

   for (c = 'A'; c <= 'Z'; c++)
     re_syntax_table[c] = Sword;

   for (c = '0'; c <= '9'; c++)
     re_syntax_table[c] = Sword;

   re_syntax_table['_'] = Sword;

   done = 1;
}
#endif /* not SYNTAX_TABLE */

#define SYNTAX(c) re_syntax_table[c]

#endif /* not emacs */


/* Compile with `-DRX_DEBUG' and use the following flags.
 *
 * Debugging flags:
 *   	rx_debug - print information as a regexp is compiled
 * 	rx_debug_trace - print information as a regexp is executed
 */

#ifdef RX_DEBUG

int rx_debug_compile = 0;
int rx_debug_trace = 0;
static struct re_pattern_buffer * dbug_rxb = 0;

#ifdef __STDC__
typedef void (*side_effect_printer) (struct rx *, void *, FILE *);
#else
typedef void (*side_effect_printer) ();
#endif

#ifdef __STDC__
static void print_cset (struct rx *rx, rx_Bitset cset, FILE * fp);
#else
static void print_cset ();
#endif

#ifdef __STDC__
static void
print_rexp (struct rx *rx,
	    struct rexp_node *node, int depth,
	    side_effect_printer seprint, FILE * fp)
#else
static void
print_rexp (rx, node, depth, seprint, fp)
     struct rx *rx;
     struct rexp_node *node;
     int depth;
     side_effect_printer seprint;
     FILE * fp;
#endif
{
  if (!node)
    return;
  else
    {
      switch (node->type)
	{
	case r_cset:
	  {
	    fprintf (fp, "%*s", depth, "");
	    print_cset (rx, node->params.cset, fp);
	    fputc ('\n', fp);
	    break;
	  }

 	case r_opt:
	case r_star:
	  fprintf (fp, "%*s%s\n", depth, "",
		   node->type == r_opt ? "opt" : "star");
	  print_rexp (rx, node->params.pair.left, depth + 3, seprint, fp);
	  break;

	case r_2phase_star:
	  fprintf (fp, "%*s2phase star\n", depth, "");
	  print_rexp (rx, node->params.pair.right, depth + 3, seprint, fp);
	  print_rexp (rx, node->params.pair.left, depth + 3, seprint, fp);
	  break;


	case r_alternate:
	case r_concat:
	  fprintf (fp, "%*s%s\n", depth, "",
		   node->type == r_alternate ? "alt" : "concat");
	  print_rexp (rx, node->params.pair.left, depth + 3, seprint, fp);
	  print_rexp (rx, node->params.pair.right, depth + 3, seprint, fp);
	  break;
	case r_side_effect:
	  fprintf (fp, "%*sSide effect: ", depth, "");
	  seprint (rx, node->params.side_effect, fp);
	  fputc ('\n', fp);
	}
    }
}


#ifdef __STDC__
static void
print_nfa (struct rx * rx,
	   struct rx_nfa_state * n,
	   side_effect_printer seprint, FILE * fp)
#else
static void
print_nfa (rx, n, seprint, fp)
     struct rx * rx;
     struct rx_nfa_state * n;
     side_effect_printer seprint;
     FILE * fp;
#endif
{
  while (n)
    {
      struct rx_nfa_edge *e = n->edges;
      struct rx_possible_future *ec = n->futures;
      fprintf (fp, "node %d %s\n", n->id,
	       n->is_final ? "final" : (n->is_start ? "start" : ""));
      while (e)
	{
	  fprintf (fp, "   edge to %d, ", e->dest->id);
	  switch (e->type)
	    {
	    case ne_epsilon:
	      fprintf (fp, "epsilon\n");
	      break;
	    case ne_side_effect:
	      fprintf (fp, "side effect ");
	      seprint (rx, e->params.side_effect, fp);
	      fputc ('\n', fp);
	      break;
	    case ne_cset:
	      fprintf (fp, "cset ");
	      print_cset (rx, e->params.cset, fp);
	      fputc ('\n', fp);
	      break;
	    }
	  e = e->next;
	}

      while (ec)
	{
	  int x;
	  struct rx_nfa_state_set * s;
	  struct rx_se_list * l;
	  fprintf (fp, "   eclosure to {");
	  for (s = ec->destset; s; s = s->cdr)
	    fprintf (fp, "%d ", s->car->id);
	  fprintf (fp, "} (");
	  for (l = ec->effects; l; l = l->cdr)
	    {
	      seprint (rx, l->car, fp);
	      fputc (' ', fp);
	    }
	  fprintf (fp, ")\n");
	  ec = ec->next;
	}
      n = n->next;
    }
}

static char * efnames [] =
{
  "bogon",
  "re_se_try",
  "re_se_pushback",
  "re_se_push0",
  "re_se_pushpos",
  "re_se_chkpos",
  "re_se_poppos",
  "re_se_at_dot",
  "re_se_syntax",
  "re_se_not_syntax",
  "re_se_begbuf",
  "re_se_hat",
  "re_se_wordbeg",
  "re_se_wordbound",
  "re_se_notwordbound",
  "re_se_wordend",
  "re_se_endbuf",
  "re_se_dollar",
  "re_se_fail",
};

static char * efnames2[] =
{
  "re_se_win"
  "re_se_lparen",
  "re_se_rparen",
  "re_se_backref",
  "re_se_iter",
  "re_se_end_iter",
  "re_se_tv"
};

static char * inx_names[] = 
{
  "rx_backtrack_point",
  "rx_do_side_effects",
  "rx_cache_miss",
  "rx_next_char",
  "rx_backtrack",
  "rx_error_inx",
  "rx_num_instructions"
};


#ifdef __STDC__
static void
re_seprint (struct rx * rx, void * effect, FILE * fp)
#else
static void
re_seprint (rx, effect, fp)
     struct rx * rx;
     void * effect;
     FILE * fp;
#endif
{
  if ((int)effect < 0)
    fputs (efnames[-(int)effect], fp);
  else if (dbug_rxb)
    {
      struct re_se_params * p = &dbug_rxb->se_params[(int)effect];
      fprintf (fp, "%s(%d,%d)", efnames2[p->se], p->op1, p->op2);
    }
  else
    fprintf (fp, "[complex op # %d]", (int)effect);
}


/* These are for so the regex.c regression tests will compile. */
void
print_compiled_pattern (rxb)
     struct re_pattern_buffer * rxb;
{
}

void
print_fastmap (fm)
     char * fm;
{
}



#endif /* RX_DEBUG */



/* This page: Bitsets.  Completely unintersting. */

#if 0
#ifdef __STDC__
RX_DECL int
rx_bitset_is_equal (int size, rx_Bitset a, rx_Bitset b)
#else
RX_DECL int
rx_bitset_is_equal (size, a, b)
     int size;
     rx_Bitset a;
     rx_Bitset b;
#endif
{
  int x;
  RX_subset s = b[0];
  b[0] = ~a[0];

  for (x = rx_bitset_numb_subsets(size) - 1; a[x] == b[x]; --x)
    ;

  b[0] = s;
  return !x && s == a[0];
}
#endif

#ifdef __STDC__
RX_DECL int
rx_bitset_is_subset (int size, rx_Bitset a, rx_Bitset b)
#else
RX_DECL int
rx_bitset_is_subset (size, a, b)
     int size;
     rx_Bitset a;
     rx_Bitset b;
#endif
{
  int x = rx_bitset_numb_subsets(size) - 1;
  while (x-- && (a[x] & b[x]) == a[x]);
  return x == -1;
}


#if 0
#ifdef __STDC__
RX_DECL int
rx_bitset_empty (int size, rx_Bitset set)
#else
RX_DECL int
rx_bitset_empty (size, set)
     int size;
     rx_Bitset set;
#endif
{
  int x;
  RX_subset s = set[0];
  set[0] = 1;
  for (x = rx_bitset_numb_subsets(size) - 1; !set[x]; --x)
    ;
  set[0] = s;
  return !s;
}
#endif

#ifdef __STDC__
RX_DECL void
rx_bitset_null (int size, rx_Bitset b)
#else
RX_DECL void
rx_bitset_null (size, b)
     int size;
     rx_Bitset b;
#endif
{
  bzero (b, rx_sizeof_bitset(size));
}


#ifdef __STDC__
RX_DECL void
rx_bitset_universe (int size, rx_Bitset b)
#else
RX_DECL void
rx_bitset_universe (size, b)
     int size;
     rx_Bitset b;
#endif
{
  int x = rx_bitset_numb_subsets (size);
  while (x--)
    *b++ = ~(RX_subset)0;
}


#ifdef __STDC__
RX_DECL void
rx_bitset_complement (int size, rx_Bitset b)
#else
RX_DECL void
rx_bitset_complement (size, b)
     int size;
     rx_Bitset b;
#endif
{
  int x = rx_bitset_numb_subsets (size);
  while (x--)
    {
      *b = ~*b;
      ++b;
    }
}


#ifdef __STDC__
RX_DECL void
rx_bitset_assign (int size, rx_Bitset a, rx_Bitset b)
#else
RX_DECL void
rx_bitset_assign (size, a, b)
     int size;
     rx_Bitset a;
     rx_Bitset b;
#endif
{
  int x;
  for (x = rx_bitset_numb_subsets(size) - 1; x >=0; --x)
    a[x] = b[x];
}


#ifdef __STDC__
RX_DECL void
rx_bitset_union (int size, rx_Bitset a, rx_Bitset b)
#else
RX_DECL void
rx_bitset_union (size, a, b)
     int size;
     rx_Bitset a;
     rx_Bitset b;
#endif
{
  int x;
  for (x = rx_bitset_numb_subsets(size) - 1; x >=0; --x)
    a[x] |= b[x];
}


#ifdef __STDC__
RX_DECL void
rx_bitset_intersection (int size,
			rx_Bitset a, rx_Bitset b)
#else
RX_DECL void
rx_bitset_intersection (size, a, b)
     int size;
     rx_Bitset a;
     rx_Bitset b;
#endif
{
  int x;
  for (x = rx_bitset_numb_subsets(size) - 1; x >=0; --x)
    a[x] &= b[x];
}


#ifdef __STDC__
RX_DECL void
rx_bitset_difference (int size, rx_Bitset a, rx_Bitset b)
#else
RX_DECL void
rx_bitset_difference (size, a, b)
     int size;
     rx_Bitset a;
     rx_Bitset b;
#endif
{
  int x;
  for (x = rx_bitset_numb_subsets(size) - 1; x >=0; --x)
    a[x] &=  ~ b[x];
}


#if 0
#ifdef __STDC__
RX_DECL void
rx_bitset_revdifference (int size,
			 rx_Bitset a, rx_Bitset b)
#else
RX_DECL void
rx_bitset_revdifference (size, a, b)
     int size;
     rx_Bitset a;
     rx_Bitset b;
#endif
{
  int x;
  for (x = rx_bitset_numb_subsets(size) - 1; x >=0; --x)
    a[x] = ~a[x] & b[x];
}

#ifdef __STDC__
RX_DECL void
rx_bitset_xor (int size, rx_Bitset a, rx_Bitset b)
#else
RX_DECL void
rx_bitset_xor (size, a, b)
     int size;
     rx_Bitset a;
     rx_Bitset b;
#endif
{
  int x;
  for (x = rx_bitset_numb_subsets(size) - 1; x >=0; --x)
    a[x] ^= b[x];
}
#endif


#ifdef __STDC__
RX_DECL unsigned long
rx_bitset_hash (int size, rx_Bitset b)
#else
RX_DECL unsigned long
rx_bitset_hash (size, b)
     int size;
     rx_Bitset b;
#endif
{
  int x;
  unsigned long hash = (unsigned long)rx_bitset_hash;

  for (x = rx_bitset_numb_subsets(size) - 1; x >= 0; --x)
    hash ^= rx_bitset_subset_val(b, x);

  return hash;
}


RX_DECL RX_subset rx_subset_singletons [RX_subset_bits] = 
{
  0x1,
  0x2,
  0x4,
  0x8,
  0x10,
  0x20,
  0x40,
  0x80,
  0x100,
  0x200,
  0x400,
  0x800,
  0x1000,
  0x2000,
  0x4000,
  0x8000,
  0x10000,
  0x20000,
  0x40000,
  0x80000,
  0x100000,
  0x200000,
  0x400000,
  0x800000,
  0x1000000,
  0x2000000,
  0x4000000,
  0x8000000,
  0x10000000,
  0x20000000,
  0x40000000,
  0x80000000
};

#ifdef RX_DEBUG

#ifdef __STDC__
static void
print_cset (struct rx *rx, rx_Bitset cset, FILE * fp)
#else
static void
print_cset (rx, cset, fp)
     struct rx *rx;
     rx_Bitset cset;
     FILE * fp;
#endif
{
  int x;
  fputc ('[', fp);
  for (x = 0; x < rx->local_cset_size; ++x)
    if (isprint(x) && RX_bitset_member (cset, x))
      fputc (x, fp);
  fputc (']', fp);
}

#endif /*  RX_DEBUG */



static unsigned long rx_hash_masks[4] =
{
  0x12488421,
  0x96699669,
  0xbe7dd7eb,
  0xffffffff
};


/* Hash tables */
#ifdef __STDC__
RX_DECL struct rx_hash_item * 
rx_hash_find (struct rx_hash * table,
	      unsigned long hash,
	      void * value,
	      struct rx_hash_rules * rules)
#else
RX_DECL struct rx_hash_item * 
rx_hash_find (table, hash, value, rules)
     struct rx_hash * table;
     unsigned long hash;
     void * value;
     struct rx_hash_rules * rules;
#endif
{
  rx_hash_eq eq = rules->eq;
  int maskc = 0;
  int mask = rx_hash_masks [0];
  int bucket = (hash & mask) % 13;

  while (table->children [bucket])
    {
      table = table->children [bucket];
      ++maskc;
      mask = rx_hash_masks[maskc];
      bucket = (hash & mask) % 13;
    }

  {
    struct rx_hash_item * it = table->buckets[bucket];
    while (it)
      if (eq (it->data, value))
	return it;
      else
	it = it->next_same_hash;
  }

  return 0;
}

#ifdef __STDC__
RX_DECL struct rx_hash_item *
rx_hash_store (struct rx_hash * table,
	       unsigned long hash,
	       void * value,
	       struct rx_hash_rules * rules)
#else
RX_DECL struct rx_hash_item *
rx_hash_store (table, hash, value, rules)
     struct rx_hash * table;
     unsigned long hash;
     void * value;
     struct rx_hash_rules * rules;
#endif
{
  rx_hash_eq eq = rules->eq;
  int maskc = 0;
  int mask = rx_hash_masks[0];
  int bucket = (hash & mask) % 13;
  int depth = 0;
  
  while (table->children [bucket])
    {
      table = table->children [bucket];
      ++maskc;
      mask = rx_hash_masks[maskc];
      bucket = (hash & mask) % 13;
      ++depth;
    }
  
  {
    struct rx_hash_item * it = table->buckets[bucket];
    while (it)
      if (eq (it->data, value))
	return it;
      else
	it = it->next_same_hash;
  }
  
  {
    if (   (depth < 3)
	&& (table->bucket_size [bucket] >= 4))
      {
	struct rx_hash * newtab = ((struct rx_hash *)
				   rules->hash_alloc (rules));
	if (!newtab)
	  goto add_to_bucket;
	bzero (newtab, sizeof (*newtab));
	newtab->parent = table;
	{
	  struct rx_hash_item * them = table->buckets[bucket];
	  unsigned long newmask = rx_hash_masks[maskc + 1];
	  while (them)
	    {
	      struct rx_hash_item * save = them->next_same_hash;
	      int new_buck = (them->hash & newmask) % 13;
	      them->next_same_hash = newtab->buckets[new_buck];
	      newtab->buckets[new_buck] = them;
	      them->table = newtab;
	      them = save;
	      ++newtab->bucket_size[new_buck];
	      ++newtab->refs;
	    }
	  table->refs = (table->refs - table->bucket_size[bucket] + 1);
	  table->bucket_size[bucket] = 0;
	  table->buckets[bucket] = 0;
	  table->children[bucket] = newtab;
	  table = newtab;
	  bucket = (hash & newmask) % 13;
	}
      }
  }
 add_to_bucket:
  {
    struct rx_hash_item  * it = ((struct rx_hash_item *)
				 rules->hash_item_alloc (rules, value));
    if (!it)
      return 0;
    it->hash = hash;
    it->table = table;
    /* DATA and BINDING are to be set in hash_item_alloc */
    it->next_same_hash = table->buckets [bucket];
    table->buckets[bucket] = it;
    ++table->bucket_size [bucket];
    ++table->refs;
    return it;
  }
}

#ifdef __STDC__
RX_DECL void
rx_hash_free (struct rx_hash_item * it, struct rx_hash_rules * rules)
#else
RX_DECL void
rx_hash_free (it, rules)
     struct rx_hash_item * it;
     struct rx_hash_rules * rules;
#endif
{
  if (it)
    {
      struct rx_hash * table = it->table;
      unsigned long hash = it->hash;
      int depth = (table->parent
		   ? (table->parent->parent
		      ? (table->parent->parent->parent
			 ? 3
			 : 2)
		      : 1)
		   : 0);
      int bucket = (hash & rx_hash_masks [depth]) % 13;
      struct rx_hash_item ** pos = &table->buckets [bucket];
      
      while (*pos != it)
	pos = &(*pos)->next_same_hash;
      *pos = it->next_same_hash;
      rules->free_hash_item (it, rules);
      --table->bucket_size[bucket];
      --table->refs;
      while (!table->refs && depth)
	{
	  struct rx_hash * save = table;
	  table = table->parent;
	  --depth;
	  bucket = (hash & rx_hash_masks [depth]) % 13;
	  --table->refs;
	  table->children[bucket] = 0;
	  rules->free_hash (save, rules);
	}
    }
}

#ifdef __STDC__
typedef void (*rx_hash_freefn) (struct rx_hash_item * it);
#else /* ndef __STDC__ */
typedef void (*rx_hash_freefn) ();
#endif /* ndef __STDC__ */

#ifdef __STDC__
RX_DECL void
rx_free_hash_table (struct rx_hash * tab, rx_hash_freefn freefn,
		    struct rx_hash_rules * rules)
#else
RX_DECL void
rx_free_hash_table (tab, freefn, rules)
     struct rx_hash * tab;
     rx_hash_freefn freefn;
     struct rx_hash_rules * rules;
#endif
{
  int x;

  for (x = 0; x < 13; ++x)
    if (tab->children[x])
      {
	rx_free_hash_table (tab->children[x], freefn, rules);
	rules->free_hash (tab->children[x], rules);
      }
    else
      {
	struct rx_hash_item * them = tab->buckets[x];
	while (them)
	  {
	    struct rx_hash_item * that = them;
	    them = that->next_same_hash;
	    freefn (that);
	    rules->free_hash_item (that, rules);
	  }
      }
}



/* Utilities for manipulating bitset represntations of characters sets. */

#ifdef __STDC__
RX_DECL rx_Bitset
rx_cset (struct rx *rx)
#else
RX_DECL rx_Bitset
rx_cset (rx)
     struct rx *rx;
#endif
{
  rx_Bitset b = (rx_Bitset) malloc (rx_sizeof_bitset (rx->local_cset_size));
  if (b)
    rx_bitset_null (rx->local_cset_size, b);
  return b;
}


#ifdef __STDC__
RX_DECL rx_Bitset
rx_copy_cset (struct rx *rx, rx_Bitset a)
#else
RX_DECL rx_Bitset
rx_copy_cset (rx, a)
     struct rx *rx;
     rx_Bitset a;
#endif
{
  rx_Bitset cs = rx_cset (rx);

  if (cs)
    rx_bitset_union (rx->local_cset_size, cs, a);

  return cs;
}


#ifdef __STDC__
RX_DECL void
rx_free_cset (struct rx * rx, rx_Bitset c)
#else
RX_DECL void
rx_free_cset (rx, c)
     struct rx * rx;
     rx_Bitset c;
#endif
{
  if (c)
    free ((char *)c);
}


/* Hash table memory allocation policy for the regexp compiler */

#ifdef __STDC__
struct rx_hash *
compiler_hash_alloc (struct rx_hash_rules * rules)
#else
struct rx_hash *
compiler_hash_alloc (rules)
     struct rx_hash_rules * rules;
#endif
{
  return (struct rx_hash *)malloc (sizeof (struct rx_hash));
}

#ifdef __STDC__
struct rx_hash_item *
compiler_hash_item_alloc (struct rx_hash_rules * rules, void * value)
#else
struct rx_hash_item *
compiler_hash_item_alloc (rules, value)
     struct rx_hash_rules * rules;
     void * value;
#endif
{
  struct rx_hash_item * it;
  it = (struct rx_hash_item *)malloc (sizeof (*it));
  if (it)
    {
      it->data = value;
      it->binding = 0;
    }
  return it;
}

#ifdef __STDC__
void
compiler_free_hash (struct rx_hash * tab,
		    struct rx_hash_rules * rules)
#else
void
compiler_free_hash (tab, rules)
     struct rx_hash * tab;
     struct rx_hash_rules * rules;
#endif
{
  free ((char *)tab);
}

#ifdef __STDC__
void
compiler_free_hash_item (struct rx_hash_item * item,
			 struct rx_hash_rules * rules)
#else
void
compiler_free_hash_item (item, rules)
     struct rx_hash_item * item;
     struct rx_hash_rules * rules;
#endif
{
  free ((char *)item);
}


/* This page: REXP_NODE (expression tree) structures. */

#ifdef __STDC__
RX_DECL struct rexp_node *
rexp_node (struct rx *rx,
	   enum rexp_node_type type)
#else
RX_DECL struct rexp_node *
rexp_node (rx, type)
     struct rx *rx;
     enum rexp_node_type type;
#endif
{
  struct rexp_node *n;

  n = (struct rexp_node *)malloc (sizeof (*n));
  bzero (n, sizeof (*n));
  if (n)
    n->type = type;
  return n;
}


/* free_rexp_node assumes that the bitset passed to rx_mk_r_cset
 * can be freed using rx_free_cset.
 */
#ifdef __STDC__
RX_DECL struct rexp_node *
rx_mk_r_cset (struct rx * rx,
	      rx_Bitset b)
#else
RX_DECL struct rexp_node *
rx_mk_r_cset (rx, b)
     struct rx * rx;
     rx_Bitset b;
#endif
{
  struct rexp_node * n = rexp_node (rx, r_cset);
  if (n)
    n->params.cset = b;
  return n;
}


#ifdef __STDC__
RX_DECL struct rexp_node *
rx_mk_r_concat (struct rx * rx,
		struct rexp_node * a,
		struct rexp_node * b)
#else
RX_DECL struct rexp_node *
rx_mk_r_concat (rx, a, b)
     struct rx * rx;
     struct rexp_node * a;
     struct rexp_node * b;
#endif
{
  struct rexp_node * n = rexp_node (rx, r_concat);
  if (n)
    {
      n->params.pair.left = a;
      n->params.pair.right = b;
    }
  return n;
}


#ifdef __STDC__
RX_DECL struct rexp_node *
rx_mk_r_alternate (struct rx * rx,
		   struct rexp_node * a,
		   struct rexp_node * b)
#else
RX_DECL struct rexp_node *
rx_mk_r_alternate (rx, a, b)
     struct rx * rx;
     struct rexp_node * a;
     struct rexp_node * b;
#endif
{
  struct rexp_node * n = rexp_node (rx, r_alternate);
  if (n)
    {
      n->params.pair.left = a;
      n->params.pair.right = b;
    }
  return n;
}


#ifdef __STDC__
RX_DECL struct rexp_node *
rx_mk_r_opt (struct rx * rx,
	     struct rexp_node * a)
#else
RX_DECL struct rexp_node *
rx_mk_r_opt (rx, a)
     struct rx * rx;
     struct rexp_node * a;
#endif
{
  struct rexp_node * n = rexp_node (rx, r_opt);
  if (n)
    {
      n->params.pair.left = a;
      n->params.pair.right = 0;
    }
  return n;
}


#ifdef __STDC__
RX_DECL struct rexp_node *
rx_mk_r_star (struct rx * rx,
	      struct rexp_node * a)
#else
RX_DECL struct rexp_node *
rx_mk_r_star (rx, a)
     struct rx * rx;
     struct rexp_node * a;
#endif
{
  struct rexp_node * n = rexp_node (rx, r_star);
  if (n)
    {
      n->params.pair.left = a;
      n->params.pair.right = 0;
    }
  return n;
}


#ifdef __STDC__
RX_DECL struct rexp_node *
rx_mk_r_2phase_star (struct rx * rx,
		     struct rexp_node * a,
		     struct rexp_node * b)
#else
RX_DECL struct rexp_node *
rx_mk_r_2phase_star (rx, a, b)
     struct rx * rx;
     struct rexp_node * a;
     struct rexp_node * b;
#endif
{
  struct rexp_node * n = rexp_node (rx, r_2phase_star);
  if (n)
    {
      n->params.pair.left = a;
      n->params.pair.right = b;
    }
  return n;
}



#ifdef __STDC__
RX_DECL struct rexp_node *
rx_mk_r_side_effect (struct rx * rx,
		     rx_side_effect a)
#else
RX_DECL struct rexp_node *
rx_mk_r_side_effect (rx, a)
     struct rx * rx;
     rx_side_effect a;
#endif
{
  struct rexp_node * n = rexp_node (rx, r_side_effect);
  if (n)
    {
      n->params.side_effect = a;
      n->params.pair.right = 0;
    }
  return n;
}


#ifdef __STDC__
RX_DECL struct rexp_node *
rx_mk_r_data  (struct rx * rx,
	       void * a)
#else
RX_DECL struct rexp_node *
rx_mk_r_data  (rx, a)
     struct rx * rx;
     void * a;
#endif
{
  struct rexp_node * n = rexp_node (rx, r_data);
  if (n)
    {
      n->params.pair.left = a;
      n->params.pair.right = 0;
    }
  return n;
}


#ifdef __STDC__
RX_DECL void
rx_free_rexp (struct rx * rx, struct rexp_node * node)
#else
RX_DECL void
rx_free_rexp (rx, node)
     struct rx * rx;
     struct rexp_node * node;
#endif
{
  if (node)
    {
      switch (node->type)
	{
	case r_cset:
	  if (node->params.cset)
	    rx_free_cset (rx, node->params.cset);

	case r_side_effect:
	  break;
	  
	case r_concat:
	case r_alternate:
	case r_2phase_star:
	case r_opt:
	case r_star:
	  rx_free_rexp (rx, node->params.pair.left);
	  rx_free_rexp (rx, node->params.pair.right);
	  break;

	case r_data:
	  /* This shouldn't occur. */
	  break;
	}
      free ((char *)node);
    }
}


#ifdef __STDC__
RX_DECL struct rexp_node * 
rx_copy_rexp (struct rx *rx,
	   struct rexp_node *node)
#else
RX_DECL struct rexp_node * 
rx_copy_rexp (rx, node)
     struct rx *rx;
     struct rexp_node *node;
#endif
{
  if (!node)
    return 0;
  else
    {
      struct rexp_node *n = rexp_node (rx, node->type);
      if (!n)
	return 0;
      switch (node->type)
	{
	case r_cset:
	  n->params.cset = rx_copy_cset (rx, node->params.cset);
	  if (!n->params.cset)
	    {
	      rx_free_rexp (rx, n);
	      return 0;
	    }
	  break;

	case r_side_effect:
	  n->params.side_effect = node->params.side_effect;
	  break;

	case r_concat:
	case r_alternate:
	case r_opt:
	case r_2phase_star:
	case r_star:
	  n->params.pair.left =
	    rx_copy_rexp (rx, node->params.pair.left);
	  n->params.pair.right =
	    rx_copy_rexp (rx, node->params.pair.right);
	  if (   (node->params.pair.left && !n->params.pair.left)
	      || (node->params.pair.right && !n->params.pair.right))
	    {
	      rx_free_rexp  (rx, n);
	      return 0;
	    }
	  break;
	case r_data:
	  /* shouldn't happen */
	  break;
	}
      return n;
    }
}



/* This page: functions to build and destroy graphs that describe nfa's */

/* Constructs a new nfa node. */
#ifdef __STDC__
RX_DECL struct rx_nfa_state *
rx_nfa_state (struct rx *rx)
#else
RX_DECL struct rx_nfa_state *
rx_nfa_state (rx)
     struct rx *rx;
#endif
{
  struct rx_nfa_state * n = (struct rx_nfa_state *)malloc (sizeof (*n));
  if (!n)
    return 0;
  bzero (n, sizeof (*n));
  n->next = rx->nfa_states;
  rx->nfa_states = n;
  return n;
}


#ifdef __STDC__
RX_DECL void
rx_free_nfa_state (struct rx_nfa_state * n)
#else
RX_DECL void
rx_free_nfa_state (n)
  struct rx_nfa_state * n;
#endif
{
  free ((char *)n);
}


/* This looks up an nfa node, given a numeric id.  Numeric id's are
 * assigned after the nfa has been built.
 */
#ifdef __STDC__
RX_DECL struct rx_nfa_state * 
rx_id_to_nfa_state (struct rx * rx,
		    int id)
#else
RX_DECL struct rx_nfa_state * 
rx_id_to_nfa_state (rx, id)
     struct rx * rx;
     int id;
#endif
{
  struct rx_nfa_state * n;
  for (n = rx->nfa_states; n; n = n->next)
    if (n->id == id)
      return n;
  return 0;
}


/* This adds an edge between two nodes, but doesn't initialize the 
 * edge label.
 */

#ifdef __STDC__
RX_DECL struct rx_nfa_edge * 
rx_nfa_edge (struct rx *rx,
	     enum rx_nfa_etype type,
	     struct rx_nfa_state *start,
	     struct rx_nfa_state *dest)
#else
RX_DECL struct rx_nfa_edge * 
rx_nfa_edge (rx, type, start, dest)
     struct rx *rx;
     enum rx_nfa_etype type;
     struct rx_nfa_state *start;
     struct rx_nfa_state *dest;
#endif
{
  struct rx_nfa_edge *e;
  e = (struct rx_nfa_edge *)malloc (sizeof (*e));
  if (!e)
    return 0;
  e->next = start->edges;
  start->edges = e;
  e->type = type;
  e->dest = dest;
  return e;
}


#ifdef __STDC__
RX_DECL void
rx_free_nfa_edge (struct rx_nfa_edge * e)
#else
RX_DECL void
rx_free_nfa_edge (e)
     struct rx_nfa_edge * e;
#endif
{
  free ((char *)e);
}


/* This constructs a POSSIBLE_FUTURE, which is a kind epsilon-closure
 * of an NFA.  These are added to an nfa automaticly by eclose_nfa.
 */  

#ifdef __STDC__
static struct rx_possible_future * 
rx_possible_future (struct rx * rx,
		 struct rx_se_list * effects)
#else
static struct rx_possible_future * 
rx_possible_future (rx, effects)
     struct rx * rx;
     struct rx_se_list * effects;
#endif
{
  struct rx_possible_future *ec;
  ec = (struct rx_possible_future *) malloc (sizeof (*ec));
  if (!ec)
    return 0;
  ec->destset = 0;
  ec->next = 0;
  ec->effects = effects;
  return ec;
}


#ifdef __STDC__
static void
rx_free_possible_future (struct rx_possible_future * pf)
#else
static void
rx_free_possible_future (pf)
     struct rx_possible_future * pf;
#endif
{
  free ((char *)pf);
}


#ifdef __STDC__
RX_DECL void
rx_free_nfa (struct rx *rx)
#else
RX_DECL void
rx_free_nfa (rx)
     struct rx *rx;
#endif
{
  while (rx->nfa_states)
    {
      while (rx->nfa_states->edges)
	{
	  switch (rx->nfa_states->edges->type)
	    {
	    case ne_cset:
	      rx_free_cset (rx, rx->nfa_states->edges->params.cset);
	      break;
	    default:
	      break;
	    }
	  {
	    struct rx_nfa_edge * e;
	    e = rx->nfa_states->edges;
	    rx->nfa_states->edges = rx->nfa_states->edges->next;
	    rx_free_nfa_edge (e);
	  }
	} /* while (rx->nfa_states->edges) */
      {
	/* Iterate over the partial epsilon closures of rx->nfa_states */
	struct rx_possible_future * pf = rx->nfa_states->futures;
	while (pf)
	  {
	    struct rx_possible_future * pft = pf;
	    pf = pf->next;
	    rx_free_possible_future (pft);
	  }
      }
      {
	struct rx_nfa_state *n;
	n = rx->nfa_states;
	rx->nfa_states = rx->nfa_states->next;
	rx_free_nfa_state (n);
      }
    }
}



/* This page: translating a pattern expression in to an nfa and doing the 
 * static part of the nfa->super-nfa translation.
 */

/* This is the thompson regexp->nfa algorithm. */
#ifdef __STDC__
RX_DECL int
rx_build_nfa (struct rx *rx,
	      struct rexp_node *rexp,
	      struct rx_nfa_state **start,
	      struct rx_nfa_state **end)
#else
RX_DECL int
rx_build_nfa (rx, rexp, start, end)
     struct rx *rx;
     struct rexp_node *rexp;
     struct rx_nfa_state **start;
     struct rx_nfa_state **end;
#endif
{
  struct rx_nfa_edge *edge;

  /* Start & end nodes may have been allocated by the caller. */
  *start = *start ? *start : rx_nfa_state (rx);

  if (!*start)
    return 0;

  if (!rexp)
    {
      *end = *start;
      return 1;
    }

  *end = *end ? *end : rx_nfa_state (rx);

  if (!*end)
    {
      rx_free_nfa_state (*start);
      return 0;
    }

  switch (rexp->type)
    {
    case r_data:
      return 0;

    case r_cset:
      edge = rx_nfa_edge (rx, ne_cset, *start, *end);
      if (!edge)
	return 0;
      edge->params.cset = rx_copy_cset (rx, rexp->params.cset);
      if (!edge->params.cset)
	{
	  rx_free_nfa_edge (edge);
	  return 0;
	}
      return 1;
 
    case r_opt:
      return (rx_build_nfa (rx, rexp->params.pair.left, start, end)
	      && rx_nfa_edge (rx, ne_epsilon, *start, *end));

    case r_star:
      {
	struct rx_nfa_state * star_start = 0;
	struct rx_nfa_state * star_end = 0;
	return (rx_build_nfa (rx, rexp->params.pair.left,
			      &star_start, &star_end)
		&& star_start
		&& star_end
		&& rx_nfa_edge (rx, ne_epsilon, star_start, star_end)
		&& rx_nfa_edge (rx, ne_epsilon, *start, star_start)
		&& rx_nfa_edge (rx, ne_epsilon, star_end, *end)

		&& rx_nfa_edge (rx, ne_epsilon, star_end, star_start));
      }

    case r_2phase_star:
      {
	struct rx_nfa_state * star_start = 0;
	struct rx_nfa_state * star_end = 0;
	struct rx_nfa_state * loop_exp_start = 0;
	struct rx_nfa_state * loop_exp_end = 0;

	return (rx_build_nfa (rx, rexp->params.pair.left,
			      &star_start, &star_end)
		&& rx_build_nfa (rx, rexp->params.pair.right,
				 &loop_exp_start, &loop_exp_end)
		&& star_start
		&& star_end
		&& loop_exp_end
		&& loop_exp_start
		&& rx_nfa_edge (rx, ne_epsilon, star_start, *end)
		&& rx_nfa_edge (rx, ne_epsilon, *start, star_start)
		&& rx_nfa_edge (rx, ne_epsilon, star_end, *end)

		&& rx_nfa_edge (rx, ne_epsilon, star_end, loop_exp_start)
		&& rx_nfa_edge (rx, ne_epsilon, loop_exp_end, star_start));
      }


    case r_concat:
      {
	struct rx_nfa_state *shared = 0;
	return
	  (rx_build_nfa (rx, rexp->params.pair.left, start, &shared)
	   && rx_build_nfa (rx, rexp->params.pair.right, &shared, end));
      }

    case r_alternate:
      {
	struct rx_nfa_state *ls = 0;
	struct rx_nfa_state *le = 0;
	struct rx_nfa_state *rs = 0;
	struct rx_nfa_state *re = 0;
	return (rx_build_nfa (rx, rexp->params.pair.left, &ls, &le)
		&& rx_build_nfa (rx, rexp->params.pair.right, &rs, &re)
		&& rx_nfa_edge (rx, ne_epsilon, *start, ls)
		&& rx_nfa_edge (rx, ne_epsilon, *start, rs)
		&& rx_nfa_edge (rx, ne_epsilon, le, *end)
		&& rx_nfa_edge (rx, ne_epsilon, re, *end));
      }

    case r_side_effect:
      edge = rx_nfa_edge (rx, ne_side_effect, *start, *end);
      if (!edge)
	return 0;
      edge->params.side_effect = rexp->params.side_effect;
      return 1;
    }

  /* this should never happen */
  return 0;
}


/* NAME_RX->NFA_STATES identifies all nodes with non-epsilon transitions.
 * These nodes can occur in super-states.  All nodes are given an integer id.
 * The id is non-negative if the node has non-epsilon out-transitions, negative
 * otherwise (this is because we want the non-negative ids to be used as 
 * array indexes in a few places).
 */

#ifdef __STDC__
RX_DECL void
rx_name_nfa_states (struct rx *rx)
#else
RX_DECL void
rx_name_nfa_states (rx)
     struct rx *rx;
#endif
{
  struct rx_nfa_state *n = rx->nfa_states;

  rx->nodec = 0;
  rx->epsnodec = -1;

  while (n)
    {
      struct rx_nfa_edge *e = n->edges;

      if (n->is_start)
	n->eclosure_needed = 1;

      while (e)
	{
	  switch (e->type)
	    {
	    case ne_epsilon:
	    case ne_side_effect:
	      break;

	    case ne_cset:
	      n->id = rx->nodec++;
	      {
		struct rx_nfa_edge *from_n = n->edges;
		while (from_n)
		  {
		    from_n->dest->eclosure_needed = 1;
		    from_n = from_n->next;
		  }
	      }
	      goto cont;
	    }
	  e = e->next;
	}
      n->id = rx->epsnodec--;
    cont:
      n = n->next;
    }
  rx->epsnodec = -rx->epsnodec;
}


/* This page: data structures for the static part of the nfa->supernfa
 * translation.
 */

/* The next several functions compare, construct, etc. lists of side
 * effects.  See ECLOSE_NFA (below) for details.
 */

/* Ordering of rx_se_list
 * (-1, 0, 1 return value convention).
 */

#ifdef __STDC__
static int 
se_list_cmp (void * va, void * vb)
#else
static int 
se_list_cmp (va, vb)
     void * va;
     void * vb;
#endif
{
  struct rx_se_list * a = (struct rx_se_list *)va;
  struct rx_se_list * b = (struct rx_se_list *)vb;

  return ((va == vb)
	  ? 0
	  : (!va
	     ? -1
	     : (!vb
		? 1
		: ((long)a->car < (long)b->car
		   ? 1
		   : ((long)a->car > (long)b->car
		      ? -1
		      : se_list_cmp ((void *)a->cdr, (void *)b->cdr))))));
}


#ifdef __STDC__
static int 
se_list_equal (void * va, void * vb)
#else
static int 
se_list_equal (va, vb)
     void * va;
     void * vb;
#endif
{
  return !(se_list_cmp (va, vb));
}

static struct rx_hash_rules se_list_hash_rules =
{
  se_list_equal,
  compiler_hash_alloc,
  compiler_free_hash,
  compiler_hash_item_alloc,
  compiler_free_hash_item
};


#ifdef __STDC__
static struct rx_se_list * 
side_effect_cons (struct rx * rx,
		  void * se, struct rx_se_list * list)
#else
static struct rx_se_list * 
side_effect_cons (rx, se, list)
     struct rx * rx;
     void * se;
     struct rx_se_list * list;
#endif
{
  struct rx_se_list * l;
  l = ((struct rx_se_list *) malloc (sizeof (*l)));
  if (!l)
    return 0;
  l->car = se;
  l->cdr = list;
  return l;
}


#ifdef __STDC__
static struct rx_se_list *
hash_cons_se_prog (struct rx * rx,
		   struct rx_hash * memo,
		   void * car, struct rx_se_list * cdr)
#else
static struct rx_se_list *
hash_cons_se_prog (rx, memo, car, cdr)
     struct rx * rx;
     struct rx_hash * memo;
     void * car;
     struct rx_se_list * cdr;
#endif
{
  long hash = (long)car ^ (long)cdr;
  struct rx_se_list template;

  template.car = car;
  template.cdr = cdr;
  {
    struct rx_hash_item * it = rx_hash_store (memo, hash,
					      (void *)&template,
					      &se_list_hash_rules);
    if (!it)
      return 0;
    if (it->data == (void *)&template)
      {
	struct rx_se_list * consed;
	consed = (struct rx_se_list *) malloc (sizeof (*consed));
	*consed = template;
	it->data = (void *)consed;
      }
    return (struct rx_se_list *)it->data;
  }
}
     

#ifdef __STDC__
static struct rx_se_list *
hash_se_prog (struct rx * rx, struct rx_hash * memo, struct rx_se_list * prog)
#else
static struct rx_se_list *
hash_se_prog (rx, memo, prog)
     struct rx * rx;
     struct rx_hash * memo;
     struct rx_se_list * prog;
#endif
{
  struct rx_se_list * answer = 0;
  while (prog)
    {
      answer = hash_cons_se_prog (rx, memo, prog->car, answer);
      if (!answer)
	return 0;
      prog = prog->cdr;
    }
  return answer;
}


/* This page: more data structures for nfa->supernfa.  Specificly,
 * sets of nfa states.
 */

#ifdef __STDC__
static int 
nfa_set_cmp (void * va, void * vb)
#else
static int 
nfa_set_cmp (va, vb)
     void * va;
     void * vb;
#endif
{
  struct rx_nfa_state_set * a = (struct rx_nfa_state_set *)va;
  struct rx_nfa_state_set * b = (struct rx_nfa_state_set *)vb;

  return ((va == vb)
	  ? 0
	  : (!va
	     ? -1
	     : (!vb
		? 1
		: (a->car->id < b->car->id
		   ? 1
		   : (a->car->id > b->car->id
		      ? -1
		      : nfa_set_cmp ((void *)a->cdr, (void *)b->cdr))))));
}

#ifdef __STDC__
static int 
nfa_set_equal (void * va, void * vb)
#else
static int 
nfa_set_equal (va, vb)
     void * va;
     void * vb;
#endif
{
  return !nfa_set_cmp (va, vb);
}

static struct rx_hash_rules nfa_set_hash_rules =
{
  nfa_set_equal,
  compiler_hash_alloc,
  compiler_free_hash,
  compiler_hash_item_alloc,
  compiler_free_hash_item
};


/* CONS -- again, sets with == elements are ==. */

#ifdef __STDC__
static struct rx_nfa_state_set * 
nfa_set_cons (struct rx * rx,
	      struct rx_hash * memo, struct rx_nfa_state * state,
	      struct rx_nfa_state_set * set)
#else
static struct rx_nfa_state_set * 
nfa_set_cons (rx, memo, state, set)
     struct rx * rx;
     struct rx_hash * memo;
     struct rx_nfa_state * state;
     struct rx_nfa_state_set * set;
#endif
{
  struct rx_nfa_state_set template;
  struct rx_hash_item * node;
  template.car = state;
  template.cdr = set;
  node = rx_hash_store (memo,
			(((long)state) >> 8) ^ (long)set,
			&template, &nfa_set_hash_rules);
  if (!node)
    return 0;
  if (node->data == &template)
    {
      struct rx_nfa_state_set * l;
      l = (struct rx_nfa_state_set *) malloc (sizeof (*l));
      node->data = (void *) l;
      if (!l)
	return 0;
      *l = template;
    }
  return (struct rx_nfa_state_set *)node->data;
}


#ifdef __STDC__
static struct rx_nfa_state_set * 
nfa_set_enjoin (struct rx * rx,
		struct rx_hash * memo, struct rx_nfa_state * state,
		struct rx_nfa_state_set * set)
#else
static struct rx_nfa_state_set * 
nfa_set_enjoin (rx, memo, state, set)
     struct rx * rx;
     struct rx_hash * memo;
     struct rx_nfa_state * state;
     struct rx_nfa_state_set * set;
#endif
{
  if (!set || state->id < set->car->id)
    return nfa_set_cons (rx, memo, state, set);
  if (state->id == set->car->id)
    return set;
  else
    {
      struct rx_nfa_state_set * newcdr
	= nfa_set_enjoin (rx, memo, state, set->cdr);
      if (newcdr != set->cdr)
	set = nfa_set_cons (rx, memo, set->car, newcdr);
      return set;
    }
}



/* This page: computing epsilon closures.  The closures aren't total.
 * Each node's closures are partitioned according to the side effects entailed
 * along the epsilon edges.  Return true on success.
 */ 

struct eclose_frame
{
  struct rx_se_list *prog_backwards;
};


#ifdef __STDC__
static int 
eclose_node (struct rx *rx, struct rx_nfa_state *outnode,
	     struct rx_nfa_state *node, struct eclose_frame *frame)
#else
static int 
eclose_node (rx, outnode, node, frame)
     struct rx *rx;
     struct rx_nfa_state *outnode;
     struct rx_nfa_state *node;
     struct eclose_frame *frame;
#endif
{
  struct rx_nfa_edge *e = node->edges;

  /* For each node, we follow all epsilon paths to build the closure.
   * The closure omits nodes that have only epsilon edges.
   * The closure is split into partial closures -- all the states in
   * a partial closure are reached by crossing the same list of
   * of side effects (though not necessarily the same path).
   */
  if (node->mark)
    return 1;
  node->mark = 1;

  if (node->id >= 0 || node->is_final)
    {
      struct rx_possible_future **ec;
      struct rx_se_list * prog_in_order
	= ((struct rx_se_list *)hash_se_prog (rx,
					      &rx->se_list_memo,
					      frame->prog_backwards));
      int cmp;

      ec = &outnode->futures;

      while (*ec)
	{
	  cmp = se_list_cmp ((void *)(*ec)->effects, (void *)prog_in_order);
	  if (cmp <= 0)
	    break;
	  ec = &(*ec)->next;
	}
      if (!*ec || (cmp < 0))
	{
	  struct rx_possible_future * saved = *ec;
	  *ec = rx_possible_future (rx, prog_in_order);
	  (*ec)->next = saved;
	  if (!*ec)
	    return 0;
	}
      if (node->id >= 0)
	{
	  (*ec)->destset = nfa_set_enjoin (rx, &rx->set_list_memo,
					   node, (*ec)->destset);
	  if (!(*ec)->destset)
	    return 0;
	}
    }

  while (e)
    {
      switch (e->type)
	{
	case ne_epsilon:
	  if (!eclose_node (rx, outnode, e->dest, frame))
	    return 0;
	  break;
	case ne_side_effect:
	  {
	    frame->prog_backwards = side_effect_cons (rx, 
						      e->params.side_effect,
						      frame->prog_backwards);
	    if (!frame->prog_backwards)
	      return 0;
	    if (!eclose_node (rx, outnode, e->dest, frame))
	      return 0;
	    {
	      struct rx_se_list * dying = frame->prog_backwards;
	      frame->prog_backwards = frame->prog_backwards->cdr;
	      free ((char *)dying);
	    }
	    break;
	  }
	default:
	  break;
	}
      e = e->next;
    }
  node->mark = 0;
  return 1;
}


#ifdef __STDC__
RX_DECL int 
rx_eclose_nfa (struct rx *rx)
#else
RX_DECL int 
rx_eclose_nfa (rx)
     struct rx *rx;
#endif
{
  struct rx_nfa_state *n = rx->nfa_states;
  struct eclose_frame frame;
  static int rx_id = 0;
  
  frame.prog_backwards = 0;
  rx->rx_id = rx_id++;
  bzero (&rx->se_list_memo, sizeof (rx->se_list_memo));
  bzero (&rx->set_list_memo, sizeof (rx->set_list_memo));
  while (n)
    {
      n->futures = 0;
      if (n->eclosure_needed && !eclose_node (rx, n, n, &frame))
	return 0;
      /* clear_marks (rx); */
      n = n->next;
    }
  return 1;
}


/* This deletes epsilon edges from an NFA.  After running eclose_node,
 * we have no more need for these edges.  They are removed to simplify
 * further operations on the NFA.
 */

#ifdef __STDC__
RX_DECL void 
rx_delete_epsilon_transitions (struct rx *rx)
#else
RX_DECL void 
rx_delete_epsilon_transitions (rx)
     struct rx *rx;
#endif
{
  struct rx_nfa_state *n = rx->nfa_states;
  struct rx_nfa_edge **e;

  while (n)
    {
      e = &n->edges;
      while (*e)
	{
	  struct rx_nfa_edge *t;
	  switch ((*e)->type)
	    {
	    case ne_epsilon:
	    case ne_side_effect:
	      t = *e;
	      *e = t->next;
	      rx_free_nfa_edge (t);
	      break;

	    default:
	      e = &(*e)->next;
	      break;
	    }
	}
      n = n->next;
    }
}


/* This page: storing the nfa in a contiguous region of memory for
 * subsequent conversion to a super-nfa.
 */


/* This is for qsort on an array of nfa_states. The order
 * is based on state ids and goes 
 *		[0...MAX][MIN..-1] where (MAX>=0) and (MIN<0)
 * This way, positive ids double as array indices.
 */

#ifdef __STDC__
static int 
nfacmp (void * va, void * vb)
#else
static int 
nfacmp (va, vb)
     void * va;
     void * vb;
#endif
{
  struct rx_nfa_state **a = (struct rx_nfa_state **)va;
  struct rx_nfa_state **b = (struct rx_nfa_state **)vb;
  return (*a == *b		/* &&&& 3.18 */
	  ? 0
	  : (((*a)->id < 0) == ((*b)->id < 0)
	     ? (((*a)->id  < (*b)->id) ? -1 : 1)
	     : (((*a)->id < 0)
		? 1 : -1)));
}

#ifdef __STDC__
static int 
count_hash_nodes (struct rx_hash * st)
#else
static int 
count_hash_nodes (st)
     struct rx_hash * st;
#endif
{
  int x;
  int count = 0;
  for (x = 0; x < 13; ++x)
    count += ((st->children[x])
	      ? count_hash_nodes (st->children[x])
	      : st->bucket_size[x]);
  
  return count;
}


#ifdef __STDC__
static void 
se_memo_freer (struct rx_hash_item * node)
#else
static void 
se_memo_freer (node)
     struct rx_hash_item * node;
#endif
{
  free ((char *)node->data);
}


#ifdef __STDC__
static void 
nfa_set_freer (struct rx_hash_item * node)
#else
static void 
nfa_set_freer (node)
     struct rx_hash_item * node;
#endif
{
  free ((char *)node->data);
}


/* This copies an entire NFA into a single malloced block of memory.
 * Mostly this is for compatability with regex.c, though it is convenient
 * to have the nfa nodes in an array.
 */

#ifdef __STDC__
RX_DECL int 
rx_compactify_nfa (struct rx *rx,
		   void **mem, unsigned long *size)
#else
RX_DECL int 
rx_compactify_nfa (rx, mem, size)
     struct rx *rx;
     void **mem;
     unsigned long *size;
#endif
{
  int total_nodec;
  struct rx_nfa_state *n;
  int edgec = 0;
  int eclosec = 0;
  int se_list_consc = count_hash_nodes (&rx->se_list_memo);
  int nfa_setc = count_hash_nodes (&rx->set_list_memo);
  unsigned long total_size;

  /* This takes place in two stages.   First, the total size of the
   * nfa is computed, then structures are copied.  
   */   
  n = rx->nfa_states;
  total_nodec = 0;
  while (n)
    {
      struct rx_nfa_edge *e = n->edges;
      struct rx_possible_future *ec = n->futures;
      ++total_nodec;
      while (e)
	{
	  ++edgec;
	  e = e->next;
	}
      while (ec)
	{
	  ++eclosec;
	  ec = ec->next;
	}
      n = n->next;
    }

  total_size = (total_nodec * sizeof (struct rx_nfa_state)
		+ edgec * rx_sizeof_bitset (rx->local_cset_size)
		+ edgec * sizeof (struct rx_nfa_edge)
		+ nfa_setc * sizeof (struct rx_nfa_state_set)
		+ eclosec * sizeof (struct rx_possible_future)
		+ se_list_consc * sizeof (struct rx_se_list)
		+ rx->reserved);

  if (total_size > *size)
    {
      *mem = remalloc (*mem, total_size);
      if (*mem)
	*size = total_size;
      else
	return 0;
    }
  /* Now we've allocated the memory; this copies the NFA. */
  {
    static struct rx_nfa_state **scratch = 0;
    static int scratch_alloc = 0;
    struct rx_nfa_state *state_base = (struct rx_nfa_state *) * mem;
    struct rx_nfa_state *new_state = state_base;
    struct rx_nfa_edge *new_edge =
      (struct rx_nfa_edge *)
	((char *) state_base + total_nodec * sizeof (struct rx_nfa_state));
    struct rx_se_list * new_se_list =
      (struct rx_se_list *)
	((char *)new_edge + edgec * sizeof (struct rx_nfa_edge));
    struct rx_possible_future *new_close =
      ((struct rx_possible_future *)
       ((char *) new_se_list
	+ se_list_consc * sizeof (struct rx_se_list)));
    struct rx_nfa_state_set * new_nfa_set =
      ((struct rx_nfa_state_set *)
       ((char *)new_close + eclosec * sizeof (struct rx_possible_future)));
    char *new_bitset =
      ((char *) new_nfa_set + nfa_setc * sizeof (struct rx_nfa_state_set));
    int x;
    struct rx_nfa_state *n;

    if (scratch_alloc < total_nodec)
      {
	scratch = ((struct rx_nfa_state **)
		   remalloc (scratch, total_nodec * sizeof (*scratch)));
	if (scratch)
	  scratch_alloc = total_nodec;
	else
	  {
	    scratch_alloc = 0;
	    return 0;
	  }
      }

    for (x = 0, n = rx->nfa_states; n; n = n->next)
      scratch[x++] = n;

    qsort (scratch, total_nodec,
	   sizeof (struct rx_nfa_state *), (int (*)())nfacmp);

    for (x = 0; x < total_nodec; ++x)
      {
	struct rx_possible_future *eclose = scratch[x]->futures;
	struct rx_nfa_edge *edge = scratch[x]->edges;
	struct rx_nfa_state *cn = new_state++;
	cn->futures = 0;
	cn->edges = 0;
	cn->next = (x == total_nodec - 1) ? 0 : (cn + 1);
	cn->id = scratch[x]->id;
	cn->is_final = scratch[x]->is_final;
	cn->is_start = scratch[x]->is_start;
	cn->mark = 0;
	while (edge)
	  {
	    int indx = (edge->dest->id < 0
			 ? (total_nodec + edge->dest->id)
			 : edge->dest->id);
	    struct rx_nfa_edge *e = new_edge++;
	    rx_Bitset cset = (rx_Bitset) new_bitset;
	    new_bitset += rx_sizeof_bitset (rx->local_cset_size);
	    rx_bitset_null (rx->local_cset_size, cset);
	    rx_bitset_union (rx->local_cset_size, cset, edge->params.cset);
	    e->next = cn->edges;
	    cn->edges = e;
	    e->type = edge->type;
	    e->dest = state_base + indx;
	    e->params.cset = cset;
	    edge = edge->next;
	  }
	while (eclose)
	  {
	    struct rx_possible_future *ec = new_close++;
	    struct rx_hash_item * sp;
	    struct rx_se_list ** sepos;
	    struct rx_se_list * sesrc;
	    struct rx_nfa_state_set * destlst;
	    struct rx_nfa_state_set ** destpos;
	    ec->next = cn->futures;
	    cn->futures = ec;
	    for (sepos = &ec->effects, sesrc = eclose->effects;
		 sesrc;
		 sesrc = sesrc->cdr, sepos = &(*sepos)->cdr)
	      {
		sp = rx_hash_find (&rx->se_list_memo,
				   (long)sesrc->car ^ (long)sesrc->cdr,
				   sesrc, &se_list_hash_rules);
		if (sp->binding)
		  {
		    sesrc = (struct rx_se_list *)sp->binding;
		    break;
		  }
		*new_se_list = *sesrc;
		sp->binding = (void *)new_se_list;
		*sepos = new_se_list;
		++new_se_list;
	      }
	    *sepos = sesrc;
	    for (destpos = &ec->destset, destlst = eclose->destset;
		 destlst;
		 destpos = &(*destpos)->cdr, destlst = destlst->cdr)
	      {
		sp = rx_hash_find (&rx->set_list_memo,
				   ((((long)destlst->car) >> 8)
				    ^ (long)destlst->cdr),
				   destlst, &nfa_set_hash_rules);
		if (sp->binding)
		  {
		    destlst = (struct rx_nfa_state_set *)sp->binding;
		    break;
		  }
		*new_nfa_set = *destlst;
		new_nfa_set->car = state_base + destlst->car->id;
		sp->binding = (void *)new_nfa_set;
		*destpos = new_nfa_set;
		++new_nfa_set;
	      }
	    *destpos = destlst;
	    eclose = eclose->next;
	  }
      }
  }
  rx_free_hash_table (&rx->se_list_memo, se_memo_freer, &se_list_hash_rules);
  bzero (&rx->se_list_memo, sizeof (rx->se_list_memo));
  rx_free_hash_table (&rx->set_list_memo, nfa_set_freer, &nfa_set_hash_rules);
  bzero (&rx->set_list_memo, sizeof (rx->set_list_memo));

  rx_free_nfa (rx);
  rx->nfa_states = (struct rx_nfa_state *)*mem;
  return 1;
}


/* The functions in the next several pages define the lazy-NFA-conversion used
 * by matchers.  The input to this construction is an NFA such as 
 * is built by compactify_nfa (rx.c).  The output is the superNFA.
 */


/* Match engines can use arbitrary values for opcodes.  So, the parse tree 
 * is built using instructions names (enum rx_opcode), but the superstate
 * nfa is populated with mystery opcodes (void *).
 *
 * For convenience, here is an id table.  The opcodes are == to their inxs
 *
 * The lables in re_search_2 would make good values for instructions.
 */

void * rx_id_instruction_table[rx_num_instructions] =
{
  (void *) rx_backtrack_point,
  (void *) rx_do_side_effects,
  (void *) rx_cache_miss,
  (void *) rx_next_char,
  (void *) rx_backtrack,
  (void *) rx_error_inx
};



/* Memory mgt. for superstate graphs. */

#ifdef __STDC__
static char *
rx_cache_malloc (struct rx_cache * cache, int bytes)
#else
static char *
rx_cache_malloc (cache, bytes)
     struct rx_cache * cache;
     int bytes;
#endif
{
  while (cache->bytes_left < bytes)
    {
      if (cache->memory_pos)
	cache->memory_pos = cache->memory_pos->next;
      if (!cache->memory_pos)
	{
	  cache->morecore (cache);
	  if (!cache->memory_pos)
	    return 0;
	}
      cache->bytes_left = cache->memory_pos->bytes;
      cache->memory_addr = ((char *)cache->memory_pos
			    + sizeof (struct rx_blocklist));
    }
  cache->bytes_left -= bytes;
  {
    char * addr = cache->memory_addr;
    cache->memory_addr += bytes;
    return addr;
  }
}

#ifdef __STDC__
static void
rx_cache_free (struct rx_cache * cache,
	       struct rx_freelist ** freelist, char * mem)
#else
static void
rx_cache_free (cache, freelist, mem)
     struct rx_cache * cache;
     struct rx_freelist ** freelist;
     char * mem;
#endif
{
  struct rx_freelist * it = (struct rx_freelist *)mem;
  it->next = *freelist;
  *freelist = it;
}


/* The partially instantiated superstate graph has a transition 
 * table at every node.  There is one entry for every character.
 * This fills in the transition for a set.
 */
#ifdef __STDC__
static void 
install_transition (struct rx_superstate *super,
		    struct rx_inx *answer, rx_Bitset trcset) 
#else
static void 
install_transition (super, answer, trcset)
     struct rx_superstate *super;
     struct rx_inx *answer;
     rx_Bitset trcset;
#endif
{
  struct rx_inx * transitions = super->transitions;
  int chr;
  for (chr = 0; chr < 256; )
    if (!*trcset)
      {
	++trcset;
	chr += 32;
      }
    else
      {
	RX_subset sub = *trcset;
	RX_subset mask = 1;
	int bound = chr + 32;
	while (chr < bound)
	  {
	    if (sub & mask)
	      transitions [chr] = *answer;
	    ++chr;
	    mask <<= 1;
	  }
	++trcset;
      }
}


#if 1
static int
qlen (q)
     struct rx_superstate * q;
{
  int count = 1;
  struct rx_superstate * it;
  if (!q)
    return 0;
  for (it = q->next_recyclable; it != q; it = it->next_recyclable)
    ++count;
  return count;
}

static void
check_cache (cache)
     struct rx_cache * cache;
{
  struct rx_cache * you_fucked_up = 0;
  int total = cache->superstates;
  int semi = cache->semifree_superstates;
  if (semi != qlen (cache->semifree_superstate))
    check_cache (you_fucked_up);
  if ((total - semi) != qlen (cache->lru_superstate))
    check_cache (you_fucked_up);
}
#endif

#ifdef __STDC__
static void
semifree_superstate (struct rx_cache * cache)
#else
static void
semifree_superstate (cache)
     struct rx_cache * cache;
#endif
{
  int disqualified = cache->semifree_superstates;
  if (disqualified == cache->superstates)
    return;
  while (cache->lru_superstate->locks)
    {
      cache->lru_superstate = cache->lru_superstate->next_recyclable;
      ++disqualified;
      if (disqualified == cache->superstates)
	return;
    }
  {
    struct rx_superstate * it = cache->lru_superstate;
    it->next_recyclable->prev_recyclable = it->prev_recyclable;
    it->prev_recyclable->next_recyclable = it->next_recyclable;
    cache->lru_superstate = (it == it->next_recyclable
			     ? 0
			     : it->next_recyclable);
    if (!cache->semifree_superstate)
      {
	cache->semifree_superstate = it;
	it->next_recyclable = it;
	it->prev_recyclable = it;
      }
    else
      {
	it->prev_recyclable = cache->semifree_superstate->prev_recyclable;
	it->next_recyclable = cache->semifree_superstate;
	it->prev_recyclable->next_recyclable = it;
	it->next_recyclable->prev_recyclable = it;
      }
    {
      struct rx_distinct_future *df;
      it->is_semifree = 1;
      ++cache->semifree_superstates;
      df = it->transition_refs;
      if (df)
	{
	  df->prev_same_dest->next_same_dest = 0;
	  for (df = it->transition_refs; df; df = df->next_same_dest)
	    {
	      df->future_frame.inx = cache->instruction_table[rx_cache_miss];
	      df->future_frame.data = 0;
	      df->future_frame.data_2 = (void *) df;
	      /* If there are any NEXT-CHAR instruction frames that
	       * refer to this state, we convert them to CACHE-MISS frames.
	       */
	      if (!df->effects
		  && (df->edge->options->next_same_super_edge[0]
		      == df->edge->options))
		install_transition (df->present, &df->future_frame,
				    df->edge->cset);
	    }
	  df = it->transition_refs;
	  df->prev_same_dest->next_same_dest = df;
	}
    }
  }
}


#ifdef __STDC__
static void 
refresh_semifree_superstate (struct rx_cache * cache,
			     struct rx_superstate * super)
#else
static void 
refresh_semifree_superstate (cache, super)
     struct rx_cache * cache;
     struct rx_superstate * super;
#endif
{
  struct rx_distinct_future *df;

  if (super->transition_refs)
    {
      super->transition_refs->prev_same_dest->next_same_dest = 0; 
      for (df = super->transition_refs; df; df = df->next_same_dest)
	{
	  df->future_frame.inx = cache->instruction_table[rx_next_char];
	  df->future_frame.data = (void *) super->transitions;
	  /* CACHE-MISS instruction frames that refer to this state,
	   * must be converted to NEXT-CHAR frames.
	   */
	  if (!df->effects
	      && (df->edge->options->next_same_super_edge[0]
		  == df->edge->options))
	    install_transition (df->present, &df->future_frame,
				df->edge->cset);
	}
      super->transition_refs->prev_same_dest->next_same_dest
	= super->transition_refs;
    }
  if (cache->semifree_superstate == super)
    cache->semifree_superstate = (super->prev_recyclable == super
				  ? 0
				  : super->prev_recyclable);
  super->next_recyclable->prev_recyclable = super->prev_recyclable;
  super->prev_recyclable->next_recyclable = super->next_recyclable;

  if (!cache->lru_superstate)
    (cache->lru_superstate
     = super->next_recyclable
     = super->prev_recyclable
     = super);
  else
    {
      super->next_recyclable = cache->lru_superstate;
      super->prev_recyclable = cache->lru_superstate->prev_recyclable;
      super->next_recyclable->prev_recyclable = super;
      super->prev_recyclable->next_recyclable = super;
    }
  super->is_semifree = 0;
  --cache->semifree_superstates;
}

#ifdef __STDC__
static void
rx_refresh_this_superstate (struct rx_cache * cache, struct rx_superstate * superstate)
#else
static void
rx_refresh_this_superstate (cache, superstate)
     struct rx_cache * cache;
     struct rx_superstate * superstate;
#endif
{
  if (superstate->is_semifree)
    refresh_semifree_superstate (cache, superstate);
  else if (cache->lru_superstate == superstate)
    cache->lru_superstate = superstate->next_recyclable;
  else if (superstate != cache->lru_superstate->prev_recyclable)
    {
      superstate->next_recyclable->prev_recyclable
	= superstate->prev_recyclable;
      superstate->prev_recyclable->next_recyclable
	= superstate->next_recyclable;
      superstate->next_recyclable = cache->lru_superstate;
      superstate->prev_recyclable = cache->lru_superstate->prev_recyclable;
      superstate->next_recyclable->prev_recyclable = superstate;
      superstate->prev_recyclable->next_recyclable = superstate;
    }
}

#ifdef __STDC__
static void 
release_superset_low (struct rx_cache * cache,
		     struct rx_superset *set)
#else
static void 
release_superset_low (cache, set)
     struct rx_cache * cache;
     struct rx_superset *set;
#endif
{
  if (!--set->refs)
    {
      if (set->cdr)
	release_superset_low (cache, set->cdr);

      set->starts_for = 0;

      rx_hash_free
	(rx_hash_find
	 (&cache->superset_table,
	  (unsigned long)set->car ^ set->id ^ (unsigned long)set->cdr,
	  (void *)set,
	  &cache->superset_hash_rules),
	 &cache->superset_hash_rules);
      rx_cache_free (cache, &cache->free_supersets, (char *)set);
    }
}

#ifdef __STDC__
RX_DECL void 
rx_release_superset (struct rx *rx,
		     struct rx_superset *set)
#else
RX_DECL void 
rx_release_superset (rx, set)
     struct rx *rx;
     struct rx_superset *set;
#endif
{
  release_superset_low (rx->cache, set);
}

/* This tries to add a new superstate to the superstate freelist.
 * It might, as a result, free some edge pieces or hash tables.
 * If nothing can be freed because too many locks are being held, fail.
 */

#ifdef __STDC__
static int
rx_really_free_superstate (struct rx_cache * cache)
#else
static int
rx_really_free_superstate (cache)
     struct rx_cache * cache;
#endif
{
  int locked_superstates = 0;
  struct rx_superstate * it;

  if (!cache->superstates)
    return 0;

  {
    /* This is a total guess.  The idea is that we should expect as
     * many misses as we've recently experienced.  I.e., cache->misses
     * should be the same as cache->semifree_superstates.
     */
    while ((cache->hits + cache->misses) > cache->superstates_allowed)
      {
	cache->hits >>= 1;
	cache->misses >>= 1;
      }
    if (  ((cache->hits + cache->misses) * cache->semifree_superstates)
	< (cache->superstates		 * cache->misses))
      {
	semifree_superstate (cache);
	semifree_superstate (cache);
      }
  }

  while (cache->semifree_superstate && cache->semifree_superstate->locks)
    {
      refresh_semifree_superstate (cache, cache->semifree_superstate);
      ++locked_superstates;
      if (locked_superstates == cache->superstates)
	return 0;
    }

  if (cache->semifree_superstate)
    {
      it = cache->semifree_superstate;
      it->next_recyclable->prev_recyclable = it->prev_recyclable;
      it->prev_recyclable->next_recyclable = it->next_recyclable;
      cache->semifree_superstate = ((it == it->next_recyclable)
				    ? 0
				    : it->next_recyclable);
      --cache->semifree_superstates;
    }
  else
    {
      while (cache->lru_superstate->locks)
	{
	  cache->lru_superstate = cache->lru_superstate->next_recyclable;
	  ++locked_superstates;
	  if (locked_superstates == cache->superstates)
	    return 0;
	}
      it = cache->lru_superstate;
      it->next_recyclable->prev_recyclable = it->prev_recyclable;
      it->prev_recyclable->next_recyclable = it->next_recyclable;
      cache->lru_superstate = ((it == it->next_recyclable)
				    ? 0
				    : it->next_recyclable);
    }

  if (it->transition_refs)
    {
      struct rx_distinct_future *df;
      for (df = it->transition_refs,
	   df->prev_same_dest->next_same_dest = 0;
	   df;
	   df = df->next_same_dest)
	{
	  df->future_frame.inx = cache->instruction_table[rx_cache_miss];
	  df->future_frame.data = 0;
	  df->future_frame.data_2 = (void *) df;
	  df->future = 0;
	}
      it->transition_refs->prev_same_dest->next_same_dest =
	it->transition_refs;
    }
  {
    struct rx_super_edge *tc = it->edges;
    while (tc)
      {
	struct rx_distinct_future * df;
	struct rx_super_edge *tct = tc->next;
	df = tc->options;
	df->next_same_super_edge[1]->next_same_super_edge[0] = 0;
	while (df)
	  {
	    struct rx_distinct_future *dft = df;
	    df = df->next_same_super_edge[0];
	    
	    
	    if (dft->future && dft->future->transition_refs == dft)
	      {
		dft->future->transition_refs = dft->next_same_dest;
		if (dft->future->transition_refs == dft)
		  dft->future->transition_refs = 0;
	      }
	    dft->next_same_dest->prev_same_dest = dft->prev_same_dest;
	    dft->prev_same_dest->next_same_dest = dft->next_same_dest;
	    rx_cache_free (cache, &cache->free_discernable_futures,
			   (char *)dft);
	  }
	rx_cache_free (cache, &cache->free_transition_classes, (char *)tc);
	tc = tct;
      }
  }
  
  if (it->contents->superstate == it)
    it->contents->superstate = 0;
  release_superset_low (cache, it->contents);
  rx_cache_free (cache, &cache->free_superstates, (char *)it);
  --cache->superstates;
  return 1;
}

#ifdef __STDC__
static char *
rx_cache_get (struct rx_cache * cache,
	      struct rx_freelist ** freelist)
#else
static char *
rx_cache_get (cache, freelist)
     struct rx_cache * cache;
     struct rx_freelist ** freelist;
#endif
{
  while (!*freelist && rx_really_free_superstate (cache))
    ;
  if (!*freelist)
    return 0;
  {
    struct rx_freelist * it = *freelist;
    *freelist = it->next;
    return (char *)it;
  }
}

#ifdef __STDC__
static char *
rx_cache_malloc_or_get (struct rx_cache * cache,
			struct rx_freelist ** freelist, int bytes)
#else
static char *
rx_cache_malloc_or_get (cache, freelist, bytes)
     struct rx_cache * cache;
     struct rx_freelist ** freelist;
     int bytes;
#endif
{
  if (!*freelist)
    {
      char * answer = rx_cache_malloc (cache, bytes);
      if (answer)
	return answer;
    }

  return rx_cache_get (cache, freelist);
}

#ifdef __STDC__
static char *
rx_cache_get_superstate (struct rx_cache * cache)
#else
static char *
rx_cache_get_superstate (cache)
	  struct rx_cache * cache;
#endif
{
  char * answer;
  int bytes = (   sizeof (struct rx_superstate)
	       +  cache->local_cset_size * sizeof (struct rx_inx));
  if (!cache->free_superstates
      && (cache->superstates < cache->superstates_allowed))
    {
      answer = rx_cache_malloc (cache, bytes);
      if (answer)
	{
	  ++cache->superstates;
	  return answer;
	}
    }
  answer = rx_cache_get (cache, &cache->free_superstates);
  if (!answer)
    {
      answer = rx_cache_malloc (cache, bytes);
      if (answer)
	++cache->superstates_allowed;
    }
  ++cache->superstates;
  return answer;
}



static int
supersetcmp (va, vb)
     void * va;
     void * vb;
{
  struct rx_superset * a = (struct rx_superset *)va;
  struct rx_superset * b = (struct rx_superset *)vb;
  return (   (a == b)
	  || (a && b && (a->car == b->car) && (a->cdr == b->cdr)));
}


#ifdef __STDC__
static struct rx_hash_item *
superset_allocator (struct rx_hash_rules * rules, void * val)
#else
static struct rx_hash_item *
superset_allocator (rules, val)
     struct rx_hash_rules * rules;
     void * val;
#endif
{
  struct rx_cache * cache
    = ((struct rx_cache *)
       ((char *)rules
	- (unsigned long)(&((struct rx_cache *)0)->superset_hash_rules)));
  struct rx_superset * template = (struct rx_superset *)val;
  struct rx_superset * newset
    = ((struct rx_superset *)
       rx_cache_malloc_or_get (cache,
			       &cache->free_supersets,
			       sizeof (*template)));
  if (!newset)
    return 0;
  newset->refs = 0;
  newset->car = template->car;
  newset->id = template->car->id;
  newset->cdr = template->cdr;
  newset->superstate = 0;
  rx_protect_superset (rx, template->cdr);
  newset->hash_item.data = (void *)newset;
  newset->hash_item.binding = 0;
  return &newset->hash_item;
}

#ifdef __STDC__
static struct rx_hash * 
super_hash_allocator (struct rx_hash_rules * rules)
#else
static struct rx_hash * 
super_hash_allocator (rules)
     struct rx_hash_rules * rules;
#endif
{
  struct rx_cache * cache
    = ((struct rx_cache *)
       ((char *)rules
	- (unsigned long)(&((struct rx_cache *)0)->superset_hash_rules)));
  return ((struct rx_hash *)
	  rx_cache_malloc_or_get (cache,
				  &cache->free_hash, sizeof (struct rx_hash)));
}


#ifdef __STDC__
static void
super_hash_liberator (struct rx_hash * hash, struct rx_hash_rules * rules)
#else
static void
super_hash_liberator (hash, rules)
     struct rx_hash * hash;
     struct rx_hash_rules * rules;
#endif
{
  struct rx_cache * cache
    = ((struct rx_cache *)
       (char *)rules - (long)(&((struct rx_cache *)0)->superset_hash_rules));
  rx_cache_free (cache, &cache->free_hash, (char *)hash);
}

#ifdef __STDC__
static void
superset_hash_item_liberator (struct rx_hash_item * it,
			      struct rx_hash_rules * rules)
#else
static void
superset_hash_item_liberator (it, rules) /* Well, it does ya know. */
     struct rx_hash_item * it;
     struct rx_hash_rules * rules;
#endif
{
}

int rx_cache_bound = 128;
static int rx_default_cache_got = 0;

#ifdef __STDC__
static int
bytes_for_cache_size (int supers, int cset_size)
#else
static int
bytes_for_cache_size (supers, cset_size)
     int supers;
     int cset_size;
#endif
{
  return (int)
    ((float)supers *
     (  (1.03 * (float) (  rx_sizeof_bitset (cset_size)
			 + sizeof (struct rx_super_edge)))
      + (1.80 * (float) sizeof (struct rx_possible_future))
      + (float) (  sizeof (struct rx_superstate)
		 + cset_size * sizeof (struct rx_inx))));
}

#ifdef __STDC__
static void
rx_morecore (struct rx_cache * cache)
#else
static void
rx_morecore (cache)
     struct rx_cache * cache;
#endif
{
  if (rx_default_cache_got >= rx_cache_bound)
    return;

  rx_default_cache_got += 16;
  cache->superstates_allowed = rx_cache_bound;
  {
    struct rx_blocklist ** pos = &cache->memory;
    int size = bytes_for_cache_size (16, cache->local_cset_size);
    while (*pos)
      pos = &(*pos)->next;
    *pos = ((struct rx_blocklist *)
	    malloc (size + sizeof (struct rx_blocklist))); 
    if (!*pos)
      return;

    (*pos)->next = 0;
    (*pos)->bytes = size;
    cache->memory_pos = *pos;
    cache->memory_addr = (char *)*pos + sizeof (**pos);
    cache->bytes_left = size;
  }
}

static struct rx_cache default_cache = 
{
  {
    supersetcmp,
    super_hash_allocator,
    super_hash_liberator,
    superset_allocator,
    superset_hash_item_liberator,
  },
  0,
  0,
  0,
  0,
  rx_morecore,

  0,
  0,
  0,
  0,
  0,

  0,
  0,

  0,

  0,
  0,
  0,
  0,
  128,

  256,
  rx_id_instruction_table,

  {
    0,
    0,
    {0},
    {0},
    {0}
  }
};

/* This adds an element to a superstate set.  These sets are lists, such
 * that lists with == elements are ==.  The empty set is returned by
 * superset_cons (rx, 0, 0) and is NOT equivelent to 
 * (struct rx_superset)0.
 */

#ifdef __STDC__
RX_DECL struct rx_superset *
rx_superset_cons (struct rx * rx,
		  struct rx_nfa_state *car, struct rx_superset *cdr)
#else
RX_DECL struct rx_superset *
rx_superset_cons (rx, car, cdr)
     struct rx * rx;
     struct rx_nfa_state *car;
     struct rx_superset *cdr;
#endif
{
  struct rx_cache * cache = rx->cache;
  if (!car && !cdr)
    {
      if (!cache->empty_superset)
	{
	  cache->empty_superset
	    = ((struct rx_superset *)
	       rx_cache_malloc_or_get (cache, &cache->free_supersets,
				       sizeof (struct rx_superset)));
	  if (!cache->empty_superset)
	    return 0;
	  bzero (cache->empty_superset, sizeof (struct rx_superset));
	  cache->empty_superset->refs = 1000;
	}
      return cache->empty_superset;
    }
  {
    struct rx_superset template;
    struct rx_hash_item * hit;
    template.car = car;
    template.cdr = cdr;
    template.id = car->id;
    hit = rx_hash_store (&cache->superset_table,
			 (unsigned long)car ^ car->id ^ (unsigned long)cdr,
			 (void *)&template,
			 &cache->superset_hash_rules);
    return (hit
	    ?  (struct rx_superset *)hit->data
	    : 0);
  }
}

/* This computes a union of two NFA state sets.  The sets do not have the
 * same representation though.  One is a RX_SUPERSET structure (part
 * of the superstate NFA) and the other is an NFA_STATE_SET (part of the NFA).
 */

#ifdef __STDC__
RX_DECL struct rx_superset *
rx_superstate_eclosure_union
  (struct rx * rx, struct rx_superset *set, struct rx_nfa_state_set *ecl) 
#else
RX_DECL struct rx_superset *
rx_superstate_eclosure_union (rx, set, ecl)
     struct rx * rx;
     struct rx_superset *set;
     struct rx_nfa_state_set *ecl;
#endif
{
  if (!ecl)
    return set;

  if (!set->car)
    return rx_superset_cons (rx, ecl->car,
			     rx_superstate_eclosure_union (rx, set, ecl->cdr));
  if (set->car == ecl->car)
    return rx_superstate_eclosure_union (rx, set, ecl->cdr);

  {
    struct rx_superset * tail;
    struct rx_nfa_state * first;
    struct rx_superset * answer;

    if (set->car > ecl->car)
      {
	tail = rx_superstate_eclosure_union (rx, set->cdr, ecl);
	first = set->car;
      }
    else
      {
	tail = rx_superstate_eclosure_union (rx, set, ecl->cdr);
	first = ecl->car;
      }
    if (!tail)
      return 0;
    else
      {
	struct rx_superset * answer;
	answer = rx_superset_cons (rx, set->car, tail);
	if (!answer)
	  {
	    rx_protect_superset (rx, tail);
	    rx_release_superset (rx, tail);
	  }
	else
	  return answer;
      }
  }
}




/*
 * This makes sure that a list of rx_distinct_futures contains
 * a future for each possible set of side effects in the eclosure
 * of a given state.  This is some of the work of filling in a
 * superstate transition. 
 */

#ifdef __STDC__
static struct rx_distinct_future *
include_futures (struct rx *rx,
		 struct rx_distinct_future *df, struct rx_nfa_state
		 *state, struct rx_superstate *superstate) 
#else
static struct rx_distinct_future *
include_futures (rx, df, state, superstate)
     struct rx *rx;
     struct rx_distinct_future *df;
     struct rx_nfa_state *state;
     struct rx_superstate *superstate;
#endif
{
  struct rx_possible_future *future;
  struct rx_cache * cache = rx->cache;
  for (future = state->futures; future; future = future->next)
    {
      struct rx_distinct_future *dfp;
      struct rx_distinct_future *insert_before = 0;
      if (df)
	df->next_same_super_edge[1]->next_same_super_edge[0] = 0;
      for (dfp = df; dfp; dfp = dfp->next_same_super_edge[0])
	if (dfp->effects == future->effects)
	  break;
	else
	  {
	    int order = rx->se_list_cmp (rx, dfp->effects, future->effects);
	    if (order > 0)
	      {
		insert_before = dfp;
		dfp = 0;
		break;
	      }
	  }
      if (df)
	df->next_same_super_edge[1]->next_same_super_edge[0] = df;
      if (!dfp)
	{
	  dfp
	    = ((struct rx_distinct_future *)
	       rx_cache_malloc_or_get (cache, &cache->free_discernable_futures,
				       sizeof (struct rx_distinct_future)));
	  if (!dfp)
	    return 0;
	  if (!df)
	    {
	      df = insert_before = dfp;
	      df->next_same_super_edge[0] = df->next_same_super_edge[1] = df;
	    }
	  else if (!insert_before)
	    insert_before = df;
	  else if (insert_before == df)
	    df = dfp;

	  dfp->next_same_super_edge[0] = insert_before;
	  dfp->next_same_super_edge[1]
	    = insert_before->next_same_super_edge[1];
	  dfp->next_same_super_edge[1]->next_same_super_edge[0] = dfp;
	  dfp->next_same_super_edge[0]->next_same_super_edge[1] = dfp;
	  dfp->next_same_dest = dfp->prev_same_dest = dfp;
	  dfp->future = 0;
	  dfp->present = superstate;
	  dfp->future_frame.inx = rx->instruction_table[rx_cache_miss];
	  dfp->future_frame.data = 0;
	  dfp->future_frame.data_2 = (void *) dfp;
	  dfp->side_effects_frame.inx
	    = rx->instruction_table[rx_do_side_effects];
	  dfp->side_effects_frame.data = 0;
	  dfp->side_effects_frame.data_2 = (void *) dfp;
	  dfp->effects = future->effects;
	}
    }
  return df;
}




/* This constructs a new superstate from its state set.  The only 
 * complexity here is memory management.
 */
#ifdef __STDC__
RX_DECL struct rx_superstate *
rx_superstate (struct rx *rx,
	       struct rx_superset *set)
#else
RX_DECL struct rx_superstate *
rx_superstate (rx, set)
     struct rx *rx;
     struct rx_superset *set;
#endif
{
  struct rx_cache * cache = rx->cache;
  struct rx_superstate * superstate = 0;

  /* Does the superstate already exist in the cache? */
  if (set->superstate)
    {
      if (set->superstate->rx_id != rx->rx_id)
	{
	  /* Aha.  It is in the cache, but belongs to a superstate
	   * that refers to an NFA that no longer exists.
	   * (We know it no longer exists because it was evidently
	   *  stored in the same region of memory as the current nfa
	   *  yet it has a different id.)
	   */
	  superstate = set->superstate;
	  if (!superstate->is_semifree)
	    {
	      if (cache->lru_superstate == superstate)
		{
		  cache->lru_superstate = superstate->next_recyclable;
		  if (cache->lru_superstate == superstate)
		    cache->lru_superstate = 0;
		}
	      {
		superstate->next_recyclable->prev_recyclable
		  = superstate->prev_recyclable;
		superstate->prev_recyclable->next_recyclable
		  = superstate->next_recyclable;
		if (!cache->semifree_superstate)
		  {
		    (cache->semifree_superstate
		     = superstate->next_recyclable
		     = superstate->prev_recyclable
		     = superstate);
		  }
		else
		  {
		    superstate->next_recyclable = cache->semifree_superstate;
		    superstate->prev_recyclable
		      = cache->semifree_superstate->prev_recyclable;
		    superstate->next_recyclable->prev_recyclable
		      = superstate;
		    superstate->prev_recyclable->next_recyclable
		      = superstate;
		    cache->semifree_superstate = superstate;
		  }
		++cache->semifree_superstates;
	      }
	    }
	  set->superstate = 0;
	  goto handle_cache_miss;
	}
      ++cache->hits;
      superstate = set->superstate;

      rx_refresh_this_superstate (cache, superstate);
      return superstate;
    }

 handle_cache_miss:

  /* This point reached only for cache misses. */
  ++cache->misses;
#if RX_DEBUG
  if (rx_debug_trace > 1)
    {
      struct rx_superset * setp = set;
      fprintf (stderr, "Building a superstet %d(%d): ", rx->rx_id, set);
      while (setp)
	{
	  fprintf (stderr, "%d ", setp->id);
	  setp = setp->cdr;
	}
      fprintf (stderr, "(%d)\n", set);
    }
#endif
  superstate = (struct rx_superstate *)rx_cache_get_superstate (cache);
  if (!superstate)
    return 0;

  if (!cache->lru_superstate)
    (cache->lru_superstate
     = superstate->next_recyclable
     = superstate->prev_recyclable
     = superstate);
  else
    {
      superstate->next_recyclable = cache->lru_superstate;
      superstate->prev_recyclable = cache->lru_superstate->prev_recyclable;
      (  superstate->prev_recyclable->next_recyclable
       = superstate->next_recyclable->prev_recyclable
       = superstate);
    }
  superstate->rx_id = rx->rx_id;
  superstate->transition_refs = 0;
  superstate->locks = 0;
  superstate->is_semifree = 0;
  set->superstate = superstate;
  superstate->contents = set;
  rx_protect_superset (rx, set);
  superstate->edges = 0;
  {
    int x;
    /* None of the transitions from this superstate are known yet. */
    for (x = 0; x < rx->local_cset_size; ++x) /* &&&&& 3.8 % */
      {
	struct rx_inx * ifr = &superstate->transitions[x];
	ifr->inx = rx->instruction_table [rx_cache_miss];
	ifr->data = ifr->data_2 = 0;
      }
  }
  return superstate;
}


/* This computes the destination set of one edge of the superstate NFA.
 * Note that a RX_DISTINCT_FUTURE is a superstate edge.
 * Returns 0 on an allocation failure.
 */

#ifdef __STDC__
static int 
solve_destination (struct rx *rx, struct rx_distinct_future *df)
#else
static int 
solve_destination (rx, df)
     struct rx *rx;
     struct rx_distinct_future *df;
#endif
{
  struct rx_super_edge *tc = df->edge;
  struct rx_superset *nfa_state;
  struct rx_superset *nil_set = rx_superset_cons (rx, 0, 0);
  struct rx_superset *solution = nil_set;
  struct rx_superstate *dest;

  rx_protect_superset (rx, solution);
  /* Iterate over all NFA states in the state set of this superstate. */
  for (nfa_state = df->present->contents;
       nfa_state->car;
       nfa_state = nfa_state->cdr)
    {
      struct rx_nfa_edge *e;
      /* Iterate over all edges of each NFA state. */
      for (e = nfa_state->car->edges; e; e = e->next)
        /* If we find an edge that is labeled with 
	 * the characters we are solving for.....
	 */
	if (rx_bitset_is_subset (rx->local_cset_size,
				 tc->cset, e->params.cset))
	  {
	    struct rx_nfa_state *n = e->dest;
	    struct rx_possible_future *pf;
	    /* ....search the partial epsilon closures of the destination
	     * of that edge for a path that involves the same set of
	     * side effects we are solving for.
	     * If we find such a RX_POSSIBLE_FUTURE, we add members to the
	     * stateset we are computing.
	     */
	    for (pf = n->futures; pf; pf = pf->next)
	      if (pf->effects == df->effects)
		{
		  struct rx_superset * old_sol;
		  old_sol = solution;
		  solution = rx_superstate_eclosure_union (rx, solution,
							   pf->destset);
		  if (!solution)
		    return 0;
		  rx_protect_superset (rx, solution);
		  rx_release_superset (rx, old_sol);
		}
	  }
    }
  /* It is possible that the RX_DISTINCT_FUTURE we are working on has 
   * the empty set of NFA states as its definition.  In that case, this
   * is a failure point.
   */
  if (solution == nil_set)
    {
      df->future_frame.inx = (void *) rx_backtrack;
      df->future_frame.data = 0;
      df->future_frame.data_2 = 0;
      return 1;
    }
  dest = rx_superstate (rx, solution);
  rx_release_superset (rx, solution);
  if (!dest)
    return 0;

  {
    struct rx_distinct_future *dft;
    dft = df;
    df->prev_same_dest->next_same_dest = 0;
    while (dft)
      {
	dft->future = dest;
	dft->future_frame.inx = rx->instruction_table[rx_next_char];
	dft->future_frame.data = (void *) dest->transitions;
	dft = dft->next_same_dest;
      }
    df->prev_same_dest->next_same_dest = df;
  }
  if (!dest->transition_refs)
    dest->transition_refs = df;
  else
    {
      struct rx_distinct_future *dft = dest->transition_refs->next_same_dest;
      dest->transition_refs->next_same_dest = df->next_same_dest;
      df->next_same_dest->prev_same_dest = dest->transition_refs;
      df->next_same_dest = dft;
      dft->prev_same_dest = df;
    }
  return 1;
}


/* This takes a superstate and a character, and computes some edges
 * from the superstate NFA.  In particular, this computes all edges
 * that lead from SUPERSTATE given CHR.   This function also 
 * computes the set of characters that share this edge set.
 * This returns 0 on allocation error.
 * The character set and list of edges are returned through 
 * the paramters CSETOUT and DFOUT.
} */

#ifdef __STDC__
static int 
compute_super_edge (struct rx *rx, struct rx_distinct_future **dfout,
			  rx_Bitset csetout, struct rx_superstate *superstate,
			  unsigned char chr)  
#else
static int 
compute_super_edge (rx, dfout, csetout, superstate, chr)
     struct rx *rx;
     struct rx_distinct_future **dfout;
     rx_Bitset csetout;
     struct rx_superstate *superstate;
     unsigned char chr;
#endif
{
  struct rx_superset *stateset = superstate->contents;

  /* To compute the set of characters that share edges with CHR, 
   * we start with the full character set, and subtract.
   */
  rx_bitset_universe (rx->local_cset_size, csetout);
  *dfout = 0;

  /* Iterate over the NFA states in the superstate state-set. */
  while (stateset->car)
    {
      struct rx_nfa_edge *e;
      for (e = stateset->car->edges; e; e = e->next)
	if (RX_bitset_member (e->params.cset, chr))
	  {
	    /* If we find an NFA edge that applies, we make sure there
	     * are corresponding edges in the superstate NFA.
	     */
	    {
	      struct rx_distinct_future * saved;
	      saved = *dfout;
	      *dfout = include_futures (rx, *dfout, e->dest, superstate);
	      if (!*dfout)
		{
		  struct rx_distinct_future * df;
		  df = saved;
		  df->next_same_super_edge[1]->next_same_super_edge[0] = 0;
		  while (df)
		    {
		      struct rx_distinct_future *dft;
		      dft = df;
		      df = df->next_same_super_edge[0];

		      if (dft->future && dft->future->transition_refs == dft)
			{
			  dft->future->transition_refs = dft->next_same_dest;
			  if (dft->future->transition_refs == dft)
			    dft->future->transition_refs = 0;
			}
		      dft->next_same_dest->prev_same_dest = dft->prev_same_dest;
		      dft->prev_same_dest->next_same_dest = dft->next_same_dest;
		      rx_cache_free (rx->cache,
				     &rx->cache->free_discernable_futures,
				     (char *)dft);
		    }
		  return 0;
		}
	    }
	    /* We also trim the character set a bit. */
	    rx_bitset_intersection (rx->local_cset_size,
				    csetout, e->params.cset);
	  }
	else
	  /* An edge that doesn't apply at least tells us some characters
	   * that don't share the same edge set as CHR.
	   */
	  rx_bitset_difference (rx->local_cset_size, csetout, e->params.cset);
      stateset = stateset->cdr;
    }
  return 1;
}


/* This is a constructor for RX_SUPER_EDGE structures.  These are
 * wrappers for lists of superstate NFA edges that share character sets labels.
 * If a transition class contains more than one rx_distinct_future (superstate
 * edge), then it represents a non-determinism in the superstate NFA.
 */

#ifdef __STDC__
static struct rx_super_edge *
rx_super_edge (struct rx *rx,
	       struct rx_superstate *super, rx_Bitset cset,
	       struct rx_distinct_future *df) 
#else
static struct rx_super_edge *
rx_super_edge (rx, super, cset, df)
     struct rx *rx;
     struct rx_superstate *super;
     rx_Bitset cset;
     struct rx_distinct_future *df;
#endif
{
  struct rx_super_edge *tc =
    (struct rx_super_edge *)rx_cache_malloc_or_get
      (rx->cache, &rx->cache->free_transition_classes,
       sizeof (struct rx_super_edge) + rx_sizeof_bitset (rx->local_cset_size));

  if (!tc)
    return 0;
  tc->next = super->edges;
  super->edges = tc;
  tc->rx_backtrack_frame.inx = rx->instruction_table[rx_backtrack_point];
  tc->rx_backtrack_frame.data = 0;
  tc->rx_backtrack_frame.data_2 = (void *) tc;
  tc->options = df;
  tc->cset = (rx_Bitset) ((char *) tc + sizeof (*tc));
  rx_bitset_assign (rx->local_cset_size, tc->cset, cset);
  if (df)
    {
      struct rx_distinct_future * dfp = df;
      df->next_same_super_edge[1]->next_same_super_edge[0] = 0;
      while (dfp)
	{
	  dfp->edge = tc;
	  dfp = dfp->next_same_super_edge[0];
	}
      df->next_same_super_edge[1]->next_same_super_edge[0] = df;
    }
  return tc;
}


/* There are three kinds of cache miss.  The first occurs when a
 * transition is taken that has never been computed during the
 * lifetime of the source superstate.  That cache miss is handled by
 * calling COMPUTE_SUPER_EDGE.  The second kind of cache miss
 * occurs when the destination superstate of a transition doesn't
 * exist.  SOLVE_DESTINATION is used to construct the destination superstate.
 * Finally, the third kind of cache miss occurs when the destination
 * superstate of a transition is in a `semi-free state'.  That case is
 * handled by UNFREE_SUPERSTATE.
 *
 * The function of HANDLE_CACHE_MISS is to figure out which of these
 * cases applies.
 */

#ifdef __STDC__
static void
install_partial_transition  (struct rx_superstate *super,
			     struct rx_inx *answer,
			     RX_subset set, int offset)
#else
static void
install_partial_transition  (super, answer, set, offset)
     struct rx_superstate *super;
     struct rx_inx *answer;
     RX_subset set;
     int offset;
#endif
{
  int start = offset;
  int end = start + 32;
  RX_subset pos = 1;
  struct rx_inx * transitions = super->transitions;
  
  while (start < end)
    {
      if (set & pos)
	transitions[start] = *answer;
      pos <<= 1;
      ++start;
    }
}


#ifdef __STDC__
RX_DECL struct rx_inx *
rx_handle_cache_miss
  (struct rx *rx, struct rx_superstate *super, unsigned char chr, void *data) 
#else
RX_DECL struct rx_inx *
rx_handle_cache_miss (rx, super, chr, data)
     struct rx *rx;
     struct rx_superstate *super;
     unsigned char chr;
     void *data;
#endif
{
  int offset = chr / RX_subset_bits;
  struct rx_distinct_future *df = data;

  if (!df)			/* must be the shared_cache_miss_frame */
    {
      /* Perhaps this is just a transition waiting to be filled. */
      struct rx_super_edge *tc;
      RX_subset mask = rx_subset_singletons [chr % RX_subset_bits];

      for (tc = super->edges; tc; tc = tc->next)
	if (tc->cset[offset] & mask)
	  {
	    struct rx_inx * answer;
	    df = tc->options;
	    answer = ((tc->options->next_same_super_edge[0] != tc->options)
		      ? &tc->rx_backtrack_frame
		      : (df->effects
			 ? &df->side_effects_frame
			 : &df->future_frame));
	    install_partial_transition (super, answer,
					tc->cset [offset], offset * 32);
	    return answer;
	  }
      /* Otherwise, it's a flushed or  newly encountered edge. */
      {
	char cset_space[1024];	/* this limit is far from unreasonable */
	rx_Bitset trcset;
	struct rx_inx *answer;

	if (rx_sizeof_bitset (rx->local_cset_size) > sizeof (cset_space))
	  return 0;		/* If the arbitrary limit is hit, always fail */
				/* cleanly. */
	trcset = (rx_Bitset)cset_space;
	rx_lock_superstate (rx, super);
	if (!compute_super_edge (rx, &df, trcset, super, chr))
	  {
	    rx_unlock_superstate (rx, super);
	    return 0;
	  }
	if (!df)		/* We just computed the fail transition. */
	  {
	    static struct rx_inx
	      shared_fail_frame = { (void *)rx_backtrack, 0, 0 };
	    answer = &shared_fail_frame;
	  }
	else
	  {
	    tc = rx_super_edge (rx, super, trcset, df);
	    if (!tc)
	      {
		rx_unlock_superstate (rx, super);
		return 0;
	      }
	    answer = ((tc->options->next_same_super_edge[0] != tc->options)
		      ? &tc->rx_backtrack_frame
		      : (df->effects
			 ? &df->side_effects_frame
			 : &df->future_frame));
	  }
	install_partial_transition (super, answer,
				    trcset[offset], offset * 32);
	rx_unlock_superstate (rx, super);
	return answer;
      }
    }
  else if (df->future) /* A cache miss on an edge with a future? Must be
			* a semi-free destination. */
    {				
      if (df->future->is_semifree)
	refresh_semifree_superstate (rx->cache, df->future);
      return &df->future_frame;
    }
  else
    /* no future superstate on an existing edge */
    {
      rx_lock_superstate (rx, super);
      if (!solve_destination (rx, df))
	{
	  rx_unlock_superstate (rx, super);
	  return 0;
	}
      if (!df->effects
	  && (df->edge->options->next_same_super_edge[0] == df->edge->options))
	install_partial_transition (super, &df->future_frame,
				    df->edge->cset[offset], offset * 32);
      rx_unlock_superstate (rx, super);
      return &df->future_frame;
    }
}




/* The rest of the code provides a regex.c compatable interface. */


const char *re_error_msg[] =
{
  0,						/* REG_NOUT */
  "No match",					/* REG_NOMATCH */
  "Invalid regular expression",			/* REG_BADPAT */
  "Invalid collation character",		/* REG_ECOLLATE */
  "Invalid character class name",		/* REG_ECTYPE */
  "Trailing backslash",				/* REG_EESCAPE */
  "Invalid back reference",			/* REG_ESUBREG */
  "Unmatched [ or [^",				/* REG_EBRACK */
  "Unmatched ( or \\(",				/* REG_EPAREN */
  "Unmatched \\{",				/* REG_EBRACE */
  "Invalid content of \\{\\}",			/* REG_BADBR */
  "Invalid range end",				/* REG_ERANGE */
  "Memory exhausted",				/* REG_ESPACE */
  "Invalid preceding regular expression",	/* REG_BADRPT */
  "Premature end of regular expression",	/* REG_EEND */
  "Regular expression too big",			/* REG_ESIZE */
  "Unmatched ) or \\)",				/* REG_ERPAREN */
};



/* 
 * Macros used while compiling patterns.
 *
 * By convention, PEND points just past the end of the uncompiled pattern,
 * P points to the read position in the pattern.  `translate' is the name
 * of the translation table (`TRANSLATE' is the name of a macro that looks
 * things up in `translate').
 */


/*
 * Fetch the next character in the uncompiled pattern---translating it 
 * if necessary. *Also cast from a signed character in the constant
 * string passed to us by the user to an unsigned char that we can use
 * as an array index (in, e.g., `translate').
 */
#define PATFETCH(c)							\
 do {if (p == pend) return REG_EEND;					\
    c = (unsigned char) *p++;						\
    c = translate[c];		 					\
 } while (0)

/* 
 * Fetch the next character in the uncompiled pattern, with no
 * translation.
 */
#define PATFETCH_RAW(c)							\
  do {if (p == pend) return REG_EEND;					\
    c = (unsigned char) *p++; 						\
  } while (0)

/* Go backwards one character in the pattern.  */
#define PATUNFETCH p--


#define TRANSLATE(d) translate[(unsigned char) (d)]

typedef unsigned regnum_t;

/* Since offsets can go either forwards or backwards, this type needs to
 * be able to hold values from -(MAX_BUF_SIZE - 1) to MAX_BUF_SIZE - 1.
 */
typedef int pattern_offset_t;

typedef struct
{
  struct rexp_node ** top_expression; /* was begalt */
  struct rexp_node ** last_expression; /* was laststart */
  pattern_offset_t inner_group_offset;
  regnum_t regnum;
} compile_stack_elt_t;

typedef struct
{
  compile_stack_elt_t *stack;
  unsigned size;
  unsigned avail;			/* Offset of next open position.  */
} compile_stack_type;


#define INIT_COMPILE_STACK_SIZE 32

#define COMPILE_STACK_EMPTY  (compile_stack.avail == 0)
#define COMPILE_STACK_FULL  (compile_stack.avail == compile_stack.size)

/* The next available element.  */
#define COMPILE_STACK_TOP (compile_stack.stack[compile_stack.avail])


/* Set the bit for character C in a list.  */
#define SET_LIST_BIT(c)                               \
  (b[((unsigned char) (c)) / BYTEWIDTH]               \
   |= 1 << (((unsigned char) c) % BYTEWIDTH))

/* Get the next unsigned number in the uncompiled pattern.  */
#define GET_UNSIGNED_NUMBER(num) 					\
  { if (p != pend)							\
     {									\
       PATFETCH (c); 							\
       while (isdigit (c)) 						\
         { 								\
           if (num < 0)							\
              num = 0;							\
           num = num * 10 + c - '0'; 					\
           if (p == pend) 						\
              break; 							\
           PATFETCH (c);						\
         } 								\
       } 								\
    }		

#define CHAR_CLASS_MAX_LENGTH  6 /* Namely, `xdigit'.  */

#define IS_CHAR_CLASS(string)						\
   (!strcmp (string, "alpha") || !strcmp (string, "upper")		\
    || !strcmp (string, "lower") || !strcmp (string, "digit")		\
    || !strcmp (string, "alnum") || !strcmp (string, "xdigit")		\
    || !strcmp (string, "space") || !strcmp (string, "print")		\
    || !strcmp (string, "punct") || !strcmp (string, "graph")		\
    || !strcmp (string, "cntrl") || !strcmp (string, "blank"))


/* These predicates are used in regex_compile. */

/* P points to just after a ^ in PATTERN.  Return true if that ^ comes
 * after an alternative or a begin-subexpression.  We assume there is at
 * least one character before the ^.  
 */

#ifdef __STDC__
static boolean
at_begline_loc_p (const char *pattern, const char * p, reg_syntax_t syntax)
#else
static boolean
at_begline_loc_p (pattern, p, syntax)
     const char *pattern;
     const char * p;
     reg_syntax_t syntax;
#endif
{
  const char *prev = p - 2;
  boolean prev_prev_backslash = ((prev > pattern) && (prev[-1] == '\\'));
  
    return
      
      (/* After a subexpression?  */
       ((*prev == '(') && ((syntax & RE_NO_BK_PARENS) || prev_prev_backslash))
       ||
       /* After an alternative?  */
       ((*prev == '|') && ((syntax & RE_NO_BK_VBAR) || prev_prev_backslash))
       );
}

/* The dual of at_begline_loc_p.  This one is for $.  We assume there is
 * at least one character after the $, i.e., `P < PEND'.
 */

#ifdef __STDC__
static boolean
at_endline_loc_p (const char *p, const char *pend, int syntax)
#else
static boolean
at_endline_loc_p (p, pend, syntax)
     const char *p;
     const char *pend;
     int syntax;
#endif
{
  const char *next = p;
  boolean next_backslash = (*next == '\\');
  const char *next_next = (p + 1 < pend) ? (p + 1) : 0;
  
  return
    (
     /* Before a subexpression?  */
     ((syntax & RE_NO_BK_PARENS)
      ? (*next == ')')
      : (next_backslash && next_next && (*next_next == ')')))
    ||
     /* Before an alternative?  */
     ((syntax & RE_NO_BK_VBAR)
      ? (*next == '|')
      : (next_backslash && next_next && (*next_next == '|')))
     );
}


static unsigned char id_translation[256] =
{
  0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
 40, 41, 42, 43, 44, 45, 46, 47, 48, 49,
 50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
 60, 61, 62, 63, 64, 65, 66, 67, 68, 69,
 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
 80, 81, 82, 83, 84, 85, 86, 87, 88, 89,
 90, 91, 92, 93, 94, 95, 96, 97, 98, 99,

 100, 101, 102, 103, 104, 105, 106, 107, 108, 109,
 110, 111, 112, 113, 114, 115, 116, 117, 118, 119,
 120, 121, 122, 123, 124, 125, 126, 127, 128, 129,
 130, 131, 132, 133, 134, 135, 136, 137, 138, 139,
 140, 141, 142, 143, 144, 145, 146, 147, 148, 149,
 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
 160, 161, 162, 163, 164, 165, 166, 167, 168, 169,
 170, 171, 172, 173, 174, 175, 176, 177, 178, 179,
 180, 181, 182, 183, 184, 185, 186, 187, 188, 189,
 190, 191, 192, 193, 194, 195, 196, 197, 198, 199,

 200, 201, 202, 203, 204, 205, 206, 207, 208, 209,
 210, 211, 212, 213, 214, 215, 216, 217, 218, 219,
 220, 221, 222, 223, 224, 225, 226, 227, 228, 229,
 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
 240, 241, 242, 243, 244, 245, 246, 247, 248, 249,
 250, 251, 252, 253, 254, 255
};

/* The compiler keeps an inverted translation table.
 * This looks up/inititalize elements.
 * VALID is an array of booleans that validate CACHE.
 */

#ifdef __STDC__
static rx_Bitset
inverse_translation (struct re_pattern_buffer * rxb,
		     char * valid, rx_Bitset cache,
		     unsigned char * translate, int c)
#else
static rx_Bitset
inverse_translation (rxb, valid, cache, translate, c)
     struct re_pattern_buffer * rxb;
     char * valid;
     rx_Bitset cache;
     unsigned char * translate;
     int c;
#endif
{
  rx_Bitset cs
    = cache + c * rx_bitset_numb_subsets (rxb->rx.local_cset_size); 

  if (!valid[c])
    {
      int x;
      int c_tr = TRANSLATE(c);
      rx_bitset_null (rxb->rx.local_cset_size, cs);
      for (x = 0; x < 256; ++x)	/* &&&& 13.37 */
	if (TRANSLATE(x) == c_tr)
	  RX_bitset_enjoin (cs, x);
      valid[c] = 1;
    }
  return cs;
}




/* More subroutine declarations and macros for regex_compile.  */

/* Returns true if REGNUM is in one of COMPILE_STACK's elements and 
   false if it's not.  */

#ifdef __STDC__
static boolean
group_in_compile_stack (compile_stack_type compile_stack, regnum_t regnum)
#else
static boolean
group_in_compile_stack (compile_stack, regnum)
    compile_stack_type compile_stack;
    regnum_t regnum;
#endif
{
  int this_element;

  for (this_element = compile_stack.avail - 1;  
       this_element >= 0; 
       this_element--)
    if (compile_stack.stack[this_element].regnum == regnum)
      return true;

  return false;
}


/*
 * Read the ending character of a range (in a bracket expression) from the
 * uncompiled pattern *P_PTR (which ends at PEND).  We assume the
 * starting character is in `P[-2]'.  (`P[-1]' is the character `-'.)
 * Then we set the translation of all bits between the starting and
 * ending characters (inclusive) in the compiled pattern B.
 * 
 * Return an error code.
 * 
 * We use these short variable names so we can use the same macros as
 * `regex_compile' itself.  
 */

#ifdef __STDC__
static reg_errcode_t
compile_range (struct re_pattern_buffer * rxb, rx_Bitset cs,
	       const char ** p_ptr, const char * pend,
	       unsigned char * translate, reg_syntax_t syntax,
	       rx_Bitset inv_tr,  char * valid_inv_tr)
#else
static reg_errcode_t
compile_range (rxb, cs, p_ptr, pend, translate, syntax, inv_tr, valid_inv_tr)
     struct re_pattern_buffer * rxb;
     rx_Bitset cs;
     const char ** p_ptr;
     const char * pend;
     unsigned char * translate;
     reg_syntax_t syntax;
     rx_Bitset inv_tr;
     char * valid_inv_tr;
#endif
{
  unsigned this_char;

  const char *p = *p_ptr;

  unsigned char range_end;
  unsigned char range_start = TRANSLATE(p[-2]);

  if (p == pend)
    return REG_ERANGE;

  PATFETCH (range_end);

  (*p_ptr)++;

  if (range_start > range_end)
    return syntax & RE_NO_EMPTY_RANGES ? REG_ERANGE : REG_NOERROR;

  for (this_char = range_start; this_char <= range_end; this_char++)
    {
      rx_Bitset it =
	inverse_translation (rxb, valid_inv_tr, inv_tr, translate, this_char);
      rx_bitset_union (rxb->rx.local_cset_size, cs, it);
    }
  
  return REG_NOERROR;
}


/* This searches a regexp for backreference side effects.
 * It fills in the array OUT with 1 at the index of every register pair
 * referenced by a backreference.
 *
 * This is used to help optimize patterns for searching.  The information is
 * useful because, if the caller doesn't want register values, backreferenced
 * registers are the only registers for which we need rx_backtrack.
 */

#ifdef __STDC__
static void
find_backrefs (char * out, struct rexp_node * rexp,
	       struct re_se_params * params)
#else
static void
find_backrefs (out, rexp, params)
     char * out;
     struct rexp_node * rexp;
     struct re_se_params * params;
#endif
{
  if (rexp)
    switch (rexp->type)
      {
      case r_cset:
      case r_data:
	return;
      case r_alternate:
      case r_concat:
      case r_opt:
      case r_star:
      case r_2phase_star:
	find_backrefs (out, rexp->params.pair.left, params);
	find_backrefs (out, rexp->params.pair.right, params);
	return;
      case r_side_effect:
	if (   ((int)rexp->params.side_effect >= 0)
	    && (params [(int)rexp->params.side_effect].se == re_se_backref))
	  out[ params [(int)rexp->params.side_effect].op1] = 1;
	return;
      }
}



/* Returns 0 unless the pattern can match the empty string. */

#ifdef __STDC__
static int
compute_fastset (struct re_pattern_buffer * rxb, struct rexp_node * rexp)
#else
static int
compute_fastset (rxb, rexp)
     struct re_pattern_buffer * rxb;
     struct rexp_node * rexp;
#endif
{
  if (!rexp)
    return 1;
  switch (rexp->type)
    {
    case r_data:
      return 1;
    case r_cset:
      {
	rx_bitset_union (rxb->rx.local_cset_size,
			 rxb->fastset, rexp->params.cset);
      }
      return 0;
    case r_concat:
      return (compute_fastset (rxb, rexp->params.pair.left)
	      && compute_fastset (rxb, rexp->params.pair.right));
    case r_2phase_star:
      compute_fastset (rxb, rexp->params.pair.left);
      /* compute_fastset (rxb, rexp->params.pair.right);  nope... */
      return 1;
    case r_alternate:
      return !!(compute_fastset (rxb, rexp->params.pair.left)
		+ compute_fastset (rxb, rexp->params.pair.right));
    case r_opt:
    case r_star:
      compute_fastset (rxb, rexp->params.pair.left);
      return 1;
    case r_side_effect:
      return 1;
    }

  /* this should never happen */
  return 0;
}


/* returns
 *  1 -- yes, definately anchored by the given side effect.
 *  2 -- maybe anchored, maybe the empty string.
 *  0 -- definately not anchored
 *  There is simply no other possibility.
 */

#ifdef __STDC__
static int
is_anchored (struct rexp_node * rexp, rx_side_effect se)
#else
static int
is_anchored (rexp, se)
     struct rexp_node * rexp;
     rx_side_effect se;
#endif
{
  if (!rexp)
    return 2;
  switch (rexp->type)
    {
    case r_cset:
    case r_data:
      return 0;
    case r_concat:
    case r_2phase_star:
      {
	int l = is_anchored (rexp->params.pair.left, se);
	return (l == 2 ? is_anchored (rexp->params.pair.right, se) : l);
      }
    case r_alternate:
      {
	int l = is_anchored (rexp->params.pair.left, se);
	int r = l ? is_anchored (rexp->params.pair.right, se) : 0;
	return MAX (l, r);
      }
    case r_opt:
    case r_star:
      return is_anchored (rexp->params.pair.left, se) ? 2 : 0;
      
    case r_side_effect:
      return ((rexp->params.side_effect == se)
	      ? 1 : 2);
    }

  /* this should never happen */
  return 0;
}


/* This removes register assignments that aren't required by backreferencing.
 * This can speed up explore_future, especially if it eliminates
 * non-determinism in the superstate NFA.
 * 
 * NEEDED is an array of characters, presumably filled in by FIND_BACKREFS.
 * The non-zero elements of the array indicate which register assignments
 * can NOT be removed from the expression.
 */

#ifdef __STDC__
static struct rexp_node *
remove_unecessary_side_effects (struct rx * rx, char * needed,
				struct rexp_node * rexp,
				struct re_se_params * params)
#else
static struct rexp_node *
remove_unecessary_side_effects (rx, needed, rexp, params)
     struct rx * rx;
     char * needed;
     struct rexp_node * rexp;
     struct re_se_params * params;
#endif
{
  struct rexp_node * l;
  struct rexp_node * r;
  if (!rexp)
    return 0;
  else
    switch (rexp->type)
      {
      case r_cset:
      case r_data:
	return rexp;
      case r_alternate:
      case r_concat:
      case r_2phase_star:
	l = remove_unecessary_side_effects (rx, needed,
					    rexp->params.pair.left, params);
	r = remove_unecessary_side_effects (rx, needed,
					    rexp->params.pair.right, params);
	if ((l && r) || (rexp->type != r_concat))
	  {
	    rexp->params.pair.left = l;
	    rexp->params.pair.right = r;
	    return rexp;
	  }
	else
	  {
	    rexp->params.pair.left = rexp->params.pair.right = 0;
	    rx_free_rexp (rx, rexp);
	    return l ? l : r;
	  }
      case r_opt:
      case r_star:
	l = remove_unecessary_side_effects (rx, needed,
					    rexp->params.pair.left, params);
	if (l)
	  {
	    rexp->params.pair.left = l;
	    return rexp;
	  }
	else
	  {
	    rexp->params.pair.left = 0;
	    rx_free_rexp (rx, rexp);
	    return 0;
	  }
      case r_side_effect:
	{
	  int se = (int)rexp->params.side_effect;
	  if (   (se >= 0)
	      && (   ((enum re_side_effects)params[se].se == re_se_lparen)
		  || ((enum re_side_effects)params[se].se == re_se_rparen))
	      && (params [se].op1 > 0)
	      && (!needed [params [se].op1]))
	    {
	      rx_free_rexp (rx, rexp);
	      return 0;
	    }
	  else
	    return rexp;
	}
      }

  /* this should never happen */
  return 0;
}



#ifdef __STDC__
static int
pointless_if_repeated (struct rexp_node * node, struct re_se_params * params)
#else
static int
pointless_if_repeated (node, params)
     struct rexp_node * node;
     struct re_se_params * params;
#endif
{
  if (!node)
    return 1;
  switch (node->type)
    {
    case r_cset:
      return 0;
    case r_alternate:
    case r_concat:
    case r_2phase_star:
      return (pointless_if_repeated (node->params.pair.left, params)
	      && pointless_if_repeated (node->params.pair.right, params));
    case r_opt:
    case r_star:
      return pointless_if_repeated (node->params.pair.left, params);
    case r_side_effect:
      switch (((int)node->params.side_effect < 0)
	      ? (enum re_side_effects)node->params.side_effect
	      : (enum re_side_effects)params[(int)node->params.side_effect].se)
	{
	case re_se_try:
	case re_se_at_dot:
	case re_se_begbuf:
	case re_se_hat:
	case re_se_wordbeg:
	case re_se_wordbound:
	case re_se_notwordbound:
	case re_se_wordend:
	case re_se_endbuf:
	case re_se_dollar:
	case re_se_fail:
	case re_se_win:
	  return 1;
	case re_se_lparen:
	case re_se_rparen:
	case re_se_iter:
	case re_se_end_iter:
	case re_se_syntax:
	case re_se_not_syntax:
	case re_se_backref:
	  return 0;
	}
    case r_data:
    default:
      return 0;
    }
}



#ifdef __STDC__
static int
registers_on_stack (struct re_pattern_buffer * rxb,
		    struct rexp_node * rexp, int in_danger,
		    struct re_se_params * params)
#else
static int
registers_on_stack (rxb, rexp, in_danger, params)
     struct re_pattern_buffer * rxb;
     struct rexp_node * rexp;
     int in_danger;
     struct re_se_params * params;
#endif
{
  if (!rexp)
    return 0;
  else
    switch (rexp->type)
      {
      case r_cset:
      case r_data:
	return 0;
      case r_alternate:
      case r_concat:
	return (   registers_on_stack (rxb, rexp->params.pair.left,
				       in_danger, params)
		|| (registers_on_stack
		    (rxb, rexp->params.pair.right,
		     in_danger, params)));
      case r_opt:
	return registers_on_stack (rxb, rexp->params.pair.left, 0, params);
      case r_star:
	return registers_on_stack (rxb, rexp->params.pair.left, 1, params);
      case r_2phase_star:
	return
	  (   registers_on_stack (rxb, rexp->params.pair.left, 1, params)
	   || registers_on_stack (rxb, rexp->params.pair.right, 1, params));
      case r_side_effect:
	{
	  int se = (int)rexp->params.side_effect;
	  if (   in_danger
	      && (se >= 0)
	      && (params [se].op1 > 0)
	      && (   ((enum re_side_effects)params[se].se == re_se_lparen)
		  || ((enum re_side_effects)params[se].se == re_se_rparen)))
	    return 1;
	  else
	    return 0;
	}
      }

  /* this should never happen */
  return 0;
}



static char idempotent_complex_se[] =
{
#define RX_WANT_SE_DEFS 1
#undef RX_DEF_SE
#undef RX_DEF_CPLX_SE
#define RX_DEF_SE(IDEM, NAME, VALUE)	      
#define RX_DEF_CPLX_SE(IDEM, NAME, VALUE)     IDEM,
#include "rx.h"
#undef RX_DEF_SE
#undef RX_DEF_CPLX_SE
#undef RX_WANT_SE_DEFS
  23
};

static char idempotent_se[] =
{
  13,
#define RX_WANT_SE_DEFS 1
#undef RX_DEF_SE
#undef RX_DEF_CPLX_SE
#define RX_DEF_SE(IDEM, NAME, VALUE)	      IDEM,
#define RX_DEF_CPLX_SE(IDEM, NAME, VALUE)     
#include "rx.h"
#undef RX_DEF_SE
#undef RX_DEF_CPLX_SE
#undef RX_WANT_SE_DEFS
  42
};




#ifdef __STDC__
static int
has_any_se (struct rx * rx,
	    struct rexp_node * rexp)
#else
static int
has_any_se (rx, rexp)
     struct rx * rx;
     struct rexp_node * rexp;
#endif
{
  if (!rexp)
    return 0;

  switch (rexp->type)
    {
    case r_cset:
    case r_data:
      return 0;

    case r_side_effect:
      return 1;
      
    case r_2phase_star:
    case r_concat:
    case r_alternate:
      return
	(   has_any_se (rx, rexp->params.pair.left)
	 || has_any_se (rx, rexp->params.pair.right));

    case r_opt:
    case r_star:
      return has_any_se (rx, rexp->params.pair.left);
    }

  /* this should never happen */
  return 0;
}



/* This must be called AFTER `convert_hard_loops' for a given REXP. */
#ifdef __STDC__
static int
has_non_idempotent_epsilon_path (struct rx * rx,
				 struct rexp_node * rexp,
				 struct re_se_params * params)
#else
static int
has_non_idempotent_epsilon_path (rx, rexp, params)
     struct rx * rx;
     struct rexp_node * rexp;
     struct re_se_params * params;
#endif
{
  if (!rexp)
    return 0;

  switch (rexp->type)
    {
    case r_cset:
    case r_data:
    case r_star:
      return 0;

    case r_side_effect:
      return
	!((int)rexp->params.side_effect > 0
	  ? idempotent_complex_se [ params [(int)rexp->params.side_effect].se ]
	  : idempotent_se [-(int)rexp->params.side_effect]);
      
    case r_alternate:
      return
	(   has_non_idempotent_epsilon_path (rx,
					     rexp->params.pair.left, params)
	 || has_non_idempotent_epsilon_path (rx,
					     rexp->params.pair.right, params));

    case r_2phase_star:
    case r_concat:
      return
	(   has_non_idempotent_epsilon_path (rx,
					     rexp->params.pair.left, params)
	 && has_non_idempotent_epsilon_path (rx,
					     rexp->params.pair.right, params));

    case r_opt:
      return has_non_idempotent_epsilon_path (rx,
					      rexp->params.pair.left, params);
    }

  /* this should never happen */
  return 0;
}



/* This computes rougly what it's name suggests.   It can (and does) go wrong 
 * in the direction of returning spurious 0 without causing disasters.
 */
#ifdef __STDC__
static int
begins_with_complex_se (struct rx * rx, struct rexp_node * rexp)
#else
static int
begins_with_complex_se (rx, rexp)
     struct rx * rx;
     struct rexp_node * rexp;
#endif
{
  if (!rexp)
    return 0;

  switch (rexp->type)
    {
    case r_cset:
    case r_data:
      return 0;

    case r_side_effect:
      return ((int)rexp->params.side_effect >= 0);
      
    case r_alternate:
      return
	(   begins_with_complex_se (rx, rexp->params.pair.left)
	 && begins_with_complex_se (rx, rexp->params.pair.right));


    case r_concat:
      return has_any_se (rx, rexp->params.pair.left);
    case r_opt:
    case r_star:
    case r_2phase_star:
      return 0;
    }

  /* this should never happen */
  return 0;
}


/* This destructively removes some of the re_se_tv side effects from 
 * a rexp tree.  In particular, during parsing re_se_tv was inserted on the
 * right half of every | to guarantee that posix path preference could be 
 * honored.  This function removes some which it can be determined aren't 
 * needed.  
 */

#ifdef __STDC__
static void
speed_up_alt (struct rx * rx,
	      struct rexp_node * rexp,
	      int unposix)
#else
static void
speed_up_alt (rx, rexp, unposix)
     struct rx * rx;
     struct rexp_node * rexp;
     int unposix;
#endif
{
  if (!rexp)
    return;

  switch (rexp->type)
    {
    case r_cset:
    case r_data:
    case r_side_effect:
      return;

    case r_opt:
    case r_star:
      speed_up_alt (rx, rexp->params.pair.left, unposix);
      return;

    case r_2phase_star:
    case r_concat:
      speed_up_alt (rx, rexp->params.pair.left, unposix);
      speed_up_alt (rx, rexp->params.pair.right, unposix);
      return;

    case r_alternate:
      /* the right child is guaranteed to be (concat re_se_tv <subexp>) */

      speed_up_alt (rx, rexp->params.pair.left, unposix);
      speed_up_alt (rx, rexp->params.pair.right->params.pair.right, unposix);
      
      if (   unposix
	  || (begins_with_complex_se
	      (rx, rexp->params.pair.right->params.pair.right))
	  || !(   has_any_se (rx, rexp->params.pair.right->params.pair.right)
	       || has_any_se (rx, rexp->params.pair.left)))
	{
	  struct rexp_node * conc = rexp->params.pair.right;
	  rexp->params.pair.right = conc->params.pair.right;
	  conc->params.pair.right = 0;
	  rx_free_rexp (rx, conc);
	}
    }
}





/* `regex_compile' compiles PATTERN (of length SIZE) according to SYNTAX.
   Returns one of error codes defined in `regex.h', or zero for success.

   Assumes the `allocated' (and perhaps `buffer') and `translate'
   fields are set in BUFP on entry.

   If it succeeds, results are put in BUFP (if it returns an error, the
   contents of BUFP are undefined):
     `buffer' is the compiled pattern;
     `syntax' is set to SYNTAX;
     `used' is set to the length of the compiled pattern;
     `fastmap_accurate' is set to zero;
     `re_nsub' is set to the number of groups in PATTERN;
     `not_bol' and `not_eol' are set to zero.
   
   The `fastmap' and `newline_anchor' fields are neither
   examined nor set.  */



#ifdef __STDC__
reg_errcode_t
rx_compile (const char *pattern, int size,
	    reg_syntax_t syntax,
	    struct re_pattern_buffer * rxb) 
#else
reg_errcode_t
rx_compile (pattern, size, syntax, rxb)
     const char *pattern;
     int size;
     reg_syntax_t syntax;
     struct re_pattern_buffer * rxb;
#endif
{
  RX_subset
    inverse_translate [CHAR_SET_SIZE * rx_bitset_numb_subsets(CHAR_SET_SIZE)];
  char
    validate_inv_tr [CHAR_SET_SIZE * rx_bitset_numb_subsets(CHAR_SET_SIZE)];

  /* We fetch characters from PATTERN here.  Even though PATTERN is
     `char *' (i.e., signed), we declare these variables as unsigned, so
     they can be reliably used as array indices.  */
  register unsigned char c, c1;
  
  /* A random tempory spot in PATTERN.  */
  const char *p1;
  
  /* Keeps track of unclosed groups.  */
  compile_stack_type compile_stack;

  /* Points to the current (ending) position in the pattern.  */
  const char *p = pattern;
  const char *pend = pattern + size;
  
  /* How to translate the characters in the pattern.  */
  unsigned char *translate = rxb->translate ? rxb->translate : (char *)id_translation;

  /* When parsing is done, this will hold the expression tree. */
  struct rexp_node * rexp = 0;

  /* In the midst of compilation, this holds onto the regexp 
   * first parst while rexp goes on to aquire additional constructs.
   */
  struct rexp_node * orig_rexp = 0;
  struct rexp_node * fewer_side_effects = 0;

  /* This and top_expression are saved on the compile stack. */
  struct rexp_node ** top_expression = &rexp;
  struct rexp_node ** last_expression = top_expression;
  
  /* Parameter to `goto append_node' */
  struct rexp_node * append;

  /* Counts open-groups as they are encountered.  This is the index of the
   * innermost group being compiled.
   */
  regnum_t regnum = 0;

  /* Place in the uncompiled pattern (i.e., the {) to
   * which to go back if the interval is invalid.  
   */
  const char *beg_interval;

  struct re_se_params * params = 0;
  int paramc = 0;		/* How many complex side effects so far? */

  rx_side_effect side;		/* param to `goto add_side_effect' */

  bzero (validate_inv_tr, sizeof (validate_inv_tr));

  rxb->rx.instruction_table = rx_id_instruction_table;


  /* Initialize the compile stack.  */
  compile_stack.stack = TALLOC (INIT_COMPILE_STACK_SIZE, compile_stack_elt_t);
  if (compile_stack.stack == 0)
    return REG_ESPACE;

  compile_stack.size = INIT_COMPILE_STACK_SIZE;
  compile_stack.avail = 0;

  /* Initialize the pattern buffer.  */
  rxb->rx.cache = &default_cache;
  rxb->syntax = syntax;
  rxb->fastmap_accurate = 0;
  rxb->not_bol = rxb->not_eol = 0;
  rxb->least_subs = 0;
  
  /* Always count groups, whether or not rxb->no_sub is set.  
   * The whole pattern is implicitly group 0, so counting begins
   * with 1.
   */
  rxb->re_nsub = 0;

#if !defined (emacs) && !defined (SYNTAX_TABLE)
  /* Initialize the syntax table.  */
   init_syntax_once ();
#endif

  /* Loop through the uncompiled pattern until we're at the end.  */
  while (p != pend)
    {
      PATFETCH (c);

      switch (c)
        {
        case '^':
          {
            if (   /* If at start of pattern, it's an operator.  */
                   p == pattern + 1
                   /* If context independent, it's an operator.  */
                || syntax & RE_CONTEXT_INDEP_ANCHORS
                   /* Otherwise, depends on what's come before.  */
                || at_begline_loc_p (pattern, p, syntax))
	      {
		struct rexp_node * n
		  = rx_mk_r_side_effect (&rxb->rx, (rx_side_effect)re_se_hat);
		if (!n)
		  return REG_ESPACE;
		append = n;
		goto append_node;
	      }
            else
              goto normal_char;
          }
          break;


        case '$':
          {
            if (   /* If at end of pattern, it's an operator.  */
                   p == pend 
                   /* If context independent, it's an operator.  */
                || syntax & RE_CONTEXT_INDEP_ANCHORS
                   /* Otherwise, depends on what's next.  */
                || at_endline_loc_p (p, pend, syntax))
	      {
		struct rexp_node * n
		  = rx_mk_r_side_effect (&rxb->rx, (rx_side_effect)re_se_dollar);
		if (!n)
		  return REG_ESPACE;
		append = n;
		goto append_node;
	      }
             else
               goto normal_char;
           }
           break;


	case '+':
        case '?':
          if ((syntax & RE_BK_PLUS_QM)
              || (syntax & RE_LIMITED_OPS))
            goto normal_char;

        handle_plus:
        case '*':
          /* If there is no previous pattern... */
          if (pointless_if_repeated (*last_expression, params))
            {
              if (syntax & RE_CONTEXT_INVALID_OPS)
                return REG_BADRPT;
              else if (!(syntax & RE_CONTEXT_INDEP_OPS))
                goto normal_char;
            }

          {
            /* 1 means zero (many) matches is allowed.  */
            char zero_times_ok = 0, many_times_ok = 0;

            /* If there is a sequence of repetition chars, collapse it
               down to just one (the right one).  We can't combine
               interval operators with these because of, e.g., `a{2}*',
               which should only match an even number of `a's.  */

            for (;;)
              {
                zero_times_ok |= c != '+';
                many_times_ok |= c != '?';

                if (p == pend)
                  break;

                PATFETCH (c);

                if (c == '*'
                    || (!(syntax & RE_BK_PLUS_QM) && (c == '+' || c == '?')))
                  ;

                else if (syntax & RE_BK_PLUS_QM  &&  c == '\\')
                  {
                    if (p == pend) return REG_EESCAPE;

                    PATFETCH (c1);
                    if (!(c1 == '+' || c1 == '?'))
                      {
                        PATUNFETCH;
                        PATUNFETCH;
                        break;
                      }

                    c = c1;
                  }
                else
                  {
                    PATUNFETCH;
                    break;
                  }

                /* If we get here, we found another repeat character.  */
               }

            /* Star, etc. applied to an empty pattern is equivalent
               to an empty pattern.  */
            if (!last_expression)
              break;

	    /* Now we know whether or not zero matches is allowed
	     * and also whether or not two or more matches is allowed.
	     */

	    {
	      struct rexp_node * inner_exp = *last_expression;
	      int need_sync = 0;

	      if (many_times_ok
		  && has_non_idempotent_epsilon_path (&rxb->rx,
						      inner_exp, params))
		{
		  struct rexp_node * pusher
		    = rx_mk_r_side_effect (&rxb->rx,
					   (rx_side_effect)re_se_pushpos);
		  struct rexp_node * checker
		    = rx_mk_r_side_effect (&rxb->rx,
					   (rx_side_effect)re_se_chkpos);
		  struct rexp_node * pushback
		    = rx_mk_r_side_effect (&rxb->rx,
					   (rx_side_effect)re_se_pushback);
		  rx_Bitset cs = rx_cset (&rxb->rx);
		  struct rexp_node * lit_t = rx_mk_r_cset (&rxb->rx, cs);
		  struct rexp_node * fake_state
		    = rx_mk_r_concat (&rxb->rx, pushback, lit_t);
		  struct rexp_node * phase2
		    = rx_mk_r_concat (&rxb->rx, checker, fake_state);
		  struct rexp_node * popper
		    = rx_mk_r_side_effect (&rxb->rx,
					   (rx_side_effect)re_se_poppos);
		  struct rexp_node * star
		    = rx_mk_r_2phase_star (&rxb->rx, inner_exp, phase2);
		  struct rexp_node * a
		    = rx_mk_r_concat (&rxb->rx, pusher, star);
		  struct rexp_node * whole_thing
		    = rx_mk_r_concat (&rxb->rx, a, popper);
		  if (!(pusher && star && pushback && lit_t && fake_state
			&& lit_t && phase2 && checker && popper
			&& a && whole_thing))
		    return REG_ESPACE;
		  RX_bitset_enjoin (cs, 't');
		  *last_expression = whole_thing;
		}
	      else
		{
		  struct rexp_node * star =
		    (many_times_ok ? rx_mk_r_star : rx_mk_r_opt)
		      (&rxb->rx, *last_expression);
		  if (!star)
		    return REG_ESPACE;
		  *last_expression = star;
		  need_sync = has_any_se (&rxb->rx, *last_expression);
		}
	      if (!zero_times_ok)
		{
		  struct rexp_node * concat
		    = rx_mk_r_concat (&rxb->rx, inner_exp,
				      rx_copy_rexp (&rxb->rx,
						    *last_expression));
		  if (!concat)
		    return REG_ESPACE;
		  *last_expression = concat;
		}
	      if (need_sync)
		{
		  int sync_se = paramc;
		  params = (params
			    ? ((struct re_se_params *)
			       realloc (params,
					sizeof (*params) * (1 + paramc)))
			    : ((struct re_se_params *)
			       malloc (sizeof (*params))));
		  if (!params)
		    return REG_ESPACE;
		  ++paramc;
		  params [sync_se].se = re_se_tv;
		  side = (rx_side_effect)sync_se;
		  goto add_side_effect;
		}
	    }
	    /* The old regex.c used to optimize `.*\n'.  
	     * Maybe rx should too?
	     */
	  }
	  break;


	case '.':
	  {
	    rx_Bitset cs = rx_cset (&rxb->rx);
	    struct rexp_node * n = rx_mk_r_cset (&rxb->rx, cs);
	    if (!(cs && n))
	      return REG_ESPACE;

	    rx_bitset_universe (rxb->rx.local_cset_size, cs);
	    if (!(rxb->syntax & RE_DOT_NEWLINE))
	      RX_bitset_remove (cs, '\n');
	    if (!(rxb->syntax & RE_DOT_NOT_NULL))
	      RX_bitset_remove (cs, 0);

	    append = n;
	    goto append_node;
	    break;
	  }


        case '[':
	  if (p == pend) return REG_EBRACK;
          {
            boolean had_char_class = false;
	    rx_Bitset cs = rx_cset (&rxb->rx);
	    struct rexp_node * node = rx_mk_r_cset (&rxb->rx, cs);
	    int is_inverted = *p == '^';
	    
	    if (!(node && cs))
	      return REG_ESPACE;
	    
	    /* This branch of the switch is normally exited with
	     *`goto append_node'
	     */
	    append = node;
	    
            if (is_inverted)
	      p++;
	    
            /* Remember the first position in the bracket expression.  */
            p1 = p;
	    
            /* Read in characters and ranges, setting map bits.  */
            for (;;)
              {
                if (p == pend) return REG_EBRACK;
		
                PATFETCH (c);
		
                /* \ might escape characters inside [...] and [^...].  */
                if ((syntax & RE_BACKSLASH_ESCAPE_IN_LISTS) && c == '\\')
                  {
                    if (p == pend) return REG_EESCAPE;
		    
                    PATFETCH (c1);
		    {
		      rx_Bitset it = inverse_translation (rxb, 
							  validate_inv_tr,
							  inverse_translate,
							  translate,
							  c1);
		      rx_bitset_union (rxb->rx.local_cset_size, cs, it);
		    }
                    continue;
                  }
		
                /* Could be the end of the bracket expression.  If it's
                   not (i.e., when the bracket expression is `[]' so
                   far), the ']' character bit gets set way below.  */
                if (c == ']' && p != p1 + 1)
                  goto finalize_class_and_append;
		
                /* Look ahead to see if it's a range when the last thing
                   was a character class.  */
                if (had_char_class && c == '-' && *p != ']')
                  return REG_ERANGE;
		
                /* Look ahead to see if it's a range when the last thing
                   was a character: if this is a hyphen not at the
                   beginning or the end of a list, then it's the range
                   operator.  */
                if (c == '-' 
                    && !(p - 2 >= pattern && p[-2] == '[') 
                    && !(p - 3 >= pattern && p[-3] == '[' && p[-2] == '^')
                    && *p != ']')
                  {
                    reg_errcode_t ret
                      = compile_range (rxb, cs, &p, pend, translate, syntax,
				       inverse_translate, validate_inv_tr);
                    if (ret != REG_NOERROR) return ret;
                  }
		
                else if (p[0] == '-' && p[1] != ']')
                  { /* This handles ranges made up of characters only.  */
                    reg_errcode_t ret;
		    
		    /* Move past the `-'.  */
                    PATFETCH (c1);
                    
                    ret = compile_range (rxb, cs, &p, pend, translate, syntax,
					 inverse_translate, validate_inv_tr);
                    if (ret != REG_NOERROR) return ret;
                  }
		
                /* See if we're at the beginning of a possible character
                   class.  */
		
		else if ((syntax & RE_CHAR_CLASSES)
			 && (c == '[') && (*p == ':'))
                  {
                    char str[CHAR_CLASS_MAX_LENGTH + 1];
		    
                    PATFETCH (c);
                    c1 = 0;
		    
                    /* If pattern is `[[:'.  */
                    if (p == pend) return REG_EBRACK;
		    
                    for (;;)
                      {
                        PATFETCH (c);
                        if (c == ':' || c == ']' || p == pend
                            || c1 == CHAR_CLASS_MAX_LENGTH)
			  break;
                        str[c1++] = c;
                      }
                    str[c1] = '\0';
		    
                    /* If isn't a word bracketed by `[:' and:`]':
                       undo the ending character, the letters, and leave 
                       the leading `:' and `[' (but set bits for them).  */
                    if (c == ':' && *p == ']')
                      {
                        int ch;
                        boolean is_alnum = !strcmp (str, "alnum");
                        boolean is_alpha = !strcmp (str, "alpha");
                        boolean is_blank = !strcmp (str, "blank");
                        boolean is_cntrl = !strcmp (str, "cntrl");
                        boolean is_digit = !strcmp (str, "digit");
                        boolean is_graph = !strcmp (str, "graph");
                        boolean is_lower = !strcmp (str, "lower");
                        boolean is_print = !strcmp (str, "print");
                        boolean is_punct = !strcmp (str, "punct");
                        boolean is_space = !strcmp (str, "space");
                        boolean is_upper = !strcmp (str, "upper");
                        boolean is_xdigit = !strcmp (str, "xdigit");
                        
                        if (!IS_CHAR_CLASS (str)) return REG_ECTYPE;
			
                        /* Throw away the ] at the end of the character
                           class.  */
                        PATFETCH (c);					
			
                        if (p == pend) return REG_EBRACK;
			
                        for (ch = 0; ch < 1 << BYTEWIDTH; ch++)
                          {
                            if (   (is_alnum  && isalnum (ch))
                                || (is_alpha  && isalpha (ch))
                                || (is_blank  && isblank (ch))
                                || (is_cntrl  && iscntrl (ch))
                                || (is_digit  && isdigit (ch))
                                || (is_graph  && isgraph (ch))
                                || (is_lower  && islower (ch))
                                || (is_print  && isprint (ch))
                                || (is_punct  && ispunct (ch))
                                || (is_space  && isspace (ch))
                                || (is_upper  && isupper (ch))
                                || (is_xdigit && isxdigit (ch)))
			      {
				rx_Bitset it =
				  inverse_translation (rxb, 
						       validate_inv_tr,
						       inverse_translate,
						       translate,
						       ch);
				rx_bitset_union (rxb->rx.local_cset_size,
						 cs, it);
			      }
                          }
                        had_char_class = true;
                      }
                    else
                      {
                        c1++;
                        while (c1--)    
                          PATUNFETCH;
			{
			  rx_Bitset it =
			    inverse_translation (rxb, 
						 validate_inv_tr,
						 inverse_translate,
						 translate,
						 '[');
			  rx_bitset_union (rxb->rx.local_cset_size,
					   cs, it);
			}
			{
			  rx_Bitset it =
			    inverse_translation (rxb, 
						 validate_inv_tr,
						 inverse_translate,
						 translate,
						 ':');
			  rx_bitset_union (rxb->rx.local_cset_size,
					   cs, it);
			}
                        had_char_class = false;
                      }
                  }
                else
                  {
                    had_char_class = false;
		    {
		      rx_Bitset it = inverse_translation (rxb, 
							  validate_inv_tr,
							  inverse_translate,
							  translate,
							  c);
		      rx_bitset_union (rxb->rx.local_cset_size, cs, it);
		    }
                  }
              }

	  finalize_class_and_append:
	    if (is_inverted)
	      {
		rx_bitset_complement (rxb->rx.local_cset_size, cs);
		if (syntax & RE_HAT_LISTS_NOT_NEWLINE)
		  RX_bitset_remove (cs, '\n');
	      }
	    goto append_node;
          }
          break;


	case '(':
          if (syntax & RE_NO_BK_PARENS)
            goto handle_open;
          else
            goto normal_char;


        case ')':
          if (syntax & RE_NO_BK_PARENS)
            goto handle_close;
          else
            goto normal_char;


        case '\n':
          if (syntax & RE_NEWLINE_ALT)
            goto handle_alt;
          else
            goto normal_char;


	case '|':
          if (syntax & RE_NO_BK_VBAR)
            goto handle_alt;
          else
            goto normal_char;


        case '{':
	  if ((syntax & RE_INTERVALS) && (syntax & RE_NO_BK_BRACES))
	    goto handle_interval;
	  else
	    goto normal_char;


        case '\\':
          if (p == pend) return REG_EESCAPE;

          /* Do not translate the character after the \, so that we can
             distinguish, e.g., \B from \b, even if we normally would
             translate, e.g., B to b.  */
          PATFETCH_RAW (c);

          switch (c)
            {
            case '(':
              if (syntax & RE_NO_BK_PARENS)
                goto normal_backslash;

            handle_open:
              rxb->re_nsub++;
              regnum++;
              if (COMPILE_STACK_FULL)
                { 
                  RETALLOC (compile_stack.stack, compile_stack.size << 1,
                            compile_stack_elt_t);
                  if (compile_stack.stack == 0) return REG_ESPACE;

                  compile_stack.size <<= 1;
                }

	      if (*last_expression)
		{
		  struct rexp_node * concat
		    = rx_mk_r_concat (&rxb->rx, *last_expression, 0);
		  if (!concat)
		    return REG_ESPACE;
		  *last_expression = concat;
		  last_expression = &concat->params.pair.right;
		}

              /*
	       * These are the values to restore when we hit end of this
               * group.  
	       */
	      COMPILE_STACK_TOP.top_expression = top_expression;
	      COMPILE_STACK_TOP.last_expression = last_expression;
              COMPILE_STACK_TOP.regnum = regnum;
	      
              compile_stack.avail++;
	      
	      top_expression = last_expression;
	      break;


            case ')':
              if (syntax & RE_NO_BK_PARENS) goto normal_backslash;

            handle_close:
              /* See similar code for backslashed left paren above.  */
              if (COMPILE_STACK_EMPTY)
                if (syntax & RE_UNMATCHED_RIGHT_PAREN_ORD)
                  goto normal_char;
                else
                  return REG_ERPAREN;

              /* Since we just checked for an empty stack above, this
                 ``can't happen''.  */

              {
                /* We don't just want to restore into `regnum', because
                   later groups should continue to be numbered higher,
                   as in `(ab)c(de)' -- the second group is #2.  */
                regnum_t this_group_regnum;
		struct rexp_node ** inner = top_expression;

                compile_stack.avail--;
		top_expression = COMPILE_STACK_TOP.top_expression;
		last_expression = COMPILE_STACK_TOP.last_expression;
                this_group_regnum = COMPILE_STACK_TOP.regnum;
		{
		  int left_se = paramc;
		  int right_se = paramc + 1;

		  params = (params
			    ? ((struct re_se_params *)
			       realloc (params,
					(paramc + 2) * sizeof (params[0])))
			    : ((struct re_se_params *)
			       malloc (2 * sizeof (params[0]))));
		  if (!params)
		    return REG_ESPACE;
		  paramc += 2;

		  params[left_se].se = re_se_lparen;
		  params[left_se].op1 = this_group_regnum;
		  params[right_se].se = re_se_rparen;
		  params[right_se].op1 = this_group_regnum;
		  {
		    struct rexp_node * left
		      = rx_mk_r_side_effect (&rxb->rx,
					     (rx_side_effect)left_se);
		    struct rexp_node * right
		      = rx_mk_r_side_effect (&rxb->rx,
					     (rx_side_effect)right_se);
		    struct rexp_node * c1
		      = (*inner
			 ? rx_mk_r_concat (&rxb->rx, left, *inner) : left);
		    struct rexp_node * c2
		      = rx_mk_r_concat (&rxb->rx, c1, right);
		    if (!(left && right && c1 && c2))
		      return REG_ESPACE;
		    *inner = c2;
		  }
		}
		break;
	      }

            case '|':					/* `\|'.  */
              if ((syntax & RE_LIMITED_OPS) || (syntax & RE_NO_BK_VBAR))
                goto normal_backslash;
            handle_alt:
              if (syntax & RE_LIMITED_OPS)
                goto normal_char;

	      {
		struct rexp_node * alt
		  = rx_mk_r_alternate (&rxb->rx, *top_expression, 0);
		if (!alt)
		  return REG_ESPACE;
		*top_expression = alt;
		last_expression = &alt->params.pair.right;
		{
		  int sync_se = paramc;

		  params = (params
			    ? ((struct re_se_params *)
			       realloc (params,
					(paramc + 1) * sizeof (params[0])))
			    : ((struct re_se_params *)
			       malloc (sizeof (params[0]))));
		  if (!params)
		    return REG_ESPACE;
		  ++paramc;

		  params[sync_se].se = re_se_tv;
		  {
		    struct rexp_node * sync
		      = rx_mk_r_side_effect (&rxb->rx,
					     (rx_side_effect)sync_se);
		    struct rexp_node * conc
		      = rx_mk_r_concat (&rxb->rx, sync, 0);

		    if (!sync || !conc)
		      return REG_ESPACE;

		    *last_expression = conc;
		    last_expression = &conc->params.pair.right;
		  }
		}
	      }
              break;


            case '{': 
              /* If \{ is a literal.  */
              if (!(syntax & RE_INTERVALS)
                     /* If we're at `\{' and it's not the open-interval 
                        operator.  */
                  || ((syntax & RE_INTERVALS) && (syntax & RE_NO_BK_BRACES))
                  || (p - 2 == pattern  &&  p == pend))
                goto normal_backslash;

            handle_interval:
              {
                /* If got here, then the syntax allows intervals.  */

                /* At least (most) this many matches must be made.  */
                int lower_bound = -1, upper_bound = -1;

                beg_interval = p - 1;

                if (p == pend)
                  {
                    if (syntax & RE_NO_BK_BRACES)
                      goto unfetch_interval;
                    else
                      return REG_EBRACE;
                  }

                GET_UNSIGNED_NUMBER (lower_bound);

                if (c == ',')
                  {
                    GET_UNSIGNED_NUMBER (upper_bound);
                    if (upper_bound < 0) upper_bound = RE_DUP_MAX;
                  }
                else
                  /* Interval such as `{1}' => match exactly once. */
                  upper_bound = lower_bound;

                if (lower_bound < 0 || upper_bound > RE_DUP_MAX
                    || lower_bound > upper_bound)
                  {
                    if (syntax & RE_NO_BK_BRACES)
                      goto unfetch_interval;
                    else 
                      return REG_BADBR;
                  }

                if (!(syntax & RE_NO_BK_BRACES)) 
                  {
                    if (c != '\\') return REG_EBRACE;
                    PATFETCH (c);
                  }

                if (c != '}')
                  {
                    if (syntax & RE_NO_BK_BRACES)
                      goto unfetch_interval;
                    else 
                      return REG_BADBR;
                  }

                /* We just parsed a valid interval.  */

                /* If it's invalid to have no preceding re.  */
                if (pointless_if_repeated (*last_expression, params))
                  {
                    if (syntax & RE_CONTEXT_INVALID_OPS)
                      return REG_BADRPT;
                    else if (!(syntax & RE_CONTEXT_INDEP_OPS))
                      goto unfetch_interval;
		    /* was: else laststart = b; */
                  }

                /* If the upper bound is zero, don't want to iterate
                 * at all.
		 */
                 if (upper_bound == 0)
		   {
		     if (*last_expression)
		       {
			 rx_free_rexp (&rxb->rx, *last_expression);
			 *last_expression = 0;
		       }
		   }
		else
		  /* Otherwise, we have a nontrivial interval. */
		  {
		    int iter_se = paramc;
		    int end_se = paramc + 1;
		    params = (params
			      ? ((struct re_se_params *)
				 realloc (params,
					  sizeof (*params) * (2 + paramc)))
			      : ((struct re_se_params *)
				 malloc (2 * sizeof (*params))));
		    if (!params)
		      return REG_ESPACE;
		    paramc += 2;
		    params [iter_se].se = re_se_iter;
		    params [iter_se].op1 = lower_bound;
		    params[iter_se].op2 = upper_bound;

		    params[end_se].se = re_se_end_iter;
		    params[end_se].op1 = lower_bound;
		    params[end_se].op2 = upper_bound;
		    {
		      struct rexp_node * push0
			= rx_mk_r_side_effect (&rxb->rx,
					       (rx_side_effect)re_se_push0);
		      struct rexp_node * start_one_iter
			= rx_mk_r_side_effect (&rxb->rx,
					       (rx_side_effect)iter_se);
		      struct rexp_node * phase1
			= rx_mk_r_concat (&rxb->rx, start_one_iter,
					  *last_expression);
		      struct rexp_node * pushback
			= rx_mk_r_side_effect (&rxb->rx,
					       (rx_side_effect)re_se_pushback);
		      rx_Bitset cs = rx_cset (&rxb->rx);
		      struct rexp_node * lit_t
			= rx_mk_r_cset (&rxb->rx, cs);
		      struct rexp_node * phase2
			= rx_mk_r_concat (&rxb->rx, pushback, lit_t);
		      struct rexp_node * loop
			= rx_mk_r_2phase_star (&rxb->rx, phase1, phase2);
		      struct rexp_node * push_n_loop
			= rx_mk_r_concat (&rxb->rx, push0, loop);
		      struct rexp_node * final_test
			= rx_mk_r_side_effect (&rxb->rx,
					       (rx_side_effect)end_se);
		      struct rexp_node * full_exp
			= rx_mk_r_concat (&rxb->rx, push_n_loop, final_test);

		      if (!(push0 && start_one_iter && phase1
			    && pushback && lit_t && phase2
			    && loop && push_n_loop && final_test && full_exp))
			return REG_ESPACE;

		      RX_bitset_enjoin(cs, 't');

		      *last_expression = full_exp;
		    }
		  }
                beg_interval = 0;
              }
              break;

            unfetch_interval:
              /* If an invalid interval, match the characters as literals.  */
               p = beg_interval;
               beg_interval = NULL;

               /* normal_char and normal_backslash need `c'.  */
               PATFETCH (c);	

               if (!(syntax & RE_NO_BK_BRACES))
                 {
                   if (p > pattern  &&  p[-1] == '\\')
                     goto normal_backslash;
                 }
               goto normal_char;

#ifdef emacs
            /* There is no way to specify the before_dot and after_dot
               operators.  rms says this is ok.  --karl  */
            case '=':
	      side = at_dot;
	      goto add_side_effect;
              break;

            case 's':
	    case 'S':
	      {
		rx_Bitset cs = cset (&rxb->rx);
		struct rexp_node * set = rx_mk_r_cset (&rxb->rx, cs);
		if (!(cs && set))
		  return REG_ESPACE;
		if (c == 'S')
		  rx_bitset_universe (rxb->rx.local_cset_size, cs);

		PATFETCH (c);
		{
		  int x;
		  char code = syntax_spec_code (c);
		  for (x = 0; x < 256; ++x)
		    {
		      
		      if (SYNTAX (x) & code)
			{
			  rx_Bitset it =
			    inverse_translation (rxb, validate_inv_tr,
						 inverse_translate,
						 translate, x);
			  rx_bitset_xor (rxb->rx.local_cset_size, cs, it);
			}
		    }
		}
		goto append_node;
	      }
              break;
#endif /* emacs */


            case 'w':
            case 'W':
	      {
		rx_Bitset cs = rx_cset (&rxb->rx);
		struct rexp_node * n = (cs ? rx_mk_r_cset (&rxb->rx, cs) : 0);
		if (!(cs && n))
		  return REG_ESPACE;
		if (c == 'W')
		  rx_bitset_universe (rxb->rx.local_cset_size ,cs);
		{
		  int x;
		  for (x = rxb->rx.local_cset_size - 1; x > 0; --x)
		    if (re_syntax_table[x] & Sword)
		      RX_bitset_toggle (cs, x);
		}
		append = n;
		goto append_node;
	      }
              break;

/* With a little extra work, some of these side effects could be optimized
 * away (basicly by looking at what we already know about the surrounding
 * chars).  
 */
            case '<':
	      side = (rx_side_effect)re_se_wordbeg;
	      goto add_side_effect;
              break;

            case '>':
              side = (rx_side_effect)re_se_wordend;
	      goto add_side_effect;
              break;

            case 'b':
              side = (rx_side_effect)re_se_wordbound;
	      goto add_side_effect;
              break;

            case 'B':
              side = (rx_side_effect)re_se_notwordbound;
	      goto add_side_effect;
              break;

            case '`':
	      side = (rx_side_effect)re_se_begbuf;
	      goto add_side_effect;
	      break;
	      
            case '\'':
	      side = (rx_side_effect)re_se_endbuf;
	      goto add_side_effect;
              break;

	    add_side_effect:
	      {
		struct rexp_node * se
		  = rx_mk_r_side_effect (&rxb->rx, side);
		if (!se)
		  return REG_ESPACE;
		append = se;
		goto append_node;
	      }
	      break;

            case '1': case '2': case '3': case '4': case '5':
            case '6': case '7': case '8': case '9':
              if (syntax & RE_NO_BK_REFS)
                goto normal_char;

              c1 = c - '0';

              if (c1 > regnum)
                return REG_ESUBREG;

              /* Can't back reference to a subexpression if inside of it.  */
              if (group_in_compile_stack (compile_stack, c1))
		return REG_ESUBREG;

	      {
		int backref_se = paramc;
		params = (params
			  ? ((struct re_se_params *)
			     realloc (params,
				      sizeof (*params) * (1 + paramc)))
			  : ((struct re_se_params *)
			     malloc (sizeof (*params))));
		if (!params)
		  return REG_ESPACE;
		++paramc;
		params[backref_se].se = re_se_backref;
		params[backref_se].op1 = c1;
		side = (rx_side_effect)backref_se;
		goto add_side_effect;
	      }
              break;

            case '+':
            case '?':
              if (syntax & RE_BK_PLUS_QM)
                goto handle_plus;
              else
                goto normal_backslash;

            default:
            normal_backslash:
              /* You might think it would be useful for \ to mean
                 not to translate; but if we don't translate it
                 it will never match anything.  */
              c = TRANSLATE (c);
              goto normal_char;
            }
          break;


	default:
        /* Expects the character in `c'.  */
	normal_char:
	    {
	      rx_Bitset cs = rx_cset(&rxb->rx);
	      struct rexp_node * match = rx_mk_r_cset (&rxb->rx, cs);
	      rx_Bitset it;
	      if (!(cs && match))
		return REG_ESPACE;
	      it = inverse_translation (rxb, validate_inv_tr,
					inverse_translate, translate, c);
	      rx_bitset_union (CHAR_SET_SIZE, cs, it);
	      append = match;

	    append_node:
	      /* This genericly appends the rexp APPEND to *LAST_EXPRESSION
	       * and then parses the next character normally.
	       */
	      if (*last_expression)
		{
		  struct rexp_node * concat
		    = rx_mk_r_concat (&rxb->rx, *last_expression, append);
		  if (!concat)
		    return REG_ESPACE;
		  *last_expression = concat;
		  last_expression = &concat->params.pair.right;
		}
	      else
		*last_expression = append;
	    }
	} /* switch (c) */
    } /* while p != pend */

  
  {
    int win_se = paramc;
    params = (params
	      ? ((struct re_se_params *)
		 realloc (params,
			  sizeof (*params) * (1 + paramc)))
	      : ((struct re_se_params *)
		 malloc (sizeof (*params))));
    if (!params)
      return REG_ESPACE;
    ++paramc;
    params[win_se].se = re_se_win;
    {
      struct rexp_node * se
	= rx_mk_r_side_effect (&rxb->rx, (rx_side_effect)win_se);
      struct rexp_node * concat
	= rx_mk_r_concat (&rxb->rx, rexp, se);
      if (!(se && concat))
	return REG_ESPACE;
      rexp = concat;
    }
  }


  /* Through the pattern now.  */

  if (!COMPILE_STACK_EMPTY) 
    return REG_EPAREN;

      free (compile_stack.stack);

  orig_rexp = rexp;
#ifdef RX_DEBUG
  if (rx_debug_compile)
    {
      dbug_rxb = rxb;
      fputs ("\n\nCompiling ", stdout);
      fwrite (pattern, 1, size, stdout);
      fputs (":\n", stdout);
      rxb->se_params = params;
      print_rexp (&rxb->rx, orig_rexp, 2, re_seprint, stdout);
    }
#endif
  {
    rx_Bitset cs = rx_cset(&rxb->rx);
    rx_Bitset cs2 = rx_cset(&rxb->rx);
    char * se_map = (char *) alloca (paramc);
    struct rexp_node * new_rexp = 0;


    bzero (se_map, paramc);
    find_backrefs (se_map, rexp, params);
    fewer_side_effects =
      remove_unecessary_side_effects (&rxb->rx, se_map,
				      rx_copy_rexp (&rxb->rx, rexp), params);

    speed_up_alt (&rxb->rx, rexp, 0);
    speed_up_alt (&rxb->rx, fewer_side_effects, 1);

    {
      char * syntax_parens = rxb->syntax_parens;
      if (syntax_parens == (char *)0x1)
	rexp = remove_unecessary_side_effects
	  (&rxb->rx, se_map, rexp, params);
      else if (syntax_parens)
	{
	  int x;
	  for (x = 0; x < paramc; ++x)
	    if ((   (params[x].se == re_se_lparen)
		 || (params[x].se == re_se_rparen))
		&& (!syntax_parens [params[x].op1]))
	      se_map [x] = 1;
	  rexp = remove_unecessary_side_effects
	    (&rxb->rx, se_map, rexp, params);
	}
    }

    /* At least one more optimization would be nice to have here but i ran out 
     * of time.  The idea would be to delay side effects.  
     * For examle, `(abc)' is the same thing as `abc()' except that the
     * left paren is offset by 3 (which we know at compile time).
     * (In this comment, write that second pattern `abc(:3:)' 
     * where `(:3:' is a syntactic unit.)
     *
     * Trickier:  `(abc|defg)'  is the same as `(abc(:3:|defg(:4:))'
     * (The paren nesting may be hard to follow -- that's an alternation
     *	of `abc(:3:' and `defg(:4:' inside (purely syntactic) parens
     *  followed by the closing paren from the original expression.)
     *
     * Neither the expression tree representation nor the the nfa make
     * this very easy to write. :(
     */

  /* What we compile is different than what the parser returns.
   * Suppose the parser returns expression R.
   * Let R' be R with unnecessary register assignments removed 
   * (see REMOVE_UNECESSARY_SIDE_EFFECTS, above).
   *
   * What we will compile is the expression:
   *
   *    m{try}R{win}\|s{try}R'{win}
   *
   * {try} and {win} denote side effect epsilons (see EXPLORE_FUTURE).
   * 
   * When trying a match, we insert an `m' at the beginning of the 
   * string if the user wants registers to be filled, `s' if not.
   */
    new_rexp =
      rx_mk_r_alternate
	(&rxb->rx,
	 rx_mk_r_concat (&rxb->rx, rx_mk_r_cset (&rxb->rx, cs2), rexp),
	 rx_mk_r_concat (&rxb->rx,
			 rx_mk_r_cset (&rxb->rx, cs), fewer_side_effects));

    if (!(new_rexp && cs && cs2))
      return REG_ESPACE;
    RX_bitset_enjoin (cs2, '\0'); /* prefixed to the rexp used for matching. */
    RX_bitset_enjoin (cs, '\1'); /* prefixed to the rexp used for searching. */
    rexp = new_rexp;
  }

#ifdef RX_DEBUG
  if (rx_debug_compile)
    {
      fputs ("\n...which is compiled as:\n", stdout);
      print_rexp (&rxb->rx, rexp, 2, re_seprint, stdout);
    }
#endif
  {
    struct rx_nfa_state *start = 0;
    struct rx_nfa_state *end = 0;

    if (!rx_build_nfa (&rxb->rx, rexp, &start, &end))
      return REG_ESPACE;	/*  */
    else
      {
	void * mem = (void *)rxb->buffer;
	unsigned long size = rxb->allocated;
	int start_id;
	char * perm_mem;
	int iterator_size = paramc * sizeof (params[0]);

	end->is_final = 1;
	start->is_start = 1;
	rx_name_nfa_states (&rxb->rx);
	start_id = start->id;
#ifdef RX_DEBUG
	if (rx_debug_compile)
	  {
	    fputs ("...giving the NFA: \n", stdout);
	    dbug_rxb = rxb;
	    print_nfa (&rxb->rx, rxb->rx.nfa_states, re_seprint, stdout);
	  }
#endif
	if (!rx_eclose_nfa (&rxb->rx))
	  return REG_ESPACE;
	else
	  {
	    rx_delete_epsilon_transitions (&rxb->rx);
	    
	    /* For compatability reasons, we need to shove the
	     * compiled nfa into one chunk of malloced memory.
	     */
	    rxb->rx.reserved = (   sizeof (params[0]) * paramc
				+  rx_sizeof_bitset (rxb->rx.local_cset_size));
#ifdef RX_DEBUG
	    if (rx_debug_compile)
	      {
		dbug_rxb = rxb;
		fputs ("...which cooks down (uncompactified) to: \n", stdout);
		print_nfa (&rxb->rx, rxb->rx.nfa_states, re_seprint, stdout);
	      }
#endif
	    if (!rx_compactify_nfa (&rxb->rx, &mem, &size))
	      return REG_ESPACE;
	    rxb->buffer = mem;
	    rxb->allocated = size;
	    rxb->rx.buffer = mem;
	    rxb->rx.allocated = size;
	    perm_mem = ((char *)rxb->rx.buffer
			+ rxb->rx.allocated - rxb->rx.reserved);
	    rxb->se_params = ((struct re_se_params *)perm_mem);
	    bcopy (params, rxb->se_params, iterator_size);
	    perm_mem += iterator_size;
	    rxb->fastset = (rx_Bitset) perm_mem;
	    rxb->start = rx_id_to_nfa_state (&rxb->rx, start_id);
	  }
	rx_bitset_null (rxb->rx.local_cset_size, rxb->fastset);
	rxb->can_match_empty = compute_fastset (rxb, orig_rexp);
	rxb->match_regs_on_stack =
	  registers_on_stack (rxb, orig_rexp, 0, params); 
	rxb->search_regs_on_stack =
	  registers_on_stack (rxb, fewer_side_effects, 0, params);
	if (rxb->can_match_empty)
	  rx_bitset_universe (rxb->rx.local_cset_size, rxb->fastset);
	rxb->is_anchored = is_anchored (orig_rexp, (rx_side_effect) re_se_hat);
	rxb->begbuf_only = is_anchored (orig_rexp,
					(rx_side_effect) re_se_begbuf);
      }
    rx_free_rexp (&rxb->rx, rexp);
    if (params)
      free (params);
#ifdef RX_DEBUG
    if (rx_debug_compile)
      {
	dbug_rxb = rxb;
	fputs ("...which cooks down to: \n", stdout);
	print_nfa (&rxb->rx, rxb->rx.nfa_states, re_seprint, stdout);
      }
#endif
  }
  return REG_NOERROR;
}



/* This table gives an error message for each of the error codes listed
   in regex.h.  Obviously the order here has to be same as there.  */

const char * rx_error_msg[] =
{ 0,						/* REG_NOERROR */
    "No match",					/* REG_NOMATCH */
    "Invalid regular expression",		/* REG_BADPAT */
    "Invalid collation character",		/* REG_ECOLLATE */
    "Invalid character class name",		/* REG_ECTYPE */
    "Trailing backslash",			/* REG_EESCAPE */
    "Invalid back reference",			/* REG_ESUBREG */
    "Unmatched [ or [^",			/* REG_EBRACK */
    "Unmatched ( or \\(",			/* REG_EPAREN */
    "Unmatched \\{",				/* REG_EBRACE */
    "Invalid content of \\{\\}",		/* REG_BADBR */
    "Invalid range end",			/* REG_ERANGE */
    "Memory exhausted",				/* REG_ESPACE */
    "Invalid preceding regular expression",	/* REG_BADRPT */
    "Premature end of regular expression",	/* REG_EEND */
    "Regular expression too big",		/* REG_ESIZE */
    "Unmatched ) or \\)",			/* REG_ERPAREN */
};



/* Test if at very beginning or at very end of the virtual concatenation
 *  of `string1' and `string2'.  If only one string, it's `string2'.  
 */

#define AT_STRINGS_BEG() \
  (string1 \
   ? ((tst_half == 0) \
      && ((unsigned char *)tst_pos == (unsigned char *)string1 - 1)) \
   : ((unsigned char *)tst_pos == (unsigned char *)string2 - 1))

#define AT_STRINGS_END() \
  (string2 \
   ? ((tst_half == 1) \
      && ((unsigned char *)tst_pos \
	  == ((unsigned char *)string2 + size2 - 1))) \
   : ((unsigned char *)tst_pos == ((unsigned char *)string1 + size1 - 1)))

/* Test if D points to a character which is word-constituent.  We have
 * two special cases to check for: if past the end of string1, look at
 * the first character in string2; and if before the beginning of
 * string2, look at the last character in string1.
 *
 * Assumes `string1' exists, so use in conjunction with AT_STRINGS_BEG ().  
 */
#define LETTER_P(d)							\
  (SYNTAX ((string2 && (tst_half == 0)					\
	    && ((d) == ((unsigned char *)string1 + size1)))		\
	   ? *(unsigned char *)string2					\
	   : ((string1 && (tst_half == 1)				\
	       && ((d) == (unsigned char *)string2 - 1))		\
	      ? *((unsigned char *)string1 + size1 - 1)			\
	      : *(d))) == Sword)

/* Test if the character at D and the one after D differ with respect
 * to being word-constituent.  
 */
#define AT_WORD_BOUNDARY(d)						\
  (AT_STRINGS_BEG () || AT_STRINGS_END () || LETTER_P (d) != LETTER_P (d + 1))


static char slowmap [256] =
{
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};

#ifdef __STDC__
static void
rx_blow_up_fastmap (struct re_pattern_buffer * rxb)
#else
static void
rx_blow_up_fastmap (rxb)
     struct re_pattern_buffer * rxb;
#endif
{
  int x;
  for (x = 0; x < 256; ++x)	/* &&&& 3.6 % */
    rxb->fastmap [x] = !!RX_bitset_member (rxb->fastset, x);
  rxb->fastmap_accurate = 1;
}




struct stack_chunk
{
  struct stack_chunk * next_chunk;
  int bytes_left;
  char * sp;
};

#define PUSH(CHUNK_VAR,BYTES)   \
  if (!CHUNK_VAR || (CHUNK_VAR->bytes_left < (BYTES)))  \
    {					\
      struct stack_chunk * new_chunk;	\
      if (free_chunks)			\
	{				\
	  new_chunk = free_chunks;	\
	  free_chunks = free_chunks->next_chunk; \
	}				\
      else				\
	{				\
	  new_chunk = (struct stack_chunk *)alloca (chunk_bytes); \
	  if (!new_chunk)		\
	    {				\
	      ret_val = 0;		\
	      goto test_do_return;	\
	    }				\
	}				\
      new_chunk->sp = (char *)new_chunk + sizeof (struct stack_chunk); \
      new_chunk->bytes_left = (chunk_bytes \
			       - (BYTES) \
			       - sizeof (struct stack_chunk)); \
      new_chunk->next_chunk = CHUNK_VAR; \
      CHUNK_VAR = new_chunk;		\
    } \
  else \
    (CHUNK_VAR->sp += (BYTES)), (CHUNK_VAR->bytes_left -= (BYTES))

#define POP(CHUNK_VAR,BYTES) \
  if (CHUNK_VAR->sp == ((char *)CHUNK_VAR + sizeof(*CHUNK_VAR))) \
    { \
      struct stack_chunk * new_chunk = CHUNK_VAR->next_chunk; \
      CHUNK_VAR->next_chunk = free_chunks; \
      free_chunks = CHUNK_VAR; \
      CHUNK_VAR = new_chunk; \
    } \
  else \
    (CHUNK_VAR->sp -= BYTES), (CHUNK_VAR->bytes_left += BYTES)

struct counter_frame
{
  int tag;
  int val;
  struct counter_frame * inherited_from; /* If this is a copy. */
  struct counter_frame * cdr;
};

struct backtrack_frame
{
  char * counter_stack_sp;

  /* A frame is used to save the matchers state when it crosses a 
   * backtracking point.  The `stk_' fields correspond to variables
   * in re_search_2 (just strip off thes `stk_').  They are documented
   * tere.
   */
  struct rx_superstate * stk_super;
  const unsigned char * stk_tst_pos;
  int stk_tst_half;
  unsigned int stk_c;
  const unsigned char * stk_tst_str_half;
  const unsigned char * stk_tst_end_half;
  int stk_last_l;
  int stk_last_r;
  int stk_test_ret;

  /* This is the list of options left to explore at the backtrack
   * point for which this frame was created. 
   */
  struct rx_distinct_future * df;
  struct rx_distinct_future * first_df;

#ifdef RX_DEBUG
   int stk_line_no;
#endif
};



#if !defined(REGEX_MALLOC) && !defined(__GNUC__)
#define RE_SEARCH_2_FN	inner_re_search_2
#else
#define RE_SEARCH_2_FN	re_search_2
#endif

#ifdef __STDC__
int
RE_SEARCH_2_FN (struct re_pattern_buffer *rxb,
		const char * string1, int size1,
		const char * string2, int size2,
		int startpos, int range,
		struct re_registers *regs,
		int stop)
#else
int
RE_SEARCH_2_FN (rxb,
		string1, size1, string2, size2, startpos, range, regs, stop)
     struct re_pattern_buffer *rxb;
     const char * string1;
     int size1;
     const char * string2;
     int size2;
     int startpos;
     int range;
     struct re_registers *regs;
     int stop;
#endif
{
  /* Two groups of registers are kept.  The group with the register state
   * of the current test match, and the group that holds the state at the end
   * of the best known match, if any.
   *
   * For some patterns, there may also be registers saved on the stack.
   */
  regoff_t * lparen = 0; /* scratch space for register returns */
  regoff_t * rparen = 0;
  regoff_t * best_lpspace = 0; /* in case the user doesn't want these */
  regoff_t * best_rpspace = 0; /* values, we still need space to store
				* them.  Normally, this memoryis unused
				* and the space pointed to by REGS is 
				* used instead.
				*/
  
  int last_l;			/* Highest index of a valid lparen. */
  int last_r;			/* It's dual. */

  int * best_lparen;		/* This contains the best known register */
  int * best_rparen;		/* assignments. 
				 * This may point to the same mem as
				 * best_lpspace, or it might point to memory
				 * passed by the caller.
				 */
  int best_last_l;		/* best_last_l:best_lparen::last_l:lparen */
  int best_last_r;
  
  

  /* Figure the number of registers we may need for use in backreferences.
   * The number here includes an element for register zero.  
   */
  unsigned num_regs = rxb->re_nsub + 1;

  int total_size = size1 + size2;


  /***** INIT re_search_2 */
  
  /* Check for out-of-range STARTPOS.  */
  if ((startpos < 0) || (startpos > total_size))
    return -1;

  /* Fix up RANGE if it might eventually take us outside
   * the virtual concatenation of STRING1 and STRING2.
   */
  {
    int endpos = startpos + range;
    if (endpos < -1)
      range = (-1 - startpos);
    else if (endpos > total_size)
      range = total_size - startpos;
  }

  /* If the search isn't to be a backwards one, don't waste time in a
   * long search for a pattern that says it is anchored.
   */
  if (rxb->begbuf_only && (range > 0))
    {
      if (startpos > 0)
	return -1;
      else
	range = 1;
    }

  /* Then, decide whether to use internal or user-provided reg buffers. */
  if (!regs || rxb->no_sub)
    {
      best_lpspace = (regoff_t *)REGEX_ALLOCATE (num_regs * sizeof(regoff_t));
      best_rpspace = (regoff_t *)REGEX_ALLOCATE (num_regs * sizeof(regoff_t));
      best_lparen = best_lpspace;
      best_rparen = best_rpspace;
    }
  else
    {	
      /* Have the register data arrays been allocated?  */
      if (rxb->regs_allocated == REGS_UNALLOCATED)
	{ /* No.  So allocate them with malloc.  We need one
	     extra element beyond `num_regs' for the `-1' marker
	     GNU code uses.  */
	  regs->num_regs = MAX (RE_NREGS, rxb->re_nsub + 1);
	  regs->start = TALLOC (regs->num_regs, regoff_t);
	  regs->end = TALLOC (regs->num_regs, regoff_t);
	  if (regs->start == 0 || regs->end == 0)
	    return -2;
	  rxb->regs_allocated = REGS_REALLOCATE;
	}
      else if (rxb->regs_allocated == REGS_REALLOCATE)
	{ /* Yes.  If we need more elements than were already
	     allocated, reallocate them.  If we need fewer, just
	     leave it alone.  */
	  if (regs->num_regs < num_regs + 1)
	    {
	      regs->num_regs = num_regs + 1;
	      RETALLOC (regs->start, regs->num_regs, regoff_t);
	      RETALLOC (regs->end, regs->num_regs, regoff_t);
	      if (regs->start == 0 || regs->end == 0)
		return -2;
	    }
	}
      else if (rxb->regs_allocated != REGS_FIXED)
	return -2;

      if (regs->num_regs < num_regs + 1)
	{
	  best_lpspace = ((regoff_t *)
			  REGEX_ALLOCATE (num_regs * sizeof(regoff_t)));
	  best_rpspace = ((regoff_t *)
			  REGEX_ALLOCATE (num_regs * sizeof(regoff_t)));
	  best_lparen = best_lpspace;
	  best_rparen = best_rpspace;
	}
      else
	{
	  best_lparen = regs->start;
	  best_rparen = regs->end;
	}
    }
  
  lparen = (regoff_t *) REGEX_ALLOCATE (num_regs * sizeof(regoff_t));
  rparen = (regoff_t *) REGEX_ALLOCATE (num_regs * sizeof(regoff_t)); 
  
  if (!(best_rparen && best_lparen && lparen && rparen))
    return -2;
  
  best_last_l = best_last_r = -1;



  /***** fastmap/search loop, initialization */

  /* This is the loop that scans using the fastmap, and sometimes tries to 
   * match. From this point on, don't return.  Instead, assign to ret_val
   * and goto fail.
   */
  {
    const unsigned char * translate = (rxb->translate
				       ? rxb->translate : (char *)id_translation);
    
    /** This is state associated with returning to the caller. */

    int ret_val = -1;

    /*   A sentinal is sometimes installed in the fastmap.  This records
     *   where so it can be removed before returning.
     */
    int fastmap_chr = -1;
    int fastmap_val = 0;

    /** End of state associated with returning to the caller. */

    /** Start of variables associated with the fastmap based search: */

    char * fastmap = rxb->fastmap ? (char *)rxb->fastmap : (char *)slowmap;
    int search_direction;	/* 1 or -1 */
    int search_end;		/* first position to not try */
    int offset;			/* either size1 or 0 as string == string2 */

    /* The string-pair position of the fastmap/search loop: */
    const unsigned char * pos;	/* The current pos. */
    const unsigned char * string; /* The current string half. */
    const unsigned char * end;	/* End of current string. */
    int size;			/* Current string's size */
    int half;			/* 0 means string1, 1 means string2 */

    /** End of variables associated with the fastmap based search: */


    /** Start of variables associated with trying a match
     *  after the fastmap has found a plausible starting point.
     */

    struct rx_superstate * start_super = 0; /* The superNFA start state. */

    /*
     * Two nfa's were compiled.  
     * `0' is complete.
     * `1' faster but gets registers wrong and ends too soon.
     */
    int nfa_choice = ((regs && !rxb->least_subs) ? '\0' : '\1');

    const unsigned char * abs_end; /* Don't fetch a character from here. */
    int first_found;		/* If true, return after finding any match. */

    /** End of variables associated with trying a match. */

    /* Update the fastmap now if not correct already. 
     * When the regexp was compiled, the fastmap was computed
     * and stored in a bitset.  This expands the bitset into a
     * character array containing 1s and 0s.
     */
    if ((fastmap == rxb->fastmap) && !rxb->fastmap_accurate)
      rx_blow_up_fastmap (rxb);

    /* Now we build the starting state of the supernfa. */
    {
      struct rx_superset * start_contents;
      struct rx_nfa_state_set * start_nfa_set;
      
      /* We presume here that the nfa start state has only one
       * possible future with no side effects.  
       */
      start_nfa_set = rxb->start->futures->destset;
      if (   rxb->rx.start_set
	  && (rxb->rx.start_set->starts_for == &rxb->rx))
	start_contents = rxb->rx.start_set;
      else
	{
	  start_contents =
	    rx_superstate_eclosure_union (&rxb->rx,
					  rx_superset_cons (&rxb->rx, 0, 0),
					  start_nfa_set);
	  
	  if (!start_contents)
	    return -1;

	  start_contents->starts_for = &rxb->rx;
	  rxb->rx.start_set = start_contents;
	}
      if (   start_contents->superstate
	  && (start_contents->superstate->rx_id == rxb->rx.rx_id))
	{
	  start_super = start_contents->superstate;
	  rx_lock_superstate (&rxb->rx, start_super);
	}
      else
	{
	  rx_protect_superset (&rxb->rx, start_contents);
	  
	  start_super = rx_superstate (&rxb->rx, start_contents);
	  if (!start_super)
	    return -1;
	  rx_lock_superstate (&rxb->rx, start_super);
	  rx_release_superset (&rxb->rx, start_contents);
	}
    }
    
    /* This computes an upper bound on string addresses for use by
     * the match-test.
     */
    abs_end = ((const unsigned char *) ((stop <= size1)
					? string1 + stop
					: string2 + stop - size1));

    /* We have the option to look for the best match or the first
     * one we can find.  If the user isn't asking for register information,
     * we don't need to find the best match.
     */
    first_found = !regs;

    /* Compute search_end & search_direction for the fastmap loop. */
    if (range >= 0)
      {
	search_end = MIN (size1 + size2, startpos + range) + 1;
	search_direction = 1;
      }
    else
      {
	search_end = MAX(-1, startpos + range);
	search_direction = -1;
      }

    /* The vacuous search always turns up nothing. */
    if ((search_direction == 1)
	? (startpos > search_end)
	: (startpos < search_end))
      return -1;

    /* Set string/size/offset/end -- the state that tells the fastmap
     * loop which half of the string we're in.  Also set pos, which
     * is the addr of the current fastmap scan position.
     */
    if (!string2 || (startpos < size1))
      {
	string = (const unsigned char *)string1;
	size = size1;
	offset = 0;
	pos = (const unsigned char *)(string1 + startpos);
	half = 0;
	end = (const unsigned char *)MIN(string1 + size1, string1 + stop);
      }
    else
      {
	string = (const unsigned char *)string2;
	size = size2;
	offset = size1;
	pos = (const unsigned char *)(string2 + startpos - size1);
	half = 1;
	end = (const unsigned char *)MIN(string2 + size2,
					 string2 + stop - size1);
      }




    /***** fastmap/search loop,  body */


  init_fastmap_sentinal:

    /* For the sake of fast fastmapping, set a sentinal in the fastmap.
     * This sentinal will trap the fastmap loop when it reaches the last
     * valid character in a string half.
     *
     * This must be reset when the fastmap/search loop crosses a string 
     * boundry, and before returning to the caller.  So sometimes,
     * the fastmap loop is restarted with `continue', othertimes by
     * `goto init_fastmap_sentinal'.
     */
    if (size)
      {
	fastmap_chr = ((search_direction == 1)
		       ? *(end - 1)
		       : *string);
	fastmap_val = fastmap[fastmap_chr];
	fastmap[fastmap_chr] = 1;
      }
    else
      {
	fastmap_chr = -1;
	fastmap_val = 0;
      }

    do
      {
	/* If we haven't reached the end of a string half, and if the
	 * pattern can't match the empty string, then the fastmap 
	 * optimization applies.  This conditional scans using the 
	 * fastmap -- stoping when a string half ends, or when a 
	 * plausible starting point for a match is found.
	 * It updates HIT_BOUND to tell which case occured.
	 */
	if (pos == end)
	  goto fastmap_hit_bound;
	else
	  {
	    if (search_direction == 1)
	      {
		if (fastmap_val)
		  {
		    for (;;)
		      {
			while (!fastmap[*pos])
			  ++pos;
			goto commence_a_matchin;
		      }
		  }
		else
		  {
		    for (;;)
		      {
			while (!fastmap[*pos])
			  ++pos;
			if (*pos != fastmap_chr)
			  goto commence_a_matchin;
			else 
			  {
			    ++pos;
			    if (pos == end)
			      goto fastmap_hit_bound;
			  }
		      }
		  }
	      }
	    else
	      {
		const unsigned char * bound = string - 1;
		if (fastmap_val)
		  {
		    for (;;)
		      {
			while (!fastmap[*pos])
			  --pos;
			goto commence_a_matchin;
		      }
		  }
		else
		  {
		    for (;;)
		      {
			while (!fastmap[*pos])
			  --pos;
			if ((*pos != fastmap_chr) || fastmap_val)
			  goto commence_a_matchin;
			else 
			  {
			    --pos;
			    if (pos == bound)
			      goto fastmap_hit_bound;
			  }
		      }
		  }
	      }
	  }
	
      fastmap_hit_bound:
	{
	  /* If we hit a bound, it may simply be time to switch sides
	   * between strings.
	   */
	  if ((search_direction == 1) && string2 && (half == 0))
	    {
	      string = (const unsigned char *)string2;
	      size = size2;
	      offset = size1;
	      half = 1;
	      end = (const unsigned char *)MIN(string2 + size2,
					       string2 + stop - size1);
	      startpos = size1;
	      pos = (const unsigned char *)string2;
	      goto init_fastmap_sentinal;
	    }
	  else if (   string1
		   && (search_direction == -1)
		   && (half == 1))
	    {
	      string = (const unsigned char *)string1;
	      size = size1;
	      offset = 0;
	      end = (const unsigned char *)string1 + size1;
	      half = 0;
	      startpos = size1 - 1;
	      pos = (const unsigned char *)string1 + size1 - 1;
	      goto init_fastmap_sentinal;
	    }
	  /* ...not a string split, simply no more string. 
	   *
	   * When searching backward, running out of string
	   * is reason to quit.
	   */
	  else if (search_direction == -1)
	    goto finish;
	  
	  /* ...when searching forward, we allow the possibility
	   * of an (empty) match after the last character in the
	   * virtual string.  So, fall through to the matcher
	   */
	}


      commence_a_matchin:

	/***** fastmap/search loop body
	 *	      test for a match that begins at pos
	 */

	/* Now the fastmap loop has brought us to a plausible 
	 * starting point for a match.  So, it's time to run the
	 * NFA and see if a match occured.
	 */

	startpos = pos - string + offset;
	if (startpos == search_end)
	  goto finish;
	
	last_l = last_r = 0;
	lparen[0] = startpos;	/* We know match-begin for this test... */

	/* The test matcher is essentially a recursive function
	 * that does an exhaustive run of the superNFA at the 
	 * test position.  For performance, that function has 
	 * been in-lined by hand.
	 */

#undef OF
#ifndef HAVE_GNUC_LABELS
#define OF(A,B)	A
#else
#define OF(A,B)	A: B
	  static void * rx_labels_instruction_table[] =
	    {
	      [rx_backtrack_point] &&backtrack_point,
	      [rx_backtrack] &&backtrack,
	      [rx_do_side_effects] &&do_side_effects,
	      [rx_cache_miss] &&cache_miss,
	      [rx_next_char] 0,
	      [rx_error_inx] 0
	    };
#endif
	{	  
	  /* The current superNFA position of the matcher. */
	  struct rx_superstate * super = start_super;
	  
	  /* The matcher interprets a series of instruction frames.
	   * This is the `instruction counter' for the interpretation.
	   */
	  struct rx_inx * ifr;
	  
	  /* We insert a ghost character in the string to prime
	   * the nfa.  tst_pos, tst_str_half, and tst_end_half
	   * keep track of the test-match position and string-half.
	   */
	  const unsigned char * tst_pos = pos - 1;
	  int tst_half = half;
	  unsigned char c = nfa_choice;
	  
	  const unsigned char * tst_str_half = string;
	  const unsigned char * tst_end_half = end;
	  
	  struct stack_chunk * counter_stack = 0;
	  struct stack_chunk * backtrack_stack = 0;
	  int backtrack_frame_bytes =
	    (sizeof (struct backtrack_frame)
	     + (rxb->match_regs_on_stack
		? sizeof (regoff_t) * (num_regs + 1) * 2
		: 0));
	  int chunk_bytes = backtrack_frame_bytes * 64;
	  struct stack_chunk * free_chunks = 0;

#ifdef RX_DEBUG
	  int backtrack_depth = 0;
#endif

	  /* To return from this function, set test_ret and 
	   * `goto test_do_return'.
	   *
	   * Possible return values are:
	   *     1   --- end of string while the superNFA is still going
	   *     0   --- internal error (out of memory)
	   *	-1   --- search completed by reaching the superNFA fail state
	   *    -2   --- a match was found, maybe not the longest.
	   *
	   * When the search is complete (-1), best_last_r indicates whether
	   * a match was found.
	   *
	   * -2 is return only if first_found is non-zero.
	   *
	   * if first_found is non-zero, a return of -1 indicates no match,
	   * otherwise, best_last_r has to be checked.
	   */
	  int test_ret = -1;
	  
	  while (1)
	    {
	      int inx;
#ifdef RX_DEBUG
	      /* There is a search tree with every node as set of deterministic
	       * transitions in the super nfa.  For every branch of a 
	       * backtrack point is an edge in the tree.
	       * This counts up a pre-order of nodes in that tree.
	       * It's saved on the search stack and printed when debugging. 
	       */
	      int line_no = 0;
	      int lines_found = 0;
#endif


	    top_of_cycle:
	      /* A superstate is basicly a transition table, indexed by 
	       * characters from the string being tested, and containing 
	       * RX_INX structures.
	       */
	      ifr = &super->transitions [c];
	      
	    recurse_test_match:
	      /* This is the point to which control is sent when the
	       * test matcher recurses.  Before jumping here, some variables
	       * need to be saved on the stack and setup for the recursion.
	       */

	    restart:
	      /* Some instructions don't advance the matcher, but just
	       * carry out some side effects and fetch a new instruction.
	       * To dispatch that new instruction, `goto restart'.
	       */
	      
	      {
		struct rx_inx * next_tr_table = (struct rx_inx *)ifr->data;
		struct rx_inx * this_tr_table = super->transitions;
		/* The fastest route through the loop is when the instruction 
		 * is RX_NEXT_CHAR.  This case is detected when IFR->DATA
		 * is non-zero.  In that case, it points to the next
		 * superstate. 
		 *
		 * This allows us to not bother fetching the bytecode.
		 */
		while (next_tr_table)
		  {
#ifdef RX_DEBUG
		    if (rx_debug_trace)
		      {
			struct rx_superset * setp;

			fprintf (stderr, "%d %d>> re_next_char @ %d (%d)",
				 line_no,
				 backtrack_depth,
				 (tst_pos - tst_str_half
				  + (tst_half == 0
				     ? 0 : size1)), c);
			
			super =
			  ((struct rx_superstate *)
			   ((char *)this_tr_table
			    - ((unsigned long)
			       ((struct rx_superstate *)0)->transitions)));

			setp = super->contents;
			fprintf (stderr, "   superstet (rx=%d, &=%x: ",
				 rxb->rx.rx_id, setp);
			while (setp)
			  {
			    fprintf (stderr, "%d ", setp->id);
			    setp = setp->cdr;
			  }
			fprintf (stderr, "\n");
		      }
#endif
		    this_tr_table = next_tr_table;
		    ++tst_pos;
		    if (tst_pos == tst_end_half)
		      {
			if (   (tst_pos != abs_end)
			    && string2
			    && half == 0)
			  {
			    /* Here we are crossing the break 
			     * in a split string. 
			     */
			    tst_str_half = (const unsigned char *)string2;
			    tst_end_half = abs_end;
			    tst_pos = (const unsigned char *)string2;
			    tst_half = 1;
			  }
			else
			  {
			    test_ret = 1;
			    goto test_do_return;
			  }
		      }
		    c = *tst_pos;
		    ifr = this_tr_table + c;
		    next_tr_table = (struct rx_inx *)ifr->data;
		  }
		
		/* Here when we ran out cached next-char transitions. 
		 * So, it will be necessary to do a more expensive
		 * dispatch on the current instruction.  The superstate
		 * pointer is allowed to become invalid during next-char
		 * transitions -- now we must bring it up to date.
		 */
		super =
		  ((struct rx_superstate *)
		   ((char *)this_tr_table
		    - ((unsigned long)
		       ((struct rx_superstate *)0)->transitions)));
	      }
	      
	      /* We've encountered an instruction other than next-char.
	       * Dispatch that instruction:
	       */
	      inx = (int)ifr->inx;
#ifdef HAVE_GNUC_LABELS
	      goto *rx_labels_instruction_table[inx];
#endif
#ifdef RX_DEBUG
	      if (rx_debug_trace)
		{
		  struct rx_superset * setp = super->contents;
		  
		  fprintf (stderr, "%d %d>> %s @ %d (%d)", line_no,
			   backtrack_depth,
			   inx_names[inx],
			   (tst_pos - tst_str_half
			    + (tst_half == 0 ? 0 : size1)), c);
		  
		  fprintf (stderr, "   superstet (rx=%d, &=%x: ",
			   rxb->rx.rx_id, setp);
		  while (setp)
		    {
		      fprintf (stderr, "%d ", setp->id);
		      setp = setp->cdr;
		    }
		  fprintf (stderr, "\n");
		}
#endif
	      switch ((enum rx_opcode)inx)
		{
		case OF(rx_do_side_effects,do_side_effects):

		  /*  RX_DO_SIDE_EFFECTS occurs when we cross epsilon 
		   *  edges associated with parentheses, backreferencing, etc.
		   */
		  {
		    struct rx_distinct_future * df =
		      (struct rx_distinct_future *)ifr->data_2;
		    struct rx_se_list * el = df->effects;
		    /* Side effects come in lists.  This walks down
		     * a list, dispatching.
		     */
		    while (el)
		      {
#ifdef HAVE_GNUC_LABELS
			static void * se_labels[] =
			  {
			    [-re_se_try] &&se_try,
			    [-re_se_pushback] &&se_pushback,
			    [-re_se_push0] &&se_push0,
			    [-re_se_pushpos] &&se_pushpos,
			    [-re_se_chkpos] &&se_chkpos,
			    [-re_se_poppos] &&se_poppos,
#ifdef emacs
			    [-re_se_at_dot] &&se_at_dot,
			    [-re_se_syntax] &&se_syntax,
			    [-re_se_not_syntax] &&se_not_syntax,
#endif
			    [-re_se_begbuf] &&se_begbuf,
			    [-re_se_hat] &&se_hat,
			    [-re_se_wordbeg] &&se_wordbeg,
			    [-re_se_wordbound] &&se_wordbound,
			    [-re_se_notwordbound] &&se_notwordbound,
			    [-re_se_wordend] &&se_wordend,
			    [-re_se_endbuf] &&se_endbuf,
			    [-re_se_dollar] &&se_dollar,
			    [-re_se_fail] &&se_fail,
			  };
			static void * se_lables2[] =
			  {
			    [re_se_win] &&se_win
			    [re_se_lparen] &&se_lparen,
			    [re_se_rparen] &&se_rparen,
			    [re_se_backref] &&se_backref,
			    [re_se_iter] &&se_iter,
			    [re_se_end_iter] &&se_end_iter,
			    [re_se_tv] &&se_tv
			  };
#endif
			int effect = (int)el->car;
			if (effect < 0)
			  {
#ifdef HAVE_GNUC_LABELS
			    goto *se_labels[-effect];
#endif
#ifdef RX_DEBUG
			    if (rx_debug_trace)
			      {
				struct rx_superset * setp = super->contents;
				
				fprintf (stderr, "....%d %d>> %s\n", line_no,
					 backtrack_depth,
					 efnames[-effect]);
			      }
#endif
			    switch ((enum re_side_effects) effect)
			      {
			      case OF(re_se_pushback,se_pushback):
				ifr = &df->future_frame;
				if (!ifr->data)
				  {
				    struct rx_superstate * sup = super;
				    rx_lock_superstate (rx, sup);
				    if (!rx_handle_cache_miss (&rxb->rx,
							       super, c,
							       ifr->data_2))
				      {
					rx_unlock_superstate (rx, sup);
					test_ret = 0;
					goto test_do_return;
				      }
				    rx_unlock_superstate (rx, sup);
				  }
				/* --tst_pos; */
				c = 't';
				super
				  = ((struct rx_superstate *)
				     ((char *)ifr->data
				      - (long)(((struct rx_superstate *)0)
					       ->transitions)));
				goto top_of_cycle;
				break;
			      case OF(re_se_push0,se_push0):
				{
				  struct counter_frame * old_cf
				     = (counter_stack
					? ((struct counter_frame *)
					   counter_stack->sp)
					: 0);
				  struct counter_frame * cf;
				  PUSH (counter_stack,
					sizeof (struct counter_frame));
				  cf = ((struct counter_frame *)
					counter_stack->sp);
				  cf->tag = re_se_iter;
				  cf->val = 0;
				  cf->inherited_from = 0;
				  cf->cdr = old_cf;
				  break;
				}
			      case OF(re_se_fail,se_fail):
				goto test_do_return;
			      case OF(re_se_begbuf,se_begbuf):
				if (!AT_STRINGS_BEG ())
				  goto test_do_return;
				break;
			      case OF(re_se_endbuf,se_endbuf):
				if (!AT_STRINGS_END ())
				  goto test_do_return;
				break;
			      case OF(re_se_wordbeg,se_wordbeg):
				if (   LETTER_P (tst_pos + 1)
				    && (   AT_STRINGS_BEG()
					|| !LETTER_P (tst_pos)))
				  break;
				else
				  goto test_do_return;
			      case OF(re_se_wordend,se_wordend):
				if (   !AT_STRINGS_BEG ()
				    && LETTER_P (tst_pos)
				    && (AT_STRINGS_END ()
					|| !LETTER_P (tst_pos + 1)))
				  break;
				else
				  goto test_do_return;
			      case OF(re_se_wordbound,se_wordbound):
				if (AT_WORD_BOUNDARY (tst_pos))
				  break;
				else
				  goto test_do_return;
			      case OF(re_se_notwordbound,se_notwordbound):
				if (!AT_WORD_BOUNDARY (tst_pos))
				  break;
				else
				  goto test_do_return;
			      case OF(re_se_hat,se_hat):
				if (AT_STRINGS_BEG ())
				  {
				    if (rxb->not_bol)
				      goto test_do_return;
				    else
				      break;
				  }
				else
				  {
				    char pos_c = *tst_pos;
				    if (   (TRANSLATE (pos_c)
					    == TRANSLATE('\n'))
					&& rxb->newline_anchor)
				      break;
				    else
				      goto test_do_return;
				  }
			      case OF(re_se_dollar,se_dollar):
				if (AT_STRINGS_END ())
				  {
				    if (rxb->not_eol)
				      goto test_do_return;
				    else
				      break;
				  }
				else
				  {
				    const unsigned char * next_pos
				      = ((string2 && (tst_half == 0) &&
					  (tst_pos
					   == ((unsigned char *)
					       string1 + size1 - 1)))
					 ? (unsigned char *)string2
					 : tst_pos + 1);
				    
				    if (   (TRANSLATE (*next_pos)
					    == TRANSLATE ('\n'))
					&& rxb->newline_anchor)
				      break;
				    else
				      goto test_do_return;
				  }
				
			      case OF(re_se_try,se_try):
				/* This is the first side effect in every
				 * expression.
				 *
				 *  FOR NO GOOD REASON...get rid of it...
				 */
				break;

			      case OF(re_se_pushpos,se_pushpos):
				{
				  int urhere =
				    ((int)(tst_pos - tst_str_half)
				     + ((tst_half == 0) ? 0 : size1));
				  struct counter_frame * old_cf
				    = (counter_stack
				       ? ((struct counter_frame *)
					  counter_stack->sp)
				       : 0);
				  struct counter_frame * cf;
				  PUSH(counter_stack,
				       sizeof (struct counter_frame));
				  cf = ((struct counter_frame *)
					counter_stack->sp);
				  cf->tag = re_se_pushpos;
				  cf->val = urhere;
				  cf->inherited_from = 0;
				  cf->cdr = old_cf;
				  break;
				}
				
			      case OF(re_se_chkpos,se_chkpos):
				{
				  int urhere =
				    ((int)(tst_pos - tst_str_half)
				     + ((tst_half == 0) ? 0 : size1));
				  struct counter_frame * cf
				    = ((struct counter_frame *)
				       counter_stack->sp);
				  if (cf->val == urhere)
				    goto test_do_return;
				  cf->val = urhere;
				  break;
				}
				break;

			      case OF(re_se_poppos,se_poppos):
				POP(counter_stack,
				    sizeof (struct counter_frame));
				break;
				
				
			      case OF(re_se_at_dot,se_at_dot):
			      case OF(re_se_syntax,se_syntax):
			      case OF(re_se_not_syntax,se_not_syntax):
#ifdef emacs
				this release lacks emacs support;
				(coming soon);
#endif
				break;
			      case re_se_win:
			      case re_se_lparen:
			      case re_se_rparen:
			      case re_se_backref:
			      case re_se_iter:
			      case re_se_end_iter:
			      case re_se_tv:
			      case re_floogle_flap:
				ret_val = 0;
				goto test_do_return;
			      }
			  }
			else
			  {
#ifdef HAVE_GNUC_LABELS
			    goto *se_lables2[(rxb->se_params [effect].se)];
#endif
#ifdef RX_DEBUG
			  if (rx_debug_trace)
			    fprintf (stderr, "....%d %d>> %s %d %d\n", line_no,
				     backtrack_depth,
				     efnames2[rxb->se_params [effect].se],
				     rxb->se_params [effect].op1,
				     rxb->se_params [effect].op2);
#endif
			    switch (rxb->se_params [effect].se)
			      {
			      case OF(re_se_win,se_win):
				/* This side effect indicates that we've 
				 * found a match, though not necessarily the 
				 * best match.  This is a fancy assignment to 
				 * register 0 unless the caller didn't 
				 * care about registers.  In which case,
				 * this stops the match.
				 */
				{
				  int urhere =
				    ((int)(tst_pos - tst_str_half)
				     + ((tst_half == 0)
					? 0 : size1));

				  if (   (best_last_r < 0)
				      || (urhere + 1 > best_rparen[0]))
				    {
				      /* Record the best known and keep
				       * looking.
				       */
				      int x;
				      for (x = 0; x <= last_l; ++x)
					best_lparen[x] = lparen[x];
				      best_last_l = last_l;
				      for (x = 0; x <= last_r; ++x)
					best_rparen[x] = rparen[x];
				      best_rparen[0] = urhere + 1;
				      best_last_r = last_r;
				    }
				  /* If we're not reporting the match-length 
				   * or other register info, we need look no
				   * further.
				   */
				  if (first_found)
				    {
				      test_ret = -2;
				      goto test_do_return;
				    }
				}
				break;
			      case OF(re_se_lparen,se_lparen):
				{
				  int urhere =
				    ((int)(tst_pos - tst_str_half)
				     + ((tst_half == 0) ? 0 : size1));
				  
				  int reg = rxb->se_params [effect].op1;
#if 0
				  if (reg > last_l)
#endif
				    {
				      lparen[reg] = urhere + 1;
				      /* In addition to making this assignment,
				       * we now know that lower numbered regs
				       * that haven't already been assigned,
				       * won't be.  We make sure they're
				       * filled with -1, so they can be
				       * recognized as unassigned.
				       */
				      if (last_l < reg)
					while (++last_l < reg)
					  lparen[last_l] = -1;
				    }
				  break;
				}
				
			      case OF(re_se_rparen,se_rparen):
				{
				  int urhere =
				    ((int)(tst_pos - tst_str_half)
				     + ((tst_half == 0) ? 0 : size1));
				  int reg = rxb->se_params [effect].op1;
				  rparen[reg] = urhere + 1;
				  if (last_r < reg)
				    {
				      while (++last_r < reg)
					rparen[last_r] = -1;
				    }
				  break;
				}
				
			      case OF(re_se_backref,se_backref):
				{
				  int reg = rxb->se_params [effect].op1;
				  if (reg > last_r || rparen[reg] < 0)
				    goto test_do_return;
				  {
				    /* fixme */
				    const unsigned char * there
				      = tst_str_half + lparen[reg];
				    const unsigned char * last
				      = tst_str_half + rparen[reg];
				    const unsigned char * here = tst_pos + 1;

				    if ((here == tst_end_half) && string2
					&& (tst_str_half
					    == (unsigned char *) string1)
					&& (tst_end_half != abs_end))
				      {
					here = (unsigned char *)string2;
					tst_end_half = abs_end;
				      }
				    
				    while (there < last && here < tst_end_half)	/* 4% */
				      if (TRANSLATE(*there) /* &&&& 6% */
					  != TRANSLATE(*here))
					goto test_do_return;
				      else
					{
					  ++there; ++here;
					  if ((here == tst_end_half) && string2
					      && (tst_str_half
						  == (unsigned char *)string1)
					      && (tst_end_half != abs_end))
					    {
					      here = (unsigned char *)string2;
					      tst_end_half = abs_end;
					      tst_half = 1;
					    }
					}
				    if (there != last)
				      goto test_do_return;
				    tst_pos = here - 1;
				    if ((here == (unsigned char *)string2)
					&& (unsigned char *)string1)
				      {
					tst_pos = ((unsigned char *)string1
						   + size1 - 1);
					tst_end_half = tst_pos + 1;
					tst_half = 0;
				      }
				  }
				  break;
				}
			      case OF(re_se_iter,se_iter):
				{
				  struct counter_frame * csp
				    = ((struct counter_frame *)
				       counter_stack->sp);
				  if (csp->val == rxb->se_params[effect].op2)
				    goto test_do_return;
				  else
				    ++csp->val;
				  break;
				}
			      case OF(re_se_end_iter,se_end_iter):
				{
				  struct counter_frame * csp
				    = ((struct counter_frame *)
				       counter_stack->sp);
				  if (csp->val < rxb->se_params[effect].op1)
				    goto test_do_return;
				  else
				    {
				      struct counter_frame * source = csp;
				      while (source->inherited_from)
					source = source->inherited_from;
				      if (!source || !source->cdr)
					{
					  POP(counter_stack,
					      sizeof(struct counter_frame));
					}
				      else
					{
					  source = source->cdr;
					  csp->val = source->val;
					  csp->tag = source->tag;
					  csp->cdr = 0;
					  csp->inherited_from = source;
					}
				    }
				  break;
				}
			      case OF(re_se_tv, se_tv):
				/* is a noop */
				break;
			      case re_se_try:
			      case re_se_pushback:
			      case re_se_push0:
			      case re_se_pushpos:
			      case re_se_chkpos:
			      case re_se_poppos:
			      case re_se_at_dot:
			      case re_se_syntax:
			      case re_se_not_syntax:
			      case re_se_begbuf:
			      case re_se_hat:
			      case re_se_wordbeg:
			      case re_se_wordbound:
			      case re_se_notwordbound:
			      case re_se_wordend:
			      case re_se_endbuf:
			      case re_se_dollar:
			      case re_se_fail:
			      case re_floogle_flap:
				ret_val = 0;
				goto test_do_return;
			      }
			  }
			el = el->cdr;
		      }
		    /* Now the side effects are done,
		     * so get the next instruction.
		     * and move on.
		     */
		    ifr = &df->future_frame;
		    goto restart;
		  }
		  
		case OF(rx_backtrack_point,backtrack_point):
		  {
		    /* A backtrack point indicates that we've reached a
		     * non-determinism in the superstate NFA.  This is a
		     * loop that exhaustively searches the possibilities.
		     *
		     * A backtracking strategy is used.  We keep track of what
		     * registers are valid so we can erase side effects.
		     *
		     * First, make sure there is some stack space to hold 
		     * our state.
		     */

		    struct backtrack_frame * bf;

		    PUSH(backtrack_stack, backtrack_frame_bytes);
#ifdef RX_DEBUG
		    ++backtrack_depth;
#endif

		    bf = ((struct backtrack_frame *)
			  backtrack_stack->sp);
		    {
		      bf->stk_super = super;
		      /* We prevent the current superstate from being
		       * deleted from the superstate cache.
		       */
		      rx_lock_superstate (&rxb->rx, super);
		      bf->stk_tst_pos = tst_pos;
#ifdef RX_DEBUG
		      bf->stk_line_no = line_no;
#endif
		      bf->stk_tst_half = tst_half;
		      bf->stk_c = c;
		      bf->stk_tst_str_half = tst_str_half;
		      bf->stk_tst_end_half = tst_end_half;
		      bf->stk_last_l = last_l;
		      bf->stk_last_r = last_r;
		      bf->df = ((struct rx_super_edge *)ifr->data_2)->options;
		      bf->first_df = bf->df;
		      bf->counter_stack_sp = (counter_stack
					      ? counter_stack->sp
					      : 0);
		      bf->stk_test_ret = test_ret;
		      if (rxb->match_regs_on_stack)
			{
			  int x;
			  regoff_t * stk =
			    (regoff_t *)((char *)bf + sizeof (*bf));
			  for (x = 0; x <= last_l; ++x)
			    stk[x] = lparen[x];
			  stk += x;
			  for (x = 0; x <= last_r; ++x)
			    stk[x] = rparen[x];
			}

		    }

		    /* Here is a while loop whose body is mainly a function
		     * call and some code to handle a return from that
		     * function.
		     *
		     * From here on for the rest of `case backtrack_point' it
		     * is unsafe to assume that the variables saved on the
		     * stack are valid -- so reread their values from the stack
		     * as needed.
		     *
		     * This lets us re-use one generation fewer stack saves in
		     * the call-graph of a search.
		     */
		    
		  while_non_det_options:
#ifdef RX_DEBUG
		    ++lines_found;
		    if (rx_debug_trace)
		      fprintf (stderr, "@@@ %d calls %d @@@\n",
			       line_no, lines_found);
		    
		    line_no = lines_found;
#endif
		    
		    if (bf->df->next_same_super_edge[0] == bf->first_df)
		      {
			/* This is a tail-call optimization -- we don't recurse
			 * for the last of the possible futures.
			 */
			ifr = (bf->df->effects
			       ? &bf->df->side_effects_frame
			       : &bf->df->future_frame);

			rx_unlock_superstate (&rxb->rx, super);
			POP(backtrack_stack, backtrack_frame_bytes);
#ifdef RX_DEBUG
			--backtrack_depth;
#endif
			goto restart;
		      }
		    else
		      {
			if (counter_stack)
			  {
			    struct counter_frame * old_cf
			      = ((struct counter_frame *)counter_stack->sp);
			    struct counter_frame * cf;
			    PUSH(counter_stack, sizeof (struct counter_frame));
			    cf = ((struct counter_frame *)counter_stack->sp);
			    cf->tag = old_cf->tag;
			    cf->val = old_cf->val;
			    cf->inherited_from = old_cf;
			    cf->cdr = 0;
			  }			
			/* `Call' this test-match block */
			ifr = (bf->df->effects
			       ? &bf->df->side_effects_frame
			       : &bf->df->future_frame);
			goto recurse_test_match;
		      }

		    /* Returns in this block are accomplished by
		     * goto test_do_return.  There are two cases.
		     * If there is some search-stack left,
		     * then it is a return from a `recursive' call.
		     * If there is no search-stack left, then
		     * we should return to the fastmap/search loop.
		     */
		    
		  test_do_return:

		    if (!backtrack_stack)
		      {
#ifdef RX_DEBUG
			if (rx_debug_trace)
			  fprintf (stderr, "!!! %d bails returning %d !!!\n",
				   line_no, test_ret);
#endif

			/* No more search-stack -- this test is done. */
			if (test_ret)
			  goto return_from_test_match;
			else
			  goto error_in_testing_match;
		      }

		    /* Ok..we're returning from a recursive call to 
		     * the test match block:
		     */
		    
		    bf = ((struct backtrack_frame *)
			  backtrack_stack->sp);
#ifdef RX_DEBUG
		    if (rx_debug_trace)
		      fprintf (stderr, "+++ %d returns %d (to %d)+++\n",
			       line_no, test_ret, bf->stk_line_no);
#endif

		    while (counter_stack
			   && (!bf->counter_stack_sp
			       || (bf->counter_stack_sp != counter_stack->sp)))
		      {
			POP(counter_stack, sizeof (struct counter_frame));
		      }

		    if (!test_ret)
		      {
			POP (backtrack_stack, backtrack_frame_bytes);
			goto test_do_return;
		      }

		    /* If any possible future reaches the end of the 
		     * string without failing, make sure we propogate 
		     * that information to the caller.
		     */
		    if ((test_ret == -2) && first_found)
		      {
			rx_unlock_superstate (&rxb->rx, bf->stk_super);
			POP (backtrack_stack, backtrack_frame_bytes);
			goto test_do_return;
		      }

		    if (bf->stk_test_ret < 0)
		      test_ret = bf->stk_test_ret;

		    last_l = bf->stk_last_l;
		    last_r = bf->stk_last_r;
		    bf->df = bf->df->next_same_super_edge[0];
		    super = bf->stk_super;
		    tst_pos = bf->stk_tst_pos;
		    tst_half = bf->stk_tst_half;
		    c = bf->stk_c;
		    tst_str_half = bf->stk_tst_str_half;
		    tst_end_half = bf->stk_tst_end_half;
#ifdef RX_DEBUG
		    line_no = bf->stk_line_no;
#endif

		    if (rxb->match_regs_on_stack)
		      {
			int x;
			regoff_t * stk =
			  (regoff_t *)((char *)bf + sizeof (*bf));
			for (x = 0; x <= last_l; ++x)
			  lparen[x] = stk[x];
			stk += x;
			for (x = 0; x <= last_r; ++x)
			  rparen[x] = stk[x];
		      }

		    goto while_non_det_options;
		  }

		  
		case OF(rx_cache_miss,cache_miss):
		  /* Because the superstate NFA is lazily constructed,
		   * and in fact may erode from underneath us, we sometimes
		   * have to construct the next instruction from the hard way.
		   * This invokes one step in the lazy-conversion.
		   */
		  ifr = rx_handle_cache_miss (&rxb->rx, super, c, ifr->data_2);
		  if (!ifr)
		    {
		      test_ret = 0;
		      goto test_do_return;
		    }
		  goto restart;
		  
		case OF(rx_backtrack,backtrack):
		  /* RX_BACKTRACK means that we've reached the empty
		   * superstate, indicating that match can't succeed
		   * from this point.
		   */
		  goto test_do_return;
		case rx_next_char:
		case rx_error_inx:
		case rx_num_instructions:
		  ret_val = 0;
		  goto test_do_return;
		}
	    }
	}


	/* Healthy exists from the test-match loop do a 
	 * `goto return_from_test_match'   On the other hand, 
	 * we might end up here.
	 */
      error_in_testing_match:
	ret_val = -2;
	goto finish;


	/***** fastmap/search loop body
	 *	      considering the results testing for a match
	 */

      return_from_test_match:

	if (best_last_l >= 0)
	  {
	    if (regs && (regs->start != best_lparen))
	      {
		bcopy (best_lparen, regs->start,
		       regs->num_regs * sizeof (int));
		bcopy (best_rparen, regs->end,
		       regs->num_regs * sizeof (int));
	      }
	    if (regs && !rxb->no_sub)
	      {
		int q;
		int bound = (regs->num_regs > num_regs
			     ? regs->num_regs
			     : num_regs);
		regoff_t * s = regs->start;
		regoff_t * e = regs->end;
		for (q = best_last_l + 1;  q < bound; ++q)
		  s[q] = e[q] = -1;
	      }
	    ret_val = best_lparen[0];
	    goto finish;
	  }

	/***** fastmap/search loop,  increment and loop-test */

	pos += search_direction;
	startpos += search_direction;

      } while (startpos < search_end);


  /**** Exit code for fastmap/searchloop and the entire re_search_2 fn. */

  finish:
    /* Unset the fastmap sentinel */
    if (fastmap_chr >= 0)
      fastmap[fastmap_chr] = fastmap_val;

    if (start_super)
      rx_unlock_superstate (&rxb->rx, start_super);

#ifdef REGEX_MALLOC
    if (lparen) free (lparen);
    if (rparen) free (rparen);
    if (best_lpspace) free (best_lpspace);
    if (best_rpspace) free (best_rpspace);
#endif
    return ret_val;
  }
}

#if !defined(REGEX_MALLOC) && !defined(__GNUC__)
#ifdef __STDC__
int
re_search_2 (struct re_pattern_buffer *rxb,
	     const char * string1, int size1,
	     const char * string2, int size2,
	     int startpos, int range,
	     struct re_registers *regs,
	     int stop)
#else
int
re_search_2 (rxb, string1, size1, string2, size2, startpos, range, regs, stop)
     struct re_pattern_buffer *rxb;
     const char * string1;
     int size1;
     const char * string2;
     int size2;
     int startpos;
     int range;
     struct re_registers *regs;
     int stop;
#endif
{
  int ret;
  ret = inner_re_search_2 (rxb, string1, size1, string2, size2, startpos,
			   range, regs, stop);
  alloca (0);
  return ret;
}
#endif


/* Like re_search_2, above, but only one string is specified, and
 * doesn't let you say where to stop matching.
 */

#ifdef __STDC__
int
re_search (struct re_pattern_buffer * rxb, const char *string,
	   int size, int startpos, int range,
	   struct re_registers *regs)
#else
int
re_search (rxb, string, size, startpos, range, regs)
     struct re_pattern_buffer * rxb;
     const char * string;
     int size;
     int startpos;
     int range;
     struct re_registers *regs;
#endif
{
  return re_search_2 (rxb, 0, 0, string, size, startpos, range, regs, size);
}

#ifdef __STDC__
int
re_match_2 (struct re_pattern_buffer * rxb,
	    const char * string1, int size1,
	    const char * string2, int size2,
	    int pos, struct re_registers *regs, int stop)
#else
int
re_match_2 (rxb, string1, size1, string2, size2, pos, regs, stop)
     struct re_pattern_buffer * rxb;
     const char * string1;
     int size1;
     const char * string2;
     int size2;
     int pos;
     struct re_registers *regs;
     int stop;
#endif
{
  struct re_registers some_regs;
  regoff_t start;
  regoff_t end;
  int srch;
  int save = rxb->regs_allocated;
  struct re_registers * regs_to_pass = regs;

  if (!regs)
    {
      some_regs.start = &start;
      some_regs.end = &end;
      some_regs.num_regs = 1;
      regs_to_pass = &some_regs;
      rxb->regs_allocated = REGS_FIXED;
    }

  srch = re_search_2 (rxb, string1, size1, string2, size2,
		      pos, 1, regs_to_pass, stop);
  if (regs_to_pass != regs)
    rxb->regs_allocated = save;
  if (srch < 0)
    return srch;
  return regs_to_pass->end[0] - regs_to_pass->start[0];
}

/* re_match is like re_match_2 except it takes only a single string.  */

#ifdef __STDC__
int
re_match (struct re_pattern_buffer * rxb,
	  const char * string,
	  int size, int pos,
	  struct re_registers *regs)
#else
int
re_match (rxb, string, size, pos, regs)
     struct re_pattern_buffer * rxb;
     const char *string;
     int size;
     int pos;
     struct re_registers *regs;
#endif
{
  return re_match_2 (rxb, string, size, 0, 0, pos, regs, size);
}



/* Set by `re_set_syntax' to the current regexp syntax to recognize.  Can
   also be assigned to arbitrarily: each pattern buffer stores its own
   syntax, so it can be changed between regex compilations.  */
reg_syntax_t re_syntax_options = RE_SYNTAX_EMACS;


/* Specify the precise syntax of regexps for compilation.  This provides
   for compatibility for various utilities which historically have
   different, incompatible syntaxes.

   The argument SYNTAX is a bit mask comprised of the various bits
   defined in regex.h.  We return the old syntax.  */

#ifdef __STDC__
reg_syntax_t
re_set_syntax (reg_syntax_t syntax)
#else
reg_syntax_t
re_set_syntax (syntax)
    reg_syntax_t syntax;
#endif
{
  reg_syntax_t ret = re_syntax_options;

  re_syntax_options = syntax;
  return ret;
}


/* Set REGS to hold NUM_REGS registers, storing them in STARTS and
   ENDS.  Subsequent matches using PATTERN_BUFFER and REGS will use
   this memory for recording register information.  STARTS and ENDS
   must be allocated using the malloc library routine, and must each
   be at least NUM_REGS * sizeof (regoff_t) bytes long.

   If NUM_REGS == 0, then subsequent matches should allocate their own
   register data.

   Unless this function is called, the first search or match using
   PATTERN_BUFFER will allocate its own register data, without
   freeing the old data.  */

#ifdef __STDC__
void
re_set_registers (struct re_pattern_buffer *bufp,
		  struct re_registers *regs,
		  unsigned num_regs,
		  regoff_t * starts, regoff_t * ends)
#else
void
re_set_registers (bufp, regs, num_regs, starts, ends)
     struct re_pattern_buffer *bufp;
     struct re_registers *regs;
     unsigned num_regs;
     regoff_t * starts;
     regoff_t * ends;
#endif
{
  if (num_regs)
    {
      bufp->regs_allocated = REGS_REALLOCATE;
      regs->num_regs = num_regs;
      regs->start = starts;
      regs->end = ends;
    }
  else
    {
      bufp->regs_allocated = REGS_UNALLOCATED;
      regs->num_regs = 0;
      regs->start = regs->end = (regoff_t) 0;
    }
}




#ifdef __STDC__
static int 
cplx_se_sublist_len (struct rx_se_list * list)
#else
static int 
cplx_se_sublist_len (list)
     struct rx_se_list * list;
#endif
{
  int x = 0;
  while (list)
    {
      if ((int)list->car >= 0)
	++x;
      list = list->cdr;
    }
  return x;
}


/* For rx->se_list_cmp */

#ifdef __STDC__
static int 
posix_se_list_order (struct rx * rx,
		     struct rx_se_list * a, struct rx_se_list * b)
#else
static int 
posix_se_list_order (rx, a, b)
     struct rx * rx;
     struct rx_se_list * a;
     struct rx_se_list * b;
#endif
{
  int al = cplx_se_sublist_len (a);
  int bl = cplx_se_sublist_len (b);

  if (!al && !bl)
    return ((a == b)
	    ? 0
	    : ((a < b) ? -1 : 1));
  
  else if (!al)
    return -1;

  else if (!bl)
    return 1;

  else
    {
      rx_side_effect * av = ((rx_side_effect *)
			     alloca (sizeof (rx_side_effect) * (al + 1)));
      rx_side_effect * bv = ((rx_side_effect *)
			     alloca (sizeof (rx_side_effect) * (bl + 1)));
      struct rx_se_list * ap = a;
      struct rx_se_list * bp = b;
      int ai, bi;
      
      for (ai = al - 1; ai >= 0; --ai)
	{
	  while ((int)ap->car < 0)
	    ap = ap->cdr;
	  av[ai] = ap->car;
	  ap = ap->cdr;
	}
      av[al] = (rx_side_effect)-2;
      for (bi = bl - 1; bi >= 0; --bi)
	{
	  while ((int)bp->car < 0)
	    bp = bp->cdr;
	  bv[bi] = bp->car;
	  bp = bp->cdr;
	}
      bv[bl] = (rx_side_effect)-1;

      {
	int ret;
	int x = 0;
	while (av[x] == bv[x])
	  ++x;
	ret = ((av[x] < bv[x]) ? -1 : 1);
	return ret;
      }
    }
}




/* re_compile_pattern is the GNU regular expression compiler: it
   compiles PATTERN (of length SIZE) and puts the result in RXB.
   Returns 0 if the pattern was valid, otherwise an error string.

   Assumes the `allocated' (and perhaps `buffer') and `translate' fields
   are set in RXB on entry.

   We call rx_compile to do the actual compilation.  */

#ifdef __STDC__
const char *
re_compile_pattern (const char *pattern,
		    int length,
		    struct re_pattern_buffer * rxb)
#else
const char *
re_compile_pattern (pattern, length, rxb)
     const char *pattern;
     int length;
     struct re_pattern_buffer * rxb;
#endif
{
  reg_errcode_t ret;

  /* GNU code is written to assume at least RE_NREGS registers will be set
     (and at least one extra will be -1).  */
  rxb->regs_allocated = REGS_UNALLOCATED;

  /* And GNU code determines whether or not to get register information
     by passing null for the REGS argument to re_match, etc., not by
     setting no_sub.  */
  rxb->no_sub = 0;

  rxb->rx.local_cset_size = 256;

  /* Match anchors at newline.  */
  rxb->newline_anchor = 1;
 
  rxb->re_nsub = 0;
  rxb->start = 0;
  rxb->se_params = 0;
  rxb->rx.nodec = 0;
  rxb->rx.epsnodec = 0;
  rxb->rx.instruction_table = 0;
  rxb->rx.nfa_states = 0;
  rxb->rx.se_list_cmp = posix_se_list_order;
  rxb->rx.start_set = 0;

  ret = rx_compile (pattern, length, re_syntax_options, rxb);
  alloca (0);
  return rx_error_msg[(int) ret];
}



#ifdef __STDC__
int
re_compile_fastmap (struct re_pattern_buffer * rxb)
#else
int
re_compile_fastmap (rxb)
     struct re_pattern_buffer * rxb;
#endif
{
  rx_blow_up_fastmap (rxb);
  return 0;
}




/* Entry points compatible with 4.2 BSD regex library.  We don't define
   them if this is an Emacs or POSIX compilation.  */

#if !defined (emacs) && !defined (_POSIX_SOURCE) && 0

/* BSD has one and only one pattern buffer.  */
static struct re_pattern_buffer rx_comp_buf;

#ifdef __STDC__
char *
re_comp (const char *s)
#else
char *
re_comp (s)
    const char *s;
#endif
{
  reg_errcode_t ret;

  if (!s)
    {
      if (!rx_comp_buf.buffer)
	return "No previous regular expression";
      return 0;
    }

  if (!rx_comp_buf.fastmap)
    {
      rx_comp_buf.fastmap = (char *) malloc (1 << BYTEWIDTH);
      if (!rx_comp_buf.fastmap)
	return "Memory exhausted";
    }

  /* Since `rx_exec' always passes NULL for the `regs' argument, we
     don't need to initialize the pattern buffer fields which affect it.  */

  /* Match anchors at newlines.  */
  rx_comp_buf.newline_anchor = 1;

  rx_comp_buf.re_nsub = 0;
  rx_comp_buf.start = 0;
  rx_comp_buf.se_params = 0;
  rx_comp_buf.rx.nodec = 0;
  rx_comp_buf.rx.epsnodec = 0;
  rx_comp_buf.rx.instruction_table = 0;
  rx_comp_buf.rx.nfa_states = 0;
  rx_comp_buf.rx.start = 0;
  rx_comp_buf.rx.se_list_cmp = posix_se_list_order;

  ret = rx_compile (s, strlen (s), rx_syntax_options, &rx_comp_buf);
  alloca (0);

  /* Yes, we're discarding `const' here.  */
  return (char *) rx_error_msg[(int) ret];
}


#ifdef __STDC__
int
rx_exec (const char *s)
#else
int
rx_exec (s)
    const char *s;
#endif
{
  const int len = strlen (s);
  return
    0 <= re_search (&rx_comp_buf, s, len, 0, len, (struct rx_registers *) 0);
}
#endif /* not emacs and not _POSIX_SOURCE */



/* POSIX.2 functions.  Don't define these for Emacs.  */

#if !defined(emacs)

/* regcomp takes a regular expression as a string and compiles it.

   PREG is a regex_t *.  We do not expect any fields to be initialized,
   since POSIX says we shouldn't.  Thus, we set

     `buffer' to the compiled pattern;
     `used' to the length of the compiled pattern;
     `syntax' to RE_SYNTAX_POSIX_EXTENDED if the
       REG_EXTENDED bit in CFLAGS is set; otherwise, to
       RE_SYNTAX_POSIX_BASIC;
     `newline_anchor' to REG_NEWLINE being set in CFLAGS;
     `fastmap' and `fastmap_accurate' to zero;
     `re_nsub' to the number of subexpressions in PATTERN.

   PATTERN is the address of the pattern string.

   CFLAGS is a series of bits which affect compilation.

     If REG_EXTENDED is set, we use POSIX extended syntax; otherwise, we
     use POSIX basic syntax.

     If REG_NEWLINE is set, then . and [^...] don't match newline.
     Also, regexec will try a match beginning after every newline.

     If REG_ICASE is set, then we considers upper- and lowercase
     versions of letters to be equivalent when matching.

     If REG_NOSUB is set, then when PREG is passed to regexec, that
     routine will report only success or failure, and nothing about the
     registers.

   It returns 0 if it succeeds, nonzero if it doesn't.  (See regex.h for
   the return codes and their meanings.)  */


#ifdef __STDC__
int
regcomp (regex_t * preg, const char * pattern, int cflags)
#else
int
regcomp (preg, pattern, cflags)
    regex_t * preg;
    const char * pattern;
    int cflags;
#endif
{
  reg_errcode_t ret;
  unsigned syntax
    = cflags & REG_EXTENDED ? RE_SYNTAX_POSIX_EXTENDED : RE_SYNTAX_POSIX_BASIC;

  /* regex_compile will allocate the space for the compiled pattern.  */
  preg->buffer = 0;
  preg->allocated = 0;

  preg->fastmap = malloc (256);
  if (!preg->fastmap)
    return REG_ESPACE;
  preg->fastmap_accurate = 0;

  if (cflags & REG_ICASE)
    {
      unsigned i;

      preg->translate = (unsigned char *) malloc (256);
      if (!preg->translate)
        return (int) REG_ESPACE;

      /* Map uppercase characters to corresponding lowercase ones.  */
      for (i = 0; i < CHAR_SET_SIZE; i++)
        preg->translate[i] = isupper (i) ? tolower (i) : i;
    }
  else
    preg->translate = 0;

  /* If REG_NEWLINE is set, newlines are treated differently.  */
  if (cflags & REG_NEWLINE)
    { /* REG_NEWLINE implies neither . nor [^...] match newline.  */
      syntax &= ~RE_DOT_NEWLINE;
      syntax |= RE_HAT_LISTS_NOT_NEWLINE;
      /* It also changes the matching behavior.  */
      preg->newline_anchor = 1;
    }
  else
    preg->newline_anchor = 0;

  preg->no_sub = !!(cflags & REG_NOSUB);

  /* POSIX says a null character in the pattern terminates it, so we
     can use strlen here in compiling the pattern.  */
  preg->re_nsub = 0;
  preg->start = 0;
  preg->se_params = 0;
  preg->rx.nodec = 0;
  preg->rx.epsnodec = 0;
  preg->rx.instruction_table = 0;
  preg->rx.nfa_states = 0;
  preg->rx.local_cset_size = 256;
  preg->rx.start = 0;
  preg->rx.se_list_cmp = posix_se_list_order;
  preg->rx.start_set = 0;
  ret = rx_compile (pattern, strlen (pattern), syntax, preg);
  alloca (0);

  /* POSIX doesn't distinguish between an unmatched open-group and an
     unmatched close-group: both are REG_EPAREN.  */
  if (ret == REG_ERPAREN) ret = REG_EPAREN;

  return (int) ret;
}


/* regexec searches for a given pattern, specified by PREG, in the
   string STRING.

   If NMATCH is zero or REG_NOSUB was set in the cflags argument to
   `regcomp', we ignore PMATCH.  Otherwise, we assume PMATCH has at
   least NMATCH elements, and we set them to the offsets of the
   corresponding matched substrings.

   EFLAGS specifies `execution flags' which affect matching: if
   REG_NOTBOL is set, then ^ does not match at the beginning of the
   string; if REG_NOTEOL is set, then $ does not match at the end.

   We return 0 if we find a match and REG_NOMATCH if not.  */

#ifdef __STDC__
int
regexec (const regex_t *preg, const char *string,
	 size_t nmatch, regmatch_t pmatch[],
	 int eflags)
#else
int
regexec (preg, string, nmatch, pmatch, eflags)
    const regex_t *preg;
    const char *string;
    size_t nmatch;
    regmatch_t pmatch[];
    int eflags;
#endif
{
  int ret;
  struct re_registers regs;
  regex_t private_preg;
  int len = strlen (string);
  boolean want_reg_info = !preg->no_sub && nmatch > 0;

  private_preg = *preg;

  private_preg.not_bol = !!(eflags & REG_NOTBOL);
  private_preg.not_eol = !!(eflags & REG_NOTEOL);

  /* The user has told us exactly how many registers to return
   * information about, via `nmatch'.  We have to pass that on to the
   * matching routines.
   */
  private_preg.regs_allocated = REGS_FIXED;

  if (want_reg_info)
    {
      regs.num_regs = nmatch;
      regs.start = TALLOC (nmatch, regoff_t);
      regs.end = TALLOC (nmatch, regoff_t);
      if (regs.start == 0 || regs.end == 0)
        return (int) REG_NOMATCH;
    }

  /* Perform the searching operation.  */
  ret = re_search (&private_preg,
		   string, len,
                   /* start: */ 0,
		   /* range: */ len,
                   want_reg_info ? &regs : (struct re_registers *) 0);

  /* Copy the register information to the POSIX structure.  */
  if (want_reg_info)
    {
      if (ret >= 0)
        {
          unsigned r;

          for (r = 0; r < nmatch; r++)
            {
              pmatch[r].rm_so = regs.start[r];
              pmatch[r].rm_eo = regs.end[r];
            }
        }

      /* If we needed the temporary register info, free the space now.  */
      free (regs.start);
      free (regs.end);
    }

  /* We want zero return to mean success, unlike `re_search'.  */
  return ret >= 0 ? (int) REG_NOERROR : (int) REG_NOMATCH;
}


/* Returns a message corresponding to an error code, ERRCODE, returned
   from either regcomp or regexec.   */

#ifdef __STDC__
size_t
regerror (int errcode, const regex_t *preg,
	  char *errbuf, size_t errbuf_size)
#else
size_t
regerror (errcode, preg, errbuf, errbuf_size)
    int errcode;
    const regex_t *preg;
    char *errbuf;
    size_t errbuf_size;
#endif
{
  const char *msg
    = rx_error_msg[errcode] == 0 ? "Success" : rx_error_msg[errcode];
  size_t msg_size = strlen (msg) + 1; /* Includes the 0.  */

  if (errbuf_size != 0)
    {
      if (msg_size > errbuf_size)
        {
          strncpy (errbuf, msg, errbuf_size - 1);
          errbuf[errbuf_size - 1] = 0;
        }
      else
        strcpy (errbuf, msg);
    }

  return msg_size;
}


/* Free dynamically allocated space used by PREG.  */

#ifdef __STDC__
void
regfree (regex_t *preg)
#else
void
regfree (preg)
    regex_t *preg;
#endif
{
  if (preg->buffer != 0)
    free (preg->buffer);
  preg->buffer = 0;
  preg->allocated = 0;

  if (preg->fastmap != 0)
    free (preg->fastmap);
  preg->fastmap = 0;
  preg->fastmap_accurate = 0;

  if (preg->translate != 0)
    free (preg->translate);
  preg->translate = 0;
}

#endif /* not emacs  */





