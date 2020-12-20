/* Try to unroll loops, and split induction variables.
   Copyright (C) 1992, 1993 Free Software Foundation, Inc.
   Contributed by James E. Wilson, Cygnus Support/UC Berkeley.

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

/* Try to unroll a loop, and split induction variables.

   Loops for which the number of iterations can be calculated exactly are
   handled specially.  If the number of iterations times the insn_count is
   less than MAX_UNROLLED_INSNS, then the loop is unrolled completely.
   Otherwise, we try to unroll the loop a number of times modulo the number
   of iterations, so that only one exit test will be needed.  It is unrolled
   a number of times approximately equal to MAX_UNROLLED_INSNS divided by
   the insn count.

   Otherwise, if the number of iterations can be calculated exactly at
   run time, and the loop is always entered at the top, then we try to
   precondition the loop.  That is, at run time, calculate how many times
   the loop will execute, and then execute the loop body a few times so
   that the remaining iterations will be some multiple of 4 (or 2 if the
   loop is large).  Then fall through to a loop unrolled 4 (or 2) times,
   with only one exit test needed at the end of the loop.

   Otherwise, if the number of iterations can not be calculated exactly,
   not even at run time, then we still unroll the loop a number of times
   approximately equal to MAX_UNROLLED_INSNS divided by the insn count,
   but there must be an exit test after each copy of the loop body.

   For each induction variable, which is dead outside the loop (replaceable)
   or for which we can easily calculate the final value, if we can easily
   calculate its value at each place where it is set as a function of the
   current loop unroll count and the variable's value at loop entry, then
   the induction variable is split into `N' different variables, one for
   each copy of the loop body.  One variable is live across the backward
   branch, and the others are all calculated as a function of this variable.
   This helps eliminate data dependencies, and leads to further opportunities
   for cse.  */

/* Possible improvements follow:  */

/* ??? Add an extra pass somewhere to determine whether unrolling will
   give any benefit.  E.g. after generating all unrolled insns, compute the
   cost of all insns and compare against cost of insns in rolled loop.

   - On traditional architectures, unrolling a non-constant bound loop
     is a win if there is a giv whose only use is in memory addresses, the
     memory addresses can be split, and hence giv increments can be
     eliminated.
   - It is also a win if the loop is executed many times, and preconditioning
     can be performed for the loop.
   Add code to check for these and similar cases.  */

/* ??? Improve control of which loops get unrolled.  Could use profiling
   info to only unroll the most commonly executed loops.  Perhaps have
   a user specifyable option to control the amount of code expansion,
   or the percent of loops to consider for unrolling.  Etc.  */

/* ??? Look at the register copies inside the loop to see if they form a
   simple permutation.  If so, iterate the permutation until it gets back to
   the start state.  This is how many times we should unroll the loop, for
   best results, because then all register copies can be eliminated.
   For example, the lisp nreverse function should be unrolled 3 times
   while (this)
     {
       next = this->cdr;
       this->cdr = prev;
       prev = this;
       this = next;
     }

   ??? The number of times to unroll the loop may also be based on data
   references in the loop.  For example, if we have a loop that references
   x[i-1], x[i], and x[i+1], we should unroll it a multiple of 3 times.  */

/* ??? Add some simple linear equation solving capability so that we can
   determine the number of loop iterations for more complex loops.
   For example, consider this loop from gdb
   #define SWAP_TARGET_AND_HOST(buffer,len)
     {
       char tmp;
       char *p = (char *) buffer;
       char *q = ((char *) buffer) + len - 1;
       int iterations = (len + 1) >> 1;
       int i;
       for (p; p < q; p++, q--;)
         {
           tmp = *q;
           *q = *p;
           *p = tmp;
         }
     }
   Note that:
     start value = p = &buffer + current_iteration
     end value   = q = &buffer + len - 1 - current_iteration
   Given the loop exit test of "p < q", then there must be "q - p" iterations,
   set equal to zero and solve for number of iterations:
     q - p = len - 1 - 2*current_iteration = 0
     current_iteration = (len - 1) / 2
   Hence, there are (len - 1) / 2 (rounded up to the nearest integer)
   iterations of this loop.  */

/* ??? Currently, no labels are marked as loop invariant when doing loop
   unrolling.  This is because an insn inside the loop, that loads the address
   of a label inside the loop into a register, could be moved outside the loop
   by the invariant code motion pass if labels were invariant.  If the loop
   is subsequently unrolled, the code will be wrong because each unrolled
   body of the loop will use the same address, whereas each actually needs a
   different address.  A case where this happens is when a loop containing
   a switch statement is unrolled.

   It would be better to let labels be considered invariant.  When we
   unroll loops here, check to see if any insns using a label local to the
   loop were moved before the loop.  If so, then correct the problem, by
   moving the insn back into the loop, or perhaps replicate the insn before
   the loop, one copy for each time the loop is unrolled.  */

/* The prime factors looked for when trying to unroll a loop by some
   number which is modulo the total number of iterations.  Just checking
   for these 4 prime factors will find at least one factor for 75% of
   all numbers theoretically.  Practically speaking, this will succeed
   almost all of the time since loops are generally a multiple of 2
   and/or 5.  */

#define NUM_FACTORS 4

struct _factor { int factor, count; } factors[NUM_FACTORS]
  = { {2, 0}, {3, 0}, {5, 0}, {7, 0}};
      
/* Describes the different types of loop unrolling performed.  */

enum unroll_types { UNROLL_COMPLETELY, UNROLL_MODULO, UNROLL_NAIVE };

#include "config.h"
#include "rtl.h"
#include "insn-config.h"
#include "integrate.h"
#include "regs.h"
#include "flags.h"
#include "expr.h"
#include <stdio.h>
#include "loop.h"

/* This controls which loops are unrolled, and by how much we unroll
   them.  */

#ifndef MAX_UNROLLED_INSNS
#define MAX_UNROLLED_INSNS 100
#endif

/* Indexed by register number, if non-zero, then it contains a pointer
   to a struct induction for a DEST_REG giv which has been combined with
   one of more address givs.  This is needed because whenever such a DEST_REG
   giv is modified, we must modify the value of all split address givs
   that were combined with this DEST_REG giv.  */

static struct induction **addr_combined_regs;

/* Indexed by register number, if this is a splittable induction variable,
   then this will hold the current value of the register, which depends on the
   iteration number.  */

static rtx *splittable_regs;

/* Indexed by register number, if this is a splittable induction variable,
   then this will hold the number of instructions in the loop that modify
   the induction variable.  Used to ensure that only the last insn modifying
   a split iv will update the original iv of the dest.  */

static int *splittable_regs_updates;

/* Values describing the current loop's iteration variable.  These are set up
   by loop_iterations, and used by precondition_loop_p.  */

static rtx loop_iteration_var;
static rtx loop_initial_value;
static rtx loop_increment;
static rtx loop_final_value;

/* Forward declarations.  */

static void init_reg_map ();
static int precondition_loop_p ();
static void copy_loop_body ();
static void iteration_info ();
static rtx approx_final_value ();
static int find_splittable_regs ();
static int find_splittable_givs ();
static rtx fold_rtx_mult_add ();

/* Try to unroll one loop and split induction variables in the loop.

   The loop is described by the arguments LOOP_END, INSN_COUNT, and
   LOOP_START.  END_INSERT_BEFORE indicates where insns should be added
   which need to be executed when the loop falls through.  STRENGTH_REDUCTION_P
   indicates whether information generated in the strength reduction pass
   is available.

   This function is intended to be called from within `strength_reduce'
   in loop.c.  */

