/* Subroutines used by or related to instruction recognition.
   Copyright (C) 1987, 1988, 1991, 1992, 1993 Free Software Foundation, Inc.

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
#include "rtl.h"
#include <stdio.h>
#include "insn-config.h"
#include "insn-attr.h"
#include "insn-flags.h"
#include "insn-codes.h"
#include "recog.h"
#include "regs.h"
#include "hard-reg-set.h"
#include "flags.h"
#include "real.h"

#ifndef STACK_PUSH_CODE
#ifdef STACK_GROWS_DOWNWARD
#define STACK_PUSH_CODE PRE_DEC
#else
#define STACK_PUSH_CODE PRE_INC
#endif
#endif

/* Import from final.c: */
extern rtx alter_subreg ();

int strict_memory_address_p ();
int memory_address_p ();

/* Nonzero means allow operands to be volatile.
   This should be 0 if you are generating rtl, such as if you are calling
   the functions in optabs.c and expmed.c (most of the time).
   This should be 1 if all valid insns need to be recognized,
   such as in regclass.c and final.c and reload.c.

   init_recog and init_recog_no_volatile are responsible for setting this.  */

int volatile_ok;

/* On return from `constrain_operands', indicate which alternative
   was satisfied.  */

int which_alternative;

/* Nonzero after end of reload pass.
   Set to 1 or 0 by toplev.c.
   Controls the significance of (SUBREG (MEM)).  */

int reload_completed;

/* Initialize data used by the function `recog'.
   This must be called once in the compilation of a function
   before any insn recognition may be done in the function.  */

void
init_recog_no_volatile ()
{
  volatile_ok = 0;
}

void
init_recog ()
{
  volatile_ok = 1;
}

/* Try recognizing the instruction INSN,
   and return the code number that results.
   Remeber the code so that repeated calls do not
   need to spend the time for actual rerecognition.

   This function is the normal interface to instruction recognition.
   The automatically-generated function `recog' is normally called
   through this one.  (The only exception is in combine.c.)  */

int
recog_memoized (insn)
     rtx insn;
{
  if (INSN_CODE (insn) < 0)
    INSN_CODE (insn) = recog (PATTERN (insn), insn, NULL_PTR);
  return INSN_CODE (insn);
}

/* Check that X is an insn-body for an `asm' with operands
   and that the operands mentioned in it are legitimate.  */

int
check_asm_operands (x)
     rtx x;
{
  int noperands = asm_noperands (x);
  rtx *operands;
  int i;

  if (noperands < 0)
    return 0;
  if (noperands == 0)
    return 1;

  operands = (rtx *) alloca (noperands * sizeof (rtx));
  decode_asm_operands (x, operands, NULL_PTR, NULL_PTR, NULL_PTR);

  for (i = 0; i < noperands; i++)
    if (!general_operand (operands[i], VOIDmode))
      return 0;

  return 1;
}

/* Static data for the next two routines.

   The maximum number of changes supported is defined as the maximum
   number of operands times 5.  This allows for repeated substitutions
   inside complex indexed address, or, alternatively, changes in up
   to 5 insns.  */

#define MAX_CHANGE_LOCS	(MAX_RECOG_OPERANDS * 5)

static rtx change_objects[MAX_CHANGE_LOCS];
static int change_old_codes[MAX_CHANGE_LOCS];
static rtx *change_locs[MAX_CHANGE_LOCS];
static rtx change_olds[MAX_CHANGE_LOCS];

static int num_changes = 0;

/* Validate a proposed change to OBJECT.  LOC is the location in the rtl for
   at which NEW will be placed.  If OBJECT is zero, no validation is done,
   the change is simply made.

   Two types of objects are supported:  If OBJECT is a MEM, memory_address_p
   will be called with the address and mode as parameters.  If OBJECT is
   an INSN, CALL_INSN, or JUMP_INSN, the insn will be re-recognized with
   the change in place.

   IN_GROUP is non-zero if this is part of a group of changes that must be
   performed as a group.  In that case, the changes will be stored.  The
   function `apply_change_group' will validate and apply the changes.

   If IN_GROUP is zero, this is a single change.  Try to recognize the insn
   or validate the memory reference with the change applied.  If the result
   is not valid for the machine, suppress the change and return zero.
   Otherwise, perform the change and return 1.  */

int
validate_change (object, loc, new, in_group)
    rtx object;
    rtx *loc;
    rtx new;
    int in_group;
{
  rtx old = *loc;

  if (old == new || rtx_equal_p (old, new))
    return 1;

  if (num_changes >= MAX_CHANGE_LOCS
      || (in_group == 0 && num_changes != 0))
    abort ();

  *loc = new;

  /* Save the information describing this change.  */
  change_objects[num_changes] = object;
  change_locs[num_changes] = loc;
  change_olds[num_changes] = old;

  if (object && GET_CODE (object) != MEM)
    {
      /* Set INSN_CODE to force rerecognition of insn.  Save old code in
	 case invalid.  */
      change_old_codes[num_changes] = INSN_CODE (object);
      INSN_CODE (object) = -1;
    }

  num_changes++;

  /* If we are making a group of changes, return 1.  Otherwise, validate the
     change group we made.  */

  if (in_group)
    return 1;
  else
    return apply_change_group ();
}

/* Apply a group of changes previously issued with `validate_change'.
   Return 1 if all changes are valid, zero otherwise.  */

int
apply_change_group ()
{
  int i;

  /* The changes have been applied and all INSN_CODEs have been reset to force
     rerecognition.

     The changes are valid if we aren't given an object, or if we are
     given a MEM and it still is a valid address, or if this is in insn
     and it is recognized.  In the latter case, if reload has completed,
     we also require that the operands meet the constraints for
     the insn.  We do not allow modifying an ASM_OPERANDS after reload
     has completed because verifying the constraints is too difficult.  */

  for (i = 0; i < num_changes; i++)
    {
      rtx object = change_objects[i];

      if (object == 0)
	continue;

      if (GET_CODE (object) == MEM)
	{
	  if (! memory_address_p (GET_MODE (object), XEXP (object, 0)))
	    break;
	}
      else if ((recog_memoized (object) < 0
		&& (asm_noperands (PATTERN (object)) < 0
		    || ! check_asm_operands (PATTERN (object))
		    || reload_completed))
	       || (reload_completed
		   && (insn_extract (object),
		       ! constrain_operands (INSN_CODE (object), 1))))
	{
	  rtx pat = PATTERN (object);

	  /* Perhaps we couldn't recognize the insn because there were
	     extra CLOBBERs at the end.  If so, try to re-recognize
	     without the last CLOBBER (later iterations will cause each of
	     them to be eliminated, in turn).  But don't do this if we
	     have an ASM_OPERAND.  */
	  if (GET_CODE (pat) == PARALLEL
	      && GET_CODE (XVECEXP (pat, 0, XVECLEN (pat, 0) - 1)) == CLOBBER
	      && asm_noperands (PATTERN (object)) < 0)
	    {
	       rtx newpat;

	       if (XVECLEN (pat, 0) == 2)
		 newpat = XVECEXP (pat, 0, 0);
	       else
		 {
		   int j;

		   newpat = gen_rtx (PARALLEL, VOIDmode, 
				     gen_rtvec (XVECLEN (pat, 0) - 1));
		   for (j = 0; j < XVECLEN (newpat, 0); j++)
		     XVECEXP (newpat, 0, j) = XVECEXP (pat, 0, j);
		 }

	       /* Add a new change to this group to replace the pattern
		  with this new pattern.  Then consider this change
		  as having succeeded.  The change we added will
		  cause the entire call to fail if things remain invalid.

		  Note that this can lose if a later change than the one
		  we are processing specified &XVECEXP (PATTERN (object), 0, X)
		  but this shouldn't occur.  */

	       validate_change (object, &PATTERN (object), newpat, 1);
	     }
	  else if (GET_CODE (pat) == USE || GET_CODE (pat) == CLOBBER)
	    /* If this insn is a CLOBBER or USE, it is always valid, but is
	       never recognized.  */
	    continue;
	  else
	    break;
	}
    }

  if (i == num_changes)
    {
      num_changes = 0;
      return 1;
    }
  else
    {
      cancel_changes (0);
      return 0;
    }
}

