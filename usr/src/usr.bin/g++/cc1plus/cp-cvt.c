/* Language-level data type conversion for GNU C++.
   Copyright (C) 1987, 1988, 1992, 1993 Free Software Foundation, Inc.
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


/* This file contains the functions for converting C expressions
   to different data types.  The only entry point is `convert'.
   Every language front end must have a `convert' function
   but what kind of conversions it does will depend on the language.  */

#include "config.h"
#include "tree.h"
#include "flags.h"
#include "cp-tree.h"
#include "convert.h"

#undef NULL
#define NULL (char *)0

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
     In tree.c: get_narrower and get_unwidened.

   C++: in multiple-inheritance, converting between pointers may involve
   adjusting them by a delta stored within the class definition.  */

/* Subroutines of `convert'.  */

static tree
cp_convert_to_pointer (type, expr)
     tree type, expr;
{
  register tree intype = TREE_TYPE (expr);
  register enum tree_code form = TREE_CODE (intype);
  
  if (form == POINTER_TYPE)
    {
      intype = TYPE_MAIN_VARIANT (intype);

      if (TYPE_MAIN_VARIANT (type) != intype
	  && TREE_CODE (TREE_TYPE (type)) == RECORD_TYPE
	  && TREE_CODE (TREE_TYPE (intype)) == RECORD_TYPE)
	{
	  enum tree_code code = PLUS_EXPR;
	  tree binfo = get_binfo (TREE_TYPE (type), TREE_TYPE (intype), 1);
	  if (binfo == error_mark_node)
	    return error_mark_node;
	  if (binfo == NULL_TREE)
	    {
	      binfo = get_binfo (TREE_TYPE (intype), TREE_TYPE (type), 1);
	      if (binfo == error_mark_node)
		return error_mark_node;
	      code = MINUS_EXPR;
	    }
	  if (binfo)
	    {
	      if (TYPE_USES_VIRTUAL_BASECLASSES (TREE_TYPE (type))
		  || TYPE_USES_VIRTUAL_BASECLASSES (TREE_TYPE (intype))
		  || ! BINFO_OFFSET_ZEROP (binfo))
		{
		  /* Need to get the path we took.  */
		  tree path;

		  if (code == PLUS_EXPR)
		    get_base_distance (TREE_TYPE (type), TREE_TYPE (intype), 0, &path);
		  else
		    get_base_distance (TREE_TYPE (intype), TREE_TYPE (type), 0, &path);
		  return build_vbase_path (code, type, expr, path, 0);
		}
	    }
	}
      return build1 (NOP_EXPR, type, expr);
    }

  my_friendly_assert (form != OFFSET_TYPE, 186);

  if (IS_AGGR_TYPE (intype))
    {
      /* If we cannot convert to the specific pointer type,
	 try to convert to the type `void *'.  */
      tree rval;
      rval = build_type_conversion (CONVERT_EXPR, type, expr, 1);
      if (rval)
	{
	  if (rval == error_mark_node)
	    error ("ambiguous pointer conversion");
	  return rval;
	}
    }

  return convert_to_pointer (type, expr);
}

/* Like convert, except permit conversions to take place which
   are not normally allowed due to visibility restrictions
   (such as conversion from sub-type to private super-type).  */
static tree
convert_to_pointer_force (type, expr)
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
    {
      intype = TYPE_MAIN_VARIANT (intype);

      if (TYPE_MAIN_VARIANT (type) != intype
	  && TREE_CODE (TREE_TYPE (type)) == RECORD_TYPE
	  && TREE_CODE (TREE_TYPE (intype)) == RECORD_TYPE)
	{
	  enum tree_code code = PLUS_EXPR;
	  tree path;
	  int distance = get_base_distance (TREE_TYPE (type),
					    TREE_TYPE (intype), 0, &path);
	  if (distance == -2)
	    {
	    ambig:
	      error_with_aggr_type (TREE_TYPE (type), "type `%s' is ambiguous baseclass of `%s'",
				    TYPE_NAME_STRING (TREE_TYPE (intype)));
	      return error_mark_node;
	    }
	  if (distance == -1)
	    {
	      distance = get_base_distance (TREE_TYPE (intype),
					    TREE_TYPE (type), 0, &path);
	      if (distance == -2)
		goto ambig;
	      if (distance < 0)
		/* Doesn't need any special help from us.  */
		return build1 (NOP_EXPR, type, expr);

	      code = MINUS_EXPR;
	    }
	  return build_vbase_path (code, type, expr, path, 0);
	}
      return build1 (NOP_EXPR, type, expr);
    }

  return cp_convert_to_pointer (type, expr);
}

/* We are passing something to a function which requires a reference.
   The type we are interested in is in TYPE. The initial
   value we have to begin with is in ARG.

   FLAGS controls how we manage visibility checking.
   CHECKCONST controls if we report error messages on const subversion.  */
