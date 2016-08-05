/* Handle exceptional things in C++.
   Copyright (C) 1989 Free Software Foundation, Inc.
   Contributed by Michael Tiemann (tiemann@mcc.com)

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


/* High-level class interface. */

#define NULL 0
#define EXCEPTION_NAME_PREFIX "__exception_"

#include "config.h"
#include "tree.h"
#include "cplus-tree.h"
#include "flags.h"
#include "assert.h"
/* On Suns this can get you to the right definition if you
   set the right value for TARGET.  */
#include <setjmp.h>
#ifdef sequent
/* Can you believe they forgot this?  */
#define _JBLEN 11
#endif

#ifndef _JBLEN
#define _JBLEN (sizeof(jmp_buf)/sizeof(int))
#endif

void init_exception_processing ();
void init_exception_processing_1 ();

/* If non-zero, a VAR_DECL whose cleanup will cause a throw to the
   next exception handler.  Its value says whether to throw or not.
   In the case of functions which do not issue a RAISE, it should be
   possible to optimize away this VAR_DECL (and overhead associated
   with it).  */
tree exception_throw_decl;
/* Use this to know that we did not set `exception_throw_decl',
   until GCC optimizer is smart enough to figure it out for itself.  */
int sets_exception_throw_decl;

/* The exception `type' currently in scope, or NULL_TREE if none.  */
tree current_exception_type;

/* The exception handler object for the given scope.  */
tree current_exception_decl;

/* The ``object'' view of the current exception parameters.
   We cast up from the `parms' field to `current_exception_type'.  */
tree current_exception_object;

/* Low-level rtl interface.  */
#include "rtl.h"

/* Cache `setjmp', `longjmp', `raise_exception', and `unhandled_exception'
   after default conversion.  Maybe later they will get built-in.  */
static tree BISJ, BILJ, BIR, BIUE;

/* Local variables which give the appearance that exception
   handling is part of the language and the execution model.  */

/* The type of the exception handler stack.  */
static tree EHS_type;

/* The global handler stack.  */
tree EHS_decl;

/* Cached component refs to fields of `EHS_decl'.  */
static tree EHS_prev, EHS_handler, EHS_parms, EHS_name;

/* The parameter names of this exception type.  */

static tree last_exception_fields;
static tree last_exception_field_types;

/* When ID is VOID_TYPE_NODE, it means ``raise all''.
   Cannot be inline, since it uses `alloca', and that
   breaks code which pushes the result of this function
   on the stack.  */
static tree
exception_object_name (prefix, id)
     tree prefix;
     tree id;
{
  /* First, cons up the `name' of this exception.  */
  char *name;
  int length = (id == void_type_node ? 3 : IDENTIFIER_LENGTH (id)) + EXCEPTION_NAME_LENGTH;

  if (prefix)
    length += IDENTIFIER_LENGTH (prefix) + 2;

  name = (char *)alloca (length);
  strcpy (name, EXCEPTION_NAME_PREFIX);
  length = EXCEPTION_NAME_LENGTH;
  if (prefix)
    {
      strcpy (name + length, IDENTIFIER_POINTER (prefix));
      name[length + IDENTIFIER_LENGTH (prefix)] = JOINER;
      length += IDENTIFIER_LENGTH (prefix) + 1;
    }
  if (id == void_type_node)
    strcpy (name + length, "all");
  else
    strcpy (name + length, IDENTIFIER_POINTER (id));
  return get_identifier (name);
}

tree
lookup_exception_cname (ctype, cname, raise_id)
     tree ctype, cname;
     tree raise_id;
{
  tree this_cname = TREE_PURPOSE (raise_id);
  if (this_cname == NULL_TREE)
    {
      if (cname)
	{
	  tree name = TREE_VALUE (raise_id);
	  if (purpose_member (name, CLASSTYPE_TAGS (ctype)))
	    this_cname = cname;
	}
    }
  else if (this_cname == void_type_node)
    this_cname = NULL_TREE;
  else if (TREE_CODE (this_cname) != IDENTIFIER_NODE)
    {
      sorry ("multiple scope refs in `cplus_expand_raise_stmt'");
      this_cname = error_mark_node;
    }
  return this_cname;
}

tree
lookup_exception_tname (oname)
     tree oname;
{
  return get_identifier (IDENTIFIER_POINTER (oname) + EXCEPTION_NAME_LENGTH);
}