/* Return the number of changes so far in the current group.   */

int
num_validated_changes ()
{
  return num_changes;
}

/* Retract the changes numbered NUM and up.  */

void
cancel_changes (num)
     int num;
{
  int i;

  /* Back out all the changes.  Do this in the opposite order in which
     they were made.  */
  for (i = num_changes - 1; i >= num; i--)
    {
      *change_locs[i] = change_olds[i];
      if (change_objects[i] && GET_CODE (change_objects[i]) != MEM)
	INSN_CODE (change_objects[i]) = change_old_codes[i];
    }
  num_changes = num;
}

/* Replace every occurrence of FROM in X with TO.  Mark each change with
   validate_change passing OBJECT.  */

static void
validate_replace_rtx_1 (loc, from, to, object)
     rtx *loc;
     rtx from, to, object;
{
  register int i, j;
  register char *fmt;
  register rtx x = *loc;
  enum rtx_code code = GET_CODE (x);

  /* X matches FROM if it is the same rtx or they are both referring to the
     same register in the same mode.  Avoid calling rtx_equal_p unless the
     operands look similar.  */

  if (x == from
      || (GET_CODE (x) == REG && GET_CODE (from) == REG
	  && GET_MODE (x) == GET_MODE (from)
	  && REGNO (x) == REGNO (from))
      || (GET_CODE (x) == GET_CODE (from) && GET_MODE (x) == GET_MODE (from)
	  && rtx_equal_p (x, from)))
    {
      validate_change (object, loc, to, 1);
      return;
    }

  /* For commutative or comparison operations, try replacing each argument
     separately and seeing if we made any changes.  If so, put a constant
     argument last.*/
  if (GET_RTX_CLASS (code) == '<' || GET_RTX_CLASS (code) == 'c')
    {
      int prev_changes = num_changes;

      validate_replace_rtx_1 (&XEXP (x, 0), from, to, object);
      validate_replace_rtx_1 (&XEXP (x, 1), from, to, object);
      if (prev_changes != num_changes && CONSTANT_P (XEXP (x, 0)))
	{
	  validate_change (object, loc,
			   gen_rtx (GET_RTX_CLASS (code) == 'c' ? code
				    : swap_condition (code),
				    GET_MODE (x), XEXP (x, 1), XEXP (x, 0)),
			   1);
	  x = *loc;
	  code = GET_CODE (x);
	}
    }

  switch (code)
    {
    case PLUS:
      /* If we have have a PLUS whose second operand is now a CONST_INT, use
	 plus_constant to try to simplify it.  */
      if (GET_CODE (XEXP (x, 1)) == CONST_INT && XEXP (x, 1) == to)
	validate_change (object, loc, 
			 plus_constant (XEXP (x, 0), INTVAL (XEXP (x, 1))), 1);
      return;
      
    case ZERO_EXTEND:
    case SIGN_EXTEND:
      /* In these cases, the operation to be performed depends on the mode
	 of the operand.  If we are replacing the operand with a VOIDmode
	 constant, we lose the information.  So try to simplify the operation
	 in that case.  If it fails, substitute in something that we know
	 won't be recognized.  */
      if (GET_MODE (to) == VOIDmode
	  && (XEXP (x, 0) == from
	      || (GET_CODE (XEXP (x, 0)) == REG && GET_CODE (from) == REG
		  && GET_MODE (XEXP (x, 0)) == GET_MODE (from)
		  && REGNO (XEXP (x, 0)) == REGNO (from))))
	{
	  rtx new = simplify_unary_operation (code, GET_MODE (x), to,
					      GET_MODE (from));
	  if (new == 0)
	    new = gen_rtx (CLOBBER, GET_MODE (x), const0_rtx);

	  validate_change (object, loc, new, 1);
	  return;
	}
      break;
	
    case SUBREG:
      /* If we have a SUBREG of a register that we are replacing and we are
	 replacing it with a MEM, make a new MEM and try replacing the
	 SUBREG with it.  Don't do this if the MEM has a mode-dependent address
	 or if we would be widening it.  */

      if (SUBREG_REG (x) == from
	  && GET_CODE (from) == REG
	  && GET_CODE (to) == MEM
	  && ! mode_dependent_address_p (XEXP (to, 0))
	  && ! MEM_VOLATILE_P (to)
	  && GET_MODE_SIZE (GET_MODE (x)) <= GET_MODE_SIZE (GET_MODE (to)))
	{
	  int offset = SUBREG_WORD (x) * UNITS_PER_WORD;
	  enum machine_mode mode = GET_MODE (x);
	  rtx new;

#if BYTES_BIG_ENDIAN
	  offset += (MIN (UNITS_PER_WORD,
			  GET_MODE_SIZE (GET_MODE (SUBREG_REG (x))))
		     - MIN (UNITS_PER_WORD, GET_MODE_SIZE (mode)));
#endif

	  new = gen_rtx (MEM, mode, plus_constant (XEXP (to, 0), offset));
	  MEM_VOLATILE_P (new) = MEM_VOLATILE_P (to);
	  RTX_UNCHANGING_P (new) = RTX_UNCHANGING_P (to);
	  MEM_IN_STRUCT_P (new) = MEM_IN_STRUCT_P (to);
	  validate_change (object, loc, new, 1);
	  return;
	}
      break;

    case ZERO_EXTRACT:
    case SIGN_EXTRACT:
      /* If we are replacing a register with memory, try to change the memory
	 to be the mode required for memory in extract operations (this isn't
	 likely to be an insertion operation; if it was, nothing bad will
	 happen, we might just fail in some cases).  */

      if (XEXP (x, 0) == from && GET_CODE (from) == REG && GET_CODE (to) == MEM
	  && GET_CODE (XEXP (x, 1)) == CONST_INT
	  && GET_CODE (XEXP (x, 2)) == CONST_INT
	  && ! mode_dependent_address_p (XEXP (to, 0))
	  && ! MEM_VOLATILE_P (to))
	{
	  enum machine_mode wanted_mode = VOIDmode;
	  enum machine_mode is_mode = GET_MODE (to);
	  int width = INTVAL (XEXP (x, 1));
	  int pos = INTVAL (XEXP (x, 2));

#ifdef HAVE_extzv
	  if (code == ZERO_EXTRACT)
	    wanted_mode = insn_operand_mode[(int) CODE_FOR_extzv][1];
#endif
#ifdef HAVE_extv
	  if (code == SIGN_EXTRACT)
	    wanted_mode = insn_operand_mode[(int) CODE_FOR_extv][1];
#endif

	  /* If we have a narrower mode, we can do something.  */
	  if (wanted_mode != VOIDmode
	      && GET_MODE_SIZE (wanted_mode) < GET_MODE_SIZE (is_mode))
	    {
	      int offset = pos / BITS_PER_UNIT;
	      rtx newmem;

		  /* If the bytes and bits are counted differently, we
		     must adjust the offset.  */
#if BYTES_BIG_ENDIAN != BITS_BIG_ENDIAN
	      offset = (GET_MODE_SIZE (is_mode) - GET_MODE_SIZE (wanted_mode)
			- offset);
#endif

	      pos %= GET_MODE_BITSIZE (wanted_mode);

	      newmem = gen_rtx (MEM, wanted_mode,
				plus_constant (XEXP (to, 0), offset));
	      RTX_UNCHANGING_P (newmem) = RTX_UNCHANGING_P (to);
	      MEM_VOLATILE_P (newmem) = MEM_VOLATILE_P (to);
	      MEM_IN_STRUCT_P (newmem) = MEM_IN_STRUCT_P (to);

	      validate_change (object, &XEXP (x, 2), GEN_INT (pos), 1);
	      validate_change (object, &XEXP (x, 0), newmem, 1);
	    }
	}

      break;
    }
      
  fmt = GET_RTX_FORMAT (code);
  for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--)
    {
      if (fmt[i] == 'e')
	validate_replace_rtx_1 (&XEXP (x, i), from, to, object);
      else if (fmt[i] == 'E')
	for (j = XVECLEN (x, i) - 1; j >= 0; j--)
	  validate_replace_rtx_1 (&XVECEXP (x, i, j), from, to, object);
    }
}