void
unroll_loop (loop_end, insn_count, loop_start, end_insert_before,
	     strength_reduce_p)
     rtx loop_end;
     int insn_count;
     rtx loop_start;
     rtx end_insert_before;
     int strength_reduce_p;
{
  int i, j, temp;
  int unroll_number = 1;
  rtx copy_start, copy_end;
  rtx insn, copy, sequence, pattern, tem;
  int max_labelno, max_insnno;
  rtx insert_before;
  struct inline_remap *map;
  char *local_label;
  int maxregnum;
  int new_maxregnum;
  rtx exit_label = 0;
  rtx start_label;
  struct iv_class *bl;
  struct induction *v;
  int splitting_not_safe = 0;
  enum unroll_types unroll_type;
  int loop_preconditioned = 0;
  rtx safety_label;
  /* This points to the last real insn in the loop, which should be either
     a JUMP_INSN (for conditional jumps) or a BARRIER (for unconditional
     jumps).  */
  rtx last_loop_insn;

  /* Don't bother unrolling huge loops.  Since the minimum factor is
     two, loops greater than one half of MAX_UNROLLED_INSNS will never
     be unrolled.  */
  if (insn_count > MAX_UNROLLED_INSNS / 2)
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream, "Unrolling failure: Loop too big.\n");
      return;
    }

  /* When emitting debugger info, we can't unroll loops with unequal numbers
     of block_beg and block_end notes, because that would unbalance the block
     structure of the function.  This can happen as a result of the
     "if (foo) bar; else break;" optimization in jump.c.  */

  if (write_symbols != NO_DEBUG)
    {
      int block_begins = 0;
      int block_ends = 0;

      for (insn = loop_start; insn != loop_end; insn = NEXT_INSN (insn))
	{
	  if (GET_CODE (insn) == NOTE)
	    {
	      if (NOTE_LINE_NUMBER (insn) == NOTE_INSN_BLOCK_BEG)
		block_begins++;
	      else if (NOTE_LINE_NUMBER (insn) == NOTE_INSN_BLOCK_END)
		block_ends++;
	    }
	}

      if (block_begins != block_ends)
	{
	  if (loop_dump_stream)
	    fprintf (loop_dump_stream,
		     "Unrolling failure: Unbalanced block notes.\n");
	  return;
	}
    }

  /* Determine type of unroll to perform.  Depends on the number of iterations
     and the size of the loop.  */

  /* If there is no strength reduce info, then set loop_n_iterations to zero.
     This can happen if strength_reduce can't find any bivs in the loop.
     A value of zero indicates that the number of iterations could not be
     calculated.  */

  if (! strength_reduce_p)
    loop_n_iterations = 0;

  if (loop_dump_stream && loop_n_iterations > 0)
    fprintf (loop_dump_stream,
	     "Loop unrolling: %d iterations.\n", loop_n_iterations);

  /* Find and save a pointer to the last nonnote insn in the loop.  */

  last_loop_insn = prev_nonnote_insn (loop_end);

  /* Calculate how many times to unroll the loop.  Indicate whether or
     not the loop is being completely unrolled.  */

  if (loop_n_iterations == 1)
    {
      /* If number of iterations is exactly 1, then eliminate the compare and
	 branch at the end of the loop since they will never be taken.
	 Then return, since no other action is needed here.  */

      /* If the last instruction is not a BARRIER or a JUMP_INSN, then
	 don't do anything.  */

      if (GET_CODE (last_loop_insn) == BARRIER)
	{
	  /* Delete the jump insn.  This will delete the barrier also.  */
	  delete_insn (PREV_INSN (last_loop_insn));
	}
      else if (GET_CODE (last_loop_insn) == JUMP_INSN)
	{
#ifdef HAVE_cc0
	  /* The immediately preceding insn is a compare which must be
	     deleted.  */
	  delete_insn (last_loop_insn);
	  delete_insn (PREV_INSN (last_loop_insn));
#else
	  /* The immediately preceding insn may not be the compare, so don't
	     delete it.  */
	  delete_insn (last_loop_insn);
#endif
	}
      return;
    }
  else if (loop_n_iterations > 0
      && loop_n_iterations * insn_count < MAX_UNROLLED_INSNS)
    {
      unroll_number = loop_n_iterations;
      unroll_type = UNROLL_COMPLETELY;
    }
  else if (loop_n_iterations > 0)
    {
      /* Try to factor the number of iterations.  Don't bother with the
	 general case, only using 2, 3, 5, and 7 will get 75% of all
	 numbers theoretically, and almost all in practice.  */

      for (i = 0; i < NUM_FACTORS; i++)
	factors[i].count = 0;

      temp = loop_n_iterations;
      for (i = NUM_FACTORS - 1; i >= 0; i--)
	while (temp % factors[i].factor == 0)
	  {
	    factors[i].count++;
	    temp = temp / factors[i].factor;
	  }

      /* Start with the larger factors first so that we generally
	 get lots of unrolling.  */

      unroll_number = 1;
      temp = insn_count;
      for (i = 3; i >= 0; i--)
	while (factors[i].count--)
	  {
	    if (temp * factors[i].factor < MAX_UNROLLED_INSNS)
	      {
		unroll_number *= factors[i].factor;
		temp *= factors[i].factor;
	      }
	    else
	      break;
	  }

      /* If we couldn't find any factors, then unroll as in the normal
	 case.  */
      if (unroll_number == 1)
	{
	  if (loop_dump_stream)
	    fprintf (loop_dump_stream,
		     "Loop unrolling: No factors found.\n");
	}
      else
	unroll_type = UNROLL_MODULO;
    }


  /* Default case, calculate number of times to unroll loop based on its
     size.  */
  if (unroll_number == 1)
    {
      if (8 * insn_count < MAX_UNROLLED_INSNS)
	unroll_number = 8;
      else if (4 * insn_count < MAX_UNROLLED_INSNS)
	unroll_number = 4;
      else
	unroll_number = 2;

      unroll_type = UNROLL_NAIVE;
    }

  /* Now we know how many times to unroll the loop.  */

  if (loop_dump_stream)
    fprintf (loop_dump_stream,
	     "Unrolling loop %d times.\n", unroll_number);


  if (unroll_type == UNROLL_COMPLETELY || unroll_type == UNROLL_MODULO)
    {
      /* Loops of these types should never start with a jump down to
	 the exit condition test.  For now, check for this case just to
	 be sure.  UNROLL_NAIVE loops can be of this form, this case is
	 handled below.  */
      insn = loop_start;
      while (GET_CODE (insn) != CODE_LABEL && GET_CODE (insn) != JUMP_INSN)
	insn = NEXT_INSN (insn);
      if (GET_CODE (insn) == JUMP_INSN)
	abort ();
    }

  if (unroll_type == UNROLL_COMPLETELY)
    {
      /* Completely unrolling the loop:  Delete the compare and branch at
	 the end (the last two instructions).   This delete must done at the
	 very end of loop unrolling, to avoid problems with calls to
	 back_branch_in_range_p, which is called by find_splittable_regs.
	 All increments of splittable bivs/givs are changed to load constant
	 instructions.  */

      copy_start = loop_start;

      /* Set insert_before to the instruction immediately after the JUMP_INSN
	 (or BARRIER), so that any NOTEs between the JUMP_INSN and the end of
	 the loop will be correctly handled by copy_loop_body.  */
      insert_before = NEXT_INSN (last_loop_insn);

      /* Set copy_end to the insn before the jump at the end of the loop.  */
      if (GET_CODE (last_loop_insn) == BARRIER)
	copy_end = PREV_INSN (PREV_INSN (last_loop_insn));
      else if (GET_CODE (last_loop_insn) == JUMP_INSN)
	{
#ifdef HAVE_cc0
	  /* The instruction immediately before the JUMP_INSN is a compare
	     instruction which we do not want to copy.  */
	  copy_end = PREV_INSN (PREV_INSN (last_loop_insn));
#else
	  /* The instruction immediately before the JUMP_INSN may not be the
	     compare, so we must copy it.  */
	  copy_end = PREV_INSN (last_loop_insn);
#endif
	}
      else
	{
	  /* We currently can't unroll a loop if it doesn't end with a
	     JUMP_INSN.  There would need to be a mechanism that recognizes
	     this case, and then inserts a jump after each loop body, which
	     jumps to after the last loop body.  */
	  if (loop_dump_stream)
	    fprintf (loop_dump_stream,
		     "Unrolling failure: loop does not end with a JUMP_INSN.\n");
	  return;
	}
    }
  else if (unroll_type == UNROLL_MODULO)
    {
      /* Partially unrolling the loop:  The compare and branch at the end
	 (the last two instructions) must remain.  Don't copy the compare
	 and branch instructions at the end of the loop.  Insert the unrolled
	 code immediately before the compare/branch at the end so that the
	 code will fall through to them as before.  */

      copy_start = loop_start;

      /* Set insert_before to the jump insn at the end of the loop.
	 Set copy_end to before the jump insn at the end of the loop.  */
      if (GET_CODE (last_loop_insn) == BARRIER)
	{
	  insert_before = PREV_INSN (last_loop_insn);
	  copy_end = PREV_INSN (insert_before);
	}
      else if (GET_CODE (last_loop_insn) == JUMP_INSN)
	{
#ifdef HAVE_cc0
	  /* The instruction immediately before the JUMP_INSN is a compare
	     instruction which we do not want to copy or delete.  */
	  insert_before = PREV_INSN (last_loop_insn);
	  copy_end = PREV_INSN (insert_before);
#else
	  /* The instruction immediately before the JUMP_INSN may not be the
	     compare, so we must copy it.  */
	  insert_before = last_loop_insn;
	  copy_end = PREV_INSN (last_loop_insn);
#endif
	}
      else
	{
	  /* We currently can't unroll a loop if it doesn't end with a
	     JUMP_INSN.  There would need to be a mechanism that recognizes
	     this case, and then inserts a jump after each loop body, which
	     jumps to after the last loop body.  */
	  if (loop_dump_stream)
	    fprintf (loop_dump_stream,
		     "Unrolling failure: loop does not end with a JUMP_INSN.\n");
	  return;
	}
    }
  else
    {
      /* Normal case: Must copy the compare and branch instructions at the
	 end of the loop.  */

      if (GET_CODE (last_loop_insn) == BARRIER)
	{
	  /* Loop ends with an unconditional jump and a barrier.
	     Handle this like above, don't copy jump and barrier.
	     This is not strictly necessary, but doing so prevents generating
	     unconditional jumps to an immediately following label.

	     This will be corrected below if the target of this jump is
	     not the start_label.  */

	  insert_before = PREV_INSN (last_loop_insn);
	  copy_end = PREV_INSN (insert_before);
	}
      else if (GET_CODE (last_loop_insn) == JUMP_INSN)
	{
	  /* Set insert_before to immediately after the JUMP_INSN, so that
	     NOTEs at the end of the loop will be correctly handled by
	     copy_loop_body.  */
	  insert_before = NEXT_INSN (last_loop_insn);
	  copy_end = last_loop_insn;
	}
      else
	{
	  /* We currently can't unroll a loop if it doesn't end with a
	     JUMP_INSN.  There would need to be a mechanism that recognizes
	     this case, and then inserts a jump after each loop body, which
	     jumps to after the last loop body.  */
	  if (loop_dump_stream)
	    fprintf (loop_dump_stream,
		     "Unrolling failure: loop does not end with a JUMP_INSN.\n");
	  return;
	}

      /* If copying exit test branches because they can not be eliminated,
	 then must convert the fall through case of the branch to a jump past
	 the end of the loop.  Create a label to emit after the loop and save
	 it for later use.  Do not use the label after the loop, if any, since
	 it might be used by insns outside the loop, or there might be insns
	 added before it later by final_[bg]iv_value which must be after
	 the real exit label.  */
      exit_label = gen_label_rtx ();

      insn = loop_start;
      while (GET_CODE (insn) != CODE_LABEL && GET_CODE (insn) != JUMP_INSN)
	insn = NEXT_INSN (insn);

      if (GET_CODE (insn) == JUMP_INSN)
	{
	  /* The loop starts with a jump down to the exit condition test.
	     Start copying the loop after the barrier following this
	     jump insn.  */
	  copy_start = NEXT_INSN (insn);

	  /* Splitting induction variables doesn't work when the loop is
	     entered via a jump to the bottom, because then we end up doing
	     a comparison against a new register for a split variable, but
	     we did not execute the set insn for the new register because
	     it was skipped over.  */
	  splitting_not_safe = 1;
	  if (loop_dump_stream)
	    fprintf (loop_dump_stream,
		     "Splitting not safe, because loop not entered at top.\n");
	}
      else
	copy_start = loop_start;
    }

  /* This should always be the first label in the loop.  */
  start_label = NEXT_INSN (copy_start);
  /* There may be a line number note and/or a loop continue note here.  */
  while (GET_CODE (start_label) == NOTE)
    start_label = NEXT_INSN (start_label);
  if (GET_CODE (start_label) != CODE_LABEL)
    {
      /* This can happen as a result of jump threading.  If the first insns in
	 the loop test the same condition as the loop's backward jump, or the
	 opposite condition, then the backward jump will be modified to point
	 to elsewhere, and the loop's start label is deleted.

	 This case currently can not be handled by the loop unrolling code.  */

      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Unrolling failure: unknown insns between BEG note and loop label.\n");
      return;
    }

  if (unroll_type == UNROLL_NAIVE
      && GET_CODE (last_loop_insn) == BARRIER
      && start_label != JUMP_LABEL (PREV_INSN (last_loop_insn)))
    {
      /* In this case, we must copy the jump and barrier, because they will
	 not be converted to jumps to an immediately following label.  */

      insert_before = NEXT_INSN (last_loop_insn);
      copy_end = last_loop_insn;
    }

  /* Allocate a translation table for the labels and insn numbers.
     They will be filled in as we copy the insns in the loop.  */

  max_labelno = max_label_num ();
  max_insnno = get_max_uid ();

  map = (struct inline_remap *) alloca (sizeof (struct inline_remap));

  map->integrating = 0;

  /* Allocate the label map.  */

  if (max_labelno > 0)
    {
      map->label_map = (rtx *) alloca (max_labelno * sizeof (rtx));

      local_label = (char *) alloca (max_labelno);
      bzero (local_label, max_labelno);
    }
  else
    map->label_map = 0;

  /* Search the loop and mark all local labels, i.e. the ones which have to
     be distinct labels when copied.  For all labels which might be
     non-local, set their label_map entries to point to themselves.
     If they happen to be local their label_map entries will be overwritten
     before the loop body is copied.  The label_map entries for local labels
     will be set to a different value each time the loop body is copied.  */

  for (insn = copy_start; insn != loop_end; insn = NEXT_INSN (insn))
    {
      if (GET_CODE (insn) == CODE_LABEL)
	local_label[CODE_LABEL_NUMBER (insn)] = 1;
      else if (GET_CODE (insn) == JUMP_INSN)
	{
	  if (JUMP_LABEL (insn))
	    map->label_map[CODE_LABEL_NUMBER (JUMP_LABEL (insn))]
	      = JUMP_LABEL (insn);
	  else if (GET_CODE (PATTERN (insn)) == ADDR_VEC
		   || GET_CODE (PATTERN (insn)) == ADDR_DIFF_VEC)
	    {
	      rtx pat = PATTERN (insn);
	      int diff_vec_p = GET_CODE (PATTERN (insn)) == ADDR_DIFF_VEC;
	      int len = XVECLEN (pat, diff_vec_p);
	      rtx label;

	      for (i = 0; i < len; i++)
		{
		  label = XEXP (XVECEXP (pat, diff_vec_p, i), 0);
		  map->label_map[CODE_LABEL_NUMBER (label)] = label;
		}
	    }
	}
    }

  /* Allocate space for the insn map.  */

  map->insn_map = (rtx *) alloca (max_insnno * sizeof (rtx));

  /* Set this to zero, to indicate that we are doing loop unrolling,
     not function inlining.  */
  map->inline_target = 0;

  /* The register and constant maps depend on the number of registers
     present, so the final maps can't be created until after
     find_splittable_regs is called.  However, they are needed for
     preconditioning, so we create temporary maps when preconditioning
     is performed.  */

  /* The preconditioning code may allocate two new pseudo registers.  */
  maxregnum = max_reg_num ();

  /* Allocate and zero out the splittable_regs and addr_combined_regs
     arrays.  These must be zeroed here because they will be used if
     loop preconditioning is performed, and must be zero for that case.

     It is safe to do this here, since the extra registers created by the
     preconditioning code and find_splittable_regs will never be used
     to access the splittable_regs[] and addr_combined_regs[] arrays.  */

  splittable_regs = (rtx *) alloca (maxregnum * sizeof (rtx));
  bzero (splittable_regs, maxregnum * sizeof (rtx));
  splittable_regs_updates = (int *) alloca (maxregnum * sizeof (int));
  bzero (splittable_regs_updates, maxregnum * sizeof (int));
  addr_combined_regs
    = (struct induction **) alloca (maxregnum * sizeof (struct induction *));
  bzero (addr_combined_regs, maxregnum * sizeof (struct induction *));

  /* If this loop requires exit tests when unrolled, check to see if we
     can precondition the loop so as to make the exit tests unnecessary.
     Just like variable splitting, this is not safe if the loop is entered
     via a jump to the bottom.  Also, can not do this if no strength
     reduce info, because precondition_loop_p uses this info.  */

  /* Must copy the loop body for preconditioning before the following
     find_splittable_regs call since that will emit insns which need to
     be after the preconditioned loop copies, but immediately before the
     unrolled loop copies.  */

  /* Also, it is not safe to split induction variables for the preconditioned
     copies of the loop body.  If we split induction variables, then the code
     assumes that each induction variable can be represented as a function
     of its initial value and the loop iteration number.  This is not true
     in this case, because the last preconditioned copy of the loop body
     could be any iteration from the first up to the `unroll_number-1'th,
     depending on the initial value of the iteration variable.  Therefore
     we can not split induction variables here, because we can not calculate
     their value.  Hence, this code must occur before find_splittable_regs
     is called.  */

  if (unroll_type == UNROLL_NAIVE && ! splitting_not_safe && strength_reduce_p)
    {
      rtx initial_value, final_value, increment;

      if (precondition_loop_p (&initial_value, &final_value, &increment,
			       loop_start, loop_end))
	{
	  register rtx diff, temp;
	  enum machine_mode mode;
	  rtx *labels;
	  int abs_inc, neg_inc;

	  map->reg_map = (rtx *) alloca (maxregnum * sizeof (rtx));

	  map->const_equiv_map = (rtx *) alloca (maxregnum * sizeof (rtx));
	  map->const_age_map = (unsigned *) alloca (maxregnum
						    * sizeof (unsigned));
	  map->const_equiv_map_size = maxregnum;
	  global_const_equiv_map = map->const_equiv_map;

	  init_reg_map (map, maxregnum);

	  /* Limit loop unrolling to 4, since this will make 7 copies of
	     the loop body.  */
	  if (unroll_number > 4)
	    unroll_number = 4;

	  /* Save the absolute value of the increment, and also whether or
	     not it is negative.  */
	  neg_inc = 0;
	  abs_inc = INTVAL (increment);
	  if (abs_inc < 0)
	    {
	      abs_inc = - abs_inc;
	      neg_inc = 1;
	    }

	  start_sequence ();

	  /* Decide what mode to do these calculations in.  Choose the larger
	     of final_value's mode and initial_value's mode, or a full-word if
	     both are constants.  */
	  mode = GET_MODE (final_value);
	  if (mode == VOIDmode)
	    {
	      mode = GET_MODE (initial_value);
	      if (mode == VOIDmode)
		mode = word_mode;
	    }
	  else if (mode != GET_MODE (initial_value)
		   && (GET_MODE_SIZE (mode)
		       < GET_MODE_SIZE (GET_MODE (initial_value))))
	    mode = GET_MODE (initial_value);

	  /* Calculate the difference between the final and initial values.
	     Final value may be a (plus (reg x) (const_int 1)) rtx.
	     Let the following cse pass simplify this if initial value is
	     a constant. 

	     We must copy the final and initial values here to avoid
	     improperly shared rtl.  */

	  diff = expand_binop (mode, sub_optab, copy_rtx (final_value),
			       copy_rtx (initial_value), NULL_RTX, 0,
			       OPTAB_LIB_WIDEN);

	  /* Now calculate (diff % (unroll * abs (increment))) by using an
	     and instruction.  */
	  diff = expand_binop (GET_MODE (diff), and_optab, diff,
			       GEN_INT (unroll_number * abs_inc - 1),
			       NULL_RTX, 0, OPTAB_LIB_WIDEN);

	  /* Now emit a sequence of branches to jump to the proper precond
	     loop entry point.  */

	  labels = (rtx *) alloca (sizeof (rtx) * unroll_number);
	  for (i = 0; i < unroll_number; i++)
	    labels[i] = gen_label_rtx ();

	  /* Assuming the unroll_number is 4, and the increment is 2, then
	     for a negative increment:	for a positive increment:
	     diff = 0,1   precond 0	diff = 0,7   precond 0
	     diff = 2,3   precond 3     diff = 1,2   precond 1
	     diff = 4,5   precond 2     diff = 3,4   precond 2
	     diff = 6,7   precond 1     diff = 5,6   precond 3  */

	  /* We only need to emit (unroll_number - 1) branches here, the
	     last case just falls through to the following code.  */

	  /* ??? This would give better code if we emitted a tree of branches
	     instead of the current linear list of branches.  */

	  for (i = 0; i < unroll_number - 1; i++)
	    {
	      int cmp_const;

	      /* For negative increments, must invert the constant compared
		 against, except when comparing against zero.  */
	      if (i == 0)
		cmp_const = 0;
	      else if (neg_inc)
		cmp_const = unroll_number - i;
	      else
		cmp_const = i;

	      emit_cmp_insn (diff, GEN_INT (abs_inc * cmp_const),
			     EQ, NULL_RTX, mode, 0, 0);

	      if (i == 0)
		emit_jump_insn (gen_beq (labels[i]));
	      else if (neg_inc)
		emit_jump_insn (gen_bge (labels[i]));
	      else
		emit_jump_insn (gen_ble (labels[i]));
	      JUMP_LABEL (get_last_insn ()) = labels[i];
	      LABEL_NUSES (labels[i])++;
	    }

	  /* If the increment is greater than one, then we need another branch,
	     to handle other cases equivalent to 0.  */

	  /* ??? This should be merged into the code above somehow to help
	     simplify the code here, and reduce the number of branches emitted.
	     For the negative increment case, the branch here could easily
	     be merged with the `0' case branch above.  For the positive
	     increment case, it is not clear how this can be simplified.  */
	     
	  if (abs_inc != 1)
	    {
	      int cmp_const;

	      if (neg_inc)
		cmp_const = abs_inc - 1;
	      else
		cmp_const = abs_inc * (unroll_number - 1) + 1;

	      emit_cmp_insn (diff, GEN_INT (cmp_const), EQ, NULL_RTX,
			     mode, 0, 0);

	      if (neg_inc)
		emit_jump_insn (gen_ble (labels[0]));
	      else
		emit_jump_insn (gen_bge (labels[0]));
	      JUMP_LABEL (get_last_insn ()) = labels[0];
	      LABEL_NUSES (labels[0])++;
	    }

	  sequence = gen_sequence ();
	  end_sequence ();
	  emit_insn_before (sequence, loop_start);
	  
	  /* Only the last copy of the loop body here needs the exit
	     test, so set copy_end to exclude the compare/branch here,
	     and then reset it inside the loop when get to the last
	     copy.  */

	  if (GET_CODE (last_loop_insn) == BARRIER)
	    copy_end = PREV_INSN (PREV_INSN (last_loop_insn));
	  else if (GET_CODE (last_loop_insn) == JUMP_INSN)
	    {
#ifdef HAVE_cc0
	      /* The immediately preceding insn is a compare which we do not
		 want to copy.  */
	      copy_end = PREV_INSN (PREV_INSN (last_loop_insn));
#else
	      /* The immediately preceding insn may not be a compare, so we
		 must copy it.  */
	      copy_end = PREV_INSN (last_loop_insn);
#endif
	    }
	  else
	    abort ();

	  for (i = 1; i < unroll_number; i++)
	    {
	      emit_label_after (labels[unroll_number - i],
				PREV_INSN (loop_start));

	      bzero (map->insn_map, max_insnno * sizeof (rtx));
	      bzero (map->const_equiv_map, maxregnum * sizeof (rtx));
	      bzero (map->const_age_map, maxregnum * sizeof (unsigned));
	      map->const_age = 0;

	      for (j = 0; j < max_labelno; j++)
		if (local_label[j])
		  map->label_map[j] = gen_label_rtx ();

	      /* The last copy needs the compare/branch insns at the end,
		 so reset copy_end here if the loop ends with a conditional
		 branch.  */

	      if (i == unroll_number - 1)
		{
		  if (GET_CODE (last_loop_insn) == BARRIER)
		    copy_end = PREV_INSN (PREV_INSN (last_loop_insn));
		  else
		    copy_end = last_loop_insn;
		}

	      /* None of the copies are the `last_iteration', so just
		 pass zero for that parameter.  */
	      copy_loop_body (copy_start, copy_end, map, exit_label, 0,
			      unroll_type, start_label, loop_end,
			      loop_start, copy_end);
	    }
	  emit_label_after (labels[0], PREV_INSN (loop_start));

	  if (GET_CODE (last_loop_insn) == BARRIER)
	    {
	      insert_before = PREV_INSN (last_loop_insn);
	      copy_end = PREV_INSN (insert_before);
	    }
	  else
	    {
#ifdef HAVE_cc0
	      /* The immediately preceding insn is a compare which we do not
		 want to copy.  */
	      insert_before = PREV_INSN (last_loop_insn);
	      copy_end = PREV_INSN (insert_before);
#else
	      /* The immediately preceding insn may not be a compare, so we
		 must copy it.  */
	      insert_before = last_loop_insn;
	      copy_end = PREV_INSN (last_loop_insn);
#endif
	    }

	  /* Set unroll type to MODULO now.  */
	  unroll_type = UNROLL_MODULO;
	  loop_preconditioned = 1;
	}
    }

  /* If reach here, and the loop type is UNROLL_NAIVE, then don't unroll
     the loop unless all loops are being unrolled.  */
  if (unroll_type == UNROLL_NAIVE && ! flag_unroll_all_loops)
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream, "Unrolling failure: Naive unrolling not being done.\n");
      return;
    }

  /* At this point, we are guaranteed to unroll the loop.  */

  /* For each biv and giv, determine whether it can be safely split into
     a different variable for each unrolled copy of the loop body.
     We precalculate and save this info here, since computing it is
     expensive.

     Do this before deleting any instructions from the loop, so that
     back_branch_in_range_p will work correctly.  */

  if (splitting_not_safe)
    temp = 0;
  else
    temp = find_splittable_regs (unroll_type, loop_start, loop_end,
				end_insert_before, unroll_number);

  /* find_splittable_regs may have created some new registers, so must
     reallocate the reg_map with the new larger size, and must realloc
     the constant maps also.  */

  maxregnum = max_reg_num ();
  map->reg_map = (rtx *) alloca (maxregnum * sizeof (rtx));

  init_reg_map (map, maxregnum);

  /* Space is needed in some of the map for new registers, so new_maxregnum
     is an (over)estimate of how many registers will exist at the end.  */
  new_maxregnum = maxregnum + (temp * unroll_number * 2);

  /* Must realloc space for the constant maps, because the number of registers
     may have changed.  */

  map->const_equiv_map = (rtx *) alloca (new_maxregnum * sizeof (rtx));
  map->const_age_map = (unsigned *) alloca (new_maxregnum * sizeof (unsigned));

  global_const_equiv_map = map->const_equiv_map;

  /* Search the list of bivs and givs to find ones which need to be remapped
     when split, and set their reg_map entry appropriately.  */

  for (bl = loop_iv_list; bl; bl = bl->next)
    {
      if (REGNO (bl->biv->src_reg) != bl->regno)
	map->reg_map[bl->regno] = bl->biv->src_reg;
#if 0
      /* Currently, non-reduced/final-value givs are never split.  */
      for (v = bl->giv; v; v = v->next_iv)
	if (REGNO (v->src_reg) != bl->regno)
	  map->reg_map[REGNO (v->dest_reg)] = v->src_reg;
#endif
    }

  /* If the loop is being partially unrolled, and the iteration variables
     are being split, and are being renamed for the split, then must fix up
     the compare instruction at the end of the loop to refer to the new
     registers.  This compare isn't copied, so the registers used in it
     will never be replaced if it isn't done here.  */

  if (unroll_type == UNROLL_MODULO)
    {
      insn = NEXT_INSN (copy_end);
      if (GET_CODE (insn) == INSN && GET_CODE (PATTERN (insn)) == SET)
	{
#if 0
	  /* If non-reduced/final-value givs were split, then this would also
	     have to remap those givs.  */
#endif

	  tem = SET_SRC (PATTERN (insn));
	  /* The set source is a register.  */
	  if (GET_CODE (tem) == REG)
	    {
	      if (REGNO (tem) < max_reg_before_loop
		  && reg_iv_type[REGNO (tem)] == BASIC_INDUCT)
		SET_SRC (PATTERN (insn))
		  = reg_biv_class[REGNO (tem)]->biv->src_reg;
	    }
	  else
	    {
	      /* The set source is a compare of some sort.  */
	      tem = XEXP (SET_SRC (PATTERN (insn)), 0);
	      if (GET_CODE (tem) == REG
		  && REGNO (tem) < max_reg_before_loop
		  && reg_iv_type[REGNO (tem)] == BASIC_INDUCT)
		XEXP (SET_SRC (PATTERN (insn)), 0)
		  = reg_biv_class[REGNO (tem)]->biv->src_reg;
	      
	      tem = XEXP (SET_SRC (PATTERN (insn)), 1);
	      if (GET_CODE (tem) == REG
		  && REGNO (tem) < max_reg_before_loop
		  && reg_iv_type[REGNO (tem)] == BASIC_INDUCT)
		XEXP (SET_SRC (PATTERN (insn)), 1)
		  = reg_biv_class[REGNO (tem)]->biv->src_reg;
	    }
	}
    }

  /* For unroll_number - 1 times, make a copy of each instruction
     between copy_start and copy_end, and insert these new instructions
     before the end of the loop.  */

  for (i = 0; i < unroll_number; i++)
    {
      bzero (map->insn_map, max_insnno * sizeof (rtx));
      bzero (map->const_equiv_map, new_maxregnum * sizeof (rtx));
      bzero (map->const_age_map, new_maxregnum * sizeof (unsigned));
      map->const_age = 0;

      for (j = 0; j < max_labelno; j++)
	if (local_label[j])
	  map->label_map[j] = gen_label_rtx ();

      /* If loop starts with a branch to the test, then fix it so that
	 it points to the test of the first unrolled copy of the loop.  */
      if (i == 0 && loop_start != copy_start)
	{
	  insn = PREV_INSN (copy_start);
	  pattern = PATTERN (insn);
	  
	  tem = map->label_map[CODE_LABEL_NUMBER
			       (XEXP (SET_SRC (pattern), 0))];
	  SET_SRC (pattern) = gen_rtx (LABEL_REF, VOIDmode, tem);

	  /* Set the jump label so that it can be used by later loop unrolling
	     passes.  */
	  JUMP_LABEL (insn) = tem;
	  LABEL_NUSES (tem)++;
	}

      copy_loop_body (copy_start, copy_end, map, exit_label,
		      i == unroll_number - 1, unroll_type, start_label,
		      loop_end, insert_before, insert_before);
    }

  /* Before deleting any insns, emit a CODE_LABEL immediately after the last
     insn to be deleted.  This prevents any runaway delete_insn call from
     more insns that it should, as it always stops at a CODE_LABEL.  */

  /* Delete the compare and branch at the end of the loop if completely
     unrolling the loop.  Deleting the backward branch at the end also
     deletes the code label at the start of the loop.  This is done at
     the very end to avoid problems with back_branch_in_range_p.  */

  if (unroll_type == UNROLL_COMPLETELY)
    safety_label = emit_label_after (gen_label_rtx (), last_loop_insn);
  else
    safety_label = emit_label_after (gen_label_rtx (), copy_end);

  /* Delete all of the original loop instructions.  Don't delete the 
     LOOP_BEG note, or the first code label in the loop.  */

  insn = NEXT_INSN (copy_start);
  while (insn != safety_label)
    {
      if (insn != start_label)
	insn = delete_insn (insn);
      else
	insn = NEXT_INSN (insn);
    }

  /* Can now delete the 'safety' label emitted to protect us from runaway
     delete_insn calls.  */
  if (INSN_DELETED_P (safety_label))
    abort ();
  delete_insn (safety_label);

  /* If exit_label exists, emit it after the loop.  Doing the emit here
     forces it to have a higher INSN_UID than any insn in the unrolled loop.
     This is needed so that mostly_true_jump in reorg.c will treat jumps
     to this loop end label correctly, i.e. predict that they are usually
     not taken.  */
  if (exit_label)
    emit_label_after (exit_label, loop_end);
}