static tree
build_up_reference (type, arg, flags, checkconst)
     tree type, arg;
     int flags, checkconst;
{
  tree rval, targ;
  int literal_flag = 0;
  tree argtype = TREE_TYPE (arg), basetype = argtype;
  tree target_type = TREE_TYPE (type);
  tree binfo = NULL_TREE;

  my_friendly_assert (TREE_CODE (type) == REFERENCE_TYPE, 187);
  if (flags != 0
      && TYPE_MAIN_VARIANT (argtype) != TYPE_MAIN_VARIANT (target_type)
      && IS_AGGR_TYPE (argtype)
      && IS_AGGR_TYPE (target_type))
    {
      binfo = get_binfo (target_type, argtype,
			      (flags & LOOKUP_PROTECTED_OK) ? 3 : 2);
      if ((flags & LOOKUP_PROTECT) && binfo == error_mark_node)
	return error_mark_node;
      if (basetype == NULL_TREE)
	return (tree) error_not_base_type (target_type, argtype);
      basetype = BINFO_TYPE (binfo);
    }

  /* Pass along const and volatile down into the type. */
  if (TYPE_READONLY (type) || TYPE_VOLATILE (type))
    target_type = build_type_variant (target_type, TYPE_READONLY (type),
				      TYPE_VOLATILE (type));
  targ = arg;
  if (TREE_CODE (targ) == SAVE_EXPR)
    targ = TREE_OPERAND (targ, 0);

  switch (TREE_CODE (targ))
    {
    case INDIRECT_REF:
      /* This is a call to a constructor which did not know what it was
	 initializing until now: it needs to initialize a temporary.  */
      if (TREE_HAS_CONSTRUCTOR (targ))
	{
	  tree temp = build_cplus_new (argtype, TREE_OPERAND (targ, 0), 1);
	  TREE_HAS_CONSTRUCTOR (targ) = 0;
	  return build_up_reference (type, temp, flags, 1);
	}
      /* Let &* cancel out to simplify resulting code.
         Also, throw away intervening NOP_EXPRs.  */
      arg = TREE_OPERAND (targ, 0);
      if (TREE_CODE (arg) == NOP_EXPR || TREE_CODE (arg) == NON_LVALUE_EXPR
	  || (TREE_CODE (arg) == CONVERT_EXPR && TREE_REFERENCE_EXPR (arg)))
	arg = TREE_OPERAND (arg, 0);

      /* in doing a &*, we have to get rid of the const'ness on the pointer
	 value.  Haven't thought about volatile here.  Pointers come to mind
	 here.  */
      if (TREE_READONLY (arg))
	{
	  arg = copy_node (arg);
	  TREE_READONLY (arg) = 0;
	}

      rval = build1 (CONVERT_EXPR, type, arg);
      TREE_REFERENCE_EXPR (rval) = 1;

      /* propagate the const flag on something like:

	 class Base {
	 public:
	   int foo;
	 };

      class Derived : public Base {
      public:
	int bar;
      };

      void func(Base&);

      void func2(const Derived& d) {
	func(d);
      }

        on the d parameter.  The below could have been avoided, if the flags
        were down in the tree, not sure why they are not.  (mrs) */
      /* The below code may have to be propagated to other parts of this
	 switch.  */
      if (TREE_READONLY (targ) && !TREE_READONLY (arg)
	  && (TREE_CODE (arg) == PARM_DECL || TREE_CODE (arg) == VAR_DECL)
	  && TREE_CODE (TREE_TYPE (arg)) == REFERENCE_TYPE
	  && (TYPE_READONLY (target_type) && checkconst))
	{
	  arg = copy_node (arg);
	  TREE_READONLY (arg) = TREE_READONLY (targ);
	}
      literal_flag = TREE_CONSTANT (arg);

      goto done_but_maybe_warn;

      /* Get this out of a register if we happened to be in one by accident.
	 Also, build up references to non-lvalues it we must.  */
      /* For &x[y], return (&) x+y */
    case ARRAY_REF:
      if (mark_addressable (TREE_OPERAND (targ, 0)) == 0)
	return error_mark_node;
      rval = build_binary_op (PLUS_EXPR, TREE_OPERAND (targ, 0),
			      TREE_OPERAND (targ, 1), 1);
      TREE_TYPE (rval) = type;
      if (TREE_CONSTANT (TREE_OPERAND (targ, 1))
	  && staticp (TREE_OPERAND (targ, 0)))
	TREE_CONSTANT (rval) = 1;
      goto done;

    case SCOPE_REF:
      /* Could be a reference to a static member.  */
      {
	tree field = TREE_OPERAND (targ, 1);
	if (TREE_STATIC (field))
	  {
	    rval = build1 (ADDR_EXPR, type, field);
	    literal_flag = 1;
	    goto done;
	  }
      }

      /* We should have farmed out member pointers above.  */
      my_friendly_abort (188);

    case COMPONENT_REF:
      rval = build_component_addr (targ, build_pointer_type (argtype),
				   "attempt to make a reference to bit-field structure member `%s'");
      TREE_TYPE (rval) = type;
      literal_flag = staticp (TREE_OPERAND (targ, 0));

      goto done_but_maybe_warn;

      /* Anything not already handled and not a true memory reference
	 needs to have a reference built up.  Do so silently for
	 things like integers and return values from function,
	 but complain if we need a reference to something declared
	 as `register'.  */

    case RESULT_DECL:
      if (staticp (targ))
	literal_flag = 1;
      TREE_ADDRESSABLE (targ) = 1;
      put_var_into_stack (targ);
      break;

    case PARM_DECL:
      if (targ == current_class_decl)
	{
	  error ("address of `this' not available");
#if 0
	  /* This code makes the following core dump the compiler on a sun4,
	     if the code below is used.

	     class e_decl;
	     class a_decl;
	     typedef a_decl* a_ref;

	     class a_s {
	     public:
	       a_s();
	       void* append(a_ref& item);
	     };
	     class a_decl {
	     public:
	       a_decl (e_decl *parent);
	       a_s  generic_s;
	       a_s  decls;
	       e_decl* parent;
	     };

	     class e_decl {
	     public:
	       e_decl();
	       a_s implementations;
	     };

	     void foobar(void *);

	     a_decl::a_decl(e_decl *parent) {
	       parent->implementations.append(this);
	     }
	   */

	  TREE_ADDRESSABLE (targ) = 1; /* so compiler doesn't die later */
	  put_var_into_stack (targ);
	  break;
#else
	  return error_mark_node;
#endif
	}
      /* Fall through.  */
    case VAR_DECL:
    case CONST_DECL:
      if (DECL_REGISTER (targ) && !TREE_ADDRESSABLE (targ))
	warning ("address needed to build reference for `%s', which is declared `register'",
		 IDENTIFIER_POINTER (DECL_NAME (targ)));
      else if (staticp (targ))
	literal_flag = 1;

      TREE_ADDRESSABLE (targ) = 1;
      put_var_into_stack (targ);
      break;

    case COMPOUND_EXPR:
      {
	tree real_reference = build_up_reference (type, TREE_OPERAND (targ, 1),
						  LOOKUP_PROTECT, checkconst);
	rval = build (COMPOUND_EXPR, type, TREE_OPERAND (targ, 0), real_reference);
	TREE_CONSTANT (rval) = staticp (TREE_OPERAND (targ, 1));
	return rval;
      }

    case MODIFY_EXPR:
    case INIT_EXPR:
      {
	tree real_reference = build_up_reference (type, TREE_OPERAND (targ, 0),
						  LOOKUP_PROTECT, checkconst);
	rval = build (COMPOUND_EXPR, type, arg, real_reference);
	TREE_CONSTANT (rval) = staticp (TREE_OPERAND (targ, 0));
	return rval;
      }

    case COND_EXPR:
      return build (COND_EXPR, type,
		    TREE_OPERAND (targ, 0),
		    build_up_reference (type, TREE_OPERAND (targ, 1),
					LOOKUP_PROTECT, checkconst),
		    build_up_reference (type, TREE_OPERAND (targ, 2),
					LOOKUP_PROTECT, checkconst));

    case WITH_CLEANUP_EXPR:
      return build (WITH_CLEANUP_EXPR, type,
		    build_up_reference (type, TREE_OPERAND (targ, 0),
					LOOKUP_PROTECT, checkconst),
		    0, TREE_OPERAND (targ, 2));

    case BIND_EXPR:
      arg = TREE_OPERAND (targ, 1);
      if (arg == NULL_TREE)
	{
	  compiler_error ("({ ... }) expression not expanded when needed for reference");
	  return error_mark_node;
	}
      rval = build1 (ADDR_EXPR, type, arg);
      TREE_REFERENCE_EXPR (rval) = 1;
      return rval;

    default:
      break;
    }

  if (TREE_ADDRESSABLE (targ) == 0)
    {
      tree temp;

      if (TREE_CODE (targ) == CALL_EXPR && IS_AGGR_TYPE (argtype))
	{
	  temp = build_cplus_new (argtype, targ, 1);
	  rval = build1 (ADDR_EXPR, type, temp);
	  goto done;
	}
      else
	{
	  temp = get_temp_name (argtype, 0);
	  if (global_bindings_p ())
	    {
	      /* Give this new temp some rtl and initialize it.  */
	      DECL_INITIAL (temp) = targ;
	      TREE_STATIC (temp) = 1;
	      finish_decl (temp, targ, NULL_TREE, 0);
	      /* Do this after declaring it static.  */
	      rval = build_unary_op (ADDR_EXPR, temp, 0);
	      literal_flag = TREE_CONSTANT (rval);
	      goto done;
	    }
	  else
	    {
	      rval = build_unary_op (ADDR_EXPR, temp, 0);
	      /* Put a value into the rtl.  */
	      if (IS_AGGR_TYPE (argtype))
		{
		  /* This may produce surprising results,
		     since we commit to initializing the temp
		     when the temp may not actually get used.  */
		  expand_aggr_init (temp, targ, 0);
		  TREE_TYPE (rval) = type;
		  literal_flag = TREE_CONSTANT (rval);
		  goto done;
		}
	      else
		{
		  if (binfo && !BINFO_OFFSET_ZEROP (binfo))
		    rval = convert_pointer_to (target_type, rval);
		  else
		    TREE_TYPE (rval) = type;
		  return build (COMPOUND_EXPR, type,
				build (MODIFY_EXPR, argtype, temp, arg), rval);
		}
	    }
	}
    }
  else
    {
      if (TREE_CODE (arg) == SAVE_EXPR)
	my_friendly_abort (5);
      rval = build1 (ADDR_EXPR, type, arg);
    }

 done_but_maybe_warn:
  if (checkconst && TREE_READONLY (arg) && ! TYPE_READONLY (target_type))
    readonly_error (arg, "conversion to reference", 1);

 done:
  if (TYPE_USES_COMPLEX_INHERITANCE (argtype))
    {
      TREE_TYPE (rval) = TYPE_POINTER_TO (argtype);
      rval = convert_pointer_to (target_type, rval);
      TREE_TYPE (rval) = type;
    }
  TREE_CONSTANT (rval) = literal_flag;
  return rval;
}

