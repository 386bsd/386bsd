/* Definitions for C++ parsing and type checking.
   Copyright (C) 1987, 1993 Free Software Foundation, Inc.
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

#include "cp-class.h"

/* Borrow everything that is C from c-tree.h,
   but do so by copy, not by inclusion, since c-tree.h defines
   lang_identifier.  */

#ifndef PARANOID
#define PARANOID 0
#endif

/* Language-dependent contents of an identifier.  */

struct lang_identifier
{
  struct tree_identifier ignore;
  tree global_value, local_value;
  tree class_value;
  tree class_template_info;
  struct lang_id2 *x;
};

struct lang_id2
{
  tree label_value, implicit_decl;
  tree type_desc, as_list, error_locus;
};

/* To identify to the debug emitters if it should pay attention to the
   flag `-Wtemplate-debugging'.  */
#define HAVE_TEMPLATES 1

/* Macros for access to language-specific slots in an identifier.  */

#if !PARANOID
#define IDENTIFIER_GLOBAL_VALUE(NODE)	\
  (((struct lang_identifier *)(NODE))->global_value)
#define IDENTIFIER_CLASS_VALUE(NODE)	\
  (((struct lang_identifier *)(NODE))->class_value)
#define IDENTIFIER_LOCAL_VALUE(NODE)	\
  (((struct lang_identifier *)(NODE))->local_value)
#define IDENTIFIER_TEMPLATE(NODE)	\
  (((struct lang_identifier *)(NODE))->class_template_info)
#else
#define IDENTIFIER_LANG_SPECIFIC_PTR(NODE) \
  (my_friendly_assert (TREE_CODE (NODE) == IDENTIFIER_NODE, 325),	\
   (struct lang_identifier *) (NODE))
#define IDENTIFIER_GLOBAL_VALUE(NODE)	\
  (((struct lang_identifier *)(NODE))->global_value)
#define IDENTIFIER_CLASS_VALUE(NODE)	\
  (((struct lang_identifier *)(NODE))->class_value)
#define IDENTIFIER_LOCAL_VALUE(NODE)	\
  (((struct lang_identifier *)(NODE))->local_value)
#define IDENTIFIER_TEMPLATE(NODE)	\
  (IDENTIFIER_LANG_SPECIFIC_PTR (NODE) -> class_template_info)
#endif

#if !PARANOID
#define IDENTIFIER_TYPE_VALUE(NODE) (TREE_TYPE (NODE))
#define SET_IDENTIFIER_TYPE_VALUE(NODE,TYPE) (TREE_TYPE (NODE) = TYPE)
#else
#define IDENTIFIER_TYPE_VALUE(NODE) (*IDENTIFIER_TYPE_VALUE_PTR(NODE))
#ifdef __GNUC__
__inline
#endif
static tree * IDENTIFIER_TYPE_VALUE_PTR(NODE) tree NODE; { return
  (my_friendly_assert (TREE_CODE (NODE) == IDENTIFIER_NODE, 326),
   &TREE_TYPE (NODE)) ;}
#define SET_IDENTIFIER_TYPE_VALUE(NODE,TYPE) (IDENTIFIER_TYPE_VALUE(NODE)=TYPE)
#endif
#define IDENTIFIER_HAS_TYPE_VALUE(NODE) (TREE_TYPE (NODE) ? 1 : 0)

#define IDENTIFIER_LABEL_VALUE(NODE)	\
  (((struct lang_identifier *)(NODE))->x \
   ? ((struct lang_identifier *)(NODE))->x->label_value : 0)
#define SET_IDENTIFIER_LABEL_VALUE(NODE,VALUE)	\
  (((struct lang_identifier *)(NODE))->x == 0 ? ((struct lang_identifier *)(NODE))->x = (struct lang_id2 *)perm_calloc (1, sizeof (struct lang_id2)) : 0, \
   ((struct lang_identifier *)(NODE))->x->label_value = (VALUE))
#define IDENTIFIER_IMPLICIT_DECL(NODE)	\
  (((struct lang_identifier *)(NODE))->x \
   ? ((struct lang_identifier *)(NODE))->x->implicit_decl : 0)
#define SET_IDENTIFIER_IMPLICIT_DECL(NODE,VALUE)	\
  (((struct lang_identifier *)(NODE))->x == 0 ? ((struct lang_identifier *)(NODE))->x = (struct lang_id2 *)perm_calloc (1, sizeof (struct lang_id2)) : 0, \
   ((struct lang_identifier *)(NODE))->x->implicit_decl = (VALUE))
#define IDENTIFIER_AS_DESC(NODE)		\
  (((struct lang_identifier *)(NODE))->x \
   ? ((struct lang_identifier *)(NODE))->x->type_desc : 0)
#define SET_IDENTIFIER_AS_DESC(NODE,DESC)	\
  (((struct lang_identifier *)(NODE))->x == 0 ? ((struct lang_identifier *)(NODE))->x = (struct lang_id2 *)perm_calloc (1, sizeof (struct lang_id2)) : 0, \
   ((struct lang_identifier *)(NODE))->x->type_desc = (DESC))
#define IDENTIFIER_AS_LIST(NODE)		\
  (((struct lang_identifier *)(NODE))->x \
   ? ((struct lang_identifier *)(NODE))->x->as_list : 0)
#define SET_IDENTIFIER_AS_LIST(NODE,LIST)	\
  (((struct lang_identifier *)(NODE))->x == 0 ? ((struct lang_identifier *)(NODE))->x = (struct lang_id2 *)perm_calloc (1, sizeof (struct lang_id2)) : 0, \
   ((struct lang_identifier *)(NODE))->x->as_list = (LIST))
#define IDENTIFIER_ERROR_LOCUS(NODE)	\
  (((struct lang_identifier *)(NODE))->x \
   ? ((struct lang_identifier *)(NODE))->x->error_locus : 0)
#define SET_IDENTIFIER_ERROR_LOCUS(NODE,VALUE)	\
  (((struct lang_identifier *)(NODE))->x == 0 ? ((struct lang_identifier *)(NODE))->x = (struct lang_id2 *)perm_calloc (1, sizeof (struct lang_id2)) : 0, \
   ((struct lang_identifier *)(NODE))->x->error_locus = (VALUE))

#define IDENTIFIER_VIRTUAL_P(NODE) TREE_LANG_FLAG_1(NODE)

/* Nonzero if this identifier is the prefix for a mangled C++ operator name.  */
#define IDENTIFIER_OPNAME_P(NODE) TREE_LANG_FLAG_2(NODE)

#define IDENTIFIER_TYPENAME_P(NODE)	\
  (! strncmp (IDENTIFIER_POINTER (NODE),			\
	      IDENTIFIER_POINTER (ansi_opname[(int) TYPE_EXPR]),	\
	      IDENTIFIER_LENGTH (ansi_opname[(int) TYPE_EXPR])))

/* Nonzero means reject anything that ANSI standard C forbids.  */
extern int pedantic;

/* In a RECORD_TYPE or UNION_TYPE, nonzero if any component is read-only.  */
#define C_TYPE_FIELDS_READONLY(type) TYPE_LANG_FLAG_0 (type)

/* If non-zero, a VAR_DECL whose cleanup will cause a throw to the
   next exception handler.  */
extern tree exception_throw_decl;

extern tree double_type_node, long_double_type_node, float_type_node;
extern tree char_type_node, unsigned_char_type_node, signed_char_type_node;
extern tree ptrdiff_type_node;

extern tree short_integer_type_node, short_unsigned_type_node;
extern tree long_integer_type_node, long_unsigned_type_node;
extern tree long_long_integer_type_node, long_long_unsigned_type_node;
extern tree unsigned_type_node;
extern tree string_type_node, char_array_type_node, int_array_type_node;
extern tree wchar_array_type_node;
extern tree wchar_type_node, signed_wchar_type_node, unsigned_wchar_type_node;
extern tree intQI_type_node, unsigned_intQI_type_node;
extern tree intHI_type_node, unsigned_intHI_type_node;
extern tree intSI_type_node, unsigned_intSI_type_node;
extern tree intDI_type_node, unsigned_intDI_type_node;

extern int current_function_returns_value;
extern int current_function_returns_null;
extern tree current_function_return_value;

extern tree ridpointers[];
extern tree ansi_opname[];
extern tree ansi_assopname[];

/* Nonzero means `$' can be in an identifier.  */

extern int dollars_in_ident;

/* Nonzero means allow type mismatches in conditional expressions;
   just make their values `void'.   */

extern int flag_cond_mismatch;

/* Nonzero means don't recognize the keyword `asm'.  */

extern int flag_no_asm;

/* For cross referencing.  */

extern int flag_gnu_xref;

/* For environments where you can use GNU binutils (as, ld in particular).  */

extern int flag_gnu_binutils;

/* Nonzero means ignore `#ident' directives.  */

extern int flag_no_ident;

/* Nonzero means warn about implicit declarations.  */

extern int warn_implicit;

/* Nonzero means warn about function definitions that default the return type
   or that use a null return and have a return-type other than void.  */

extern int warn_return_type, explicit_warn_return_type;

/* Nonzero means give string constants the type `const char *'
   to get extra warnings from them.  These warnings will be too numerous
   to be useful, except in thoroughly ANSIfied programs.  */

extern int warn_write_strings;

/* Nonzero means warn about sizeof(function) or addition/subtraction
   of function pointers.  */

extern int warn_pointer_arith;

/* Nonzero means warn for all old-style non-prototype function decls.  */

extern int warn_strict_prototypes;

/* Nonzero means warn about suggesting putting in ()'s.  */

extern int warn_parentheses;

/* Warn about a subscript that has type char.  */

extern int warn_char_subscripts;

/* Nonzero means warn about pointer casts that can drop a type qualifier
   from the pointer target type.  */

extern int warn_cast_qual;

/* Warn about traditional constructs whose meanings changed in ANSI C.  */

extern int warn_traditional;

/* Nonzero means warn about non virtual destructors in classes that have
   virtual functions. */

extern int warn_nonvdtor;

/* Nonzero means do some things the same way PCC does.  */

extern int flag_traditional;

/* Nonzero means to treat bitfields as unsigned unless they say `signed'.  */

extern int flag_signed_bitfields;

/* 3 means write out only virtuals function tables `defined'
   in this implementation file.
   2 means write out only specific virtual function tables
   and give them (C) public visibility.
   1 means write out virtual function tables and give them
   (C) public visibility.
   0 means write out virtual function tables and give them
   (C) static visibility (default).
   -1 means declare virtual function tables extern.  */

extern int write_virtuals;

/* INTERFACE_ONLY nonzero means that we are in an "interface"
   section of the compiler.  INTERFACE_UNKNOWN nonzero means
   we cannot trust the value of INTERFACE_ONLY.  If INTERFACE_UNKNOWN
   is zero and INTERFACE_ONLY is zero, it means that we are responsible
   for exporting definitions that others might need.  */
extern int interface_only, interface_unknown;

/* Nonzero means we should attempt to elide constructors when possible.  */

extern int flag_elide_constructors;

/* Nonzero means recognize and handle exception handling constructs.  */

extern int flag_handle_exceptions;

/* Nonzero means recognize and handle ansi-style exception handling constructs.  */

extern int flag_ansi_exceptions;

/* Nonzero means that member functions defined in class scope are
   inline by default.  */

extern int flag_default_inline;

/* Nonzero means emit cadillac protocol.  */

extern int flag_cadillac;

/* C++ language-specific tree codes.  */
#define DEFTREECODE(SYM, NAME, TYPE, LENGTH) SYM,
enum cplus_tree_code {
  __DUMMY = LAST_AND_UNUSED_TREE_CODE,
#include "cp-tree.def"
  LAST_CPLUS_TREE_CODE
};
#undef DEFTREECODE

enum languages { lang_c, lang_cplusplus };

/* Macros to make error reporting functions' lives easier.  */
#if !PARANOID
#define TYPE_IDENTIFIER(NODE) (DECL_NAME (TYPE_NAME (NODE)))
#else
#define TYPE_IDENTIFIER(NODE) (*TYPE_IDENTIFIER_PTR (NODE))
#ifdef __GNUC__
__inline
#endif
static tree *
TYPE_IDENTIFIER_PTR(NODE) tree NODE; { return
  (my_friendly_assert (TREE_CODE_CLASS (TREE_CODE (NODE)) == 't', 327),
   &DECL_NAME (TYPE_NAME (NODE))) ;}
#endif

#define TYPE_NAME_STRING(NODE) (IDENTIFIER_POINTER (TYPE_IDENTIFIER (NODE)))
#define TYPE_NAME_LENGTH(NODE) (IDENTIFIER_LENGTH (TYPE_IDENTIFIER (NODE)))

#define TYPE_ASSEMBLER_NAME_STRING(NODE) (IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (TYPE_NAME  (NODE))))
#define TYPE_ASSEMBLER_NAME_LENGTH(NODE) (IDENTIFIER_LENGTH (DECL_ASSEMBLER_NAME (TYPE_NAME (NODE))))

#define IS_AGGR_TYPE_2(TYPE1,TYPE2) \
  (TREE_CODE (TYPE1) == TREE_CODE (TYPE2)	\
   && IS_AGGR_TYPE (TYPE1)&IS_AGGR_TYPE (TYPE2))