/* Return true if the loop can be safely, and profitably, preconditioned
   so that the unrolled copies of the loop body don't need exit tests.

   This only works if final_value, initial_value and increment can be
   determined, and if increment is a constant power of 2.
   If increment is not a power of 2, then the preconditioning modulo
   operation would require a real modulo instead of a boolean AND, and this
   is not considered `profitable'.  */

/* ??? If the loop is known to be executed very many times, or the machine
   has a very cheap divide instruction, then preconditioning is a win even
   when the increment is not a power of 2.  Use RTX_COST to compute
   whether divide is cheap.  */

static int
precondition_loop_p (initial_value, final_value, increment, loop_start,
		     loop_end)
     rtx *initial_value, *final_value, *increment;
     rtx loop_start, loop_end;
{
  int unsigned_compare, compare_dir;

  if (loop_n_iterations > 0)
    {
      *initial_value = const0_rtx;
      *increment = const1_rtx;
      *final_value = GEN_INT (loop_n_iterations);

      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Preconditioning: Success, number of iterations known, %d.\n",
		 loop_n_iterations);
      return 1;
    }

  if (loop_initial_value == 0)
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Preconditioning: Could not find initial value.\n");
      return 0;
    }
  else if (loop_increment == 0)
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Preconditioning: Could not find increment value.\n");
      return 0;
    }
  else if (GET_CODE (loop_increment) != CONST_INT)
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Preconditioning: Increment not a constant.\n");
      return 0;
    }
  else if ((exact_log2 (INTVAL (loop_increment)) < 0)
	   && (exact_log2 (- INTVAL (loop_increment)) < 0))
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Preconditioning: Increment not a constant power of 2.\n");
      return 0;
    }

  /* Unsigned_compare and compare_dir can be ignored here, since they do
     not matter for preconditioning.  */

  if (loop_final_value == 0)
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Preconditioning: EQ comparison loop.\n");
      return 0;
    }

  /* Must ensure that final_value is invariant, so call invariant_p to
     check.  Before doing so, must check regno against max_reg_before_loop
     to make sure that the register is in the range covered by invariant_p.
     If it isn't, then it is most likely a biv/giv which by definition are
     not invariant.  */
  if ((GET_CODE (loop_final_value) == REG
       && REGNO (loop_final_value) >= max_reg_before_loop)
      || (GET_CODE (loop_final_value) == PLUS
	  && REGNO (XEXP (loop_final_value, 0)) >= max_reg_before_loop)
      || ! invariant_p (loop_final_value))
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Preconditioning: Final value not invariant.\n");
      return 0;
    }

  /* Fail for floating point values, since the caller of this function
     does not have code to deal with them.  */
  if (GET_MODE_CLASS (GET_MODE (loop_final_value)) == MODE_FLOAT
      || GET_MODE_CLASS (GET_MODE (loop_initial_value)) == MODE_FLOAT)
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Preconditioning: Floating point final or initial value.\n");
      return 0;
    }

  /* Now set initial_value to be the iteration_var, since that may be a
     simpler expression, and is guaranteed to be correct if all of the
     above tests succeed.

     We can not use the initial_value as calculated, because it will be
     one too small for loops of the form "while (i-- > 0)".  We can not
     emit code before the loop_skip_over insns to fix this problem as this
     will then give a number one too large for loops of the form
     "while (--i > 0)".

     Note that all loops that reach here are entered at the top, because
     this function is not called if the loop starts with a jump.  */

  /* Fail if loop_iteration_var is not live before loop_start, since we need
     to test its value in the preconditioning code.  */

  if (uid_luid[regno_first_uid[REGNO (loop_iteration_var)]]
      > INSN_LUID (loop_start))
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Preconditioning: Iteration var not live before loop start.\n");
      return 0;
    }

  *initial_value = loop_iteration_var;
  *increment = loop_increment;
  *final_value = loop_final_value;

  /* Success! */
  if (loop_dump_stream)
    fprintf (loop_dump_stream, "Preconditioning: Successful.\n");
  return 1;
}


