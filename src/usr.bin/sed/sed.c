/*  GNU SED, a batch stream editor.
    Copyright (C) 1989, 1990, 1991 Free Software Foundation, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#ifdef __STDC__
#define VOID void
#else
#define VOID char
#endif


#define _GNU_SOURCE
#include <ctype.h>
#ifndef isblank
#define isblank(c) ((c) == ' ' || (c) == '\t')
#endif
#include <stdio.h>
#include <sys/types.h>
#include "rx.h"
#include "getopt.h"
#if defined(STDC_HEADERS)
#include <stdlib.h>
#endif
#if HAVE_STRING_H || defined(STDC_HEADERS)
#include <string.h>
#ifndef bzero
#define bzero(s, n)	memset ((s), 0, (n))
#endif
#if !defined(STDC_HEADERS)
#include <memory.h>
#endif
#else
#include <strings.h>
#endif

#ifdef RX_MEMDBUG
#include <malloc.h>
#endif

#ifndef HAVE_BCOPY
#ifdef HAVE_MEMCPY
#define bcopy(FROM,TO,LEN)  memcpy(TO,FROM,LEN)
#else
void
bcopy (from, to, len)
     char *from;
     char *to;
     int len;
{
  if (from < to)
    {
      from += len - 1;
      to += len - 1;
      while (len--)
	*to-- = *from--;
    }
  else
    while (len--)
      *to++ = *from++;
}

#endif
#endif

char *version_string = "GNU sed version 2.04";

/* Struct vector is used to describe a chunk of a compiled sed program.  
 * There is one vector for the main program, and one for each { } pair,
 * and one for the entire program.  For {} blocks, RETURN_[VI] tells where
 * to continue execution after this VECTOR.
 */

struct vector
{
  struct sed_cmd *v;
  int v_length;
  int v_allocated;
  struct vector *return_v;
  int return_i;
};


/* Goto structure is used to hold both GOTO's and labels.  There are two
 * separate lists, one of goto's, called 'jumps', and one of labels, called
 * 'labels'.
 * the V element points to the descriptor for the program-chunk in which the
 * goto was encountered.
 * the v_index element counts which element of the vector actually IS the
 * goto/label.  The first element of the vector is zero.
 * the NAME element is the null-terminated name of the label.
 * next is the next goto/label in the list. 
 */

struct sed_label
{
  struct vector *v;
  int v_index;
  char *name;
  struct sed_label *next;
};

/* ADDR_TYPE is zero for a null address,
 *  one if addr_number is valid, or
 * two if addr_regex is valid,
 * three, if the address is '$'
 * Other values are undefined.
 */

enum addr_types
{
  addr_is_null = 0,
  addr_is_num = 1,
  addr_is_regex = 2,
  addr_is_last = 3,
  addr_is_mod = 4
};

struct addr
{
  int addr_type;
  struct re_pattern_buffer *addr_regex;
  int addr_number;
  int modulo, offset;
};


/* Aflags:  If the low order bit is set, a1 has been
 * matched; apply this command until a2 matches.
 * If the next bit is set, apply this command to all
 * lines that DON'T match the address(es).
 */

#define A1_MATCHED_BIT	01
#define ADDR_BANG_BIT	02

struct sed_cmd
{
  struct addr a1, a2;
  int aflags;
  
  char cmd;
  
  union
    {
      /* This structure is used for a, i, and c commands */
      struct
	{
	  char *text;
	  int text_len;
	}
      cmd_txt;
      
      /* This is used for b and t commands */
      struct sed_cmd *label;
      
      /* This for r and w commands */
      FILE *io_file;
      
      /* This for the hairy s command */
      /* For the flags var:
	 low order bit means the 'g' option was given,
	 next bit means the 'p' option was given,
	 and the next bit means a 'w' option was given,
	 and wio_file contains the file to write to. */
      
#define S_GLOBAL_BIT	01
#define S_PRINT_BIT	02
#define S_WRITE_BIT	04
#define S_NUM_BIT	010
      
      struct
	{
	  struct re_pattern_buffer *regx;
	  char *replacement;
	  int replace_length;
	  int flags;
	  int numb;
	  FILE *wio_file;
	}
      cmd_regex;
      
      /* This for the y command */
      unsigned char *translate;
      
      /* For { */
      struct vector *sub;
      
      /* for t and b */
      struct sed_label *jump;
    } x;
};

/* Sed operates a line at a time. */
struct line
{
  char *text;			/* Pointer to line allocated by malloc. */
  int length;			/* Length of text. */
  int alloc;			/* Allocated space for text. */
};

/* This structure holds information about files opend by the 'r', 'w',
   and 's///w' commands.  In paticular, it holds the FILE pointers to
   use, the file's name. */

#define NUM_FPS	32
struct
  {
    FILE *for_read;
    FILE *for_write;
    char *name;
  }

file_ptrs[NUM_FPS];


#if defined(__STDC__)
# define P_(s) s
#else
# define P_(s) ()
#endif

void close_files ();
void panic P_ ((char *str,...));
char *__fp_name P_ ((FILE * fp));
FILE *ck_fopen P_ ((char *name, char *mode));
void ck_fwrite P_ ((char *ptr, int size, int nmemb, FILE * stream));
void ck_fclose P_ ((FILE * stream));
VOID *ck_malloc P_ ((int size));
VOID *ck_realloc P_ ((VOID * ptr, int size));
char *ck_strdup P_ ((char *str));
VOID *init_buffer P_ ((void));
void flush_buffer P_ ((VOID * bb));
int size_buffer P_ ((VOID * b));
void add_buffer P_ ((VOID * bb, char *p, int n));
void add1_buffer P_ ((VOID * bb, int ch));
char *get_buffer P_ ((VOID * bb));

void compile_string P_ ((char *str));
void compile_file P_ ((char *str));
struct vector *compile_program P_ ((struct vector * vector, int));
void bad_prog P_ ((char *why));
int inchar P_ ((void));
void savchar P_ ((int ch));
int compile_address P_ ((struct addr * addr));
char * last_regex_string = 0;
void buffer_regex  P_ ((int slash));
void compile_regex P_ ((void));
struct sed_label *setup_jump P_ ((struct sed_label * list, struct sed_cmd * cmd, struct vector * vec));
FILE *compile_filename P_ ((int readit));
void read_file P_ ((char *name));
void execute_program P_ ((struct vector * vec));
int match_address P_ ((struct addr * addr));
int read_pattern_space P_ ((void));
void append_pattern_space P_ ((void));
void line_copy P_ ((struct line * from, struct line * to));
void line_append P_ ((struct line * from, struct line * to));
void str_append P_ ((struct line * to, char *string, int length));
void usage P_ ((int));