tree
lookup_exception_object (cname, name, complain)
     tree cname, name;
     int complain;
{
  tree oname;
  tree decl;

  if (cname == void_type_node)
    cname = NULL_TREE;
  else if (cname && TREE_CODE (cname) != IDENTIFIER_NODE)
    {
      sorry ("multiple scope refs in `lookup_exception_object'");
      cname = NULL_TREE;
    }
  oname = exception_object_name (cname, name);
  decl = IDENTIFIER_GLOBAL_VALUE (oname);
  if (decl == NULL_TREE || TREE_CODE (decl) != VAR_DECL)
    {
      if (complain)
	{
	  int temp = allocation_temporary_p ();
	  if (cname)
	    error ("no exception name object for name `%s::%s'",
		   IDENTIFIER_POINTER (cname),
		   IDENTIFIER_POINTER (name));
	  else
	    error ("no exception name object for name `%s'",
		   IDENTIFIER_POINTER (name));
	  if (temp)
	    end_temporary_allocation ();
	  /* Avoid further error messages.  */
	  pushdecl_top_level (build_lang_field_decl (VAR_DECL,
						     exception_object_name (cname, name),
						     error_mark_node));
	  if (temp)
	    resume_temporary_allocation ();
	}
      return NULL_TREE;
    }
  return decl;
}

tree
lookup_exception_type (ctype, cname, raise_id)
     tree ctype, cname;
     tree raise_id;
{
  tree name = TREE_VALUE (raise_id);
  tree purpose = TREE_PURPOSE (raise_id);

  if (cname && purpose == NULL_TREE)
    purpose = cname;

  if (purpose && purpose != void_type_node)
    {
      tree assoc = NULL_TREE;

      if (TREE_CODE (purpose) != IDENTIFIER_NODE)
	{
	  sorry ("multiple scope refs in `lookup_exception_type'");
	  TREE_PURPOSE (raise_id) = NULL_TREE;
	  return NULL_TREE;
	}
      if (! is_aggr_typedef (purpose, 1))
	return NULL_TREE;
      ctype = TREE_TYPE (TREE_TYPE (purpose));
      assoc = purpose_member (name, CLASSTYPE_TAGS (ctype));
      if (assoc)
	return TREE_VALUE (assoc);
    }

  ctype = lookup_name (name);
  if (ctype && TREE_CODE (ctype) == TYPE_DECL)
    ctype = TREE_TYPE (ctype);
  if (ctype && TREE_CODE (ctype) == RECORD_TYPE
      && CLASSTYPE_DECLARED_EXCEPTION (ctype))
    return ctype;
  return NULL_TREE;
}

tree
finish_exception (e, list_of_fieldlists)
     tree e;
     tree list_of_fieldlists;
{
  tree parmtypes = NULL_TREE, name_field;
  tree cname = TYPE_NAME (e);

  if (TREE_CODE (cname) == TYPE_DECL)
    cname = DECL_NAME (cname);

  if (last_exception_fields)
    error ("cannot declare exceptions within exceptions");
  if (list_of_fieldlists && ! ANON_AGGRNAME_P (cname))
    error_with_aggr_type (e, "exception name `%s' must follow body declaration");
  if (list_of_fieldlists)
    {
      tree prev, field;

      /* Note: no public, private, or protected allowed.  */
      if (TREE_CHAIN (list_of_fieldlists))
	error ("visibility declarations invalid in exception declaration");
      else if (TREE_PURPOSE (list_of_fieldlists) != (tree)visibility_default)
	error ("visibility declarations invalid in exception declaration");
      TREE_PURPOSE (list_of_fieldlists) = (tree)visibility_default;

      /* Note also: no member function declarations allowed.  */
      for (prev = 0, field = TREE_VALUE (list_of_fieldlists);
	   field; prev = field, field = TREE_CHAIN (field))
	{
	  switch (TREE_CODE (field))
	    {
	    case FIELD_DECL:
	      /* ok.  */
	      parmtypes = tree_cons (NULL_TREE, TREE_TYPE (field), parmtypes);
	      continue;
	    case FUNCTION_DECL:
	      error_with_decl (field, "declaration of function `%s' in exception invalid");
	      break;
	    case VAR_DECL:
	      if (TREE_STATIC (field))
		error_with_decl (field, "declaration of static variable `%s' in exception invalid");
	      else
		error_with_decl (field, "declaration of constant field `%s' in exception invalid");
	      break;
	    case CONST_DECL:
	      error_with_decl (field, "declaration of enum value `%s' in exception invalid");
	      break;
	    case SCOPE_REF:
	      error ("use of `::' in exception context invalid");
	      break;
	    }
	  if (prev)
	    TREE_CHAIN (prev) = TREE_CHAIN (field);
	  else
	    TREE_VALUE (list_of_fieldlists) = TREE_CHAIN (field);
	}
    }

  /* Now that we've cleaned up the fields, add a name identifier at front.  */
  name_field = build_lang_field_decl (FIELD_DECL, get_identifier ("__name"),
				      ptr_type_node);
  if (list_of_fieldlists)
    {
      TREE_CHAIN (name_field) = TREE_VALUE (list_of_fieldlists);
      TREE_VALUE (list_of_fieldlists) = name_field;
    }
  else
    list_of_fieldlists = build_tree_list (NULL_TREE, name_field);

  last_exception_fields = TREE_VALUE (list_of_fieldlists);
  if (parmtypes)
    {
      last_exception_field_types = nreverse (parmtypes);
      /* Set the TREE_CHAIN of what is now at the end of the
	 list to `void_list_node'.  */
      TREE_CHAIN (parmtypes) = void_list_node;
    }
  else
    last_exception_field_types = void_list_node;

  popclass (0);

#if 0
  /* Remove aggregate types from the list of tags,
     since these appear at global scope.  */
  while (x && IS_AGGR_TYPE (TREE_VALUE (x)))
    x = TREE_CHAIN (x);
  CLASSTYPE_TAGS (t) = x;
  y = x;
  while (x)
    {
      if (IS_AGGR_TYPE (TREE_VALUE (x)))
	TREE_CHAIN (y) = TREE_CHAIN (x);
      x = TREE_CHAIN (x);
    }
#endif

  if (flag_cadillac)
    cadillac_finish_exception (e);

  return e;
}

