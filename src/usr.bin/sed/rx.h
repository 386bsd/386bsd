#if !defined(RXH) || defined(RX_WANT_SE_DEFS)
#define RXH

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
/*  t. lord	Wed Sep 23 18:20:57 1992	*/




#ifndef RX_WANT_SE_DEFS

/* This page: Bitsets */

#ifndef RX_subset
typedef unsigned int RX_subset;
#define RX_subset_bits	(32)
#define RX_subset_mask	(RX_subset_bits - 1)
#endif

typedef RX_subset * rx_Bitset;

#ifdef __STDC__
typedef void (*rx_bitset_iterator) (void *, int member_index);
#else
typedef void (*rx_bitset_iterator) ();
#endif

#define rx_bitset_subset(N)  ((N) / RX_subset_bits)
#define rx_bitset_subset_val(B,N)  ((B)[rx_bitset_subset(N)])
#define RX_bitset_access(B,N,OP) \
  ((B)[rx_bitset_subset(N)] OP rx_subset_singletons[(N) & RX_subset_mask])
#define RX_bitset_member(B,N)   RX_bitset_access(B, N, &)
#define RX_bitset_enjoin(B,N)   RX_bitset_access(B, N, |=)
#define RX_bitset_remove(B,N)   RX_bitset_access(B, N, &= ~)
#define RX_bitset_toggle(B,N)   RX_bitset_access(B, N, ^= )
#define rx_bitset_numb_subsets(N) (((N) + RX_subset_bits - 1) / RX_subset_bits)
#define rx_sizeof_bitset(N)	(rx_bitset_numb_subsets(N) * sizeof(RX_subset))



/* This page: Splay trees. */

#ifdef __STDC__
typedef int (*rx_sp_comparer) (void * a, void * b);
#else
typedef int (*rx_sp_comparer) ();
#endif

struct rx_sp_node 
{
  void * key;
  void * data;
  struct rx_sp_node * kids[2];
};

#ifdef __STDC__
typedef void (*rx_sp_key_data_freer) (struct rx_sp_node *);
#else
typedef void (*rx_sp_key_data_freer) ();
#endif


/* giant inflatable hash trees */

struct rx_hash_item
{
  struct rx_hash_item * next_same_hash;
  struct rx_hash * table;
  unsigned long hash;
  void * data;
  void * binding;
};

struct rx_hash
{
  struct rx_hash * parent;
  int refs;
  struct rx_hash * children[13];
  struct rx_hash_item * buckets [13];
  int bucket_size [13];
};

struct rx_hash_rules;

#ifdef __STDC__
/* should return like == */
typedef int (*rx_hash_eq)(void *, void *);
typedef struct rx_hash * (*rx_alloc_hash)(struct rx_hash_rules *);
typedef void (*rx_free_hash)(struct rx_hash *,
			    struct rx_hash_rules *);
typedef struct rx_hash_item * (*rx_alloc_hash_item)(struct rx_hash_rules *,
						    void *);
typedef void (*rx_free_hash_item)(struct rx_hash_item *,
				 struct rx_hash_rules *);
#else
typedef int (*rx_hash_eq)();
typedef struct rx_hash * (*rx_alloc_hash)();
typedef void (*rx_free_hash)();
typedef struct rx_hash_item * (*rx_alloc_hash_item)();
typedef void (*rx_free_hash_item)();
#endif

struct rx_hash_rules
{
  rx_hash_eq eq;
  rx_alloc_hash hash_alloc;
  rx_free_hash free_hash;
  rx_alloc_hash_item hash_item_alloc;
  rx_free_hash_item free_hash_item;
};



/* Matchers decide what to do by examining a series of these.
 * Instruction types are described below.
 */
struct rx_inx 
{
  void * inx;
  void * data;
  void * data_2;
  void * fnord;
};

/* Struct RX holds a compiled regular expression - that is, an nfa ready to be
 * converted on demand to a more efficient nfa.  This is for the low level interface.
 * The high-level interface incloses this in a `struct re_pattern_buffer'.
 */
struct rx_cache;
#ifdef __STDC__
struct rx_se_list;
struct rx;
typedef int (*rx_se_list_order) (struct rx *,
				 struct rx_se_list *, struct rx_se_list *);
#else
typedef int (*rx_se_list_order) ();
#endif

struct rx_superset;

struct rx
{
  int rx_id;		/* Every edition numbered and signed by eclose_nfa. */

  struct rx_cache * cache;  /* Where superstates come from */

  /* Every regex defines the size of its own character set. */
  int local_cset_size;

  void * buffer;		/* Malloced memory for the nfa. */
  unsigned long allocated;	/* Size of that memory. */