/* All pseudo-registers must be mapped to themselves.  Two hard registers
   must be mapped, VIRTUAL_STACK_VARS_REGNUM and VIRTUAL_INCOMING_ARGS_
   REGNUM, to avoid function-inlining specific conversions of these
   registers.  All other hard regs can not be mapped because they may be
   used with different
   modes.  */

static void
init_reg_map (map, maxregnum)
     struct inline_remap *map;
     int maxregnum;
{
  int i;

  for (i = maxregnum - 1; i > LAST_VIRTUAL_REGISTER; i--)
    map->reg_map[i] = regno_reg_rtx[i];
  /* Just clear the rest of the entries.  */
  for (i = LAST_VIRTUAL_REGISTER; i >= 0; i--)
    map->reg_map[i] = 0;

  map->reg_map[VIRTUAL_STACK_VARS_REGNUM]
    = regno_reg_rtx[VIRTUAL_STACK_VARS_REGNUM];
  map->reg_map[VIRTUAL_INCOMING_ARGS_REGNUM]
    = regno_reg_rtx[VIRTUAL_INCOMING_ARGS_REGNUM];
}

/* Strength-reduction will often emit code for optimized biv/givs which
   calculates their value in a temporary register, and then copies the result
   to the iv.  This procedure reconstructs the pattern computing the iv;
   verifying that all operands are of the proper form.

   The return value is the amount that the giv is incremented by.  */

static rtx
calculate_giv_inc (pattern, src_insn, regno)
     rtx pattern, src_insn;
     int regno;
{
  rtx increment;
  rtx increment_total = 0;
  int tries = 0;

 retry:
  /* Verify that we have an increment insn here.  First check for a plus
     as the set source.  */
  if (GET_CODE (SET_SRC (pattern)) != PLUS)
    {
      /* SR sometimes computes the new giv value in a temp, then copies it
	 to the new_reg.  */
      src_insn = PREV_INSN (src_insn);
      pattern = PATTERN (src_insn);
      if (GET_CODE (SET_SRC (pattern)) != PLUS)
	abort ();
		  
      /* The last insn emitted is not needed, so delete it to avoid confusing
	 the second cse pass.  This insn sets the giv unnecessarily.  */
      delete_insn (get_last_insn ());
    }

  /* Verify that we have a constant as the second operand of the plus.  */
  increment = XEXP (SET_SRC (pattern), 1);
  if (GET_CODE (increment) != CONST_INT)
    {
      /* SR sometimes puts the constant in a register, especially if it is
	 too big to be an add immed operand.  */
      src_insn = PREV_INSN (src_insn);
      increment = SET_SRC (PATTERN (src_insn));

      /* SR may have used LO_SUM to compute the constant if it is too large
	 for a load immed operand.  In this case, the constant is in operand
	 one of the LO_SUM rtx.  */
      if (GET_CODE (increment) == LO_SUM)
	increment = XEXP (increment, 1);

      if (GET_CODE (increment) != CONST_INT)
	abort ();
		  
      /* The insn loading the constant into a register is not longer needed,
	 so delete it.  */
      delete_insn (get_last_insn ());
    }

  if (increment_total)
    increment_total = GEN_INT (INTVAL (increment_total) + INTVAL (increment));
  else
    increment_total = increment;

  /* Check that the source register is the same as the register we expected
     to see as the source.  If not, something is seriously wrong.  */
  if (GET_CODE (XEXP (SET_SRC (pattern), 0)) != REG
      || REGNO (XEXP (SET_SRC (pattern), 0)) != regno)
    {
      /* Some machines (e.g. the romp), may emit two add instructions for
	 certain constants, so lets try looking for another add immediately
	 before this one if we have only seen one add insn so far.  */

      if (tries == 0)
	{
	  tries++;

	  src_insn = PREV_INSN (src_insn);
	  pattern = PATTERN (src_insn);

	  delete_insn (get_last_insn ());

	  goto retry;
	}

      abort ();
    }

  return increment_total;
}

/* Copy REG_NOTES, except for insn references, because not all insn_map
   entries are valid yet.  We do need to copy registers now though, because
   the reg_map entries can change during copying.  */

static rtx
initial_reg_note_copy (notes, map)
     rtx notes;
     struct inline_remap *map;
{
  rtx copy;

  if (notes == 0)
    return 0;

  copy = rtx_alloc (GET_CODE (notes));
  PUT_MODE (copy, GET_MODE (notes));

  if (GET_CODE (notes) == EXPR_LIST)
    XEXP (copy, 0) = copy_rtx_and_substitute (XEXP (notes, 0), map);
  else if (GET_CODE (notes) == INSN_LIST)
    /* Don't substitute for these yet.  */
    XEXP (copy, 0) = XEXP (notes, 0);
  else
    abort ();

  XEXP (copy, 1) = initial_reg_note_copy (XEXP (notes, 1), map);

  return copy;
}

/* Fixup insn references in copied REG_NOTES.  */

static void
final_reg_note_copy (notes, map)
     rtx notes;
     struct inline_remap *map;
{
  rtx note;

  for (note = notes; note; note = XEXP (note, 1))
    if (GET_CODE (note) == INSN_LIST)
      XEXP (note, 0) = map->insn_map[INSN_UID (XEXP (note, 0))];
}

/* Copy each instruction in the loop, substituting from map as appropriate.
   This is very similar to a loop in expand_inline_function.  */
  