/* For C++: Only need to do one-level references, but cannot
   get tripped up on signed/unsigned differences.

   If DECL is NULL_TREE it means convert as though casting (by force).
   If it is ERROR_MARK_NODE, it means the conversion is implicit,
   and that temporaries may be created.
   Make sure the use of user-defined conversion operators is un-ambiguous.
   Otherwise, DECL is a _DECL node which can be used in error reporting.

   FNDECL, PARMNUM, and ERRTYPE are only used when checking for use of
   volatile or const references where they aren't desired.  */

tree
convert_to_reference (decl, reftype, expr, fndecl, parmnum,
		      errtype, strict, flags)

     tree decl;
     tree reftype, expr;
     tree fndecl;
     int parmnum;
     char *errtype;
     int strict, flags;
{
  register tree type = TYPE_MAIN_VARIANT (TREE_TYPE (reftype));
  register tree intype = TREE_TYPE (expr);
  register enum tree_code form = TREE_CODE (intype);
  tree rval = NULL_TREE;

  if (form == REFERENCE_TYPE)
    intype = TREE_TYPE (intype);
  intype = TYPE_MAIN_VARIANT (intype);

  /* @@ Probably need to have a check for X(X&) here.  */

  if (IS_AGGR_TYPE (intype))
    {
      rval = build_type_conversion (CONVERT_EXPR, reftype, expr, 1);
      if (rval)
	{
	  if (rval == error_mark_node)
	    error ("ambiguous pointer conversion");
	  return rval;
	}
      else if (type != intype
	       && (rval = build_type_conversion (CONVERT_EXPR, type, expr, 1)))
	{
	  if (rval == error_mark_node)
	    return rval;
	  if (TYPE_NEEDS_DESTRUCTOR (type))
	    {
	      rval = convert_to_reference (NULL_TREE, reftype, rval, NULL_TREE,
-1, (char *)NULL, strict, flags);
	    }
	  else
	    {
	      decl = get_temp_name (type, 0);
	      rval = build (INIT_EXPR, type, decl, rval);
	      rval = build (COMPOUND_EXPR, reftype, rval,
			    convert_to_reference (NULL_TREE, reftype, decl,
						  NULL_TREE, -1, (char*)NULL,
						  strict, flags));
	    }
	}

      if (form == REFERENCE_TYPE
	  && type != intype
	  && TYPE_USES_COMPLEX_INHERITANCE (intype))
	{
	  /* If it may move around, build a fresh reference.  */
	  expr = convert_from_reference (expr);
	  form = TREE_CODE (TREE_TYPE (expr));
	}
    }

  /* @@ Perhaps this should try to go through a constructor first
     @@ for proper initialization, but I am not sure when that
     @@ is needed or desirable.

     @@ The second disjunct is provided to make references behave
     @@ as some people think they should, i.e., an interconvertibility
     @@ between references to builtin types (such as short and
     @@ unsigned short).  There should be no conversion between
     @@ types whose codes are different, or whose sizes are different.  */

  if (((IS_AGGR_TYPE (type) || IS_AGGR_TYPE (intype))
       && comptypes (type, intype, strict))
      || (!IS_AGGR_TYPE (type)
	  && TREE_CODE (type) == TREE_CODE (intype)
	  && int_size_in_bytes (type) == int_size_in_bytes (intype)))
    {
      /* Section 13.  */
      /* Since convert_for_initialization didn't call convert_for_assignment,
	 we have to do this checking here.  XXX We should have a common
	 routine between here and convert_for_assignment.  */
      if (TREE_CODE (TREE_TYPE (expr)) == REFERENCE_TYPE)
	{
	  register tree ttl = TREE_TYPE (reftype);
	  register tree ttr = TREE_TYPE (TREE_TYPE (expr));

	  if (! TYPE_READONLY (ttl) && TYPE_READONLY (ttr))
	    warn_for_assignment ("%s of non-`const &' reference from `const &'",
				 "reference to const given for argument %d of `%s'",
				 errtype, fndecl, parmnum, pedantic);
	  if (! TYPE_VOLATILE (ttl) && TYPE_VOLATILE (ttr))
	    warn_for_assignment ("%s of non-`volatile &' reference from `volatile &'",
				 "reference to volatile given for argument %d of `%s'",	
				 errtype, fndecl, parmnum, pedantic);
	}

      /* If EXPR is of aggregate type, and is really a CALL_EXPR,
	 then we don't need to convert it to reference type if
	 it is only being used to initialize DECL which is also
	 of the same aggregate type.  */
      if (form == REFERENCE_TYPE
	  || (decl != NULL_TREE && decl != error_mark_node
	      && IS_AGGR_TYPE (type)
	      && TREE_CODE (expr) == CALL_EXPR
	      && TYPE_MAIN_VARIANT (type) == intype))
	{
	  if (decl && decl != error_mark_node)
	    {
	      tree e1 = build (INIT_EXPR, void_type_node, decl, expr);
	      tree e2;

	      TREE_SIDE_EFFECTS (e1) = 1;
	      if (form == REFERENCE_TYPE)
		e2 = build1 (NOP_EXPR, reftype, decl);
	      else
		{
		  e2 = build_unary_op (ADDR_EXPR, decl, 0);
		  TREE_TYPE (e2) = reftype;
		  TREE_REFERENCE_EXPR (e2) = 1;
		}
	      return build_compound_expr (tree_cons (NULL_TREE, e1,
						     build_tree_list (NULL_TREE, e2)));
	    }
	  expr = copy_node (expr);
	  TREE_TYPE (expr) = reftype;
	  return expr;
	}
      if (decl == error_mark_node)
	flags |= LOOKUP_PROTECTED_OK;
      return build_up_reference (reftype, expr, flags, decl!=NULL_TREE);
    }

  /* Definitely need to go through a constructor here.  */
  if (TYPE_HAS_CONSTRUCTOR (type))
    {
      tree init = build_method_call (NULL_TREE, constructor_name (type),
				     build_tree_list (NULL_TREE, expr),
				     TYPE_BINFO (type), LOOKUP_NO_CONVERSION);

      if (init != error_mark_node)
	if (rval)
	  {
	    error ("both constructor and type conversion operator apply");
	    return error_mark_node;
	  }

      init = build_method_call (NULL_TREE, constructor_name (type),
				build_tree_list (NULL_TREE, expr),
				TYPE_BINFO (type), LOOKUP_NORMAL|LOOKUP_NO_CONVERSION);

      if (init == error_mark_node)
	return error_mark_node;
      rval = build_cplus_new (type, init, 1);
      if (decl == error_mark_node)
	flags |= LOOKUP_PROTECTED_OK;
      return build_up_reference (reftype, rval, flags, decl!=NULL_TREE);
    }

  my_friendly_assert (form != OFFSET_TYPE, 189);

  /* This is in two pieces for now, because pointer to first becomes
     invalid once type_as_string is called again. */
  error ("cannot convert type `%s'", type_as_string (intype));
  error ("       to type `%s'", type_as_string (reftype));

  return error_mark_node;
}

