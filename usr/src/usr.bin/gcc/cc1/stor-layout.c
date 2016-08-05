/* C-compiler utilities for types and variables storage layout
   Copyright (C) 1987, 1988, 1992 Free Software Foundation, Inc.

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


#include "config.h"
#include <stdio.h>

#include "tree.h"
#include "function.h"

#define CEIL(x,y) (((x) + (y) - 1) / (y))

/* Data type for the expressions representing sizes of data types.
   It is the first integer type laid out.
   In C, this is int.  */

tree sizetype;

/* An integer constant with value 0 whose type is sizetype.  */

tree size_zero_node;

/* An integer constant with value 1 whose type is sizetype.  */

tree size_one_node;

/* If nonzero, this is an upper limit on alignment of structure fields.
   The value is measured in bits.  */
int maximum_field_alignment;

#define GET_MODE_ALIGNMENT(MODE)   \
  MIN (BIGGEST_ALIGNMENT, 	   \
       MAX (1, (GET_MODE_UNIT_SIZE (MODE) * BITS_PER_UNIT)))

/* SAVE_EXPRs for sizes of types and decls, waiting to be expanded.  */

static tree pending_sizes;

/* Nonzero means cannot safely call expand_expr now,
   so put variable sizes onto `pending_sizes' instead.  */

int immediate_size_expand;

tree
get_pending_sizes ()
{
  tree chain = pending_sizes;
  tree t;

  /* Put each SAVE_EXPR into the current function.  */
  for (t = chain; t; t = TREE_CHAIN (t))
    SAVE_EXPR_CONTEXT (TREE_VALUE (t)) = current_function_decl;
  pending_sizes = 0;
  return chain;
}

/* Given a size SIZE that isn't constant, return a SAVE_EXPR
   to serve as the actual size-expression for a type or decl.  */

tree
variable_size (size)
     tree size;
{
  size = save_expr (size);

  if (global_bindings_p ())
    {
      if (TREE_CONSTANT (size))
	error ("type size can't be explicitly evaluated");
      else
	error ("variable-size type declared outside of any function");

      return size_int (1);
    }

  if (immediate_size_expand)
    /* NULL_RTX is not defined; neither is the rtx type. 
       Also, we would like to pass const0_rtx here, but don't have it.  */
    expand_expr (size, expand_expr (integer_zero_node, NULL_PTR, VOIDmode, 0),
		 VOIDmode, 0);
  else
    pending_sizes = tree_cons (NULL_TREE, size, pending_sizes);

  return size;
}

#ifndef MAX_FIXED_MODE_SIZE
#define MAX_FIXED_MODE_SIZE GET_MODE_BITSIZE (DImode)
#endif

/* Return the machine mode to use for a nonscalar of SIZE bits.
   The mode must be in class CLASS, and have exactly that many bits.
   If LIMIT is nonzero, modes of wider than MAX_FIXED_MODE_SIZE will not
   be used.  */

enum machine_mode
mode_for_size (size, class, limit)
     unsigned int size;
     enum mode_class class;
     int limit;
{
  register enum machine_mode mode;

  if (limit && size > MAX_FIXED_MODE_SIZE)
    return BLKmode;

  /* Get the last mode which has this size, in the specified class.  */
  for (mode = GET_CLASS_NARROWEST_MODE (class); mode != VOIDmode;
       mode = GET_MODE_WIDER_MODE (mode))
    if (GET_MODE_BITSIZE (mode) == size)
      return mode;

  return BLKmode;
}

/* Return the value of VALUE, rounded up to a multiple of DIVISOR.  */

tree
round_up (value, divisor)
     tree value;
     int divisor;
{
  return size_binop (MULT_EXPR,
		     size_binop (CEIL_DIV_EXPR, value, size_int (divisor)),
		     size_int (divisor));
}

/* Set the size, mode and alignment of a ..._DECL node.
   TYPE_DECL does need this for C++.
   Note that LABEL_DECL and CONST_DECL nodes do not need this,
   and FUNCTION_DECL nodes have them set up in a special (and simple) way.
   Don't call layout_decl for them.

   KNOWN_ALIGN is the amount of alignment we can assume this
   decl has with no special effort.  It is relevant only for FIELD_DECLs
   and depends on the previous fields.
   All that matters about KNOWN_ALIGN is which powers of 2 divide it.
   If KNOWN_ALIGN is 0, it means, "as much alignment as you like":
   the record will be aligned to suit.  */

