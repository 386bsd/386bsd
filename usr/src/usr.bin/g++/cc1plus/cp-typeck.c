/* Build expressions with type checking for C++ compiler.
   Copyright (C) 1987, 1988, 1989, 1992, 1993 Free Software Foundation, Inc.
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


/* This file is part of the C++ front end.
   It contains routines to build C++ expressions given their operands,
   including computing the types of the result, C and C++ specific error
   checks, and some optimization.

   There are also routines to build RETURN_STMT nodes and CASE_STMT nodes,
   and to process initializations in declarations (since they work
   like a strange sort of assignment).  */

extern void error ();
extern void warning ();

#include "config.h"
#include <stdio.h>
#include "tree.h"
#include "rtl.h"
#include "cp-tree.h"
#include "flags.h"

int mark_addressable ();
static tree convert_for_assignment ();
/* static */ tree convert_for_initialization ();
int compparms ();
static int self_promoting_args_p ();
int comp_target_types ();
extern tree shorten_compare ();
void warn_for_assignment ();
extern void binary_op_error ();
static tree pointer_int_sum ();
static tree pointer_diff ();
static tree convert_sequence ();
/* static */ tree unary_complex_lvalue ();
static void pedantic_lvalue_warning ();
tree truthvalue_conversion ();

extern rtx original_result_rtx;

/* Return the target type of TYPE, which meas return T for:
   T*, T&, T[], T (...), and otherwise, just T.  */

tree
target_type (type)
     tree type;
{
  if (TREE_CODE (type) == REFERENCE_TYPE)
    type = TREE_TYPE (type);
  while (TREE_CODE (type) == POINTER_TYPE
	 || TREE_CODE (type) == ARRAY_TYPE
	 || TREE_CODE (type) == FUNCTION_TYPE
	 || TREE_CODE (type) == METHOD_TYPE
	 || TREE_CODE (type) == OFFSET_TYPE)
    type = TREE_TYPE (type);
  return type;
}

/* Do `exp = require_complete_type (exp);' to make sure exp
   does not have an incomplete type.  (That includes void types.)  */

tree
require_complete_type (value)
     tree value;
{
  tree type = TREE_TYPE (value);

  /* First, detect a valid value with a complete type.  */
  if (TYPE_SIZE (type) != 0
      && type != void_type_node)
    return value;

  /* If we see X::Y, we build an OFFSET_TYPE which has
     not been laid out.  Try to avoid an error by interpreting
     it as this->X::Y, if reasonable.  */
  if (TREE_CODE (value) == OFFSET_REF
      && C_C_D != 0
      && TREE_OPERAND (value, 0) == C_C_D)
    {
      tree base, member = TREE_OPERAND (value, 1);
      tree basetype = TYPE_OFFSET_BASETYPE (type);
      my_friendly_assert (TREE_CODE (member) == FIELD_DECL, 305);
      base = convert_pointer_to (basetype, current_class_decl);
      value = build (COMPONENT_REF, TREE_TYPE (member),
		     build_indirect_ref (base, NULL_PTR), member);
      return require_complete_type (value);
    }

  incomplete_type_error (value, type);
  return error_mark_node;
}

/* Return truthvalue of whether type of EXP is instantiated.  */
int
type_unknown_p (exp)
     tree exp;
{
  return (TREE_CODE (exp) == TREE_LIST
	  || TREE_TYPE (exp) == unknown_type_node
	  || (TREE_CODE (TREE_TYPE (exp)) == OFFSET_TYPE
	      && TREE_TYPE (TREE_TYPE (exp)) == unknown_type_node));
}

/* Do `exp = require_instantiated_type (type, exp);' to make sure EXP
   does not have an uninstantiated type.
   TYPE is type to instantiate with, if uninstantiated.  */
tree
require_instantiated_type (type, exp, errval)
     tree type, exp, errval;
{
  if (TREE_TYPE (exp) == unknown_type_node
      || (TREE_CODE (TREE_TYPE (exp)) == OFFSET_TYPE
	  && TREE_TYPE (TREE_TYPE (exp)) == unknown_type_node))
    {
      exp = instantiate_type (type, exp, 1);
      if (TREE_TYPE (exp) == error_mark_node)
	return errval;
    }
  return exp;
}

/* Return a variant of TYPE which has all the type qualifiers of LIKE
   as well as those of TYPE.  */

static tree
qualify_type (type, like)
     tree type, like;
{
  int constflag = TYPE_READONLY (type) || TYPE_READONLY (like);
  int volflag = TYPE_VOLATILE (type) || TYPE_VOLATILE (like);
  /* @@ Must do member pointers here.  */
  return build_type_variant (type, constflag, volflag);
}

/* Return the common type of two parameter lists.
   We assume that comptypes has already been done and returned 1;
   if that isn't so, this may crash.

   As an optimization, free the space we allocate if the parameter
   lists are already common.  */

tree
commonparms (p1, p2)
     tree p1, p2;
{
  tree oldargs = p1, newargs, n;
  int i, len;
  int any_change = 0;
  char *first_obj = (char *) oballoc (0);

  len = list_length (p1);
  newargs = tree_last (p1);

  if (newargs == void_list_node)
    i = 1;
  else
    {
      i = 0;
      newargs = 0;
    }

  for (; i < len; i++)
    newargs = tree_cons (NULL_TREE, NULL_TREE, newargs);

  n = newargs;

  for (i = 0; p1;
       p1 = TREE_CHAIN (p1), p2 = TREE_CHAIN (p2), n = TREE_CHAIN (n), i++)
    {
      if (TREE_PURPOSE (p1) && !TREE_PURPOSE (p2))
	{
	  /* We used to give a warning here that advised about a default
	     argument being given in the prototype but not in the function's
	     declaration.  It's best not to bother.  */
	  TREE_PURPOSE (n) = TREE_PURPOSE (p1);
	  any_change = 1;
	}
      else if (! TREE_PURPOSE (p1))
	{
	  if (TREE_PURPOSE (p2))
	    {
	      TREE_PURPOSE (n) = TREE_PURPOSE (p2);
	      any_change = 1;
	    }
	}
      else
	{
	  int cmp = simple_cst_equal (TREE_PURPOSE (p1), TREE_PURPOSE (p2));
	  if (cmp < 0)
	    my_friendly_abort (111);
	  if (cmp == 0)
	    {
	      error ("redeclaration of default argument %d", i+1);
	      any_change = 1;
	    }
	  TREE_PURPOSE (n) = TREE_PURPOSE (p2);
	}
      if (TREE_VALUE (p1) != TREE_VALUE (p2))
	{
	  any_change = 1;
	  TREE_VALUE (n) = common_type (TREE_VALUE (p1), TREE_VALUE (p2));
	}
      else
	TREE_VALUE (n) = TREE_VALUE (p1);
    }
  if (! any_change)
    {
      obfree (first_obj);
      return oldargs;
    }

  return newargs;
}

/* Return the common type of two types.
   We assume that comptypes has already been done and returned 1;
   if that isn't so, this may crash.

   This is the type for the result of most arithmetic operations
   if the operands have the given two types.

   We do not deal with enumeral types here because they have already been
   converted to integer types.  */

tree
common_type (t1, t2)
     tree t1, t2;
{
  register enum tree_code code1;
  register enum tree_code code2;

  /* Save time if the two types are the same.  */

  if (t1 == t2) return t1;

  /* If one type is nonsense, use the other.  */
  if (t1 == error_mark_node)
    return t2;
  if (t2 == error_mark_node)
    return t1;

  /* Treat an enum type as the unsigned integer type of the same width.  */

  if (TREE_CODE (t1) == ENUMERAL_TYPE)
    t1 = type_for_size (TYPE_PRECISION (t1), 1);
  if (TREE_CODE (t2) == ENUMERAL_TYPE)
    t2 = type_for_size (TYPE_PRECISION (t2), 1);

  code1 = TREE_CODE (t1);
  code2 = TREE_CODE (t2);

  switch (code1)
    {
    case INTEGER_TYPE:
    case REAL_TYPE:
      /* If only one is real, use it as the result.  */

      if (code1 == REAL_TYPE && code2 != REAL_TYPE)
	return t1;

      if (code2 == REAL_TYPE && code1 != REAL_TYPE)
	return t2;

      /* Both real or both integers; use the one with greater precision.  */

      if (TYPE_PRECISION (t1) > TYPE_PRECISION (t2))
	return t1;
      else if (TYPE_PRECISION (t2) > TYPE_PRECISION (t1))
	return t2;

      /* Same precision.  Prefer longs to ints even when same size.  */

      if (t1 == long_unsigned_type_node
	  || t2 == long_unsigned_type_node)
	return long_unsigned_type_node;

      if (t1 == long_integer_type_node
	  || t2 == long_integer_type_node)
	{
	  /* But preserve unsignedness from the other type,
	     since long cannot hold all the values of an unsigned int.  */
	  if (TREE_UNSIGNED (t1) || TREE_UNSIGNED (t2))
	    return long_unsigned_type_node;
	  return long_integer_type_node;
	}

      /* Otherwise prefer the unsigned one.  */

      if (TREE_UNSIGNED (t1))
	return t1;
      else return t2;

    case POINTER_TYPE:
    case REFERENCE_TYPE:
      /* For two pointers, do this recursively on the target type,
	 and combine the qualifiers of the two types' targets.  */
      /* This code was turned off; I don't know why.
 	 But ANSI C++ specifies doing this with the qualifiers.
 	 So I turned it on again.  */
      {
	tree target = common_type (TYPE_MAIN_VARIANT (TREE_TYPE (t1)),
				   TYPE_MAIN_VARIANT (TREE_TYPE (t2)));
	int constp
	  = TYPE_READONLY (TREE_TYPE (t1)) || TYPE_READONLY (TREE_TYPE (t2));
	int volatilep
	  = TYPE_VOLATILE (TREE_TYPE (t1)) || TYPE_VOLATILE (TREE_TYPE (t2));
	target = build_type_variant (target, constp, volatilep);
	if (code1 == POINTER_TYPE)
	  return build_pointer_type (target);
	else
	  return build_reference_type (target);
      }
#if 0
    case POINTER_TYPE:
      return build_pointer_type (common_type (TREE_TYPE (t1), TREE_TYPE (t2)));

    case REFERENCE_TYPE:
      return build_reference_type (common_type (TREE_TYPE (t1), TREE_TYPE (t2)));
#endif

    case ARRAY_TYPE:
      {
	tree elt = common_type (TREE_TYPE (t1), TREE_TYPE (t2));
	/* Save space: see if the result is identical to one of the args.  */
	if (elt == TREE_TYPE (t1) && TYPE_DOMAIN (t1))
	  return t1;
	if (elt == TREE_TYPE (t2) && TYPE_DOMAIN (t2))
	  return t2;
	/* Merge the element types, and have a size if either arg has one.  */
	return build_array_type (elt, TYPE_DOMAIN (TYPE_DOMAIN (t1) ? t1 : t2));
      }

    case FUNCTION_TYPE:
      /* Function types: prefer the one that specified arg types.
	 If both do, merge the arg types.  Also merge the return types.  */
      {
	tree valtype = common_type (TREE_TYPE (t1), TREE_TYPE (t2));
	tree p1 = TYPE_ARG_TYPES (t1);
	tree p2 = TYPE_ARG_TYPES (t2);
	tree rval, raises;

	/* Save space: see if the result is identical to one of the args.  */
	if (valtype == TREE_TYPE (t1) && ! p2)
	  return t1;
	if (valtype == TREE_TYPE (t2) && ! p1)
	  return t2;

	/* Simple way if one arg fails to specify argument types.  */
	if (p1 == NULL_TREE || TREE_VALUE (p1) == void_type_node)
	  {
	    rval = build_function_type (valtype, p2);
	    if (raises = TYPE_RAISES_EXCEPTIONS (t2))
	      rval = build_exception_variant (NULL_TREE, rval, raises);
	    return rval;
	  }
	raises = TYPE_RAISES_EXCEPTIONS (t1);
	if (p2 == NULL_TREE || TREE_VALUE (p2) == void_type_node)
	  {
	    rval = build_function_type (valtype, p1);
	    if (raises)
	      rval = build_exception_variant (NULL_TREE, rval, raises);
	    return rval;
	  }

	rval = build_function_type (valtype, commonparms (p1, p2));
	return build_exception_variant (NULL_TREE, rval, raises);
      }

    case RECORD_TYPE:
    case UNION_TYPE:
      my_friendly_assert (TYPE_MAIN_VARIANT (t1) == t1
			  && TYPE_MAIN_VARIANT (t2) == t2, 306);

      if (binfo_or_else (t1, t2))
	return t1;
      compiler_error ("common_type called with uncommon aggregate types");
      return t1;

    case METHOD_TYPE:
      if (TYPE_METHOD_BASETYPE (t1) == TYPE_METHOD_BASETYPE (t2)
	  && TREE_CODE (TREE_TYPE (t1)) == TREE_CODE (TREE_TYPE (t2)))
	{
	  /* Get this value the long way, since TYPE_METHOD_BASETYPE
	     is just the main variant of this.  */
	  tree basetype = TREE_TYPE (TREE_VALUE (TYPE_ARG_TYPES (t1)));
	  tree raises, t3;

	  raises = TYPE_RAISES_EXCEPTIONS (t1);

	  /* If this was a member function type, get back to the
	     original type of type member function (i.e., without
	     the class instance variable up front.  */
	  t1 = build_function_type (TREE_TYPE (t1), TREE_CHAIN (TYPE_ARG_TYPES (t1)));
	  t2 = build_function_type (TREE_TYPE (t2), TREE_CHAIN (TYPE_ARG_TYPES (t2)));
	  t3 = common_type (t1, t2);
	  t3 = build_cplus_method_type (basetype, TREE_TYPE (t3), TYPE_ARG_TYPES (t3));
	  return build_exception_variant (basetype, t3, raises);
	}
      compiler_error ("common_type called with uncommon method types");
      return t1;

    case OFFSET_TYPE:
      if (TYPE_OFFSET_BASETYPE (t1) == TYPE_OFFSET_BASETYPE (t2)
	  && TREE_CODE (TREE_TYPE (t1)) == TREE_CODE (TREE_TYPE (t2)))
	{
	  tree basetype = TYPE_OFFSET_BASETYPE (t1);
	  return build_offset_type (basetype,
				    common_type (TREE_TYPE (t1), TREE_TYPE (t2)));
	}
      compiler_error ("common_type called with uncommon member types");
      return t1;

    default:
      return t1;
    }
}

/* Return 1 if TYPE1 and TYPE2 raise the same exceptions.  */
int
compexcepttypes (t1, t2, strict)
     tree t1, t2;
     int strict;
{
  return TYPE_RAISES_EXCEPTIONS (t1) == TYPE_RAISES_EXCEPTIONS (t2);
}

static int
comp_array_types (cmp, t1, t2, strict)
     register int (*cmp)();
     tree t1, t2;
     int strict;
{
  tree d1 = TYPE_DOMAIN (t1);
  tree d2 = TYPE_DOMAIN (t2);

  /* Target types must match incl. qualifiers.  */
  if (!(TREE_TYPE (t1) == TREE_TYPE (t2)
	|| (*cmp) (TREE_TYPE (t1), TREE_TYPE (t2), strict)))
    return 0;

  /* Sizes must match unless one is missing or variable.  */
  if (d1 == 0 || d2 == 0 || d1 == d2
      || TREE_CODE (TYPE_MIN_VALUE (d1)) != INTEGER_CST
      || TREE_CODE (TYPE_MIN_VALUE (d2)) != INTEGER_CST
      || TREE_CODE (TYPE_MAX_VALUE (d1)) != INTEGER_CST
      || TREE_CODE (TYPE_MAX_VALUE (d2)) != INTEGER_CST)
    return 1;

  return ((TREE_INT_CST_LOW (TYPE_MIN_VALUE (d1))
	   == TREE_INT_CST_LOW (TYPE_MIN_VALUE (d2)))
	  && (TREE_INT_CST_HIGH (TYPE_MIN_VALUE (d1))
	      == TREE_INT_CST_HIGH (TYPE_MIN_VALUE (d2)))
	  && (TREE_INT_CST_LOW (TYPE_MAX_VALUE (d1))
	      == TREE_INT_CST_LOW (TYPE_MAX_VALUE (d2)))
	  && (TREE_INT_CST_HIGH (TYPE_MAX_VALUE (d1))
	      == TREE_INT_CST_HIGH (TYPE_MAX_VALUE (d2))));
}

/* Return 1 if TYPE1 and TYPE2 are compatible types for assignment
   or various other operations.  This is what ANSI C++ speaks of as
   "being the same".

   For C++: argument STRICT says we should be strict about this
   comparison:

	1 : strict (compared according to ANSI C)
	0 : <= (compared according to C++)
	-1: <= or >= (relaxed)

   Otherwise, pointers involving base classes and derived classes
   can be mixed as legal: i.e. a pointer to a base class may be assigned
   to a pointer to one of its derived classes, as per C++. A pointer to
   a derived class may be passed as a parameter to a function expecting a
   pointer to a base classes. These allowances do not commute. In this
   case, TYPE1 is assumed to be the base class, and TYPE2 is assumed to
   be the derived class.  */
int
comptypes (type1, type2, strict)
     tree type1, type2;
     int strict;
{
  register tree t1 = type1;
  register tree t2 = type2;

  /* Suppress errors caused by previously reported errors */

  if (t1 == t2)
    return 1;
  /* This should never happen.  */
  my_friendly_assert (t1 != error_mark_node, 307);

  /* We don't want this to happen.  */
  if (t2 == error_mark_node)
    {
      warning ("Internal error: t2 == error_mark_node in `comptypes'");
      return 0;
    }

  if (strict < 0)
    {
      /* Treat an enum type as the unsigned integer type of the same width.  */

      if (TREE_CODE (t1) == ENUMERAL_TYPE)
	t1 = type_for_size (TYPE_PRECISION (t1), 1);
      if (TREE_CODE (t2) == ENUMERAL_TYPE)
	t2 = type_for_size (TYPE_PRECISION (t2), 1);
    }

  if (t1 == t2)
    return 1;

  /* Different classes of types can't be compatible.  */

  if (TREE_CODE (t1) != TREE_CODE (t2)) return 0;

  /* Qualifiers must match.  */

  if (TYPE_READONLY (t1) != TYPE_READONLY (t2))
    return 0;
  if (TREE_THIS_VOLATILE (t1) != TREE_THIS_VOLATILE (t2))
    return 0;

  switch (TREE_CODE (t1))
    {
    case RECORD_TYPE:
    case UNION_TYPE:
      if (t1 == t2)
	return 1;
      if (strict <= 0)
	goto look_hard;
      return 0;

    case OFFSET_TYPE:
      return (comptypes (TYPE_POINTER_TO (TYPE_OFFSET_BASETYPE (t1)),
			 TYPE_POINTER_TO (TYPE_OFFSET_BASETYPE (t2)), strict)
	      && comptypes (TREE_TYPE (t1), TREE_TYPE (t2), strict));

    case METHOD_TYPE:
      if (! compexcepttypes (t1, t2, strict))
	return 0;

      /* This case is anti-symmetrical!
	 One can pass a base member (or member function)
	 to something expecting a derived member (or member function),
	 but not vice-versa!  */

      return (comptypes (TYPE_POINTER_TO (TYPE_METHOD_BASETYPE (t2)),
			 TYPE_POINTER_TO (TYPE_METHOD_BASETYPE (t1)), strict)
	      && comptypes (TREE_TYPE (t1), TREE_TYPE (t2), strict)
	      && compparms (TREE_CHAIN (TYPE_ARG_TYPES (t1)),
			    TREE_CHAIN (TYPE_ARG_TYPES(t2)), strict));
    case POINTER_TYPE:
    case REFERENCE_TYPE:
      t1 = TREE_TYPE (t1);
      t2 = TREE_TYPE (t2);
      if (t1 == t2)
	return 1;
      if (strict <= 0)
	{
	  if (TREE_CODE (t1) == RECORD_TYPE && TREE_CODE (t2) == RECORD_TYPE)
	    {
	      int rval;
	    look_hard:
	      rval = t1 == t2 || UNIQUELY_DERIVED_FROM_P (t1, t2);

	      if (rval)
		return 1;
	      if (strict < 0)
		return UNIQUELY_DERIVED_FROM_P (t2, t1);
	    }
	  return 0;
	}
      else
	return comptypes (t1, t2, strict);

    case FUNCTION_TYPE:
      if (! compexcepttypes (t1, t2, strict))
	return 0;

      return ((TREE_TYPE (t1) == TREE_TYPE (t2)
	       || comptypes (TREE_TYPE (t1), TREE_TYPE (t2), strict))
	      && compparms (TYPE_ARG_TYPES (t1), TYPE_ARG_TYPES (t2), strict));

    case ARRAY_TYPE:
      /* Target types must match incl. qualifiers.  */
      return comp_array_types (comptypes, t1, t2, strict);

    }
  return 0;
}

/* Return 1 if TTL and TTR are pointers to types that are equivalent,
   ignoring their qualifiers.

   NPTRS is the number of pointers we can strip off and keep cool.
   This is used to permit (for aggr A, aggr B) A, B* to convert to A*,
   but to not permit B** to convert to A**.  */

int
comp_target_types (ttl, ttr, nptrs)
     tree ttl, ttr;
     int nptrs;
{
  ttl = TYPE_MAIN_VARIANT (ttl);
  ttr = TYPE_MAIN_VARIANT (ttr);
  if (ttl == ttr)
    return 1;

  if (TREE_CODE (ttr) != TREE_CODE (ttl))
    return 0;

  if (TREE_CODE (ttr) == POINTER_TYPE)
    return comp_target_types (TREE_TYPE (ttl), TREE_TYPE (ttr), nptrs - 1);

  if (TREE_CODE (ttr) == REFERENCE_TYPE)
    return comp_target_types (TREE_TYPE (ttl), TREE_TYPE (ttr), nptrs);
  if (TREE_CODE (ttr) == ARRAY_TYPE)
    return comp_array_types (comp_target_types, ttl, ttr, 0);
  else if (TREE_CODE (ttr) == FUNCTION_TYPE || TREE_CODE (ttr) == METHOD_TYPE)
    if (comp_target_types (TREE_TYPE (ttl), TREE_TYPE (ttr), nptrs))
      switch (comp_target_parms (TYPE_ARG_TYPES (ttl), TYPE_ARG_TYPES (ttr), 0))
	{
	case 0:
	  return 0;
	case 1:
	  return 1;
	case 2:
	  warning ("contravariance violation for method types ignored");
	  return 1;
	default:
	  my_friendly_abort (112);
	}
    else
      return 0;

  /* for C++ */
  else if (TREE_CODE (ttr) == OFFSET_TYPE)
    {
      /* Contravariance: we can assign a pointer to base member to a pointer
	 to derived member.  Note difference from simple pointer case, where
	 we can pass a pointer to derived to a pointer to base.  */
      if (comptypes (TYPE_OFFSET_BASETYPE (ttr), TYPE_OFFSET_BASETYPE (ttl), 0))
	return comp_target_types (TREE_TYPE (ttl), TREE_TYPE (ttr), nptrs);
      else if (comptypes (TYPE_OFFSET_BASETYPE (ttl), TYPE_OFFSET_BASETYPE (ttr), 0)
	       && comp_target_types (TREE_TYPE (ttl), TREE_TYPE (ttr), nptrs))
	{
	  warning ("contravariance violation for member types ignored");
	  return 1;
	}
    }
  else if (IS_AGGR_TYPE (ttl))
    {
      if (nptrs < 0)
	return 0;
      return comptypes (TYPE_POINTER_TO (ttl), TYPE_POINTER_TO (ttr), 0);
    }

  return 0;
}

/* If two types share a common base type, return that basetype.
   If there is not a unique most-derived base type, this function
   returns ERROR_MARK_NODE.  */
tree
common_base_type (tt1, tt2)
     tree tt1, tt2;
{
  tree best = NULL_TREE, tmp;
  int i;

  /* If one is a baseclass of another, that's good enough.  */
  if (UNIQUELY_DERIVED_FROM_P (tt1, tt2))
    return tt1;
  if (UNIQUELY_DERIVED_FROM_P (tt2, tt1))
    return tt2;

  /* If they share a virtual baseclass, that's good enough.  */
  for (tmp = CLASSTYPE_VBASECLASSES (tt1); tmp; tmp = TREE_CHAIN (tmp))
    {
      if (binfo_member (BINFO_TYPE (tmp), CLASSTYPE_VBASECLASSES (tt2)))
	return BINFO_TYPE (tmp);
    }

  /* Otherwise, try to find a unique baseclass of TT1
     that is shared by TT2, and follow that down.  */
  for (i = CLASSTYPE_N_BASECLASSES (tt1)-1; i >= 0; i--)
    {
      tree basetype = TYPE_BINFO_BASETYPE (tt1, i);
      tree trial = common_base_type (basetype, tt2);
      if (trial)
	{
	  if (trial == error_mark_node)
	    return trial;
	  if (best == NULL_TREE)
	    best = trial;
	  else if (best != trial)
	    return error_mark_node;
	}
    }

  /* Same for TT2.  */
  for (i = CLASSTYPE_N_BASECLASSES (tt2)-1; i >= 0; i--)
    {
      tree basetype = TYPE_BINFO_BASETYPE (tt2, i);
      tree trial = common_base_type (tt1, basetype);
      if (trial)
	{
	  if (trial == error_mark_node)
	    return trial;
	  if (best == NULL_TREE)
	    best = trial;
	  else if (best != trial)
	    return error_mark_node;
	}
    }
  return best;
}

/* Subroutines of `comptypes'.  */

/* Return 1 if two parameter type lists PARMS1 and PARMS2
   are equivalent in the sense that functions with those parameter types
   can have equivalent types.
   If either list is empty, we win.
   Otherwise, the two lists must be equivalent, element by element.

   C++: See comment above about TYPE1, TYPE2, STRICT.
   If STRICT == 3, it means checking is strict, but do not compare
   default parameter values.  */
int
compparms (parms1, parms2, strict)
     tree parms1, parms2;
     int strict;
{
  register tree t1 = parms1, t2 = parms2;

  /* An unspecified parmlist matches any specified parmlist
     whose argument types don't need default promotions.  */

  if (t1 == 0)
    return self_promoting_args_p (t2);
  if (t2 == 0)
    return self_promoting_args_p (t1);

  while (1)
    {
      if (t1 == 0 && t2 == 0)
	return 1;
      /* If one parmlist is shorter than the other,
	 they fail to match, unless STRICT is <= 0.  */
      if (t1 == 0 || t2 == 0)
	{
	  if (strict > 0)
	    return 0;
	  if (strict < 0)
	    return 1;
	  if (strict == 0)
	    return t1 && TREE_PURPOSE (t1);
	}
      if (! comptypes (TREE_VALUE (t2), TREE_VALUE (t1), strict))
	{
	  if (strict > 0)
	    return 0;
	  if (strict == 0)
	    return t2 == void_list_node && TREE_PURPOSE (t1);
	  return TREE_PURPOSE (t1) || TREE_PURPOSE (t2);
	}
      if (strict != 3 && TREE_PURPOSE (t1) && TREE_PURPOSE (t2))
	{
	  int cmp = simple_cst_equal (TREE_PURPOSE (t1), TREE_PURPOSE (t2));
	  if (cmp < 0)
	    my_friendly_abort (113);
	  if (cmp == 0)
	    return 0;
	}

      t1 = TREE_CHAIN (t1);
      t2 = TREE_CHAIN (t2);
    }
}

/* This really wants return whether or not parameter type lists
   would make their owning functions assignment compatible or not.  */