/* We are using a reference VAL for its value. Bash that reference all the
   way down to its lowest form. */
tree
convert_from_reference (val)
     tree val;
{
  tree type = TREE_TYPE (val);

  if (TREE_CODE (type) == OFFSET_TYPE)
    type = TREE_TYPE (type);
 if (TREE_CODE (type) == REFERENCE_TYPE)
    {
      tree target_type = TREE_TYPE (type);
      tree nval;

      /* This can happen if we cast to a reference type.  */
      if (TREE_CODE (val) == ADDR_EXPR)
	{
	  nval = build1 (NOP_EXPR, build_pointer_type (target_type), val);
	  nval = build_indirect_ref (nval, 0);
	  /* The below was missing, are other important flags missing too? */
	  TREE_SIDE_EFFECTS (nval) = TREE_SIDE_EFFECTS (val);
	  return nval;
	}

      nval = build1 (INDIRECT_REF, TYPE_MAIN_VARIANT (target_type), val);

      TREE_THIS_VOLATILE (nval) = TYPE_VOLATILE (target_type);
      TREE_SIDE_EFFECTS (nval) = TYPE_VOLATILE (target_type);
      TREE_READONLY (nval) = TYPE_READONLY (target_type);
      /* The below was missing, are other important flags missing too? */
      TREE_SIDE_EFFECTS (nval) = TREE_SIDE_EFFECTS (val);
      return nval;
    }
  return val;
}

/* See if there is a constructor of type TYPE which will convert
   EXPR.  The reference manual seems to suggest (8.5.6) that we need
   not worry about finding constructors for base classes, then converting
   to the derived class.

   MSGP is a pointer to a message that would be an appropriate error
   string.  If MSGP is NULL, then we are not interested in reporting
   errors.  */
tree
convert_to_aggr (type, expr, msgp, protect)
     tree type, expr;
     char **msgp;
     int protect;
{
  tree basetype = type;
  tree name = TYPE_IDENTIFIER (basetype);
  tree function, fndecl, fntype, parmtypes, parmlist, result;
  tree method_name;
  enum visibility_type visibility;
  int can_be_private, can_be_protected;

  if (! TYPE_HAS_CONSTRUCTOR (basetype))
    {
      if (msgp)
	*msgp = "type `%s' does not have a constructor";
      return error_mark_node;
    }

  visibility = visibility_public;
  can_be_private = 0;
  can_be_protected = IDENTIFIER_CLASS_VALUE (name) || name == current_class_name;

  parmlist = build_tree_list (NULL_TREE, expr);
  parmtypes = tree_cons (NULL_TREE, TREE_TYPE (expr), void_list_node);

  if (TYPE_USES_VIRTUAL_BASECLASSES (basetype))
    {
      parmtypes = tree_cons (NULL_TREE, integer_type_node, parmtypes);
      parmlist = tree_cons (NULL_TREE, integer_one_node, parmlist);
    }

  /* The type of the first argument will be filled in inside the loop.  */
  parmlist = tree_cons (NULL_TREE, integer_zero_node, parmlist);
  parmtypes = tree_cons (NULL_TREE, TYPE_POINTER_TO (basetype), parmtypes);

  method_name = build_decl_overload (name, parmtypes, 1);

  /* constructors are up front.  */
  fndecl = TREE_VEC_ELT (CLASSTYPE_METHOD_VEC (basetype), 0);
  if (TYPE_HAS_DESTRUCTOR (basetype))
    fndecl = DECL_CHAIN (fndecl);

  while (fndecl)
    {
      if (DECL_ASSEMBLER_NAME (fndecl) == method_name)
	{
	  function = fndecl;
	  if (protect)
	    {
	      if (TREE_PRIVATE (fndecl))
		{
		  can_be_private =
		    (basetype == current_class_type
		     || is_friend (basetype, current_function_decl)
		     || purpose_member (basetype, DECL_VISIBILITY (fndecl)));
		  if (! can_be_private)
		    goto found;
		}
	      else if (TREE_PROTECTED (fndecl))
		{
		  if (! can_be_protected)
		    goto found;
		}
	    }
	  goto found_and_ok;
	}
      fndecl = DECL_CHAIN (fndecl);
    }

  /* No exact conversion was found.  See if an approximate
     one will do.  */
  fndecl = TREE_VEC_ELT (CLASSTYPE_METHOD_VEC (basetype), 0);
  if (TYPE_HAS_DESTRUCTOR (basetype))
    fndecl = DECL_CHAIN (fndecl);

  {
    int saw_private = 0;
    int saw_protected = 0;
    struct candidate *candidates =
      (struct candidate *) alloca ((decl_list_length (fndecl)+1) * sizeof (struct candidate));
    struct candidate *cp = candidates;

    while (fndecl)
      {
	function = fndecl;
	cp->harshness = (unsigned short *)alloca (3 * sizeof (short));
	compute_conversion_costs (fndecl, parmlist, cp, 2);
	if (cp->evil == 0)
	  {
	    cp->u.field = fndecl;
	    if (protect)
	      {
		if (TREE_PRIVATE (fndecl))
		  visibility = visibility_private;
		else if (TREE_PROTECTED (fndecl))
		  visibility = visibility_protected;
		else
		  visibility = visibility_public;
	      }
	    else
	      visibility = visibility_public;

	    if (visibility == visibility_private
		? (basetype == current_class_type
		   || is_friend (basetype, cp->function)
		   || purpose_member (basetype, DECL_VISIBILITY (fndecl)))
		: visibility == visibility_protected
		? (can_be_protected
		   || purpose_member (basetype, DECL_VISIBILITY (fndecl)))
		: 1)
	      {
		if (cp->user == 0 && cp->b_or_d == 0
		    && cp->easy <= 1)
		  {
		    goto found_and_ok;
		  }
		cp++;
	      }
	    else
	      {
		if (visibility == visibility_private)
		  saw_private = 1;
		else
		  saw_protected = 1;
	      }
	  }
	fndecl = DECL_CHAIN (fndecl);
      }
    if (cp - candidates)
      {
	/* Rank from worst to best.  Then cp will point to best one.
	   Private fields have their bits flipped.  For unsigned
	   numbers, this should make them look very large.
	   If the best alternate has a (signed) negative value,
	   then all we ever saw were private members.  */
	if (cp - candidates > 1)
	  qsort (candidates,	/* char *base */
		 cp - candidates, /* int nel */
		 sizeof (struct candidate), /* int width */
		 rank_for_overload); /* int (*compar)() */

	--cp;
	if (cp->evil > 1)
	  {
	    if (msgp)
	      *msgp = "ambiguous type conversion possible for `%s'";
	    return error_mark_node;
	  }

	function = cp->function;
	fndecl = cp->u.field;
	goto found_and_ok;
      }
    else if (msgp)
      {
	if (saw_private)
	  if (saw_protected)
	    *msgp = "only private and protected conversions apply";
	  else
	    *msgp = "only private conversions apply";
	else if (saw_protected)
	  *msgp = "only protected conversions apply";
      }
    return error_mark_node;
  }
  /* NOTREACHED */

 not_found:
  if (msgp) *msgp = "no appropriate conversion to type `%s'";
  return error_mark_node;
 found:
  if (visibility == visibility_private)
    if (! can_be_private)
      {
	if (msgp)
	  *msgp = TREE_PRIVATE (fndecl)
	    ? "conversion to type `%s' is private"
	    : "conversion to type `%s' is from private base class";
	return error_mark_node;
      }
  if (visibility == visibility_protected)
    if (! can_be_protected)
      {
	if (msgp)
	  *msgp = TREE_PRIVATE (fndecl)
	    ? "conversion to type `%s' is protected"
	    : "conversion to type `%s' is from protected base class";
	return error_mark_node;
      }
  function = fndecl;
 found_and_ok:

  /* It will convert, but we don't do anything about it yet.  */
  if (msgp == 0)
    return NULL_TREE;

  fntype = TREE_TYPE (function);
  if (DECL_INLINE (function) && TREE_CODE (function) == FUNCTION_DECL)
    function = build1 (ADDR_EXPR, build_pointer_type (fntype), function);
  else
    function = default_conversion (function);

  result = build_nt (CALL_EXPR, function,
		     convert_arguments (NULL_TREE, TYPE_ARG_TYPES (fntype),
					parmlist, NULL_TREE, LOOKUP_NORMAL),
		     NULL_TREE);
  TREE_TYPE (result) = TREE_TYPE (fntype);
  TREE_SIDE_EFFECTS (result) = 1;
  TREE_RAISES (result) = !! TYPE_RAISES_EXCEPTIONS (fntype);
  return result;
}