/* Try replacing every occurrence of FROM in INSN with TO.  After all
   changes have been made, validate by seeing if INSN is still valid.  */

int
validate_replace_rtx (from, to, insn)
     rtx from, to, insn;
{
  validate_replace_rtx_1 (&PATTERN (insn), from, to, insn);
  return apply_change_group ();
}

#ifdef HAVE_cc0
/* Return 1 if the insn using CC0 set by INSN does not contain
   any ordered tests applied to the condition codes.
   EQ and NE tests do not count.  */

int
next_insn_tests_no_inequality (insn)
     rtx insn;
{
  register rtx next = next_cc0_user (insn);

  /* If there is no next insn, we have to take the conservative choice.  */
  if (next == 0)
    return 0;

  return ((GET_CODE (next) == JUMP_INSN
	   || GET_CODE (next) == INSN
	   || GET_CODE (next) == CALL_INSN)
	  && ! inequality_comparisons_p (PATTERN (next)));
}

#if 0  /* This is useless since the insn that sets the cc's
	  must be followed immediately by the use of them.  */
/* Return 1 if the CC value set up by INSN is not used.  */

int
next_insns_test_no_inequality (insn)
     rtx insn;
{
  register rtx next = NEXT_INSN (insn);

  for (; next != 0; next = NEXT_INSN (next))
    {
      if (GET_CODE (next) == CODE_LABEL
	  || GET_CODE (next) == BARRIER)
	return 1;
      if (GET_CODE (next) == NOTE)
	continue;
      if (inequality_comparisons_p (PATTERN (next)))
	return 0;
      if (sets_cc0_p (PATTERN (next)) == 1)
	return 1;
      if (! reg_mentioned_p (cc0_rtx, PATTERN (next)))
	return 1;
    }
  return 1;
}
#endif
#endif

/* This is used by find_single_use to locate an rtx that contains exactly one
   use of DEST, which is typically either a REG or CC0.  It returns a
   pointer to the innermost rtx expression containing DEST.  Appearances of
   DEST that are being used to totally replace it are not counted.  */

static rtx *
find_single_use_1 (dest, loc)
     rtx dest;
     rtx *loc;
{
  rtx x = *loc;
  enum rtx_code code = GET_CODE (x);
  rtx *result = 0;
  rtx *this_result;
  int i;
  char *fmt;

  switch (code)
    {
    case CONST_INT:
    case CONST:
    case LABEL_REF:
    case SYMBOL_REF:
    case CONST_DOUBLE:
    case CLOBBER:
      return 0;

    case SET:
      /* If the destination is anything other than CC0, PC, a REG or a SUBREG
	 of a REG that occupies all of the REG, the insn uses DEST if
	 it is mentioned in the destination or the source.  Otherwise, we
	 need just check the source.  */
      if (GET_CODE (SET_DEST (x)) != CC0
	  && GET_CODE (SET_DEST (x)) != PC
	  && GET_CODE (SET_DEST (x)) != REG
	  && ! (GET_CODE (SET_DEST (x)) == SUBREG
		&& GET_CODE (SUBREG_REG (SET_DEST (x))) == REG
		&& (((GET_MODE_SIZE (GET_MODE (SUBREG_REG (SET_DEST (x))))
		      + (UNITS_PER_WORD - 1)) / UNITS_PER_WORD)
		    == ((GET_MODE_SIZE (GET_MODE (SET_DEST (x)))
			 + (UNITS_PER_WORD - 1)) / UNITS_PER_WORD))))
	break;

      return find_single_use_1 (dest, &SET_SRC (x));

    case MEM:
    case SUBREG:
      return find_single_use_1 (dest, &XEXP (x, 0));
    }

  /* If it wasn't one of the common cases above, check each expression and
     vector of this code.  Look for a unique usage of DEST.  */

  fmt = GET_RTX_FORMAT (code);
  for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--)
    {
      if (fmt[i] == 'e')
	{
	  if (dest == XEXP (x, i)
	      || (GET_CODE (dest) == REG && GET_CODE (XEXP (x, i)) == REG
		  && REGNO (dest) == REGNO (XEXP (x, i))))
	    this_result = loc;
	  else
	    this_result = find_single_use_1 (dest, &XEXP (x, i));

	  if (result == 0)
	    result = this_result;
	  else if (this_result)
	    /* Duplicate usage.  */
	    return 0;
	}
      else if (fmt[i] == 'E')
	{
	  int j;

	  for (j = XVECLEN (x, i) - 1; j >= 0; j--)
	    {
	      if (XVECEXP (x, i, j) == dest
		  || (GET_CODE (dest) == REG
		      && GET_CODE (XVECEXP (x, i, j)) == REG
		      && REGNO (XVECEXP (x, i, j)) == REGNO (dest)))
		this_result = loc;
	      else
		this_result = find_single_use_1 (dest, &XVECEXP (x, i, j));

	      if (result == 0)
		result = this_result;
	      else if (this_result)
		return 0;
	    }
	}
    }

  return result;
}

/* See if DEST, produced in INSN, is used only a single time in the
   sequel.  If so, return a pointer to the innermost rtx expression in which
   it is used.

   If PLOC is non-zero, *PLOC is set to the insn containing the single use.

   This routine will return usually zero either before flow is called (because
   there will be no LOG_LINKS notes) or after reload (because the REG_DEAD
   note can't be trusted).

   If DEST is cc0_rtx, we look only at the next insn.  In that case, we don't
   care about REG_DEAD notes or LOG_LINKS.

   Otherwise, we find the single use by finding an insn that has a
   LOG_LINKS pointing at INSN and has a REG_DEAD note for DEST.  If DEST is
   only referenced once in that insn, we know that it must be the first
   and last insn referencing DEST.  */

rtx *
find_single_use (dest, insn, ploc)
     rtx dest;
     rtx insn;
     rtx *ploc;
{
  rtx next;
  rtx *result;
  rtx link;

#ifdef HAVE_cc0
  if (dest == cc0_rtx)
    {
      next = NEXT_INSN (insn);
      if (next == 0
	  || (GET_CODE (next) != INSN && GET_CODE (next) != JUMP_INSN))
	return 0;

      result = find_single_use_1 (dest, &PATTERN (next));
      if (result && ploc)
	*ploc = next;
      return result;
    }
#endif

  if (reload_completed || reload_in_progress || GET_CODE (dest) != REG)
    return 0;

  for (next = next_nonnote_insn (insn);
       next != 0 && GET_CODE (next) != CODE_LABEL;
       next = next_nonnote_insn (next))
    if (GET_RTX_CLASS (GET_CODE (next)) == 'i' && dead_or_set_p (next, dest))
      {
	for (link = LOG_LINKS (next); link; link = XEXP (link, 1))
	  if (XEXP (link, 0) == insn)
	    break;

	if (link)
	  {
	    result = find_single_use_1 (dest, &PATTERN (next));
	    if (ploc)
	      *ploc = next;
	    return result;
	  }
      }

  return 0;
}

/* Return 1 if OP is a valid general operand for machine mode MODE.
   This is either a register reference, a memory reference,
   or a constant.  In the case of a memory reference, the address
   is checked for general validity for the target machine.

   Register and memory references must have mode MODE in order to be valid,
   but some constants have no machine mode and are valid for any mode.

   If MODE is VOIDmode, OP is checked for validity for whatever mode
   it has.

   The main use of this function is as a predicate in match_operand
   expressions in the machine description.

   For an explanation of this function's behavior for registers of
   class NO_REGS, see the comment for `register_operand'.  */