int
comp_target_parms (parms1, parms2, strict)
     tree parms1, parms2;
     int strict;
{
  register tree t1 = parms1, t2 = parms2;
  int warn_contravariance = 0;

  /* An unspecified parmlist matches any specified parmlist
     whose argument types don't need default promotions.  */

  if (t1 == 0)
    return self_promoting_args_p (t2);
  if (t2 == 0)
    return self_promoting_args_p (t1);

  for (; t1 || t2; t1 = TREE_CHAIN (t1), t2 = TREE_CHAIN (t2))
    {
      tree p1, p2;

      /* If one parmlist is shorter than the other,
	 they fail to match, unless STRICT is <= 0.  */
      if (t1 == 0 || t2 == 0)
	{
	  if (strict > 0)
	    return 0;
	  if (strict < 0)
	    return 1 + warn_contravariance;
	  return ((t1 && TREE_PURPOSE (t1)) + warn_contravariance);
	}
      p1 = TREE_VALUE (t1);
      p2 = TREE_VALUE (t2);
      if (p1 == p2)
	continue;
      if ((TREE_CODE (p1) == POINTER_TYPE && TREE_CODE (p2) == POINTER_TYPE)
	  || (TREE_CODE (p1) == REFERENCE_TYPE && TREE_CODE (p2) == REFERENCE_TYPE))
	{
	  if (strict <= 0
	      && (TYPE_MAIN_VARIANT (TREE_TYPE (p1))
		  == TYPE_MAIN_VARIANT (TREE_TYPE (p2))))
	    continue;

	  /* The following is wrong for contravariance,
	     but many programs depend on it.  */
	  if (TREE_TYPE (p1) == void_type_node)
	    continue;
	  if (TREE_TYPE (p2) == void_type_node)
	    {
	      warn_contravariance = 1;
	      continue;
	    }
	  if (IS_AGGR_TYPE (TREE_TYPE (p1)))
	    {
	      if (comptypes (p2, p1, 0) == 0)
		{
		  if (comptypes (p1, p2, 0) != 0)
		    warn_contravariance = 1;
		  else
		    return 0;
		}
	      continue;
	    }
	}
      /* Note backwards order due to contravariance.  */
      if (comp_target_types (p2, p1, 1) == 0)
	{
	  if (comp_target_types (p1, p2, 1))
	    {
	      warn_contravariance = 1;
	      continue;
	    }
	  if (strict > 0)
	    return 0;
#if 0
	  /* What good do these cases do?  */
	  if (strict == 0)
	    return p2 == void_type_node && TREE_PURPOSE (t1);
	  return TREE_PURPOSE (t1) || TREE_PURPOSE (t2);
#endif
	}
      /* Target types are compatible--just make sure that if
	 we use parameter lists, that they are ok as well.  */
      if (TREE_CODE (p1) == FUNCTION_TYPE || TREE_CODE (p1) == METHOD_TYPE)
	switch (comp_target_parms (TYPE_ARG_TYPES (p1),
				   TYPE_ARG_TYPES (p2),
				   strict))
	  {
	  case 0:
	    return 0;
	  case 1:
	    break;
	  case 2:
	    warn_contravariance = 1;
	  }

      if (TREE_PURPOSE (t1) && TREE_PURPOSE (t2))
	{
	  int cmp = simple_cst_equal (TREE_PURPOSE (t1), TREE_PURPOSE (t2));
	  if (cmp < 0)
	    my_friendly_abort (114);
	  if (cmp == 0)
	    return 0;
	}
    }
  return 1 + warn_contravariance;
}

/* Return 1 if PARMS specifies a fixed number of parameters
   and none of their types is affected by default promotions.  */

static int
self_promoting_args_p (parms)
     tree parms;
{
  register tree t;
  for (t = parms; t; t = TREE_CHAIN (t))
    {
      register tree type = TREE_VALUE (t);

      if (TREE_CHAIN (t) == 0 && type != void_type_node)
	return 0;

      if (TYPE_MAIN_VARIANT (type) == float_type_node)
	return 0;

      if (type == 0)
	return 0;

      if (C_PROMOTING_INTEGER_TYPE_P (type))
	return 0;
    }
  return 1;
}

/* Return an unsigned type the same as TYPE in other respects.

   C++: must make these work for type variants as well.  */

tree
unsigned_type (type)
     tree type;
{
  tree type1 = TYPE_MAIN_VARIANT (type);
  if (type1 == signed_char_type_node || type1 == char_type_node)
    return unsigned_char_type_node;
  if (type1 == integer_type_node)
    return unsigned_type_node;
  if (type1 == short_integer_type_node)
    return short_unsigned_type_node;
  if (type1 == long_integer_type_node)
    return long_unsigned_type_node;
  if (type1 == long_long_integer_type_node)
    return long_long_unsigned_type_node;
  return type;
}

/* Return a signed type the same as TYPE in other respects.  */

tree
signed_type (type)
     tree type;
{
  tree type1 = TYPE_MAIN_VARIANT (type);
  if (type1 == unsigned_char_type_node || type1 == char_type_node)
    return signed_char_type_node;
  if (type1 == unsigned_type_node)
    return integer_type_node;
  if (type1 == short_unsigned_type_node)
    return short_integer_type_node;
  if (type1 == long_unsigned_type_node)
    return long_integer_type_node;
  if (type1 == long_long_unsigned_type_node)
    return long_long_integer_type_node;
  return type;
}

/* Return a type the same as TYPE except unsigned or
   signed according to UNSIGNEDP.  */

tree
signed_or_unsigned_type (unsignedp, type)
     int unsignedp;
     tree type;
{
  if (TREE_CODE (type) != INTEGER_TYPE)
    return type;
  if (TYPE_PRECISION (type) == TYPE_PRECISION (signed_char_type_node))
    return unsignedp ? unsigned_char_type_node : signed_char_type_node;
  if (TYPE_PRECISION (type) == TYPE_PRECISION (integer_type_node)) 
    return unsignedp ? unsigned_type_node : integer_type_node;
  if (TYPE_PRECISION (type) == TYPE_PRECISION (short_integer_type_node)) 
    return unsignedp ? short_unsigned_type_node : short_integer_type_node;
  if (TYPE_PRECISION (type) == TYPE_PRECISION (long_integer_type_node)) 
    return unsignedp ? long_unsigned_type_node : long_integer_type_node;
  if (TYPE_PRECISION (type) == TYPE_PRECISION (long_long_integer_type_node)) 
    return (unsignedp ? long_long_unsigned_type_node
	    : long_long_integer_type_node);
  return type;
}

tree
c_sizeof (type)
     tree type;
{
  enum tree_code code = TREE_CODE (type);
  tree t;

  if (code == FUNCTION_TYPE)
    {
      if (pedantic || warn_pointer_arith)
	pedwarn ("ANSI C++ forbids taking the sizeof a function type");
      return size_int (1);
    }
  if (code == METHOD_TYPE)
    {
      if (pedantic || warn_pointer_arith)
	pedwarn ("ANSI C++ forbids taking the sizeof a method type");
      return size_int (1);
    }
  if (code == VOID_TYPE)
    {
      if (pedantic || warn_pointer_arith)
	pedwarn ("ANSI C++ forbids taking the sizeof a void type");
      return size_int (1);
    }
  if (code == ERROR_MARK)
    return size_int (1);

  /* ARM $5.3.2: ``When applied to a reference, the result is the size of the
     referenced object.'' */
  if (code == REFERENCE_TYPE)
    type = TREE_TYPE (type);

  if (TYPE_SIZE (type) == 0)
    {
      error ("sizeof applied to an incomplete type");
      return size_int (0);
    }

  /* Convert in case a char is more than one unit.  */
  t = size_binop (CEIL_DIV_EXPR, TYPE_SIZE (type), 
		  size_int (TYPE_PRECISION (char_type_node)));
  force_fit_type (t, 0);
  return t;
}

tree
c_sizeof_nowarn (type)
     tree type;
{
  enum tree_code code = TREE_CODE (type);
  tree t;

  if (code == FUNCTION_TYPE
      || code == METHOD_TYPE
      || code == VOID_TYPE
      || code == ERROR_MARK)
    return size_int (1);
  if (code == REFERENCE_TYPE)
    type = TREE_TYPE (type);

  if (TYPE_SIZE (type) == 0)
    {
      /* ??? Tiemann, why have any diagnostic here?
	 There is none in the corresponding function for C.  */
      warning ("sizeof applied to an incomplete type");
      return size_int (0);
    }

  /* Convert in case a char is more than one unit.  */
  t = size_binop (CEIL_DIV_EXPR, TYPE_SIZE (type), 
		     size_int (TYPE_PRECISION (char_type_node)));
  force_fit_type (t, 0);
  return t;
}

/* Implement the __alignof keyword: Return the minimum required
   alignment of TYPE, measured in bytes.  */

tree
c_alignof (type)
     tree type;
{
  enum tree_code code = TREE_CODE (type);
  tree t;

  if (code == FUNCTION_TYPE || code == METHOD_TYPE)
    return size_int (FUNCTION_BOUNDARY / BITS_PER_UNIT);

  if (code == VOID_TYPE || code == ERROR_MARK)
    return size_int (1);

  /* C++: this is really correct!  */
  if (code == REFERENCE_TYPE)
    type = TREE_TYPE (type);

  t = size_int (TYPE_ALIGN (type) / BITS_PER_UNIT);
  force_fit_type (t, 0);
  return t;
}

/* Perform default promotions for C data used in expressions.
   Arrays and functions are converted to pointers;
   enumeral types or short or char, to int.
   In addition, manifest constants symbols are replaced by their values.

   C++: this will automatically bash references to their target type.  */

tree
default_conversion (exp)
     tree exp;
{
  register tree type = TREE_TYPE (exp);
  register enum tree_code code = TREE_CODE (type);

  if (code == OFFSET_TYPE /* || TREE_CODE (exp) == OFFSET_REF */ )
    {
      if (TREE_CODE (exp) == OFFSET_REF)
	return default_conversion (resolve_offset_ref (exp));

      type = TREE_TYPE (type);
      code = TREE_CODE (type);
    }

  if (code == REFERENCE_TYPE)
    {
      exp = convert_from_reference (exp);
      type = TREE_TYPE (exp);
      code = TREE_CODE (type);
    }

  /* Constants can be used directly unless they're not loadable.  */
  if (TREE_CODE (exp) == CONST_DECL)
    exp = DECL_INITIAL (exp);
  /* Replace a nonvolatile const static variable with its value.  */
  else if (TREE_READONLY_DECL_P (exp) && DECL_MODE (exp) != BLKmode)
    {
      exp = decl_constant_value (exp);
      type = TREE_TYPE (exp);
    }

  /* build_c_cast puts on a NOP_EXPR to make the result not an lvalue.
     Strip such NOP_EXPRs, since EXP is being used in non-lvalue context.  */
  if (TREE_CODE (exp) == NOP_EXPR
      && TREE_TYPE (exp) == TREE_TYPE (TREE_OPERAND (exp, 0)))
    exp = TREE_OPERAND (exp, 0);

  /* Normally convert enums to int,
     but convert wide enums to something wider.  */
  if (code == ENUMERAL_TYPE)
    {
      type = type_for_size (MAX (TYPE_PRECISION (type),
				 TYPE_PRECISION (integer_type_node)),
			    (flag_traditional && TREE_UNSIGNED (type)));
      return convert (type, exp);
    }

  if (C_PROMOTING_INTEGER_TYPE_P (type))
    {
      /* Traditionally, unsignedness is preserved in default promotions.
         Otherwise, retain unsignedness if really not getting bigger.  */
      if (TREE_UNSIGNED (type)
	  && (flag_traditional
	      || TYPE_PRECISION (type) == TYPE_PRECISION (integer_type_node)))
	return convert (unsigned_type_node, exp);
      return convert (integer_type_node, exp);
    }
  if (flag_traditional
      && TYPE_MAIN_VARIANT (type) == float_type_node)
    return convert (double_type_node, exp);
  if (code == VOID_TYPE)
    {
      error ("void value not ignored as it ought to be");
      return error_mark_node;
    }
  if (code == FUNCTION_TYPE)
    {
      return build_unary_op (ADDR_EXPR, exp, 0);
    }
  if (code == METHOD_TYPE)
    {
      if (TREE_CODE (exp) == OFFSET_REF)
	{
	  my_friendly_assert (TREE_CODE (TREE_OPERAND (exp, 1)) == FUNCTION_DECL,
			      308);
	  return build_unary_op (ADDR_EXPR, TREE_OPERAND (exp, 1), 0);
	}
      return build_unary_op (ADDR_EXPR, exp, 0);
    }
  if (code == ARRAY_TYPE)
    {
      register tree adr;
      tree restype = TREE_TYPE (type);
      tree ptrtype;

      if (TREE_CODE (exp) == INDIRECT_REF)
	{
	  /* Stripping away the INDIRECT_REF is not the right
	     thing to do for references...  */
	  tree inner = TREE_OPERAND (exp, 0);
	  if (TREE_CODE (TREE_TYPE (inner)) == REFERENCE_TYPE)
	    {
	      inner = build1 (CONVERT_EXPR,
			      build_pointer_type (TREE_TYPE (TREE_TYPE (inner))),
			      inner);
	      TREE_REFERENCE_EXPR (inner) = 1;
	    }
	  return convert (TYPE_POINTER_TO (TREE_TYPE (type)), inner);
	}

      if (TREE_CODE (exp) == COMPOUND_EXPR)
	{
	  tree op1 = default_conversion (TREE_OPERAND (exp, 1));
	  return build (COMPOUND_EXPR, TREE_TYPE (op1),
			TREE_OPERAND (exp, 0), op1);
	}

      if (!lvalue_p (exp)
	  && ! (TREE_CODE (exp) == CONSTRUCTOR && TREE_STATIC (exp)))
	{
	  error ("invalid use of non-lvalue array");
	  return error_mark_node;
	}

      if (TYPE_READONLY (type) || TYPE_VOLATILE (type))
	restype = build_type_variant (restype, TYPE_READONLY (type),
				      TYPE_VOLATILE (type));

      ptrtype = build_pointer_type (restype);

      if (TREE_CODE (exp) == VAR_DECL)
	{
	  /* ??? This is not really quite correct
	     in that the type of the operand of ADDR_EXPR
	     is not the target type of the type of the ADDR_EXPR itself.
	     Question is, can this lossage be avoided?  */
	  adr = build1 (ADDR_EXPR, ptrtype, exp);
	  if (mark_addressable (exp) == 0)
	    return error_mark_node;
	  TREE_CONSTANT (adr) = staticp (exp);
	  TREE_SIDE_EFFECTS (adr) = 0;   /* Default would be, same as EXP.  */
	  return adr;
	}
      /* This way is better for a COMPONENT_REF since it can
	 simplify the offset for a component.  */
      adr = build_unary_op (ADDR_EXPR, exp, 1);
      return convert (ptrtype, adr);
    }
  return exp;
}

/* Like `build_component_ref, but uses an already found field.
   Must compute visibility for C_C_D.  Otherwise, ok.  */
tree
build_component_ref_1 (datum, field, protect)
     tree datum, field;
     int protect;
{
  register tree basetype = TREE_TYPE (datum);
  register enum tree_code code = TREE_CODE (basetype);
  register tree ref;

  if (code == REFERENCE_TYPE)
    {
      datum = convert_from_reference (datum);
      basetype = TREE_TYPE (datum);
      code = TREE_CODE (basetype);
    }

  if (! IS_AGGR_TYPE_CODE (code))
    {
      if (code != ERROR_MARK)
	error_with_decl (field, "request for member `%s' in something not a class, structure or union");
      return error_mark_node;
    }

  if (TYPE_SIZE (basetype) == 0)
    {
      incomplete_type_error (0, basetype);
      return error_mark_node;
    }

  /* Look up component name in the structure type definition.  */

  if (field == error_mark_node)
    my_friendly_abort (115);

  if (TREE_STATIC (field))
    return field;

  if (datum == C_C_D && ! DECL_PUBLIC (field))
    {
      enum visibility_type visibility
	= compute_visibility (TYPE_BINFO (current_class_type), field);

    if (visibility == visibility_private)
      {
	error_with_decl (field, "field `%s' is private");
	return error_mark_node;
      }
    else if (visibility == visibility_protected)
      {
	error_with_decl (field, "field `%s' is protected");
	return error_mark_node;
      }
    }

  ref = build (COMPONENT_REF, TREE_TYPE (field), datum, field);

  if (TREE_READONLY (datum) || TREE_READONLY (field))
    TREE_READONLY (ref) = 1;
  if (TREE_THIS_VOLATILE (datum) || TREE_THIS_VOLATILE (field))
    TREE_THIS_VOLATILE (ref) = 1;

  return ref;
}

tree
build_component_ref (datum, component, basetype_path, protect)
     tree datum, component, basetype_path;
     int protect;
{
  register tree basetype = TREE_TYPE (datum);
  register enum tree_code code = TREE_CODE (basetype);
  register tree field = NULL;
  register tree ref;

  /* If DATUM is a COMPOUND_EXPR or COND_EXPR, move our reference inside it
     unless we are not to support things not strictly ANSI.  */
  switch (TREE_CODE (datum))
    {
    case COMPOUND_EXPR:
      {
	tree value = build_component_ref (TREE_OPERAND (datum, 1), component,
					  basetype_path, protect);
	return build (COMPOUND_EXPR, TREE_TYPE (value),
		      TREE_OPERAND (datum, 0), value);
      }
    case COND_EXPR:
      return build_conditional_expr
	(TREE_OPERAND (datum, 0),
	 build_component_ref (TREE_OPERAND (datum, 1), component,
			      basetype_path, protect),
	 build_component_ref (TREE_OPERAND (datum, 2), component,
			      basetype_path, protect));
    }

  if (code == REFERENCE_TYPE)
    {
#if 0
      /* TREE_REFERENCE_EXPRs are not converted by `convert_from_reference'.
	 @@ Maybe that is not right.  */
      if (TREE_REFERENCE_EXPR (datum))
	datum = build1 (INDIRECT_REF, TREE_TYPE (basetype), datum);
      else
#endif
	datum = convert_from_reference (datum);
      basetype = TREE_TYPE (datum);
      code = TREE_CODE (basetype);
    }

  /* First, see if there is a field or component with name COMPONENT. */
  if (TREE_CODE (component) == TREE_LIST)
    {
      my_friendly_assert (!(TREE_CHAIN (component) == NULL_TREE
		&& DECL_CHAIN (TREE_VALUE (component)) == NULL_TREE), 309);
      return build (COMPONENT_REF, TREE_TYPE (component), datum, component);
    }
  if (TREE_CODE (component) == TYPE_EXPR)
    return build_component_type_expr (datum, component, NULL_TREE, protect);

  if (! IS_AGGR_TYPE_CODE (code))
    {
      if (code != ERROR_MARK)
	error ("request for member `%s' in something not a class, structure or union",
	       IDENTIFIER_POINTER (component));
      return error_mark_node;
    }

  if (TYPE_SIZE (basetype) == 0)
    {
      incomplete_type_error (0, basetype);
      return error_mark_node;
    }

  if (TREE_CODE (component) == BIT_NOT_EXPR)
    {
      if (TYPE_IDENTIFIER (basetype) != TREE_OPERAND (component, 0))
	{
	  error_with_aggr_type (basetype,
				"destructor specifier `%s::~%s' must have matching names",
				IDENTIFIER_POINTER (TREE_OPERAND (component, 0)));
	  return error_mark_node;
	}
      if (! TYPE_HAS_DESTRUCTOR (basetype))
	{
	  error_with_aggr_type (basetype, "type `%s' has no destructor");
	  return error_mark_node;
	}
      return TREE_VEC_ELT (CLASSTYPE_METHOD_VEC (basetype), 0);
    }

  /* Look up component name in the structure type definition.  */
  if (CLASSTYPE_VFIELD (basetype)
      && DECL_NAME (CLASSTYPE_VFIELD (basetype)) == component)
    /* Special-case this because if we use normal lookups in an ambiguous
       hierarchy, the compiler will abort (because vptr lookups are
       not supposed to be ambiguous.  */
    field = CLASSTYPE_VFIELD (basetype);
  else
    {
      if (basetype_path == NULL_TREE)
	basetype_path = TYPE_BINFO (basetype);
      field = lookup_field (basetype_path, component,
			    protect && ! VFIELD_NAME_P (component), 0);
      if (field == error_mark_node)
	return error_mark_node;

      if (field == NULL_TREE)
	{
	  /* Not found as a data field, look for it as a method.  If found,
	     then if this is the only possible one, return it, else
	     report ambiguity error.  */
	  tree fndecls = lookup_fnfields (basetype_path, component, 1);
	  if (fndecls == error_mark_node)
	    return error_mark_node;
	  if (fndecls)
	    {
	      if (TREE_CHAIN (fndecls) == NULL_TREE
		  && DECL_CHAIN (TREE_VALUE (fndecls)) == NULL_TREE)
		{
		  enum visibility_type visibility;
		  tree fndecl;

		  /* Unique, so use this one now.  */
		  basetype = TREE_PURPOSE (fndecls);
		  fndecl = TREE_VALUE (fndecls);
		  visibility = compute_visibility (TREE_PURPOSE (fndecls), fndecl);
		  if (visibility == visibility_public)
		    {
		      if (DECL_VINDEX (fndecl)
			  && ! resolves_to_fixed_type_p (datum, 0))
			{
			  tree addr = build_unary_op (ADDR_EXPR, datum, 0);
			  addr = convert_pointer_to (DECL_CONTEXT (fndecl), addr);
			  datum = build_indirect_ref (addr, NULL_PTR);
			  my_friendly_assert (datum != error_mark_node, 310);
			  fndecl = build_vfn_ref (&addr, datum, DECL_VINDEX (fndecl));
			}
		      return fndecl;
		    }
		  if (visibility == visibility_protected)
		    error_with_decl (fndecl, "member function `%s' is protected");
		  else
		    error_with_decl (fndecl, "member function `%s' is private");
		  return error_mark_node;
		}
	      else
		return build (COMPONENT_REF, unknown_type_node, datum, fndecls);
	    }

	  if (component == ansi_opname[(int) TYPE_EXPR])
	    error ("%s has no such type conversion operator",
		   code == RECORD_TYPE ? "structure" : "union");
	  else
	    error (code == RECORD_TYPE
		   ? "structure has no member named `%s'"
		   : "union has no member named `%s'",
		   IDENTIFIER_POINTER (component));
	  return error_mark_node;
	}
      else if (TREE_TYPE (field) == error_mark_node)
	return error_mark_node;

      if (TREE_CODE (field) != FIELD_DECL)
	{
	  if (TREE_CODE (field) == TYPE_DECL)
	    {
	      error ("invalid use of type decl `%s' as expression",
		     IDENTIFIER_POINTER (DECL_NAME (field)));
	      return error_mark_node;
	    }
 	  if (DECL_RTL (field) != 0)
	    assemble_external (field);
	  TREE_USED (field) = 1;
	  return field;
	}
    }

  if (DECL_FIELD_CONTEXT (field) != basetype
      && TYPE_USES_COMPLEX_INHERITANCE (basetype))
    {
      tree addr = build_unary_op (ADDR_EXPR, datum, 0);
      if (integer_zerop (addr))
	{
	  error ("invalid reference to NULL ptr, use ptr-to-member instead");
	  return error_mark_node;
	}
      addr = convert_pointer_to (DECL_FIELD_CONTEXT (field), addr);
      datum = build_indirect_ref (addr, NULL_PTR);
      my_friendly_assert (datum != error_mark_node, 311);
    }
  ref = build (COMPONENT_REF, TREE_TYPE (field), break_out_cleanups (datum), field);

  if (TREE_READONLY (datum) || TREE_READONLY (field))
    TREE_READONLY (ref) = 1;
  if (TREE_THIS_VOLATILE (datum) || TREE_THIS_VOLATILE (field))
    TREE_THIS_VOLATILE (ref) = 1;

  return ref;
}

/* Given an expression PTR for a pointer, return an expression
   for the value pointed to.
   ERRORSTRING is the name of the operator to appear in error messages.

   This function may need to overload OPERATOR_FNNAME.
   Must also handle REFERENCE_TYPEs for C++.  */

tree
build_x_indirect_ref (ptr, errorstring)
     tree ptr;
     char *errorstring;
{
  tree rval = build_opfncall (INDIRECT_REF, LOOKUP_NORMAL, ptr, NULL_TREE, NULL_TREE);

  if (rval) return rval;
  return build_indirect_ref (ptr, errorstring);
}

tree
build_indirect_ref (ptr, errorstring)
     tree ptr;
     char *errorstring;
{
  register tree pointer = default_conversion (ptr);
  register tree type = TREE_TYPE (pointer);

  if (ptr == current_class_decl)
    return C_C_D;

  if (TREE_CODE (type) == POINTER_TYPE || TREE_CODE (type) == REFERENCE_TYPE)
    if (TREE_CODE (pointer) == ADDR_EXPR
	&& (TREE_TYPE (TREE_OPERAND (pointer, 0))
	    == TREE_TYPE (type)))
      return TREE_OPERAND (pointer, 0);
    else
      {
	tree t = TREE_TYPE (type);
	register tree ref = build1 (INDIRECT_REF,
				    TYPE_MAIN_VARIANT (t), pointer);

	TREE_READONLY (ref) = TYPE_READONLY (t);
	TREE_THIS_VOLATILE (ref) = TYPE_VOLATILE (t);
	TREE_SIDE_EFFECTS (ref)
	  = TYPE_VOLATILE (t) || TREE_SIDE_EFFECTS (pointer);
	return ref;
      }
  else if (pointer != error_mark_node)
    {
      if (errorstring)
	error ("invalid type argument of `%s'", errorstring);
      else
	error ("invalid type argument");
    }
  return error_mark_node;
}

/* This handles expressions of the form "a[i]", which denotes
   an array reference.

   This is logically equivalent in C to *(a+i), but we may do it differently.
   If A is a variable or a member, we generate a primitive ARRAY_REF.
   This avoids forcing the array out of registers, and can work on
   arrays that are not lvalues (for example, members of structures returned
   by functions).

   If INDEX is of some user-defined type, it must be converted to
   integer type.  Otherwise, to make a compatible PLUS_EXPR, it
   will inherit the type of the array, which will be some pointer type.  */

tree
build_x_array_ref (array, index)
     tree array, index;
{
  tree rval;

  rval = build_opfncall (ARRAY_REF, LOOKUP_NORMAL, array, index, NULL_TREE);
  if (rval)
    return rval;
  return build_array_ref (array, index);
}

tree
build_array_ref (array, index)
     tree array, index;
{
  tree itype;

  if (index == 0)
    {
      error ("subscript missing in array reference");
      return error_mark_node;
    }

  if (TREE_TYPE (array) == error_mark_node
      || TREE_TYPE (index) == error_mark_node)
    return error_mark_node;

  itype = TREE_TYPE (index);
  /* We must check here for the reference, so we can do the possible
     conversions immediately afterwards.  */
  if (TREE_CODE (itype) == REFERENCE_TYPE)
    {
      index = convert_from_reference (index);
      itype = TREE_TYPE (index);
    }

  if (IS_AGGR_TYPE (itype))
    {
      if (TYPE_HAS_INT_CONVERSION (itype))
	index = build_type_conversion (CONVERT_EXPR,
				       integer_type_node, index, 1);
      else
	{
	  error_with_aggr_type (itype,
				"type `%s' requires integer conversion for array indexing");
	  return error_mark_node;
	}
    }

  if (TREE_CODE (TREE_TYPE (array)) == ARRAY_TYPE
      && TREE_CODE (array) != INDIRECT_REF)
    {
      tree rval, type;

      /* Subscripting with type char is likely to lose
	 on a machine where chars are signed.
	 So warn on any machine, but optionally.
	 Don't warn for unsigned char since that type is safe.
	 Don't warn for signed char because anyone who uses that
	 must have done so deliberately.  */
      if (warn_char_subscripts
	  && TYPE_MAIN_VARIANT (TREE_TYPE (index)) == char_type_node)
	warning ("array subscript has type `char'");

      /* Apply default promotions *after* noticing character types.  */
      index = default_conversion (index);

      if (TREE_CODE (TREE_TYPE (index)) != INTEGER_TYPE)
	{
	  error ("array subscript is not an integer");
	  return error_mark_node;
	}

      /* An array that is indexed by a non-constant
	 cannot be stored in a register; we must be able to do
	 address arithmetic on its address.
	 Likewise an array of elements of variable size.  */
      if (TREE_CODE (index) != INTEGER_CST
	  || (TYPE_SIZE (TREE_TYPE (TREE_TYPE (array))) != 0
	      && TREE_CODE (TYPE_SIZE (TREE_TYPE (TREE_TYPE (array)))) != INTEGER_CST))
	{
	  if (mark_addressable (array) == 0)
	    return error_mark_node;
	}

      /* Note in C++ we don't bother warning about subscripting a
	 `register' array, since it's legal in C++ to take the address
	 of something with that storage specification.  */
      if (pedantic && !lvalue_p (array))
	pedwarn ("ANSI C++ forbids subscripting non-lvalue array");

      if (pedantic)
	{
	  tree foo = array;
	  while (TREE_CODE (foo) == COMPONENT_REF)
	    foo = TREE_OPERAND (foo, 0);
	  if (TREE_CODE (foo) == VAR_DECL && DECL_REGISTER (foo))
	    pedwarn ("ANSI C++ forbids subscripting non-lvalue array");
	}

      type = TYPE_MAIN_VARIANT (TREE_TYPE (TREE_TYPE (array)));
      rval = build (ARRAY_REF, type, array, index);
      /* Array ref is const/volatile if the array elements are
	 or if the array is..  */
      TREE_READONLY (rval)
	|= (TYPE_READONLY (TREE_TYPE (TREE_TYPE (array)))
	    | TREE_READONLY (array));
      TREE_SIDE_EFFECTS (rval)
	|= (TYPE_VOLATILE (TREE_TYPE (TREE_TYPE (array)))
	    | TREE_SIDE_EFFECTS (array));
      TREE_THIS_VOLATILE (rval)
	|= (TYPE_VOLATILE (TREE_TYPE (TREE_TYPE (array)))
	    /* This was added by rms on 16 Nov 91.
	       It fixes  vol struct foo *a;  a->elts[1] 
	       in an inline function.
	       Hope it doesn't break something else.  */
	    | TREE_THIS_VOLATILE (array));
      return require_complete_type (fold (rval));
    }

  {
    tree ar = default_conversion (array);
    tree ind = default_conversion (index);

    /* Put the integer in IND to simplify error checking.  */
    if (TREE_CODE (TREE_TYPE (ar)) == INTEGER_TYPE)
      {
	tree temp = ar;
	ar = ind;
	ind = temp;
      }

    if (ar == error_mark_node)
      return ar;

    if (TREE_CODE (TREE_TYPE (ar)) != POINTER_TYPE)
      {
	error ("subscripted value is neither array nor pointer");
	return error_mark_node;
      }
    if (TREE_CODE (TREE_TYPE (ind)) != INTEGER_TYPE)
      {
	error ("array subscript is not an integer");
	return error_mark_node;
      }

    return build_indirect_ref (build_binary_op_nodefault (PLUS_EXPR, ar, ind, PLUS_EXPR),
			       "array indexing");
  }
}