/* Call this when we know (for any reason) that expr is
   not, in fact, zero.  */
tree
convert_pointer_to (binfo, expr)
     tree binfo, expr;
{
  register tree intype = TREE_TYPE (expr);
  tree ptr_type;
  tree type, rval;

  if (TREE_CODE (binfo) == TREE_VEC)
    type = BINFO_TYPE (binfo);
  else if (IS_AGGR_TYPE (binfo))
    {
      type = binfo;
      binfo = TYPE_BINFO (binfo);
    }
  else
    {
      type = binfo;
      binfo = NULL_TREE;
    }

  ptr_type = build_pointer_type (type);
  if (ptr_type == TYPE_MAIN_VARIANT (intype))
    return expr;

  if (intype == error_mark_node)
    return error_mark_node;

  my_friendly_assert (!integer_zerop (expr), 191);

  if (TREE_CODE (type) == RECORD_TYPE
      && TREE_CODE (TREE_TYPE (intype)) == RECORD_TYPE
      && type != TYPE_MAIN_VARIANT (TREE_TYPE (intype)))
    {
      tree path;
      int distance
	= get_base_distance (type, TYPE_MAIN_VARIANT (TREE_TYPE (intype)),
			     0, &path);

      /* This function shouldn't be called with unqualified arguments
	 but if it is, give them an error message that they can read.
	 */
      if (distance < 0)
	{
	  error ("cannot convert a pointer of type `%s'",
		 TYPE_NAME_STRING (TREE_TYPE (intype)));
	  error_with_aggr_type (type, "to a pointer of type `%s'");

	  return error_mark_node;
	}

      return build_vbase_path (PLUS_EXPR, ptr_type, expr, path, 1);
    }
  rval = build1 (NOP_EXPR, ptr_type,
		 TREE_CODE (expr) == NOP_EXPR ? TREE_OPERAND (expr, 0) : expr);
  TREE_CONSTANT (rval) = TREE_CONSTANT (expr);
  return rval;
}

/* Same as above, but don't abort if we get an "ambiguous" baseclass.
   There's only one virtual baseclass we are looking for, and once
   we find one such virtual baseclass, we have found them all.  */

tree
convert_pointer_to_vbase (binfo, expr)
     tree binfo;
     tree expr;
{
  tree intype = TREE_TYPE (TREE_TYPE (expr));
  tree binfos = TYPE_BINFO_BASETYPES (intype);
  int i;

  for (i = TREE_VEC_LENGTH (binfos)-1; i >= 0; i--)
    {
      tree basetype = BINFO_TYPE (TREE_VEC_ELT (binfos, i));
      if (BINFO_TYPE (binfo) == basetype)
	return convert_pointer_to (binfo, expr);
      if (binfo_member (BINFO_TYPE (binfo), CLASSTYPE_VBASECLASSES (basetype)))
	return convert_pointer_to_vbase (binfo, convert_pointer_to (basetype, expr));
    }
  my_friendly_abort (6);
  /* NOTREACHED */
  return NULL_TREE;
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

  if (type == TREE_TYPE (expr)
      || TREE_CODE (expr) == ERROR_MARK)
    return expr;
  if (TYPE_MAIN_VARIANT (type) == TYPE_MAIN_VARIANT (TREE_TYPE (expr)))
    return fold (build1 (NOP_EXPR, type, expr));
  if (TREE_CODE (TREE_TYPE (expr)) == ERROR_MARK)
    return error_mark_node;
  if (TREE_CODE (TREE_TYPE (expr)) == VOID_TYPE)
    {
      error ("void value not ignored as it ought to be");
      return error_mark_node;
    }
  if (code == VOID_TYPE)
    {
      tree rval = build_type_conversion (NOP_EXPR, type, e, 0);
      /* If we can convert to void type via a type conversion, do so.  */
      if (rval)
	return rval;
      return build1 (CONVERT_EXPR, type, e);
    }
#if 0
  /* This is incorrect.  A truncation can't be stripped this way.
     Extensions will be stripped by the use of get_unwidened.  */
  if (TREE_CODE (expr) == NOP_EXPR)
    return convert (type, TREE_OPERAND (expr, 0));
#endif

  /* Just convert to the type of the member.  */
  if (code == OFFSET_TYPE)
    {
      type = TREE_TYPE (type);
      code = TREE_CODE (type);
    }

  /* C++ */
  if (code == REFERENCE_TYPE)
    return fold (convert_to_reference (error_mark_node,
				       type, e,
				       NULL_TREE, -1, (char *)NULL,
				       -1, LOOKUP_NORMAL));
  else if (TREE_CODE (TREE_TYPE (e)) == REFERENCE_TYPE)
    e = convert_from_reference (e);

  if (code == INTEGER_TYPE || code == ENUMERAL_TYPE)
    {
      tree intype = TREE_TYPE (expr);
      enum tree_code form = TREE_CODE (intype);
      if (flag_int_enum_equivalence == 0
	  && TREE_CODE (type) == ENUMERAL_TYPE
	  && form == INTEGER_TYPE)
	{
	  if (pedantic)
	    pedwarn ("anachronistic conversion from integer type to enumeral type `%s'",
		     TYPE_NAME_STRING (type));
	  if (flag_pedantic_errors)
	    return error_mark_node;
	}
      if (form == OFFSET_TYPE)
	error_with_decl (TYPE_NAME (TYPE_OFFSET_BASETYPE (intype)),
			 "pointer-to-member expression object not composed with type `%s' object");
      else if (IS_AGGR_TYPE (intype))
	{
	  tree rval;
	  rval = build_type_conversion (CONVERT_EXPR, type, expr, 1);
	  if (rval) return rval;
	  error ("aggregate value used where an integer was expected");
	  return error_mark_node;
	}
      return fold (convert_to_integer (type, e));
    }
  if (code == POINTER_TYPE)
    return fold (cp_convert_to_pointer (type, e));
  if (code == REAL_TYPE)
    {
      if (IS_AGGR_TYPE (TREE_TYPE (e)))
	{
	  tree rval;
	  rval = build_type_conversion (CONVERT_EXPR, type, e, 1);
	  if (rval)
	    return rval;
	  else
	    error ("aggregate value used where a floating point value was expected");
	}
      return fold (convert_to_real (type, e));
    }

  /* New C++ semantics:  since assignment is now based on
     memberwise copying,  if the rhs type is derived from the
     lhs type, then we may still do a conversion.  */
  if (IS_AGGR_TYPE_CODE (code))
    {
      tree dtype = TREE_TYPE (e);

      if (TREE_CODE (dtype) == REFERENCE_TYPE)
	{
	  e = convert_from_reference (e);
	  dtype = TREE_TYPE (e);
	}
      dtype = TYPE_MAIN_VARIANT (dtype);

      /* Conversion between aggregate types.  New C++ semantics allow
	 objects of derived type to be cast to objects of base type.
	 Old semantics only allowed this between pointers.

	 There may be some ambiguity between using a constructor
	 vs. using a type conversion operator when both apply.  */

      if (IS_AGGR_TYPE (dtype))
	{
	  tree binfo;

	  tree conversion = TYPE_HAS_CONVERSION (dtype)
	    ? build_type_conversion (CONVERT_EXPR, type, e, 1) : NULL_TREE;

	  if (TYPE_HAS_CONSTRUCTOR (type))
	    {
	      tree rval = build_method_call (NULL_TREE, constructor_name (type),
					     build_tree_list (NULL_TREE, e),
					     TYPE_BINFO (type),
					     conversion ? LOOKUP_NO_CONVERSION : 0);

	      if (rval != error_mark_node)
		{
		  if (conversion)
		    {
		      error ("both constructor and type conversion operator apply");
		      return error_mark_node;
		    }
		  /* call to constructor successful.  */
		  rval = build_cplus_new (type, rval, 0);
		  return rval;
		}
	    }
	  /* Type conversion successful/applies.  */
	  if (conversion)
	    {
	      if (conversion == error_mark_node)
		error ("ambiguous pointer conversion");
	      return conversion;
	    }

	  /* now try normal C++ assignment semantics.  */
	  binfo = TYPE_BINFO (dtype);
	  if (BINFO_TYPE (binfo) == type
	      || (binfo = get_binfo (type, dtype, 1)))
	    {
	      if (binfo == error_mark_node)
		return error_mark_node;
	    }
	  if (binfo != NULL_TREE)
	    {
	      if (lvalue_p (e))
		{
		  e = build_unary_op (ADDR_EXPR, e, 0);

		  if (! BINFO_OFFSET_ZEROP (binfo))
		    e = build (PLUS_EXPR, TYPE_POINTER_TO (type),
			       e, BINFO_OFFSET (binfo));
		  return build1 (INDIRECT_REF, type, e);
		}

	      sorry ("addressable aggregates");
	      return error_mark_node;
	    }
	  error ("conversion between incompatible aggregate types requested");
	  return error_mark_node;
	}
      /* conversion from non-aggregate to aggregate type requires constructor.  */
      else if (TYPE_HAS_CONSTRUCTOR (type))
	{
	  tree rval;
	  tree init = build_method_call (NULL_TREE, constructor_name (type),
					 build_tree_list (NULL_TREE, e),
					 TYPE_BINFO (type), LOOKUP_NORMAL);
	  if (init == error_mark_node)
	    {
	      error_with_aggr_type (type, "in conversion to type `%s'");
	      return error_mark_node;
	    }
	  rval = build_cplus_new (type, init, 0);
	  return rval;
	}
    }

  /* If TYPE or TREE_TYPE (EXPR) is not on the permanent_obstack,
     then the it won't be hashed and hence compare as not equal,
     even when it is.  */
  if (code == ARRAY_TYPE
      && TREE_TYPE (TREE_TYPE (expr)) == TREE_TYPE (type)
      && index_type_equal (TYPE_DOMAIN (TREE_TYPE (expr)), TYPE_DOMAIN (type)))
    return expr;

  error ("conversion to non-scalar type requested");
  return error_mark_node;
}

