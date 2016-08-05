/* Language-level data type conversion for GNU C.
   Copyright (C) 1987, 1988 Free Software Foundation, Inc.

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */


/* This file contains the functions for converting C expressions
   to different data types.  The only entry point is `convert'.
   Every language front end must have a `convert' function
   but what kind of conversions it does will depend on the language.  */

#include "config.h"
#include "tree.h"

/* Change of width--truncation and extension of integers or reals--
   is represented with NOP_EXPR.  Proper functioning of many things
   assumes that no other conversions can be NOP_EXPRs.

   Conversion between integer and pointer is represented with CONVERT_EXPR.
   Converting integer to real uses FLOAT_EXPR
   and real to integer uses FIX_TRUNC_EXPR.

   Here is a list of all the functions that assume that widening and
   narrowing is always done with a NOP_EXPR:
     In c-convert.c, convert_to_integer.
     In c-typeck.c, build_binary_op_nodefault (boolean ops),
        and truthvalue_conversion.
     In expr.c: expand_expr, for operands of a MULT_EXPR.
     In fold-const.c: fold.
     In tree.c: get_narrower and get_unwidened.  */

/* Subroutines of `convert'.  */

static tree
convert_to_pointer (type, expr)
     tree type, expr;
{
  register tree intype = TREE_TYPE (expr);
  register enum tree_code form = TREE_CODE (intype);
  
  if (integer_zerop (expr))
    {
      if (type == TREE_TYPE (null_pointer_node))
	return null_pointer_node;
      expr = build_int_2 (0, 0);
      TREE_TYPE (expr) = type;
      return expr;
    }

  if (form == POINTER_TYPE)
    return build (NOP_EXPR, type, expr);


  if (form == INTEGER_TYPE || form == ENUMERAL_TYPE)
    {
      if (type_precision (intype) == POINTER_SIZE)
	return build (CONVERT_EXPR, type, expr);
      return convert_to_pointer (type,
				 convert (type_for_size (POINTER_SIZE, 0),
					  expr));
    }

  error ("cannot convert to a pointer type");

  return null_pointer_node;
}

static tree
convert_to_real (type, expr)
     tree type, expr;
{
  register enum tree_code form = TREE_CODE (TREE_TYPE (expr));
  extern int flag_float_store;

  if (form == REAL_TYPE)
    return build (flag_float_store ? CONVERT_EXPR : NOP_EXPR,
		  type, expr);

  if (form == INTEGER_TYPE || form == ENUMERAL_TYPE)
    return build (FLOAT_EXPR, type, expr);

  if (form == POINTER_TYPE)
    error ("pointer value used where a float was expected");
  else
    error ("aggregate value used where a float was expected");

  {
    register tree tem = make_node (REAL_CST);
    TREE_TYPE (tem) = type;
    TREE_REAL_CST (tem) = REAL_VALUE_ATOF ("0.0");
    return tem;
  }
}

/* The result of this is always supposed to be a newly created tree node
   not in use in any existing structure.  */