/* Build a function call to function FUNCTION with parameters PARAMS.
   PARAMS is a list--a chain of TREE_LIST nodes--in which the
   TREE_VALUE of each node is a parameter-expression.
   FUNCTION's data type may be a function type or a pointer-to-function.

   For C++: If FUNCTION's data type is a TREE_LIST, then the tree list
   is the list of possible methods that FUNCTION could conceivably
   be.  If the list of methods comes from a class, then it will be
   a list of lists (where each element is associated with the class
   that produced it), otherwise it will be a simple list (for
   functions overloaded in global scope).

   In the first case, TREE_VALUE (function) is the head of one of those
   lists, and TREE_PURPOSE is the name of the function.

   In the second case, TREE_PURPOSE (function) is the function's
   name directly.

   DECL is the class instance variable, usually CURRENT_CLASS_DECL.  */

/*
 * [eichin:19911015.1726EST] actually return a possibly incomplete
 * type
 */
tree
build_x_function_call (function, params, decl)
     tree function, params, decl;
{
  tree type = TREE_TYPE (function);
  int is_method = ((TREE_CODE (function) == TREE_LIST
		    && current_class_type != NULL_TREE
		    && IDENTIFIER_CLASS_VALUE (TREE_PURPOSE (function)) == function)
		   || TREE_CODE (function) == IDENTIFIER_NODE
		   || TREE_CODE (type) == METHOD_TYPE
		   || (TREE_CODE (type) == POINTER_TYPE
		       && TREE_CODE (TREE_TYPE (type)) == METHOD_TYPE));

  /* Handle methods, friends, and overloaded functions, respectively.  */
  if (is_method)
    {
      if (TREE_CODE (function) == FUNCTION_DECL)
	{
	  if (DECL_NAME (function))
	    function = DECL_NAME (function);
	  else
	    function = TYPE_IDENTIFIER (DECL_CLASS_CONTEXT (function));
	}
      else if (TREE_CODE (function) == TREE_LIST)
	{
#if 0
	  if (TREE_CODE (TREE_VALUE (function)) == TREE_LIST)
	    function = TREE_PURPOSE (TREE_VALUE (function));
	  else
	    function = TREE_PURPOSE (function);
#else
	  my_friendly_assert (TREE_CODE (TREE_VALUE (function)) == FUNCTION_DECL, 312);
	  function = TREE_PURPOSE (function);
#endif
	}
      else if (TREE_CODE (function) != IDENTIFIER_NODE)
	{
	  /* Call via a pointer to member function.  */
	  if (decl == NULL_TREE)
	    {
	      error ("pointer to member function called, but not in class scope");
	      return error_mark_node;
	    }
	  if (TREE_CODE (TREE_TYPE (function)) != POINTER_TYPE)
	    function = build (OFFSET_REF, TREE_TYPE (type), NULL_TREE, function);
	  goto do_x_function;
	}

      /* this is an abbreviated method call.
         must go through here in case it is a virtual function.
	 @@ Perhaps this could be optimized.  */

      if (decl == NULL_TREE)
	{
	  if (current_class_type == NULL_TREE)
	    {
	      error ("object missing in call to method `%s'",
		     IDENTIFIER_POINTER (function));
	      return error_mark_node;
	    }
	  /* Yow: call from a static member function.  */
	  decl = build1 (NOP_EXPR,
			 TYPE_POINTER_TO (current_class_type),
			 error_mark_node);
	}

      return build_method_call (decl, function, params,
				NULL_TREE, LOOKUP_NORMAL);
    }
  else if (TREE_CODE (function) == COMPONENT_REF
	   && type == unknown_type_node)
    {
      function = TREE_PURPOSE (TREE_OPERAND (function, 1));
      return build_method_call (decl, function, params,
				NULL_TREE, LOOKUP_NORMAL);
    }
  else if (TREE_CODE (function) == TREE_LIST)
    {
      if (TREE_CHAIN (function) != NULL_TREE)
        {
          if (TREE_CODE (TREE_VALUE (function)) == TEMPLATE_DECL)
            return build_overload_call_maybe (TREE_PURPOSE (function),
					      params, 1, (struct candidate *)0);
          else
            return build_overload_call (TREE_PURPOSE (function), params, 1,
					(struct candidate *)0);
        }
      else if (TREE_VALUE (function) == NULL_TREE)
	{
	  error ("function `%s' declared overloaded, but no definitions appear with which to resolve it",
		 IDENTIFIER_POINTER (TREE_PURPOSE (function)));
	  return error_mark_node;
	}
      else if (TREE_CODE (TREE_VALUE (function)) == TEMPLATE_DECL)
	return build_overload_call_maybe (TREE_PURPOSE (function),
					  params, 1, (struct candidate *)0);
      else
	function = TREE_VALUE (function);
    }
  else if (TREE_CODE (type) == POINTER_TYPE
	   && TREE_CODE (TREE_TYPE (type)) == METHOD_TYPE
	   && (TREE_CODE (function) == VAR_DECL
	       || TREE_CODE (function) == PARM_DECL
	       || TREE_CODE (function) == FIELD_DECL))
    {
      error_with_decl (function, "call via pointer-to-member-function `%s' must be composed with object");
      return error_mark_node;
    }

 do_x_function:
  if (TREE_CODE (function) == OFFSET_REF)
    {
      /* If the component is a data element (or a virtual function), we play
	 games here to make things work.  */
      tree decl_addr;

      if (TREE_OPERAND (function, 0))
	decl = TREE_OPERAND (function, 0);
      else
	decl = C_C_D;

      decl_addr = build_unary_op (ADDR_EXPR, decl, 0);
      function = get_member_function (&decl_addr, decl,
				      TREE_OPERAND (function, 1));
      params = tree_cons (NULL_TREE, decl_addr, params);
      return build_function_call (function, params);
    }

  type = TREE_TYPE (function);
  if (type != error_mark_node)
    {
      if (TREE_CODE (type) == REFERENCE_TYPE)
	type = TREE_TYPE (type);

      if (TYPE_LANG_SPECIFIC (type) && TYPE_OVERLOADS_CALL_EXPR (type))
	return build_opfncall (CALL_EXPR, LOOKUP_NORMAL, function, params, NULL_TREE);
    }

  if (is_method)
    {
      tree fntype = TREE_TYPE (function);
      tree ctypeptr;
      /* Explicitly named method?  */
      if (TREE_CODE (function) == FUNCTION_DECL)
	ctypeptr = TYPE_POINTER_TO (DECL_CLASS_CONTEXT (function));
      /* Expression with ptr-to-method type?  It could either be a plain
	 usage, or it might be a case where the ptr-to-method is being
	 passed in as an argument.  */
      else if (TREE_CODE (fntype) == POINTER_TYPE
	       && (TREE_CODE_CLASS (TREE_CODE (function)) == 'e'
		   || ((TREE_CODE_CLASS (TREE_CODE (function)) == 'd'
			|| TREE_CODE_CLASS (TREE_CODE (function)) == 'r')
		       && TREE_CODE (TREE_TYPE (fntype)) == METHOD_TYPE)))
	{
	  tree rec = TYPE_METHOD_BASETYPE (TREE_TYPE (fntype));
	  ctypeptr = TYPE_POINTER_TO (rec);
	}
      /* Unexpected node type?  */
      else
	my_friendly_abort (116);
      if (decl == NULL_TREE)
	{
	  if (current_function_decl
	      && DECL_STATIC_FUNCTION_P (current_function_decl))
	    error ("invalid call to member function needing `this' in static member function scope");
	  else
	    error ("pointer to member function called, but not in class scope");
	  return error_mark_node;
	}
      if (TREE_CODE (TREE_TYPE (decl)) != POINTER_TYPE)
	{
	  decl = build_unary_op (ADDR_EXPR, decl, 0);
	  decl = convert_pointer_to (TREE_TYPE (ctypeptr), decl);
	}
      else
	decl = build_c_cast (ctypeptr, decl);
      params = tree_cons (NULL_TREE, decl, params);
    }

  return build_function_call (function, params);
}

tree
build_function_call_real (function, params, require_complete)
     tree function, params;
     int require_complete;
{
  register tree fntype, fndecl;
  register tree value_type;
  register tree coerced_params;
  int is_method;

  /* build_c_cast puts on a NOP_EXPR to make the result not an lvalue.
     Strip such NOP_EXPRs, since FUNCTION is used in non-lvalue context.  */
  if (TREE_CODE (function) == NOP_EXPR
      && TREE_TYPE (function) == TREE_TYPE (TREE_OPERAND (function, 0)))
    function = TREE_OPERAND (function, 0);

  if (TREE_CODE (function) == FUNCTION_DECL)
    {
      GNU_xref_call (current_function_decl,
		     IDENTIFIER_POINTER (DECL_NAME (function)
					 ? DECL_NAME (function)
					 : TYPE_IDENTIFIER (DECL_CLASS_CONTEXT (function))));
      assemble_external (function);
      fndecl = function;

      /* Convert anything with function type to a pointer-to-function.  */
      if (pedantic
	  && DECL_NAME (function)
	  && IDENTIFIER_LENGTH (DECL_NAME (function)) == 4
	  && ! strcmp (IDENTIFIER_POINTER (DECL_NAME (function)), "main")
	  && DECL_CONTEXT (function) == NULL_TREE)
	{
	  pedwarn ("ANSI C++ forbids calling `main' from within program");
	}

      /* Differs from default_conversion by not setting TREE_ADDRESSABLE
	 (because calling an inline function does not mean the function
	 needs to be separately compiled).  */

      if (! DECL_INLINE (function))
	{
	  assemble_external (function);
	  TREE_USED (function) = 1;
	}

      fntype = build_type_variant (TREE_TYPE (function),
				   TREE_READONLY (function),
				   TREE_THIS_VOLATILE (function));
      function = build1 (ADDR_EXPR, build_pointer_type (fntype), function);
    }
  else
    {
      fndecl = NULL_TREE;

      /* Convert anything with function type to a pointer-to-function.  */
      if (function == error_mark_node)
	return error_mark_node;
      function = default_conversion (function);
    }

  fntype = TREE_TYPE (function);

  is_method = (TREE_CODE (fntype) == POINTER_TYPE
	       && TREE_CODE (TREE_TYPE (fntype)) == METHOD_TYPE);

  if (!(TREE_CODE (fntype) == POINTER_TYPE
	&& (TREE_CODE (TREE_TYPE (fntype)) == FUNCTION_TYPE || is_method)))
    {
      error ("called object is not a function");
      return error_mark_node;
    }

  /* fntype now gets the type of function pointed to.  */
  fntype = TREE_TYPE (fntype);

  /* Convert the parameters to the types declared in the
     function prototype, or apply default promotions.  */

  coerced_params = convert_arguments (NULL_TREE, TYPE_ARG_TYPES (fntype),
				      params, fndecl, LOOKUP_NORMAL);

  /* Recognize certain built-in functions so we can make tree-codes
     other than CALL_EXPR.  We do this when it enables fold-const.c
     to do something useful.  */

  if (TREE_CODE (function) == ADDR_EXPR
      && TREE_CODE (TREE_OPERAND (function, 0)) == FUNCTION_DECL
      && DECL_BUILT_IN (TREE_OPERAND (function, 0)))
    switch (DECL_FUNCTION_CODE (TREE_OPERAND (function, 0)))
      {
      case BUILT_IN_ABS:
      case BUILT_IN_LABS:
      case BUILT_IN_FABS:
	if (coerced_params == 0)
	  return integer_zero_node;
	return build_unary_op (ABS_EXPR, TREE_VALUE (coerced_params), 0);
      }

  /* C++ */
  value_type = TREE_TYPE (fntype) ? TREE_TYPE (fntype) : void_type_node;
  {
    register tree result = 
      build (CALL_EXPR, value_type,
	     function, coerced_params, NULL_TREE);

    TREE_SIDE_EFFECTS (result) = 1;
    TREE_RAISES (result) |= !! TYPE_RAISES_EXCEPTIONS (fntype);
    if (! require_complete)
      return result;
    if (value_type == void_type_node)
      return result;
    return require_complete_type (result);
  }
}

tree
build_function_call (function, params)
     tree function, params;
{
  return build_function_call_real (function, params, 1);
}
     
tree
build_function_call_maybe (function, params)
     tree function, params;
{
  return build_function_call_real (function, params, 0);
}


/* Convert the actual parameter expressions in the list VALUES
   to the types in the list TYPELIST.
   If parmdecls is exhausted, or when an element has NULL as its type,
   perform the default conversions.

   RETURN_LOC is the location of the return value, if known, NULL_TREE
   otherwise.  This is useful in the case where we can avoid creating
   a temporary variable in the case where we can initialize the return
   value directly.  If we are not eliding constructors, then we set this
   to NULL_TREE to avoid this avoidance.

   NAME is an IDENTIFIER_NODE or 0.  It is used only for error messages.

   This is also where warnings about wrong number of args are generated.
   
   Return a list of expressions for the parameters as converted.

   Both VALUES and the returned value are chains of TREE_LIST nodes
   with the elements of the list in the TREE_VALUE slots of those nodes.

   In C++, unspecified trailing parameters can be filled in with their
   default arguments, if such were specified.  Do so here.  */

tree
convert_arguments (return_loc, typelist, values, fndecl, flags)
     tree return_loc, typelist, values, fndecl;
     int flags;
{
  extern tree gc_protect_fndecl;
  register tree typetail, valtail;
  register tree result = NULL_TREE;
  char *called_thing;
  int maybe_raises = 0;
  int i = 0;

  if (! flag_elide_constructors)
    return_loc = 0;

  if (fndecl)
    {
      if (TREE_CODE (TREE_TYPE (fndecl)) == METHOD_TYPE)
	{
	  if (DECL_NAME (fndecl) == NULL_TREE
	      || IDENTIFIER_HAS_TYPE_VALUE (DECL_NAME (fndecl)))
	    called_thing = "constructor";
	  else
	    called_thing = "member function";
	  i -= 1;
	}
      else
	{
	  called_thing = "function";
	}
    }

  for (valtail = values, typetail = typelist;
       valtail;
       valtail = TREE_CHAIN (valtail), i++)
    {
      register tree type = typetail ? TREE_VALUE (typetail) : 0;
      register tree val = TREE_VALUE (valtail);

      if (type == void_type_node)
	{
	  if (fndecl)
	    {
	      char *buf = (char *)alloca (40 + strlen (called_thing));
	      sprintf (buf, "too many arguments to %s `%%s'", called_thing);
	      error_with_decl (fndecl, buf);
	      error ("at this point in file");
	    }
	  else
	    error ("too many arguments to function");
	  /* In case anybody wants to know if this argument
	     list is valid.  */
	  if (result)
	    TREE_TYPE (tree_last (result)) = error_mark_node;
	  break;
	}

      /* The tree type of the parameter being passed may not yet be
	 known.  In this case, its type is TYPE_UNKNOWN, and will
	 be instantiated by the type given by TYPE.  If TYPE
	 is also NULL, the tree type of VAL is ERROR_MARK_NODE.  */
      if (type && type_unknown_p (val))
	val = require_instantiated_type (type, val, integer_zero_node);
      else if (type_unknown_p (val))
	{
	  /* Strip the `&' from an overloaded FUNCTION_DECL.  */
	  if (TREE_CODE (val) == ADDR_EXPR)
	    val = TREE_OPERAND (val, 0);
	  if (TREE_CODE (val) == TREE_LIST
	      && TREE_CHAIN (val) == NULL_TREE
	      && TREE_TYPE (TREE_VALUE (val)) != NULL_TREE
	      && (TREE_TYPE (val) == unknown_type_node
		  || DECL_CHAIN (TREE_VALUE (val)) == NULL_TREE))
	    /* Instantiates automatically.  */
	    val = TREE_VALUE (val);
	  else
	    {
	      error ("insufficient type information in parameter list");
	      val = integer_zero_node;
	    }
	}
      else if (TREE_CODE (val) == OFFSET_REF)
	val = resolve_offset_ref (val);

      {
#if 0
	/* This code forces the assumption that if we have a ptr-to-func
	   type in an arglist, that every routine that wants to check
	   its validity has done so, and thus we need not do any
	   more conversion.  I don't remember why this is necessary.  */
	else if (TREE_CODE (ttype) == FUNCTION_TYPE
		 && (type == NULL
		     || TREE_CODE (TREE_TYPE (type)) == FUNCTION_TYPE
		     || TREE_CODE (TREE_TYPE (type)) == VOID_TYPE))
	  {
	    type = build_pointer_type (ttype);
	  }
#endif
      }

      /* build_c_cast puts on a NOP_EXPR to make the result not an lvalue.
	 Strip such NOP_EXPRs, since VAL is used in non-lvalue context.  */
      if (TREE_CODE (val) == NOP_EXPR
	  && TREE_TYPE (val) == TREE_TYPE (TREE_OPERAND (val, 0)))
	val = TREE_OPERAND (val, 0);

      if ((type == 0 || TREE_CODE (type) != REFERENCE_TYPE)
	  && (TREE_CODE (TREE_TYPE (val)) == ARRAY_TYPE
	      || TREE_CODE (TREE_TYPE (val)) == FUNCTION_TYPE
	      || TREE_CODE (TREE_TYPE (val)) == METHOD_TYPE))
	val = default_conversion (val);

      val = require_complete_type (val);

      if (val == error_mark_node)
	continue;

      maybe_raises |= TREE_RAISES (val);

      if (type != 0)
	{
	  /* Formal parm type is specified by a function prototype.  */
	  tree parmval;

	  if (TYPE_SIZE (type) == 0)
	    {
	      error ("parameter type of called function is incomplete");
	      parmval = val;
	    }
	  else
	    {
#ifdef PROMOTE_PROTOTYPES
	      /* Rather than truncating and then reextending,
		 convert directly to int, if that's the type we will want.  */
	      if (! flag_traditional
		  && TREE_CODE (type) == INTEGER_TYPE
		  && (TYPE_PRECISION (type) < TYPE_PRECISION (integer_type_node)))
		type = integer_type_node;
#endif
	      parmval = convert_for_initialization (return_loc, type, val, flags,
						    "argument passing", fndecl, i);
#ifdef PROMOTE_PROTOTYPES
	      if (TREE_CODE (type) == INTEGER_TYPE
		  && (TYPE_PRECISION (type) < TYPE_PRECISION (integer_type_node)))
		parmval = default_conversion (parmval);
#endif
	    }
	  result = tree_cons (NULL_TREE, parmval, result);
	}
      else
	{
	  if (TREE_CODE (TREE_TYPE (val)) == REFERENCE_TYPE)
	    val = convert_from_reference (val);

	  if (TREE_CODE (TREE_TYPE (val)) == REAL_TYPE
	      && (TYPE_PRECISION (TREE_TYPE (val))
		  < TYPE_PRECISION (double_type_node)))
	    /* Convert `float' to `double'.  */
	    result = tree_cons (NULL_TREE, convert (double_type_node, val), result);
	  else if (TYPE_LANG_SPECIFIC (TREE_TYPE (val))
		   && (TYPE_GETS_INIT_REF (TREE_TYPE (val))
		       || TYPE_GETS_ASSIGN_REF (TREE_TYPE (val))))
	    {
	      if (pedantic)
		pedwarn ("ANSI C++ forbids passing objects of type `%s' through `...'",
			 TYPE_NAME_STRING (TREE_TYPE (val)));
	      else
		warning ("cannot pass objects of type `%s' through `...'",
			 TYPE_NAME_STRING (TREE_TYPE (val)));
	      result = tree_cons (NULL_TREE, val, result);
	    }
	  else
	    /* Convert `short' and `char' to full-size `int'.  */
	    result = tree_cons (NULL_TREE, default_conversion (val), result);
	}

      if (flag_gc
	  /* There are certain functions for which we don't need
	     to protect our arguments.  GC_PROTECT_FNDECL is one.  */
	  && fndecl != gc_protect_fndecl
	  && type_needs_gc_entry (TREE_TYPE (TREE_VALUE (result)))
	  && ! value_safe_from_gc (NULL_TREE, TREE_VALUE (result)))
	/* This will build a temporary variable whose cleanup is
	   to clear the obstack entry.  */
	TREE_VALUE (result) = protect_value_from_gc (NULL_TREE,
						     TREE_VALUE (result));

      if (typetail)
	typetail = TREE_CHAIN (typetail);
    }

  if (typetail != 0 && typetail != void_list_node)
    {
      /* See if there are default arguments that can be used */
      if (TREE_PURPOSE (typetail))
	{
	  while (typetail != void_list_node)
	    {
	      tree type = TREE_VALUE (typetail);
	      tree val = TREE_PURPOSE (typetail);
	      tree parmval;

	      if (val == NULL_TREE)
		parmval = error_mark_node;
	      else if (TREE_CODE (val) == CONSTRUCTOR)
		{
		  parmval = digest_init (type, val, (tree *)0);
		  parmval = convert_for_initialization (return_loc, type, parmval, flags,
							"default constructor", fndecl, i);
		}
	      else
		{
		  /* This could get clobbered by the following call.  */
		  if (TREE_HAS_CONSTRUCTOR (val))
		    val = copy_node (val);

		  parmval = convert_for_initialization (return_loc, type, val, flags,
							"default argument", fndecl, i);
#ifdef PROMOTE_PROTOTYPES
		  if (TREE_CODE (type) == INTEGER_TYPE
		      && (TYPE_PRECISION (type) < TYPE_PRECISION (integer_type_node)))
		    parmval = default_conversion (parmval);
#endif
		}
	      maybe_raises |= TREE_RAISES (parmval);

	      if (flag_gc
		  && type_needs_gc_entry (TREE_TYPE (parmval))
		  && ! value_safe_from_gc (NULL_TREE, parmval))
		parmval = protect_value_from_gc (NULL_TREE, parmval);

	      result = tree_cons (0, parmval, result);
	      typetail = TREE_CHAIN (typetail);
	      /* ends with `...'.  */
	      if (typetail == NULL_TREE)
		break;
	    }
	}
      else
	{
	  if (fndecl)
	    {
	      char *buf = (char *)alloca (32 + strlen (called_thing));
	      sprintf (buf, "too few arguments to %s `%%s'", called_thing);
	      error_with_decl (fndecl, buf);
	      error ("at this point in file");
	    }
	  else
	    error ("too few arguments to function");
	  return error_mark_list;
	}
    }
  if (result)
    TREE_RAISES (result) = maybe_raises;

  return nreverse (result);
}

/* Build a binary-operation expression, after performing default
   conversions on the operands.  CODE is the kind of expression to build.  */

tree
build_x_binary_op (code, arg1, arg2)
     enum tree_code code;
     tree arg1, arg2;
{
  tree rval;

  /* If there's any way we can call this function, do so.  */
  if (rval = build_opfncall (code, 0, arg1, arg2, NULL_TREE))
    {
      /* If it's accessible, we win.  */
      if (rval = build_opfncall (code, LOOKUP_PROTECT, arg1, arg2, NULL_TREE))
	return rval;
      /* Else, report an error.  */
      build_opfncall (code, LOOKUP_NORMAL, arg1, arg2, NULL_TREE);
      return error_mark_node;
    }
  if (code == MEMBER_REF)
    return build_m_component_ref (build_indirect_ref (arg1, NULL_PTR),
				  build_indirect_ref (arg2, NULL_PTR));
  return build_binary_op (code, arg1, arg2, 1);
}

tree
build_binary_op (code, arg1, arg2, convert_p)
     enum tree_code code;
     tree arg1, arg2;
     int convert_p;
{
  tree type1, type2;
  tree args[2];

  args[0] = arg1;
  args[1] = arg2;

  if (convert_p)
    {
      args[0] = default_conversion (args[0]);
      args[1] = default_conversion (args[1]);

      if (type_unknown_p (args[0]))
	{
	  args[0] = instantiate_type (TREE_TYPE (args[1]), args[0], 1);
	  args[0] = default_conversion (args[0]);
	}
      else if (type_unknown_p (args[1]))
	{
	  args[1] = require_instantiated_type (TREE_TYPE (args[0]),
					       args[1],
					       error_mark_node);
	  args[1] = default_conversion (args[1]);
	}

      type1 = TREE_TYPE (args[0]);
      type2 = TREE_TYPE (args[1]);

      if (IS_AGGR_TYPE_2 (type1, type2))
	{
	  /* Try to convert this to something reasonable.  */
	  if (! build_default_binary_type_conversion(code, &args[0], &args[1]))
	    return error_mark_node;
	}
      else if (IS_AGGR_TYPE (type1) || IS_AGGR_TYPE (type2))
	{
	  int convert_index = IS_AGGR_TYPE (type2);
	  /* Avoid being tripped up by things like (ARG1 != 0).  */
	  tree types[2], try;
	  
	  types[0] = type1; types[1] = type2;
	  try = build_type_conversion (code, types[convert_index ^ 1],
				       args[convert_index], 1);
	  
	  if (try == 0
	      && args[1] == integer_zero_node
	      && (code == NE_EXPR || code == EQ_EXPR))
	    try = build_type_conversion (code, ptr_type_node,
					 args[convert_index], 1);
	  if (try == 0)
	    {
	      error_with_aggr_type (types[convert_index],
				    "no member function `%s::operator %s'",
				    opname_tab[(int) code]
				    ? opname_tab[(int) code]
				    : "<unknown>");
	      return error_mark_node;
	    }
	  if (try == error_mark_node)
	    error ("ambiguous pointer conversion");
	  args[convert_index] = try;
	}
    }
  return build_binary_op_nodefault (code, args[0], args[1], code);
}

/* Build a binary-operation expression without default conversions.
   CODE is the kind of expression to build.
   This function differs from `build' in several ways:
   the data type of the result is computed and recorded in it,
   warnings are generated if arg data types are invalid,
   special handling for addition and subtraction of pointers is known,
   and some optimization is done (operations on narrow ints
   are done in the narrower type when that gives the same result).
   Constant folding is also done before the result is returned.

   ERROR_CODE is the code that determines what to say in error messages.
   It is usually, but not always, the same as CODE.

   Note that the operands will never have enumeral types
   because either they have just had the default conversions performed
   or they have both just been converted to some other type in which
   the arithmetic is to be done.

   C++: must do special pointer arithmetic when implementing
   multiple inheritance.  */

tree
build_binary_op_nodefault (code, op0, op1, error_code)
     enum tree_code code;
     tree op0, op1;
     enum tree_code error_code;
{
  tree type0 = TREE_TYPE (op0), type1 = TREE_TYPE (op1);

  /* The expression codes of the data types of the arguments tell us
     whether the arguments are integers, floating, pointers, etc.  */
  register enum tree_code code0 = TREE_CODE (type0);
  register enum tree_code code1 = TREE_CODE (type1);

  /* Expression code to give to the expression when it is built.
     Normally this is CODE, which is what the caller asked for,
     but in some special cases we change it.  */
  register enum tree_code resultcode = code;

  /* Data type in which the computation is to be performed.
     In the simplest cases this is the common type of the arguments.  */
  register tree result_type = NULL;

  /* Nonzero means operands have already been type-converted
     in whatever way is necessary.
     Zero means they need to be converted to RESULT_TYPE.  */
  int converted = 0;

  /* Nonzero means after finally constructing the expression
     give it this type.  Otherwise, give it type RESULT_TYPE.  */
  tree final_type = 0;

  /* Nonzero if this is an operation like MIN or MAX which can
     safely be computed in short if both args are promoted shorts.
     Also implies COMMON.
     -1 indicates a bitwise operation; this makes a difference
     in the exact conditions for when it is safe to do the operation
     in a narrower mode.  */
  int shorten = 0;

  /* Nonzero if this is a comparison operation;
     if both args are promoted shorts, compare the original shorts.
     Also implies COMMON.  */
  int short_compare = 0;

  /* Nonzero if this is a right-shift operation, which can be computed on the
     original short and then promoted if the operand is a promoted short.  */
  int short_shift = 0;

  /* Nonzero means set RESULT_TYPE to the common type of the args.  */
  int common = 0;

  /* Strip NON_LVALUE_EXPRs, etc., since we aren't using as an lvalue.  */
  STRIP_TYPE_NOPS (op0);
  STRIP_TYPE_NOPS (op1);

  /* If an error was already reported for one of the arguments,
     avoid reporting another error.  */

  if (code0 == ERROR_MARK || code1 == ERROR_MARK)
    return error_mark_node;

