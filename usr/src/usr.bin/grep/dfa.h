/* dfa.h - declarations for GNU deterministic regexp compiler
   Copyright (C) 1988 Free Software Foundation, Inc.
                      Written June, 1988 by Mike Haertel

		       NO WARRANTY

  BECAUSE THIS PROGRAM IS LICENSED FREE OF CHARGE, WE PROVIDE ABSOLUTELY
NO WARRANTY, TO THE EXTENT PERMITTED BY APPLICABLE STATE LAW.  EXCEPT
WHEN OTHERWISE STATED IN WRITING, FREE SOFTWARE FOUNDATION, INC,
RICHARD M. STALLMAN AND/OR OTHER PARTIES PROVIDE THIS PROGRAM "AS IS"
WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS FOR A PARTICULAR PURPOSE.  THE ENTIRE RISK AS TO THE QUALITY
AND PERFORMANCE OF THE PROGRAM IS WITH YOU.  SHOULD THE PROGRAM PROVE
DEFECTIVE, YOU ASSUME THE COST OF ALL NECESSARY SERVICING, REPAIR OR
CORRECTION.

 IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW WILL RICHARD M.
STALLMAN, THE FREE SOFTWARE FOUNDATION, INC., AND/OR ANY OTHER PARTY
WHO MAY MODIFY AND REDISTRIBUTE THIS PROGRAM AS PERMITTED BELOW, BE
LIABLE TO YOU FOR DAMAGES, INCLUDING ANY LOST PROFITS, LOST MONIES, OR
OTHER SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE
USE OR INABILITY TO USE (INCLUDING BUT NOT LIMITED TO LOSS OF DATA OR
DATA BEING RENDERED INACCURATE OR LOSSES SUSTAINED BY THIRD PARTIES OR
A FAILURE OF THE PROGRAM TO OPERATE WITH ANY OTHER PROGRAMS) THIS
PROGRAM, EVEN IF YOU HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH
DAMAGES, OR FOR ANY CLAIM BY ANY OTHER PARTY.

		GENERAL PUBLIC LICENSE TO COPY

  1. You may copy and distribute verbatim copies of this source file
as you receive it, in any medium, provided that you conspicuously and
appropriately publish on each copy a valid copyright notice "Copyright
 (C) 1988 Free Software Foundation, Inc."; and include following the
copyright notice a verbatim copy of the above disclaimer of warranty
and of this License.  You may charge a distribution fee for the
physical act of transferring a copy.

  2. You may modify your copy or copies of this source file or
any portion of it, and copy and distribute such modifications under
the terms of Paragraph 1 above, provided that you also do the following:

    a) cause the modified files to carry prominent notices stating
    that you changed the files and the date of any change; and

    b) cause the whole of any work that you distribute or publish,
    that in whole or in part contains or is a derivative of this
    program or any part thereof, to be licensed at no charge to all
    third parties on terms identical to those contained in this
    License Agreement (except that you may choose to grant more extensive
    warranty protection to some or all third parties, at your option).

    c) You may charge a distribution fee for the physical act of
    transferring a copy, and you may at your option offer warranty
    protection in exchange for a fee.

Mere aggregation of another unrelated program with this program (or its
derivative) on a volume of a storage or distribution medium does not bring
the other program under the scope of these terms.

  3. You may copy and distribute this program or any portion of it in
compiled, executable or object code form under the terms of Paragraphs
1 and 2 above provided that you do the following:

    a) accompany it with the complete corresponding machine-readable
    source code, which must be distributed under the terms of
    Paragraphs 1 and 2 above; or,

    b) accompany it with a written offer, valid for at least three
    years, to give any third party free (except for a nominal
    shipping charge) a complete machine-readable copy of the
    corresponding source code, to be distributed under the terms of
    Paragraphs 1 and 2 above; or,

    c) accompany it with the information you received as to where the
    corresponding source code may be obtained.  (This alternative is
    allowed only for noncommercial distribution and only if you
    received the program in object code or executable form alone.)

For an executable file, complete source code means all the source code for
all modules it contains; but, as a special exception, it need not include
source code for modules which are standard libraries that accompany the
operating system on which the executable file runs.

  4. You may not copy, sublicense, distribute or transfer this program
except as expressly provided under this License Agreement.  Any attempt
otherwise to copy, sublicense, distribute or transfer this program is void and
your rights to use the program under this License agreement shall be
automatically terminated.  However, parties who have received computer
software programs from you with this License Agreement will not have
their licenses terminated so long as such parties remain in full compliance.

  5. If you wish to incorporate parts of this program into other free
programs whose distribution conditions are different, write to the Free
Software Foundation at 675 Mass Ave, Cambridge, MA 02139.  We have not yet
worked out a simple rule that can be stated here, but we will often permit
this.  We will be guided by the two goals of preserving the free status of
all derivatives our free software and of promoting the sharing and reuse of
software.


In other words, you are welcome to use, share and improve this program.
You are forbidden to forbid anyone else to use, share and improve
what you give them.   Help stamp out software-hoarding!  */