/* Macros which might want to be replaced by function calls.  */

#if 1
/* Virtual function addresses can be gotten from a virtual function
   table entry using this macro.  */
#define FNADDR_FROM_VTABLE_ENTRY(ENTRY) \
  TREE_VALUE (TREE_CHAIN (TREE_CHAIN (CONSTRUCTOR_ELTS (ENTRY))))
#define SET_FNADDR_FROM_VTABLE_ENTRY(ENTRY,VALUE) \
  (TREE_VALUE (TREE_CHAIN (TREE_CHAIN (CONSTRUCTOR_ELTS (ENTRY)))) = (VALUE))

#define FUNCTION_ARG_CHAIN(NODE) (TREE_CHAIN (TYPE_ARG_TYPES (TREE_TYPE (NODE))))
#define PROMOTES_TO_AGGR_TYPE(NODE,CODE)	\
  (((CODE) == TREE_CODE (NODE)			\
       && IS_AGGR_TYPE (TREE_TYPE (NODE)))	\
   || IS_AGGR_TYPE (NODE))

#else
#define FNADDR_FROM_VTABLE_ENTRY(ENTRY) (fnaddr_from_vtable_entry (ENTRY))
#define SET_FNADDR_FROM_VTABLE_ENTRY(ENTRY,VALUE) \
  (set_fnaddr_from_vtable_entry (ENTRY, VALUE))
/* #define TYPE_NAME_STRING(NODE) (type_name_string (NODE)) */
#define FUNCTION_ARG_CHAIN(NODE) (function_arg_chain (NODE))
#define PROMOTES_TO_AGGR_TYPE(NODE,CODE) (promotes_to_aggr_type (NODE, CODE))
/* #define IS_AGGR_TYPE_2(TYPE1, TYPE2) (is_aggr_type_2 (TYPE1, TYPE2)) */
#endif
/* Nonzero iff TYPE is uniquely derived from PARENT.  Under MI, PARENT can be an
   ambiguous base class of TYPE, and this macro will be false.  */
#define UNIQUELY_DERIVED_FROM_P(PARENT, TYPE) (get_base_distance (PARENT, TYPE, 0, (tree *)0) >= 0)

enum conversion_type { ptr_conv, constptr_conv, int_conv, real_conv, last_conversion_type };

/* Statistics show that while the GNU C++ compiler may generate
   thousands of different types during a compilation run, it
   generates relatively few (tens) of classtypes.  Because of this,
   it is not costly to store a generous amount of information
   in classtype nodes.  This struct must fill out to a multiple of 4 bytes.  */
struct lang_type
{
  struct
    {
      unsigned has_type_conversion : 1;
      unsigned has_int_conversion : 1;
      unsigned has_float_conversion : 1;
      unsigned has_init_ref : 1;
      unsigned gets_init_ref : 1;
      unsigned gets_init_aggr : 1;
      unsigned has_assignment : 1;
      unsigned gets_assignment : 1;

      unsigned needs_constructor : 1;
      unsigned has_default_ctor : 1;
      unsigned uses_multiple_inheritance : 1;
      unsigned const_needs_init : 1;
      unsigned ref_needs_init : 1;
      unsigned gets_const_init_ref : 1;
      unsigned has_const_assign_ref : 1;
      unsigned gets_const_assign_ref : 1;

      unsigned vtable_needs_writing : 1;
      unsigned has_assign_ref : 1;
      unsigned gets_assign_ref : 1;
      unsigned gets_new : 1;
      unsigned gets_delete : 1;
      unsigned has_call_overloaded : 1;
      unsigned has_array_ref_overloaded : 1;
      unsigned has_arrow_overloaded : 1;

      unsigned local_typedecls : 1;
      unsigned interface_only : 1;
      unsigned interface_unknown : 1;
      unsigned needs_virtual_reinit : 1;
      unsigned declared_exception : 1;
      unsigned declared_class : 1;
      unsigned being_defined : 1;
      unsigned redefined : 1;

      unsigned marked : 1;
      unsigned marked2 : 1;
      unsigned marked3 : 1;
      unsigned marked4 : 1;
      unsigned marked5 : 1;
      unsigned marked6 : 1;
      unsigned use_template : 2;

      unsigned debug_requested : 1;
      unsigned dynamic : 1;
      unsigned has_method_call_overloaded : 1;
      unsigned private_attr : 1;
      unsigned alters_visibilities : 1;
      unsigned got_semicolon : 1;
      unsigned dummy : 1;

      /* The MIPS compiler gets it wrong if this struct also
	 does not fill out to a multiple of 4 bytes.  */
      unsigned n_vancestors : 16;
    } type_flags;

  int cid;
  int n_ancestors;
  int vsize;
  int max_depth;

  union tree_node *vbinfo[2];
  union tree_node *baselink_vec;
  union tree_node *vfields;
  union tree_node *vbases;
  union tree_node *vbase_size;

  union tree_node *tags;
  char *memoized_table_entry;

  char *search_slot;

#ifdef ONLY_INT_FIELDS
  unsigned int mode : 8;
#else
  enum machine_mode mode : 8;
#endif

  unsigned char size_unit;
  unsigned char align;
  unsigned char sep_unit;

  union tree_node *sep;
  union tree_node *size;

  union tree_node *base_init_list;
  union tree_node *abstract_virtuals;
  union tree_node *as_list;
  union tree_node *id_as_list;
  union tree_node *binfo_as_list;
  union tree_node *vtbl_ptr;
  union tree_node *instance_variable;
  union tree_node *friend_classes;

  char *mi_matrix;
  union tree_node *conversions[last_conversion_type];

  union tree_node *dossier;
};

/* Indicates whether a template should be (or has been) expanded for this
   class definition.  0=do, 1=did, 2=don't, 3=didn't.  */
#define CLASSTYPE_USE_TEMPLATE(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.use_template)

/* Fields used for storing information before the class is defined.
   After the class is defined, these fields hold other information.  */

/* List of friends which were defined inline in this class definition.  */
#define CLASSTYPE_INLINE_FRIENDS(NODE) (TYPE_NONCOPIED_PARTS (NODE))

/* Nonzero for _CLASSTYPE means that the _CLASSTYPE either has
   a special meaning for the assignment operator ("operator="),
   or one of its fields (or base members) has a special meaning
   defined.  */
#define TYPE_HAS_ASSIGNMENT(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.has_assignment)
#define TYPE_GETS_ASSIGNMENT(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.gets_assignment)

/* Nonzero for _CLASSTYPE means that operator new and delete are defined,
   respectively.  */
#define TREE_GETS_NEW(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.gets_new)
#define TREE_GETS_DELETE(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.gets_delete)

/* Nonzero for TREE_LIST or _TYPE node means that this node is class-local.  */
#define TREE_NONLOCAL_FLAG(NODE) (TREE_LANG_FLAG_0 (NODE))

/* Nonzero for a _CLASSTYPE node which we know to be private.  */
#define TYPE_PRIVATE_P(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.private_attr)

/* Nonzero means that this _CLASSTYPE node defines ways of converting
   itself to other types.  */
#define TYPE_HAS_CONVERSION(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.has_type_conversion)

/* Nonzero means that this _CLASSTYPE node can convert itself to an
   INTEGER_TYPE.  */
#define TYPE_HAS_INT_CONVERSION(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.has_int_conversion)

/* Nonzero means that this _CLASSTYPE node can convert itself to an
   REAL_TYPE.  */
#define TYPE_HAS_REAL_CONVERSION(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.has_float_conversion)

/* Nonzero means that this _CLASSTYPE node overloads operator=(X&).  */
#define TYPE_HAS_ASSIGN_REF(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.has_assign_ref)
#define TYPE_GETS_ASSIGN_REF(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.gets_assign_ref)
#define TYPE_HAS_CONST_ASSIGN_REF(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.has_const_assign_ref)
#define TYPE_GETS_CONST_ASSIGN_REF(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.gets_const_assign_ref)

/* Nonzero means that this _CLASSTYPE node has an X(X&) constructor.  */
#define TYPE_HAS_INIT_REF(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.has_init_ref)
#define TYPE_GETS_INIT_REF(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.gets_init_ref)
#define TYPE_GETS_CONST_INIT_REF(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.gets_const_init_ref)

/* Nonzero means that this _CLASSTYPE node has an X(X ...) constructor.
   Note that there must be other arguments, or this constructor is flagged
   as being erroneous.  */
#define TYPE_GETS_INIT_AGGR(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.gets_init_aggr)

/* Nonzero means that this type is being defined.  I.e., the left brace
   starting the definition of this type has been seen.  */
#define TYPE_BEING_DEFINED(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.being_defined)
/* Nonzero means that this type has been redefined.  In this case, if
   convenient, don't reprocess any methods that appear in its redefinition.  */
#define TYPE_REDEFINED(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.redefined)

/* Nonzero means that this _CLASSTYPE node overloads the method call
   operator.  In this case, all method calls go through `operator->()(...).  */
#define TYPE_OVERLOADS_METHOD_CALL_EXPR(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.has_method_call_overloaded)

/* The is the VAR_DECL that contains NODE's dossier.  */
#define CLASSTYPE_DOSSIER(NODE) (TYPE_LANG_SPECIFIC(NODE)->dossier)

/* Nonzero means that this _CLASSTYPE node overloads operator().  */
#define TYPE_OVERLOADS_CALL_EXPR(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.has_call_overloaded)

/* Nonzero means that this _CLASSTYPE node overloads operator[].  */
#define TYPE_OVERLOADS_ARRAY_REF(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.has_array_ref_overloaded)

/* Nonzero means that this _CLASSTYPE node overloads operator->.  */
#define TYPE_OVERLOADS_ARROW(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.has_arrow_overloaded)

/* Nonzero means that this _CLASSTYPE (or one of its ancestors) uses
   multiple inheritance.  If this is 0 for the root of a type
   hierarchy, then we can use more efficient search techniques.  */
#define TYPE_USES_MULTIPLE_INHERITANCE(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.uses_multiple_inheritance)

/* Nonzero means that this _CLASSTYPE (or one of its ancestors) uses
   virtual base classes.  If this is 0 for the root of a type
   hierarchy, then we can use more efficient search techniques.  */
#define TYPE_USES_VIRTUAL_BASECLASSES(NODE) (TREE_LANG_FLAG_3(NODE))

/* List of lists of member functions defined in this class.  */
#define CLASSTYPE_METHOD_VEC(NODE) TYPE_METHODS(NODE)

/* Pointer from any member function to the head of the list of
   member functions of the type that member function belongs to.  */
#define CLASSTYPE_BASELINK_VEC(NODE) (TYPE_LANG_SPECIFIC(NODE)->baselink_vec)