void
layout_decl (decl, known_align)
     tree decl;
     unsigned known_align;
{
  register tree type = TREE_TYPE (decl);
  register enum tree_code code = TREE_CODE (decl);
  int spec_size = DECL_FIELD_SIZE (decl);

  if (code == CONST_DECL)
    return;

  if (code != VAR_DECL && code != PARM_DECL && code != RESULT_DECL
      && code != FIELD_DECL && code != TYPE_DECL)
    abort ();

  if (type == error_mark_node)
    {
      type = void_type_node;
      spec_size = 0;
    }

  /* Usually the size and mode come from the data type without change.  */

  DECL_MODE (decl) = TYPE_MODE (type);
  DECL_SIZE (decl) = TYPE_SIZE (type);
  TREE_UNSIGNED (decl) = TREE_UNSIGNED (type);

  if (code == FIELD_DECL && DECL_BIT_FIELD (decl))
    {
      /* This is a bit-field.  We don't know how to handle
	 them except for integers and enums, and front end should
	 never generate them otherwise.  */

      if (! (TREE_CODE (type) == INTEGER_TYPE
	     || TREE_CODE (type) == ENUMERAL_TYPE))
	abort ();

      if (spec_size == 0 && DECL_NAME (decl) != 0)
	abort ();

      /* Size is specified number of bits.  */
      DECL_SIZE (decl) = size_int (spec_size);
    }
  /* Force alignment required for the data type.
     But if the decl itself wants greater alignment, don't override that.
     Likewise, if the decl is packed, don't override it.  */
  else if (DECL_ALIGN (decl) == 0
	   || (! DECL_PACKED (decl) &&  TYPE_ALIGN (type) > DECL_ALIGN (decl)))
    DECL_ALIGN (decl) = TYPE_ALIGN (type);

  /* See if we can use an ordinary integer mode for a bit-field.  */
  /* Conditions are: a fixed size that is correct for another mode
     and occupying a complete byte or bytes on proper boundary.  */
  if (code == FIELD_DECL)
    {
      DECL_BIT_FIELD_TYPE (decl) = DECL_BIT_FIELD (decl) ? type : 0;
      if (maximum_field_alignment != 0)
	DECL_ALIGN (decl) = MIN (DECL_ALIGN (decl), maximum_field_alignment);
    }

  if (DECL_BIT_FIELD (decl)
      && TYPE_SIZE (type) != 0
      && TREE_CODE (TYPE_SIZE (type)) == INTEGER_CST)
    {
      register enum machine_mode xmode
	= mode_for_size (TREE_INT_CST_LOW (DECL_SIZE (decl)), MODE_INT, 1);

      if (xmode != BLKmode
	  && known_align % GET_MODE_ALIGNMENT (xmode) == 0)
	{
	  DECL_ALIGN (decl) = MAX (GET_MODE_ALIGNMENT (xmode),
				   DECL_ALIGN (decl));
	  DECL_MODE (decl) = xmode;
	  DECL_SIZE (decl) = size_int (GET_MODE_BITSIZE (xmode));
	  /* This no longer needs to be accessed as a bit field.  */
	  DECL_BIT_FIELD (decl) = 0;
	}
    }

  /* Evaluate nonconstant size only once, either now or as soon as safe.  */
  if (DECL_SIZE (decl) != 0 && TREE_CODE (DECL_SIZE (decl)) != INTEGER_CST)
    DECL_SIZE (decl) = variable_size (DECL_SIZE (decl));
}

/* Lay out a RECORD_TYPE type (a C struct).
   This means laying out the fields, determining their positions,
   and computing the overall size and required alignment of the record.
   Note that if you set the TYPE_ALIGN before calling this
   then the struct is aligned to at least that boundary.

   If the type has basetypes, you must call layout_basetypes
   before calling this function.

   The return value is a list of static members of the record.
   They still need to be laid out.  */

