/* Generate from machine description:

   - some flags HAVE_... saying which simple standard instructions are
   available for this machine.
   Copyright (C) 1987, 1991 Free Software Foundation, Inc.

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


#include <stdio.h>
#include "hconfig.h"
#include "rtl.h"
#include "obstack.h"

static struct obstack obstack;
struct obstack *rtl_obstack = &obstack;

#define obstack_chunk_alloc xmalloc
#define obstack_chunk_free free

extern void free ();
extern rtx read_rtx ();

char *xmalloc ();
static void fatal ();
void fancy_abort ();

/* Names for patterns.  Need to allow linking with print-rtl.  */
char **insn_name_ptr;

/* Obstacks to remember normal, and call insns.  */
static struct obstack call_obstack, normal_obstack;

/* Max size of names encountered.  */
static int max_id_len;

/* Count the number of match_operand's found.  */
static int
num_operands (x)
     rtx x;
{
  int count = 0;
  int i, j;
  enum rtx_code code = GET_CODE (x);
  char *format_ptr = GET_RTX_FORMAT (code);

  if (code == MATCH_OPERAND)
    return 1;

  if (code == MATCH_OPERATOR || code == MATCH_PARALLEL)
    count++;

  for (i = 0; i < GET_RTX_LENGTH (code); i++)
    {
      switch (*format_ptr++)
	{
	case 'u':
	case 'e':
	  count += num_operands (XEXP (x, i));
	  break;

	case 'E':
	  if (XVEC (x, i) != NULL)
	    for (j = 0; j < XVECLEN (x, i); j++)
	      count += num_operands (XVECEXP (x, i, j));

	  break;
	}
    }

  return count;
}

/* Print out prototype information for a function.  */
static void
gen_proto (insn)
     rtx insn;
{
  int num = num_operands (insn);
  printf ("extern rtx gen_%-*s PROTO((", max_id_len, XSTR (insn, 0));

  if (num == 0)
    printf ("void");
  else
    {
      while (num-- > 1)
	printf ("rtx, ");

      printf ("rtx");
    }

  printf ("));\n");
}

/* Print out a function declaration without a prototype.  */
static void
gen_nonproto (insn)
     rtx insn;
{
  printf ("extern rtx gen_%s ();\n", XSTR (insn, 0));
}

static void
gen_insn (insn)
     rtx insn;
{
  char *name = XSTR (insn, 0);
  char *p;
  struct obstack *obstack_ptr;
  int len;

  /* Don't mention instructions whose names are the null string.
     They are in the machine description just to be recognized.  */
  len = strlen (name);
  if (len == 0)
    return;

  if (len > max_id_len)
    max_id_len = len;

  printf ("#define HAVE_%s ", name);
  if (strlen (XSTR (insn, 2)) == 0)
    printf ("1\n");
  else
    {
      /* Write the macro definition, putting \'s at the end of each line,
	 if more than one.  */
      printf ("(");
      for (p = XSTR (insn, 2); *p; p++)
	{
	  if (*p == '\n')
	    printf (" \\\n");
	  else
	    printf ("%c", *p);
	}
      printf (")\n");
    }

  /* Save the current insn, so that we can later put out appropriate
     prototypes.  At present, most md files have the wrong number of
     arguments for the call insns (call, call_value, call_pop,
     call_value_pop) ignoring the extra arguments that are passed for
     some machines, so by default, turn off the prototype.  */

  obstack_ptr = (name[0] == 'c'
		 && (!strcmp (name, "call")
		     || !strcmp (name, "call_value")
		     || !strcmp (name, "call_pop")
		     || !strcmp (name, "call_value_pop")))
    ? &call_obstack : &normal_obstack;

  obstack_grow (obstack_ptr, &insn, sizeof (rtx));
}

char *
xmalloc (size)
     unsigned size;
{
  register char *val = (char *) malloc (size);

  if (val == 0)
    fatal ("virtual memory exhausted");

  return val;
}

char *
xrealloc (ptr, size)
     char *ptr;
     unsigned size;
{
  char *result = (char *) realloc (ptr, size);
  if (!result)
    fatal ("virtual memory exhausted");
  return result;
}

static void
fatal (s, a1, a2)
     char *s;
{
  fprintf (stderr, "genflags: ");
  fprintf (stderr, s, a1, a2);
  fprintf (stderr, "\n");
  exit (FATAL_EXIT_CODE);
}

/* More 'friendly' abort that prints the line and file.
   config.h can #define abort fancy_abort if you like that sort of thing.  */

void
fancy_abort ()
{
  fatal ("Internal gcc abort.");
}

int
main (argc, argv)
     int argc;
     char **argv;
{
  rtx desc;
  rtx dummy;
  rtx *call_insns;
  rtx *normal_insns;
  rtx *insn_ptr;
  FILE *infile;
  register int c;

  obstack_init (rtl_obstack);
  obstack_init (&call_obstack);
  obstack_init (&normal_obstack);

  if (argc <= 1)
    fatal ("No input file name.");

  infile = fopen (argv[1], "r");
  if (infile == 0)
    {
      perror (argv[1]);
      exit (FATAL_EXIT_CODE);
    }

  init_rtl ();

  printf ("/* Generated automatically by the program `genflags'\n\
from the machine description file `md'.  */\n\n");

  /* Read the machine description.  */

  while (1)
    {
      c = read_skip_spaces (infile);
      if (c == EOF)
	break;
      ungetc (c, infile);

      desc = read_rtx (infile);
      if (GET_CODE (desc) == DEFINE_INSN || GET_CODE (desc) == DEFINE_EXPAND)
	gen_insn (desc);
    }

  /* Print out the prototypes now.  */
  dummy = (rtx)0;
  obstack_grow (&call_obstack, &dummy, sizeof (rtx));
  call_insns = (rtx *) obstack_finish (&call_obstack);

  obstack_grow (&normal_obstack, &dummy, sizeof (rtx));
  normal_insns = (rtx *) obstack_finish (&normal_obstack);

  printf ("\n#ifndef NO_MD_PROTOTYPES\n");
  for (insn_ptr = normal_insns; *insn_ptr; insn_ptr++)
    gen_proto (*insn_ptr);

  printf ("\n#ifdef MD_CALL_PROTOTYPES\n");
  for (insn_ptr = call_insns; *insn_ptr; insn_ptr++)
    gen_proto (*insn_ptr);

  printf ("\n#else /* !MD_CALL_PROTOTYPES */\n");
  for (insn_ptr = call_insns; *insn_ptr; insn_ptr++)
    gen_nonproto (*insn_ptr);

  printf ("#endif /* !MD_CALL_PROTOTYPES */\n");
  printf ("\n#else  /* NO_MD_PROTOTYPES */\n");
  for (insn_ptr = normal_insns; *insn_ptr; insn_ptr++)
    gen_nonproto (*insn_ptr);

  for (insn_ptr = call_insns; *insn_ptr; insn_ptr++)
    gen_nonproto (*insn_ptr);

  printf ("#endif  /* NO_MD_PROTOTYPES */\n");

  fflush (stdout);
  exit (ferror (stdout) != 0 ? FATAL_EXIT_CODE : SUCCESS_EXIT_CODE);
  /* NOTREACHED */
  return 0;
}