/* Mark bits for depth-first and breath-first searches.  */
#if !PARANOID
#define CLASSTYPE_MARKED(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.marked)
#define CLASSTYPE_MARKED2(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.marked2)
#define CLASSTYPE_MARKED3(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.marked3)
#define CLASSTYPE_MARKED4(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.marked4)
#define CLASSTYPE_MARKED5(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.marked5)
#define CLASSTYPE_MARKED6(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.marked6)
/* Macros to modify the above flags */
#define SET_CLASSTYPE_MARKED(NODE) (CLASSTYPE_MARKED(NODE) = 1)
#define CLEAR_CLASSTYPE_MARKED(NODE) (CLASSTYPE_MARKED(NODE) = 0)
#define SET_CLASSTYPE_MARKED2(NODE) (CLASSTYPE_MARKED2(NODE) = 1)
#define CLEAR_CLASSTYPE_MARKED2(NODE) (CLASSTYPE_MARKED2(NODE) = 0)
#define SET_CLASSTYPE_MARKED3(NODE) (CLASSTYPE_MARKED3(NODE) = 1)
#define CLEAR_CLASSTYPE_MARKED3(NODE) (CLASSTYPE_MARKED3(NODE) = 0)
#define SET_CLASSTYPE_MARKED4(NODE) (CLASSTYPE_MARKED4(NODE) = 1)
#define CLEAR_CLASSTYPE_MARKED4(NODE) (CLASSTYPE_MARKED4(NODE) = 0)
#define SET_CLASSTYPE_MARKED5(NODE) (CLASSTYPE_MARKED5(NODE) = 1)
#define CLEAR_CLASSTYPE_MARKED5(NODE) (CLASSTYPE_MARKED5(NODE) = 0)
#define SET_CLASSTYPE_MARKED6(NODE) (CLASSTYPE_MARKED6(NODE) = 1)
#define CLEAR_CLASSTYPE_MARKED6(NODE) (CLASSTYPE_MARKED6(NODE) = 0)
#else
#define CLASSTYPE_MARKED(NODE) (my_friendly_assert (TREE_CODE_CLASS (TREE_CODE (NODE)) == 't', 328), TYPE_LANG_SPECIFIC(NODE)->type_flags.marked)
#define CLASSTYPE_MARKED2(NODE) (my_friendly_assert (TREE_CODE_CLASS (TREE_CODE (NODE)) == 't', 329), TYPE_LANG_SPECIFIC(NODE)->type_flags.marked2)
#define CLASSTYPE_MARKED3(NODE) (my_friendly_assert (TREE_CODE_CLASS (TREE_CODE (NODE)) == 't', 330), TYPE_LANG_SPECIFIC(NODE)->type_flags.marked3)
#define CLASSTYPE_MARKED4(NODE) (my_friendly_assert (TREE_CODE_CLASS (TREE_CODE (NODE)) == 't', 331), TYPE_LANG_SPECIFIC(NODE)->type_flags.marked4)
#define CLASSTYPE_MARKED5(NODE) (my_friendly_assert (TREE_CODE_CLASS (TREE_CODE (NODE)) == 't', 332), TYPE_LANG_SPECIFIC(NODE)->type_flags.marked5)
#define CLASSTYPE_MARKED6(NODE) (my_friendly_assert (TREE_CODE_CLASS (TREE_CODE (NODE)) == 't', 333), TYPE_LANG_SPECIFIC(NODE)->type_flags.marked6)
/* Macros to modify the above flags */
#define SET_CLASSTYPE_MARKED(NODE) (my_friendly_assert (TREE_CODE_CLASS (TREE_CODE (NODE)) == 't', 334), TYPE_LANG_SPECIFIC(NODE)->type_flags.marked = 1)
#define CLEAR_CLASSTYPE_MARKED(NODE) (my_friendly_assert (TREE_CODE_CLASS (TREE_CODE (NODE)) == 't', 335), TYPE_LANG_SPECIFIC(NODE)->type_flags.marked = 0)
#define SET_CLASSTYPE_MARKED2(NODE) (my_friendly_assert (TREE_CODE_CLASS (TREE_CODE (NODE)) == 't', 336), TYPE_LANG_SPECIFIC(NODE)->type_flags.marked2 = 1)
#define CLEAR_CLASSTYPE_MARKED2(NODE) (my_friendly_assert (TREE_CODE_CLASS (TREE_CODE (NODE)) == 't', 337), TYPE_LANG_SPECIFIC(NODE)->type_flags.marked2 = 0)
#define SET_CLASSTYPE_MARKED3(NODE) (my_friendly_assert (TREE_CODE_CLASS (TREE_CODE (NODE)) == 't', 338), TYPE_LANG_SPECIFIC(NODE)->type_flags.marked3 = 1)
#define CLEAR_CLASSTYPE_MARKED3(NODE) (my_friendly_assert (TREE_CODE_CLASS (TREE_CODE (NODE)) == 't', 339), TYPE_LANG_SPECIFIC(NODE)->type_flags.marked3 = 0)
#define SET_CLASSTYPE_MARKED4(NODE) (my_friendly_assert (TREE_CODE_CLASS (TREE_CODE (NODE)) == 't', 340), TYPE_LANG_SPECIFIC(NODE)->type_flags.marked4 = 1)
#define CLEAR_CLASSTYPE_MARKED4(NODE) (my_friendly_assert (TREE_CODE_CLASS (TREE_CODE (NODE)) == 't', 341), TYPE_LANG_SPECIFIC(NODE)->type_flags.marked4 = 0)
#define SET_CLASSTYPE_MARKED5(NODE) (my_friendly_assert (TREE_CODE_CLASS (TREE_CODE (NODE)) == 't', 342), TYPE_LANG_SPECIFIC(NODE)->type_flags.marked5 = 1)
#define CLEAR_CLASSTYPE_MARKED5(NODE) (my_friendly_assert (TREE_CODE_CLASS (TREE_CODE (NODE)) == 't', 343), TYPE_LANG_SPECIFIC(NODE)->type_flags.marked5 = 0)
#define SET_CLASSTYPE_MARKED6(NODE) (my_friendly_assert (TREE_CODE_CLASS (TREE_CODE (NODE)) == 't', 344), TYPE_LANG_SPECIFIC(NODE)->type_flags.marked6 = 1)
#define CLEAR_CLASSTYPE_MARKED6(NODE) (my_friendly_assert (TREE_CODE_CLASS (TREE_CODE (NODE)) == 't', 345), TYPE_LANG_SPECIFIC(NODE)->type_flags.marked6 = 0)
#endif

#define CLASSTYPE_TAGS(NODE) (TYPE_LANG_SPECIFIC(NODE)->tags)

/* Remove when done merging.  */
#define CLASSTYPE_VFIELD(NODE) TYPE_VFIELD(NODE)

/* The number of virtual functions defined for this
   _CLASSTYPE node.  */
#define CLASSTYPE_VSIZE(NODE) (TYPE_LANG_SPECIFIC(NODE)->vsize)
/* The virtual base classes that this type uses.  */
#define CLASSTYPE_VBASECLASSES(NODE) (TYPE_LANG_SPECIFIC(NODE)->vbases)
/* The virtual function pointer fields that this type contains.  */
#define CLASSTYPE_VFIELDS(NODE) (TYPE_LANG_SPECIFIC(NODE)->vfields)

/* Number of baseclasses defined for this type.
   0 means no base classes.  */
#define CLASSTYPE_N_BASECLASSES(NODE) \
  (TYPE_BINFO_BASETYPES (NODE) ? TREE_VEC_LENGTH (TYPE_BINFO_BASETYPES(NODE)) : 0)

/* Memoize the number of super classes (base classes) tha this node
   has.  That way we can know immediately (albeit conservatively how
   large a multiple-inheritance matrix we need to build to find
   derivation information.  */
#define CLASSTYPE_N_SUPERCLASSES(NODE) (TYPE_LANG_SPECIFIC(NODE)->n_ancestors)
#define CLASSTYPE_N_VBASECLASSES(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.n_vancestors)

/* Record how deep the inheritance is for this class so `void*' conversions
   are less favorable than a conversion to the most base type.  */
#define CLASSTYPE_MAX_DEPTH(NODE) (TYPE_LANG_SPECIFIC(NODE)->max_depth)

/* Used for keeping search-specific information.  Any search routine
   which uses this must define what exactly this slot is used for.  */
#define CLASSTYPE_SEARCH_SLOT(NODE) (TYPE_LANG_SPECIFIC(NODE)->search_slot)

/* Entry for keeping memoization tables for this type to
   hopefully speed up search routines.  Since it is a pointer,
   it can mean almost anything.  */
#define CLASSTYPE_MTABLE_ENTRY(NODE) (TYPE_LANG_SPECIFIC(NODE)->memoized_table_entry)

/* This is the total size of the baseclasses defined for this type.
   Needed because it is desirable to layout such information
   before beginning to process the class itself, and we
   don't want to compute it second time when actually laying
   out the type for real.  */
#define CLASSTYPE_SIZE(NODE) (TYPE_LANG_SPECIFIC(NODE)->size)
#define CLASSTYPE_SIZE_UNIT(NODE) (TYPE_LANG_SPECIFIC(NODE)->size_unit)
#define CLASSTYPE_MODE(NODE) (TYPE_LANG_SPECIFIC(NODE)->mode)
#define CLASSTYPE_ALIGN(NODE) (TYPE_LANG_SPECIFIC(NODE)->align)

/* This is the space needed for virtual base classes.  NULL if
   there are no virtual basetypes.  */
#define CLASSTYPE_VBASE_SIZE(NODE) (TYPE_LANG_SPECIFIC(NODE)->vbase_size)

/* A cons list of structure elements which either have constructors
   to be called, or virtual function table pointers which
   need initializing.  Depending on what is being initialized,
   the TREE_PURPOSE and TREE_VALUE fields have different meanings:

   Member initialization: <FIELD_DECL, TYPE>
   Base class construction: <NULL_TREE, BASETYPE>
   Base class initialization: <BASE_INITIALIZATION, THESE_INITIALIZATIONS>
   Whole type: <MEMBER_INIT, BASE_INIT>.  */
#define CLASSTYPE_BASE_INIT_LIST(NODE) (TYPE_LANG_SPECIFIC(NODE)->base_init_list)

/* A cons list of virtual functions which cannot be inherited by
   derived classes.  When deriving from this type, the derived
   class must provide its own definition for each of these functions.  */
#define CLASSTYPE_ABSTRACT_VIRTUALS(NODE) (TYPE_LANG_SPECIFIC(NODE)->abstract_virtuals)

#define CLASSTYPE_ALTERS_VISIBILITIES_P(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.alters_visibilities)

/* Nonzero means that this aggr type has been `closed' by a semicolon.  */
#define CLASSTYPE_GOT_SEMICOLON(NODE) (TYPE_LANG_SPECIFIC (NODE)->type_flags.got_semicolon)

/* Nonzero means that the main virtual function table pointer needs to be
   set because base constructors have placed the wrong value there.
   If this is zero, it means that they placed the right value there,
   and there is no need to change it.  */
#define CLASSTYPE_NEEDS_VIRTUAL_REINIT(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.needs_virtual_reinit)

/* Nonzero means that if this type has virtual functions, that
   the virtual function table will be written out.  */
#define CLASSTYPE_VTABLE_NEEDS_WRITING(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.vtable_needs_writing)

/* Nonzero means that this type defines its own local type declarations.  */
#define CLASSTYPE_LOCAL_TYPEDECLS(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.local_typedecls)

/* Nonzero means that this type has an X() constructor.  */
#define TYPE_HAS_DEFAULT_CONSTRUCTOR(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.has_default_ctor)

/* Many routines need to cons up a list of basetypes for visibility
   checking.  This field contains a TREE_LIST node whose TREE_VALUE
   is the main variant of the type, and whose TREE_VIA_PUBLIC
   and TREE_VIA_VIRTUAL bits are correctly set.  */
#define CLASSTYPE_AS_LIST(NODE) (TYPE_LANG_SPECIFIC(NODE)->as_list)
/* Same, but cache a list whose value is the name of this type.  */
#define CLASSTYPE_ID_AS_LIST(NODE) (TYPE_LANG_SPECIFIC(NODE)->id_as_list)
/* Same, but cache a list whose value is the binfo of this type.  */
#define CLASSTYPE_BINFO_AS_LIST(NODE) (TYPE_LANG_SPECIFIC(NODE)->binfo_as_list)

/* Slot in which to cache a copy of the local vtable pointer.  */
#define CLASSTYPE_VTBL_PTR(NODE) (TYPE_LANG_SPECIFIC(NODE)->vtbl_ptr)

/* Hold the instance object associated with this method.  */
#define CLASSTYPE_INST_VAR(NODE) (TYPE_LANG_SPECIFIC(NODE)->instance_variable)

/* A list of class types with which this type is a friend.  */
#define CLASSTYPE_FRIEND_CLASSES(NODE) (TYPE_LANG_SPECIFIC(NODE)->friend_classes)

/* Keep an inheritance lattice around so we can quickly tell whether
   a type is derived from another or not.  */
#define CLASSTYPE_MI_MATRIX(NODE) (TYPE_LANG_SPECIFIC(NODE)->mi_matrix)

/* If there is exactly one conversion to a non-void, non-const pointer type,
   remember that here.  If there are more than one, put
   `error_mark_node' here.  If there are none, this holds NULL_TREE.  */
#define CLASSTYPE_CONVERSION(NODE,KIND) \
  (TYPE_LANG_SPECIFIC(NODE)->conversions[(int) KIND])

/* Say whether this node was declared as a "class" or a "struct".  */
#define CLASSTYPE_DECLARED_CLASS(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.declared_class)
/* Say whether this node was declared as a "class" or a "struct".  */
#define CLASSTYPE_DECLARED_EXCEPTION(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.declared_exception)

/* Nonzero if this class has const members which have no specified initialization.  */
#define CLASSTYPE_READONLY_FIELDS_NEED_INIT(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.const_needs_init)

/* Nonzero if this class has ref members which have no specified initialization.  */
#define CLASSTYPE_REF_FIELDS_NEED_INIT(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.ref_needs_init)

/* Nonzero if this class is included from a header file which employs
   `#pragma interface', and it is not included in its implementation file.  */
#define CLASSTYPE_INTERFACE_ONLY(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.interface_only)

/* Same as above, but for classes whose purpose we do not know.  */
#define CLASSTYPE_INTERFACE_UNKNOWN(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.interface_unknown)

/* Nonzero if a _DECL node requires us to output debug info for this class.  */
#define CLASSTYPE_DEBUG_REQUESTED(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.debug_requested)

/* Additional macros for inheritance information.  */

#define CLASSTYPE_VBINFO(NODE,VIA_PUBLIC) \
  (TYPE_LANG_SPECIFIC (NODE)->vbinfo[VIA_PUBLIC])

/* When following an binfo-specific chain, this is the cumulative
   via-public flag.  */
#define BINFO_VIA_PUBLIC(NODE) TREE_LANG_FLAG_5 (NODE)

/* When building a matrix to determine by a single lookup
   whether one class is derived from another or not,
   this field is the index of the class in the table.  */
#define CLASSTYPE_CID(NODE) (TYPE_LANG_SPECIFIC(NODE)->cid)
#define BINFO_CID(NODE) CLASSTYPE_CID(BINFO_TYPE(NODE))

/* Nonzero means marked by DFS or BFS search, including searches
   by `get_binfo' and `get_base_distance'.  */
#define BINFO_MARKED(NODE) (TREE_VIA_VIRTUAL(NODE)?CLASSTYPE_MARKED(BINFO_TYPE(NODE)):TREE_LANG_FLAG_0(NODE))
/* Macros needed because of C compilers that don't allow conditional
   expressions to be lvalues.  Grr!  */