static tree
layout_record (rec)
     tree rec;
{
  register tree field;
#ifdef STRUCTURE_SIZE_BOUNDARY
  unsigned record_align = MAX (STRUCTURE_SIZE_BOUNDARY, TYPE_ALIGN (rec));
#else
  unsigned record_align = MAX (BITS_PER_UNIT, TYPE_ALIGN (rec));
#endif
  /* These must be laid out *after* the record is.  */
  tree pending_statics = NULL_TREE;
  /* Record size so far is CONST_SIZE + VAR_SIZE bits,
     where CONST_SIZE is an integer
     and VAR_SIZE is a tree expression.
     If VAR_SIZE is null, the size is just CONST_SIZE.
     Naturally we try to avoid using VAR_SIZE.  */
  register int const_size = 0;
  register tree var_size = 0;
  /* Once we start using VAR_SIZE, this is the maximum alignment
     that we know VAR_SIZE has.  */
  register int var_align = BITS_PER_UNIT;


  for (field = TYPE_FIELDS (rec); field; field = TREE_CHAIN (field))
    {
      register int desired_align;

      /* If FIELD is static, then treat it like a separate variable,
	 not really like a structure field.
	 If it is a FUNCTION_DECL, it's a method.
	 In both cases, all we do is lay out the decl,
	 and we do it *after* the record is laid out.  */

      if (TREE_STATIC (field))
	{
	  pending_statics = tree_cons (NULL_TREE, field, pending_statics);
	  continue;
	}
      /* Enumerators and enum types which are local to this class need not
	 be laid out.  Likewise for initialized constant fields.  */
      if (TREE_CODE (field) != FIELD_DECL)
	continue;

      /* Lay out the field so we know what alignment it needs.
	 For KNOWN_ALIGN, pass the number of bits from start of record
	 or some divisor of it.  */

      /* For a packed field, use the alignment as specified,
	 disregarding what the type would want.  */
      if (DECL_PACKED (field))
	desired_align = DECL_ALIGN (field);
      layout_decl (field, var_size ? var_align : const_size);
      if (! DECL_PACKED (field))
	desired_align = DECL_ALIGN (field);
      /* Some targets (i.e. VMS) limit struct field alignment
	 to a lower boundary than alignment of variables.  */
#ifdef BIGGEST_FIELD_ALIGNMENT
      desired_align = MIN (desired_align, BIGGEST_FIELD_ALIGNMENT);
#endif

      /* Record must have at least as much alignment as any field.
	 Otherwise, the alignment of the field within the record
	 is meaningless.  */

#ifndef PCC_BITFIELD_TYPE_MATTERS
      record_align = MAX (record_align, desired_align);
#else
      if (PCC_BITFIELD_TYPE_MATTERS && TREE_TYPE (field) != error_mark_node
	  && DECL_BIT_FIELD_TYPE (field)
	  && ! integer_zerop (TYPE_SIZE (TREE_TYPE (field))))
	{
	  /* For these machines, a zero-length field does not
	     affect the alignment of the structure as a whole.
	     It does, however, affect the alignment of the next field
	     within the structure.  */
	  if (! integer_zerop (DECL_SIZE (field)))
	    record_align = MAX (record_align, desired_align);
	  else if (! DECL_PACKED (field))
	    desired_align = TYPE_ALIGN (TREE_TYPE (field));
	  /* A named bit field of declared type `int'
	     forces the entire structure to have `int' alignment.  */
	  if (DECL_NAME (field) != 0)
	    {
	      int type_align = TYPE_ALIGN (TREE_TYPE (field));
	      if (maximum_field_alignment != 0)
		type_align = MIN (type_align, maximum_field_alignment);

	      record_align = MAX (record_align, type_align);
	    }
	}
      else
	record_align = MAX (record_align, desired_align);
#endif

      /* Does this field automatically have alignment it needs
	 by virtue of the fields that precede it and the record's
	 own alignment?  */

      if (const_size % desired_align != 0
	  || (var_align % desired_align != 0
	      && var_size != 0))
	{
	  /* No, we need to skip space before this field.
	     Bump the cumulative size to multiple of field alignment.  */

	  if (var_size == 0
	      || var_align % desired_align == 0)
	    const_size
	      = CEIL (const_size, desired_align) * desired_align;
	  else
	    {
	      if (const_size > 0)
		var_size = size_binop (PLUS_EXPR, var_size,
				       size_int (const_size));
	      const_size = 0;
	      var_size = round_up (var_size, desired_align);
	      var_align = MIN (var_align, desired_align);
	    }
	}

#ifdef PCC_BITFIELD_TYPE_MATTERS
      if (PCC_BITFIELD_TYPE_MATTERS
	  && TREE_CODE (field) == FIELD_DECL
	  && TREE_TYPE (field) != error_mark_node
	  && DECL_BIT_FIELD_TYPE (field)
	  && !DECL_PACKED (field)
	  && !integer_zerop (DECL_SIZE (field)))
	{
	  int type_align = TYPE_ALIGN (TREE_TYPE (field));
	  register tree dsize = DECL_SIZE (field);
	  int field_size = TREE_INT_CST_LOW (dsize);

	  if (maximum_field_alignment != 0)
	    type_align = MIN (type_align, maximum_field_alignment);

	  /* A bit field may not span the unit of alignment of its type.
	     Advance to next boundary if necessary.  */
	  /* ??? There is some uncertainty here as to what
	     should be done if type_align is less than the width of the type.
	     That can happen because the width exceeds BIGGEST_ALIGNMENT
	     or because it exceeds maximum_field_alignment.  */
	  if (const_size / type_align
	      != (const_size + field_size - 1) / type_align)
	    const_size = CEIL (const_size, type_align) * type_align;
	}
#endif

/* No existing machine description uses this parameter.
   So I have made it in this aspect identical to PCC_BITFIELD_TYPE_MATTERS.  */
#ifdef BITFIELD_NBYTES_LIMITED
      if (BITFIELD_NBYTES_LIMITED
	  && TREE_CODE (field) == FIELD_DECL
	  && TREE_TYPE (field) != error_mark_node
	  && DECL_BIT_FIELD_TYPE (field)
	  && !DECL_PACKED (field)
	  && !integer_zerop (DECL_SIZE (field)))
	{
	  int type_align = TYPE_ALIGN (TREE_TYPE (field));
	  register tree dsize = DECL_SIZE (field);
	  int field_size = TREE_INT_CST_LOW (dsize);

	  if (maximum_field_alignment != 0)
	    type_align = MIN (type_align, maximum_field_alignment);

	  /* A bit field may not span the unit of alignment of its type.
	     Advance to next boundary if necessary.  */
	  if (const_size / type_align
	      != (const_size + field_size - 1) / type_align)
	    const_size = CEIL (const_size, type_align) * type_align;
	}
#endif

      /* Size so far becomes the position of this field.  */

      if (var_size && const_size)
	DECL_FIELD_BITPOS (field)
	  = size_binop (PLUS_EXPR, var_size, size_int (const_size));
      else if (var_size)
	DECL_FIELD_BITPOS (field) = var_size;
      else
	DECL_FIELD_BITPOS (field) = size_int (const_size);

      /* If this field is an anonymous union,
	 give each union-member the same position as the union has.

	 ??? This is a real kludge because it makes the structure
	 of the types look strange.  This feature is only used by
	 C++, which should have build_component_ref build two
	 COMPONENT_REF operations, one for the union and one for
	 the inner field.  We set the offset of this field to zero
	 so that either the old or the correct method will work.
	 Setting DECL_FIELD_CONTEXT is wrong unless the inner fields are
	 moved into the type of this field, but nothing seems to break
	 by doing this.  This kludge should be removed after 2.4.  */

      if (DECL_NAME (field) == 0
	  && TREE_CODE (TREE_TYPE (field)) == UNION_TYPE)
	{
	  tree uelt = TYPE_FIELDS (TREE_TYPE (field));
	  for (; uelt; uelt = TREE_CHAIN (uelt))
	    {
	      DECL_FIELD_CONTEXT (uelt) = DECL_FIELD_CONTEXT (field);
	      DECL_FIELD_BITPOS (uelt) = DECL_FIELD_BITPOS (field);
	    }

	  DECL_FIELD_BITPOS (field) = integer_zero_node;
	}

      /* Now add size of this field to the size of the record.  */

      {
        register tree dsize = DECL_SIZE (field);

	/* This can happen when we have an invalid nested struct definition,
	   such as struct j { struct j { int i; } }.  The error message is
	   printed in finish_struct.  */
	if (dsize == 0)
	  /* Do nothing.  */;
	else if (TREE_CODE (dsize) == INTEGER_CST
		 && TREE_INT_CST_HIGH (dsize) == 0
		 && TREE_INT_CST_LOW (dsize) + const_size > const_size)
	  /* Use const_size if there's no overflow.  */
	  const_size += TREE_INT_CST_LOW (dsize);
	else
	  {
	    if (var_size == 0)
	      var_size = dsize;
	    else
	      var_size = size_binop (PLUS_EXPR, var_size, dsize);
	  }
      }
    }

  /* Work out the total size and alignment of the record
     as one expression and store in the record type.
     Round it up to a multiple of the record's alignment.  */

  if (var_size == 0)
    {
      TYPE_SIZE (rec) = size_int (const_size);
    }
  else
    {
      if (const_size)
	var_size
	  = size_binop (PLUS_EXPR, var_size, size_int (const_size));
      TYPE_SIZE (rec) = var_size;
    }

  /* Determine the desired alignment.  */
#ifdef ROUND_TYPE_ALIGN
  TYPE_ALIGN (rec) = ROUND_TYPE_ALIGN (rec, TYPE_ALIGN (rec), record_align);
#else
  TYPE_ALIGN (rec) = MAX (TYPE_ALIGN (rec), record_align);
#endif

#ifdef ROUND_TYPE_SIZE
  TYPE_SIZE (rec) = ROUND_TYPE_SIZE (rec, TYPE_SIZE (rec), TYPE_ALIGN (rec));
#else
  /* Round the size up to be a multiple of the required alignment */
  TYPE_SIZE (rec) = round_up (TYPE_SIZE (rec), TYPE_ALIGN (rec));
#endif

  return pending_statics;
}