void
finish_exception_decl (cname, decl)
     tree cname, decl;
{
  /* In cplus-decl.h.  */
  extern tree last_function_parms;

  /* An exception declaration.  */
  tree t, ctor;
  tree parmdecls = NULL_TREE, fields;
  tree list_of_fieldlists = temp_tree_cons (NULL_TREE,
					    copy_list (last_exception_fields),
					    NULL_TREE);
  tree edecl = build_lang_field_decl (VAR_DECL,
				      exception_object_name (cname, DECL_NAME (decl)),
				      ptr_type_node);

  DECL_LANGUAGE (edecl) = lang_c;
  TREE_STATIC (edecl) = 1;
  TREE_PUBLIC (edecl) = 1;
  finish_decl (pushdecl (edecl), 0, 0);

  /* Now instantiate the exception decl.  */
  t = xref_tag (exception_type_node, DECL_NAME (decl), NULL_TREE);

  /* finish_struct will pop this.  */
  pushclass (t, 0);

  /* Now add a constructor which takes as parameters all the types we
     just defined.  */
  ctor = build_lang_decl (FUNCTION_DECL, DECL_NAME (decl),
			  build_cplus_method_type (t, TYPE_POINTER_TO (t),
						   last_exception_field_types));
  /* Don't take `name'.  The constructor handles that.  */
  fields = TREE_CHAIN (TREE_VALUE (list_of_fieldlists));
  while (fields)
    {
      tree parm = build_decl (PARM_DECL, DECL_NAME (fields), TREE_TYPE (fields));
      /* Since there is a prototype, args are passed in their own types.  */
      DECL_ARG_TYPE (parm) = TREE_TYPE (parm);
#ifdef PROMOTE_PROTOTYPES
      if (TREE_CODE (TREE_TYPE (fields)) == INTEGER_TYPE
	  && TYPE_PRECISION (TREE_TYPE (fields)) < TYPE_PRECISION (integer_type_node))
	DECL_ARG_TYPE (parm) = integer_type_node;
#endif
      TREE_CHAIN (parm) = parmdecls;
      parmdecls = parm;
      fields = TREE_CHAIN (fields);
    }
  fields = TREE_VALUE (list_of_fieldlists);
  last_function_parms = nreverse (parmdecls);

  DECL_CONSTRUCTOR_P (ctor) = 1;
  TYPE_HAS_CONSTRUCTOR (t) = 1;
  grokclassfn (t, DECL_NAME (decl), ctor, NO_SPECIAL, 0, NULL_TREE);
  TREE_EXTERNAL (ctor) = 1;
  TREE_STATIC (ctor) = 1;
  TREE_PUBLIC (ctor) = 0;
  TREE_INLINE (ctor) = 1;
  make_decl_rtl (ctor, 0, 1);
  finish_decl (ctor, NULL_TREE, 0);
  TREE_CHAIN (ctor) = TREE_VALUE (list_of_fieldlists);
  TREE_VALUE (list_of_fieldlists) = ctor;

  finish_struct (t, list_of_fieldlists, 0, 0);

  if (current_function_decl)
    error ("cannot define exception inside function scope");
  else
    {
      /* Now build the constructor for this exception.  */
      parmdecls = DECL_ARGUMENTS (ctor);
      start_function (NULL_TREE, ctor, 0, 1);
      store_parm_decls ();
      pushlevel (0);
      clear_last_expr ();
      push_momentary ();
      expand_start_bindings (0);

      /* Move all the parameters to the fields, skipping `this'.  */
      parmdecls = TREE_CHAIN (parmdecls);
      /* Install `name' of this exception handler.  */
      DECL_INITIAL (fields) = build_unary_op (ADDR_EXPR, edecl, 0);
      fields = TREE_CHAIN (fields);
      /* Install all the values.  */
      while (fields)
	{
	  /* Set up the initialization for this field.  */
	  DECL_INITIAL (fields) = parmdecls;
	  fields = TREE_CHAIN (fields);
	  parmdecls = TREE_CHAIN (parmdecls);
	}
      emit_base_init (t, 0);

      finish_function (DECL_SOURCE_LINE (ctor), 1);
    }
}