extern char *myname;

/* If set, don't write out the line unless explictly told to */
int no_default_output = 0;

/* Current input line # */
int input_line_number = 0;

/* Are we on the last input file? */
int last_input_file = 0;

/* Have we hit EOF on the last input file?  This is used to decide if we
   have hit the '$' address yet. */
int input_EOF = 0;

/* non-zero if a quit command has been executed. */
int quit_cmd = 0;

/* Have we done any replacements lately?  This is used by the 't' command. */
int replaced = 0;

/* How many '{'s are we executing at the moment */
int program_depth = 0;

/* The complete compiled SED program that we are going to run */
struct vector *the_program = 0;

/* information about labels and jumps-to-labels.  This is used to do
   the required backpatching after we have compiled all the scripts. */
struct sed_label *jumps = 0;
struct sed_label *labels = 0;

/* The 'current' input line. */
struct line line;

/* An input line that's been stored by later use by the program */
struct line hold;

/* A 'line' to append to the current line when it comes time to write it out */
struct line append;


/* When we're reading a script command from a string, 'prog_start' and
   'prog_end' point to the beginning and end of the string.  This
   would allow us to compile script strings that contain nulls, except
   that script strings are only read from the command line, which is
   null-terminated */
unsigned char *prog_start;
unsigned char *prog_end;

/* When we're reading a script command from a string, 'prog_cur' points
   to the current character in the string */
unsigned char *prog_cur;

/* This is the name of the current script file.
   It is used for error messages. */
char *prog_name;

/* This is the current script file.  If it is zero, we are reading
   from a string stored in 'prog_start' instead.  If both 'prog_file'
   and 'prog_start' are zero, we're in trouble! */
FILE *prog_file;

/* this is the number of the current script line that we're compiling.  It is
   used to give out useful and informative error messages. */
int prog_line = 1;

/* This is the file pointer that we're currently reading data from.  It may
   be stdin */
FILE *input_file;

/* If this variable is non-zero at exit, one or more of the input
   files couldn't be opened. */

int bad_input = 0;

/* 'an empty regular expression is equivalent to the last regular
   expression read' so we have to keep track of the last regex used.
   Here's where we store a pointer to it (it is only malloc()'d once) */
struct re_pattern_buffer *last_regex;

/* Various error messages we may want to print */
static char ONE_ADDR[] = "Command only uses one address";
static char NO_ADDR[] = "Command doesn't take any addresses";
static char LINE_JUNK[] = "Extra characters after command";
static char BAD_EOF[] = "Unexpected End-of-file";
static char NO_REGEX[] = "No previous regular expression";
static char NO_COMMAND[] = "Missing command";

static struct option longopts[] =
{
  {"expression", 1, NULL, 'e'},
  {"file", 1, NULL, 'f'},
  {"quiet", 0, NULL, 'n'},
  {"silent", 0, NULL, 'n'},
  {"version", 0, NULL, 'V'},
  {"help", 0, NULL, 'h'},
  {NULL, 0, NULL, 0}
};

void
main (argc, argv)
     int argc;
     char **argv;
{
  int opt;
  char *e_strings = NULL;
  int compiled = 0;
  struct sed_label *go, *lbl;

  /* see regex.h */
  re_set_syntax (RE_SYNTAX_POSIX_BASIC);
  rx_cache_bound = 4096;	/* Consume memory rampantly. */

  myname = argv[0];
  while ((opt = getopt_long (argc, argv, "hne:f:V", longopts, (int *) 0))
	 != EOF)
    {
      switch (opt)
	{
	case 'n':
	  no_default_output = 1;
	  break;
	case 'e':
	  if (e_strings == NULL)
	    {
	      e_strings = ck_malloc (strlen (optarg) + 2);
	      strcpy (e_strings, optarg);
	    }
	  else
	    {
	      e_strings = ck_realloc (e_strings, strlen (e_strings) + strlen (optarg) + 2);
	      strcat (e_strings, optarg);
	    }
	  strcat (e_strings, "\n");
	  compiled = 1;
	  break;
	case 'f':
	  if (e_strings)
	    {
	      compile_string (e_strings);
	      free (e_strings);
	      e_strings = 0;
	    }
	  compile_file (optarg);
	  compiled = 1;
	  break;
	case 'V':
	  fprintf (stderr, "%s\n", version_string);
	  exit (0);
	  break;
	case 'h':
	  usage (0);
	  break;
	default:
	  usage (4);
	  break;
	}
    }
  if (e_strings)
    {
      compile_string (e_strings);
      free (e_strings);
    }
  if (!compiled)
    {
      if (optind == argc)
	usage (4);
      compile_string (argv[optind++]);
    }

  for (go = jumps; go; go = go->next)
    {
      for (lbl = labels; lbl; lbl = lbl->next)
	if (!strcmp (lbl->name, go->name))
	  break;
      if (*go->name && !lbl)
	panic ("Can't find label for jump to '%s'", go->name);
      go->v->v[go->v_index].x.jump = lbl;
    }

  line.length = 0;
  line.alloc = 50;
  line.text = ck_malloc (50);

  append.length = 0;
  append.alloc = 50;
  append.text = ck_malloc (50);

  hold.length = 1;
  hold.alloc = 50;
  hold.text = ck_malloc (50);
  hold.text[0] = '\n';

  if (argc <= optind)
    {
      last_input_file++;
      read_file ("-");
    }
  else
    while (optind < argc)
      {
	if (optind == argc - 1)
	  last_input_file++;
	read_file (argv[optind]);
	optind++;
	if (quit_cmd)
	  break;
      }
  close_files ();
  if (bad_input)
    exit (2);
  exit (0);
}

void
close_files ()
{
  int nf;

  for (nf = 0; nf < NUM_FPS; nf++)
    {
      if (file_ptrs[nf].for_write)
	fclose (file_ptrs[nf].for_write);
      if (file_ptrs[nf].for_read)
	fclose (file_ptrs[nf].for_read);
    }
}

/* 'str' is a string (from the command line) that contains a sed command.
   Compile the command, and add it to the end of 'the_program' */