  switch (code)
    {
    case PLUS_EXPR:
      /* Handle the pointer + int case.  */
      if (code0 == POINTER_TYPE && code1 == INTEGER_TYPE)
	return pointer_int_sum (PLUS_EXPR, op0, op1);
      else if (code1 == POINTER_TYPE && code0 == INTEGER_TYPE)
	return pointer_int_sum (PLUS_EXPR, op1, op0);
      else
	common = 1;
      break;

    case MINUS_EXPR:
      /* Subtraction of two similar pointers.
	 We must subtract them as integers, then divide by object size.  */
      if (code0 == POINTER_TYPE && code1 == POINTER_TYPE
	  && comp_target_types (type0, type1, 1))
	return pointer_diff (op0, op1);
      /* Handle pointer minus int.  Just like pointer plus int.  */
      else if (code0 == POINTER_TYPE && code1 == INTEGER_TYPE)
	return pointer_int_sum (MINUS_EXPR, op0, op1);
      else
	common = 1;
      break;

    case MULT_EXPR:
      common = 1;
      break;

    case TRUNC_DIV_EXPR:
    case CEIL_DIV_EXPR:
    case FLOOR_DIV_EXPR:
    case ROUND_DIV_EXPR:
    case EXACT_DIV_EXPR:
      if ((code0 == INTEGER_TYPE || code0 == REAL_TYPE)
	  && (code1 == INTEGER_TYPE || code1 == REAL_TYPE))
	{
	  if (!(code0 == INTEGER_TYPE && code1 == INTEGER_TYPE))
	    resultcode = RDIV_EXPR;
	  else
	    shorten = 1;
	  common = 1;
	}
      break;

    case BIT_AND_EXPR:
    case BIT_ANDTC_EXPR:
    case BIT_IOR_EXPR:
    case BIT_XOR_EXPR:
      if (code0 == INTEGER_TYPE && code1 == INTEGER_TYPE)
	shorten = -1;
      /* If one operand is a constant, and the other is a short type
	 that has been converted to an int,
	 really do the work in the short type and then convert the
	 result to int.  If we are lucky, the constant will be 0 or 1
	 in the short type, making the entire operation go away.  */
      if (TREE_CODE (op0) == INTEGER_CST
	  && TREE_CODE (op1) == NOP_EXPR
	  && TYPE_PRECISION (type1) > TYPE_PRECISION (TREE_TYPE (TREE_OPERAND (op1, 0)))
	  && TREE_UNSIGNED (TREE_TYPE (TREE_OPERAND (op1, 0))))
	{
	  final_type = result_type;
	  op1 = TREE_OPERAND (op1, 0);
	  result_type = TREE_TYPE (op1);
	}
      if (TREE_CODE (op1) == INTEGER_CST
	  && TREE_CODE (op0) == NOP_EXPR
	  && TYPE_PRECISION (type0) > TYPE_PRECISION (TREE_TYPE (TREE_OPERAND (op0, 0)))
	  && TREE_UNSIGNED (TREE_TYPE (TREE_OPERAND (op0, 0))))
	{
	  final_type = result_type;
	  op0 = TREE_OPERAND (op0, 0);
	  result_type = TREE_TYPE (op0);
	}
      break;

    case TRUNC_MOD_EXPR:
    case FLOOR_MOD_EXPR:
      if (code0 == INTEGER_TYPE && code1 == INTEGER_TYPE)
	shorten = 1;
      break;

    case TRUTH_ANDIF_EXPR:
    case TRUTH_ORIF_EXPR:
    case TRUTH_AND_EXPR:
    case TRUTH_OR_EXPR:
      if ((code0 == INTEGER_TYPE || code0 == POINTER_TYPE || code0 == REAL_TYPE)
	  && (code1 == INTEGER_TYPE || code1 == POINTER_TYPE || code1 == REAL_TYPE))
	{
	  /* Result of these operations is always an int,
	     but that does not mean the operands should be
	     converted to ints!  */
	  result_type = integer_type_node;
	  op0 = truthvalue_conversion (op0);
	  op1 = truthvalue_conversion (op1);
	  converted = 1;
	}
      break;

      /* Shift operations: result has same type as first operand;
	 always convert second operand to int.
	 Also set SHORT_SHIFT if shifting rightward.  */

    case RSHIFT_EXPR:
      if (code0 == INTEGER_TYPE && code1 == INTEGER_TYPE)
	{
	  result_type = type0;
	  if (TREE_CODE (op1) == INTEGER_CST)
	    {
	      if (tree_int_cst_lt (op1, integer_zero_node))
		warning ("shift count is negative");
	      else
		{
		  if (TREE_INT_CST_LOW (op1) | TREE_INT_CST_HIGH (op1))
		    short_shift = 1;
		  if (TREE_INT_CST_HIGH (op1) != 0
		      || ((unsigned HOST_WIDE_INT) TREE_INT_CST_LOW (op1)
			  >= TYPE_PRECISION (type0)))
		    warning ("shift count >= width of type");
		}
	    }
	  /* Convert the shift-count to an integer, regardless of
	     size of value being shifted.  */
	  if (TYPE_MAIN_VARIANT (TREE_TYPE (op1)) != integer_type_node)
	    op1 = convert (integer_type_node, op1);
	  /* Avoid converting op1 to result_type later.  */
	  converted = 1;
	}
      break;

    case LSHIFT_EXPR:
      if (code0 == INTEGER_TYPE && code1 == INTEGER_TYPE)
	{
	  result_type = type0;
	  if (TREE_CODE (op1) == INTEGER_CST)
	    {
	      if (tree_int_cst_lt (op1, integer_zero_node))
		warning ("shift count is negative");
	      else if (TREE_INT_CST_HIGH (op1) != 0
		       || ((unsigned HOST_WIDE_INT) TREE_INT_CST_LOW (op1)
			   >= TYPE_PRECISION (type0)))
		warning ("shift count >= width of type");
	    }
	  /* Convert the shift-count to an integer, regardless of
	     size of value being shifted.  */
	  if (TYPE_MAIN_VARIANT (TREE_TYPE (op1)) != integer_type_node)
	    op1 = convert (integer_type_node, op1);
	  /* Avoid converting op1 to result_type later.  */
	  converted = 1;
	}
      break;

    case RROTATE_EXPR:
    case LROTATE_EXPR:
      if (code0 == INTEGER_TYPE && code1 == INTEGER_TYPE)
	{
	  result_type = type0;
	  if (TREE_CODE (op1) == INTEGER_CST)
	    {
	      if (tree_int_cst_lt (op1, integer_zero_node))
		warning ("shift count is negative");
	      else if (TREE_INT_CST_HIGH (op1) != 0
		       || ((unsigned HOST_WIDE_INT) TREE_INT_CST_LOW (op1)
			   >= TYPE_PRECISION (type0)))
		warning ("shift count >= width of type");
	    }
	  /* Convert the shift-count to an integer, regardless of
	     size of value being shifted.  */
	  if (TYPE_MAIN_VARIANT (TREE_TYPE (op1)) != integer_type_node)
	    op1 = convert (integer_type_node, op1);
	}
      break;

    case EQ_EXPR:
    case NE_EXPR:
      /* Result of comparison is always int,
	 but don't convert the args to int!  */
      result_type = integer_type_node;
      converted = 1;
      if ((code0 == INTEGER_TYPE || code0 == REAL_TYPE)
	  && (code1 == INTEGER_TYPE || code1 == REAL_TYPE))
	short_compare = 1;
      else if (code0 == POINTER_TYPE && code1 == POINTER_TYPE)
	{
	  register tree tt0 = TYPE_MAIN_VARIANT (TREE_TYPE (type0));
	  register tree tt1 = TYPE_MAIN_VARIANT (TREE_TYPE (type1));
	  /* Anything compares with void *.  void * compares with anything.
	     Otherwise, the targets must be the same.  */
	  if (tt0 != tt1 && TREE_CODE (tt0) == RECORD_TYPE
	      && TREE_CODE (tt1) == RECORD_TYPE)
	    {
	      tree base = common_base_type (tt0, tt1);
	      if (base == NULL_TREE)
		warning ("comparison of distinct object pointer types");
	      else if (base == error_mark_node)
		{
		  message_2_types (error, "comparison of pointer types `%s*' and `%s*' requires conversion to ambiguous supertype", tt0, tt1);
		  return error_mark_node;
		}
	      else
		{
		  if (integer_zerop (op0))
		    op0 = null_pointer_node;
		  else
		    op0 = convert_pointer_to (base, op0);
		  if (integer_zerop (op1))
		    op1 = null_pointer_node;
		  else
		    op1 = convert_pointer_to (base, op1);
		}
	    }
	  else if (comp_target_types (type0, type1, 1))
	    ;
	  else if (tt0 == void_type_node)
	    {
	      if (pedantic && TREE_CODE (tt1) == FUNCTION_TYPE)
		pedwarn ("ANSI C++ forbids comparison of `void *' with function pointer");
	    }
	  else if (tt1 == void_type_node)
	    {
	      if (pedantic && TREE_CODE (tt0) == FUNCTION_TYPE)
		pedwarn ("ANSI C++ forbids comparison of `void *' with function pointer");
	    }
	  else
	    warning ("comparison of distinct pointer types lacks a cast");
	}
      else if (code0 == POINTER_TYPE && TREE_CODE (op1) == INTEGER_CST
	       && integer_zerop (op1))
	op1 = null_pointer_node;
      else if (code1 == POINTER_TYPE && TREE_CODE (op0) == INTEGER_CST
	       && integer_zerop (op0))
	op0 = null_pointer_node;
      else if (code0 == POINTER_TYPE && code1 == INTEGER_TYPE)
	{
	  error ("ANSI C++ forbids comparison between pointer and integer");
	  op1 = convert (TREE_TYPE (op0), op1);
	}
      else if (code0 == INTEGER_TYPE && code1 == POINTER_TYPE)
	{
	  error ("ANSI C++ forbids comparison between pointer and integer");
	  op0 = convert (TREE_TYPE (op1), op0);
	}
      else
	/* If args are not valid, clear out RESULT_TYPE
	   to cause an error message later.  */
	result_type = 0;
      break;

    case MAX_EXPR:
    case MIN_EXPR:
      if ((code0 == INTEGER_TYPE || code0 == REAL_TYPE)
	   && (code1 == INTEGER_TYPE || code1 == REAL_TYPE))
	shorten = 1;
      else if (code0 == POINTER_TYPE && code1 == POINTER_TYPE)
	{
	  if (! comp_target_types (type0, type1, 1))
	    warning ("comparison of distinct pointer types lacks a cast");
	  else if (pedantic
		   && TREE_CODE (TREE_TYPE (type0)) == FUNCTION_TYPE)
	    pedwarn ("ANSI C++ forbids ordered comparisons of pointers to functions");
	  result_type = common_type (type0, type1);
	}
      break;

    case LE_EXPR:
    case GE_EXPR:
    case LT_EXPR:
    case GT_EXPR:
      if ((code0 == INTEGER_TYPE || code0 == REAL_TYPE)
	   && (code1 == INTEGER_TYPE || code1 == REAL_TYPE))
	short_compare = 1;
      else if (code0 == POINTER_TYPE && code1 == POINTER_TYPE)
	{
	  if (! comp_target_types (type0, type1, 1))
	    warning ("comparison of distinct pointer types lacks a cast");
	  else if (pedantic 
		   && TREE_CODE (TREE_TYPE (type0)) == FUNCTION_TYPE)
	    pedwarn ("ANSI C++ forbids ordered comparisons of pointers to functions");
	  result_type = integer_type_node;
	}
      else if (code0 == POINTER_TYPE && TREE_CODE (op1) == INTEGER_CST
	       && integer_zerop (op1))
	{
	  result_type = integer_type_node;
	  op1 = null_pointer_node;
	  if (! flag_traditional)
	    warning ("ordered comparison of pointer with integer zero");
	}
      else if (code1 == POINTER_TYPE && TREE_CODE (op0) == INTEGER_CST
	       && integer_zerop (op0))
	{
	  result_type = integer_type_node;
	  op0 = null_pointer_node;
	  if (pedantic)
	    pedwarn ("ANSI C++ forbids ordered comparison of pointer with integer zero");
	}
      else if (code0 == POINTER_TYPE && code1 == INTEGER_TYPE)
	{
	  result_type = integer_type_node;
	  if (pedantic)
	    pedwarn ("ANSI C++ forbids comparison between pointer and integer");
	  else if (! flag_traditional)
	    warning ("comparison between pointer and integer");
	  op1 = convert (TREE_TYPE (op0), op1);
	}
      else if (code0 == INTEGER_TYPE && code1 == POINTER_TYPE)
	{
	  result_type = integer_type_node;
	  if (pedantic)
	    pedwarn ("ANSI C++ forbids comparison between pointer and integer");
	  else if (! flag_traditional)
	    warning ("comparison between pointer and integer");
	  op0 = convert (TREE_TYPE (op1), op0);
	}
      converted = 1;
      break;
    }

  if ((code0 == INTEGER_TYPE || code0 == REAL_TYPE)
      && (code1 == INTEGER_TYPE || code1 == REAL_TYPE))
    {
      if (shorten || common || short_compare)
	result_type = common_type (type0, type1);

      /* For certain operations (which identify themselves by shorten != 0)
	 if both args were extended from the same smaller type,
	 do the arithmetic in that type and then extend.

	 shorten !=0 and !=1 indicates a bitwise operation.
	 For them, this optimization is safe only if
	 both args are zero-extended or both are sign-extended.
	 Otherwise, we might change the result.
	 Eg, (short)-1 | (unsigned short)-1 is (int)-1
	 but calculated in (unsigned short) it would be (unsigned short)-1.  */

      if (shorten)
	{
	  int unsigned0, unsigned1;
	  tree arg0 = get_narrower (op0, &unsigned0);
	  tree arg1 = get_narrower (op1, &unsigned1);
	  /* UNS is 1 if the operation to be done is an unsigned one.  */
	  int uns = TREE_UNSIGNED (result_type);
	  tree type;

	  final_type = result_type;

	  /* Handle the case that OP0 does not *contain* a conversion
	     but it *requires* conversion to FINAL_TYPE.  */

	  if (op0 == arg0 && TREE_TYPE (op0) != final_type)
	    unsigned0 = TREE_UNSIGNED (TREE_TYPE (op0));
	  if (op1 == arg1 && TREE_TYPE (op1) != final_type)
	    unsigned1 = TREE_UNSIGNED (TREE_TYPE (op1));

	  /* Now UNSIGNED0 is 1 if ARG0 zero-extends to FINAL_TYPE.  */

	  /* For bitwise operations, signedness of nominal type
	     does not matter.  Consider only how operands were extended.  */
	  if (shorten == -1)
	    uns = unsigned0;

	  /* Note that in all three cases below we refrain from optimizing
	     an unsigned operation on sign-extended args.
	     That would not be valid.  */

	  /* Both args variable: if both extended in same way
	     from same width, do it in that width.
	     Do it unsigned if args were zero-extended.  */
	  if ((TYPE_PRECISION (TREE_TYPE (arg0))
	       < TYPE_PRECISION (result_type))
	      && (TYPE_PRECISION (TREE_TYPE (arg1))
		  == TYPE_PRECISION (TREE_TYPE (arg0)))
	      && unsigned0 == unsigned1
	      && (unsigned0 || !uns))
	    result_type
	      = signed_or_unsigned_type (unsigned0,
					 common_type (TREE_TYPE (arg0), TREE_TYPE (arg1)));
	  else if (TREE_CODE (arg0) == INTEGER_CST
		   && (unsigned1 || !uns)
		   && (TYPE_PRECISION (TREE_TYPE (arg1))
		       < TYPE_PRECISION (result_type))
		   && (type = signed_or_unsigned_type (unsigned1,
						       TREE_TYPE (arg1)),
		       int_fits_type_p (arg0, type)))
	    result_type = type;
	  else if (TREE_CODE (arg1) == INTEGER_CST
		   && (unsigned0 || !uns)
		   && (TYPE_PRECISION (TREE_TYPE (arg0))
		       < TYPE_PRECISION (result_type))
		   && (type = signed_or_unsigned_type (unsigned0,
						       TREE_TYPE (arg0)),
		       int_fits_type_p (arg1, type)))
	    result_type = type;
	}

      /* Shifts can be shortened if shifting right.  */

      if (short_shift)
	{
	  int unsigned_arg;
	  tree arg0 = get_narrower (op0, &unsigned_arg);

	  final_type = result_type;

	  if (arg0 == op0 && final_type == TREE_TYPE (op0))
	    unsigned_arg = TREE_UNSIGNED (TREE_TYPE (op0));

	  if (TYPE_PRECISION (TREE_TYPE (arg0)) < TYPE_PRECISION (result_type)
	      /* If arg is sign-extended and then unsigned-shifted,
		 we can simulate this with a signed shift in arg's type
		 only if the extended result is at least twice as wide
		 as the arg.  Otherwise, the shift could use up all the
		 ones made by sign-extension and bring in zeros.
		 We can't optimize that case at all, but in most machines
		 it never happens because available widths are 2**N.  */
	      && (!TREE_UNSIGNED (final_type)
		  || unsigned_arg
		  || ((unsigned) 2 * TYPE_PRECISION (TREE_TYPE (arg0))
		      <= TYPE_PRECISION (result_type))))
	    {
	      /* Do an unsigned shift if the operand was zero-extended.  */
	      result_type
		= signed_or_unsigned_type (unsigned_arg,
					   TREE_TYPE (arg0));
	      /* Convert value-to-be-shifted to that type.  */
	      if (TREE_TYPE (op0) != result_type)
		op0 = convert (result_type, op0);
	      converted = 1;
	    }
	}

      /* Comparison operations are shortened too but differently.
	 They identify themselves by setting short_compare = 1.  */

      if (short_compare)
	{
	  /* Don't write &op0, etc., because that would prevent op0
	     from being kept in a register.
	     Instead, make copies of the our local variables and
	     pass the copies by reference, then copy them back afterward.  */
	  tree xop0 = op0, xop1 = op1, xresult_type = result_type;
	  enum tree_code xresultcode = resultcode;
	  tree val 
	    = shorten_compare (&xop0, &xop1, &xresult_type, &xresultcode);
	  if (val != 0)
	    return val;
	  op0 = xop0, op1 = xop1, result_type = xresult_type;
	  resultcode = xresultcode;
	}

      if (short_compare && extra_warnings)
	{
	  int unsignedp0, unsignedp1;
	  tree primop0 = get_narrower (op0, &unsignedp0);
	  tree primop1 = get_narrower (op1, &unsignedp1);

	  /* Warn if signed and unsigned are being compared in a size larger
	     than their original size, as this will always fail.  */

	  if (unsignedp0 != unsignedp1
	      && (TYPE_PRECISION (TREE_TYPE (primop0))
		  < TYPE_PRECISION (result_type))
	      && (TYPE_PRECISION (TREE_TYPE (primop1))
		  < TYPE_PRECISION (result_type)))
	    warning ("comparison between promoted unsigned and signed");

	  /* Warn if two unsigned values are being compared in a size
	     larger than their original size, and one (and only one) is the
	     result of a `~' operator.  This comparison will always fail.

	     Also warn if one operand is a constant, and the constant does not
	     have all bits set that are set in the ~ operand when it is
	     extended.  */

	  else if (TREE_CODE (primop0) == BIT_NOT_EXPR
		   ^ TREE_CODE (primop1) == BIT_NOT_EXPR)
	    {
	      if (TREE_CODE (primop0) == BIT_NOT_EXPR)
		primop0 = get_narrower (TREE_OPERAND (op0, 0), &unsignedp0);
	      if (TREE_CODE (primop1) == BIT_NOT_EXPR)
		primop1 = get_narrower (TREE_OPERAND (op1, 0), &unsignedp1);
	      
	      if (TREE_CODE (primop0) == INTEGER_CST
		  || TREE_CODE (primop1) == INTEGER_CST)
		{
		  tree primop;
		  HOST_WIDE_INT constant, mask;
		  int unsignedp;
		  unsigned bits;

		  if (TREE_CODE (primop0) == INTEGER_CST)
		    {
		      primop = primop1;
		      unsignedp = unsignedp1;
		      constant = TREE_INT_CST_LOW (primop0);
		    }
		  else
		    {
		      primop = primop0;
		      unsignedp = unsignedp0;
		      constant = TREE_INT_CST_LOW (primop1);
		    }

		  bits = TYPE_PRECISION (TREE_TYPE (primop));
		  if (bits < TYPE_PRECISION (result_type)
		      && bits < HOST_BITS_PER_LONG && unsignedp)
		    {
		      mask = (~ (HOST_WIDE_INT) 0) << bits;
		      if ((mask & constant) != mask)
			warning ("comparison of promoted ~unsigned with constant");
		    }
		}
	      else if (unsignedp0 && unsignedp1
		       && (TYPE_PRECISION (TREE_TYPE (primop0))
			   < TYPE_PRECISION (result_type))
		       && (TYPE_PRECISION (TREE_TYPE (primop1))
			   < TYPE_PRECISION (result_type)))
		warning ("comparison of promoted ~unsigned with unsigned");
	    }
	}
    }

  /* At this point, RESULT_TYPE must be nonzero to avoid an error message.
     If CONVERTED is zero, both args will be converted to type RESULT_TYPE.
     Then the expression will be built.
     It will be given type FINAL_TYPE if that is nonzero;
     otherwise, it will be given type RESULT_TYPE.  */

  if (!result_type)
    {
      binary_op_error (error_code);
      return error_mark_node;
    }

  if (! converted)
    {
      if (TREE_TYPE (op0) != result_type)
	op0 = convert (result_type, op0); 
      if (TREE_TYPE (op1) != result_type)
	op1 = convert (result_type, op1); 
    }

  {
    register tree result = build (resultcode, result_type, op0, op1);
    register tree folded;

    folded = fold (result);
    if (folded == result)
      TREE_CONSTANT (folded) = TREE_CONSTANT (op0) & TREE_CONSTANT (op1);
    if (final_type != 0)
      return convert (final_type, folded);
    return folded;
  }
}

/* Return a tree for the sum or difference (RESULTCODE says which)
   of pointer PTROP and integer INTOP.  */

static tree
pointer_int_sum (resultcode, ptrop, intop)
     enum tree_code resultcode;
     register tree ptrop, intop;
{
  tree size_exp;

  register tree result;
  register tree folded = fold (intop);

  /* The result is a pointer of the same type that is being added.  */

  register tree result_type = TREE_TYPE (ptrop);

  /* Needed to make OOPS V2R3 work.  */
  intop = folded;
  if (TREE_CODE (intop) == INTEGER_CST
      && TREE_INT_CST_LOW (intop) == 0
      && TREE_INT_CST_HIGH (intop) == 0)
    return ptrop;

  if (TREE_CODE (TREE_TYPE (result_type)) == VOID_TYPE)
    {
      if (pedantic || warn_pointer_arith)
	pedwarn ("ANSI C++ forbids using pointer of type `void *' in arithmetic");
      size_exp = integer_one_node;
    }
  else if (TREE_CODE (TREE_TYPE (result_type)) == FUNCTION_TYPE)
    {
      if (pedantic || warn_pointer_arith)
	pedwarn ("ANSI C++ forbids using pointer to a function in arithmetic");
      size_exp = integer_one_node;
    }
  else if (TREE_CODE (TREE_TYPE (result_type)) == METHOD_TYPE)
    {
      if (pedantic || warn_pointer_arith)
	pedwarn ("ANSI C++ forbids using pointer to a method in arithmetic");
      size_exp = integer_one_node;
    }
  else if (TREE_CODE (TREE_TYPE (result_type)) == OFFSET_TYPE)
    {
      if (pedantic)
	pedwarn ("ANSI C++ forbids using pointer to a member in arithmetic");
      size_exp = integer_one_node;
    }
  else
    size_exp = size_in_bytes (TREE_TYPE (result_type));

  /* If what we are about to multiply by the size of the elements
     contains a constant term, apply distributive law
     and multiply that constant term separately.
     This helps produce common subexpressions.  */

  if ((TREE_CODE (intop) == PLUS_EXPR || TREE_CODE (intop) == MINUS_EXPR)
      && ! TREE_CONSTANT (intop)
      && TREE_CONSTANT (TREE_OPERAND (intop, 1))
      && TREE_CONSTANT (size_exp))
    {
      enum tree_code subcode = resultcode;
      if (TREE_CODE (intop) == MINUS_EXPR)
	subcode = (subcode == PLUS_EXPR ? MINUS_EXPR : PLUS_EXPR);
      ptrop = build_binary_op (subcode, ptrop, TREE_OPERAND (intop, 1), 1);
      intop = TREE_OPERAND (intop, 0);
    }

  /* Convert the integer argument to a type the same size as a pointer
     so the multiply won't overflow spuriously.  */

  if (TYPE_PRECISION (TREE_TYPE (intop)) != POINTER_SIZE)
    intop = convert (type_for_size (POINTER_SIZE, 0), intop);

  /* Replace the integer argument
     with a suitable product by the object size.  */

  intop = build_binary_op (MULT_EXPR, intop, size_exp, 1);

  /* Create the sum or difference.  */

  result = build (resultcode, result_type, ptrop, intop);

  folded = fold (result);
  if (folded == result)
    TREE_CONSTANT (folded) = TREE_CONSTANT (ptrop) & TREE_CONSTANT (intop);
  return folded;
}

/* Return a tree for the difference of pointers OP0 and OP1.
   The resulting tree has type int.  */

static tree
pointer_diff (op0, op1)
     register tree op0, op1;
{
  register tree result, folded;
  tree restype = ptrdiff_type_node;
  tree target_type = TREE_TYPE (TREE_TYPE (op0));

  if (pedantic)
    {
      if (TREE_CODE (target_type) == VOID_TYPE)
	pedwarn ("ANSI C++ forbids using pointer of type `void *' in subtraction");
      if (TREE_CODE (target_type) == FUNCTION_TYPE)
	pedwarn ("ANSI C++ forbids using pointer to a function in subtraction");
      if (TREE_CODE (target_type) == METHOD_TYPE)
	pedwarn ("ANSI C++ forbids using pointer to a method in subtraction");
      if (TREE_CODE (target_type) == OFFSET_TYPE)
	pedwarn ("ANSI C++ forbids using pointer to a member in subtraction");
    }

  /* First do the subtraction as integers;
     then drop through to build the divide operator.  */

  op0 = build_binary_op (MINUS_EXPR,
			 convert (restype, op0), convert (restype, op1), 1);
  op1 = ((TREE_CODE (target_type) == VOID_TYPE
	  || TREE_CODE (target_type) == FUNCTION_TYPE
	  || TREE_CODE (target_type) == METHOD_TYPE
	  || TREE_CODE (target_type) == OFFSET_TYPE)
	 ? integer_one_node
	 : size_in_bytes (target_type));

  /* Do the division.  */

  result = build (EXACT_DIV_EXPR, restype, op0, op1);

  folded = fold (result);
  if (folded == result)
    TREE_CONSTANT (folded) = TREE_CONSTANT (op0) & TREE_CONSTANT (op1);
  return folded;
}

/* Handle the case of taking the address of a COMPONENT_REF.
   Called by `build_unary_op' and `build_up_reference'.

   ARG is the COMPONENT_REF whose address we want.
   ARGTYPE is the pointer type that this address should have.
   MSG is an error message to print if this COMPONENT_REF is not
   addressable (such as a bitfield).  */

tree
build_component_addr (arg, argtype, msg)
     tree arg, argtype;
     char *msg;
{
  tree field = TREE_OPERAND (arg, 1);
  tree basetype = decl_type_context (field);
  tree rval = build_unary_op (ADDR_EXPR, TREE_OPERAND (arg, 0), 0);

  if (DECL_BIT_FIELD (field))
    {
      error (msg, IDENTIFIER_POINTER (DECL_NAME (field)));
      return error_mark_node;
    }

  if (flag_gc)
    warning ("address of `%s::%s' taken", TYPE_NAME_STRING (basetype),
	     IDENTIFIER_POINTER (DECL_NAME (field)));

  if (TREE_CODE (field) == FIELD_DECL
      && TYPE_USES_COMPLEX_INHERITANCE (basetype))
    /* Can't convert directly to ARGTYPE, since that
       may have the same pointer type as one of our
       baseclasses.  */
    rval = build1 (NOP_EXPR, argtype,
		   convert_pointer_to (basetype, rval));
  else
    /* This conversion is harmless.  */
    rval = convert (argtype, rval);

  if (! integer_zerop (DECL_FIELD_BITPOS (field)))
    {
      tree offset = size_binop (EASY_DIV_EXPR, DECL_FIELD_BITPOS (field),
				size_int (BITS_PER_UNIT));
      int flag = TREE_CONSTANT (rval);
      rval = fold (build (PLUS_EXPR, argtype,
			  rval, convert (argtype, offset)));
      TREE_CONSTANT (rval) = flag;
    }
  return rval;
}
   
/* Construct and perhaps optimize a tree representation
   for a unary operation.  CODE, a tree_code, specifies the operation
   and XARG is the operand.  */

tree
build_x_unary_op (code, xarg)
     enum tree_code code;
     tree xarg;
{
  tree rval;

  /* See comments in `build_x_unary_op'.  */
  if (rval = build_opfncall (code, 0, xarg, NULL_TREE, NULL_TREE))
    {
      if (rval = build_opfncall (code, LOOKUP_PROTECT, xarg, NULL_TREE, NULL_TREE))
	return rval;
      build_opfncall (code, LOOKUP_NORMAL, xarg, NULL_TREE, NULL_TREE);
      return error_mark_node;
    }
  return build_unary_op (code, xarg, 0);
}