void
end_exception_decls ()
{
  last_exception_field_types = NULL_TREE;
  last_exception_fields = NULL_TREE;
}

/* Statement-level exception semantics.  */

void
cplus_expand_start_try (implicit)
     int implicit;
{
  tree call_to_setjmp;
  tree handler, ref;

  /* Start a new block enclosing the whole handler.  */
  if (implicit)
    {
      pushlevel_temporary (1);
    }
  else
    {
      pushlevel (0);
      clear_last_expr ();
      push_momentary ();

      /* Encompass whole exception handler in one big binding contour.
	 If RAISE should throw out of the whole TRY/EXCEPT block, call
	 `expand_start_bindings' with argument of 1.  */
      expand_start_bindings (0);
    }

  /* Allocate handler in that block.  It's real name will come later.
     Note that it will be the first name in this binding contour.  */
  handler = get_temp_name (EHS_type, 0);
  DECL_INITIAL (handler) = error_mark_node;
  finish_decl (handler, NULL_TREE, 0);

  /* Must come after call to `finish_decl', else the cleanup for the temp
     for the handler will cause the contour we just created to be popped.  */
  if (implicit)
    declare_implicit_exception ();

  /* Catch via `setjmp'.  */
  ref = build_component_ref (handler, get_identifier ("handler"), NULL_TREE, 0);
  call_to_setjmp = build_function_call (BISJ, build_tree_list (NULL_TREE, ref));

  /* RAISE throws to EXCEPT part.  */
  expand_start_try (build_binary_op (EQ_EXPR, call_to_setjmp, integer_zero_node), 0, 1);
}

/* If KEEP is 1, then declarations in the TRY statement are worth keeping.
   If KEEP is 2, then the TRY statement was generated by the compiler.
   If KEEP is 0, the declarations in the TRY statement contain errors.  */

tree
cplus_expand_end_try (keep)
     int keep;
{
  tree decls, decl, block;

  if (keep < 2)
    pop_implicit_try_blocks (NULL_TREE);

  decls = getdecls ();

  /* Emit code to avoid falling through into a default
     handler that might come later.  */
  expand_end_try ();

  /* Pops binding contour local to TRY, and get the exception handler
     object built by `...start_try'.  */
  switch (keep)
    {
    case 0:
      expand_end_bindings (decls, 0, 1);
      block = poplevel (0, 0, 0);
      pop_momentary (); 
      decl = getdecls ();
      break;

    case 1:
      expand_end_bindings (decls, 1, 1);
      block = poplevel (1, 1, 0);
      pop_momentary ();
      decl = getdecls ();
      break;

    default:
      decl = tree_last (decls);
      block = NULL_TREE;
      break;
    }

  assert (TREE_CODE (decl) == VAR_DECL && TREE_TYPE (decl) == EHS_type);
  if (block)
    {
      TREE_LANG_FLAG_1 (block) = 1;
      TREE_USED (block) = 1;
    }

  /* Pass it back so that its rtl can be bound to its name
     (or vice versa).  */
  return decl;
}