void
compile_string (str)
     char *str;
{
  prog_file = 0;
  prog_line = 0;
  prog_start = prog_cur = (unsigned char *)str;
  prog_end = (unsigned char *)str + strlen (str);
  the_program = compile_program (the_program, prog_line);
}

/* 'str' is the name of a file containing sed commands.  Read them in
   and add them to the end of 'the_program' */
void
compile_file (str)
     char *str;
{
  int ch;

  prog_start = prog_cur = prog_end = 0;
  prog_name = str;
  prog_line = 1;
  if (str[0] == '-' && str[1] == '\0')
    prog_file = stdin;
  else
    prog_file = ck_fopen (str, "r");
  ch = getc (prog_file);
  if (ch == '#')
    {
      ch = getc (prog_file);
      if (ch == 'n')
	no_default_output++;
      while (ch != EOF && ch != '\n')
	{
	  ch = getc (prog_file);
	  if (ch == '\\')
	    ch = getc (prog_file);
	}
      ++prog_line;
    }
  else if (ch != EOF)
    ungetc (ch, prog_file);
  the_program = compile_program (the_program, prog_line);
}

#define MORE_CMDS 40

/* Read a program (or a subprogram within '{' '}' pairs) in and store
   the compiled form in *'vector'  Return a pointer to the new vector.  */