int
general_operand (op, mode)
     register rtx op;
     enum machine_mode mode;
{
  register enum rtx_code code = GET_CODE (op);
  int mode_altering_drug = 0;

  if (mode == VOIDmode)
    mode = GET_MODE (op);

  /* Don't accept CONST_INT or anything similar
     if the caller wants something floating.  */
  if (GET_MODE (op) == VOIDmode && mode != VOIDmode
      && GET_MODE_CLASS (mode) != MODE_INT
      && GET_MODE_CLASS (mode) != MODE_PARTIAL_INT)
    return 0;

  if (CONSTANT_P (op))
    return ((GET_MODE (op) == VOIDmode || GET_MODE (op) == mode)
#ifdef LEGITIMATE_PIC_OPERAND_P
	    && (! flag_pic || LEGITIMATE_PIC_OPERAND_P (op))
#endif
	    && LEGITIMATE_CONSTANT_P (op));

  /* Except for certain constants with VOIDmode, already checked for,
     OP's mode must match MODE if MODE specifies a mode.  */

  if (GET_MODE (op) != mode)
    return 0;

  if (code == SUBREG)
    {
#ifdef INSN_SCHEDULING
      /* On machines that have insn scheduling, we want all memory
	 reference to be explicit, so outlaw paradoxical SUBREGs.  */
      if (GET_CODE (SUBREG_REG (op)) == MEM
	  && GET_MODE_SIZE (mode) > GET_MODE_SIZE (GET_MODE (SUBREG_REG (op))))
	return 0;
#endif

      op = SUBREG_REG (op);
      code = GET_CODE (op);
#if 0
      /* No longer needed, since (SUBREG (MEM...))
	 will load the MEM into a reload reg in the MEM's own mode.  */
      mode_altering_drug = 1;
#endif
    }

  if (code == REG)
    /* A register whose class is NO_REGS is not a general operand.  */
    return (REGNO (op) >= FIRST_PSEUDO_REGISTER
	    || REGNO_REG_CLASS (REGNO (op)) != NO_REGS);

  if (code == MEM)
    {
      register rtx y = XEXP (op, 0);
      if (! volatile_ok && MEM_VOLATILE_P (op))
	return 0;
      /* Use the mem's mode, since it will be reloaded thus.  */
      mode = GET_MODE (op);
      GO_IF_LEGITIMATE_ADDRESS (mode, y, win);
    }
  return 0;

 win:
  if (mode_altering_drug)
    return ! mode_dependent_address_p (XEXP (op, 0));
  return 1;
}

/* Return 1 if OP is a valid memory address for a memory reference
   of mode MODE.

   The main use of this function is as a predicate in match_operand
   expressions in the machine description.  */

int
address_operand (op, mode)
     register rtx op;
     enum machine_mode mode;
{
  return memory_address_p (mode, op);
}

/* Return 1 if OP is a register reference of mode MODE.
   If MODE is VOIDmode, accept a register in any mode.

   The main use of this function is as a predicate in match_operand
   expressions in the machine description.

   As a special exception, registers whose class is NO_REGS are
   not accepted by `register_operand'.  The reason for this change
   is to allow the representation of special architecture artifacts
   (such as a condition code register) without extending the rtl
   definitions.  Since registers of class NO_REGS cannot be used
   as registers in any case where register classes are examined,
   it is most consistent to keep this function from accepting them.  */

int
register_operand (op, mode)
     register rtx op;
     enum machine_mode mode;
{
  if (GET_MODE (op) != mode && mode != VOIDmode)
    return 0;

  if (GET_CODE (op) == SUBREG)
    {
      /* Before reload, we can allow (SUBREG (MEM...)) as a register operand
	 because it is guaranteed to be reloaded into one.
	 Just make sure the MEM is valid in itself.
	 (Ideally, (SUBREG (MEM)...) should not exist after reload,
	 but currently it does result from (SUBREG (REG)...) where the
	 reg went on the stack.)  */
      if (! reload_completed && GET_CODE (SUBREG_REG (op)) == MEM)
	return general_operand (op, mode);
      op = SUBREG_REG (op);
    }

  /* We don't consider registers whose class is NO_REGS
     to be a register operand.  */
  return (GET_CODE (op) == REG
	  && (REGNO (op) >= FIRST_PSEUDO_REGISTER
	      || REGNO_REG_CLASS (REGNO (op)) != NO_REGS));
}

/* Return 1 if OP should match a MATCH_SCRATCH, i.e., if it is a SCRATCH
   or a hard register.  */

int
scratch_operand (op, mode)
     register rtx op;
     enum machine_mode mode;
{
  return (GET_MODE (op) == mode
	  && (GET_CODE (op) == SCRATCH
	      || (GET_CODE (op) == REG
		  && REGNO (op) < FIRST_PSEUDO_REGISTER)));
}

/* Return 1 if OP is a valid immediate operand for mode MODE.

   The main use of this function is as a predicate in match_operand
   expressions in the machine description.  */

int
immediate_operand (op, mode)
     register rtx op;
     enum machine_mode mode;
{
  /* Don't accept CONST_INT or anything similar
     if the caller wants something floating.  */
  if (GET_MODE (op) == VOIDmode && mode != VOIDmode
      && GET_MODE_CLASS (mode) != MODE_INT
      && GET_MODE_CLASS (mode) != MODE_PARTIAL_INT)
    return 0;

  return (CONSTANT_P (op)
	  && (GET_MODE (op) == mode || mode == VOIDmode
	      || GET_MODE (op) == VOIDmode)
#ifdef LEGITIMATE_PIC_OPERAND_P
	  && (! flag_pic || LEGITIMATE_PIC_OPERAND_P (op))
#endif
	  && LEGITIMATE_CONSTANT_P (op));
}

/* Returns 1 if OP is an operand that is a CONST_INT.  */

int
const_int_operand (op, mode)
     register rtx op;
     enum machine_mode mode;
{
  return GET_CODE (op) == CONST_INT;
}

/* Returns 1 if OP is an operand that is a constant integer or constant
   floating-point number.  */

int
const_double_operand (op, mode)
     register rtx op;
     enum machine_mode mode;
{
  /* Don't accept CONST_INT or anything similar
     if the caller wants something floating.  */
  if (GET_MODE (op) == VOIDmode && mode != VOIDmode
      && GET_MODE_CLASS (mode) != MODE_INT
      && GET_MODE_CLASS (mode) != MODE_PARTIAL_INT)
    return 0;

  return ((GET_CODE (op) == CONST_DOUBLE || GET_CODE (op) == CONST_INT)
	  && (mode == VOIDmode || GET_MODE (op) == mode
	      || GET_MODE (op) == VOIDmode));
}

/* Return 1 if OP is a general operand that is not an immediate operand.  */

int
nonimmediate_operand (op, mode)
     register rtx op;
     enum machine_mode mode;
{
  return (general_operand (op, mode) && ! CONSTANT_P (op));
}

/* Return 1 if OP is a register reference or immediate value of mode MODE.  */

int
nonmemory_operand (op, mode)
     register rtx op;
     enum machine_mode mode;
{
  if (CONSTANT_P (op))
    {
      /* Don't accept CONST_INT or anything similar
	 if the caller wants something floating.  */
      if (GET_MODE (op) == VOIDmode && mode != VOIDmode
	  && GET_MODE_CLASS (mode) != MODE_INT
	  && GET_MODE_CLASS (mode) != MODE_PARTIAL_INT)
	return 0;

      return ((GET_MODE (op) == VOIDmode || GET_MODE (op) == mode)
#ifdef LEGITIMATE_PIC_OPERAND_P
	      && (! flag_pic || LEGITIMATE_PIC_OPERAND_P (op))
#endif
	      && LEGITIMATE_CONSTANT_P (op));
    }

  if (GET_MODE (op) != mode && mode != VOIDmode)
    return 0;