/* C++: Must handle pointers to members.

   Perhaps type instantiation should be extended to handle conversion
   from aggregates to types we don't yet know we want?  (Or are those
   cases typically errors which should be reported?)

   NOCONVERT nonzero suppresses the default promotions
   (such as from short to int).  */
tree
build_unary_op (code, xarg, noconvert)
     enum tree_code code;
     tree xarg;
     int noconvert;
{
  /* No default_conversion here.  It causes trouble for ADDR_EXPR.  */
  register tree arg = xarg;
  register tree argtype = 0;
  register enum tree_code typecode = TREE_CODE (TREE_TYPE (arg));
  char *errstring = NULL;
  tree val;
  int isaggrtype;

  if (typecode == ERROR_MARK)
    return error_mark_node;

  if (typecode == REFERENCE_TYPE && code != ADDR_EXPR && ! noconvert)
    {
      arg = convert_from_reference (arg);
      typecode = TREE_CODE (TREE_TYPE (arg));
    }

  if (typecode == ENUMERAL_TYPE)
    typecode = INTEGER_TYPE;

  isaggrtype = IS_AGGR_TYPE_CODE (typecode);

  switch (code)
    {
    case CONVERT_EXPR:
      /* This is used for unary plus, because a CONVERT_EXPR
	 is enough to prevent anybody from looking inside for
	 associativity, but won't generate any code.  */
      if (!(typecode == INTEGER_TYPE || typecode == REAL_TYPE))
        errstring = "wrong type argument to unary plus";
      else if (!noconvert)
	arg = default_conversion (arg);
      break;

    case NEGATE_EXPR:
      if (isaggrtype)
	{
	  if (!noconvert)
	    arg = default_conversion (arg);
	  else
	    {
	      error_with_aggr_type (TREE_TYPE (arg), "type conversion for type `%s' not allowed");
	      return error_mark_node;
	    }
	  typecode = TREE_CODE (TREE_TYPE (arg));
	  noconvert = 1;
	}

      if (!(typecode == INTEGER_TYPE || typecode == REAL_TYPE))
        errstring = "wrong type argument to unary minus";
      else if (!noconvert)
	arg = default_conversion (arg);
      break;

    case BIT_NOT_EXPR:
      if (isaggrtype)
	{
	  if (!noconvert)
	    arg = default_conversion (arg);
	  else
	    {
	      error_with_aggr_type (TREE_TYPE (arg), "type conversion for type `%s' not allowed");
	      return error_mark_node;
	    }
	  typecode = TREE_CODE (TREE_TYPE (arg));
	  noconvert = 1;
	}

      if (typecode != INTEGER_TYPE)
        errstring = "wrong type argument to bit-complement";
      else if (!noconvert)
	arg = default_conversion (arg);
      break;

    case ABS_EXPR:
      if (isaggrtype)
	{
	  if (!noconvert)
	    arg = default_conversion (arg);
	  else
	    {
	      error_with_aggr_type (TREE_TYPE (arg), "type conversion for type `%s' not allowed");
	      return error_mark_node;
	    }
	  typecode = TREE_CODE (TREE_TYPE (arg));
	  noconvert = 1;
	}

      if (!(typecode == INTEGER_TYPE || typecode == REAL_TYPE))
        errstring = "wrong type argument to abs";
      else if (!noconvert)
	arg = default_conversion (arg);
      break;

    case TRUTH_NOT_EXPR:
      if (isaggrtype)
	{
	  arg = truthvalue_conversion (arg);
	  typecode = TREE_CODE (TREE_TYPE (arg));
	}

      if (typecode != INTEGER_TYPE
	  && typecode != REAL_TYPE && typecode != POINTER_TYPE
	  /* These will convert to a pointer.  */
	  && typecode != ARRAY_TYPE && typecode != FUNCTION_TYPE)
	{
	  errstring = "wrong type argument to unary exclamation mark";
	  break;
	}
      arg = truthvalue_conversion (arg);
      val = invert_truthvalue (arg);
      if (val) return val;
      break;

    case NOP_EXPR:
      break;
      
    case PREINCREMENT_EXPR:
    case POSTINCREMENT_EXPR:
    case PREDECREMENT_EXPR:
    case POSTDECREMENT_EXPR:
      /* Handle complex lvalues (when permitted)
	 by reduction to simpler cases.  */

      val = unary_complex_lvalue (code, arg);
      if (val != 0)
	return val;

      /* Report invalid types.  */

      if (isaggrtype)
	{
	  arg = default_conversion (arg);
	  typecode = TREE_CODE (TREE_TYPE (arg));
	}

      if (typecode != POINTER_TYPE
	  && typecode != INTEGER_TYPE && typecode != REAL_TYPE)
	{
	  if (code == PREINCREMENT_EXPR)
	    errstring ="no pre-increment operator for type";
	  else if (code == POSTINCREMENT_EXPR)
	    errstring ="no post-increment operator for type";
	  else if (code == PREDECREMENT_EXPR)
	    errstring ="no pre-decrement operator for type";
	  else
	    errstring ="no post-decrement operator for type";
	  break;
	}

      /* Report something read-only.  */

      if (TYPE_READONLY (TREE_TYPE (arg))
	  || TREE_READONLY (arg))
	readonly_error (arg, ((code == PREINCREMENT_EXPR
			       || code == POSTINCREMENT_EXPR)
			      ? "increment" : "decrement"),
			0);

      {
	register tree inc;
	tree result_type = TREE_TYPE (arg);

	arg = get_unwidened (arg, 0);
	argtype = TREE_TYPE (arg);

	/* ARM $5.2.5 last annotation says this should be forbidden.  */
	if (TREE_CODE (argtype) == ENUMERAL_TYPE)
	  pedwarn ("ANSI C++ forbids %sing an enum",
		   (code == PREINCREMENT_EXPR || code == POSTINCREMENT_EXPR)
		   ? "increment" : "decrement");
	    
	/* Compute the increment.  */

	if (typecode == POINTER_TYPE)
	  {
	    enum tree_code tmp = TREE_CODE (TREE_TYPE (argtype));
	    if (tmp == FUNCTION_TYPE || tmp == METHOD_TYPE
		|| tmp == VOID_TYPE || tmp == OFFSET_TYPE)
	      pedwarn ("ANSI C++ forbids %sing a pointer of type `%s'",
		       ((code == PREINCREMENT_EXPR
			 || code == POSTINCREMENT_EXPR)
			? "increment" : "decrement"),
		       type_as_string (argtype));
	    inc = c_sizeof_nowarn (TREE_TYPE (argtype));
	  }
	else
	  inc = integer_one_node;

	inc = convert (argtype, inc);

	/* Handle incrementing a cast-expression.  */

	switch (TREE_CODE (arg))
	  {
	  case NOP_EXPR:
	  case CONVERT_EXPR:
	  case FLOAT_EXPR:
	  case FIX_TRUNC_EXPR:
	  case FIX_FLOOR_EXPR:
	  case FIX_ROUND_EXPR:
	  case FIX_CEIL_EXPR:
	    {
	      tree incremented, modify, value;
	      pedantic_lvalue_warning (CONVERT_EXPR);
	      arg = stabilize_reference (arg);
	      if (code == PREINCREMENT_EXPR || code == PREDECREMENT_EXPR)
		value = arg;
	      else
		value = save_expr (arg);
	      incremented = build (((code == PREINCREMENT_EXPR
				     || code == POSTINCREMENT_EXPR)
				    ? PLUS_EXPR : MINUS_EXPR),
				   argtype, value, inc);
	      TREE_SIDE_EFFECTS (incremented) = 1;
	      modify = build_modify_expr (arg, NOP_EXPR, incremented);
	      return build (COMPOUND_EXPR, TREE_TYPE (arg), modify, value);
	    }
	  }

	if (TREE_CODE (arg) == OFFSET_REF)
	  arg = resolve_offset_ref (arg);

	/* Complain about anything else that is not a true lvalue.  */
	if (!lvalue_or_else (arg, ((code == PREINCREMENT_EXPR
				    || code == POSTINCREMENT_EXPR)
				   ? "increment" : "decrement")))
	  return error_mark_node;

	val = build (code, TREE_TYPE (arg), arg, inc);
	TREE_SIDE_EFFECTS (val) = 1;
	return convert (result_type, val);
      }

    case ADDR_EXPR:
      /* Note that this operation never does default_conversion
	 regardless of NOCONVERT.  */

      if (TREE_REFERENCE_EXPR (arg))
	{
	  error ("references are not lvalues");
	  return error_mark_node;
	}
      else if (typecode == REFERENCE_TYPE)
	{
	  arg = build1 (CONVERT_EXPR, build_pointer_type (TREE_TYPE (TREE_TYPE (arg))), arg);
	  TREE_REFERENCE_EXPR (arg) = 1;
	  return arg;
	}
      else if (TREE_CODE (arg) == FUNCTION_DECL
	       && DECL_NAME (arg)
	       && DECL_CONTEXT (arg) == NULL_TREE
	       && IDENTIFIER_LENGTH (DECL_NAME (arg)) == 4
	       && IDENTIFIER_POINTER (DECL_NAME (arg))[0] == 'm'
	       && ! strcmp (IDENTIFIER_POINTER (DECL_NAME (arg)), "main"))
	{
	  /* ARM $3.4 */
	  error ("attempt to take address of function `main'");
	  return error_mark_node;
	}

      /* Let &* cancel out to simplify resulting code.  */
      if (TREE_CODE (arg) == INDIRECT_REF)
	{
	  /* We don't need to have `current_class_decl' wrapped in a
	     NON_LVALUE_EXPR node.  */
	  if (arg == C_C_D)
	    return current_class_decl;

	  /* Keep `default_conversion' from converting if
	     ARG is of REFERENCE_TYPE.  */
	  arg = TREE_OPERAND (arg, 0);
	  if (TREE_CODE (TREE_TYPE (arg)) == REFERENCE_TYPE)
	    {
	      if (TREE_CODE (arg) == VAR_DECL && DECL_INITIAL (arg)
		  && !TREE_SIDE_EFFECTS (DECL_INITIAL (arg)))
		arg = DECL_INITIAL (arg);
	      arg = build1 (CONVERT_EXPR, build_pointer_type (TREE_TYPE (TREE_TYPE (arg))), arg);
	      TREE_REFERENCE_EXPR (arg) = 1;
	      TREE_CONSTANT (arg) = TREE_CONSTANT (TREE_OPERAND (arg, 0));
	    }
	  else if (lvalue_p (arg))
	    /* Don't let this be an lvalue.  */
	    return non_lvalue (arg);
	  return arg;
	}

      /* For &x[y], return x+y */
      if (TREE_CODE (arg) == ARRAY_REF)
	{
	  if (mark_addressable (TREE_OPERAND (arg, 0)) == 0)
	    return error_mark_node;
	  return build_binary_op (PLUS_EXPR, TREE_OPERAND (arg, 0),
				  TREE_OPERAND (arg, 1), 1);
	}

      /* Uninstantiated types are all functions.  Taking the
	 address of a function is a no-op, so just return the
	 argument.  */

      if (TREE_CODE (arg) == IDENTIFIER_NODE
	  && IDENTIFIER_OPNAME_P (arg))
	{
	  my_friendly_abort (117);
	  /* We don't know the type yet, so just work around the problem.
	     We know that this will resolve to an lvalue.  */
	  return build1 (ADDR_EXPR, unknown_type_node, arg);
	}

      if (TREE_CODE (arg) == TREE_LIST)
	{
	  /* Look at methods with only this name.  */
	  if (TREE_CODE (TREE_VALUE (arg)) == FUNCTION_DECL)
	    {
	      tree targ = TREE_VALUE (arg);

	      /* If this function is unique, or it is a unique
		 constructor, we can takes its address easily.  */
	      if (DECL_CHAIN (targ) == NULL_TREE
		  || (DESTRUCTOR_NAME_P (DECL_ASSEMBLER_NAME (targ))
		      && DECL_CHAIN (DECL_CHAIN (targ)) == NULL_TREE))
		{
		  if (DECL_CHAIN (targ))
		    targ = DECL_CHAIN (targ);
		  if (DECL_CLASS_CONTEXT (targ))
		    targ = build (OFFSET_REF, TREE_TYPE (targ), C_C_D, targ);

		  val = unary_complex_lvalue (ADDR_EXPR, targ);
		  if (val)
		    return val;
		}

	      /* This possible setting of TREE_CONSTANT is what makes it possible
		 with an initializer list to emit the entire thing in the data
		 section, rather than a run-time initialization.  */
	      arg = build1 (ADDR_EXPR, unknown_type_node, arg);
	      if (staticp (targ))
		TREE_CONSTANT (arg) = 1;
	      return arg;
	    }
	  if (TREE_CHAIN (arg) == NULL_TREE
	      && DECL_CHAIN (TREE_VALUE (TREE_VALUE (arg))) == NULL_TREE)
	    {
	      /* Unique overloaded member function.  */
	      return build_unary_op (ADDR_EXPR, TREE_VALUE (TREE_VALUE (arg)), 0);
	    }
	  return build1 (ADDR_EXPR, unknown_type_node, arg);
	}

      /* Handle complex lvalues (when permitted)
	 by reduction to simpler cases.  */
      val = unary_complex_lvalue (code, arg);
      if (val != 0)
	return val;

#if 0 /* Turned off because inconsistent;
	 float f; *&(int)f = 3.4 stores in int format
	 whereas (int)f = 3.4 stores in float format.  */
      /* Address of a cast is just a cast of the address
	 of the operand of the cast.  */
      switch (TREE_CODE (arg))
	{
	case NOP_EXPR:
	case CONVERT_EXPR:
	case FLOAT_EXPR:
	case FIX_TRUNC_EXPR:
	case FIX_FLOOR_EXPR:
	case FIX_ROUND_EXPR:
	case FIX_CEIL_EXPR:
	  if (pedantic)
	    pedwarn ("ANSI C++ forbids taking the address of a cast expression");
	  return convert (build_pointer_type (TREE_TYPE (arg)),
			  build_unary_op (ADDR_EXPR, TREE_OPERAND (arg, 0), 0));
	}
#endif

      /* Allow the address of a constructor if all the elements
	 are constant.  */
      if (TREE_CODE (arg) == CONSTRUCTOR && TREE_CONSTANT (arg))
	;
      /* Anything not already handled and not a true memory reference
	 is an error.  */
      else if (typecode != FUNCTION_TYPE
	       && typecode != METHOD_TYPE
	       && !lvalue_or_else (arg, "unary `&'"))
	return error_mark_node;

      /* Ordinary case; arg is a COMPONENT_REF or a decl.  */
      argtype = TREE_TYPE (arg);
      /* If the lvalue is const or volatile,
	 merge that into the type that the address will point to.  */
      if (TREE_CODE_CLASS (TREE_CODE (arg)) == 'd'
	  || TREE_CODE_CLASS (TREE_CODE (arg)) == 'r')
	{
	  if (TREE_READONLY (arg) || TREE_THIS_VOLATILE (arg))
	    argtype = build_type_variant (argtype,
					  TREE_READONLY (arg),
					  TREE_THIS_VOLATILE (arg));
	}

      argtype = build_pointer_type (argtype);

      if (mark_addressable (arg) == 0)
	return error_mark_node;

      {
	tree addr;

	if (TREE_CODE (arg) == COMPONENT_REF)
	  addr = build_component_addr (arg, argtype,
				       "attempt to take address of bit-field structure member `%s'");
	else
	  addr = build1 (code, argtype, arg);

	/* Address of a static or external variable or
	   function counts as a constant */
	if (staticp (arg))
	  TREE_CONSTANT (addr) = 1;
	return addr;
      }
    }

  if (!errstring)
    {
      if (argtype == 0)
	argtype = TREE_TYPE (arg);
      return fold (build1 (code, argtype, arg));
    }

  error (errstring);
  return error_mark_node;
}

/* If CONVERSIONS is a conversion expression or a nested sequence of such,
   convert ARG with the same conversions in the same order
   and return the result.  */

static tree
convert_sequence (conversions, arg)
     tree conversions;
     tree arg;
{
  switch (TREE_CODE (conversions))
    {
    case NOP_EXPR:
    case CONVERT_EXPR:
    case FLOAT_EXPR:
    case FIX_TRUNC_EXPR:
    case FIX_FLOOR_EXPR:
    case FIX_ROUND_EXPR:
    case FIX_CEIL_EXPR:
      return convert (TREE_TYPE (conversions),
		      convert_sequence (TREE_OPERAND (conversions, 0),
					arg));

    default:
      return arg;
    }
}

/* Apply unary lvalue-demanding operator CODE to the expression ARG
   for certain kinds of expressions which are not really lvalues
   but which we can accept as lvalues.

   If ARG is not a kind of expression we can handle, return zero.  */
   
tree
unary_complex_lvalue (code, arg)
     enum tree_code code;
     tree arg;
{
  /* Handle (a, b) used as an "lvalue".  */
  if (TREE_CODE (arg) == COMPOUND_EXPR)
    {
      tree real_result = build_unary_op (code, TREE_OPERAND (arg, 1), 0);
      pedantic_lvalue_warning (COMPOUND_EXPR);
      return build (COMPOUND_EXPR, TREE_TYPE (real_result),
		    TREE_OPERAND (arg, 0), real_result);
    }

  /* Handle (a ? b : c) used as an "lvalue".  */
  if (TREE_CODE (arg) == COND_EXPR)
    {
      pedantic_lvalue_warning (COND_EXPR);
      return (build_conditional_expr
	      (TREE_OPERAND (arg, 0),
	       build_unary_op (code, TREE_OPERAND (arg, 1), 0),
	       build_unary_op (code, TREE_OPERAND (arg, 2), 0)));
    }

  if (code != ADDR_EXPR)
    return 0;

  /* Handle (a = b) used as an "lvalue" for `&'.  */
  if (TREE_CODE (arg) == MODIFY_EXPR
      || TREE_CODE (arg) == INIT_EXPR)
    {
      tree real_result = build_unary_op (code, TREE_OPERAND (arg, 0), 0);
      return build (COMPOUND_EXPR, TREE_TYPE (real_result), arg, real_result);
    }

  if (TREE_CODE (arg) == WITH_CLEANUP_EXPR)
    {
      tree real_result = build_unary_op (code, TREE_OPERAND (arg, 0), 0);
      real_result = build (WITH_CLEANUP_EXPR, TREE_TYPE (real_result),
			   real_result, 0, TREE_OPERAND (arg, 2));
      return real_result;
    }

  if (TREE_CODE (TREE_TYPE (arg)) == FUNCTION_TYPE
      || TREE_CODE (TREE_TYPE (arg)) == METHOD_TYPE
      || TREE_CODE (TREE_TYPE (arg)) == OFFSET_TYPE)
    {
      /* The representation of something of type OFFSET_TYPE
	 is really the representation of a pointer to it.
	 Here give the representation its true type.  */
      tree t;
      tree offset;

      my_friendly_assert (TREE_CODE (arg) != SCOPE_REF, 313);

      if (TREE_CODE (arg) != OFFSET_REF)
	return 0;

      t = TREE_OPERAND (arg, 1);

      if (TREE_CODE (t) == FUNCTION_DECL)
	{
	  tree context = NULL_TREE;

	  if (DECL_VINDEX (t)
	      || (flag_all_virtual == 1
		  && ((context = decl_type_context (t))
		      && TYPE_OVERLOADS_METHOD_CALL_EXPR (context)
		      && ! DECL_CONSTRUCTOR_P (t))))
	    {
	      offset = copy_node (DECL_VINDEX (t));
	      TREE_TYPE (offset) = build_pointer_type (TREE_TYPE (arg));
	    }
	  else
	    offset = build_unary_op (ADDR_EXPR, t, 0);

	  return offset;
	}
      if (TREE_CODE (t) == VAR_DECL)
	return build_unary_op (ADDR_EXPR, t, 0);
      else
	{
	  /* Can't build a pointer to member if the member must
	     go through virtual base classes.  */
	  if (virtual_member (DECL_FIELD_CONTEXT (t),
			      CLASSTYPE_VBASECLASSES (TREE_TYPE (TREE_OPERAND (arg, 0)))))
	    {
	      sorry ("pointer to member via virtual baseclass");
	      return error_mark_node;
	    }

	  if (TREE_OPERAND (arg, 0)
	      && (TREE_CODE (TREE_OPERAND (arg, 0)) != NOP_EXPR
		  || TREE_OPERAND (TREE_OPERAND (arg, 0), 0) != error_mark_node))
	    {
	      /* Don't know if this should return address to just
		 _DECL, or actual address resolved in this expression.  */
	      sorry ("address of bound pointer-to-member expression");
	      return error_mark_node;
	    }

	  return convert (build_pointer_type (TREE_TYPE (arg)),
			  size_binop (EASY_DIV_EXPR, 
				      DECL_FIELD_BITPOS (t),
				      size_int (BITS_PER_UNIT)));
	}
    }

  if (TREE_CODE (arg) == OFFSET_REF)
    {
      tree left = TREE_OPERAND (arg, 0), left_addr;
      tree right_addr = build_unary_op (ADDR_EXPR, TREE_OPERAND (arg, 1), 0);

      if (left == 0)
	if (current_class_decl)
	  left_addr = current_class_decl;
	else
	  {
	    error ("no `this' for pointer to member");
	    return error_mark_node;
	  }
      else
	left_addr = build_unary_op (ADDR_EXPR, left, 0);

      return build (PLUS_EXPR, build_pointer_type (TREE_TYPE (arg)),
		    build1 (NOP_EXPR, integer_type_node, left_addr),
		    build1 (NOP_EXPR, integer_type_node, right_addr));
    }

  /* We permit compiler to make function calls returning
     objects of aggregate type look like lvalues.  */
  {
    tree targ = arg;

    if (TREE_CODE (targ) == SAVE_EXPR)
      targ = TREE_OPERAND (targ, 0);

    if (TREE_CODE (targ) == CALL_EXPR && IS_AGGR_TYPE (TREE_TYPE (targ)))
      {
	if (TREE_CODE (arg) == SAVE_EXPR)
	  targ = arg;
	else
	  targ = build_cplus_new (TREE_TYPE (arg), arg, 1);
	return build1 (ADDR_EXPR, TYPE_POINTER_TO (TREE_TYPE (arg)), targ);
      }

    if (TREE_CODE (arg) == SAVE_EXPR && TREE_CODE (targ) == INDIRECT_REF)
      return build (SAVE_EXPR, TYPE_POINTER_TO (TREE_TYPE (arg)),
		     TREE_OPERAND (targ, 0), current_function_decl, NULL);

    /* We shouldn't wrap WITH_CLEANUP_EXPRs inside of SAVE_EXPRs, but in case
       we do, here's how to handle it.  */
    if (TREE_CODE (arg) == SAVE_EXPR && TREE_CODE (targ) == WITH_CLEANUP_EXPR)
      {
#if 0
	/* Not really a bug, but something to turn on when testing.  */
	compiler_error ("WITH_CLEANUP_EXPR wrapped in SAVE_EXPR");
#endif
	return unary_complex_lvalue (ADDR_EXPR, targ);
      }
  }

  /* Don't let anything else be handled specially.  */
  return 0;
}

/* If pedantic, warn about improper lvalue.   CODE is either COND_EXPR
   COMPOUND_EXPR, or CONVERT_EXPR (for casts).  */

static void
pedantic_lvalue_warning (code)
     enum tree_code code;
{
  if (pedantic)
    pedwarn ("ANSI C++ forbids use of %s expressions as lvalues",
	     code == COND_EXPR ? "conditional"
	     : code == COMPOUND_EXPR ? "compound" : "cast");
}

/* Mark EXP saying that we need to be able to take the
   address of it; it should not be allocated in a register.
   Value is 1 if successful.

   C++: we do not allow `current_class_decl' to be addressable.  */

int
mark_addressable (exp)
     tree exp;
{
  register tree x = exp;

  if (TREE_ADDRESSABLE (x) == 1)
    return 1;

  while (1)
    switch (TREE_CODE (x))
      {
      case ADDR_EXPR:
      case COMPONENT_REF:
      case ARRAY_REF:
	x = TREE_OPERAND (x, 0);
	break;

      case PARM_DECL:
	if (x == current_class_decl)
	  {
	    error ("address of `this' not available");
	    TREE_ADDRESSABLE (x) = 1; /* so compiler doesn't die later */
	    put_var_into_stack (x);
	    return 1;
	  }
      case VAR_DECL:
	if (TREE_STATIC (x)
	    && TREE_READONLY (x)
	    && DECL_RTL (x) != 0
	    && ! decl_in_memory_p (x))
	  {
	    /* We thought this would make a good constant variable,
	       but we were wrong.  */
	    push_obstacks_nochange ();
	    end_temporary_allocation ();

	    TREE_ASM_WRITTEN (x) = 0;
	    DECL_RTL (x) = 0;
	    rest_of_decl_compilation (x, 0, IDENTIFIER_LOCAL_VALUE (x) == 0, 0);
	    TREE_ADDRESSABLE (x) = 1;

	    pop_obstacks ();

	    return 1;
	  }
	/* Caller should not be trying to mark initialized
	   constant fields addressable.  */
	my_friendly_assert (DECL_LANG_SPECIFIC (x) == 0
			    || DECL_IN_AGGR_P (x) == 0
			    || TREE_STATIC (x)
			    || DECL_EXTERNAL (x), 314);

      case CONST_DECL:
	if (DECL_REGISTER (x))
	  {
	    if (TREE_PUBLIC (x))
	      {
		error ("address of global register variable `%s' requested",
		       IDENTIFIER_POINTER (DECL_NAME (x)));
		return 0;
	      }
	    warning ("address requested for `%s', which is declared `register'",
		     IDENTIFIER_POINTER (DECL_NAME (x)));
	  }
	put_var_into_stack (x);
	TREE_ADDRESSABLE (x) = 1;
	return 1;

      case RESULT_DECL:
	put_var_into_stack (x);
	TREE_ADDRESSABLE (x) = 1;
	return 1;

      case FUNCTION_DECL:
	/* We have to test both conditions here.  The first may
	   be non-zero in the case of processing a default function.
	   The second may be non-zero in the case of a template function.  */
	x = DECL_MAIN_VARIANT (x);
	if ((DECL_INLINE (x) || DECL_PENDING_INLINE_INFO (x))
	    && (DECL_CONTEXT (x) == NULL_TREE
		|| TREE_CODE_CLASS (TREE_CODE (DECL_CONTEXT (x))) != 't'
		|| ! CLASSTYPE_INTERFACE_ONLY (DECL_CONTEXT (x))))
	  {
	    mark_inline_for_output (x);
	    if (x == current_function_decl)
	      DECL_EXTERNAL (x) = 0;
	  }
	TREE_ADDRESSABLE (x) = 1;
	TREE_USED (x) = 1;
	TREE_ADDRESSABLE (DECL_ASSEMBLER_NAME (x)) = 1;
	return 1;

      default:
	return 1;
    }
}

/* Build and return a conditional expression IFEXP ? OP1 : OP2.  */

tree
build_x_conditional_expr (ifexp, op1, op2)
     tree ifexp, op1, op2;
{
  tree rval;

  /* See comments in `build_x_binary_op'.  */
  if (op1 != 0 && (rval = build_opfncall (COND_EXPR, 0, ifexp, op1, op2)))
    {
      if (rval = build_opfncall (COND_EXPR, LOOKUP_PROTECT, ifexp, op1, op2))
	return rval;
      build_opfncall (COND_EXPR, LOOKUP_NORMAL, ifexp, op1, op2);
      return error_mark_node;
    }
  return build_conditional_expr (ifexp, op1, op2);
}