struct vector *
compile_program (vector, open_line)
     struct vector *vector;
     int open_line;
{
  struct sed_cmd *cur_cmd;
  int ch = 0;
  int pch;
  int slash;
  VOID *b;
  unsigned char *string;
  int num;

  if (!vector)
    {
      vector = (struct vector *) ck_malloc (sizeof (struct vector));
      vector->v = (struct sed_cmd *) ck_malloc (MORE_CMDS * sizeof (struct sed_cmd));
      vector->v_allocated = MORE_CMDS;
      vector->v_length = 0;
      vector->return_v = 0;
      vector->return_i = 0;
    }
  for (;;)
    {
    skip_comment:
      do
	{
	  pch = ch;
	  ch = inchar ();
	  if ((pch == '\\') && (ch == '\n'))
	    ch = inchar ();
	}
      while (ch != EOF && (isblank (ch) || ch == '\n' || ch == ';'));
      if (ch == EOF)
	break;
      savchar (ch);

      if (vector->v_length == vector->v_allocated)
	{
	  vector->v = ((struct sed_cmd *)
		       ck_realloc ((VOID *) vector->v,
				   ((vector->v_length + MORE_CMDS)
				    * sizeof (struct sed_cmd))));
	  vector->v_allocated += MORE_CMDS;
	}
      cur_cmd = vector->v + vector->v_length;
      vector->v_length++;

      cur_cmd->a1.addr_type = 0;
      cur_cmd->a2.addr_type = 0;
      cur_cmd->aflags = 0;
      cur_cmd->cmd = 0;

      if (compile_address (&(cur_cmd->a1)))
	{
	  ch = inchar ();
	  if (ch == ',')
	    {
	      do
		ch = inchar ();
	      while (ch != EOF && isblank (ch));
	      savchar (ch);
	      if (compile_address (&(cur_cmd->a2)))
		;
	      else
		bad_prog ("Unexpected ','");
	    }
	  else
	    savchar (ch);
	}
      if (cur_cmd->a1.addr_type == addr_is_num
	  && cur_cmd->a2.addr_type == addr_is_num
	  && cur_cmd->a2.addr_number < cur_cmd->a1.addr_number)
	cur_cmd->a2.addr_number = cur_cmd->a1.addr_number;

      ch = inchar ();
      if (ch == EOF)
	bad_prog (NO_COMMAND);
    new_cmd:
      switch (ch)
	{
	case '#':
	  if (cur_cmd->a1.addr_type != 0)
	    bad_prog (NO_ADDR);
	  do
	    {
	      ch = inchar ();
	      if (ch == '\\')
		ch = inchar ();
	    }
	  while (ch != EOF && ch != '\n');
	  vector->v_length--;
	  goto skip_comment;
	case '!':
	  if (cur_cmd->aflags & ADDR_BANG_BIT)
	    bad_prog ("Multiple '!'s");
	  cur_cmd->aflags |= ADDR_BANG_BIT;
	  do
	    ch = inchar ();
	  while (ch != EOF && isblank (ch));
	  if (ch == EOF)
	    bad_prog (NO_COMMAND);
#if 0
	  savchar (ch);
#endif
	  goto new_cmd;
	case 'a':
	case 'i':
	  if (cur_cmd->a2.addr_type != 0)
	    bad_prog (ONE_ADDR);
	  /* Fall Through */
	case 'c':
	  cur_cmd->cmd = ch;
	  if (inchar () != '\\' || inchar () != '\n')
	    bad_prog (LINE_JUNK);
	  b = init_buffer ();
	  while ((ch = inchar ()) != EOF && ch != '\n')
	    {
	      if (ch == '\\')
		ch = inchar ();
	      add1_buffer (b, ch);
	    }
	  if (ch != EOF)
	    add1_buffer (b, ch);
	  num = size_buffer (b);
	  string = (unsigned char *) ck_malloc (num);
	  bcopy (get_buffer (b), string, num);
	  flush_buffer (b);
	  cur_cmd->x.cmd_txt.text_len = num;
	  cur_cmd->x.cmd_txt.text = (char *) string;
	  break;
	case '{':
	  cur_cmd->cmd = ch;
	  program_depth++;
#if 0
	  while ((ch = inchar ()) != EOF && ch != '\n')
	    if (!isblank (ch))
	      bad_prog (LINE_JUNK);
#endif
	  cur_cmd->x.sub = compile_program ((struct vector *) 0, prog_line);
	  /* FOO JF is this the right thing to do?
			   almost.  don't forget a return addr.  -t */
	  cur_cmd->x.sub->return_v = vector;
	  cur_cmd->x.sub->return_i = vector->v_length - 1;
	  break;
	case '}':
	  if (!program_depth)
	    bad_prog ("Unexpected '}'");
	  --program_depth;
	  /* a return insn for subprograms -t */
	  cur_cmd->cmd = ch;
	  if (cur_cmd->a1.addr_type != 0)
	    bad_prog ("} doesn't want any addresses");
	  while ((ch = inchar ()) != EOF && ch != '\n' && ch != ';')
	    if (!isblank (ch))
	      bad_prog (LINE_JUNK);
	  return vector;
	case ':':
	  cur_cmd->cmd = ch;
	  if (cur_cmd->a1.addr_type != 0)
	    bad_prog (": doesn't want any addresses");
	  labels = setup_jump (labels, cur_cmd, vector);
	  break;
	case 'b':
	case 't':
	  cur_cmd->cmd = ch;
	  jumps = setup_jump (jumps, cur_cmd, vector);
	  break;
	case 'q':
	case '=':
	  if (cur_cmd->a2.addr_type)
	    bad_prog (ONE_ADDR);
	  /* Fall Through */
	case 'd':
	case 'D':
	case 'g':
	case 'G':
	case 'h':
	case 'H':
	case 'l':
	case 'n':
	case 'N':
	case 'p':
	case 'P':
	case 'x':
	  cur_cmd->cmd = ch;
	  do
	    ch = inchar ();
	  while (ch != EOF && isblank (ch) && ch != '\n' && ch != ';');
	  if (ch != '\n' && ch != ';' && ch != EOF)
	    bad_prog (LINE_JUNK);
	  break;

	case 'r':
	  if (cur_cmd->a2.addr_type != 0)
	    bad_prog (ONE_ADDR);
	  /* FALL THROUGH */
	case 'w':
	  cur_cmd->cmd = ch;
	  cur_cmd->x.io_file = compile_filename (ch == 'r');
	  break;

	case 's':
	  cur_cmd->cmd = ch;
	  slash = inchar ();
	  buffer_regex (slash);
	  compile_regex ();

	  cur_cmd->x.cmd_regex.regx = last_regex;

	  b = init_buffer ();
	  while (((ch = inchar ()) != EOF) && (ch != slash) && (ch != '\n'))
	    {
	      if (ch == '\\')
		{
		  int ci;

		  ci = inchar ();
		  if (ci != EOF)
		    {
		      if (ci != '\n')
			add1_buffer (b, ch);
		      add1_buffer (b, ci);
		    }
		}
	      else
		add1_buffer (b, ch);
	    }
	  if (ch != slash)
	    {
	      if (ch == '\n' && prog_line > 1)
		--prog_line;
	      bad_prog ("Unterminated `s' command");
	    }
	  cur_cmd->x.cmd_regex.replace_length = size_buffer (b);
	  cur_cmd->x.cmd_regex.replacement = ck_malloc (cur_cmd->x.cmd_regex.replace_length);
	  bcopy (get_buffer (b), cur_cmd->x.cmd_regex.replacement, cur_cmd->x.cmd_regex.replace_length);
	  flush_buffer (b);

	  cur_cmd->x.cmd_regex.flags = 0;
	  cur_cmd->x.cmd_regex.numb = 0;

	  if (ch == EOF)
	    break;
	  do
	    {
	      ch = inchar ();
	      switch (ch)
		{
		case 'p':
		  if (cur_cmd->x.cmd_regex.flags & S_PRINT_BIT)
		    bad_prog ("multiple 'p' options to 's' command");
		  cur_cmd->x.cmd_regex.flags |= S_PRINT_BIT;
		  break;
		case 'g':
		  if (cur_cmd->x.cmd_regex.flags & S_NUM_BIT)
		    cur_cmd->x.cmd_regex.flags &= ~S_NUM_BIT;
		  if (cur_cmd->x.cmd_regex.flags & S_GLOBAL_BIT)
		    bad_prog ("multiple 'g' options to 's' command");
		  cur_cmd->x.cmd_regex.flags |= S_GLOBAL_BIT;
		  break;
		case 'w':
		  cur_cmd->x.cmd_regex.flags |= S_WRITE_BIT;
		  cur_cmd->x.cmd_regex.wio_file = compile_filename (0);
		  ch = '\n';
		  break;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		  if (cur_cmd->x.cmd_regex.flags & S_NUM_BIT)
		    bad_prog ("multiple number options to 's' command");
		  if ((cur_cmd->x.cmd_regex.flags & S_GLOBAL_BIT) == 0)
		    cur_cmd->x.cmd_regex.flags |= S_NUM_BIT;
		  num = 0;
		  while (isdigit (ch))
		    {
		      num = num * 10 + ch - '0';
		      ch = inchar ();
		    }
		  savchar (ch);
		  cur_cmd->x.cmd_regex.numb = num;
		  break;
		case '\n':
		case ';':
		case EOF:
		  break;
		default:
		  bad_prog ("Unknown option to 's'");
		  break;
		}
	    }
	  while (ch != EOF && ch != '\n' && ch != ';');
	  if (ch == EOF)
	    break;
	  break;

	case 'y':
	  cur_cmd->cmd = ch;
	  string = (unsigned char *) ck_malloc (256);
	  for (num = 0; num < 256; num++)
	    string[num] = num;
	  b = init_buffer ();
	  slash = inchar ();
	  while ((ch = inchar ()) != EOF && ch != slash)
	    add1_buffer (b, ch);
	  cur_cmd->x.translate = string;
	  string = (unsigned char *) get_buffer (b);
	  for (num = size_buffer (b); num; --num)
	    {
	      ch = inchar ();
	      if (ch == EOF)
		bad_prog (BAD_EOF);
	      if (ch == slash)
		bad_prog ("strings for y command are different lengths");
	      cur_cmd->x.translate[*string++] = ch;
	    }
	  flush_buffer (b);
	  if (inchar () != slash || ((ch = inchar ()) != EOF && ch != '\n' && ch != ';'))
	    bad_prog (LINE_JUNK);
	  break;

	default:
	  bad_prog ("Unknown command");
	}
    }
  if (program_depth)
    {
      prog_line = open_line;
      bad_prog ("Unmatched `{'");
    }
  return vector;
}

/* Complain about a programming error and exit. */
void
bad_prog (why)
     char *why;
{
  if (prog_line > 0)
    fprintf (stderr, "%s: file %s line %d: %s\n",
	     myname, prog_name, prog_line, why);
  else
    fprintf (stderr, "%s: %s\n", myname, why);
  exit (1);
}

/* Read the next character from the program.  Return EOF if there isn't
   anything to read.  Keep prog_line up to date, so error messages can
   be meaningful. */
int
inchar ()
{
  int ch;
  if (prog_file)
    {
      if (feof (prog_file))
	return EOF;
      else
	ch = getc (prog_file);
    }
  else
    {
      if (!prog_cur)
	return EOF;
      else if (prog_cur == prog_end)
	{
	  ch = EOF;
	  prog_cur = 0;
	}
      else
	ch = *prog_cur++;
    }
  if ((ch == '\n') && prog_line)
    prog_line++;
  return ch;
}