  if (GET_CODE (op) == SUBREG)
    {
      /* Before reload, we can allow (SUBREG (MEM...)) as a register operand
	 because it is guaranteed to be reloaded into one.
	 Just make sure the MEM is valid in itself.
	 (Ideally, (SUBREG (MEM)...) should not exist after reload,
	 but currently it does result from (SUBREG (REG)...) where the
	 reg went on the stack.)  */
      if (! reload_completed && GET_CODE (SUBREG_REG (op)) == MEM)
	return general_operand (op, mode);
      op = SUBREG_REG (op);
    }

  /* We don't consider registers whose class is NO_REGS
     to be a register operand.  */
  return (GET_CODE (op) == REG
	  && (REGNO (op) >= FIRST_PSEUDO_REGISTER
	      || REGNO_REG_CLASS (REGNO (op)) != NO_REGS));
}

/* Return 1 if OP is a valid operand that stands for pushing a
   value of mode MODE onto the stack.

   The main use of this function is as a predicate in match_operand
   expressions in the machine description.  */

int
push_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  if (GET_CODE (op) != MEM)
    return 0;

  if (GET_MODE (op) != mode)
    return 0;

  op = XEXP (op, 0);

  if (GET_CODE (op) != STACK_PUSH_CODE)
    return 0;

  return XEXP (op, 0) == stack_pointer_rtx;
}

/* Return 1 if ADDR is a valid memory address for mode MODE.  */

int
memory_address_p (mode, addr)
     enum machine_mode mode;
     register rtx addr;
{
  GO_IF_LEGITIMATE_ADDRESS (mode, addr, win);
  return 0;

 win:
  return 1;
}

/* Return 1 if OP is a valid memory reference with mode MODE,
   including a valid address.

   The main use of this function is as a predicate in match_operand
   expressions in the machine description.  */

int
memory_operand (op, mode)
     register rtx op;
     enum machine_mode mode;
{
  rtx inner;

  if (! reload_completed)
    /* Note that no SUBREG is a memory operand before end of reload pass,
       because (SUBREG (MEM...)) forces reloading into a register.  */
    return GET_CODE (op) == MEM && general_operand (op, mode);

  if (mode != VOIDmode && GET_MODE (op) != mode)
    return 0;

  inner = op;
  if (GET_CODE (inner) == SUBREG)
    inner = SUBREG_REG (inner);

  return (GET_CODE (inner) == MEM && general_operand (op, mode));
}

/* Return 1 if OP is a valid indirect memory reference with mode MODE;
   that is, a memory reference whose address is a general_operand.  */

int
indirect_operand (op, mode)
     register rtx op;
     enum machine_mode mode;
{
  /* Before reload, a SUBREG isn't in memory (see memory_operand, above).  */
  if (! reload_completed
      && GET_CODE (op) == SUBREG && GET_CODE (SUBREG_REG (op)) == MEM)
    {
      register int offset = SUBREG_WORD (op) * UNITS_PER_WORD;
      rtx inner = SUBREG_REG (op);

#if BYTES_BIG_ENDIAN
      offset -= (MIN (UNITS_PER_WORD, GET_MODE_SIZE (GET_MODE (op)))
		 - MIN (UNITS_PER_WORD, GET_MODE_SIZE (GET_MODE (inner))));
#endif

      /* The only way that we can have a general_operand as the resulting
	 address is if OFFSET is zero and the address already is an operand
	 or if the address is (plus Y (const_int -OFFSET)) and Y is an
	 operand.  */

      return ((offset == 0 && general_operand (XEXP (inner, 0), Pmode))
	      || (GET_CODE (XEXP (inner, 0)) == PLUS
		  && GET_CODE (XEXP (XEXP (inner, 0), 1)) == CONST_INT
		  && INTVAL (XEXP (XEXP (inner, 0), 1)) == -offset
		  && general_operand (XEXP (XEXP (inner, 0), 0), Pmode)));
    }

  return (GET_CODE (op) == MEM
	  && memory_operand (op, mode)
	  && general_operand (XEXP (op, 0), Pmode));
}

/* Return 1 if this is a comparison operator.  This allows the use of
   MATCH_OPERATOR to recognize all the branch insns.  */

int
comparison_operator (op, mode)
    register rtx op;
    enum machine_mode mode;
{
  return ((mode == VOIDmode || GET_MODE (op) == mode)
	  && GET_RTX_CLASS (GET_CODE (op)) == '<');
}

/* If BODY is an insn body that uses ASM_OPERANDS,
   return the number of operands (both input and output) in the insn.
   Otherwise return -1.  */

int
asm_noperands (body)
     rtx body;
{
  if (GET_CODE (body) == ASM_OPERANDS)
    /* No output operands: return number of input operands.  */
    return ASM_OPERANDS_INPUT_LENGTH (body);
  if (GET_CODE (body) == SET && GET_CODE (SET_SRC (body)) == ASM_OPERANDS)
    /* Single output operand: BODY is (set OUTPUT (asm_operands ...)).  */
    return ASM_OPERANDS_INPUT_LENGTH (SET_SRC (body)) + 1;
  else if (GET_CODE (body) == PARALLEL
	   && GET_CODE (XVECEXP (body, 0, 0)) == SET
	   && GET_CODE (SET_SRC (XVECEXP (body, 0, 0))) == ASM_OPERANDS)
    {
      /* Multiple output operands, or 1 output plus some clobbers:
	 body is [(set OUTPUT (asm_operands ...))... (clobber (reg ...))...].  */
      int i;
      int n_sets;

      /* Count backwards through CLOBBERs to determine number of SETs.  */
      for (i = XVECLEN (body, 0); i > 0; i--)
	{
	  if (GET_CODE (XVECEXP (body, 0, i - 1)) == SET)
	    break;
	  if (GET_CODE (XVECEXP (body, 0, i - 1)) != CLOBBER)
	    return -1;
	}

      /* N_SETS is now number of output operands.  */
      n_sets = i;

      /* Verify that all the SETs we have
	 came from a single original asm_operands insn
	 (so that invalid combinations are blocked).  */
      for (i = 0; i < n_sets; i++)
	{
	  rtx elt = XVECEXP (body, 0, i);
	  if (GET_CODE (elt) != SET)
	    return -1;
	  if (GET_CODE (SET_SRC (elt)) != ASM_OPERANDS)
	    return -1;
	  /* If these ASM_OPERANDS rtx's came from different original insns
	     then they aren't allowed together.  */
	  if (ASM_OPERANDS_INPUT_VEC (SET_SRC (elt))
	      != ASM_OPERANDS_INPUT_VEC (SET_SRC (XVECEXP (body, 0, 0))))
	    return -1;
	}
      return (ASM_OPERANDS_INPUT_LENGTH (SET_SRC (XVECEXP (body, 0, 0)))
	      + n_sets);
    }
  else if (GET_CODE (body) == PARALLEL
	   && GET_CODE (XVECEXP (body, 0, 0)) == ASM_OPERANDS)
    {
      /* 0 outputs, but some clobbers:
	 body is [(asm_operands ...) (clobber (reg ...))...].  */
      int i;

      /* Make sure all the other parallel things really are clobbers.  */
      for (i = XVECLEN (body, 0) - 1; i > 0; i--)
	if (GET_CODE (XVECEXP (body, 0, i)) != CLOBBER)
	  return -1;

      return ASM_OPERANDS_INPUT_LENGTH (XVECEXP (body, 0, 0));
    }
  else
    return -1;
}

/* Assuming BODY is an insn body that uses ASM_OPERANDS,
   copy its operands (both input and output) into the vector OPERANDS,
   the locations of the operands within the insn into the vector OPERAND_LOCS,
   and the constraints for the operands into CONSTRAINTS.
   Write the modes of the operands into MODES.
   Return the assembler-template.

   If MODES, OPERAND_LOCS, CONSTRAINTS or OPERANDS is 0,
   we don't store that info.  */

char *
decode_asm_operands (body, operands, operand_locs, constraints, modes)
     rtx body;
     rtx *operands;
     rtx **operand_locs;
     char **constraints;
     enum machine_mode *modes;
{
  register int i;
  int noperands;
  char *template = 0;

