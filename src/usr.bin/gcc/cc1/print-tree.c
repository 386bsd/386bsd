/* Prints out tree in human readable form - GNU C-compiler
   Copyright (C) 1990, 1991, 1993 Free Software Foundation, Inc.

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
#include "tree.h"
#include <stdio.h>

extern char **tree_code_name;

extern char *mode_name[];

void print_node ();
void indent_to ();

/* Define the hash table of nodes already seen.
   Such nodes are not repeated; brief cross-references are used.  */

#define HASH_SIZE 37

struct bucket
{
  tree node;
  struct bucket *next;
};

static struct bucket **table;

/* Print the node NODE on standard error, for debugging.
   Most nodes referred to by this one are printed recursively
   down to a depth of six.  */

void
debug_tree (node)
     tree node;
{
  char *object = (char *) oballoc (0);
  table = (struct bucket **) oballoc (HASH_SIZE * sizeof (struct bucket *));
  bzero (table, HASH_SIZE * sizeof (struct bucket *));
  print_node (stderr, "", node, 0);
  table = 0;
  obfree (object);
  fprintf (stderr, "\n");
}

/* Print a node in brief fashion, with just the code, address and name.  */

void
print_node_brief (file, prefix, node, indent)
     FILE *file;
     char *prefix;
     tree node;
     int indent;
{
  char class;

  if (node == 0)
    return;

  class = TREE_CODE_CLASS (TREE_CODE (node));

  /* Always print the slot this node is in, and its code, address and
     name if any.  */
  if (indent > 0)
    fprintf (file, " ");
  fprintf (file, "%s <%s ", prefix, tree_code_name[(int) TREE_CODE (node)]);
  fprintf (file, HOST_PTR_PRINTF, (HOST_WIDE_INT) node);

  if (class == 'd')
    {
      if (DECL_NAME (node))
	fprintf (file, " %s", IDENTIFIER_POINTER (DECL_NAME (node)));
    }
  else if (class == 't')
    {
      if (TYPE_NAME (node))
	{
	  if (TREE_CODE (TYPE_NAME (node)) == IDENTIFIER_NODE)
	    fprintf (file, " %s", IDENTIFIER_POINTER (TYPE_NAME (node)));
	  else if (TREE_CODE (TYPE_NAME (node)) == TYPE_DECL
		   && DECL_NAME (TYPE_NAME (node)))
	    fprintf (file, " %s",
		     IDENTIFIER_POINTER (DECL_NAME (TYPE_NAME (node))));
	}
    }
  if (TREE_CODE (node) == IDENTIFIER_NODE)
    fprintf (file, " %s", IDENTIFIER_POINTER (node));
  /* We might as well always print the value of an integer.  */
  if (TREE_CODE (node) == INTEGER_CST)
    {
      if (TREE_INT_CST_HIGH (node) == 0)
	fprintf (file, " %1u", TREE_INT_CST_LOW (node));
      else if (TREE_INT_CST_HIGH (node) == -1
	       && TREE_INT_CST_LOW (node) != 0)
	fprintf (file, " -%1u", -TREE_INT_CST_LOW (node));
      else
	fprintf (file,
#if HOST_BITS_PER_WIDE_INT == 64
#if HOST_BITS_PER_WIDE_INT != HOST_BITS_PER_INT
		 " 0x%lx%016lx",
#else
		 " 0x%x%016x",
#endif
#else
#if HOST_BITS_PER_WIDE_INT != HOST_BITS_PER_INT
		 " 0x%lx%08lx",
#else
		 " 0x%x%08x",
#endif
#endif
		 TREE_INT_CST_HIGH (node), TREE_INT_CST_LOW (node));
    }
  if (TREE_CODE (node) == REAL_CST)
    {
#ifndef REAL_IS_NOT_DOUBLE
      fprintf (file, " %e", TREE_REAL_CST (node));
#else
      {
	int i;
	char *p = (char *) &TREE_REAL_CST (node);
	fprintf (file, " 0x");
	for (i = 0; i < sizeof TREE_REAL_CST (node); i++)
	  fprintf (file, "%02x", *p++);
	fprintf (file, "");
      }
#endif /* REAL_IS_NOT_DOUBLE */
    }

  fprintf (file, ">");
}

void
indent_to (file, column)
     FILE *file;
     int column;
{
  int i;

  /* Since this is the long way, indent to desired column.  */
  if (column > 0)
    fprintf (file, "\n");
  for (i = 0; i < column; i++)
    fprintf (file, " ");
}