/* Like convert, except permit conversions to take place which
   are not normally allowed due to visibility restrictions
   (such as conversion from sub-type to private super-type).  */
tree
convert_force (type, expr)
     tree type;
     tree expr;
{
  register tree e = expr;
  register enum tree_code code = TREE_CODE (type);

  if (code == REFERENCE_TYPE)
    return fold (convert_to_reference (0, type, e,
				       NULL_TREE, -1, (char *)NULL,
				       -1, 0));
  else if (TREE_CODE (TREE_TYPE (e)) == REFERENCE_TYPE)
    e = convert_from_reference (e);

  if (code == POINTER_TYPE)
    return fold (convert_to_pointer_force (type, e));

  {
    int old_equiv = flag_int_enum_equivalence;
    flag_int_enum_equivalence = 1;
    e = convert (type, e);
    flag_int_enum_equivalence = old_equiv;
  }
  return e;
}

/* Subroutine of build_type_conversion.  */
static tree
build_type_conversion_1 (xtype, basetype, expr, typename, for_sure)
     tree xtype, basetype;
     tree expr;
     tree typename;
     int for_sure;
{
  tree first_arg = expr;
  tree rval;
  int flags;

  if (for_sure == 0)
    {
      if (! lvalue_p (expr))
	first_arg = build1 (NOP_EXPR, TYPE_POINTER_TO (basetype), integer_zero_node);
      flags = LOOKUP_PROTECT;
    }
  else
    flags = LOOKUP_NORMAL;

  rval = build_method_call (first_arg, constructor_name (typename),
			    NULL_TREE, NULL_TREE, flags);
  if (rval == error_mark_node)
    {
      if (for_sure == 0)
	return NULL_TREE;
      return error_mark_node;
    }
  if (first_arg != expr)
    {
      expr = build_up_reference (build_reference_type (TREE_TYPE (expr)), expr,
				 LOOKUP_COMPLAIN, 1);
      TREE_VALUE (TREE_OPERAND (rval, 1)) = build_unary_op (ADDR_EXPR, expr, 0);
    }
  if (TREE_CODE (TREE_TYPE (rval)) == REFERENCE_TYPE
      && TREE_CODE (xtype) != REFERENCE_TYPE)
    rval = default_conversion (rval);

  if (pedantic
      && TREE_TYPE (xtype)
      && (TREE_READONLY (TREE_TYPE (TREE_TYPE (rval)))
	  > TREE_READONLY (TREE_TYPE (xtype))))
    pedwarn ("user-defined conversion casting away `const'");
  return convert (xtype, rval);
}

/* Convert an aggregate EXPR to type XTYPE.  If a conversion
   exists, return the attempted conversion.  This may
   return ERROR_MARK_NODE if the conversion is not
   allowed (references private members, etc).
   If no conversion exists, NULL_TREE is returned.

   If (FOR_SURE & 1) is non-zero, then we allow this type conversion
   to take place immediately.  Otherwise, we build a SAVE_EXPR
   which can be evaluated if the results are ever needed.

   If FOR_SURE >= 2, then we only look for exact conversions.

   TYPE may be a reference type, in which case we first look
   for something that will convert to a reference type.  If
   that fails, we will try to look for something of the
   reference's target type, and then return a reference to that.  */