  if (GET_CODE (body) == SET && GET_CODE (SET_SRC (body)) == ASM_OPERANDS)
    {
      rtx asmop = SET_SRC (body);
      /* Single output operand: BODY is (set OUTPUT (asm_operands ....)).  */

      noperands = ASM_OPERANDS_INPUT_LENGTH (asmop) + 1;

      for (i = 1; i < noperands; i++)
	{
	  if (operand_locs)
	    operand_locs[i] = &ASM_OPERANDS_INPUT (asmop, i - 1);
	  if (operands)
	    operands[i] = ASM_OPERANDS_INPUT (asmop, i - 1);
	  if (constraints)
	    constraints[i] = ASM_OPERANDS_INPUT_CONSTRAINT (asmop, i - 1);
	  if (modes)
	    modes[i] = ASM_OPERANDS_INPUT_MODE (asmop, i - 1);
	}

      /* The output is in the SET.
	 Its constraint is in the ASM_OPERANDS itself.  */
      if (operands)
	operands[0] = SET_DEST (body);
      if (operand_locs)
	operand_locs[0] = &SET_DEST (body);
      if (constraints)
	constraints[0] = ASM_OPERANDS_OUTPUT_CONSTRAINT (asmop);
      if (modes)
	modes[0] = GET_MODE (SET_DEST (body));
      template = ASM_OPERANDS_TEMPLATE (asmop);
    }
  else if (GET_CODE (body) == ASM_OPERANDS)
    {
      rtx asmop = body;
      /* No output operands: BODY is (asm_operands ....).  */

      noperands = ASM_OPERANDS_INPUT_LENGTH (asmop);

      /* The input operands are found in the 1st element vector.  */
      /* Constraints for inputs are in the 2nd element vector.  */
      for (i = 0; i < noperands; i++)
	{
	  if (operand_locs)
	    operand_locs[i] = &ASM_OPERANDS_INPUT (asmop, i);
	  if (operands)
	    operands[i] = ASM_OPERANDS_INPUT (asmop, i);
	  if (constraints)
	    constraints[i] = ASM_OPERANDS_INPUT_CONSTRAINT (asmop, i);
	  if (modes)
	    modes[i] = ASM_OPERANDS_INPUT_MODE (asmop, i);
	}
      template = ASM_OPERANDS_TEMPLATE (asmop);
    }
  else if (GET_CODE (body) == PARALLEL
	   && GET_CODE (XVECEXP (body, 0, 0)) == SET)
    {
      rtx asmop = SET_SRC (XVECEXP (body, 0, 0));
      int nparallel = XVECLEN (body, 0); /* Includes CLOBBERs.  */
      int nin = ASM_OPERANDS_INPUT_LENGTH (asmop);
      int nout = 0;		/* Does not include CLOBBERs.  */

      /* At least one output, plus some CLOBBERs.  */

      /* The outputs are in the SETs.
	 Their constraints are in the ASM_OPERANDS itself.  */
      for (i = 0; i < nparallel; i++)
	{
	  if (GET_CODE (XVECEXP (body, 0, i)) == CLOBBER)
	    break;		/* Past last SET */
	  
	  if (operands)
	    operands[i] = SET_DEST (XVECEXP (body, 0, i));
	  if (operand_locs)
	    operand_locs[i] = &SET_DEST (XVECEXP (body, 0, i));
	  if (constraints)
	    constraints[i] = XSTR (SET_SRC (XVECEXP (body, 0, i)), 1);
	  if (modes)
	    modes[i] = GET_MODE (SET_DEST (XVECEXP (body, 0, i)));
	  nout++;
	}

      for (i = 0; i < nin; i++)
	{
	  if (operand_locs)
	    operand_locs[i + nout] = &ASM_OPERANDS_INPUT (asmop, i);
	  if (operands)
	    operands[i + nout] = ASM_OPERANDS_INPUT (asmop, i);
	  if (constraints)
	    constraints[i + nout] = ASM_OPERANDS_INPUT_CONSTRAINT (asmop, i);
	  if (modes)
	    modes[i + nout] = ASM_OPERANDS_INPUT_MODE (asmop, i);
	}

      template = ASM_OPERANDS_TEMPLATE (asmop);
    }
  else if (GET_CODE (body) == PARALLEL
	   && GET_CODE (XVECEXP (body, 0, 0)) == ASM_OPERANDS)
    {
      /* No outputs, but some CLOBBERs.  */

      rtx asmop = XVECEXP (body, 0, 0);
      int nin = ASM_OPERANDS_INPUT_LENGTH (asmop);

      for (i = 0; i < nin; i++)
	{
	  if (operand_locs)
	    operand_locs[i] = &ASM_OPERANDS_INPUT (asmop, i);
	  if (operands)
	    operands[i] = ASM_OPERANDS_INPUT (asmop, i);
	  if (constraints)
	    constraints[i] = ASM_OPERANDS_INPUT_CONSTRAINT (asmop, i);
	  if (modes)
	    modes[i] = ASM_OPERANDS_INPUT_MODE (asmop, i);
	}

      template = ASM_OPERANDS_TEMPLATE (asmop);
    }

  return template;
}

/* Given an rtx *P, if it is a sum containing an integer constant term,
   return the location (type rtx *) of the pointer to that constant term.
   Otherwise, return a null pointer.  */

static rtx *
find_constant_term_loc (p)
     rtx *p;
{
  register rtx *tem;
  register enum rtx_code code = GET_CODE (*p);

  /* If *P IS such a constant term, P is its location.  */

  if (code == CONST_INT || code == SYMBOL_REF || code == LABEL_REF
      || code == CONST)
    return p;

  /* Otherwise, if not a sum, it has no constant term.  */

  if (GET_CODE (*p) != PLUS)
    return 0;

  /* If one of the summands is constant, return its location.  */

  if (XEXP (*p, 0) && CONSTANT_P (XEXP (*p, 0))
      && XEXP (*p, 1) && CONSTANT_P (XEXP (*p, 1)))
    return p;

  /* Otherwise, check each summand for containing a constant term.  */

  if (XEXP (*p, 0) != 0)
    {
      tem = find_constant_term_loc (&XEXP (*p, 0));
      if (tem != 0)
	return tem;
    }

  if (XEXP (*p, 1) != 0)
    {
      tem = find_constant_term_loc (&XEXP (*p, 1));
      if (tem != 0)
	return tem;
    }

  return 0;
}

/* Return 1 if OP is a memory reference
   whose address contains no side effects
   and remains valid after the addition
   of a positive integer less than the
   size of the object being referenced.

   We assume that the original address is valid and do not check it.

   This uses strict_memory_address_p as a subroutine, so
   don't use it before reload.  */

int
offsettable_memref_p (op)
     rtx op;
{
  return ((GET_CODE (op) == MEM)
	  && offsettable_address_p (1, GET_MODE (op), XEXP (op, 0)));
}

/* Similar, but don't require a strictly valid mem ref:
   consider pseudo-regs valid as index or base regs.  */

int
offsettable_nonstrict_memref_p (op)
     rtx op;
{
  return ((GET_CODE (op) == MEM)
	  && offsettable_address_p (0, GET_MODE (op), XEXP (op, 0)));
}

/* Return 1 if Y is a memory address which contains no side effects
   and would remain valid after the addition of a positive integer
   less than the size of that mode.

   We assume that the original address is valid and do not check it.
   We do check that it is valid for narrower modes.

   If STRICTP is nonzero, we require a strictly valid address,
   for the sake of use in reload.c.  */