/* Lay out a UNION_TYPE or QUAL_UNION_TYPE type.
   Lay out all the fields, set their positions to zero,
   and compute the size and alignment of the union (maximum of any field).
   Note that if you set the TYPE_ALIGN before calling this
   then the union align is aligned to at least that boundary.  */

static void
layout_union (rec)
     tree rec;
{
  register tree field;
#ifdef STRUCTURE_SIZE_BOUNDARY
  unsigned union_align = STRUCTURE_SIZE_BOUNDARY;
#else
  unsigned union_align = BITS_PER_UNIT;
#endif

  /* The size of the union, based on the fields scanned so far,
     is max (CONST_SIZE, VAR_SIZE).
     VAR_SIZE may be null; then CONST_SIZE by itself is the size.  */
  register int const_size = 0;
  register tree var_size = 0;

  /* If this is a QUAL_UNION_TYPE, we want to process the fields in
     the reverse order in building the COND_EXPR that denotes its
     size.  We reverse them again later.  */
  if (TREE_CODE (rec) == QUAL_UNION_TYPE)
    TYPE_FIELDS (rec) = nreverse (TYPE_FIELDS (rec));

  for (field = TYPE_FIELDS (rec); field; field = TREE_CHAIN (field))
    {
      /* Enums which are local to this class need not be laid out.  */
      if (TREE_CODE (field) == CONST_DECL || TREE_CODE (field) == TYPE_DECL)
	continue;

      layout_decl (field, 0);
      DECL_FIELD_BITPOS (field) = size_int (0);

      /* Union must be at least as aligned as any field requires.  */

      union_align = MAX (union_align, DECL_ALIGN (field));

#ifdef PCC_BITFIELD_TYPE_MATTERS
      /* On the m88000, a bit field of declare type `int'
	 forces the entire union to have `int' alignment.  */
      if (PCC_BITFIELD_TYPE_MATTERS && DECL_BIT_FIELD_TYPE (field))
	union_align = MAX (union_align, TYPE_ALIGN (TREE_TYPE (field)));
#endif

      if (TREE_CODE (rec) == UNION_TYPE)
	{
	  /* Set union_size to max (decl_size, union_size).
	     There are more and less general ways to do this.
	     Use only CONST_SIZE unless forced to use VAR_SIZE.  */

	  if (TREE_CODE (DECL_SIZE (field)) == INTEGER_CST)
	    const_size
	      = MAX (const_size, TREE_INT_CST_LOW (DECL_SIZE (field)));
	  else if (var_size == 0)
	    var_size = DECL_SIZE (field);
	  else
	    var_size = size_binop (MAX_EXPR, var_size, DECL_SIZE (field));
	}
      else if (TREE_CODE (rec) == QUAL_UNION_TYPE)
	var_size = fold (build (COND_EXPR, sizetype, DECL_QUALIFIER (field),
				DECL_SIZE (field),
				var_size ? var_size : integer_zero_node));
      }

  if (TREE_CODE (rec) == QUAL_UNION_TYPE)
    TYPE_FIELDS (rec) = nreverse (TYPE_FIELDS (rec));

  /* Determine the ultimate size of the union (in bytes).  */
  if (NULL == var_size)
    TYPE_SIZE (rec) = size_int (CEIL (const_size, BITS_PER_UNIT)
				* BITS_PER_UNIT);
  else if (const_size == 0)
    TYPE_SIZE (rec) = var_size;
  else
    TYPE_SIZE (rec) = size_binop (MAX_EXPR, var_size,
				  round_up (size_int (const_size),
					    BITS_PER_UNIT));

  /* Determine the desired alignment.  */
#ifdef ROUND_TYPE_ALIGN
  TYPE_ALIGN (rec) = ROUND_TYPE_ALIGN (rec, TYPE_ALIGN (rec), union_align);
#else
  TYPE_ALIGN (rec) = MAX (TYPE_ALIGN (rec), union_align);
#endif

#ifdef ROUND_TYPE_SIZE
  TYPE_SIZE (rec) = ROUND_TYPE_SIZE (rec, TYPE_SIZE (rec), TYPE_ALIGN (rec));
#else
  /* Round the size up to be a multiple of the required alignment */
  TYPE_SIZE (rec) = round_up (TYPE_SIZE (rec), TYPE_ALIGN (rec));
#endif
}