/* unget 'ch' so the next call to inchar will return it.  'ch' must not be
   EOF or anything nasty like that. */
void
savchar (ch)
     int ch;
{
  if (ch == EOF)
    return;
  if (ch == '\n' && prog_line > 1)
    --prog_line;
  if (prog_file)
    ungetc (ch, prog_file);
  else
    *--prog_cur = ch;
}


/* Try to read an address for a sed command.  If it succeeeds,
   return non-zero and store the resulting address in *'addr'.
   If the input doesn't look like an address read nothing
   and return zero. */
int
compile_address (addr)
     struct addr *addr;
{
  int ch;
  int num;

  ch = inchar ();

  if (isdigit (ch))
    {
      num = ch - '0';
      while ((ch = inchar ()) != EOF && isdigit (ch))
      if (ch == '~')
	{
	  addr->addr_type = addr_is_mod;
	  addr->offset = num;
	  ch = inchar();
	  num=0;
	  if (isdigit(ch)) {
	    num = ch - '0';
	    while ((ch = inchar ()) != EOF && isdigit (ch))
	      num = num * 10 + ch - '0';
	    addr->modulo = num;
	  }
	  addr->modulo += (addr->modulo==0);
	} else {
	  addr->addr_type = addr_is_num;
	  addr->addr_number = num;
	}
	num = num * 10 + ch - '0';
      while (ch != EOF && isblank (ch))
	ch = inchar ();
      savchar (ch);
      return 1;
    }
  else if (ch == '/' || ch == '\\')
    {
      addr->addr_type = addr_is_regex;
      if (ch == '\\')
	ch = inchar ();
      buffer_regex (ch);
      compile_regex ();
      addr->addr_regex = last_regex;
      do
	ch = inchar ();
      while (ch != EOF && isblank (ch));
      savchar (ch);
      return 1;
    }
  else if (ch == '$')
    {
      addr->addr_type = addr_is_last;
      do
	ch = inchar ();
      while (ch != EOF && isblank (ch));
      savchar (ch);
      return 1;
    }
  else
    savchar (ch);
  return 0;
}

void
buffer_regex (slash)
     int slash;
{
  VOID *b;
  int ch;
  int char_class_pos = -1;

  b = init_buffer ();
  while ((ch = inchar ()) != EOF && (ch != slash || (char_class_pos >= 0)))
    {
      if (ch == '^')
	{
	  if (size_buffer (b) == 0)
	    {
	      add1_buffer (b, '\\');
	      add1_buffer (b, '`');
	    }
	  else
	    add1_buffer (b, ch);
	  continue;
	}
      else if (ch == '$')
	{
	  ch = inchar ();
	  savchar (ch);
	  if (ch == slash)
	    {
	      add1_buffer (b, '\\');
	      add1_buffer (b, '\'');
	    }
	  else
	    add1_buffer (b, '$');
	  continue;
	}
      else if (ch == '[')
	{
	  if (char_class_pos < 0)
	    char_class_pos = size_buffer (b);
	  add1_buffer (b, ch);
	  continue;
	}
      else if (ch == ']')
	{
	  add1_buffer (b, ch);
	  {
	    char * regexp = get_buffer (b);
	    int pos = size_buffer (b) - 1;
	    if (!(   (char_class_pos >= 0)
		  && (   (pos == char_class_pos + 1)
		      || (   (pos == char_class_pos + 2)
			  && (regexp[char_class_pos + 1] == '^')))))
	      char_class_pos = -1;
	    continue;
	  }
	}
      else if (ch != '\\' || (char_class_pos >= 0))
	{
	  add1_buffer (b, ch);
	  continue;
	}
      ch = inchar ();
      switch (ch)
	{
	case 'n':
	  add1_buffer (b, '\n');
	  break;
#if 0
	case 'b':
	  add1_buffer (b, '\b');
	  break;
	case 'f':
	  add1_buffer (b, '\f');
	  break;
	case 'r':
	  add1_buffer (b, '\r');
	  break;
	case 't':
	  add1_buffer (b, '\t');
	  break;
#endif /* 0 */
	case EOF:
	  break;
	default:
	  add1_buffer (b, '\\');
	  add1_buffer (b, ch);
	  break;
	}
    }
  if (ch == EOF)
    bad_prog (BAD_EOF);
  if (size_buffer (b))
    {
      if (last_regex_string)
	last_regex_string = (char *)ck_realloc (last_regex_string,
						size_buffer (b) + 1);
      else
	last_regex_string = (char *)ck_malloc (size_buffer (b) + 1);
      bcopy (get_buffer (b), last_regex_string, size_buffer (b));
      last_regex_string [size_buffer (b)] = 0;
    }
  else if (!last_regex)
    bad_prog (NO_REGEX);
  flush_buffer (b);
}

void
compile_regex ()
{
  const char * error;
  last_regex = ((struct re_pattern_buffer *)
		ck_malloc (sizeof (struct re_pattern_buffer)));
  bzero (last_regex, sizeof (*last_regex));
  last_regex->fastmap = ck_malloc (256);
  error = re_compile_pattern (last_regex_string,
			      strlen (last_regex_string), last_regex);
  if (error)
    bad_prog ((char *)error);
}

/* Store a label (or label reference) created by a ':', 'b', or 't'
   comand so that the jump to/from the lable can be backpatched after
   compilation is complete */
struct sed_label *
setup_jump (list, cmd, vec)
     struct sed_label *list;
     struct sed_cmd *cmd;
     struct vector *vec;
{
  struct sed_label *tmp;
  VOID *b;
  int ch;

  b = init_buffer ();
  while ((ch = inchar ()) != EOF && isblank (ch))
    ;
  /* Possible non posixicity. */
  while (ch != EOF && ch != '\n' && (!isblank (ch)) && ch != ';' && ch != '}')
    {
      add1_buffer (b, ch);
      ch = inchar ();
    }
  savchar (ch);
  add1_buffer (b, '\0');
  tmp = (struct sed_label *) ck_malloc (sizeof (struct sed_label));
  tmp->v = vec;
  tmp->v_index = cmd - vec->v;
  tmp->name = ck_strdup (get_buffer (b));
  tmp->next = list;
  flush_buffer (b);
  return tmp;
}