int
offsettable_address_p (strictp, mode, y)
     int strictp;
     enum machine_mode mode;
     register rtx y;
{
  register enum rtx_code ycode = GET_CODE (y);
  register rtx z;
  rtx y1 = y;
  rtx *y2;
  int (*addressp) () = (strictp ? strict_memory_address_p : memory_address_p);

  if (CONSTANT_ADDRESS_P (y))
    return 1;

  /* Adjusting an offsettable address involves changing to a narrower mode.
     Make sure that's OK.  */

  if (mode_dependent_address_p (y))
    return 0;

  /* If the expression contains a constant term,
     see if it remains valid when max possible offset is added.  */

  if ((ycode == PLUS) && (y2 = find_constant_term_loc (&y1)))
    {
      int good;

      y1 = *y2;
      *y2 = plus_constant (*y2, GET_MODE_SIZE (mode) - 1);
      /* Use QImode because an odd displacement may be automatically invalid
	 for any wider mode.  But it should be valid for a single byte.  */
      good = (*addressp) (QImode, y);

      /* In any case, restore old contents of memory.  */
      *y2 = y1;
      return good;
    }

  if (ycode == PRE_DEC || ycode == PRE_INC
      || ycode == POST_DEC || ycode == POST_INC)
    return 0;

  /* The offset added here is chosen as the maximum offset that
     any instruction could need to add when operating on something
     of the specified mode.  We assume that if Y and Y+c are
     valid addresses then so is Y+d for all 0<d<c.  */

  z = plus_constant_for_output (y, GET_MODE_SIZE (mode) - 1);

  /* Use QImode because an odd displacement may be automatically invalid
     for any wider mode.  But it should be valid for a single byte.  */
  return (*addressp) (QImode, z);
}

/* Return 1 if ADDR is an address-expression whose effect depends
   on the mode of the memory reference it is used in.

   Autoincrement addressing is a typical example of mode-dependence
   because the amount of the increment depends on the mode.  */

int
mode_dependent_address_p (addr)
     rtx addr;
{
  GO_IF_MODE_DEPENDENT_ADDRESS (addr, win);
  return 0;
 win:
  return 1;
}

/* Return 1 if OP is a general operand
   other than a memory ref with a mode dependent address.  */

int
mode_independent_operand (op, mode)
     enum machine_mode mode;
     rtx op;
{
  rtx addr;

  if (! general_operand (op, mode))
    return 0;

  if (GET_CODE (op) != MEM)
    return 1;

  addr = XEXP (op, 0);
  GO_IF_MODE_DEPENDENT_ADDRESS (addr, lose);
  return 1;
 lose:
  return 0;
}

/* Given an operand OP that is a valid memory reference
   which satisfies offsettable_memref_p,
   return a new memory reference whose address has been adjusted by OFFSET.
   OFFSET should be positive and less than the size of the object referenced.
*/

rtx
adj_offsettable_operand (op, offset)
     rtx op;
     int offset;
{
  register enum rtx_code code = GET_CODE (op);

  if (code == MEM) 
    {
      register rtx y = XEXP (op, 0);
      register rtx new;

      if (CONSTANT_ADDRESS_P (y))
	{
	  new = gen_rtx (MEM, GET_MODE (op), plus_constant_for_output (y, offset));
	  RTX_UNCHANGING_P (new) = RTX_UNCHANGING_P (op);
	  return new;
	}

      if (GET_CODE (y) == PLUS)
	{
	  rtx z = y;
	  register rtx *const_loc;

	  op = copy_rtx (op);
	  z = XEXP (op, 0);
	  const_loc = find_constant_term_loc (&z);
	  if (const_loc)
	    {
	      *const_loc = plus_constant_for_output (*const_loc, offset);
	      return op;
	    }
	}

      new = gen_rtx (MEM, GET_MODE (op), plus_constant_for_output (y, offset));
      RTX_UNCHANGING_P (new) = RTX_UNCHANGING_P (op);
      return new;
    }
  abort ();
}

#ifdef REGISTER_CONSTRAINTS

/* Check the operands of an insn (found in recog_operands)
   against the insn's operand constraints (found via INSN_CODE_NUM)
   and return 1 if they are valid.

   WHICH_ALTERNATIVE is set to a number which indicates which
   alternative of constraints was matched: 0 for the first alternative,
   1 for the next, etc.

   In addition, when two operands are match
   and it happens that the output operand is (reg) while the
   input operand is --(reg) or ++(reg) (a pre-inc or pre-dec),
   make the output operand look like the input.
   This is because the output operand is the one the template will print.

   This is used in final, just before printing the assembler code and by
   the routines that determine an insn's attribute.

   If STRICT is a positive non-zero value, it means that we have been
   called after reload has been completed.  In that case, we must
   do all checks strictly.  If it is zero, it means that we have been called
   before reload has completed.  In that case, we first try to see if we can
   find an alternative that matches strictly.  If not, we try again, this
   time assuming that reload will fix up the insn.  This provides a "best
   guess" for the alternative and is used to compute attributes of insns prior
   to reload.  A negative value of STRICT is used for this internal call.  */

struct funny_match
{
  int this, other;
};