#ifdef USG
#include <string.h>
extern char *index();
#else
#include <strings.h>
/*extern char *strchr(), *strrchr(), *memcpy();*/
#endif

#ifdef __STDC__

/* Missing include files for GNU C. */
#include <stdlib.h>
/*typedef int size_t;
extern void *calloc(int, size_t);
extern void *malloc(size_t);
extern void *realloc(void *, size_t);
extern void free(void *);

extern char *bcopy(), *bzero();*/

#ifdef SOMEDAY
#define ISALNUM(c) isalnum(c)
#define ISALPHA(c) isalpha(c)
#define ISUPPER(c) isupper(c)
#else
#define ISALNUM(c) (isascii(c) && isalnum(c))
#define ISALPHA(c) (isascii(c) && isalpha(c))
#define ISUPPER(c) (isascii(c) && isupper(c))
#endif

#else /* ! __STDC__ */

#define const
typedef int size_t;
extern char *calloc(), *malloc(), *realloc();
extern void free();

extern char *bcopy(), *bzero();

#define ISALNUM(c) (isascii(c) && isalnum(c))
#define ISALPHA(c) (isascii(c) && isalpha(c))
#define ISUPPER(c) (isascii(c) && isupper(c))

#endif /* ! __STDC__ */

/* 1 means plain parentheses serve as grouping, and backslash
     parentheses are needed for literal searching.
   0 means backslash-parentheses are grouping, and plain parentheses
     are for literal searching.  */
#define RE_NO_BK_PARENS 1

/* 1 means plain | serves as the "or"-operator, and \| is a literal.
   0 means \| serves as the "or"-operator, and | is a literal.  */
#define RE_NO_BK_VBAR 2

/* 0 means plain + or ? serves as an operator, and \+, \? are literals.
   1 means \+, \? are operators and plain +, ? are literals.  */
#define RE_BK_PLUS_QM 4

/* 1 means | binds tighter than ^ or $.
   0 means the contrary.  */
#define RE_TIGHT_VBAR 8

/* 1 means treat \n as an _OR operator
   0 means treat it as a normal character */
#define RE_NEWLINE_OR 16

/* 0 means that a special characters (such as *, ^, and $) always have
     their special meaning regardless of the surrounding context.
   1 means that special characters may act as normal characters in some
     contexts.  Specifically, this applies to:
	^ - only special at the beginning, or after ( or |
	$ - only special at the end, or before ) or |
	*, +, ? - only special when not after the beginning, (, or | */
#define RE_CONTEXT_INDEP_OPS 32

/* Now define combinations of bits for the standard possibilities.  */
#define RE_SYNTAX_AWK (RE_NO_BK_PARENS | RE_NO_BK_VBAR | RE_CONTEXT_INDEP_OPS)
#define RE_SYNTAX_EGREP (RE_SYNTAX_AWK | RE_NEWLINE_OR)
#define RE_SYNTAX_GREP (RE_BK_PLUS_QM | RE_NEWLINE_OR)
#define RE_SYNTAX_EMACS 0

/* The NULL pointer. */
#define NULL 0

/* Number of bits in an unsigned char. */
#define CHARBITS 8

/* First integer value that is greater than any character code. */
#define _NOTCHAR (1 << CHARBITS)

/* INTBITS need not be exact, just a lower bound. */
#define INTBITS (CHARBITS * sizeof (int))

/* Number of ints required to hold a bit for every character. */
#define _CHARSET_INTS ((_NOTCHAR + INTBITS - 1) / INTBITS)

/* Sets of unsigned characters are stored as bit vectors in arrays of ints. */
typedef int _charset[_CHARSET_INTS];

/* The regexp is parsed into an array of tokens in postfix form.  Some tokens
   are operators and others are terminal symbols.  Most (but not all) of these
   codes are returned by the lexical analyzer. */
#ifdef __STDC__