#define SET_BINFO_MARKED(NODE) (TREE_VIA_VIRTUAL(NODE)?SET_CLASSTYPE_MARKED(BINFO_TYPE(NODE)):(TREE_LANG_FLAG_0(NODE)=1))
#define CLEAR_BINFO_MARKED(NODE) (TREE_VIA_VIRTUAL(NODE)?CLEAR_CLASSTYPE_MARKED(BINFO_TYPE(NODE)):(TREE_LANG_FLAG_0(NODE)=0))

/* Nonzero means marked in building initialization list.  */
#define BINFO_BASEINIT_MARKED(NODE) CLASSTYPE_MARKED2 (BINFO_TYPE (NODE))
/* Modifier macros */
#define SET_BINFO_BASEINIT_MARKED(NODE) SET_CLASSTYPE_MARKED2 (BINFO_TYPE (NODE))
#define CLEAR_BINFO_BASEINIT_MARKED(NODE) CLEAR_CLASSTYPE_MARKED2 (BINFO_TYPE (NODE))

/* Nonzero means marked in search through virtual inheritance hierarchy.  */
#define BINFO_VBASE_MARKED(NODE) CLASSTYPE_MARKED2 (BINFO_TYPE (NODE))
/* Modifier macros */
#define SET_BINFO_VBASE_MARKED(NODE) SET_CLASSTYPE_MARKED2 (BINFO_TYPE (NODE))
#define CLEAR_BINFO_VBASE_MARKED(NODE) CLEAR_CLASSTYPE_MARKED2 (BINFO_TYPE (NODE))

/* Nonzero means marked in search for members or member functions.  */
#define BINFO_FIELDS_MARKED(NODE) \
  (TREE_VIA_VIRTUAL(NODE)?CLASSTYPE_MARKED2 (BINFO_TYPE (NODE)):TREE_LANG_FLAG_2(NODE))
#define SET_BINFO_FIELDS_MARKED(NODE) (TREE_VIA_VIRTUAL(NODE)?SET_CLASSTYPE_MARKED2(BINFO_TYPE(NODE)):(TREE_LANG_FLAG_2(NODE)=1))
#define CLEAR_BINFO_FIELDS_MARKED(NODE) (TREE_VIA_VIRTUAL(NODE)?CLEAR_CLASSTYPE_MARKED2(BINFO_TYPE(NODE)):(TREE_LANG_FLAG_2(NODE)=0))

/* Nonzero means that this class is on a path leading to a new vtable.  */
#define BINFO_VTABLE_PATH_MARKED(NODE) \
  (TREE_VIA_VIRTUAL(NODE)?CLASSTYPE_MARKED3(BINFO_TYPE(NODE)):TREE_LANG_FLAG_3(NODE))
#define SET_BINFO_VTABLE_PATH_MARKED(NODE) (TREE_VIA_VIRTUAL(NODE)?SET_CLASSTYPE_MARKED3(BINFO_TYPE(NODE)):(TREE_LANG_FLAG_3(NODE)=1))
#define CLEAR_BINFO_VTABLE_PATH_MARKED(NODE) (TREE_VIA_VIRTUAL(NODE)?CLEAR_CLASSTYPE_MARKED3(BINFO_TYPE(NODE)):(TREE_LANG_FLAG_3(NODE)=0))

/* Nonzero means that this class has a new vtable.  */
#define BINFO_NEW_VTABLE_MARKED(NODE) \
  (TREE_VIA_VIRTUAL(NODE)?CLASSTYPE_MARKED4(BINFO_TYPE(NODE)):TREE_LANG_FLAG_4(NODE))
#define SET_BINFO_NEW_VTABLE_MARKED(NODE) (TREE_VIA_VIRTUAL(NODE)?SET_CLASSTYPE_MARKED4(BINFO_TYPE(NODE)):(TREE_LANG_FLAG_4(NODE)=1))
#define CLEAR_BINFO_NEW_VTABLE_MARKED(NODE) (TREE_VIA_VIRTUAL(NODE)?CLEAR_CLASSTYPE_MARKED4(BINFO_TYPE(NODE)):(TREE_LANG_FLAG_4(NODE)=0))

/* Nonzero means this class has initialized its virtual baseclasses.  */
#define BINFO_VBASE_INIT_MARKED(NODE) \
  (TREE_VIA_VIRTUAL(NODE)?CLASSTYPE_MARKED5(BINFO_TYPE(NODE)):TREE_LANG_FLAG_5(NODE))
#define SET_BINFO_VBASE_INIT_MARKED(NODE) (TREE_VIA_VIRTUAL(NODE)?SET_CLASSTYPE_MARKED5(BINFO_TYPE(NODE)):(TREE_LANG_FLAG_5(NODE)=1))
#define CLEAR_BINFO_VBASE_INIT_MARKED(NODE) (TREE_VIA_VIRTUAL(NODE)?CLEAR_CLASSTYPE_MARKED5(BINFO_TYPE(NODE)):(TREE_LANG_FLAG_5(NODE)=0))

/* Accessor macros for the vfield slots in structures.  */

/* Get the assoc info that caused this vfield to exist.  */
#define VF_BINFO_VALUE(NODE) TREE_PURPOSE (NODE)

/* Get that same information as a _TYPE.  */
#define VF_BASETYPE_VALUE(NODE) TREE_VALUE (NODE)

/* Get the value of the top-most type dominating the non-`normal' vfields.  */
#define VF_DERIVED_VALUE(NODE) (VF_BINFO_VALUE (NODE) ? BINFO_TYPE (VF_BINFO_VALUE (NODE)) : NULL_TREE)

/* Get the value of the top-most type that's `normal' for the vfield.  */
#define VF_NORMAL_VALUE(NODE) TREE_TYPE (NODE)

/* Nonzero for TREE_LIST node means that this list of things
   is a list of parameters, as opposed to a list of expressions.  */
#define TREE_PARMLIST(NODE) ((NODE)->common.unsigned_flag) /* overloaded! */

/* Nonzero for FIELD_DECL node means that this FIELD_DECL is
   a member of an anonymous union construct.  The name of the
   union is .  */
#define TREE_ANON_UNION_ELEM(NODE) ((NODE)->decl.regdecl_flag) /* overloaded! */

/* For FUNCTION_TYPE or METHOD_TYPE, a list of the exceptions that
   this type can raise.  */
#define TYPE_RAISES_EXCEPTIONS(NODE) TYPE_NONCOPIED_PARTS (NODE)

struct lang_decl_flags
{
#ifdef ONLY_INT_FIELDS
  int language : 8;
#else
  enum languages language : 8;
#endif

  unsigned operator_attr : 1;
  unsigned constructor_attr : 1;
  unsigned returns_first_arg : 1;
  unsigned preserves_first_arg : 1;
  unsigned friend_attr : 1;
  unsigned static_function : 1;
  unsigned const_memfunc : 1;
  unsigned volatile_memfunc : 1;

  unsigned abstract_virtual : 1;
  unsigned permanent_attr : 1 ;
  unsigned constructor_for_vbase_attr : 1;
  unsigned dummy : 13;

  tree visibility;
  tree context;
};

struct lang_decl
{
  struct lang_decl_flags decl_flags;

  struct template_info *template_info;
  tree main_decl_variant;
  struct pending_inline *pending_inline_info;
  tree vbase_init_list;
  tree chain;
};

/* Non-zero if NODE is a _DECL with TREE_READONLY set.  */
#define TREE_READONLY_DECL_P(NODE) \
  (TREE_READONLY (NODE) && TREE_CODE_CLASS (TREE_CODE (NODE)) == 'd')

/* For FUNCTION_DECLs: return the language in which this decl
   was declared.  */
#define DECL_LANGUAGE(NODE) (DECL_LANG_SPECIFIC(NODE)->decl_flags.language)

/* For FUNCTION_DECLs: nonzero means that this function is a constructor.  */
#define DECL_CONSTRUCTOR_P(NODE) (DECL_LANG_SPECIFIC(NODE)->decl_flags.constructor_attr)
/* For FUNCTION_DECLs: nonzero means that this function is a constructor
   for an object with virtual baseclasses.  */
#define DECL_CONSTRUCTOR_FOR_VBASE_P(NODE) (DECL_LANG_SPECIFIC(NODE)->decl_flags.constructor_for_vbase_attr)

/* For FUNCTION_DECLs: nonzero means that the constructor
   is known to return a non-zero `this' unchanged.  */
#define DECL_RETURNS_FIRST_ARG(NODE) (DECL_LANG_SPECIFIC(NODE)->decl_flags.returns_first_arg)

/* Nonzero for FUNCTION_DECL means that this constructor is known to
   not make any assignment to `this', and therefore can be trusted
   to return it unchanged.  Otherwise, we must re-assign `current_class_decl'
   after performing base initializations.  */
#define DECL_PRESERVES_THIS(NODE) (DECL_LANG_SPECIFIC(NODE)->decl_flags.preserves_first_arg)

/* Nonzero for _DECL means that this decl appears in (or will appear
   in) as a member in a RECORD_TYPE or UNION_TYPE node.  It is also for
   detecting circularity in case members are multiply defined.  In the
   case of a VAR_DECL, it is also used to determine how program storage
   should be allocated.  */
#define DECL_IN_AGGR_P(NODE) (DECL_LANG_FLAG_3(NODE))

/* Nonzero for FUNCTION_DECL means that this decl is just a
   friend declaration, and should not be added to the list of
   member functions for this class.  */
#define DECL_FRIEND_P(NODE) (DECL_LANG_SPECIFIC(NODE)->decl_flags.friend_attr)

/* Nonzero for FUNCTION_DECL means that this decl is a static
   member function.  */
#define DECL_STATIC_FUNCTION_P(NODE) (DECL_LANG_SPECIFIC(NODE)->decl_flags.static_function)

/* Nonzero for FUNCTION_DECL means that this member function
   has `this' as const X *const.  */
#define DECL_CONST_MEMFUNC_P(NODE) (DECL_LANG_SPECIFIC(NODE)->decl_flags.const_memfunc)

/* Nonzero for FUNCTION_DECL means that this member function
   has `this' as volatile X *const.  */
#define DECL_VOLATILE_MEMFUNC_P(NODE) (DECL_LANG_SPECIFIC(NODE)->decl_flags.volatile_memfunc)

/* Nonzero for FUNCTION_DECL means that this member function
   exists as part of an abstract class's interface.  */
#define DECL_ABSTRACT_VIRTUAL_P(NODE) (DECL_LANG_SPECIFIC(NODE)->decl_flags.abstract_virtual)

/* Nonzero if allocated on permanent_obstack.  */
#define LANG_DECL_PERMANENT(LANGDECL) ((LANGDECL)->decl_flags.permanent_attr)

/* The _TYPE context in which this _DECL appears.  This field is used
   only to compute visibility information.  */
#define DECL_CLASS_CONTEXT(NODE) (DECL_LANG_SPECIFIC(NODE)->decl_flags.context)

/* For a FUNCTION_DECL: the chain through which the next method
   in the method chain is found.  We now use TREE_CHAIN to
   link into the FIELD_DECL chain.  */
#if 1
#define DECL_CHAIN(NODE) (DECL_LANG_SPECIFIC(NODE)->chain)
#else
#define DECL_CHAIN(NODE) (TREE_CHAIN (NODE))
#endif

/* Points back to the decl which caused this lang_decl to be allocated.  */
#define DECL_MAIN_VARIANT(NODE) (DECL_LANG_SPECIFIC(NODE)->main_decl_variant)

/* For a FUNCTION_DECL: if this function was declared inline inside of
   a class declaration, this is where the text for the function is
   squirreled away.  */
#define DECL_PENDING_INLINE_INFO(NODE) (DECL_LANG_SPECIFIC(NODE)->pending_inline_info)

/* Holds information about how virtual base classes should be initialized
   by this constructor *if* this constructor is the one to perform
   such initialization.  */
#define DECL_VBASE_INIT_LIST(NODE) (DECL_LANG_SPECIFIC(NODE)->vbase_init_list)

/* For a TEMPLATE_DECL: template-specific information.  */
#define DECL_TEMPLATE_INFO(NODE) (DECL_LANG_SPECIFIC(NODE)->template_info)

/* Nonzero in INT_CST means that this int is negative by dint of
   using a twos-complement negated operand.  */
#define TREE_NEGATED_INT(NODE) (TREE_LANG_FLAG_0 (NODE))

/* Nonzero in any kind of _EXPR or _REF node means that it is a call
   to a storage allocation routine.  If, later, alternate storage
   is found to hold the object, this call can be ignored.  */
#define TREE_CALLS_NEW(NODE) (TREE_LANG_FLAG_1 (NODE))

/* Nonzero in any kind of _TYPE that uses multiple inheritance
   or virtual baseclasses.  */
#define TYPE_USES_COMPLEX_INHERITANCE(NODE) (TREE_LANG_FLAG_1 (NODE))

/* Nonzero in IDENTIFIER_NODE means that this name is overloaded, and
   should be looked up in a non-standard way.  */
#define TREE_OVERLOADED(NODE) (TREE_LANG_FLAG_0 (NODE))
#define DECL_OVERLOADED(NODE) (DECL_LANG_FLAG_4 (NODE))