void
cplus_expand_start_except (name, decl)
     tree name, decl;
{
  int yes;
  tree tmp;

  expand_start_except (0, 1);

  /* This is internal `eh'.  */
  current_exception_decl = decl;
  /* Get the exception object into scope (user declared `ex').  */
  tmp = pushdecl (build_decl (VAR_DECL, name, ptr_type_node));
  DECL_INITIAL (tmp) = error_mark_node;
  finish_decl (tmp, build (COMPONENT_REF, ptr_type_node, decl, TREE_OPERAND (EHS_parms, 1)), 0);
  current_exception_type = NULL_TREE;
  yes = suspend_momentary ();
  /* From now on, send the user to our faked-up object.  */
  current_exception_object = build1 (INDIRECT_REF, void_type_node, tmp);
  IDENTIFIER_LOCAL_VALUE (name) = current_exception_object;
  resume_momentary (yes);

  /* Pop exception handler stack.  */
  expand_assignment (EHS_decl, EHS_prev, 0, 0);
}

/* Generate the call to `unhandled_exception' that is appropriate
   for this particular unhandled exception.  */
static tree
call_to_unhandled_exception ()
{
  extern int lineno;
  tree parms = tree_cons (NULL_TREE,
			  combine_strings (build_string (strlen (input_filename + 1), input_filename)),
			  build_tree_list (NULL_TREE, build_int_2 (lineno, 0)));
  return build_function_call (BIUE, parms);
}

/* Note that this must be mirror image of `...start_try'.
   DFAULT is the default clause, if there was one.
   DFAULT is ERROR_MARK_NODE when this ends an implicit handler.  */
void
cplus_expand_end_except (dfault)
     tree dfault;
{
  extern tree expand_end_except (); /* stmt.c.  */
  tree decls, raised;

  if (dfault == NULL_TREE)
    {
      /* Uncaught exception at outermost level.  If raised locally,
	 reraise the exception.  Otherwise, generate code to call `abort'.  */
      if (in_try_block (1) == 0)
	{
	  expand_start_cond (build (EQ_EXPR, integer_type_node,
				    exception_throw_decl, integer_zero_node), 0);
	  expand_expr (call_to_unhandled_exception (), 0, VOIDmode, 0);
	  expand_end_cond ();
	}
      /* Try the next handler.  */
      if (! expand_escape_except ())
	compiler_error ("except nesting botch");
    }

  raised = expand_end_except ();

  decls = getdecls ();
  expand_end_bindings (decls, decls != 0, 1);
  poplevel (decls != 0, 1, 0);

  /* Implicit handlers do not use the momentary obstack.  */
  if (dfault != error_mark_node)
    pop_momentary ();

  if (! in_try_block (1))
    {
      /* Check that this function is not raising exceptions
	 it is not supposed to.  */
      while (raised)
	{
	  error_with_decl (TREE_VALUE (raised), "exception `%s' raised but not declared raisable");
	  raised = TREE_CHAIN (raised);
	}
    }
  else if (dfault == NULL_TREE || dfault == error_mark_node)
    {
      expand_start_cond (build (NE_EXPR, integer_type_node,
				exception_throw_decl,
				integer_zero_node), 0);
      /* We fell off the end of this try block.  Try going to the next.
	 The escape_label will be the beginning of the next try block.  */
      if (! expand_escape_except ())
	compiler_error ("except nesting botch");
      expand_end_cond ();
    }
}

/* Generate code to raise exception RAISE_ID.
   If EXP is NULL_TREE, then PARMS is the list of parameters to use
   for constructing this exception.
   If EXP is non-NULL, then it is an already constructed object
   of the kind that we want.  */