static tree
convert_to_integer (type, expr)
     tree type, expr;
{
  register tree intype = TREE_TYPE (expr);
  register enum tree_code form = TREE_CODE (intype);
  extern tree build_binary_op_nodefault ();
  extern tree build_unary_op ();

  if (form == POINTER_TYPE)
    {
      if (integer_zerop (expr))
	expr = integer_zero_node;
      else
	expr = fold (build (CONVERT_EXPR,
			    type_for_size (POINTER_SIZE, 0), expr));
      intype = TREE_TYPE (expr);
      form = TREE_CODE (intype);
      if (intype == type)
	return expr;
    }

  if (form == INTEGER_TYPE || form == ENUMERAL_TYPE)
    {
      register int outprec = TYPE_PRECISION (type);
      register int inprec = TYPE_PRECISION (intype);
      register enum tree_code ex_form = TREE_CODE (expr);

      if (outprec >= inprec)
	return build (NOP_EXPR, type, expr);

/* Here detect when we can distribute the truncation down past some arithmetic.
   For example, if adding two longs and converting to an int,
   we can equally well convert both to ints and then add.
   For the operations handled here, such truncation distribution
   is always safe.
   It is desirable in these cases:
   1) when truncating down to full-word from a larger size
   2) when truncating takes no work.
   3) when at least one operand of the arithmetic has been extended
   (as by C's default conversions).  In this case we need two conversions
   if we do the arithmetic as already requested, so we might as well
   truncate both and then combine.  Perhaps that way we need only one.

   Note that in general we cannot do the arithmetic in a type
   shorter than the desired result of conversion, even if the operands
   are both extended from a shorter type, because they might overflow
   if combined in that type.  The exceptions to this--the times when
   two narrow values can be combined in their narrow type even to
   make a wider result--are handled by "shorten" in build_binary_op.  */

      switch (ex_form)
	{
	case RSHIFT_EXPR:
	  /* We can pass truncation down through right shifting
	     when the shift count is a negative constant.  */
	  if (TREE_CODE (TREE_OPERAND (expr, 1)) != INTEGER_CST
	      || TREE_INT_CST_LOW (TREE_OPERAND (expr, 1)) > 0)
	    break;
	  goto trunc1;

	case LSHIFT_EXPR:
	  /* We can pass truncation down through left shifting
	     when the shift count is a positive constant.  */
	  if (TREE_CODE (TREE_OPERAND (expr, 1)) != INTEGER_CST
	      || TREE_INT_CST_LOW (TREE_OPERAND (expr, 1)) < 0)
	    break;
	  /* In this case, shifting is like multiplication.  */
	  goto trunc1;

	case MAX_EXPR:
	case MIN_EXPR:
	case MULT_EXPR:
	  {
	    tree arg0 = get_unwidened (TREE_OPERAND (expr, 0), type);
	    tree arg1 = get_unwidened (TREE_OPERAND (expr, 1), type);

	    /* Don't distribute unless the output precision is at least as big
	       as the actual inputs.  Otherwise, the comparison of the
	       truncated values will be wrong.  */
	    if (outprec >= TYPE_PRECISION (TREE_TYPE (arg0))
		&& outprec >= TYPE_PRECISION (TREE_TYPE (arg1))
		/* If signedness of arg0 and arg1 don't match,
		   we can't necessarily find a type to compare them in.  */
		&& (TREE_UNSIGNED (TREE_TYPE (arg0))
		    == TREE_UNSIGNED (TREE_TYPE (arg1))))
	      goto trunc1;
	    break;
	  }

	case PLUS_EXPR:
	case MINUS_EXPR:
	case BIT_AND_EXPR:
	case BIT_IOR_EXPR:
	case BIT_XOR_EXPR:
	case BIT_ANDTC_EXPR:
	trunc1:
	  {
	    tree arg0 = get_unwidened (TREE_OPERAND (expr, 0), type);
	    tree arg1 = get_unwidened (TREE_OPERAND (expr, 1), type);

	    if (outprec >= BITS_PER_WORD
		|| TRULY_NOOP_TRUNCATION (outprec, inprec)
		|| inprec > TYPE_PRECISION (TREE_TYPE (arg0))
		|| inprec > TYPE_PRECISION (TREE_TYPE (arg1)))
	      {
		/* Do the arithmetic in type TYPEX,
		   then convert result to TYPE.  */
		register tree typex = type;

		/* Can't do arithmetic in enumeral types
		   so use an integer type that will hold the values.  */
		if (TREE_CODE (typex) == ENUMERAL_TYPE)
		  typex = type_for_size (TYPE_PRECISION (typex),
					 TREE_UNSIGNED (typex));

		/* But now perhaps TYPEX is as wide as INPREC.
		   In that case, do nothing special here.
		   (Otherwise would recurse infinitely in convert.  */
		if (TYPE_PRECISION (typex) != inprec)
		  {
		    /* Don't do unsigned arithmetic where signed was wanted,
		       or vice versa.
		       Exception: if the original operands were unsigned
		       then can safely do the work as unsigned.
		       And we may need to do it as unsigned
		       if we truncate to the original size.  */
		    typex = ((TREE_UNSIGNED (TREE_TYPE (expr))
			      || TREE_UNSIGNED (TREE_TYPE (arg0)))
			     ? unsigned_type (typex) : signed_type (typex));
		    return convert (type,
				    build_binary_op_nodefault (ex_form,
							       convert (typex, arg0),
							       convert (typex, arg1),
							       ex_form));
		  }
	      }
	  }
	  break;

	case EQ_EXPR:
	case NE_EXPR:
	case GT_EXPR:
	case GE_EXPR:
	case LT_EXPR:
	case LE_EXPR:
	case TRUTH_AND_EXPR:
	case TRUTH_ANDIF_EXPR:
	case TRUTH_OR_EXPR:
	case TRUTH_ORIF_EXPR:
	case TRUTH_NOT_EXPR:
	  /* If we want result of comparison converted to a byte,
	     we can just regard it as a byte, since it is 0 or 1.  */
	  TREE_TYPE (expr) = type;
	  return expr;

	case NEGATE_EXPR:
	case BIT_NOT_EXPR:
	case ABS_EXPR:
	  {
	    register tree typex = type;

	    /* Can't do arithmetic in enumeral types
	       so use an integer type that will hold the values.  */
	    if (TREE_CODE (typex) == ENUMERAL_TYPE)
	      typex = type_for_size (TYPE_PRECISION (typex),
				     TREE_UNSIGNED (typex));

	    /* But now perhaps TYPEX is as wide as INPREC.
	       In that case, do nothing special here.
	       (Otherwise would recurse infinitely in convert.  */
	    if (TYPE_PRECISION (typex) != inprec)
	      {
		/* Don't do unsigned arithmetic where signed was wanted,
		   or vice versa.  */
		typex = (TREE_UNSIGNED (TREE_TYPE (expr))
			 ? unsigned_type (typex) : signed_type (typex));
		return convert (type,
				build_unary_op (ex_form,
						convert (typex, TREE_OPERAND (expr, 0)),
						1));
	      }
	  }

	case NOP_EXPR:
	  /* If truncating after truncating, might as well do all at once.
	     If truncating after extending, we may get rid of wasted work.  */
	  return convert (type, get_unwidened (TREE_OPERAND (expr, 0), type));

	case COND_EXPR:
	  /* Can treat the two alternative values like the operands
	     of an arithmetic expression.  */
	  {
	    tree arg1 = get_unwidened (TREE_OPERAND (expr, 1), type);
	    tree arg2 = get_unwidened (TREE_OPERAND (expr, 2), type);

	    if (outprec >= BITS_PER_WORD
		|| TRULY_NOOP_TRUNCATION (outprec, inprec)
		|| inprec > TYPE_PRECISION (TREE_TYPE (arg1))
		|| inprec > TYPE_PRECISION (TREE_TYPE (arg2)))
	      {
		/* Do the arithmetic in type TYPEX,
		   then convert result to TYPE.  */
		register tree typex = type;

		/* Can't do arithmetic in enumeral types
		   so use an integer type that will hold the values.  */
		if (TREE_CODE (typex) == ENUMERAL_TYPE)
		  typex = type_for_size (TYPE_PRECISION (typex),
					 TREE_UNSIGNED (typex));

		/* But now perhaps TYPEX is as wide as INPREC.
		   In that case, do nothing special here.
		   (Otherwise would recurse infinitely in convert.  */
		if (TYPE_PRECISION (typex) != inprec)
		  {
		    /* Don't do unsigned arithmetic where signed was wanted,
		       or vice versa.  */
		    typex = (TREE_UNSIGNED (TREE_TYPE (expr))
			     ? unsigned_type (typex) : signed_type (typex));
		    return convert (type,
				    build (COND_EXPR, typex,
					   TREE_OPERAND (expr, 0),
					   convert (typex, arg1),
					   convert (typex, arg2)));
		  }
	      }
	  }

	}

      return build (NOP_EXPR, type, expr);
    }

  if (form == REAL_TYPE)
    return build (FIX_TRUNC_EXPR, type, expr);

  error ("aggregate value used where an integer was expected");

  {
    register tree tem = build_int_2 (0, 0);
    TREE_TYPE (tem) = type;
    return tem;
  }
}