/* Nonzero if this (non-TYPE)_DECL has its virtual attribute set.
   For a FUNCTION_DECL, this is when the function is a virtual function.
   For a VAR_DECL, this is when the variable is a virtual function table.
   For a FIELD_DECL, when the field is the field for the virtual function table.
   For an IDENTIFIER_NODE, nonzero if any function with this name
   has been declared virtual.

   For a _TYPE if it uses virtual functions (or is derived from
   one that does).  */
#define TYPE_VIRTUAL_P(NODE) (TREE_LANG_FLAG_2 (NODE))

/* Same, but tells if this field is private in current context.  */
#define DECL_PRIVATE(NODE) (DECL_LANG_FLAG_5 (NODE))

/* Same, but tells if this field is private in current context.  */
#define DECL_PROTECTED(NODE) (DECL_LANG_FLAG_6(NODE))

#define DECL_PUBLIC(NODE) (DECL_LANG_FLAG_7(NODE))

/* Record whether a typedef for type `int' was actually `signed int'.  */
#define C_TYPEDEF_EXPLICITLY_SIGNED(exp) DECL_LANG_FLAG_1 ((exp))

/* Nonzero if the type T promotes to itself.
   ANSI C states explicitly the list of types that promote;
   in particular, short promotes to int even if they have the same width.  */
#define C_PROMOTING_INTEGER_TYPE_P(t)				\
  (TREE_CODE ((t)) == INTEGER_TYPE				\
   && (TYPE_MAIN_VARIANT (t) == char_type_node			\
       || TYPE_MAIN_VARIANT (t) == signed_char_type_node	\
       || TYPE_MAIN_VARIANT (t) == unsigned_char_type_node	\
       || TYPE_MAIN_VARIANT (t) == short_integer_type_node	\
       || TYPE_MAIN_VARIANT (t) == short_unsigned_type_node))

/* Mark which labels are explicitly declared.
   These may be shadowed, and may be referenced from nested functions.  */
#define C_DECLARED_LABEL_FLAG(label) TREE_LANG_FLAG_1 (label)

/* Record whether a type or decl was written with nonconstant size.
   Note that TYPE_SIZE may have simplified to a constant.  */
#define C_TYPE_VARIABLE_SIZE(type) TREE_LANG_FLAG_4 (type)
#define C_DECL_VARIABLE_SIZE(type) DECL_LANG_FLAG_8 (type)

/* Nonzero for _TYPE means that the _TYPE defines
   at least one constructor.  */
#define TYPE_HAS_CONSTRUCTOR(NODE) (TYPE_LANG_FLAG_1(NODE))

/* When appearing in an INDIRECT_REF, it means that the tree structure
   underneath is actually a call to a constructor.  This is needed
   when the constructor must initialize local storage (which can
   be automatically destroyed), rather than allowing it to allocate
   space from the heap.

   When appearing in a SAVE_EXPR, it means that underneath
   is a call to a constructor.

   When appearing in a CONSTRUCTOR, it means that it was
   a GNU C constructor expression.

   When appearing in a FIELD_DECL, it means that this field
   has been duly initialized in its constructor.  */
#define TREE_HAS_CONSTRUCTOR(NODE) (TREE_LANG_FLAG_4(NODE))

/* Indicates that a NON_LVALUE_EXPR came from a C++ reference.
   Used to generate more helpful error message in case somebody
   tries to take its address.  */
#define TREE_REFERENCE_EXPR(NODE) (TREE_LANG_FLAG_3(NODE))

/* Nonzero for _TYPE means that the _TYPE defines a destructor.  */
#define TYPE_HAS_DESTRUCTOR(NODE) (TYPE_LANG_FLAG_2(NODE))

/* Nonzero for _TYPE node means that creating an object of this type
   will involve a call to a constructor.  This can apply to objects
   of ARRAY_TYPE if the type of the elements needs a constructor.  */
#define TYPE_NEEDS_CONSTRUCTING(NODE) (TYPE_LANG_FLAG_3(NODE))
#define TYPE_NEEDS_CONSTRUCTOR(NODE) (TYPE_LANG_SPECIFIC(NODE)->type_flags.needs_constructor)

/* Nonzero for _TYPE node means that destroying an object of this type
   will involve a call to a destructor.  This can apply to objects
   of ARRAY_TYPE is the type of the elements needs a destructor.  */
#define TYPE_NEEDS_DESTRUCTOR(NODE) (TYPE_LANG_FLAG_4(NODE))

/* Nonzero for VAR_DECL node means that `external' was specified in
   its declaration.  */
#define DECL_THIS_EXTERN(NODE) (DECL_LANG_FLAG_2(NODE))

/* Nonzero for SAVE_EXPR if used to initialize a PARM_DECL.  */
#define PARM_DECL_EXPR(NODE) (TREE_LANG_FLAG_2(NODE))

/* Nonzero in FUNCTION_DECL means it is really an operator.
   Just used to communicate formatting information to dbxout.c.  */
#define DECL_OPERATOR(NODE) (DECL_LANG_SPECIFIC(NODE)->decl_flags.operator_attr)

/* Define fields and accessors for nodes representing declared names.  */

#if 0
/* C++: A derived class may be able to directly use the virtual
   function table of a base class.  When it does so, it may
   still have a decl node used to access the virtual function
   table (so that variables of this type can initialize their
   virtual function table pointers by name).  When such thievery
   is committed, know exactly which base class's virtual function
   table is the one being stolen.  This effectively computes the
   transitive closure.  */
#define DECL_VPARENT(NODE) ((NODE)->decl.arguments)
#endif

/* Make a slot so we can implement nested types.  This slot holds
   the IDENTIFIER_NODE that uniquely names the nested type.  This
   is for TYPE_DECLs only.  */
#if !PARANOID
#define DECL_NESTED_TYPENAME(NODE) ((NODE)->decl.arguments)
#else
#define DECL_NESTED_TYPENAME(NODE) (*DECL_NESTED_TYPENAME_PTR(NODE))
#ifdef __GNUC__
__inline
#endif
static tree * DECL_NESTED_TYPENAME_PTR(NODE) tree NODE; { return
  (my_friendly_assert (TREE_CODE_CLASS (TREE_CODE (NODE)) == 'd', 346),
   &(NODE)->decl.arguments) ;}
#endif

/* C++: all of these are overloaded!  These apply only to TYPE_DECLs.  */
#define DECL_FRIENDLIST(NODE) (DECL_INITIAL (NODE))
#define DECL_UNDEFINED_FRIENDS(NODE) ((NODE)->decl.result)
#define DECL_WAITING_FRIENDS(NODE) ((tree)(NODE)->decl.rtl)
#define SET_DECL_WAITING_FRIENDS(NODE,VALUE) ((NODE)->decl.rtl=(struct rtx_def*)VALUE)

/* The DECL_VISIBILITY is used to record under which context
   special visibility rules apply.  */
#define DECL_VISIBILITY(NODE) (DECL_LANG_SPECIFIC(NODE)->decl_flags.visibility)

/* C++: all of these are overloaded!
   These apply to PARM_DECLs and VAR_DECLs.  */
#define DECL_REFERENCE_SLOT(NODE) ((tree)(NODE)->decl.arguments)
#define SET_DECL_REFERENCE_SLOT(NODE,VAL) ((NODE)->decl.arguments=VAL)

/* For local VAR_DECLs, holds index into gc-protected obstack.  */
#define DECL_GC_OFFSET(NODE) ((NODE)->decl.result)

/* Accessor macros for C++ template decl nodes.  */
#define DECL_TEMPLATE_IS_CLASS(NODE)    (DECL_RESULT(NODE) == NULL_TREE)
#define DECL_TEMPLATE_PARMS(NODE)       DECL_ARGUMENTS(NODE)
/* For class templates.  */
#define DECL_TEMPLATE_MEMBERS(NODE)     DECL_INITIAL(NODE)
/* For function, method, class-data templates.  */
#define DECL_TEMPLATE_RESULT(NODE)      DECL_RESULT(NODE)
#define DECL_TEMPLATE_INSTANTIATIONS(NODE) DECL_VINDEX(NODE)

/* ...and for unexpanded-parameterized-type nodes.  */
#define UPT_TEMPLATE(NODE)      TREE_PURPOSE(TYPE_VALUES(NODE))
#define UPT_PARMS(NODE)         TREE_VALUE(TYPE_VALUES(NODE))

/* An enumeration of the kind of tags that C++ accepts.  */
enum tag_types { record_type, class_type, union_type, enum_type, exception_type };

/* Zero means prototype weakly, as in ANSI C (no args means nothing).
   Each language context defines how this variable should be set.  */
extern int strict_prototype;
extern int strict_prototypes_lang_c, strict_prototypes_lang_cplusplus;

/* Non-zero means that if a label exists, and no other identifier
   applies, use the value of the label.  */
extern int flag_labels_ok;

/* Non-zero means to collect statistics which might be expensive
   and to print them when we are done.  */
extern int flag_detailed_statistics;

/* Non-zero means warn in function declared in derived class has the
   same name as a virtual in the base class, but fails to match the
   type signature of any virtual function in the base class.  */
extern int warn_overloaded_virtual;

/* in cp-decl{2}.c */
extern tree void_list_node;
extern tree void_zero_node;
extern tree default_function_type;
extern tree vtable_entry_type;
extern tree __t_desc_type_node, __i_desc_type_node, __m_desc_type_node;
extern tree class_star_type_node;

/* A node that is a list (length 1) of error_mark_nodes.  */
extern tree error_mark_list;

extern tree ptr_type_node;
extern tree class_type_node, record_type_node, union_type_node, enum_type_node;
extern tree exception_type_node, unknown_type_node;

/* The largest size a virtual function table can be.
   Must be a (power of 2).  */
#ifndef VINDEX_MAX
#define VINDEX_MAX ((unsigned)128)
/* This is the integer ~ (vindex_max - 1).  */
#endif
extern tree vtbl_mask;

/* Array type `(void *)[]' */
extern tree vtbl_type_node;

extern tree long_long_integer_type_node, long_long_unsigned_type_node;
/* For building calls to `delete'.  */
extern tree integer_two_node, integer_three_node;

/* in cp-except.c */
extern tree current_exception_type;
extern tree current_exception_decl;
extern tree current_exception_object;

/* in cp-pt.c  */
/* PARM_VEC is a vector of template parameters, either IDENTIFIER_NODEs or
   PARM_DECLs.  BINDINGS, if non-null, is a vector of bindings for those
   parameters.  */
struct template_info {
  /* Vector of template parameters, either PARM_DECLs or IDENTIFIER_NODEs.  */
  tree parm_vec;
  /* If non-null, a vector of bindings for the template parms.  */
  tree bindings;

  /* Text of template, and length.  */
  char *text;
  int length;
  /* Where it came from.  */
  char *filename;
  int lineno;

  /* What kind of aggregate -- struct, class, or null.  */
  tree aggr;
};
extern int processing_template_decl, processing_template_defn;

#define PRINT_LANG_DECL
#define PRINT_LANG_TYPE

#define UNKNOWN_TYPE LANG_TYPE

/* in cp-class.c */
extern tree current_class_name;
extern tree current_class_type;

extern tree current_lang_name, lang_name_cplusplus, lang_name_c;

/* Points to the name of that function. May not be the DECL_NAME
   of CURRENT_FUNCTION_DECL due to overloading */
extern tree original_function_name;

# define IS_AGGR_TYPE(t) (TYPE_LANG_FLAG_5 (t))

# define IS_AGGR_TYPE_CODE(t) \
  (t == RECORD_TYPE || t == UNION_TYPE)

extern tree current_class_name, current_class_type, current_class_decl, C_C_D;
extern tree current_vtable_decl;

/* in cp-init.c  */
extern tree global_base_init_list;
extern tree current_base_init_list, current_member_init_list;

extern int current_function_assigns_this;
extern int current_function_just_assigned_this;
extern int current_function_parms_stored;

/* Here's where we control how name mangling takes place.  */

#define OPERATOR_ASSIGN_FORMAT "__a%s"
#define OPERATOR_FORMAT "__%s"
#define OPERATOR_TYPENAME_FORMAT "__op"

/* Cannot use '$' up front, because this confuses gdb
   (names beginning with '$' are gdb-local identifiers).

   Note that all forms in which the '$' is significant are long enough
   for direct indexing (meaning that if we know there is a '$'
   at a particular location, we can index into the string at
   any other location that provides distinguishing characters).  */

/* Define NO_DOLLAR_IN_LABEL in your favorite tm file if your assembler
   doesn't allow '$' in symbol names.  */
#ifndef NO_DOLLAR_IN_LABEL

#define JOINER '$'

#define VPTR_NAME "$v"
#define THROW_NAME "$eh_throw"
#define DESTRUCTOR_DECL_PREFIX "_$_"
#define IN_CHARGE_NAME "__in$chrg"
#define AUTO_VTABLE_NAME "__vtbl$me__"
#define AUTO_TEMP_NAME "_$tmp_"
#define AUTO_TEMP_FORMAT "_$tmp_%d"
#define VTBL_PTR_TYPE "$vtbl_ptr_type"
#define VTABLE_BASE "$vb"
#define VTABLE_NAME_FORMAT "_vt$%s"
#define VFIELD_BASE "$vf"
#define VFIELD_NAME "_vptr$"
#define VFIELD_NAME_FORMAT "_vptr$%s"
#define VBASE_NAME "_vb$"
#define VBASE_NAME_FORMAT "_vb$%s"
#define STATIC_NAME_FORMAT "_%s$%s"
#define FILE_FUNCTION_FORMAT "_GLOBAL_$D$%s"
#define ANON_AGGRNAME_FORMAT "$_%d"