static void
copy_loop_body (copy_start, copy_end, map, exit_label, last_iteration,
		unroll_type, start_label, loop_end, insert_before,
		copy_notes_from)
     rtx copy_start, copy_end;
     struct inline_remap *map;
     rtx exit_label;
     int last_iteration;
     enum unroll_types unroll_type;
     rtx start_label, loop_end, insert_before, copy_notes_from;
{
  rtx insn, pattern;
  rtx tem, copy;
  int dest_reg_was_split, i;
  rtx cc0_insn = 0;
  rtx final_label = 0;
  rtx giv_inc, giv_dest_reg, giv_src_reg;

  /* If this isn't the last iteration, then map any references to the
     start_label to final_label.  Final label will then be emitted immediately
     after the end of this loop body if it was ever used.

     If this is the last iteration, then map references to the start_label
     to itself.  */
  if (! last_iteration)
    {
      final_label = gen_label_rtx ();
      map->label_map[CODE_LABEL_NUMBER (start_label)] = final_label;
    }
  else
    map->label_map[CODE_LABEL_NUMBER (start_label)] = start_label;

  start_sequence ();
  
  insn = copy_start;
  do
    {
      insn = NEXT_INSN (insn);
      
      map->orig_asm_operands_vector = 0;
      
      switch (GET_CODE (insn))
	{
	case INSN:
	  pattern = PATTERN (insn);
	  copy = 0;
	  giv_inc = 0;
	  
	  /* Check to see if this is a giv that has been combined with
	     some split address givs.  (Combined in the sense that 
	     `combine_givs' in loop.c has put two givs in the same register.)
	     In this case, we must search all givs based on the same biv to
	     find the address givs.  Then split the address givs.
	     Do this before splitting the giv, since that may map the
	     SET_DEST to a new register.  */
	  
	  if (GET_CODE (pattern) == SET
	      && GET_CODE (SET_DEST (pattern)) == REG
	      && addr_combined_regs[REGNO (SET_DEST (pattern))])
	    {
	      struct iv_class *bl;
	      struct induction *v, *tv;
	      int regno = REGNO (SET_DEST (pattern));
	      
	      v = addr_combined_regs[REGNO (SET_DEST (pattern))];
	      bl = reg_biv_class[REGNO (v->src_reg)];
	      
	      /* Although the giv_inc amount is not needed here, we must call
		 calculate_giv_inc here since it might try to delete the
		 last insn emitted.  If we wait until later to call it,
		 we might accidentally delete insns generated immediately
		 below by emit_unrolled_add.  */

	      giv_inc = calculate_giv_inc (pattern, insn, regno);

	      /* Now find all address giv's that were combined with this
		 giv 'v'.  */
	      for (tv = bl->giv; tv; tv = tv->next_iv)
		if (tv->giv_type == DEST_ADDR && tv->same == v)
		  {
		    int this_giv_inc = INTVAL (giv_inc);

		    /* Scale this_giv_inc if the multiplicative factors of
		       the two givs are different.  */
		    if (tv->mult_val != v->mult_val)
		      this_giv_inc = (this_giv_inc / INTVAL (v->mult_val)
				      * INTVAL (tv->mult_val));
		       
		    tv->dest_reg = plus_constant (tv->dest_reg, this_giv_inc);
		    *tv->location = tv->dest_reg;
		    
		    if (last_iteration && unroll_type != UNROLL_COMPLETELY)
		      {
			/* Must emit an insn to increment the split address
			   giv.  Add in the const_adjust field in case there
			   was a constant eliminated from the address.  */
			rtx value, dest_reg;
			
			/* tv->dest_reg will be either a bare register,
			   or else a register plus a constant.  */
			if (GET_CODE (tv->dest_reg) == REG)
			  dest_reg = tv->dest_reg;
			else
			  dest_reg = XEXP (tv->dest_reg, 0);
			
			/* tv->dest_reg may actually be a (PLUS (REG) (CONST))
			   here, so we must call plus_constant to add
			   the const_adjust amount before calling
			   emit_unrolled_add below.  */
			value = plus_constant (tv->dest_reg, tv->const_adjust);

			/* The constant could be too large for an add
			   immediate, so can't directly emit an insn here.  */
			emit_unrolled_add (dest_reg, XEXP (value, 0),
					   XEXP (value, 1));
			
			/* Reset the giv to be just the register again, in case
			   it is used after the set we have just emitted.
			   We must subtract the const_adjust factor added in
			   above.  */
			tv->dest_reg = plus_constant (dest_reg,
						      - tv->const_adjust);
			*tv->location = tv->dest_reg;
		      }
		  }
	    }
	  
	  /* If this is a setting of a splittable variable, then determine
	     how to split the variable, create a new set based on this split,
	     and set up the reg_map so that later uses of the variable will
	     use the new split variable.  */
	  
	  dest_reg_was_split = 0;
	  
	  if (GET_CODE (pattern) == SET
	      && GET_CODE (SET_DEST (pattern)) == REG
	      && splittable_regs[REGNO (SET_DEST (pattern))])
	    {
	      int regno = REGNO (SET_DEST (pattern));
	      
	      dest_reg_was_split = 1;
	      
	      /* Compute the increment value for the giv, if it wasn't
		 already computed above.  */

	      if (giv_inc == 0)
		giv_inc = calculate_giv_inc (pattern, insn, regno);
	      giv_dest_reg = SET_DEST (pattern);
	      giv_src_reg = SET_DEST (pattern);

	      if (unroll_type == UNROLL_COMPLETELY)
		{
		  /* Completely unrolling the loop.  Set the induction
		     variable to a known constant value.  */
		  
		  /* The value in splittable_regs may be an invariant
		     value, so we must use plus_constant here.  */
		  splittable_regs[regno]
		    = plus_constant (splittable_regs[regno], INTVAL (giv_inc));

		  if (GET_CODE (splittable_regs[regno]) == PLUS)
		    {
		      giv_src_reg = XEXP (splittable_regs[regno], 0);
		      giv_inc = XEXP (splittable_regs[regno], 1);
		    }
		  else
		    {
		      /* The splittable_regs value must be a REG or a
			 CONST_INT, so put the entire value in the giv_src_reg
			 variable.  */
		      giv_src_reg = splittable_regs[regno];
		      giv_inc = const0_rtx;
		    }
		}
	      else
		{
		  /* Partially unrolling loop.  Create a new pseudo
		     register for the iteration variable, and set it to
		     be a constant plus the original register.  Except
		     on the last iteration, when the result has to
		     go back into the original iteration var register.  */
		  
		  /* Handle bivs which must be mapped to a new register
		     when split.  This happens for bivs which need their
		     final value set before loop entry.  The new register
		     for the biv was stored in the biv's first struct
		     induction entry by find_splittable_regs.  */

		  if (regno < max_reg_before_loop
		      && reg_iv_type[regno] == BASIC_INDUCT)
		    {
		      giv_src_reg = reg_biv_class[regno]->biv->src_reg;
		      giv_dest_reg = giv_src_reg;
		    }
		  
#if 0
		  /* If non-reduced/final-value givs were split, then
		     this would have to remap those givs also.  See
		     find_splittable_regs.  */
#endif
		  
		  splittable_regs[regno]
		    = GEN_INT (INTVAL (giv_inc)
			       + INTVAL (splittable_regs[regno]));
		  giv_inc = splittable_regs[regno];
		  
		  /* Now split the induction variable by changing the dest
		     of this insn to a new register, and setting its
		     reg_map entry to point to this new register.

		     If this is the last iteration, and this is the last insn
		     that will update the iv, then reuse the original dest,
		     to ensure that the iv will have the proper value when
		     the loop exits or repeats.

		     Using splittable_regs_updates here like this is safe,
		     because it can only be greater than one if all
		     instructions modifying the iv are always executed in
		     order.  */

		  if (! last_iteration
		      || (splittable_regs_updates[regno]-- != 1))
		    {
		      tem = gen_reg_rtx (GET_MODE (giv_src_reg));
		      giv_dest_reg = tem;
		      map->reg_map[regno] = tem;
		    }
		  else
		    map->reg_map[regno] = giv_src_reg;
		}

	      /* The constant being added could be too large for an add
		 immediate, so can't directly emit an insn here.  */
	      emit_unrolled_add (giv_dest_reg, giv_src_reg, giv_inc);
	      copy = get_last_insn ();
	      pattern = PATTERN (copy);
	    }
	  else
	    {
	      pattern = copy_rtx_and_substitute (pattern, map);
	      copy = emit_insn (pattern);
	    }
	  REG_NOTES (copy) = initial_reg_note_copy (REG_NOTES (insn), map);
	  
#ifdef HAVE_cc0
	  /* If this insn is setting CC0, it may need to look at
	     the insn that uses CC0 to see what type of insn it is.
	     In that case, the call to recog via validate_change will
	     fail.  So don't substitute constants here.  Instead,
	     do it when we emit the following insn.

	     For example, see the pyr.md file.  That machine has signed and
	     unsigned compares.  The compare patterns must check the
	     following branch insn to see which what kind of compare to
	     emit.

	     If the previous insn set CC0, substitute constants on it as
	     well.  */
	  if (sets_cc0_p (copy) != 0)
	    cc0_insn = copy;
	  else
	    {
	      if (cc0_insn)
		try_constants (cc0_insn, map);
	      cc0_insn = 0;
	      try_constants (copy, map);
	    }
#else
	  try_constants (copy, map);
#endif

	  /* Make split induction variable constants `permanent' since we
	     know there are no backward branches across iteration variable
	     settings which would invalidate this.  */
	  if (dest_reg_was_split)
	    {
	      int regno = REGNO (SET_DEST (pattern));

	      if (map->const_age_map[regno] == map->const_age)
		map->const_age_map[regno] = -1;
	    }
	  break;
	  
	case JUMP_INSN:
	  pattern = copy_rtx_and_substitute (PATTERN (insn), map);
	  copy = emit_jump_insn (pattern);
	  REG_NOTES (copy) = initial_reg_note_copy (REG_NOTES (insn), map);

	  if (JUMP_LABEL (insn) == start_label && insn == copy_end
	      && ! last_iteration)
	    {
	      /* This is a branch to the beginning of the loop; this is the
		 last insn being copied; and this is not the last iteration.
		 In this case, we want to change the original fall through
		 case to be a branch past the end of the loop, and the
		 original jump label case to fall_through.  */

	      if (! invert_exp (pattern, copy)
		  || ! redirect_exp (&pattern,
				     map->label_map[CODE_LABEL_NUMBER
						    (JUMP_LABEL (insn))],
				     exit_label, copy))
		abort ();
	    }
	  
#ifdef HAVE_cc0
	  if (cc0_insn)
	    try_constants (cc0_insn, map);
	  cc0_insn = 0;
#endif
	  try_constants (copy, map);

	  /* Set the jump label of COPY correctly to avoid problems with
	     later passes of unroll_loop, if INSN had jump label set.  */
	  if (JUMP_LABEL (insn))
	    {
	      rtx label = 0;

	      /* Can't use the label_map for every insn, since this may be
		 the backward branch, and hence the label was not mapped.  */
	      if (GET_CODE (pattern) == SET)
		{
		  tem = SET_SRC (pattern);
		  if (GET_CODE (tem) == LABEL_REF)
		    label = XEXP (tem, 0);
		  else if (GET_CODE (tem) == IF_THEN_ELSE)
		    {
		      if (XEXP (tem, 1) != pc_rtx)
			label = XEXP (XEXP (tem, 1), 0);
		      else
			label = XEXP (XEXP (tem, 2), 0);
		    }
		}

	      if (label && GET_CODE (label) == CODE_LABEL)
		JUMP_LABEL (copy) = label;
	      else
		{
		  /* An unrecognizable jump insn, probably the entry jump
		     for a switch statement.  This label must have been mapped,
		     so just use the label_map to get the new jump label.  */
		  JUMP_LABEL (copy) = map->label_map[CODE_LABEL_NUMBER
						     (JUMP_LABEL (insn))];
		}
	  
	      /* If this is a non-local jump, then must increase the label
		 use count so that the label will not be deleted when the
		 original jump is deleted.  */
	      LABEL_NUSES (JUMP_LABEL (copy))++;
	    }
	  else if (GET_CODE (PATTERN (copy)) == ADDR_VEC
		   || GET_CODE (PATTERN (copy)) == ADDR_DIFF_VEC)
	    {
	      rtx pat = PATTERN (copy);
	      int diff_vec_p = GET_CODE (pat) == ADDR_DIFF_VEC;
	      int len = XVECLEN (pat, diff_vec_p);
	      int i;

	      for (i = 0; i < len; i++)
		LABEL_NUSES (XEXP (XVECEXP (pat, diff_vec_p, i), 0))++;
	    }

	  /* If this used to be a conditional jump insn but whose branch
	     direction is now known, we must do something special.  */
	  if (condjump_p (insn) && !simplejump_p (insn) && map->last_pc_value)
	    {
#ifdef HAVE_cc0
	      /* The previous insn set cc0 for us.  So delete it.  */
	      delete_insn (PREV_INSN (copy));
#endif

	      /* If this is now a no-op, delete it.  */
	      if (map->last_pc_value == pc_rtx)
		{
		  delete_insn (copy);
		  copy = 0;
		}
	      else
		/* Otherwise, this is unconditional jump so we must put a
		   BARRIER after it.  We could do some dead code elimination
		   here, but jump.c will do it just as well.  */
		emit_barrier ();
	    }
	  break;
	  
	case CALL_INSN:
	  pattern = copy_rtx_and_substitute (PATTERN (insn), map);
	  copy = emit_call_insn (pattern);
	  REG_NOTES (copy) = initial_reg_note_copy (REG_NOTES (insn), map);

#ifdef HAVE_cc0
	  if (cc0_insn)
	    try_constants (cc0_insn, map);
	  cc0_insn = 0;
#endif
	  try_constants (copy, map);

	  /* Be lazy and assume CALL_INSNs clobber all hard registers.  */
	  for (i = 0; i < FIRST_PSEUDO_REGISTER; i++)
	    map->const_equiv_map[i] = 0;
	  break;
	  
	case CODE_LABEL:
	  /* If this is the loop start label, then we don't need to emit a
	     copy of this label since no one will use it.  */

	  if (insn != start_label)
	    {
	      copy = emit_label (map->label_map[CODE_LABEL_NUMBER (insn)]);
	      map->const_age++;
	    }
	  break;
	  
	case BARRIER:
	  copy = emit_barrier ();
	  break;
	  
	case NOTE:
	  /* VTOP notes are valid only before the loop exit test.  If placed
	     anywhere else, loop may generate bad code.  */
	     
	  if (NOTE_LINE_NUMBER (insn) != NOTE_INSN_DELETED
	      && (NOTE_LINE_NUMBER (insn) != NOTE_INSN_LOOP_VTOP
		  || (last_iteration && unroll_type != UNROLL_COMPLETELY)))
	    copy = emit_note (NOTE_SOURCE_FILE (insn),
			      NOTE_LINE_NUMBER (insn));
	  else
	    copy = 0;
	  break;
	  
	default:
	  abort ();
	  break;
	}
      
      map->insn_map[INSN_UID (insn)] = copy;
    }
  while (insn != copy_end);
  
  /* Now finish coping the REG_NOTES.  */
  insn = copy_start;
  do
    {
      insn = NEXT_INSN (insn);
      if ((GET_CODE (insn) == INSN || GET_CODE (insn) == JUMP_INSN
	   || GET_CODE (insn) == CALL_INSN)
	  && map->insn_map[INSN_UID (insn)])
	final_reg_note_copy (REG_NOTES (map->insn_map[INSN_UID (insn)]), map);
    }
  while (insn != copy_end);

  /* There may be notes between copy_notes_from and loop_end.  Emit a copy of
     each of these notes here, since there may be some important ones, such as
     NOTE_INSN_BLOCK_END notes, in this group.  We don't do this on the last
     iteration, because the original notes won't be deleted.

     We can't use insert_before here, because when from preconditioning,
     insert_before points before the loop.  We can't use copy_end, because
     there may be insns already inserted after it (which we don't want to
     copy) when not from preconditioning code.  */

  if (! last_iteration)
    {
      for (insn = copy_notes_from; insn != loop_end; insn = NEXT_INSN (insn))
	{
	  if (GET_CODE (insn) == NOTE
	      && NOTE_LINE_NUMBER (insn) != NOTE_INSN_DELETED)
	    emit_note (NOTE_SOURCE_FILE (insn), NOTE_LINE_NUMBER (insn));
	}
    }

  if (final_label && LABEL_NUSES (final_label) > 0)
    emit_label (final_label);

  tem = gen_sequence ();
  end_sequence ();
  emit_insn_before (tem, insert_before);
}

/* Emit an insn, using the expand_binop to ensure that a valid insn is
   emitted.  This will correctly handle the case where the increment value
   won't fit in the immediate field of a PLUS insns.  */

void
emit_unrolled_add (dest_reg, src_reg, increment)
     rtx dest_reg, src_reg, increment;
{
  rtx result;

  result = expand_binop (GET_MODE (dest_reg), add_optab, src_reg, increment,
			 dest_reg, 0, OPTAB_LIB_WIDEN);

  if (dest_reg != result)
    emit_move_insn (dest_reg, result);
}

/* Searches the insns between INSN and LOOP_END.  Returns 1 if there
   is a backward branch in that range that branches to somewhere between
   LOOP_START and INSN.  Returns 0 otherwise.  */

/* ??? This is quadratic algorithm.  Could be rewritten to be linear.
   In practice, this is not a problem, because this function is seldom called,
   and uses a negligible amount of CPU time on average.  */

static int
back_branch_in_range_p (insn, loop_start, loop_end)
     rtx insn;
     rtx loop_start, loop_end;
{
  rtx p, q, target_insn;

  /* Stop before we get to the backward branch at the end of the loop.  */
  loop_end = prev_nonnote_insn (loop_end);
  if (GET_CODE (loop_end) == BARRIER)
    loop_end = PREV_INSN (loop_end);

  /* Check in case insn has been deleted, search forward for first non
     deleted insn following it.  */
  while (INSN_DELETED_P (insn))
    insn = NEXT_INSN (insn);

  /* Check for the case where insn is the last insn in the loop.  */
  if (insn == loop_end)
    return 0;

  for (p = NEXT_INSN (insn); p != loop_end; p = NEXT_INSN (p))
    {
      if (GET_CODE (p) == JUMP_INSN)
	{
	  target_insn = JUMP_LABEL (p);
	  
	  /* Search from loop_start to insn, to see if one of them is
	     the target_insn.  We can't use INSN_LUID comparisons here,
	     since insn may not have an LUID entry.  */
	  for (q = loop_start; q != insn; q = NEXT_INSN (q))
	    if (q == target_insn)
	      return 1;
	}
    }

  return 0;
}

/* Try to generate the simplest rtx for the expression
   (PLUS (MULT mult1 mult2) add1).  This is used to calculate the initial
   value of giv's.  */

static rtx
fold_rtx_mult_add (mult1, mult2, add1, mode)
     rtx mult1, mult2, add1;
     enum machine_mode mode;
{
  rtx temp, mult_res;
  rtx result;

  /* The modes must all be the same.  This should always be true.  For now,
     check to make sure.  */
  if ((GET_MODE (mult1) != mode && GET_MODE (mult1) != VOIDmode)
      || (GET_MODE (mult2) != mode && GET_MODE (mult2) != VOIDmode)
      || (GET_MODE (add1) != mode && GET_MODE (add1) != VOIDmode))
    abort ();

  /* Ensure that if at least one of mult1/mult2 are constant, then mult2
     will be a constant.  */
  if (GET_CODE (mult1) == CONST_INT)
    {
      temp = mult2;
      mult2 = mult1;
      mult1 = temp;
    }