tree
build_conditional_expr (ifexp, op1, op2)
     tree ifexp, op1, op2;
{
  register tree type1;
  register tree type2;
  register enum tree_code code1;
  register enum tree_code code2;
  register tree result_type = NULL_TREE;

  /* If second operand is omitted, it is the same as the first one;
     make sure it is calculated only once.  */
  if (op1 == 0)
    {
      if (pedantic)
	pedwarn ("ANSI C++ forbids omitting the middle term of a ?: expression");
      ifexp = op1 = save_expr (ifexp);
    }

  ifexp = truthvalue_conversion (default_conversion (ifexp));

  if (TREE_CODE (ifexp) == ERROR_MARK)
    return error_mark_node;

  op1 = require_instantiated_type (TREE_TYPE (op2), op1, error_mark_node);
  if (op1 == error_mark_node)
    return error_mark_node;
  op2 = require_instantiated_type (TREE_TYPE (op1), op2, error_mark_node);
  if (op2 == error_mark_node)
    return error_mark_node;

  /* C++: REFERENCE_TYPES must be dereferenced.  */
  type1 = TREE_TYPE (op1);
  code1 = TREE_CODE (type1);
  type2 = TREE_TYPE (op2);
  code2 = TREE_CODE (type2);

  if (code1 == REFERENCE_TYPE)
    {
      op1 = convert_from_reference (op1);
      type1 = TREE_TYPE (op1);
      code1 = TREE_CODE (type1);
    }
  if (code2 == REFERENCE_TYPE)
    {
      op2 = convert_from_reference (op2);
      type2 = TREE_TYPE (op2);
      code2 = TREE_CODE (type2);
    }

#if 1 /* Produces wrong result if within sizeof.  Sorry.  */
  /* Don't promote the operands separately if they promote
     the same way.  Return the unpromoted type and let the combined
     value get promoted if necessary.  */

  if (TYPE_MAIN_VARIANT (type1) == TYPE_MAIN_VARIANT (type2)
      && code2 != ARRAY_TYPE
#if 0
      /* For C++, let the enumeral type come through.  */
      && code2 != ENUMERAL_TYPE
#endif
      && code2 != FUNCTION_TYPE
      && code2 != METHOD_TYPE)
    {
      tree result;

      if (TREE_CONSTANT (ifexp)
	  && (TREE_CODE (ifexp) == INTEGER_CST
	      || TREE_CODE (ifexp) == ADDR_EXPR))
	return (integer_zerop (ifexp) ? op2 : op1);

      if (TREE_CODE (op1) == CONST_DECL)
	op1 = DECL_INITIAL (op1);
      else if (TREE_READONLY_DECL_P (op1))
	op1 = decl_constant_value (op1);
      if (TREE_CODE (op2) == CONST_DECL)
	op2 = DECL_INITIAL (op2);
      else if (TREE_READONLY_DECL_P (op2))
	op2 = decl_constant_value (op2);
      if (type1 != type2)
	type1 = build_type_variant
			(type1,
			 TREE_READONLY (op1) || TREE_READONLY (op2),
			 TREE_THIS_VOLATILE (op1) || TREE_THIS_VOLATILE (op2));
      /* ??? This is a kludge to deal with the fact that
	 we don't sort out integers and enums properly, yet.  */
      result = fold (build (COND_EXPR, type1, ifexp, op1, op2));
      if (TREE_TYPE (result) != type1)
	result = build1 (NOP_EXPR, type1, result);
      return result;
    }
#endif

  /* They don't match; promote them both and then try to reconcile them.
     But don't permit mismatching enum types.  */
  if (code1 == ENUMERAL_TYPE)
    {
      if (code2 == ENUMERAL_TYPE)
	{
	  message_2_types (error, "enumeral mismatch in conditional expression: `%s' vs `%s'", type1, type2);
	  return error_mark_node;
	}
      else if (extra_warnings && ! IS_AGGR_TYPE_CODE (code2))
	warning ("enumeral and non-enumeral type in conditional expression");
    }
  else if (extra_warnings
	   && code2 == ENUMERAL_TYPE && ! IS_AGGR_TYPE_CODE (code1))
    warning ("enumeral and non-enumeral type in conditional expression");

  if (code1 != VOID_TYPE)
    {
      op1 = default_conversion (op1);
      type1 = TREE_TYPE (op1);
      code1 = TREE_CODE (type1);
    }
  if (code2 != VOID_TYPE)
    {
      op2 = default_conversion (op2);
      type2 = TREE_TYPE (op2);
      code2 = TREE_CODE (type2);
    }

  /* Quickly detect the usual case where op1 and op2 have the same type
     after promotion.  */
  if (TYPE_MAIN_VARIANT (type1) == TYPE_MAIN_VARIANT (type2))
    {
      if (type1 == type2)
	result_type = type1;
      else
	result_type = build_type_variant
			(type1,
			 TREE_READONLY (op1) || TREE_READONLY (op2),
			 TREE_THIS_VOLATILE (op1) || TREE_THIS_VOLATILE (op2));
    }
  else if ((code1 == INTEGER_TYPE || code1 == REAL_TYPE)
           && (code2 == INTEGER_TYPE || code2 == REAL_TYPE))
    {
      result_type = common_type (type1, type2);
    }
  else if (code1 == VOID_TYPE || code2 == VOID_TYPE)
    {
      if (pedantic && (code1 != VOID_TYPE || code2 != VOID_TYPE))
	pedwarn ("ANSI C++ forbids conditional expr with only one void side");
      result_type = void_type_node;
    }
  else if (code1 == POINTER_TYPE && code2 == POINTER_TYPE)
    {
      if (comp_target_types (type1, type2, 1))
	result_type = common_type (type1, type2);
      else if (integer_zerop (op1) && TREE_TYPE (type1) == void_type_node)
	result_type = qualify_type (type2, type1);
      else if (integer_zerop (op2) && TREE_TYPE (type2) == void_type_node)
	result_type = qualify_type (type1, type2);
      else if (TYPE_MAIN_VARIANT (TREE_TYPE (type1)) == void_type_node)
	{
	  if (pedantic && TREE_CODE (type2) == FUNCTION_TYPE)
	    pedwarn ("ANSI C++ forbids conditional expr between `void *' and function pointer");
	  result_type = qualify_type (type1, type2);
	}
      else if (TYPE_MAIN_VARIANT (TREE_TYPE (type2)) == void_type_node)
	{
	  if (pedantic && TREE_CODE (type1) == FUNCTION_TYPE)
	    pedwarn ("ANSI C++ forbids conditional expr between `void *' and function pointer");
	  result_type = qualify_type (type2, type1);
	}
      /* C++ */
      else if (comptypes (type2, type1, 0))
	result_type = type2;
      else if (IS_AGGR_TYPE (TREE_TYPE (type1))
	       && IS_AGGR_TYPE (TREE_TYPE (type2))
	       && (result_type = common_base_type (TREE_TYPE (type1), TREE_TYPE (type2))))
	{
	  if (result_type == error_mark_node)
	    {
	      message_2_types (error, "common base type of types `%s' and `%s' is ambiguous",
			       TREE_TYPE (type1), TREE_TYPE (type2));
	      result_type = ptr_type_node;
	    }
	  else result_type = TYPE_POINTER_TO (result_type);
	}
      else
	{
	  warning ("pointer type mismatch in conditional expression");
	  result_type = ptr_type_node;
	}
    }
  else if (code1 == POINTER_TYPE && code2 == INTEGER_TYPE)
    {
      if (!integer_zerop (op2))
	warning ("pointer/integer type mismatch in conditional expression");
      else
	{
	  op2 = null_pointer_node;
	  if (pedantic && TREE_CODE (type1) == FUNCTION_TYPE)
	    pedwarn ("ANSI C++ forbids conditional expr between 0 and function pointer");
	}
      result_type = type1;
    }
  else if (code2 == POINTER_TYPE && code1 == INTEGER_TYPE)
    {
      if (!integer_zerop (op1))
	warning ("pointer/integer type mismatch in conditional expression");
      else
	{
	  op1 = null_pointer_node;
	  if (pedantic && TREE_CODE (type2) == FUNCTION_TYPE)
	    pedwarn ("ANSI C++ forbids conditional expr between 0 and function pointer");
	}
      result_type = type2;
      op1 = null_pointer_node;
    }

  if (!result_type)
    {
      /* The match does not look good.  If either is
	 an aggregate value, try converting to a scalar type.  */
      if (code1 == RECORD_TYPE && code2 == RECORD_TYPE)
	{
	  message_2_types (error, "aggregate mismatch in conditional expression: `%s' vs `%s'", type1, type2);
	  return error_mark_node;
	}
      if (code1 == RECORD_TYPE && TYPE_HAS_CONVERSION (type1))
	{
	  tree tmp = build_type_conversion (CONVERT_EXPR, type2, op1, 0);
	  if (tmp == NULL_TREE)
	    {
	      error_with_aggr_type (type1, "aggregate type `%s' could not convert on lhs of `:'");
	      return error_mark_node;
	    }
	  if (tmp == error_mark_node)
	    error ("ambiguous pointer conversion");
	  result_type = type2;
	  op1 = tmp;
	}
      else if (code2 == RECORD_TYPE && TYPE_HAS_CONVERSION (type2))
	{
	  tree tmp = build_type_conversion (CONVERT_EXPR, type1, op2, 0);
	  if (tmp == NULL_TREE)
	    {
	      error_with_aggr_type (type2, "aggregate type `%s' could not convert on rhs of `:'");
	      return error_mark_node;
	    }
	  if (tmp == error_mark_node)
	    error ("ambiguous pointer conversion");
	  result_type = type1;
	  op2 = tmp;
	}
      else if (flag_cond_mismatch)
	result_type = void_type_node;
      else
	{
	  error ("type mismatch in conditional expression");
	  return error_mark_node;
	}
    }

  if (result_type != TREE_TYPE (op1))
    op1 = convert (result_type, op1);
  if (result_type != TREE_TYPE (op2))
    op2 = convert (result_type, op2);

#if 0
  /* XXX delete me, I've been here for years.  */
  if (IS_AGGR_TYPE_CODE (code1))
    {
      result_type = TREE_TYPE (op1);
      if (TREE_CONSTANT (ifexp))
	return (integer_zerop (ifexp) ? op2 : op1);

      if (TYPE_MODE (result_type) == BLKmode)
	{
	  register tree tempvar
	    = build_decl (VAR_DECL, NULL_TREE, result_type);
	  register tree xop1 = build_modify_expr (tempvar, NOP_EXPR, op1);
	  register tree xop2 = build_modify_expr (tempvar, NOP_EXPR, op2);
	  register tree result = fold (build (COND_EXPR, result_type,
					      ifexp, xop1, xop2));

	  layout_decl (tempvar, 0);
	  /* No way to handle variable-sized objects here.
	     I fear that the entire handling of BLKmode conditional exprs
	     needs to be redone.  */
	  my_friendly_assert (TREE_CONSTANT (DECL_SIZE (tempvar)), 315);
	  DECL_RTL (tempvar)
	    = assign_stack_local (DECL_MODE (tempvar),
				  (TREE_INT_CST_LOW (DECL_SIZE (tempvar))
				   + BITS_PER_UNIT - 1)
				  / BITS_PER_UNIT,
				  0);

	  TREE_SIDE_EFFECTS (result)
	    = TREE_SIDE_EFFECTS (ifexp) | TREE_SIDE_EFFECTS (op1)
	      | TREE_SIDE_EFFECTS (op2);
	  return build (COMPOUND_EXPR, result_type, result, tempvar);
	}
    }
#endif /* 0 */

  if (TREE_CONSTANT (ifexp))
    return (integer_zerop (ifexp) ? op2 : op1);

  return fold (build (COND_EXPR, result_type, ifexp, op1, op2));
}

/* Handle overloading of the ',' operator when needed.  Otherwise,
   this function just builds an expression list.  */
tree
build_x_compound_expr (list)
     tree list;
{
  tree rest = TREE_CHAIN (list);
  tree result;

  if (rest == NULL_TREE)
    return build_compound_expr (list);

  result = build_opfncall (COMPOUND_EXPR, LOOKUP_NORMAL,
			   TREE_VALUE (list), TREE_VALUE (rest), NULL_TREE);
  if (result)
    return build_x_compound_expr (tree_cons (NULL_TREE, result, TREE_CHAIN (rest)));
  else
    return build_compound_expr (tree_cons (NULL_TREE, TREE_VALUE (list),
					   build_tree_list (NULL_TREE, build_x_compound_expr (rest))));
}

/* Given a list of expressions, return a compound expression
   that performs them all and returns the value of the last of them.  */

tree
build_compound_expr (list)
     tree list;
{
  register tree rest;

  if (TREE_READONLY_DECL_P (TREE_VALUE (list)))
    TREE_VALUE (list) = decl_constant_value (TREE_VALUE (list));

  if (TREE_CHAIN (list) == 0)
    {
      /* build_c_cast puts on a NOP_EXPR to make the result not an lvalue.
	 Strip such NOP_EXPRs, since LIST is used in non-lvalue context.  */
      if (TREE_CODE (list) == NOP_EXPR
	  && TREE_TYPE (list) == TREE_TYPE (TREE_OPERAND (list, 0)))
	list = TREE_OPERAND (list, 0);

      /* Convert arrays to pointers.  */
      if (TREE_CODE (TREE_TYPE (TREE_VALUE (list))) == ARRAY_TYPE)
	return default_conversion (TREE_VALUE (list));
      else
	return TREE_VALUE (list);
    }

  rest = build_compound_expr (TREE_CHAIN (list));

  if (! TREE_SIDE_EFFECTS (TREE_VALUE (list)))
    return rest;

  return build (COMPOUND_EXPR, TREE_TYPE (rest),
		break_out_cleanups (TREE_VALUE (list)), rest);
}

/* Build an expression representing a cast to type TYPE of expression EXPR.  */

tree
build_c_cast (type, expr)
     register tree type;
     tree expr;
{
  register tree value = expr;

  if (type == error_mark_node || expr == error_mark_node)
    return error_mark_node;

  /* build_c_cast puts on a NOP_EXPR to make the result not an lvalue.
     Strip such NOP_EXPRs, since VALUE is being used in non-lvalue context.  */
  if (TREE_CODE (value) == NOP_EXPR
      && TREE_TYPE (value) == TREE_TYPE (TREE_OPERAND (value, 0)))
    value = TREE_OPERAND (value, 0);

  if (TREE_TYPE (expr)
      && TREE_CODE (TREE_TYPE (expr)) == OFFSET_TYPE
      && TREE_CODE (type) != OFFSET_TYPE)
    value = resolve_offset_ref (value);

  if (TREE_CODE (type) == ARRAY_TYPE)
    {
      /* Allow casting from T1* to T2[] because Cfront allows it.
	 NIHCL uses it. It is not valid ANSI C however, and hence, not
	 valid ANSI C++.  */
      if (TREE_CODE (TREE_TYPE (expr)) == POINTER_TYPE)
	{
	  if (pedantic)
	    pedwarn ("ANSI C++ forbids casting to an array type");
	  type = build_pointer_type (TREE_TYPE (type));
	}
      else
	{
	  error ("ANSI C++ forbids casting to an array type");
	  return error_mark_node;
	}
    }

  /* When converting into a reference type, just convert into a pointer
     to the new type and deference it.  While this is not exactly what ARM 5.4
     calls for, it is pretty close for now.  (int &)ri  --->  *(int*)&ri  */
  if (TREE_CODE (type) == REFERENCE_TYPE)
    {
      value = build_unary_op (ADDR_EXPR, value, 0);
      if (value != error_mark_node)
	value = convert (build_pointer_type (TREE_TYPE (type)), value);
      if (value != error_mark_node)
	value = build_indirect_ref (value, "reference conversion");
      return value;
    }

  if (TREE_TYPE (value)
      && TYPE_MAIN_VARIANT (type) == TYPE_MAIN_VARIANT (TREE_TYPE (value)))
    {
      /* For C++, we must copy the constness of TYPE into VALUE.  */
      if (TREE_READONLY (value) != TYPE_READONLY (type))
	{
	  value = copy_node (value);
	  TREE_READONLY (value) = TYPE_READONLY (type);
	}
      else if (pedantic)
	{
	  if (TREE_CODE (type) == RECORD_TYPE
	      || TREE_CODE (type) == UNION_TYPE)
	    pedwarn ("ANSI C++ forbids casting nonscalar to the same type");
	}
      return value;
    }

  /* If there's only one function in the overloaded space,
     just take it.  */
  if (TREE_CODE (value) == TREE_LIST
      && TREE_CHAIN (value) == NULL_TREE)
    value = TREE_VALUE (value);

  /* Make up for the fact that we do not always perform
     `default_conversion' anymore.  */
  if (TREE_READONLY_DECL_P (value))
    value = decl_constant_value (value);

  if (TREE_TYPE (value) == NULL_TREE
      || type_unknown_p (value))
    {
      value = instantiate_type (type, value, 1);
      /* Did we lose?  */
      if (value == error_mark_node)
	return error_mark_node;
    }
  else
    {
      tree otype;
      /* Convert functions and arrays to pointers and
	 convert references to their expanded types,
	 but don't convert any other types.  */
      if (TREE_CODE (TREE_TYPE (value)) == FUNCTION_TYPE
	  || TREE_CODE (TREE_TYPE (value)) == ARRAY_TYPE
	  || TREE_CODE (TREE_TYPE (value)) == REFERENCE_TYPE)
	value = default_conversion (value);
      otype = TREE_TYPE (value);

      /* Optionally warn about potentially worrisome casts.  */

      if (warn_cast_qual
	  && TREE_CODE (type) == POINTER_TYPE
	  && TREE_CODE (otype) == POINTER_TYPE)
	{
	  /* For C++ we make these regular warnings, rather than
	     softening them into pedwarns.  */
	  if (TYPE_VOLATILE (TREE_TYPE (otype))
	      && ! TYPE_VOLATILE (TREE_TYPE (type)))
	    warning ("cast discards `volatile' from pointer target type");
	  if (TYPE_READONLY (TREE_TYPE (otype))
	      && ! TYPE_READONLY (TREE_TYPE (type)))
	    warning ("cast discards `const' from pointer target type");
	}

      /* Warn about possible alignment problems.  */
      if (STRICT_ALIGNMENT && warn_cast_align
	  && TREE_CODE (type) == POINTER_TYPE
	  && TREE_CODE (otype) == POINTER_TYPE
	  && TREE_CODE (TREE_TYPE (otype)) != VOID_TYPE
	  && TREE_CODE (TREE_TYPE (otype)) != FUNCTION_TYPE
	  && TYPE_ALIGN (TREE_TYPE (type)) > TYPE_ALIGN (TREE_TYPE (otype)))
	warning ("cast increases required alignment of target type");

#if 0
      if (TREE_CODE (type) == INTEGER_TYPE
	  && TREE_CODE (otype) == POINTER_TYPE
	  && TYPE_PRECISION (type) != TYPE_PRECISION (otype))
	warning ("cast from pointer to integer of different size");

      if (TREE_CODE (type) == POINTER_TYPE
	  && TREE_CODE (otype) == INTEGER_TYPE
	  && TYPE_PRECISION (type) != TYPE_PRECISION (otype)
	  /* Don't warn about converting 0 to pointer,
	     provided the 0 was explicit--not cast or made by folding.  */
	  && !(TREE_CODE (value) == INTEGER_CST && integer_zerop (value)))
	warning ("cast to pointer from integer of different size");
#endif

      value = convert_force (type, value);
    }
  if (value == expr)
    /* Always produce some operator for an explicit cast,
       so we can tell (for -pedantic) that the cast is no lvalue.  */
    {
      tree nvalue = build1 (NOP_EXPR, type, value);
      TREE_CONSTANT (nvalue) = TREE_CONSTANT (value);
      return nvalue;
    }

  return value;
}

/* Build an assignment expression of lvalue LHS from value RHS.

   In C++, if the left hand side of the assignment is a REFERENCE_TYPE,
   that reference becomes deferenced down to it base type. */

/* Return a reference to the BASE_INDEX part of EXPR.  TYPE is
   the type to which BASE_INDEX applies.  */
static tree
get_base_ref (type, base_index, expr)
     tree type;
     int base_index;
     tree expr;
{
  tree binfos = TYPE_BINFO_BASETYPES (type);
  tree base_binfo = TREE_VEC_ELT (binfos, base_index);
  tree ref;

  if (TREE_CODE (expr) == ARRAY_REF
      || ! BINFO_OFFSET_ZEROP (base_binfo)
      || TREE_VIA_VIRTUAL (base_binfo)
      || TYPE_MODE (type) != TYPE_MODE (BINFO_TYPE (base_binfo)))
    {
      tree addr = build_unary_op (ADDR_EXPR, expr, 0);
      ref = build_indirect_ref (convert_pointer_to (base_binfo, addr),
				NULL_PTR);
    }
  else
    {
      ref = copy_node (expr);
      TREE_TYPE (ref) = BINFO_TYPE (base_binfo);
    }
  return ref;
}

/* Build an assignment expression of lvalue LHS from value RHS.
   MODIFYCODE is the code for a binary operator that we use
   to combine the old value of LHS with RHS to get the new value.
   Or else MODIFYCODE is NOP_EXPR meaning do a simple assignment.

   C++: If MODIFYCODE is INIT_EXPR, then leave references unbashed.

   `build_modify_expr_1' implements recursive part of memberwise
   assignment operation.  */
static tree
build_modify_expr_1 (lhs, modifycode, rhs, basetype_path)
     tree lhs, rhs;
     enum tree_code modifycode;
     tree basetype_path;
{
  register tree result;
  tree newrhs = rhs;
  tree lhstype = TREE_TYPE (lhs);
  tree olhstype = lhstype;

  /* Avoid duplicate error messages from operands that had errors.  */
  if (TREE_CODE (lhs) == ERROR_MARK || TREE_CODE (rhs) == ERROR_MARK)
    return error_mark_node;

  /* If a binary op has been requested, combine the old LHS value with the RHS
     producing the value we should actually store into the LHS.  */

  if (modifycode == INIT_EXPR)
    ;
  else if (modifycode == NOP_EXPR)
    {
      /* must deal with overloading of `operator=' here.  */
      if (TREE_CODE (lhstype) == REFERENCE_TYPE)
	lhstype = TREE_TYPE (lhstype);
      else
	lhstype = olhstype;
    }
  else
    {
      lhs = stabilize_reference (lhs);
      newrhs = build_binary_op (modifycode, lhs, rhs, 1);
      modifycode = NOP_EXPR;
    }

  /* If storing into a structure or union member,
     it has probably been given type `int'.
     Compute the type that would go with
     the actual amount of storage the member occupies.  */

  if (TREE_CODE (lhs) == COMPONENT_REF
      && (TREE_CODE (lhstype) == INTEGER_TYPE
	  || TREE_CODE (lhstype) == REAL_TYPE
	  || TREE_CODE (lhstype) == ENUMERAL_TYPE))
    lhstype = TREE_TYPE (get_unwidened (lhs, 0));

  /* C++: The semantics of C++ differ from those of C when an
     assignment of an aggregate is desired.  Assignment in C++ is
     now defined as memberwise assignment of non-static members
     and base class objects.  This rule applies recursively
     until a member of a built-in type is found.

     Also, we cannot do a bit-wise copy of aggregates which
     contain virtual function table pointers.  Those
     pointer values must be preserved through the copy.
     However, this is handled in expand_expr, and not here.
     This is because much better code can be generated at
     that stage than this one.  */
  if (TREE_CODE (lhstype) == RECORD_TYPE
      && TYPE_LANG_SPECIFIC (lhstype)
      && TYPE_MAIN_VARIANT (lhstype) == TYPE_MAIN_VARIANT (TREE_TYPE (newrhs)))
    {
      register tree elt;
      int i;

      /* Perform operation on object.  */
      if (modifycode == INIT_EXPR && TYPE_HAS_INIT_REF (lhstype))
	{
	  result = build_method_call (lhs, constructor_name (lhstype),
				      build_tree_list (NULL_TREE, rhs),
				      basetype_path, LOOKUP_NORMAL);
	  return build_indirect_ref (result, NULL_PTR);
	}
      else if (modifycode == NOP_EXPR)
	{
	  /* `operator=' is not an inheritable operator.  */
	  if (TYPE_LANG_SPECIFIC (lhstype) && TYPE_HAS_ASSIGNMENT (lhstype))
	    {
	      result = build_opfncall (MODIFY_EXPR, LOOKUP_NORMAL,
				       lhs, rhs, make_node (NOP_EXPR));
	      if (result == NULL_TREE)
		return error_mark_node;
	      return result;
	    }
	}

      if (TYPE_USES_VIRTUAL_BASECLASSES (lhstype)
	  || (modifycode == NOP_EXPR && TYPE_GETS_ASSIGNMENT (lhstype))
	  || (modifycode == INIT_EXPR && TYPE_GETS_INIT_REF (lhstype)))
	{
	  tree binfos = BINFO_BASETYPES (TYPE_BINFO (lhstype));
	  result = NULL_TREE;

	  if (binfos != NULL_TREE)
	    /* Perform operation on each member, depth-first, left-right.  */
	    for (i = 0; i <= TREE_VEC_LENGTH (binfos)-1; i++)
	      {
		tree base_binfo = TREE_VEC_ELT (binfos, i);
		tree base_lhs, base_rhs;
		tree new_result;

		/* Assignments from virtual baseclasses handled elsewhere.  */
		if (TREE_VIA_VIRTUAL (base_binfo))
		  continue;

		base_lhs = get_base_ref (lhstype, i, lhs);
		base_rhs = get_base_ref (lhstype, i, newrhs);

		BINFO_INHERITANCE_CHAIN (base_binfo) = basetype_path;
		new_result
		  = build_modify_expr_1 (base_lhs, modifycode, base_rhs,
					 base_binfo);

		/* We either get back a compound stmt, or a simple one.  */
		if (new_result && TREE_CODE (new_result) == TREE_LIST)
		  new_result = build_compound_expr (new_result);
		result = tree_cons (NULL_TREE, new_result, result);
	      }

	  for (elt = TYPE_FIELDS (lhstype); elt; elt = TREE_CHAIN (elt))
	    {
	      tree vbases = NULL_TREE;
	      tree elt_lhs, elt_rhs;

	      if (TREE_CODE (elt) != FIELD_DECL)
		continue;
	      if (DECL_NAME (elt)
		  && (VFIELD_NAME_P (DECL_NAME (elt))
		      || VBASE_NAME_P (DECL_NAME (elt))))
		continue;

	      if (IS_AGGR_TYPE (TREE_TYPE (elt))
		  && TYPE_LANG_SPECIFIC (TREE_TYPE (elt)))
		vbases = CLASSTYPE_VBASECLASSES (TREE_TYPE (elt));

	      elt_lhs = build (COMPONENT_REF, TREE_TYPE (elt), lhs, elt);
	      elt_rhs = build (COMPONENT_REF, TREE_TYPE (elt), newrhs, elt);
	      /* It is not always safe to go through `build_modify_expr_1'
		 when performing element-wise copying.  This is because
		 an element may be of ARRAY_TYPE, which will not
		 be properly copied as a naked element.  */
	      if (TREE_CODE (TREE_TYPE (elt)) == RECORD_TYPE
		  && TYPE_LANG_SPECIFIC (TREE_TYPE (elt)))
		basetype_path = TYPE_BINFO (TREE_TYPE (elt));

	      while (vbases)
		{
		  tree elt_lhs_addr = build_unary_op (ADDR_EXPR, elt_lhs, 0);
		  tree elt_rhs_addr = build_unary_op (ADDR_EXPR, elt_rhs, 0);

		  elt_lhs_addr = convert_pointer_to (vbases, elt_lhs_addr);
		  elt_rhs_addr = convert_pointer_to (vbases, elt_rhs_addr);
		  result
		    = tree_cons (NULL_TREE,
				 build_modify_expr_1
				 (build_indirect_ref (elt_lhs_addr, NULL_PTR),
				  modifycode,
				  build_indirect_ref (elt_rhs_addr, NULL_PTR),
				  basetype_path),
				 result);
		  if (TREE_VALUE (result) == error_mark_node)
		    return error_mark_node;
		  vbases = TREE_CHAIN (vbases);
		}
	      elt_lhs = build_modify_expr_1 (elt_lhs, modifycode, elt_rhs,
					     basetype_path);
	      result = tree_cons (NULL_TREE, elt_lhs, result);
	    }

	  if (result)
	    return build_compound_expr (result);
	  /* No fields to move.  */
	  return integer_zero_node;
	}
      else
	{
	  result = build (modifycode == NOP_EXPR ? MODIFY_EXPR : INIT_EXPR,
			  void_type_node, lhs, rhs);
	  TREE_SIDE_EFFECTS (result) = 1;
	  return result;
	}
    }

  result = build_modify_expr (lhs, modifycode, newrhs);
  /* ARRAY_TYPEs cannot be converted to anything meaningful,
     and leaving it there screws up `build_compound_expr' when
     it tries to defaultly convert everything.  */
  if (TREE_CODE (TREE_TYPE (result)) == ARRAY_TYPE)
    TREE_TYPE (result) = void_type_node;
  return result;
}