#else /* NO_DOLLAR_IN_LABEL */

#ifndef NO_DOT_IN_LABEL

#define JOINER '.'

#define VPTR_NAME ".v"
#define THROW_NAME ".eh_throw"
#define DESTRUCTOR_DECL_PREFIX "_._"
#define IN_CHARGE_NAME "__in.chrg"
#define AUTO_VTABLE_NAME "__vtbl.me__"
#define AUTO_TEMP_NAME "_.tmp_"
#define AUTO_TEMP_FORMAT "_.tmp_%d"
#define VTBL_PTR_TYPE ".vtbl_ptr_type"
#define VTABLE_BASE ".vb"
#define VTABLE_NAME_FORMAT "_vt.%s"
#define VFIELD_BASE ".vf"
#define VFIELD_NAME "_vptr."
#define VFIELD_NAME_FORMAT "_vptr.%s"
#define VBASE_NAME "_vb."
#define VBASE_NAME_FORMAT "_vb.%s"
#define STATIC_NAME_FORMAT "_%s.%s"
#define FILE_FUNCTION_FORMAT "_GLOBAL_.D.%s"

#define ANON_AGGRNAME_FORMAT "._%d"

#else /* NO_DOT_IN_LABEL */

#define VPTR_NAME "__vptr"
#define VPTR_NAME_P(ID_NODE) \
  (!strncmp (IDENTIFIER_POINTER (ID_NODE), VPTR_NAME, sizeof (VPTR_NAME) - 1))
#define THROW_NAME "__eh_throw"
#define DESTRUCTOR_DECL_PREFIX "__destr_"
#define DESTRUCTOR_NAME_P(ID_NODE) \
  (!strncmp (IDENTIFIER_POINTER (ID_NODE), DESTRUCTOR_DECL_PREFIX, \
	     sizeof (DESTRUCTOR_DECL_PREFIX) - 1))
#define IN_CHARGE_NAME "__in_chrg"
#define AUTO_VTABLE_NAME "__vtbl_me__"
#define AUTO_TEMP_NAME "__tmp_"
#define TEMP_NAME_P(ID_NODE) \
  (!strncmp (IDENTIFIER_POINTER (ID_NODE), AUTO_TEMP_NAME, \
	     sizeof (AUTO_TEMP_NAME) - 1))
#define AUTO_TEMP_FORMAT "__tmp_%d"
#define VTBL_PTR_TYPE "__vtbl_ptr_type"
#define VTABLE_BASE "__vtb"
#define VTABLE_NAME_FORMAT "__vt_%s"
#define VFIELD_BASE "__vfb"
#define VFIELD_NAME "__vptr_"
#define VFIELD_NAME_P(ID_NODE) \
  (!strncmp (IDENTIFIER_POINTER (ID_NODE), VFIELD_NAME, \
	    sizeof (VFIELD_NAME) - 1))
#define VFIELD_NAME_FORMAT "_vptr_%s"
#define VBASE_NAME "__vb_"
#define VBASE_NAME_P(ID_NODE) \
  (!strncmp (IDENTIFIER_POINTER (ID_NODE), VBASE_NAME, \
	     sizeof (VBASE_NAME) - 1))
#define VBASE_NAME_FORMAT "__vb_%s"
#define STATIC_NAME_FORMAT "__static_%s_%s"
#define FILE_FUNCTION_FORMAT "__GLOBAL_D_%s"

#define ANON_AGGRNAME_PREFIX "__anon_"
#define ANON_AGGRNAME_P(ID_NODE) \
  (!strncmp (IDENTIFIER_POINTER (ID_NODE), ANON_AGGRNAME_PREFIX, \
	     sizeof (ANON_AGGRNAME_PREFIX) - 1))
#define ANON_AGGRNAME_FORMAT "__anon_%d"
#define ANON_PARMNAME_FORMAT "__%d"
#define ANON_PARMNAME_P(ID_NODE) (IDENTIFIER_POINTER (ID_NODE)[0] == '_' \
				  && IDENTIFIER_POINTER (ID_NODE)[1] == '_' \
				  && IDENTIFIER_POINTER (ID_NODE)[2] <= '9')

#endif	/* NO_DOT_IN_LABEL */
#endif	/* NO_DOLLAR_IN_LABEL */

#define THIS_NAME "this"
#define DESTRUCTOR_NAME_FORMAT "~%s"
#define FILE_FUNCTION_PREFIX_LEN 9
#define VTABLE_DELTA_NAME "delta"
#define VTABLE_DELTA2_NAME "delta2"
#define VTABLE_INDEX_NAME "index"
#define VTABLE_PFN_NAME "pfn"
#define EXCEPTION_CLEANUP_NAME "exception cleanup"

#define THIS_NAME_P(ID_NODE) (strcmp(IDENTIFIER_POINTER (ID_NODE), "this") == 0)

#if !defined(NO_DOLLAR_IN_LABEL) || !defined(NO_DOT_IN_LABEL)

#define VPTR_NAME_P(ID_NODE) (IDENTIFIER_POINTER (ID_NODE)[0] == JOINER \
			      && IDENTIFIER_POINTER (ID_NODE)[1] == 'v')
#define DESTRUCTOR_NAME_P(ID_NODE) (IDENTIFIER_POINTER (ID_NODE)[1] == JOINER)

#define VTABLE_NAME_P(ID_NODE) (IDENTIFIER_POINTER (ID_NODE)[1] == 'v' \
  && IDENTIFIER_POINTER (ID_NODE)[2] == 't' \
  && IDENTIFIER_POINTER (ID_NODE)[3] == JOINER)

#define VBASE_NAME_P(ID_NODE) (IDENTIFIER_POINTER (ID_NODE)[1] == 'v' \
  && IDENTIFIER_POINTER (ID_NODE)[2] == 'b' \
  && IDENTIFIER_POINTER (ID_NODE)[3] == JOINER)

#define OPERATOR_TYPENAME_P(ID_NODE) \
  (IDENTIFIER_POINTER (ID_NODE)[0] == '_'	\
   && IDENTIFIER_POINTER (ID_NODE)[1] == '_'	\
   && IDENTIFIER_POINTER (ID_NODE)[2] == 'o'	\
   && IDENTIFIER_POINTER (ID_NODE)[3] == 'p')

#define TEMP_NAME_P(ID_NODE) (!strncmp (IDENTIFIER_POINTER (ID_NODE), AUTO_TEMP_NAME, sizeof (AUTO_TEMP_NAME)-1))
#define VFIELD_NAME_P(ID_NODE) (!strncmp (IDENTIFIER_POINTER (ID_NODE), VFIELD_NAME, sizeof(VFIELD_NAME)-1))

/* For anonymous aggregate types, we need some sort of name to
   hold on to.  In practice, this should not appear, but it should
   not be harmful if it does.  */
#define ANON_AGGRNAME_P(ID_NODE) (IDENTIFIER_POINTER (ID_NODE)[0] == JOINER \
				  && IDENTIFIER_POINTER (ID_NODE)[1] == '_')
#define ANON_PARMNAME_FORMAT "_%d"
#define ANON_PARMNAME_P(ID_NODE) (IDENTIFIER_POINTER (ID_NODE)[0] == '_' \
				  && IDENTIFIER_POINTER (ID_NODE)[1] <= '9')
#endif /* !defined(NO_DOLLAR_IN_LABEL) || !defined(NO_DOT_IN_LABEL) */

/* Define the sets of attributes that member functions and baseclasses
   can have.  These are sensible combinations of {public,private,protected}
   cross {virtual,non-virtual}.  */

enum visibility_type {
  visibility_default,
  visibility_public,
  visibility_private,
  visibility_protected,
  visibility_default_virtual,
  visibility_public_virtual,
  visibility_private_virtual
};

/* in cp-lex.c  */
extern tree current_unit_name, current_unit_language;

/* Things for handling inline functions.  */

struct pending_inline
{
  struct pending_inline *next;	/* pointer to next in chain */
  int lineno;			/* line number we got the text from */
  char *filename;		/* name of file we were processing */
  tree fndecl;			/* FUNCTION_DECL that brought us here */
  int token;			/* token we were scanning */
  int token_value;		/* value of token we were scanning (YYSTYPE) */

  char *buf;			/* pointer to character stream */
  int len;			/* length of stream */
  tree parm_vec, bindings;	/* in case this is derived from a template */
  unsigned int can_free : 1;	/* free this after we're done with it? */
  unsigned int deja_vu : 1;	/* set iff we don't want to see it again.  */
  unsigned int interface : 2;	/* 0=interface 1=unknown 2=implementation */
};

/* in cp-method.c */
extern struct pending_inline *pending_inlines;

/* 1 for -fall-virtual: make every member function (except
   constructors) lay down in the virtual function table.
   Calls can then either go through the virtual function table or not,
   depending on whether we know what function will actually be called.  */

extern int flag_all_virtual;

/* Positive values means that we cannot make optimizing assumptions about
   `this'.  Negative values means we know `this' to be of static type.  */

extern int flag_this_is_variable;

/* Controls whether enums and ints freely convert.
   1 means with complete freedom.
   0 means enums can convert to ints, but not vice-versa.  */

extern int flag_int_enum_equivalence;

/* Nonzero means layout structures so that we can do garbage collection.  */

extern int flag_gc;

/* Nonzero means generate 'dossiers' that give run-time type information.  */

extern int flag_dossier;

/* Current end of entries in the gc obstack for stack pointer variables.  */

extern int current_function_obstack_index;

/* Flag saying whether we have used the obstack in this function or not.  */

extern int current_function_obstack_usage;

enum overload_flags { NO_SPECIAL = 0, DTOR_FLAG, OP_FLAG, TYPENAME_FLAG };

extern tree current_class_decl, C_C_D;	/* PARM_DECL: the class instance variable */

/* The following two can be derived from the previous one */
extern tree current_class_name;	/* IDENTIFIER_NODE: name of current class */
extern tree current_class_type;	/* _TYPE: the type of the current class */

/* Some macros for char-based bitfields.  */
#define B_SET(a,x) (a[x>>3] |= (1 << (x&7)))
#define B_CLR(a,x) (a[x>>3] &= ~(1 << (x&7)))
#define B_TST(a,x) (a[x>>3] & (1 << (x&7)))

/* These are uses as bits in flags passed to build_method_call
   to control its error reporting behavior.

   LOOKUP_PROTECT means flag visibility violations.
   LOOKUP_COMPLAIN mean complain if no suitable member function
     matching the arguments is found.
   LOOKUP_NORMAL is just a combination of these two.
   LOOKUP_AGGR requires the instance to be of aggregate type.
   LOOKUP_NONVIRTUAL means make a direct call to the member function found
   LOOKUP_GLOBAL means search through the space of overloaded functions,
     rather than the space of member functions.
   LOOKUP_HAS_IN_CHARGE means that the "in charge" variable is already
     in the parameter list.
   LOOKUP_PROTECTED_OK means that even if the constructor we find appears
     to be non-visible to current scope, call it anyway.
   LOOKUP_NO_CONVERSION means that user-defined conversions are not
     permitted.  Built-in conversions are permitted.
   LOOKUP_DESTRUCTOR means explicit call to destructor.  */

#define LOOKUP_PROTECT (1)
#define LOOKUP_COMPLAIN (2)
#define LOOKUP_NORMAL (3)
#define LOOKUP_AGGR (4)
#define LOOKUP_NONVIRTUAL (8)
#define LOOKUP_GLOBAL (16)
#define LOOKUP_HAS_IN_CHARGE (32)
#define LOOKUP_SPECULATIVELY (64)
#define LOOKUP_PROTECTED_OK (128)
/* 256 is free */
#define LOOKUP_NO_CONVERSION (512)
#define LOOKUP_DESTRUCTOR (512)

/* Anatomy of a DECL_FRIENDLIST (which is a TREE_LIST):
   purpose = friend name (IDENTIFIER_NODE);
   value = TREE_LIST of FUNCTION_DECLS;
   chain, type = EMPTY;  */
#define FRIEND_NAME(LIST) (TREE_PURPOSE (LIST))
#define FRIEND_DECLS(LIST) (TREE_VALUE (LIST))

/* These macros are for accessing the fields of TEMPLATE...PARM nodes.  */
#define TEMPLATE_TYPE_TPARMLIST(NODE) TREE_PURPOSE (TYPE_FIELDS (NODE))
#define TEMPLATE_TYPE_IDX(NODE) TREE_INT_CST_LOW (TREE_VALUE (TYPE_FIELDS (NODE)))
#define TEMPLATE_TYPE_SET_INFO(NODE,P,I) \
  (TYPE_FIELDS (NODE) = build_tree_list (P, build_int_2 (I, 0)))
#define TEMPLATE_CONST_TPARMLIST(NODE) (*(tree*)&TREE_INT_CST_LOW(NODE))
#define TEMPLATE_CONST_IDX(NODE) (TREE_INT_CST_HIGH(NODE))
#define TEMPLATE_CONST_SET_INFO(NODE,P,I) \
  (TEMPLATE_CONST_TPARMLIST (NODE) = saved_parmlist, \
   TEMPLATE_CONST_IDX (NODE) = I)