  /* How much buffer space to save for external uses.  After compilation,
   * this space will be available at (buffer + allocated - reserved)
   */
  unsigned long reserved;

  /* --------- The remaining fields are for internal use only. --------- */
  /* --------- But! they should be initialized to 0.	       --------- */
  /* NODEC is the number of nodes in the NFA with non-epsilon
   * orx transitions. 
   */
  int nodec;

  /* EPSNODEC is the number of nodes with only epsilon (orx) transitions. */
  int epsnodec;

  /* The sum of NODEC & EPSNODEC is the total number of states in the
   * compiled NFA.
   */

  /* side_effect_progs temporarily holds a tree of side effect lists. */
  struct rx_hash se_list_memo;

  /* A memo for sets of states in the possible_future lists of an nfa: */
  struct rx_hash set_list_memo;

  /* The instruction table is indexed by the enum of instructions defined in 
   * rxrun.h.  The values in the table are used to fill in the `inx'
   * slot of instruction frames (see rxrun.h).
   */
  void ** instruction_table;
  struct rx_nfa_state *nfa_states;
  struct rx_nfa_state *start;

  /* This orders the search through super-nfa paths. */
  rx_se_list_order se_list_cmp;

  struct rx_superset * start_set;
};

/* An RX NFA may contain epsilon edges labeled with side effects.
 * These side effects represent match actions that can not normally be
 * defined in a `pure' NFA; for example, recording the location at
 * which a paren is crossed in a register structure.  
 *
 * A matcher is supposed to find a particular path
 * through the NFA (such as leftmost-longest), and then to execute the
 * side effects along that path.  Operationally, the space of paths is
 * searched and side effects are carried out incrementally, and with
 * backtracking.
 *
 * As the NFA is manipulated during matching sets of side effects.
 * Simple lists are used to hold side effect lists. 
 */

typedef void * rx_side_effect;

struct rx_se_list
{
  rx_side_effect car;
  struct rx_se_list * cdr;
};



/* Struct rexp_node holds an expression tree that represents a regexp.
 * In this expression tree, every node has a type, and some parameters
 * appropriate to that type.
 */

enum rexp_node_type
{
  r_cset,			/* Match from a character set. `a' or `[a-z]'*/
  r_concat,			/* Concat two regexps.   `ab' */
  r_alternate,			/* Choose one of two regexps. `a\|b' */
  r_opt,			/* Optional regexp. `a?' */
  r_star,			/* Repeated regexp. `a*' */
  r_2phase_star,		/* hard to explain */
  r_side_effect,		/* Matches the empty string, but
				 * implies that a side effect must
				 * take place.  These nodes are used
				 * by the parser to implement parens,
				 * backreferences etc.
				 */

  r_data			/* R_DATA is soley for the convenience
				 * of parsers or other rexp
				 * transformers that want to
				 * (temporarily) introduce new node
				 * types in rexp structures.  These
				 * must be eliminated
			    	 * by the time build_nfa is called.
			  	 */
};

struct rexp_node
{
  enum rexp_node_type type;
  union
  {
    rx_Bitset cset;
    rx_side_effect side_effect;
    struct
      {
	struct rexp_node *left;
	struct rexp_node *right;
      } pair;
    void * data;
  } params;
};



/* This defines the structure of the NFA into which rexps are compiled. */

struct rx_nfa_state
{
  int id;		
  struct rx_nfa_edge *edges;
  struct rx_possible_future *futures;
  unsigned int is_final:1;
  unsigned int is_start:1;
  unsigned int eclosure_needed:1;
  struct rx_nfa_state *next;
  unsigned int mark:1;
};

enum rx_nfa_etype
{
  ne_cset,
  ne_epsilon,
  ne_side_effect		/* A special kind of epsilon. */
};

struct rx_nfa_edge
{
  struct rx_nfa_edge *next;
  enum rx_nfa_etype type;
  struct rx_nfa_state *dest;
  union
  {
    rx_Bitset cset;
    rx_side_effect side_effect;
  } params;
};

struct rx_nfa_state_set
{
  struct rx_nfa_state * car;
  struct rx_nfa_state_set * cdr;
};

struct rx_possible_future
{
  struct rx_possible_future *next;
  struct rx_se_list * effects;
  struct rx_nfa_state_set * destset;
};



enum rx_opcode
{
  /* 
   * BACKTRACK_POINT is invoked when a transition results in more
   * than one possible future.
   *
   * There is one occurence of this instruction per transition_class
   * structure; that occurence is only ever executed if the 
   * transition_class contains a list of more than 1 edge.
   */
  rx_backtrack_point = 0,	/* data is (struct transition_class *) */