tree
build_modify_expr (lhs, modifycode, rhs)
     tree lhs;
     enum tree_code modifycode;
     tree rhs;
{
  register tree result;
  tree newrhs = rhs;
  tree lhstype = TREE_TYPE (lhs);
  tree olhstype = lhstype;

  /* Types that aren't fully specified cannot be used in assignments.  */
  lhs = require_complete_type (lhs);

  /* Avoid duplicate error messages from operands that had errors.  */
  if (TREE_CODE (lhs) == ERROR_MARK || TREE_CODE (rhs) == ERROR_MARK)
    return error_mark_node;

  /* build_c_cast puts on a NOP_EXPR to make the result not an lvalue.
     Strip such NOP_EXPRs, since RHS is being used in non-lvalue context.  */
  if (TREE_CODE (rhs) == NOP_EXPR
      && TREE_TYPE (rhs) == TREE_TYPE (TREE_OPERAND (rhs, 0)))
    rhs = TREE_OPERAND (rhs, 0);

  /* Decide early if we are going to protect RHS from GC
     before assigning it to LHS.  */
  if (type_needs_gc_entry (TREE_TYPE (rhs))
      && ! value_safe_from_gc (lhs, rhs))
    rhs = protect_value_from_gc (lhs, rhs);

  newrhs = rhs;

  /* Handle control structure constructs used as "lvalues".  */

  switch (TREE_CODE (lhs))
    {
      /* Handle --foo = 5; as these are valid constructs in C++ */
    case PREDECREMENT_EXPR:
    case PREINCREMENT_EXPR:
      if (TREE_SIDE_EFFECTS (TREE_OPERAND (lhs, 0)))
	lhs = build (TREE_CODE (lhs), TREE_TYPE (lhs),
		     stabilize_reference (TREE_OPERAND (lhs, 0)));
      return build (COMPOUND_EXPR, lhstype,
		    lhs,
		    build_modify_expr (TREE_OPERAND (lhs, 0),
				       modifycode, rhs));

      /* Handle (a, b) used as an "lvalue".  */
    case COMPOUND_EXPR:
      pedantic_lvalue_warning (COMPOUND_EXPR);
      return build (COMPOUND_EXPR, lhstype,
		    TREE_OPERAND (lhs, 0),
		    build_modify_expr (TREE_OPERAND (lhs, 1),
				       modifycode, rhs));

      /* Handle (a ? b : c) used as an "lvalue".  */
    case COND_EXPR:
      pedantic_lvalue_warning (COND_EXPR);
      rhs = save_expr (rhs);
      {
	/* Produce (a ? (b = rhs) : (c = rhs))
	   except that the RHS goes through a save-expr
	   so the code to compute it is only emitted once.  */
	tree cond
	  = build_conditional_expr (TREE_OPERAND (lhs, 0),
				    build_modify_expr (TREE_OPERAND (lhs, 1),
						       modifycode, rhs),
				    build_modify_expr (TREE_OPERAND (lhs, 2),
						       modifycode, rhs));
	/* Make sure the code to compute the rhs comes out
	   before the split.  */
	return build (COMPOUND_EXPR, TREE_TYPE (lhs),
		      /* Case to void to suppress warning
			 from warn_if_unused_value.  */
		      convert (void_type_node, rhs), cond);
      }
    }

  /* If a binary op has been requested, combine the old LHS value with the RHS
     producing the value we should actually store into the LHS.  */

  if (modifycode == INIT_EXPR)
    ;
  else if (modifycode == NOP_EXPR)
    {
      /* must deal with overloading of `operator=' here.  */
      if (TREE_CODE (lhstype) == REFERENCE_TYPE)
	lhstype = TREE_TYPE (lhstype);
#if 1
      /* `operator=' is not an inheritable operator.  */
      if (TYPE_LANG_SPECIFIC (lhstype) && TYPE_HAS_ASSIGNMENT (lhstype))
	{
	  result = build_opfncall (MODIFY_EXPR, LOOKUP_NORMAL,
				   lhs, rhs, make_node (NOP_EXPR));
	  if (result == NULL_TREE)
	    return error_mark_node;
	  return result;
	}
#else
      /* Treat `operator=' as an inheritable operator.  */
      if (TYPE_LANG_SPECIFIC (lhstype) && TYPE_GETS_ASSIGNMENT (lhstype))
	{
	  tree orig_lhstype = lhstype;
	  while (! TYPE_HAS_ASSIGNMENT (lhstype))
	    {
	      int i, n_baseclasses = CLASSTYPE_N_BASECLASSES (lhstype);
	      tree basetype = NULL_TREE;
	      for (i = 0; i < n_baseclasses; i++)
		if (TYPE_GETS_ASSIGNMENT (TYPE_BINFO_BASETYPE (lhstype, i)))
		  {
		    if (basetype != NULL_TREE)
		      {
			message_2_types (error, "base classes `%s' and `%s' both have operator ='",
					 basetype,
					 TYPE_BINFO_BASETYPE (lhstype, i));
			return error_mark_node;
		      }
		    basetype = TYPE_BINFO_BASETYPE (lhstype, i);
		  }
	      lhstype = basetype;
	    }
	  if (orig_lhstype != lhstype)
	    {
	      lhs = build_indirect_ref (convert_pointer_to (lhstype,
							    build_unary_op (ADDR_EXPR, lhs, 0)), NULL);
	      if (lhs == error_mark_node)
		{
		  error_with_aggr_type (lhstype, "conversion to private basetype `%s'");
		  return error_mark_node;
		}
	    }
	  result = build_opfncall (MODIFY_EXPR, LOOKUP_NORMAL,
				   lhs, rhs, make_node (NOP_EXPR));
	  if (result == NULL_TREE)
	    return error_mark_node;
	  return result;
	}
#endif
      lhstype = olhstype;
    }
  else if (PROMOTES_TO_AGGR_TYPE (lhstype, REFERENCE_TYPE))
    {
      /* This case must convert to some sort of lvalue that
	 can participate in an op= operation.  */
      tree lhs_tmp = lhs;
      tree rhs_tmp = rhs;
      if (build_default_binary_type_conversion (modifycode, &lhs_tmp, &rhs_tmp))
	{
	  lhs = stabilize_reference (lhs_tmp);
	  /* Forget is was ever anything else.  */
	  olhstype = lhstype = TREE_TYPE (lhs);
	  newrhs = build_binary_op (modifycode, lhs, rhs_tmp, 1);
	}
      else
	return error_mark_node;
    }
  else
    {
      lhs = stabilize_reference (lhs);
      newrhs = build_binary_op (modifycode, lhs, rhs, 1);
    }

  /* Handle a cast used as an "lvalue".
     We have already performed any binary operator using the value as cast.
     Now convert the result to the cast type of the lhs,
     and then true type of the lhs and store it there;
     then convert result back to the cast type to be the value
     of the assignment.  */

  switch (TREE_CODE (lhs))
    {
    case NOP_EXPR:
    case CONVERT_EXPR:
    case FLOAT_EXPR:
    case FIX_TRUNC_EXPR:
    case FIX_FLOOR_EXPR:
    case FIX_ROUND_EXPR:
    case FIX_CEIL_EXPR:
      if (TREE_CODE (TREE_TYPE (newrhs)) == ARRAY_TYPE
	  || TREE_CODE (TREE_TYPE (newrhs)) == FUNCTION_TYPE
	  || TREE_CODE (TREE_TYPE (newrhs)) == METHOD_TYPE
	  || TREE_CODE (TREE_TYPE (newrhs)) == OFFSET_TYPE)
	newrhs = default_conversion (newrhs);
      {
	tree inner_lhs = TREE_OPERAND (lhs, 0);
	tree result;
	result = build_modify_expr (inner_lhs, NOP_EXPR,
				    convert (TREE_TYPE (inner_lhs),
					     convert (lhstype, newrhs)));
	return convert (TREE_TYPE (lhs), result);
      }
    }

  if (TREE_CODE (lhs) == OFFSET_REF)
    if (TREE_OPERAND (lhs, 0) == NULL_TREE)
      {
	/* Static class member?  */
	tree member = TREE_OPERAND (lhs, 1);
	if (TREE_CODE (member) == VAR_DECL)
	  lhs = member;
	else
	  {
	    compiler_error ("invalid static class member");
	    return error_mark_node;
	  }
      }
    else
      lhs = resolve_offset_ref (lhs);

  /* Now we have handled acceptable kinds of LHS that are not truly lvalues.
     Reject anything strange now.  */

  if (!lvalue_or_else (lhs, "assignment"))
    return error_mark_node;

  GNU_xref_assign (lhs);

  /* Warn about storing in something that is `const'.  */
  /* For C++, don't warn if this is initialization.  */
  if (modifycode != INIT_EXPR
      && (TREE_READONLY (lhs) || TYPE_READONLY (lhstype)
	  || ((TREE_CODE (lhstype) == RECORD_TYPE
	       || TREE_CODE (lhstype) == UNION_TYPE)
	      && C_TYPE_FIELDS_READONLY (lhstype))
	  || (TREE_CODE (lhstype) == REFERENCE_TYPE
	      && TYPE_READONLY (TREE_TYPE (lhstype)))))
    readonly_error (lhs, "assignment", 0);

  /* If storing into a structure or union member,
     it has probably been given type `int'.
     Compute the type that would go with
     the actual amount of storage the member occupies.  */

  if (TREE_CODE (lhs) == COMPONENT_REF
      && (TREE_CODE (lhstype) == INTEGER_TYPE
	  || TREE_CODE (lhstype) == REAL_TYPE
	  || TREE_CODE (lhstype) == ENUMERAL_TYPE))
    lhstype = TREE_TYPE (get_unwidened (lhs, 0));

  /* check to see if there is an assignment to `this' */
  if (lhs == current_class_decl)
    {
      if (flag_this_is_variable > 0
	  && DECL_NAME (current_function_decl) != NULL_TREE
	  && current_class_name != DECL_NAME (current_function_decl))
	warning ("assignment to `this' not in constructor or destructor");
      current_function_just_assigned_this = 1;
    }

  /* The TREE_TYPE of RHS may be TYPE_UNKNOWN.  This can happen
     when the type of RHS is not yet known, i.e. its type
     is inherited from LHS.  */
  rhs = require_instantiated_type (lhstype, newrhs, error_mark_node);
  if (rhs == error_mark_node)
    return error_mark_node;
  newrhs = rhs;

  if (modifycode != INIT_EXPR)
    {
      /* Make modifycode now either a NOP_EXPR or an INIT_EXPR.  */
      modifycode = NOP_EXPR;
      /* Reference-bashing */
      if (TREE_CODE (lhstype) == REFERENCE_TYPE)
	{
	  tree tmp = convert_from_reference (lhs);
	  lhstype = TREE_TYPE (tmp);
	  if (TYPE_SIZE (lhstype) == 0)
	    {
	      incomplete_type_error (lhs, lhstype);
	      return error_mark_node;
	    }
	  lhs = tmp;
	  olhstype = lhstype;
	}
      if (TREE_CODE (TREE_TYPE (newrhs)) == REFERENCE_TYPE)
	{
	  tree tmp = convert_from_reference (newrhs);
	  if (TYPE_SIZE (TREE_TYPE (tmp)) == 0)
	    {
	      incomplete_type_error (newrhs, TREE_TYPE (tmp));
	      return error_mark_node;
	    }
	  newrhs = tmp;
	}
    }

  if (TREE_SIDE_EFFECTS (lhs))
    lhs = stabilize_reference (lhs);
  if (TREE_SIDE_EFFECTS (newrhs))
    newrhs = stabilize_reference (newrhs);

  /* C++: The semantics of C++ differ from those of C when an
     assignment of an aggregate is desired.  Assignment in C++ is
     now defined as memberwise assignment of non-static members
     and base class objects.  This rule applies recursively
     until a member of a built-in type is found.

     Also, we cannot do a bit-wise copy of aggregates which
     contain virtual function table pointers.  Those
     pointer values must be preserved through the copy.
     However, this is handled in expand_expr, and not here.
     This is because much better code can be generated at
     that stage than this one.  */
  if (TREE_CODE (lhstype) == RECORD_TYPE
      && (TYPE_USES_VIRTUAL_BASECLASSES (lhstype)
	  || (modifycode != INIT_EXPR && TYPE_GETS_ASSIGNMENT (lhstype))
	  || (modifycode == INIT_EXPR && TYPE_GETS_INIT_REF (lhstype)))
      && (TYPE_MAIN_VARIANT (lhstype) == TYPE_MAIN_VARIANT (TREE_TYPE (newrhs))
	  || (TREE_CODE (TREE_TYPE (newrhs)) == RECORD_TYPE
	      && UNIQUELY_DERIVED_FROM_P (lhstype, TREE_TYPE (newrhs)))))
    {
      tree vbases = CLASSTYPE_VBASECLASSES (lhstype);
      tree lhs_addr = build_unary_op (ADDR_EXPR, lhs, 0);
      tree rhs_addr;

      /* Memberwise assignment would cause NEWRHS to be
	 evaluated for every member that gets assigned.
	 By wrapping side-effecting exprs in a SAVE_EXPR,
	 NEWRHS will only be evaluated once.  */
      if (IS_AGGR_TYPE (TREE_TYPE (newrhs))
	  && TREE_SIDE_EFFECTS (newrhs)
	  /* This are things we don't have to save.  */
	  && TREE_CODE (newrhs) != TARGET_EXPR
	  && TREE_CODE (newrhs) != WITH_CLEANUP_EXPR)
	/* Call `break_out_cleanups' on NEWRHS in case there are cleanups.
	   If NEWRHS is a CALL_EXPR that needs a cleanup, failure to do so
	   will result in expand_expr expanding the call without knowing
	   that it should run the cleanup.  */
	newrhs = save_expr (break_out_cleanups (newrhs));

      rhs_addr = build_unary_op (ADDR_EXPR, newrhs, 0);
      result = NULL_TREE;

      if (! comptypes (TREE_TYPE (lhs_addr), TREE_TYPE (rhs_addr), 1))
	rhs_addr = convert_pointer_to (TREE_TYPE (TREE_TYPE (lhs_addr)), rhs_addr);
      /* Once we have our hands on an address, we must change NEWRHS
	 to work from there.  Otherwise we can get multiple evaluations
	 of NEWRHS.  */
      if (TREE_CODE (newrhs) != SAVE_EXPR)
	newrhs = build_indirect_ref (rhs_addr, NULL);

      while (vbases)
	{
	  tree elt_lhs = convert_pointer_to (vbases, lhs_addr);
	  tree elt_rhs = convert_pointer_to (vbases, rhs_addr);
	  result
	    = tree_cons (NULL_TREE,
			 build_modify_expr_1 (build_indirect_ref (elt_lhs, NULL),
					      modifycode,
					      build_indirect_ref (elt_rhs, NULL),
					      TYPE_BINFO (lhstype)),
			 result);
	  if (TREE_VALUE (result) == error_mark_node)
	    return error_mark_node;
	  vbases = TREE_CHAIN (vbases);
	}
      result = tree_cons (NULL_TREE,
			  build_modify_expr_1 (lhs,
					       modifycode,
					       newrhs,
					       TYPE_BINFO (lhstype)),
			  result);
      return build_compound_expr (result);
    }

  /* It is now illegal to assign unions which contain members that
     have non-default assignment operators.  */
  if (! flag_traditional && TREE_CODE (lhstype) == UNION_TYPE)
    {
      if (modifycode == INIT_EXPR && TYPE_GETS_INIT_REF (lhstype))
	{
	  error ("invalid initialization of union containing members with X(X&) constructor");
	  return error_mark_node;
	}
      else if (modifycode == NOP_EXPR && TYPE_GETS_ASSIGNMENT (lhstype))
	{
	  error ("invalid assignment of union containing members with non-default operator=");
	  return error_mark_node;
	}
    }

  /* If storing in a field that is in actuality a short or narrower than one,
     we must store in the field in its actual type.  */

  if (lhstype != TREE_TYPE (lhs))
    {
      lhs = copy_node (lhs);
      TREE_TYPE (lhs) = lhstype;
    }

  /* Convert new value to destination type.  */

  if (TREE_CODE (lhstype) == ARRAY_TYPE)
    {
      /* Have to wrap this in RTL_EXPR for two cases:
	 in base or member initialization and if we
	 are a branch of a ?: operator.  Since we
	 can't easily know the latter, just do it always.  */

      result = make_node (RTL_EXPR);

      TREE_TYPE (result) = void_type_node;
      do_pending_stack_adjust ();
      start_sequence ();

      /* As a matter of principle, `start_sequence' should do this.  */
      emit_note (0, -1);

      expand_vec_init (lhs, lhs, array_type_nelts (lhstype), newrhs,
		       1 + (modifycode != INIT_EXPR));

      do_pending_stack_adjust ();

      TREE_SIDE_EFFECTS (result) = 1;
      RTL_EXPR_SEQUENCE (result) = get_insns ();
      RTL_EXPR_RTL (result) = const0_rtx;
      end_sequence ();
      return result;
    }

  if (modifycode == INIT_EXPR)
    {
      newrhs = convert_for_initialization (lhs, lhstype, newrhs, LOOKUP_NORMAL,
					   "assignment", NULL_TREE, 0);
      if (lhs == DECL_RESULT (current_function_decl))
	{
	  if (DECL_INITIAL (lhs))
	    warning ("return value from function receives multiple initializations");
	  DECL_INITIAL (lhs) = newrhs;
	}
    }
  else
    {
      if (IS_AGGR_TYPE (lhstype))
	{
	  if (TYPE_GETS_ASSIGNMENT (lhstype)
	      && ! TYPE_HAS_ASSIGNMENT (lhstype))
	    {
	      error_with_aggr_type (lhstype, "assignment not defined for type `%s'");
	      return error_mark_node;
	    }
	  if (result = build_opfncall (MODIFY_EXPR,
				       LOOKUP_NORMAL, lhs, newrhs,
				       make_node (NOP_EXPR)))
	    return result;
	}
      newrhs = convert_for_assignment (lhstype, newrhs, "assignment",
				       NULL_TREE, 0);
      if (flag_elide_constructors == 0
	  && TREE_CODE (newrhs) == CALL_EXPR
	  && TREE_ADDRESSABLE (lhstype))
	{
	  /* Can't initialized directly from a CALL_EXPR, since
	     we don't know about what doesn't alias what.  */

	  tree temp = get_temp_name (lhstype, 0);
	  newrhs = build (COMPOUND_EXPR, lhstype,
			  build_modify_expr (temp, INIT_EXPR, newrhs),
			  temp);
	}
    }

  if (TREE_CODE (newrhs) == ERROR_MARK)
    return error_mark_node;

  if (TREE_CODE (newrhs) == COND_EXPR)
    {
      tree lhs1;
      tree cond = TREE_OPERAND (newrhs, 0);

      if (TREE_SIDE_EFFECTS (lhs))
	cond = build_compound_expr (tree_cons
				    (NULL_TREE, lhs,
				     build_tree_list (NULL_TREE, cond)));

      /* Cannot have two identical lhs on this one tree (result) as preexpand
	 calls will rip them out and fill in RTL for them, but when the
	 rtl is generated, the calls will only be in the first side of the
	 condition, not on both, or before the conditional jump! (mrs) */
      lhs1 = break_out_calls (lhs);

      if (lhs == lhs1)
	/* If there's no change, the COND_EXPR behaves like any other rhs.  */
	result = build (modifycode == NOP_EXPR ? MODIFY_EXPR : INIT_EXPR,
			lhstype, lhs, newrhs);
      else
	{
	  tree result_type = TREE_TYPE (newrhs);
	  /* We have to convert each arm to the proper type because the
	     types may have been munged by constant folding.  */
	  result
	    = build (COND_EXPR, result_type, cond,
		     build_modify_expr (lhs, modifycode,
					convert (result_type,
						 TREE_OPERAND (newrhs, 1))),
		     build_modify_expr (lhs1, modifycode,
					convert (result_type,
						 TREE_OPERAND (newrhs, 2))));
	}
    }
  else if (modifycode != INIT_EXPR && TREE_CODE (newrhs) == WITH_CLEANUP_EXPR)
    {
      tree cleanup = TREE_OPERAND (newrhs, 2);
      tree slot;

      /* Finish up by running cleanups and having the "value" of the lhs.  */
      tree exprlist = tree_cons (NULL_TREE, cleanup,
				 build_tree_list (NULL_TREE, lhs));
      newrhs = TREE_OPERAND (newrhs, 0);
      if (TREE_CODE (newrhs) == TARGET_EXPR)
	  slot = TREE_OPERAND (newrhs, 0);
      else if (TREE_CODE (newrhs) == ADDR_EXPR)
	{
	  /* Bad but legal.  */
	  slot = newrhs;
	  warning ("address taken of temporary object");
	}
      else
	my_friendly_abort (118);

      /* Copy the value computed in SLOT into LHS.  */
      exprlist = tree_cons (NULL_TREE,
			    build_modify_expr (lhs, modifycode, slot),
			    exprlist);
      /* Evaluate the expression that needs CLEANUP.  This will
	 compute the value into SLOT.  */
      exprlist = tree_cons (NULL_TREE, newrhs, exprlist);
      result = convert (lhstype, build_compound_expr (exprlist));
    }
  else
    result = build (modifycode == NOP_EXPR ? MODIFY_EXPR : INIT_EXPR,
		    lhstype, lhs, newrhs);
  TREE_SIDE_EFFECTS (result) = 1;

  /* If we got the LHS in a different type for storing in,
     convert the result back to the nominal type of LHS
     so that the value we return always has the same type
     as the LHS argument.  */

  if (olhstype == TREE_TYPE (result))
    return result;
  return convert_for_assignment (olhstype, result, "assignment",
				 NULL_TREE, 0);
}


/* Return 0 if EXP is not a valid lvalue in this language
   even though `lvalue_or_else' would accept it.  */

int
language_lvalue_valid (exp)
     tree exp;
{
  return 1;
}

/* Convert value RHS to type TYPE as preparation for an assignment
   to an lvalue of type TYPE.
   The real work of conversion is done by `convert'.
   The purpose of this function is to generate error messages
   for assignments that are not allowed in C.
   ERRTYPE is a string to use in error messages:
   "assignment", "return", etc.

   C++: attempts to allow `convert' to find conversions involving
   implicit type conversion between aggregate and scalar types
   as per 8.5.6 of C++ manual.  Does not randomly dereference
   pointers to aggregates!  */

static tree
convert_for_assignment (type, rhs, errtype, fndecl, parmnum)
     tree type, rhs;
     char *errtype;
     tree fndecl;
     int parmnum;
{
  register enum tree_code codel = TREE_CODE (type);
  register tree rhstype;
  register enum tree_code coder = TREE_CODE (TREE_TYPE (rhs));

  if (coder == UNKNOWN_TYPE)
    rhs = instantiate_type (type, rhs, 1);

  if (coder == ERROR_MARK)
    return error_mark_node;

  if (codel == OFFSET_TYPE)
    {
      type = TREE_TYPE (type);
      codel = TREE_CODE (type);
    }

  /* Strip NON_LVALUE_EXPRs since we aren't using as an lvalue.  */
  if (TREE_CODE (rhs) == NON_LVALUE_EXPR)
    rhs = TREE_OPERAND (rhs, 0);

  if (rhs == error_mark_node)
    return error_mark_node;

  if (TREE_CODE (TREE_TYPE (rhs)) == OFFSET_TYPE)
    {
      rhs = resolve_offset_ref (rhs);
      if (rhs == error_mark_node)
	return error_mark_node;
      rhstype = TREE_TYPE (rhs);
      coder = TREE_CODE (rhstype);
    }

  if (TREE_CODE (TREE_TYPE (rhs)) == ARRAY_TYPE
      || TREE_CODE (TREE_TYPE (rhs)) == FUNCTION_TYPE
      || TREE_CODE (TREE_TYPE (rhs)) == METHOD_TYPE)
    rhs = default_conversion (rhs);
  else if (TREE_CODE (TREE_TYPE (rhs)) == REFERENCE_TYPE)
    rhs = convert_from_reference (rhs);

  rhstype = TREE_TYPE (rhs);
  coder = TREE_CODE (rhstype);

  /* This should no longer change types on us.  */
  if (TREE_CODE (rhs) == CONST_DECL)
    rhs = DECL_INITIAL (rhs);
  else if (TREE_READONLY_DECL_P (rhs))
    rhs = decl_constant_value (rhs);

  if (type == rhstype)
    return rhs;

  if (coder == VOID_TYPE)
    {
      error ("void value not ignored as it ought to be");
      return error_mark_node;
    }
  /* Arithmetic types all interconvert.  */
  if ((codel == INTEGER_TYPE || codel == REAL_TYPE)
       && (coder == INTEGER_TYPE || coder == REAL_TYPE))
    {
      /* But we should warn if assigning REAL_TYPE to INTEGER_TYPE.  */
      if (coder == REAL_TYPE && codel == INTEGER_TYPE)
	warn_for_assignment ("float or double assigned to integer data type",
			     "float or double used for argument %d of `%s'",
			     errtype, fndecl, parmnum, 0);
      /* And we should warn if assigning a negative value to
	 an unsigned variable.  */
      else if (TREE_UNSIGNED (type))
	{
	  if (TREE_CODE (rhs) == INTEGER_CST
	      && TREE_NEGATED_INT (rhs))
	    warn_for_assignment ("negative value assigned to unsigned quantity",
				 "negative value passed as argument %d of `%s'",
				 errtype, fndecl, parmnum, 0);
	  if (TREE_CONSTANT (rhs))
	    rhs = fold (rhs);
	}

      return convert (type, rhs);
    }
  /* Conversions involving enums.  */
  else if ((codel == ENUMERAL_TYPE
	    && (coder == ENUMERAL_TYPE || coder == INTEGER_TYPE || coder == REAL_TYPE))
	   || (coder == ENUMERAL_TYPE
	       && (codel == ENUMERAL_TYPE || codel == INTEGER_TYPE || codel == REAL_TYPE)))
    {
      extern int warn_enum_clash;

      if (TYPE_MAIN_VARIANT (type) == TYPE_MAIN_VARIANT (rhstype))
	return convert (type, rhs);
      if (warn_enum_clash)
	{
	  if (codel == ENUMERAL_TYPE && coder == ENUMERAL_TYPE)
	    message_2_types (warning, "conversion between incompatible enumeral types `%s' and `%s'",
			     type, rhstype);
	  else if (coder == REAL_TYPE)
	    warn_for_assignment ("float or double assigned to enumeral data type",
				 "float or double passed as enumeral data type for argument %d of `%s'",
				 errtype, fndecl, parmnum, pedantic);
	  else if (codel == REAL_TYPE)
	    warn_for_assignment ("enumeral value assigned to real data type",
				 "enumeral value passed as real data type for argument %d of `%s'",
				 errtype, fndecl, parmnum, pedantic);
	  else if (coder == INTEGER_TYPE)
	    warn_for_assignment ("assignment of integer to enumeral data type",
				 "passing integer as enumeral data type for argument %d of `%s'",
				 errtype, fndecl, parmnum, pedantic);
	}
      return convert (type, rhs);
    }
  /* Conversions among pointers */
  else if (codel == POINTER_TYPE && coder == POINTER_TYPE)
    {
      register tree ttl = TREE_TYPE (type);
      register tree ttr = TREE_TYPE (rhstype);

      /* If both pointers are of aggregate type, then we
	 can give better error messages, and save some work
	 as well.  */
      if (TREE_CODE (ttl) == RECORD_TYPE && TREE_CODE (ttr) == RECORD_TYPE)
	{
	  tree binfo;

	  if (TYPE_MAIN_VARIANT (ttl) == TYPE_MAIN_VARIANT (ttr)
	      || type == class_star_type_node
	      || rhstype == class_star_type_node)
	    binfo = TYPE_BINFO (ttl);
	  else
	    binfo = get_binfo (ttl, ttr, 1);

	  if (binfo == error_mark_node)
	    return error_mark_node;
	  if (binfo == 0)
	    return (tree) error_not_base_type (ttl, ttr);

	  if (! TYPE_READONLY (ttl) && TYPE_READONLY (ttr))
	    warn_for_assignment ("%s of non-`const *' pointer from `const *'",
				 "pointer to const given for argument %d of `%s'",
				 errtype, fndecl, parmnum, 0);
	  if (! TYPE_VOLATILE (ttl) && TYPE_VOLATILE (ttr))
	    warn_for_assignment ("%s of non-`volatile *' pointer from `volatile *'",
				 "pointer to volatile given for argument %d of `%s'",
				 errtype, fndecl, parmnum, 0);
	}

      /* Any non-function converts to a [const][volatile] void *
	 and vice versa; otherwise, targets must be the same.
	 Meanwhile, the lhs target must have all the qualifiers of the rhs.  */
      else if (TYPE_MAIN_VARIANT (ttl) == void_type_node
	       || TYPE_MAIN_VARIANT (ttr) == void_type_node
	       || comp_target_types (type, rhstype, 1))
	{
	  if (TYPE_MAIN_VARIANT (ttl) != void_type_node
	      && TYPE_MAIN_VARIANT (ttr) == void_type_node
	      && rhs != null_pointer_node)
	    pedwarn ("ANSI C++ forbids implicit conversion from `void *' in %s",
		     errtype);
	  else if (pedantic
	      && ((TYPE_MAIN_VARIANT (ttl) == void_type_node
		   && (TREE_CODE (ttr) == FUNCTION_TYPE
		       || TREE_CODE (ttr) == METHOD_TYPE))
		  ||
		  (TYPE_MAIN_VARIANT (ttr) == void_type_node
		   && (TREE_CODE (ttl) == FUNCTION_TYPE
		       || TREE_CODE (ttl) == METHOD_TYPE))))
	    warn_for_assignment ("%s between function pointer and `void *'",
				 "function pointer and `void *' incompatible; argument %d of `%s'",
				 errtype, fndecl, parmnum, flag_pedantic_errors);
	  /* Const and volatile mean something different for function types,
	     so the usual warnings are not appropriate.  */
	  else if ((TREE_CODE (ttr) != FUNCTION_TYPE && TREE_CODE (ttr) != METHOD_TYPE)
		   || (TREE_CODE (ttl) != FUNCTION_TYPE && TREE_CODE (ttl) != METHOD_TYPE))
	    {
	      if (TREE_CODE (ttl) == OFFSET_TYPE
		  && binfo_member (TYPE_OFFSET_BASETYPE (ttr),
				   CLASSTYPE_VBASECLASSES (TYPE_OFFSET_BASETYPE (ttl))))
		{
		  sorry ("%s between pointer to members converting across virtual baseclasses", errtype);
		  return error_mark_node;
		}
	      if (! TYPE_READONLY (ttl) && TYPE_READONLY (ttr))
		warn_for_assignment ("%s of non-`const *' pointer from `const *'",
				     "pointer to const given for argument %d of `%s'",
				     errtype, fndecl, parmnum, flag_pedantic_errors);
	      if (! TYPE_VOLATILE (ttl) && TYPE_VOLATILE (ttr))
		warn_for_assignment ("%s of non-`volatile *' pointer from `volatile *'",
				     "pointer to volatile given for argument %d of `%s'",
				     errtype, fndecl, parmnum, flag_pedantic_errors);
	    }
	}
      else if (TREE_CODE (ttr) == OFFSET_TYPE
	       && TREE_CODE (ttl) != OFFSET_TYPE)
	{
	  /* Normally, pointers to different type codes (other
	     than void) are not compatible, but we perform
	     some type instantiation if that resolves the
	     ambiguity of (X Y::*) and (X *).  */

	  if (current_class_decl)
	    {
	      if (TREE_CODE (rhs) == INTEGER_CST)
		{
		  rhs = build (PLUS_EXPR, build_pointer_type (TREE_TYPE (ttr)),
			       current_class_decl, rhs);
		  return convert_for_assignment (type, rhs,
						 errtype, fndecl, parmnum);
		}
	    }
	  if (TREE_CODE (ttl) == METHOD_TYPE)
	    error ("%s between pointer-to-method and pointer-to-member types",
		   errtype);
	  else
	    error ("%s between pointer and pointer-to-member types", errtype);
	  return error_mark_node;
	}
      else
	{
	  int const_parity = TYPE_READONLY (type) ^ TYPE_READONLY (rhstype);
	  int volatile_parity = TYPE_VOLATILE (type) ^ TYPE_VOLATILE (rhstype);
	  int unsigned_parity;
	  int nptrs = 0;

	  while (TREE_CODE (ttl) == POINTER_TYPE
		 && TREE_CODE (ttr) == POINTER_TYPE)
	    {
	      nptrs -= 1;
	      const_parity |= TYPE_READONLY (ttl) ^ TYPE_READONLY (ttr);
	      volatile_parity |= TYPE_VOLATILE (ttl) ^ TYPE_VOLATILE (ttr);
	      ttl = TREE_TYPE (ttl);
	      ttr = TREE_TYPE (ttr);
	    }
	  unsigned_parity = TREE_UNSIGNED (ttl) - TREE_UNSIGNED (ttr);
	  if (unsigned_parity)
	    if (TREE_UNSIGNED (ttl))
	      ttr = unsigned_type (ttr);
	    else
	      ttl = unsigned_type (ttl);

	  if (comp_target_types (ttl, ttr, nptrs))
	    {
	      if (const_parity)
		warn_for_assignment ("%s of non-`const *' pointer from `const *'",
				     "pointer to const given for argument %d of `%s'",
				     errtype, fndecl, parmnum, 0);
	      if (volatile_parity)
		warn_for_assignment ("%s of non-`volatile *' pointer from volatile *",
				     "pointer to volatile given for argument %d of `%s'",
				     errtype, fndecl, parmnum, 0);
	      if (unsigned_parity > 0)
		warn_for_assignment ("%s of unsigned pointer from signed pointer",
				     "passing signed pointer to unsigned pointer argument %d of `%s'",
				     errtype, fndecl, parmnum, flag_pedantic_errors);
	      else if (unsigned_parity < 0)
		warn_for_assignment ("%s of signed pointer from unsigned pointer",
				     "passing unsigned pointer to signed pointer argument %d of `%s'",
				     errtype, fndecl, parmnum, flag_pedantic_errors);

	      /* C++ is not so friendly about converting function and
		 member function pointers as C.  Emit warnings here.  */
	      if (TREE_CODE (ttl) == FUNCTION_TYPE
		  || TREE_CODE (ttl) == METHOD_TYPE)
		if (! comptypes (ttl, ttr, 0))
		  {
		    char *tmpbuf, *lhsbuf;
		    char *rhsbuf;
		    tree null_name = get_identifier ("");
		    tree lhs = build_decl (FUNCTION_DECL, null_name, ttl);
		    tree rhs = build_decl (FUNCTION_DECL, null_name, ttr);
		    tmpbuf = fndecl_as_string (0, lhs, 1);
		    lhsbuf = (char *) alloca (strlen (tmpbuf));
		    strcpy (lhsbuf, tmpbuf);
		    rhsbuf = fndecl_as_string (0, rhs, 1);
		    warning ("conflicting function types in %s:", errtype);
		    warning ("\t`%s' != `%s'", lhsbuf, rhsbuf);
		  }
	    }
	  else if (TREE_CODE (TREE_TYPE (rhs)) == METHOD_TYPE)
	    {
	      /* When does this happen?  */
	      my_friendly_abort (119);
	      /* Conversion of a pointer-to-member type to void *.  */
	      rhs = build_unary_op (ADDR_EXPR, rhs, 0);
	      TREE_TYPE (rhs) = type;
	      return rhs;
	    }
	  else if (TREE_CODE (TREE_TYPE (rhs)) == OFFSET_TYPE)
	    {
	      /* When does this happen?  */
	      my_friendly_abort (120);
	      /* Conversion of a pointer-to-member type to void *.  */
	      rhs = build_unary_op (ADDR_EXPR, rhs, 0);
	      TREE_TYPE (rhs) = type;
	      return rhs;
	    }
	  else
	    {
	      if (fndecl)
		error ("incompatible pointer types for argument %d of `%s'",
		       parmnum + 1, lang_printable_name (fndecl));
	      else
		error ("%s between incompatible pointer types", errtype);
	      return error_mark_node;
	    }
	}
      return convert (type, rhs);
    }
  else if (codel == POINTER_TYPE && coder == INTEGER_TYPE)
    {
      if (! integer_zerop (rhs))
	{
	  warn_for_assignment ("%s of pointer from integer lacks a cast",
			       "passing integer to pointer argument %d of `%s' lacks a cast",
			       errtype, fndecl, parmnum, flag_pedantic_errors);
	  return convert (type, rhs);
	}
      return null_pointer_node;
    }
  else if (codel == INTEGER_TYPE && coder == POINTER_TYPE)
    {
      warn_for_assignment ("%s of integer from pointer lacks a cast",
			   "passing pointer to integer argument %d of `%s' lacks a cast",
			   errtype, fndecl, parmnum, flag_pedantic_errors);
      return convert (type, rhs);
    }

  /* C++ */
  else if (codel == ERROR_MARK || coder == ERROR_MARK)
    return error_mark_node;

  /* This should no longer happen.  References are initialized via
     `convert_for_initialization'.  They should otherwise be
     bashed before coming here.  */
  else if (codel == REFERENCE_TYPE)
    /* Force an abort.  */
    my_friendly_assert (codel != REFERENCE_TYPE, 317);
  else if (TYPE_MAIN_VARIANT (type) == TYPE_MAIN_VARIANT (TREE_TYPE (rhs)))
    return build1 (NOP_EXPR, type, rhs);
  else if (TYPE_HAS_CONSTRUCTOR (type) || IS_AGGR_TYPE (TREE_TYPE (rhs)))
    return convert (type, rhs);

  error ("incompatible types in %s", errtype);
  return error_mark_node;
}