int
constrain_operands (insn_code_num, strict)
     int insn_code_num;
     int strict;
{
  char *constraints[MAX_RECOG_OPERANDS];
  int matching_operands[MAX_RECOG_OPERANDS];
  enum op_type {OP_IN, OP_OUT, OP_INOUT} op_types[MAX_RECOG_OPERANDS];
  int earlyclobber[MAX_RECOG_OPERANDS];
  register int c;
  int noperands = insn_n_operands[insn_code_num];

  struct funny_match funny_match[MAX_RECOG_OPERANDS];
  int funny_match_index;
  int nalternatives = insn_n_alternatives[insn_code_num];

  if (noperands == 0 || nalternatives == 0)
    return 1;

  for (c = 0; c < noperands; c++)
    {
      constraints[c] = insn_operand_constraint[insn_code_num][c];
      matching_operands[c] = -1;
      op_types[c] = OP_IN;
    }

  which_alternative = 0;

  while (which_alternative < nalternatives)
    {
      register int opno;
      int lose = 0;
      funny_match_index = 0;

      for (opno = 0; opno < noperands; opno++)
	{
	  register rtx op = recog_operand[opno];
	  enum machine_mode mode = GET_MODE (op);
	  register char *p = constraints[opno];
	  int offset = 0;
	  int win = 0;
	  int val;

	  earlyclobber[opno] = 0;

	  if (GET_CODE (op) == SUBREG)
	    {
	      if (GET_CODE (SUBREG_REG (op)) == REG
		  && REGNO (SUBREG_REG (op)) < FIRST_PSEUDO_REGISTER)
		offset = SUBREG_WORD (op);
	      op = SUBREG_REG (op);
	    }

	  /* An empty constraint or empty alternative
	     allows anything which matched the pattern.  */
	  if (*p == 0 || *p == ',')
	    win = 1;

	  while (*p && (c = *p++) != ',')
	    switch (c)
	      {
	      case '?':
	      case '#':
	      case '!':
	      case '*':
	      case '%':
		break;

	      case '=':
		op_types[opno] = OP_OUT;
		break;

	      case '+':
		op_types[opno] = OP_INOUT;
		break;

	      case '&':
		earlyclobber[opno] = 1;
		break;

	      case '0':
	      case '1':
	      case '2':
	      case '3':
	      case '4':
		/* This operand must be the same as a previous one.
		   This kind of constraint is used for instructions such
		   as add when they take only two operands.

		   Note that the lower-numbered operand is passed first.

		   If we are not testing strictly, assume that this constraint
		   will be satisfied.  */
		if (strict < 0)
		  val = 1;
		else
		  val = operands_match_p (recog_operand[c - '0'],
					  recog_operand[opno]);

		matching_operands[opno] = c - '0';
		matching_operands[c - '0'] = opno;

		if (val != 0)
		  win = 1;
		/* If output is *x and input is *--x,
		   arrange later to change the output to *--x as well,
		   since the output op is the one that will be printed.  */
		if (val == 2 && strict > 0)
		  {
		    funny_match[funny_match_index].this = opno;
		    funny_match[funny_match_index++].other = c - '0';
		  }
		break;

	      case 'p':
		/* p is used for address_operands.  When we are called by
		   gen_input_reload, no one will have checked that the
		   address is strictly valid, i.e., that all pseudos
		   requiring hard regs have gotten them.  */
		if (strict <= 0
		    || (strict_memory_address_p
			(insn_operand_mode[insn_code_num][opno], op)))
		  win = 1;
		break;

		/* No need to check general_operand again;
		   it was done in insn-recog.c.  */
	      case 'g':
		/* Anything goes unless it is a REG and really has a hard reg
		   but the hard reg is not in the class GENERAL_REGS.  */
		if (strict < 0
		    || GENERAL_REGS == ALL_REGS
		    || GET_CODE (op) != REG
		    || (reload_in_progress
			&& REGNO (op) >= FIRST_PSEUDO_REGISTER)
		    || reg_fits_class_p (op, GENERAL_REGS, offset, mode))
		  win = 1;
		break;

	      case 'r':
		if (strict < 0
		    || (strict == 0
			&& GET_CODE (op) == REG
			&& REGNO (op) >= FIRST_PSEUDO_REGISTER)
		    || (strict == 0 && GET_CODE (op) == SCRATCH)
		    || (GET_CODE (op) == REG
			&& ((GENERAL_REGS == ALL_REGS
			     && REGNO (op) < FIRST_PSEUDO_REGISTER)
			    || reg_fits_class_p (op, GENERAL_REGS,
						 offset, mode))))
		  win = 1;
		break;

	      case 'X':
		/* This is used for a MATCH_SCRATCH in the cases when we
		   don't actually need anything.  So anything goes any time. */
		win = 1;
		break;

	      case 'm':
		if (GET_CODE (op) == MEM
		    /* Before reload, accept what reload can turn into mem.  */
		    || (strict < 0 && CONSTANT_P (op))
		    /* During reload, accept a pseudo  */
		    || (reload_in_progress && GET_CODE (op) == REG
			&& REGNO (op) >= FIRST_PSEUDO_REGISTER))
		  win = 1;
		break;

	      case '<':
		if (GET_CODE (op) == MEM
		    && (GET_CODE (XEXP (op, 0)) == PRE_DEC
			|| GET_CODE (XEXP (op, 0)) == POST_DEC))
		  win = 1;
		break;

	      case '>':
		if (GET_CODE (op) == MEM
		    && (GET_CODE (XEXP (op, 0)) == PRE_INC
			|| GET_CODE (XEXP (op, 0)) == POST_INC))
		  win = 1;
		break;

	      case 'E':
		/* Match any CONST_DOUBLE, but only if
		   we can examine the bits of it reliably.  */
		if ((HOST_FLOAT_FORMAT != TARGET_FLOAT_FORMAT
		     || HOST_BITS_PER_WIDE_INT != BITS_PER_WORD)
		    && GET_MODE (op) != VOIDmode && ! flag_pretend_float)
		  break;
		if (GET_CODE (op) == CONST_DOUBLE)
		  win = 1;
		break;

	      case 'F':
		if (GET_CODE (op) == CONST_DOUBLE)
		  win = 1;
		break;

	      case 'G':
	      case 'H':
		if (GET_CODE (op) == CONST_DOUBLE
		    && CONST_DOUBLE_OK_FOR_LETTER_P (op, c))
		  win = 1;
		break;

	      case 's':
		if (GET_CODE (op) == CONST_INT
		    || (GET_CODE (op) == CONST_DOUBLE
			&& GET_MODE (op) == VOIDmode))
		  break;
	      case 'i':
		if (CONSTANT_P (op))
		  win = 1;
		break;

	      case 'n':
		if (GET_CODE (op) == CONST_INT
		    || (GET_CODE (op) == CONST_DOUBLE
			&& GET_MODE (op) == VOIDmode))
		  win = 1;
		break;

	      case 'I':
	      case 'J':
	      case 'K':
	      case 'L':
	      case 'M':
	      case 'N':
	      case 'O':
	      case 'P':
		if (GET_CODE (op) == CONST_INT
		    && CONST_OK_FOR_LETTER_P (INTVAL (op), c))
		  win = 1;
		break;

#ifdef EXTRA_CONSTRAINT
              case 'Q':
              case 'R':
              case 'S':
              case 'T':
              case 'U':
		if (EXTRA_CONSTRAINT (op, c))
		  win = 1;
		break;
#endif

	      case 'V':
		if (GET_CODE (op) == MEM
		    && ! offsettable_memref_p (op))
		  win = 1;
		break;

	      case 'o':
		if ((strict > 0 && offsettable_memref_p (op))
		    || (strict == 0 && offsettable_nonstrict_memref_p (op))
		    /* Before reload, accept what reload can handle.  */
		    || (strict < 0
			&& (CONSTANT_P (op) || GET_CODE (op) == MEM))
		    /* During reload, accept a pseudo  */
		    || (reload_in_progress && GET_CODE (op) == REG
			&& REGNO (op) >= FIRST_PSEUDO_REGISTER))
		  win = 1;
		break;

	      default:
		if (strict < 0
		    || (strict == 0
			&& GET_CODE (op) == REG
			&& REGNO (op) >= FIRST_PSEUDO_REGISTER)
		    || (strict == 0 && GET_CODE (op) == SCRATCH)
		    || (GET_CODE (op) == REG
			&& reg_fits_class_p (op, REG_CLASS_FROM_LETTER (c),
					     offset, mode)))
		  win = 1;
	      }

	  constraints[opno] = p;
	  /* If this operand did not win somehow,
	     this alternative loses.  */
	  if (! win)
	    lose = 1;
	}
      /* This alternative won; the operands are ok.
	 Change whichever operands this alternative says to change.  */
      if (! lose)
	{
	  int opno, eopno;

	  /* See if any earlyclobber operand conflicts with some other
	     operand.  */

	  if (strict > 0)
	    for (eopno = 0; eopno < noperands; eopno++)
	      /* Ignore earlyclobber operands now in memory,
		 because we would often report failure when we have
		 two memory operands, one of which was formerly a REG.  */
	      if (earlyclobber[eopno]
		  && GET_CODE (recog_operand[eopno]) == REG)
		for (opno = 0; opno < noperands; opno++)
		  if ((GET_CODE (recog_operand[opno]) == MEM
		       || op_types[opno] != OP_OUT)
		      && opno != eopno
		      /* Ignore things like match_operator operands. */
		      && *constraints[opno] != 0
		      && ! (matching_operands[opno] == eopno
			    && rtx_equal_p (recog_operand[opno],
					    recog_operand[eopno]))
		      && ! safe_from_earlyclobber (recog_operand[opno],
						   recog_operand[eopno]))
		    lose = 1;

	  if (! lose)
	    {
	      while (--funny_match_index >= 0)
		{
		  recog_operand[funny_match[funny_match_index].other]
		    = recog_operand[funny_match[funny_match_index].this];
		}

	      return 1;
	    }
	}

      which_alternative++;
    }

  /* If we are about to reject this, but we are not to test strictly,
     try a very loose test.  Only return failure if it fails also.  */
  if (strict == 0)
    return constrain_operands (insn_code_num, -1);
  else
    return 0;
}

/* Return 1 iff OPERAND (assumed to be a REG rtx)
   is a hard reg in class CLASS when its regno is offsetted by OFFSET
   and changed to mode MODE.
   If REG occupies multiple hard regs, all of them must be in CLASS.  */

int
reg_fits_class_p (operand, class, offset, mode)
     rtx operand;
     register enum reg_class class;
     int offset;
     enum machine_mode mode;
{
  register int regno = REGNO (operand);
  if (regno < FIRST_PSEUDO_REGISTER
      && TEST_HARD_REG_BIT (reg_class_contents[(int) class],
			    regno + offset))
    {
      register int sr;
      regno += offset;
      for (sr = HARD_REGNO_NREGS (regno, mode) - 1;
	   sr > 0; sr--)
	if (! TEST_HARD_REG_BIT (reg_class_contents[(int) class],
				 regno + sr))
	  break;
      return sr == 0;
    }

  return 0;
}

#endif /* REGISTER_CONSTRAINTS */