void
cplus_expand_raise (raise_id, parms, exp)
     tree raise_id;
     tree parms;
     tree exp;
{
  /* Allocate new exception of appropriate type, passing
     PARMS to its constructor.  */
  tree cname, name;
  tree decl;
  tree xexp = exp;

  cname = lookup_exception_cname (current_class_type, current_class_name, raise_id);
  if (cname == error_mark_node)
    return;
  name = TREE_VALUE (raise_id);

  decl = lookup_exception_object (cname, name, 1);
  if (decl == NULL_TREE)
    return;

  if (exp == NULL_TREE)
    {
      exp = build_method_call (NULL_TREE, name, parms, NULL_TREE, LOOKUP_COMPLAIN);
      if (exp == error_mark_node)
	return;
    }

  if (in_try_block (1))
    {
      expand_raise (decl);
    }
  else if (! current_function_decl)
    {
      if (xexp == NULL_TREE)
	error_with_decl (decl, "invalid raise of `%s' outside of functions");
      else
	error_with_decl (decl, "invalid reraise of `%s' outside of functions");
    }
  else
    {
      /* Test this raise against what this function permits.  */
      tree names = TYPE_RAISES_EXCEPTIONS (TREE_TYPE (current_function_decl));
      while (names)
	{
	  if (decl == TREE_TYPE (names))
	    break;
	  names = TREE_CHAIN (names);
	}
      if (names == NULL_TREE)
	{
	  error ("current function not declared to raise exception `%s'",
		 IDENTIFIER_POINTER (name));
	  return;
	}
    }

  expand_assignment (EHS_parms, exp, 0, 0);

  /* Set the global exception handler stack's NAME field
     to the `name' of this exception.  The global exception
     handler stack is the container for the exception object
     we just built.

     We go through a function call to make life easier when debugging.  */
#if 0
  expand_assignment (EHS_name, build_unary_op (ADDR_EXPR, decl, 0), 0, 0);
#else
  parms = tree_cons (NULL_TREE, build_unary_op (ADDR_EXPR, EHS_name, 0),
		     build_tree_list (NULL_TREE,
				      build_unary_op (ADDR_EXPR, decl, 0)));
  expand_expr (build_function_call (BIR, parms));
#endif

  /* Activate thrower.  If we are inside a TRY statement,
     we can cheat and not do this, saving a longjmp.  */
  if (in_try_block (1) == 0)
    {
      sets_exception_throw_decl = 1;
      expand_assignment (exception_throw_decl, integer_one_node, 0, 0);
    }

  if (xexp == NULL_TREE)
    {    
      /* Invoke destructors for current procedure or handler.  */
      if (! expand_escape_except ())
	compiler_error ("except nesting botch");
      /* Throw via `longjmp'... Done as side-effect of goto.  */
    }
  /* If we were re-raising, just let this fall through.
     At the end of the reraises, the call to `expand_goto'
     will take care of everybody.  */
}

tree
cplus_expand_start_catch (raise_id)
     tree raise_id;
{
  tree cname = lookup_exception_cname (current_class_type, current_class_name, raise_id);
  tree decl;
  tree ref, cond;

  if (cname == error_mark_node)
    {
      decl = error_mark_node;
      cond = error_mark_node;
    }
  else
    {
      decl = lookup_exception_object (cname, TREE_VALUE (raise_id), 1);
      if (decl == NULL_TREE)
	{
	  cond = error_mark_node;
	}
      else
	{
	  ref = build (COMPONENT_REF, ptr_type_node,
		       current_exception_decl, TREE_OPERAND (EHS_name, 1));
	  cond = build_binary_op (EQ_EXPR, build_unary_op (ADDR_EXPR, decl, 0), ref);
	}
    }
  expand_start_cond (cond, 0);

  /* Does nothing right now.  */
  expand_catch (decl);
  if (current_exception_type
      && TYPE_NEEDS_DESTRUCTOR (current_exception_type))
    {
      /* Make a cleanup for the name-specific exception object now in scope.  */
      tree cleanup = maybe_build_cleanup (current_exception_object);
      expand_start_bindings (0);
      expand_decl_cleanup (NULL_TREE, cleanup);
    }
  return decl;
}

void
cplus_expand_end_catch (for_reraise)
     int for_reraise;
{
  if (current_exception_type
      && TYPE_NEEDS_DESTRUCTOR (current_exception_type))
    {
      /* Destroy the specific exception object now in scope.  */
      expand_end_bindings (getdecls (), 0, 1);
    }
  if (for_reraise)
    {
      if (! expand_escape_except ())
	abort ();
    }
  else
    {
      if (! expand_end_catch ())
	abort ();
    }
  expand_end_cond ();
}

/* Reraise an exception.
   If EXCEPTIONS is NULL_TREE, it means reraise whatever exception was caught.
   If EXCEPTIONS is an IDENTIFIER_NODE, it means reraise the exception
   object named by EXCEPTIONS.  This must be a variable declared in
   an `except' clause.
   If EXCEPTIONS is a TREE_LIST, it is the list of exceptions we are
   willing to reraise.  */