tree
build_type_conversion (code, xtype, expr, for_sure)
     enum tree_code code;
     tree xtype, expr;
     int for_sure;
{
  /* C++: check to see if we can convert this aggregate type
     into the required scalar type.  */
  tree type, type_default;
  tree typename = build_typename_overload (xtype), *typenames;
  int n_variants = 0;
  tree basetype, save_basetype;
  tree rval;
  int exact_conversion = for_sure >= 2;
  for_sure &= 1;

  if (expr == error_mark_node)
    return error_mark_node;

  basetype = TREE_TYPE (expr);
  if (TREE_CODE (basetype) == REFERENCE_TYPE)
    basetype = TREE_TYPE (basetype);

  basetype = TYPE_MAIN_VARIANT (basetype);
  if (! TYPE_LANG_SPECIFIC (basetype) || ! TYPE_HAS_CONVERSION (basetype))
    return 0;

  if (TREE_CODE (xtype) == POINTER_TYPE
      || TREE_CODE (xtype) == REFERENCE_TYPE)
    {
      /* Prepare to match a variant of this type.  */
      type = TYPE_MAIN_VARIANT (TREE_TYPE (xtype));
      for (n_variants = 0; type; type = TYPE_NEXT_VARIANT (type))
	n_variants++;
      typenames = (tree *)alloca (n_variants * sizeof (tree));
      for (n_variants = 0, type = TYPE_MAIN_VARIANT (TREE_TYPE (xtype));
	   type; n_variants++, type = TYPE_NEXT_VARIANT (type))
	{
	  if (type == TREE_TYPE (xtype))
	    typenames[n_variants] = typename;
	  else if (TREE_CODE (xtype) == POINTER_TYPE)
	    typenames[n_variants] = build_typename_overload (build_pointer_type (type));
	  else
	    typenames[n_variants] = build_typename_overload (build_reference_type (type));
	}
    }

  save_basetype = basetype;
  type = xtype;

  while (TYPE_HAS_CONVERSION (basetype))
    {
      int i;
      if (lookup_fnfields (TYPE_BINFO (basetype), typename, 0))
	return build_type_conversion_1 (xtype, basetype, expr, typename, for_sure);
      for (i = 0; i < n_variants; i++)
	if (typenames[i] != typename
	    && lookup_fnfields (TYPE_BINFO (basetype), typenames[i], 0))
	  return build_type_conversion_1 (xtype, basetype, expr, typenames[i], for_sure);

      if (TYPE_BINFO_BASETYPES (basetype))
	basetype = TYPE_BINFO_BASETYPE (basetype, 0);
      else break;
    }

  if (TREE_CODE (type) == REFERENCE_TYPE)
    {
      tree first_arg = expr;
      type = TYPE_MAIN_VARIANT (TREE_TYPE (type));
      basetype = save_basetype;

      /* May need to build a temporary for this.  */
      while (TYPE_HAS_CONVERSION (basetype))
	{
	  if (lookup_fnfields (TYPE_BINFO (basetype), typename, 0))
	    {
	      int flags;

	      if (for_sure == 0)
		{
		  if (! lvalue_p (expr))
		    first_arg = build1 (NOP_EXPR, TYPE_POINTER_TO (basetype), integer_zero_node);
		  flags = LOOKUP_PROTECT;
		}
	      else
		flags = LOOKUP_NORMAL;
	      rval = build_method_call (first_arg, constructor_name (typename),
					NULL_TREE, NULL_TREE, flags);
	      if (rval == error_mark_node)
		{
		  if (for_sure == 0)
		    return NULL_TREE;
		  return error_mark_node;
		}
	      TREE_VALUE (TREE_OPERAND (rval, 1)) = expr;

	      if (IS_AGGR_TYPE (type))
		{
		  tree init = build_method_call (NULL_TREE,
						 constructor_name (type),
						 build_tree_list (NULL_TREE, rval), NULL_TREE, LOOKUP_NORMAL);
		  tree temp = build_cplus_new (type, init, 1);
		  return build_up_reference (TYPE_REFERENCE_TO (type), temp,
					     LOOKUP_COMPLAIN, 1);
		}
	      return convert (xtype, rval);
	    }
	  if (TYPE_BINFO_BASETYPES (basetype))
	    basetype = TYPE_BINFO_BASETYPE (basetype, 0);
	  else break;
	}
      /* No free conversions for reference types, right?.  */
      return NULL_TREE;
    }

  if (exact_conversion)
    return NULL_TREE;

  /* No perfect match found, try default.  */
  if (code == CONVERT_EXPR && TREE_CODE (type) == POINTER_TYPE)
    type_default = ptr_type_node;
  else if (type == void_type_node)
    return NULL_TREE;
  else
    {
      tree tmp = default_conversion (build1 (NOP_EXPR, type, integer_zero_node));
      if (tmp == error_mark_node)
	return NULL_TREE;
      type_default = TREE_TYPE (tmp);
    }

  basetype = save_basetype;

  if (type_default != type)
    {
      type = type_default;
      typename = build_typename_overload (type);

      while (TYPE_HAS_CONVERSION (basetype))
	{
	  if (lookup_fnfields (TYPE_BINFO (basetype), typename, 0))
	    return build_type_conversion_1 (xtype, basetype, expr, typename, for_sure);
	  if (TYPE_BINFO_BASETYPES (basetype))
	    basetype = TYPE_BINFO_BASETYPE (basetype, 0);
	  else break;
	}
    }

 try_pointer:

  if (type == ptr_type_node)
    {
      /* Try converting to some other pointer type
	 with which void* is compatible, or in situations
	 in which void* is appropriate (such as &&,||, and !).  */

      while (TYPE_HAS_CONVERSION (basetype))
	{
	  if (CLASSTYPE_CONVERSION (basetype, ptr_conv) != 0)
	    {
	      if (CLASSTYPE_CONVERSION (basetype, ptr_conv) == error_mark_node)
		return error_mark_node;
	      typename = DECL_NAME (CLASSTYPE_CONVERSION (basetype, ptr_conv));
	      return build_type_conversion_1 (xtype, basetype, expr, typename, for_sure);
	    }
	  if (TYPE_BINFO_BASETYPES (basetype))
	    basetype = TYPE_BINFO_BASETYPE (basetype, 0);
	  else break;
	}
    }
  if (TREE_CODE (type) == POINTER_TYPE
      && TYPE_READONLY (TREE_TYPE (type))
      && TYPE_MAIN_VARIANT (TREE_TYPE (type)) == void_type_node)
    {
      /* Try converting to some other pointer type
	 with which const void* is compatible.  */

      while (TYPE_HAS_CONVERSION (basetype))
	{
	  if (CLASSTYPE_CONVERSION (basetype, constptr_conv) != 0)
	    {
	      if (CLASSTYPE_CONVERSION (basetype, constptr_conv) == error_mark_node)
		return error_mark_node;
	      typename = DECL_NAME (CLASSTYPE_CONVERSION (basetype, constptr_conv));
	      return build_type_conversion_1 (xtype, basetype, expr, typename, for_sure);
	    }
	  if (TYPE_BINFO_BASETYPES (basetype))
	    basetype = TYPE_BINFO_BASETYPE (basetype, 0);
	  else break;
	}
    }
  /* Use the longer or shorter conversion that is appropriate.  Have
     to check against 0 because the conversion may come from a baseclass.  */
  if (TREE_CODE (type) == INTEGER_TYPE
      && TYPE_HAS_INT_CONVERSION (basetype)
      && CLASSTYPE_CONVERSION (basetype, int_conv) != 0
      && CLASSTYPE_CONVERSION (basetype, int_conv) != error_mark_node)
    {
      typename = DECL_NAME (CLASSTYPE_CONVERSION (basetype, int_conv));
      return build_type_conversion_1 (xtype, basetype, expr, typename, for_sure);
    }
  if (TREE_CODE (type) == REAL_TYPE
      && TYPE_HAS_REAL_CONVERSION (basetype)
      && CLASSTYPE_CONVERSION (basetype, real_conv) != 0
      && CLASSTYPE_CONVERSION (basetype, real_conv) != error_mark_node)
    {
      typename = DECL_NAME (CLASSTYPE_CONVERSION (basetype, real_conv));
      return build_type_conversion_1 (xtype, basetype, expr, typename, for_sure);
    }

  /* THIS IS A KLUDGE.  */
  if (TREE_CODE (type) != POINTER_TYPE
      && (code == TRUTH_ANDIF_EXPR
	  || code == TRUTH_ORIF_EXPR
	  || code == TRUTH_NOT_EXPR))
    {
      /* Here's when we can convert to a pointer.  */
      type = ptr_type_node;
      goto try_pointer;
    }

  /* THESE ARE TOTAL KLUDGES.  */
  /* Default promotion yields no new alternatives, try
     conversions which are anti-default, such as

     double -> float or int -> unsigned or unsigned -> long

     */
  if (type_default == type
      && (TREE_CODE (type) == INTEGER_TYPE || TREE_CODE (type) == REAL_TYPE))
    {
      int not_again = 0;

      if (type == double_type_node)
	typename = build_typename_overload (float_type_node);
      else if (type == integer_type_node)
	typename = build_typename_overload (unsigned_type_node);
      else if (type == unsigned_type_node)
	typename = build_typename_overload (long_integer_type_node);

    again:
      basetype = save_basetype;
      while (TYPE_HAS_CONVERSION (basetype))
	{
	  if (lookup_fnfields (TYPE_BINFO (basetype), typename, 0))
	    return build_type_conversion_1 (xtype, basetype, expr, typename, for_sure);
	  if (TYPE_BINFO_BASETYPES (basetype))
	    basetype = TYPE_BINFO_BASETYPE (basetype, 0);
	  else break;
	}
      if (! not_again)
	{
	  if (type == integer_type_node)
	    {
	      typename = build_typename_overload (long_integer_type_node);
	      not_again = 1;
	      goto again;
	    }
	  else
	    {
	      typename = build_typename_overload (integer_type_node);
	      not_again = 1;
	      goto again;
	    }
	}
    }

  /* Now, try C promotions...

     float -> int
     int -> float, void *
     void * -> int

     Truthvalue conversions let us try to convert
     to pointer if we were going for int, and to int
     if we were looking for pointer.  */

    basetype = save_basetype;
    if (TREE_CODE (type) == REAL_TYPE
	|| (TREE_CODE (type) == POINTER_TYPE
	    && (code == TRUTH_ANDIF_EXPR
		|| code == TRUTH_ORIF_EXPR
		|| code == TRUTH_NOT_EXPR)))
      type = integer_type_node;
    else if (TREE_CODE (type) == INTEGER_TYPE)
      if (TYPE_HAS_REAL_CONVERSION (basetype))
	type = double_type_node;
      else
	return NULL_TREE;
    else
      return NULL_TREE;

    typename = build_typename_overload (type);
    while (TYPE_HAS_CONVERSION (basetype))
      {
	if (lookup_fnfields (TYPE_BINFO (basetype), typename, 0))
	  {
	    rval = build_type_conversion_1 (xtype, basetype, expr, typename, for_sure);
	    return rval;
	  }
	if (TYPE_BINFO_BASETYPES (basetype))
	  basetype = TYPE_BINFO_BASETYPE (basetype, 0);
	else
	  break;
      }

  return NULL_TREE;
}