  /* 
   * RX_DO_SIDE_EFFECTS evaluates the side effects of an epsilon path.
   * There is one occurence of this instruction per rx_distinct_future.
   * This instruction is skipped if a rx_distinct_future has no side effects.
   */
  rx_do_side_effects = rx_backtrack_point + 1,
  /* data is (struct rx_distinct_future *) */

  /* 
   * RX_CACHE_MISS instructions are stored in rx_distinct_futures whose
   * destination superstate has been reclaimed (or was never built).
   * It recomputes the destination superstate.
   * RX_CACHE_MISS is also stored in a superstate transition table before
   * any of its edges have been built.
   */
  rx_cache_miss = rx_do_side_effects + 1,
  /* data is (struct rx_distinct_future *) */

  /* 
   * RX_NEXT_CHAR is called to consume the next character and take the
   * corresponding transition.  This is the only instruction that uses 
   * the DATA field of the instruction frame instead of DATA_2.
   * (see EXPLORE_FUTURE in regex.c).
   */
  rx_next_char = rx_cache_miss + 1, /* data is (struct superstate *) */

  /* RX_BACKTRACK indicates that a transition fails.
   */
  rx_backtrack = rx_next_char + 1, /* no data */

  /* 
   * RX_ERROR_INX is stored only in places that should never be executed.
   */
  rx_error_inx = rx_backtrack + 1, /* Not supposed to occur. */

  rx_num_instructions = rx_error_inx + 1
};

/* An id_instruction_table holds the values stored in instruction
 * frames.  The table is indexed by the enums declared above.
 */
extern void * rx_id_instruction_table[rx_num_instructions];

#if 0				/* Already declared way above. */
/*  If the instruction is `rx_next_char' then data is valid.  Otherwise it's 0
 *  and data_2 is valid.
 */
struct rx_inx 
{
  void * inx;
  void * data;
  void * data_2;
};
#endif


#ifndef RX_TAIL_ARRAY
#define RX_TAIL_ARRAY  1
#endif

/* A superstate corresponds to a set of nfa states.  Those sets are
 * represented by STRUCT RX_SUPERSET.  The constructors
 * guarantee that only one (shared) structure is created for a given set.
 */
struct rx_superset
{
  int refs;
  struct rx_nfa_state * car;	/* May or may not be a valid addr. */
  int id;			/* == car->id for the initial value of *car */
  struct rx_superset * cdr;	/* May be NULL or a live or semifreed super*/

  /* If the corresponding superstate exists: */
  struct rx_superstate * superstate;

  /* If this is a starting state (as built by re_search_2)
   * this points to the `struct rx'.  The memory for these objects
   * is typed -- so even after they are freed it is safe to look
   * at this field (to check, in fact, if this was freed.)
   */
  struct rx * starts_for;

  struct rx_hash_item hash_item;
};

#define rx_protect_superset(RX,CON) (++(CON)->refs)

/* Every character occurs in at most one super edge per super-state.
 * But, that edge might have more than one option, indicating a point
 * of non-determinism. 
 */
struct rx_super_edge
{
  struct rx_super_edge *next;
  struct rx_inx rx_backtrack_frame;
  int cset_size;
  rx_Bitset cset;
  struct rx_distinct_future *options;
};

/* A superstate is a set of nfa states (RX_SUPERSET) along
 * with a transition table.  Superstates are built on demand and reclaimed
 * without warning.  To protect a superstate, use LOCK_SUPERSTATE.
 *
 * Joe Keane thought of calling these superstates and several people
 * have commented on what a good name it is for what they do. 
 */
struct rx_superstate
{
  int rx_id;
  int locks;
  struct rx_superstate * next_recyclable;
  struct rx_superstate * prev_recyclable;
  struct rx_distinct_future * transition_refs;
  struct rx_superset * contents;
  struct rx_super_edge * edges;
  int is_semifree;
  int trans_size;
  struct rx_inx transitions[RX_TAIL_ARRAY]; /* cset sized */
};

struct rx_distinct_future
{
  struct rx_distinct_future * next_same_super_edge[2];
  struct rx_distinct_future * next_same_dest;
  struct rx_distinct_future * prev_same_dest;
  struct rx_superstate * present;	/* source state */
  struct rx_superstate * future;	/* destination state */
  struct rx_super_edge * edge;
  struct rx_inx future_frame;
  struct rx_inx side_effects_frame;
  struct rx_se_list * effects;
};

#define rx_lock_superstate(R,S)  ((S)->locks++)
#define rx_unlock_superstate(R,S) (--(S)->locks)


/* This page destined for rx.h */

struct rx_blocklist
{
  struct rx_blocklist * next;
  int bytes;
};

struct rx_freelist
{
  struct rx_freelist * next;
};

struct rx_cache;

#ifdef __STDC__
typedef void (*rx_morecore_fn)(struct rx_cache *);
#else
typedef void (*rx_morecore_fn)();
#endif