typedef enum
{
  _END = -1,			/* _END is a terminal symbol that matches the
				   end of input; any value of _END or less in
				   the parse tree is such a symbol.  Accepting
				   states of the DFA are those that would have
				   a transition on _END. */

  /* Ordinary character values are terminal symbols that match themselves. */

  _EMPTY = _NOTCHAR,		/* _EMPTY is a terminal symbol that matches
				   the empty string. */

  _BACKREF,			/* _BACKREF is generated by \<digit>; it
				   it not completely handled.  If the scanner
				   detects a transition on backref, it returns
				   a kind of "semi-success" indicating that
				   the match will have to be verified with
				   a backtracking matcher. */

  _BEGLINE,			/* _BEGLINE is a terminal symbol that matches
				   the empty string if it is at the beginning
				   of a line. */

  _ALLBEGLINE,			/* _ALLBEGLINE is a terminal symbol that
				   matches the empty string if it is at the
				   beginning of a line; _ALLBEGLINE applies
				   to the entire regexp and can only occur
				   as the first token thereof.  _ALLBEGLINE
				   never appears in the parse tree; a _BEGLINE
				   is prepended with _CAT to the entire
				   regexp instead. */

  _ENDLINE,			/* _ENDLINE is a terminal symbol that matches
				   the empty string if it is at the end of
				   a line. */

  _ALLENDLINE,			/* _ALLENDLINE is to _ENDLINE as _ALLBEGLINE
				   is to _BEGLINE. */

  _BEGWORD,			/* _BEGWORD is a terminal symbol that matches
				   the empty string if it is at the beginning
				   of a word. */

  _ENDWORD,			/* _ENDWORD is a terminal symbol that matches
				   the empty string if it is at the end of
				   a word. */

  _LIMWORD,			/* _LIMWORD is a terminal symbol that matches
				   the empty string if it is at the beginning
				   or the end of a word. */

  _NOTLIMWORD,			/* _NOTLIMWORD is a terminal symbol that
				   matches the empty string if it is not at
				   the beginning or end of a word. */

  _QMARK,			/* _QMARK is an operator of one argument that
				   matches zero or one occurences of its
				   argument. */

  _STAR,			/* _STAR is an operator of one argument that
				   matches the Kleene closure (zero or more
				   occurrences) of its argument. */

  _PLUS,			/* _PLUS is an operator of one argument that
				   matches the positive closure (one or more
				   occurrences) of its argument. */

  _CAT,				/* _CAT is an operator of two arguments that
				   matches the concatenation of its
				   arguments.  _CAT is never returned by the
				   lexical analyzer. */

  _OR,				/* _OR is an operator of two arguments that
				   matches either of its arguments. */

  _LPAREN,			/* _LPAREN never appears in the parse tree,
				   it is only a lexeme. */

  _RPAREN,			/* _RPAREN never appears in the parse tree. */

  _SET				/* _SET and (and any value greater) is a
				   terminal symbol that matches any of a
				   class of characters. */
} _token;

#else /* ! __STDC__ */

typedef short _token;

#define _END -1
#define _EMPTY _NOTCHAR
#define _BACKREF (_EMPTY + 1)
#define _BEGLINE (_EMPTY + 2)
#define _ALLBEGLINE (_EMPTY + 3)
#define _ENDLINE (_EMPTY + 4)
#define _ALLENDLINE (_EMPTY + 5)
#define _BEGWORD (_EMPTY + 6)
#define _ENDWORD (_EMPTY + 7)
#define _LIMWORD (_EMPTY + 8)
#define _NOTLIMWORD (_EMPTY + 9)
#define _QMARK (_EMPTY + 10)
#define _STAR (_EMPTY + 11)
#define _PLUS (_EMPTY + 12)
#define _CAT (_EMPTY + 13)
#define _OR (_EMPTY + 14)
#define _LPAREN (_EMPTY + 15)
#define _RPAREN (_EMPTY + 16)
#define _SET (_EMPTY + 17)

#endif /* ! __STDC__ */

/* Sets are stored in an array in the compiled regexp; the index of the
   array corresponding to a given set token is given by _SET_INDEX(t). */
#define _SET_INDEX(t) ((t) - _SET)