/* read in a filename for a 'r', 'w', or 's///w' command, and
   update the internal structure about files.  The file is
   opened if it isn't already open. */
FILE *
compile_filename (readit)
     int readit;
{
  char *file_name;
  int n;
  VOID *b;
  int ch;

  if (inchar () != ' ')
    bad_prog ("missing ' ' before filename");
  b = init_buffer ();
  while ((ch = inchar ()) != EOF && ch != '\n')
    add1_buffer (b, ch);
  add1_buffer (b, '\0');
  file_name = get_buffer (b);
  for (n = 0; n < NUM_FPS; n++)
    {
      if (!file_ptrs[n].name)
	break;
    }
  if (n < NUM_FPS)
    {
      file_ptrs[n].name = ck_strdup (file_name);
      if (!readit)
	{
	  if (!file_ptrs[n].for_write)
	    file_ptrs[n].for_write = ck_fopen (file_name, "w");
	}
      else
	{
	  if (!file_ptrs[n].for_read)
	    file_ptrs[n].for_read = fopen (file_name, "r");
	}
      flush_buffer (b);
      return readit ? file_ptrs[n].for_read : file_ptrs[n].for_write;
    }
  else
    {
      bad_prog ("Hopelessely evil compiled in limit on number of open files.  re-compile sed");
      return 0;
    }
}

/* Read a file and apply the compiled script to it. */
void
read_file (name)
     char *name;
{
  if (*name == '-' && name[1] == '\0')
    input_file = stdin;
  else
    {
      input_file = fopen (name, "r");
      if (input_file == 0)
	{
	  extern int errno;
	  extern char *sys_errlist[];
	  extern int sys_nerr;

	  char *ptr;

	  ptr = ((errno >= 0 && errno < sys_nerr)
		 ? sys_errlist[errno] : "Unknown error code");
	  bad_input++;
	  fprintf (stderr, "%s: can't read %s: %s\n", myname, name, ptr);
	  return;
	}
    }
  
  while (read_pattern_space ())
    {
      execute_program (the_program);
      if (!no_default_output)
	ck_fwrite (line.text, 1, line.length, stdout);
      if (append.length)
	{
	  ck_fwrite (append.text, 1, append.length, stdout);
	  append.length = 0;
	}
      if (quit_cmd)
	break;
    }
  ck_fclose (input_file);
}

static char *
eol_pos (str, len)
     char *str;
     int len;
{
  while (len--)
    if (*str++ == '\n')
      return --str;
  return --str;
}

static void
chr_copy (dest, src, len)
     char *dest;
     char *src;
     int len;
{
  while (len--)
    *dest++ = *src++;
}

/* Execute the program 'vec' on the current input line. */
static struct re_registers regs =
{0, 0, 0};

void
execute_program (vec)
     struct vector *vec;
{
  struct sed_cmd *cur_cmd;
  int n;
  int addr_matched;
  static int end_cycle;

  int start;
  int remain;
  int offset;

  static struct line tmp;
  struct line t;
  char *rep, *rep_end, *rep_next, *rep_cur;

  int count;
  struct vector *restart_vec = vec;

restart:
  vec = restart_vec;
  count = 0;

  end_cycle = 0;