/* Print the node NODE in full on file FILE, preceded by PREFIX,
   starting in column INDENT.  */

void
print_node (file, prefix, node, indent)
     FILE *file;
     char *prefix;
     tree node;
     int indent;
{
  int hash;
  struct bucket *b;
  enum machine_mode mode;
  char class;
  int len;
  int first_rtl;
  int i;

  if (node == 0)
    return;

  class = TREE_CODE_CLASS (TREE_CODE (node));

  /* Don't get too deep in nesting.  If the user wants to see deeper,
     it is easy to use the address of a lowest-level node
     as an argument in another call to debug_tree.  */

  if (indent > 24)
    {
      print_node_brief (file, prefix, node, indent);
      return;
    }

  if (indent > 8 && (class == 't' || class == 'd'))
    {
      print_node_brief (file, prefix, node, indent);
      return;
    }

  /* It is unsafe to look at any other filds of an ERROR_MARK node. */
  if (TREE_CODE (node) == ERROR_MARK)
    {
      print_node_brief (file, prefix, node, indent);
      return;
    }

  hash = ((unsigned HOST_WIDE_INT) node) % HASH_SIZE;

  /* If node is in the table, just mention its address.  */
  for (b = table[hash]; b; b = b->next)
    if (b->node == node)
      {
	print_node_brief (file, prefix, node, indent);
	return;
      }

  /* Add this node to the table.  */
  b = (struct bucket *) oballoc (sizeof (struct bucket));
  b->node = node;
  b->next = table[hash];
  table[hash] = b;

  /* Indent to the specified column, since this is the long form.  */
  indent_to (file, indent);

  /* Print the slot this node is in, and its code, and address.  */
  fprintf (file, "%s <%s ", prefix, tree_code_name[(int) TREE_CODE (node)]);
  fprintf (file, HOST_PTR_PRINTF, (HOST_WIDE_INT) node);

  /* Print the name, if any.  */
  if (class == 'd')
    {
      if (DECL_NAME (node))
	fprintf (file, " %s", IDENTIFIER_POINTER (DECL_NAME (node)));
    }
  else if (class == 't')
    {
      if (TYPE_NAME (node))
	{
	  if (TREE_CODE (TYPE_NAME (node)) == IDENTIFIER_NODE)
	    fprintf (file, " %s", IDENTIFIER_POINTER (TYPE_NAME (node)));
	  else if (TREE_CODE (TYPE_NAME (node)) == TYPE_DECL
		   && DECL_NAME (TYPE_NAME (node)))
	    fprintf (file, " %s",
		     IDENTIFIER_POINTER (DECL_NAME (TYPE_NAME (node))));
	}
    }
  if (TREE_CODE (node) == IDENTIFIER_NODE)
    fprintf (file, " %s", IDENTIFIER_POINTER (node));

  if (TREE_CODE (node) == INTEGER_CST)
    {
      if (indent <= 4)
	print_node_brief (file, "type", TREE_TYPE (node), indent + 4);
    }
  else
    {
      print_node (file, "type", TREE_TYPE (node), indent + 4);
      if (TREE_TYPE (node))
	indent_to (file, indent + 3);
    }

  /* If a permanent object is in the wrong obstack, or the reverse, warn.  */
  if (object_permanent_p (node) != TREE_PERMANENT (node))
    {
      if (TREE_PERMANENT (node))
	fputs (" !!permanent object in non-permanent obstack!!", file);
      else
	fputs (" !!non-permanent object in permanent obstack!!", file);
      indent_to (file, indent + 3);
    }

  if (TREE_SIDE_EFFECTS (node))
    fputs (" side-effects", file);
  if (TREE_READONLY (node))
    fputs (" readonly", file);
  if (TREE_CONSTANT (node))
    fputs (" constant", file);
  if (TREE_ADDRESSABLE (node))
    fputs (" addressable", file);
  if (TREE_THIS_VOLATILE (node))
    fputs (" volatile", file);
  if (TREE_UNSIGNED (node))
    fputs (" unsigned", file);
  if (TREE_ASM_WRITTEN (node))
    fputs (" asm_written", file);
  if (TREE_USED (node))
    fputs (" used", file);
  if (TREE_RAISES (node))
    fputs (" raises", file);
  if (TREE_PERMANENT (node))
    fputs (" permanent", file);
  if (TREE_PUBLIC (node))
    fputs (" public", file);
  if (TREE_STATIC (node))
    fputs (" static", file);
  if (TREE_LANG_FLAG_0 (node))
    fputs (" tree_0", file);
  if (TREE_LANG_FLAG_1 (node))
    fputs (" tree_1", file);
  if (TREE_LANG_FLAG_2 (node))
    fputs (" tree_2", file);
  if (TREE_LANG_FLAG_3 (node))
    fputs (" tree_3", file);
  if (TREE_LANG_FLAG_4 (node))
    fputs (" tree_4", file);
  if (TREE_LANG_FLAG_5 (node))
    fputs (" tree_5", file);
  if (TREE_LANG_FLAG_6 (node))
    fputs (" tree_6", file);

  /* DECL_ nodes have additional attributes.  */

  switch (TREE_CODE_CLASS (TREE_CODE (node)))
    {
    case 'd':
      mode = DECL_MODE (node);

      if (DECL_EXTERNAL (node))
	fputs (" external", file);
      if (DECL_NONLOCAL (node))
	fputs (" nonlocal", file);
      if (DECL_REGISTER (node))
	fputs (" regdecl", file);
      if (DECL_INLINE (node))
	fputs (" inline", file);
      if (DECL_BIT_FIELD (node))
	fputs (" bit-field", file);
      if (DECL_VIRTUAL_P (node))
	fputs (" virtual", file);
      if (DECL_IGNORED_P (node))
	fputs (" ignored", file);
      if (DECL_IN_SYSTEM_HEADER (node))
	fputs (" in_system_header", file);
      if (DECL_LANG_FLAG_0 (node))
	fputs (" decl_0", file);
      if (DECL_LANG_FLAG_1 (node))
	fputs (" decl_1", file);
      if (DECL_LANG_FLAG_2 (node))
	fputs (" decl_2", file);
      if (DECL_LANG_FLAG_3 (node))
	fputs (" decl_3", file);
      if (DECL_LANG_FLAG_4 (node))
	fputs (" decl_4", file);
      if (DECL_LANG_FLAG_5 (node))
	fputs (" decl_5", file);
      if (DECL_LANG_FLAG_6 (node))
	fputs (" decl_6", file);
      if (DECL_LANG_FLAG_7 (node))
	fputs (" decl_7", file);

      fprintf (file, " %s", mode_name[(int) mode]);

      fprintf (file, " file %s line %d",
	       DECL_SOURCE_FILE (node), DECL_SOURCE_LINE (node));

      print_node (file, "size", DECL_SIZE (node), indent + 4);
      indent_to (file, indent + 3);
      if (TREE_CODE (node) != FUNCTION_DECL)
	fprintf (file, " align %d", DECL_ALIGN (node));
      else if (DECL_INLINE (node))
	fprintf (file, " frame_size %d", DECL_FRAME_SIZE (node));
      else if (DECL_BUILT_IN (node))
	fprintf (file, " built-in code %d", DECL_FUNCTION_CODE (node));
      if (TREE_CODE (node) == FIELD_DECL)
	print_node (file, "bitpos", DECL_FIELD_BITPOS (node), indent + 4);
      print_node_brief (file, "context", DECL_CONTEXT (node), indent + 4);
      print_node_brief (file, "abstract_origin",
			DECL_ABSTRACT_ORIGIN (node), indent + 4);

      print_node (file, "arguments", DECL_ARGUMENTS (node), indent + 4);
      print_node (file, "result", DECL_RESULT (node), indent + 4);
      print_node_brief (file, "initial", DECL_INITIAL (node), indent + 4);

      print_lang_decl (file, node, indent);

      if (DECL_RTL (node) != 0)
	{
	  indent_to (file, indent + 4);
	  print_rtl (file, DECL_RTL (node));
	}

      if (DECL_SAVED_INSNS (node) != 0)
	{
	  indent_to (file, indent + 4);
	  if (TREE_CODE (node) == PARM_DECL)
	    {
	      fprintf (file, "incoming-rtl ");
	      print_rtl (file, DECL_INCOMING_RTL (node));
	    }
	  else if (TREE_CODE (node) == FUNCTION_DECL)
	    {
	      fprintf (file, "saved-insns ");
	      fprintf (file, HOST_PTR_PRINTF,
 		       (HOST_WIDE_INT) DECL_SAVED_INSNS (node));
	    }
	}

      /* Print the decl chain only if decl is at second level.  */
      if (indent == 4)
	print_node (file, "chain", TREE_CHAIN (node), indent + 4);
      else
	print_node_brief (file, "chain", TREE_CHAIN (node), indent + 4);
      break;

    case 't':
      if (TYPE_NO_FORCE_BLK (node))
	fputs (" no_force_blk", file);
      if (TYPE_LANG_FLAG_0 (node))
	fputs (" type_0", file);
      if (TYPE_LANG_FLAG_1 (node))
	fputs (" type_1", file);
      if (TYPE_LANG_FLAG_2 (node))
	fputs (" type_2", file);
      if (TYPE_LANG_FLAG_3 (node))
	fputs (" type_3", file);
      if (TYPE_LANG_FLAG_4 (node))
	fputs (" type_4", file);
      if (TYPE_LANG_FLAG_5 (node))
	fputs (" type_5", file);
      if (TYPE_LANG_FLAG_6 (node))
	fputs (" type_6", file);

      mode = TYPE_MODE (node);
      fprintf (file, " %s", mode_name[(int) mode]);

      print_node (file, "size", TYPE_SIZE (node), indent + 4);
      indent_to (file, indent + 3);

      fprintf (file, " align %d", TYPE_ALIGN (node));
      fprintf (file, " symtab %d", TYPE_SYMTAB_ADDRESS (node));

      if (TREE_CODE (node) == ARRAY_TYPE || TREE_CODE (node) == SET_TYPE)
	print_node (file, "domain", TYPE_DOMAIN (node), indent + 4);
      else if (TREE_CODE (node) == INTEGER_TYPE
	       || TREE_CODE (node) == BOOLEAN_TYPE
	       || TREE_CODE (node) == CHAR_TYPE)
	{
	  fprintf (file, " precision %d", TYPE_PRECISION (node));
	  print_node (file, "min", TYPE_MIN_VALUE (node), indent + 4);
	  print_node (file, "max", TYPE_MAX_VALUE (node), indent + 4);
	}
      else if (TREE_CODE (node) == ENUMERAL_TYPE)
	{
	  fprintf (file, " precision %d", TYPE_PRECISION (node));
	  print_node (file, "min", TYPE_MIN_VALUE (node), indent + 4);
	  print_node (file, "max", TYPE_MAX_VALUE (node), indent + 4);
	  print_node (file, "values", TYPE_VALUES (node), indent + 4);
	}
      else if (TREE_CODE (node) == REAL_TYPE)
	fprintf (file, " precision %d", TYPE_PRECISION (node));
      else if (TREE_CODE (node) == RECORD_TYPE
	       || TREE_CODE (node) == UNION_TYPE
	       || TREE_CODE (node) == QUAL_UNION_TYPE)
	print_node (file, "fields", TYPE_FIELDS (node), indent + 4);
      else if (TREE_CODE (node) == FUNCTION_TYPE || TREE_CODE (node) == METHOD_TYPE)
	{
	  if (TYPE_METHOD_BASETYPE (node))
	    print_node_brief (file, "method basetype", TYPE_METHOD_BASETYPE (node), indent + 4);
	  print_node (file, "arg-types", TYPE_ARG_TYPES (node), indent + 4);
	}
      if (TYPE_CONTEXT (node))
	print_node_brief (file, "context", TYPE_CONTEXT (node), indent + 4);

      print_lang_type (file, node, indent);

      if (TYPE_POINTER_TO (node) || TREE_CHAIN (node))
	indent_to (file, indent + 3);
      print_node_brief (file, "pointer_to_this", TYPE_POINTER_TO (node), indent + 4);
      print_node_brief (file, "reference_to_this", TYPE_REFERENCE_TO (node), indent + 4);
      print_node_brief (file, "chain", TREE_CHAIN (node), indent + 4);
      break;

    case 'b':
      print_node (file, "vars", BLOCK_VARS (node), indent + 4);
      print_node (file, "tags", BLOCK_TYPE_TAGS (node), indent + 4);
      print_node (file, "supercontext", BLOCK_SUPERCONTEXT (node), indent + 4);
      print_node (file, "subblocks", BLOCK_SUBBLOCKS (node), indent + 4);
      print_node (file, "chain", BLOCK_CHAIN (node), indent + 4);
      print_node (file, "abstract_origin",
		  BLOCK_ABSTRACT_ORIGIN (node), indent + 4);
      return;

    case 'e':
    case '<':
    case '1':
    case '2':
    case 'r':
    case 's':
      switch (TREE_CODE (node))
	{
	case BIND_EXPR:
	  print_node (file, "vars", TREE_OPERAND (node, 0), indent + 4);
	  print_node (file, "body", TREE_OPERAND (node, 1), indent + 4);
	  print_node (file, "block", TREE_OPERAND (node, 2), indent + 4);
	  return;
	}

      first_rtl = len = tree_code_length[(int) TREE_CODE (node)];
      /* These kinds of nodes contain rtx's, not trees,
	 after a certain point.  Print the rtx's as rtx's.  */
      switch (TREE_CODE (node))
	{
	case SAVE_EXPR:
	  first_rtl = 2;
	  break;
	case CALL_EXPR:
	  first_rtl = 2;
	  break;
	case METHOD_CALL_EXPR:
	  first_rtl = 3;
	  break;
	case WITH_CLEANUP_EXPR:
	  /* Should be defined to be 2.  */
	  first_rtl = 1;
	  break;
	case RTL_EXPR:
	  first_rtl = 0;
	}
      for (i = 0; i < len; i++)
	{
	  if (i >= first_rtl)
	    {
	      indent_to (file, indent + 4);
	      fprintf (file, "rtl %d ", i);
	      if (TREE_OPERAND (node, i))
		print_rtl (file, (struct rtx_def *) TREE_OPERAND (node, i));
	      else
		fprintf (file, "(nil)");
	      fprintf (file, "\n");
	    }
	  else
	    {
	      char temp[10];

	      sprintf (temp, "arg %d", i);
	      print_node (file, temp, TREE_OPERAND (node, i), indent + 4);
	    }
	}
      break;

    case 'c':
    case 'x':
      switch (TREE_CODE (node))
	{
	case INTEGER_CST:
	  if (TREE_INT_CST_HIGH (node) == 0)
	    fprintf (file, " %1u", TREE_INT_CST_LOW (node));
	  else if (TREE_INT_CST_HIGH (node) == -1
		   && TREE_INT_CST_LOW (node) != 0)
	    fprintf (file, " -%1u", -TREE_INT_CST_LOW (node));
	  else
	    fprintf (file,
#if HOST_BITS_PER_WIDE_INT == 64
#if HOST_BITS_PER_WIDE_INT != HOST_BITS_PER_INT
		     " 0x%lx%016lx",
#else
		     " 0x%x%016x",
#endif
#else
#if HOST_BITS_PER_WIDE_INT != HOST_BITS_PER_INT
		     " 0x%lx%08lx",
#else
		     " 0x%x%08x",
#endif
#endif
		     TREE_INT_CST_HIGH (node), TREE_INT_CST_LOW (node));
	  break;

	case REAL_CST:
#ifndef REAL_IS_NOT_DOUBLE
	  fprintf (file, " %e", TREE_REAL_CST (node));
#else
	  {
	    char *p = (char *) &TREE_REAL_CST (node);
	    fprintf (file, " 0x");
	    for (i = 0; i < sizeof TREE_REAL_CST (node); i++)
	      fprintf (file, "%02x", *p++);
	    fprintf (file, "");
	  }
#endif /* REAL_IS_NOT_DOUBLE */
	  break;

	case COMPLEX_CST:
	  print_node (file, "real", TREE_REALPART (node), indent + 4);
	  print_node (file, "imag", TREE_IMAGPART (node), indent + 4);
	  break;

	case STRING_CST:
	  fprintf (file, " \"%s\"", TREE_STRING_POINTER (node));
	  /* Print the chain at second level.  */
	  if (indent == 4)
	    print_node (file, "chain", TREE_CHAIN (node), indent + 4);
	  else
	    print_node_brief (file, "chain", TREE_CHAIN (node), indent + 4);
	  break;

	case IDENTIFIER_NODE:
	  print_lang_identifier (file, node, indent);
	  break;

	case TREE_LIST:
	  print_node (file, "purpose", TREE_PURPOSE (node), indent + 4);
	  print_node (file, "value", TREE_VALUE (node), indent + 4);
	  print_node (file, "chain", TREE_CHAIN (node), indent + 4);
	  break;

	case TREE_VEC:
	  len = TREE_VEC_LENGTH (node);
	  for (i = 0; i < len; i++)
	    if (TREE_VEC_ELT (node, i))
	      {
		char temp[10];
		sprintf (temp, "elt %d", i);
		indent_to (file, indent + 4);
		print_node_brief (file, temp, TREE_VEC_ELT (node, i), 0);
	      }
	  break;

	case OP_IDENTIFIER:
	  print_node (file, "op1", TREE_PURPOSE (node), indent + 4);
	  print_node (file, "op2", TREE_VALUE (node), indent + 4);
	}

      break;
    }

  fprintf (file, ">");
}