/* Create an expression whose value is that of EXPR,
   converted to type TYPE.  The TREE_TYPE of the value
   is always TYPE.  This function implements all reasonable
   conversions; callers should filter out those that are
   not permitted by the language being compiled.  */

tree
convert (type, expr)
     tree type, expr;
{
  register tree e = expr;
  register enum tree_code code = TREE_CODE (type);

  if (type == TREE_TYPE (expr) || TREE_CODE (expr) == ERROR_MARK)
    return expr;
  if (TREE_CODE (TREE_TYPE (expr)) == ERROR_MARK)
    return error_mark_node;
  if (TREE_CODE (TREE_TYPE (expr)) == VOID_TYPE)
    {
      error ("void value not ignored as it ought to be");
      return error_mark_node;
    }
  if (code == VOID_TYPE)
    return build (CONVERT_EXPR, type, e);
#if 0
  /* This is incorrect.  A truncation can't be stripped this way.
     Extensions will be stripped by the use of get_unwidened.  */
  if (TREE_CODE (expr) == NOP_EXPR)
    return convert (type, TREE_OPERAND (expr, 0));
#endif
  if (code == INTEGER_TYPE || code == ENUMERAL_TYPE)
    return fold (convert_to_integer (type, e));
  if (code == POINTER_TYPE)
    return fold (convert_to_pointer (type, e));
  if (code == REAL_TYPE)
    return fold (convert_to_real (type, e));

  error ("conversion to non-scalar type requested");
  return error_mark_node;
}