  for (cur_cmd = vec->v, n = vec->v_length; n; cur_cmd++, n--)
    {
    exe_loop:
      addr_matched = 0;
      if (cur_cmd->aflags & A1_MATCHED_BIT)
	{
	  addr_matched = 1;
	  if (match_address (&(cur_cmd->a2)))
	    cur_cmd->aflags &= ~A1_MATCHED_BIT;
	}
      else if (match_address (&(cur_cmd->a1)))
	{
	  addr_matched = 1;
	  if (cur_cmd->a2.addr_type != addr_is_null)
	    if (   (cur_cmd->a2.addr_type == addr_is_regex)
		|| !match_address (&(cur_cmd->a2)))
	      cur_cmd->aflags |= A1_MATCHED_BIT;

	}
      if (cur_cmd->aflags & ADDR_BANG_BIT)
	addr_matched = !addr_matched;
      if (!addr_matched)
	continue;
      switch (cur_cmd->cmd)
	{
	case '{':		/* Execute sub-program */
	  if (cur_cmd->x.sub->v_length)
	    {
	      vec = cur_cmd->x.sub;
	      cur_cmd = vec->v;
	      n = vec->v_length;
	      goto exe_loop;
	    }
	  break;

	case '}':
	  cur_cmd = vec->return_v->v + vec->return_i;
	  n = vec->return_v->v_length - vec->return_i;
	  vec = vec->return_v;
	  break;

	case ':':		/* Executing labels is easy. */
	  break;

	case '=':
	  printf ("%d\n", input_line_number);
	  break;

	case 'a':
	  while (append.alloc - append.length < cur_cmd->x.cmd_txt.text_len)
	    {
	      append.alloc *= 2;
	      append.text = ck_realloc (append.text, append.alloc);
	    }
	  bcopy (cur_cmd->x.cmd_txt.text,
		 append.text + append.length, cur_cmd->x.cmd_txt.text_len);
	  append.length += cur_cmd->x.cmd_txt.text_len;
	  break;

	case 'b':
	  if (!cur_cmd->x.jump)
	    end_cycle++;
	  else
	    {
	      struct sed_label *j = cur_cmd->x.jump;

	      n = j->v->v_length - j->v_index;
	      cur_cmd = j->v->v + j->v_index;
	      vec = j->v;
	      goto exe_loop;
	    }
	  break;

	case 'c':
	  line.length = 0;
	  if (!((cur_cmd->aflags & A1_MATCHED_BIT)))
	    ck_fwrite (cur_cmd->x.cmd_txt.text,
		       1, cur_cmd->x.cmd_txt.text_len, stdout);
	  end_cycle++;
	  break;

	case 'd':
	  line.length = 0;
	  end_cycle++;
	  break;

	case 'D':
	  {
	    char *tmp;
	    int newlength;

	    tmp = eol_pos (line.text, line.length);
	    newlength = line.length - (tmp - line.text) - 1;
	    if (newlength)
	      {
		chr_copy (line.text, tmp + 1, newlength);
		line.length = newlength;
		goto restart;
	      }
	    line.length = 0;
	    end_cycle++;
	  }
	  break;

	case 'g':
	  line_copy (&hold, &line);
	  break;

	case 'G':
	  line_append (&hold, &line);
	  break;

	case 'h':
	  line_copy (&line, &hold);
	  break;

	case 'H':
	  line_append (&line, &hold);
	  break;

	case 'i':
	  ck_fwrite (cur_cmd->x.cmd_txt.text, 1,
		     cur_cmd->x.cmd_txt.text_len, stdout);
	  break;

	case 'l':
	  {
	    char *tmp;
	    int n;
	    int width = 0;

	    n = line.length;
	    tmp = line.text;
	    while (n--)
	      {
		/* Skip the trailing newline, if there is one */
		if (!n && (*tmp == '\n'))
		  break;
		if (width > 77)
		  {
		    width = 0;
		    putchar ('\n');
		  }
		if (*tmp == '\\')
		  {
		    printf ("\\\\");
		    width += 2;
		  }
		else if (isprint (*tmp))
		  {
		    putchar (*tmp);
		    width++;
		  }
		else
		  switch (*tmp)
		    {
#if 0
		      /* Should print \00 instead of \0 because (a) POSIX */
		      /* requires it, and (b) this way \01 is unambiguous.  */
		    case '\0':
		      printf ("\\0");
		      width += 2;
		      break;
#endif
		    case 007:
		      printf ("\\a");
		      width += 2;
		      break;
		    case '\b':
		      printf ("\\b");
		      width += 2;
		      break;
		    case '\f':
		      printf ("\\f");
		      width += 2;
		      break;
		    case '\n':
		      printf ("\\n");
		      width += 2;
		      break;
		    case '\r':
		      printf ("\\r");
		      width += 2;
		      break;
		    case '\t':
		      printf ("\\t");
		      width += 2;
		      break;
		    case '\v':
		      printf ("\\v");
		      width += 2;
		      break;
		    default:
		      printf ("\\%02x", (*tmp) & 0xFF);
		      width += 2;
		      break;
		    }
		tmp++;
	      }
	    putchar ('\n');
	  }
	  break;

	case 'n':
	  if (feof (input_file))
	    goto quit;
	  if (!no_default_output)
	    ck_fwrite (line.text, 1, line.length, stdout);
	  read_pattern_space ();
	  break;

	case 'N':
	  if (feof (input_file))
	    {
	      line.length = 0;
	      goto quit;
	    }
	  append_pattern_space ();
	  break;

	case 'p':
	  ck_fwrite (line.text, 1, line.length, stdout);
	  break;

	case 'P':
	  {
	    char *tmp;

	    tmp = eol_pos (line.text, line.length);
	    ck_fwrite (line.text, 1,
		       tmp ? tmp - line.text + 1
		       : line.length, stdout);
	  }
	  break;

	case 'q':
	quit:
	  quit_cmd++;
	  end_cycle++;
	  break;

	case 'r':
	  {
	    int n = 0;

	    if (cur_cmd->x.io_file)
	      {
		rewind (cur_cmd->x.io_file);
		do
		  {
		    append.length += n;
		    if (append.length == append.alloc)
		      {
			append.alloc *= 2;
			append.text = ck_realloc (append.text, append.alloc);
		      }
		    n = fread (append.text + append.length, sizeof (char),
			       append.alloc - append.length,
			       cur_cmd->x.io_file);
		  }
		while (n > 0);
		if (ferror (cur_cmd->x.io_file))
		  panic ("Read error on input file to 'r' command");
	      }
	  }
	  break;

	case 's':
	  {
	    int trail_nl_p = line.text [line.length - 1] == '\n';
	    if (!tmp.alloc)
	      {
		tmp.alloc = 50;
		tmp.text = ck_malloc (50);
	      }
	    count = 0;
	    start = 0;
	    remain = line.length - trail_nl_p;
	    tmp.length = 0;
	    rep = cur_cmd->x.cmd_regex.replacement;
	    rep_end = rep + cur_cmd->x.cmd_regex.replace_length;
	    
	    while ((offset = re_search (cur_cmd->x.cmd_regex.regx,
					line.text,
					line.length - trail_nl_p,
					start,
					remain,
					&regs)) >= 0)
	      {
		count++;
		if (offset - start)
		  str_append (&tmp, line.text + start, offset - start);
		
		if (cur_cmd->x.cmd_regex.flags & S_NUM_BIT)
		  {
		    if (count != cur_cmd->x.cmd_regex.numb)
		      {
			int matched = regs.end[0] - regs.start[0];
			if (!matched) matched = 1;
			str_append (&tmp, line.text + regs.start[0], matched);
			start = (offset == regs.end[0]
				 ? offset + 1 : regs.end[0]);
			remain = (line.length - trail_nl_p) - start;
			continue;
		      }
		  }
		
		for (rep_next = rep_cur = rep; rep_next < rep_end; rep_next++)
		  {
		    if (*rep_next == '&')
		      {
			if (rep_next - rep_cur)
			  str_append (&tmp, rep_cur, rep_next - rep_cur);
			str_append (&tmp, line.text + regs.start[0], regs.end[0] - regs.start[0]);
			rep_cur = rep_next + 1;
		      }
		    else if (*rep_next == '\\')
		      {
			if (rep_next - rep_cur)
			  str_append (&tmp, rep_cur, rep_next - rep_cur);
			rep_next++;
			if (rep_next != rep_end)
			  {
			    int n;
			    
			    if (*rep_next >= '0' && *rep_next <= '9')
			      {
				n = *rep_next - '0';
				str_append (&tmp, line.text + regs.start[n], regs.end[n] - regs.start[n]);
			      }
			    else
			      str_append (&tmp, rep_next, 1);
			  }
			rep_cur = rep_next + 1;
		      }
		  }
		if (rep_next - rep_cur)
		  str_append (&tmp, rep_cur, rep_next - rep_cur);
		if (offset == regs.end[0])
		  {
		    str_append (&tmp, line.text + offset, 1);
		    ++regs.end[0];
		  }
		start = regs.end[0];
		
		remain = (line.length - trail_nl_p) - start;
		if (remain < 0)
		  break;
		if (!(cur_cmd->x.cmd_regex.flags & S_GLOBAL_BIT))
		  break;
	      }
	    if (!count)
	      break;
	    replaced = 1;
	    str_append (&tmp, line.text + start, remain + trail_nl_p);
	    t.text = line.text;
	    t.length = line.length;
	    t.alloc = line.alloc;
	    line.text = tmp.text;
	    line.length = tmp.length;
	    line.alloc = tmp.alloc;
	    tmp.text = t.text;
	    tmp.length = t.length;
	    tmp.alloc = t.alloc;
	    if ((cur_cmd->x.cmd_regex.flags & S_WRITE_BIT)
		&& cur_cmd->x.cmd_regex.wio_file)
	      ck_fwrite (line.text, 1, line.length,
			 cur_cmd->x.cmd_regex.wio_file);
	    if (cur_cmd->x.cmd_regex.flags & S_PRINT_BIT)
	      ck_fwrite (line.text, 1, line.length, stdout);
	    break;
	  }
	    
	case 't':
	  if (replaced)
	    {
	      replaced = 0;
	      if (!cur_cmd->x.jump)
		end_cycle++;
	      else
		{
		  struct sed_label *j = cur_cmd->x.jump;

		  n = j->v->v_length - j->v_index;
		  cur_cmd = j->v->v + j->v_index;
		  vec = j->v;
		  goto exe_loop;
		}
	    }
	  break;

	case 'w':
	  if (cur_cmd->x.io_file)
	    {
	      ck_fwrite (line.text, 1, line.length, cur_cmd->x.io_file);
	      fflush (cur_cmd->x.io_file);
	    }
	  break;

	case 'x':
	  {
	    struct line tmp;

	    tmp = line;
	    line = hold;
	    hold = tmp;
	  }
	  break;

	case 'y':
	  {
	    unsigned char *p, *e;

	    for (p = (unsigned char *) (line.text), e = p + line.length;
		 p < e;
		 p++)
	      *p = cur_cmd->x.translate[*p];
	  }
	  break;

	default:
	  panic ("INTERNAL ERROR: Bad cmd %c", cur_cmd->cmd);
	}
      if (end_cycle)
	break;
    }
}