/* Must convert two aggregate types to non-aggregate type.
   Attempts to find a non-ambiguous, "best" type conversion.

   Return 1 on success, 0 on failure.

   @@ What are the real semantics of this supposed to be??? */
int
build_default_binary_type_conversion (code, arg1, arg2)
     enum tree_code code;
     tree *arg1, *arg2;
{
  tree type1 = TREE_TYPE (*arg1);
  tree type2 = TREE_TYPE (*arg2);
  char *name1, *name2;

  if (TREE_CODE (type1) == REFERENCE_TYPE)
    type1 = TREE_TYPE (type1);
  if (TREE_CODE (type2) == REFERENCE_TYPE)
    type2 = TREE_TYPE (type2);

  if (TREE_CODE (TYPE_NAME (type1)) != TYPE_DECL)
    {
      tree decl = typedecl_for_tag (type1);
      if (decl)
	error ("type conversion nonexistent for type `%s'",
	       IDENTIFIER_POINTER (DECL_NAME (decl)));
      else
	error ("type conversion nonexistent for non-C++ type");
      return 0;
    }
  if (TREE_CODE (TYPE_NAME (type2)) != TYPE_DECL)
    {
      tree decl = typedecl_for_tag (type2);
      if (decl)
	error ("type conversion nonexistent for type `%s'",
	       IDENTIFIER_POINTER (decl));
      else
	error ("type conversion nonexistent for non-C++ type");
      return 0;
    }

  name1 = TYPE_NAME_STRING (type1);
  name2 = TYPE_NAME_STRING (type2);

  if (!IS_AGGR_TYPE (type1) || !TYPE_HAS_CONVERSION (type1))
    {
      if (!IS_AGGR_TYPE (type2) || !TYPE_HAS_CONVERSION (type2))
	error ("type conversion required for binary operation on types `%s' and `%s'",
	       name1, name2);
      else
	error ("type conversion required for type `%s'", name1);
      return 0;
    }
  else if (!IS_AGGR_TYPE (type2) || !TYPE_HAS_CONVERSION (type2))
    {
      error ("type conversion required for type `%s'", name2);
      return 0;
    }

  if (TYPE_HAS_INT_CONVERSION (type1) && TYPE_HAS_REAL_CONVERSION (type1))
    warning ("ambiguous type conversion for type `%s', defaulting to int", name1);
  if (TYPE_HAS_INT_CONVERSION (type1))
    {
      *arg1 = build_type_conversion (code, integer_type_node, *arg1, 1);
      *arg2 = build_type_conversion (code, integer_type_node, *arg2, 1);
    }
  else if (TYPE_HAS_REAL_CONVERSION (type1))
    {
      *arg1 = build_type_conversion (code, double_type_node, *arg1, 1);
      *arg2 = build_type_conversion (code, double_type_node, *arg2, 1);
    }
  else
    {
      *arg1 = build_type_conversion (code, ptr_type_node, *arg1, 1);
      if (*arg1 == error_mark_node)
	error ("ambiguous pointer conversion");
      *arg2 = build_type_conversion (code, ptr_type_node, *arg2, 1);
      if (*arg1 != error_mark_node && *arg2 == error_mark_node)
	error ("ambiguous pointer conversion");
    }
  if (*arg1 == 0)
    {
      if (*arg2 == 0 && type1 != type2)
	error ("default type conversion for types `%s' and `%s' failed",
	       name1, name2);
      else
	error ("default type conversion for type `%s' failed", name1);
      return 0;
    }
  else if (*arg2 == 0)
    {
      error ("default type conversion for type `%s' failed", name2);
      return 0;
    }
  return 1;
}

/* Must convert two aggregate types to non-aggregate type.
   Attempts to find a non-ambiguous, "best" type conversion.

   Return 1 on success, 0 on failure.

   The type of the argument is expected to be of aggregate type here.

   @@ What are the real semantics of this supposed to be??? */
int
build_default_unary_type_conversion (code, arg)
     enum tree_code code;
     tree *arg;
{
  tree type = TREE_TYPE (*arg);
  tree id = TREE_CODE (TYPE_NAME (type)) == TYPE_DECL
    ? TYPE_IDENTIFIER (type) : TYPE_NAME (type);
  char *name = IDENTIFIER_POINTER (id);

  if (! TYPE_HAS_CONVERSION (type))
    {
      error ("type conversion required for type `%s'", name);
      return 0;
    }

  if (TYPE_HAS_INT_CONVERSION (type) && TYPE_HAS_REAL_CONVERSION (type))
    warning ("ambiguous type conversion for type `%s', defaulting to int", name);
  if (TYPE_HAS_INT_CONVERSION (type))
    *arg = build_type_conversion (code, integer_type_node, *arg, 1);
  else if (TYPE_HAS_REAL_CONVERSION (type))
    *arg = build_type_conversion (code, double_type_node, *arg, 1);
  else
    {
      *arg = build_type_conversion (code, ptr_type_node, *arg, 1);
      if (*arg == error_mark_node)
	error ("ambiguous pointer conversion");
    }
  if (*arg == 0)
    {
      error ("default type conversion for type `%s' failed", name);
      return 0;
    }
  return 1;
}