/* Calculate the mode, size, and alignment for TYPE.
   For an array type, calculate the element separation as well.
   Record TYPE on the chain of permanent or temporary types
   so that dbxout will find out about it.

   TYPE_SIZE of a type is nonzero if the type has been laid out already.
   layout_type does nothing on such a type.

   If the type is incomplete, its TYPE_SIZE remains zero.  */

void
layout_type (type)
     tree type;
{
  int old;
  tree pending_statics;

  if (type == 0)
    abort ();

  /* Do nothing if type has been laid out before.  */
  if (TYPE_SIZE (type))
    return;

  /* Make sure all nodes we allocate are not momentary;
     they must last past the current statement.  */
  old = suspend_momentary ();

  /* If we are processing a permanent type, make nodes permanent.
     If processing a temporary type, make it saveable, since the
     type node itself is.  This is important if the function is inline,
     since its decls will get copied later.  */
  push_obstacks_nochange ();
  if (allocation_temporary_p ())
    {
      if (TREE_PERMANENT (type))
	end_temporary_allocation ();
      else
	saveable_allocation ();
    }

  switch (TREE_CODE (type))
    {
    case LANG_TYPE:
      /* This kind of type is the responsibility
	 of the languge-specific code.  */
      abort ();

    case INTEGER_TYPE:
    case ENUMERAL_TYPE:
      if (TREE_INT_CST_HIGH (TYPE_MIN_VALUE (type)) >= 0)
	TREE_UNSIGNED (type) = 1;

      /* We pass 0 for the last arg of mode_for_size because otherwise
	 on the Apollo using long long causes a crash.
	 It seems better to use integer modes than to try to support
	 integer types with BLKmode.  */
      TYPE_MODE (type) = mode_for_size (TYPE_PRECISION (type), MODE_INT, 0);
      TYPE_SIZE (type) = size_int (GET_MODE_BITSIZE (TYPE_MODE (type)));
      break;

    case REAL_TYPE:
      TYPE_MODE (type) = mode_for_size (TYPE_PRECISION (type), MODE_FLOAT, 0);
      TYPE_SIZE (type) = size_int (GET_MODE_BITSIZE (TYPE_MODE (type)));
      break;

    case COMPLEX_TYPE:
      TREE_UNSIGNED (type) = TREE_UNSIGNED (TREE_TYPE (type));
      TYPE_MODE (type)
	= mode_for_size (2 * TYPE_PRECISION (TREE_TYPE (type)),
			 (TREE_CODE (TREE_TYPE (type)) == INTEGER_TYPE
			  ? MODE_COMPLEX_INT : MODE_COMPLEX_FLOAT),
			 0);
      TYPE_SIZE (type) = size_int (GET_MODE_BITSIZE (TYPE_MODE (type)));
      break;

    case VOID_TYPE:
      TYPE_SIZE (type) = size_zero_node;
      TYPE_ALIGN (type) = 1;
      TYPE_MODE (type) = VOIDmode;
      break;

    case OFFSET_TYPE:
      TYPE_SIZE (type) = size_int (GET_MODE_BITSIZE (Pmode));
      TYPE_MODE (type) = Pmode;
      break;

    case FUNCTION_TYPE:
    case METHOD_TYPE:
      TYPE_MODE (type) = mode_for_size (2 * GET_MODE_BITSIZE (Pmode),
					MODE_INT, 0);
      TYPE_SIZE (type) = size_int (GET_MODE_BITSIZE (TYPE_MODE (type)));
      break;

    case POINTER_TYPE:
    case REFERENCE_TYPE:
      TYPE_MODE (type) = Pmode;
      TYPE_SIZE (type) = size_int (GET_MODE_BITSIZE (TYPE_MODE (type)));
      TREE_UNSIGNED (type) = 1;
      TYPE_PRECISION (type) = GET_MODE_BITSIZE (TYPE_MODE (type));
      break;

    case ARRAY_TYPE:
      {
	register tree index = TYPE_DOMAIN (type);
	register tree element = TREE_TYPE (type);

	build_pointer_type (element);

	/* We need to know both bounds in order to compute the size.  */
	if (index && TYPE_MAX_VALUE (index) && TYPE_MIN_VALUE (index)
	    && TYPE_SIZE (element))
	  {
	    tree length
	      = size_binop (PLUS_EXPR, size_one_node,
			    size_binop (MINUS_EXPR, TYPE_MAX_VALUE (index),
					TYPE_MIN_VALUE (index)));

	    TYPE_SIZE (type) = size_binop (MULT_EXPR, length,
					   TYPE_SIZE (element));
	  }

	/* Now round the alignment and size,
	   using machine-dependent criteria if any.  */

#ifdef ROUND_TYPE_ALIGN
	TYPE_ALIGN (type)
	  = ROUND_TYPE_ALIGN (type, TYPE_ALIGN (element), BITS_PER_UNIT);
#else
	TYPE_ALIGN (type) = MAX (TYPE_ALIGN (element), BITS_PER_UNIT);
#endif

#ifdef ROUND_TYPE_SIZE
	if (TYPE_SIZE (type) != 0)
	  TYPE_SIZE (type)
	    = ROUND_TYPE_SIZE (type, TYPE_SIZE (type), TYPE_ALIGN (type));
#endif

	TYPE_MODE (type) = BLKmode;
	if (TYPE_SIZE (type) != 0
	    && TREE_CODE (TYPE_SIZE (type)) == INTEGER_CST
	    /* BLKmode elements force BLKmode aggregate;
	       else extract/store fields may lose.  */
	    && (TYPE_MODE (TREE_TYPE (type)) != BLKmode
		|| TYPE_NO_FORCE_BLK (TREE_TYPE (type))))
	  {
	    TYPE_MODE (type)
	      = mode_for_size (TREE_INT_CST_LOW (TYPE_SIZE (type)),
			       MODE_INT, 1);

	    if (STRICT_ALIGNMENT && TYPE_ALIGN (type) < BIGGEST_ALIGNMENT
		&& TYPE_ALIGN (type) < TREE_INT_CST_LOW (TYPE_SIZE (type))
		&& TYPE_MODE (type) != BLKmode)
	      {
		TYPE_NO_FORCE_BLK (type) = 1;
		TYPE_MODE (type) = BLKmode;
	      }
	  }
	break;
      }

    case RECORD_TYPE:
      pending_statics = layout_record (type);
      TYPE_MODE (type) = BLKmode;
      if (TREE_CODE (TYPE_SIZE (type)) == INTEGER_CST)
	{
	  tree field;
	  /* A record which has any BLKmode members must itself be BLKmode;
	     it can't go in a register.
	     Unless the member is BLKmode only because it isn't aligned.  */
	  for (field = TYPE_FIELDS (type); field; field = TREE_CHAIN (field))
	    {
	      int bitpos;

	      if (TREE_CODE (field) != FIELD_DECL)
		continue;

	      if (TYPE_MODE (TREE_TYPE (field)) == BLKmode
		  && ! TYPE_NO_FORCE_BLK (TREE_TYPE (field)))
		goto record_lose;

	      if (TREE_CODE (DECL_FIELD_BITPOS (field)) != INTEGER_CST)
		goto record_lose;

	      bitpos = TREE_INT_CST_LOW (DECL_FIELD_BITPOS (field));

	      /* Must be BLKmode if any field crosses a word boundary,
		 since extract_bit_field can't handle that in registers.  */
	      if (bitpos / BITS_PER_WORD
		  != ((TREE_INT_CST_LOW (DECL_SIZE (field)) + bitpos - 1)
		      / BITS_PER_WORD)
		  /* But there is no problem if the field is entire words.  */
		  && TREE_INT_CST_LOW (DECL_SIZE (field)) % BITS_PER_WORD == 0)
		goto record_lose;
	    }

	  TYPE_MODE (type)
	    = mode_for_size (TREE_INT_CST_LOW (TYPE_SIZE (type)),
			     MODE_INT, 1);

	  /* If structure's known alignment is less than
	     what the scalar mode would need, and it matters,
	     then stick with BLKmode.  */
	  if (STRICT_ALIGNMENT
	      && ! (TYPE_ALIGN (type) >= BIGGEST_ALIGNMENT
		    || (TYPE_ALIGN (type)
			>= TREE_INT_CST_LOW (TYPE_SIZE (type)))))
	    {
	      if (TYPE_MODE (type) != BLKmode)
		/* If this is the only reason this type is BLKmode,
		   then don't force containing types to be BLKmode.  */
		TYPE_NO_FORCE_BLK (type) = 1;
	      TYPE_MODE (type) = BLKmode;
	    }

	record_lose: ;
	}

      /* Lay out any static members.  This is done now
	 because their type may use the record's type.  */
      while (pending_statics)
	{
	  layout_decl (TREE_VALUE (pending_statics), 0);
	  pending_statics = TREE_CHAIN (pending_statics);
	}
      break;

    case UNION_TYPE:
    case QUAL_UNION_TYPE:
      layout_union (type);
      TYPE_MODE (type) = BLKmode;
      if (TREE_CODE (TYPE_SIZE (type)) == INTEGER_CST
	  /* If structure's known alignment is less than
	     what the scalar mode would need, and it matters,
	     then stick with BLKmode.  */
	  && (! STRICT_ALIGNMENT
	      || TYPE_ALIGN (type) >= BIGGEST_ALIGNMENT
	      || TYPE_ALIGN (type) >= TREE_INT_CST_LOW (TYPE_SIZE (type))))
	{
	  tree field;
	  /* A union which has any BLKmode members must itself be BLKmode;
	     it can't go in a register.
	     Unless the member is BLKmode only because it isn't aligned.  */
	  for (field = TYPE_FIELDS (type); field; field = TREE_CHAIN (field))
	    {
	      if (TREE_CODE (field) != FIELD_DECL)
		continue;

	      if (TYPE_MODE (TREE_TYPE (field)) == BLKmode
		  && ! TYPE_NO_FORCE_BLK (TREE_TYPE (field)))
		goto union_lose;
	    }

	  TYPE_MODE (type)
	    = mode_for_size (TREE_INT_CST_LOW (TYPE_SIZE (type)),
			     MODE_INT, 1);

	union_lose: ;
	}
      break;

    /* Pascal types */
    case BOOLEAN_TYPE:		 /* store one byte/boolean for now. */
      TYPE_MODE (type) = QImode;
      TYPE_SIZE (type) = size_int (GET_MODE_BITSIZE (TYPE_MODE (type)));
      TYPE_PRECISION (type) = 1;
      TYPE_ALIGN (type) = GET_MODE_ALIGNMENT (TYPE_MODE (type));
      break;

    case CHAR_TYPE:
      TYPE_MODE (type) = QImode;
      TYPE_SIZE (type) = size_int (GET_MODE_BITSIZE (TYPE_MODE (type)));
      TYPE_PRECISION (type) = GET_MODE_BITSIZE (TYPE_MODE (type));
      TYPE_ALIGN (type) = GET_MODE_ALIGNMENT (TYPE_MODE (type));
      break;

    case FILE_TYPE:
      /* The size may vary in different languages, so the language front end
	 should fill in the size.  */
      TYPE_ALIGN (type) = BIGGEST_ALIGNMENT;
      TYPE_MODE  (type) = BLKmode;
      break;

    default:
      abort ();
    } /* end switch */

  /* Normally, use the alignment corresponding to the mode chosen.
     However, where strict alignment is not required, avoid
     over-aligning structures, since most compilers do not do this
     alignment.  */

  if (TYPE_MODE (type) != BLKmode && TYPE_MODE (type) != VOIDmode
      && (STRICT_ALIGNMENT
	  || (TREE_CODE (type) != RECORD_TYPE && TREE_CODE (type) != UNION_TYPE
	      && TREE_CODE (type) != QUAL_UNION_TYPE
	      && TREE_CODE (type) != ARRAY_TYPE)))
    TYPE_ALIGN (type) = GET_MODE_ALIGNMENT (TYPE_MODE (type));

  /* Evaluate nonconstant size only once, either now or as soon as safe.  */
  if (TYPE_SIZE (type) != 0 && TREE_CODE (TYPE_SIZE (type)) != INTEGER_CST)
    TYPE_SIZE (type) = variable_size (TYPE_SIZE (type));

  /* Also layout any other variants of the type.  */
  if (TYPE_NEXT_VARIANT (type)
      || type != TYPE_MAIN_VARIANT (type))
    {
      tree variant;
      /* Record layout info of this variant.  */
      tree size = TYPE_SIZE (type);
      int align = TYPE_ALIGN (type);
      enum machine_mode mode = TYPE_MODE (type);

      /* Copy it into all variants.  */
      for (variant = TYPE_MAIN_VARIANT (type);
	   variant;
	   variant = TYPE_NEXT_VARIANT (variant))
	{
	  TYPE_SIZE (variant) = size;
	  TYPE_ALIGN (variant) = align;
	  TYPE_MODE (variant) = mode;
	}
    }
	
  pop_obstacks ();
  resume_momentary (old);
}