/* Return non-zero if the current line matches the address
   pointed to by 'addr'. */
int
match_address (addr)
     struct addr *addr;
{
  switch (addr->addr_type)
    {
    case addr_is_null:
      return 1;
    case addr_is_num:
      return (input_line_number == addr->addr_number);
    case addr_is_mod:
      return ((input_line_number%addr->modulo) == addr->offset);


    case addr_is_regex:
      {
	int trail_nl_p = line.text [line.length - 1] == '\n';
	return (re_search (addr->addr_regex,
			   line.text,
			   line.length - trail_nl_p,
			   0,
			   line.length - trail_nl_p,
			   (struct re_registers *) 0) >= 0) ? 1 : 0;
      }
    case addr_is_last:
      return (input_EOF) ? 1 : 0;

    default:
      panic ("INTERNAL ERROR: bad address type");
      break;
    }
  return -1;
}

/* Read in the next line of input, and store it in the
   pattern space.  Return non-zero if this is the last line of input */

int
read_pattern_space ()
{
  int n;
  char *p;
  int ch;

  p = line.text;
  n = line.alloc;

  if (feof (input_file))
    return 0;
  input_line_number++;
  replaced = 0;
  for (;;)
    {
      if (n == 0)
	{
	  line.text = ck_realloc (line.text, line.alloc * 2);
	  p = line.text + line.alloc;
	  n = line.alloc;
	  line.alloc *= 2;
	}
      ch = getc (input_file);
      if (ch == EOF)
	{
	  if (n == line.alloc)
	    return 0;
	  /* *p++ = '\n'; */
	  /* --n; */
	  line.length = line.alloc - n;
	  if (last_input_file)
	    input_EOF++;
	  return 1;
	}
      *p++ = ch;
      --n;
      if (ch == '\n')
	{
	  line.length = line.alloc - n;
	  break;
	}
    }
  ch = getc (input_file);
  if (ch != EOF)
    ungetc (ch, input_file);
  else if (last_input_file)
    input_EOF++;
  return 1;
}

/* Inplement the 'N' command, which appends the next line of input to
   the pattern space. */
void
append_pattern_space ()
{
  char *p;
  int n;
  int ch;

  p = line.text + line.length;
  n = line.alloc - line.length;

  input_line_number++;
  replaced = 0;
  for (;;)
    {
      ch = getc (input_file);
      if (ch == EOF)
	{
	  if (n == line.alloc)
	    return;
	  /* *p++ = '\n'; */
	  /* --n; */
	  line.length = line.alloc - n;
	  if (last_input_file)
	    input_EOF++;
	  return;
	}
      if (n == 0)
	{
	  line.text = ck_realloc (line.text, line.alloc * 2);
	  p = line.text + line.alloc;
	  n = line.alloc;
	  line.alloc *= 2;
	}
      *p++ = ch;
      --n;
      if (ch == '\n')
	{
	  line.length = line.alloc - n;
	  break;
	}
    }
  ch = getc (input_file);
  if (ch != EOF)
    ungetc (ch, input_file);
  else if (last_input_file)
    input_EOF++;
}

/* Copy the contents of the line 'from' into the line 'to'.
   This destroys the old contents of 'to'.  It will still work
   if the line 'from' contains nulls. */
void
line_copy (from, to)
     struct line *from, *to;
{
  if (from->length > to->alloc)
    {
      to->alloc = from->length;
      to->text = ck_realloc (to->text, to->alloc);
    }
  bcopy (from->text, to->text, from->length);
  to->length = from->length;
}

/* Append the contents of the line 'from' to the line 'to'.
   This routine will work even if the line 'from' contains nulls */
void
line_append (from, to)
     struct line *from, *to;
{
  if (from->length > (to->alloc - to->length))
    {
      to->alloc += from->length;
      to->text = ck_realloc (to->text, to->alloc);
    }
  bcopy (from->text, to->text + to->length, from->length);
  to->length += from->length;
}

/* Append 'length' bytes from 'string' to the line 'to'
   This routine *will* append bytes with nulls in them, without
   failing. */
void
str_append (to, string, length)
     struct line *to;
     char *string;
     int length;
{
  if (length > to->alloc - to->length)
    {
      to->alloc += length;
      to->text = ck_realloc (to->text, to->alloc);
    }
  bcopy (string, to->text + to->length, length);
  to->length += length;
}

void
usage (status)
     int status;
{
  fprintf (status ? stderr : stdout, "\
Usage: %s [-nV] [--quiet] [--silent] [--version] [-e script]\n\
        [-f script-file] [--expression=script] [--file=script-file] [file...]\n",
	   myname);
  exit (status);
}