/* Sometimes characters can only be matched depending on the surrounding
   context.  Such context decisions depend on what the previous character
   was, and the value of the current (lookahead) character.  Context
   dependent constraints are encoded as 8 bit integers.  Each bit that
   is set indicates that the constraint succeeds in the corresponding
   context.

   bit 7 - previous and current are newlines
   bit 6 - previous was newline, current isn't
   bit 5 - previous wasn't newline, current is
   bit 4 - neither previous nor current is a newline
   bit 3 - previous and current are word-constituents
   bit 2 - previous was word-constituent, current isn't
   bit 1 - previous wasn't word-constituent, current is
   bit 0 - neither previous nor current is word-constituent

   Word-constituent characters are those that satisfy isalnum().

   The macro _SUCCEEDS_IN_CONTEXT determines whether a a given constraint
   succeeds in a particular context.  Prevn is true if the previous character
   was a newline, currn is true if the lookahead character is a newline.
   Prevl and currl similarly depend upon whether the previous and current
   characters are word-constituent letters. */
#define _MATCHES_NEWLINE_CONTEXT(constraint, prevn, currn) \
  ((constraint) & 1 << ((prevn) ? 2 : 0) + ((currn) ? 1 : 0) + 4)
#define _MATCHES_LETTER_CONTEXT(constraint, prevl, currl) \
  ((constraint) & 1 << ((prevl) ? 2 : 0) + ((currl) ? 1 : 0))
#define _SUCCEEDS_IN_CONTEXT(constraint, prevn, currn, prevl, currl) \
  (_MATCHES_NEWLINE_CONTEXT(constraint, prevn, currn)		     \
   && _MATCHES_LETTER_CONTEXT(constraint, prevl, currl))

/* The following macros give information about what a constraint depends on. */
#define _PREV_NEWLINE_DEPENDENT(constraint) \
  (((constraint) & 0xc0) >> 2 != ((constraint) & 0x30))
#define _PREV_LETTER_DEPENDENT(constraint) \
  (((constraint) & 0x0c) >> 2 != ((constraint) & 0x03))

/* Tokens that match the empty string subject to some constraint actually
   work by applying that constraint to determine what may follow them,
   taking into account what has gone before.  The following values are
   the constraints corresponding to the special tokens previously defined. */
#define _NO_CONSTRAINT 0xff
#define _BEGLINE_CONSTRAINT 0xcf
#define _ENDLINE_CONSTRAINT 0xaf
#define _BEGWORD_CONSTRAINT 0xf2
#define _ENDWORD_CONSTRAINT 0xf4
#define _LIMWORD_CONSTRAINT 0xf6
#define _NOTLIMWORD_CONSTRAINT 0xf9

/* States of the recognizer correspond to sets of positions in the parse
   tree, together with the constraints under which they may be matched.
   So a position is encoded as an index into the parse tree together with
   a constraint. */
typedef struct
{
  unsigned index;		/* Index into the parse array. */
  unsigned constraint;		/* Constraint for matching this position. */
} _position;

/* Sets of positions are stored as arrays. */
typedef struct
{
  _position *elems;		/* Elements of this position set. */
  int nelem;			/* Number of elements in this set. */
} _position_set;

/* A state of the regexp consists of a set of positions, some flags,
   and the token value of the lowest-numbered position of the state that
   contains an _END token. */
typedef struct
{
  int hash;			/* Hash of the positions of this state. */
  _position_set elems;		/* Positions this state could match. */
  char newline;			/* True if previous state matched newline. */
  char letter;			/* True if previous state matched a letter. */
  char backref;			/* True if this state matches a \<digit>. */
  unsigned char constraint;	/* Constraint for this state to accept. */
  int first_end;		/* Token value of the first _END in elems. */
} _dfa_state;

/* If an r.e. is at most MUST_MAX characters long, we look for a string which
   must appear in it; whatever's found is dropped into the struct reg. */

#define MUST_MAX	50

/* A compiled regular expression. */
struct regexp
{
  /* Stuff built by the scanner. */
  _charset *charsets;		/* Array of character sets for _SET tokens. */
  int cindex;			/* Index for adding new charsets. */
  int calloc;			/* Number of charsets currently allocated. */

  /* Stuff built by the parser. */
  _token *tokens;		/* Postfix parse array. */
  int tindex;			/* Index for adding new tokens. */
  int talloc;			/* Number of tokens currently allocated. */
  int depth;			/* Depth required of an evaluation stack
				   used for depth-first traversal of the
				   parse tree. */
  int nleaves;			/* Number of leaves on the parse tree. */
  int nregexps;			/* Count of parallel regexps being built
				   with regparse(). */

  /* Stuff owned by the state builder. */
  _dfa_state *states;		/* States of the regexp. */
  int sindex;			/* Index for adding new states. */
  int salloc;			/* Number of states currently allocated. */