struct rx_cache
{
  struct rx_hash_rules superset_hash_rules;

  /* Objects are allocated by incrementing a pointer that 
   * scans across rx_blocklists.
   */
  struct rx_blocklist * memory;
  struct rx_blocklist * memory_pos;
  int bytes_left;
  char * memory_addr;
  rx_morecore_fn morecore;

  /* Freelists. */
  struct rx_freelist * free_superstates;
  struct rx_freelist * free_transition_classes;
  struct rx_freelist * free_discernable_futures;
  struct rx_freelist * free_supersets;
  struct rx_freelist * free_hash;

  /* Two sets of superstates -- those that are semifreed, and those
   * that are being used.
   */
  struct rx_superstate * lru_superstate;
  struct rx_superstate * semifree_superstate;

  struct rx_superset * empty_superset;

  int superstates;
  int semifree_superstates;
  int hits;
  int misses;
  int superstates_allowed;

  int local_cset_size;
  void ** instruction_table;

  struct rx_hash superset_table;
};



/* regex.h
 * 
 * The remaining declarations replace regex.h.
 */

/* This is an array of error messages corresponding to the error codes.
 */
extern const char *re_error_msg[];

/* If any error codes are removed, changed, or added, update the
   `re_error_msg' table in regex.c.  */
typedef enum
{
  REG_NOERROR = 0,	/* Success.  */
  REG_NOMATCH,		/* Didn't find a match (for regexec).  */

  /* POSIX regcomp return error codes.  (In the order listed in the
     standard.)  */
  REG_BADPAT,		/* Invalid pattern.  */
  REG_ECOLLATE,		/* Not implemented.  */
  REG_ECTYPE,		/* Invalid character class name.  */
  REG_EESCAPE,		/* Trailing backslash.  */
  REG_ESUBREG,		/* Invalid back reference.  */
  REG_EBRACK,		/* Unmatched left bracket.  */
  REG_EPAREN,		/* Parenthesis imbalance.  */ 
  REG_EBRACE,		/* Unmatched \{.  */
  REG_BADBR,		/* Invalid contents of \{\}.  */
  REG_ERANGE,		/* Invalid range end.  */
  REG_ESPACE,		/* Ran out of memory.  */
  REG_BADRPT,		/* No preceding re for repetition op.  */

  /* Error codes we've added.  */
  REG_EEND,		/* Premature end.  */
  REG_ESIZE,		/* Compiled pattern bigger than 2^16 bytes.  */
  REG_ERPAREN		/* Unmatched ) or \); not returned from regcomp.  */
} reg_errcode_t;

/* The regex.c support, as a client of rx, defines a set of possible
 * side effects that can be added to the edge lables of nfa edges.
 * Here is the list of sidef effects in use.
 */

enum re_side_effects
{
#define RX_WANT_SE_DEFS 1
#undef RX_DEF_SE
#undef RX_DEF_CPLX_SE
#define RX_DEF_SE(IDEM, NAME, VALUE)	      NAME VALUE,
#define RX_DEF_CPLX_SE(IDEM, NAME, VALUE)     NAME VALUE,
#include "rx.h"
#undef RX_DEF_SE
#undef RX_DEF_CPLX_SE
#undef RX_WANT_SE_DEFS
   re_floogle_flap = 65533
};

/* These hold paramaters for the kinds of side effects that are possible
 * in the supported pattern languages.  These include things like the 
 * numeric bounds of {} operators and the index of paren registers for 
 * subexpression measurement or backreferencing.
 */
struct re_se_params
{
  enum re_side_effects se;
  int op1;
  int op2;
};

typedef unsigned reg_syntax_t;

struct re_pattern_buffer
{
  struct rx rx;
  reg_syntax_t syntax;		/* See below for syntax bit definitions. */

  unsigned int no_sub:1;	/* If set, don't  return register offsets. */
  unsigned int not_bol:1;	/* If set, the anchors ('^' and '$') don't */
  unsigned int not_eol:1;	/*     match at the ends of the string.  */  
  unsigned int newline_anchor:1;/* If true, an anchor at a newline matches.*/
  unsigned int least_subs:1;	/* If set, and returning registers, return
				 * as few values as possible.  Only 
				 * backreferenced groups and group 0 (the whole
				 * match) will be returned.
				 */

  /* If true, this says that the matcher should keep registers on its
   * backtracking stack.  For many patterns, we can easily determine that
   * this isn't necessary.
   */
  unsigned int match_regs_on_stack:1;
  unsigned int search_regs_on_stack:1;

