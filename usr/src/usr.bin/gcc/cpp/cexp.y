/* Parse C expressions for CCCP.
   Copyright (C) 1987 Free Software Foundation.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 1, or (at your option) any
later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

 In other words, you are welcome to use, share and improve this program.
 You are forbidden to forbid anyone else to use, share and improve
 what you give them.   Help stamp out software-hoarding!

 Adapted from expread.y of GDB by Paul Rubin, July 1986.

/* Parse a C expression from text in a string  */
   
%{
#include "config.h"
#include <setjmp.h>
/* #define YYDEBUG 1 */

  int yylex ();
  void yyerror ();
  int expression_value;

  static jmp_buf parse_return_error;

  /* some external tables of character types */
  extern unsigned char is_idstart[], is_idchar[];

#ifndef CHAR_TYPE_SIZE
#define CHAR_TYPE_SIZE BITS_PER_UNIT
#endif
%}

%union {
  struct constant {long value; int unsignedp;} integer;
  int voidval;
  char *sval;
}

%type <integer> exp exp1 start
%token <integer> INT CHAR
%token <sval> NAME
%token <integer> ERROR

%right '?' ':'
%left ','
%left OR
%left AND
%left '|'
%left '^'
%left '&'
%left EQUAL NOTEQUAL
%left '<' '>' LEQ GEQ
%left LSH RSH
%left '+' '-'
%left '*' '/' '%'
%right UNARY

/* %expect 40 */

%%

start   :	exp1
		{ expression_value = $1.value; }
	;

/* Expressions, including the comma operator.  */
exp1	:	exp
	|	exp1 ',' exp
			{ $$ = $3; }
	;

/* Expressions, not including the comma operator.  */
exp	:	'-' exp    %prec UNARY
			{ $$.value = - $2.value;
			  $$.unsignedp = $2.unsignedp; }
	|	'!' exp    %prec UNARY
			{ $$.value = ! $2.value;
			  $$.unsignedp = 0; }
	|	'+' exp    %prec UNARY
			{ $$ = $2; }
	|	'~' exp    %prec UNARY
			{ $$.value = ~ $2.value;
			  $$.unsignedp = $2.unsignedp; }
	|	'(' exp1 ')'
			{ $$ = $2; }
	;