  /* Stuff built by the structure analyzer. */
  _position_set *follows;	/* Array of follow sets, indexed by position
				   index.  The follow of a position is the set
				   of positions containing characters that
				   could conceivably follow a character
				   matching the given position in a string
				   matching the regexp.  Allocated to the
				   maximum possible position index. */
  int searchflag;		/* True if we are supposed to build a searching
				   as opposed to an exact matcher.  A searching
				   matcher finds the first and shortest string
				   matching a regexp anywhere in the buffer,
				   whereas an exact matcher finds the longest
				   string matching, but anchored to the
				   beginning of the buffer. */

  /* Stuff owned by the executor. */
  int tralloc;			/* Number of transition tables that have
				   slots so far. */
  int trcount;			/* Number of transition tables that have
				   actually been built. */
  int **trans;			/* Transition tables for states that can
				   never accept.  If the transitions for a
				   state have not yet been computed, or the
				   state could possibly accept, its entry in
				   this table is NULL. */
  int **realtrans;		/* Trans always points to realtrans + 1; this
				   is so trans[-1] can contain NULL. */
  int **fails;			/* Transition tables after failing to accept
				   on a state that potentially could do so. */
  int *success;			/* Table of acceptance conditions used in
				   regexecute and computed in build_state. */
  int *newlines;		/* Transitions on newlines.  The entry for a
				   newline in any transition table is always
				   -1 so we can count lines without wasting
				   too many cycles.  The transition for a
				   newline is stored separately and handled
				   as a special case.  Newline is also used
				   as a sentinel at the end of the buffer. */
  char must[MUST_MAX];
  int mustn;
};

/* Some macros for user access to regexp internals. */

/* ACCEPTING returns true if s could possibly be an accepting state of r. */
#define ACCEPTING(s, r) ((r).states[s].constraint)

/* ACCEPTS_IN_CONTEXT returns true if the given state accepts in the
   specified context. */
#define ACCEPTS_IN_CONTEXT(prevn, currn, prevl, currl, state, reg) \
  _SUCCEEDS_IN_CONTEXT((reg).states[state].constraint,		   \
		       prevn, currn, prevl, currl)

/* FIRST_MATCHING_REGEXP returns the index number of the first of parallel
   regexps that a given state could accept.  Parallel regexps are numbered
   starting at 1. */
#define FIRST_MATCHING_REGEXP(state, reg) (-(reg).states[state].first_end)

/* Entry points. */

#ifdef __STDC__

/* Regsyntax() takes two arguments; the first sets the syntax bits described
   earlier in this file, and the second sets the case-folding flag. */
extern void regsyntax(int, int);

/* Compile the given string of the given length into the given struct regexp.
   Final argument is a flag specifying whether to build a searching or an
   exact matcher. */
extern void regcompile(const char *, size_t, struct regexp *, int);

/* Execute the given struct regexp on the buffer of characters.  The
   first char * points to the beginning, and the second points to the
   first character after the end of the buffer, which must be a writable
   place so a sentinel end-of-buffer marker can be stored there.  The
   second-to-last argument is a flag telling whether to allow newlines to
   be part of a string matching the regexp.  The next-to-last argument,
   if non-NULL, points to a place to increment every time we see a
   newline.  The final argument, if non-NULL, points to a flag that will
   be set if further examination by a backtracking matcher is needed in
   order to verify backreferencing; otherwise the flag will be cleared.
   Returns NULL if no match is found, or a pointer to the first
   character after the first & shortest matching string in the buffer. */
extern char *regexecute(struct regexp *, char *, char *, int, int *, int *);

/* Free the storage held by the components of a struct regexp. */
extern void regfree(struct regexp *);

/* Entry points for people who know what they're doing. */

/* Initialize the components of a struct regexp. */
extern void reginit(struct regexp *);

/* Incrementally parse a string of given length into a struct regexp. */
extern void regparse(const char *, size_t, struct regexp *);

/* Analyze a parsed regexp; second argument tells whether to build a searching
   or an exact matcher. */
extern void reganalyze(struct regexp *, int);

/* Compute, for each possible character, the transitions out of a given
   state, storing them in an array of integers. */
extern void regstate(int, struct regexp *, int []);

/* Error handling. */

/* Regerror() is called by the regexp routines whenever an error occurs.  It
   takes a single argument, a NUL-terminated string describing the error.
   The default regerror() prints the error message to stderr and exits.
   The user can provide a different regfree() if so desired. */
extern void regerror(const char *);

#else /* ! __STDC__ */
extern void regsyntax(), regcompile(), regfree(), reginit(), regparse();
extern void reganalyze(), regstate(), regerror();
extern char *regexecute();
#endif