void
cplus_expand_reraise (exceptions)
     tree exceptions;
{
  tree ex_ptr;
  tree ex_object = current_exception_object;

  if (exceptions && TREE_CODE (exceptions) == IDENTIFIER_NODE)
    {
      /* Don't get tripped up if its TREE_TYPE is `error_mark_node'.  */
      ex_object = IDENTIFIER_LOCAL_VALUE (exceptions);
      if (ex_object == NULL_TREE || TREE_CODE (ex_object) != INDIRECT_REF)
	{
	  error ("`%s' is not an exception decl", IDENTIFIER_POINTER (exceptions));
	  return;
	}
      assert (TREE_CODE (TREE_OPERAND (ex_object, 0)) == VAR_DECL);
      exceptions = NULL_TREE;
    }

  /* reraise ALL, used by compiler.  */
  if (exceptions == NULL_TREE)
    {
      /* Now treat reraise like catch/raise.  */
      expand_catch (error_mark_node);
      expand_raise (error_mark_node);
      expand_assignment (EHS_name,
			 build (COMPONENT_REF, ptr_type_node,
				current_exception_decl, TREE_OPERAND (EHS_name, 1)), 0, 0);
      expand_assignment (EHS_parms,
			 build (COMPONENT_REF, ptr_type_node,
				current_exception_decl, TREE_OPERAND (EHS_parms, 1)), 0, 0);
      if (in_try_block (1) == 0)
	{
	  sets_exception_throw_decl = 1;
	  expand_assignment (exception_throw_decl, integer_one_node, 0, 0);
	}
      /* Set to zero so that destructor will not be called.  */
      expand_assignment (build1 (NOP_EXPR, ptr_type_node, TREE_OPERAND (ex_object, 0)),
			 integer_zero_node, 0, 0);
      if (! expand_escape_except ())
	abort ();
      return;
    }

  ex_ptr = build1 (NOP_EXPR, NULL_TREE, TREE_OPERAND (ex_object, 0));

  /* reraise from a list of exceptions.  */
  while (exceptions)
    {
      tree type = lookup_exception_type (current_class_type, current_class_name,
					 exceptions);
      if (type == NULL_TREE)
	{
	  error ("`%s' is not an exception type",
		 IDENTIFIER_POINTER (TREE_VALUE (exceptions)));
	  current_exception_type = NULL_TREE;
	  TREE_TYPE (ex_object) = error_mark_node;
	  TREE_TYPE (ex_ptr) = error_mark_node;
	}
      else
	{
	  current_exception_type = type;
	  /* In-place union.  */
	  TREE_TYPE (ex_object) = type;
	  TREE_TYPE (ex_ptr) = TYPE_POINTER_TO (type);
	}

      /* Now treat reraise like catch/raise.  */
      cplus_expand_start_catch (exceptions);
      cplus_expand_raise (exceptions, NULL_TREE, ex_ptr);
      /* Set to zero so that destructor will not be called.  */
      if (TREE_TYPE (ex_ptr) != error_mark_node)
	expand_assignment (ex_ptr, integer_zero_node, 0, 0);
      cplus_expand_end_catch (1);
      exceptions = TREE_CHAIN (exceptions);
    }
  /* Don't propagate any unhandled exceptions.  */
  expand_expr (call_to_unhandled_exception (), 0, VOIDmode, 0);
}

void
setup_exception_throw_decl ()
{
  tree call_to_longjmp, parms;

  int old = suspend_momentary ();

  exception_throw_decl = build_decl (VAR_DECL, get_identifier (THROW_NAME), integer_type_node);
  pushdecl (exception_throw_decl);
  parms = tree_cons (NULL_TREE, EHS_handler,
		     build_tree_list (0, integer_one_node));
  call_to_longjmp = build_function_call (BILJ, parms);

  expand_decl (exception_throw_decl);
  expand_decl_cleanup (exception_throw_decl,
		       build (COND_EXPR, void_type_node,
			      exception_throw_decl,
			      call_to_longjmp, integer_zero_node));
  DECL_INITIAL (exception_throw_decl) = integer_zero_node;
  sets_exception_throw_decl = 0;
  resume_momentary (old);
}