  mult_res = simplify_binary_operation (MULT, mode, mult1, mult2);
  if (! mult_res)
    mult_res = gen_rtx (MULT, mode, mult1, mult2);

  /* Again, put the constant second.  */
  if (GET_CODE (add1) == CONST_INT)
    {
      temp = add1;
      add1 = mult_res;
      mult_res = temp;
    }

  result = simplify_binary_operation (PLUS, mode, add1, mult_res);
  if (! result)
    result = gen_rtx (PLUS, mode, add1, mult_res);

  return result;
}

/* Searches the list of induction struct's for the biv BL, to try to calculate
   the total increment value for one iteration of the loop as a constant.

   Returns the increment value as an rtx, simplified as much as possible,
   if it can be calculated.  Otherwise, returns 0.  */

rtx 
biv_total_increment (bl, loop_start, loop_end)
     struct iv_class *bl;
     rtx loop_start, loop_end;
{
  struct induction *v;
  rtx result;

  /* For increment, must check every instruction that sets it.  Each
     instruction must be executed only once each time through the loop.
     To verify this, we check that the the insn is always executed, and that
     there are no backward branches after the insn that branch to before it.
     Also, the insn must have a mult_val of one (to make sure it really is
     an increment).  */

  result = const0_rtx;
  for (v = bl->biv; v; v = v->next_iv)
    {
      if (v->always_computable && v->mult_val == const1_rtx
	  && ! back_branch_in_range_p (v->insn, loop_start, loop_end))
	result = fold_rtx_mult_add (result, const1_rtx, v->add_val, v->mode);
      else
	return 0;
    }

  return result;
}

/* Determine the initial value of the iteration variable, and the amount
   that it is incremented each loop.  Use the tables constructed by
   the strength reduction pass to calculate these values.

   Initial_value and/or increment are set to zero if their values could not
   be calculated.  */

static void
iteration_info (iteration_var, initial_value, increment, loop_start, loop_end)
     rtx iteration_var, *initial_value, *increment;
     rtx loop_start, loop_end;
{
  struct iv_class *bl;
  struct induction *v, *b;

  /* Clear the result values, in case no answer can be found.  */
  *initial_value = 0;
  *increment = 0;

  /* The iteration variable can be either a giv or a biv.  Check to see
     which it is, and compute the variable's initial value, and increment
     value if possible.  */

  /* If this is a new register, can't handle it since we don't have any
     reg_iv_type entry for it.  */
  if (REGNO (iteration_var) >= max_reg_before_loop)
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Loop unrolling: No reg_iv_type entry for iteration var.\n");
      return;
    }
  /* Reject iteration variables larger than the host long size, since they
     could result in a number of iterations greater than the range of our
     `unsigned long' variable loop_n_iterations.  */
  else if (GET_MODE_BITSIZE (GET_MODE (iteration_var)) > HOST_BITS_PER_LONG)
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Loop unrolling: Iteration var rejected because mode larger than host long.\n");
      return;
    }
  else if (GET_MODE_CLASS (GET_MODE (iteration_var)) != MODE_INT)
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Loop unrolling: Iteration var not an integer.\n");
      return;
    }
  else if (reg_iv_type[REGNO (iteration_var)] == BASIC_INDUCT)
    {
      /* Grab initial value, only useful if it is a constant.  */
      bl = reg_biv_class[REGNO (iteration_var)];
      *initial_value = bl->initial_value;

      *increment = biv_total_increment (bl, loop_start, loop_end);
    }
  else if (reg_iv_type[REGNO (iteration_var)] == GENERAL_INDUCT)
    {
#if 1
      /* ??? The code below does not work because the incorrect number of
	 iterations is calculated when the biv is incremented after the giv
	 is set (which is the usual case).  This can probably be accounted
	 for by biasing the initial_value by subtracting the amount of the
	 increment that occurs between the giv set and the giv test.  However,
	 a giv as an iterator is very rare, so it does not seem worthwhile
	 to handle this.  */
      /* ??? An example failure is: i = 6; do {;} while (i++ < 9).  */
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Loop unrolling: Giv iterators are not handled.\n");
      return;
#else
      /* Initial value is mult_val times the biv's initial value plus
	 add_val.  Only useful if it is a constant.  */
      v = reg_iv_info[REGNO (iteration_var)];
      bl = reg_biv_class[REGNO (v->src_reg)];
      *initial_value = fold_rtx_mult_add (v->mult_val, bl->initial_value,
					  v->add_val, v->mode);
      
      /* Increment value is mult_val times the increment value of the biv.  */

      *increment = biv_total_increment (bl, loop_start, loop_end);
      if (*increment)
	*increment = fold_rtx_mult_add (v->mult_val, *increment, const0_rtx,
					v->mode);
#endif
    }
  else
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Loop unrolling: Not basic or general induction var.\n");
      return;
    }
}

/* Calculate the approximate final value of the iteration variable
   which has an loop exit test with code COMPARISON_CODE and comparison value
   of COMPARISON_VALUE.  Also returns an indication of whether the comparison
   was signed or unsigned, and the direction of the comparison.  This info is
   needed to calculate the number of loop iterations.  */

static rtx
approx_final_value (comparison_code, comparison_value, unsigned_p, compare_dir)
     enum rtx_code comparison_code;
     rtx comparison_value;
     int *unsigned_p;
     int *compare_dir;
{
  /* Calculate the final value of the induction variable.
     The exact final value depends on the branch operator, and increment sign.
     This is only an approximate value.  It will be wrong if the iteration
     variable is not incremented by one each time through the loop, and
     approx final value - start value % increment != 0.  */

  *unsigned_p = 0;
  switch (comparison_code)
    {
    case LEU:
      *unsigned_p = 1;
    case LE:
      *compare_dir = 1;
      return plus_constant (comparison_value, 1);
    case GEU:
      *unsigned_p = 1;
    case GE:
      *compare_dir = -1;
      return plus_constant (comparison_value, -1);
    case EQ:
      /* Can not calculate a final value for this case.  */
      *compare_dir = 0;
      return 0;
    case LTU:
      *unsigned_p = 1;
    case LT:
      *compare_dir = 1;
      return comparison_value;
      break;
    case GTU:
      *unsigned_p = 1;
    case GT:
      *compare_dir = -1;
      return comparison_value;
    case NE:
      *compare_dir = 0;
      return comparison_value;
    default:
      abort ();
    }
}

/* For each biv and giv, determine whether it can be safely split into
   a different variable for each unrolled copy of the loop body.  If it
   is safe to split, then indicate that by saving some useful info
   in the splittable_regs array.

   If the loop is being completely unrolled, then splittable_regs will hold
   the current value of the induction variable while the loop is unrolled.
   It must be set to the initial value of the induction variable here.
   Otherwise, splittable_regs will hold the difference between the current
   value of the induction variable and the value the induction variable had
   at the top of the loop.  It must be set to the value 0 here.  */

/* ?? If the loop is only unrolled twice, then most of the restrictions to
   constant values are unnecessary, since we can easily calculate increment
   values in this case even if nothing is constant.  The increment value
   should not involve a multiply however.  */

/* ?? Even if the biv/giv increment values aren't constant, it may still
   be beneficial to split the variable if the loop is only unrolled a few
   times, since multiplies by small integers (1,2,3,4) are very cheap.  */

static int
find_splittable_regs (unroll_type, loop_start, loop_end, end_insert_before,
		     unroll_number)
     enum unroll_types unroll_type;
     rtx loop_start, loop_end;
     rtx end_insert_before;
     int unroll_number;
{
  struct iv_class *bl;
  struct induction *v;
  rtx increment, tem;
  rtx biv_final_value;
  int biv_splittable;
  int result = 0;

  for (bl = loop_iv_list; bl; bl = bl->next)
    {
      /* Biv_total_increment must return a constant value,
	 otherwise we can not calculate the split values.  */

      increment = biv_total_increment (bl, loop_start, loop_end);
      if (! increment || GET_CODE (increment) != CONST_INT)
	continue;

      /* The loop must be unrolled completely, or else have a known number
	 of iterations and only one exit, or else the biv must be dead
	 outside the loop, or else the final value must be known.  Otherwise,
	 it is unsafe to split the biv since it may not have the proper
	 value on loop exit.  */

      /* loop_number_exit_labels is non-zero if the loop has an exit other than
	 a fall through at the end.  */

      biv_splittable = 1;
      biv_final_value = 0;
      if (unroll_type != UNROLL_COMPLETELY
	  && (loop_number_exit_labels[uid_loop_num[INSN_UID (loop_start)]]
	      || unroll_type == UNROLL_NAIVE)
	  && (uid_luid[regno_last_uid[bl->regno]] >= INSN_LUID (loop_end)
	      || ! bl->init_insn
	      || INSN_UID (bl->init_insn) >= max_uid_for_loop
	      || (uid_luid[regno_first_uid[bl->regno]]
		  < INSN_LUID (bl->init_insn))
	      || reg_mentioned_p (bl->biv->dest_reg, SET_SRC (bl->init_set)))
	  && ! (biv_final_value = final_biv_value (bl, loop_start, loop_end)))
	biv_splittable = 0;

      /* If any of the insns setting the BIV don't do so with a simple
	 PLUS, we don't know how to split it.  */
      for (v = bl->biv; biv_splittable && v; v = v->next_iv)
	if ((tem = single_set (v->insn)) == 0
	    || GET_CODE (SET_DEST (tem)) != REG
	    || REGNO (SET_DEST (tem)) != bl->regno
	    || GET_CODE (SET_SRC (tem)) != PLUS)
	  biv_splittable = 0;

      /* If final value is non-zero, then must emit an instruction which sets
	 the value of the biv to the proper value.  This is done after
	 handling all of the givs, since some of them may need to use the
	 biv's value in their initialization code.  */

      /* This biv is splittable.  If completely unrolling the loop, save
	 the biv's initial value.  Otherwise, save the constant zero.  */

      if (biv_splittable == 1)
	{
	  if (unroll_type == UNROLL_COMPLETELY)
	    {
	      /* If the initial value of the biv is itself (i.e. it is too
		 complicated for strength_reduce to compute), or is a hard
		 register, then we must create a new pseudo reg to hold the
		 initial value of the biv.  */

	      if (GET_CODE (bl->initial_value) == REG
		  && (REGNO (bl->initial_value) == bl->regno
		      || REGNO (bl->initial_value) < FIRST_PSEUDO_REGISTER))
		{
		  rtx tem = gen_reg_rtx (bl->biv->mode);
		  
		  emit_insn_before (gen_move_insn (tem, bl->biv->src_reg),
				    loop_start);

		  if (loop_dump_stream)
		    fprintf (loop_dump_stream, "Biv %d initial value remapped to %d.\n",
			     bl->regno, REGNO (tem));

		  splittable_regs[bl->regno] = tem;
		}
	      else
		splittable_regs[bl->regno] = bl->initial_value;
	    }
	  else
	    splittable_regs[bl->regno] = const0_rtx;

	  /* Save the number of instructions that modify the biv, so that
	     we can treat the last one specially.  */

	  splittable_regs_updates[bl->regno] = bl->biv_count;

	  result++;

	  if (loop_dump_stream)
	    fprintf (loop_dump_stream,
		     "Biv %d safe to split.\n", bl->regno);
	}

      /* Check every giv that depends on this biv to see whether it is
	 splittable also.  Even if the biv isn't splittable, givs which
	 depend on it may be splittable if the biv is live outside the
	 loop, and the givs aren't.  */

      result = find_splittable_givs (bl, unroll_type, loop_start, loop_end,
				     increment, unroll_number, result);

      /* If final value is non-zero, then must emit an instruction which sets
	 the value of the biv to the proper value.  This is done after
	 handling all of the givs, since some of them may need to use the
	 biv's value in their initialization code.  */
      if (biv_final_value)
	{
	  /* If the loop has multiple exits, emit the insns before the
	     loop to ensure that it will always be executed no matter
	     how the loop exits.  Otherwise emit the insn after the loop,
	     since this is slightly more efficient.  */
	  if (! loop_number_exit_labels[uid_loop_num[INSN_UID (loop_start)]])
	    emit_insn_before (gen_move_insn (bl->biv->src_reg,
					     biv_final_value),
			      end_insert_before);
	  else
	    {
	      /* Create a new register to hold the value of the biv, and then
		 set the biv to its final value before the loop start.  The biv
		 is set to its final value before loop start to ensure that
		 this insn will always be executed, no matter how the loop
		 exits.  */
	      rtx tem = gen_reg_rtx (bl->biv->mode);
	      emit_insn_before (gen_move_insn (tem, bl->biv->src_reg),
				loop_start);
	      emit_insn_before (gen_move_insn (bl->biv->src_reg,
					       biv_final_value),
				loop_start);

	      if (loop_dump_stream)
		fprintf (loop_dump_stream, "Biv %d mapped to %d for split.\n",
			 REGNO (bl->biv->src_reg), REGNO (tem));

	      /* Set up the mapping from the original biv register to the new
		 register.  */
	      bl->biv->src_reg = tem;
	    }
	}
    }
  return result;
}

/* For every giv based on the biv BL, check to determine whether it is
   splittable.  This is a subroutine to find_splittable_regs ().  */

