/* Variables and structures for declaration processing.
   Copyright (C) 1993 Free Software Foundation, Inc.

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

/* In grokdeclarator, distinguish syntactic contexts of declarators.  */
enum decl_context
{ NORMAL,			/* Ordinary declaration */
  FUNCDEF,			/* Function definition */
  PARM,				/* Declaration of parm before function body */
  FIELD,			/* Declaration inside struct or union */
  BITFIELD,			/* Likewise but with specified width */
  TYPENAME,			/* Typename (inside cast or sizeof)  */
  MEMFUNCDEF			/* Member function definition */
};

/* C++: Keep these around to reduce calls to `get_identifier'.
   Identifiers for `this' in member functions and the auto-delete
   parameter for destructors.  */
extern tree this_identifier, in_charge_identifier;

/* Parsing a function declarator leaves a list of parameter names
   or a chain or parameter decls here.  */
extern tree last_function_parms;

/* A list of static class variables.  This is needed, because a
   static class variable can be declared inside the class without
   an initializer, and then initialized, staticly, outside the class.  */
extern tree pending_statics;

/* A list of objects which have constructors or destructors
   which reside in the global scope.  The decl is stored in
   the TREE_VALUE slot and the initializer is stored
   in the TREE_PURPOSE slot.  */
extern tree static_aggregates;

/* A list of functions which were declared inline, but later had their
   address taken.  Used only for non-virtual member functions, since we can
   find other functions easily enough.  */
extern tree pending_addressable_inlines;

#ifdef DEBUG_CP_BINDING_LEVELS
/* Purely for debugging purposes.  */
extern int debug_bindings_indentation;
#endif