/* in cp-lex.c  */
/* Indexed by TREE_CODE, these tables give C-looking names to
   operators represented by TREE_CODES.  For example,
   opname_tab[(int) MINUS_EXPR] == "-".  */
extern char **opname_tab, **assignop_tab;

/* in cp-call.c */
extern int rank_for_overload			PROTO((struct candidate *, struct candidate *));
extern void compute_conversion_costs		PROTO((tree, tree, struct candidate *, int));
extern int get_arglist_len_in_bytes		PROTO((tree));
extern tree build_vfield_ref			PROTO((tree, tree));
extern tree find_scoped_type			PROTO((tree, tree, tree));
extern tree resolve_scope_to_name		PROTO((tree, tree));
extern tree build_scoped_method_call		PROTO((tree, tree, tree, tree));
extern tree build_method_call			PROTO((tree, tree, tree, tree, int));
extern tree build_overload_call_real		PROTO((tree, tree, int, struct candidate *, int));
extern tree build_overload_call			PROTO((tree, tree, int, struct candidate *));
extern tree build_overload_call_maybe		PROTO((tree, tree, int, struct candidate *));

/* in cp-class.c */
extern tree build_vbase_pointer			PROTO((tree, tree));
extern tree build_vbase_path			PROTO((enum tree_code, tree, tree, tree, int));
extern tree build_vtable_entry			PROTO((tree, tree));
extern tree build_vfn_ref			PROTO((tree *, tree, tree));
extern void add_method				PROTO((tree, tree *, tree));
extern void duplicate_tag_error			PROTO((tree));
extern tree finish_struct			PROTO((tree, tree, int));
extern int resolves_to_fixed_type_p		PROTO((tree, int *));
extern void init_class_processing		PROTO((void));
extern void pushclass				PROTO((tree, int));
extern void popclass				PROTO((int));
extern void push_lang_context			PROTO((tree));
extern void pop_lang_context			PROTO((void));
extern int root_lang_context_p			PROTO((void));
extern tree instantiate_type			PROTO((tree, tree, int));
extern void print_class_statistics		PROTO((void));

/* in cp-cvt.c */
extern tree convert_to_reference		PROTO((tree, tree, tree, tree, int, char *, int, int));
extern tree convert_from_reference		PROTO((tree));
extern tree convert_to_aggr			PROTO((tree, tree, char **, int));
extern tree convert_pointer_to			PROTO((tree, tree));
extern tree convert_pointer_to_vbase		PROTO((tree, tree));
extern tree convert				PROTO((tree, tree));
extern tree convert_force			PROTO((tree, tree));
extern tree build_type_conversion		PROTO((enum tree_code, tree, tree, int));
extern int build_default_binary_type_conversion	PROTO((enum tree_code, tree *, tree *));
extern int build_default_unary_type_conversion	PROTO((enum tree_code, tree *));

/* cp-decl.c */
#ifdef PARANOID
extern void binding_levels_sane			PROTO((void));
#endif
extern int global_bindings_p			PROTO((void));
extern void keep_next_level			PROTO((void));
extern int kept_level_p				PROTO((void));
extern void declare_parm_level			PROTO((void));
extern void declare_implicit_exception		PROTO((void));
extern int have_exceptions_p			PROTO((void));
extern void declare_uninstantiated_type_level	PROTO((void));
extern int uninstantiated_type_level_p		PROTO((void));
extern void declare_pseudo_global_level		PROTO((void));
extern int pseudo_global_level_p		PROTO((void));
extern void pushlevel				PROTO((int));
extern void pushlevel_temporary			PROTO((int));
extern tree poplevel				PROTO((int, int, int));
extern void delete_block			PROTO((tree));
extern void insert_block			PROTO((tree));
extern void add_block_current_level		PROTO((tree));
extern void set_block				PROTO((tree));
extern void pushlevel_class			PROTO((void));
extern tree poplevel_class			PROTO((void));
/* skip print_other_binding_stack and print_binding_level */
extern void print_binding_stack			PROTO((void));
extern void push_to_top_level			PROTO((void));
extern void pop_from_top_level			PROTO((void));
extern void set_identifier_type_value		PROTO((tree, tree));
extern void set_identifier_local_value		PROTO((tree, tree));
extern tree make_type_decl			PROTO((tree, tree));
extern void pushtag				PROTO((tree, tree));
extern tree make_anon_name			PROTO((void));
extern void clear_anon_tags			PROTO((void));
extern void adjust_type_value			PROTO((tree));
extern tree pushdecl				PROTO((tree));
extern tree pushdecl_top_level			PROTO((tree));
extern void push_overloaded_decl_top_level	PROTO((tree, int));
extern tree pushdecl_class_level		PROTO((tree));
extern int overloaded_globals_p			PROTO((tree));
extern tree push_overloaded_decl		PROTO((tree, int));
extern tree implicitly_declare			PROTO((tree));
extern tree lookup_label			PROTO((tree));
extern tree shadow_label			PROTO((tree));
extern tree define_label			PROTO((char *, int, tree));
extern void define_case_label			PROTO((tree));
extern tree getdecls				PROTO((void));
extern tree gettags				PROTO((void));
extern void set_current_level_tags_transparency	PROTO((int));
extern tree typedecl_for_tag			PROTO((tree));
extern tree lookup_name				PROTO((tree, int));
extern tree lookup_name_current_level		PROTO((tree));
extern void init_decl_processing		PROTO((void));
/* skipped define_function */
extern void shadow_tag				PROTO((tree));
extern void grok_ctor_properties		PROTO((tree, tree));
extern tree groktypename			PROTO((tree));
extern tree start_decl				PROTO((tree, tree, int, tree));
extern void finish_decl				PROTO((tree, tree, tree, int));
extern int complete_array_type			PROTO((tree, tree, int));
extern tree grokdeclarator			(); /* PROTO((tree, tree, enum decl_context, int, tree)); */
extern tree xref_defn_tag			PROTO((tree, tree, tree));
extern tree xref_tag				PROTO((tree, tree, tree));
extern tree start_enum				PROTO((tree));
extern tree finish_enum				PROTO((tree, tree));
extern tree build_enumerator			PROTO((tree, tree));
extern tree grok_enum_decls			PROTO((tree, tree));
extern int start_function			PROTO((tree, tree, tree, int));
extern void store_parm_decls			PROTO((void));
extern void store_return_init			PROTO((tree, tree));
extern void finish_function			PROTO((int, int));
extern tree start_method			PROTO((tree, tree, tree));
extern tree finish_method			PROTO((tree));
extern void hack_incomplete_structures		PROTO((tree));
extern tree maybe_build_cleanup			PROTO((tree));
extern void cplus_expand_expr_stmt		PROTO((tree));
extern void finish_stmt				PROTO((void));
extern void pop_implicit_try_blocks		PROTO((tree));
extern void push_exception_cleanup		PROTO((tree));
extern void revert_static_member_fn		PROTO((tree *, tree *, tree *));

/* in cp-decl2.c */
extern int lang_decode_option			PROTO((char *));
extern tree grok_method_quals			PROTO((tree, tree, tree));
extern void grokclassfn				PROTO((tree, tree, tree, enum overload_flags, tree));
extern void bad_specifiers			PROTO((char *, int, int, int, int));
extern void check_classfn			PROTO((tree, tree, tree));
extern tree grokfield				PROTO((tree, tree, tree, tree, tree));
extern tree grokbitfield			PROTO((tree, tree, tree));
extern tree groktypefield			PROTO((tree, tree));
extern tree grokoptypename			PROTO((tree, int));
extern tree build_push_scope			PROTO((tree, tree));
extern tree constructor_name			PROTO((tree));
extern void setup_vtbl_ptr			PROTO((void));
extern void mark_inline_for_output		PROTO((tree));
extern void clear_temp_name			PROTO((void));
extern tree get_temp_name			PROTO((tree, int));
extern tree get_temp_regvar			PROTO((tree, tree));
extern void finish_anon_union			PROTO((tree));
extern tree finish_table			PROTO((tree, tree, tree, int));
extern void finish_builtin_type			PROTO((tree, char *, tree *, int, tree));
extern tree coerce_new_type			PROTO((tree));
extern tree coerce_delete_type			PROTO((tree));
extern void walk_vtables			PROTO((void (*)(), void (*)()));
extern void finish_file				PROTO((void));

/* in cp-dem.c */
extern char *cplus_demangle			PROTO((char *));

/* in cp-edsel.c */

/* in cp-except.c */
extern tree lookup_exception_cname		PROTO((tree, tree, tree));
extern tree lookup_exception_tname		PROTO((tree));
extern tree lookup_exception_object		PROTO((tree, tree, int));
extern tree lookup_exception_type		PROTO((tree, tree, tree));
extern tree finish_exception			PROTO((tree, tree));
extern void finish_exception_decl		PROTO((tree, tree));
extern void end_exception_decls			PROTO((void));
extern void cplus_expand_start_try		PROTO((int));
extern tree cplus_expand_end_try		PROTO((int));
extern void cplus_expand_start_except		PROTO((tree, tree));
extern void cplus_expand_end_except		PROTO((tree));
extern void cplus_expand_raise			PROTO((tree, tree, tree, int));
extern tree ansi_exception_object_lookup	PROTO((tree));
extern void cplus_expand_throw			PROTO((tree));
extern tree cplus_expand_start_catch		PROTO((tree));
extern tree ansi_expand_start_catch		PROTO((tree));
extern void cplus_expand_end_catch		PROTO((int));
extern void cplus_expand_reraise		PROTO((tree));
extern void setup_exception_throw_decl		PROTO((void));
extern void init_exception_processing		PROTO((void));
extern void init_exception_processing_1		PROTO((void));

/* in cp-expr.c */
/* skip cplus_expand_expr */
extern void init_cplus_expand			PROTO((void));
extern void fixup_result_decl			PROTO((tree, struct rtx_def *));
extern int decl_in_memory_p			PROTO((tree));

/* in cp-gc.c */
extern int type_needs_gc_entry			PROTO((tree));
extern int value_safe_from_gc			PROTO((tree, tree));
extern void build_static_gc_entry		PROTO((tree, tree));
extern tree protect_value_from_gc		PROTO((tree, tree));
extern tree build_headof			PROTO((tree));
extern tree build_classof			PROTO((tree));
extern tree build_t_desc			PROTO((tree, int));
extern tree build_i_desc			PROTO((tree));
extern tree build_m_desc			PROTO((tree));
extern void expand_gc_prologue_and_epilogue	PROTO((void));
extern void lang_expand_end_bindings		PROTO((struct rtx_def *, struct rtx_def *));
extern void init_gc_processing			PROTO((void));

/* in cp-init.c */
extern void emit_base_init			PROTO((tree, int));
extern void check_base_init			PROTO((tree));
extern tree build_virtual_init			PROTO((tree, tree, tree));
extern void do_member_init			PROTO((tree, tree, tree));
extern void expand_member_init			PROTO((tree, tree, tree));
extern void expand_aggr_init			PROTO((tree, tree, int));
extern int is_aggr_typedef			PROTO((tree, int));
extern tree build_member_call			PROTO((tree, tree, tree));
extern tree build_offset_ref			PROTO((tree, tree));
extern tree get_member_function			PROTO((tree *, tree, tree));
extern tree resolve_offset_ref			PROTO((tree));
extern tree decl_constant_value			PROTO((tree));
extern int is_friend				PROTO((tree, tree));
extern void xref_friend				PROTO((tree, tree, tree));
extern void xref_friends			PROTO((tree, tree, tree));
extern void make_friend_class			PROTO((tree, tree));
extern tree do_friend				PROTO((tree, tree, tree, tree, enum overload_flags, tree));
extern void embrace_waiting_friends		PROTO((tree));
extern tree build_builtin_call			PROTO((tree, tree, tree));
extern tree build_new				PROTO((tree, tree, tree, int));
extern tree expand_vec_init			PROTO((tree, tree, tree, tree, int));
extern tree build_x_delete			PROTO((tree, tree, int, tree));
extern tree build_delete			PROTO((tree, tree, tree, int, int, int));
extern tree build_vbase_delete			PROTO((tree, tree));
extern tree build_vec_delete			PROTO((tree, tree, tree, tree, tree, tree));

/* in cp-input.c */