  /* is_anchored and begbuf_only are filled in by rx_compile. */
  unsigned int is_anchored:1;	/* Anchorded by ^? */
  unsigned int begbuf_only:1;	/* Anchored to char position 0? */

  
  /* If REGS_UNALLOCATED, allocate space in the `regs' structure
   * for `max (RE_NREGS, re_nsub + 1)' groups.
   * If REGS_REALLOCATE, reallocate space if necessary.
   * If REGS_FIXED, use what's there.  
   */
#define REGS_UNALLOCATED 0
#define REGS_REALLOCATE 1
#define REGS_FIXED 2
  unsigned int regs_allocated:2;

  
  /* Either a translate table to apply to all characters before
   * comparing them, or zero for no translation.  The translation
   * is applied to a pattern when it is compiled and to a string
   * when it is matched.
   */
  char * translate;

  /* If this is a valid pointer, it tells rx not to store the extents of 
   * certain subexpressions (those corresponding to non-zero entries).
   * Passing 0x1 is the same as passing an array of all ones.  Passing 0x0
   * is the same as passing an array of all zeros.
   * The array should contain as many entries as their are subexps in the 
   * regexp.
   */
  char * syntax_parens;

	/* Number of subexpressions found by the compiler.  */
  size_t re_nsub;

  void * buffer;		/* Malloced memory for the nfa. */
  unsigned long allocated;	/* Size of that memory. */

  /* Pointer to a fastmap, if any, otherwise zero.  re_search uses
   * the fastmap, if there is one, to skip over impossible
   * starting points for matches.  */
  char *fastmap;

  unsigned int fastmap_accurate:1; /* These three are internal. */
  unsigned int can_match_empty:1;  
  struct rx_nfa_state * start;	/* The nfa starting state. */

  /* This is the list of iterator bounds for {lo,hi} constructs.
   * The memory pointed to is part of the rx->buffer.
   */
  struct re_se_params *se_params;

  /* This is a bitset representation of the fastmap.
   * This is a true fastmap that already takes the translate
   * table into account.
   */
  rx_Bitset fastset;
};

/* Type for byte offsets within the string.  POSIX mandates this.  */
typedef int regoff_t;

/* This is the structure we store register match data in.  See
   regex.texinfo for a full description of what registers match.  */
struct re_registers
{
  unsigned num_regs;
  regoff_t *start;
  regoff_t *end;
};

typedef struct re_pattern_buffer regex_t;

/* POSIX specification for registers.  Aside from the different names than
   `re_registers', POSIX uses an array of structures, instead of a
   structure of arrays.  */
typedef struct
{
  regoff_t rm_so;  /* Byte offset from string's start to substring's start.  */
  regoff_t rm_eo;  /* Byte offset from string's start to substring's end.  */
} regmatch_t;


/* The following bits are used to determine the regexp syntax we
   recognize.  The set/not-set meanings are chosen so that Emacs syntax
   remains the value 0.  The bits are given in alphabetical order, and
   the definitions shifted by one from the previous bit; thus, when we
   add or remove a bit, only one other definition need change.  */

/* If this bit is not set, then \ inside a bracket expression is literal.
   If set, then such a \ quotes the following character.  */
#define RE_BACKSLASH_ESCAPE_IN_LISTS (1)

/* If this bit is not set, then + and ? are operators, and \+ and \? are
     literals. 
   If set, then \+ and \? are operators and + and ? are literals.  */
#define RE_BK_PLUS_QM (RE_BACKSLASH_ESCAPE_IN_LISTS << 1)

/* If this bit is set, then character classes are supported.  They are:
     [:alpha:], [:upper:], [:lower:],  [:digit:], [:alnum:], [:xdigit:],
     [:space:], [:print:], [:punct:], [:graph:], and [:cntrl:].
   If not set, then character classes are not supported.  */
#define RE_CHAR_CLASSES (RE_BK_PLUS_QM << 1)

/* If this bit is set, then ^ and $ are always anchors (outside bracket
     expressions, of course).
   If this bit is not set, then it depends:
        ^  is an anchor if it is at the beginning of a regular
           expression or after an open-group or an alternation operator;
        $  is an anchor if it is at the end of a regular expression, or
           before a close-group or an alternation operator.  

   This bit could be (re)combined with RE_CONTEXT_INDEP_OPS, because
   POSIX draft 11.2 says that * etc. in leading positions is undefined.
   We already implemented a previous draft which made those constructs
   invalid, though, so we haven't changed the code back.  */
#define RE_CONTEXT_INDEP_ANCHORS (RE_CHAR_CLASSES << 1)

/* If this bit is set, then special characters are always special
     regardless of where they are in the pattern.
   If this bit is not set, then special characters are special only in
     some contexts; otherwise they are ordinary.  Specifically, 
     * + ? and intervals are only special when not after the beginning,
     open-group, or alternation operator.  */