static int
find_splittable_givs (bl, unroll_type, loop_start, loop_end, increment,
		      unroll_number, result)
     struct iv_class *bl;
     enum unroll_types unroll_type;
     rtx loop_start, loop_end;
     rtx increment;
     int unroll_number, result;
{
  struct induction *v;
  rtx final_value;
  rtx tem;

  for (v = bl->giv; v; v = v->next_iv)
    {
      rtx giv_inc, value;

      /* Only split the giv if it has already been reduced, or if the loop is
	 being completely unrolled.  */
      if (unroll_type != UNROLL_COMPLETELY && v->ignore)
	continue;

      /* The giv can be split if the insn that sets the giv is executed once
	 and only once on every iteration of the loop.  */
      /* An address giv can always be split.  v->insn is just a use not a set,
	 and hence it does not matter whether it is always executed.  All that
	 matters is that all the biv increments are always executed, and we
	 won't reach here if they aren't.  */
      if (v->giv_type != DEST_ADDR
	  && (! v->always_computable
	      || back_branch_in_range_p (v->insn, loop_start, loop_end)))
	continue;
      
      /* The giv increment value must be a constant.  */
      giv_inc = fold_rtx_mult_add (v->mult_val, increment, const0_rtx,
				   v->mode);
      if (! giv_inc || GET_CODE (giv_inc) != CONST_INT)
	continue;

      /* The loop must be unrolled completely, or else have a known number of
	 iterations and only one exit, or else the giv must be dead outside
	 the loop, or else the final value of the giv must be known.
	 Otherwise, it is not safe to split the giv since it may not have the
	 proper value on loop exit.  */
	  
      /* The used outside loop test will fail for DEST_ADDR givs.  They are
	 never used outside the loop anyways, so it is always safe to split a
	 DEST_ADDR giv.  */

      final_value = 0;
      if (unroll_type != UNROLL_COMPLETELY
	  && (loop_number_exit_labels[uid_loop_num[INSN_UID (loop_start)]]
	      || unroll_type == UNROLL_NAIVE)
	  && v->giv_type != DEST_ADDR
	  && ((regno_first_uid[REGNO (v->dest_reg)] != INSN_UID (v->insn)
	       /* Check for the case where the pseudo is set by a shift/add
		  sequence, in which case the first insn setting the pseudo
		  is the first insn of the shift/add sequence.  */
	       && (! (tem = find_reg_note (v->insn, REG_RETVAL, NULL_RTX))
		   || (regno_first_uid[REGNO (v->dest_reg)]
		       != INSN_UID (XEXP (tem, 0)))))
	      /* Line above always fails if INSN was moved by loop opt.  */
	      || (uid_luid[regno_last_uid[REGNO (v->dest_reg)]]
		  >= INSN_LUID (loop_end)))
	  && ! (final_value = v->final_value))
	continue;

#if 0
      /* Currently, non-reduced/final-value givs are never split.  */
      /* Should emit insns after the loop if possible, as the biv final value
	 code below does.  */

      /* If the final value is non-zero, and the giv has not been reduced,
	 then must emit an instruction to set the final value.  */
      if (final_value && !v->new_reg)
	{
	  /* Create a new register to hold the value of the giv, and then set
	     the giv to its final value before the loop start.  The giv is set
	     to its final value before loop start to ensure that this insn
	     will always be executed, no matter how we exit.  */
	  tem = gen_reg_rtx (v->mode);
	  emit_insn_before (gen_move_insn (tem, v->dest_reg), loop_start);
	  emit_insn_before (gen_move_insn (v->dest_reg, final_value),
			    loop_start);
	  
	  if (loop_dump_stream)
	    fprintf (loop_dump_stream, "Giv %d mapped to %d for split.\n",
		     REGNO (v->dest_reg), REGNO (tem));
	  
	  v->src_reg = tem;
	}
#endif

      /* This giv is splittable.  If completely unrolling the loop, save the
	 giv's initial value.  Otherwise, save the constant zero for it.  */

      if (unroll_type == UNROLL_COMPLETELY)
	{
	  /* It is not safe to use bl->initial_value here, because it may not
	     be invariant.  It is safe to use the initial value stored in
	     the splittable_regs array if it is set.  In rare cases, it won't
	     be set, so then we do exactly the same thing as
	     find_splittable_regs does to get a safe value.  */
	  rtx biv_initial_value;

	  if (splittable_regs[bl->regno])
	    biv_initial_value = splittable_regs[bl->regno];
	  else if (GET_CODE (bl->initial_value) != REG
		   || (REGNO (bl->initial_value) != bl->regno
		       && REGNO (bl->initial_value) >= FIRST_PSEUDO_REGISTER))
	    biv_initial_value = bl->initial_value;
	  else
	    {
	      rtx tem = gen_reg_rtx (bl->biv->mode);

	      emit_insn_before (gen_move_insn (tem, bl->biv->src_reg),
				loop_start);
	      biv_initial_value = tem;
	    }
	  value = fold_rtx_mult_add (v->mult_val, biv_initial_value,
				     v->add_val, v->mode);
	}
      else
	value = const0_rtx;

      if (v->new_reg)
	{
	  /* If a giv was combined with another giv, then we can only split
	     this giv if the giv it was combined with was reduced.  This
	     is because the value of v->new_reg is meaningless in this
	     case.  */
	  if (v->same && ! v->same->new_reg)
	    {
	      if (loop_dump_stream)
		fprintf (loop_dump_stream,
			 "giv combined with unreduced giv not split.\n");
	      continue;
	    }
	  /* If the giv is an address destination, it could be something other
	     than a simple register, these have to be treated differently.  */
	  else if (v->giv_type == DEST_REG)
	    {
	      /* If value is not a constant, register, or register plus
		 constant, then compute its value into a register before
		 loop start.  This prevents illegal rtx sharing, and should
		 generate better code.  We can use bl->initial_value here
		 instead of splittable_regs[bl->regno] because this code
		 is going before the loop start.  */
	      if (unroll_type == UNROLL_COMPLETELY
		  && GET_CODE (value) != CONST_INT
		  && GET_CODE (value) != REG
		  && (GET_CODE (value) != PLUS
		      || GET_CODE (XEXP (value, 0)) != REG
		      || GET_CODE (XEXP (value, 1)) != CONST_INT))
		{
		  rtx tem = gen_reg_rtx (v->mode);
		  emit_iv_add_mult (bl->initial_value, v->mult_val,
				    v->add_val, tem, loop_start);
		  value = tem;
		}
		
	      splittable_regs[REGNO (v->new_reg)] = value;
	    }
	  else
	    {
	      /* Splitting address givs is useful since it will often allow us
		 to eliminate some increment insns for the base giv as
		 unnecessary.  */

	      /* If the addr giv is combined with a dest_reg giv, then all
		 references to that dest reg will be remapped, which is NOT
		 what we want for split addr regs. We always create a new
		 register for the split addr giv, just to be safe.  */

	      /* ??? If there are multiple address givs which have been
		 combined with the same dest_reg giv, then we may only need
		 one new register for them.  Pulling out constants below will
		 catch some of the common cases of this.  Currently, I leave
		 the work of simplifying multiple address givs to the
		 following cse pass.  */
	      
	      v->const_adjust = 0;
	      if (unroll_type != UNROLL_COMPLETELY)
		{
		  /* If not completely unrolling the loop, then create a new
		     register to hold the split value of the DEST_ADDR giv.
		     Emit insn to initialize its value before loop start.  */
		  tem = gen_reg_rtx (v->mode);

		  /* If the address giv has a constant in its new_reg value,
		     then this constant can be pulled out and put in value,
		     instead of being part of the initialization code.  */
		  
		  if (GET_CODE (v->new_reg) == PLUS
		      && GET_CODE (XEXP (v->new_reg, 1)) == CONST_INT)
		    {
		      v->dest_reg
			= plus_constant (tem, INTVAL (XEXP (v->new_reg,1)));
		      
		      /* Only succeed if this will give valid addresses.
			 Try to validate both the first and the last
			 address resulting from loop unrolling, if
			 one fails, then can't do const elim here.  */
		      if (memory_address_p (v->mem_mode, v->dest_reg)
			  && memory_address_p (v->mem_mode,
				       plus_constant (v->dest_reg,
						      INTVAL (giv_inc)
						      * (unroll_number - 1))))
			{
			  /* Save the negative of the eliminated const, so
			     that we can calculate the dest_reg's increment
			     value later.  */
			  v->const_adjust = - INTVAL (XEXP (v->new_reg, 1));

			  v->new_reg = XEXP (v->new_reg, 0);
			  if (loop_dump_stream)
			    fprintf (loop_dump_stream,
				     "Eliminating constant from giv %d\n",
				     REGNO (tem));
			}
		      else
			v->dest_reg = tem;
		    }
		  else
		    v->dest_reg = tem;
		  
		  /* If the address hasn't been checked for validity yet, do so
		     now, and fail completely if either the first or the last
		     unrolled copy of the address is not a valid address.  */
		  if (v->dest_reg == tem
		      && (! memory_address_p (v->mem_mode, v->dest_reg)
			  || ! memory_address_p (v->mem_mode,
				 plus_constant (v->dest_reg,
						INTVAL (giv_inc)
						* (unroll_number -1)))))
		    {
		      if (loop_dump_stream)
			fprintf (loop_dump_stream,
				 "Illegal address for giv at insn %d\n",
				 INSN_UID (v->insn));
		      continue;
		    }
		  
		  /* To initialize the new register, just move the value of
		     new_reg into it.  This is not guaranteed to give a valid
		     instruction on machines with complex addressing modes.
		     If we can't recognize it, then delete it and emit insns
		     to calculate the value from scratch.  */
		  emit_insn_before (gen_rtx (SET, VOIDmode, tem,
					     copy_rtx (v->new_reg)),
				    loop_start);
		  if (recog_memoized (PREV_INSN (loop_start)) < 0)
		    {
		      delete_insn (PREV_INSN (loop_start));
		      emit_iv_add_mult (bl->initial_value, v->mult_val,
					v->add_val, tem, loop_start);
		      if (loop_dump_stream)
			fprintf (loop_dump_stream,
				 "Illegal init insn, rewritten.\n");
		    }
		}
	      else
		{
		  v->dest_reg = value;
		  
		  /* Check the resulting address for validity, and fail
		     if the resulting address would be illegal.  */
		  if (! memory_address_p (v->mem_mode, v->dest_reg)
		      || ! memory_address_p (v->mem_mode,
				     plus_constant (v->dest_reg,
						    INTVAL (giv_inc) *
						    (unroll_number -1))))
		    {
		      if (loop_dump_stream)
			fprintf (loop_dump_stream,
				 "Illegal address for giv at insn %d\n",
				 INSN_UID (v->insn));
		      continue;
		    }
		}
	      
	      /* Store the value of dest_reg into the insn.  This sharing
		 will not be a problem as this insn will always be copied
		 later.  */
	      
	      *v->location = v->dest_reg;
	      
	      /* If this address giv is combined with a dest reg giv, then
		 save the base giv's induction pointer so that we will be
		 able to handle this address giv properly.  The base giv
		 itself does not have to be splittable.  */
	      
	      if (v->same && v->same->giv_type == DEST_REG)
		addr_combined_regs[REGNO (v->same->new_reg)] = v->same;
	      
	      if (GET_CODE (v->new_reg) == REG)
		{
		  /* This giv maybe hasn't been combined with any others.
		     Make sure that it's giv is marked as splittable here.  */
		  
		  splittable_regs[REGNO (v->new_reg)] = value;
		  
		  /* Make it appear to depend upon itself, so that the
		     giv will be properly split in the main loop above.  */
		  if (! v->same)
		    {
		      v->same = v;
		      addr_combined_regs[REGNO (v->new_reg)] = v;
		    }
		}

	      if (loop_dump_stream)
		fprintf (loop_dump_stream, "DEST_ADDR giv being split.\n");
	    }
	}
      else
	{
#if 0
	  /* Currently, unreduced giv's can't be split.  This is not too much
	     of a problem since unreduced giv's are not live across loop
	     iterations anyways.  When unrolling a loop completely though,
	     it makes sense to reduce&split givs when possible, as this will
	     result in simpler instructions, and will not require that a reg
	     be live across loop iterations.  */
	  
	  splittable_regs[REGNO (v->dest_reg)] = value;
	  fprintf (stderr, "Giv %d at insn %d not reduced\n",
		   REGNO (v->dest_reg), INSN_UID (v->insn));
#else
	  continue;
#endif
	}
      
      /* Givs are only updated once by definition.  Mark it so if this is
	 a splittable register.  Don't need to do anything for address givs
	 where this may not be a register.  */

      if (GET_CODE (v->new_reg) == REG)
	splittable_regs_updates[REGNO (v->new_reg)] = 1;

      result++;
      
      if (loop_dump_stream)
	{
	  int regnum;
	  
	  if (GET_CODE (v->dest_reg) == CONST_INT)
	    regnum = -1;
	  else if (GET_CODE (v->dest_reg) != REG)
	    regnum = REGNO (XEXP (v->dest_reg, 0));
	  else
	    regnum = REGNO (v->dest_reg);
	  fprintf (loop_dump_stream, "Giv %d at insn %d safe to split.\n",
		   regnum, INSN_UID (v->insn));
	}
    }

  return result;
}

/* Try to prove that the register is dead after the loop exits.  Trace every
   loop exit looking for an insn that will always be executed, which sets
   the register to some value, and appears before the first use of the register
   is found.  If successful, then return 1, otherwise return 0.  */

/* ?? Could be made more intelligent in the handling of jumps, so that
   it can search past if statements and other similar structures.  */

static int
reg_dead_after_loop (reg, loop_start, loop_end)
     rtx reg, loop_start, loop_end;
{
  rtx insn, label;
  enum rtx_code code;
  int jump_count = 0;

  /* HACK: Must also search the loop fall through exit, create a label_ref
     here which points to the loop_end, and append the loop_number_exit_labels
     list to it.  */
  label = gen_rtx (LABEL_REF, VOIDmode, loop_end);
  LABEL_NEXTREF (label)
    = loop_number_exit_labels[uid_loop_num[INSN_UID (loop_start)]];

  for ( ; label; label = LABEL_NEXTREF (label))
    {
      /* Succeed if find an insn which sets the biv or if reach end of
	 function.  Fail if find an insn that uses the biv, or if come to
	 a conditional jump.  */

      insn = NEXT_INSN (XEXP (label, 0));
      while (insn)
	{
	  code = GET_CODE (insn);
	  if (GET_RTX_CLASS (code) == 'i')
	    {
	      rtx set;

	      if (reg_referenced_p (reg, PATTERN (insn)))
		return 0;

	      set = single_set (insn);
	      if (set && rtx_equal_p (SET_DEST (set), reg))
		break;
	    }

	  if (code == JUMP_INSN)
	    {
	      if (GET_CODE (PATTERN (insn)) == RETURN)
		break;
	      else if (! simplejump_p (insn)
		       /* Prevent infinite loop following infinite loops. */
		       || jump_count++ > 20)
		return 0;
	      else
		insn = JUMP_LABEL (insn);
	    }

	  insn = NEXT_INSN (insn);
	}
    }

  /* Success, the register is dead on all loop exits.  */
  return 1;
}