void
init_exception_processing ()
{
  extern tree unhandled_exception_fndecl;
  tree cname = get_identifier ("ExceptionHandler");
  tree field, chain;
  tree ctor, dtor;
  tree jmp_buf_type = build_array_type (integer_type_node,
					build_index_type (build_int_2 (_JBLEN-1, 0)));
  tree jmp_buf_arg_type = build_pointer_type (integer_type_node);

  tree parmtypes = hash_tree_chain (jmp_buf_arg_type, NULL_TREE);
  tree setjmp_fndecl, longjmp_fndecl, raise_fndecl;

  EHS_type = xref_tag (record_type_node, cname, NULL_TREE);

  push_lang_context (lang_name_c);
  setjmp_fndecl = define_function ("setjmp",
				   build_function_type (integer_type_node,
							parmtypes),
				   NOT_BUILT_IN, 0);
  BISJ = default_conversion (setjmp_fndecl);
  longjmp_fndecl = define_function ("longjmp",
				    build_function_type (integer_type_node,
							 hash_tree_chain (jmp_buf_arg_type,
									  hash_tree_chain (integer_type_node, NULL_TREE))),
				    NOT_BUILT_IN, 0);
  raise_fndecl = define_function ("__raise_exception",
				  build_function_type (void_type_node,
						       hash_tree_chain (ptr_type_node,
									hash_tree_chain (build_pointer_type (ptr_type_node), NULL_TREE))));
  BILJ = default_conversion (longjmp_fndecl);
  BIR = default_conversion (raise_fndecl);
  BIUE = default_conversion (unhandled_exception_fndecl);

  pop_lang_context ();

  /* finish_struct will pop this.  */
  pushclass (EHS_type, 0);
  field = build_lang_field_decl (FIELD_DECL, get_identifier ("parms"), ptr_type_node);
  chain = field;
  field = build_lang_field_decl (FIELD_DECL, get_identifier ("name"),
				 build_pointer_type (default_function_type));
  TREE_CHAIN (field) = chain;
  chain = field;
  field = build_lang_field_decl (FIELD_DECL, get_identifier ("handler"), jmp_buf_type);
  TREE_CHAIN (field) = chain;
  chain = field;
  field = build_lang_field_decl (FIELD_DECL, get_identifier ("prev"),
				 TYPE_POINTER_TO (EHS_type));
  TREE_CHAIN (field) = chain;
  chain = field;

  ctor = build_lang_decl (FUNCTION_DECL, cname,
			  build_cplus_method_type (EHS_type, TYPE_POINTER_TO (EHS_type), void_list_node));
  DECL_CONSTRUCTOR_P (ctor) = 1;
  TREE_STATIC (ctor) = 1;
  TREE_PUBLIC (ctor) = 1;
  grokclassfn (EHS_type, cname, ctor, NO_SPECIAL, 0, 0);
  finish_decl (ctor, 0, 0);
  TREE_CHAIN (ctor) = chain;
  chain = ctor;
  dtor = build_lang_decl (FUNCTION_DECL, cname,
			  build_cplus_method_type (EHS_type, TYPE_POINTER_TO (EHS_type), void_list_node));
  TREE_STATIC (dtor) = 1;
  TREE_PUBLIC (dtor) = 1;
  grokclassfn (EHS_type, cname, dtor, DTOR_FLAG, 0, 0);
  finish_decl (dtor, 0, 0);
  TREE_CHAIN (dtor) = chain;
  chain = dtor;
  TYPE_HAS_CONSTRUCTOR (EHS_type) = 1;
  TYPE_HAS_DESTRUCTOR (EHS_type) = 1;
  finish_struct (EHS_type, temp_tree_cons (NULL_TREE, chain, NULL_TREE), 0, 0);
}

void
init_exception_processing_1 ()
{
  register tree EHS_id = get_identifier ("exceptionHandlerStack");

  EHS_decl = IDENTIFIER_GLOBAL_VALUE (EHS_id);

  /* If we have no other definition, default to library implementation.  */
  if (EHS_decl == NULL_TREE)
    {
      EHS_decl = build_decl (VAR_DECL, EHS_id, TYPE_POINTER_TO (EHS_type));
      /* If we don't push this, its definition, should it be encountered,
	 will not be seen.  */
      EHS_decl = pushdecl (EHS_decl);
      TREE_EXTERNAL (EHS_decl) = 1;
      TREE_STATIC (EHS_decl) = 1;
      TREE_PUBLIC (EHS_decl) = 1;
      finish_decl (EHS_decl, 0, 0);
    }
  else if (TREE_CODE (EHS_decl) != VAR_DECL
	   || TREE_TYPE (EHS_decl) != TYPE_POINTER_TO (EHS_type))
    fatal ("exception handling declarations conflict with compiler's internal model");

  if (EHS_prev == NULL_TREE)
    {
      register tree EHS_DECL = build1 (INDIRECT_REF, EHS_type, EHS_decl);
      EHS_prev = build_component_ref (EHS_DECL, get_identifier ("prev"), 0, 0);
      EHS_handler = build_component_ref (EHS_DECL, get_identifier ("handler"), 0, 0);
      EHS_parms = build_component_ref (EHS_DECL, get_identifier ("parms"), 0, 0);
      EHS_name = build_component_ref (EHS_DECL, get_identifier ("name"), 0, 0);
    }

  if (use_gdb_dbx_extensions)
    dbxout_eh_init (EHS_type, EHS_decl);    
}