/* Binary operators in order of decreasing precedence.  */
exp	:	exp '*' exp
			{ $$.unsignedp = $1.unsignedp || $3.unsignedp;
			  if ($$.unsignedp)
			    $$.value = (unsigned) $1.value * $3.value;
			  else
			    $$.value = $1.value * $3.value; }
	|	exp '/' exp
			{ if ($3.value == 0)
			    {
			      error ("division by zero in #if");
			      $3.value = 1;
			    }
			  $$.unsignedp = $1.unsignedp || $3.unsignedp;
			  if ($$.unsignedp)
			    $$.value = (unsigned) $1.value / $3.value;
			  else
			    $$.value = $1.value / $3.value; }
	|	exp '%' exp
			{ if ($3.value == 0)
			    {
			      error ("division by zero in #if");
			      $3.value = 1;
			    }
			  $$.unsignedp = $1.unsignedp || $3.unsignedp;
			  if ($$.unsignedp)
			    $$.value = (unsigned) $1.value % $3.value;
			  else
			    $$.value = $1.value % $3.value; }
	|	exp '+' exp
			{ $$.value = $1.value + $3.value;
			  $$.unsignedp = $1.unsignedp || $3.unsignedp; }
	|	exp '-' exp
			{ $$.value = $1.value - $3.value;
			  $$.unsignedp = $1.unsignedp || $3.unsignedp; }
	|	exp LSH exp
			{ $$.unsignedp = $1.unsignedp;
			  if ($$.unsignedp)
			    $$.value = (unsigned) $1.value << $3.value;
			  else
			    $$.value = $1.value << $3.value; }
	|	exp RSH exp
			{ $$.unsignedp = $1.unsignedp;
			  if ($$.unsignedp)
			    $$.value = (unsigned) $1.value >> $3.value;
			  else
			    $$.value = $1.value >> $3.value; }
	|	exp EQUAL exp
			{ $$.value = ($1.value == $3.value);
			  $$.unsignedp = 0; }
	|	exp NOTEQUAL exp
			{ $$.value = ($1.value != $3.value);
			  $$.unsignedp = 0; }
	|	exp LEQ exp
			{ $$.unsignedp = 0;
			  if ($1.unsignedp || $3.unsignedp)
			    $$.value = (unsigned) $1.value <= $3.value;
			  else
			    $$.value = $1.value <= $3.value; }
	|	exp GEQ exp
			{ $$.unsignedp = 0;
			  if ($1.unsignedp || $3.unsignedp)
			    $$.value = (unsigned) $1.value >= $3.value;
			  else
			    $$.value = $1.value >= $3.value; }
	|	exp '<' exp
			{ $$.unsignedp = 0;
			  if ($1.unsignedp || $3.unsignedp)
			    $$.value = (unsigned) $1.value < $3.value;
			  else
			    $$.value = $1.value < $3.value; }
	|	exp '>' exp
			{ $$.unsignedp = 0;
			  if ($1.unsignedp || $3.unsignedp)
			    $$.value = (unsigned) $1.value > $3.value;
			  else
			    $$.value = $1.value > $3.value; }
	|	exp '&' exp
			{ $$.value = $1.value & $3.value;
			  $$.unsignedp = $1.unsignedp || $3.unsignedp; }
	|	exp '^' exp
			{ $$.value = $1.value ^ $3.value;
			  $$.unsignedp = $1.unsignedp || $3.unsignedp; }
	|	exp '|' exp
			{ $$.value = $1.value | $3.value;
			  $$.unsignedp = $1.unsignedp || $3.unsignedp; }
	|	exp AND exp
			{ $$.value = ($1.value && $3.value);
			  $$.unsignedp = 0; }
	|	exp OR exp
			{ $$.value = ($1.value || $3.value);
			  $$.unsignedp = 0; }
	|	exp '?' exp ':' exp
			{ $$.value = $1.value ? $3.value : $5.value;
			  $$.unsignedp = $3.unsignedp || $5.unsignedp; }
	|	INT
			{ $$ = yylval.integer; }
	|	CHAR
			{ $$ = yylval.integer; }
	|	NAME
			{ $$.value = 0;
			  $$.unsignedp = 0; }
	;
%%

/* During parsing of a C expression, the pointer to the next character
   is in this variable.  */

static char *lexptr;

/* Take care of parsing a number (anything that starts with a digit).
   Set yylval and return the token type; update lexptr.
   LEN is the number of characters in it.  */

/* maybe needs to actually deal with floating point numbers */

int
parse_number (olen)
     int olen;
{
  register char *p = lexptr;
  register long n = 0;
  register int c;
  register int base = 10;
  register int len = olen;

  for (c = 0; c < len; c++)
    if (p[c] == '.') {
      /* It's a float since it contains a point.  */
      yyerror ("floating point numbers not allowed in #if expressions");
      return ERROR;
    }

  yylval.integer.unsignedp = 0;

  if (len >= 3 && (!strncmp (p, "0x", 2) || !strncmp (p, "0X", 2))) {
    p += 2;
    base = 16;
    len -= 2;
  }
  else if (*p == '0')
    base = 8;

  while (len > 0) {
    c = *p++;
    len--;
    if (c >= 'A' && c <= 'Z') c += 'a' - 'A';

    if (c >= '0' && c <= '9') {
      n *= base;
      n += c - '0';
    } else if (base == 16 && c >= 'a' && c <= 'f') {
      n *= base;
      n += c - 'a' + 10;
    } else {
      /* `l' means long, and `u' means unsigned.  */
      while (1) {
	if (c == 'l' || c == 'L')
	  ;
	else if (c == 'u' || c == 'U')
	  yylval.integer.unsignedp = 1;
	else
	  break;

	if (len == 0)
	  break;
	c = *p++;
	len--;
      }
      /* Don't look for any more digits after the suffixes.  */
      break;
    }
  }

  if (len != 0) {
    yyerror ("Invalid number in #if expression");
    return ERROR;
  }

  /* If too big to be signed, consider it unsigned.  */
  if (n < 0)
    yylval.integer.unsignedp = 1;

  lexptr = p;
  yylval.integer.value = n;
  return INT;
}

struct token {
  char *operator;
  int token;
};

#ifndef NULL
#define NULL 0
#endif

