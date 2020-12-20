/* Process declarations and variables for C compiler.
   Copyright (C) 1988, 1992, 1993 Free Software Foundation, Inc.
   Hacked by Michael Tiemann (tiemann@cygnus.com)

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */


/* Process declarations and symbol lookup for C front end.
   Also constructs types; the standard scalar types at initialization,
   and structure, union, array and enum types when they are declared.  */

/* ??? not all decl nodes are given the most useful possible
   line numbers.  For example, the CONST_DECLs for enum values.  */

#include <stdio.h>
#include "config.h"
#include "tree.h"
#include "rtl.h"
#include "flags.h"
#include "cp-tree.h"
#include "cp-decl.h"
#include "cp-lex.h"
#include <sys/types.h>
#include <signal.h>
#include "obstack.h"

#define obstack_chunk_alloc xmalloc
#define obstack_chunk_free free

extern struct obstack permanent_obstack;

/* Stack of places to restore the search obstack back to.  */
   
/* Obstack used for remembering local class declarations (like
   enums and static (const) members.  */
#include "stack.h"
static struct obstack decl_obstack;
static struct stack_level *decl_stack;

#undef NULL
#define NULL (char *)0

#ifndef CHAR_TYPE_SIZE
#define CHAR_TYPE_SIZE BITS_PER_UNIT
#endif

#ifndef SHORT_TYPE_SIZE
#define SHORT_TYPE_SIZE (BITS_PER_UNIT * MIN ((UNITS_PER_WORD + 1) / 2, 2))
#endif

#ifndef INT_TYPE_SIZE
#define INT_TYPE_SIZE BITS_PER_WORD
#endif

#ifndef LONG_TYPE_SIZE
#define LONG_TYPE_SIZE BITS_PER_WORD
#endif

#ifndef LONG_LONG_TYPE_SIZE
#define LONG_LONG_TYPE_SIZE (BITS_PER_WORD * 2)
#endif

#ifndef WCHAR_UNSIGNED
#define WCHAR_UNSIGNED 0
#endif

#ifndef FLOAT_TYPE_SIZE
#define FLOAT_TYPE_SIZE BITS_PER_WORD
#endif

#ifndef DOUBLE_TYPE_SIZE
#define DOUBLE_TYPE_SIZE (BITS_PER_WORD * 2)
#endif

#ifndef LONG_DOUBLE_TYPE_SIZE
#define LONG_DOUBLE_TYPE_SIZE (BITS_PER_WORD * 2)
#endif

/* We let tm.h override the types used here, to handle trivial differences
   such as the choice of unsigned int or long unsigned int for size_t.
   When machines start needing nontrivial differences in the size type,
   it would be best to do something here to figure out automatically
   from other information what type to use.  */

#ifndef SIZE_TYPE
#define SIZE_TYPE "long unsigned int"
#endif

#ifndef PTRDIFF_TYPE
#define PTRDIFF_TYPE "long int"
#endif

#ifndef WCHAR_TYPE
#define WCHAR_TYPE "int"
#endif

#define builtin_function(NAME, TYPE, CODE, LIBNAME) \
  define_function (NAME, TYPE, CODE, (void (*)())pushdecl, LIBNAME)
#define auto_function(NAME, TYPE, CODE) \
  do {					\
    tree __name = NAME;		\
    tree __type = TYPE;			\
    define_function (IDENTIFIER_POINTER (__name), __type, CODE,	\
		     (void (*)())push_overloaded_decl_1,	\
		     IDENTIFIER_POINTER (build_decl_overload (__name, TYPE_ARG_TYPES (__type), 0)));\
  } while (0)

static tree grokparms				PROTO((tree, int));
static tree lookup_nested_type			PROTO((tree, tree));
static char *redeclaration_error_message	PROTO((tree, tree));
static int parmlist_is_random			PROTO((tree));
static void grok_op_properties			PROTO((tree, int));
static void expand_static_init			PROTO((tree, tree));
static void deactivate_exception_cleanups	PROTO((void));

tree define_function				PROTO((char *, tree, enum built_in_function, void (*)(), char *));

/* a node which has tree code ERROR_MARK, and whose type is itself.
   All erroneous expressions are replaced with this node.  All functions
   that accept nodes as arguments should avoid generating error messages
   if this node is one of the arguments, since it is undesirable to get
   multiple error messages from one error in the input.  */

tree error_mark_node;

/* Erroneous argument lists can use this *IFF* they do not modify it.  */
tree error_mark_list;

/* INTEGER_TYPE and REAL_TYPE nodes for the standard data types */

tree short_integer_type_node;
tree integer_type_node;
tree long_integer_type_node;
tree long_long_integer_type_node;

tree short_unsigned_type_node;
tree unsigned_type_node;
tree long_unsigned_type_node;
tree long_long_unsigned_type_node;

tree ptrdiff_type_node;

tree unsigned_char_type_node;
tree signed_char_type_node;
tree char_type_node;
tree wchar_type_node;
tree signed_wchar_type_node;
tree unsigned_wchar_type_node;

tree float_type_node;
tree double_type_node;
tree long_double_type_node;

tree intQI_type_node;
tree intHI_type_node;
tree intSI_type_node;
tree intDI_type_node;

tree unsigned_intQI_type_node;
tree unsigned_intHI_type_node;
tree unsigned_intSI_type_node;
tree unsigned_intDI_type_node;

/* a VOID_TYPE node, and the same, packaged in a TREE_LIST.  */

tree void_type_node, void_list_node;
tree void_zero_node;

/* Nodes for types `void *' and `const void *'.  */

tree ptr_type_node, const_ptr_type_node;

/* Nodes for types `char *' and `const char *'.  */

tree string_type_node, const_string_type_node;

/* Type `char[256]' or something like it.
   Used when an array of char is needed and the size is irrelevant.  */

tree char_array_type_node;

/* Type `int[256]' or something like it.
   Used when an array of int needed and the size is irrelevant.  */

tree int_array_type_node;

/* Type `wchar_t[256]' or something like it.
   Used when a wide string literal is created.  */

tree wchar_array_type_node;

/* type `int ()' -- used for implicit declaration of functions.  */

tree default_function_type;

/* function types `double (double)' and `double (double, double)', etc.  */

tree double_ftype_double, double_ftype_double_double;
tree int_ftype_int, long_ftype_long;

/* Function type `void (void *, void *, int)' and similar ones.  */

tree void_ftype_ptr_ptr_int, int_ftype_ptr_ptr_int, void_ftype_ptr_int_int;

/* Function type `char *(char *, char *)' and similar ones */
tree string_ftype_ptr_ptr, int_ftype_string_string;

/* Function type `size_t (const char *)' */
tree sizet_ftype_string;

/* Function type `int (const void *, const void *, size_t)' */
tree int_ftype_cptr_cptr_sizet;

/* C++ extensions */
tree vtable_entry_type;
tree __t_desc_type_node, __i_desc_type_node, __m_desc_type_node;
tree __t_desc_array_type, __i_desc_array_type, __m_desc_array_type;
tree class_star_type_node;
tree class_type_node, record_type_node, union_type_node, enum_type_node;
tree exception_type_node, unknown_type_node;
tree maybe_gc_cleanup;

/* Used for virtual function tables.  */
tree vtbl_mask;

/* Array type `(void *)[]' */
tree vtbl_type_node;

/* Static decls which do not have static initializers have no
   initializers as far as GNU C is concerned.  EMPTY_INIT_NODE
   is a static initializer which makes varasm code place the decl
   in data rather than in bss space.  Such gymnastics are necessary
   to avoid the problem that the linker will not include a library
   file if all the library appears to contribute are bss variables.  */

tree empty_init_node;

/* In a destructor, the point at which all derived class destroying
   has been done, just before any base class destroying will be done.  */

tree dtor_label;

/* In a constructor, the point at which we are ready to return
   the pointer to the initialized object.  */

tree ctor_label;

/* A FUNCTION_DECL which can call `unhandled_exception'.
   Not necessarily the one that the user will declare,
   but sufficient to be called by routines that want to abort the program.  */

tree unhandled_exception_fndecl;

/* A FUNCTION_DECL which can call `abort'.  Not necessarily the
   one that the user will declare, but sufficient to be called
   by routines that want to abort the program.  */

tree abort_fndecl;

extern rtx cleanup_label, return_label;

/* If original DECL_RESULT of current function was a register,
   but due to being an addressable named return value, would up
   on the stack, this variable holds the named return value's
   original location.  */
rtx original_result_rtx;

/* Sequence of insns which represents base initialization.  */
rtx base_init_insns;

/* C++: Keep these around to reduce calls to `get_identifier'.
   Identifiers for `this' in member functions and the auto-delete
   parameter for destructors.  */
tree this_identifier, in_charge_identifier;

/* A list (chain of TREE_LIST nodes) of named label uses.
   The TREE_PURPOSE field is the list of variables defined
   the the label's scope defined at the point of use.
   The TREE_VALUE field is the LABEL_DECL used.
   The TREE_TYPE field holds `current_binding_level' at the
   point of the label's use.

   Used only for jumps to as-yet undefined labels, since
   jumps to defined labels can have their validity checked
   by stmt.c.  */

static tree named_label_uses;

/* A list of objects which have constructors or destructors
   which reside in the global scope.  The decl is stored in
   the TREE_VALUE slot and the initializer is stored
   in the TREE_PURPOSE slot.  */
tree static_aggregates;

/* A list of functions which were declared inline, but later had their
   address taken.  Used only for non-virtual member functions, since we can
   find other functions easily enough.  */
tree pending_addressable_inlines;

/* A list of overloaded functions which we should forget ever
   existed, such as functions declared in a function's scope,
   once we leave that function's scope.  */
static tree overloads_to_forget;

/* -- end of C++ */

/* Two expressions that are constants with value zero.
   The first is of type `int', the second of type `void *'.  */

tree integer_zero_node;
tree null_pointer_node;

/* A node for the integer constants 1, 2, and 3.  */

tree integer_one_node, integer_two_node, integer_three_node;

/* Nonzero if we have seen an invalid cross reference
   to a struct, union, or enum, but not yet printed the message.  */

tree pending_invalid_xref;
/* File and line to appear in the eventual error message.  */
char *pending_invalid_xref_file;
int pending_invalid_xref_line;

/* While defining an enum type, this is 1 plus the last enumerator
   constant value.  */

static tree enum_next_value;

/* Parsing a function declarator leaves a list of parameter names
   or a chain or parameter decls here.  */

tree last_function_parms;

/* Parsing a function declarator leaves here a chain of structure
   and enum types declared in the parmlist.  */

static tree last_function_parm_tags;

/* After parsing the declarator that starts a function definition,
   `start_function' puts here the list of parameter names or chain of decls.
   `store_parm_decls' finds it here.  */

static tree current_function_parms;

/* Similar, for last_function_parm_tags.  */
static tree current_function_parm_tags;

/* A list (chain of TREE_LIST nodes) of all LABEL_DECLs in the function
   that have names.  Here so we can clear out their names' definitions
   at the end of the function.  */

static tree named_labels;

/* A list of LABEL_DECLs from outer contexts that are currently shadowed.  */

static tree shadowed_labels;

#if 0 /* Not needed by C++ */
/* Nonzero when store_parm_decls is called indicates a varargs function.
   Value not meaningful after store_parm_decls.  */

static int c_function_varargs;
#endif

/* The FUNCTION_DECL for the function currently being compiled,
   or 0 if between functions.  */
tree current_function_decl;

/* Set to 0 at beginning of a function definition, set to 1 if
   a return statement that specifies a return value is seen.  */

int current_function_returns_value;

/* Set to 0 at beginning of a function definition, set to 1 if
   a return statement with no argument is seen.  */

int current_function_returns_null;

/* Set to 0 at beginning of a function definition, and whenever
   a label (case or named) is defined.  Set to value of expression
   returned from function when that value can be transformed into
   a named return value.  */

tree current_function_return_value;

/* Nonzero means warn about multiple (redundant) decls for the same single
   variable or function.  */

extern int warn_redundant_decls;

/* Set to nonzero by `grokdeclarator' for a function
   whose return type is defaulted, if warnings for this are desired.  */

static int warn_about_return_type;

/* Nonzero when starting a function declared `extern inline'.  */

static int current_extern_inline;

/* Nonzero means give `double' the same size as `float'.  */

extern int flag_short_double;

/* Nonzero means don't recognize any builtin functions.  */

extern int flag_no_builtin;

/* Nonzero means do emit exported implementations of functions even if
   they can be inlined.  */

extern int flag_implement_inlines;

/* Nonzero means handle things in ANSI, instead of GNU fashion.  This
   flag should be tested for language behavior that's different between
   ANSI and GNU, but not so horrible as to merit a PEDANTIC label.  */

extern int flag_ansi;

/* Pointers to the base and current top of the language name stack.  */

extern tree *current_lang_base, *current_lang_stack;

/* C and C++ flags are in cp-decl2.c.  */

/* Set to 0 at beginning of a constructor, set to 1
   if that function does an allocation before referencing its
   instance variable.  */
int current_function_assigns_this;
int current_function_just_assigned_this;

/* Set to 0 at beginning of a function.  Set non-zero when
   store_parm_decls is called.  Don't call store_parm_decls
   if this flag is non-zero!  */
int current_function_parms_stored;

/* Current end of entries in the gc obstack for stack pointer variables.  */

int current_function_obstack_index;

/* Flag saying whether we have used the obstack in this function or not.  */

int current_function_obstack_usage;

/* Flag used when debugging cp-spew.c */

extern int spew_debug;

/* Allocate a level of searching.  */
struct stack_level *
push_decl_level (stack, obstack)
     struct stack_level *stack;
     struct obstack *obstack;
{
  struct stack_level tem;
  tem.prev = stack;

  return push_stack_level (obstack, (char *)&tem, sizeof (tem));
}

/* Discard a level of decl allocation.  */

static struct stack_level *
pop_decl_level (stack)
     struct stack_level *stack;
{
  tree *bp, *tp;
  struct obstack *obstack = stack->obstack;
  bp = stack->first;
  tp = (tree *)obstack_next_free (obstack);
  while (tp != bp)
    {
      --tp;
      if (*tp != NULL_TREE)
	IDENTIFIER_CLASS_VALUE (DECL_NAME (*tp)) = NULL_TREE;
    }
  return pop_stack_level (stack);
}

/* For each binding contour we allocate a binding_level structure
 * which records the names defined in that contour.
 * Contours include:
 *  0) the global one
 *  1) one for each function definition,
 *     where internal declarations of the parameters appear.
 *  2) one for each compound statement,
 *     to record its declarations.
 *
 * The current meaning of a name can be found by searching the levels from
 * the current one out to the global one.
 *
 * Off to the side, may be the class_binding_level.  This exists
 * only to catch class-local declarations.  It is otherwise
 * nonexistent.
 * 
 * Also there may be binding levels that catch cleanups that
 * must be run when exceptions occur.
 */

/* Note that the information in the `names' component of the global contour
   is duplicated in the IDENTIFIER_GLOBAL_VALUEs of all identifiers.  */

struct binding_level
  {
    /* A chain of _DECL nodes for all variables, constants, functions,
     * and typedef types.  These are in the reverse of the order supplied.
     */
    tree names;

    /* A list of structure, union and enum definitions,
     * for looking up tag names.
     * It is a chain of TREE_LIST nodes, each of whose TREE_PURPOSE is a name,
     * or NULL_TREE; and whose TREE_VALUE is a RECORD_TYPE, UNION_TYPE,
     * or ENUMERAL_TYPE node.
     *
     * C++: the TREE_VALUE nodes can be simple types for component_bindings.
     *
     */
    tree tags;

    /* For each level, a list of shadowed outer-level local definitions
       to be restored when this level is popped.
       Each link is a TREE_LIST whose TREE_PURPOSE is an identifier and
       whose TREE_VALUE is its old definition (a kind of ..._DECL node).  */
    tree shadowed;

    /* Same, for IDENTIFIER_CLASS_VALUE.  */
    tree class_shadowed;

    /* Same, for IDENTIFIER_TYPE_VALUE.  */
    tree type_shadowed;

    /* For each level (except not the global one),
       a chain of BLOCK nodes for all the levels
       that were entered and exited one level down.  */
    tree blocks;

    /* The BLOCK node for this level, if one has been preallocated.
       If 0, the BLOCK is allocated (if needed) when the level is popped.  */
    tree this_block;

    /* The binding level which this one is contained in (inherits from).  */
    struct binding_level *level_chain;

    /* Number of decls in `names' that have incomplete 
       structure or union types.  */
    unsigned short n_incomplete;

    /* 1 for the level that holds the parameters of a function.
       2 for the level that holds a class declaration.
       3 for levels that hold parameter declarations.  */
    unsigned parm_flag : 4;

    /* 1 means make a BLOCK for this level regardless of all else.
       2 for temporary binding contours created by the compiler.  */
    unsigned keep : 3;

    /* Nonzero if this level "doesn't exist" for tags.  */
    unsigned tag_transparent : 1;

    /* Nonzero if this level can safely have additional
       cleanup-needing variables added to it.  */
    unsigned more_cleanups_ok : 1;
    unsigned have_cleanups : 1;

    /* Nonzero if this level can safely have additional
       exception-raising statements added to it.  */
    unsigned more_exceptions_ok : 1;
    unsigned have_exceptions : 1;

    /* Nonzero if we should accept any name as an identifier in
       this scope.  This happens in some template definitions.  */
    unsigned accept_any : 1;

    /* Nonzero if this level is for completing a template class definition
       inside a binding level that temporarily binds the parameters.  This
       means that definitions here should not be popped off when unwinding
       this binding level.  (Not actually implemented this way,
       unfortunately.)  */
    unsigned pseudo_global : 1;

    /* Two bits left for this word.  */

#if PARANOID
    unsigned char depth;
#endif
  };

#define NULL_BINDING_LEVEL (struct binding_level *) NULL
  
/* The binding level currently in effect.  */

static struct binding_level *current_binding_level;

/* The binding level of the current class, if any.  */

static struct binding_level *class_binding_level;

/* A chain of binding_level structures awaiting reuse.  */

static struct binding_level *free_binding_level;

/* The outermost binding level, for names of file scope.
   This is created when the compiler is started and exists
   through the entire run.  */

static struct binding_level *global_binding_level;

/* Binding level structures are initialized by copying this one.  */

static struct binding_level clear_binding_level;

/* Nonzero means unconditionally make a BLOCK for the next level pushed.  */

static int keep_next_level_flag;

#if PARANOID
/* Perform sanity checking on binding levels.  Normally not needed.  */
void
binding_levels_sane ()
{
  struct binding_level *b = current_binding_level;
  static int n;
  if (++n < 3)
    return;
  my_friendly_assert (global_binding_level != 0, 126);
  my_friendly_assert (current_binding_level != 0, 127);
  for (b = current_binding_level; b != global_binding_level; b = b->level_chain)
    {
      my_friendly_assert (b->level_chain != 0, 128);
      my_friendly_assert (b->depth == 1 + b->level_chain->depth, 129);
    }
  if (class_binding_level)
    for (b = class_binding_level;
         b != global_binding_level && b != current_binding_level;
         b = b->level_chain)
    {
      my_friendly_assert (b->level_chain != 0, 130);
      my_friendly_assert (b->depth == 1 + b->level_chain->depth, 131);
    }
  my_friendly_assert (global_binding_level->depth == 0, 132);
  my_friendly_assert (global_binding_level->level_chain == 0, 133);
  return;
}

#else
#define binding_levels_sane() ((void)(1))
#endif

#ifdef DEBUG_CP_BINDING_LEVELS
int debug_bindings_indentation;
#endif

static void
#if !PARANOID && defined (__GNUC__)
__inline
#endif
push_binding_level (newlevel, tag_transparent, keep)
     struct binding_level *newlevel;
     int tag_transparent, keep;
{
  binding_levels_sane();
  /* Add this level to the front of the chain (stack) of levels that
     are active.  */
#ifdef DEBUG_CP_BINDING_LEVELS
  indent_to (stderr, debug_bindings_indentation);
  fprintf (stderr, "pushing binding level ");
  fprintf (stderr, HOST_PTR_PRINTF, newlevel);
  fprintf (stderr, "\n");
#endif
  *newlevel = clear_binding_level;
  if (class_binding_level)
    {
      newlevel->level_chain = class_binding_level;
      class_binding_level = (struct binding_level *)0;
    }
  else
    {
      newlevel->level_chain = current_binding_level;
    }
  current_binding_level = newlevel;
  newlevel->tag_transparent = tag_transparent;
  newlevel->more_cleanups_ok = 1;
  newlevel->more_exceptions_ok = 1;
  newlevel->keep = keep;
#if PARANOID
  newlevel->depth = (newlevel->level_chain
		     ? newlevel->level_chain->depth + 1
		     : 0);
#endif
  binding_levels_sane();
}

static void
#if !PARANOID && defined (__GNUC__)
__inline
#endif
pop_binding_level ()
{
  binding_levels_sane();
#ifdef DEBUG_CP_BINDING_LEVELS
  indent_to (stderr, debug_bindings_indentation);
  fprintf (stderr, "popping binding level ");
  fprintf (stderr, HOST_PTR_PRINTF, current_binding_level);
  fprintf (stderr, "\n");
#endif
  if (global_binding_level)
    {
      /* cannot pop a level, if there are none left to pop. */
      if (current_binding_level == global_binding_level)
	my_friendly_abort (123);
    }
  /* Pop the current level, and free the structure for reuse.  */
  {
    register struct binding_level *level = current_binding_level;
    current_binding_level = current_binding_level->level_chain;
    level->level_chain = free_binding_level;
#ifdef DEBUG_CP_BINDING_LEVELS
    memset (level, 0x69, sizeof (*level));
#else
    free_binding_level = level;
#if PARANOID
    level->depth = ~0;	/* ~0 assumes that the depth is unsigned. */
#endif
#endif
    if (current_binding_level->parm_flag == 2)
      {
	class_binding_level = current_binding_level;
	do
	  {
	    current_binding_level = current_binding_level->level_chain;
	  }
	while (current_binding_level->parm_flag == 2);
      }
  }
  binding_levels_sane();
}

/* Nonzero if we are currently in the global binding level.  */

int
global_bindings_p ()
{
  return current_binding_level == global_binding_level;
}

void
keep_next_level ()
{
  keep_next_level_flag = 1;
}

/* Nonzero if the current level needs to have a BLOCK made.  */

int
kept_level_p ()
{
  return (current_binding_level->blocks != NULL_TREE
	  || current_binding_level->keep
	  || current_binding_level->names != NULL_TREE
	  || (current_binding_level->tags != NULL_TREE
	      && !current_binding_level->tag_transparent));
}

/* Identify this binding level as a level of parameters.  */

void
declare_parm_level ()
{
  current_binding_level->parm_flag = 1;
}

/* Identify this binding level as a level of a default exception handler.  */

void
declare_implicit_exception ()
{
  current_binding_level->parm_flag = 3;
}

/* Nonzero if current binding contour contains expressions
   that might raise exceptions.  */

int
have_exceptions_p ()
{
  return current_binding_level->have_exceptions;
}

void
declare_uninstantiated_type_level ()
{
  current_binding_level->accept_any = 1;
}

int
uninstantiated_type_level_p ()
{
  return current_binding_level->accept_any;
}

void
declare_pseudo_global_level ()
{
  current_binding_level->pseudo_global = 1;
}

int
pseudo_global_level_p ()
{
  return current_binding_level->pseudo_global;
}

/* Enter a new binding level.
   If TAG_TRANSPARENT is nonzero, do so only for the name space of variables,
   not for that of tags.  */

void
pushlevel (tag_transparent)
     int tag_transparent;
{
  register struct binding_level *newlevel = NULL_BINDING_LEVEL;

#ifdef DEBUG_CP_BINDING_LEVELS
  indent_to (stderr, debug_bindings_indentation);
  fprintf (stderr, "pushlevel");
  debug_bindings_indentation += 4;
#endif

  /* If this is the top level of a function,
     just make sure that NAMED_LABELS is 0.
     They should have been set to 0 at the end of the previous function.  */

  if (current_binding_level == global_binding_level)
    my_friendly_assert (named_labels == NULL_TREE, 134);

  /* Reuse or create a struct for this binding level.  */

  if (free_binding_level)
    {
      newlevel = free_binding_level;
      free_binding_level = free_binding_level->level_chain;
    }
  else
    {
      /* Create a new `struct binding_level'.  */
      newlevel = (struct binding_level *) xmalloc (sizeof (struct binding_level));
    }
  push_binding_level (newlevel, tag_transparent, keep_next_level_flag);
  GNU_xref_start_scope ((int) newlevel);
  keep_next_level_flag = 0;

#ifdef DEBUG_CP_BINDING_LEVELS
  debug_bindings_indentation -= 4;
#endif
}

void
pushlevel_temporary (tag_transparent)
     int tag_transparent;
{
  pushlevel (tag_transparent);
  current_binding_level->keep = 2;
  clear_last_expr ();

  /* Note we don't call push_momentary() here.  Otherwise, it would cause
     cleanups to be allocated on the momentary obstack, and they will be
     overwritten by the next statement.  */

  expand_start_bindings (0);
}

/* Exit a binding level.
   Pop the level off, and restore the state of the identifier-decl mappings
   that were in effect when this level was entered.

   If KEEP == 1, this level had explicit declarations, so
   and create a "block" (a BLOCK node) for the level
   to record its declarations and subblocks for symbol table output.

   If KEEP == 2, this level's subblocks go to the front,
   not the back of the current binding level.  This happens,
   for instance, when code for constructors and destructors
   need to generate code at the end of a function which must
   be moved up to the front of the function.

   If FUNCTIONBODY is nonzero, this level is the body of a function,
   so create a block as if KEEP were set and also clear out all
   label names.

   If REVERSE is nonzero, reverse the order of decls before putting
   them into the BLOCK.  */

tree
poplevel (keep, reverse, functionbody)
     int keep;
     int reverse;
     int functionbody;
{
  register tree link;
  /* The chain of decls was accumulated in reverse order.
     Put it into forward order, just for cleanliness.  */
  tree decls;
  int tmp = functionbody;
  int implicit_try_block = current_binding_level->parm_flag == 3;
  int real_functionbody = current_binding_level->keep == 2
    ? ((functionbody = 0), tmp) : functionbody;
  tree tags = functionbody >= 0 ? current_binding_level->tags : 0;
  tree subblocks = functionbody >= 0 ? current_binding_level->blocks : 0;
  tree block = NULL_TREE;
  tree decl;
  int block_previously_created;

#ifdef DEBUG_CP_BINDING_LEVELS
  indent_to (stderr, debug_bindings_indentation);
  fprintf (stderr, "poplevel");
  debug_bindings_indentation += 4;
#endif

  binding_levels_sane();
  GNU_xref_end_scope ((HOST_WIDE_INT) current_binding_level,
		      (HOST_WIDE_INT) current_binding_level->level_chain,
		      current_binding_level->parm_flag,
		      current_binding_level->keep,
		      current_binding_level->tag_transparent);

  if (current_binding_level->keep == 1)
    keep = 1;

  /* This warning is turned off because it causes warnings for
     declarations like `extern struct foo *x'.  */
#if 0
  /* Warn about incomplete structure types in this level.  */
  for (link = tags; link; link = TREE_CHAIN (link))
    if (TYPE_SIZE (TREE_VALUE (link)) == NULL_TREE)
      {
	tree type = TREE_VALUE (link);
	char *errmsg;
	switch (TREE_CODE (type))
	  {
	  case RECORD_TYPE:
	    errmsg = "`struct %s' incomplete in scope ending here";
	    break;
	  case UNION_TYPE:
	    errmsg = "`union %s' incomplete in scope ending here";
	    break;
	  case ENUMERAL_TYPE:
	    errmsg = "`enum %s' incomplete in scope ending here";
	    break;
	  }
	if (TREE_CODE (TYPE_NAME (type)) == IDENTIFIER_NODE)
	  error (errmsg, IDENTIFIER_POINTER (TYPE_NAME (type)));
	else
	  /* If this type has a typedef-name, the TYPE_NAME is a TYPE_DECL.  */
	  error (errmsg, TYPE_NAME_STRING (type));
      }
#endif /* 0 */

  /* Get the decls in the order they were written.
     Usually current_binding_level->names is in reverse order.
     But parameter decls were previously put in forward order.  */

  if (reverse)
    current_binding_level->names
      = decls = nreverse (current_binding_level->names);
  else
    decls = current_binding_level->names;

  /* Output any nested inline functions within this block
     if they weren't already output.  */

  for (decl = decls; decl; decl = TREE_CHAIN (decl))
    if (TREE_CODE (decl) == FUNCTION_DECL
	&& ! TREE_ASM_WRITTEN (decl)
	&& DECL_INITIAL (decl) != NULL_TREE
	&& TREE_ADDRESSABLE (decl))
      {
	/* If this decl was copied from a file-scope decl
	   on account of a block-scope extern decl,
	   propagate TREE_ADDRESSABLE to the file-scope decl.  */
	if (DECL_ABSTRACT_ORIGIN (decl) != NULL_TREE)
	  TREE_ADDRESSABLE (DECL_ABSTRACT_ORIGIN (decl)) = 1;
	else
	  output_inline_function (decl);
      }

  /* If there were any declarations or structure tags in that level,
     or if this level is a function body,
     create a BLOCK to record them for the life of this function.  */

  block = NULL_TREE;
  block_previously_created = (current_binding_level->this_block != NULL_TREE);
  if (block_previously_created)
    block = current_binding_level->this_block;
  else if (keep == 1 || functionbody)
    block = make_node (BLOCK);
  if (block != NULL_TREE)
    {
      BLOCK_VARS (block) = decls;
      BLOCK_TYPE_TAGS (block) = tags;
      BLOCK_SUBBLOCKS (block) = subblocks;
      remember_end_note (block);
    }

  /* In each subblock, record that this is its superior.  */

  if (keep >= 0)
    for (link = subblocks; link; link = TREE_CHAIN (link))
      BLOCK_SUPERCONTEXT (link) = block;

  /* Clear out the meanings of the local variables of this level.  */

  for (link = decls; link; link = TREE_CHAIN (link))
    {
      if (DECL_NAME (link) != NULL_TREE)
	{
	  /* If the ident. was used or addressed via a local extern decl,
	     don't forget that fact.  */
	  if (DECL_EXTERNAL (link))
	    {
	      if (TREE_USED (link))
		TREE_USED (DECL_ASSEMBLER_NAME (link)) = 1;
	      if (TREE_ADDRESSABLE (link))
		TREE_ADDRESSABLE (DECL_ASSEMBLER_NAME (link)) = 1;
	    }
	  IDENTIFIER_LOCAL_VALUE (DECL_NAME (link)) = NULL_TREE;
	}
    }

  /* Restore all name-meanings of the outer levels
     that were shadowed by this level.  */

  for (link = current_binding_level->shadowed; link; link = TREE_CHAIN (link))
    IDENTIFIER_LOCAL_VALUE (TREE_PURPOSE (link)) = TREE_VALUE (link);
  for (link = current_binding_level->class_shadowed;
       link; link = TREE_CHAIN (link))
    IDENTIFIER_CLASS_VALUE (TREE_PURPOSE (link)) = TREE_VALUE (link);
  for (link = current_binding_level->type_shadowed;
       link; link = TREE_CHAIN (link))
    IDENTIFIER_TYPE_VALUE (TREE_PURPOSE (link)) = TREE_VALUE (link);

  /* If the level being exited is the top level of a function,
     check over all the labels.  */

  if (functionbody)
    {
      /* If this is the top level block of a function,
         the vars are the function's parameters.
         Don't leave them in the BLOCK because they are
         found in the FUNCTION_DECL instead.  */

      BLOCK_VARS (block) = 0;

      /* Clear out the definitions of all label names,
	 since their scopes end here.  */

      for (link = named_labels; link; link = TREE_CHAIN (link))
	{
	  register tree label = TREE_VALUE (link);

	  if (DECL_INITIAL (label) == NULL_TREE)
	    {
	      error_with_decl (label, "label `%s' used but not defined");
	      /* Avoid crashing later.  */
	      define_label (input_filename, 1, DECL_NAME (label));
	    }
	  else if (warn_unused && !TREE_USED (label))
	    warning_with_decl (label, 
			       "label `%s' defined but not used");
	  SET_IDENTIFIER_LABEL_VALUE (DECL_NAME (label), 0);

          /* Put the labels into the "variables" of the
             top-level block, so debugger can see them.  */
          TREE_CHAIN (label) = BLOCK_VARS (block);
          BLOCK_VARS (block) = label;
	}

      named_labels = NULL_TREE;
    }

  /* Any uses of undefined labels now operate under constraints
     of next binding contour.  */
  {
    struct binding_level *level_chain;
    level_chain = current_binding_level->level_chain;
    if (level_chain)
      {
	tree labels;
	for (labels = named_label_uses; labels; labels = TREE_CHAIN (labels))
	  if (TREE_TYPE (labels) == (tree)current_binding_level)
	    {
	      TREE_TYPE (labels) = (tree)level_chain;
	      TREE_PURPOSE (labels) = level_chain->names;
	    }
      }
  }

  tmp = current_binding_level->keep;

  pop_binding_level ();
  if (functionbody)
    DECL_INITIAL (current_function_decl) = block;
  else if (block)
    {
      if (!block_previously_created)
        current_binding_level->blocks
          = chainon (current_binding_level->blocks, block);
    }

  /* If we did not make a block for the level just exited,
     any blocks made for inner levels
     (since they cannot be recorded as subblocks in that level)
     must be carried forward so they will later become subblocks
     of something else.  */
  else if (subblocks)
    if (keep == 2)
      current_binding_level->blocks = chainon (subblocks, current_binding_level->blocks);
    else
      current_binding_level->blocks
        = chainon (current_binding_level->blocks, subblocks);

  /* Take care of compiler's internal binding structures.  */
  if (tmp == 2 && !implicit_try_block)
    {
#if 0
      /* We did not call push_momentary for this
	 binding contour, so there is nothing to pop.  */
      pop_momentary ();
#endif
      expand_end_bindings (getdecls (), keep, 1);
      block = poplevel (keep, reverse, real_functionbody);
    }
  if (block)
    TREE_USED (block) = 1;
  binding_levels_sane();
#ifdef DEBUG_CP_BINDING_LEVELS
  debug_bindings_indentation -= 4;
#endif
  return block;
}

/* Delete the node BLOCK from the current binding level.
   This is used for the block inside a stmt expr ({...})
   so that the block can be reinserted where appropriate.  */

void
delete_block (block)
     tree block;
{
  tree t;
  if (current_binding_level->blocks == block)
    current_binding_level->blocks = TREE_CHAIN (block);
  for (t = current_binding_level->blocks; t;)
    {
      if (TREE_CHAIN (t) == block)
	TREE_CHAIN (t) = TREE_CHAIN (block);
      else
	t = TREE_CHAIN (t);
    }
  TREE_CHAIN (block) = NULL_TREE;
  /* Clear TREE_USED which is always set by poplevel.
     The flag is set again if insert_block is called.  */
  TREE_USED (block) = 0;
}

/* Insert BLOCK at the end of the list of subblocks of the
   current binding level.  This is used when a BIND_EXPR is expanded,
   to handle the BLOCK node inside the BIND_EXPR.  */

void
insert_block (block)
     tree block;
{
  TREE_USED (block) = 1;
  current_binding_level->blocks
    = chainon (current_binding_level->blocks, block);
}

/* Add BLOCK to the current list of blocks for this binding contour.  */
void
add_block_current_level (block)
     tree block;
{
  current_binding_level->blocks
    = chainon (current_binding_level->blocks, block);
}

/* Set the BLOCK node for the innermost scope
   (the one we are currently in).  */

void
set_block (block)
    register tree block;
{
  current_binding_level->this_block = block;
}

/* Do a pushlevel for class declarations.  */
void
pushlevel_class ()
{
  binding_levels_sane();
#ifdef DEBUG_CP_BINDING_LEVELS
  indent_to (stderr, debug_bindings_indentation);
  fprintf (stderr, "pushlevel_class");
  debug_bindings_indentation += 4;
#endif
  pushlevel (0);
  decl_stack = push_decl_level (decl_stack, &decl_obstack);
  class_binding_level = current_binding_level;
  class_binding_level->parm_flag = 2;
  do
    {
      current_binding_level = current_binding_level->level_chain;
    }
  while (current_binding_level->parm_flag == 2);
  binding_levels_sane();
#ifdef DEBUG_CP_BINDING_LEVELS
  debug_bindings_indentation -= 4;
#endif
}

/* ...and a poplevel for class declarations.  */
tree
poplevel_class ()
{
  register struct binding_level *level = class_binding_level;
  tree block = NULL_TREE;
  tree shadowed;

#ifdef DEBUG_CP_BINDING_LEVELS
  indent_to (stderr, debug_bindings_indentation);
  fprintf (stderr, "poplevel_class");
  debug_bindings_indentation += 4;
#endif
  binding_levels_sane();
  if (level == (struct binding_level *)0)
    {
      while (current_binding_level && class_binding_level == (struct binding_level *)0)
	block = poplevel (0, 0, 0);
      if (current_binding_level == (struct binding_level *)0)
	fatal ("syntax error too serious");
      level = class_binding_level;
    }
  decl_stack = pop_decl_level (decl_stack);
  for (shadowed = level->shadowed; shadowed; shadowed = TREE_CHAIN (shadowed))
    IDENTIFIER_LOCAL_VALUE (TREE_PURPOSE (shadowed)) = TREE_VALUE (shadowed);
  for (shadowed = level->class_shadowed; shadowed; shadowed = TREE_CHAIN (shadowed))
    IDENTIFIER_CLASS_VALUE (TREE_PURPOSE (shadowed)) = TREE_VALUE (shadowed);
  for (shadowed = level->type_shadowed; shadowed; shadowed = TREE_CHAIN (shadowed))
    IDENTIFIER_TYPE_VALUE (TREE_PURPOSE (shadowed)) = TREE_VALUE (shadowed);

  GNU_xref_end_scope ((HOST_WIDE_INT) class_binding_level,
		      (HOST_WIDE_INT) class_binding_level->level_chain,
		      class_binding_level->parm_flag,
		      class_binding_level->keep,
		      class_binding_level->tag_transparent);

  class_binding_level = level->level_chain;
  if (class_binding_level->parm_flag != 2)
    class_binding_level = (struct binding_level *)0;

#ifdef DEBUG_CP_BINDING_LEVELS
  indent_to (stderr, debug_bindings_indentation);
  fprintf (stderr, "popping class binding level ");
  fprintf (stderr, HOST_PTR_PRINTF, level);
  fprintf (stderr, "\n");
  memset (level, 0x69, sizeof (*level));
  debug_bindings_indentation -= 4;
#else
  level->level_chain = free_binding_level;
  free_binding_level = level;
#endif
  binding_levels_sane();

  return block;
}

/* For debugging.  */
int no_print_functions = 0;
int no_print_builtins = 0;

void
print_binding_level (lvl)
     struct binding_level *lvl;
{
  tree t;
  int i = 0, len;
  fprintf (stderr, " blocks=");
  fprintf (stderr, HOST_PTR_PRINTF, lvl->blocks);
  fprintf (stderr, " n_incomplete=%d parm_flag=%d keep=%d",
	   lvl->n_incomplete, lvl->parm_flag, lvl->keep);
  if (lvl->tag_transparent)
    fprintf (stderr, " tag-transparent");
  if (lvl->more_cleanups_ok)
    fprintf (stderr, " more-cleanups-ok");
  if (lvl->have_cleanups)
    fprintf (stderr, " have-cleanups");
  if (lvl->more_exceptions_ok)
    fprintf (stderr, " more-exceptions-ok");
  if (lvl->have_exceptions)
    fprintf (stderr, " have-exceptions");
  fprintf (stderr, "\n");
  if (lvl->names)
    {
      fprintf (stderr, " names:\t");
      /* We can probably fit 3 names to a line?  */
      for (t = lvl->names; t; t = TREE_CHAIN (t))
	{
	  if (no_print_functions && (TREE_CODE(t) == FUNCTION_DECL)) 
	    continue;
	  if (no_print_builtins
	      && (TREE_CODE(t) == TYPE_DECL)
	      && (!strcmp(DECL_SOURCE_FILE(t),"<built-in>")))
	    continue;

	  /* Function decls tend to have longer names.  */
	  if (TREE_CODE (t) == FUNCTION_DECL)
	    len = 3;
	  else
	    len = 2;
	  i += len;
	  if (i > 6)
	    {
	      fprintf (stderr, "\n\t");
	      i = len;
	    }
	  print_node_brief (stderr, "", t, 0);
	  if (TREE_CODE (t) == ERROR_MARK)
	    break;
	}
      if (i)
        fprintf (stderr, "\n");
    }
  if (lvl->tags)
    {
      fprintf (stderr, " tags:\t");
      i = 0;
      for (t = lvl->tags; t; t = TREE_CHAIN (t))
	{
	  if (TREE_PURPOSE (t) == NULL_TREE)
	    len = 3;
	  else if (TREE_PURPOSE (t) == TYPE_IDENTIFIER (TREE_VALUE (t)))
	    len = 2;
	  else
	    len = 4;
	  i += len;
	  if (i > 5)
	    {
	      fprintf (stderr, "\n\t");
	      i = len;
	    }
	  if (TREE_PURPOSE (t) == NULL_TREE)
	    {
	      print_node_brief (stderr, "<unnamed-typedef", TREE_VALUE (t), 0);
	      fprintf (stderr, ">");
	    }
	  else if (TREE_PURPOSE (t) == TYPE_IDENTIFIER (TREE_VALUE (t)))
	    print_node_brief (stderr, "", TREE_VALUE (t), 0);
	  else
	    {
	      print_node_brief (stderr, "<typedef", TREE_PURPOSE (t), 0);
	      print_node_brief (stderr, "", TREE_VALUE (t), 0);
	      fprintf (stderr, ">");
	    }
	}
      if (i)
	fprintf (stderr, "\n");
    }
  if (lvl->shadowed)
    {
      fprintf (stderr, " shadowed:");
      for (t = lvl->shadowed; t; t = TREE_CHAIN (t))
	{
	  fprintf (stderr, " %s ", IDENTIFIER_POINTER (TREE_PURPOSE (t)));
	}
      fprintf (stderr, "\n");
    }
  if (lvl->class_shadowed)
    {
      fprintf (stderr, " class-shadowed:");
      for (t = lvl->class_shadowed; t; t = TREE_CHAIN (t))
	{
	  fprintf (stderr, " %s ", IDENTIFIER_POINTER (TREE_PURPOSE (t)));
	}
      fprintf (stderr, "\n");
    }
  if (lvl->type_shadowed)
    {
      fprintf (stderr, " type-shadowed:");
      for (t = lvl->type_shadowed; t; t = TREE_CHAIN (t))
        {
#if 0
          fprintf (stderr, "\n\t");
          print_node_brief (stderr, "<", TREE_PURPOSE (t), 0);
          if (TREE_VALUE (t))
            print_node_brief (stderr, " ", TREE_VALUE (t), 0);
          else
            fprintf (stderr, " (none)");
          fprintf (stderr, ">");
#else
	  fprintf (stderr, " %s ", IDENTIFIER_POINTER (TREE_PURPOSE (t)));
#endif
        }
      fprintf (stderr, "\n");
    }
}

void
print_other_binding_stack (stack)
     struct binding_level *stack;
{
  struct binding_level *level;
  for (level = stack; level != global_binding_level; level = level->level_chain)
    {
      fprintf (stderr, "binding level ");
      fprintf (stderr, HOST_PTR_PRINTF, level);
      fprintf (stderr, "\n");
      print_binding_level (level);
    }
}

void
print_binding_stack ()
{
  struct binding_level *b;
  fprintf (stderr, "current_binding_level=");
  fprintf (stderr, HOST_PTR_PRINTF, current_binding_level);
  fprintf (stderr, "\nclass_binding_level=");
  fprintf (stderr, HOST_PTR_PRINTF, class_binding_level);
  fprintf (stderr, "\nglobal_binding_level=");
  fprintf (stderr, HOST_PTR_PRINTF, global_binding_level);
  fprintf (stderr, "\n");
  if (class_binding_level)
    {
      for (b = class_binding_level; b; b = b->level_chain)
	if (b == current_binding_level)
	  break;
      if (b)
	b = class_binding_level;
      else
	b = current_binding_level;
    }
  else
    b = current_binding_level;
  print_other_binding_stack (b);
  fprintf (stderr, "global:\n");
  print_binding_level (global_binding_level);
}

/* Subroutines for reverting temporarily to top-level for instantiation
   of templates and such.  We actually need to clear out the class- and
   local-value slots of all identifiers, so that only the global values
   are at all visible.  Simply setting current_binding_level to the global
   scope isn't enough, because more binding levels may be pushed.  */
struct saved_scope {
  struct binding_level *old_binding_level;
  tree old_bindings;
  struct saved_scope *prev;
  tree class_name, class_type, class_decl, function_decl;
  struct binding_level *class_bindings;
};
static struct saved_scope *current_saved_scope;
extern tree prev_class_type;

void
push_to_top_level ()
{
  struct saved_scope *s =
    (struct saved_scope *) xmalloc (sizeof (struct saved_scope));
  struct binding_level *b = current_binding_level;
  tree old_bindings = NULL_TREE;

#ifdef DEBUG_CP_BINDING_LEVELS
  fprintf (stderr, "PUSH_TO_TOP_LEVEL\n");
#endif

  /* Have to include global_binding_level, because class-level decls
     aren't listed anywhere useful.  */
  for (; b; b = b->level_chain)
    {
      tree t;
      for (t = b->names; t; t = TREE_CHAIN (t))
	if (b != global_binding_level)
	  {
	    tree binding, t1, t2 = t;
	    tree id = DECL_ASSEMBLER_NAME (t2);

	    if (!id
		|| (!IDENTIFIER_LOCAL_VALUE (id)
		    && !IDENTIFIER_CLASS_VALUE (id)))
	      continue;

	    for (t1 = old_bindings; t1; t1 = TREE_CHAIN (t1))
	      if (TREE_VEC_ELT (t1, 0) == id)
		goto skip_it;
	    
	    binding = make_tree_vec (4);
	    if (id)
	      {
		my_friendly_assert (TREE_CODE (id) == IDENTIFIER_NODE, 135);
		TREE_VEC_ELT (binding, 0) = id;
		TREE_VEC_ELT (binding, 1) = IDENTIFIER_TYPE_VALUE (id);
		TREE_VEC_ELT (binding, 2) = IDENTIFIER_LOCAL_VALUE (id);
		TREE_VEC_ELT (binding, 3) = IDENTIFIER_CLASS_VALUE (id);
		IDENTIFIER_LOCAL_VALUE (id) = NULL_TREE;
		IDENTIFIER_CLASS_VALUE (id) = NULL_TREE;
		adjust_type_value (id);
	      }
	    TREE_CHAIN (binding) = old_bindings;
	    old_bindings = binding;
	    skip_it:
	    ;
	  }
      /* Unwind type-value slots back to top level.  */
      if (b != global_binding_level)
        for (t = b->type_shadowed; t; t = TREE_CHAIN (t))
          SET_IDENTIFIER_TYPE_VALUE (TREE_PURPOSE (t), TREE_VALUE (t));
    }

  s->old_binding_level = current_binding_level;
  current_binding_level = global_binding_level;

  s->class_name = current_class_name;
  s->class_type = current_class_type;
  s->class_decl = current_class_decl;
  s->function_decl = current_function_decl;
  s->class_bindings = class_binding_level;
  current_class_name = current_class_type = current_class_decl = NULL_TREE;
  current_function_decl = NULL_TREE;
  class_binding_level = (struct binding_level *)0;

  s->prev = current_saved_scope;
  s->old_bindings = old_bindings;
  current_saved_scope = s;
  binding_levels_sane();
}

void
pop_from_top_level ()
{
  struct saved_scope *s = current_saved_scope;
  tree t;

#ifdef DEBUG_CP_BINDING_LEVELS
  fprintf (stderr, "POP_FROM_TOP_LEVEL\n");
#endif

  binding_levels_sane();
  current_binding_level = s->old_binding_level;
  current_saved_scope = s->prev;
  for (t = s->old_bindings; t; t = TREE_CHAIN (t))
    {
      tree id = TREE_VEC_ELT (t, 0);
      if (id)
	{
	  IDENTIFIER_TYPE_VALUE (id) = TREE_VEC_ELT (t, 1);
	  IDENTIFIER_LOCAL_VALUE (id) = TREE_VEC_ELT (t, 2);
	  IDENTIFIER_CLASS_VALUE (id) = TREE_VEC_ELT (t, 3);
	}
    }
  current_class_name = s->class_name;
  current_class_type = s->class_type;
  current_class_decl = s->class_decl;
  if (current_class_type)
    C_C_D = CLASSTYPE_INST_VAR (current_class_type);
  else
    C_C_D = NULL_TREE;
  current_function_decl = s->function_decl;
  class_binding_level = s->class_bindings;
  free (s);
  binding_levels_sane();
}

/* Push a definition of struct, union or enum tag "name".
   "type" should be the type node.
   We assume that the tag "name" is not already defined.

   Note that the definition may really be just a forward reference.
   In that case, the TYPE_SIZE will be a NULL_TREE.

   C++ gratuitously puts all these tags in the name space. */

/* When setting the IDENTIFIER_TYPE_VALUE field of an identifier ID,
   record the shadowed value for this binding contour.  TYPE is
   the type that ID maps to.  */
void
set_identifier_type_value (id, type)
     tree id;
     tree type;
{
  if (current_binding_level != global_binding_level)
    {
      tree old_type_value = IDENTIFIER_TYPE_VALUE (id);
      current_binding_level->type_shadowed
	= tree_cons (id, old_type_value, current_binding_level->type_shadowed);
    }
  else if (class_binding_level)
    {
      tree old_type_value = IDENTIFIER_TYPE_VALUE (id);
      class_binding_level->type_shadowed
	= tree_cons (id, old_type_value, class_binding_level->type_shadowed);
    }      
  SET_IDENTIFIER_TYPE_VALUE (id, type);
}

/*
 * local values can need to be shadowed too, but it only happens
 * explicitly from pushdecl, in support of nested enums.
 */
void
set_identifier_local_value (id, type)
     tree id;
     tree type;
{
  if (current_binding_level != global_binding_level)
    {
      tree old_local_value = IDENTIFIER_LOCAL_VALUE (id);
      current_binding_level->shadowed
	= tree_cons (id, old_local_value, current_binding_level->shadowed);
    }
  else if (class_binding_level)
    {
      tree old_local_value = IDENTIFIER_LOCAL_VALUE (id);
      class_binding_level->shadowed
	= tree_cons (id, old_local_value, class_binding_level->shadowed);
    }      
  IDENTIFIER_LOCAL_VALUE (id) = type;
}

/* Subroutine "set_nested_typename" builds the nested-typename of
   the type decl in question.  (Argument CLASSNAME can actually be
   a function as well, if that's the smallest containing scope.)  */

static void
set_nested_typename (decl, classname, name, type)
     tree decl, classname, name, type;
{
  my_friendly_assert (TREE_CODE (decl) == TYPE_DECL, 136);
  if (classname != NULL_TREE)
    {
      char *buf;
      my_friendly_assert (TREE_CODE (classname) == IDENTIFIER_NODE, 137);
      my_friendly_assert (TREE_CODE (name) == IDENTIFIER_NODE, 138);
      buf = (char *) alloca (4 + IDENTIFIER_LENGTH (classname)
			     + IDENTIFIER_LENGTH (name));
      sprintf (buf, "%s::%s", IDENTIFIER_POINTER (classname),
	       IDENTIFIER_POINTER (name));
      DECL_NESTED_TYPENAME (decl) = get_identifier (buf);
      SET_IDENTIFIER_TYPE_VALUE (DECL_NESTED_TYPENAME (decl), type);
    }
  else
    DECL_NESTED_TYPENAME (decl) = name;
}

#if 0 /* not yet, should get fixed properly later */
/* Create a TYPE_DECL node with the correct DECL_ASSEMBLER_NAME.
   Other routines shouldn't use build_decl directly; they'll produce
   incorrect results with `-g' unless they duplicate this code.

   This is currently needed mainly for dbxout.c, but we can make
   use of it in cp-method.c later as well.  */
tree
make_type_decl (name, type)
     tree name, type;
{
  tree decl, id;
  decl = build_decl (TYPE_DECL, name, type);
  if (TYPE_NAME (type) == name)
    /* Class/union/enum definition, or a redundant typedef for same.  */
    {
      id = get_identifier (build_overload_name (type, 1, 1));
      DECL_ASSEMBLER_NAME (decl) = id;
    }
  else if (TYPE_NAME (type) != NULL_TREE)
    /* Explicit typedef, or implicit typedef for template expansion.  */
    DECL_ASSEMBLER_NAME (decl) = DECL_ASSEMBLER_NAME (TYPE_NAME (type));
  else
    {
      /* Typedef for unnamed struct; some other situations.
	 TYPE_NAME is null; what's right here?  */
    }
  return decl;
}

#endif
void
pushtag (name, type)
     tree name, type;
{
  register struct binding_level *b;

  if (class_binding_level)
    b = class_binding_level;
  else
    {
      b = current_binding_level;
      while (b->tag_transparent) b = b->level_chain;
    }

  if (b == global_binding_level)
    b->tags = perm_tree_cons (name, type, b->tags);
  else
    b->tags = saveable_tree_cons (name, type, b->tags);

  if (name)
    {
      /* Record the identifier as the type's name if it has none.  */

      if (TYPE_NAME (type) == NULL_TREE)
        TYPE_NAME (type) = name;
      
      /* Do C++ gratuitous typedefing.  */
      if (IDENTIFIER_TYPE_VALUE (name) != type
	  && (TREE_CODE (type) != RECORD_TYPE
	      || class_binding_level == (struct binding_level *)0
	      || !CLASSTYPE_DECLARED_EXCEPTION (type)))
        {
          register tree d;
	  if (current_class_type == NULL_TREE
	      || TYPE_SIZE (current_class_type) != NULL_TREE)
	    {
	      if (current_lang_name == lang_name_cplusplus)
		d = lookup_nested_type (type, current_class_type ? TYPE_NAME (current_class_type) : NULL_TREE);
	      else
		d = NULL_TREE;

	      if (d == NULL_TREE)
		{
#if 0 /* not yet, should get fixed properly later */
		  d = make_type_decl (name, type);
		  DECL_ASSEMBLER_NAME (d) = get_identifier (build_overload_name (type, 1, 1));
#else
		  d = build_decl (TYPE_DECL, name, type);
		  DECL_ASSEMBLER_NAME (d) = get_identifier (build_overload_name (type, 1, 1));
#endif
		  /* mark the binding layer marker as internal. (mrs) */
		  DECL_SOURCE_LINE (d) = 0;
		  set_identifier_type_value (name, type);
		}
	      else
		d = TYPE_NAME (d);

	      /* If it is anonymous, then we are called from pushdecl,
		 and we don't want to infinitely recurse.  Also, if the
		 name is already in scope, we don't want to push it
		 again--pushdecl is only for pushing new decls.  */
	      if (! ANON_AGGRNAME_P (name)
		  && TYPE_NAME (type)
		  && (TREE_CODE (TYPE_NAME (type)) != TYPE_DECL
		      || lookup_name (name, 1) != TYPE_NAME (type)))
		{
		  if (class_binding_level)
		    d = pushdecl_class_level (d);
		  else
		    d = pushdecl (d);
		}
	    }
	  else
	    {
	      /* Make nested declarations go into class-level scope.  */
	      d = build_lang_field_decl (TYPE_DECL, name, type);
	      set_identifier_type_value (name, type);
	      d = pushdecl_class_level (d);
	    }
	  if (ANON_AGGRNAME_P (name))
	    DECL_IGNORED_P (d) = 1;
	  TYPE_NAME (type) = d;

	  if ((current_class_type == NULL_TREE
	       && current_function_decl == NULL_TREE)
	      || current_lang_name != lang_name_cplusplus)
	    /* Non-nested class.  */
	    DECL_NESTED_TYPENAME (d) = name;
	  else if (current_function_decl != NULL_TREE)
	    {
	      /* Function-nested class.  */
	      set_nested_typename (d, DECL_ASSEMBLER_NAME (current_function_decl),
				   name, type);
	      /* This builds the links for classes nested in fn scope.  */
	      DECL_CONTEXT (d) = current_function_decl;
	    }
	  else if (TYPE_SIZE (current_class_type) == NULL_TREE)
	    {
	      /* Class-nested class.  */
	      set_nested_typename (d, DECL_NESTED_TYPENAME (TYPE_NAME (current_class_type)),
				   name, type);
	      /* This builds the links for classes nested in type scope.  */
	      DECL_CONTEXT (d) = current_class_type;
	      DECL_CLASS_CONTEXT (d) = current_class_type;
	    }
        }
      if (b->parm_flag == 2)
	{
	  TREE_NONLOCAL_FLAG (type) = 1;
	  IDENTIFIER_CLASS_VALUE (name) = TYPE_NAME (type);
	  if (TYPE_SIZE (current_class_type) == NULL_TREE)
	    CLASSTYPE_TAGS (current_class_type) = b->tags;
	}
    }

  if (TREE_CODE (TYPE_NAME (type)) == TYPE_DECL)
    /* Use the canonical TYPE_DECL for this node.  */
    TYPE_STUB_DECL (type) = TYPE_NAME (type);
  else
    {
      /* Create a fake NULL-named TYPE_DECL node whose TREE_TYPE
	 will be the tagged type we just added to the current
	 binding level.  This fake NULL-named TYPE_DECL node helps
	 dwarfout.c to know when it needs to output a
	 representation of a tagged type, and it also gives us a
	 convenient place to record the "scope start" address for
	 the tagged type.  */

#if 0 /* not yet, should get fixed properly later */
      TYPE_STUB_DECL (type) = pushdecl (make_type_decl (NULL, type));
#else
      TYPE_STUB_DECL (type) = pushdecl (build_decl (TYPE_DECL, NULL_TREE, type));
#endif
    }
}

/* Counter used to create anonymous type names.  */
static int anon_cnt = 0;

/* Return an IDENTIFIER which can be used as a name for
   anonymous structs and unions.  */
tree
make_anon_name ()
{
  char buf[32];

  sprintf (buf, ANON_AGGRNAME_FORMAT, anon_cnt++);
  return get_identifier (buf);
}

/* Clear the TREE_PURPOSE slot of tags which have anonymous typenames.
   This keeps dbxout from getting confused.  */
void
clear_anon_tags ()
{
  register struct binding_level *b;
  register tree tags;
  static int last_cnt = 0;

  /* Fast out if no new anon names were declared.  */
  if (last_cnt == anon_cnt)
    return;

  b = current_binding_level;
  while (b->tag_transparent)
    b = b->level_chain;
  tags = b->tags;
  while (tags)
    {
      /* A NULL purpose means we have already processed all tags
	 from here to the end of the list.  */
      if (TREE_PURPOSE (tags) == NULL_TREE)
	break;
      if (ANON_AGGRNAME_P (TREE_PURPOSE (tags)))
	TREE_PURPOSE (tags) = NULL_TREE;
      tags = TREE_CHAIN (tags);
    }
  last_cnt = anon_cnt;
}

/* Subroutine of duplicate_decls: return truthvalue of whether
   or not types of these decls match.  */
static int
decls_match (newdecl, olddecl)
     tree newdecl, olddecl;
{
  int types_match;

  if (TREE_CODE (newdecl) == FUNCTION_DECL && TREE_CODE (olddecl) == FUNCTION_DECL)
    {
      tree f1 = TREE_TYPE (newdecl);
      tree f2 = TREE_TYPE (olddecl);
      tree p1 = TYPE_ARG_TYPES (f1);
      tree p2 = TYPE_ARG_TYPES (f2);

      /* When we parse a static member function definition,
	 we put together a FUNCTION_DECL which thinks its type
	 is METHOD_TYPE.  Change that to FUNCTION_TYPE, and
	 proceed.  */
      if (TREE_CODE (f1) == METHOD_TYPE && DECL_STATIC_FUNCTION_P (olddecl))
	revert_static_member_fn (&f1, &newdecl, &p1);
      else if (TREE_CODE (f2) == METHOD_TYPE
	       && DECL_STATIC_FUNCTION_P (newdecl))
	revert_static_member_fn (&f2, &olddecl, &p2);

      /* Here we must take care of the case where new default
	 parameters are specified.  Also, warn if an old
	 declaration becomes ambiguous because default
	 parameters may cause the two to be ambiguous.  */
      if (TREE_CODE (f1) != TREE_CODE (f2))
	{
	  if (TREE_CODE (f1) == OFFSET_TYPE)
	    compiler_error_with_decl (newdecl, "`%s' redeclared as member function");
	  else
	    compiler_error_with_decl (newdecl, "`%s' redeclared as non-member function");
	  return 0;
	}

      if (comptypes (TYPE_MAIN_VARIANT (TREE_TYPE (f1)),
		     TYPE_MAIN_VARIANT (TREE_TYPE (f2)), 1))
	types_match = compparms (p1, p2, 1);
      else types_match = 0;
    }
  else
    {
      if (TREE_TYPE (newdecl) == error_mark_node)
	types_match = TREE_TYPE (olddecl) == error_mark_node;
      else if (TREE_TYPE (olddecl) == NULL_TREE)
	types_match = TREE_TYPE (newdecl) == NULL_TREE;
      else
	types_match = comptypes (TREE_TYPE (newdecl), TREE_TYPE (olddecl), 1);
    }

  return types_match;
}

/* Handle when a new declaration NEWDECL has the same name as an old
   one OLDDECL in the same binding contour.  Prints an error message
   if appropriate.

   If safely possible, alter OLDDECL to look like NEWDECL, and return 1.
   Otherwise, return 0.  */

static int
duplicate_decls (newdecl, olddecl)
     register tree newdecl, olddecl;
{
  extern struct obstack permanent_obstack;
  unsigned olddecl_uid = DECL_UID (olddecl);
  int olddecl_friend = 0, types_match;
  int new_defines_function;
  register unsigned saved_old_decl_uid;
  register int saved_old_decl_friend_p;

  if (TREE_CODE (olddecl) == TREE_LIST
      && TREE_CODE (newdecl) == FUNCTION_DECL)
    {
      /* If a new decl finds a list of old decls, then
	 we assume that the new decl has C linkage, and
	 that the old decls have C++ linkage.  In this case,
	 we must look through the list to see whether
	 there is an ambiguity or not.  */
      tree olddecls = olddecl;

      /* If the overload list is empty, just install the decl.  */
      if (TREE_VALUE (olddecls) == NULL_TREE)
	{
	  TREE_VALUE (olddecls) = newdecl;
	  return 1;
	}

      while (olddecls)
	{
	  if (decls_match (newdecl, TREE_VALUE (olddecls)))
	    {
	      if (TREE_CODE (newdecl) == VAR_DECL)
		;
	      else if (DECL_LANGUAGE (newdecl)
		       != DECL_LANGUAGE (TREE_VALUE (olddecls)))
		{
		  error_with_decl (newdecl, "declaration of `%s' with different language linkage");
		  error_with_decl (TREE_VALUE (olddecls), "previous declaration here");
		}
	      types_match = 1;
	      break;
	    }
	  olddecls = TREE_CHAIN (olddecls);
	}
      if (olddecls)
	olddecl = TREE_VALUE (olddecl);
      else
	return 1;
    }
  else
    {
      if (TREE_CODE (olddecl) != TREE_LIST)
	olddecl_friend = DECL_LANG_SPECIFIC (olddecl) && DECL_FRIEND_P (olddecl);
      types_match = decls_match (newdecl, olddecl);
    }

  if ((TREE_TYPE (newdecl) && TREE_CODE (TREE_TYPE (newdecl)) == ERROR_MARK)
      || (TREE_TYPE (olddecl) && TREE_CODE (TREE_TYPE (olddecl)) == ERROR_MARK))
    types_match = 0;

  /* If this decl has linkage, and the old one does too, maybe no error.  */
  if (TREE_CODE (olddecl) != TREE_CODE (newdecl))
    {
      error_with_decl (newdecl, "`%s' redeclared as different kind of symbol");
      if (TREE_CODE (olddecl) == TREE_LIST)
	olddecl = TREE_VALUE (olddecl);
      error_with_decl (olddecl, "previous declaration of `%s'");

      /* New decl is completely inconsistent with the old one =>
	 tell caller to replace the old one.  */

      return 0;
    }

  if (TREE_CODE (newdecl) == FUNCTION_DECL)
    {
      /* Now that functions must hold information normally held
	 by field decls, there is extra work to do so that
	 declaration information does not get destroyed during
	 definition.  */
      if (DECL_VINDEX (olddecl))
	DECL_VINDEX (newdecl) = DECL_VINDEX (olddecl);
      if (DECL_CONTEXT (olddecl))
	DECL_CONTEXT (newdecl) = DECL_CONTEXT (olddecl);
      if (DECL_CLASS_CONTEXT (olddecl))
	DECL_CLASS_CONTEXT (newdecl) = DECL_CLASS_CONTEXT (olddecl);
      if (DECL_CHAIN (newdecl) == NULL_TREE)
	DECL_CHAIN (newdecl) = DECL_CHAIN (olddecl);
      if (DECL_PENDING_INLINE_INFO (newdecl) == (struct pending_inline *)0)
	DECL_PENDING_INLINE_INFO (newdecl) = DECL_PENDING_INLINE_INFO (olddecl);
    }

  if (flag_traditional && TREE_CODE (newdecl) == FUNCTION_DECL
      && IDENTIFIER_IMPLICIT_DECL (DECL_ASSEMBLER_NAME (newdecl)) == olddecl)
    /* If -traditional, avoid error for redeclaring fcn
       after implicit decl.  */
    ;
  else if (TREE_CODE (olddecl) == FUNCTION_DECL
	   && DECL_BUILT_IN (olddecl))
    {
      if (!types_match)
	{
	  error_with_decl (newdecl, "declaration of `%s'");
	  error_with_decl (olddecl, "conflicts with built-in declaration `%s'");
	}
    }
  else if (!types_match)
    {
      tree oldtype = TREE_TYPE (olddecl);
      tree newtype = TREE_TYPE (newdecl);
      int give_error = 0;

      /* Already complained about this, so don't do so again.  */
      if (current_class_type == NULL_TREE
	  || IDENTIFIER_ERROR_LOCUS (DECL_ASSEMBLER_NAME (newdecl)) != current_class_type)
	{
	  give_error = 1;
	  error_with_decl (newdecl, "conflicting types for `%s'");
	}

      /* Check for function type mismatch
	 involving an empty arglist vs a nonempty one.  */
      if (TREE_CODE (olddecl) == FUNCTION_DECL
	  && comptypes (TREE_TYPE (oldtype),
			TREE_TYPE (newtype), 1)
	  && ((TYPE_ARG_TYPES (oldtype) == NULL_TREE
	       && DECL_INITIAL (olddecl) == NULL_TREE)
	      || (TYPE_ARG_TYPES (newtype) == NULL_TREE
		  && DECL_INITIAL (newdecl) == NULL_TREE)))
	{
	  /* Classify the problem further.  */
	  register tree t = TYPE_ARG_TYPES (oldtype);
	  if (t == NULL_TREE)
	    t = TYPE_ARG_TYPES (newtype);
	  for (; t; t = TREE_CHAIN (t))
	    {
	      register tree type = TREE_VALUE (t);

	      if (TREE_CHAIN (t) == NULL_TREE && type != void_type_node)
		{
		  error ("A parameter list with an ellipsis can't match");
		  error ("an empty parameter name list declaration.");
		  break;
		}

	      if (TYPE_MAIN_VARIANT (type) == float_type_node
		  || C_PROMOTING_INTEGER_TYPE_P (type))
		{
		  error ("An argument type that has a default promotion");
		  error ("can't match an empty parameter name list declaration.");
		  break;
		}
	    }
	}
      if (give_error)
	error_with_decl (olddecl, "previous declaration of `%s'");

      /* There is one thing GNU C++ cannot tolerate: a constructor
	 which takes the type of object being constructed.
	 Farm that case out here.  */
      if (TREE_CODE (newdecl) == FUNCTION_DECL
	  && DECL_CONSTRUCTOR_P (newdecl))
	{
	  tree tmp = TREE_CHAIN (TYPE_ARG_TYPES (newtype));

	  if (tmp != NULL_TREE
	      && (TYPE_MAIN_VARIANT (TREE_VALUE (tmp))
		  == TYPE_METHOD_BASETYPE (newtype)))
	    {
	      tree parm = TREE_CHAIN (DECL_ARGUMENTS (newdecl));
	      tree argtypes
		= hash_tree_chain (build_reference_type (TREE_VALUE (tmp)),
				   TREE_CHAIN (tmp));

	      DECL_ARG_TYPE (parm)
		= TREE_TYPE (parm)
		  = TYPE_REFERENCE_TO (TREE_VALUE (tmp));

	      TREE_TYPE (newdecl) = newtype
		= build_cplus_method_type (TYPE_METHOD_BASETYPE (newtype),
					   TREE_TYPE (newtype), argtypes);
	      error ("constructor cannot take as argument the type being constructed");
	      SET_IDENTIFIER_ERROR_LOCUS (DECL_ASSEMBLER_NAME (newdecl), current_class_type);
	    }
	}
    }
  else
    {
      char *errmsg = redeclaration_error_message (newdecl, olddecl);
      if (errmsg)
	{
	  error_with_decl (newdecl, errmsg);
	  if (DECL_NAME (olddecl) != NULL_TREE)
	    error_with_decl (olddecl,
			     (DECL_INITIAL (olddecl)
			      && current_binding_level == global_binding_level)
			        ? "`%s' previously defined here"
			        : "`%s' previously declared here");
	}
      else if (TREE_CODE (olddecl) == FUNCTION_DECL
	       && DECL_INITIAL (olddecl) != NULL_TREE
	       && TYPE_ARG_TYPES (TREE_TYPE (olddecl)) == NULL_TREE
	       && TYPE_ARG_TYPES (TREE_TYPE (newdecl)) != NULL_TREE)
	{
	  /* Prototype decl follows defn w/o prototype.  */
	  warning_with_decl (newdecl, "prototype for `%s'");
	  warning_with_decl (olddecl,
			     "follows non-prototype definition here");
	}

      /* These bits are logically part of the type.  */
      if (pedantic
	  && (TREE_READONLY (newdecl) != TREE_READONLY (olddecl)
	      || TREE_THIS_VOLATILE (newdecl) != TREE_THIS_VOLATILE (olddecl)))
	error_with_decl (newdecl, "type qualifiers for `%s' conflict with previous decl");
    }

  /* Deal with C++: must preserve virtual function table size.  */
  if (TREE_CODE (olddecl) == TYPE_DECL)
    {
      if (TYPE_LANG_SPECIFIC (TREE_TYPE (newdecl))
          && TYPE_LANG_SPECIFIC (TREE_TYPE (olddecl)))
	{
	  CLASSTYPE_VSIZE (TREE_TYPE (newdecl))
	    = CLASSTYPE_VSIZE (TREE_TYPE (olddecl));
	  CLASSTYPE_FRIEND_CLASSES (TREE_TYPE (newdecl))
	    = CLASSTYPE_FRIEND_CLASSES (TREE_TYPE (olddecl));
	}
      /* why assert here?  Just because debugging information is
	 messed up? (mrs) */
      /* it happens on something like:
	 	typedef struct Thing {
                	Thing();
		        int     x;
		} Thing;
      */
#if 0
      my_friendly_assert (DECL_IGNORED_P (olddecl) == DECL_IGNORED_P (newdecl), 139);
#endif
    }

  /* Special handling ensues if new decl is a function definition.  */
  new_defines_function = (TREE_CODE (newdecl) == FUNCTION_DECL
			  && DECL_INITIAL (newdecl) != NULL_TREE);

  /* Optionally warn about more than one declaration for the same name,
     but don't warn about a function declaration followed by a definition.  */
  if (warn_redundant_decls
      && DECL_SOURCE_LINE (olddecl) != 0
      && !(new_defines_function && DECL_INITIAL (olddecl) == NULL_TREE))
    {
      warning_with_decl (newdecl, "redundant redeclaration of `%s' in same scope");
      warning_with_decl (olddecl, "previous declaration of `%s'");
    }

  /* Copy all the DECL_... slots specified in the new decl
     except for any that we copy here from the old type.  */

  if (types_match)
    {
      /* Automatically handles default parameters.  */
      tree oldtype = TREE_TYPE (olddecl);
      /* Merge the data types specified in the two decls.  */
      tree newtype = common_type (TREE_TYPE (newdecl), TREE_TYPE (olddecl));

      if (TREE_CODE (newdecl) == VAR_DECL)
	DECL_THIS_EXTERN (newdecl) |= DECL_THIS_EXTERN (olddecl);
      /* Do this after calling `common_type' so that default
	 parameters don't confuse us.  */
      else if (TREE_CODE (newdecl) == FUNCTION_DECL
	  && (TYPE_RAISES_EXCEPTIONS (TREE_TYPE (newdecl))
	      != TYPE_RAISES_EXCEPTIONS (TREE_TYPE (olddecl))))
	{
	  tree ctype = NULL_TREE;
	  ctype = DECL_CLASS_CONTEXT (newdecl);
	  TREE_TYPE (newdecl) = build_exception_variant (ctype, newtype,
							 TYPE_RAISES_EXCEPTIONS (TREE_TYPE (newdecl)));
	  TREE_TYPE (olddecl) = build_exception_variant (ctype, newtype,
							 TYPE_RAISES_EXCEPTIONS (oldtype));

	  if (! compexcepttypes (TREE_TYPE (newdecl), TREE_TYPE(olddecl), 0))
	    {
	      error_with_decl (newdecl, "declaration of `%s' raises different exceptions...");
	      error_with_decl (olddecl, "...from previous declaration here");
	    }
	}
      TREE_TYPE (newdecl) = TREE_TYPE (olddecl) = newtype;

      /* Lay the type out, unless already done.  */
      if (oldtype != TREE_TYPE (newdecl))
	{
	  if (TREE_TYPE (newdecl) != error_mark_node)
	    layout_type (TREE_TYPE (newdecl));
	  if (TREE_CODE (newdecl) != FUNCTION_DECL
	      && TREE_CODE (newdecl) != TYPE_DECL
	      && TREE_CODE (newdecl) != CONST_DECL)
	    layout_decl (newdecl, 0);
	}
      else
	{
	  /* Since the type is OLDDECL's, make OLDDECL's size go with.  */
	  DECL_SIZE (newdecl) = DECL_SIZE (olddecl);
	}

      /* Merge the type qualifiers.  */
      if (TREE_READONLY (newdecl))
	TREE_READONLY (olddecl) = 1;
      if (TREE_THIS_VOLATILE (newdecl))
	TREE_THIS_VOLATILE (olddecl) = 1;

      /* Merge the initialization information.  */
      if (DECL_INITIAL (newdecl) == NULL_TREE)
	DECL_INITIAL (newdecl) = DECL_INITIAL (olddecl);
      /* Keep the old rtl since we can safely use it, unless it's the
	 call to abort() used for abstract virtuals.  */
      if ((DECL_LANG_SPECIFIC (olddecl)
	   && !DECL_ABSTRACT_VIRTUAL_P (olddecl))
	  || DECL_RTL (olddecl) != DECL_RTL (abort_fndecl))
	DECL_RTL (newdecl) = DECL_RTL (olddecl);
    }
  /* If cannot merge, then use the new type and qualifiers,
     and don't preserve the old rtl.  */
  else
    {
      /* Clean out any memory we had of the old declaration.  */
      tree oldstatic = value_member (olddecl, static_aggregates);
      if (oldstatic)
	TREE_VALUE (oldstatic) = error_mark_node;

      TREE_TYPE (olddecl) = TREE_TYPE (newdecl);
      TREE_READONLY (olddecl) = TREE_READONLY (newdecl);
      TREE_THIS_VOLATILE (olddecl) = TREE_THIS_VOLATILE (newdecl);
      TREE_SIDE_EFFECTS (olddecl) = TREE_SIDE_EFFECTS (newdecl);
    }

  /* Merge the storage class information.  */
  if (DECL_EXTERNAL (newdecl))
    {
      TREE_STATIC (newdecl) = TREE_STATIC (olddecl);
      DECL_EXTERNAL (newdecl) = DECL_EXTERNAL (olddecl);

      /* For functions, static overrides non-static.  */
      if (TREE_CODE (newdecl) == FUNCTION_DECL)
	{
	  TREE_PUBLIC (newdecl) &= TREE_PUBLIC (olddecl);
	  /* This is since we don't automatically
	     copy the attributes of NEWDECL into OLDDECL.  */
	  TREE_PUBLIC (olddecl) = TREE_PUBLIC (newdecl);
	  /* If this clears `static', clear it in the identifier too.  */
	  if (! TREE_PUBLIC (olddecl))
	    TREE_PUBLIC (DECL_ASSEMBLER_NAME (olddecl)) = 0;
	}
      else
	TREE_PUBLIC (newdecl) = TREE_PUBLIC (olddecl);
    }
  else
    {
      TREE_STATIC (olddecl) = TREE_STATIC (newdecl);
      /* A `const' which was not declared `extern' and is
	 in static storage is invisible.  */
      if (TREE_CODE (newdecl) == VAR_DECL
	  && TREE_READONLY (newdecl) && TREE_STATIC (newdecl)
	  && ! DECL_THIS_EXTERN (newdecl))
	TREE_PUBLIC (newdecl) = 0;
      TREE_PUBLIC (olddecl) = TREE_PUBLIC (newdecl);
    }

  /* If either decl says `inline', this fn is inline,
     unless its definition was passed already.  */
  if (DECL_INLINE (newdecl) && DECL_INITIAL (olddecl) == NULL_TREE)
    DECL_INLINE (olddecl) = 1;
  DECL_INLINE (newdecl) = DECL_INLINE (olddecl);

  if (TREE_CODE (newdecl) == FUNCTION_DECL)
    {
      if (new_defines_function)
	/* If defining a function declared with other language
	   linkage, use the previously declared language linkage.  */
	DECL_LANGUAGE (newdecl) = DECL_LANGUAGE (olddecl);
      else
	{
	  /* If redeclaring a builtin function, and not a definition,
	     it stays built in.  */
	  if (DECL_BUILT_IN (olddecl))
	    {
	      DECL_BUILT_IN (newdecl) = 1;
	      DECL_SET_FUNCTION_CODE (newdecl, DECL_FUNCTION_CODE (olddecl));
	      /* If we're keeping the built-in definition, keep the rtl,
		 regardless of declaration matches.  */
	      DECL_RTL (newdecl) = DECL_RTL (olddecl);
	    }
	  else
	    DECL_FRAME_SIZE (newdecl) = DECL_FRAME_SIZE (olddecl);

	  DECL_RESULT (newdecl) = DECL_RESULT (olddecl);
	  if (DECL_SAVED_INSNS (newdecl) = DECL_SAVED_INSNS (olddecl))
	    /* Previously saved insns go together with
	       the function's previous definition.  */
	    DECL_INITIAL (newdecl) = DECL_INITIAL (olddecl);
	  /* Don't clear out the arguments if we're redefining a function.  */
	  if (DECL_ARGUMENTS (olddecl))
	    DECL_ARGUMENTS (newdecl) = DECL_ARGUMENTS (olddecl);
	}
    }

  /* Now preserve various other info from the definition.  */
  TREE_ADDRESSABLE (newdecl) = TREE_ADDRESSABLE (olddecl);
  TREE_ASM_WRITTEN (newdecl) = TREE_ASM_WRITTEN (olddecl);

  /* Don't really know how much of the language-specific
     values we should copy from old to new.  */
#if 1
  if (DECL_LANG_SPECIFIC (olddecl))
    DECL_IN_AGGR_P (newdecl) = DECL_IN_AGGR_P (olddecl);
#endif

  /* We are about to copy the contexts of newdecl into olddecl, so save a
     few tidbits of information from olddecl that we may need to restore
     after the copying takes place.  */

  saved_old_decl_uid = DECL_UID (olddecl);
  saved_old_decl_friend_p
    = DECL_LANG_SPECIFIC (olddecl) ? DECL_FRIEND_P (olddecl) : 0;

  if (TREE_CODE (newdecl) == FUNCTION_DECL)
    {
      int function_size;
      struct lang_decl *ol = DECL_LANG_SPECIFIC (olddecl);
      struct lang_decl *nl = DECL_LANG_SPECIFIC (newdecl);

      function_size = sizeof (struct tree_decl);

      bcopy ((char *) newdecl + sizeof (struct tree_common),
	     (char *) olddecl + sizeof (struct tree_common),
	     function_size - sizeof (struct tree_common));

      if ((char *)newdecl + ((function_size + sizeof (struct lang_decl)
			     + obstack_alignment_mask (&permanent_obstack))
			     & ~ obstack_alignment_mask (&permanent_obstack))
	  == obstack_next_free (&permanent_obstack))
	{
	  DECL_MAIN_VARIANT (newdecl) = olddecl;
	  DECL_LANG_SPECIFIC (olddecl) = ol;
	  bcopy ((char *)nl, (char *)ol, sizeof (struct lang_decl));

	  obstack_free (&permanent_obstack, newdecl);
	}
      else if (LANG_DECL_PERMANENT (ol))
	{
	  if (DECL_MAIN_VARIANT (olddecl) == olddecl)
	    {
	      /* Save these lang_decls that would otherwise be lost.  */
	      extern tree free_lang_decl_chain;
	      tree free_lang_decl = (tree) ol;
	      TREE_CHAIN (free_lang_decl) = free_lang_decl_chain;
	      free_lang_decl_chain = free_lang_decl;
	    }
	  else
	    {
	      /* Storage leak.  */
	    }
	}
    }
  else
    {
      bcopy ((char *) newdecl + sizeof (struct tree_common),
	     (char *) olddecl + sizeof (struct tree_common),
	     sizeof (struct tree_decl) - sizeof (struct tree_common)
	     + tree_code_length [(int)TREE_CODE (newdecl)] * sizeof (char *));
    }

  DECL_UID (olddecl) = olddecl_uid;
  if (olddecl_friend)
    DECL_FRIEND_P (olddecl) = 1;

  /* Restore some pieces of information which were originally in olddecl.  */

  DECL_UID (olddecl) = saved_old_decl_uid;
  if (DECL_LANG_SPECIFIC (olddecl))
    DECL_FRIEND_P (olddecl) |= saved_old_decl_friend_p;

  return 1;
}

void
adjust_type_value (id)
     tree id;
{
  tree t;

  if (current_binding_level != global_binding_level)
    {
      if (current_binding_level != class_binding_level)
	{
	  t = IDENTIFIER_LOCAL_VALUE (id);
	  if (t && TREE_CODE (t) == TYPE_DECL)
	    {
	    set_it:
	      SET_IDENTIFIER_TYPE_VALUE (id, TREE_TYPE (t));
	      return;
	    }
	}
      else
	my_friendly_abort (7);

      if (current_class_type)
	{
	  t = IDENTIFIER_CLASS_VALUE (id);
	  if (t && TREE_CODE (t) == TYPE_DECL)
	    goto set_it;
	}
    }

  t = IDENTIFIER_GLOBAL_VALUE (id);
  if (t && TREE_CODE (t) == TYPE_DECL)
    goto set_it;
  if (t && TREE_CODE (t) == TEMPLATE_DECL)
    SET_IDENTIFIER_TYPE_VALUE (id, NULL_TREE);
}

/* Record a decl-node X as belonging to the current lexical scope.
   Check for errors (such as an incompatible declaration for the same
   name already seen in the same scope).

   Returns either X or an old decl for the same name.
   If an old decl is returned, it may have been smashed
   to agree with what X says.  */

tree
pushdecl (x)
     tree x;
{
  register tree t;
#if 0 /* not yet, should get fixed properly later */
  register tree name;
#else
  register tree name = DECL_ASSEMBLER_NAME (x);
#endif
  register struct binding_level *b = current_binding_level;

#if 0
  static int nglobals; int len;

  len = list_length (global_binding_level->names);
  if (len < nglobals)
    my_friendly_abort (8);
  else if (len > nglobals)
    nglobals = len;
#endif

  /* Don't change DECL_CONTEXT of virtual methods.  */
  if (x != current_function_decl
      && (TREE_CODE (x) != FUNCTION_DECL
	  || !DECL_VIRTUAL_P (x)))
    DECL_CONTEXT (x) = current_function_decl;
  /* A local declaration for a function doesn't constitute nesting.  */
  if (TREE_CODE (x) == FUNCTION_DECL && DECL_INITIAL (x) == 0)
    DECL_CONTEXT (x) = 0;

#if 0 /* not yet, should get fixed properly later */
  /* For functions and class static data, we currently look up the encoded
     form of the name.  For types, we want the real name.  The former will
     probably be changed soon, according to MDT.  */
  if (TREE_CODE (x) == FUNCTION_DECL || TREE_CODE (x) == VAR_DECL)
    name = DECL_ASSEMBLER_NAME (x);
  else
    name = DECL_NAME (x);
#else
  /* Type are looked up using the DECL_NAME, as that is what the rest of the
     compiler wants to use. */
  if (TREE_CODE (x) == TYPE_DECL)
    name = DECL_NAME (x);
#endif

  if (name)
    {
      char *file;
      int line;

      t = lookup_name_current_level (name);
      if (t == error_mark_node)
	{
	  /* error_mark_node is 0 for a while during initialization!  */
	  t = NULL_TREE;
	  error_with_decl (x, "`%s' used prior to declaration");
	}

      if (t != NULL_TREE)
	{
	  if (TREE_CODE (t) == PARM_DECL)
	    {
	      if (DECL_CONTEXT (t) == NULL_TREE)
		fatal ("parse errors have confused me too much");
	    }
	  file = DECL_SOURCE_FILE (t);
	  line = DECL_SOURCE_LINE (t);
	}

      if (t != NULL_TREE && TREE_CODE (t) != TREE_CODE (x))
	{
	  if (TREE_CODE (t) == TYPE_DECL || TREE_CODE (x) == TYPE_DECL)
	    {
	      /* We do nothing special here, because C++ does such nasty
		 things with TYPE_DECLs.  Instead, just let the TYPE_DECL
		 get shadowed, and know that if we need to find a TYPE_DECL
		 for a given name, we can look in the IDENTIFIER_TYPE_VALUE
		 slot of the identifier.  */
	      ;
	    }
	  else if (duplicate_decls (x, t))
	    return t;
	}
      else if (t != NULL_TREE && duplicate_decls (x, t))
	{
	  /* If this decl is `static' and an `extern' was seen previously,
	     that is erroneous.  But don't complain if -traditional,
	     since traditional compilers don't complain.

	     Note that this does not apply to the C++ case of declaring
	     a variable `extern const' and then later `const'.  */
	  if (!flag_traditional && TREE_PUBLIC (name)
	      && ! TREE_PUBLIC (x) && ! DECL_EXTERNAL (x) && ! DECL_INLINE (x))
	    {
	      /* Due to interference in memory reclamation (X may be
		 obstack-deallocated at this point), we must guard against
		 one really special case.  */
	      if (current_function_decl == x)
		current_function_decl = t;
	      if (IDENTIFIER_IMPLICIT_DECL (name))
		warning ("`%s' was declared implicitly `extern' and later `static'",
			 lang_printable_name (t));
	      else
		warning ("`%s' was declared `extern' and later `static'",
			 lang_printable_name (t));
	      warning_with_file_and_line (file, line,
					  "previous declaration of `%s'",
					  lang_printable_name (t));
	    }
	  return t;
	}

      /* If declaring a type as a typedef, and the type has no known
	 typedef name, install this TYPE_DECL as its typedef name.

	 C++: If it had an anonymous aggregate or enum name,
	 give it a `better' one.  */
      if (TREE_CODE (x) == TYPE_DECL)
	{
	  tree name = TYPE_NAME (TREE_TYPE (x));

	  if (name == NULL_TREE || TREE_CODE (name) != TYPE_DECL)
	    {
	      /* If these are different names, and we're at the global
		 binding level, make two equivalent definitions.  */
              name = x;
              if (global_bindings_p ())
                TYPE_NAME (TREE_TYPE (x)) = x;
	    }
	  else
	    {
	      tree tname = DECL_NAME (name);
	      if (global_bindings_p () && ANON_AGGRNAME_P (tname))
		{
		  /* do gratuitous C++ typedefing, and make sure that
		     we access this type either through TREE_TYPE field
		     or via the tags list.  */
		  TYPE_NAME (TREE_TYPE (x)) = x;
		  pushtag (tname, TREE_TYPE (x));
		}
	    }
	  my_friendly_assert (TREE_CODE (name) == TYPE_DECL, 140);
	  if (DECL_NAME (name) && !DECL_NESTED_TYPENAME (name))
	    set_nested_typename (x, current_class_name, DECL_NAME (name),
				 TREE_TYPE (x));
	  if (TYPE_NAME (TREE_TYPE (x)) && TYPE_IDENTIFIER (TREE_TYPE (x)))
            set_identifier_type_value (DECL_NAME (x), TREE_TYPE (x));
/* was using TYPE_IDENTIFIER (TREE_TYPE (x)) */
	}

      /* Multiple external decls of the same identifier ought to match.  */

      if (DECL_EXTERNAL (x) && IDENTIFIER_GLOBAL_VALUE (name) != NULL_TREE
	  && (DECL_EXTERNAL (IDENTIFIER_GLOBAL_VALUE (name))
	      || TREE_PUBLIC (IDENTIFIER_GLOBAL_VALUE (name)))
	  /* We get warnings about inline functions where they are defined.
	     Avoid duplicate warnings where they are used.  */
	  && !DECL_INLINE (x))
	{
	  if (! comptypes (TREE_TYPE (x),
			   TREE_TYPE (IDENTIFIER_GLOBAL_VALUE (name)), 1))
	    {
	      warning_with_decl (x,
				 "type mismatch with previous external decl");
	      warning_with_decl (IDENTIFIER_GLOBAL_VALUE (name),
				 "previous external decl of `%s'");
	    }
	}

      /* In PCC-compatibility mode, extern decls of vars with no current decl
	 take effect at top level no matter where they are.  */
      if (flag_traditional && DECL_EXTERNAL (x)
	  && lookup_name (name, 0) == NULL_TREE)
	b = global_binding_level;

      /* This name is new in its binding level.
	 Install the new declaration and return it.  */
      if (b == global_binding_level)
	{
	  /* Install a global value.  */

	  /* Rule for VAR_DECLs, but not for other kinds of _DECLs:
	     A `const' which was not declared `extern' is invisible.  */
	  if (TREE_CODE (x) == VAR_DECL
	      && TREE_READONLY (x) && ! DECL_THIS_EXTERN (x))
	    TREE_PUBLIC (x) = 0;

	  /* If the first global decl has external linkage,
	     warn if we later see static one.  */
	  if (IDENTIFIER_GLOBAL_VALUE (name) == NULL_TREE && TREE_PUBLIC (x))
	    TREE_PUBLIC (name) = 1;

	  /* Don't install a TYPE_DECL if we already have another
	     sort of _DECL with that name.  */
	  if (TREE_CODE (x) != TYPE_DECL
	      || t == NULL_TREE
	      || TREE_CODE (t) == TYPE_DECL)
#if 0
	    /* This has not be thoroughly tested yet. */
	    /* It allows better dwarf debugging. */
	    IDENTIFIER_GLOBAL_VALUE (name)
	      = TREE_CODE_CLASS (TREE_CODE (x)) == 'd'
		? x : build_decl (TYPE_DECL, NULL, TREE_TYPE (x));
#else
	    IDENTIFIER_GLOBAL_VALUE (name) = x;
#endif

	  /* Don't forget if the function was used via an implicit decl.  */
	  if (IDENTIFIER_IMPLICIT_DECL (name)
	      && TREE_USED (IDENTIFIER_IMPLICIT_DECL (name)))
	    TREE_USED (x) = 1;

	  /* Don't forget if its address was taken in that way.  */
	  if (IDENTIFIER_IMPLICIT_DECL (name)
	      && TREE_ADDRESSABLE (IDENTIFIER_IMPLICIT_DECL (name)))
	    TREE_ADDRESSABLE (x) = 1;

	  /* Warn about mismatches against previous implicit decl.  */
	  if (IDENTIFIER_IMPLICIT_DECL (name) != NULL_TREE
	      /* If this real decl matches the implicit, don't complain.  */
	      && ! (TREE_CODE (x) == FUNCTION_DECL
		    && TREE_TYPE (TREE_TYPE (x)) == integer_type_node))
	    warning ("`%s' was previously implicitly declared to return `int'",
		     lang_printable_name (x));

	  /* If this decl is `static' and an `extern' was seen previously,
	     that is erroneous.  Don't do this for TYPE_DECLs.  */
	  if (TREE_PUBLIC (name)
	      && TREE_CODE (x) != TYPE_DECL
	      && ! TREE_PUBLIC (x) && ! DECL_EXTERNAL (x))
	    {
	      if (IDENTIFIER_IMPLICIT_DECL (name))
		warning ("`%s' was declared implicitly `extern' and later `static'",
			 lang_printable_name (x));
	      else
		warning ("`%s' was declared `extern' and later `static'",
			 lang_printable_name (x));
	    }
	}
      else
	{
	  /* Here to install a non-global value.  */
	  tree oldlocal = IDENTIFIER_LOCAL_VALUE (name);
	  tree oldglobal = IDENTIFIER_GLOBAL_VALUE (name);
	  set_identifier_local_value (name, x);

	  /* If this is an extern function declaration, see if we
	     have a global definition or declaration for the function.  */
	  if (oldlocal == NULL_TREE
	      && DECL_EXTERNAL (x) && !DECL_INLINE (x)
	      && oldglobal != NULL_TREE
	      && TREE_CODE (x) == FUNCTION_DECL
	      && TREE_CODE (oldglobal) == FUNCTION_DECL)
	    {
	      /* We have one.  Their types must agree.  */
	      if (! comptypes (TREE_TYPE (x), TREE_TYPE (oldglobal), 1))
		warning_with_decl (x, "extern declaration of `%s' doesn't match global one");
	      else
		{
		  /* Inner extern decl is inline if global one is.
		     Copy enough to really inline it.  */
		  if (DECL_INLINE (oldglobal))
		    {
		      DECL_INLINE (x) = DECL_INLINE (oldglobal);
		      DECL_INITIAL (x) = (current_function_decl == oldglobal
					  ? NULL_TREE : DECL_INITIAL (oldglobal));
		      DECL_SAVED_INSNS (x) = DECL_SAVED_INSNS (oldglobal);
		      DECL_ARGUMENTS (x) = DECL_ARGUMENTS (oldglobal);
		      DECL_RESULT (x) = DECL_RESULT (oldglobal);
		      TREE_ASM_WRITTEN (x) = TREE_ASM_WRITTEN (oldglobal);
		      DECL_ABSTRACT_ORIGIN (x) = oldglobal;
		    }
		  /* Inner extern decl is built-in if global one is.  */
		  if (DECL_BUILT_IN (oldglobal))
		    {
		      DECL_BUILT_IN (x) = DECL_BUILT_IN (oldglobal);
		      DECL_SET_FUNCTION_CODE (x, DECL_FUNCTION_CODE (oldglobal));
		    }
		  /* Keep the arg types from a file-scope fcn defn.  */
		  if (TYPE_ARG_TYPES (TREE_TYPE (oldglobal)) != NULL_TREE
		      && DECL_INITIAL (oldglobal)
		      && TYPE_ARG_TYPES (TREE_TYPE (x)) == NULL_TREE)
		    TREE_TYPE (x) = TREE_TYPE (oldglobal);
		}
	    }
	  /* If we have a local external declaration,
	     and no file-scope declaration has yet been seen,
	     then if we later have a file-scope decl it must not be static.  */
	  if (oldlocal == NULL_TREE
	      && oldglobal == NULL_TREE
	      && DECL_EXTERNAL (x)
	      && TREE_PUBLIC (x))
	    {
	      TREE_PUBLIC (name) = 1;
	    }

	  if (DECL_FROM_INLINE (x))
	    /* Inline decls shadow nothing.  */;

	  /* Warn if shadowing an argument at the top level of the body.  */
	  else if (oldlocal != NULL_TREE && !DECL_EXTERNAL (x)
	      && TREE_CODE (oldlocal) == PARM_DECL
	      && TREE_CODE (x) != PARM_DECL)
	    {
	      /* Go to where the parms should be and see if we
		 find them there.  */
	      struct binding_level *b = current_binding_level->level_chain;

	      if (cleanup_label)
		b = b->level_chain;

	      /* ARM $8.3 */
	      if (b->parm_flag == 1)
		pedwarn ("declaration of `%s' shadows a parameter",
			 IDENTIFIER_POINTER (name));
	    }
	  /* Maybe warn if shadowing something else.  */
	  else if (warn_shadow && !DECL_EXTERNAL (x)
		   /* No shadow warnings for internally generated vars.  */
		   && DECL_SOURCE_LINE (x) != 0
		   /* No shadow warnings for vars made for inlining.  */
		   && ! DECL_FROM_INLINE (x))
	    {
	      char *warnstring = NULL;

	      if (oldlocal != NULL_TREE && TREE_CODE (oldlocal) == PARM_DECL)
		warnstring = "declaration of `%s' shadows a parameter";
	      else if (IDENTIFIER_CLASS_VALUE (name) != NULL_TREE)
		warnstring = "declaration of `%s' shadows a member of `this'";
	      else if (oldlocal != NULL_TREE)
		warnstring = "declaration of `%s' shadows previous local";
	      else if (oldglobal != NULL_TREE)
		warnstring = "declaration of `%s' shadows global declaration";

	      if (warnstring)
		warning (warnstring, IDENTIFIER_POINTER (name));
	    }

	  /* If storing a local value, there may already be one (inherited).
	     If so, record it for restoration when this binding level ends.  */
	  if (oldlocal != NULL_TREE)
	    b->shadowed = tree_cons (name, oldlocal, b->shadowed);
	}

      /* Keep count of variables in this level with incomplete type.  */
      if (TREE_CODE (x) != TEMPLATE_DECL
	  && TREE_CODE (x) != CPLUS_CATCH_DECL
	  && TYPE_SIZE (TREE_TYPE (x)) == NULL_TREE
	  && PROMOTES_TO_AGGR_TYPE (TREE_TYPE (x), ARRAY_TYPE))
	{
	  if (++b->n_incomplete == 0)
	    error ("too many incomplete variables at this point");
	}
    }

  if (TREE_CODE (x) == TYPE_DECL && name != NULL_TREE)
    {
      adjust_type_value (name);
      if (current_class_name)
	{
	  if (!DECL_NESTED_TYPENAME (x))
	    set_nested_typename (x, current_class_name, DECL_NAME (x),
				 TREE_TYPE (x));
	  adjust_type_value (DECL_NESTED_TYPENAME (x));
	}
    }

  /* Put decls on list in reverse order.
     We will reverse them later if necessary.  */
  TREE_CHAIN (x) = b->names;
  b->names = x;
  if (! (b != global_binding_level || TREE_PERMANENT (x)))
    my_friendly_abort (124);

  return x;
}

/* Like pushdecl, only it places X in GLOBAL_BINDING_LEVEL,
   if appropriate.  */
tree
pushdecl_top_level (x)
     tree x;
{
  register tree t;
  register struct binding_level *b = current_binding_level;

  current_binding_level = global_binding_level;
  t = pushdecl (x);
  current_binding_level = b;
  if (class_binding_level)
    b = class_binding_level;
  /* Now, the type_shadowed stack may screw us.  Munge it so it does
     what we want.  */
  if (TREE_CODE (x) == TYPE_DECL)
    {
      tree name = DECL_NAME (x);
      tree newval;
      tree *ptr = (tree *)0;
      for (; b != global_binding_level; b = b->level_chain)
        {
          tree shadowed = b->type_shadowed;
          for (; shadowed; shadowed = TREE_CHAIN (shadowed))
            if (TREE_PURPOSE (shadowed) == name)
              {
		ptr = &TREE_VALUE (shadowed);
		/* Can't break out of the loop here because sometimes
		   a binding level will have duplicate bindings for
		   PT names.  It's gross, but I haven't time to fix it.  */
              }
        }
      newval = TREE_TYPE (x);
      if (ptr == (tree *)0)
        {
          /* @@ This shouldn't be needed.  My test case "zstring.cc" trips
             up here if this is changed to an assertion.  --KR  */
	  SET_IDENTIFIER_TYPE_VALUE (name, newval);
	}
      else
        {
#if 0
	  /* Disabled this 11/10/92, since there are many cases which
	     behave just fine when *ptr doesn't satisfy either of these.
	     For example, nested classes declared as friends of their enclosing
	     class will not meet this criteria.  (bpk) */
	  my_friendly_assert (*ptr == NULL_TREE || *ptr == newval, 141);
#endif
	  *ptr = newval;
        }
    }
  return t;
}

/* Like push_overloaded_decl, only it places X in GLOBAL_BINDING_LEVEL,
   if appropriate.  */
void
push_overloaded_decl_top_level (x, forget)
     tree x;
     int forget;
{
  struct binding_level *b = current_binding_level;

  current_binding_level = global_binding_level;
  push_overloaded_decl (x, forget);
  current_binding_level = b;
}

/* Make the declaration of X appear in CLASS scope.  */
tree
pushdecl_class_level (x)
     tree x;
{
  /* Don't use DECL_ASSEMBLER_NAME here!  Everything that looks in class
     scope looks for the pre-mangled name.  */
  register tree name = DECL_NAME (x);

  if (name)
    {
      tree oldclass = IDENTIFIER_CLASS_VALUE (name);
      if (oldclass)
	class_binding_level->class_shadowed
	  = tree_cons (name, oldclass, class_binding_level->class_shadowed);
      IDENTIFIER_CLASS_VALUE (name) = x;
      obstack_ptr_grow (&decl_obstack, x);
      if (TREE_CODE (x) == TYPE_DECL && !DECL_NESTED_TYPENAME (x))
	set_nested_typename (x, current_class_name, name, TREE_TYPE (x));
    }
  return x;
}

/* Tell caller how to interpret a TREE_LIST which contains
   chains of FUNCTION_DECLS.  */
int
overloaded_globals_p (list)
     tree list;
{
  my_friendly_assert (TREE_CODE (list) == TREE_LIST, 142);

  /* Don't commit caller to seeing them as globals.  */
  if (TREE_NONLOCAL_FLAG (list))
    return -1;
  /* Do commit caller to seeing them as globals.  */
  if (TREE_CODE (TREE_PURPOSE (list)) == IDENTIFIER_NODE)
    return 1;
  /* Do commit caller to not seeing them as globals.  */
  return 0;
}

/* DECL is a FUNCTION_DECL which may have other definitions already in place.
   We get around this by making IDENTIFIER_GLOBAL_VALUE (DECL_NAME (DECL))
   point to a list of all the things that want to be referenced by that name.
   It is then up to the users of that name to decide what to do with that
   list.

   DECL may also be a TEMPLATE_DECL, with a FUNCTION_DECL in its DECL_RESULT
   slot.  It is dealt with the same way.

   The value returned may be a previous declaration if we guessed wrong
   about what language DECL should belong to (C or C++).  Otherwise,
   it's always DECL (and never something that's not a _DECL).  */
tree
push_overloaded_decl (decl, forgettable)
     tree decl;
     int forgettable;
{
  tree orig_name = DECL_NAME (decl);
  tree glob = IDENTIFIER_GLOBAL_VALUE (orig_name);

  DECL_OVERLOADED (decl) = 1;
  if (glob)
    {
      if (TREE_CODE (glob) != TREE_LIST)
	{
	  if (DECL_LANGUAGE (decl) == lang_c)
	    {
	      if (TREE_CODE (glob) == FUNCTION_DECL)
		{
		  if (DECL_LANGUAGE (glob) == lang_c)
		    {
		      error_with_decl (decl, "C-language function `%s' overloaded here");
		      error_with_decl (glob, "Previous C-language version of this function was `%s'");
		    }
		}
	      else
		my_friendly_abort (9);
	    }
	  if (forgettable
	      && ! flag_traditional
	      && TREE_PERMANENT (glob) == 1
	      && !global_bindings_p ())
	    overloads_to_forget = tree_cons (orig_name, glob, overloads_to_forget);
	  /* We cache the value of builtin functions as ADDR_EXPRs
	     in the name space.  Convert it to some kind of _DECL after
	     remembering what to forget.  */
	  if (TREE_CODE (glob) == ADDR_EXPR)
	    glob = TREE_OPERAND (glob, 0);

	  if (TREE_CODE (glob) == FUNCTION_DECL
	      && DECL_LANGUAGE (glob) != DECL_LANGUAGE (decl)
	      && comptypes (TREE_TYPE (glob), TREE_TYPE (decl), 1))
	    {
	      if (current_lang_stack == current_lang_base)
		{
		  DECL_LANGUAGE (decl) = DECL_LANGUAGE (glob);
		  return glob;
		}
	      else
		{
		  error_with_decl (decl, "conflicting language contexts for declaration of `%s';");
		  error_with_decl (glob, "conflicts with previous declaration here");
		}
	    }
	  if (pedantic && TREE_CODE (glob) == VAR_DECL)
	    {
	      my_friendly_assert (TREE_CODE_CLASS (TREE_CODE (glob)) == 'd', 143);
	      error_with_decl (glob, "non-function declaration `%s'");
	      error_with_decl (decl, "conflicts with function declaration `%s'");
	    }
	  glob = tree_cons (orig_name, glob, NULL_TREE);
	  glob = tree_cons (TREE_PURPOSE (glob), decl, glob);
	  IDENTIFIER_GLOBAL_VALUE (orig_name) = glob;
	  TREE_TYPE (glob) = unknown_type_node;
	  return decl;
	}

      if (TREE_VALUE (glob) == NULL_TREE)
	{
	  TREE_VALUE (glob) = decl;
	  return decl;
	}
      if (TREE_CODE (decl) != TEMPLATE_DECL)
        {
          tree name = DECL_ASSEMBLER_NAME (decl);
          tree tmp;
	  
	  for (tmp = glob; tmp; tmp = TREE_CHAIN (tmp))
	    {
	      if (TREE_CODE (TREE_VALUE (tmp)) == FUNCTION_DECL
		  && DECL_LANGUAGE (TREE_VALUE (tmp)) != DECL_LANGUAGE (decl)
		  && comptypes (TREE_TYPE (TREE_VALUE (tmp)), TREE_TYPE (decl),
				1))
		{
		  error_with_decl (decl,
				   "conflicting language contexts for declaration of `%s';");
		  error_with_decl (TREE_VALUE (tmp),
				   "conflicts with previous declaration here");
		}
	      if (TREE_CODE (TREE_VALUE (tmp)) != TEMPLATE_DECL
		  && DECL_ASSEMBLER_NAME (TREE_VALUE (tmp)) == name)
		return decl;
	    }
	}
    }
  if (DECL_LANGUAGE (decl) == lang_c)
    {
      tree decls = glob;
      while (decls && DECL_LANGUAGE (TREE_VALUE (decls)) == lang_cplusplus)
	decls = TREE_CHAIN (decls);
      if (decls)
	{
	  error_with_decl (decl, "C-language function `%s' overloaded here");
	  error_with_decl (TREE_VALUE (decls), "Previous C-language version of this function was `%s'");
	}
    }

  if (forgettable
      && ! flag_traditional
      && (glob == NULL_TREE || TREE_PERMANENT (glob) == 1)
      && !global_bindings_p ()
      && !pseudo_global_level_p ())
    overloads_to_forget = tree_cons (orig_name, glob, overloads_to_forget);
  glob = tree_cons (orig_name, decl, glob);
  IDENTIFIER_GLOBAL_VALUE (orig_name) = glob;
  TREE_TYPE (glob) = unknown_type_node;
  return decl;
}

/* Generate an implicit declaration for identifier FUNCTIONID
   as a function of type int ().  Print a warning if appropriate.  */

tree
implicitly_declare (functionid)
     tree functionid;
{
  register tree decl;
  int temp = allocation_temporary_p ();

  push_obstacks_nochange ();

  /* Save the decl permanently so we can warn if definition follows.
     In ANSI C, warn_implicit is usually false, so the saves little space.
     But in C++, it's usually true, hence the extra code.  */
  if (temp && (flag_traditional || !warn_implicit
	       || current_binding_level == global_binding_level))
    end_temporary_allocation ();

  /* We used to reuse an old implicit decl here,
     but this loses with inline functions because it can clobber
     the saved decl chains.  */
  decl = build_lang_decl (FUNCTION_DECL, functionid, default_function_type);

  DECL_EXTERNAL (decl) = 1;
  TREE_PUBLIC (decl) = 1;

  /* ANSI standard says implicit declarations are in the innermost block.
     So we record the decl in the standard fashion.
     If flag_traditional is set, pushdecl does it top-level.  */
  pushdecl (decl);
  rest_of_decl_compilation (decl, NULL_PTR, 0, 0);

  if (warn_implicit
      /* Only one warning per identifier.  */
      && IDENTIFIER_IMPLICIT_DECL (functionid) == NULL_TREE)
    {
      pedwarn ("implicit declaration of function `%s'",
	       IDENTIFIER_POINTER (functionid));
    }

  SET_IDENTIFIER_IMPLICIT_DECL (functionid, decl);

  pop_obstacks ();

  return decl;
}

/* Return zero if the declaration NEWDECL is valid
   when the declaration OLDDECL (assumed to be for the same name)
   has already been seen.
   Otherwise return an error message format string with a %s
   where the identifier should go.  */

static char *
redeclaration_error_message (newdecl, olddecl)
     tree newdecl, olddecl;
{
  if (TREE_CODE (newdecl) == TYPE_DECL)
    {
      /* Because C++ can put things into name space for free,
	 constructs like "typedef struct foo { ... } foo"
	 would look like an erroneous redeclaration.  */
      if (TREE_TYPE (olddecl) == TREE_TYPE (newdecl))
	return 0;
      else
	return "redefinition of `%s'";
    }
  else if (TREE_CODE (newdecl) == FUNCTION_DECL)
    {
      /* If this is a pure function, its olddecl will actually be
	 the original initialization to `0' (which we force to call
	 abort()).  Don't complain about redefinition in this case.  */
      if (DECL_LANG_SPECIFIC (olddecl) && DECL_ABSTRACT_VIRTUAL_P (olddecl))
	return 0;

      /* Declarations of functions can insist on internal linkage
	 but they can't be inconsistent with internal linkage,
	 so there can be no error on that account.
	 However defining the same name twice is no good.  */
      if (DECL_INITIAL (olddecl) != NULL_TREE
	  && DECL_INITIAL (newdecl) != NULL_TREE
	  /* However, defining once as extern inline and a second
	     time in another way is ok.  */
	  && !(DECL_INLINE (olddecl) && DECL_EXTERNAL (olddecl)
	       && !(DECL_INLINE (newdecl) && DECL_EXTERNAL (newdecl))))
	{
	  if (DECL_NAME (olddecl) == NULL_TREE)
	    return "`%s' not declared in class";
	  else
	    return "redefinition of `%s'";
	}
      return 0;
    }
  else if (current_binding_level == global_binding_level)
    {
      /* Objects declared at top level:  */
      /* If at least one is a reference, it's ok.  */
      if (DECL_EXTERNAL (newdecl) || DECL_EXTERNAL (olddecl))
	return 0;
      /* Reject two definitions.  */
      if (DECL_INITIAL (olddecl) != NULL_TREE
	  && DECL_INITIAL (newdecl) != NULL_TREE)
	return "redefinition of `%s'";
      /* Now we have two tentative defs, or one tentative and one real def.  */
      /* Insist that the linkage match.  */
      if (TREE_PUBLIC (olddecl) != TREE_PUBLIC (newdecl))
	return "conflicting declarations of `%s'";
      return 0;
    }
  else
    {
      /* Objects declared with block scope:  */
      /* Reject two definitions, and reject a definition
	 together with an external reference.  */
      if (!(DECL_EXTERNAL (newdecl) && DECL_EXTERNAL (olddecl)))
	return "redeclaration of `%s'";
      return 0;
    }
}

/* Get the LABEL_DECL corresponding to identifier ID as a label.
   Create one if none exists so far for the current function.
   This function is called for both label definitions and label references.  */

tree
lookup_label (id)
     tree id;
{
  register tree decl = IDENTIFIER_LABEL_VALUE (id);

  if ((decl == NULL_TREE
      || DECL_SOURCE_LINE (decl) == 0)
      && (named_label_uses == 0
	  || TREE_PURPOSE (named_label_uses) != current_binding_level->names
	  || TREE_VALUE (named_label_uses) != decl))
    {
      named_label_uses
	= tree_cons (current_binding_level->names, decl, named_label_uses);
      TREE_TYPE (named_label_uses) = (tree)current_binding_level;
    }

  /* Use a label already defined or ref'd with this name.  */
  if (decl != NULL_TREE)
    {
      /* But not if it is inherited and wasn't declared to be inheritable.  */
      if (DECL_CONTEXT (decl) != current_function_decl
	  && ! C_DECLARED_LABEL_FLAG (decl))
	return shadow_label (id);
      return decl;
    }

  decl = build_decl (LABEL_DECL, id, void_type_node);

  /* A label not explicitly declared must be local to where it's ref'd.  */
  DECL_CONTEXT (decl) = current_function_decl;

  DECL_MODE (decl) = VOIDmode;

  /* Say where one reference is to the label,
     for the sake of the error if it is not defined.  */
  DECL_SOURCE_LINE (decl) = lineno;
  DECL_SOURCE_FILE (decl) = input_filename;

  SET_IDENTIFIER_LABEL_VALUE (id, decl);

  named_labels = tree_cons (NULL_TREE, decl, named_labels);
  TREE_VALUE (named_label_uses) = decl;

  return decl;
}

/* Make a label named NAME in the current function,
   shadowing silently any that may be inherited from containing functions
   or containing scopes.

   Note that valid use, if the label being shadowed
   comes from another scope in the same function,
   requires calling declare_nonlocal_label right away.  */

tree
shadow_label (name)
     tree name;
{
  register tree decl = IDENTIFIER_LABEL_VALUE (name);

  if (decl != NULL_TREE)
    {
      shadowed_labels = tree_cons (NULL_TREE, decl, shadowed_labels);
      SET_IDENTIFIER_LABEL_VALUE (name, 0);
      SET_IDENTIFIER_LABEL_VALUE (decl, 0);
    }

  return lookup_label (name);
}

/* Define a label, specifying the location in the source file.
   Return the LABEL_DECL node for the label, if the definition is valid.
   Otherwise return 0.  */

tree
define_label (filename, line, name)
     char *filename;
     int line;
     tree name;
{
  tree decl = lookup_label (name);

  /* After labels, make any new cleanups go into their
     own new (temporary) binding contour.  */
  current_binding_level->more_cleanups_ok = 0;

  /* If label with this name is known from an outer context, shadow it.  */
  if (decl != NULL_TREE && DECL_CONTEXT (decl) != current_function_decl)
    {
      shadowed_labels = tree_cons (NULL_TREE, decl, shadowed_labels);
      SET_IDENTIFIER_LABEL_VALUE (name, 0);
      decl = lookup_label (name);
    }

  if (DECL_INITIAL (decl) != NULL_TREE)
    {
      error_with_decl (decl, "duplicate label `%s'");
      return 0;
    }
  else
    {
      tree uses, prev;

      /* Mark label as having been defined.  */
      DECL_INITIAL (decl) = error_mark_node;
      /* Say where in the source.  */
      DECL_SOURCE_FILE (decl) = filename;
      DECL_SOURCE_LINE (decl) = line;

      for (prev = NULL_TREE, uses = named_label_uses;
	   uses;
	   prev = uses, uses = TREE_CHAIN (uses))
	if (TREE_VALUE (uses) == decl)
	  {
	    struct binding_level *b = current_binding_level;
	    while (b)
	      {
		tree new_decls = b->names;
		tree old_decls = ((tree)b == TREE_TYPE (uses)
				  ? TREE_PURPOSE (uses) : NULL_TREE);
		while (new_decls != old_decls)
		  {
		    if (TREE_CODE (new_decls) == VAR_DECL
			/* Don't complain about crossing initialization
			   of internal entities.  They can't be accessed,
			   and they should be cleaned up
			   by the time we get to the label.  */
			&& DECL_SOURCE_LINE (new_decls) != 0
			&& ((DECL_INITIAL (new_decls) != NULL_TREE
			     && DECL_INITIAL (new_decls) != error_mark_node)
			    || TYPE_NEEDS_CONSTRUCTING (TREE_TYPE (new_decls))))
		      {
			if (IDENTIFIER_ERROR_LOCUS (decl) == NULL_TREE)
			  error_with_decl (decl, "invalid jump to label `%s'");
			SET_IDENTIFIER_ERROR_LOCUS (decl, current_function_decl);
			error_with_decl (new_decls, "crosses initialization of `%s'");
		      }
		    new_decls = TREE_CHAIN (new_decls);
		  }
		if ((tree)b == TREE_TYPE (uses))
		  break;
		b = b->level_chain;
	      }

	    if (prev)
	      TREE_CHAIN (prev) = TREE_CHAIN (uses);
	    else
	      named_label_uses = TREE_CHAIN (uses);
	  }
      current_function_return_value = NULL_TREE;
      return decl;
    }
}

/* Same, but for CASE labels.  If DECL is NULL_TREE, it's the default.  */
/* XXX Note decl is never actually used. (bpk) */
void
define_case_label (decl)
     tree decl;
{
  tree cleanup = last_cleanup_this_contour ();
  if (cleanup)
    {
      static int explained = 0;
      error_with_decl (TREE_PURPOSE (cleanup), "destructor needed for `%s'");
      error ("where case label appears here");
      if (!explained)
	{
	  error ("(enclose actions of previous case statements requiring");
	  error ("destructors in their own binding contours.)");
	  explained = 1;
	}
    }

  /* After labels, make any new cleanups go into their
     own new (temporary) binding contour.  */

  current_binding_level->more_cleanups_ok = 0;
  current_function_return_value = NULL_TREE;
}

/* Return the list of declarations of the current level.
   Note that this list is in reverse order unless/until
   you nreverse it; and when you do nreverse it, you must
   store the result back using `storedecls' or you will lose.  */

tree
getdecls ()
{
  return current_binding_level->names;
}

/* Return the list of type-tags (for structs, etc) of the current level.  */

tree
gettags ()
{
  return current_binding_level->tags;
}

/* Store the list of declarations of the current level.
   This is done for the parameter declarations of a function being defined,
   after they are modified in the light of any missing parameters.  */

static void
storedecls (decls)
     tree decls;
{
  current_binding_level->names = decls;
}

/* Similarly, store the list of tags of the current level.  */

static void
storetags (tags)
     tree tags;
{
  current_binding_level->tags = tags;
}

/* Given NAME, an IDENTIFIER_NODE,
   return the structure (or union or enum) definition for that name.
   Searches binding levels from BINDING_LEVEL up to the global level.
   If THISLEVEL_ONLY is nonzero, searches only the specified context
   (but skips any tag-transparent contexts to find one that is
   meaningful for tags).
   FORM says which kind of type the caller wants;
   it is RECORD_TYPE or UNION_TYPE or ENUMERAL_TYPE.
   If the wrong kind of type is found, and it's not a template, an error is
   reported.  */

static tree
lookup_tag (form, name, binding_level, thislevel_only)
     enum tree_code form;
     struct binding_level *binding_level;
     tree name;
     int thislevel_only;
{
  register struct binding_level *level;

  for (level = binding_level; level; level = level->level_chain)
    {
      register tree tail;
      if (ANON_AGGRNAME_P (name))
	for (tail = level->tags; tail; tail = TREE_CHAIN (tail))
	  {
	    /* There's no need for error checking here, because
	       anon names are unique throughout the compilation.  */
	    if (TYPE_IDENTIFIER (TREE_VALUE (tail)) == name)
	      return TREE_VALUE (tail);
	  }
      else
	for (tail = level->tags; tail; tail = TREE_CHAIN (tail))
	  {
	    if (TREE_PURPOSE (tail) == name)
	      {
		enum tree_code code = TREE_CODE (TREE_VALUE (tail));
		/* Should tighten this up; it'll probably permit
		   UNION_TYPE and a struct template, for example.  */
		if (code != form
		    && !(form != ENUMERAL_TYPE
			 && (code == TEMPLATE_DECL
			     || code == UNINSTANTIATED_P_TYPE)))
			   
		  {
		    /* Definition isn't the kind we were looking for.  */
		    error ("`%s' defined as wrong kind of tag",
			   IDENTIFIER_POINTER (name));
		  }
		return TREE_VALUE (tail);
	      }
	  }
      if (thislevel_only && ! level->tag_transparent)
	return NULL_TREE;
      if (current_class_type && level->level_chain == global_binding_level)
	{
	  /* Try looking in this class's tags before heading into
	     global binding level.  */
	  tree context = current_class_type;
	  while (context)
	    {
	      switch (TREE_CODE_CLASS (TREE_CODE (context)))
		{
		case 't':
		  {
		    tree these_tags = CLASSTYPE_TAGS (context);
		    if (ANON_AGGRNAME_P (name))
		      while (these_tags)
			{
			  if (TYPE_IDENTIFIER (TREE_VALUE (these_tags))
			      == name)
			    return TREE_VALUE (tail);
			  these_tags = TREE_CHAIN (these_tags);
			}
		    else
		      while (these_tags)
			{
			  if (TREE_PURPOSE (these_tags) == name)
			    {
			      if (TREE_CODE (TREE_VALUE (these_tags)) != form)
				{
				  error ("`%s' defined as wrong kind of tag in class scope",
					 IDENTIFIER_POINTER (name));
				}
			      return TREE_VALUE (tail);
			    }
			  these_tags = TREE_CHAIN (these_tags);
			}
		    /* If this type is not yet complete, then don't
		       look at its context.  */
		    if (TYPE_SIZE (context) == NULL_TREE)
		      goto no_context;
		    /* Go to next enclosing type, if any.  */
		    context = DECL_CONTEXT (TYPE_NAME (context));
		    break;
		  case 'd':
		    context = DECL_CONTEXT (context);
		    break;
		  default:
		    my_friendly_abort (10);
		  }
		  continue;
		}
	    no_context:
	      break;
	    }
	}
    }
  return NULL_TREE;
}

void
set_current_level_tags_transparency (tags_transparent)
     int tags_transparent;
{
  current_binding_level->tag_transparent = tags_transparent;
}

/* Given a type, find the tag that was defined for it and return the tag name.
   Otherwise return 0.  However, the value can never be 0
   in the cases in which this is used.

   C++: If NAME is non-zero, this is the new name to install.  This is
   done when replacing anonymous tags with real tag names.  */

static tree
lookup_tag_reverse (type, name)
     tree type;
     tree name;
{
  register struct binding_level *level;

  for (level = current_binding_level; level; level = level->level_chain)
    {
      register tree tail;
      for (tail = level->tags; tail; tail = TREE_CHAIN (tail))
	{
	  if (TREE_VALUE (tail) == type)
	    {
	      if (name)
		TREE_PURPOSE (tail) = name;
	      return TREE_PURPOSE (tail);
	    }
	}
    }
  return NULL_TREE;
}

/* Given type TYPE which was not declared in C++ language context,
   attempt to find a name by which it is referred.  */
tree
typedecl_for_tag (tag)
     tree tag;
{
  struct binding_level *b = current_binding_level;

  if (TREE_CODE (TYPE_NAME (tag)) == TYPE_DECL)
    return TYPE_NAME (tag);

  while (b)
    {
      tree decls = b->names;
      while (decls)
	{
	  if (TREE_CODE (decls) == TYPE_DECL && TREE_TYPE (decls) == tag)
	    break;
	  decls = TREE_CHAIN (decls);
	}
      if (decls)
	return decls;
      b = b->level_chain;
    }
  return NULL_TREE;
}

/* Called when we must retroactively globalize a type we previously
   thought needed to be nested.  This happens, for example, when
   a `friend class' declaration is seen for an undefined type.  */

static void
globalize_nested_type (type)
     tree type;
{
  tree t, prev = NULL_TREE, d = TYPE_NAME (type);
  struct binding_level *b;

  my_friendly_assert (TREE_CODE (d) == TYPE_DECL, 144);
  /* If the type value has already been globalized, then we're set.  */
  if (IDENTIFIER_GLOBAL_VALUE (DECL_NAME (d)) == d)
    return;
  if (IDENTIFIER_HAS_TYPE_VALUE (DECL_NAME (d)))
    {
      /* If this type already made it into the global tags,
	 silently return.  */
      if (value_member (type, global_binding_level->tags))
	return;
    }

  set_identifier_type_value (DECL_NESTED_TYPENAME (d), NULL_TREE);
  DECL_NESTED_TYPENAME (d) = DECL_NAME (d);
  DECL_CONTEXT (d) = NULL_TREE;
  if (class_binding_level)
    b = class_binding_level;
  else
    b = current_binding_level;
  while (b != global_binding_level)
    {
      prev = NULL_TREE;
      if (b->parm_flag == 2)
	for (t = b->tags; t != NULL_TREE; prev = t, t = TREE_CHAIN (t))
	  if (TREE_VALUE (t) == type)
	    goto found;
      b = b->level_chain;
    }
  /* We failed to find this tag anywhere up the binding chains.
     B is now the global binding level... check there.  */
  prev = NULL_TREE;
  if (b->parm_flag == 2)
    for (t = b->tags; t != NULL_TREE; prev = t, t = TREE_CHAIN (t))
      if (TREE_VALUE (t) == type)
	goto foundglobal;
  /* It wasn't in global scope either, so this is an anonymous forward ref
     of some kind; let it happen.  */
  return;

foundglobal:
  print_node_brief (stderr, "Tried to globalize already-global type ",
		    type, 0);
  my_friendly_abort (11);

found:
  /* Pull the tag out of the nested binding contour.  */
  if (prev)
    TREE_CHAIN (prev) = TREE_CHAIN (t);
  else
    b->tags = TREE_CHAIN (t);
  
  set_identifier_type_value (TREE_PURPOSE (t), TREE_VALUE (t));
  global_binding_level->tags
    = perm_tree_cons (TREE_PURPOSE (t), TREE_VALUE (t),
		      global_binding_level->tags);

  /* Pull the tag out of the class's tags (if there).
     It won't show up if it appears e.g. in a parameter declaration
     or definition of a member function of this type.  */
  if (current_class_type != NULL_TREE)
    {
      for (t = CLASSTYPE_TAGS (current_class_type), prev = NULL_TREE;
	   t != NULL_TREE;
	   prev = t, t = TREE_CHAIN (t))
	if (TREE_VALUE (t) == type)
	  break;

      if (t != NULL_TREE)
	{
	  if (prev)
	    TREE_CHAIN (prev) = TREE_CHAIN (t);
	  else
	    CLASSTYPE_TAGS (current_class_type) = TREE_CHAIN (t);
	}
    }

  pushdecl_top_level (d);
}

static void
maybe_globalize_type (type)
     tree type;
{
  if ((((TREE_CODE (type) == RECORD_TYPE
	 || TREE_CODE (type) == UNION_TYPE)
	&& ! TYPE_BEING_DEFINED (type))
       || TREE_CODE (type) == ENUMERAL_TYPE)
      && TYPE_SIZE (type) == NULL_TREE
      /* This part is gross.  We keep calling here with types that
	 are instantiations of templates, when that type should is
	 global, or doesn't have the type decl established yet,
	 so globalizing will fail (because it won't find the type in any
	 non-global scope).  So we short-circuit that path.  */
      && !(TYPE_NAME (type) != NULL_TREE
	   && TYPE_IDENTIFIER (type) != NULL_TREE
	   && ! IDENTIFIER_HAS_TYPE_VALUE (TYPE_IDENTIFIER (type)))
      )
    globalize_nested_type (type);
}

/* Lookup TYPE in CONTEXT (a chain of nested types or a FUNCTION_DECL).
   Return the type value, or NULL_TREE if not found.  */
static tree
lookup_nested_type (type, context)
     tree type;
     tree context;
{
  if (context == NULL_TREE)
    return NULL_TREE;
  while (context)
    {
      switch (TREE_CODE (context))
	{
	case TYPE_DECL:
	  {
	    tree ctype = TREE_TYPE (context);
	    tree match = value_member (type, CLASSTYPE_TAGS (ctype));
	    if (match)
	      return TREE_VALUE (match);
	    context = DECL_CONTEXT (context);

	    /* When we have a nested class whose member functions have
	       local types (e.g., a set of enums), we'll arrive here
	       with the DECL_CONTEXT as the actual RECORD_TYPE node for
	       the enclosing class.  Instead, we want to make sure we
	       come back in here with the TYPE_DECL, not the RECORD_TYPE.  */
	    if (context && TREE_CODE (context) == RECORD_TYPE)
	      context = TREE_CHAIN (context);
	  }
	  break;
	case FUNCTION_DECL:
	  return TYPE_IDENTIFIER (type) ? lookup_name (TYPE_IDENTIFIER (type), 1) : NULL_TREE;
	  break;
	default:
	  my_friendly_abort (12);
	}
    }
  return NULL_TREE;
}

/* Look up NAME in the current binding level and its superiors in the
   namespace of variables, functions and typedefs.  Return a ..._DECL
   node of some kind representing its definition if there is only one
   such declaration, or return a TREE_LIST with all the overloaded
   definitions if there are many, or return 0 if it is undefined.

   If PREFER_TYPE is > 0, we prefer TYPE_DECLs.
   If PREFER_TYPE is = 0, we prefer non-TYPE_DECLs.
   If PREFER_TYPE is < 0, we arbitrate according to lexical context.  */

tree
lookup_name (name, prefer_type)
     tree name;
     int prefer_type;
{
  register tree val;

  if (current_binding_level != global_binding_level
      && IDENTIFIER_LOCAL_VALUE (name))
    val = IDENTIFIER_LOCAL_VALUE (name);
  /* In C++ class fields are between local and global scope,
     just before the global scope.  */
  else if (current_class_type)
    {
      val = IDENTIFIER_CLASS_VALUE (name);
      if (val == NULL_TREE
	  && TYPE_SIZE (current_class_type) == NULL_TREE
	  && CLASSTYPE_LOCAL_TYPEDECLS (current_class_type))
	{
	  /* Try to find values from base classes
	     if we are presently defining a type.
	     We are presently only interested in TYPE_DECLs.  */
	  val = lookup_field (current_class_type, name, 0, prefer_type < 0);
	  if (val == error_mark_node)
	    return val;
	  if (val && TREE_CODE (val) != TYPE_DECL)
	    val = NULL_TREE;
	}

      /* yylex() calls this with -2, since we should never start digging for
	 the nested name at the point where we haven't even, for example,
	 created the COMPONENT_REF or anything like that.  */
      if (val == NULL_TREE)
	val = lookup_nested_field (name, prefer_type != -2);

      if (val == NULL_TREE)
	val = IDENTIFIER_GLOBAL_VALUE (name);
    }
  else
    val = IDENTIFIER_GLOBAL_VALUE (name);

  if (val)
    {
      extern int looking_for_typename;

      /* Arbitrate between finding a TYPE_DECL and finding
	 other kinds of _DECLs.  */
      if (TREE_CODE (val) == TYPE_DECL || looking_for_typename < 0)
	return val;

      if (IDENTIFIER_HAS_TYPE_VALUE (name))
	{
	  register tree val_as_type = TYPE_NAME (IDENTIFIER_TYPE_VALUE (name));

	  if (val == val_as_type || prefer_type > 0
	      || looking_for_typename > 0)
	    return val_as_type;
	  if (prefer_type == 0)
	    return val;
	  return arbitrate_lookup (name, val, val_as_type);
	}
      if (TREE_TYPE (val) == error_mark_node)
	return error_mark_node;
    }

  return val;
}

/* Similar to `lookup_name' but look only at current binding level.  */

tree
lookup_name_current_level (name)
     tree name;
{
  register tree t;

  if (current_binding_level == global_binding_level)
    return IDENTIFIER_GLOBAL_VALUE (name);

  if (IDENTIFIER_LOCAL_VALUE (name) == NULL_TREE)
    return 0;

  for (t = current_binding_level->names; t; t = TREE_CHAIN (t))
    if (DECL_NAME (t) == name)
      break;

  return t;
}

/* Arrange for the user to get a source line number, even when the
   compiler is going down in flames, so that she at least has a
   chance of working around problems in the compiler.  We used to
   call error(), but that let the segmentation fault continue
   through; now, it's much more passive by asking them to send the
   maintainers mail about the problem.  */

static void
sigsegv (sig)
     int sig;
{
  signal (SIGSEGV, SIG_DFL);
#ifdef SIGIOT
  signal (SIGIOT, SIG_DFL);
#endif
#ifdef SIGILL
  signal (SIGILL, SIG_DFL);
#endif
#ifdef SIGABRT
  signal (SIGABRT, SIG_DFL);
#endif
  my_friendly_abort (0);
}

/* Array for holding types considered "built-in".  These types
   are output in the module in which `main' is defined.  */
static tree *builtin_type_tdescs_arr;
static int builtin_type_tdescs_len, builtin_type_tdescs_max;

/* Push the declarations of builtin types into the namespace.
   RID_INDEX, if < RID_MAX is the index of the builtin type
   in the array RID_POINTERS.  NAME is the name used when looking
   up the builtin type.  TYPE is the _TYPE node for the builtin type.  */

static void
record_builtin_type (rid_index, name, type)
     enum rid rid_index;
     char *name;
     tree type;
{
  tree rname = NULL_TREE, tname = NULL_TREE;
  tree tdecl;

  if ((int) rid_index < (int) RID_MAX)
    rname = ridpointers[(int) rid_index];
  if (name)
    tname = get_identifier (name);

  if (tname)
    {
#if 0 /* not yet, should get fixed properly later */
      tdecl = pushdecl (make_type_decl (tname, type));
#else
      tdecl = pushdecl (build_decl (TYPE_DECL, tname, type));
#endif
      set_identifier_type_value (tname, NULL_TREE);
      if ((int) rid_index < (int) RID_MAX)
	IDENTIFIER_GLOBAL_VALUE (tname) = tdecl;
    }
  if (rname != NULL_TREE)
    {
      if (tname != NULL_TREE)
	{
	  set_identifier_type_value (rname, NULL_TREE);
	  IDENTIFIER_GLOBAL_VALUE (rname) = tdecl;
	}
      else
	{
#if 0 /* not yet, should get fixed properly later */
	  tdecl = pushdecl (make_type_decl (rname, type));
#else
	  tdecl = pushdecl (build_decl (TYPE_DECL, rname, type));
#endif
	  set_identifier_type_value (rname, NULL_TREE);
	}
    }

  if (flag_dossier)
    {
      if (builtin_type_tdescs_len+5 >= builtin_type_tdescs_max)
	{
	  builtin_type_tdescs_max *= 2;
	  builtin_type_tdescs_arr
	    = (tree *)xrealloc (builtin_type_tdescs_arr,
				builtin_type_tdescs_max * sizeof (tree));
	}
      builtin_type_tdescs_arr[builtin_type_tdescs_len++] = type;
      if (TREE_CODE (type) != POINTER_TYPE)
	{
	  builtin_type_tdescs_arr[builtin_type_tdescs_len++]
	    = build_pointer_type (type);
	  builtin_type_tdescs_arr[builtin_type_tdescs_len++]
	    = build_type_variant (TYPE_POINTER_TO (type), 1, 0);
	}
      if (TREE_CODE (type) != VOID_TYPE)
	{
	  builtin_type_tdescs_arr[builtin_type_tdescs_len++]
	    = build_reference_type (type);
	  builtin_type_tdescs_arr[builtin_type_tdescs_len++]
	    = build_type_variant (TYPE_REFERENCE_TO (type), 1, 0);
	}
    }
}

static void
output_builtin_tdesc_entries ()
{
  extern struct obstack permanent_obstack;

  /* If there's more than one main in this file, don't crash.  */
  if (builtin_type_tdescs_arr == 0)
    return;

  push_obstacks (&permanent_obstack, &permanent_obstack);
  while (builtin_type_tdescs_len > 0)
    {
      tree type = builtin_type_tdescs_arr[--builtin_type_tdescs_len];
      tree tdesc = build_t_desc (type, 0);
      TREE_ASM_WRITTEN (tdesc) = 0;
      build_t_desc (type, 2);
    }
  free (builtin_type_tdescs_arr);
  builtin_type_tdescs_arr = 0;
  pop_obstacks ();
}

/* Push overloaded decl, in global scope, with one argument so it
   can be used as a callback from define_function.  */
static void
push_overloaded_decl_1 (x)
     tree x;
{
  push_overloaded_decl (x, 0);
}

/* Create the predefined scalar types of C,
   and some nodes representing standard constants (0, 1, (void *)0).
   Initialize the global binding level.
   Make definitions for built-in primitive functions.  */

void
init_decl_processing ()
{
  tree decl;
  register tree endlink, int_endlink, double_endlink, ptr_endlink;
  tree fields[20];
  /* Either char* or void*.  */
  tree traditional_ptr_type_node;
  /* Data type of memcpy.  */
  tree memcpy_ftype;
  int wchar_type_size;

  /* Have to make these distinct before we try using them.  */
  lang_name_cplusplus = get_identifier ("C++");
  lang_name_c = get_identifier ("C");

  /* Initially, C.  */
  current_lang_name = lang_name_c;

  current_function_decl = NULL_TREE;
  named_labels = NULL_TREE;
  named_label_uses = NULL_TREE;
  current_binding_level = NULL_BINDING_LEVEL;
  free_binding_level = NULL_BINDING_LEVEL;

  /* Because most segmentation signals can be traced back into user
     code, catch them and at least give the user a chance of working
     around compiler bugs. */
  signal (SIGSEGV, sigsegv);

  /* We will also catch aborts in the back-end through sigsegv and give the
     user a chance to see where the error might be, and to defeat aborts in
     the back-end when there have been errors previously in their code. */
#ifdef SIGIOT
  signal (SIGIOT, sigsegv);
#endif
#ifdef SIGILL
  signal (SIGILL, sigsegv);
#endif
#ifdef SIGABRT
  signal (SIGABRT, sigsegv);
#endif

  gcc_obstack_init (&decl_obstack);
  if (flag_dossier)
    {
      builtin_type_tdescs_max = 100;
      builtin_type_tdescs_arr = (tree *)xmalloc (100 * sizeof (tree));
    }

  /* Must lay these out before anything else gets laid out.  */
  error_mark_node = make_node (ERROR_MARK);
  TREE_PERMANENT (error_mark_node) = 1;
  TREE_TYPE (error_mark_node) = error_mark_node;
  error_mark_list = build_tree_list (error_mark_node, error_mark_node);
  TREE_TYPE (error_mark_list) = error_mark_node;

  pushlevel (0);	/* make the binding_level structure for global names.  */
  global_binding_level = current_binding_level;

  this_identifier = get_identifier (THIS_NAME);
  in_charge_identifier = get_identifier (IN_CHARGE_NAME);

  /* Define `int' and `char' first so that dbx will output them first.  */

  integer_type_node = make_signed_type (INT_TYPE_SIZE);
  record_builtin_type (RID_INT, NULL_PTR, integer_type_node);

  /* Define `char', which is like either `signed char' or `unsigned char'
     but not the same as either.  */

  char_type_node =
    (flag_signed_char
     ? make_signed_type (CHAR_TYPE_SIZE)
     : make_unsigned_type (CHAR_TYPE_SIZE));
  record_builtin_type (RID_CHAR, "char", char_type_node);

  long_integer_type_node = make_signed_type (LONG_TYPE_SIZE);
  record_builtin_type (RID_LONG, "long int", long_integer_type_node);

  unsigned_type_node = make_unsigned_type (INT_TYPE_SIZE);
  record_builtin_type (RID_UNSIGNED, "unsigned int", unsigned_type_node);

  long_unsigned_type_node = make_unsigned_type (LONG_TYPE_SIZE);
  record_builtin_type (RID_MAX, "long unsigned int", long_unsigned_type_node);
  record_builtin_type (RID_MAX, "unsigned long", long_unsigned_type_node);

  /* `unsigned long' is the standard type for sizeof.
     Traditionally, use a signed type.
     Note that stddef.h uses `unsigned long',
     and this must agree, even of long and int are the same size.  */
  if (flag_traditional)
    sizetype = long_integer_type_node;
  else
    sizetype
      = TREE_TYPE (IDENTIFIER_GLOBAL_VALUE (get_identifier (SIZE_TYPE)));

  ptrdiff_type_node
    = TREE_TYPE (IDENTIFIER_GLOBAL_VALUE (get_identifier (PTRDIFF_TYPE)));

  TREE_TYPE (TYPE_SIZE (integer_type_node)) = sizetype;
  TREE_TYPE (TYPE_SIZE (char_type_node)) = sizetype;
  TREE_TYPE (TYPE_SIZE (unsigned_type_node)) = sizetype;
  TREE_TYPE (TYPE_SIZE (long_unsigned_type_node)) = sizetype;
  TREE_TYPE (TYPE_SIZE (long_integer_type_node)) = sizetype;

  short_integer_type_node = make_signed_type (SHORT_TYPE_SIZE);
  record_builtin_type (RID_SHORT, "short int", short_integer_type_node);
  long_long_integer_type_node = make_signed_type (LONG_LONG_TYPE_SIZE);
  record_builtin_type (RID_MAX, "long long int", long_long_integer_type_node);
  short_unsigned_type_node = make_unsigned_type (SHORT_TYPE_SIZE);
  record_builtin_type (RID_MAX, "short unsigned int", short_unsigned_type_node);
  record_builtin_type (RID_MAX, "unsigned short", short_unsigned_type_node);
  long_long_unsigned_type_node = make_unsigned_type (LONG_LONG_TYPE_SIZE);
  record_builtin_type (RID_MAX, "long long unsigned int", long_long_unsigned_type_node);
  record_builtin_type (RID_MAX, "long long unsigned", long_long_unsigned_type_node);

  /* Define both `signed char' and `unsigned char'.  */
  signed_char_type_node = make_signed_type (CHAR_TYPE_SIZE);
  record_builtin_type (RID_MAX, "signed char", signed_char_type_node);
  unsigned_char_type_node = make_unsigned_type (CHAR_TYPE_SIZE);
  record_builtin_type (RID_MAX, "unsigned char", unsigned_char_type_node);

  /* These are types that type_for_size and type_for_mode use.  */
  intQI_type_node = make_signed_type (GET_MODE_BITSIZE (QImode));
  pushdecl (build_decl (TYPE_DECL, NULL_TREE, intQI_type_node));
  intHI_type_node = make_signed_type (GET_MODE_BITSIZE (HImode));
  pushdecl (build_decl (TYPE_DECL, NULL_TREE, intHI_type_node));
  intSI_type_node = make_signed_type (GET_MODE_BITSIZE (SImode));
  pushdecl (build_decl (TYPE_DECL, NULL_TREE, intSI_type_node));
  intDI_type_node = make_signed_type (GET_MODE_BITSIZE (DImode));
  pushdecl (build_decl (TYPE_DECL, NULL_TREE, intDI_type_node));
  unsigned_intQI_type_node = make_unsigned_type (GET_MODE_BITSIZE (QImode));
  pushdecl (build_decl (TYPE_DECL, NULL_TREE, unsigned_intQI_type_node));
  unsigned_intHI_type_node = make_unsigned_type (GET_MODE_BITSIZE (HImode));
  pushdecl (build_decl (TYPE_DECL, NULL_TREE, unsigned_intHI_type_node));
  unsigned_intSI_type_node = make_unsigned_type (GET_MODE_BITSIZE (SImode));
  pushdecl (build_decl (TYPE_DECL, NULL_TREE, unsigned_intSI_type_node));
  unsigned_intDI_type_node = make_unsigned_type (GET_MODE_BITSIZE (DImode));
  pushdecl (build_decl (TYPE_DECL, NULL_TREE, unsigned_intDI_type_node));

  float_type_node = make_node (REAL_TYPE);
  TYPE_PRECISION (float_type_node) = FLOAT_TYPE_SIZE;
  record_builtin_type (RID_FLOAT, NULL, float_type_node);
  layout_type (float_type_node);

  double_type_node = make_node (REAL_TYPE);
  if (flag_short_double)
    TYPE_PRECISION (double_type_node) = FLOAT_TYPE_SIZE;
  else
    TYPE_PRECISION (double_type_node) = DOUBLE_TYPE_SIZE;
  record_builtin_type (RID_DOUBLE, NULL, double_type_node);
  layout_type (double_type_node);

  long_double_type_node = make_node (REAL_TYPE);
  TYPE_PRECISION (long_double_type_node) = LONG_DOUBLE_TYPE_SIZE;
  record_builtin_type (RID_MAX, "long double", long_double_type_node);
  layout_type (long_double_type_node);

  integer_zero_node = build_int_2 (0, 0);
  TREE_TYPE (integer_zero_node) = integer_type_node;
  integer_one_node = build_int_2 (1, 0);
  TREE_TYPE (integer_one_node) = integer_type_node;
  integer_two_node = build_int_2 (2, 0);
  TREE_TYPE (integer_two_node) = integer_type_node;
  integer_three_node = build_int_2 (3, 0);
  TREE_TYPE (integer_three_node) = integer_type_node;
  empty_init_node = build_nt (CONSTRUCTOR, NULL_TREE, NULL_TREE);

  /* These are needed by stor-layout.c.  */
  size_zero_node = size_int (0);
  size_one_node = size_int (1);

  void_type_node = make_node (VOID_TYPE);
  record_builtin_type (RID_VOID, NULL, void_type_node);
  layout_type (void_type_node); /* Uses integer_zero_node.  */
  void_list_node = build_tree_list (NULL_TREE, void_type_node);
  TREE_PARMLIST (void_list_node) = 1;

  null_pointer_node = build_int_2 (0, 0);
  TREE_TYPE (null_pointer_node) = build_pointer_type (void_type_node);
  layout_type (TREE_TYPE (null_pointer_node));

  /* Used for expressions that do nothing, but are not errors.  */
  void_zero_node = build_int_2 (0, 0);
  TREE_TYPE (void_zero_node) = void_type_node;

  string_type_node = build_pointer_type (char_type_node);
  const_string_type_node = build_pointer_type (build_type_variant (char_type_node, 1, 0));
  record_builtin_type (RID_MAX, NULL, string_type_node);

  /* make a type for arrays of 256 characters.
     256 is picked randomly because we have a type for integers from 0 to 255.
     With luck nothing will ever really depend on the length of this
     array type.  */
  char_array_type_node
    = build_array_type (char_type_node, unsigned_char_type_node);
  /* Likewise for arrays of ints.  */
  int_array_type_node
    = build_array_type (integer_type_node, unsigned_char_type_node);

  /* This is just some anonymous class type.  Nobody should ever
     need to look inside this envelope.  */
  class_star_type_node = build_pointer_type (make_lang_type (RECORD_TYPE));

  default_function_type
    = build_function_type (integer_type_node, NULL_TREE);
  build_pointer_type (default_function_type);

  ptr_type_node = build_pointer_type (void_type_node);
  const_ptr_type_node = build_pointer_type (build_type_variant (void_type_node, 1, 0));
  record_builtin_type (RID_MAX, NULL, ptr_type_node);
  endlink = void_list_node;
  int_endlink = tree_cons (NULL_TREE, integer_type_node, endlink);
  double_endlink = tree_cons (NULL_TREE, double_type_node, endlink);
  ptr_endlink = tree_cons (NULL_TREE, ptr_type_node, endlink);

  double_ftype_double
    = build_function_type (double_type_node, double_endlink);

  double_ftype_double_double
    = build_function_type (double_type_node,
			   tree_cons (NULL_TREE, double_type_node, double_endlink));

  int_ftype_int
    = build_function_type (integer_type_node, int_endlink);

  long_ftype_long
    = build_function_type (long_integer_type_node,
			   tree_cons (NULL_TREE, long_integer_type_node, endlink));

  void_ftype_ptr_ptr_int
    = build_function_type (void_type_node,
			   tree_cons (NULL_TREE, ptr_type_node,
				      tree_cons (NULL_TREE, ptr_type_node,
						 int_endlink)));

  int_ftype_cptr_cptr_sizet
    = build_function_type (integer_type_node,
			   tree_cons (NULL_TREE, const_ptr_type_node,
				      tree_cons (NULL_TREE, const_ptr_type_node,
						 tree_cons (NULL_TREE,
							    sizetype,
							    endlink))));

  void_ftype_ptr_int_int
    = build_function_type (void_type_node,
			   tree_cons (NULL_TREE, ptr_type_node,
				      tree_cons (NULL_TREE, integer_type_node,
						 int_endlink)));

  string_ftype_ptr_ptr		/* strcpy prototype */
    = build_function_type (string_type_node,
			   tree_cons (NULL_TREE, string_type_node,
				      tree_cons (NULL_TREE,
						 const_string_type_node,
						 endlink)));

  int_ftype_string_string	/* strcmp prototype */
    = build_function_type (integer_type_node,
			   tree_cons (NULL_TREE, const_string_type_node,
				      tree_cons (NULL_TREE,
						 const_string_type_node,
						 endlink)));

  sizet_ftype_string		/* strlen prototype */
    = build_function_type (sizetype,
			   tree_cons (NULL_TREE, const_string_type_node,
				      endlink));

  traditional_ptr_type_node
    = (flag_traditional ? string_type_node : ptr_type_node);

  memcpy_ftype	/* memcpy prototype */
    = build_function_type (traditional_ptr_type_node,
			   tree_cons (NULL_TREE, ptr_type_node,
				      tree_cons (NULL_TREE, const_ptr_type_node,
						 tree_cons (NULL_TREE,
							    sizetype,
							    endlink))));

#ifdef VTABLE_USES_MASK
  /* This is primarily for virtual function definition.  We
     declare an array of `void *', which can later be
     converted to the appropriate function pointer type.
     To do pointers to members, we need a mask which can
     distinguish an index value into a virtual function table
     from an address.  */
  vtbl_mask = build_int_2 (~((HOST_WIDE_INT) VINDEX_MAX - 1), -1);
#endif

  vtbl_type_node
    = build_array_type (ptr_type_node, NULL_TREE);
  layout_type (vtbl_type_node);
  vtbl_type_node = build_type_variant (vtbl_type_node, 1, 0);
  record_builtin_type (RID_MAX, NULL, vtbl_type_node);

  builtin_function ("__builtin_constant_p", int_ftype_int,
		    BUILT_IN_CONSTANT_P, NULL_PTR);

  builtin_function ("__builtin_alloca",
		    build_function_type (ptr_type_node,
					 tree_cons (NULL_TREE,
						    sizetype,
						    endlink)),
		    BUILT_IN_ALLOCA, "alloca");
#if 0
  builtin_function ("alloca",
		    build_function_type (ptr_type_node,
					 tree_cons (NULL_TREE,
						    sizetype,
						    endlink)),
		    BUILT_IN_ALLOCA, NULL_PTR);
#endif

  builtin_function ("__builtin_abs", int_ftype_int,
		    BUILT_IN_ABS, NULL_PTR);
  builtin_function ("__builtin_fabs", double_ftype_double,
		    BUILT_IN_FABS, NULL_PTR);
  builtin_function ("__builtin_labs", long_ftype_long,
		    BUILT_IN_LABS, NULL_PTR);
  builtin_function ("__builtin_ffs", int_ftype_int,
		    BUILT_IN_FFS, NULL_PTR);
  builtin_function ("__builtin_fsqrt", double_ftype_double,
		    BUILT_IN_FSQRT, NULL_PTR);
  builtin_function ("__builtin_sin", double_ftype_double, 
		    BUILT_IN_SIN, "sin");
  builtin_function ("__builtin_cos", double_ftype_double, 
		    BUILT_IN_COS, "cos");
  builtin_function ("__builtin_saveregs",
		    build_function_type (ptr_type_node, NULL_TREE),
		    BUILT_IN_SAVEREGS, NULL_PTR);
/* EXPAND_BUILTIN_VARARGS is obsolete.  */
#if 0
  builtin_function ("__builtin_varargs",
		    build_function_type (ptr_type_node,
					 tree_cons (NULL_TREE,
						    integer_type_node,
						    endlink)),
		    BUILT_IN_VARARGS, NULL_PTR);
#endif
  builtin_function ("__builtin_classify_type", default_function_type,
		    BUILT_IN_CLASSIFY_TYPE, NULL_PTR);
  builtin_function ("__builtin_next_arg",
		    build_function_type (ptr_type_node, endlink),
		    BUILT_IN_NEXT_ARG, NULL_PTR);
  builtin_function ("__builtin_args_info",
		    build_function_type (integer_type_node,
					 tree_cons (NULL_TREE,
						    integer_type_node,
						    endlink)),
		    BUILT_IN_ARGS_INFO, NULL_PTR);

  /* Currently under experimentation.  */
  builtin_function ("__builtin_memcpy", memcpy_ftype,
		    BUILT_IN_MEMCPY, "memcpy");
  builtin_function ("__builtin_memcmp", int_ftype_cptr_cptr_sizet,
		    BUILT_IN_MEMCMP, "memcmp");
  builtin_function ("__builtin_strcmp", int_ftype_string_string,
		    BUILT_IN_STRCMP, "strcmp");
  builtin_function ("__builtin_strcpy", string_ftype_ptr_ptr,
		    BUILT_IN_STRCPY, "strcpy");
  builtin_function ("__builtin_strlen", sizet_ftype_string,
		    BUILT_IN_STRLEN, "strlen");

  if (!flag_no_builtin)
    {
#if 0 /* These do not work well with libg++.  */
      builtin_function ("abs", int_ftype_int, BUILT_IN_ABS, NULL_PTR);
      builtin_function ("fabs", double_ftype_double, BUILT_IN_FABS, NULL_PTR);
      builtin_function ("labs", long_ftype_long, BUILT_IN_LABS, NULL_PTR);
#endif
      builtin_function ("memcpy", memcpy_ftype, BUILT_IN_MEMCPY, NULL_PTR);
      builtin_function ("memcmp", int_ftype_cptr_cptr_sizet, BUILT_IN_MEMCMP,
			NULL_PTR);
      builtin_function ("strcmp", int_ftype_string_string, BUILT_IN_STRCMP, NULL_PTR);
      builtin_function ("strcpy", string_ftype_ptr_ptr, BUILT_IN_STRCPY, NULL_PTR);
      builtin_function ("strlen", sizet_ftype_string, BUILT_IN_STRLEN, NULL_PTR);
      builtin_function ("sin", double_ftype_double, BUILT_IN_SIN, NULL_PTR);
      builtin_function ("cos", double_ftype_double, BUILT_IN_COS, NULL_PTR);
    }

#if 0
  /* Support for these has not been written in either expand_builtin
     or build_function_call.  */
  builtin_function ("__builtin_div", default_ftype, BUILT_IN_DIV, 0);
  builtin_function ("__builtin_ldiv", default_ftype, BUILT_IN_LDIV, 0);
  builtin_function ("__builtin_ffloor", double_ftype_double, BUILT_IN_FFLOOR, 0);
  builtin_function ("__builtin_fceil", double_ftype_double, BUILT_IN_FCEIL, 0);
  builtin_function ("__builtin_fmod", double_ftype_double_double, BUILT_IN_FMOD, 0);
  builtin_function ("__builtin_frem", double_ftype_double_double, BUILT_IN_FREM, 0);
  builtin_function ("__builtin_memset", ptr_ftype_ptr_int_int, BUILT_IN_MEMSET, 0);
  builtin_function ("__builtin_getexp", double_ftype_double, BUILT_IN_GETEXP, 0);
  builtin_function ("__builtin_getman", double_ftype_double, BUILT_IN_GETMAN, 0);
#endif

  /* C++ extensions */

  unknown_type_node = make_node (UNKNOWN_TYPE);
#if 0 /* not yet, should get fixed properly later */
  pushdecl (make_type_decl (get_identifier ("unknown type"),
		       unknown_type_node));
#else
  decl = pushdecl (build_decl (TYPE_DECL, get_identifier ("unknown type"),
			unknown_type_node));
  /* Make sure the "unknown type" typedecl gets ignored for debug info.  */
  DECL_IGNORED_P (decl) = 1;
#endif
  TYPE_SIZE (unknown_type_node) = TYPE_SIZE (void_type_node);
  TYPE_ALIGN (unknown_type_node) = 1;
  TYPE_MODE (unknown_type_node) = TYPE_MODE (void_type_node);
  /* Indirecting an UNKNOWN_TYPE node yields an UNKNOWN_TYPE node.  */
  TREE_TYPE (unknown_type_node) = unknown_type_node;
  /* Looking up TYPE_POINTER_TO and TYPE_REFERENCE_TO yield the same result.  */
  TYPE_POINTER_TO (unknown_type_node) = unknown_type_node;
  TYPE_REFERENCE_TO (unknown_type_node) = unknown_type_node;

  /* This is special for C++ so functions can be overloaded. */
  wchar_type_node
    = TREE_TYPE (IDENTIFIER_GLOBAL_VALUE (get_identifier (WCHAR_TYPE)));
  wchar_type_size = TYPE_PRECISION (wchar_type_node);
  signed_wchar_type_node = make_signed_type (wchar_type_size);
  unsigned_wchar_type_node = make_unsigned_type (wchar_type_size);
  wchar_type_node
    = TREE_UNSIGNED (wchar_type_node)
      ? unsigned_wchar_type_node
      : signed_wchar_type_node;
  record_builtin_type (RID_WCHAR, "__wchar_t", wchar_type_node);

  /* This is for wide string constants.  */
  wchar_array_type_node
    = build_array_type (wchar_type_node, unsigned_char_type_node);

  /* This is a hack that should go away when we deliver the
     real gc code.  */
  if (flag_gc)
    {
      builtin_function ("__gc_main", default_function_type, NOT_BUILT_IN, 0);
      pushdecl (lookup_name (get_identifier ("__gc_main"), 0));
    }

  /* Simplify life by making a "vtable_entry_type".  Give its
     fields names so that the debugger can use them.  */

  vtable_entry_type = make_lang_type (RECORD_TYPE);
  fields[0] = build_lang_field_decl (FIELD_DECL, get_identifier (VTABLE_DELTA_NAME), short_integer_type_node);
  fields[1] = build_lang_field_decl (FIELD_DECL, get_identifier (VTABLE_INDEX_NAME), short_integer_type_node);
  fields[2] = build_lang_field_decl (FIELD_DECL, get_identifier (VTABLE_PFN_NAME), ptr_type_node);
  finish_builtin_type (vtable_entry_type, VTBL_PTR_TYPE, fields, 2,
		       double_type_node);

  /* Make this part of an invisible union.  */
  fields[3] = copy_node (fields[2]);
  TREE_TYPE (fields[3]) = short_integer_type_node;
  DECL_NAME (fields[3]) = get_identifier (VTABLE_DELTA2_NAME);
  DECL_MODE (fields[3]) = TYPE_MODE (short_integer_type_node);
  DECL_SIZE (fields[3]) = TYPE_SIZE (short_integer_type_node);
  TREE_UNSIGNED (fields[3]) = 0;
  TREE_CHAIN (fields[2]) = fields[3];
  vtable_entry_type = build_type_variant (vtable_entry_type, 1, 0);
  record_builtin_type (RID_MAX, VTBL_PTR_TYPE, vtable_entry_type);

  if (flag_dossier)
    {
      /* Must build __t_desc type.  Currently, type descriptors look like this:

	 struct __t_desc
	 {
           const char *name;
	   int size;
	   int bits;
	   struct __t_desc *points_to;
	   int ivars_count, meths_count;
	   struct __i_desc *ivars[];
	   struct __m_desc *meths[];
	   struct __t_desc *parents[];
	   struct __t_desc *vbases[];
	   int offsets[];
	 };

	 ...as per Linton's paper.  */

      __t_desc_type_node = make_lang_type (RECORD_TYPE);
      __i_desc_type_node = make_lang_type (RECORD_TYPE);
      __m_desc_type_node = make_lang_type (RECORD_TYPE);
      __t_desc_array_type = build_array_type (TYPE_POINTER_TO (__t_desc_type_node), NULL_TREE);
      __i_desc_array_type = build_array_type (TYPE_POINTER_TO (__i_desc_type_node), NULL_TREE);
      __m_desc_array_type = build_array_type (TYPE_POINTER_TO (__m_desc_type_node), NULL_TREE);

      fields[0] = build_lang_field_decl (FIELD_DECL, get_identifier ("name"),
					 string_type_node);
      fields[1] = build_lang_field_decl (FIELD_DECL, get_identifier ("size"),
					 unsigned_type_node);
      fields[2] = build_lang_field_decl (FIELD_DECL, get_identifier ("bits"),
					 unsigned_type_node);
      fields[3] = build_lang_field_decl (FIELD_DECL, get_identifier ("points_to"),
					 TYPE_POINTER_TO (__t_desc_type_node));
      fields[4] = build_lang_field_decl (FIELD_DECL,
					 get_identifier ("ivars_count"),
					 integer_type_node);
      fields[5] = build_lang_field_decl (FIELD_DECL,
					 get_identifier ("meths_count"),
					 integer_type_node);
      fields[6] = build_lang_field_decl (FIELD_DECL, get_identifier ("ivars"),
					 build_pointer_type (__i_desc_array_type));
      fields[7] = build_lang_field_decl (FIELD_DECL, get_identifier ("meths"),
					 build_pointer_type (__m_desc_array_type));
      fields[8] = build_lang_field_decl (FIELD_DECL, get_identifier ("parents"),
					 build_pointer_type (__t_desc_array_type));
      fields[9] = build_lang_field_decl (FIELD_DECL, get_identifier ("vbases"),
					 build_pointer_type (__t_desc_array_type));
      fields[10] = build_lang_field_decl (FIELD_DECL, get_identifier ("offsets"),
					 build_pointer_type (integer_type_node));
      finish_builtin_type (__t_desc_type_node, "__t_desc", fields, 10, integer_type_node);

      /* ivar descriptors look like this:

	 struct __i_desc
	 {
	   const char *name;
	   int offset;
	   struct __t_desc *type;
	 };
      */

      fields[0] = build_lang_field_decl (FIELD_DECL, get_identifier ("name"),
					 string_type_node);
      fields[1] = build_lang_field_decl (FIELD_DECL, get_identifier ("offset"),
					 integer_type_node);
      fields[2] = build_lang_field_decl (FIELD_DECL, get_identifier ("type"),
					 TYPE_POINTER_TO (__t_desc_type_node));
      finish_builtin_type (__i_desc_type_node, "__i_desc", fields, 2, integer_type_node);

      /* method descriptors look like this:

	 struct __m_desc
	 {
	   const char *name;
	   int vindex;
	   struct __t_desc *vcontext;
	   struct __t_desc *return_type;
	   void (*address)();
	   short parm_count;
	   short required_parms;
	   struct __t_desc *parm_types[];
	 };
      */

      fields[0] = build_lang_field_decl (FIELD_DECL, get_identifier ("name"),
					 string_type_node);
      fields[1] = build_lang_field_decl (FIELD_DECL, get_identifier ("vindex"),
					 integer_type_node);
      fields[2] = build_lang_field_decl (FIELD_DECL, get_identifier ("vcontext"),
					 TYPE_POINTER_TO (__t_desc_type_node));
      fields[3] = build_lang_field_decl (FIELD_DECL, get_identifier ("return_type"),
					 TYPE_POINTER_TO (__t_desc_type_node));
      fields[4] = build_lang_field_decl (FIELD_DECL, get_identifier ("address"),
					 build_pointer_type (default_function_type));
      fields[5] = build_lang_field_decl (FIELD_DECL, get_identifier ("parm_count"),
					 short_integer_type_node);
      fields[6] = build_lang_field_decl (FIELD_DECL, get_identifier ("required_parms"),
					 short_integer_type_node);
      fields[7] = build_lang_field_decl (FIELD_DECL, get_identifier ("parm_types"),
					 build_pointer_type (build_array_type (TYPE_POINTER_TO (__t_desc_type_node), NULL_TREE)));
      finish_builtin_type (__m_desc_type_node, "__m_desc", fields, 7, integer_type_node);
    }

  /* Now, C++.  */
  current_lang_name = lang_name_cplusplus;
  if (flag_dossier)
    {
      int i = builtin_type_tdescs_len;
      while (i > 0)
	{
	  tree tdesc = build_t_desc (builtin_type_tdescs_arr[--i], 0);
	  TREE_ASM_WRITTEN (tdesc) = 1;
	  TREE_PUBLIC (TREE_OPERAND (tdesc, 0)) = 1;
	}
    }

  auto_function (ansi_opname[(int) NEW_EXPR],
		 build_function_type (ptr_type_node,
				      tree_cons (NULL_TREE, sizetype,
						 void_list_node)),
		 NOT_BUILT_IN);
  auto_function (ansi_opname[(int) DELETE_EXPR],
		 build_function_type (void_type_node,
				      tree_cons (NULL_TREE, ptr_type_node,
						 void_list_node)),
		 NOT_BUILT_IN);

  abort_fndecl
    = define_function ("abort",
		       build_function_type (void_type_node, void_list_node),
		       NOT_BUILT_IN, 0, 0);

  unhandled_exception_fndecl
    = define_function ("__unhandled_exception",
		       build_function_type (void_type_node, NULL_TREE),
		       NOT_BUILT_IN, 0, 0);

  /* Perform other language dependent initializations.  */
  init_class_processing ();
  init_init_processing ();
  init_search_processing ();

  if (flag_handle_exceptions)
    {
      if (flag_handle_exceptions == 2)
	/* Too much trouble to inline all the trys needed for this.  */
	flag_this_is_variable = 2;
      init_exception_processing ();
    }
  if (flag_gc)
    init_gc_processing ();
  if (flag_no_inline)
    flag_inline_functions = 0, flag_default_inline = 0;
  if (flag_cadillac)
    init_cadillac ();

  /* Create the global bindings for __FUNCTION__ and __PRETTY_FUNCTION__.  */
  declare_function_name ();

  /* Warnings about failure to return values are too valuable to forego.  */
  warn_return_type = 1;
}

/* Make a definition for a builtin function named NAME and whose data type
   is TYPE.  TYPE should be a function type with argument types.
   FUNCTION_CODE tells later passes how to compile calls to this function.
   See tree.h for its possible values.

   If LIBRARY_NAME is nonzero, use that for DECL_ASSEMBLER_NAME,
   the name to be called if we can't opencode the function.  */

tree
define_function (name, type, function_code, pfn, library_name)
     char *name;
     tree type;
     enum built_in_function function_code;
     void (*pfn)();
     char *library_name;
{
  tree decl = build_lang_decl (FUNCTION_DECL, get_identifier (name), type);
  DECL_EXTERNAL (decl) = 1;
  TREE_PUBLIC (decl) = 1;

  /* Since `pushdecl' relies on DECL_ASSEMBLER_NAME instead of DECL_NAME,
     we cannot change DECL_ASSEMBLER_NAME until we have installed this
     function in the namespace.  */
  if (pfn) (*pfn) (decl);
  if (library_name)
    DECL_ASSEMBLER_NAME (decl) = get_identifier (library_name);
  make_function_rtl (decl);
  if (function_code != NOT_BUILT_IN)
    {
      DECL_BUILT_IN (decl) = 1;
      DECL_SET_FUNCTION_CODE (decl, function_code);
    }
  return decl;
}

/* Called when a declaration is seen that contains no names to declare.
   If its type is a reference to a structure, union or enum inherited
   from a containing scope, shadow that tag name for the current scope
   with a forward reference.
   If its type defines a new named structure or union
   or defines an enum, it is valid but we need not do anything here.
   Otherwise, it is an error.

   C++: may have to grok the declspecs to learn about static,
   complain for anonymous unions.  */

void
shadow_tag (declspecs)
     tree declspecs;
{
  int found_tag = 0;
  int warned = 0;
  register tree link;
  register enum tree_code code, ok_code = ERROR_MARK;
  register tree t = NULL_TREE;

  for (link = declspecs; link; link = TREE_CHAIN (link))
    {
      register tree value = TREE_VALUE (link);

      code = TREE_CODE (value);
      if (IS_AGGR_TYPE_CODE (code) || code == ENUMERAL_TYPE)
	/* Used to test also that TYPE_SIZE (value) != 0.
	   That caused warning for `struct foo;' at top level in the file.  */
	{
	  register tree name = TYPE_NAME (value);

	  if (name == NULL_TREE)
	    name = lookup_tag_reverse (value, NULL_TREE);

	  if (name && TREE_CODE (name) == TYPE_DECL)
	    name = DECL_NAME (name);

	  if (class_binding_level)
	    t = lookup_tag (code, name, class_binding_level, 1);
	  else
	    t = lookup_tag (code, name, current_binding_level, 1);

	  if (t == NULL_TREE)
	    {
	      push_obstacks (&permanent_obstack, &permanent_obstack);
	      if (IS_AGGR_TYPE_CODE (code))
		t = make_lang_type (code);
	      else
		t = make_node (code);
	      pushtag (name, t);
	      pop_obstacks ();
	      ok_code = code;
	      break;
	    }
	  else if (name != NULL_TREE || code == ENUMERAL_TYPE)
	    ok_code = code;

	  if (ok_code != ERROR_MARK)
	    found_tag++;
	  else
	    {
	      if (!warned)
		pedwarn ("useless keyword or type name in declaration");
	      warned = 1;
	    }
	}
    }

  /* This is where the variables in an anonymous union are
     declared.  An anonymous union declaration looks like:
     union { ... } ;
     because there is no declarator after the union, the parser
     sends that declaration here.  */
  if (ok_code == UNION_TYPE
      && t != NULL_TREE
      && ((TREE_CODE (TYPE_NAME (t)) == IDENTIFIER_NODE
	   && ANON_AGGRNAME_P (TYPE_NAME (t)))
	  || (TREE_CODE (TYPE_NAME (t)) == TYPE_DECL
	      && ANON_AGGRNAME_P (TYPE_IDENTIFIER (t)))))
    {
      /* ANSI C++ June 5 1992 WP 9.5.3.  Anonymous unions may not have
	 function members.  */
      if (TYPE_FIELDS (t))
	{
	  tree decl = grokdeclarator (NULL_TREE, declspecs, NORMAL, 0, NULL_TREE);
	  finish_anon_union (decl);
	}
      else
	error ("anonymous union cannot have a function member");
    }
  else if (ok_code == RECORD_TYPE
	   && found_tag == 1
	   && TYPE_LANG_SPECIFIC (t)
	   && CLASSTYPE_DECLARED_EXCEPTION (t))
    {
      if (TYPE_SIZE (t))
	error_with_aggr_type (t, "redeclaration of exception `%s'");
      else
	{
	  tree ename, decl;

	  push_obstacks (&permanent_obstack, &permanent_obstack);

	  pushclass (t, 0);
	  finish_exception (t, NULL_TREE);

	  ename = TYPE_NAME (t);
	  if (TREE_CODE (ename) == TYPE_DECL)
	    ename = DECL_NAME (ename);
	  decl = build_lang_field_decl (VAR_DECL, ename, t);
	  finish_exception_decl (current_class_name, decl);
	  end_exception_decls ();

	  pop_obstacks ();
	}
    }
  else if (!warned && found_tag > 1)
    warning ("multiple types in one declaration");
}

/* Decode a "typename", such as "int **", returning a ..._TYPE node.  */

tree
groktypename (typename)
     tree typename;
{
  if (TREE_CODE (typename) != TREE_LIST)
    return typename;
  return grokdeclarator (TREE_VALUE (typename),
			 TREE_PURPOSE (typename),
			 TYPENAME, 0, NULL_TREE);
}

/* Decode a declarator in an ordinary declaration or data definition.
   This is called as soon as the type information and variable name
   have been parsed, before parsing the initializer if any.
   Here we create the ..._DECL node, fill in its type,
   and put it on the list of decls for the current context.
   The ..._DECL node is returned as the value.

   Exception: for arrays where the length is not specified,
   the type is left null, to be filled in by `finish_decl'.

   Function definitions do not come here; they go to start_function
   instead.  However, external and forward declarations of functions
   do go through here.  Structure field declarations are done by
   grokfield and not through here.  */

/* Set this to zero to debug not using the temporary obstack
   to parse initializers.  */
int debug_temp_inits = 1;

tree
start_decl (declarator, declspecs, initialized, raises)
     tree declspecs, declarator;
     int initialized;
     tree raises;
{
  register tree decl;
  register tree type, tem;
  tree context;
  extern int have_extern_spec;
  extern int used_extern_spec;

  int init_written = initialized;

  /* This should only be done once on the top most decl. */
  if (have_extern_spec && !used_extern_spec)
    {
      declspecs = decl_tree_cons (NULL_TREE, get_identifier ("extern"), declspecs);
      used_extern_spec = 1;
    }

  decl = grokdeclarator (declarator, declspecs, NORMAL, initialized, raises);
  if (decl == NULL_TREE || decl == void_type_node)
    return NULL_TREE;

  type = TREE_TYPE (decl);

  /* Don't lose if destructors must be executed at file-level.  */
  if (TREE_STATIC (decl)
      && TYPE_NEEDS_DESTRUCTOR (type)
      && !TREE_PERMANENT (decl))
    {
      push_obstacks (&permanent_obstack, &permanent_obstack);
      decl = copy_node (decl);
      if (TREE_CODE (type) == ARRAY_TYPE)
	{
	  tree itype = TYPE_DOMAIN (type);
	  if (itype && ! TREE_PERMANENT (itype))
	    {
	      itype = build_index_type (copy_to_permanent (TYPE_MAX_VALUE (itype)));
	      type = build_cplus_array_type (TREE_TYPE (type), itype);
	      TREE_TYPE (decl) = type;
	    }
	}
      pop_obstacks ();
    }

  /* Interesting work for this is done in `finish_exception_decl'.  */
  if (TREE_CODE (type) == RECORD_TYPE
      && CLASSTYPE_DECLARED_EXCEPTION (type))
    return decl;

  /* Corresponding pop_obstacks is done in `finish_decl'.  */
  push_obstacks_nochange ();

  context
    = (TREE_CODE (decl) == FUNCTION_DECL && DECL_VIRTUAL_P (decl))
      ? DECL_CLASS_CONTEXT (decl)
      : DECL_CONTEXT (decl);

  if (processing_template_decl)
    {
      tree d;
      if (TREE_CODE (decl) == FUNCTION_DECL)
        {
          /* Declarator is a call_expr; extract arguments from it, since
             grokdeclarator didn't do it.  */
          tree args;
          args = copy_to_permanent (last_function_parms);
          if (TREE_CODE (TREE_TYPE (decl)) == METHOD_TYPE)
            {
	      tree t = TREE_TYPE (decl);
              tree decl;
	      
              t = TYPE_METHOD_BASETYPE (t); /* type method belongs to */
	      if (TREE_CODE (t) != UNINSTANTIATED_P_TYPE)
		{
		  t = build_pointer_type (t); /* base type of `this' */
#if 1
		  /* I suspect this is wrong. */
		  t = build_type_variant (t, flag_this_is_variable <= 0,
					  0); /* type of `this' */
#else
		  t = build_type_variant (t, 0, 0); /* type of `this' */
#endif
		  t = build (PARM_DECL, t, this_identifier);
		  TREE_CHAIN (t) = args;
		  args = t;
		}
	    }
          DECL_ARGUMENTS (decl) = args;
        }
      d = build_lang_decl (TEMPLATE_DECL, DECL_NAME (decl), TREE_TYPE (decl));
      TREE_PUBLIC (d) = TREE_PUBLIC (decl) = 0;
      TREE_STATIC (d) = TREE_STATIC (decl);
      DECL_EXTERNAL (d) = (DECL_EXTERNAL (decl)
			   && !(context && !DECL_THIS_EXTERN (decl)));
      DECL_TEMPLATE_RESULT (d) = decl;
      DECL_OVERLOADED (d) = 1;
      decl = d;
    }

  if (context && TYPE_SIZE (context) != NULL_TREE)
    {
      /* If it was not explicitly declared `extern',
	 revoke any previous claims of DECL_EXTERNAL.  */
      if (DECL_THIS_EXTERN (decl) == 0)
	DECL_EXTERNAL (decl) = 0;
      if (DECL_LANG_SPECIFIC (decl))
	DECL_IN_AGGR_P (decl) = 0;
      pushclass (context, 2);
    }

  /* If this type of object needs a cleanup, and control may
     jump past it, make a new binding level so that it is cleaned
     up only when it is initialized first.  */
  if (TYPE_NEEDS_DESTRUCTOR (type)
      && current_binding_level->more_cleanups_ok == 0)
    pushlevel_temporary (1);

  if (initialized)
    /* Is it valid for this decl to have an initializer at all?
       If not, set INITIALIZED to zero, which will indirectly
       tell `finish_decl' to ignore the initializer once it is parsed.  */
    switch (TREE_CODE (decl))
      {
      case TYPE_DECL:
	/* typedef foo = bar  means give foo the same type as bar.
	   We haven't parsed bar yet, so `finish_decl' will fix that up.
	   Any other case of an initialization in a TYPE_DECL is an error.  */
	if (pedantic || list_length (declspecs) > 1)
	  {
	    error ("typedef `%s' is initialized",
		   IDENTIFIER_POINTER (DECL_NAME (decl)));
	    initialized = 0;
	  }
	break;

      case FUNCTION_DECL:
	error ("function `%s' is initialized like a variable",
	       IDENTIFIER_POINTER (DECL_NAME (decl)));
	initialized = 0;
	break;

      default:
	/* Don't allow initializations for incomplete types
	   except for arrays which might be completed by the initialization.  */
	if (TYPE_SIZE (type) != NULL_TREE)
	  ;                     /* A complete type is ok.  */
	else if (TREE_CODE (type) != ARRAY_TYPE)
	  {
	    error ("variable `%s' has initializer but incomplete type",
		   IDENTIFIER_POINTER (DECL_NAME (decl)));
	    initialized = 0;
	  }
	else if (TYPE_SIZE (TREE_TYPE (type)) == NULL_TREE)
	  {
	    error ("elements of array `%s' have incomplete type",
		   IDENTIFIER_POINTER (DECL_NAME (decl)));
	    initialized = 0;
	  }
      }

  if (!initialized
      && TREE_CODE (decl) != TYPE_DECL
      && TREE_CODE (decl) != TEMPLATE_DECL
      && IS_AGGR_TYPE (type) && ! DECL_EXTERNAL (decl))
    {
      if (TYPE_SIZE (type) == NULL_TREE)
	{
	  error ("aggregate `%s' has incomplete type and cannot be initialized",
		 IDENTIFIER_POINTER (DECL_NAME (decl)));
	  /* Change the type so that assemble_variable will give
	     DECL an rtl we can live with: (mem (const_int 0)).  */
	  TREE_TYPE (decl) = error_mark_node;
	  type = error_mark_node;
	}
      else
	{
	  /* If any base type in the hierarchy of TYPE needs a constructor,
	     then we set initialized to 1.  This way any nodes which are
	     created for the purposes of initializing this aggregate
	     will live as long as it does.  This is necessary for global
	     aggregates which do not have their initializers processed until
	     the end of the file.  */
	  initialized = TYPE_NEEDS_CONSTRUCTING (type);
	}
    }

  if (initialized)
    {
      if (current_binding_level != global_binding_level
	  && DECL_EXTERNAL (decl))
	warning ("declaration of `%s' has `extern' and is initialized",
		 IDENTIFIER_POINTER (DECL_NAME (decl)));
      DECL_EXTERNAL (decl) = 0;
      if (current_binding_level == global_binding_level)
	TREE_STATIC (decl) = 1;

      /* Tell `pushdecl' this is an initialized decl
	 even though we don't yet have the initializer expression.
	 Also tell `finish_decl' it may store the real initializer.  */
      DECL_INITIAL (decl) = error_mark_node;
    }

  /* Add this decl to the current binding level, but not if it
     comes from another scope, e.g. a static member variable.
     TEM may equal DECL or it may be a previous decl of the same name.  */
  if ((TREE_CODE (decl) != PARM_DECL && DECL_CONTEXT (decl) != NULL_TREE)
      || (TREE_CODE (decl) == TEMPLATE_DECL && !global_bindings_p ())
      || TREE_CODE (type) == LANG_TYPE)
    tem = decl;
  else
    {
      tem = pushdecl (decl);
      if (TREE_CODE (tem) == TREE_LIST)
	{
	  tree tem2 = value_member (decl, tem);
	  if (tem2 != NULL_TREE)
	    tem = TREE_VALUE (tem2);
	  else
	    {
	      while (tem && ! decls_match (decl, TREE_VALUE (tem)))
		tem = TREE_CHAIN (tem);
	      if (tem == NULL_TREE)
		tem = decl;
	      else
		tem = TREE_VALUE (tem);
	    }
	}
    }

#if 0
  /* We don't do this yet for GNU C++.  */
  /* For a local variable, define the RTL now.  */
  if (current_binding_level != global_binding_level
      /* But not if this is a duplicate decl
	 and we preserved the rtl from the previous one
	 (which may or may not happen).  */
      && DECL_RTL (tem) == NULL_RTX)
    {
      if (TYPE_SIZE (TREE_TYPE (tem)) != NULL_TREE)
	expand_decl (tem);
      else if (TREE_CODE (TREE_TYPE (tem)) == ARRAY_TYPE
	       && DECL_INITIAL (tem) != NULL_TREE)
	expand_decl (tem);
    }
#endif

  if (TREE_CODE (decl) == FUNCTION_DECL && DECL_OVERLOADED (decl))
    /* @@ Also done in start_function.  */
    tem = push_overloaded_decl (tem, 1);
  else if (TREE_CODE (decl) == TEMPLATE_DECL)
    {
      tree result = DECL_TEMPLATE_RESULT (decl);
      if (DECL_CONTEXT (result) != NULL_TREE)
	{
          tree type;
          type = DECL_CONTEXT (result);
          my_friendly_assert (TREE_CODE (type) == UNINSTANTIATED_P_TYPE, 145);
          if (/* TREE_CODE (result) == VAR_DECL */ 1)
            {
#if 0
              tree tmpl = UPT_TEMPLATE (type);
	      
	      fprintf (stderr, "%s:%d: adding ", __FILE__, __LINE__);
	      print_node_brief (stderr, "", DECL_NAME (tem), 0);
	      fprintf (stderr, " to class %s\n",
		       IDENTIFIER_POINTER (DECL_NAME (tmpl)));
              DECL_TEMPLATE_MEMBERS (tmpl)
                = perm_tree_cons (DECL_NAME (tem), tem,
				  DECL_TEMPLATE_MEMBERS (tmpl));
#endif
	      return tem;
	    }
          my_friendly_abort (13);
        }
      else if (TREE_CODE (result) == FUNCTION_DECL)
        tem = push_overloaded_decl (tem, 0);
      else if (TREE_CODE (result) == VAR_DECL
	       || TREE_CODE (result) == TYPE_DECL)
	{
	  error ("invalid template `%s'",
		 IDENTIFIER_POINTER (DECL_NAME (result)));
	  return NULL_TREE;
	}
      else
	my_friendly_abort (14);
    }

  if (init_written
      && ! (TREE_CODE (tem) == PARM_DECL
	    || (TREE_READONLY (tem)
		&& (TREE_CODE (tem) == VAR_DECL
		    || TREE_CODE (tem) == FIELD_DECL))))
    {
      /* When parsing and digesting the initializer,
	 use temporary storage.  Do this even if we will ignore the value.  */
      if (current_binding_level == global_binding_level && debug_temp_inits)
	{
	  if (TYPE_NEEDS_CONSTRUCTING (type) || TREE_CODE (type) == REFERENCE_TYPE)
	    /* In this case, the initializer must lay down in permanent
	       storage, since it will be saved until `finish_file' is run.   */
	    ;
	  else
	    temporary_allocation ();
	}
    }

  if (flag_cadillac)
    cadillac_start_decl (tem);

  return tem;
}

static void
make_temporary_for_reference (decl, ctor_call, init, cleanupp)
     tree decl, ctor_call, init;
     tree *cleanupp;
{
  tree type = TREE_TYPE (decl);
  tree target_type = TREE_TYPE (type);
  tree tmp, tmp_addr;

  if (ctor_call)
    {
      tmp_addr = TREE_VALUE (TREE_OPERAND (ctor_call, 1));
      if (TREE_CODE (tmp_addr) == NOP_EXPR)
	tmp_addr = TREE_OPERAND (tmp_addr, 0);
      my_friendly_assert (TREE_CODE (tmp_addr) == ADDR_EXPR, 146);
      tmp = TREE_OPERAND (tmp_addr, 0);
    }
  else
    {
      tmp = get_temp_name (target_type,
			   current_binding_level == global_binding_level);
      tmp_addr = build_unary_op (ADDR_EXPR, tmp, 0);
    }

  TREE_TYPE (tmp_addr) = build_pointer_type (target_type);
  DECL_INITIAL (decl) = convert (TYPE_POINTER_TO (target_type), tmp_addr);
  TREE_TYPE (DECL_INITIAL (decl)) = type;
  if (TYPE_NEEDS_CONSTRUCTING (target_type))
    {
      if (current_binding_level == global_binding_level)
	{
	  /* lay this variable out now.  Otherwise `output_addressed_constants'
	     gets confused by its initializer.  */
	  make_decl_rtl (tmp, NULL_PTR, 1);
	  static_aggregates = perm_tree_cons (init, tmp, static_aggregates);
	}
      else
	{
	  if (ctor_call != NULL_TREE)
	    init = ctor_call;
	  else
	    init = build_method_call (tmp, constructor_name (target_type),
				      build_tree_list (NULL_TREE, init),
				      NULL_TREE, LOOKUP_NORMAL);
	  DECL_INITIAL (decl) = build (COMPOUND_EXPR, type, init,
				       DECL_INITIAL (decl));
	  *cleanupp = maybe_build_cleanup (tmp);
	}
    }
  else
    {
      DECL_INITIAL (tmp) = init;
      TREE_STATIC (tmp) = current_binding_level == global_binding_level;
      finish_decl (tmp, init, 0, 0);
    }
  if (TREE_STATIC (tmp))
    preserve_initializer ();
}

/* Handle initialization of references.
   These three arguments from from `finish_decl', and have the
   same meaning here that they do there.  */
/* quotes on semantics can be found in ARM 8.4.3. */
static void
grok_reference_init (decl, type, init, cleanupp)
     tree decl, type, init;
     tree *cleanupp;
{
  char *errstr = NULL;
  int is_reference;
  tree tmp;
  tree this_ptr_type, actual_init;

  if (init == NULL_TREE)
    {
      if (DECL_LANG_SPECIFIC (decl) == 0
	  || DECL_IN_AGGR_P (decl) == 0)
	{
	  error ("variable declared as reference not initialized");
	  if (TREE_CODE (decl) == VAR_DECL)
	    SET_DECL_REFERENCE_SLOT (decl, error_mark_node);
	}
      return;
    }

  if (TREE_CODE (init) == TREE_LIST)
    init = build_compound_expr (init);
  is_reference = TREE_CODE (TREE_TYPE (init)) == REFERENCE_TYPE;
  tmp = is_reference ? convert_from_reference (init) : init;

  if (is_reference)
    {
      if (! comptypes (TYPE_MAIN_VARIANT (TREE_TYPE (type)),
		       TYPE_MAIN_VARIANT (TREE_TYPE (tmp)), 0))
	errstr = "initialization of `%s' from dissimilar reference type";
      else if (TYPE_READONLY (TREE_TYPE (type))
	       >= TYPE_READONLY (TREE_TYPE (TREE_TYPE (init))))
	{
	  is_reference = 0;
	  init = tmp;
	}
    }
  else
    {
      if (TREE_CODE (TREE_TYPE (type)) != ARRAY_TYPE
	  && TREE_CODE (TREE_TYPE (init)) == ARRAY_TYPE)
	{
	  /* Note: default conversion is only called in very
	     special cases.  */
	  init = default_conversion (init);
	}
      if (TREE_CODE (TREE_TYPE (type)) == TREE_CODE (TREE_TYPE (init)))
	{
	  if (comptypes (TYPE_MAIN_VARIANT (TREE_TYPE (type)),
			 TYPE_MAIN_VARIANT (TREE_TYPE (init)), 0))
	    {
	      /* This section implements ANSI C++ June 5 1992 WP 8.4.3.5. */

	      /* A reference to a volatile T cannot be initialized with
		 a const T, and vice-versa.  */
	      if (TYPE_VOLATILE (TREE_TYPE (type)) && TREE_READONLY (init))
		errstr = "cannot initialize a reference to a volatile T with a const T";
	      else if (TYPE_READONLY (TREE_TYPE (type)) && TREE_THIS_VOLATILE (init))
		errstr = "cannot initialize a reference to a const T with a volatile T";
	      /* A reference to a plain T can be initialized only with
		 a plain T.  */
	      else if (!TYPE_VOLATILE (TREE_TYPE (type))
		       && !TYPE_READONLY (TREE_TYPE (type)))
		{
		  if (TREE_READONLY (init))
		    errstr = "cannot initialize a reference to T with a const T";
		  else if (TREE_THIS_VOLATILE (init))
		    errstr = "cannot initialize a reference to T with a volatile T";
		}
	    }
	  else
	    init = convert (TREE_TYPE (type), init);
	}
      else if (init != error_mark_node
	       && ! comptypes (TYPE_MAIN_VARIANT (TREE_TYPE (type)),
			       TYPE_MAIN_VARIANT (TREE_TYPE (init)), 0))
	errstr = "invalid type conversion for reference";
    }

  if (errstr)
    {
      /* Things did not go smoothly; look for operator& type conversion.  */
      if (IS_AGGR_TYPE (TREE_TYPE (tmp)))
	{
	  tmp = build_type_conversion (CONVERT_EXPR, type, init, 0);
	  if (tmp != NULL_TREE)
	    {
	      init = tmp;
	      if (tmp == error_mark_node)
		errstr = "ambiguous pointer conversion";
	      else
		errstr = NULL;
	      is_reference = 1;
	    }
	  else
	    {
	      tmp = build_type_conversion (CONVERT_EXPR, TREE_TYPE (type), init, 0);
	      if (tmp != NULL_TREE)
		{
		  init = tmp;
		  if (tmp == error_mark_node)
		    errstr = "ambiguous pointer conversion";
		  else
		    errstr = NULL;
		  is_reference = 0;
		}
	    }
	}
      /* Look for constructor.  */
      else if (IS_AGGR_TYPE (TREE_TYPE (type))
	       && TYPE_HAS_CONSTRUCTOR (TREE_TYPE (type)))
	{
	  tmp = get_temp_name (TREE_TYPE (type),
			       current_binding_level == global_binding_level);
	  tmp = build_method_call (tmp, constructor_name (TREE_TYPE (type)),
				   build_tree_list (NULL_TREE, init),
				   NULL_TREE, LOOKUP_NORMAL);
	  if (tmp == NULL_TREE || tmp == error_mark_node)
	    {
	      if (TREE_CODE (decl) == VAR_DECL)
		SET_DECL_REFERENCE_SLOT (decl, error_mark_node);
	      error_with_decl (decl, "constructor failed to build reference initializer");
	      return;
	    }
	  make_temporary_for_reference (decl, tmp, init, cleanupp);
	  goto done;
	}
    }

  if (errstr)
    {
      error_with_decl (decl, errstr);
      if (TREE_CODE (decl) == VAR_DECL)
	SET_DECL_REFERENCE_SLOT (decl, error_mark_node);
      return;
    }

  /* In the case of initialization, it is permissible
     to assign one reference to another.  */
  this_ptr_type = build_pointer_type (TREE_TYPE (type));

  if (is_reference)
    {
      if (TREE_SIDE_EFFECTS (init))
	DECL_INITIAL (decl) = save_expr (init);
      else
	DECL_INITIAL (decl) = init;
    }
  else if (lvalue_p (init))
    {
      tmp = build_unary_op (ADDR_EXPR, init, 0);
      if (TREE_CODE (tmp) == ADDR_EXPR
	  && TREE_CODE (TREE_OPERAND (tmp, 0)) == WITH_CLEANUP_EXPR)
	{
	  /* Associate the cleanup with the reference so that we
	     don't get burned by "aggressive" cleanup policy.  */
	  *cleanupp = TREE_OPERAND (TREE_OPERAND (tmp, 0), 2);
	  TREE_OPERAND (TREE_OPERAND (tmp, 0), 2) = error_mark_node;
	}
      if (IS_AGGR_TYPE (TREE_TYPE (this_ptr_type)))
	DECL_INITIAL (decl) = convert_pointer_to (TREE_TYPE (this_ptr_type), tmp);
      else
	DECL_INITIAL (decl) = convert (this_ptr_type, tmp);

      DECL_INITIAL (decl) = save_expr (DECL_INITIAL (decl));
      if (DECL_INITIAL (decl) == current_class_decl)
	DECL_INITIAL (decl) = copy_node (current_class_decl);
      TREE_TYPE (DECL_INITIAL (decl)) = type;
    }
  else if ((actual_init = unary_complex_lvalue (ADDR_EXPR, init)))
    {
      /* The initializer for this decl goes into its
	 DECL_REFERENCE_SLOT.  Make sure that we can handle
	 multiple evaluations without ill effect.  */
      if (TREE_CODE (actual_init) == ADDR_EXPR
	  && TREE_CODE (TREE_OPERAND (actual_init, 0)) == TARGET_EXPR)
	actual_init = save_expr (actual_init);
      DECL_INITIAL (decl) = convert_pointer_to (TREE_TYPE (this_ptr_type), actual_init);
      DECL_INITIAL (decl) = save_expr (DECL_INITIAL (decl));
      TREE_TYPE (DECL_INITIAL (decl)) = type;
    }
  else if (TYPE_READONLY (TREE_TYPE (type)))
    /* Section 8.4.3 allows us to make a temporary for
       the initialization of const&.  */
    make_temporary_for_reference (decl, NULL_TREE, init, cleanupp);
  else
    {
      error_with_decl (decl, "type mismatch in initialization of `%s' (use `const')");
      DECL_INITIAL (decl) = error_mark_node;
    }

 done:
  /* ?? Can this be optimized in some cases to
     hand back the DECL_INITIAL slot??  */
  if (TYPE_SIZE (TREE_TYPE (type)))
    {
      init = convert_from_reference (decl);
      if (TREE_PERMANENT (decl))
	init = copy_to_permanent (init);
      SET_DECL_REFERENCE_SLOT (decl, init);
    }

  if (TREE_STATIC (decl) && ! TREE_CONSTANT (DECL_INITIAL (decl)))
    {
      expand_static_init (decl, DECL_INITIAL (decl));
      DECL_INITIAL (decl) = NULL_TREE;
    }
}

/* Finish processing of a declaration;
   install its line number and initial value.
   If the length of an array type is not known before,
   it must be determined now, from the initial value, or it is an error.

   Call `pop_obstacks' iff NEED_POP is nonzero.

   For C++, `finish_decl' must be fairly evasive:  it must keep initializers
   for aggregates that have constructors alive on the permanent obstack,
   so that the global initializing functions can be written at the end.

   INIT0 holds the value of an initializer that should be allowed to escape
   the normal rules.

   For functions that take default parameters, DECL points to its
   "maximal" instantiation.  `finish_decl' must then also declared its
   subsequently lower and lower forms of instantiation, checking for
   ambiguity as it goes.  This can be sped up later.  */

void
finish_decl (decl, init, asmspec_tree, need_pop)
     tree decl, init;
     tree asmspec_tree;
     int need_pop;
{
  register tree type;
  tree cleanup = NULL_TREE, ttype;
  int was_incomplete;
  int temporary = allocation_temporary_p ();
  char *asmspec = NULL;
  int was_readonly = 0;

  /* If this is 0, then we did not change obstacks.  */
  if (! decl)
    {
      if (init)
	error ("assignment (not initialization) in declaration");
      return;
    }

  if (asmspec_tree)
    {
      asmspec = TREE_STRING_POINTER (asmspec_tree);
      /* Zero out old RTL, since we will rewrite it.  */
      DECL_RTL (decl) = NULL_RTX;
    }

  /* If the type of the thing we are declaring either has
     a constructor, or has a virtual function table pointer,
     AND its initialization was accepted by `start_decl',
     then we stayed on the permanent obstack through the
     declaration, otherwise, changed obstacks as GCC would.  */

  type = TREE_TYPE (decl);

  was_incomplete = (DECL_SIZE (decl) == NULL_TREE);

  /* Take care of TYPE_DECLs up front.  */
  if (TREE_CODE (decl) == TYPE_DECL)
    {
      if (init && DECL_INITIAL (decl))
	{
	  /* typedef foo = bar; store the type of bar as the type of foo.  */
	  TREE_TYPE (decl) = type = TREE_TYPE (init);
	  DECL_INITIAL (decl) = init = NULL_TREE;
	}
      if (IS_AGGR_TYPE (type) && DECL_NAME (decl))
	{
	  if (TREE_TYPE (DECL_NAME (decl)) && TREE_TYPE (decl) != type)
	    warning ("shadowing previous type declaration of `%s'",
		     IDENTIFIER_POINTER (DECL_NAME (decl)));
	  set_identifier_type_value (DECL_NAME (decl), type);
	  CLASSTYPE_GOT_SEMICOLON (type) = 1;
	}
      GNU_xref_decl (current_function_decl, decl);
      rest_of_decl_compilation (decl, NULL_PTR,
				DECL_CONTEXT (decl) == NULL_TREE, 0);
      goto finish_end;
    }
  if (type != error_mark_node && IS_AGGR_TYPE (type)
      && CLASSTYPE_DECLARED_EXCEPTION (type))
    {
      finish_exception_decl (NULL_TREE, decl);
      CLASSTYPE_GOT_SEMICOLON (type) = 1;
      goto finish_end;
    }
  if (TREE_CODE (decl) != FUNCTION_DECL)
    {
      ttype = target_type (type);
#if 0 /* WTF?  -KR
	 Leave this out until we can figure out why it was
	 needed/desirable in the first place.  Then put a comment
	 here explaining why.  Or just delete the code if no ill
	 effects arise.  */
      if (TYPE_NAME (ttype)
	  && TREE_CODE (TYPE_NAME (ttype)) == TYPE_DECL
	  && ANON_AGGRNAME_P (TYPE_IDENTIFIER (ttype)))
	{
	  tree old_id = TYPE_IDENTIFIER (ttype);
	  char *newname = (char *)alloca (IDENTIFIER_LENGTH (old_id) + 2);
	  /* Need to preserve template data for UPT nodes.  */
	  tree old_template = IDENTIFIER_TEMPLATE (old_id);
	  newname[0] = '_';
	  bcopy (IDENTIFIER_POINTER (old_id), newname + 1,
		 IDENTIFIER_LENGTH (old_id) + 1);
	  old_id = get_identifier (newname);
	  lookup_tag_reverse (ttype, old_id);
	  TYPE_IDENTIFIER (ttype) = old_id;
	  IDENTIFIER_TEMPLATE (old_id) = old_template;
	}
#endif
    }

  if (! DECL_EXTERNAL (decl) && TREE_READONLY (decl)
      && TYPE_NEEDS_CONSTRUCTING (type))
    {

      /* Currently, GNU C++ puts constants in text space, making them
	 impossible to initialize.  In the future, one would hope for
	 an operating system which understood the difference between
	 initialization and the running of a program.  */
      was_readonly = 1;
      TREE_READONLY (decl) = 0;
    }

  if (TREE_CODE (decl) == FIELD_DECL)
    {
      if (init && init != error_mark_node)
	my_friendly_assert (TREE_PERMANENT (init), 147);

      if (asmspec)
	{
	  /* This must override the asm specifier which was placed
	     by grokclassfn.  Lay this out fresh.
	     
	     @@ Should emit an error if this redefines an asm-specified
	     @@ name, or if we have already used the function's name.  */
	  DECL_RTL (TREE_TYPE (decl)) = NULL_RTX;
	  DECL_ASSEMBLER_NAME (decl) = get_identifier (asmspec);
	  make_decl_rtl (decl, asmspec, 0);
	}
    }
  /* If `start_decl' didn't like having an initialization, ignore it now.  */
  else if (init != NULL_TREE && DECL_INITIAL (decl) == NULL_TREE)
    init = NULL_TREE;
  else if (DECL_EXTERNAL (decl))
    ;
  else if (TREE_CODE (type) == REFERENCE_TYPE)
    {
      grok_reference_init (decl, type, init, &cleanup);
      init = NULL_TREE;
    }

  GNU_xref_decl (current_function_decl, decl);

  if (TREE_CODE (decl) == FIELD_DECL || DECL_EXTERNAL (decl))
    ;
  else if (TREE_CODE (decl) == CONST_DECL)
    {
      my_friendly_assert (TREE_CODE (decl) != REFERENCE_TYPE, 148);

      DECL_INITIAL (decl) = init;

      /* This will keep us from needing to worry about our obstacks.  */
      my_friendly_assert (init != NULL_TREE, 149);
      init = NULL_TREE;
    }
  else if (init)
    {
      if (TYPE_NEEDS_CONSTRUCTING (type))
	{
	  if (TREE_CODE (type) == ARRAY_TYPE)
	    init = digest_init (type, init, (tree *) 0);
	  else if (TREE_CODE (init) == CONSTRUCTOR
		   && CONSTRUCTOR_ELTS (init) != NULL_TREE)
	    {
	      error_with_decl (decl, "`%s' must be initialized by constructor, not by `{...}'");
	      init = error_mark_node;
	    }
#if 0
	  /* fix this in `build_functional_cast' instead.
	     Here's the trigger code:

		struct ostream
		{
		  ostream ();
		  ostream (int, char *);
		  ostream (char *);
		  operator char *();
		  ostream (void *);
		  operator void *();
		  operator << (int);
		};
		int buf_size = 1024;
		static char buf[buf_size];
		const char *debug(int i) {
		  char *b = &buf[0];
		  ostream o = ostream(buf_size, b);
		  o << i;
		  return buf;
		}
		*/

	  else if (TREE_CODE (init) == TARGET_EXPR
		   && TREE_CODE (TREE_OPERAND (init, 1) == NEW_EXPR))
	    {
	      /* User wrote something like `foo x = foo (args)'  */
	      my_friendly_assert (TREE_CODE (TREE_OPERAND (init, 0)) == VAR_DECL, 150);
	      my_friendly_assert (DECL_NAME (TREE_OPERAND (init, 0)) == NULL_TREE, 151);

	      /* User wrote exactly `foo x = foo (args)'  */
	      if (TYPE_MAIN_VARIANT (type) == TREE_TYPE (init))
		{
		  init = build (CALL_EXPR, TREE_TYPE (init),
				TREE_OPERAND (TREE_OPERAND (init, 1), 0),
				TREE_OPERAND (TREE_OPERAND (init, 1), 1), 0);
		  TREE_SIDE_EFFECTS (init) = 1;
		}
	    }
#endif

	  /* We must hide the initializer so that expand_decl
	     won't try to do something it does not understand.  */
	  if (current_binding_level == global_binding_level)
	    {
	      tree value = digest_init (type, empty_init_node, (tree *) 0);
	      DECL_INITIAL (decl) = value;
	    }
	  else
	    DECL_INITIAL (decl) = error_mark_node;
	}
      else
	{
	  if (TREE_CODE (init) != TREE_VEC)
	    init = store_init_value (decl, init);

	  if (init)
	    /* Don't let anyone try to initialize this variable
	       until we are ready to do so.  */
	    DECL_INITIAL (decl) = error_mark_node;
	}
    }
  else if (TREE_CODE_CLASS (TREE_CODE (type)) == 't'
	   && (IS_AGGR_TYPE (type) || TYPE_NEEDS_CONSTRUCTING (type)))
    {
      tree ctype = type;
      while (TREE_CODE (ctype) == ARRAY_TYPE)
	ctype = TREE_TYPE (ctype);
      if (! TYPE_NEEDS_CONSTRUCTOR (ctype))
	{
	  if (CLASSTYPE_READONLY_FIELDS_NEED_INIT (ctype))
	    error_with_decl (decl, "structure `%s' with uninitialized const members");
	  if (CLASSTYPE_REF_FIELDS_NEED_INIT (ctype))
	    error_with_decl (decl, "structure `%s' with uninitialized reference members");
	}

      if (TREE_CODE (decl) == VAR_DECL
	  && !TYPE_NEEDS_CONSTRUCTING (type)
	  && (TYPE_READONLY (type) || TREE_READONLY (decl)))
	error_with_decl (decl, "uninitialized const `%s'");

      /* Initialize variables in need of static initialization
	 with `empty_init_node' to keep assemble_variable from putting them
	 in the wrong program space.  (Common storage is okay for non-public
	 uninitialized data; the linker can't match it with storage from other
	 files, and we may save some disk space.)  */
      if (flag_pic == 0
	  && TREE_STATIC (decl)
	  && TREE_PUBLIC (decl)
	  && ! DECL_EXTERNAL (decl)
	  && TREE_CODE (decl) == VAR_DECL
	  && TYPE_NEEDS_CONSTRUCTING (type)
	  && (DECL_INITIAL (decl) == NULL_TREE
	      || DECL_INITIAL (decl) == error_mark_node))
	{
	  tree value = digest_init (type, empty_init_node, (tree *) 0);
	  DECL_INITIAL (decl) = value;
	}
    }
  else if (TREE_CODE (decl) == VAR_DECL
	   && TREE_CODE (type) != REFERENCE_TYPE
	   && (TYPE_READONLY (type) || TREE_READONLY (decl)))
    {
      /* ``Unless explicitly declared extern, a const object does not have
	 external linkage and must be initialized. ($8.4; $12.1)'' ARM 7.1.6
	 However, if it's `const int foo = 1; const int foo;', don't complain
	 about the second decl, since it does have an initializer before.  */
      if (! DECL_INITIAL (decl) && (!pedantic || !current_class_type))
	error_with_decl (decl, "uninitialized const `%s'");
    }

  /* For top-level declaration, the initial value was read in
     the temporary obstack.  MAXINDEX, rtl, etc. to be made below
     must go in the permanent obstack; but don't discard the
     temporary data yet.  */

  if (current_binding_level == global_binding_level && temporary)
    end_temporary_allocation ();

  /* Deduce size of array from initialization, if not already known.  */

  if (TREE_CODE (type) == ARRAY_TYPE
      && TYPE_DOMAIN (type) == NULL_TREE
      && TREE_CODE (decl) != TYPE_DECL)
    {
      int do_default
	= (TREE_STATIC (decl)
	   /* Even if pedantic, an external linkage array
	      may have incomplete type at first.  */
	   ? pedantic && DECL_EXTERNAL (decl)
	   : !DECL_EXTERNAL (decl));
      tree initializer = init ? init : DECL_INITIAL (decl);
      int failure = complete_array_type (type, initializer, do_default);

      if (failure == 1)
	error_with_decl (decl, "initializer fails to determine size of `%s'");

      if (failure == 2)
	{
	  if (do_default)
	    error_with_decl (decl, "array size missing in `%s'");
	  else if (!pedantic && TREE_STATIC (decl))
	    DECL_EXTERNAL (decl) = 1;
	}

      if (pedantic && TYPE_DOMAIN (type) != NULL_TREE
	  && tree_int_cst_lt (TYPE_MAX_VALUE (TYPE_DOMAIN (type)),
			      integer_zero_node))
	error_with_decl (decl, "zero-size array `%s'");

      layout_decl (decl, 0);
    }

  if (TREE_CODE (decl) == VAR_DECL)
    {
      if (TREE_STATIC (decl) && DECL_SIZE (decl) == NULL_TREE)
	{
	  /* A static variable with an incomplete type:
	     that is an error if it is initialized or `static'.
	     Otherwise, let it through, but if it is not `extern'
	     then it may cause an error message later.  */
	  if (!DECL_EXTERNAL (decl) || DECL_INITIAL (decl) != NULL_TREE)
	    error_with_decl (decl, "storage size of `%s' isn't known");
	  init = NULL_TREE;
	}
      else if (!DECL_EXTERNAL (decl) && DECL_SIZE (decl) == NULL_TREE)
	{
	  /* An automatic variable with an incomplete type: that is an error.
	     Don't talk about array types here, since we took care of that
	     message in grokdeclarator.  */
	  error_with_decl (decl, "storage size of `%s' isn't known");
	  TREE_TYPE (decl) = error_mark_node;
	}
      else if (!DECL_EXTERNAL (decl) && IS_AGGR_TYPE (ttype))
	/* Let debugger know it should output info for this type.  */
	note_debug_info_needed (ttype);

      if ((DECL_EXTERNAL (decl) || TREE_STATIC (decl))
	  && DECL_SIZE (decl) != NULL_TREE
	  && ! TREE_CONSTANT (DECL_SIZE (decl)))
	error_with_decl (decl, "storage size of `%s' isn't constant");

      if (!DECL_EXTERNAL (decl) && TYPE_NEEDS_DESTRUCTOR (type))
	{
	  int yes = suspend_momentary ();

	  /* If INIT comes from a functional cast, use the cleanup
	     we built for that.  Otherwise, make our own cleanup.  */
	  if (init && TREE_CODE (init) == WITH_CLEANUP_EXPR
	      && comptypes (TREE_TYPE (decl), TREE_TYPE (init), 1))
	    {
	      cleanup = TREE_OPERAND (init, 2);
	      init = TREE_OPERAND (init, 0);
	      current_binding_level->have_cleanups = 1;
	      current_binding_level->more_exceptions_ok = 0;
	    }
	  else
	    cleanup = maybe_build_cleanup (decl);
	  resume_momentary (yes);
	}
    }
  /* PARM_DECLs get cleanups, too.  */
  else if (TREE_CODE (decl) == PARM_DECL && TYPE_NEEDS_DESTRUCTOR (type))
    {
      if (temporary)
	end_temporary_allocation ();
      cleanup = maybe_build_cleanup (decl);
      if (temporary)
	resume_temporary_allocation ();
    }

  /* Output the assembler code and/or RTL code for variables and functions,
     unless the type is an undefined structure or union.
     If not, it will get done when the type is completed.  */

  if (TREE_CODE (decl) == VAR_DECL || TREE_CODE (decl) == FUNCTION_DECL
      || TREE_CODE (decl) == RESULT_DECL)
    {
      int toplev = current_binding_level == global_binding_level;
      int was_temp
	= ((flag_traditional
	    || (TREE_STATIC (decl) && TYPE_NEEDS_DESTRUCTOR (type)))
	   && allocation_temporary_p ());

      if (was_temp)
	end_temporary_allocation ();

      /* If we are in need of a cleanup, get out of any implicit
	 handlers that have been established so far.  */
      if (cleanup && current_binding_level->parm_flag == 3)
	{
	  pop_implicit_try_blocks (decl);
	  current_binding_level->more_exceptions_ok = 0;
	}

      if (TREE_CODE (decl) == VAR_DECL
	  && current_binding_level != global_binding_level
	  && ! TREE_STATIC (decl)
	  && type_needs_gc_entry (type))
	DECL_GC_OFFSET (decl) = size_int (++current_function_obstack_index);

      if (TREE_CODE (decl) == VAR_DECL && DECL_VIRTUAL_P (decl))
	make_decl_rtl (decl, NULL_PTR, toplev);
      else if (TREE_CODE (decl) == VAR_DECL
	       && TREE_READONLY (decl)
	       && DECL_INITIAL (decl) != NULL_TREE
	       && DECL_INITIAL (decl) != error_mark_node
	       && DECL_INITIAL (decl) != empty_init_node)
	{
	  DECL_INITIAL (decl) = save_expr (DECL_INITIAL (decl));

	  if (asmspec)
	    DECL_ASSEMBLER_NAME (decl) = get_identifier (asmspec);

	  if (! toplev
	      && TREE_STATIC (decl)
	      && ! TREE_SIDE_EFFECTS (decl)
	      && ! TREE_PUBLIC (decl)
	      && ! DECL_EXTERNAL (decl)
	      && ! TYPE_NEEDS_DESTRUCTOR (type)
	      && DECL_MODE (decl) != BLKmode)
	    {
	      /* If this variable is really a constant, then fill its DECL_RTL
		 slot with something which won't take up storage.
		 If something later should take its address, we can always give
		 it legitimate RTL at that time.  */
	      DECL_RTL (decl) = gen_reg_rtx (DECL_MODE (decl));
	      store_expr (DECL_INITIAL (decl), DECL_RTL (decl), 0);
	      TREE_ASM_WRITTEN (decl) = 1;
	    }
	  else if (toplev)
	    {
	      /* Keep GCC from complaining that this variable
		 is defined but never used.  */
	      TREE_USED (decl) = 1;
	      /* If this is a static const, change its apparent linkage
		 if it belongs to a #pragma interface.  */
	      if (TREE_STATIC (decl) && !interface_unknown)
		{
		  TREE_PUBLIC (decl) = 1;
		  DECL_EXTERNAL (decl) = interface_only;
		}
	      make_decl_rtl (decl, asmspec, toplev);
	    }
	  else
	    rest_of_decl_compilation (decl, asmspec, toplev, 0);
	}
      else if (TREE_CODE (decl) == VAR_DECL
	       && DECL_LANG_SPECIFIC (decl)
	       && DECL_IN_AGGR_P (decl))
	{
	  if (TREE_STATIC (decl))
	    if (init == NULL_TREE
#ifdef DEFAULT_STATIC_DEFS
		/* If this code is dead, then users must
		   explicitly declare static member variables
		   outside the class def'n as well.  */
		&& TYPE_NEEDS_CONSTRUCTING (type)
#endif
		)
	      {
		DECL_EXTERNAL (decl) = 1;
		make_decl_rtl (decl, asmspec, 1);
	      }
	    else
	      rest_of_decl_compilation (decl, asmspec, toplev, 0);
	  else
	    /* Just a constant field.  Should not need any rtl.  */
	    goto finish_end0;
	}
      else
	rest_of_decl_compilation (decl, asmspec, toplev, 0);

      if (was_temp)
	resume_temporary_allocation ();

      if (type != error_mark_node
	  && TYPE_LANG_SPECIFIC (type)
	  && CLASSTYPE_ABSTRACT_VIRTUALS (type))
	abstract_virtuals_error (decl, type);
      else if ((TREE_CODE (type) == FUNCTION_TYPE
		|| TREE_CODE (type) == METHOD_TYPE)
	       && TYPE_LANG_SPECIFIC (TREE_TYPE (type))
	       && CLASSTYPE_ABSTRACT_VIRTUALS (TREE_TYPE (type)))
	abstract_virtuals_error (decl, TREE_TYPE (type));

      if (TREE_CODE (decl) == FUNCTION_DECL)
	{
	  /* C++: Handle overloaded functions with default parameters.  */
	  if (DECL_OVERLOADED (decl))
	    {
	      tree parmtypes = TYPE_ARG_TYPES (type);
	      tree prev = NULL_TREE;
	      tree original_name = DECL_NAME (decl);
	      struct lang_decl *tmp_lang_decl = DECL_LANG_SPECIFIC (decl);
	      /* All variants will share an uncollectible lang_decl.  */
	      copy_decl_lang_specific (decl);

	      while (parmtypes && parmtypes != void_list_node)
		{
		  if (TREE_PURPOSE (parmtypes))
		    {
		      tree fnname, fndecl;
		      tree *argp = prev
			? & TREE_CHAIN (prev)
			  : & TYPE_ARG_TYPES (type);

		      *argp = NULL_TREE;
		      fnname = build_decl_overload (original_name, TYPE_ARG_TYPES (type), 0);
		      *argp = parmtypes;
		      fndecl = build_decl (FUNCTION_DECL, fnname, type);
		      DECL_EXTERNAL (fndecl) = DECL_EXTERNAL (decl);
		      TREE_PUBLIC (fndecl) = TREE_PUBLIC (decl);
		      DECL_INLINE (fndecl) = DECL_INLINE (decl);
		      /* Keep G++ from thinking this function is unused.
			 It is only used to speed up search in name space.  */
		      TREE_USED (fndecl) = 1;
		      TREE_ASM_WRITTEN (fndecl) = 1;
		      DECL_INITIAL (fndecl) = NULL_TREE;
		      DECL_LANG_SPECIFIC (fndecl) = DECL_LANG_SPECIFIC (decl);
		      fndecl = pushdecl (fndecl);
		      DECL_INITIAL (fndecl) = error_mark_node;
		      DECL_RTL (fndecl) = DECL_RTL (decl);
		    }
		  prev = parmtypes;
		  parmtypes = TREE_CHAIN (parmtypes);
		}
	      DECL_LANG_SPECIFIC (decl) = tmp_lang_decl;
	    }
	}
      else if (DECL_EXTERNAL (decl))
	;
      else if (TREE_STATIC (decl) && type != error_mark_node)
	{
	  /* Cleanups for static variables are handled by `finish_file'.  */
	  if (TYPE_NEEDS_CONSTRUCTING (type) || init != NULL_TREE)
	    expand_static_init (decl, init);
	  else if (TYPE_NEEDS_DESTRUCTOR (type))
	    static_aggregates = perm_tree_cons (NULL_TREE, decl,
						static_aggregates);

	  /* Make entry in appropriate vector.  */
	  if (flag_gc && type_needs_gc_entry (type))
	    build_static_gc_entry (decl, type);
	}
      else if (current_binding_level != global_binding_level)
	{
	  /* This is a declared decl which must live until the
	     end of the binding contour.  It may need a cleanup.  */

	  /* Recompute the RTL of a local array now
	     if it used to be an incomplete type.  */
	  if (was_incomplete && ! TREE_STATIC (decl))
	    {
	      /* If we used it already as memory, it must stay in memory.  */
	      TREE_ADDRESSABLE (decl) = TREE_USED (decl);
	      /* If it's still incomplete now, no init will save it.  */
	      if (DECL_SIZE (decl) == NULL_TREE)
		DECL_INITIAL (decl) = NULL_TREE;
	      expand_decl (decl);
	    }
	  else if (! TREE_ASM_WRITTEN (decl)
		   && (TYPE_SIZE (type) != NULL_TREE
		       || TREE_CODE (type) == ARRAY_TYPE))
	    {
	      /* Do this here, because we did not expand this decl's
		 rtl in start_decl.  */
	      if (DECL_RTL (decl) == NULL_RTX)
		expand_decl (decl);
	      else if (cleanup)
		{
		  expand_decl_cleanup (NULL_TREE, cleanup);
		  /* Cleanup used up here.  */
		  cleanup = NULL_TREE;
		}
	    }

	  if (DECL_SIZE (decl) && type != error_mark_node)
	    {
	      /* Compute and store the initial value.  */
	      expand_decl_init (decl);

	      if (init || TYPE_NEEDS_CONSTRUCTING (type))
		{
		  emit_line_note (DECL_SOURCE_FILE (decl), DECL_SOURCE_LINE (decl));
		  expand_aggr_init (decl, init, 0);
		}

	      /* Set this to 0 so we can tell whether an aggregate
		 which was initialized was ever used.  */
	      if (TYPE_NEEDS_CONSTRUCTING (type))
		TREE_USED (decl) = 0;

	      /* Store the cleanup, if there was one.  */
	      if (cleanup)
		{
		  if (! expand_decl_cleanup (decl, cleanup))
		    error_with_decl (decl, "parser lost in parsing declaration of `%s'");
		}
	    }
	}
    finish_end0:

      /* Undo call to `pushclass' that was done in `start_decl'
	 due to initialization of qualified member variable.
	 I.e., Foo::x = 10;  */
      {
	tree context = DECL_CONTEXT (decl);
	if (context
	    && TREE_CODE_CLASS (TREE_CODE (context)) == 't'
	    && (TREE_CODE (decl) == VAR_DECL
		/* We also have a pushclass done that we need to undo here
		   if we're at top level and declare a method.  */
		|| (TREE_CODE (decl) == FUNCTION_DECL
		    /* If size hasn't been set, we're still defining it,
		       and therefore inside the class body; don't pop
		       the binding level..  */
		    && TYPE_SIZE (context) != NULL_TREE
		    /* The binding level gets popped elsewhere for a
		       friend declaration inside another class.  */
		    && TYPE_IDENTIFIER (context) == current_class_name
		    )))
	  popclass (1);
      }
    }

 finish_end:

  if (need_pop)
    {
      /* Resume permanent allocation, if not within a function.  */
      /* The corresponding push_obstacks_nochange is in start_decl,
	 start_method, groktypename, and in grokfield.  */
      pop_obstacks ();
    }

  if (was_readonly)
    TREE_READONLY (decl) = 1;

  if (flag_cadillac)
    cadillac_finish_decl (decl);
}

static void
expand_static_init (decl, init)
     tree decl;
     tree init;
{
  tree oldstatic = value_member (decl, static_aggregates);
  if (oldstatic)
    {
      if (TREE_PURPOSE (oldstatic))
	error_with_decl (decl, "multiple initializations given for `%s'");
    }
  else if (current_binding_level != global_binding_level)
    {
      /* Emit code to perform this initialization but once.  */
      tree temp;

      /* Remember this information until end of file. */
      push_obstacks (&permanent_obstack, &permanent_obstack);

      /* Emit code to perform this initialization but once.  */
      temp = get_temp_name (integer_type_node, 1);
      rest_of_decl_compilation (temp, NULL_PTR, 0, 0);
      expand_start_cond (build_binary_op (EQ_EXPR, temp,
					  integer_zero_node, 1), 0);
      expand_assignment (temp, integer_one_node, 0, 0);
      if (TYPE_NEEDS_CONSTRUCTING (TREE_TYPE (decl)))
	{
	  expand_aggr_init (decl, init, 0);
	  do_pending_stack_adjust ();
	}
      else
	expand_assignment (decl, init, 0, 0);
      expand_end_cond ();
      if (TYPE_NEEDS_DESTRUCTOR (TREE_TYPE (decl)))
	{
	  static_aggregates = perm_tree_cons (temp, decl, static_aggregates);
	  TREE_STATIC (static_aggregates) = 1;
	}

      /* Resume old (possibly temporary) allocation. */
      pop_obstacks ();
    }
  else
    {
      /* This code takes into account memory allocation
	 policy of `start_decl'.  Namely, if TYPE_NEEDS_CONSTRUCTING
	 does not hold for this object, then we must make permanent
	 the storage currently in the temporary obstack.  */
      if (! TYPE_NEEDS_CONSTRUCTING (TREE_TYPE (decl)))
	preserve_initializer ();
      static_aggregates = perm_tree_cons (init, decl, static_aggregates);
    }
}

/* Make TYPE a complete type based on INITIAL_VALUE.
   Return 0 if successful, 1 if INITIAL_VALUE can't be deciphered,
   2 if there was no information (in which case assume 1 if DO_DEFAULT).  */

int
complete_array_type (type, initial_value, do_default)
     tree type, initial_value;
     int do_default;
{
  register tree maxindex = NULL_TREE;
  int value = 0;

  if (initial_value)
    {
      /* Note MAXINDEX  is really the maximum index,
	 one less than the size.  */
      if (TREE_CODE (initial_value) == STRING_CST)
	maxindex = build_int_2 (TREE_STRING_LENGTH (initial_value) - 1, 0);
      else if (TREE_CODE (initial_value) == CONSTRUCTOR)
	{
	  register int nelts
	    = list_length (CONSTRUCTOR_ELTS (initial_value));
	  maxindex = build_int_2 (nelts - 1, 0);
	}
      else
	{
	  /* Make an error message unless that happened already.  */
	  if (initial_value != error_mark_node)
	    value = 1;

	  /* Prevent further error messages.  */
	  maxindex = build_int_2 (1, 0);
	}
    }

  if (!maxindex)
    {
      if (do_default)
	maxindex = build_int_2 (1, 0);
      value = 2;
    }

  if (maxindex)
    {
      TYPE_DOMAIN (type) = build_index_type (maxindex);
      if (!TREE_TYPE (maxindex))
	TREE_TYPE (maxindex) = TYPE_DOMAIN (type);
    }

  /* Lay out the type now that we can get the real answer.  */

  layout_type (type);

  return value;
}

/* Return zero if something is declared to be a member of type
   CTYPE when in the context of CUR_TYPE.  STRING is the error
   message to print in that case.  Otherwise, quietly return 1.  */
static int
member_function_or_else (ctype, cur_type, string)
     tree ctype, cur_type;
     char *string;
{
  if (ctype && ctype != cur_type)
    {
      error (string, TYPE_NAME_STRING (ctype));
      return 0;
    }
  return 1;
}

/* Subroutine of `grokdeclarator'.  */

/* CTYPE is class type, or null if non-class.
   TYPE is type this FUNCTION_DECL should have, either FUNCTION_TYPE
   or METHOD_TYPE.
   DECLARATOR is the function's name.
   VIRTUALP is truthvalue of whether the function is virtual or not.
   FLAGS are to be passed through to `grokclassfn'.
   QUALS are qualifiers indicating whether the function is `const'
   or `volatile'.
   RAISES is a list of exceptions that this function can raise.
   CHECK is 1 if we must find this method in CTYPE, 0 if we should
   not look, and -1 if we should not call `grokclassfn' at all.  */
static tree
grokfndecl (ctype, type, declarator, virtualp, flags, quals, raises, check, publicp)
     tree ctype, type;
     tree declarator;
     int virtualp;
     enum overload_flags flags;
     tree quals, raises;
     int check, publicp;
{
  tree cname, decl;
  int staticp = ctype && TREE_CODE (type) == FUNCTION_TYPE;

  if (ctype)
    cname = TREE_CODE (TYPE_NAME (ctype)) == TYPE_DECL
      ? TYPE_IDENTIFIER (ctype) : TYPE_NAME (ctype);
  else
    cname = NULL_TREE;

  if (raises)
    {
      type = build_exception_variant (ctype, type, raises);
      raises = TYPE_RAISES_EXCEPTIONS (type);
    }
  decl = build_lang_decl (FUNCTION_DECL, declarator, type);
  /* propagate volatile out from type to decl */
  if (TYPE_VOLATILE (type))
      TREE_THIS_VOLATILE (decl) = 1;

  /* Should probably propagate const out from type to decl I bet (mrs).  */
  if (staticp)
    {
      DECL_STATIC_FUNCTION_P (decl) = 1;
      DECL_CONTEXT (decl) = ctype;
      DECL_CLASS_CONTEXT (decl) = ctype;
    }

  if (publicp)
    TREE_PUBLIC (decl) = 1;

  DECL_EXTERNAL (decl) = 1;
  if (quals != NULL_TREE && TREE_CODE (type) == FUNCTION_TYPE)
    {
      error ("functions cannot have method qualifiers");
      quals = NULL_TREE;
    }

  /* Only two styles of delete's are valid. */
  if (declarator == ansi_opname[(int) DELETE_EXPR])
    {
      tree args = TYPE_ARG_TYPES (type);
      int style1, style2;

      if (ctype && args && TREE_CODE (type) == METHOD_TYPE)
	/* remove this */
	args = TREE_CHAIN (args);
     
      style1 = type_list_equal (args,
				tree_cons (NULL_TREE, ptr_type_node,
					   void_list_node));
      style2 = style1 != 0 ? 0 :
	type_list_equal (args,
			 tree_cons (NULL_TREE, ptr_type_node,
				    tree_cons (NULL_TREE, sizetype,
					       void_list_node)));

      if (ctype == NULL_TREE)
	{
	  if (! style1)
	    /* ANSI C++ June 5 1992 WP 12.5.5.2 */
	    error ("global operator delete must be declared as taking a single argument of type void*");
	}
      else
	if (! style1 && ! style2)
	  /* ANSI C++ June 5 1992 WP 12.5.4.1 */
	  error ("operator delete cannot be overloaded");
    }
  else if (DECL_NAME (decl) == ansi_opname[(int) POSTINCREMENT_EXPR]
	   || DECL_NAME (decl) == ansi_opname[(int) POSTDECREMENT_EXPR])
    {
      /* According to ARM $13.4.7, postfix operator++ must take an int as
	 its second argument.  */
      tree parmtypes, argtypes = TYPE_ARG_TYPES (TREE_TYPE (decl));

      if (argtypes)
	{
	  parmtypes = TREE_CHAIN (argtypes);
	  if (parmtypes != NULL_TREE
	      && TREE_VALUE (parmtypes) != void_type_node
	      && TREE_VALUE (parmtypes) != integer_type_node)
	    error ("postfix operator%s may only take `int' as its argument",
		   POSTINCREMENT_EXPR ? "++" : "--");
	}
    }

  /* Caller will do the rest of this.  */
  if (check < 0)
    return decl;

  if (flags == NO_SPECIAL && ctype && constructor_name (cname) == declarator)
    {
      tree tmp;
      /* Just handle constructors here.  We could do this
	 inside the following if stmt, but I think
	 that the code is more legible by breaking this
	 case out.  See comments below for what each of
	 the following calls is supposed to do.  */
      DECL_CONSTRUCTOR_P (decl) = 1;

      grokclassfn (ctype, declarator, decl, flags, quals);
      if (check)
	check_classfn (ctype, declarator, decl);
      grok_ctor_properties (ctype, decl);
      if (check == 0)
	{
	  /* FIXME: this should only need to look at IDENTIFIER_GLOBAL_VALUE.  */
	  tmp = lookup_name (DECL_ASSEMBLER_NAME (decl), 0);
	  if (tmp == NULL_TREE)
	    IDENTIFIER_GLOBAL_VALUE (DECL_ASSEMBLER_NAME (decl)) = decl;
	  else if (TREE_CODE (tmp) != TREE_CODE (decl))
	    error_with_decl (decl, "inconsistent declarations for `%s'");
	  else
	    {
	      duplicate_decls (decl, tmp);
	      decl = tmp;
	      /* avoid creating circularities.  */
	      DECL_CHAIN (decl) = NULL_TREE;
	    }
	  make_decl_rtl (decl, NULL_PTR, 1);
	}
    }
  else
    {
      tree tmp;

      /* Function gets the ugly name, field gets the nice one.
	 This call may change the type of the function (because
	 of default parameters)!  */
      if (ctype != NULL_TREE)
	grokclassfn (ctype, cname, decl, flags, quals);

      if (IDENTIFIER_OPNAME_P (DECL_NAME (decl)))
	grok_op_properties (decl, virtualp);

      if (ctype != NULL_TREE && check)
	check_classfn (ctype, cname, decl);

      if (ctype == NULL_TREE || check)
	return decl;

      /* Now install the declaration of this function so that
	 others may find it (esp. its DECL_FRIENDLIST).
	 Pretend we are at top level, we will get true
	 reference later, perhaps.

	 FIXME: This should only need to look at IDENTIFIER_GLOBAL_VALUE.  */
      tmp = lookup_name (DECL_ASSEMBLER_NAME (decl), 0);
      if (tmp == NULL_TREE)
	IDENTIFIER_GLOBAL_VALUE (DECL_ASSEMBLER_NAME (decl)) = decl;
      else if (TREE_CODE (tmp) != TREE_CODE (decl))
	error_with_decl (decl, "inconsistent declarations for `%s'");
      else
	{
	  duplicate_decls (decl, tmp);
	  decl = tmp;
	  /* avoid creating circularities.  */
	  DECL_CHAIN (decl) = NULL_TREE;
	}
      make_decl_rtl (decl, NULL_PTR, 1);

      /* If this declaration supersedes the declaration of
	 a method declared virtual in the base class, then
	 mark this field as being virtual as well.  */
      {
	tree binfos = BINFO_BASETYPES (TYPE_BINFO (ctype));
	int i, n_baselinks = binfos ? TREE_VEC_LENGTH (binfos) : 0;

	for (i = 0; i < n_baselinks; i++)
	  {
	    tree base_binfo = TREE_VEC_ELT (binfos, i);
	    if (TYPE_VIRTUAL_P (BINFO_TYPE (base_binfo)) || flag_all_virtual == 1)
	      {
		tmp = get_first_matching_virtual (base_binfo, decl,
						  flags == DTOR_FLAG);
		if (tmp)
		  {
		    /* The TMP we really want is the one from the deepest
		       baseclass on this path, taking care not to
		       duplicate if we have already found it (via another
		       path to its virtual baseclass.  */
		    if (staticp)
		      {
			error_with_decl (decl, "method `%s' may not be declared static");
			error_with_decl (tmp, "(since `%s' declared virtual in base class.)");
			break;
		      }
		    virtualp = 1;

		    if ((TYPE_USES_VIRTUAL_BASECLASSES (BINFO_TYPE (base_binfo))
			 || TYPE_USES_MULTIPLE_INHERITANCE (ctype))
			&& BINFO_TYPE (base_binfo) != DECL_CONTEXT (tmp))
		      tmp = get_first_matching_virtual (TYPE_BINFO (DECL_CONTEXT (tmp)),
							decl, flags == DTOR_FLAG);
		    if (value_member (tmp, DECL_VINDEX (decl)) == NULL_TREE)
		      {
			/* The argument types may have changed... */
			tree argtypes = TYPE_ARG_TYPES (TREE_TYPE (decl));
			tree base_variant = TREE_TYPE (TREE_VALUE (argtypes));

			argtypes = commonparms (TREE_CHAIN (TYPE_ARG_TYPES (TREE_TYPE (tmp))),
						TREE_CHAIN (argtypes));
			/* But the return type has not.  */
			type = build_cplus_method_type (base_variant, TREE_TYPE (type), argtypes);
			if (raises)
			  {
			    type = build_exception_variant (ctype, type, raises);
			    raises = TYPE_RAISES_EXCEPTIONS (type);
			  }
			TREE_TYPE (decl) = type;
			DECL_VINDEX (decl)
			  = tree_cons (NULL_TREE, tmp, DECL_VINDEX (decl));
		      }
		  }
	      }
	  }
      }
      if (virtualp)
	{
	  if (DECL_VINDEX (decl) == NULL_TREE)
	    DECL_VINDEX (decl) = error_mark_node;
	  IDENTIFIER_VIRTUAL_P (DECL_NAME (decl)) = 1;
	  if (ctype && CLASSTYPE_VTABLE_NEEDS_WRITING (ctype)
	      /* If this function is derived from a template, don't
		 make it public.  This shouldn't be here, but there's
		 no good way to override the interface pragmas for one
		 function or class only.  Bletch.  */
	      && IDENTIFIER_TEMPLATE (TYPE_IDENTIFIER (ctype)) == NULL_TREE
	      && (write_virtuals == 2
		  || (write_virtuals == 3
		      && ! CLASSTYPE_INTERFACE_UNKNOWN (ctype))))
	    TREE_PUBLIC (decl) = 1;
	}
    }
  return decl;
}

static tree
grokvardecl (type, declarator, specbits, initialized)
     tree type;
     tree declarator;
     RID_BIT_TYPE specbits;
     int initialized;
{
  tree decl;

  /* This implements the "one definition rule" for global variables.
     Note that declarator can come in as null when we're doing work
     on an anonymous union.  */
  if (declarator && IDENTIFIER_GLOBAL_VALUE (declarator)
      && current_binding_level == global_binding_level
      && TREE_STATIC (IDENTIFIER_GLOBAL_VALUE (declarator))
      && (! (specbits & RIDBIT (RID_EXTERN))
	  || initialized))
    {
      error ("redefinition of `%s'", IDENTIFIER_POINTER (declarator));
      error_with_decl (IDENTIFIER_GLOBAL_VALUE (declarator),
		       "previously defined here");
    }

  if (TREE_CODE (type) == OFFSET_TYPE)
    {
      /* If you declare a static member so that it
	 can be initialized, the code will reach here.  */
      tree field = lookup_field (TYPE_OFFSET_BASETYPE (type),
				 declarator, 0, 0);
      if (field == NULL_TREE || TREE_CODE (field) != VAR_DECL)
	{
	  tree basetype = TYPE_OFFSET_BASETYPE (type);
	  error ("`%s' is not a static member of class `%s'",
		 IDENTIFIER_POINTER (declarator),
		 TYPE_NAME_STRING (basetype));
	  type = TREE_TYPE (type);
	  decl = build_lang_field_decl (VAR_DECL, declarator, type);
	  DECL_CONTEXT (decl) = basetype;
	  DECL_CLASS_CONTEXT (decl) = basetype;
	}
      else
	{
	  tree f_type = TREE_TYPE (field);
	  tree o_type = TREE_TYPE (type);

	  if (TYPE_SIZE (f_type) == NULL_TREE)
	    {
	      if (TREE_CODE (f_type) != TREE_CODE (o_type)
		  || (TREE_CODE (f_type) == ARRAY_TYPE
		      && TREE_TYPE (f_type) != TREE_TYPE (o_type)))
		error ("redeclaration of type for `%s'",
		       IDENTIFIER_POINTER (declarator));
	      else if (TYPE_SIZE (o_type) != NULL_TREE)
		TREE_TYPE (field) = type;
	    }
	  else if (f_type != o_type)
	    error ("redeclaration of type for `%s'",
		   IDENTIFIER_POINTER (declarator));
	  decl = field;
	  if (initialized && DECL_INITIAL (decl)
	      /* Complain about multiply-initialized
		 member variables, but don't be faked
		 out if initializer is faked up from `empty_init_node'.  */
	      && (TREE_CODE (DECL_INITIAL (decl)) != CONSTRUCTOR
		  || CONSTRUCTOR_ELTS (DECL_INITIAL (decl)) != NULL_TREE))
	    error_with_aggr_type (DECL_CONTEXT (decl),
				  "multiple initializations of static member `%s::%s'",
				  IDENTIFIER_POINTER (DECL_NAME (decl)));
	}
    }
  else decl = build_decl (VAR_DECL, declarator, type);

  if (specbits & RIDBIT (RID_EXTERN))
    {
      DECL_THIS_EXTERN (decl) = 1;
      DECL_EXTERNAL (decl) = !initialized;
    }

  /* In class context, static means one per class,
     public visibility, and static storage.  */
  if (DECL_FIELD_CONTEXT (decl) != NULL_TREE
      && IS_AGGR_TYPE (DECL_FIELD_CONTEXT (decl)))
    {
      TREE_PUBLIC (decl) = 1;
      TREE_STATIC (decl) = 1;
      DECL_EXTERNAL (decl) = !initialized;
    }
  /* At top level, either `static' or no s.c. makes a definition
     (perhaps tentative), and absence of `static' makes it public.  */
  else if (current_binding_level == global_binding_level)
    {
      TREE_PUBLIC (decl) = ! (specbits & RIDBIT (RID_STATIC));
      TREE_STATIC (decl) = ! DECL_EXTERNAL (decl);
    }
  /* Not at top level, only `static' makes a static definition.  */
  else
    {
      TREE_STATIC (decl) = !! (specbits & RIDBIT (RID_STATIC));
      TREE_PUBLIC (decl) = DECL_EXTERNAL (decl);
    }
  return decl;
}

/* Given declspecs and a declarator,
   determine the name and type of the object declared
   and construct a ..._DECL node for it.
   (In one case we can return a ..._TYPE node instead.
    For invalid input we sometimes return 0.)

   DECLSPECS is a chain of tree_list nodes whose value fields
    are the storage classes and type specifiers.

   DECL_CONTEXT says which syntactic context this declaration is in:
     NORMAL for most contexts.  Make a VAR_DECL or FUNCTION_DECL or TYPE_DECL.
     FUNCDEF for a function definition.  Like NORMAL but a few different
      error messages in each case.  Return value may be zero meaning
      this definition is too screwy to try to parse.
     MEMFUNCDEF for a function definition.  Like FUNCDEF but prepares to
      handle member functions (which have FIELD context).
      Return value may be zero meaning this definition is too screwy to
      try to parse.
     PARM for a parameter declaration (either within a function prototype
      or before a function body).  Make a PARM_DECL, or return void_type_node.
     TYPENAME if for a typename (in a cast or sizeof).
      Don't make a DECL node; just return the ..._TYPE node.
     FIELD for a struct or union field; make a FIELD_DECL.
     BITFIELD for a field with specified width.
   INITIALIZED is 1 if the decl has an initializer.

   In the TYPENAME case, DECLARATOR is really an absolute declarator.
   It may also be so in the PARM case, for a prototype where the
   argument type is specified but not the name.

   This function is where the complicated C meanings of `static'
   and `extern' are interpreted.

   For C++, if there is any monkey business to do, the function which
   calls this one must do it, i.e., prepending instance variables,
   renaming overloaded function names, etc.

   Note that for this C++, it is an error to define a method within a class
   which does not belong to that class.

   Except in the case where SCOPE_REFs are implicitly known (such as
   methods within a class being redundantly qualified),
   declarations which involve SCOPE_REFs are returned as SCOPE_REFs
   (class_name::decl_name).  The caller must also deal with this.

   If a constructor or destructor is seen, and the context is FIELD,
   then the type gains the attribute TREE_HAS_x.  If such a declaration
   is erroneous, NULL_TREE is returned.

   QUALS is used only for FUNCDEF and MEMFUNCDEF cases.  For a member
   function, these are the qualifiers to give to the `this' pointer.

   May return void_type_node if the declarator turned out to be a friend.
   See grokfield for details.  */

enum return_types { return_normal, return_ctor, return_dtor, return_conversion };

tree
grokdeclarator (declarator, declspecs, decl_context, initialized, raises)
     tree declspecs;
     tree declarator;
     enum decl_context decl_context;
     int initialized;
     tree raises;
{
  extern int current_class_depth;

  RID_BIT_TYPE specbits = 0;
  int nclasses = 0;
  tree spec;
  tree type = NULL_TREE;
  int longlong = 0;
  int constp;
  int volatilep;
  int virtualp, friendp, inlinep, staticp;
  int explicit_int = 0;
  int explicit_char = 0;
  tree typedef_decl = NULL_TREE;
  char *name;
  tree typedef_type = NULL_TREE;
  int funcdef_flag = 0;
  enum tree_code innermost_code = ERROR_MARK;
  int bitfield = 0;
  int size_varies = 0;
  /* Set this to error_mark_node for FIELD_DECLs we could not handle properly.
     All FIELD_DECLs we build here have `init' put into their DECL_INITIAL.  */
  tree init = NULL_TREE;

  /* Keep track of what sort of function is being processed
     so that we can warn about default return values, or explicit
     return values which do not match prescribed defaults.  */
  enum return_types return_type = return_normal;

  tree dname = NULL_TREE;
  tree ctype = current_class_type;
  tree ctor_return_type = NULL_TREE;
  enum overload_flags flags = NO_SPECIAL;
  int seen_scope_ref = 0;
  tree quals = NULL_TREE;

  if (decl_context == FUNCDEF)
    funcdef_flag = 1, decl_context = NORMAL;
  else if (decl_context == MEMFUNCDEF)
    funcdef_flag = -1, decl_context = FIELD;
  else if (decl_context == BITFIELD)
    bitfield = 1, decl_context = FIELD;

  if (flag_traditional && allocation_temporary_p ())
    end_temporary_allocation ();

  /* Look inside a declarator for the name being declared
     and get it as a string, for an error message.  */
  {
    tree type, last = NULL_TREE;
    register tree decl = declarator;
    name = NULL;

    /* If we see something of the form `aggr_type xyzzy (a, b, c)'
       it is either an old-style function declaration or a call to
       a constructor.  The following conditional makes recognizes this
       case as being a call to a constructor.  Too bad if it is not.  */

    /* For Doug Lea, also grok `aggr_type xyzzy (a, b, c)[10][10][10]'.  */
    while (decl && TREE_CODE (decl) == ARRAY_REF)
      {
	last = decl;
	decl = TREE_OPERAND (decl, 0);
      }

    if (decl && declspecs
        && TREE_CODE (decl) == CALL_EXPR
        && TREE_OPERAND (decl, 0)
        && (TREE_CODE (TREE_OPERAND (decl, 0)) == IDENTIFIER_NODE
	    || TREE_CODE (TREE_OPERAND (decl, 0)) == SCOPE_REF))
      {
        type = TREE_CODE (TREE_VALUE (declspecs)) == IDENTIFIER_NODE
          ? lookup_name (TREE_VALUE (declspecs), 1) :
        (IS_AGGR_TYPE (TREE_VALUE (declspecs))
         ? TYPE_NAME (TREE_VALUE (declspecs)) : NULL_TREE);

        if (type && TREE_CODE (type) == TYPE_DECL
            && IS_AGGR_TYPE (TREE_TYPE (type))
            && parmlist_is_exprlist (TREE_OPERAND (decl, 1)))
          {
            if (decl_context == FIELD
                && TREE_CHAIN (TREE_OPERAND (decl, 1)))
              {
                /* That was an initializer list.  */
                sorry ("initializer lists for field declarations");
                decl = TREE_OPERAND (decl, 0);
		if (last)
		  {
		    TREE_OPERAND (last, 0) = decl;
		    decl = declarator;
		  }
                declarator = decl;
                init = error_mark_node;
                goto bot;
              }
            else
              {
		init = TREE_OPERAND (decl, 1);
		if (last)
		  {
		    TREE_OPERAND (last, 0) = TREE_OPERAND (decl, 0);
		    if (pedantic && init)
		      {
		      error ("arrays cannot take initializers");
		      init = error_mark_node;
		    }
		  }
		else
		  declarator = TREE_OPERAND (declarator, 0);
		decl = start_decl (declarator, declspecs, 1, NULL_TREE);
		finish_decl (decl, init, NULL_TREE, 1);
		return 0;
              }
          }

        if (parmlist_is_random (TREE_OPERAND (decl, 1)))
          {
	    decl = TREE_OPERAND (decl, 0);
	    if (TREE_CODE (decl) == SCOPE_REF)
	      {
		if (TREE_COMPLEXITY (decl))
		  my_friendly_abort (15);
		decl = TREE_OPERAND (decl, 1);
	      }
	    if (TREE_CODE (decl) == IDENTIFIER_NODE)
	      name = IDENTIFIER_POINTER (decl);
	    if (name)
	      error ("bad parameter list specification for function `%s'",
		     name);
	    else
	      error ("bad parameter list specification for function");
            return void_type_node;
          }
      bot:
        ;
      }
    else
      /* It didn't look like we thought it would, leave the ARRAY_REFs on.  */
      decl = declarator;

    while (decl)
      switch (TREE_CODE (decl))
        {
	case COND_EXPR:
	  ctype = NULL_TREE;
	  decl = TREE_OPERAND (decl, 0);
	  break;

	case BIT_NOT_EXPR:      /* for C++ destructors!  */
	  {
	    tree name = TREE_OPERAND (decl, 0);
	    tree rename = NULL_TREE;

	    my_friendly_assert (flags == NO_SPECIAL, 152);
	    flags = DTOR_FLAG;
	    return_type = return_dtor;
	    my_friendly_assert (TREE_CODE (name) == IDENTIFIER_NODE, 153);
	    if (ctype == NULL_TREE)
	      {
		if (current_class_type == NULL_TREE)
		  {
		    error ("destructors must be member functions");
		    flags = NO_SPECIAL;
		  }
		else
		  {
		    tree t = constructor_name (current_class_name);
		    if (t != name)
		      rename = t;
		  }
	      }
	    else
	      {
		tree t = constructor_name (ctype);
		if (t != name)
		  rename = t;
	      }

	    if (rename)
	      {
		error ("destructor `%s' must match class name `%s'",
		       IDENTIFIER_POINTER (name),
		       IDENTIFIER_POINTER (rename));
		TREE_OPERAND (decl, 0) = rename;
	      }
	    decl = name;
	  }
	  break;

	case ADDR_EXPR:         /* C++ reference declaration */
	  /* fall through */
	case ARRAY_REF:
	case INDIRECT_REF:
	  ctype = NULL_TREE;
	  innermost_code = TREE_CODE (decl);
	  decl = TREE_OPERAND (decl, 0);
	  break;

	case CALL_EXPR:
	  innermost_code = TREE_CODE (decl);
	  decl = TREE_OPERAND (decl, 0);
	  if (decl_context == FIELD && ctype == NULL_TREE)
	    ctype = current_class_type;
	  if (ctype != NULL_TREE
	      && decl != NULL_TREE && flags != DTOR_FLAG
	      && decl == constructor_name (ctype))
	    {
	      return_type = return_ctor;
	      ctor_return_type = ctype;
	    }
	  ctype = NULL_TREE;
	  break;

	case IDENTIFIER_NODE:
	  dname = decl;
	  name = IDENTIFIER_POINTER (decl);
	  decl = NULL_TREE;
	  break;

	case RECORD_TYPE:
	case UNION_TYPE:
	case ENUMERAL_TYPE:
	  /* Parse error puts this typespec where
	     a declarator should go.  */
	  error ("declarator name missing");
	  dname = TYPE_NAME (decl);
	  if (dname && TREE_CODE (dname) == TYPE_DECL)
	    dname = DECL_NAME (dname);
	  name = dname ? IDENTIFIER_POINTER (dname) : "<nameless>";
	  declspecs = temp_tree_cons (NULL_TREE, decl, declspecs);
	  decl = NULL_TREE;
	  break;

	case TYPE_EXPR:
	  if (ctype == NULL_TREE)
	    {
	      /* ANSI C++ June 5 1992 WP 12.3.2 only describes
		 conversion functions in terms of being declared
		 as a member function.  */
	      error ("operator `%s' must be declared as a member",
		     IDENTIFIER_POINTER (TREE_VALUE (TREE_TYPE (decl))));
	      return NULL_TREE;
	    }
	      
	  ctype = NULL_TREE;
	  my_friendly_assert (flags == NO_SPECIAL, 154);
	  flags = TYPENAME_FLAG;
	  name = "operator <typename>";
	  /* Go to the absdcl.  */
	  decl = TREE_OPERAND (decl, 0);
	  return_type = return_conversion;
	  break;

	  /* C++ extension */
	case SCOPE_REF:
	  if (seen_scope_ref == 1)
	    error ("multiple `::' terms in declarator invalid");
	  seen_scope_ref += 1;
	  {
	    /* Perform error checking, and convert class names to types.
	       We may call grokdeclarator multiple times for the same
	       tree structure, so only do the conversion once.  In this
	       case, we have exactly what we want for `ctype'.  */
	    tree cname = TREE_OPERAND (decl, 0);
	    if (cname == NULL_TREE)
	      ctype = NULL_TREE;
	    /* Can't use IS_AGGR_TYPE because CNAME might not be a type.  */
	    else if (IS_AGGR_TYPE_CODE (TREE_CODE (cname))
		     || TREE_CODE (cname) == UNINSTANTIATED_P_TYPE)
	      ctype = cname;
	    else if (! is_aggr_typedef (cname, 1))
	      {
		TREE_OPERAND (decl, 0) = NULL_TREE;
	      }
	    /* Must test TREE_OPERAND (decl, 1), in case user gives
	       us `typedef (class::memfunc)(int); memfunc *memfuncptr;'  */
	    else if (TREE_OPERAND (decl, 1)
		     && TREE_CODE (TREE_OPERAND (decl, 1)) == INDIRECT_REF)
	      {
		TREE_OPERAND (decl, 0) = IDENTIFIER_TYPE_VALUE (cname);
	      }
	    else if (ctype == NULL_TREE)
	      {
		ctype = IDENTIFIER_TYPE_VALUE (cname);
		TREE_OPERAND (decl, 0) = ctype;
	      }
	    else if (TREE_COMPLEXITY (decl) == current_class_depth)
	      TREE_OPERAND (decl, 0) = ctype;
	    else
	      {
		if (! UNIQUELY_DERIVED_FROM_P (IDENTIFIER_TYPE_VALUE (cname), ctype))
		  {
		    error ("type `%s' is not derived from type `%s'",
			   IDENTIFIER_POINTER (cname),
			   TYPE_NAME_STRING (ctype));
		    TREE_OPERAND (decl, 0) = NULL_TREE;
		  }
		else
		  {
		    ctype = IDENTIFIER_TYPE_VALUE (cname);
		    TREE_OPERAND (decl, 0) = ctype;
		  }
	      }

	    decl = TREE_OPERAND (decl, 1);
	    if (ctype)
	      {
		if (TREE_CODE (decl) == IDENTIFIER_NODE
		    && constructor_name (ctype) == decl)
		  {
		    return_type = return_ctor;
		    ctor_return_type = ctype;
		  }
		else if (TREE_CODE (decl) == BIT_NOT_EXPR
			 && TREE_CODE (TREE_OPERAND (decl, 0)) == IDENTIFIER_NODE
			 && constructor_name (ctype) == TREE_OPERAND (decl, 0))
		  {
		    return_type = return_dtor;
		    ctor_return_type = ctype;
		    flags = DTOR_FLAG;
		    decl = TREE_OPERAND (decl, 0);
		  }
	      }
	  }
	  break;

	case ERROR_MARK:
	  decl = NULL_TREE;
	  break;

	default:
	  my_friendly_abort (155);
	}
    if (name == NULL)
      name = "type name";
  }

  /* A function definition's declarator must have the form of
     a function declarator.  */

  if (funcdef_flag && innermost_code != CALL_EXPR)
    return 0;

  /* Anything declared one level down from the top level
     must be one of the parameters of a function
     (because the body is at least two levels down).  */

  /* This heuristic cannot be applied to C++ nodes! Fixed, however,
     by not allowing C++ class definitions to specify their parameters
     with xdecls (must be spec.d in the parmlist).

     Since we now wait to push a class scope until we are sure that
     we are in a legitimate method context, we must set oldcname
     explicitly (since current_class_name is not yet alive).  */

  if (decl_context == NORMAL
      && current_binding_level->level_chain == global_binding_level)
    decl_context = PARM;

  /* Look through the decl specs and record which ones appear.
     Some typespecs are defined as built-in typenames.
     Others, the ones that are modifiers of other types,
     are represented by bits in SPECBITS: set the bits for
     the modifiers that appear.  Storage class keywords are also in SPECBITS.

     If there is a typedef name or a type, store the type in TYPE.
     This includes builtin typedefs such as `int'.

     Set EXPLICIT_INT if the type is `int' or `char' and did not
     come from a user typedef.

     Set LONGLONG if `long' is mentioned twice.

     For C++, constructors and destructors have their own fast treatment.  */

  for (spec = declspecs; spec; spec = TREE_CHAIN (spec))
    {
      register int i;
      register tree id = TREE_VALUE (spec);

      /* Certain parse errors slip through.  For example,
	 `int class;' is not caught by the parser. Try
	 weakly to recover here.  */
      if (TREE_CODE (spec) != TREE_LIST)
	return 0;

      if (TREE_CODE (id) == IDENTIFIER_NODE)
	{
	  if (id == ridpointers[(int) RID_INT])
	    {
	      if (type)
		error ("extraneous `int' ignored");
	      else
		{
		  explicit_int = 1;
		  type = TREE_TYPE (IDENTIFIER_GLOBAL_VALUE (id));
		}
	      goto found;
	    }
	  if (id == ridpointers[(int) RID_CHAR])
	    {
	      if (type)
		error ("extraneous `char' ignored");
	      else
		{
		  explicit_char = 1;
		  type = TREE_TYPE (IDENTIFIER_GLOBAL_VALUE (id));
		}
	      goto found;
	    }
	  if (id == ridpointers[(int) RID_WCHAR])
	    {
	      if (type)
		error ("extraneous `__wchar_t' ignored");
	      else
		{
		  type = TREE_TYPE (IDENTIFIER_GLOBAL_VALUE (id));
		}
	      goto found;
	    }
	  /* C++ aggregate types.  */
	  if (IDENTIFIER_HAS_TYPE_VALUE (id))
	    {
	      if (type)
		error ("multiple declarations `%s' and `%s'",
		       IDENTIFIER_POINTER (type),
		       IDENTIFIER_POINTER (id));
	      else
		type = IDENTIFIER_TYPE_VALUE (id);
	      goto found;
	    }

	  for (i = (int) RID_FIRST_MODIFIER; i < (int) RID_MAX; i++)
	    {
	      if (ridpointers[i] == id)
		{
		  if (i == (int) RID_LONG && (specbits & RIDBIT (i)))
		    {
		      if (pedantic)
			pedwarn ("duplicate `%s'", IDENTIFIER_POINTER (id));
		      else if (longlong)
		        error ("`long long long' is too long for GCC");
		      else
			longlong = 1;
		    }
		  else if (specbits & RIDBIT (i))
		    warning ("duplicate `%s'", IDENTIFIER_POINTER (id));
		  specbits |= RIDBIT (i);
		  goto found;
		}
	    }
	}
      if (type)
	error ("two or more data types in declaration of `%s'", name);
      else if (TREE_CODE (id) == IDENTIFIER_NODE)
	{
	  register tree t = lookup_name (id, 1);
	  if (!t || TREE_CODE (t) != TYPE_DECL)
	    error ("`%s' fails to be a typedef or built in type",
		   IDENTIFIER_POINTER (id));
	  else
	    {
	      type = TREE_TYPE (t);
	      typedef_decl = t;
	    }
	}
      else if (TREE_CODE (id) != ERROR_MARK)
	/* Can't change CLASS nodes into RECORD nodes here!  */
	type = id;

    found: {}
    }

  typedef_type = type;

  /* No type at all: default to `int', and set EXPLICIT_INT
     because it was not a user-defined typedef.  */

  if (type == NULL_TREE)
    {
      explicit_int = -1;
      if (return_type == return_dtor)
	type = void_type_node;
      else if (return_type == return_ctor)
	type = TYPE_POINTER_TO (ctor_return_type);
      else
	{
	  if (funcdef_flag && explicit_warn_return_type
	      && return_type == return_normal
	      && ! (specbits & (RIDBIT (RID_SIGNED) | RIDBIT (RID_UNSIGNED)
				| RIDBIT (RID_LONG) | RIDBIT (RID_SHORT))))
	    warn_about_return_type = 1;
	  /* Save warning until we know what is really going on.  */
	  type = integer_type_node;
	}
    }
  else if (return_type == return_dtor)
    {
      error ("return type specification for destructor invalid");
      type = void_type_node;
    }
  else if (return_type == return_ctor)
    {
      error ("return type specification for constructor invalid");
      type = TYPE_POINTER_TO (ctor_return_type);
    }
  else if ((specbits & RIDBIT (RID_FRIEND))
	   && IS_AGGR_TYPE (type)
	   && ! TYPE_BEING_DEFINED (type)
	   && TYPE_SIZE (type) == NULL_TREE
	   && ! ANON_AGGRNAME_P (TYPE_IDENTIFIER (type))
	   && current_function_decl == NULL_TREE
	   && decl_context != PARM)
    {
      /* xref_tag will make friend class declarations look like
	 nested class declarations.  Retroactively change that
	 if the type has not yet been defined.

	 ??? ANSI C++ doesn't say what to do in this case yet.  */
      globalize_nested_type (type);
    }

  ctype = NULL_TREE;

  /* Now process the modifiers that were specified
     and check for invalid combinations.  */

  /* Long double is a special combination.  */

  if ((specbits & RIDBIT (RID_LONG))
      && TYPE_MAIN_VARIANT (type) == double_type_node)
    {
      specbits &= ~ RIDBIT (RID_LONG);
      type = build_type_variant (long_double_type_node, TYPE_READONLY (type),
				 TYPE_VOLATILE (type));
    }

  /* Check all other uses of type modifiers.  */

  if (specbits & (RIDBIT (RID_UNSIGNED) | RIDBIT (RID_SIGNED)
		  | RIDBIT (RID_LONG) | RIDBIT (RID_SHORT)))
    {
      int ok = 0;

      if (TREE_CODE (type) == REAL_TYPE)
	error ("short, signed or unsigned invalid for `%s'", name);
      else if (TREE_CODE (type) != INTEGER_TYPE || type == wchar_type_node)
	error ("long, short, signed or unsigned invalid for `%s'", name);
      else if ((specbits & RIDBIT (RID_LONG))
	       && (specbits & RIDBIT (RID_SHORT)))
	error ("long and short specified together for `%s'", name);
      else if (((specbits & RIDBIT (RID_LONG))
		|| (specbits & RIDBIT (RID_SHORT)))
	       && explicit_char)
	error ("long or short specified with char for `%s'", name);
      else if (((specbits & RIDBIT (RID_LONG))
		|| (specbits & RIDBIT (RID_SHORT)))
	       && TREE_CODE (type) == REAL_TYPE)
	error ("long or short specified with floating type for `%s'", name);
      else if ((specbits & RIDBIT (RID_SIGNED))
	       && (specbits & RIDBIT (RID_UNSIGNED)))
	error ("signed and unsigned given together for `%s'", name);
      else
	{
	  ok = 1;
	  if (!explicit_int && !explicit_char && pedantic)
	    {
	      pedwarn ("long, short, signed or unsigned used invalidly for `%s'",
		       name);
	      if (flag_pedantic_errors)
		ok = 0;
	    }
	}

      /* Discard the type modifiers if they are invalid.  */
      if (! ok)
	{
	  specbits &= ~ (RIDBIT (RID_UNSIGNED) | RIDBIT (RID_SIGNED)
			 | RIDBIT (RID_LONG) | RIDBIT (RID_SHORT));
	  longlong = 0;
	}
    }

  /* Decide whether an integer type is signed or not.
     Optionally treat bitfields as signed by default.  */
  if ((specbits & RIDBIT (RID_UNSIGNED))
      /* Traditionally, all bitfields are unsigned.  */
      || (bitfield && flag_traditional)
      || (bitfield && ! flag_signed_bitfields
	  && (explicit_int || explicit_char
	      /* A typedef for plain `int' without `signed'
		 can be controlled just like plain `int'.  */
	      || ! (typedef_decl != NULL_TREE
		    && C_TYPEDEF_EXPLICITLY_SIGNED (typedef_decl)))
	  && TREE_CODE (type) != ENUMERAL_TYPE
	  && !(specbits & RIDBIT (RID_SIGNED))))
    {
      if (longlong)
	type = long_long_unsigned_type_node;
      else if (specbits & RIDBIT (RID_LONG))
	type = long_unsigned_type_node;
      else if (specbits & RIDBIT (RID_SHORT))
	type = short_unsigned_type_node;
      else if (type == char_type_node)
	type = unsigned_char_type_node;
      else if (typedef_decl)
	type = unsigned_type (type);
      else
	type = unsigned_type_node;
    }
  else if ((specbits & RIDBIT (RID_SIGNED))
	   && type == char_type_node)
    type = signed_char_type_node;
  else if (longlong)
    type = long_long_integer_type_node;
  else if (specbits & RIDBIT (RID_LONG))
    type = long_integer_type_node;
  else if (specbits & RIDBIT (RID_SHORT))
    type = short_integer_type_node;

  /* Set CONSTP if this declaration is `const', whether by
     explicit specification or via a typedef.
     Likewise for VOLATILEP.  */

  constp = !! (specbits & RIDBIT (RID_CONST)) + TYPE_READONLY (type);
  volatilep = !! (specbits & RIDBIT (RID_VOLATILE)) + TYPE_VOLATILE (type);
  staticp = 0;
  inlinep = !! (specbits & RIDBIT (RID_INLINE));
  if (constp > 1)
    warning ("duplicate `const'");
  if (volatilep > 1)
    warning ("duplicate `volatile'");
  virtualp = specbits & RIDBIT (RID_VIRTUAL);
  if (specbits & RIDBIT (RID_STATIC))
    staticp = 1 + (decl_context == FIELD);

  if (virtualp && staticp == 2)
    {
      error ("member `%s' cannot be declared both virtual and static", name);
      staticp = 0;
    }
  friendp = specbits & RIDBIT (RID_FRIEND);
  specbits &= ~ (RIDBIT (RID_VIRTUAL) | RIDBIT (RID_FRIEND));

  /* Warn if two storage classes are given. Default to `auto'.  */

  if (specbits)
    {
      if (specbits & RIDBIT (RID_STATIC)) nclasses++;
      if (specbits & RIDBIT (RID_EXTERN)) nclasses++;
      if (decl_context == PARM && nclasses > 0)
	error ("storage class specifiers invalid in parameter declarations");
      if (specbits & RIDBIT (RID_TYPEDEF))
	{
	  if (decl_context == PARM)
	    error ("typedef declaration invalid in parameter declaration");
	  nclasses++;
	}
      if (specbits & RIDBIT (RID_AUTO)) nclasses++;
      if (specbits & RIDBIT (RID_REGISTER)) nclasses++;
    }

  /* Give error if `virtual' is used outside of class declaration.  */
  if (virtualp && current_class_name == NULL_TREE)
    {
      error ("virtual outside class declaration");
      virtualp = 0;
    }

  /* Warn about storage classes that are invalid for certain
     kinds of declarations (parameters, typenames, etc.).  */

  if (nclasses > 1)
    error ("multiple storage classes in declaration of `%s'", name);
  else if (decl_context != NORMAL && nclasses > 0)
    {
      if (decl_context == PARM
	  && ((specbits & RIDBIT (RID_REGISTER)) | RIDBIT (RID_AUTO)))
	;
      else if ((decl_context == FIELD
		|| decl_context == TYPENAME)
	       && (specbits & RIDBIT (RID_TYPEDEF)))
	{
	  /* A typedef which was made in a class's scope.  */
	  tree loc_typedecl;
	  register int i = sizeof (struct lang_decl_flags) / sizeof (int);
	  register int *pi;
	  struct binding_level *local_binding_level;

	  /* keep `grokdeclarator' from thinking we are in PARM context.  */
	  pushlevel (0);
  	  /* poplevel_class may be called by grokdeclarator which is called in
  	     start_decl which is called below. In this case, our pushed level
  	     may vanish and poplevel mustn't be called. So remember what we
  	     have pushed and pop only if that is matched by 
  	     current_binding_level later. mnl@dtro.e-technik.th-darmstadt.de */
  	  local_binding_level = current_binding_level;

	  loc_typedecl = start_decl (declarator, declspecs, initialized, NULL_TREE);

	  pi = (int *) permalloc (sizeof (struct lang_decl_flags));
	  while (i > 0)
	    pi[--i] = 0;
	  DECL_LANG_SPECIFIC (loc_typedecl) = (struct lang_decl *) pi;
	  /* This poplevel conflicts with the popclass over in
	     grokdeclarator.  See ``This popclass conflicts'' */
	  if (current_binding_level == local_binding_level)
	    poplevel (0, 0, 0);

#if 0
	  if (TREE_CODE (TREE_TYPE (loc_typedecl)) == ENUMERAL_TYPE)
	    {
	      tree ref = lookup_tag (ENUMERAL_TYPE, DECL_NAME (loc_typedecl), current_binding_level, 0);
	      if (! ref)
		pushtag (DECL_NAME (loc_typedecl), TREE_TYPE (loc_typedecl));
	    }
#endif

	  /* We used to check for a typedef hiding a previous decl in
	     class scope, but delete_duplicate_fields_1 will now do
	     that for us in the proper place.  */

	  /* We reset loc_typedecl because the IDENTIFIER_CLASS_NAME is
	     set by pushdecl_class_level.  */
	  loc_typedecl = pushdecl_class_level (loc_typedecl);

	  return loc_typedecl;
	}
      else if (decl_context == FIELD
 	       /* C++ allows static class elements  */
 	       && (specbits & RIDBIT (RID_STATIC)))
 	/* C++ also allows inlines and signed and unsigned elements,
 	   but in those cases we don't come in here.  */
	;
      else
	{
	  if (decl_context == FIELD)
	    {
	      tree tmp = TREE_OPERAND (declarator, 0);
	      register int op = IDENTIFIER_OPNAME_P (tmp);
	      error ("storage class specified for %s `%s'",
		     op ? "member operator" : "structure field",
		     op ? operator_name_string (tmp) : name);
	    }
	  else
	    error ((decl_context == PARM
		    ? "storage class specified for parameter `%s'"
		    : "storage class specified for typename"), name);
	  specbits &= ~ (RIDBIT (RID_REGISTER) | RIDBIT (RID_AUTO)
			 | RIDBIT (RID_EXTERN));
	}
    }
  else if ((specbits & RIDBIT (RID_EXTERN)) && initialized && !funcdef_flag)
    {
      if (current_binding_level == global_binding_level)
	{
	  /* It's common practice (and completely legal) to have a const
	     be initialized and declared extern.  */
	  if (! constp)
	    warning ("`%s' initialized and declared `extern'", name);
	}
      else
	error ("`%s' has both `extern' and initializer", name);
    }
  else if ((specbits & RIDBIT (RID_EXTERN)) && funcdef_flag
	   && current_binding_level != global_binding_level)
    error ("nested function `%s' declared `extern'", name);
  else if (current_binding_level == global_binding_level)
    {
      if (specbits & RIDBIT (RID_AUTO))
	error ("top-level declaration of `%s' specifies `auto'", name);
#if 0
      if (specbits & RIDBIT (RID_REGISTER))
	error ("top-level declaration of `%s' specifies `register'", name);
#endif
#if 0
      /* I'm not sure under what circumstances we should turn
	 on the extern bit, and under what circumstances we should
	 warn if other bits are turned on.  */
      if (decl_context == NORMAL
	  && ! (specbits & RIDBIT (RID_EXTERN))
	  && ! root_lang_context_p ())
	{
	  specbits |= RIDBIT (RID_EXTERN);
	}
#endif
    }

  /* Now figure out the structure of the declarator proper.
     Descend through it, creating more complex types, until we reach
     the declared identifier (or NULL_TREE, in an absolute declarator).  */

  while (declarator && TREE_CODE (declarator) != IDENTIFIER_NODE)
    {
      /* Each level of DECLARATOR is either an ARRAY_REF (for ...[..]),
	 an INDIRECT_REF (for *...),
	 a CALL_EXPR (for ...(...)),
	 an identifier (for the name being declared)
	 or a null pointer (for the place in an absolute declarator
	 where the name was omitted).
	 For the last two cases, we have just exited the loop.

	 For C++ it could also be
	 a SCOPE_REF (for class :: ...).  In this case, we have converted
	 sensible names to types, and those are the values we use to
	 qualify the member name.
	 an ADDR_EXPR (for &...),
	 a BIT_NOT_EXPR (for destructors)
	 a TYPE_EXPR (for operator typenames)

	 At this point, TYPE is the type of elements of an array,
	 or for a function to return, or for a pointer to point to.
	 After this sequence of ifs, TYPE is the type of the
	 array or function or pointer, and DECLARATOR has had its
	 outermost layer removed.  */

      if (TREE_CODE (type) == ERROR_MARK)
	{
	  if (TREE_CODE (declarator) == SCOPE_REF)
	    declarator = TREE_OPERAND (declarator, 1);
	  else
	    declarator = TREE_OPERAND (declarator, 0);
	  continue;
	}
      if (quals != NULL_TREE
	  && (declarator == NULL_TREE
	      || TREE_CODE (declarator) != SCOPE_REF))
	{
	  if (ctype == NULL_TREE && TREE_CODE (type) == METHOD_TYPE)
	    ctype = TYPE_METHOD_BASETYPE (type);
	  if (ctype != NULL_TREE)
	    {
#if 0 /* not yet, should get fixed properly later */
	      tree dummy = make_type_decl (NULL_TREE, type);
#else
	      tree dummy = build_decl (TYPE_DECL, NULL_TREE, type);
#endif
	      ctype = grok_method_quals (ctype, dummy, quals);
	      type = TREE_TYPE (dummy);
	      quals = NULL_TREE;
	    }
	}
      switch (TREE_CODE (declarator))
	{
	case ARRAY_REF:
	  maybe_globalize_type (type);
	  {
	    register tree itype = NULL_TREE;
	    register tree size = TREE_OPERAND (declarator, 1);

	    declarator = TREE_OPERAND (declarator, 0);

	    /* Check for some types that there cannot be arrays of.  */

	    if (TYPE_MAIN_VARIANT (type) == void_type_node)
	      {
		error ("declaration of `%s' as array of voids", name);
		type = error_mark_node;
	      }

	    if (TREE_CODE (type) == FUNCTION_TYPE)
	      {
		error ("declaration of `%s' as array of functions", name);
		type = error_mark_node;
	      }

	    /* ARM $8.4.3: Since you can't have a pointer to a reference,
	       you can't have arrays of references.  If we allowed them,
	       then we'd be saying x[i] is legal for an array x, but
	       then you'd have to ask: what does `*(x + i)' mean?  */
	    if (TREE_CODE (type) == REFERENCE_TYPE)
	      error ("declaration of `%s' as array of references", name);

	    if (size == error_mark_node)
	      type = error_mark_node;

	    if (type == error_mark_node)
	      continue;

	    if (size)
	      {
		/* Must suspend_momentary here because the index
		   type may need to live until the end of the function.
		   For example, it is used in the declaration of a
		   variable which requires destructing at the end of
		   the function; then build_vec_delete will need this
		   value.  */
		int yes = suspend_momentary ();
		/* might be a cast */
		if (TREE_CODE (size) == NOP_EXPR
		    && TREE_TYPE (size) == TREE_TYPE (TREE_OPERAND (size, 0)))
		  size = TREE_OPERAND (size, 0);

		/* If this is a template parameter, it'll be constant, but
		   we don't know what the value is yet.  */
		if (TREE_CODE (size) == TEMPLATE_CONST_PARM)
		  goto dont_grok_size;

		if (TREE_CODE (TREE_TYPE (size)) != INTEGER_TYPE
		    && TREE_CODE (TREE_TYPE (size)) != ENUMERAL_TYPE)
		  {
		    error ("size of array `%s' has non-integer type", name);
		    size = integer_one_node;
		  }
		if (TREE_READONLY_DECL_P (size))
		  size = decl_constant_value (size);
		if (pedantic && integer_zerop (size))
		  pedwarn ("ANSI C++ forbids zero-size array `%s'", name);
		if (TREE_CONSTANT (size))
		  {
		    if (INT_CST_LT (size, integer_zero_node))
		      {
			error ("size of array `%s' is negative", name);
			size = integer_one_node;
		      }
		    itype = build_index_type (size_binop (MINUS_EXPR, size,
							  integer_one_node));
		  }
		else
		  {
		    if (pedantic)
		      pedwarn ("ANSI C++ forbids variable-size array `%s'", name);
		  dont_grok_size:
		    itype =
		      build_binary_op (MINUS_EXPR, size, integer_one_node, 1);
		    /* Make sure the array size remains visibly nonconstant
		       even if it is (eg) a const variable with known value.  */
		    size_varies = 1;
		    itype = variable_size (itype);
		    itype = build_index_type (itype);
		  }
		resume_momentary (yes);
	      }

	    /* Build the array type itself.
	       Merge any constancy or volatility into the target type.  */

	    if (constp || volatilep)
	      type = build_type_variant (type, constp, volatilep);

	    type = build_cplus_array_type (type, itype);
	    ctype = NULL_TREE;
	  }
	  break;

	case CALL_EXPR:
	  maybe_globalize_type (type);
	  {
	    tree arg_types;

	    /* Declaring a function type.
	       Make sure we have a valid type for the function to return.  */
#if 0
	    /* Is this an error?  Should they be merged into TYPE here?  */
	    if (pedantic && (constp || volatilep))
	      pedwarn ("function declared to return const or volatile result");
#else
	    /* Merge any constancy or volatility into the target type
	       for the pointer.  */

	    if (constp || volatilep)
	      {
		type = build_type_variant (type, constp, volatilep);
		if (IS_AGGR_TYPE (type))
		  build_pointer_type (type);
		constp = 0;
		volatilep = 0;
	      }
#endif

	    /* Warn about some types functions can't return.  */

	    if (TREE_CODE (type) == FUNCTION_TYPE)
	      {
		error ("`%s' declared as function returning a function", name);
		type = integer_type_node;
	      }
	    if (TREE_CODE (type) == ARRAY_TYPE)
	      {
		error ("`%s' declared as function returning an array", name);
		type = integer_type_node;
	      }

	    if (ctype == NULL_TREE
		&& decl_context == FIELD
		&& (friendp == 0 || dname == current_class_name))
	      ctype = current_class_type;

	    if (ctype && flags == TYPENAME_FLAG)
	      TYPE_HAS_CONVERSION (ctype) = 1;
	    if (ctype && constructor_name (ctype) == dname)
	      {
		/* We are within a class's scope. If our declarator name
		   is the same as the class name, and we are defining
		   a function, then it is a constructor/destructor, and
		   therefore returns a void type.  */

		if (flags == DTOR_FLAG)
		  {
		    /* ANSI C++ June 5 1992 WP 12.4.1.  A destructor may
		       not be declared const or volatile.  A destructor
		       may not be static.  */
		    if (staticp == 2)
		      error ("destructor cannot be static member function");
		    if (TYPE_READONLY (type))
		      {
			error ("destructors cannot be declared `const'");
			return void_type_node;
		      }
		    if (TYPE_VOLATILE (type))
		      {
			error ("destructors cannot be declared `volatile'");
			return void_type_node;
		      }
		    if (decl_context == FIELD)
		      {
			if (! member_function_or_else (ctype, current_class_type,
						       "destructor for alien class `%s' cannot be a member"))
			  return void_type_node;
		      }
		  }
		else            /* it's a constructor. */
		  {
		    /* ANSI C++ June 5 1992 WP 12.1.2.  A constructor may
		       not be declared const or volatile.  A constructor may
		       not be virtual.  A constructor may not be static.  */
		    if (staticp == 2)
		      error ("constructor cannot be static member function");
		    if (virtualp)
		      {
			pedwarn ("constructors cannot be declared virtual");
			virtualp = 0;
		      }
		    if (TYPE_READONLY (type))
		      {
			error ("constructors cannot be declared `const'");
			return void_type_node;
 		      }
		    if (TYPE_VOLATILE (type))
		      {
			error ("constructors cannot be declared `volatile'");
			return void_type_node;
		      }
		    if (specbits & ~(RIDBIT (RID_INLINE)|RIDBIT (RID_STATIC)))
		      error ("return value type specifier for constructor ignored");
		    type = TYPE_POINTER_TO (ctype);
		    if (decl_context == FIELD)
		      {
			if (! member_function_or_else (ctype, current_class_type,
						       "constructor for alien class `%s' cannot be member"))
			  return void_type_node;
			TYPE_HAS_CONSTRUCTOR (ctype) = 1;
			if (return_type != return_ctor)
			  return NULL_TREE;
		      }
		  }
		if (decl_context == FIELD)
		  staticp = 0;
	      }
	    else if (friendp && virtualp)
	      {
		/* Cannot be both friend and virtual.  */
		error ("virtual functions cannot be friends");
		specbits &= ~ RIDBIT (RID_FRIEND);
	      }

	    if (decl_context == NORMAL && friendp)
	      error ("friend declaration not in class definition");

	    /* Pick up type qualifiers which should be applied to `this'.  */
	    quals = TREE_OPERAND (declarator, 2);

	    /* Traditionally, declaring return type float means double.  */

	    if (flag_traditional
		&& TYPE_MAIN_VARIANT (type) == float_type_node)
	      {
		type = build_type_variant (double_type_node,
					   TYPE_READONLY (type),
					   TYPE_VOLATILE (type));
	      }

	    /* Construct the function type and go to the next
	       inner layer of declarator.  */

	    {
	      int funcdef_p;
	      tree inner_parms = TREE_OPERAND (declarator, 1);
	      tree inner_decl = TREE_OPERAND (declarator, 0);

	      declarator = TREE_OPERAND (declarator, 0);

	      if (inner_decl && TREE_CODE (inner_decl) == SCOPE_REF)
		inner_decl = TREE_OPERAND (inner_decl, 1);

	      /* Say it's a definition only for the CALL_EXPR
		 closest to the identifier.  */
	      funcdef_p =
		(inner_decl &&
		 (TREE_CODE (inner_decl) == IDENTIFIER_NODE
		  || TREE_CODE (inner_decl) == TYPE_EXPR)) ? funcdef_flag : 0;

	      arg_types = grokparms (inner_parms, funcdef_p);
	    }

	    if (declarator)
	      {
		/* Get past destructors, etc.
		   We know we have one because FLAGS will be non-zero.

		   Complain about improper parameter lists here.  */
		if (TREE_CODE (declarator) == BIT_NOT_EXPR)
		  {
		    declarator = TREE_OPERAND (declarator, 0);

		    if (strict_prototype == 0 && arg_types == NULL_TREE)
		      arg_types = void_list_node;
		    else if (arg_types == NULL_TREE
			     || arg_types != void_list_node)
		      {
			error ("destructors cannot be specified with parameters");
			arg_types = void_list_node;
		      }
		  }
	      }
	    /* the top level const or volatile is attached semantically only
	       to the function not the actual type. */
	    if (TYPE_READONLY (type) || TYPE_VOLATILE (type))
	      {
		int constp = TYPE_READONLY (type);
		int volatilep = TYPE_VOLATILE (type);
		type = build_function_type (TYPE_MAIN_VARIANT (type),
					    flag_traditional
					    ? 0
					    : arg_types);
		type = build_type_variant (type, constp, volatilep);
	      }
	    else
	      type = build_function_type (type,
					  flag_traditional
					  ? 0
					  : arg_types);
	  }
	  break;

	case ADDR_EXPR:
	case INDIRECT_REF:
	  maybe_globalize_type (type);

	  /* Filter out pointers-to-references and references-to-references.
	     We can get these if a TYPE_DECL is used.  */

	  if (TREE_CODE (type) == REFERENCE_TYPE)
	    {
	      error ("cannot declare %s to references",
		     TREE_CODE (declarator) == ADDR_EXPR
		     ? "references" : "pointers");
	      declarator = TREE_OPERAND (declarator, 0);
	      continue;
	    }

	  /* Merge any constancy or volatility into the target type
	     for the pointer.  */

	  if (constp || volatilep)
	    {
	      type = build_type_variant (type, constp, volatilep);
	      if (IS_AGGR_TYPE (type))
		build_pointer_type (type);
	      constp = 0;
	      volatilep = 0;
	    }

	  if (TREE_CODE (declarator) == ADDR_EXPR)
	    {
	      if (TREE_CODE (type) == FUNCTION_TYPE)
		{
		  error ("cannot declare references to functions; use pointer to function instead");
		  type = build_pointer_type (type);
		}
	      else
		{
		  if (TYPE_MAIN_VARIANT (type) == void_type_node)
		    error ("invalid type: `void &'");
		  else
		    type = build_reference_type (type);
		}
	    }
	  else
	    type = build_pointer_type (type);

	  /* Process a list of type modifier keywords (such as
	     const or volatile) that were given inside the `*' or `&'.  */

	  if (TREE_TYPE (declarator))
	    {
	      register tree typemodlist;
	      int erred = 0;
	      for (typemodlist = TREE_TYPE (declarator); typemodlist;
		   typemodlist = TREE_CHAIN (typemodlist))
		{
		  if (TREE_VALUE (typemodlist) == ridpointers[(int) RID_CONST])
		    constp++;
		  else if (TREE_VALUE (typemodlist) == ridpointers[(int) RID_VOLATILE])
		    volatilep++;
		  else if (!erred)
		    {
		      erred = 1;
		      error ("invalid type modifier within %s declarator",
			     TREE_CODE (declarator) == ADDR_EXPR
			     ? "reference" : "pointer");
		    }
		}
	      if (constp > 1)
		warning ("duplicate `const'");
	      if (volatilep > 1)
		warning ("duplicate `volatile'");
	    }
	  declarator = TREE_OPERAND (declarator, 0);
	  ctype = NULL_TREE;
	  break;

	case SCOPE_REF:
	  {
	    /* We have converted type names to NULL_TREE if the
	       name was bogus, or to a _TYPE node, if not.

	       The variable CTYPE holds the type we will ultimately
	       resolve to.  The code here just needs to build
	       up appropriate member types.  */
	    tree sname = TREE_OPERAND (declarator, 1);
	    /* Destructors can have their visibilities changed as well.  */
	    if (TREE_CODE (sname) == BIT_NOT_EXPR)
	      sname = TREE_OPERAND (sname, 0);

	    if (TREE_COMPLEXITY (declarator) == 0)
	      /* This needs to be here, in case we are called
		 multiple times.  */ ;
	    else if (friendp && (TREE_COMPLEXITY (declarator) < 2))
	      /* don't fall out into global scope. Hides real bug? --eichin */ ;
	    else if (TREE_COMPLEXITY (declarator) == current_class_depth)
	      {
		TREE_COMPLEXITY (declarator) -= 1;
		/* This popclass conflicts with the poplevel over in
		   grokdeclarator.  See ``This poplevel conflicts'' */
		popclass (1);
	      }
	    else
	      my_friendly_abort (16);

	    if (TREE_OPERAND (declarator, 0) == NULL_TREE)
	      {
		/* We had a reference to a global decl, or
		   perhaps we were given a non-aggregate typedef,
		   in which case we cleared this out, and should just
		   keep going as though it wasn't there.  */
		declarator = sname;
		continue;
	      }
	    ctype = TREE_OPERAND (declarator, 0);

	    if (sname == NULL_TREE)
	      goto done_scoping;

	    if (TREE_CODE (sname) == IDENTIFIER_NODE)
	      {
		/* This is the `standard' use of the scoping operator:
		   basetype :: member .  */

		if (TYPE_MAIN_VARIANT (ctype) == current_class_type || friendp)
		  {
		    if (TREE_CODE (type) == FUNCTION_TYPE)
		      type = build_cplus_method_type (build_type_variant (ctype, constp, volatilep),
						      TREE_TYPE (type), TYPE_ARG_TYPES (type));
		    else
		      {
			if (TYPE_MAIN_VARIANT (ctype) != current_class_type)
			  {
			    error ("cannot declare member `%s::%s' within this class",
				   TYPE_NAME_STRING (ctype), name);
			    return void_type_node;
			  }
			else if (extra_warnings)
			  warning ("extra qualification `%s' on member `%s' ignored",
				   TYPE_NAME_STRING (ctype), name);
			type = build_offset_type (ctype, type);
		      }
		  }
		else if (TYPE_SIZE (ctype) != NULL_TREE
			 || (specbits & RIDBIT (RID_TYPEDEF)))
		  {
		    tree t;
		    /* have to move this code elsewhere in this function.
		       this code is used for i.e., typedef int A::M; M *pm; */

		    if (explicit_int == -1 && decl_context == FIELD
			&& funcdef_flag == 0)
		      {
			/* The code in here should only be used to build
			   stuff that will be grokked as visibility decls.  */
			t = lookup_field (ctype, sname, 0, 0);
			if (t)
			  {
			    t = build_lang_field_decl (FIELD_DECL, build_nt (SCOPE_REF, ctype, t), type);
			    DECL_INITIAL (t) = init;
			    return t;
			  }
			/* No such field, try member functions.  */
			t = lookup_fnfields (TYPE_BINFO (ctype), sname, 0);
			if (t)
			  {
			    if (flags == DTOR_FLAG)
			      t = TREE_VALUE (t);
			    else if (CLASSTYPE_METHOD_VEC (ctype)
				     && TREE_VALUE (t) == TREE_VEC_ELT (CLASSTYPE_METHOD_VEC (ctype), 0))
			      {
				/* Don't include destructor with constructors.  */
				t = DECL_CHAIN (TREE_VALUE (t));
				if (t == NULL_TREE)
				  error ("class `%s' does not have any constructors", IDENTIFIER_POINTER (sname));
				t = build_tree_list (NULL_TREE, t);
			      }
			    t = build_lang_field_decl (FIELD_DECL, build_nt (SCOPE_REF, ctype, t), type);
			    DECL_INITIAL (t) = init;
			    return t;
			  }

			if (flags == TYPENAME_FLAG)
			  error_with_aggr_type (ctype, "type conversion is not a member of structure `%s'");
			else
			  error ("field `%s' is not a member of structure `%s'",
				 IDENTIFIER_POINTER (sname),
				 TYPE_NAME_STRING (ctype));
		      }
		    if (TREE_CODE (type) == FUNCTION_TYPE)
		      type = build_cplus_method_type (build_type_variant (ctype, constp, volatilep),
						      TREE_TYPE (type), TYPE_ARG_TYPES (type));
		    else
		      {
			if (current_class_type)
			  {
			    if (TYPE_MAIN_VARIANT (ctype) != current_class_type)
			      {
				error ("cannot declare member `%s::%s' within this class",
				       TYPE_NAME_STRING (ctype), name);
			      return void_type_node;
			      }
			    else if (extra_warnings)
			      warning ("extra qualification `%s' on member `%s' ignored",
				       TYPE_NAME_STRING (ctype), name);
			  }
			type = build_offset_type (ctype, type);
		      }
		  }
		else if (uses_template_parms (ctype))
		  {
                    enum tree_code c;
                    if (TREE_CODE (type) == FUNCTION_TYPE)
		      {
			type = build_cplus_method_type (build_type_variant (ctype, constp, volatilep),
							TREE_TYPE (type),
							TYPE_ARG_TYPES (type));
			c = FUNCTION_DECL;
		      }
  		  }
		else
		  sorry ("structure `%s' not yet defined",
			 TYPE_NAME_STRING (ctype));
		declarator = sname;
	      }
	    else if (TREE_CODE (sname) == TYPE_EXPR)
	      {
		/* A TYPE_EXPR will change types out from under us.
		   So do the TYPE_EXPR now, and make this SCOPE_REF
		   inner to the TYPE_EXPR's CALL_EXPR.

		   This does not work if we don't get a CALL_EXPR back.
		   I did not think about error recovery, hence the
		   my_friendly_abort.  */

		/* Get the CALL_EXPR.  */
		sname = grokoptypename (sname, 0);
		my_friendly_assert (TREE_CODE (sname) == CALL_EXPR, 157);
		type = TREE_TYPE (TREE_OPERAND (sname, 0));
		/* Scope the CALL_EXPR's name.  */
		TREE_OPERAND (declarator, 1) = TREE_OPERAND (sname, 0);
		/* Put the SCOPE_EXPR in the CALL_EXPR's innermost position.  */
		TREE_OPERAND (sname, 0) = declarator;
		/* Now work from the CALL_EXPR.  */
		declarator = sname;
		continue;
	      }
	    else if (TREE_CODE (sname) == SCOPE_REF)
	      my_friendly_abort (17);
	    else
	      {
	      done_scoping:
		declarator = TREE_OPERAND (declarator, 1);
		if (declarator && TREE_CODE (declarator) == CALL_EXPR)
		  /* In this case, we will deal with it later.  */
		  ;
		else
		  {
		    if (TREE_CODE (type) == FUNCTION_TYPE)
		      type = build_cplus_method_type (build_type_variant (ctype, constp, volatilep), TREE_TYPE (type), TYPE_ARG_TYPES (type));
		    else
		      type = build_offset_type (ctype, type);
		  }
	      }
	  }
	  break;

	case BIT_NOT_EXPR:
	  declarator = TREE_OPERAND (declarator, 0);
	  break;

	case TYPE_EXPR:
	  declarator = grokoptypename (declarator, 0);
	  if (explicit_int != -1)
	    if (comp_target_types (type,
				   TREE_TYPE (TREE_OPERAND (declarator, 0)),
				   1) == 0)
	      error ("type conversion function declared to return incongruent type");
	    else
	      pedwarn ("return type specified for type conversion function");
	  type = TREE_TYPE (TREE_OPERAND (declarator, 0));
	  maybe_globalize_type (type);
	  break;

	case RECORD_TYPE:
	case UNION_TYPE:
	case ENUMERAL_TYPE:
	  declarator = NULL_TREE;
	  break;

	case ERROR_MARK:
	  declarator = NULL_TREE;
	  break;

	default:
	  my_friendly_abort (158);
	}
    }

  /* Now TYPE has the actual type.  */

  /* If this is declaring a typedef name, return a TYPE_DECL.  */

  if (specbits & RIDBIT (RID_TYPEDEF))
    {
      tree decl;

      /* Note that the grammar rejects storage classes
	 in typenames, fields or parameters.  */
      if (constp || volatilep)
	type = build_type_variant (type, constp, volatilep);

      /* If the user declares "struct {...} foo" then `foo' will have
	 an anonymous name.  Fill that name in now.  Nothing can
	 refer to it, so nothing needs know about the name change.
	 The TYPE_NAME field was filled in by build_struct_xref.  */
      if (TYPE_NAME (type)
	  && TREE_CODE (TYPE_NAME (type)) == TYPE_DECL
	  && ANON_AGGRNAME_P (TYPE_IDENTIFIER (type)))
	{
	  /* replace the anonymous name with the real name everywhere.  */
	  lookup_tag_reverse (type, declarator);
	  TYPE_IDENTIFIER (type) = declarator;
	}

#if 0 /* not yet, should get fixed properly later */
      decl = make_type_decl (declarator, type);
#else
      decl = build_decl (TYPE_DECL, declarator, type);
#endif
      if (quals)
	{
	  if (ctype == NULL_TREE)
	    {
	      if (TREE_CODE (type) != METHOD_TYPE)
		error_with_decl (decl, "invalid type qualifier for non-method type");
	      else
		ctype = TYPE_METHOD_BASETYPE (type);
	    }
	  if (ctype != NULL_TREE)
	    grok_method_quals (ctype, decl, quals);
	}

      if ((specbits & RIDBIT (RID_SIGNED))
	  || (typedef_decl && C_TYPEDEF_EXPLICITLY_SIGNED (typedef_decl)))
	C_TYPEDEF_EXPLICITLY_SIGNED (decl) = 1;

      return decl;
    }

  /* Detect the case of an array type of unspecified size
     which came, as such, direct from a typedef name.
     We must copy the type, so that each identifier gets
     a distinct type, so that each identifier's size can be
     controlled separately by its own initializer.  */

  if (type == typedef_type && TREE_CODE (type) == ARRAY_TYPE
      && TYPE_DOMAIN (type) == NULL_TREE)
    {
      type = build_cplus_array_type (TREE_TYPE (type), TYPE_DOMAIN (type));
    }

  /* If this is a type name (such as, in a cast or sizeof),
     compute the type and return it now.  */

  if (decl_context == TYPENAME)
    {
      /* Note that the grammar rejects storage classes
	 in typenames, fields or parameters.  */
      if (constp || volatilep)
	type = build_type_variant (type, constp, volatilep);

      /* Special case: "friend class foo" looks like a TYPENAME context.  */
      if (friendp)
	{
	  /* A friendly class?  */
	  if (current_class_type)
	    make_friend_class (current_class_type, TYPE_MAIN_VARIANT (type));
	  else
	    error("trying to make class `%s' a friend of global scope",
		  TYPE_NAME_STRING (type));
	  type = void_type_node;
	}
      else if (quals)
	{
#if 0 /* not yet, should get fixed properly later */
	  tree dummy = make_type_decl (declarator, type);
#else
	  tree dummy = build_decl (TYPE_DECL, declarator, type);
#endif
	  if (ctype == NULL_TREE)
	    {
	      my_friendly_assert (TREE_CODE (type) == METHOD_TYPE, 159);
	      ctype = TYPE_METHOD_BASETYPE (type);
	    }
	  grok_method_quals (ctype, dummy, quals);
	  type = TREE_TYPE (dummy);
	}

      return type;
    }

  /* `void' at top level (not within pointer)
     is allowed only in typedefs or type names.
     We don't complain about parms either, but that is because
     a better error message can be made later.  */

  if (TYPE_MAIN_VARIANT (type) == void_type_node && decl_context != PARM)
    {
      if (declarator != NULL_TREE
	  && TREE_CODE (declarator) == IDENTIFIER_NODE)
	{
	  if (IDENTIFIER_OPNAME_P (declarator))
	    error ("operator `%s' declared void",
		   operator_name_string (declarator));
	  else
	    error ("variable or field `%s' declared void", name);
	}
      else
	error ("variable or field declared void");
      type = integer_type_node;
    }

  /* Now create the decl, which may be a VAR_DECL, a PARM_DECL
     or a FUNCTION_DECL, depending on DECL_CONTEXT and TYPE.  */

  {
    register tree decl;

    if (decl_context == PARM)
      {
	tree parmtype = type;

	if (ctype)
	  error ("cannot use `::' in parameter declaration");
	bad_specifiers ("parameter", virtualp, quals != NULL_TREE,
			friendp, raises != NULL_TREE);

	/* A parameter declared as an array of T is really a pointer to T.
	   One declared as a function is really a pointer to a function.
	   One declared as a member is really a pointer to member.

	   Don't be misled by references.  */

	if (TREE_CODE (type) == REFERENCE_TYPE)
	  type = TREE_TYPE (type);

	if (TREE_CODE (type) == ARRAY_TYPE)
	  {
	    if (parmtype == type)
	      {
		/* Transfer const-ness of array into that of type
		   pointed to.  */
		type = build_pointer_type
		  (build_type_variant (TREE_TYPE (type), constp, volatilep));
		volatilep = constp = 0;
	      }
	    else
	      type = build_pointer_type (TREE_TYPE (type));
	  }
	else if (TREE_CODE (type) == FUNCTION_TYPE)
	  type = build_pointer_type (type);
	else if (TREE_CODE (type) == OFFSET_TYPE)
	  type = build_pointer_type (type);

	if (TREE_CODE (parmtype) == REFERENCE_TYPE)
	  {
	    /* Transfer const-ness of reference into that of type pointed to.  */
	    type = build_type_variant (build_reference_type (type), constp, volatilep);
	    constp = volatilep = 0;
	  }

	decl = build_decl (PARM_DECL, declarator, type);

	/* Compute the type actually passed in the parmlist,
	   for the case where there is no prototype.
	   (For example, shorts and chars are passed as ints.)
	   When there is a prototype, this is overridden later.  */

	DECL_ARG_TYPE (decl) = type;
	if (TYPE_MAIN_VARIANT (type) == float_type_node)
	  DECL_ARG_TYPE (decl) = build_type_variant (double_type_node,
						     TYPE_READONLY (type),
						     TYPE_VOLATILE (type));
	else if (C_PROMOTING_INTEGER_TYPE_P (type))
	  {
	    tree argtype;

	    /* Retain unsignedness if traditional or if not really
	       getting wider.  */
	    if (TREE_UNSIGNED (type)
		&& (flag_traditional
		    || TYPE_PRECISION (type)
			== TYPE_PRECISION (integer_type_node)))
	      argtype = unsigned_type_node;
	    else
	      argtype = integer_type_node;
	    DECL_ARG_TYPE (decl) = build_type_variant (argtype,
						       TYPE_READONLY (type),
						       TYPE_VOLATILE (type));
	  }
      }
    else if (decl_context == FIELD)
      {
	if (type == error_mark_node)
	  {
	    /* Happens when declaring arrays of sizes which
	       are error_mark_node, for example.  */
	    decl = NULL_TREE;
	  }
	else if (TREE_CODE (type) == FUNCTION_TYPE)
	  {
	    int publicp = 0;

	    if (friendp == 0)
	      {
		if (ctype == NULL_TREE)
		  ctype = current_class_type;

		if (ctype == NULL_TREE)
		  {
		    register int op = IDENTIFIER_OPNAME_P (declarator);
		    error ("can't make %s `%s' into a method -- not in a class",
			   op ? "operator" : "function",
			   op ? operator_name_string (declarator) : IDENTIFIER_POINTER (declarator));
		    return void_type_node;
		  }

		/* ``A union may [ ... ] not [ have ] virtual functions.''
		   ARM 9.5 */
		if (virtualp && TREE_CODE (ctype) == UNION_TYPE)
		  {
		    error ("function `%s' declared virtual inside a union",
			   IDENTIFIER_POINTER (declarator));
		    return void_type_node;
		  }

		/* Don't convert type of operators new and delete to
		   METHOD_TYPE; they remain FUNCTION_TYPEs.  */
		if (staticp < 2
		    && declarator != ansi_opname[(int) NEW_EXPR]
		    && declarator != ansi_opname[(int) DELETE_EXPR])
		  type = build_cplus_method_type (build_type_variant (ctype, constp, volatilep),
						  TREE_TYPE (type), TYPE_ARG_TYPES (type));
	      }

	    /* Tell grokfndecl if it needs to set TREE_PUBLIC on the node.  */
	    publicp = ((specbits & RIDBIT (RID_EXTERN))
		       || (ctype != NULL_TREE && funcdef_flag >= 0)
#if 0
		       /* These are replicated in each object, so we shouldn't
			  set TREE_PUBLIC. */
		       || (friendp
			   && !(specbits & RIDBIT (RID_STATIC))
			   && !(specbits & RIDBIT (RID_INLINE)))
#endif
		       );
	    decl = grokfndecl (ctype, type, declarator,
			       virtualp, flags, quals,
			       raises, friendp ? -1 : 0, publicp);
	    DECL_INLINE (decl) = inlinep;
	  }
	else if (TREE_CODE (type) == METHOD_TYPE)
	  {
	    /* All method decls are public, so tell grokfndecl to set
	       TREE_PUBLIC, also.  */
	    decl = grokfndecl (ctype, type, declarator,
			       virtualp, flags, quals,
			       raises, friendp ? -1 : 0, 1);
	    DECL_INLINE (decl) = inlinep;
	  }
	else if (TREE_CODE (type) == RECORD_TYPE
		 && CLASSTYPE_DECLARED_EXCEPTION (type))
	  {
	    /* Handle a class-local exception declaration.  */
	    decl = build_lang_field_decl (VAR_DECL, declarator, type);
	    if (ctype == NULL_TREE)
	      ctype = current_class_type;
	    finish_exception_decl (TREE_CODE (TYPE_NAME (ctype)) == TYPE_DECL
				   ? TYPE_IDENTIFIER (ctype) : TYPE_NAME (ctype), decl);
	    return void_type_node;
	  }
	else if (TYPE_SIZE (type) == NULL_TREE && !staticp
		 && (TREE_CODE (type) != ARRAY_TYPE || initialized == 0))
	  {
	    if (declarator)
	      error ("field `%s' has incomplete type",
		     IDENTIFIER_POINTER (declarator));
	    else
	      error ("field has incomplete type");

	    /* If we're instantiating a template, tell them which
	       instantiation made the field's type be incomplete.  */
	    if (current_class_type
		&& IDENTIFIER_TEMPLATE (current_class_type)
		&& declspecs && TREE_VALUE (declspecs)
		&& TREE_TYPE (TREE_VALUE (declspecs)) == type)
	      error ("  in instantiation of template `%s'",
		     TYPE_NAME_STRING (current_class_type));
		
	    type = error_mark_node;
	    decl = NULL_TREE;
	  }
	else
	  {
	    if (friendp)
	      {
		if (declarator)
		  error ("`%s' is neither function nor method; cannot be declared friend",
			 IDENTIFIER_POINTER (declarator));
		else
		  {
		    error ("invalid friend declaration");
		    return void_type_node;
		  }
		friendp = 0;
	      }
	    decl = NULL_TREE;
	  }

	if (friendp)
	  {
	    tree t;

	    /* Friends are treated specially.  */
	    if (ctype == current_class_type)
	      warning ("member functions are implicitly friends of their class");
	    else if (decl && (t = DECL_NAME (decl)))
	      {
		/* ARM $13.4.3 */
		if (t == ansi_opname[(int) MODIFY_EXPR])
		  pedwarn ("operator `=' must be a member function");
		else
		  return do_friend (ctype, declarator, decl,
				    last_function_parms, flags, quals);
	      }
	    else return void_type_node;
	  }

	/* Structure field.  It may not be a function, except for C++ */

	if (decl == NULL_TREE)
	  {
	    bad_specifiers ("field", virtualp, quals != NULL_TREE,
			    friendp, raises != NULL_TREE);

	    /* ANSI C++ June 5 1992 WP 9.2.2 and 9.4.2.  A member-declarator
	       cannot have an initializer, and a static member declaration must
	       be defined elsewhere.  */
	    if (initialized)
	      {
		if (staticp)
		  error ("static member `%s' must be defined separately from its declaration",
			  IDENTIFIER_POINTER (declarator));
		/* Note that initialization of const members is not
		   mentioned in the ARM or draft ANSI standard explicitly,
		   and it appears to be in common practice.  However,
		   reading the draft section 9.2.2, it does say that a
		   member declarator can't have an initializer--it does
		   not except constant members, which also qualify as
		   member-declarators.  */
		else if (!pedantic && (!constp || flag_ansi))
		  warning ("ANSI C++ forbids initialization of %s `%s'",
			   constp ? "const member" : "member",
			   IDENTIFIER_POINTER (declarator));
	      }

	    if (staticp || (constp && initialized))
	      {
		/* C++ allows static class members.
		   All other work for this is done by grokfield.
		   This VAR_DECL is built by build_lang_field_decl.
		   All other VAR_DECLs are built by build_decl.  */
		decl = build_lang_field_decl (VAR_DECL, declarator, type);
		if (staticp || TREE_CODE (type) == ARRAY_TYPE)
		  TREE_STATIC (decl) = 1;
		/* In class context, static means public visibility.  */
		TREE_PUBLIC (decl) = 1;
		DECL_EXTERNAL (decl) = !initialized;
	      }
	    else
	      decl = build_lang_field_decl (FIELD_DECL, declarator, type);
	  }
      }
    else if (TREE_CODE (type) == FUNCTION_TYPE || TREE_CODE (type) == METHOD_TYPE)
      {
	int was_overloaded = 0;
	tree original_name = declarator;
	int publicp = 0;

	if (! declarator) return NULL_TREE;

	if (specbits & (RIDBIT (RID_AUTO) | RIDBIT (RID_REGISTER)))
	  error ("invalid storage class for function `%s'", name);
	/* Function declaration not at top level.
	   Storage classes other than `extern' are not allowed
	   and `extern' makes no difference.  */
	if (current_binding_level != global_binding_level
	    && (specbits & (RIDBIT (RID_STATIC) | RIDBIT (RID_INLINE)))
	    && pedantic)
	  pedwarn ("invalid storage class for function `%s'", name);

	if (ctype == NULL_TREE)
	  {
	    if (virtualp)
	      {
		error ("virtual non-class function `%s'", name);
		virtualp = 0;
	      }

	    /* ARM $13.4.3 */
	    /* XXX: It's likely others should also be forbidden.  (bpk) */
	    if (declarator == ansi_opname[(int) MODIFY_EXPR])
	      warning ("operator `=' must be a member function");

	    if (current_lang_name == lang_name_cplusplus
		&& ! (IDENTIFIER_LENGTH (original_name) == 4
		      && IDENTIFIER_POINTER (original_name)[0] == 'm'
		      && strcmp (IDENTIFIER_POINTER (original_name), "main") == 0)
		&& ! (IDENTIFIER_LENGTH (original_name) > 10
		      && IDENTIFIER_POINTER (original_name)[0] == '_'
		      && IDENTIFIER_POINTER (original_name)[1] == '_'
		      && strncmp (IDENTIFIER_POINTER (original_name)+2, "builtin_", 8) == 0))
	      {
		/* Plain overloading: will not be grok'd by grokclassfn.  */
		declarator = build_decl_overload (dname, TYPE_ARG_TYPES (type), 0);
		was_overloaded = 1;
	      }
	  }
	else if (TREE_CODE (type) == FUNCTION_TYPE && staticp < 2)
	  type = build_cplus_method_type (build_type_variant (ctype, constp, volatilep),
					  TREE_TYPE (type), TYPE_ARG_TYPES (type));

	/* Record presence of `static'.  In C++, `inline' is like `static'.
	   Methods of classes should be public, unless we're dropping them
	   into some other file, so we don't clear TREE_PUBLIC for them.  */
	publicp
	  = ((ctype
	      && ! CLASSTYPE_INTERFACE_UNKNOWN (ctype)
	      && ! CLASSTYPE_INTERFACE_ONLY (ctype))
	    || !(specbits & (RIDBIT (RID_STATIC)
			     | RIDBIT (RID_INLINE))));

	decl = grokfndecl (ctype, type, original_name,
			   virtualp, flags, quals,
			   raises,
			   processing_template_decl ? 0 : friendp ? 2 : 1,
			   publicp);

	if (ctype == NULL_TREE)
	  DECL_ASSEMBLER_NAME (decl) = declarator;

	if (staticp == 1)
	  {
	    int illegal_static = 0;

	    /* Don't allow a static member function in a class, and forbid
	       declaring main to be static.  */
	    if (TREE_CODE (type) == METHOD_TYPE)
	      {
		error_with_decl (decl,
				 "cannot declare member function `%s' to have static linkage");
		illegal_static = 1;
	      }
	    else if (! was_overloaded
		     && ! ctype
		     && IDENTIFIER_LENGTH (original_name) == 4
		     && IDENTIFIER_POINTER (original_name)[0] == 'm'
		     && ! strcmp (IDENTIFIER_POINTER (original_name), "main"))
	      {
		error ("cannot declare function `main' to have static linkage");
		illegal_static = 1;
	      }

	    if (illegal_static)
	      {
		staticp = 0;
		specbits &= ~ RIDBIT (RID_STATIC);
	      }
	  }

	/* Record presence of `inline', if it is reasonable.  */
	if (inlinep)
	  {
	    tree last = tree_last (TYPE_ARG_TYPES (type));

	    if (! was_overloaded
		&& ! ctype
		&& ! strcmp (IDENTIFIER_POINTER (original_name), "main"))
	      warning ("cannot inline function `main'");
	    else if (last && last != void_list_node)
	      warning ("inline declaration ignored for function with `...'");
	    else
	      /* Assume that otherwise the function can be inlined.  */
	      DECL_INLINE (decl) = 1;

	    if (specbits & RIDBIT (RID_EXTERN))
	      {
		current_extern_inline = 1;
		if (pedantic)
		  error ("ANSI C++ does not permit `extern inline'");
		else if (flag_ansi)
		  warning ("ANSI C++ does not permit `extern inline'");
	      }
	  }
	if (was_overloaded)
	  DECL_OVERLOADED (decl) = 1;
      }
    else
      {
	/* It's a variable.  */

	bad_specifiers ("variable", virtualp, quals != NULL_TREE,
			friendp, raises != NULL_TREE);
	if (inlinep)
	  warning ("variable declared `inline'");

	/* An uninitialized decl with `extern' is a reference.  */
	decl = grokvardecl (type, declarator, specbits, initialized);
	if (ctype)
	  {
	    if (staticp == 1)
	      {
	        error ("cannot declare member `%s' to have static linkage",
		       lang_printable_name (decl));
	        staticp = 0;
	        specbits &= ~ RIDBIT (RID_STATIC);
	      }
	    if (specbits & RIDBIT (RID_EXTERN))
	      {
	        error ("cannot explicitly declare member `%s' to have extern linkage",
		       lang_printable_name (decl));
	        specbits &= ~ RIDBIT (RID_EXTERN);
	      }
	  }
      }

    /* Record `register' declaration for warnings on &
       and in case doing stupid register allocation.  */

    if (specbits & RIDBIT (RID_REGISTER))
      DECL_REGISTER (decl) = 1;

    /* Record constancy and volatility.  */

    if (constp)
      TREE_READONLY (decl) = TREE_CODE (type) != REFERENCE_TYPE;
    if (volatilep)
      {
	TREE_SIDE_EFFECTS (decl) = 1;
	TREE_THIS_VOLATILE (decl) = 1;
      }

    return decl;
  }
}

/* Tell if a parmlist/exprlist looks like an exprlist or a parmlist.
   An empty exprlist is a parmlist.  An exprlist which
   contains only identifiers at the global level
   is a parmlist.  Otherwise, it is an exprlist.  */
int
parmlist_is_exprlist (exprs)
     tree exprs;
{
  if (exprs == NULL_TREE || TREE_PARMLIST (exprs))
    return 0;

  if (current_binding_level == global_binding_level)
    {
      /* At the global level, if these are all identifiers,
	 then it is a parmlist.  */
      while (exprs)
	{
	  if (TREE_CODE (TREE_VALUE (exprs)) != IDENTIFIER_NODE)
	    return 1;
	  exprs = TREE_CHAIN (exprs);
	}
      return 0;
    }
  return 1;
}

/* Make sure that the this list of PARMS has a chance of being
   grokked by `grokparms'.

   @@ This is really weak, but the grammar does not allow us
   @@ to easily reject things that this has to catch as syntax errors.  */
static int
parmlist_is_random (parms)
     tree parms;
{
  if (parms == NULL_TREE)
    return 0;

  if (TREE_CODE (parms) != TREE_LIST)
    return 1;

  while (parms)
    {
      if (parms == void_list_node)
	return 0;

      if (TREE_CODE (TREE_VALUE (parms)) != TREE_LIST)
	return 1;
      /* Don't get faked out by overloaded functions, which
	 masquerade as TREE_LISTs!  */
      if (TREE_TYPE (TREE_VALUE (parms)) == unknown_type_node)
	return 1;
      parms = TREE_CHAIN (parms);
    }
  return 0;
}

/* Subroutine of `grokparms'.  In a fcn definition, arg types must
   be complete.

   C++: also subroutine of `start_function'.  */
static void
require_complete_types_for_parms (parms)
     tree parms;
{
  while (parms)
    {
      tree type = TREE_TYPE (parms);
      if (TYPE_SIZE (type) == NULL_TREE)
	{
	  if (DECL_NAME (parms))
	    error ("parameter `%s' has incomplete type",
		   IDENTIFIER_POINTER (DECL_NAME (parms)));
	  else
	    error ("parameter has incomplete type");
	  TREE_TYPE (parms) = error_mark_node;
	}
#if 0
      /* If the arg types are incomplete in a declaration,
	 they must include undefined tags.
	 These tags can never be defined in the scope of the declaration,
	 so the types can never be completed,
	 and no call can be compiled successfully.  */
      /* This is not the right behavior for C++, but not having
	 it is also probably wrong.  */
      else
	{
	  /* Now warn if is a pointer to an incomplete type.  */
	  while (TREE_CODE (type) == POINTER_TYPE
		 || TREE_CODE (type) == REFERENCE_TYPE)
	    type = TREE_TYPE (type);
	  type = TYPE_MAIN_VARIANT (type);
	  if (TYPE_SIZE (type) == NULL_TREE)
	    {
	      if (DECL_NAME (parm) != NULL_TREE)
		warning ("parameter `%s' points to incomplete type",
			 IDENTIFIER_POINTER (DECL_NAME (parm)));
	      else
		warning ("parameter points to incomplete type");
	    }
	}
#endif
      parms = TREE_CHAIN (parms);
    }
}

/* Decode the list of parameter types for a function type.
   Given the list of things declared inside the parens,
   return a list of types.

   The list we receive can have three kinds of elements:
   an IDENTIFIER_NODE for names given without types,
   a TREE_LIST node for arguments given as typespecs or names with typespecs,
   or void_type_node, to mark the end of an argument list
   when additional arguments are not permitted (... was not used).

   FUNCDEF_FLAG is nonzero for a function definition, 0 for
   a mere declaration.  A nonempty identifier-list gets an error message
   when FUNCDEF_FLAG is zero.
   If FUNCDEF_FLAG is 1, then parameter types must be complete.
   If FUNCDEF_FLAG is -1, then parameter types may be incomplete.

   If all elements of the input list contain types,
   we return a list of the types.
   If all elements contain no type (except perhaps a void_type_node
   at the end), we return a null list.
   If some have types and some do not, it is an error, and we
   return a null list.

   Also set last_function_parms to either
   a list of names (IDENTIFIER_NODEs) or a chain of PARM_DECLs.
   A list of names is converted to a chain of PARM_DECLs
   by store_parm_decls so that ultimately it is always a chain of decls.

   Note that in C++, parameters can take default values.  These default
   values are in the TREE_PURPOSE field of the TREE_LIST.  It is
   an error to specify default values which are followed by parameters
   that have no default values, or an ELLIPSES.  For simplicities sake,
   only parameters which are specified with their types can take on
   default values.  */

static tree
grokparms (first_parm, funcdef_flag)
     tree first_parm;
     int funcdef_flag;
{
  tree result = NULL_TREE;
  tree decls = NULL_TREE;

  if (first_parm != NULL_TREE
      && TREE_CODE (TREE_VALUE (first_parm)) == IDENTIFIER_NODE)
    {
      if (! funcdef_flag)
	warning ("parameter names (without types) in function declaration");
      last_function_parms = first_parm;
      return NULL_TREE;
    }
  else
    {
      /* Types were specified.  This is a list of declarators
	 each represented as a TREE_LIST node.  */
      register tree parm, chain;
      int any_init = 0, any_error = 0, saw_void = 0;

      if (first_parm != NULL_TREE)
	{
	  tree last_result = NULL_TREE;
	  tree last_decl = NULL_TREE;

	  for (parm = first_parm; parm != NULL_TREE; parm = chain)
	    {
	      tree type, list_node = parm;
	      register tree decl = TREE_VALUE (parm);
	      tree init = TREE_PURPOSE (parm);

	      chain = TREE_CHAIN (parm);
	      /* @@ weak defense against parse errors.  */
	      if (decl != void_type_node && TREE_CODE (decl) != TREE_LIST)
		{
		  /* Give various messages as the need arises.  */
		  if (TREE_CODE (decl) == STRING_CST)
		    error ("invalid string constant `%s'",
			   TREE_STRING_POINTER (decl));
		  else if (TREE_CODE (decl) == INTEGER_CST)
		    error ("invalid integer constant in parameter list, did you forget to give parameter name?");
		  continue;
		}

	      if (decl != void_type_node)
		{
		  /* @@ May need to fetch out a `raises' here.  */
		  decl = grokdeclarator (TREE_VALUE (decl),
					 TREE_PURPOSE (decl),
					 PARM, init != NULL_TREE, NULL_TREE);
		  if (! decl)
		    continue;
		  type = TREE_TYPE (decl);
		  if (TYPE_MAIN_VARIANT (type) == void_type_node)
		    decl = void_type_node;
		  else if (TREE_CODE (type) == METHOD_TYPE)
		    {
		      if (DECL_NAME (decl))
			/* Cannot use `error_with_decl' here because
			   we don't have DECL_CONTEXT set up yet.  */
			error ("parameter `%s' invalidly declared method type",
			       IDENTIFIER_POINTER (DECL_NAME (decl)));
		      else
			error ("parameter invalidly declared method type");
		      type = build_pointer_type (type);
		      TREE_TYPE (decl) = type;
		    }
		  else if (TREE_CODE (type) == OFFSET_TYPE)
		    {
		      if (DECL_NAME (decl))
			error ("parameter `%s' invalidly declared offset type",
			       IDENTIFIER_POINTER (DECL_NAME (decl)));
		      else
			error ("parameter invalidly declared offset type");
		      type = build_pointer_type (type);
		      TREE_TYPE (decl) = type;
		    }
                  else if (TREE_CODE (type) == RECORD_TYPE
                           && TYPE_LANG_SPECIFIC (type)
                           && CLASSTYPE_ABSTRACT_VIRTUALS (type))
                    {
                      abstract_virtuals_error (decl, type);
                      any_error = 1;  /* seems like a good idea */
                    }
		}

	      if (decl == void_type_node)
		{
		  if (result == NULL_TREE)
		    {
		      result = void_list_node;
		      last_result = result;
		    }
		  else
		    {
		      TREE_CHAIN (last_result) = void_list_node;
		      last_result = void_list_node;
		    }
		  saw_void = 1;
		  if (chain
		      && (chain != void_list_node || TREE_CHAIN (chain)))
		    error ("`void' in parameter list must be entire list");
		  break;
		}

	      /* Since there is a prototype, args are passed in their own types.  */
	      DECL_ARG_TYPE (decl) = TREE_TYPE (decl);
#ifdef PROMOTE_PROTOTYPES
	      if (C_PROMOTING_INTEGER_TYPE_P (type))
		DECL_ARG_TYPE (decl) = integer_type_node;
#endif
	      if (!any_error)
		{
		  if (init)
		    {
		      any_init++;
		      if (TREE_CODE (init) == SAVE_EXPR)
			PARM_DECL_EXPR (init) = 1;
		      else if (TREE_CODE (init) == VAR_DECL)
			{
			  if (IDENTIFIER_LOCAL_VALUE (DECL_NAME (init)))
			    {
			      /* ``Local variables may not be used in default
				 argument expressions.'' dpANSI C++ 8.2.6 */
			      /* If extern int i; within a function is not
				 considered a local variable, then this code is
				 wrong. */
			      error_with_decl (init, "local variable `%s' may not be used as a default argument");
			      any_error = 1;
			    }
			  else if (TREE_READONLY_DECL_P (init))
			    init = decl_constant_value (init);
			}
		      else
			init = require_instantiated_type (type, init, integer_zero_node);
		    }
		  else if (any_init)
		    {
		      error ("all trailing parameters must have default arguments");
		      any_error = 1;
		    }
		}
	      else
		init = NULL_TREE;

	      if (decls == NULL_TREE)
		{
		  decls = decl;
		  last_decl = decls;
		}
	      else
		{
		  TREE_CHAIN (last_decl) = decl;
		  last_decl = decl;
		}
	      if (TREE_PERMANENT (list_node))
		{
		  TREE_PURPOSE (list_node) = init;
		  TREE_VALUE (list_node) = type;
		  TREE_CHAIN (list_node) = NULL_TREE;
		}
	      else
		list_node = saveable_tree_cons (init, type, NULL_TREE);
	      if (result == NULL_TREE)
		{
		  result = list_node;
		  last_result = result;
		}
	      else
		{
		  TREE_CHAIN (last_result) = list_node;
		  last_result = list_node;
		}
	    }
	  if (last_result)
	    TREE_CHAIN (last_result) = NULL_TREE;
	  /* If there are no parameters, and the function does not end
	     with `...', then last_decl will be NULL_TREE.  */
	  if (last_decl != NULL_TREE)
	    TREE_CHAIN (last_decl) = NULL_TREE;
	}
    }

  last_function_parms = decls;

  /* In a fcn definition, arg types must be complete.  */
  if (funcdef_flag > 0)
    require_complete_types_for_parms (last_function_parms);

  return result;
}

/* These memoizing functions keep track of special properties which
   a class may have.  `grok_ctor_properties' notices whether a class
   has a constructor of the for X(X&), and also complains
   if the class has a constructor of the form X(X).
   `grok_op_properties' takes notice of the various forms of
   operator= which are defined, as well as what sorts of type conversion
   may apply.  Both functions take a FUNCTION_DECL as an argument.  */
void
grok_ctor_properties (ctype, decl)
     tree ctype, decl;
{
  tree parmtypes = FUNCTION_ARG_CHAIN (decl);
  tree parmtype = parmtypes ? TREE_VALUE (parmtypes) : void_type_node;

  if (parmtypes && TREE_CHAIN (parmtypes)
      && TREE_CODE (TREE_VALUE (TREE_CHAIN (parmtypes))) == REFERENCE_TYPE
      && TYPE_USES_VIRTUAL_BASECLASSES (TREE_TYPE (TREE_VALUE (TREE_CHAIN (parmtypes)))))
    {
      parmtypes = TREE_CHAIN (parmtypes);
      parmtype = TREE_VALUE (parmtypes);
    }

  if (TREE_CODE (parmtype) == REFERENCE_TYPE
      && TYPE_MAIN_VARIANT (TREE_TYPE (parmtype)) == ctype)
    {
      if (TREE_CHAIN (parmtypes) == NULL_TREE
	  || TREE_CHAIN (parmtypes) == void_list_node
	  || TREE_PURPOSE (TREE_CHAIN (parmtypes)))
	{
	  TYPE_HAS_INIT_REF (ctype) = 1;
	  TYPE_GETS_INIT_REF (ctype) = 1;
	  if (TYPE_READONLY (TREE_TYPE (parmtype)))
	    TYPE_GETS_CONST_INIT_REF (ctype) = 1;
	}
      else
	TYPE_GETS_INIT_AGGR (ctype) = 1;
    }
  else if (TYPE_MAIN_VARIANT (parmtype) == ctype)
    {
      if (TREE_CHAIN (parmtypes) != NULL_TREE
	  && TREE_CHAIN (parmtypes) == void_list_node)
	error ("invalid constructor; you probably meant `%s (%s&)'",
	       TYPE_NAME_STRING (ctype),
	       TYPE_NAME_STRING (ctype));
      SET_IDENTIFIER_ERROR_LOCUS (DECL_NAME (decl), ctype);
      TYPE_GETS_INIT_AGGR (ctype) = 1;
    }
  else if (TREE_CODE (parmtype) == VOID_TYPE
	   || TREE_PURPOSE (parmtypes) != NULL_TREE)
    TYPE_HAS_DEFAULT_CONSTRUCTOR (ctype) = 1;
}

/* Do a little sanity-checking on how they declared their operator.  */
static void
grok_op_properties (decl, virtualp)
     tree decl;
     int virtualp;
{
  tree argtypes = TYPE_ARG_TYPES (TREE_TYPE (decl));

  if (DECL_STATIC_FUNCTION_P (decl))
    {
      if (DECL_NAME (decl) == ansi_opname[(int) NEW_EXPR])
	{
	  if (virtualp)
	    error ("`operator new' cannot be declared virtual");

	  /* Take care of function decl if we had syntax errors.  */
	  if (argtypes == NULL_TREE)
	    TREE_TYPE (decl) =
	      build_function_type (ptr_type_node,
				   hash_tree_chain (integer_type_node,
						    void_list_node));
	  else
	    decl = coerce_new_type (TREE_TYPE (decl));
	}
      else if (DECL_NAME (decl) == ansi_opname[(int) DELETE_EXPR])
	{
	  if (virtualp)
	    error ("`operator delete' cannot be declared virtual");

	  if (argtypes == NULL_TREE)
	    TREE_TYPE (decl) =
	      build_function_type (void_type_node,
				   hash_tree_chain (ptr_type_node,
						    void_list_node));
	  else
	    decl = coerce_delete_type (TREE_TYPE (decl));
	}
      else
	error_with_decl (decl, "`%s' cannot be a static member function");
    }
  else if (DECL_NAME (decl) == ansi_opname[(int) MODIFY_EXPR])
    {
      tree parmtypes;
      tree parmtype;

      if (argtypes == NULL_TREE)
	{
	  error_with_decl (decl, "too few arguments to `%s'");
	  return;
	}
      parmtypes = TREE_CHAIN (argtypes);
      parmtype = parmtypes ? TREE_VALUE (parmtypes) : void_type_node;

      if (TREE_CODE (parmtype) == REFERENCE_TYPE
	  && TREE_TYPE (parmtype) == current_class_type)
	{
	  TYPE_HAS_ASSIGN_REF (current_class_type) = 1;
	  TYPE_GETS_ASSIGN_REF (current_class_type) = 1;
	  if (TYPE_READONLY (TREE_TYPE (parmtype)))
	    TYPE_GETS_CONST_INIT_REF (current_class_type) = 1;
	}
    }
}

/* Get the struct, enum or union (CODE says which) with tag NAME.
   Define the tag as a forward-reference if it is not defined.

   C++: If a class derivation is given, process it here, and report
   an error if multiple derivation declarations are not identical.

   If this is a definition, come in through xref_tag and only look in
   the current frame for the name (since C++ allows new names in any
   scope.)  */

/* avoid rewriting all callers of xref_tag */
static int xref_next_defn = 0;

tree
xref_defn_tag (code_type_node, name, binfo)
     tree code_type_node;
     tree name, binfo;
{
  tree rv, ncp;
  xref_next_defn = 1;

  if (class_binding_level)
    {
      tree n1;
      char *buf;
      /* we need to build a new IDENTIFIER_NODE for name which nukes
       * the pieces... */
      n1 = IDENTIFIER_LOCAL_VALUE (current_class_name);
      if (n1)
	n1 = DECL_NAME (n1);
      else
	n1 = current_class_name;
      
      buf = (char *) alloca (4 + IDENTIFIER_LENGTH (n1)
			     + IDENTIFIER_LENGTH (name));
      
      sprintf (buf, "%s::%s", IDENTIFIER_POINTER (n1),
	       IDENTIFIER_POINTER (name));
      ncp = get_identifier (buf);
#ifdef SPEW_DEBUG
      if (spew_debug)
	printf("*** %s ***\n", IDENTIFIER_POINTER (ncp));
#endif
#if 0
      IDENTIFIER_LOCAL_VALUE (name) =
	build_lang_decl (TYPE_DECL, ncp, NULL_TREE);
#endif
      rv = xref_tag (code_type_node, name, binfo);
      pushdecl_top_level (build_lang_decl (TYPE_DECL, ncp, rv));
    }
  else
    {
      rv = xref_tag (code_type_node, name, binfo);
    }
  xref_next_defn = 0;
  return rv;
}

tree
xref_tag (code_type_node, name, binfo)
     tree code_type_node;
     tree name, binfo;
{
  enum tag_types tag_code;
  enum tree_code code;
  int temp = 0;
  int i, len;
  register tree ref;
  struct binding_level *b
    = (class_binding_level ? class_binding_level : current_binding_level);

  tag_code = (enum tag_types) TREE_INT_CST_LOW (code_type_node);
  switch (tag_code)
    {
    case record_type:
    case class_type:
    case exception_type:
      code = RECORD_TYPE;
      len = list_length (binfo);
      break;
    case union_type:
      code = UNION_TYPE;
      if (binfo)
	{
	  error ("derived union `%s' invalid", IDENTIFIER_POINTER (name));
	  binfo = NULL_TREE;
	}
      len = 0;
      break;
    case enum_type:
      code = ENUMERAL_TYPE;
      break;
    default:
      my_friendly_abort (18);
    }

  /* If a cross reference is requested, look up the type
     already defined for this tag and return it.  */
  if (xref_next_defn)
    {
      /* If we know we are defining this tag, only look it up in this scope
       * and don't try to find it as a type. */
      xref_next_defn = 0;
      ref = lookup_tag (code, name, b, 1);
    }
  else
    {
      ref = lookup_tag (code, name, b, 0);

      if (! ref)
	{
	  /* Try finding it as a type declaration.  If that wins, use it.  */
	  ref = lookup_name (name, 1);
	  if (ref && TREE_CODE (ref) == TYPE_DECL
	      && TREE_CODE (TREE_TYPE (ref)) == code)
	    ref = TREE_TYPE (ref);
	  else
	    ref = NULL_TREE;
	}
    }

  push_obstacks_nochange ();

  if (! ref)
    {
      /* If no such tag is yet defined, create a forward-reference node
	 and record it as the "definition".
	 When a real declaration of this type is found,
	 the forward-reference will be altered into a real type.  */

      /* In C++, since these migrate into the global scope, we must
	 build them on the permanent obstack.  */

      temp = allocation_temporary_p ();
      if (temp)
	end_temporary_allocation ();

      if (code == ENUMERAL_TYPE)
	{
	  ref = make_node (ENUMERAL_TYPE);

	  /* Give the type a default layout like unsigned int
	     to avoid crashing if it does not get defined.  */
	  TYPE_MODE (ref) = TYPE_MODE (unsigned_type_node);
	  TYPE_ALIGN (ref) = TYPE_ALIGN (unsigned_type_node);
	  TREE_UNSIGNED (ref) = 1;
	  TYPE_PRECISION (ref) = TYPE_PRECISION (unsigned_type_node);
	  TYPE_MIN_VALUE (ref) = TYPE_MIN_VALUE (unsigned_type_node);
	  TYPE_MAX_VALUE (ref) = TYPE_MAX_VALUE (unsigned_type_node);

	  /* Enable us to recognize when a type is created in class context.
	     To do nested classes correctly, this should probably be cleared
	     out when we leave this classes scope.  Currently this in only
	     done in `start_enum'.  */

	  pushtag (name, ref);
	  if (flag_cadillac)
	    cadillac_start_enum (ref);
	}
      else if (tag_code == exception_type)
	{
	  ref = make_lang_type (code);
	  /* Enable us to recognize when an exception type is created in
	     class context.  To do nested classes correctly, this should
	     probably be cleared out when we leave this class's scope.  */
	  CLASSTYPE_DECLARED_EXCEPTION (ref) = 1;
	  pushtag (name, ref);
	  if (flag_cadillac)
	    cadillac_start_struct (ref);
	}
      else
	{
	  extern tree pending_vtables;
	  struct binding_level *old_b = class_binding_level;
	  int needs_writing;

	  ref = make_lang_type (code);

	  /* Record how to set the visibility of this class's
	     virtual functions.  If write_virtuals == 2 or 3, then
	     inline virtuals are ``extern inline''.  */
	  switch (write_virtuals)
	    {
	    case 0:
	    case 1:
	      needs_writing = 1;
	      break;
	    case 2:
	      needs_writing = !! value_member (name, pending_vtables);
	      break;
	    case 3:
	      needs_writing
		= ! (CLASSTYPE_INTERFACE_ONLY (ref) || CLASSTYPE_INTERFACE_UNKNOWN (ref));
	      break;
	    default:
	      needs_writing = 0;
	    }

	  CLASSTYPE_VTABLE_NEEDS_WRITING (ref) = needs_writing;

#ifdef NONNESTED_CLASSES
	  /* Class types don't nest the way enums do.  */
	  class_binding_level = (struct binding_level *)0;
#endif
	  pushtag (name, ref);
	  class_binding_level = old_b;

	  if (flag_cadillac)
	    cadillac_start_struct (ref);
	}
    }
  else
    {
      if (IS_AGGR_TYPE_CODE (code))
	{
	  if (IS_AGGR_TYPE (ref)
	      && ((tag_code == exception_type)
		  != (CLASSTYPE_DECLARED_EXCEPTION (ref) == 1)))
	    {
	      error ("type `%s' is both exception and aggregate type",
		     IDENTIFIER_POINTER (name));
	      CLASSTYPE_DECLARED_EXCEPTION (ref) = (tag_code == exception_type);
	    }
	}

      /* If it no longer looks like a nested type, make sure it's
	 in global scope.  */
      if (b == global_binding_level && !class_binding_level
	  && IDENTIFIER_GLOBAL_VALUE (name) == NULL_TREE)
	IDENTIFIER_GLOBAL_VALUE (name) = TYPE_NAME (ref);

      if (binfo)
	{
	  tree tt1 = binfo;
	  tree tt2 = TYPE_BINFO_BASETYPES (ref);

	  if (TYPE_BINFO_BASETYPES (ref))
	    for (i = 0; tt1; i++, tt1 = TREE_CHAIN (tt1))
	      if (TREE_VALUE (tt1) != TYPE_IDENTIFIER (BINFO_TYPE (TREE_VEC_ELT (tt2, i))))
		{
		  error ("redeclaration of derivation chain of type `%s'",
			 IDENTIFIER_POINTER (name));
		  break;
		}

	  if (tt1 == NULL_TREE)
	    /* The user told us something we already knew.  */
	    goto just_return;

	  /* In C++, since these migrate into the global scope, we must
	     build them on the permanent obstack.  */
	  end_temporary_allocation ();
	}
    }

  if (binfo)
    {
      /* In the declaration `A : X, Y, ... Z' we mark all the types
	 (A, X, Y, ..., Z) so we can check for duplicates.  */
      tree binfos;

      SET_CLASSTYPE_MARKED (ref);
      BINFO_BASETYPES (TYPE_BINFO (ref)) = binfos = make_tree_vec (len);

      for (i = 0; binfo; binfo = TREE_CHAIN (binfo))
	{
	  /* The base of a derived struct is public.  */
	  int via_public = (tag_code != class_type
			    || TREE_PURPOSE (binfo) == (tree)visibility_public
			    || TREE_PURPOSE (binfo) == (tree)visibility_public_virtual);
	  int via_protected = TREE_PURPOSE (binfo) == (tree)visibility_protected;
	  int via_virtual = (TREE_PURPOSE (binfo) == (tree)visibility_private_virtual
			     || TREE_PURPOSE (binfo) == (tree)visibility_public_virtual
			     || TREE_PURPOSE (binfo) == (tree)visibility_default_virtual);
	  tree basetype = TREE_TYPE (TREE_VALUE (binfo));
	  tree base_binfo;

	  GNU_xref_hier (IDENTIFIER_POINTER (name),
			 IDENTIFIER_POINTER (TREE_VALUE (binfo)),
			 via_public, via_virtual, 0);

	  if (basetype && TREE_CODE (basetype) == TYPE_DECL)
	    basetype = TREE_TYPE (basetype);
	  if (!basetype || TREE_CODE (basetype) != RECORD_TYPE)
	    {
	      error ("base type `%s' fails to be a struct or class type",
		     IDENTIFIER_POINTER (TREE_VALUE (binfo)));
	      continue;
	    }
#if 1
	  /* This code replaces similar code in layout_basetypes.  */
	  else if (TYPE_SIZE (basetype) == NULL_TREE)
	    {
	      error_with_aggr_type (basetype, "base class `%s' has incomplete type");
	      continue;
	    }
#endif
	  else
	    {
	      if (CLASSTYPE_MARKED (basetype))
		{
		  if (basetype == ref)
		    error_with_aggr_type (basetype, "recursive type `%s' undefined");
		  else
		    error_with_aggr_type (basetype, "duplicate base type `%s' invalid");
		  continue;
		}

 	      /* Note that the BINFO records which describe individual
 		 inheritances are *not* shared in the lattice!  They
 		 cannot be shared because a given baseclass may be
 		 inherited with different `accessibility' by different
 		 derived classes.  (Each BINFO record describing an
 		 individual inheritance contains flags which say what
 		 the `accessibility' of that particular inheritance is.)  */
  
	      base_binfo = make_binfo (integer_zero_node, basetype,
				  TYPE_BINFO_VTABLE (basetype),
 				  TYPE_BINFO_VIRTUALS (basetype), 0);
 
	      TREE_VEC_ELT (binfos, i) = base_binfo;
	      TREE_VIA_PUBLIC (base_binfo) = via_public;
 	      TREE_VIA_PROTECTED (base_binfo) = via_protected;
	      TREE_VIA_VIRTUAL (base_binfo) = via_virtual;

	      SET_CLASSTYPE_MARKED (basetype);
#if 0
/* XYZZY TEST VIRTUAL BASECLASSES */
if (CLASSTYPE_N_BASECLASSES (basetype) == NULL_TREE
    && TYPE_HAS_DEFAULT_CONSTRUCTOR (basetype)
    && via_virtual == 0)
  {
    warning ("making type `%s' a virtual baseclass",
	     TYPE_NAME_STRING (basetype));
    via_virtual = 1;
  }
#endif
	      /* We are free to modify these bits because they are meaningless
		 at top level, and BASETYPE is a top-level type.  */
	      if (via_virtual || TYPE_USES_VIRTUAL_BASECLASSES (basetype))
		{
		  TYPE_USES_VIRTUAL_BASECLASSES (ref) = 1;
		  TYPE_USES_COMPLEX_INHERITANCE (ref) = 1;
		}

	      TYPE_GETS_ASSIGNMENT (ref) |= TYPE_GETS_ASSIGNMENT (basetype);
	      TYPE_OVERLOADS_METHOD_CALL_EXPR (ref) |= TYPE_OVERLOADS_METHOD_CALL_EXPR (basetype);
	      TREE_GETS_NEW (ref) |= TREE_GETS_NEW (basetype);
	      TREE_GETS_DELETE (ref) |= TREE_GETS_DELETE (basetype);
	      CLASSTYPE_LOCAL_TYPEDECLS (ref) |= CLASSTYPE_LOCAL_TYPEDECLS (basetype);
	      i += 1;
	    }
	}
      if (i)
	TREE_VEC_LENGTH (binfos) = i;
      else
	BINFO_BASETYPES (TYPE_BINFO (ref)) = NULL_TREE;

      if (i > 1)
	TYPE_USES_MULTIPLE_INHERITANCE (ref) = 1;
      else if (i == 1)
	TYPE_USES_MULTIPLE_INHERITANCE (ref)
	  = TYPE_USES_MULTIPLE_INHERITANCE (BINFO_TYPE (TREE_VEC_ELT (binfos, 0)));
      if (TYPE_USES_MULTIPLE_INHERITANCE (ref))
	TYPE_USES_COMPLEX_INHERITANCE (ref) = 1;

      /* Unmark all the types.  */
      while (--i >= 0)
	CLEAR_CLASSTYPE_MARKED (BINFO_TYPE (TREE_VEC_ELT (binfos, i)));
      CLEAR_CLASSTYPE_MARKED (ref);
    }

 just_return:

  /* Until the type is defined, tentatively accept whatever
     structure tag the user hands us.  */
  if (TYPE_SIZE (ref) == NULL_TREE
      && ref != current_class_type
      /* Have to check this, in case we have contradictory tag info.  */
      && IS_AGGR_TYPE_CODE (TREE_CODE (ref)))
    {
      if (tag_code == class_type)
	CLASSTYPE_DECLARED_CLASS (ref) = 1;
      else if (tag_code == record_type)
	CLASSTYPE_DECLARED_CLASS (ref) = 0;
    }

  pop_obstacks ();

  return ref;
}

static tree current_local_enum = NULL_TREE;

/* Begin compiling the definition of an enumeration type.
   NAME is its name (or null if anonymous).
   Returns the type object, as yet incomplete.
   Also records info about it so that build_enumerator
   may be used to declare the individual values as they are read.  */

tree
start_enum (name)
     tree name;
{
  register tree enumtype = NULL_TREE;
  struct binding_level *b
    = (class_binding_level ? class_binding_level : current_binding_level);

  /* If this is the real definition for a previous forward reference,
     fill in the contents in the same object that used to be the
     forward reference.  */

  if (name != NULL_TREE)
    enumtype = lookup_tag (ENUMERAL_TYPE, name, b, 1);

  if (enumtype == NULL_TREE || TREE_CODE (enumtype) != ENUMERAL_TYPE)
    {
      enumtype = make_node (ENUMERAL_TYPE);
      pushtag (name, enumtype);
    }

  if (current_class_type)
    TREE_ADDRESSABLE (b->tags) = 1;
  current_local_enum = NULL_TREE;

  if (TYPE_VALUES (enumtype) != NULL_TREE)
    {
      /* This enum is a named one that has been declared already.  */
      error ("redeclaration of `enum %s'", IDENTIFIER_POINTER (name));

      /* Completely replace its old definition.
	 The old enumerators remain defined, however.  */
      TYPE_VALUES (enumtype) = NULL_TREE;
    }

  /* Initially, set up this enum as like `int'
     so that we can create the enumerators' declarations and values.
     Later on, the precision of the type may be changed and
     it may be laid out again.  */

  TYPE_PRECISION (enumtype) = TYPE_PRECISION (integer_type_node);
  TYPE_SIZE (enumtype) = NULL_TREE;
  fixup_unsigned_type (enumtype);

  /* We copy this value because enumerated type constants
     are really of the type of the enumerator, not integer_type_node.  */
  enum_next_value = copy_node (integer_zero_node);

  GNU_xref_decl (current_function_decl, enumtype);
  return enumtype;
}

/* After processing and defining all the values of an enumeration type,
   install their decls in the enumeration type and finish it off.
   ENUMTYPE is the type object and VALUES a list of name-value pairs.
   Returns ENUMTYPE.  */

tree
finish_enum (enumtype, values)
     register tree enumtype, values;
{
  register tree pair;
  register HOST_WIDE_INT maxvalue = 0;
  register HOST_WIDE_INT minvalue = 0;
  register HOST_WIDE_INT i;

  TYPE_VALUES (enumtype) = values;

  /* Calculate the maximum value of any enumerator in this type.  */

  if (values)
    {
      /* Speed up the main loop by performing some precalculations */

      HOST_WIDE_INT value = TREE_INT_CST_LOW (TREE_VALUE (values));
      TREE_TYPE (TREE_VALUE (values)) = enumtype;
      minvalue = maxvalue = value;
      
      for (pair = TREE_CHAIN (values); pair; pair = TREE_CHAIN (pair))
	{
	  value = TREE_INT_CST_LOW (TREE_VALUE (pair));
	  if (value > maxvalue)
	    maxvalue = value;
	  else if (value < minvalue)
	    minvalue = value;
	  TREE_TYPE (TREE_VALUE (pair)) = enumtype;
	}
    }

  if (flag_short_enums)
    {
      /* Determine the precision this type needs, lay it out, and define it.  */

      for (i = maxvalue; i; i >>= 1)
	TYPE_PRECISION (enumtype)++;

      if (!TYPE_PRECISION (enumtype))
	TYPE_PRECISION (enumtype) = 1;

      /* Cancel the laying out previously done for the enum type,
	 so that fixup_unsigned_type will do it over.  */
      TYPE_SIZE (enumtype) = NULL_TREE;

      fixup_unsigned_type (enumtype);
    }

  TREE_INT_CST_LOW (TYPE_MAX_VALUE (enumtype)) = maxvalue;

  /* An enum can have some negative values; then it is signed.  */
  if (minvalue < 0)
    {
      TREE_INT_CST_LOW (TYPE_MIN_VALUE (enumtype)) = minvalue;
      TREE_INT_CST_HIGH (TYPE_MIN_VALUE (enumtype)) = -1;
      TREE_UNSIGNED (enumtype) = 0;
    }
  if (flag_cadillac)
    cadillac_finish_enum (enumtype);

  /* Finish debugging output for this type.  */
#if 0
  /* @@ Do we ever generate generate ENUMERAL_TYPE nodes for which debugging
     information should *not* be generated?  I think not.  */
  if (! DECL_IGNORED_P (TYPE_NAME (enumtype)))
#endif
    rest_of_type_compilation (enumtype, global_bindings_p ());

  return enumtype;
}

/* Build and install a CONST_DECL for one value of the
   current enumeration type (one that was begun with start_enum).
   Return a tree-list containing the name and its value.
   Assignment of sequential values by default is handled here.  */

tree
build_enumerator (name, value)
     tree name, value;
{
  tree decl, result;
  /* Change this to zero if we find VALUE is not shareable.  */
  int shareable = 1;

  /* Remove no-op casts from the value.  */
  if (value)
    STRIP_TYPE_NOPS (value);

  /* Validate and default VALUE.  */
  if (value != NULL_TREE)
    {
      if (TREE_READONLY_DECL_P (value))
	{
	  value = decl_constant_value (value);
	  shareable = 0;
	}

      if (TREE_CODE (value) != INTEGER_CST)
	{
	  error ("enumerator value for `%s' not integer constant",
		 IDENTIFIER_POINTER (name));
	  value = NULL_TREE;
	}
    }
  /* The order of things is reversed here so that we
     can check for possible sharing of enum values,
     to keep that from happening.  */
  /* Default based on previous value.  */
  if (value == NULL_TREE)
    value = enum_next_value;

  /* Remove no-op casts from the value.  */
  if (value)
    STRIP_TYPE_NOPS (value);

  /* Make up for hacks in cp-lex.c.  */
  if (value == integer_zero_node)
    value = build_int_2 (0, 0);
  else if (value == integer_one_node)
    value = build_int_2 (1, 0);
  else if (TREE_CODE (value) == INTEGER_CST
	   && (shareable == 0
	       || TREE_CODE (TREE_TYPE (value)) == ENUMERAL_TYPE))
    {
      value = copy_node (value);
      TREE_TYPE (value) = integer_type_node;
    }

  result = saveable_tree_cons (name, value, NULL_TREE);

  /* C++ associates enums with global, function, or class declarations.  */
  if (current_class_type == NULL_TREE || current_function_decl != NULL_TREE)
    {
      /* Create a declaration for the enum value name.  */

      decl = build_decl (CONST_DECL, name, integer_type_node);
      DECL_INITIAL (decl) = value;

      pushdecl (decl);
      GNU_xref_decl (current_function_decl, decl);
    }

  if (current_class_type)
    {
      /* class-local enum declaration */
      decl = build_lang_field_decl (CONST_DECL, name, integer_type_node);
      DECL_INITIAL (decl) = value;
      TREE_READONLY (decl) = 1;
      pushdecl_class_level (decl);
      TREE_CHAIN (decl) = current_local_enum;
      current_local_enum = decl;
    }

  /* Set basis for default for next value.  */
  enum_next_value = build_binary_op_nodefault (PLUS_EXPR, value,
					       integer_one_node, PLUS_EXPR);
  if (enum_next_value == integer_one_node)
    enum_next_value = copy_node (enum_next_value);

  return result;
}

tree
grok_enum_decls (type, decl)
     tree type, decl;
{
  tree d = current_local_enum;
  
  if (d == NULL_TREE)
    return decl;
  
  while (1)
    {
      TREE_TYPE (d) = type;
      if (TREE_CHAIN (d) == NULL_TREE)
	{
	  TREE_CHAIN (d) = decl;
	  break;
	}
      d = TREE_CHAIN (d);
    }

  decl = current_local_enum;
  current_local_enum = NULL_TREE;
  
  return decl;
}

/* Create the FUNCTION_DECL for a function definition.
   DECLSPECS and DECLARATOR are the parts of the declaration;
   they describe the function's name and the type it returns,
   but twisted together in a fashion that parallels the syntax of C.

   This function creates a binding context for the function body
   as well as setting up the FUNCTION_DECL in current_function_decl.

   Returns 1 on success.  If the DECLARATOR is not suitable for a function
   (it defines a datum instead), we return 0, which tells
   yyparse to report a parse error.

   For C++, we must first check whether that datum makes any sense.
   For example, "class A local_a(1,2);" means that variable local_a
   is an aggregate of type A, which should have a constructor
   applied to it with the argument list [1, 2].

   @@ There is currently no way to retrieve the storage
   @@ allocated to FUNCTION (or all of its parms) if we return
   @@ something we had previously.  */

int
start_function (declspecs, declarator, raises, pre_parsed_p)
     tree declarator, declspecs, raises;
     int pre_parsed_p;
{
  extern tree EHS_decl;
  tree decl1, olddecl;
  tree ctype = NULL_TREE;
  tree fntype;
  tree restype;
  extern int have_extern_spec;
  extern int used_extern_spec;
  int doing_friend = 0;

  if (flag_handle_exceptions && EHS_decl == NULL_TREE)
    init_exception_processing_1 ();

  /* Sanity check.  */
  my_friendly_assert (TREE_VALUE (void_list_node) == void_type_node, 160);
  my_friendly_assert (TREE_CHAIN (void_list_node) == NULL_TREE, 161);

  /* Assume, until we see it does. */
  current_function_returns_value = 0;
  current_function_returns_null = 0;
  warn_about_return_type = 0;
  current_extern_inline = 0;
  current_function_assigns_this = 0;
  current_function_just_assigned_this = 0;
  current_function_parms_stored = 0;
  original_result_rtx = NULL_RTX;
  current_function_obstack_index = 0;
  current_function_obstack_usage = 0;

  clear_temp_name ();

  /* This should only be done once on the top most decl. */
  if (have_extern_spec && !used_extern_spec)
    {
      declspecs = decl_tree_cons (NULL_TREE, get_identifier ("extern"), declspecs);
      used_extern_spec = 1;
    }

  if (pre_parsed_p)
    {
      decl1 = declarator;
      last_function_parms = DECL_ARGUMENTS (decl1);
      last_function_parm_tags = NULL_TREE;
      fntype = TREE_TYPE (decl1);
      if (TREE_CODE (fntype) == METHOD_TYPE)
	ctype = TYPE_METHOD_BASETYPE (fntype);

      /* ANSI C++ June 5 1992 WP 11.4.5.  A friend function defined in a
	 class is in the (lexical) scope of the class in which it is
	 defined.  */
      if (!ctype && DECL_FRIEND_P (decl1))
	{
	  ctype = TREE_TYPE (TREE_CHAIN (decl1));

	  /* CTYPE could be null here if we're dealing with a template;
	     for example, `inline friend float foo()' inside a template
	     will have no CTYPE set.  */
	  if (ctype && TREE_CODE (ctype) != RECORD_TYPE)
	    ctype = NULL_TREE;
	  else
	    doing_friend = 1;
	}

      if ( !(DECL_VINDEX (decl1)
	     && write_virtuals >= 2
	     && CLASSTYPE_VTABLE_NEEDS_WRITING (ctype)))
	current_extern_inline = TREE_PUBLIC (decl1) && DECL_INLINE (decl1);

      raises = TYPE_RAISES_EXCEPTIONS (fntype);

      /* In a fcn definition, arg types must be complete.  */
      require_complete_types_for_parms (last_function_parms);
    }
  else
    {
      decl1 = grokdeclarator (declarator, declspecs, FUNCDEF, 1, raises);
      /* If the declarator is not suitable for a function definition,
	 cause a syntax error.  */
      if (decl1 == NULL_TREE || TREE_CODE (decl1) != FUNCTION_DECL) return 0;

      fntype = TREE_TYPE (decl1);

      restype = TREE_TYPE (fntype);
      if (IS_AGGR_TYPE (restype)
	  && ! CLASSTYPE_GOT_SEMICOLON (restype))
	{
	  error_with_aggr_type (restype, "semicolon missing after declaration of `%s'");
	  shadow_tag (build_tree_list (NULL_TREE, restype));
	  CLASSTYPE_GOT_SEMICOLON (restype) = 1;
	  if (TREE_CODE (fntype) == FUNCTION_TYPE)
	    fntype = build_function_type (integer_type_node,
					  TYPE_ARG_TYPES (fntype));
	  else
	    fntype = build_cplus_method_type (build_type_variant (TYPE_METHOD_BASETYPE (fntype), TREE_READONLY (decl1), TREE_SIDE_EFFECTS (decl1)),
					      integer_type_node,
					      TYPE_ARG_TYPES (fntype));
	  TREE_TYPE (decl1) = fntype;
	}

      if (TREE_CODE (fntype) == METHOD_TYPE)
	ctype = TYPE_METHOD_BASETYPE (fntype);
      else if (IDENTIFIER_LENGTH (DECL_NAME (decl1)) == 4
	       && ! strcmp (IDENTIFIER_POINTER (DECL_NAME (decl1)), "main")
	       && DECL_CONTEXT (decl1) == NULL_TREE)
	{
	  /* If this doesn't return integer_type, complain.  */
	  if (TREE_TYPE (TREE_TYPE (decl1)) != integer_type_node)
	    {
	      warning ("return type for `main' changed to integer type");
	      TREE_TYPE (decl1) = fntype = default_function_type;
	    }
	  warn_about_return_type = 0;
	}
    }

  /* Warn if function was previously implicitly declared
     (but not if we warned then).  */
  if (! warn_implicit
      && IDENTIFIER_IMPLICIT_DECL (DECL_NAME (decl1)) != NULL_TREE)
    warning_with_decl (IDENTIFIER_IMPLICIT_DECL (DECL_NAME (decl1)),
		       "`%s' implicitly declared before its definition");

  current_function_decl = decl1;

  if (flag_cadillac)
    cadillac_start_function (decl1);
  else
    announce_function (decl1);

  if (TYPE_SIZE (TREE_TYPE (fntype)) == NULL_TREE)
    {
      if (IS_AGGR_TYPE (TREE_TYPE (fntype)))
	error_with_aggr_type (TREE_TYPE (fntype),
			      "return-type `%s' is an incomplete type");
      else
	error ("return-type is an incomplete type");

      /* Make it return void instead, but don't change the
	 type of the DECL_RESULT, in case we have a named return value.  */
      if (ctype)
	TREE_TYPE (decl1)
	  = build_cplus_method_type (build_type_variant (ctype,
							 TREE_READONLY (decl1),
							 TREE_SIDE_EFFECTS (decl1)),
				     void_type_node,
				     FUNCTION_ARG_CHAIN (decl1));
      else
	TREE_TYPE (decl1)
	  = build_function_type (void_type_node,
				 TYPE_ARG_TYPES (TREE_TYPE (decl1)));
      DECL_RESULT (decl1) = build_decl (RESULT_DECL, 0, TREE_TYPE (fntype));
    }

  if (warn_about_return_type)
    warning ("return-type defaults to `int'");

  /* Make the init_value nonzero so pushdecl knows this is not tentative.
     error_mark_node is replaced below (in poplevel) with the BLOCK.  */
  DECL_INITIAL (decl1) = error_mark_node;

  /* Didn't get anything from C.  */
  olddecl = NULL_TREE;

  /* This function exists in static storage.
     (This does not mean `static' in the C sense!)  */
  TREE_STATIC (decl1) = 1;

  /* If this function belongs to an interface, it is public.
     If it belongs to someone else's interface, it is also external.
     It doesn't matter whether it's inline or not.  */
  if (interface_unknown == 0)
    {
      TREE_PUBLIC (decl1) = 1;
      DECL_EXTERNAL (decl1) = (interface_only
			       || (DECL_INLINE (decl1)
				   && ! flag_implement_inlines));
    }
  else
    /* This is a definition, not a reference.
       So normally clear DECL_EXTERNAL.
       However, `extern inline' acts like a declaration except for
       defining how to inline.  So set DECL_EXTERNAL in that case.  */
    DECL_EXTERNAL (decl1) = current_extern_inline;

  /* Now see if this is the implementation of a function
     declared with "C" linkage.  */
  if (ctype == NULL_TREE && current_lang_name == lang_name_cplusplus
      && !DECL_CONTEXT (decl1))
    {
      olddecl = lookup_name_current_level (DECL_NAME (decl1));
      if (olddecl && TREE_CODE (olddecl) != FUNCTION_DECL)
	olddecl = NULL_TREE;
      if (olddecl && DECL_NAME (decl1) != DECL_NAME (olddecl))
	{
	  /* Collision between user and internal naming scheme.  */
	  olddecl = lookup_name_current_level (DECL_ASSEMBLER_NAME (decl1));
	  if (olddecl == NULL_TREE)
	    olddecl = decl1;
	}
      if (olddecl && olddecl != decl1
	  && DECL_NAME (decl1) == DECL_NAME (olddecl))
	{
	  if (TREE_CODE (olddecl) == FUNCTION_DECL
	      && decls_match (decl1, olddecl))
	    {
	      olddecl = DECL_MAIN_VARIANT (olddecl);
	      /* The following copy is needed to handle forcing a function's
		 linkage to obey the linkage of the original decl.  */
	      DECL_ASSEMBLER_NAME (decl1) = DECL_ASSEMBLER_NAME (olddecl);
	      DECL_OVERLOADED (decl1) = DECL_OVERLOADED (olddecl);
	      if (DECL_INITIAL (olddecl))
		redeclaration_error_message (decl1, olddecl);
	      if (! duplicate_decls (decl1, olddecl))
		my_friendly_abort (19);
	      decl1 = olddecl;
	    }
	  else
	    olddecl = NULL_TREE;
	}
    }

  /* Record the decl so that the function name is defined.
     If we already have a decl for this name, and it is a FUNCTION_DECL,
     use the old decl.  */

  if (olddecl)
    current_function_decl = olddecl;
  else if (pre_parsed_p == 0)
    {
      current_function_decl = pushdecl (decl1);
      if (TREE_CODE (current_function_decl) == TREE_LIST)
	{
	  /* @@ revert to modified original declaration.  */
	  decl1 = DECL_MAIN_VARIANT (decl1);
	  current_function_decl = decl1;
	}
      else
	{
	  decl1 = current_function_decl;
	  DECL_MAIN_VARIANT (decl1) = decl1;
	}
      fntype = TREE_TYPE (decl1);
    }
  else
    current_function_decl = decl1;

  if (DECL_OVERLOADED (decl1))
    decl1 = push_overloaded_decl (decl1, 1);

  if (ctype != NULL_TREE && DECL_STATIC_FUNCTION_P (decl1))
    {
      if (TREE_CODE (fntype) == METHOD_TYPE)
	TREE_TYPE (decl1) = fntype
	  = build_function_type (TREE_TYPE (fntype),
				 TREE_CHAIN (TYPE_ARG_TYPES (fntype)));
      last_function_parms = TREE_CHAIN (last_function_parms);
      DECL_ARGUMENTS (decl1) = last_function_parms;
      ctype = NULL_TREE;
    }
  restype = TREE_TYPE (fntype);

  pushlevel (0);
  current_binding_level->parm_flag = 1;

  /* Save the parm names or decls from this function's declarator
     where store_parm_decls will find them.  */
  current_function_parms = last_function_parms;
  current_function_parm_tags = last_function_parm_tags;

  GNU_xref_function (decl1, current_function_parms);

  make_function_rtl (decl1);

  if (ctype)
    {
      pushclass (ctype, 1);

      /* If we're compiling a friend function, neither of the variables
	 current_class_decl nor current_class_type will have values.  */
      if (! doing_friend)
	{
	  /* We know that this was set up by `grokclassfn'.
	     We do not wait until `store_parm_decls', since evil
	     parse errors may never get us to that point.  Here
	     we keep the consistency between `current_class_type'
	     and `current_class_decl'.  */
	  current_class_decl = last_function_parms;
	  my_friendly_assert (current_class_decl != NULL_TREE
		  && TREE_CODE (current_class_decl) == PARM_DECL, 162);
	  if (TREE_CODE (TREE_TYPE (current_class_decl)) == POINTER_TYPE)
	    {
	      tree variant = TREE_TYPE (TREE_TYPE (current_class_decl));
	      if (CLASSTYPE_INST_VAR (ctype) == NULL_TREE)
		{
		  /* Can't call build_indirect_ref here, because it has special
		     logic to return C_C_D given this argument.  */
		  C_C_D = build1 (INDIRECT_REF, current_class_type, current_class_decl);
		  CLASSTYPE_INST_VAR (ctype) = C_C_D;
		}
	      else
		{
		  C_C_D = CLASSTYPE_INST_VAR (ctype);
		  /* `current_class_decl' is different for every
		     function we compile.  */
		  TREE_OPERAND (C_C_D, 0) = current_class_decl;
		}
	      TREE_READONLY (C_C_D) = TYPE_READONLY (variant);
	      TREE_SIDE_EFFECTS (C_C_D) = TYPE_VOLATILE (variant);
	      TREE_THIS_VOLATILE (C_C_D) = TYPE_VOLATILE (variant);
	    }
	  else
	    C_C_D = current_class_decl;
	}
    }
  else
    {
      if (DECL_STATIC_FUNCTION_P (decl1))
	pushclass (DECL_CONTEXT (decl1), 2);
      else
	push_memoized_context (0, 1);
    }

  /* Allocate further tree nodes temporarily during compilation
     of this function only.  Tiemann moved up here from bottom of fn.  */
  temporary_allocation ();

  /* Promote the value to int before returning it.  */
  if (C_PROMOTING_INTEGER_TYPE_P (restype))
    {
      /* It retains unsignedness if traditional or if it isn't
	 really getting wider.  */
      if (TREE_UNSIGNED (restype)
	  && (flag_traditional
	      || TYPE_PRECISION (restype)
		   == TYPE_PRECISION (integer_type_node)))
	restype = unsigned_type_node;
      else
	restype = integer_type_node;
    }
  if (DECL_RESULT (decl1) == NULL_TREE)
    DECL_RESULT (decl1) = build_decl (RESULT_DECL, 0, restype);

  if (DESTRUCTOR_NAME_P (DECL_ASSEMBLER_NAME (decl1)))
    {
      dtor_label = build_decl (LABEL_DECL, NULL_TREE, NULL_TREE);
      ctor_label = NULL_TREE;
    }
  else
    {
      dtor_label = NULL_TREE;
      if (DECL_CONSTRUCTOR_P (decl1))
	ctor_label = build_decl (LABEL_DECL, NULL_TREE, NULL_TREE);
    }

  /* If this fcn was already referenced via a block-scope `extern' decl
     (or an implicit decl), propagate certain information about the usage.  */
  if (TREE_ADDRESSABLE (DECL_ASSEMBLER_NAME (decl1)))
    TREE_ADDRESSABLE (decl1) = 1;

  return 1;
}

/* Store the parameter declarations into the current function declaration.
   This is called after parsing the parameter declarations, before
   digesting the body of the function.

   Also install to binding contour return value identifier, if any.  */

void
store_parm_decls ()
{
  register tree fndecl = current_function_decl;
  register tree parm;
  int parms_have_cleanups = 0;
  tree eh_decl;

  /* This is either a chain of PARM_DECLs (when a prototype is used).  */
  tree specparms = current_function_parms;

  /* This is a list of types declared among parms in a prototype.  */
  tree parmtags = current_function_parm_tags;

  /* This is a chain of any other decls that came in among the parm
     declarations.  If a parm is declared with  enum {foo, bar} x;
     then CONST_DECLs for foo and bar are put here.  */
  tree nonparms = NULL_TREE;

  if (current_binding_level == global_binding_level)
    fatal ("parse errors have confused me too much");

  /* Initialize RTL machinery.  */
  init_function_start (fndecl, input_filename, lineno);

  /* Declare __FUNCTION__ and __PRETTY_FUNCTION__ for this function.  */
  declare_function_name ();

  /* Create a binding level for the parms.  */
  expand_start_bindings (0);

  /* Prepare to catch raises, if appropriate.  */
  if (flag_handle_exceptions)
    {
      /* Get this cleanup to be run last, since it
	 is a call to `longjmp'.  */
      setup_exception_throw_decl ();
      eh_decl = current_binding_level->names;
      current_binding_level->names = TREE_CHAIN (current_binding_level->names);
    }
  if (flag_handle_exceptions)
    expand_start_try (integer_one_node, 0, 1);

  if (specparms != NULL_TREE)
    {
      /* This case is when the function was defined with an ANSI prototype.
	 The parms already have decls, so we need not do anything here
	 except record them as in effect
	 and complain if any redundant old-style parm decls were written.  */

      register tree next;

      /* Must clear this because it might contain TYPE_DECLs declared
	 at class level.  */
      storedecls (NULL_TREE);
      for (parm = nreverse (specparms); parm; parm = next)
	{
	  next = TREE_CHAIN (parm);
	  if (TREE_CODE (parm) == PARM_DECL)
	    {
	      tree cleanup = maybe_build_cleanup (parm);
	      if (DECL_NAME (parm) == NULL_TREE)
		{
#if 0
		  error_with_decl (parm, "parameter name omitted");
#else
		  /* for C++, this is not an error.  */
		  pushdecl (parm);
#endif
		}
	      else if (TYPE_MAIN_VARIANT (TREE_TYPE (parm)) == void_type_node)
		error_with_decl (parm, "parameter `%s' declared void");
	      else
		{
		  /* Now fill in DECL_REFERENCE_SLOT for any of the parm decls.
		     A parameter is assumed not to have any side effects.
		     If this should change for any reason, then this
		     will have to wrap the bashed reference type in a save_expr.
		     
		     Also, if the parameter type is declared to be an X
		     and there is an X(X&) constructor, we cannot lay it
		     into the stack (any more), so we make this parameter
		     look like it is really of reference type.  Functions
		     which pass parameters to this function will know to
		     create a temporary in their frame, and pass a reference
		     to that.  */

		  if (TREE_CODE (TREE_TYPE (parm)) == REFERENCE_TYPE
		      && TYPE_SIZE (TREE_TYPE (TREE_TYPE (parm))))
		    SET_DECL_REFERENCE_SLOT (parm, convert_from_reference (parm));

		  pushdecl (parm);
		}
	      if (cleanup)
		{
		  expand_decl (parm);
		  expand_decl_cleanup (parm, cleanup);
		  parms_have_cleanups = 1;
		}
	    }
	  else
	    {
	      /* If we find an enum constant or a type tag,
		 put it aside for the moment.  */
	      TREE_CHAIN (parm) = NULL_TREE;
	      nonparms = chainon (nonparms, parm);
	    }
	}

      /* Get the decls in their original chain order
	 and record in the function.  This is all and only the
	 PARM_DECLs that were pushed into scope by the loop above.  */
      DECL_ARGUMENTS (fndecl) = getdecls ();

      storetags (chainon (parmtags, gettags ()));
    }
  else
    DECL_ARGUMENTS (fndecl) = NULL_TREE;

  /* Now store the final chain of decls for the arguments
     as the decl-chain of the current lexical scope.
     Put the enumerators in as well, at the front so that
     DECL_ARGUMENTS is not modified.  */

  storedecls (chainon (nonparms, DECL_ARGUMENTS (fndecl)));

  /* Initialize the RTL code for the function.  */
  DECL_SAVED_INSNS (fndecl) = NULL_RTX;
  expand_function_start (fndecl, parms_have_cleanups);

  if (flag_handle_exceptions)
    {
      /* Make the throw decl visible at this level, just
	 not in the way of the parameters.  */
      pushdecl (eh_decl);
      expand_decl_init (eh_decl);
    }

  /* Create a binding contour which can be used to catch
     cleanup-generated temporaries.  Also, if the return value needs or
     has initialization, deal with that now.  */
  if (parms_have_cleanups)
    {
      pushlevel (0);
      expand_start_bindings (0);
    }

  current_function_parms_stored = 1;

  if (flag_gc)
    {
      maybe_gc_cleanup = build_tree_list (NULL_TREE, error_mark_node);
      expand_decl_cleanup (NULL_TREE, maybe_gc_cleanup);
    }

  /* If this function is `main', emit a call to `__main'
     to run global initializers, etc.  */
  if (DECL_NAME (fndecl)
      && IDENTIFIER_LENGTH (DECL_NAME (fndecl)) == 4
      && strcmp (IDENTIFIER_POINTER (DECL_NAME (fndecl)), "main") == 0
      && DECL_CONTEXT (fndecl) == NULL_TREE)
    {
      expand_main_function ();

      if (flag_gc)
	expand_expr (build_function_call (lookup_name (get_identifier ("__gc_main"), 0), NULL_TREE),
		     0, VOIDmode, 0);

      if (flag_dossier)
	output_builtin_tdesc_entries ();
    }
}

/* Bind a name and initialization to the return value of
   the current function.  */
void
store_return_init (return_id, init)
     tree return_id, init;
{
  tree decl = DECL_RESULT (current_function_decl);

  if (pedantic)
    /* Give this error as many times as there are occurrences,
       so that users can use Emacs compilation buffers to find
       and fix all such places.  */
    error ("ANSI C++ does not permit named return values");

  if (return_id != NULL_TREE)
    {
      if (DECL_NAME (decl) == NULL_TREE)
	{
	  DECL_NAME (decl) = return_id;
	  DECL_ASSEMBLER_NAME (decl) = return_id;
	}
      else
	error ("return identifier `%s' already in place",
	       IDENTIFIER_POINTER (DECL_NAME (decl)));
    }

  /* Can't let this happen for constructors.  */
  if (DECL_CONSTRUCTOR_P (current_function_decl))
    {
      error ("can't redefine default return value for constructors");
      return;
    }

  /* If we have a named return value, put that in our scope as well.  */
  if (DECL_NAME (decl) != NULL_TREE)
    {
      /* If this named return value comes in a register,
	 put it in a pseudo-register.  */
      if (DECL_REGISTER (decl))
	{
	  original_result_rtx = DECL_RTL (decl);
	  DECL_RTL (decl) = gen_reg_rtx (DECL_MODE (decl));
	}

      /* Let `finish_decl' know that this initializer is ok.  */
      DECL_INITIAL (decl) = init;
      pushdecl (decl);
      finish_decl (decl, init, 0, 0);
    }
}

/* Generate code for default X(X&) constructor.  */
static void
build_default_constructor (fndecl)
     tree fndecl;
{
  int i = CLASSTYPE_N_BASECLASSES (current_class_type);
  tree parm = TREE_CHAIN (DECL_ARGUMENTS (fndecl));
  tree fields = TYPE_FIELDS (current_class_type);
  tree binfos = TYPE_BINFO_BASETYPES (current_class_type);

  if (TYPE_USES_VIRTUAL_BASECLASSES (current_class_type))
    parm = TREE_CHAIN (parm);
  parm = DECL_REFERENCE_SLOT (parm);

  while (--i >= 0)
    {
      tree basetype = TREE_VEC_ELT (binfos, i);
      if (TYPE_GETS_INIT_REF (basetype))
	{
	  tree name = TYPE_NAME (basetype);
	  if (TREE_CODE (name) == TYPE_DECL)
	    name = DECL_NAME (name);
	  current_base_init_list = tree_cons (name, parm, current_base_init_list);
	}
    }
  for (; fields; fields = TREE_CHAIN (fields))
    {
      tree name, init;
      if (TREE_STATIC (fields))
	continue;
      if (TREE_CODE (fields) != FIELD_DECL)
	continue;
      if (DECL_NAME (fields))
	{
	  if (VFIELD_NAME_P (DECL_NAME (fields)))
	    continue;
	  if (VBASE_NAME_P (DECL_NAME (fields)))
	    continue;

	  /* True for duplicate members.  */
	  if (IDENTIFIER_CLASS_VALUE (DECL_NAME (fields)) != fields)
	    continue;
	}

      init = build (COMPONENT_REF, TREE_TYPE (fields), parm, fields);

      if (TREE_ANON_UNION_ELEM (fields))
	name = build (COMPONENT_REF, TREE_TYPE (fields), C_C_D, fields);
      else
	{
	  name = DECL_NAME (fields);
	  init = build_tree_list (NULL_TREE, init);
	}

      current_member_init_list
	= tree_cons (name, init, current_member_init_list);
    }
}


/* Finish up a function declaration and compile that function
   all the way to assembler language output.  The free the storage
   for the function definition.

   This is called after parsing the body of the function definition.
   LINENO is the current line number.

   C++: CALL_POPLEVEL is non-zero if an extra call to poplevel
   (and expand_end_bindings) must be made to take care of the binding
   contour for the base initializers.  This is only relevant for
   constructors.  */

void
finish_function (lineno, call_poplevel)
     int lineno;
     int call_poplevel;
{
  register tree fndecl = current_function_decl;
  tree fntype, ctype = NULL_TREE;
  rtx head, last_parm_insn, mark;
  extern int sets_exception_throw_decl;
  /* Label to use if this function is supposed to return a value.  */
  tree no_return_label = NULL_TREE;

  /* When we get some parse errors, we can end up without a
     current_function_decl, so cope.  */
  if (fndecl == NULL_TREE)
    return;

  fntype = TREE_TYPE (fndecl);

/*  TREE_READONLY (fndecl) = 1;
    This caused &foo to be of type ptr-to-const-function
    which then got a warning when stored in a ptr-to-function variable.  */

  /* This happens on strange parse errors.  */
  if (! current_function_parms_stored)
    {
      call_poplevel = 0;
      store_parm_decls ();
    }

  if (write_symbols != NO_DEBUG && TREE_CODE (fntype) != METHOD_TYPE)
    {
      tree ttype = target_type (fntype);
      tree parmdecl;

      if (IS_AGGR_TYPE (ttype))
	/* Let debugger know it should output info for this type.  */
	note_debug_info_needed (ttype);

      for (parmdecl = DECL_ARGUMENTS (fndecl); parmdecl; parmdecl = TREE_CHAIN (parmdecl))
	{
	  ttype = target_type (TREE_TYPE (parmdecl));
	  if (IS_AGGR_TYPE (ttype))
	    /* Let debugger know it should output info for this type.  */
	    note_debug_info_needed (ttype);
	}
    }

  /* Clean house because we will need to reorder insns here.  */
  do_pending_stack_adjust ();

  if (dtor_label)
    {
      tree binfo = TYPE_BINFO (current_class_type);
      tree cond = integer_one_node;
      tree exprstmt, vfields;
      tree in_charge_node = lookup_name (in_charge_identifier, 0);
      tree virtual_size;
      int ok_to_optimize_dtor = 0;

      if (current_function_assigns_this)
	cond = build (NE_EXPR, integer_type_node,
		      current_class_decl, integer_zero_node);
      else
	{
	  int n_baseclasses = CLASSTYPE_N_BASECLASSES (current_class_type);

	  /* If this destructor is empty, then we don't need to check
	     whether `this' is NULL in some cases.  */
	  mark = get_last_insn ();
	  last_parm_insn = get_first_nonparm_insn ();

	  if ((flag_this_is_variable & 1) == 0)
	    ok_to_optimize_dtor = 1;
	  else if (mark == last_parm_insn)
	    ok_to_optimize_dtor
	      = (n_baseclasses == 0
		 || (n_baseclasses == 1
		     && TYPE_HAS_DESTRUCTOR (TYPE_BINFO_BASETYPE (current_class_type, 0))));
	}

      /* These initializations might go inline.  Protect
	 the binding level of the parms.  */
      pushlevel (0);

      if (current_function_assigns_this)
	{
	  current_function_assigns_this = 0;
	  current_function_just_assigned_this = 0;
	}

      /* Generate the code to call destructor on base class.
	 If this destructor belongs to a class with virtual
	 functions, then set the virtual function table
	 pointer to represent the type of our base class.  */

      /* This side-effect makes call to `build_delete' generate the
	 code we have to have at the end of this destructor.  */
      TYPE_HAS_DESTRUCTOR (current_class_type) = 0;

      /* These are two cases where we cannot delegate deletion.  */
      if (TYPE_USES_VIRTUAL_BASECLASSES (current_class_type)
	  || TREE_GETS_DELETE (current_class_type))
	exprstmt = build_delete (current_class_type, C_C_D, integer_zero_node,
				 LOOKUP_NONVIRTUAL|LOOKUP_DESTRUCTOR, 0, 0);
      else
	exprstmt = build_delete (current_class_type, C_C_D, in_charge_node,
				 LOOKUP_NONVIRTUAL|LOOKUP_DESTRUCTOR, 0, 0);

      /* If we did not assign to this, then `this' is non-zero at
	 the end of a destructor.  As a special optimization, don't
	 emit test if this is an empty destructor.  If it does nothing,
	 it does nothing.  If it calls a base destructor, the base
	 destructor will perform the test.  */

      if (exprstmt != error_mark_node
	  && (TREE_CODE (exprstmt) != NOP_EXPR
	      || TREE_OPERAND (exprstmt, 0) != integer_zero_node
	      || TYPE_USES_VIRTUAL_BASECLASSES (current_class_type)))
	{
	  expand_label (dtor_label);
	  if (cond != integer_one_node)
	    expand_start_cond (cond, 0);
	  if (exprstmt != void_zero_node)
	    /* Don't call `expand_expr_stmt' if we're not going to do
	       anything, since -Wall will give a diagnostic.  */
	    expand_expr_stmt (exprstmt);

	  /* Run destructor on all virtual baseclasses.  */
	  if (TYPE_USES_VIRTUAL_BASECLASSES (current_class_type))
	    {
	      tree vbases = nreverse (copy_list (CLASSTYPE_VBASECLASSES (current_class_type)));
	      expand_start_cond (build (BIT_AND_EXPR, integer_type_node,
					in_charge_node, integer_two_node), 0);
	      while (vbases)
		{
		  if (TYPE_NEEDS_DESTRUCTOR (BINFO_TYPE (vbases)))
		    {
		      tree ptr = convert_pointer_to_vbase (vbases, current_class_decl);
		      expand_expr_stmt (build_delete (TYPE_POINTER_TO (BINFO_TYPE (vbases)),
						      ptr, integer_zero_node,
						      LOOKUP_NONVIRTUAL|LOOKUP_DESTRUCTOR|LOOKUP_HAS_IN_CHARGE, 0, 0));
		    }
		  vbases = TREE_CHAIN (vbases);
		}
	      expand_end_cond ();
	    }

	  do_pending_stack_adjust ();
	  if (cond != integer_one_node)
	    expand_end_cond ();
	}

      TYPE_HAS_DESTRUCTOR (current_class_type) = 1;

      virtual_size = c_sizeof (current_class_type);

      /* At the end, call delete if that's what's requested.  */
      if (TREE_GETS_DELETE (current_class_type))
	/* This NOP_EXPR means we are in a static call context.  */
	exprstmt =
	  build_method_call
	    (build1 (NOP_EXPR,
		     TYPE_POINTER_TO (current_class_type), error_mark_node),
	     ansi_opname[(int) DELETE_EXPR],
	     tree_cons (NULL_TREE, current_class_decl,
			build_tree_list (NULL_TREE, virtual_size)),
	     NULL_TREE, LOOKUP_NORMAL);
      else if (TYPE_USES_VIRTUAL_BASECLASSES (current_class_type))
	exprstmt = build_x_delete (ptr_type_node, current_class_decl, 0,
				   virtual_size);
      else
	exprstmt = NULL_TREE;

      if (exprstmt)
	{
	  cond = build (BIT_AND_EXPR, integer_type_node,
			in_charge_node, integer_one_node);
	  expand_start_cond (cond, 0);
	  expand_expr_stmt (exprstmt);
	  expand_end_cond ();
	}

      /* End of destructor.  */
      poplevel (2, 0, 0);

      /* Back to the top of destructor.  */
      /* Dont execute destructor code if `this' is NULL.  */
      mark = get_last_insn ();
      last_parm_insn = get_first_nonparm_insn ();
      if (last_parm_insn == NULL_RTX)
	last_parm_insn = mark;
      else
	last_parm_insn = previous_insn (last_parm_insn);

      /* Make all virtual function table pointers point to CURRENT_CLASS_TYPE's
	 virtual function tables.  */
      if (CLASSTYPE_VFIELDS (current_class_type))
	{
	  for (vfields = CLASSTYPE_VFIELDS (current_class_type);
	       TREE_CHAIN (vfields);
	       vfields = TREE_CHAIN (vfields))
	    {
	      tree vf_decl = current_class_decl;
	      /* ??? This may need to be a loop if there are multiple
		 levels of replication.  */
	      if (VF_BINFO_VALUE (vfields))
		vf_decl = convert_pointer_to (VF_BINFO_VALUE (vfields), vf_decl);
	      if (vf_decl != error_mark_node)
		{
		  /* It is one of these two, or a combination...  */
		  /* basically speaking, I want to get down to the right
		     VF_BASETYPE_VALUE (vfields) */
#if 0
		  if (VF_NORMAL_VALUE (vfields) != VF_DERIVED_VALUE (vfields))
		    warning ("hum, wonder if I am doing the right thing");
#endif
		  expand_expr_stmt (build_virtual_init (binfo,
							get_binfo (VF_BASETYPE_VALUE (vfields),
								   get_binfo (VF_DERIVED_VALUE (vfields), binfo, 0), 0),
							vf_decl));
		}
	    }
	  expand_expr_stmt (build_virtual_init (binfo, binfo,
						current_class_decl));
	}
      if (TYPE_USES_VIRTUAL_BASECLASSES (current_class_type))
	expand_expr_stmt (build_vbase_vtables_init (binfo, binfo,
						    C_C_D, current_class_decl, 0));
      if (! ok_to_optimize_dtor)
	{
	  cond = build_binary_op (NE_EXPR,
				  current_class_decl, integer_zero_node, 1);
	  expand_start_cond (cond, 0);
	}
      if (mark != get_last_insn ())
	reorder_insns (next_insn (mark), get_last_insn (), last_parm_insn);
      if (! ok_to_optimize_dtor)
	expand_end_cond ();
    }
  else if (current_function_assigns_this)
    {
      /* Does not need to call emit_base_init, because
	 that is done (if needed) just after assignment to this
	 is seen.  */

      if (DECL_CONSTRUCTOR_P (current_function_decl))
	{
	  expand_label (ctor_label);
	  ctor_label = NULL_TREE;

	  if (call_poplevel)
	    {
	      tree decls = getdecls ();
	      if (flag_handle_exceptions == 2)
		deactivate_exception_cleanups ();
	      expand_end_bindings (decls, decls != NULL_TREE, 0);
	      poplevel (decls != NULL_TREE, 0, 0);
	    }
	  c_expand_return (current_class_decl);
	}
      else if (TYPE_MAIN_VARIANT (TREE_TYPE (
			DECL_RESULT (current_function_decl))) != void_type_node
	       && return_label != NULL_RTX)
	no_return_label = build_decl (LABEL_DECL, NULL_TREE, NULL_TREE);

      current_function_assigns_this = 0;
      current_function_just_assigned_this = 0;
      base_init_insns = NULL_RTX;
    }
  else if (DECL_CONSTRUCTOR_P (fndecl))
    {
      tree allocated_this;
      tree cond, thenclause;
      /* Allow constructor for a type to get a new instance of the object
	 using `build_new'.  */
      tree abstract_virtuals = CLASSTYPE_ABSTRACT_VIRTUALS (current_class_type);
      CLASSTYPE_ABSTRACT_VIRTUALS (current_class_type) = NULL_TREE;

      DECL_RETURNS_FIRST_ARG (fndecl) = 1;

      if (flag_this_is_variable > 0)
	{
	  cond = build_binary_op (EQ_EXPR,
				  current_class_decl, integer_zero_node, 1);
	  thenclause = build_modify_expr (current_class_decl, NOP_EXPR,
					  build_new (NULL_TREE, current_class_type, void_type_node, 0));
	  if (flag_handle_exceptions == 2)
	    {
	      tree cleanup, cleanup_deallocate;
	      tree virtual_size;

	      /* This is the size of the virtual object pointed to by
		 allocated_this.  In this case, it is simple.  */
	      virtual_size = c_sizeof (current_class_type);

	      allocated_this = build_decl (VAR_DECL, NULL_TREE, ptr_type_node);
	      DECL_REGISTER (allocated_this) = 1;
	      DECL_INITIAL (allocated_this) = error_mark_node;
	      expand_decl (allocated_this);
	      expand_decl_init (allocated_this);
	      /* How we cleanup `this' if an exception was raised before
		 we are ready to bail out.  */
	      cleanup = TREE_GETS_DELETE (current_class_type)
		? build_opfncall (DELETE_EXPR, LOOKUP_NORMAL, allocated_this, virtual_size, NULL_TREE)
		  /* The size of allocated_this is wrong, and hence the
		     second argument to operator delete will be wrong. */
		  : build_delete (TREE_TYPE (allocated_this), allocated_this,
				  integer_three_node,
				  LOOKUP_NORMAL|LOOKUP_HAS_IN_CHARGE, 1, 0);
	      cleanup_deallocate
		= build_modify_expr (current_class_decl, NOP_EXPR, integer_zero_node);
	      cleanup = tree_cons (NULL_TREE, cleanup,
				   build_tree_list (NULL_TREE, cleanup_deallocate));

	      expand_decl_cleanup (allocated_this,
				   build (COND_EXPR, integer_type_node,
					  build (NE_EXPR, integer_type_node,
						 allocated_this, integer_zero_node),
					  build_compound_expr (cleanup),
					  integer_zero_node));
	    }
	}
      else if (TREE_GETS_NEW (current_class_type))
	/* Just check visibility here.  */
	build_method_call (build1 (NOP_EXPR, TYPE_POINTER_TO (current_class_type), error_mark_node),
			   ansi_opname[(int) NEW_EXPR],
			   build_tree_list (NULL_TREE, integer_zero_node),
			   NULL_TREE, LOOKUP_NORMAL);

      CLASSTYPE_ABSTRACT_VIRTUALS (current_class_type) = abstract_virtuals;

      /* must keep the first insn safe.  */
      head = get_insns ();

      /* this note will come up to the top with us.  */
      mark = get_last_insn ();

      if (flag_this_is_variable > 0)
	{
	  expand_start_cond (cond, 0);
	  expand_expr_stmt (thenclause);
	  if (flag_handle_exceptions == 2)
	    expand_assignment (allocated_this, current_class_decl, 0, 0);
	  expand_end_cond ();
	}

      if (DECL_NAME (fndecl) == NULL_TREE
	  && TREE_CHAIN (DECL_ARGUMENTS (fndecl)) != NULL_TREE)
	build_default_constructor (fndecl);

      /* Emit insns from `emit_base_init' which sets up virtual
	 function table pointer(s).  */
      emit_insns (base_init_insns);
      base_init_insns = NULL_RTX;

      /* This is where the body of the constructor begins.
	 If there were no insns in this function body, then the
	 last_parm_insn is also the last insn.

	 If optimization is enabled, last_parm_insn may move, so
	 we don't hold on to it (across emit_base_init).  */
      last_parm_insn = get_first_nonparm_insn ();
      if (last_parm_insn == NULL_RTX) last_parm_insn = mark;
      else last_parm_insn = previous_insn (last_parm_insn);

      if (mark != get_last_insn ())
	reorder_insns (next_insn (mark), get_last_insn (), last_parm_insn);

      /* This is where the body of the constructor ends.  */
      expand_label (ctor_label);
      ctor_label = NULL_TREE;
      if (flag_handle_exceptions == 2)
	{
	  expand_assignment (allocated_this, integer_zero_node, 0, 0);
	  if (call_poplevel)
	    deactivate_exception_cleanups ();
	}

      pop_implicit_try_blocks (NULL_TREE);

      if (call_poplevel)
	{
	  expand_end_bindings (getdecls (), 1, 0);
	  poplevel (1, 1, 0);
	}

      c_expand_return (current_class_decl);

      current_function_assigns_this = 0;
      current_function_just_assigned_this = 0;
    }
  else if (IDENTIFIER_LENGTH (DECL_NAME (fndecl)) == 4
	   && ! strcmp (IDENTIFIER_POINTER (DECL_NAME (fndecl)), "main")
	   && DECL_CONTEXT (fndecl) == NULL_TREE)
    {
      /* Make it so that `main' always returns 0 by default.  */
#ifdef VMS
      c_expand_return (integer_one_node);
#else
      c_expand_return (integer_zero_node);
#endif
    }
  else if (return_label != NULL_RTX
	   && current_function_return_value == NULL_TREE
	   && ! DECL_NAME (DECL_RESULT (current_function_decl)))
    no_return_label = build_decl (LABEL_DECL, NULL_TREE, NULL_TREE);

  if (flag_gc)
    expand_gc_prologue_and_epilogue ();

  /* That's the end of the vtable decl's life.  Need to mark it such
     if doing stupid register allocation.

     Note that current_vtable_decl is really an INDIRECT_REF
     on top of a VAR_DECL here.  */
  if (obey_regdecls && current_vtable_decl)
    use_variable (DECL_RTL (TREE_OPERAND (current_vtable_decl, 0)));

  /* If this function is supposed to return a value, ensure that
     we do not fall into the cleanups by mistake.  The end of our
     function will look like this:

	user code (may have return stmt somewhere)
	goto no_return_label
	cleanup_label:
	cleanups
	goto return_label
	no_return_label:
	NOTE_INSN_FUNCTION_END
	return_label:
	things for return

     If the user omits a return stmt in the USER CODE section, we
     will have a control path which reaches NOTE_INSN_FUNCTION_END.
     Otherwise, we won't.  */
  if (no_return_label)
    {
      DECL_CONTEXT (no_return_label) = fndecl;
      DECL_INITIAL (no_return_label) = error_mark_node;
      DECL_SOURCE_FILE (no_return_label) = input_filename;
      DECL_SOURCE_LINE (no_return_label) = lineno;
      expand_goto (no_return_label);
    }

  if (cleanup_label)
    {
      /* remove the binding contour which is used
	 to catch cleanup-generated temporaries.  */
      expand_end_bindings (0, 0, 0);
      poplevel (0, 0, 0);
    }

  if (cleanup_label)
    /* Emit label at beginning of cleanup code for parameters.  */
    emit_label (cleanup_label);

#if 1
  /* Cheap hack to get better code from GNU C++.  Remove when cse is fixed.  */
  if (exception_throw_decl && sets_exception_throw_decl == 0)
    expand_assignment (exception_throw_decl, integer_zero_node, 0, 0);
#endif

  if (flag_handle_exceptions)
    {
      expand_end_try ();
      expand_start_except (0, 0);
      expand_end_except ();
    }
  expand_end_bindings (0, 0, 0);

  /* Get return value into register if that's where it's supposed to be.  */
  if (original_result_rtx)
    fixup_result_decl (DECL_RESULT (fndecl), original_result_rtx);

  /* Finish building code that will trigger warnings if users forget
     to make their functions return values.  */
  if (no_return_label || cleanup_label)
    emit_jump (return_label);
  if (no_return_label)
    {
      /* We don't need to call `expand_*_return' here because we
	 don't need any cleanups here--this path of code is only
	 for error checking purposes.  */
      expand_label (no_return_label);
    }

  /* reset scope for C++: if we were in the scope of a class,
     then when we finish this function, we are not longer so.
     This cannot be done until we know for sure that no more
     class members will ever be referenced in this function
     (i.e., calls to destructors).  */
  if (current_class_name)
    {
      ctype = current_class_type;
      popclass (1);
    }
  else
    pop_memoized_context (1);

  /* Forget about all overloaded functions defined in
     this scope which go away.  */
  while (overloads_to_forget)
    {
      IDENTIFIER_GLOBAL_VALUE (TREE_PURPOSE (overloads_to_forget))
	= TREE_VALUE (overloads_to_forget);
      overloads_to_forget = TREE_CHAIN (overloads_to_forget);
    }

  /* Generate rtl for function exit.  */
  expand_function_end (input_filename, lineno);

  /* This must come after expand_function_end because cleanups might
     have declarations (from inline functions) that need to go into
     this function's blocks.  */
  if (current_binding_level->parm_flag != 1)
    my_friendly_abort (122);
  poplevel (1, 0, 1);

  /* Must mark the RESULT_DECL as being in this function.  */
  DECL_CONTEXT (DECL_RESULT (fndecl)) = DECL_INITIAL (fndecl);

  /* Obey `register' declarations if `setjmp' is called in this fn.  */
  if (flag_traditional && current_function_calls_setjmp)
    setjmp_protect (DECL_INITIAL (fndecl));

  /* Set the BLOCK_SUPERCONTEXT of the outermost function scope to point
     to the FUNCTION_DECL node itself.  */
  BLOCK_SUPERCONTEXT (DECL_INITIAL (fndecl)) = fndecl;

  /* So we can tell if jump_optimize sets it to 1.  */
  can_reach_end = 0;

  /* ??? Compensate for Sun brain damage in dealing with data segments
     of PIC code.  */
  if (flag_pic
      && (DECL_CONSTRUCTOR_P (fndecl)
	  || DESTRUCTOR_NAME_P (DECL_ASSEMBLER_NAME (fndecl)))
      && CLASSTYPE_NEEDS_VIRTUAL_REINIT (TYPE_METHOD_BASETYPE (fntype)))
    DECL_INLINE (fndecl) = 0;

  if (DECL_EXTERNAL (fndecl)
      /* This function is just along for the ride.  If we can make
	 it inline, that's great.  Otherwise, just punt it.  */
      && (DECL_INLINE (fndecl) == 0
	  || flag_no_inline
	  || function_cannot_inline_p (fndecl)))
    {
      extern int rtl_dump_and_exit;
      int old_rtl_dump_and_exit = rtl_dump_and_exit;
      int inline_spec = DECL_INLINE (fndecl);

      /* This throws away the code for FNDECL.  */
      rtl_dump_and_exit = 1;
      /* This throws away the memory of the code for FNDECL.  */
      if (flag_no_inline)
	DECL_INLINE (fndecl) = 0;
      rest_of_compilation (fndecl);
      rtl_dump_and_exit = old_rtl_dump_and_exit;
      DECL_INLINE (fndecl) = inline_spec;
    }
  else
    {
      /* Run the optimizers and output the assembler code for this function.  */
      rest_of_compilation (fndecl);
    }

  if (ctype && TREE_ASM_WRITTEN (fndecl))
    note_debug_info_needed (ctype);

  current_function_returns_null |= can_reach_end;

  /* Since we don't normally go through c_expand_return for constructors,
     this normally gets the wrong value.
     Also, named return values have their return codes emitted after
     NOTE_INSN_FUNCTION_END, confusing jump.c.  */
  if (DECL_CONSTRUCTOR_P (fndecl)
      || DECL_NAME (DECL_RESULT (fndecl)) != NULL_TREE)
    current_function_returns_null = 0;

  if (TREE_THIS_VOLATILE (fndecl) && current_function_returns_null)
    warning ("`volatile' function does return");
  else if (warn_return_type && current_function_returns_null
	   && TYPE_MAIN_VARIANT (TREE_TYPE (fntype)) != void_type_node)
    {
      /* If this function returns non-void and control can drop through,
	 complain.  */
      pedwarn ("control reaches end of non-void function");
    }
  /* With just -W, complain only if function returns both with
     and without a value.  */
  else if (extra_warnings
	   && current_function_returns_value && current_function_returns_null)
    warning ("this function may return with or without a value");

  /* Free all the tree nodes making up this function.  */
  /* Switch back to allocating nodes permanently
     until we start another function.  */
  permanent_allocation ();

  if (flag_cadillac)
    cadillac_finish_function (fndecl);

  if (DECL_SAVED_INSNS (fndecl) == NULL_RTX)
    {
      /* Stop pointing to the local nodes about to be freed.  */
      /* But DECL_INITIAL must remain nonzero so we know this
	 was an actual function definition.  */
      DECL_INITIAL (fndecl) = error_mark_node;
      if (! DECL_CONSTRUCTOR_P (fndecl)
	  || !TYPE_USES_VIRTUAL_BASECLASSES (TYPE_METHOD_BASETYPE (fntype)))
	DECL_ARGUMENTS (fndecl) = NULL_TREE;
    }

  /* Let the error reporting routines know that we're outside a function.  */
  current_function_decl = NULL_TREE;
  named_label_uses = NULL_TREE;
  clear_anon_parm_name ();
}

/* Create the FUNCTION_DECL for a function definition.
   LINE1 is the line number that the definition absolutely begins on.
   LINE2 is the line number that the name of the function appears on.
   DECLSPECS and DECLARATOR are the parts of the declaration;
   they describe the function's name and the type it returns,
   but twisted together in a fashion that parallels the syntax of C.

   This function creates a binding context for the function body
   as well as setting up the FUNCTION_DECL in current_function_decl.

   Returns a FUNCTION_DECL on success.

   If the DECLARATOR is not suitable for a function (it defines a datum
   instead), we return 0, which tells yyparse to report a parse error.

   May return void_type_node indicating that this method is actually
   a friend.  See grokfield for more details.

   Came here with a `.pushlevel' .

   DO NOT MAKE ANY CHANGES TO THIS CODE WITHOUT MAKING CORRESPONDING
   CHANGES TO CODE IN `grokfield'.  */
tree
start_method (declspecs, declarator, raises)
     tree declarator, declspecs, raises;
{
  tree fndecl = grokdeclarator (declarator, declspecs, MEMFUNCDEF, 0, raises);

  /* Something too ugly to handle.  */
  if (fndecl == NULL_TREE)
    return NULL_TREE;

  /* Pass friends other than inline friend functions back.  */
  if (TYPE_MAIN_VARIANT (fndecl) == void_type_node)
    return fndecl;

  if (TREE_CODE (fndecl) != FUNCTION_DECL)
    /* Not a function, tell parser to report parse error.  */
    return NULL_TREE;

  if (DECL_IN_AGGR_P (fndecl))
    {
      if (IDENTIFIER_ERROR_LOCUS (DECL_ASSEMBLER_NAME (fndecl)) != current_class_type)
	{
	  if (DECL_CONTEXT (fndecl))
	    error_with_decl (fndecl, "`%s' is already defined in class %s",
			     TYPE_NAME_STRING (DECL_CONTEXT (fndecl)));
	}
      return void_type_node;
    }

  /* If we're expanding a template, a function must be explicitly declared
     inline if we're to compile it now.  If it isn't, we have to wait to see
     whether it's needed, and whether an override exists.  */
  if (flag_default_inline && !processing_template_defn)
    DECL_INLINE (fndecl) = 1;

  /* We read in the parameters on the maybepermanent_obstack,
     but we won't be getting back to them until after we
     may have clobbered them.  So the call to preserve_data
     will keep them safe.  */
  preserve_data ();

  if (! DECL_FRIEND_P (fndecl))
    {
      if (DECL_CHAIN (fndecl) != NULL_TREE)
	{
	  /* Need a fresh node here so that we don't get circularity
	     when we link these together.  If FNDECL was a friend, then
	     `pushdecl' does the right thing, which is nothing wrt its
	     current value of DECL_CHAIN.  */
	  fndecl = copy_node (fndecl);
	}
      if (TREE_CHAIN (fndecl))
	{
	  fndecl = copy_node (fndecl);
	  TREE_CHAIN (fndecl) = NULL_TREE;
	}

      if (DECL_CONSTRUCTOR_P (fndecl))
	grok_ctor_properties (current_class_type, fndecl);
      else if (IDENTIFIER_OPNAME_P (DECL_NAME (fndecl)))
	grok_op_properties (fndecl, DECL_VIRTUAL_P (fndecl));
    }

  finish_decl (fndecl, NULL_TREE, NULL_TREE, 0);

  /* Make a place for the parms */
  pushlevel (0);
  current_binding_level->parm_flag = 1;
  
  DECL_IN_AGGR_P (fndecl) = 1;
  return fndecl;
}

/* Go through the motions of finishing a function definition.
   We don't compile this method until after the whole class has
   been processed.

   FINISH_METHOD must return something that looks as though it
   came from GROKFIELD (since we are defining a method, after all).

   This is called after parsing the body of the function definition.
   STMTS is the chain of statements that makes up the function body.

   DECL is the ..._DECL that `start_method' provided.  */

tree
finish_method (decl)
     tree decl;
{
  register tree fndecl = decl;
  tree old_initial;
  tree context = DECL_CONTEXT (fndecl);

  register tree link;

  if (TYPE_MAIN_VARIANT (decl) == void_type_node)
    return decl;

#ifdef DEBUG_CP_BINDING_LEVELS
  indent_to (stderr, debug_bindings_indentation);
  fprintf (stderr, "finish_method");
  debug_bindings_indentation += 4;
#endif

  old_initial = DECL_INITIAL (fndecl);

  /* Undo the level for the parms (from start_method).
     This is like poplevel, but it causes nothing to be
     saved.  Saving information here confuses symbol-table
     output routines.  Besides, this information will
     be correctly output when this method is actually
     compiled.  */

  /* Clear out the meanings of the local variables of this level;
     also record in each decl which block it belongs to.  */

  for (link = current_binding_level->names; link; link = TREE_CHAIN (link))
    {
      if (DECL_NAME (link) != NULL_TREE)
	IDENTIFIER_LOCAL_VALUE (DECL_NAME (link)) = 0;
      my_friendly_assert (TREE_CODE (link) != FUNCTION_DECL, 163);
      DECL_CONTEXT (link) = NULL_TREE;
    }

  /* Restore all name-meanings of the outer levels
     that were shadowed by this level.  */

  for (link = current_binding_level->shadowed; link; link = TREE_CHAIN (link))
      IDENTIFIER_LOCAL_VALUE (TREE_PURPOSE (link)) = TREE_VALUE (link);
  for (link = current_binding_level->class_shadowed;
       link; link = TREE_CHAIN (link))
    IDENTIFIER_CLASS_VALUE (TREE_PURPOSE (link)) = TREE_VALUE (link);
  for (link = current_binding_level->type_shadowed;
       link; link = TREE_CHAIN (link))
    IDENTIFIER_TYPE_VALUE (TREE_PURPOSE (link)) = TREE_VALUE (link);

  GNU_xref_end_scope ((HOST_WIDE_INT) current_binding_level,
		      (HOST_WIDE_INT) current_binding_level->level_chain,
		      current_binding_level->parm_flag,
		      current_binding_level->keep,
		      current_binding_level->tag_transparent);

  pop_binding_level ();

  DECL_INITIAL (fndecl) = old_initial;
#if 0
  /* tiemann would like this, but is causes String.cc to not compile. */
  if (DECL_FRIEND_P (fndecl) || DECL_CONTEXT (fndecl) != current_class_type)
#else
  if (DECL_FRIEND_P (fndecl))
#endif
    {
      CLASSTYPE_INLINE_FRIENDS (current_class_type)
	= tree_cons (NULL_TREE, fndecl, CLASSTYPE_INLINE_FRIENDS (current_class_type));
      decl = void_type_node;
    }
#if 0
  /* Work in progress, 9/17/92. */
  else if (context != current_class_type
	   && TREE_CHAIN (context) != NULL_TREE
	   && !DESTRUCTOR_NAME_P (DECL_ASSEMBLER_NAME (decl)))
    {
      /* Don't allow them to declare a function like this:
       class A {
       public:
         class B {
         public:
           int f();
         };
         int B::f() {}
       };

       Note we can get in here if it's a friend (in which case we'll
       avoid lots of nasty cruft), or it's a destructor.  Compensate.
      */
      tree tmp = DECL_ARGUMENTS (TREE_CHAIN (context));
      if (tmp
	  && TREE_CODE (tmp) == IDENTIFIER_NODE
	  && TREE_CHAIN (IDENTIFIER_GLOBAL_VALUE (tmp))
	  && TREE_CODE (TREE_CHAIN (IDENTIFIER_GLOBAL_VALUE (tmp))) == TYPE_DECL)
	{
	  error_with_decl (decl,
			   "qualified name used in declaration of `%s'");
	  /* Make this node virtually unusable in the end.  */
	  TREE_CHAIN (decl) = NULL_TREE;
	}
    }
#endif

#ifdef DEBUG_CP_BINDING_LEVELS
  debug_bindings_indentation -= 4;
#endif

  return decl;
}

/* Called when a new struct TYPE is defined.
   If this structure or union completes the type of any previous
   variable declaration, lay it out and output its rtl.  */

void
hack_incomplete_structures (type)
     tree type;
{
  tree decl;

  if (current_binding_level->n_incomplete == 0)
    return;

  if (!type) /* Don't do this for class templates.  */
    return;

  for (decl = current_binding_level->names; decl; decl = TREE_CHAIN (decl))
    if (TREE_TYPE (decl) == type
	|| (TREE_TYPE (decl)
	    && TREE_CODE (TREE_TYPE (decl)) == ARRAY_TYPE
	    && TREE_TYPE (TREE_TYPE (decl)) == type))
      {
	if (TREE_CODE (decl) == TYPE_DECL)
	  layout_type (TREE_TYPE (decl));
	else
	  {
	    int toplevel = global_binding_level == current_binding_level;
	    if (TREE_CODE (TREE_TYPE (decl)) == ARRAY_TYPE
		&& TREE_TYPE (TREE_TYPE (decl)) == type)
	      layout_type (TREE_TYPE (decl));
	    layout_decl (decl, 0);
	    rest_of_decl_compilation (decl, NULL_PTR, toplevel, 0);
	    if (! toplevel)
	      {
		expand_decl (decl);
		expand_decl_cleanup (decl, maybe_build_cleanup (decl));
		expand_decl_init (decl);
	      }
	  }
	my_friendly_assert (current_binding_level->n_incomplete > 0, 164);
	--current_binding_level->n_incomplete;
      }
}

/* Nonzero if presently building a cleanup.  Needed because
   SAVE_EXPRs are not the right things to use inside of cleanups.
   They are only ever evaluated once, where the cleanup
   might be evaluated several times.  In this case, a later evaluation
   of the cleanup might fill in the SAVE_EXPR_RTL, and it will
   not be valid for an earlier cleanup.  */

int building_cleanup;

/* If DECL is of a type which needs a cleanup, build that cleanup here.
   We don't build cleanups if just going for syntax checking, since
   fixup_cleanups does not know how to not handle them.

   Don't build these on the momentary obstack; they must live
   the life of the binding contour.  */
tree
maybe_build_cleanup (decl)
     tree decl;
{
  tree type = TREE_TYPE (decl);
  if (TYPE_NEEDS_DESTRUCTOR (type))
    {
      int temp = 0, flags = LOOKUP_NORMAL|LOOKUP_DESTRUCTOR;
      tree rval;
      int old_building_cleanup = building_cleanup;
      building_cleanup = 1;

      if (TREE_CODE (decl) != PARM_DECL)
	temp = suspend_momentary ();

      if (TREE_CODE (type) == ARRAY_TYPE)
	rval = decl;
      else
	{
	  mark_addressable (decl);
	  rval = build_unary_op (ADDR_EXPR, decl, 0);
	}

      /* Optimize for space over speed here.  */
      if (! TYPE_USES_VIRTUAL_BASECLASSES (type)
	  || flag_expensive_optimizations)
	flags |= LOOKUP_NONVIRTUAL;

      /* Use TYPE_MAIN_VARIANT so we don't get a warning about
	 calling delete on a `const' variable.  */
      if (TYPE_READONLY (TREE_TYPE (TREE_TYPE (rval))))
	rval = build1 (NOP_EXPR, TYPE_POINTER_TO (TYPE_MAIN_VARIANT (TREE_TYPE (TREE_TYPE (rval)))), rval);

      rval = build_delete (TREE_TYPE (rval), rval, integer_two_node, flags, 0, 0);

      if (TYPE_USES_VIRTUAL_BASECLASSES (type)
	  && ! TYPE_HAS_DESTRUCTOR (type))
	rval = build_compound_expr (tree_cons (NULL_TREE, rval,
					       build_tree_list (NULL_TREE, build_vbase_delete (type, decl))));

      current_binding_level->have_cleanups = 1;
      current_binding_level->more_exceptions_ok = 0;

      if (TREE_CODE (decl) != PARM_DECL)
	resume_momentary (temp);

      building_cleanup = old_building_cleanup;

      return rval;
    }
  return 0;
}

/* Expand a C++ expression at the statement level.
   This is needed to ferret out nodes which have UNKNOWN_TYPE.
   The C++ type checker should get all of these out when
   expressions are combined with other, type-providing, expressions,
   leaving only orphan expressions, such as:

   &class::bar;		/ / takes its address, but does nothing with it.

   */
void
cplus_expand_expr_stmt (exp)
     tree exp;
{
  if (TREE_TYPE (exp) == unknown_type_node)
    {
      if (TREE_CODE (exp) == ADDR_EXPR || TREE_CODE (exp) == TREE_LIST)
	error ("address of overloaded function with no contextual type information");
      else if (TREE_CODE (exp) == COMPONENT_REF)
	warning ("useless reference to a member function name, did you forget the ()?");
    }
  else
    {
      int remove_implicit_immediately = 0;

      if (TREE_CODE (exp) == FUNCTION_DECL)
	{
	  warning_with_decl (exp, "reference, not call, to function `%s'");
	  warning ("at this point in file");
	}
      if (TREE_RAISES (exp))
	{
	  my_friendly_assert (flag_handle_exceptions, 165);
	  if (flag_handle_exceptions == 2)
	    {
	      if (! current_binding_level->more_exceptions_ok)
		{
		  extern struct nesting *nesting_stack, *block_stack;

		  remove_implicit_immediately
		    = (nesting_stack != block_stack);
		  cplus_expand_start_try (1);
		}
	      current_binding_level->have_exceptions = 1;
	    }
	}

      expand_expr_stmt (break_out_cleanups (exp));

      if (remove_implicit_immediately)
	pop_implicit_try_blocks (NULL_TREE);
    }

  /* Clean up any pending cleanups.  This happens when a function call
     returns a cleanup-needing value that nobody uses.  */
  expand_cleanups_to (NULL_TREE);
}

/* When a stmt has been parsed, this function is called.

   Currently, this function only does something within a
   constructor's scope: if a stmt has just assigned to this,
   and we are in a derived class, we call `emit_base_init'.  */

void
finish_stmt ()
{
  extern struct nesting *cond_stack, *loop_stack, *case_stack;

  
  if (current_function_assigns_this
      || ! current_function_just_assigned_this)
    return;
  if (DECL_CONSTRUCTOR_P (current_function_decl))
    {
      /* Constructors must wait until we are out of control
	 zones before calling base constructors.  */
      if (cond_stack || loop_stack || case_stack)
	return;
      emit_insns (base_init_insns);
      check_base_init (current_class_type);
    }
  current_function_assigns_this = 1;

  if (flag_cadillac)
    cadillac_finish_stmt ();
}

void
pop_implicit_try_blocks (decl)
     tree decl;
{
  if (decl)
    {
      my_friendly_assert (current_binding_level->parm_flag == 3, 166);
      current_binding_level->names = TREE_CHAIN (decl);
    }

  while (current_binding_level->parm_flag == 3)
    {
      tree name = get_identifier ("(compiler error)");
      tree orig_ex_type = current_exception_type;
      tree orig_ex_decl = current_exception_decl;
      tree orig_ex_obj = current_exception_object;
      tree decl = cplus_expand_end_try (2);

      /* @@ It would be nice to make all these point
	 to exactly the same handler.  */
      /* Start hidden EXCEPT.  */
      cplus_expand_start_except (name, decl);
      /* reraise ALL.  */
      cplus_expand_reraise (NULL_TREE);
      current_exception_type = orig_ex_type;
      current_exception_decl = orig_ex_decl;
      current_exception_object = orig_ex_obj;
      /* This will reraise for us.  */
      cplus_expand_end_except (error_mark_node);
    }

  if (decl)
    {
      TREE_CHAIN (decl) = current_binding_level->names;
      current_binding_level->names = decl;
    }
}

/* Push a cleanup onto the current binding contour that will cause
   ADDR to be cleaned up, in the case that an exception propagates
   through its binding contour.  */

void
push_exception_cleanup (addr)
     tree addr;
{
  tree decl = build_decl (VAR_DECL, get_identifier (EXCEPTION_CLEANUP_NAME), ptr_type_node);
  tree cleanup;

  decl = pushdecl (decl);
  DECL_REGISTER (decl) = 1;
  store_init_value (decl, addr);
  expand_decl (decl);
  expand_decl_init (decl);

  cleanup = build (COND_EXPR, integer_type_node,
		   build (NE_EXPR, integer_type_node,
			  decl, integer_zero_node),
		   build_delete (TREE_TYPE (addr), decl,
				 lookup_name (in_charge_identifier, 0),
				 LOOKUP_NORMAL|LOOKUP_DESTRUCTOR, 0, 0),
		   integer_zero_node);
  expand_decl_cleanup (decl, cleanup);
}

/* For each binding contour, emit code that deactivates the
   exception cleanups.  All other cleanups are left as they were.  */

static void
deactivate_exception_cleanups ()
{
  struct binding_level *b = current_binding_level;
  tree xyzzy = get_identifier (EXCEPTION_CLEANUP_NAME);
  while (b != class_binding_level)
    {
      if (b->parm_flag == 3)
	{
	  tree decls = b->names;
	  while (decls)
	    {
	      if (DECL_NAME (decls) == xyzzy)
		expand_assignment (decls, integer_zero_node, 0, 0);
	      decls = TREE_CHAIN (decls);
	    }
	}
      b = b->level_chain;
    }
}

/* Change a static member function definition into a FUNCTION_TYPE, instead
   of the METHOD_TYPE that we create when it's originally parsed.  */
void
revert_static_member_fn (fn, decl, argtypes)
     tree *fn, *decl, *argtypes;
{
  tree tmp, function = *fn;

  *argtypes = TREE_CHAIN (*argtypes);
  tmp = build_function_type (TREE_TYPE (function), *argtypes);
  tmp = build_type_variant (tmp, TYPE_READONLY (function),
			    TYPE_VOLATILE (function));
  tmp = build_exception_variant (TYPE_METHOD_BASETYPE (function), tmp,
				 TYPE_RAISES_EXCEPTIONS (function));
  TREE_TYPE (*decl) = tmp;
  *fn = tmp;
  DECL_STATIC_FUNCTION_P (*decl) = 1;
}