/* Create and return a type for signed integers of PRECISION bits.  */

tree
make_signed_type (precision)
     int precision;
{
  register tree type = make_node (INTEGER_TYPE);

  TYPE_PRECISION (type) = precision;

  /* Create the extreme values based on the number of bits.  */

  TYPE_MIN_VALUE (type)
    = build_int_2 ((precision - HOST_BITS_PER_WIDE_INT > 0
		    ? 0 : (HOST_WIDE_INT) (-1) << (precision - 1)),
		   (((HOST_WIDE_INT) (-1)
		     << (precision - HOST_BITS_PER_WIDE_INT - 1 > 0
			 ? precision - HOST_BITS_PER_WIDE_INT - 1
			 : 0))));
  TYPE_MAX_VALUE (type)
    = build_int_2 ((precision - HOST_BITS_PER_WIDE_INT > 0
		    ? -1 : ((HOST_WIDE_INT) 1 << (precision - 1)) - 1),
		   (precision - HOST_BITS_PER_WIDE_INT - 1 > 0
		    ? (((HOST_WIDE_INT) 1
			<< (precision - HOST_BITS_PER_WIDE_INT - 1))) - 1
		    : 0));

  /* Give this type's extreme values this type as their type.  */

  TREE_TYPE (TYPE_MIN_VALUE (type)) = type;
  TREE_TYPE (TYPE_MAX_VALUE (type)) = type;

  /* The first type made with this or `make_unsigned_type'
     is the type for size values.  */

  if (sizetype == 0)
    {
      sizetype = type;
    }

  /* Lay out the type: set its alignment, size, etc.  */

  layout_type (type);

  return type;
}