static struct token tokentab2[] = {
  {"&&", AND},
  {"||", OR},
  {"<<", LSH},
  {">>", RSH},
  {"==", EQUAL},
  {"!=", NOTEQUAL},
  {"<=", LEQ},
  {">=", GEQ},
  {NULL, ERROR}
};

/* Read one token, getting characters through lexptr.  */

int
yylex ()
{
  register int c;
  register int namelen;
  register char *tokstart;
  register struct token *toktab;

 retry:

  tokstart = lexptr;
  c = *tokstart;
  /* See if it is a special token of length 2.  */
  for (toktab = tokentab2; toktab->operator != NULL; toktab++)
    if (c == *toktab->operator && tokstart[1] == toktab->operator[1]) {
      lexptr += 2;
      return toktab->token;
    }

  switch (c) {
  case 0:
    return 0;
    
  case ' ':
  case '\t':
  case '\r':
  case '\n':
    lexptr++;
    goto retry;
    
  case '\'':
    lexptr++;
    c = *lexptr++;
    if (c == '\\')
      c = parse_escape (&lexptr);

    /* Sign-extend the constant if chars are signed on target machine.  */
    {
      if (lookup ("__CHAR_UNSIGNED__", sizeof ("__CHAR_UNSIGNED__")-1, -1)
	  || ((c >> (CHAR_TYPE_SIZE - 1)) & 1) == 0)
	yylval.integer.value = c & ((1 << CHAR_TYPE_SIZE) - 1);
      else
	yylval.integer.value = c | ~((1 << CHAR_TYPE_SIZE) - 1);
    }

    yylval.integer.unsignedp = 0;
    c = *lexptr++;
    if (c != '\'') {
      yyerror ("Invalid character constant in #if");
      return ERROR;
    }
    
    return CHAR;

    /* some of these chars are invalid in constant expressions;
       maybe do something about them later */
  case '/':
  case '+':
  case '-':
  case '*':
  case '%':
  case '|':
  case '&':
  case '^':
  case '~':
  case '!':
  case '@':
  case '<':
  case '>':
  case '(':
  case ')':
  case '[':
  case ']':
  case '.':
  case '?':
  case ':':
  case '=':
  case '{':
  case '}':
  case ',':
    lexptr++;
    return c;
    
  case '"':
    yyerror ("double quoted strings not allowed in #if expressions");
    return ERROR;
  }
  if (c >= '0' && c <= '9') {
    /* It's a number */
    for (namelen = 0;
	 c = tokstart[namelen], is_idchar[c] || c == '.'; 
	 namelen++)
      ;
    return parse_number (namelen);
  }
  
  if (!is_idstart[c]) {
    yyerror ("Invalid token in expression");
    return ERROR;
  }
  
  /* It is a name.  See how long it is.  */
  
  for (namelen = 0; is_idchar[tokstart[namelen]]; namelen++)
    ;
  
  lexptr += namelen;
  return NAME;
}


/* Parse a C escape sequence.  STRING_PTR points to a variable
   containing a pointer to the string to parse.  That pointer
   is updated past the characters we use.  The value of the
   escape sequence is returned.

   A negative value means the sequence \ newline was seen,
   which is supposed to be equivalent to nothing at all.

   If \ is followed by a null character, we return a negative
   value and leave the string pointer pointing at the null character.

   If \ is followed by 000, we return 0 and leave the string pointer
   after the zeros.  A value of 0 does not mean end of string.  */