#define RE_CONTEXT_INDEP_OPS (RE_CONTEXT_INDEP_ANCHORS << 1)

/* If this bit is set, then *, +, ?, and { cannot be first in an re or
     immediately after an alternation or begin-group operator.  */
#define RE_CONTEXT_INVALID_OPS (RE_CONTEXT_INDEP_OPS << 1)

/* If this bit is set, then . matches newline.
   If not set, then it doesn't.  */
#define RE_DOT_NEWLINE (RE_CONTEXT_INVALID_OPS << 1)

/* If this bit is set, then . doesn't match NUL.
   If not set, then it does.  */
#define RE_DOT_NOT_NULL (RE_DOT_NEWLINE << 1)

/* If this bit is set, nonmatching lists [^...] do not match newline.
   If not set, they do.  */
#define RE_HAT_LISTS_NOT_NEWLINE (RE_DOT_NOT_NULL << 1)

/* If this bit is set, either \{...\} or {...} defines an
     interval, depending on RE_NO_BK_BRACES. 
   If not set, \{, \}, {, and } are literals.  */
#define RE_INTERVALS (RE_HAT_LISTS_NOT_NEWLINE << 1)

/* If this bit is set, +, ? and | aren't recognized as operators.
   If not set, they are.  */
#define RE_LIMITED_OPS (RE_INTERVALS << 1)

/* If this bit is set, newline is an alternation operator.
   If not set, newline is literal.  */
#define RE_NEWLINE_ALT (RE_LIMITED_OPS << 1)

/* If this bit is set, then `{...}' defines an interval, and \{ and \}
     are literals.
  If not set, then `\{...\}' defines an interval.  */
#define RE_NO_BK_BRACES (RE_NEWLINE_ALT << 1)

/* If this bit is set, (...) defines a group, and \( and \) are literals.
   If not set, \(...\) defines a group, and ( and ) are literals.  */
#define RE_NO_BK_PARENS (RE_NO_BK_BRACES << 1)

/* If this bit is set, then \<digit> matches <digit>.
   If not set, then \<digit> is a back-reference.  */
#define RE_NO_BK_REFS (RE_NO_BK_PARENS << 1)

/* If this bit is set, then | is an alternation operator, and \| is literal. 
   If not set, then \| is an alternation operator, and | is literal.  */
#define RE_NO_BK_VBAR (RE_NO_BK_REFS << 1)

/* If this bit is set, then an ending range point collating higher
     than the starting range point, as in [z-a], is invalid.
   If not set, then when ending range point collates higher than the
     starting range point, the range is ignored.  */
#define RE_NO_EMPTY_RANGES (RE_NO_BK_VBAR << 1)

/* If this bit is set, then an unmatched ) is ordinary.
   If not set, then an unmatched ) is invalid.  */
#define RE_UNMATCHED_RIGHT_PAREN_ORD (RE_NO_EMPTY_RANGES << 1)

/* This global variable defines the particular regexp syntax to use (for
   some interfaces).  When a regexp is compiled, the syntax used is
   stored in the pattern buffer, so changing this does not affect
   already-compiled regexps.  */
extern reg_syntax_t re_syntax_options;

/* Define combinations of the above bits for the standard possibilities.
   (The [[[ comments delimit what gets put into the Texinfo file, so
   don't delete them!)  */ 
/* [[[begin syntaxes]]] */
#define RE_SYNTAX_EMACS 0

#define RE_SYNTAX_AWK							\
  (RE_BACKSLASH_ESCAPE_IN_LISTS | RE_DOT_NOT_NULL			\
   | RE_NO_BK_PARENS            | RE_NO_BK_REFS				\
   | RE_NO_BK_VAR               | RE_NO_EMPTY_RANGES			\
   | RE_UNMATCHED_RIGHT_PAREN_ORD)

#define RE_SYNTAX_POSIX_AWK 						\
  (RE_SYNTAX_POSIX_EXTENDED | RE_BACKSLASH_ESCAPE_IN_LISTS)

#define RE_SYNTAX_GREP							\
  (RE_BK_PLUS_QM              | RE_CHAR_CLASSES				\
   | RE_HAT_LISTS_NOT_NEWLINE | RE_INTERVALS				\
   | RE_NEWLINE_ALT)

#define RE_SYNTAX_EGREP							\
  (RE_CHAR_CLASSES        | RE_CONTEXT_INDEP_ANCHORS			\
   | RE_CONTEXT_INDEP_OPS | RE_HAT_LISTS_NOT_NEWLINE			\
   | RE_NEWLINE_ALT       | RE_NO_BK_PARENS				\
   | RE_NO_BK_VBAR)

#define RE_SYNTAX_POSIX_EGREP						\
  (RE_SYNTAX_EGREP | RE_INTERVALS | RE_NO_BK_BRACES)