/* Create and return a type for unsigned integers of PRECISION bits.  */

tree
make_unsigned_type (precision)
     int precision;
{
  register tree type = make_node (INTEGER_TYPE);

  TYPE_PRECISION (type) = precision;

  /* The first type made with this or `make_signed_type'
     is the type for size values.  */

  if (sizetype == 0)
    {
      sizetype = type;
    }

  fixup_unsigned_type (type);
  return type;
}

/* Set the extreme values of TYPE based on its precision in bits,
   then lay it out.  Used when make_signed_type won't do
   because the tree code is not INTEGER_TYPE.
   E.g. for Pascal, when the -fsigned-char option is given.  */

void
fixup_signed_type (type)
     tree type;
{
  register int precision = TYPE_PRECISION (type);

  TYPE_MIN_VALUE (type)
    = build_int_2 ((precision - HOST_BITS_PER_WIDE_INT > 0
		    ? 0 : (HOST_WIDE_INT) (-1) << (precision - 1)),
		   (((HOST_WIDE_INT) (-1)
		     << (precision - HOST_BITS_PER_WIDE_INT - 1 > 0
			 ? precision - HOST_BITS_PER_WIDE_INT - 1
			 : 0))));
  TYPE_MAX_VALUE (type)
    = build_int_2 ((precision - HOST_BITS_PER_WIDE_INT > 0
		    ? -1 : ((HOST_WIDE_INT) 1 << (precision - 1)) - 1),
		   (precision - HOST_BITS_PER_WIDE_INT - 1 > 0
		    ? (((HOST_WIDE_INT) 1
			<< (precision - HOST_BITS_PER_WIDE_INT - 1))) - 1
		    : 0));

  TREE_TYPE (TYPE_MIN_VALUE (type)) = type;
  TREE_TYPE (TYPE_MAX_VALUE (type)) = type;

  /* Lay out the type: set its alignment, size, etc.  */

  layout_type (type);
}