/* Print a warning using either ANON_MSG or NAMED_MSG.
   ANON_MSG is used if DECL and FUNCTION are 0; it gets one parameter, OPNAME.
   NAMED_MSG is used if DECL is non-0;
   it gets two parameters, the name of DECL and that of FUNCTION.
   FUNCTION_MSG is used if DECL is 0 and FUNCTION is non-0;
   it gets one parameter, the name FUNCTION.

   If SEVERE is non-0, the report an error instead of a warning.

   If FNDECL is nonzero, the message concerns an argument in a call
   to that function.  ARGNUM is the number of the argument, origin 0.  */

void
warn_for_assignment (anon_msg, arg_msg, opname, fndecl, argnum, severe)
     char *anon_msg;
     char *arg_msg;
     char *opname;
     tree fndecl;
     int argnum;
     int severe;
{
  if (fndecl)
    {
      if (argnum < 0)
	{
	  char *buf = (char *)alloca (strlen (arg_msg) + 1);
	  char *p;
	  strcpy (buf, arg_msg);
	  for (p = buf; *p; p++)
	    if (p[0] == '%' && p[1] == 'd')
	      {
		p[1] = 's';
		if (severe)
		  error (buf, "`this'", lang_printable_name (fndecl));
		else
		  warning (buf, "`this'", lang_printable_name (fndecl));
		break;
	      }
	}
      else if (severe)
	error (arg_msg, argnum + 1, lang_printable_name (fndecl));
      else
	warning (arg_msg, argnum + 1, lang_printable_name (fndecl));
    }
  else if (severe)
    error (anon_msg, opname);
  else
    warning (anon_msg, opname);
}

/* Convert RHS to be of type TYPE.  If EXP is non-zero,
   it is the target of the initialization.
   ERRTYPE is a string to use in error messages.

   Two major differences between the behavior of
   `convert_for_assignment' and `convert_for_initialization'
   are that references are bashed in the former, while
   copied in the latter, and aggregates are assigned in
   the former (operator=) while initialized in the
   latter (X(X&)).

   If using constructor make sure no conversion operator exists, if one does
   exist, an ambiguity exists.  */
tree
convert_for_initialization (exp, type, rhs, flags, errtype, fndecl, parmnum)
     tree exp, type, rhs;
     int flags;
     char *errtype;
     tree fndecl;
     int parmnum;
{
  register enum tree_code codel = TREE_CODE (type);
  register tree rhstype;
  register enum tree_code coder;

  /* build_c_cast puts on a NOP_EXPR to make the result not an lvalue.
     Strip such NOP_EXPRs, since RHS is used in non-lvalue context.  */
  if (TREE_CODE (rhs) == NOP_EXPR
      && TREE_TYPE (rhs) == TREE_TYPE (TREE_OPERAND (rhs, 0)))
    rhs = TREE_OPERAND (rhs, 0);

  if (rhs == error_mark_node
      || (TREE_CODE (rhs) == TREE_LIST && TREE_VALUE (rhs) == error_mark_node))
    return error_mark_node;

  if (TREE_CODE (TREE_TYPE (rhs)) == OFFSET_TYPE)
    {
      rhs = resolve_offset_ref (rhs);
      if (rhs == error_mark_node)
	return error_mark_node;
      rhstype = TREE_TYPE (rhs);
      coder = TREE_CODE (rhstype);
    }

  if ((TREE_CODE (TREE_TYPE (rhs)) == ARRAY_TYPE
       && TREE_CODE (type) != ARRAY_TYPE && TREE_CODE (type) != REFERENCE_TYPE)
      || TREE_CODE (TREE_TYPE (rhs)) == FUNCTION_TYPE
      || TREE_CODE (TREE_TYPE (rhs)) == METHOD_TYPE)
    rhs = default_conversion (rhs);

  rhstype = TREE_TYPE (rhs);
  coder = TREE_CODE (rhstype);

  if (coder == UNKNOWN_TYPE)
    {
      rhs = instantiate_type (type, rhs, 1);
      rhstype = TREE_TYPE (rhs);
      coder = TREE_CODE (rhstype);
    }

  if (coder == ERROR_MARK)
    return error_mark_node;

#if 0
  /* This is *not* the quick way out!  It is the way to disaster.  */
  if (type == rhstype)
    goto converted;
#endif

  /* We accept references to incomplete types, so we can
     return here before checking if RHS is of complete type.  */
     
  if (codel == REFERENCE_TYPE)
    return convert_to_reference ((exp ? exp : error_mark_node),
		 		 type, rhs, fndecl, parmnum, errtype,
				 0, flags);

  rhs = require_complete_type (rhs);
  if (rhs == error_mark_node)
    return error_mark_node;

  if (exp != 0) exp = require_complete_type (exp);
  if (exp == error_mark_node)
    return error_mark_node;

  if (TREE_CODE (rhstype) == REFERENCE_TYPE)
    rhstype = TREE_TYPE (rhstype);

  if (IS_AGGR_TYPE (type) && TYPE_NEEDS_CONSTRUCTOR (type))
    {
      if (TYPE_MAIN_VARIANT (type) == TYPE_MAIN_VARIANT (rhstype))
	{
	  /* This is sufficient to perform initialization.  No need, apparently,
	     to go through X(X&) to do first-cut initialization.  Return through
	     a TARGET_EXPR so that we get cleanups if it is used.  */
	  if (TREE_CODE (rhs) == CALL_EXPR)
	    {
	      rhs = build_cplus_new (type, rhs, 0);
	      return rhs;
	    }
	  /* Handle the case of default parameter initialization and
	     initialization of static variables.  */
	  else if (TREE_CODE (rhs) == INDIRECT_REF && TREE_HAS_CONSTRUCTOR (rhs))
	    {
	      my_friendly_assert (TREE_CODE (TREE_OPERAND (rhs, 0)) == CALL_EXPR, 318);
	      if (exp)
		{
		  my_friendly_assert (TREE_VALUE (TREE_OPERAND (TREE_OPERAND (rhs, 0), 1)) == NULL_TREE, 316);
		  TREE_VALUE (TREE_OPERAND (TREE_OPERAND (rhs, 0), 1))
		    = build_unary_op (ADDR_EXPR, exp, 0);
		}
	      else
		rhs = build_cplus_new (type, TREE_OPERAND (rhs, 0), 0);
	      return rhs;
	    }
	}
      if (TYPE_MAIN_VARIANT (type) == TYPE_MAIN_VARIANT (rhstype)
	  || (IS_AGGR_TYPE (rhstype) && UNIQUELY_DERIVED_FROM_P (type, rhstype)))
	{
	  if (TYPE_HAS_INIT_REF (type))
	    {
	      /* We have to check for template classes here, or else
		 build_method_call won't be able to see the constructor.  */
	      tree id = TYPE_IDENTIFIER (type);
	      tree name
		= IDENTIFIER_TEMPLATE (id) ? id : constructor_name (type);
	      tree init = build_method_call (exp, name,
					     build_tree_list (NULL_TREE, rhs),
					     NULL_TREE, LOOKUP_NORMAL);

	      if (init == error_mark_node)
		return error_mark_node;

	      if (exp == 0)
		{
		  exp = build_cplus_new (type, init, 0);
		  return exp;
		}

	      return build (COMPOUND_EXPR, type, init, exp);
	    }

#if 0
	  /* ??? The following warnings are turned off because
	     this is another place where the default X(X&) constructor
	     is implemented.  */
	  if (TYPE_HAS_ASSIGNMENT (type))
	    warning ("bitwise copy: `%s' defines operator=()",
		     TYPE_NAME_STRING (type));
	  else if (TYPE_GETS_ASSIGNMENT (type))
	    warning ("bitwise copy: `%s' has a member with operator=()",
		     TYPE_NAME_STRING (type));
#endif

	  if (TREE_CODE (TREE_TYPE (rhs)) == REFERENCE_TYPE)
	    rhs = convert_from_reference (rhs);
	  if (type != rhstype)
	    return build1 (NOP_EXPR, type, rhs);
	  return rhs;
	}

      return convert (type, rhs);
    }
#if 0
  /* ??? The following warnings are turned off because
     this is another place where the default X(X&) constructor
     is implemented.  */
  if (TYPE_LANG_SPECIFIC (type))
    {
      if (TYPE_HAS_ASSIGNMENT (type))
	warning ("bitwise copy: `%s' defines operator=()",
		 TYPE_NAME_STRING (type));
      else if (TYPE_GETS_ASSIGNMENT (type))
	warning ("bitwise copy: `%s' has a member with operator=()",
		 TYPE_NAME_STRING (type));
    }
#endif

  if (type == TREE_TYPE (rhs))
    {
      if (TREE_READONLY_DECL_P (rhs))
	rhs = decl_constant_value (rhs);
      return rhs;
    }

  return convert_for_assignment (type, rhs, errtype, fndecl, parmnum);
}

/* Expand an ASM statement with operands, handling output operands
   that are not variables or INDIRECT_REFS by transforming such
   cases into cases that expand_asm_operands can handle.

   Arguments are same as for expand_asm_operands.

   We don't do default conversions on all inputs, because it can screw
   up operands that are expected to be in memory.  */

void
c_expand_asm_operands (string, outputs, inputs, clobbers, vol, filename, line)
     tree string, outputs, inputs, clobbers;
     int vol;
     char *filename;
     int line;
{
  int noutputs = list_length (outputs);
  register int i;
  /* o[I] is the place that output number I should be written.  */
  register tree *o = (tree *) alloca (noutputs * sizeof (tree));
  register tree tail;

  /* Record the contents of OUTPUTS before it is modified.  */
  for (i = 0, tail = outputs; tail; tail = TREE_CHAIN (tail), i++)
    o[i] = TREE_VALUE (tail);

  /* Generate the ASM_OPERANDS insn;
     store into the TREE_VALUEs of OUTPUTS some trees for
     where the values were actually stored.  */
  expand_asm_operands (string, outputs, inputs, clobbers, vol, filename, line);

  /* Copy all the intermediate outputs into the specified outputs.  */
  for (i = 0, tail = outputs; tail; tail = TREE_CHAIN (tail), i++)
    {
      if (o[i] != TREE_VALUE (tail))
	{
	  expand_expr (build_modify_expr (o[i], NOP_EXPR, TREE_VALUE (tail)),
		       const0_rtx, VOIDmode, 0);
	  free_temp_slots ();
	}
      /* Detect modification of read-only values.
	 (Otherwise done by build_modify_expr.)  */
      else
	{
	  tree type = TREE_TYPE (o[i]);
	  if (TYPE_READONLY (type)
	      || ((TREE_CODE (type) == RECORD_TYPE
		   || TREE_CODE (type) == UNION_TYPE)
		  && C_TYPE_FIELDS_READONLY (type)))
	    readonly_error (o[i], "modification by `asm'", 1);
	}
    }

  /* Those MODIFY_EXPRs could do autoincrements.  */
  emit_queue ();
}

/* Expand a C `return' statement.
   RETVAL is the expression for what to return,
   or a null pointer for `return;' with no value.

   C++: upon seeing a `return', we must call destructors on all
   variables in scope which had constructors called on them.
   This means that if in a destructor, the base class destructors
   must be called before returning.

   The RETURN statement in C++ has initialization semantics.  */

void
c_expand_return (retval)
     tree retval;
{
  extern struct nesting *cond_stack, *loop_stack, *case_stack;
  extern tree dtor_label, ctor_label;
  tree result = DECL_RESULT (current_function_decl);
  tree valtype = TREE_TYPE (result);
  register int use_temp = 0;

  if (TREE_THIS_VOLATILE (current_function_decl))
    warning ("function declared `volatile' has a `return' statement");

  if (retval == error_mark_node)
    {
      current_function_returns_null = 1;
      return;
    }

  if (retval == NULL_TREE)
    {
      /* A non-named return value does not count.  */

      /* Can't just return from a destructor.  */
      if (dtor_label)
	{
	  expand_goto (dtor_label);
	  return;
	}

      if (DECL_CONSTRUCTOR_P (current_function_decl))
	retval = current_class_decl;
      else if (DECL_NAME (result) != 0 && TREE_CODE (valtype) != VOID_TYPE)
	retval = result;
      else
	{
	  current_function_returns_null = 1;
	  if (valtype != 0 && TREE_CODE (valtype) != VOID_TYPE)
	    {
	      if (DECL_NAME (DECL_RESULT (current_function_decl)) == 0)
		warning ("`return' with no value, in function returning non-void");
	    }

	  expand_null_return ();
	  return;
	}
    }
  else if (DECL_CONSTRUCTOR_P (current_function_decl)
	   && retval != current_class_decl)
    {
      error ("return from a constructor: use `this = ...' instead");
      retval = current_class_decl;
    }

  if (valtype == 0 || TREE_CODE (valtype) == VOID_TYPE)
    {
      current_function_returns_null = 1;
      if (TREE_CODE (TREE_TYPE (retval)) != VOID_TYPE)
	pedwarn ("ANSI C++ forbids `return' with a value, in function returning void");
      expand_return (retval);
    }
  /* Add some useful error checking for C++.  */
  else if (TREE_CODE (valtype) == REFERENCE_TYPE)
    {
      tree whats_returned;
      tree tmp_result = result;

      /* Don't initialize directly into a non-BLKmode retval, since that
	 could lose when being inlined by another caller.  (GCC can't
	 read the function return register in an inline function when
	 the return value is being ignored).  */
      if (result && TYPE_MODE (TREE_TYPE (tmp_result)) != BLKmode)
	tmp_result = 0;

      /* convert to reference now, so we can give error if we
	 return an reference to a non-lvalue.  */
      retval = convert_for_initialization (tmp_result, valtype, retval,
					   LOOKUP_NORMAL, "return",
					   NULL_TREE, 0);

      /* Sort through common things to see what it is
	 we are returning.  */
      whats_returned = retval;
      if (TREE_CODE (whats_returned) == COMPOUND_EXPR)
	{
	  whats_returned = TREE_OPERAND (whats_returned, 1);
	  if (TREE_CODE (whats_returned) == ADDR_EXPR)
	    whats_returned = TREE_OPERAND (whats_returned, 0);
	}
      if (TREE_CODE (whats_returned) == ADDR_EXPR)
	{
	  whats_returned = TREE_OPERAND (whats_returned, 0);
	  while (TREE_CODE (whats_returned) == NEW_EXPR
		 || TREE_CODE (whats_returned) == TARGET_EXPR
		 || TREE_CODE (whats_returned) == WITH_CLEANUP_EXPR)
	    /* Get the target.  */
	    whats_returned = TREE_OPERAND (whats_returned, 0);
	}

      if (TREE_CODE (whats_returned) == VAR_DECL && DECL_NAME (whats_returned))
	{
	  if (TEMP_NAME_P (DECL_NAME (whats_returned)))
	    warning ("reference to non-lvalue returned");
	  else if (! TREE_STATIC (whats_returned)
		   && IDENTIFIER_LOCAL_VALUE (DECL_NAME (whats_returned)))
	    warning_with_decl (whats_returned,
			       "reference to local variable `%s' returned");
	}
    }
  else if (TREE_CODE (retval) == ADDR_EXPR)
    {
      tree whats_returned = TREE_OPERAND (retval, 0);

      if (TREE_CODE (whats_returned) == TREE_LIST)
	whats_returned = TREE_VALUE (whats_returned);

      if (DECL_NAME (whats_returned)
	  && IDENTIFIER_LOCAL_VALUE (DECL_NAME (whats_returned))
	  && !TREE_STATIC (whats_returned))
	warning_with_decl (whats_returned,
			   "address of local variable `%s' returned");
    }
  
  /* Now deal with possible C++ hair:
     (1) Compute the return value.
     (2) If there are aggregate values with destructors which
     must be cleaned up, clean them (taking care
     not to clobber the return value).
     (3) If an X(X&) constructor is defined, the return
     value must be returned via that.  */

  if (retval == result
      /* Watch out for constructors, which "return" aggregates
	 via initialization, but which otherwise "return" a pointer.  */
      || DECL_CONSTRUCTOR_P (current_function_decl))
    {
      /* This is just an error--it's already been reported.  */
      if (TYPE_SIZE (valtype) == NULL_TREE)
	return;

      if (TYPE_MODE (valtype) != BLKmode
	  && any_pending_cleanups (1))
	{
	  retval = get_temp_regvar (valtype, retval);
	  use_temp = obey_regdecls;
	}
    }
  else if (IS_AGGR_TYPE (valtype) && TYPE_NEEDS_CONSTRUCTOR (valtype))
    {
      /* Throw away the cleanup that `build_functional_cast' gave us.  */
      if (TREE_CODE (retval) == WITH_CLEANUP_EXPR
	  && TREE_CODE (TREE_OPERAND (retval, 0)) == TARGET_EXPR)
	retval = TREE_OPERAND (retval, 0);
      expand_aggr_init (result, retval, 0);
      DECL_INITIAL (result) = NULL_TREE;
      retval = 0;
    }
  else
    {
      if (TYPE_MODE (valtype) == VOIDmode)
	{
	  if (TYPE_MODE (TREE_TYPE (result)) != VOIDmode
	      && warn_return_type)
	    warning ("return of void value in function returning non-void");
	  expand_expr_stmt (retval);
	  retval = 0;
	  result = 0;
	}
      else if (TYPE_MODE (valtype) != BLKmode
	       && any_pending_cleanups (1))
	{
	  retval = get_temp_regvar (valtype, retval);
	  use_temp = obey_regdecls;
	  result = 0;
	}
      else
	{
	  retval = convert_for_initialization (result, valtype, retval,
					       LOOKUP_NORMAL,
					       "return", NULL_TREE, 0);
	  DECL_INITIAL (result) = NULL_TREE;
	}
      if (retval == error_mark_node)
	return;
    }

  emit_queue ();

  if (retval != NULL_TREE
      && TREE_CODE_CLASS (TREE_CODE (retval)) == 'd'
      && cond_stack == 0 && loop_stack == 0 && case_stack == 0)
    current_function_return_value = retval;

  if (result)
    {
      /* Everything's great--RETVAL is in RESULT.  */
      if (original_result_rtx)
	store_expr (result, original_result_rtx, 0);
      else if (retval && retval != result)
	{
	  /* Clear this out so the later call to decl_function_context
	     won't end up bombing on us.  */
	  if (DECL_CONTEXT (result) == error_mark_node)
	    DECL_CONTEXT (result) = NULL_TREE;
	  /* Here is where we finally get RETVAL into RESULT.
	     `expand_return' does the magic of protecting
	     RESULT from cleanups.  */
	  retval = build (INIT_EXPR, TREE_TYPE (result), result, retval);
	  TREE_SIDE_EFFECTS (retval) = 1;
	  expand_return (retval);
	}
      else
	expand_return (result);

      use_variable (DECL_RTL (result));
      if (ctor_label  && TREE_CODE (ctor_label) != ERROR_MARK)
	expand_goto (ctor_label);
      else
	expand_null_return ();
    }
  else
    {
      /* We may still need to put RETVAL into RESULT.  */
      result = DECL_RESULT (current_function_decl);
      if (original_result_rtx)
	{
	  /* Here we have a named return value that went
	     into memory.  We can compute RETVAL into that.  */
	  if (retval)
	    expand_assignment (result, retval, 0, 0);
	  else
	    store_expr (result, original_result_rtx, 0);
	  result = make_tree (TREE_TYPE (result), original_result_rtx);
	}
      else if (ctor_label && TREE_CODE (ctor_label) != ERROR_MARK)
	{
	  /* Here RETVAL is CURRENT_CLASS_DECL, so there's nothing to do.  */
	  expand_goto (ctor_label);
	}
      else if (retval)
	{
	  /* Here is where we finally get RETVAL into RESULT.
	     `expand_return' does the magic of protecting
	     RESULT from cleanups.  */
	  result = build (INIT_EXPR, TREE_TYPE (result), result, retval);
	  TREE_SIDE_EFFECTS (result) = 1;
	  expand_return (result);
	}
      else if (TYPE_MODE (TREE_TYPE (result)) != VOIDmode)
	expand_return (result);
    }

  current_function_returns_value = 1;
  if (original_result_rtx)
    use_variable (original_result_rtx);
  if (use_temp)
    use_variable (DECL_RTL (DECL_RESULT (current_function_decl)));

  /* One way to clear out cleanups that EXPR might
     generate.  Note that this code will really be
     dead code, but that is ok--cleanups that were
     needed were handled by the magic of `return'.  */
  expand_cleanups_to (NULL_TREE);
}

/* Start a C switch statement, testing expression EXP.
   Return EXP if it is valid, an error node otherwise.  */

tree
c_expand_start_case (exp)
     tree exp;
{
  tree type = TREE_TYPE (exp);
  register enum tree_code code = TREE_CODE (type);

  if (IS_AGGR_TYPE_CODE (code))
    exp = build_type_conversion (CONVERT_EXPR, integer_type_node, exp, 1);
  else
    exp = default_conversion (exp);
  if (exp == NULL_TREE)
    {
      error ("switch quantity not an integer");
      exp = error_mark_node;
    }
  type = TREE_TYPE (exp);
  code = TREE_CODE (type);

  if (code != INTEGER_TYPE && code != ENUMERAL_TYPE && code != ERROR_MARK)
    {
      error ("switch quantity not an integer");
      exp = error_mark_node;
    }
  else
    {
      tree index;

      exp = default_conversion (exp);
      type = TREE_TYPE (exp);
      index = get_unwidened (exp, 0);
      /* We can't strip a conversion from a signed type to an unsigned,
	 because if we did, int_fits_type_p would do the wrong thing
	 when checking case values for being in range,
	 and it's too hard to do the right thing.  */
      if (TREE_UNSIGNED (TREE_TYPE (exp))
	  == TREE_UNSIGNED (TREE_TYPE (index)))
	exp = index;
    }

  expand_start_case (1, exp, type, "switch statement");

  return exp;
}

/* C++ does not yet support type checking of format strings.  */

void
record_format_info (function_ident, is_scan, format_num, first_arg_num)
      tree function_ident;
      int is_scan;
      int format_num;
      int first_arg_num;
{}