/* Try to calculate the final value of the biv, the value it will have at
   the end of the loop.  If we can do it, return that value.  */
  
rtx
final_biv_value (bl, loop_start, loop_end)
     struct iv_class *bl;
     rtx loop_start, loop_end;
{
  rtx increment, tem;

  /* ??? This only works for MODE_INT biv's.  Reject all others for now.  */

  if (GET_MODE_CLASS (bl->biv->mode) != MODE_INT)
    return 0;

  /* The final value for reversed bivs must be calculated differently than
      for ordinary bivs.  In this case, there is already an insn after the
     loop which sets this biv's final value (if necessary), and there are
     no other loop exits, so we can return any value.  */
  if (bl->reversed)
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Final biv value for %d, reversed biv.\n", bl->regno);
		 
      return const0_rtx;
    }

  /* Try to calculate the final value as initial value + (number of iterations
     * increment).  For this to work, increment must be invariant, the only
     exit from the loop must be the fall through at the bottom (otherwise
     it may not have its final value when the loop exits), and the initial
     value of the biv must be invariant.  */

  if (loop_n_iterations != 0
      && ! loop_number_exit_labels[uid_loop_num[INSN_UID (loop_start)]]
      && invariant_p (bl->initial_value))
    {
      increment = biv_total_increment (bl, loop_start, loop_end);
      
      if (increment && invariant_p (increment))
	{
	  /* Can calculate the loop exit value, emit insns after loop
	     end to calculate this value into a temporary register in
	     case it is needed later.  */

	  tem = gen_reg_rtx (bl->biv->mode);
	  /* Make sure loop_end is not the last insn.  */
	  if (NEXT_INSN (loop_end) == 0)
	    emit_note_after (NOTE_INSN_DELETED, loop_end);
	  emit_iv_add_mult (increment, GEN_INT (loop_n_iterations),
			    bl->initial_value, tem, NEXT_INSN (loop_end));

	  if (loop_dump_stream)
	    fprintf (loop_dump_stream,
		     "Final biv value for %d, calculated.\n", bl->regno);
	  
	  return tem;
	}
    }

  /* Check to see if the biv is dead at all loop exits.  */
  if (reg_dead_after_loop (bl->biv->src_reg, loop_start, loop_end))
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Final biv value for %d, biv dead after loop exit.\n",
		 bl->regno);

      return const0_rtx;
    }

  return 0;
}

/* Try to calculate the final value of the giv, the value it will have at
   the end of the loop.  If we can do it, return that value.  */

rtx
final_giv_value (v, loop_start, loop_end)
     struct induction *v;
     rtx loop_start, loop_end;
{
  struct iv_class *bl;
  rtx insn;
  rtx increment, tem;
  enum rtx_code code;
  rtx insert_before, seq;

  bl = reg_biv_class[REGNO (v->src_reg)];

  /* The final value for givs which depend on reversed bivs must be calculated
     differently than for ordinary givs.  In this case, there is already an
     insn after the loop which sets this giv's final value (if necessary),
     and there are no other loop exits, so we can return any value.  */
  if (bl->reversed)
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Final giv value for %d, depends on reversed biv\n",
		 REGNO (v->dest_reg));
      return const0_rtx;
    }

  /* Try to calculate the final value as a function of the biv it depends
     upon.  The only exit from the loop must be the fall through at the bottom
     (otherwise it may not have its final value when the loop exits).  */
      
  /* ??? Can calculate the final giv value by subtracting off the
     extra biv increments times the giv's mult_val.  The loop must have
     only one exit for this to work, but the loop iterations does not need
     to be known.  */

  if (loop_n_iterations != 0
      && ! loop_number_exit_labels[uid_loop_num[INSN_UID (loop_start)]])
    {
      /* ?? It is tempting to use the biv's value here since these insns will
	 be put after the loop, and hence the biv will have its final value
	 then.  However, this fails if the biv is subsequently eliminated.
	 Perhaps determine whether biv's are eliminable before trying to
	 determine whether giv's are replaceable so that we can use the
	 biv value here if it is not eliminable.  */

      increment = biv_total_increment (bl, loop_start, loop_end);

      if (increment && invariant_p (increment))
	{
	  /* Can calculate the loop exit value of its biv as
	     (loop_n_iterations * increment) + initial_value */
	      
	  /* The loop exit value of the giv is then
	     (final_biv_value - extra increments) * mult_val + add_val.
	     The extra increments are any increments to the biv which
	     occur in the loop after the giv's value is calculated.
	     We must search from the insn that sets the giv to the end
	     of the loop to calculate this value.  */

	  insert_before = NEXT_INSN (loop_end);

	  /* Put the final biv value in tem.  */
	  tem = gen_reg_rtx (bl->biv->mode);
	  emit_iv_add_mult (increment, GEN_INT (loop_n_iterations),
			    bl->initial_value, tem, insert_before);

	  /* Subtract off extra increments as we find them.  */
	  for (insn = NEXT_INSN (v->insn); insn != loop_end;
	       insn = NEXT_INSN (insn))
	    {
	      struct induction *biv;

	      for (biv = bl->biv; biv; biv = biv->next_iv)
		if (biv->insn == insn)
		  {
		    start_sequence ();
		    tem = expand_binop (GET_MODE (tem), sub_optab, tem,
					biv->add_val, NULL_RTX, 0,
					OPTAB_LIB_WIDEN);
		    seq = gen_sequence ();
		    end_sequence ();
		    emit_insn_before (seq, insert_before);
		  }
	    }
	  
	  /* Now calculate the giv's final value.  */
	  emit_iv_add_mult (tem, v->mult_val, v->add_val, tem,
			    insert_before);
	  
	  if (loop_dump_stream)
	    fprintf (loop_dump_stream,
		     "Final giv value for %d, calc from biv's value.\n",
		     REGNO (v->dest_reg));

	  return tem;
	}
    }

  /* Replaceable giv's should never reach here.  */
  if (v->replaceable)
    abort ();

  /* Check to see if the biv is dead at all loop exits.  */
  if (reg_dead_after_loop (v->dest_reg, loop_start, loop_end))
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Final giv value for %d, giv dead after loop exit.\n",
		 REGNO (v->dest_reg));

      return const0_rtx;
    }

  return 0;
}


/* Calculate the number of loop iterations.  Returns the exact number of loop
   iterations if it can be calculated, otherwise returns zero.  */

unsigned HOST_WIDE_INT
loop_iterations (loop_start, loop_end)
     rtx loop_start, loop_end;
{
  rtx comparison, comparison_value;
  rtx iteration_var, initial_value, increment, final_value;
  enum rtx_code comparison_code;
  HOST_WIDE_INT i;
  int increment_dir;
  int unsigned_compare, compare_dir, final_larger;
  unsigned long tempu;
  rtx last_loop_insn;

  /* First find the iteration variable.  If the last insn is a conditional
     branch, and the insn before tests a register value, make that the
     iteration variable.  */
  
  loop_initial_value = 0;
  loop_increment = 0;
  loop_final_value = 0;
  loop_iteration_var = 0;

  last_loop_insn = prev_nonnote_insn (loop_end);

  comparison = get_condition_for_loop (last_loop_insn);
  if (comparison == 0)
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Loop unrolling: No final conditional branch found.\n");
      return 0;
    }

  /* ??? Get_condition may switch position of induction variable and
     invariant register when it canonicalizes the comparison.  */

  comparison_code = GET_CODE (comparison);
  iteration_var = XEXP (comparison, 0);
  comparison_value = XEXP (comparison, 1);

  if (GET_CODE (iteration_var) != REG)
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Loop unrolling: Comparison not against register.\n");
      return 0;
    }

  /* Loop iterations is always called before any new registers are created
     now, so this should never occur.  */

  if (REGNO (iteration_var) >= max_reg_before_loop)
    abort ();

  iteration_info (iteration_var, &initial_value, &increment,
		  loop_start, loop_end);
  if (initial_value == 0)
    /* iteration_info already printed a message.  */
    return 0;

  if (increment == 0)
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Loop unrolling: Increment value can't be calculated.\n");
      return 0;
    }
  if (GET_CODE (increment) != CONST_INT)
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Loop unrolling: Increment value not constant.\n");
      return 0;
    }
  if (GET_CODE (initial_value) != CONST_INT)
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Loop unrolling: Initial value not constant.\n");
      return 0;
    }

  /* If the comparison value is an invariant register, then try to find
     its value from the insns before the start of the loop.  */

  if (GET_CODE (comparison_value) == REG && invariant_p (comparison_value))
    {
      rtx insn, set;
    
      for (insn = PREV_INSN (loop_start); insn ; insn = PREV_INSN (insn))
	{
	  if (GET_CODE (insn) == CODE_LABEL)
	    break;

	  else if (GET_RTX_CLASS (GET_CODE (insn)) == 'i'
		   && reg_set_p (comparison_value, insn))
	    {
	      /* We found the last insn before the loop that sets the register.
		 If it sets the entire register, and has a REG_EQUAL note,
		 then use the value of the REG_EQUAL note.  */
	      if ((set = single_set (insn))
		  && (SET_DEST (set) == comparison_value))
		{
		  rtx note = find_reg_note (insn, REG_EQUAL, NULL_RTX);

		  if (note && GET_CODE (XEXP (note, 0)) != EXPR_LIST)
		    comparison_value = XEXP (note, 0);
		}
	      break;
	    }
	}
    }

  final_value = approx_final_value (comparison_code, comparison_value,
				    &unsigned_compare, &compare_dir);

  /* Save the calculated values describing this loop's bounds, in case
     precondition_loop_p will need them later.  These values can not be
     recalculated inside precondition_loop_p because strength reduction
     optimizations may obscure the loop's structure.  */

  loop_iteration_var = iteration_var;
  loop_initial_value = initial_value;
  loop_increment = increment;
  loop_final_value = final_value;

  if (final_value == 0)
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Loop unrolling: EQ comparison loop.\n");
      return 0;
    }
  else if (GET_CODE (final_value) != CONST_INT)
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Loop unrolling: Final value not constant.\n");
      return 0;
    }

  /* ?? Final value and initial value do not have to be constants.
     Only their difference has to be constant.  When the iteration variable
     is an array address, the final value and initial value might both
     be addresses with the same base but different constant offsets.
     Final value must be invariant for this to work.

     To do this, need some way to find the values of registers which are
     invariant.  */

  /* Final_larger is 1 if final larger, 0 if they are equal, otherwise -1.  */
  if (unsigned_compare)
    final_larger
      = ((unsigned HOST_WIDE_INT) INTVAL (final_value)
	 > (unsigned HOST_WIDE_INT) INTVAL (initial_value))
	- ((unsigned HOST_WIDE_INT) INTVAL (final_value)
	   < (unsigned HOST_WIDE_INT) INTVAL (initial_value));
  else
    final_larger = (INTVAL (final_value) > INTVAL (initial_value))
      - (INTVAL (final_value) < INTVAL (initial_value));

  if (INTVAL (increment) > 0)
    increment_dir = 1;
  else if (INTVAL (increment) == 0)
    increment_dir = 0;
  else
    increment_dir = -1;

  /* There are 27 different cases: compare_dir = -1, 0, 1;
     final_larger = -1, 0, 1; increment_dir = -1, 0, 1.
     There are 4 normal cases, 4 reverse cases (where the iteration variable
     will overflow before the loop exits), 4 infinite loop cases, and 15
     immediate exit (0 or 1 iteration depending on loop type) cases.
     Only try to optimize the normal cases.  */
     
  /* (compare_dir/final_larger/increment_dir)
     Normal cases: (0/-1/-1), (0/1/1), (-1/-1/-1), (1/1/1)
     Reverse cases: (0/-1/1), (0/1/-1), (-1/-1/1), (1/1/-1)
     Infinite loops: (0/-1/0), (0/1/0), (-1/-1/0), (1/1/0)
     Immediate exit: (0/0/X), (-1/0/X), (-1/1/X), (1/0/X), (1/-1/X) */

  /* ?? If the meaning of reverse loops (where the iteration variable
     will overflow before the loop exits) is undefined, then could
     eliminate all of these special checks, and just always assume
     the loops are normal/immediate/infinite.  Note that this means
     the sign of increment_dir does not have to be known.  Also,
     since it does not really hurt if immediate exit loops or infinite loops
     are optimized, then that case could be ignored also, and hence all
     loops can be optimized.

     According to ANSI Spec, the reverse loop case result is undefined,
     because the action on overflow is undefined.

     See also the special test for NE loops below.  */

  if (final_larger == increment_dir && final_larger != 0
      && (final_larger == compare_dir || compare_dir == 0))
    /* Normal case.  */
    ;
  else
    {
      if (loop_dump_stream)
	fprintf (loop_dump_stream,
		 "Loop unrolling: Not normal loop.\n");
      return 0;
    }

  /* Calculate the number of iterations, final_value is only an approximation,
     so correct for that.  Note that tempu and loop_n_iterations are
     unsigned, because they can be as large as 2^n - 1.  */

  i = INTVAL (increment);
  if (i > 0)
    tempu = INTVAL (final_value) - INTVAL (initial_value);
  else if (i < 0)
    {
      tempu = INTVAL (initial_value) - INTVAL (final_value);
      i = -i;
    }
  else
    abort ();

  /* For NE tests, make sure that the iteration variable won't miss the
     final value.  If tempu mod i is not zero, then the iteration variable
     will overflow before the loop exits, and we can not calculate the
     number of iterations.  */
  if (compare_dir == 0 && (tempu % i) != 0)
    return 0;

  return tempu / i + ((tempu % i) != 0);
}