/* Set the extreme values of TYPE based on its precision in bits,
   then lay it out.  This is used both in `make_unsigned_type'
   and for enumeral types.  */

void
fixup_unsigned_type (type)
     tree type;
{
  register int precision = TYPE_PRECISION (type);

  TYPE_MIN_VALUE (type) = build_int_2 (0, 0);
  TYPE_MAX_VALUE (type)
    = build_int_2 (precision - HOST_BITS_PER_WIDE_INT >= 0
		   ? -1 : ((HOST_WIDE_INT) 1 << precision) - 1,
		   precision - HOST_BITS_PER_WIDE_INT > 0
		   ? ((unsigned HOST_WIDE_INT) ~0
		      >> (HOST_BITS_PER_WIDE_INT
			  - (precision - HOST_BITS_PER_WIDE_INT)))
		   : 0);
  TREE_TYPE (TYPE_MIN_VALUE (type)) = type;
  TREE_TYPE (TYPE_MAX_VALUE (type)) = type;

  /* Lay out the type: set its alignment, size, etc.  */

  layout_type (type);
}

/* Find the best machine mode to use when referencing a bit field of length
   BITSIZE bits starting at BITPOS.

   The underlying object is known to be aligned to a boundary of ALIGN bits.
   If LARGEST_MODE is not VOIDmode, it means that we should not use a mode
   larger than LARGEST_MODE (usually SImode).

   If no mode meets all these conditions, we return VOIDmode.  Otherwise, if
   VOLATILEP is true or SLOW_BYTE_ACCESS is false, we return the smallest
   mode meeting these conditions.

   Otherwise (VOLATILEP is false and SLOW_BYTE_ACCESS is true), we return
   the largest mode (but a mode no wider than UNITS_PER_WORD) that meets
   all the conditions.  */

enum machine_mode
get_best_mode (bitsize, bitpos, align, largest_mode, volatilep)
     int bitsize, bitpos;
     int align;
     enum machine_mode largest_mode;
     int volatilep;
{
  enum machine_mode mode;
  int unit;

  /* Find the narrowest integer mode that contains the bit field.  */
  for (mode = GET_CLASS_NARROWEST_MODE (MODE_INT); mode != VOIDmode;
       mode = GET_MODE_WIDER_MODE (mode))
    {
      unit = GET_MODE_BITSIZE (mode);
      if (bitpos / unit == (bitpos + bitsize - 1) / unit)
	break;
    }

  if (mode == MAX_MACHINE_MODE
      /* It is tempting to omit the following line
	 if STRICT_ALIGNMENT is true.
	 But that is incorrect, since if the bitfield uses part of 3 bytes
	 and we use a 4-byte mode, we could get a spurious segv
	 if the extra 4th byte is past the end of memory.
	 (Though at least one Unix compiler ignores this problem:
	 that on the Sequent 386 machine.  */
      || MIN (unit, BIGGEST_ALIGNMENT) > align
      || (largest_mode != VOIDmode && unit > GET_MODE_BITSIZE (largest_mode)))
    return VOIDmode;

  if (SLOW_BYTE_ACCESS && ! volatilep)
    {
      enum machine_mode wide_mode = VOIDmode, tmode;

      for (tmode = GET_CLASS_NARROWEST_MODE (MODE_INT); tmode != VOIDmode;
	   tmode = GET_MODE_WIDER_MODE (tmode))
	{
	  unit = GET_MODE_BITSIZE (tmode);
	  if (bitpos / unit == (bitpos + bitsize - 1) / unit
	      && unit <= BITS_PER_WORD
	      && unit <= MIN (align, BIGGEST_ALIGNMENT)
	      && (largest_mode == VOIDmode
		  || unit <= GET_MODE_BITSIZE (largest_mode)))
	    wide_mode = tmode;
	}

      if (wide_mode != VOIDmode)
	return wide_mode;
    }

  return mode;
}

/* Save all variables describing the current status into the structure *P.
   This is used before starting a nested function.  */

void
save_storage_status (p)
     struct function *p;
{
#if 0  /* Need not save, since always 0 and non0 (resp.) within a function.  */
  p->pending_sizes = pending_sizes;
  p->immediate_size_expand = immediate_size_expand;
#endif /* 0 */
}

/* Restore all variables describing the current status from the structure *P.
   This is used after a nested function.  */

void
restore_storage_status (p)
     struct function *p;
{
#if 0
  pending_sizes = p->pending_sizes;
  immediate_size_expand = p->immediate_size_expand;
#endif /* 0 */
}