/* in cp-lex.c */
extern tree make_pointer_declarator		PROTO((tree, tree));
extern tree make_reference_declarator		PROTO((tree, tree));
extern char *operator_name_string		PROTO((tree));
extern void lang_init				PROTO((void));
extern void lang_finish				PROTO((void));
extern void init_filename_times			PROTO((void));
extern void reinit_lang_specific		PROTO((void));
extern void init_lex				PROTO((void));
extern void reinit_parse_for_function		PROTO((void));
extern int *init_parse				PROTO((void));
extern void print_parse_statistics		PROTO((void));
extern void extract_interface_info		PROTO((void));
extern void set_vardecl_interface_info		PROTO((tree, tree));
extern void do_pending_inlines			PROTO((void));
extern void process_next_inline			PROTO((tree));
#if 0
extern void consume_string			PROTO((struct obstack *));
#endif
/* skip restore_pending_input */
extern void yyungetc				PROTO((int, int));
extern void reinit_parse_for_method		PROTO((int, tree));
#if 0
extern void reinit_parse_for_block		PROTO((int, struct obstack *, int));
#endif
extern tree cons_up_default_function		PROTO((tree, tree, int));
extern void check_for_missing_semicolon		PROTO((tree));
extern void note_got_semicolon			PROTO((tree));
extern void note_list_got_semicolon		PROTO((tree));
extern int check_newline			PROTO((void));
extern void dont_see_typename			PROTO((void));
extern int identifier_type			PROTO((tree));
extern void see_typename			PROTO((void));
extern tree do_identifier			PROTO((tree));
extern tree identifier_typedecl_value		PROTO((tree));
extern int real_yylex				PROTO((void));
extern tree build_lang_decl			PROTO((enum tree_code, tree, tree));
extern tree build_lang_field_decl		PROTO((enum tree_code, tree, tree));
extern void copy_lang_decl			PROTO((tree));
extern tree make_lang_type			PROTO((enum tree_code));
extern void copy_decl_lang_specific		PROTO((tree));
extern void dump_time_statistics		PROTO((void));
/* extern void compiler_error			PROTO((char *, HOST_WIDE_INT, HOST_WIDE_INT)); */
extern void compiler_error_with_decl		PROTO((tree, char *));
extern void yyerror				PROTO((char *));

/* in cp-method.c */
extern void init_method				PROTO((void));
extern tree make_anon_parm_name			PROTO((void));
extern void clear_anon_parm_name		PROTO((void));
extern char *fndecl_as_string			PROTO((tree, tree, int));
extern char *type_as_string			PROTO((tree));
extern char *decl_as_string			PROTO((tree));
extern void do_inline_function_hair		PROTO((tree, tree));
/* skip report_type_mismatch */
extern char *build_overload_name		PROTO((tree, int, int));
extern tree cplus_exception_name		PROTO((tree));
extern tree build_decl_overload			PROTO((tree, tree, int));
extern tree build_typename_overload		PROTO((tree));
extern tree build_t_desc_overload		PROTO((tree));
extern void declare_overloaded			PROTO((tree));
#ifdef NO_AUTO_OVERLOAD
extern int is_overloaded			PROTO((tree));
#endif
extern tree build_opfncall			PROTO((enum tree_code, int, tree, tree, tree));
extern tree hack_identifier			PROTO((tree, tree, int));
extern tree build_component_type_expr		PROTO((tree, tree, tree, int));

/* in cp-pt.c */
extern void begin_template_parm_list		PROTO((void));
extern tree process_template_parm		PROTO((tree, tree));
extern tree end_template_parm_list		PROTO((tree));
extern void end_template_decl			PROTO((tree, tree, tree));
extern tree lookup_template_class		PROTO((tree, tree, tree));
extern void push_template_decls			PROTO((tree, tree, int));
extern void pop_template_decls			PROTO((tree, tree, int));
extern int uses_template_parms			PROTO((tree));
extern void instantiate_member_templates	PROTO((tree));
extern tree instantiate_class_template		PROTO((tree, int));
extern tree instantiate_template		PROTO((tree, tree *));
extern void undo_template_name_overload		PROTO((tree, int));
extern void overload_template_name		PROTO((tree, int));
extern void end_template_instantiation		PROTO((tree, tree));
extern void reinit_parse_for_template		PROTO((int, tree, tree));
extern int type_unification			PROTO((tree, tree *, tree, tree, int *));
extern int do_pending_expansions		PROTO((void));
extern void do_pending_templates		PROTO((void));

/* in cp-search.c */
extern tree make_memoized_table_entry		PROTO((tree, tree, int));
extern void push_memoized_context		PROTO((tree, int));
extern void pop_memoized_context		PROTO((int));
extern tree get_binfo				PROTO((tree, tree, int));
extern int get_base_distance			PROTO((tree, tree, int, tree *));
extern enum visibility_type check_visibility	PROTO((tree, tree));
extern tree lookup_field			PROTO((tree, tree, int, int));
extern tree lookup_nested_field			PROTO((tree, int));
extern tree lookup_fnfields			PROTO((tree, tree, int));
extern HOST_WIDE_INT breadth_first_search	PROTO((tree, int (*)(), int (*)()));
extern int tree_needs_constructor_p		PROTO((tree, int));
extern int tree_has_any_destructor_p		PROTO((tree, int));
extern tree get_first_matching_virtual		PROTO((tree, tree, int));
extern tree get_abstract_virtuals		PROTO((tree));
extern tree get_baselinks			PROTO((tree, tree, tree));
extern tree next_baselink			PROTO((tree));
extern tree init_vbase_pointers			PROTO((tree, tree));
extern tree build_vbase_vtables_init		PROTO((tree, tree, tree, tree, int));
extern void clear_search_slots			PROTO((tree));
extern tree get_vbase_types			PROTO((tree));
extern void build_mi_matrix			PROTO((tree));
extern void free_mi_matrix			PROTO((void));
extern void build_mi_virtuals			PROTO((int, int));
extern void add_mi_virtuals			PROTO((int, tree));
extern void report_ambiguous_mi_virtuals	PROTO((int, tree));
extern void note_debug_info_needed		PROTO((tree));
extern void push_class_decls			PROTO((tree));
extern void pop_class_decls			PROTO((tree));
extern tree build_type_pathname			PROTO((char *, tree, tree));
extern void unmark_finished_struct		PROTO((tree));
extern void print_search_statistics		PROTO((void));
extern void init_search_processing		PROTO((void));
extern void reinit_search_statistics		PROTO((void));

/* in cp-spew.c */
extern void init_spew				PROTO((void));
extern int yylex				PROTO((void));
extern tree arbitrate_lookup			PROTO((tree, tree, tree));

/* in cp-tree.c */
extern int lvalue_p				PROTO((tree));
extern int lvalue_or_else			PROTO((tree, char *));
extern tree build_cplus_new			PROTO((tree, tree, int));
extern tree break_out_cleanups			PROTO((tree));
extern tree break_out_calls			PROTO((tree));
extern tree build_cplus_method_type		PROTO((tree, tree, tree));
extern tree build_cplus_staticfn_type		PROTO((tree, tree, tree));
extern tree build_cplus_array_type		PROTO((tree, tree));
extern void propagate_binfo_offsets		PROTO((tree, tree));
extern int layout_vbasetypes			PROTO((tree, int));
extern tree layout_basetypes			PROTO((tree, tree));
extern int list_hash				PROTO((tree));
extern tree list_hash_lookup			PROTO((int, tree));
extern void list_hash_add			PROTO((int, tree));
extern tree list_hash_canon			PROTO((int, tree));
extern tree hash_tree_cons			PROTO((int, int, int, tree, tree, tree));
extern tree hash_tree_chain			PROTO((tree, tree));
extern tree hash_chainon			PROTO((tree, tree));
extern tree get_decl_list			PROTO((tree));
extern tree list_hash_lookup_or_cons		PROTO((tree));
extern tree make_binfo				PROTO((tree, tree, tree, tree, tree));
extern tree copy_binfo				PROTO((tree));
extern tree binfo_value				PROTO((tree, tree));
extern tree reverse_path			PROTO((tree));
extern tree virtual_member			PROTO((tree, tree));
extern tree virtual_offset			PROTO((tree, tree, tree));
extern void debug_binfo				PROTO((tree));
extern int decl_list_length			PROTO((tree));
extern tree fnaddr_from_vtable_entry		PROTO((tree));
extern void set_fnaddr_from_vtable_entry	PROTO((tree, tree));
extern tree function_arg_chain			PROTO((tree));
extern int promotes_to_aggr_type		PROTO((tree, enum tree_code));
extern int is_aggr_type_2			PROTO((tree, tree));
extern void message_2_types			PROTO((void (*)(), char *, tree, tree));
extern char *lang_printable_name		PROTO((tree));
extern tree build_exception_variant		PROTO((tree, tree, tree));
extern tree copy_to_permanent			PROTO((tree));
extern void print_lang_statistics		PROTO((void));
/* skip __eprintf */
extern tree array_type_nelts_total		PROTO((tree));
extern tree array_type_nelts_top		PROTO((tree));

/* in cp-typeck.c */
extern tree target_type				PROTO((tree));
extern tree require_complete_type		PROTO((tree));
extern int type_unknown_p			PROTO((tree));
extern tree require_instantiated_type		PROTO((tree, tree, tree));
extern tree commonparms				PROTO((tree, tree));
extern tree common_type				PROTO((tree, tree));
extern int compexcepttypes			PROTO((tree, tree, int));
extern int comptypes				PROTO((tree, tree, int));
extern int comp_target_types			PROTO((tree, tree, int));
extern tree common_base_types			PROTO((tree, tree));
extern int compparms				PROTO((tree, tree, int));
extern int comp_target_types			PROTO((tree, tree, int));
extern tree unsigned_type			PROTO((tree));
extern tree signed_type				PROTO((tree));
extern tree signed_or_unsigned_type		PROTO((int, tree));
extern tree c_sizeof				PROTO((tree));
extern tree c_sizeof_nowarn			PROTO((tree));
extern tree c_alignof				PROTO((tree));
extern tree default_conversion			PROTO((tree));
extern tree build_component_ref_1		PROTO((tree, tree, int));
extern tree build_component_ref			PROTO((tree, tree, tree, int));
extern tree build_x_indirect_ref		PROTO((tree, char *));
extern tree build_indirect_ref			PROTO((tree, char *));
extern tree build_x_array_ref			PROTO((tree, tree));
extern tree build_array_ref			PROTO((tree, tree));
extern tree build_x_function_call		PROTO((tree, tree, tree));
extern tree build_function_call_real		PROTO((tree, tree, int));
extern tree build_function_call			PROTO((tree, tree));
extern tree build_function_call_maybe		PROTO((tree, tree));
extern tree convert_arguments			PROTO((tree, tree, tree, tree, int));
extern tree build_x_binary_op			PROTO((enum tree_code, tree, tree));
extern tree build_binary_op			PROTO((enum tree_code, tree, tree, int));
extern tree build_binary_op_nodefault		PROTO((enum tree_code, tree, tree, enum tree_code));
extern tree build_component_addr		PROTO((tree, tree, char *));
extern tree build_x_unary_op			PROTO((enum tree_code, tree));
extern tree build_unary_op			PROTO((enum tree_code, tree, int));
extern tree unary_complex_lvalue		PROTO((enum tree_code, tree));
extern int mark_addressable			PROTO((tree));
extern tree build_x_conditional_expr		PROTO((tree, tree, tree));
extern tree build_conditional_expr		PROTO((tree, tree, tree));
extern tree build_x_compound_expr		PROTO((tree));
extern tree build_compound_expr			PROTO((tree));
extern tree build_c_cast			PROTO((tree, tree));
extern tree build_modify_expr			PROTO((tree, enum tree_code, tree));
extern int language_lvalue_valid		PROTO((tree));
extern void warn_for_assignment			PROTO((char *, char *, char *, tree, int, int));
extern tree convert_for_initialization		PROTO((tree, tree, tree, int, char *, tree, int));
extern void c_expand_asm_operands		PROTO((tree, tree, tree, tree, int, char *, int));
extern void c_expand_return			PROTO((tree));
extern tree c_expand_start_case			PROTO((tree));
extern void record_format_info			PROTO((tree, int, int, int));
extern tree build_component_ref			PROTO((tree, tree, tree, int));

/* in cp-type2.c */
extern tree error_not_base_type			PROTO((tree, tree));
extern tree binfo_or_else			PROTO((tree, tree));
extern void error_with_aggr_type		(); /* PROTO((tree, char *, HOST_WIDE_INT)); */
extern void readonly_error			PROTO((tree, char *, int));
extern void abstract_virtuals_error		PROTO((tree, tree));
extern void incomplete_type_error		PROTO((tree, tree));
extern void my_friendly_abort			PROTO((int));
extern void my_friendly_assert			PROTO((int, int));
extern tree store_init_value			PROTO((tree, tree));
extern tree digest_init				PROTO((tree, tree, tree *));
extern tree build_scoped_ref			PROTO((tree, tree));
extern tree build_x_arrow			PROTO((tree));
extern tree build_m_component_ref		PROTO((tree, tree));
extern tree build_functional_cast		PROTO((tree, tree));
extern char *enum_name_string			PROTO((tree, tree));
extern void report_case_error			PROTO((int, tree, tree, tree));

/* in cp-xref.c */
extern void GNU_xref_begin			PROTO((char *));
extern void GNU_xref_end			PROTO((int));
extern void GNU_xref_file			PROTO((char *));
extern void GNU_xref_start_scope		PROTO((HOST_WIDE_INT));
extern void GNU_xref_end_scope			PROTO((HOST_WIDE_INT, HOST_WIDE_INT, int, int, int));
extern void GNU_xref_def			PROTO((tree, char *));
extern void GNU_xref_decl			PROTO((tree, tree));
extern void GNU_xref_call			PROTO((tree, char *));
extern void GNU_xref_function			PROTO((tree, tree));
extern void GNU_xref_assign			PROTO((tree));
extern void GNU_xref_hier			PROTO((char *, char *, int, int, int));
extern void GNU_xref_member			PROTO((tree, tree));

/* -- end of C++ */
