/* Declarations for interface to insn recognizer and insn-output.c.
   Copyright (C) 1987 Free Software Foundation, Inc.

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

/* Add prototype support.  */
#ifndef PROTO
#if defined (USE_PROTOTYPES) ? USE_PROTOTYPES : defined (__STDC__)
#define PROTO(ARGS) ARGS
#else
#define PROTO(ARGS) ()
#endif
#endif

/* Recognize an insn and return its insn-code,
   which is the sequence number of the DEFINE_INSN that it matches.
   If the insn does not match, return -1.  */

extern int recog_memoized PROTO((rtx));

/* Determine whether a proposed change to an insn or MEM will make it
   invalid.  Make the change if not.  */

extern int validate_change PROTO((rtx, rtx *, rtx, int));

/* Apply a group of changes if valid.  */

extern int apply_change_group PROTO((void));

/* Return the number of changes so far in the current group.   */

extern int num_validated_changes PROTO((void));

/* Retract some changes.  */

extern void cancel_changes PROTO((int));

/* Nonzero means volatile operands are recognized.  */

extern int volatile_ok;

/* Extract the operands from an insn that has been recognized.  */

extern void insn_extract PROTO((rtx));

/* The following vectors hold the results from insn_extract.  */

/* Indexed by N, gives value of operand N.  */
extern rtx recog_operand[];

/* Indexed by N, gives location where operand N was found.  */
extern rtx *recog_operand_loc[];

/* Indexed by N, gives location where the Nth duplicate-appearance of
   an operand was found.  This is something that matched MATCH_DUP.  */
extern rtx *recog_dup_loc[];

/* Indexed by N, gives the operand number that was duplicated in the
   Nth duplicate-appearance of an operand.  */
extern char recog_dup_num[];

#ifndef __STDC__
#ifndef const
#define const
#endif
#endif

/* Access the output function for CODE.  */

#define OUT_FCN(CODE) (*insn_outfun[(int) (CODE)])

/* Tables defined in insn-output.c that give information about
   each insn-code value.  */

/* These are vectors indexed by insn-code.  Details in genoutput.c.  */

extern char *const insn_template[];

extern char *(*const insn_outfun[]) ();

extern const int insn_n_operands[];

extern const int insn_n_dups[];

/* Indexed by insn code number, gives # of constraint alternatives.  */

extern const int insn_n_alternatives[];

/* These are two-dimensional arrays indexed first by the insn-code
   and second by the operand number.  Details in genoutput.c.  */

#ifdef REGISTER_CONSTRAINTS  /* Avoid undef sym in certain broken linkers.  */
extern char *const insn_operand_constraint[][MAX_RECOG_OPERANDS];
#endif

#ifndef REGISTER_CONSTRAINTS  /* Avoid undef sym in certain broken linkers.  */
extern const char insn_operand_address_p[][MAX_RECOG_OPERANDS];
#endif

extern const enum machine_mode insn_operand_mode[][MAX_RECOG_OPERANDS];

extern const char insn_operand_strict_low[][MAX_RECOG_OPERANDS];

extern int (*const insn_operand_predicate[][MAX_RECOG_OPERANDS]) ();

extern char * insn_name[];