#define RE_SYNTAX_SED RE_SYNTAX_POSIX_BASIC

/* Syntax bits common to both basic and extended POSIX regex syntax.  */
#define _RE_SYNTAX_POSIX_COMMON						\
  (RE_CHAR_CLASSES | RE_DOT_NEWLINE      | RE_DOT_NOT_NULL		\
   | RE_INTERVALS  | RE_NO_EMPTY_RANGES)

#define RE_SYNTAX_POSIX_BASIC						\
  (_RE_SYNTAX_POSIX_COMMON | RE_BK_PLUS_QM)

/* Differs from ..._POSIX_BASIC only in that RE_BK_PLUS_QM becomes
   RE_LIMITED_OPS, i.e., \? \+ \| are not recognized.  Actually, this
   isn't minimal, since other operators, such as \`, aren't disabled.  */
#define RE_SYNTAX_POSIX_MINIMAL_BASIC					\
  (_RE_SYNTAX_POSIX_COMMON | RE_LIMITED_OPS)

#define RE_SYNTAX_POSIX_EXTENDED					\
  (_RE_SYNTAX_POSIX_COMMON | RE_CONTEXT_INDEP_ANCHORS			\
   | RE_CONTEXT_INDEP_OPS  | RE_NO_BK_BRACES				\
   | RE_NO_BK_PARENS       | RE_NO_BK_VBAR				\
   | RE_UNMATCHED_RIGHT_PAREN_ORD)

/* Differs from ..._POSIX_EXTENDED in that RE_CONTEXT_INVALID_OPS
   replaces RE_CONTEXT_INDEP_OPS and RE_NO_BK_REFS is added.  */
#define RE_SYNTAX_POSIX_MINIMAL_EXTENDED				\
  (_RE_SYNTAX_POSIX_COMMON  | RE_CONTEXT_INDEP_ANCHORS			\
   | RE_CONTEXT_INVALID_OPS | RE_NO_BK_BRACES				\
   | RE_NO_BK_PARENS        | RE_NO_BK_REFS				\
   | RE_NO_BK_VBAR	    | RE_UNMATCHED_RIGHT_PAREN_ORD)
/* [[[end syntaxes]]] */

/* Maximum number of duplicates an interval can allow.  Some systems
   (erroneously) define this in other header files, but we want our
   value, so remove any previous define.  */
#ifdef RE_DUP_MAX
#undef RE_DUP_MAX
#endif
#define RE_DUP_MAX ((1 << 15) - 1) 



/* POSIX `cflags' bits (i.e., information for `regcomp').  */

/* If this bit is set, then use extended regular expression syntax.
   If not set, then use basic regular expression syntax.  */
#define REG_EXTENDED 1

/* If this bit is set, then ignore case when matching.
   If not set, then case is significant.  */
#define REG_ICASE (REG_EXTENDED << 1)
 
/* If this bit is set, then anchors do not match at newline
     characters in the string.
   If not set, then anchors do match at newlines.  */
#define REG_NEWLINE (REG_ICASE << 1)

/* If this bit is set, then report only success or fail in regexec.
   If not set, then returns differ between not matching and errors.  */
#define REG_NOSUB (REG_NEWLINE << 1)


/* POSIX `eflags' bits (i.e., information for regexec).  */

/* If this bit is set, then the beginning-of-line operator doesn't match
     the beginning of the string (presumably because it's not the
     beginning of a line).
   If not set, then the beginning-of-line operator does match the
     beginning of the string.  */
#define REG_NOTBOL 1

/* Like REG_NOTBOL, except for the end-of-line.  */
#define REG_NOTEOL (1 << 1)

/* If `regs_allocated' is REGS_UNALLOCATED in the pattern buffer,
 * `re_match_2' returns information about at least this many registers
 * the first time a `regs' structure is passed. 
 *
 * Also, this is the greatest number of backreferenced subexpressions
 * allowed in a pattern being matched without caller-supplied registers.
 */
#ifndef RE_NREGS
#define RE_NREGS 30
#endif

extern int rx_cache_bound;

#ifdef __STDC__
extern reg_errcode_t rx_compile (const char *pattern, int size,
	    reg_syntax_t syntax,
	    struct re_pattern_buffer * rxb) ;
extern int re_search_2 (struct re_pattern_buffer *rxb,
	     const char * string1, int size1,
	     const char * string2, int size2,
	     int startpos, int range,
	     struct re_registers *regs,
	     int stop);
extern int re_search (struct re_pattern_buffer * rxb, const char *string,
	   int size, int startpos, int range,
	   struct re_registers *regs);
extern int re_match_2 (struct re_pattern_buffer * rxb,
	    const char * string1, int size1,
	    const char * string2, int size2,
	    int pos, struct re_registers *regs, int stop);