int
parse_escape (string_ptr)
     char **string_ptr;
{
  register int c = *(*string_ptr)++;
  switch (c)
    {
    case 'a':
      return TARGET_BELL;
    case 'b':
      return TARGET_BS;
    case 'e':
      return 033;
    case 'f':
      return TARGET_FF;
    case 'n':
      return TARGET_NEWLINE;
    case 'r':
      return TARGET_CR;
    case 't':
      return TARGET_TAB;
    case 'v':
      return TARGET_VT;
    case '\n':
      return -2;
    case 0:
      (*string_ptr)--;
      return 0;
    case '^':
      c = *(*string_ptr)++;
      if (c == '\\')
	c = parse_escape (string_ptr);
      if (c == '?')
	return 0177;
      return (c & 0200) | (c & 037);
      
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
      {
	register int i = c - '0';
	register int count = 0;
	while (++count < 3)
	  {
	    c = *(*string_ptr)++;
	    if (c >= '0' && c <= '7')
	      i = (i << 3) + c - '0';
	    else
	      {
		(*string_ptr)--;
		break;
	      }
	  }
	if ((i & ~((1 << CHAR_TYPE_SIZE) - 1)) != 0)
	  {
	    i &= (1 << CHAR_TYPE_SIZE) - 1;
	    warning ("octal character constant does not fit in a byte");
	  }
	return i;
      }
    case 'x':
      {
	register int i = 0;
	register int count = 0;
	for (;;)
	  {
	    c = *(*string_ptr)++;
	    if (c >= '0' && c <= '9')
	      i = (i << 4) + c - '0';
	    else if (c >= 'a' && c <= 'f')
	      i = (i << 4) + c - 'a' + 10;
	    else if (c >= 'A' && c <= 'F')
	      i = (i << 4) + c - 'A' + 10;
	    else
	      {
		(*string_ptr)--;
		break;
	      }
	  }
	if ((i & ~((1 << BITS_PER_UNIT) - 1)) != 0)
	  {
	    i &= (1 << BITS_PER_UNIT) - 1;
	    warning ("hex character constant does not fit in a byte");
	  }
	return i;
      }
    default:
      return c;
    }
}

void
yyerror (s)
     char *s;
{
  error (s);
  longjmp (parse_return_error, 1);
}

/* This page contains the entry point to this file.  */

/* Parse STRING as an expression, and complain if this fails
   to use up all of the contents of STRING.  */
/* We do not support C comments.  They should be removed before
   this function is called.  */

int
parse_c_expression (string)
     char *string;
{
  lexptr = string;
  
  if (lexptr == 0 || *lexptr == 0) {
    error ("empty #if expression");
    return 0;			/* don't include the #if group */
  }

  /* if there is some sort of scanning error, just return 0 and assume
     the parsing routine has printed an error message somewhere.
     there is surely a better thing to do than this.     */
  if (setjmp (parse_return_error))
    return 0;

  if (yyparse ())
    return 0;			/* actually this is never reached
				   the way things stand. */
  if (*lexptr)
    error ("Junk after end of expression.");

  return expression_value;	/* set by yyparse () */
}

#ifdef TEST_EXP_READER
/* main program, for testing purposes. */
main ()
{
  int n, c;
  char buf[1024];
  extern int yydebug;
/*
  yydebug = 1;
*/
  initialize_random_junk ();

  for (;;) {
    printf ("enter expression: ");
    n = 0;
    while ((buf[n] = getchar ()) != '\n' && buf[n] != EOF)
      n++;
    if (buf[n] == EOF)
      break;
    buf[n] = '\0';
    printf ("parser returned %d\n", parse_c_expression (buf));
  }
}

/* table to tell if char can be part of a C identifier. */
unsigned char is_idchar[256];
/* table to tell if char can be first char of a c identifier. */
unsigned char is_idstart[256];
/* table to tell if c is horizontal space.  isspace () thinks that
   newline is space; this is not a good idea for this program. */
char is_hor_space[256];

/*
 * initialize random junk in the hash table and maybe other places
 */
initialize_random_junk ()
{
  register int i;

  /*
   * Set up is_idchar and is_idstart tables.  These should be
   * faster than saying (is_alpha (c) || c == '_'), etc.
   * Must do set up these things before calling any routines tthat
   * refer to them.
   */
  for (i = 'a'; i <= 'z'; i++) {
    ++is_idchar[i - 'a' + 'A'];
    ++is_idchar[i];
    ++is_idstart[i - 'a' + 'A'];
    ++is_idstart[i];
  }
  for (i = '0'; i <= '9'; i++)
    ++is_idchar[i];
  ++is_idchar['_'];
  ++is_idstart['_'];
#if DOLLARS_IN_IDENTIFIERS
  ++is_idchar['$'];
  ++is_idstart['$'];
#endif

  /* horizontal space table */
  ++is_hor_space[' '];
  ++is_hor_space['\t'];
}

error (msg)
{
  printf ("error: %s\n", msg);
}

warning (msg)
{
  printf ("warning: %s\n", msg);
}

struct hashnode *
lookup (name, len, hash)
     char *name;
     int len;
     int hash;
{
  return (DEFAULT_SIGNED_CHAR) ? 0 : ((struct hashnode *) -1);
}
#endif