extern int re_match (struct re_pattern_buffer * rxb,
	  const char * string,
	  int size, int pos,
	  struct re_registers *regs);
extern reg_syntax_t re_set_syntax (reg_syntax_t syntax);
extern void re_set_registers (struct re_pattern_buffer *bufp,
		  struct re_registers *regs,
		  unsigned num_regs,
		  regoff_t * starts, regoff_t * ends);
extern const char * re_compile_pattern (const char *pattern,
		    int length,
		    struct re_pattern_buffer * rxb);
extern int re_compile_fastmap (struct re_pattern_buffer * rxb);
extern char * re_comp (const char *s);
extern int rx_exec (const char *s);
extern int regcomp (regex_t * preg, const char * pattern, int cflags);
extern int regexec (const regex_t *preg, const char *string,
	 size_t nmatch, regmatch_t pmatch[],
	 int eflags);
extern size_t regerror (int errcode, const regex_t *preg,
	  char *errbuf, size_t errbuf_size);
extern void regfree (regex_t *preg);

#else
extern reg_errcode_t rx_compile ();
extern int re_search_2 ();
extern int re_search ();
extern int re_match_2 ();
extern int re_match ();
extern reg_syntax_t re_set_syntax ();
extern void re_set_registers ();
extern const char * re_compile_pattern ();
extern int re_compile_fastmap ();
extern char * re_comp ();
extern int rx_exec ();
extern int regcomp ();
extern int regexec ();
extern size_t regerror ();
extern void regfree ();

#endif


#else /* RX_WANT_SE_DEFS */
  /* Integers are used to represent side effects.
   *
   * Simple side effects are given negative integer names by these enums.
   * 
   * Non-negative names are reserved for complex effects.
   *
   * Complex effects are those that take arguments.  For example, 
   * a register assignment associated with a group is complex because
   * it requires an argument to tell which group is being matched.
   * 
   * The integer name of a complex effect is an index into rxb->se_params.
   */
 
  RX_DEF_SE(1, re_se_try, = -1)		/* Epsilon from start state */

  RX_DEF_SE(0, re_se_pushback, = re_se_try - 1)
  RX_DEF_SE(0, re_se_push0, = re_se_pushback -1)
  RX_DEF_SE(0, re_se_pushpos, = re_se_push0 - 1)
  RX_DEF_SE(0, re_se_chkpos, = re_se_pushpos -1)
  RX_DEF_SE(0, re_se_poppos, = re_se_chkpos - 1)

  RX_DEF_SE(1, re_se_at_dot, = re_se_poppos - 1)	/* Emacs only */
  RX_DEF_SE(0, re_se_syntax, = re_se_at_dot - 1) /* Emacs only */
  RX_DEF_SE(0, re_se_not_syntax, = re_se_syntax - 1) /* Emacs only */

  RX_DEF_SE(1, re_se_begbuf, = re_se_not_syntax - 1) /* match beginning of buffer */
  RX_DEF_SE(1, re_se_hat, = re_se_begbuf - 1) /* match beginning of line */

  RX_DEF_SE(1, re_se_wordbeg, = re_se_hat - 1) 
  RX_DEF_SE(1, re_se_wordbound, = re_se_wordbeg - 1)
  RX_DEF_SE(1, re_se_notwordbound, = re_se_wordbound - 1)

  RX_DEF_SE(1, re_se_wordend, = re_se_notwordbound - 1)
  RX_DEF_SE(1, re_se_endbuf, = re_se_wordend - 1)

  /* This fails except at the end of a line. 
   * It deserves to go here since it is typicly one of the last steps 
   * in a match.
   */
  RX_DEF_SE(1, re_se_dollar, = re_se_endbuf - 1)

  /* Simple effects: */
  RX_DEF_SE(1, re_se_fail, = re_se_dollar - 1)

  /* Complex effects.  These are used in the 'se' field of 
   * a struct re_se_params.  Indexes into the se array
   * are stored as instructions on nfa edges.
   */
  RX_DEF_CPLX_SE(1, re_se_win, = 0)
  RX_DEF_CPLX_SE(1, re_se_lparen, = re_se_win + 1)
  RX_DEF_CPLX_SE(1, re_se_rparen, = re_se_lparen + 1)
  RX_DEF_CPLX_SE(0, re_se_backref, = re_se_rparen + 1)
  RX_DEF_CPLX_SE(0, re_se_iter, = re_se_backref + 1) 
  RX_DEF_CPLX_SE(0, re_se_end_iter, = re_se_iter + 1)
  RX_DEF_CPLX_SE(0, re_se_tv, = re_se_end_iter + 1)

#endif

#endif
