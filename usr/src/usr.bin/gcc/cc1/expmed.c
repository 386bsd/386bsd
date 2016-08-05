/* Medium-level subroutines: convert bit-field store and extract
   and shifts, multiplies and divides to rtl instructions.
   Copyright (C) 1987, 1988, 1989, 1992, 1993 Free Software Foundation, Inc.

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
#include "tree.h"
#include "flags.h"
#include "insn-flags.h"
#include "insn-codes.h"
#include "insn-config.h"
#include "expr.h"
#include "real.h"
#include "recog.h"

static rtx extract_split_bit_field ();
static rtx extract_fixed_bit_field ();
static void store_split_bit_field ();
static void store_fixed_bit_field ();
static rtx mask_rtx ();
static rtx lshift_value ();

#define CEIL(x,y) (((x) + (y) - 1) / (y))

/* Non-zero means multiply instructions are cheaper than shifts.  */
int mult_is_very_cheap;

/* Non-zero means divides or modulus operations are relatively cheap for
   powers of two, so don't use branches; emit the operation instead. 
   Usually, this will mean that the MD file will emit non-branch
   sequences.  */

static int sdiv_pow2_cheap, smod_pow2_cheap;

/* For compilers that support multiple targets with different word sizes,
   MAX_BITS_PER_WORD contains the biggest value of BITS_PER_WORD.  An example
   is the H8/300(H) compiler.  */

#ifndef MAX_BITS_PER_WORD
#define MAX_BITS_PER_WORD BITS_PER_WORD
#endif

/* Cost of various pieces of RTL.  */
static int add_cost, mult_cost, negate_cost, zero_cost;
static int shift_cost[MAX_BITS_PER_WORD];
static int shiftadd_cost[MAX_BITS_PER_WORD];
static int shiftsub_cost[MAX_BITS_PER_WORD];

void
init_expmed ()
{
  char *free_point;
  /* This is "some random pseudo register" for purposes of calling recog
     to see what insns exist.  */
  rtx reg = gen_rtx (REG, word_mode, FIRST_PSEUDO_REGISTER);
  rtx shift_insn, shiftadd_insn, shiftsub_insn;
  int dummy;
  int m;

  start_sequence ();

  /* Since we are on the permanent obstack, we must be sure we save this
     spot AFTER we call start_sequence, since it will reuse the rtl it
     makes.  */

  free_point = (char *) oballoc (0);

  zero_cost = rtx_cost (const0_rtx, 0);
  add_cost = rtx_cost (gen_rtx (PLUS, word_mode, reg, reg), SET);

  shift_insn = emit_insn (gen_rtx (SET, VOIDmode, reg,
				   gen_rtx (ASHIFT, word_mode, reg,
					    const0_rtx)));

  shiftadd_insn = emit_insn (gen_rtx (SET, VOIDmode, reg,
				      gen_rtx (PLUS, word_mode,
					       gen_rtx (MULT, word_mode,
							reg, const0_rtx),
					       reg)));

  shiftsub_insn = emit_insn (gen_rtx (SET, VOIDmode, reg,
				      gen_rtx (MINUS, word_mode,
					       gen_rtx (MULT, word_mode,
							 reg, const0_rtx),
						reg)));

  init_recog ();

  shift_cost[0] = 0;
  shiftadd_cost[0] = shiftsub_cost[0] = add_cost;

  for (m = 1; m < BITS_PER_WORD; m++)
    {
      shift_cost[m] = shiftadd_cost[m] = shiftsub_cost[m] = 32000;

      XEXP (SET_SRC (PATTERN (shift_insn)), 1) = GEN_INT (m);
      if (recog (PATTERN (shift_insn), shift_insn, &dummy) >= 0)
	shift_cost[m] = rtx_cost (SET_SRC (PATTERN (shift_insn)), SET);

      XEXP (XEXP (SET_SRC (PATTERN (shiftadd_insn)), 0), 1)
	= GEN_INT ((HOST_WIDE_INT) 1 << m);
      if (recog (PATTERN (shiftadd_insn), shiftadd_insn, &dummy) >= 0)
	shiftadd_cost[m] = rtx_cost (SET_SRC (PATTERN (shiftadd_insn)), SET);

      XEXP (XEXP (SET_SRC (PATTERN (shiftsub_insn)), 0), 1)
	= GEN_INT ((HOST_WIDE_INT) 1 << m);
      if (recog (PATTERN (shiftsub_insn), shiftsub_insn, &dummy) >= 0)
	shiftsub_cost[m] = rtx_cost (SET_SRC (PATTERN (shiftsub_insn)), SET);
    }

  mult_cost = rtx_cost (gen_rtx (MULT, word_mode, reg, reg), SET);
  /* For gcc 2.4 keep MULT_COST small to avoid really slow searches
     in synth_mult.  */
  mult_cost = MIN (12 * add_cost, mult_cost);
  negate_cost = rtx_cost (gen_rtx (NEG, word_mode, reg), SET);

  /* 999999 is chosen to avoid any plausible faster special case.  */
  mult_is_very_cheap
    = (rtx_cost (gen_rtx (MULT, word_mode, reg, GEN_INT (999999)), SET)
       < rtx_cost (gen_rtx (ASHIFT, word_mode, reg, GEN_INT (7)), SET));

  sdiv_pow2_cheap
    = (rtx_cost (gen_rtx (DIV, word_mode, reg, GEN_INT (32)), SET)
       <= 2 * add_cost);
  smod_pow2_cheap
    = (rtx_cost (gen_rtx (MOD, word_mode, reg, GEN_INT (32)), SET)
       <= 2 * add_cost);

  /* Free the objects we just allocated.  */
  end_sequence ();
  obfree (free_point);
}

/* Return an rtx representing minus the value of X.
   MODE is the intended mode of the result,
   useful if X is a CONST_INT.  */

rtx
negate_rtx (mode, x)
     enum machine_mode mode;
     rtx x;
{
  if (GET_CODE (x) == CONST_INT)
    {
      HOST_WIDE_INT val = - INTVAL (x);
      if (GET_MODE_BITSIZE (mode) < HOST_BITS_PER_WIDE_INT)
	{
	  /* Sign extend the value from the bits that are significant.  */
	  if (val & ((HOST_WIDE_INT) 1 << (GET_MODE_BITSIZE (mode) - 1)))
	    val |= (HOST_WIDE_INT) (-1) << GET_MODE_BITSIZE (mode);
	  else
	    val &= ((HOST_WIDE_INT) 1 << GET_MODE_BITSIZE (mode)) - 1;
	}
      return GEN_INT (val);
    }
  else
    return expand_unop (GET_MODE (x), neg_optab, x, NULL_RTX, 0);
}

/* Generate code to store value from rtx VALUE
   into a bit-field within structure STR_RTX
   containing BITSIZE bits starting at bit BITNUM.
   FIELDMODE is the machine-mode of the FIELD_DECL node for this field.
   ALIGN is the alignment that STR_RTX is known to have, measured in bytes.
   TOTAL_SIZE is the size of the structure in bytes, or -1 if varying.  */

/* ??? Note that there are two different ideas here for how
   to determine the size to count bits within, for a register.
   One is BITS_PER_WORD, and the other is the size of operand 3
   of the insv pattern.  (The latter assumes that an n-bit machine
   will be able to insert bit fields up to n bits wide.)
   It isn't certain that either of these is right.
   extract_bit_field has the same quandary.  */

rtx
store_bit_field (str_rtx, bitsize, bitnum, fieldmode, value, align, total_size)
     rtx str_rtx;
     register int bitsize;
     int bitnum;
     enum machine_mode fieldmode;
     rtx value;
     int align;
     int total_size;
{
  int unit = (GET_CODE (str_rtx) == MEM) ? BITS_PER_UNIT : BITS_PER_WORD;
  register int offset = bitnum / unit;
  register int bitpos = bitnum % unit;
  register rtx op0 = str_rtx;

  if (GET_CODE (str_rtx) == MEM && ! MEM_IN_STRUCT_P (str_rtx))
    abort ();

  /* Discount the part of the structure before the desired byte.
     We need to know how many bytes are safe to reference after it.  */
  if (total_size >= 0)
    total_size -= (bitpos / BIGGEST_ALIGNMENT
		   * (BIGGEST_ALIGNMENT / BITS_PER_UNIT));

  while (GET_CODE (op0) == SUBREG)
    {
      /* The following line once was done only if WORDS_BIG_ENDIAN,
	 but I think that is a mistake.  WORDS_BIG_ENDIAN is
	 meaningful at a much higher level; when structures are copied
	 between memory and regs, the higher-numbered regs
	 always get higher addresses.  */
      offset += SUBREG_WORD (op0);
      /* We used to adjust BITPOS here, but now we do the whole adjustment
	 right after the loop.  */
      op0 = SUBREG_REG (op0);
    }

#if BYTES_BIG_ENDIAN
  /* If OP0 is a register, BITPOS must count within a word.
     But as we have it, it counts within whatever size OP0 now has.
     On a bigendian machine, these are not the same, so convert.  */
  if (GET_CODE (op0) != MEM && unit > GET_MODE_BITSIZE (GET_MODE (op0)))
    bitpos += unit - GET_MODE_BITSIZE (GET_MODE (op0));
#endif

  value = protect_from_queue (value, 0);

  if (flag_force_mem)
    value = force_not_mem (value);

  /* Note that the adjustment of BITPOS above has no effect on whether
     BITPOS is 0 in a REG bigger than a word.  */
  if (GET_MODE_SIZE (fieldmode) >= UNITS_PER_WORD
      && (! STRICT_ALIGNMENT || GET_CODE (op0) != MEM)
      && bitpos == 0 && bitsize == GET_MODE_BITSIZE (fieldmode))
    {
      /* Storing in a full-word or multi-word field in a register
	 can be done with just SUBREG.  */
      if (GET_MODE (op0) != fieldmode)
	if (GET_CODE (op0) == REG)
	  op0 = gen_rtx (SUBREG, fieldmode, op0, offset);
	else
	  op0 = change_address (op0, fieldmode,
				plus_constant (XEXP (op0, 0), offset));
      emit_move_insn (op0, value);
      return value;
    }

  /* Storing an lsb-aligned field in a register
     can be done with a movestrict instruction.  */

  if (GET_CODE (op0) != MEM
#if BYTES_BIG_ENDIAN
      && bitpos + bitsize == unit
#else
      && bitpos == 0
#endif
      && bitsize == GET_MODE_BITSIZE (fieldmode)
      && (GET_MODE (op0) == fieldmode
	  || (movstrict_optab->handlers[(int) fieldmode].insn_code
	      != CODE_FOR_nothing)))
    {
      /* Get appropriate low part of the value being stored.  */
      if (GET_CODE (value) == CONST_INT || GET_CODE (value) == REG)
	value = gen_lowpart (fieldmode, value);
      else if (!(GET_CODE (value) == SYMBOL_REF
		 || GET_CODE (value) == LABEL_REF
		 || GET_CODE (value) == CONST))
	value = convert_to_mode (fieldmode, value, 0);

      if (GET_MODE (op0) == fieldmode)
	emit_move_insn (op0, value);
      else
	{
	  int icode = movstrict_optab->handlers[(int) fieldmode].insn_code;
	  if(! (*insn_operand_predicate[icode][1]) (value, fieldmode))
	    value = copy_to_mode_reg (fieldmode, value);
	  emit_insn (GEN_FCN (icode)
		   (gen_rtx (SUBREG, fieldmode, op0, offset), value));
	}
      return value;
    }

  /* Handle fields bigger than a word.  */

  if (bitsize > BITS_PER_WORD)
    {
      /* Here we transfer the words of the field
	 in the order least significant first.
	 This is because the most significant word is the one which may
	 be less than full.  */

      int nwords = (bitsize + (BITS_PER_WORD - 1)) / BITS_PER_WORD;
      int i;

      /* This is the mode we must force value to, so that there will be enough
	 subwords to extract.  Note that fieldmode will often (always?) be
	 VOIDmode, because that is what store_field uses to indicate that this
	 is a bit field, but passing VOIDmode to operand_subword_force will
	 result in an abort.  */
      fieldmode = mode_for_size (nwords * BITS_PER_WORD, MODE_INT, 0);

      for (i = 0; i < nwords; i++)
	{
	  /* If I is 0, use the low-order word in both field and target;
	     if I is 1, use the next to lowest word; and so on.  */
	  int wordnum = (WORDS_BIG_ENDIAN ? nwords - i - 1 : i);
	  int bit_offset = (WORDS_BIG_ENDIAN
			    ? MAX (bitsize - (i + 1) * BITS_PER_WORD, 0)
			    : i * BITS_PER_WORD);
	  store_bit_field (op0, MIN (BITS_PER_WORD,
				     bitsize - i * BITS_PER_WORD),
			   bitnum + bit_offset, word_mode,
			   operand_subword_force (value, wordnum, fieldmode),
			   align, total_size);
	}
      return value;
    }

  /* From here on we can assume that the field to be stored in is
     a full-word (whatever type that is), since it is shorter than a word.  */

  /* OFFSET is the number of words or bytes (UNIT says which)
     from STR_RTX to the first word or byte containing part of the field.  */

  if (GET_CODE (op0) == REG)
    {
      if (offset != 0
	  || GET_MODE_SIZE (GET_MODE (op0)) > UNITS_PER_WORD)
	op0 = gen_rtx (SUBREG, TYPE_MODE (type_for_size (BITS_PER_WORD, 0)),
		       op0, offset);
      offset = 0;
    }
  else
    {
      op0 = protect_from_queue (op0, 1);
    }

  /* Now OFFSET is nonzero only if OP0 is memory
     and is therefore always measured in bytes.  */

#ifdef HAVE_insv
  if (HAVE_insv
      && !(bitsize == 1 && GET_CODE (value) == CONST_INT)
      /* Ensure insv's size is wide enough for this field.  */
      && (GET_MODE_BITSIZE (insn_operand_mode[(int) CODE_FOR_insv][3])
	  >= bitsize))
    {
      int xbitpos = bitpos;
      rtx value1;
      rtx xop0 = op0;
      rtx last = get_last_insn ();
      rtx pat;
      enum machine_mode maxmode
	= insn_operand_mode[(int) CODE_FOR_insv][3];

      int save_volatile_ok = volatile_ok;
      volatile_ok = 1;

      /* If this machine's insv can only insert into a register, or if we
	 are to force MEMs into a register, copy OP0 into a register and
	 save it back later.  */
      if (GET_CODE (op0) == MEM
	  && (flag_force_mem
	      || ! ((*insn_operand_predicate[(int) CODE_FOR_insv][0])
		    (op0, VOIDmode))))
	{
	  rtx tempreg;
	  enum machine_mode bestmode;

	  /* Get the mode to use for inserting into this field.  If OP0 is
	     BLKmode, get the smallest mode consistent with the alignment. If
	     OP0 is a non-BLKmode object that is no wider than MAXMODE, use its
	     mode. Otherwise, use the smallest mode containing the field.  */

	  if (GET_MODE (op0) == BLKmode
	      || GET_MODE_SIZE (GET_MODE (op0)) > GET_MODE_SIZE (maxmode))
	    bestmode
	      = get_best_mode (bitsize, bitnum, align * BITS_PER_UNIT, maxmode,
			       MEM_VOLATILE_P (op0));
	  else
	    bestmode = GET_MODE (op0);

	  if (bestmode == VOIDmode)
	    goto insv_loses;

	  /* Adjust address to point to the containing unit of that mode.  */
	  unit = GET_MODE_BITSIZE (bestmode);
	  /* Compute offset as multiple of this unit, counting in bytes.  */
	  offset = (bitnum / unit) * GET_MODE_SIZE (bestmode);
	  bitpos = bitnum % unit;
	  op0 = change_address (op0, bestmode, 
				plus_constant (XEXP (op0, 0), offset));

	  /* Fetch that unit, store the bitfield in it, then store the unit.  */
	  tempreg = copy_to_reg (op0);
	  store_bit_field (tempreg, bitsize, bitpos, fieldmode, value,
			   align, total_size);
	  emit_move_insn (op0, tempreg);
	  return value;
	}
      volatile_ok = save_volatile_ok;

      /* Add OFFSET into OP0's address.  */
      if (GET_CODE (xop0) == MEM)
	xop0 = change_address (xop0, byte_mode,
			       plus_constant (XEXP (xop0, 0), offset));

      /* If xop0 is a register, we need it in MAXMODE
	 to make it acceptable to the format of insv.  */
      if (GET_CODE (xop0) == SUBREG)
	PUT_MODE (xop0, maxmode);
      if (GET_CODE (xop0) == REG && GET_MODE (xop0) != maxmode)
	xop0 = gen_rtx (SUBREG, maxmode, xop0, 0);

      /* On big-endian machines, we count bits from the most significant.
	 If the bit field insn does not, we must invert.  */

#if BITS_BIG_ENDIAN != BYTES_BIG_ENDIAN
      xbitpos = unit - bitsize - xbitpos;
#endif
      /* We have been counting XBITPOS within UNIT.
	 Count instead within the size of the register.  */
#if BITS_BIG_ENDIAN
      if (GET_CODE (xop0) != MEM)
	xbitpos += GET_MODE_BITSIZE (maxmode) - unit;
#endif
      unit = GET_MODE_BITSIZE (maxmode);

      /* Convert VALUE to maxmode (which insv insn wants) in VALUE1.  */
      value1 = value;
      if (GET_MODE (value) != maxmode)
	{
	  if (GET_MODE_BITSIZE (GET_MODE (value)) >= bitsize)
	    {
	      /* Optimization: Don't bother really extending VALUE
		 if it has all the bits we will actually use.  However,
		 if we must narrow it, be sure we do it correctly.  */

	      if (GET_MODE_SIZE (GET_MODE (value)) < GET_MODE_SIZE (maxmode))
		{
		  /* Avoid making subreg of a subreg, or of a mem.  */
		  if (GET_CODE (value1) != REG)
		value1 = copy_to_reg (value1);
		  value1 = gen_rtx (SUBREG, maxmode, value1, 0);
		}
	      else
		value1 = gen_lowpart (maxmode, value1);
	    }
	  else if (!CONSTANT_P (value))
	    /* Parse phase is supposed to make VALUE's data type
	       match that of the component reference, which is a type
	       at least as wide as the field; so VALUE should have
	       a mode that corresponds to that type.  */
	    abort ();
	}

      /* If this machine's insv insists on a register,
	 get VALUE1 into a register.  */
      if (! ((*insn_operand_predicate[(int) CODE_FOR_insv][3])
	     (value1, maxmode)))
	value1 = force_reg (maxmode, value1);

      pat = gen_insv (xop0, GEN_INT (bitsize), GEN_INT (xbitpos), value1);
      if (pat)
	emit_insn (pat);
      else
        {
	  delete_insns_since (last);
	  store_fixed_bit_field (op0, offset, bitsize, bitpos, value, align);
	}
    }
  else
    insv_loses:
#endif
    /* Insv is not available; store using shifts and boolean ops.  */
    store_fixed_bit_field (op0, offset, bitsize, bitpos, value, align);
  return value;
}

/* Use shifts and boolean operations to store VALUE
   into a bit field of width BITSIZE
   in a memory location specified by OP0 except offset by OFFSET bytes.
     (OFFSET must be 0 if OP0 is a register.)
   The field starts at position BITPOS within the byte.
    (If OP0 is a register, it may be a full word or a narrower mode,
     but BITPOS still counts within a full word,
     which is significant on bigendian machines.)
   STRUCT_ALIGN is the alignment the structure is known to have (in bytes).

   Note that protect_from_queue has already been done on OP0 and VALUE.  */

static void
store_fixed_bit_field (op0, offset, bitsize, bitpos, value, struct_align)
     register rtx op0;
     register int offset, bitsize, bitpos;
     register rtx value;
     int struct_align;
{
  register enum machine_mode mode;
  int total_bits = BITS_PER_WORD;
  rtx subtarget, temp;
  int all_zero = 0;
  int all_one = 0;

  /* Add OFFSET to OP0's address (if it is in memory)
     and if a single byte contains the whole bit field
     change OP0 to a byte.  */

  /* There is a case not handled here:
     a structure with a known alignment of just a halfword
     and a field split across two aligned halfwords within the structure.
     Or likewise a structure with a known alignment of just a byte
     and a field split across two bytes.
     Such cases are not supposed to be able to occur.  */

  if (GET_CODE (op0) == REG || GET_CODE (op0) == SUBREG)
    {
      if (offset != 0)
	abort ();
      /* Special treatment for a bit field split across two registers.  */
      if (bitsize + bitpos > BITS_PER_WORD)
	{
	  store_split_bit_field (op0, bitsize, bitpos, value, BITS_PER_WORD);
	  return;
	}
    }
  else
    {
      /* Get the proper mode to use for this field.  We want a mode that
	 includes the entire field.  If such a mode would be larger than
	 a word, we won't be doing the extraction the normal way.  */

      mode = get_best_mode (bitsize, bitpos + offset * BITS_PER_UNIT,
			    struct_align * BITS_PER_UNIT, word_mode,
			    GET_CODE (op0) == MEM && MEM_VOLATILE_P (op0));

      if (mode == VOIDmode)
	{
	  /* The only way this should occur is if the field spans word
	     boundaries.  */
	  store_split_bit_field (op0, bitsize, bitpos + offset * BITS_PER_UNIT,
				 value, struct_align);
	  return;
	}

      total_bits = GET_MODE_BITSIZE (mode);

      /* Get ref to an aligned byte, halfword, or word containing the field.
	 Adjust BITPOS to be position within a word,
	 and OFFSET to be the offset of that word.
	 Then alter OP0 to refer to that word.  */
      bitpos += (offset % (total_bits / BITS_PER_UNIT)) * BITS_PER_UNIT;
      offset -= (offset % (total_bits / BITS_PER_UNIT));
      op0 = change_address (op0, mode,
			    plus_constant (XEXP (op0, 0), offset));
    }

  mode = GET_MODE (op0);

  /* Now MODE is either some integral mode for a MEM as OP0,
     or is a full-word for a REG as OP0.  TOTAL_BITS corresponds.
     The bit field is contained entirely within OP0.
     BITPOS is the starting bit number within OP0.
     (OP0's mode may actually be narrower than MODE.)  */

#if BYTES_BIG_ENDIAN
  /* BITPOS is the distance between our msb
     and that of the containing datum.
     Convert it to the distance from the lsb.  */

  bitpos = total_bits - bitsize - bitpos;
#endif
  /* Now BITPOS is always the distance between our lsb
     and that of OP0.  */

  /* Shift VALUE left by BITPOS bits.  If VALUE is not constant,
     we must first convert its mode to MODE.  */

  if (GET_CODE (value) == CONST_INT)
    {
      register HOST_WIDE_INT v = INTVAL (value);

      if (bitsize < HOST_BITS_PER_WIDE_INT)
	v &= ((HOST_WIDE_INT) 1 << bitsize) - 1;

      if (v == 0)
	all_zero = 1;
      else if ((bitsize < HOST_BITS_PER_WIDE_INT
		&& v == ((HOST_WIDE_INT) 1 << bitsize) - 1)
	       || (bitsize == HOST_BITS_PER_WIDE_INT && v == -1))
	all_one = 1;

      value = lshift_value (mode, value, bitpos, bitsize);
    }
  else
    {
      int must_and = (GET_MODE_BITSIZE (GET_MODE (value)) != bitsize
		      && bitpos + bitsize != GET_MODE_BITSIZE (mode));

      if (GET_MODE (value) != mode)
	{
	  /* If VALUE is a floating-point mode, access it as an integer
	     of the corresponding size, then convert it.  This can occur on
	     a machine with 64 bit registers that uses SFmode for float.  */
	  if (GET_MODE_CLASS (GET_MODE (value)) == MODE_FLOAT)
	    {
	      if (GET_CODE (value) != REG)
		value = copy_to_reg (value);
	      value
		= gen_rtx (SUBREG, word_mode, value, 0);
	    }

	  if ((GET_CODE (value) == REG || GET_CODE (value) == SUBREG)
	      && GET_MODE_SIZE (mode) < GET_MODE_SIZE (GET_MODE (value)))
	    value = gen_lowpart (mode, value);
	  else
	    value = convert_to_mode (mode, value, 1);
	}

      if (must_and)
	value = expand_binop (mode, and_optab, value,
			      mask_rtx (mode, 0, bitsize, 0),
			      NULL_RTX, 1, OPTAB_LIB_WIDEN);
      if (bitpos > 0)
	value = expand_shift (LSHIFT_EXPR, mode, value,
			      build_int_2 (bitpos, 0), NULL_RTX, 1);
    }

  /* Now clear the chosen bits in OP0,
     except that if VALUE is -1 we need not bother.  */

  subtarget = (GET_CODE (op0) == REG || ! flag_force_mem) ? op0 : 0;

  if (! all_one)
    {
      temp = expand_binop (mode, and_optab, op0,
			   mask_rtx (mode, bitpos, bitsize, 1),
			   subtarget, 1, OPTAB_LIB_WIDEN);
      subtarget = temp;
    }
  else
    temp = op0;

  /* Now logical-or VALUE into OP0, unless it is zero.  */

  if (! all_zero)
    temp = expand_binop (mode, ior_optab, temp, value,
			 subtarget, 1, OPTAB_LIB_WIDEN);
  if (op0 != temp)
    emit_move_insn (op0, temp);
}

/* Store a bit field that is split across two words.

   OP0 is the REG, SUBREG or MEM rtx for the first of the two words.
   BITSIZE is the field width; BITPOS the position of its first bit
   (within the word).
   VALUE is the value to store.  */

static void
store_split_bit_field (op0, bitsize, bitpos, value, align)
     rtx op0;
     int bitsize, bitpos;
     rtx value;
     int align;
{
  /* BITSIZE_1 is size of the part in the first word.  */
  int bitsize_1 = BITS_PER_WORD - bitpos % BITS_PER_WORD;
  /* BITSIZE_2 is size of the rest (in the following word).  */
  int bitsize_2 = bitsize - bitsize_1;
  rtx part1, part2;
  int unit = GET_CODE (op0) == MEM ? BITS_PER_UNIT : BITS_PER_WORD;
  int offset = bitpos / unit;
  rtx word;

  /* The field must span exactly one word boundary.  */
  if (bitpos / BITS_PER_WORD != (bitpos + bitsize - 1) / BITS_PER_WORD - 1)
    abort ();

  if (GET_MODE (value) != VOIDmode)
    value = convert_to_mode (word_mode, value, 1);

  if (GET_CODE (value) == CONST_DOUBLE
      && (part1 = gen_lowpart_common (word_mode, value)) != 0)
    value = part1;

  if (CONSTANT_P (value) && GET_CODE (value) != CONST_INT)
    value = copy_to_mode_reg (word_mode, value);

  /* Split the value into two parts:
     PART1 gets that which goes in the first word; PART2 the other.  */
#if BYTES_BIG_ENDIAN
  /* PART1 gets the more significant part.  */
  if (GET_CODE (value) == CONST_INT)
    {
      part1 = GEN_INT ((unsigned HOST_WIDE_INT) (INTVAL (value)) >> bitsize_2);
      part2 = GEN_INT ((unsigned HOST_WIDE_INT) (INTVAL (value))
		       & (((HOST_WIDE_INT) 1 << bitsize_2) - 1));
    }
  else
    {
      part1 = extract_fixed_bit_field (word_mode, value, 0, bitsize_1,
				       BITS_PER_WORD - bitsize, NULL_RTX, 1,
				       BITS_PER_WORD);
      part2 = extract_fixed_bit_field (word_mode, value, 0, bitsize_2,
				       BITS_PER_WORD - bitsize_2, NULL_RTX, 1,
				       BITS_PER_WORD);
    }
#else
  /* PART1 gets the less significant part.  */
  if (GET_CODE (value) == CONST_INT)
    {
      part1 = GEN_INT ((unsigned HOST_WIDE_INT) (INTVAL (value))
		       & (((HOST_WIDE_INT) 1 << bitsize_1) - 1));
      part2 = GEN_INT ((unsigned HOST_WIDE_INT) (INTVAL (value)) >> bitsize_1);
    }
  else
    {
      part1 = extract_fixed_bit_field (word_mode, value, 0, bitsize_1, 0,
				       NULL_RTX, 1, BITS_PER_WORD);
      part2 = extract_fixed_bit_field (word_mode, value, 0, bitsize_2,
				       bitsize_1, NULL_RTX, 1, BITS_PER_WORD);
    }
#endif

  /* Store PART1 into the first word.  If OP0 is a MEM, pass OP0 and the
     offset computed above.  Otherwise, get the proper word and pass an
     offset of zero.  */
  word = (GET_CODE (op0) == MEM ? op0
	  : operand_subword (op0, offset, 1, GET_MODE (op0)));
  if (word == 0)
    abort ();

  store_fixed_bit_field (word, GET_CODE (op0) == MEM ? offset : 0,
			 bitsize_1, bitpos % unit, part1, align);

  /* Offset op0 by 1 word to get to the following one.  */
  if (GET_CODE (op0) == SUBREG)
    word = operand_subword (SUBREG_REG (op0), SUBREG_WORD (op0) + offset + 1,
			    1, VOIDmode);
  else if (GET_CODE (op0) == MEM)
    word = op0;
  else
    word = operand_subword (op0, offset + 1, 1, GET_MODE (op0));

  if (word == 0)
    abort ();

  /* Store PART2 into the second word.  */
  store_fixed_bit_field (word,
			 (GET_CODE (op0) == MEM
			  ? CEIL (offset + 1, UNITS_PER_WORD) * UNITS_PER_WORD
			  : 0),
			 bitsize_2, 0, part2, align);
}

/* Generate code to extract a byte-field from STR_RTX
   containing BITSIZE bits, starting at BITNUM,
   and put it in TARGET if possible (if TARGET is nonzero).
   Regardless of TARGET, we return the rtx for where the value is placed.
   It may be a QUEUED.

   STR_RTX is the structure containing the byte (a REG or MEM).
   UNSIGNEDP is nonzero if this is an unsigned bit field.
   MODE is the natural mode of the field value once extracted.
   TMODE is the mode the caller would like the value to have;
   but the value may be returned with type MODE instead.

   ALIGN is the alignment that STR_RTX is known to have, measured in bytes.
   TOTAL_SIZE is the size in bytes of the containing structure,
   or -1 if varying.

   If a TARGET is specified and we can store in it at no extra cost,
   we do so, and return TARGET.
   Otherwise, we return a REG of mode TMODE or MODE, with TMODE preferred
   if they are equally easy.  */

rtx
extract_bit_field (str_rtx, bitsize, bitnum, unsignedp,
		   target, mode, tmode, align, total_size)
     rtx str_rtx;
     register int bitsize;
     int bitnum;
     int unsignedp;
     rtx target;
     enum machine_mode mode, tmode;
     int align;
     int total_size;
{
  int unit = (GET_CODE (str_rtx) == MEM) ? BITS_PER_UNIT : BITS_PER_WORD;
  register int offset = bitnum / unit;
  register int bitpos = bitnum % unit;
  register rtx op0 = str_rtx;
  rtx spec_target = target;
  rtx spec_target_subreg = 0;

  if (GET_CODE (str_rtx) == MEM && ! MEM_IN_STRUCT_P (str_rtx))
    abort ();

  /* Discount the part of the structure before the desired byte.
     We need to know how many bytes are safe to reference after it.  */
  if (total_size >= 0)
    total_size -= (bitpos / BIGGEST_ALIGNMENT
		   * (BIGGEST_ALIGNMENT / BITS_PER_UNIT));

  if (tmode == VOIDmode)
    tmode = mode;
  while (GET_CODE (op0) == SUBREG)
    {
      offset += SUBREG_WORD (op0);
      op0 = SUBREG_REG (op0);
    }
  
#if BYTES_BIG_ENDIAN
  /* If OP0 is a register, BITPOS must count within a word.
     But as we have it, it counts within whatever size OP0 now has.
     On a bigendian machine, these are not the same, so convert.  */
  if (GET_CODE (op0) != MEM && unit > GET_MODE_BITSIZE (GET_MODE (op0)))
    bitpos += unit - GET_MODE_BITSIZE (GET_MODE (op0));
#endif

  /* Extracting a full-word or multi-word value
     from a structure in a register.
     This can be done with just SUBREG.
     So too extracting a subword value in
     the least significant part of the register.  */

  if (GET_CODE (op0) == REG
      && ((bitsize >= BITS_PER_WORD && bitsize == GET_MODE_BITSIZE (mode)
	   && bitpos % BITS_PER_WORD == 0)
	  || (mode_for_size (bitsize, GET_MODE_CLASS (tmode), 0) != BLKmode
#if BYTES_BIG_ENDIAN
	      && bitpos + bitsize == BITS_PER_WORD
#else
	      && bitpos == 0
#endif
	      )))
    {
      enum machine_mode mode1
	= mode_for_size (bitsize, GET_MODE_CLASS (tmode), 0);

      if (mode1 != GET_MODE (op0))
	op0 = gen_rtx (SUBREG, mode1, op0, offset);

      if (mode1 != mode)
	return convert_to_mode (tmode, op0, unsignedp);
      return op0;
    }

  /* Handle fields bigger than a word.  */
  
  if (bitsize > BITS_PER_WORD)
    {
      /* Here we transfer the words of the field
	 in the order least significant first.
	 This is because the most significant word is the one which may
	 be less than full.  */

      int nwords = (bitsize + (BITS_PER_WORD - 1)) / BITS_PER_WORD;
      int i;

      if (target == 0 || GET_CODE (target) != REG)
	target = gen_reg_rtx (mode);

      for (i = 0; i < nwords; i++)
	{
	  /* If I is 0, use the low-order word in both field and target;
	     if I is 1, use the next to lowest word; and so on.  */
	  int wordnum = (WORDS_BIG_ENDIAN ? nwords - i - 1 : i);
	  int bit_offset = (WORDS_BIG_ENDIAN
			    ? MAX (0, bitsize - (i + 1) * BITS_PER_WORD)
			    : i * BITS_PER_WORD);
	  rtx target_part = operand_subword (target, wordnum, 1, VOIDmode);
	  rtx result_part
	    = extract_bit_field (op0, MIN (BITS_PER_WORD,
					   bitsize - i * BITS_PER_WORD),
				 bitnum + bit_offset,
				 1, target_part, mode, word_mode,
				 align, total_size);

	  if (target_part == 0)
	    abort ();

	  if (result_part != target_part)
	    emit_move_insn (target_part, result_part);
	}

      return target;
    }
  
  /* From here on we know the desired field is smaller than a word
     so we can assume it is an integer.  So we can safely extract it as one
     size of integer, if necessary, and then truncate or extend
     to the size that is wanted.  */

  /* OFFSET is the number of words or bytes (UNIT says which)
     from STR_RTX to the first word or byte containing part of the field.  */

  if (GET_CODE (op0) == REG)
    {
      if (offset != 0
	  || GET_MODE_SIZE (GET_MODE (op0)) > UNITS_PER_WORD)
	op0 = gen_rtx (SUBREG, TYPE_MODE (type_for_size (BITS_PER_WORD, 0)),
		       op0, offset);
      offset = 0;
    }
  else
    {
      op0 = protect_from_queue (str_rtx, 1);
    }

  /* Now OFFSET is nonzero only for memory operands.  */

  if (unsignedp)
    {
#ifdef HAVE_extzv
      if (HAVE_extzv
	  && (GET_MODE_BITSIZE (insn_operand_mode[(int) CODE_FOR_extzv][0])
	      >= bitsize))
	{
	  int xbitpos = bitpos, xoffset = offset;
	  rtx bitsize_rtx, bitpos_rtx;
	  rtx last = get_last_insn();
	  rtx xop0 = op0;
	  rtx xtarget = target;
	  rtx xspec_target = spec_target;
	  rtx xspec_target_subreg = spec_target_subreg;
	  rtx pat;
	  enum machine_mode maxmode
	    = insn_operand_mode[(int) CODE_FOR_extzv][0];

	  if (GET_CODE (xop0) == MEM)
	    {
	      int save_volatile_ok = volatile_ok;
	      volatile_ok = 1;

	      /* Is the memory operand acceptable?  */
	      if (flag_force_mem
		  || ! ((*insn_operand_predicate[(int) CODE_FOR_extzv][1])
			(xop0, GET_MODE (xop0))))
		{
		  /* No, load into a reg and extract from there.  */
		  enum machine_mode bestmode;

		  /* Get the mode to use for inserting into this field.  If
		     OP0 is BLKmode, get the smallest mode consistent with the
		     alignment. If OP0 is a non-BLKmode object that is no
		     wider than MAXMODE, use its mode. Otherwise, use the
		     smallest mode containing the field.  */

		  if (GET_MODE (xop0) == BLKmode
		      || (GET_MODE_SIZE (GET_MODE (op0))
			  > GET_MODE_SIZE (maxmode)))
		    bestmode = get_best_mode (bitsize, bitnum,
					      align * BITS_PER_UNIT, maxmode,
					      MEM_VOLATILE_P (xop0));
		  else
		    bestmode = GET_MODE (xop0);

		  if (bestmode == VOIDmode)
		    goto extzv_loses;

		  /* Compute offset as multiple of this unit,
		     counting in bytes.  */
		  unit = GET_MODE_BITSIZE (bestmode);
		  xoffset = (bitnum / unit) * GET_MODE_SIZE (bestmode);
		  xbitpos = bitnum % unit;
		  xop0 = change_address (xop0, bestmode,
					 plus_constant (XEXP (xop0, 0),
							xoffset));
		  /* Fetch it to a register in that size.  */
		  xop0 = force_reg (bestmode, xop0);

		  /* XBITPOS counts within UNIT, which is what is expected.  */
		}
	      else
		/* Get ref to first byte containing part of the field.  */
		xop0 = change_address (xop0, byte_mode,
				       plus_constant (XEXP (xop0, 0), xoffset));

	      volatile_ok = save_volatile_ok;
	    }

	  /* If op0 is a register, we need it in MAXMODE (which is usually
	     SImode). to make it acceptable to the format of extzv.  */
	  if (GET_CODE (xop0) == SUBREG && GET_MODE (xop0) != maxmode)
	    abort ();
	  if (GET_CODE (xop0) == REG && GET_MODE (xop0) != maxmode)
	    xop0 = gen_rtx (SUBREG, maxmode, xop0, 0);

	  /* On big-endian machines, we count bits from the most significant.
	     If the bit field insn does not, we must invert.  */
#if BITS_BIG_ENDIAN != BYTES_BIG_ENDIAN
	  xbitpos = unit - bitsize - xbitpos;
#endif
	  /* Now convert from counting within UNIT to counting in MAXMODE.  */
#if BITS_BIG_ENDIAN
	  if (GET_CODE (xop0) != MEM)
	    xbitpos += GET_MODE_BITSIZE (maxmode) - unit;
#endif
	  unit = GET_MODE_BITSIZE (maxmode);

	  if (xtarget == 0
	      || (flag_force_mem && GET_CODE (xtarget) == MEM))
	    xtarget = xspec_target = gen_reg_rtx (tmode);

	  if (GET_MODE (xtarget) != maxmode)
	    {
	      if (GET_CODE (xtarget) == REG)
		{
		  int wider = (GET_MODE_SIZE (maxmode)
			       > GET_MODE_SIZE (GET_MODE (xtarget)));
		  xtarget = gen_lowpart (maxmode, xtarget);
		  if (wider)
		    xspec_target_subreg = xtarget;
		}
	      else
		xtarget = gen_reg_rtx (maxmode);
	    }

	  /* If this machine's extzv insists on a register target,
	     make sure we have one.  */
	  if (! ((*insn_operand_predicate[(int) CODE_FOR_extzv][0])
		 (xtarget, maxmode)))
	    xtarget = gen_reg_rtx (maxmode);

	  bitsize_rtx = GEN_INT (bitsize);
	  bitpos_rtx = GEN_INT (xbitpos);

	  pat = gen_extzv (protect_from_queue (xtarget, 1),
			   xop0, bitsize_rtx, bitpos_rtx);
	  if (pat)
	    {
	      emit_insn (pat);
	      target = xtarget;
	      spec_target = xspec_target;
	      spec_target_subreg = xspec_target_subreg;
	    }
	  else
	    {
	      delete_insns_since (last);
	      target = extract_fixed_bit_field (tmode, op0, offset, bitsize,
						bitpos, target, 1, align);
	    }
	}
      else
        extzv_loses:
#endif
	target = extract_fixed_bit_field (tmode, op0, offset, bitsize, bitpos,
					  target, 1, align);
    }
  else
    {
#ifdef HAVE_extv
      if (HAVE_extv
	  && (GET_MODE_BITSIZE (insn_operand_mode[(int) CODE_FOR_extv][0])
	      >= bitsize))
	{
	  int xbitpos = bitpos, xoffset = offset;
	  rtx bitsize_rtx, bitpos_rtx;
	  rtx last = get_last_insn();
	  rtx xop0 = op0, xtarget = target;
	  rtx xspec_target = spec_target;
	  rtx xspec_target_subreg = spec_target_subreg;
	  rtx pat;
	  enum machine_mode maxmode
	    = insn_operand_mode[(int) CODE_FOR_extv][0];

	  if (GET_CODE (xop0) == MEM)
	    {
	      /* Is the memory operand acceptable?  */
	      if (! ((*insn_operand_predicate[(int) CODE_FOR_extv][1])
		     (xop0, GET_MODE (xop0))))
		{
		  /* No, load into a reg and extract from there.  */
		  enum machine_mode bestmode;

		  /* Get the mode to use for inserting into this field.  If
		     OP0 is BLKmode, get the smallest mode consistent with the
		     alignment. If OP0 is a non-BLKmode object that is no
		     wider than MAXMODE, use its mode. Otherwise, use the
		     smallest mode containing the field.  */

		  if (GET_MODE (xop0) == BLKmode
		      || (GET_MODE_SIZE (GET_MODE (op0))
			  > GET_MODE_SIZE (maxmode)))
		    bestmode = get_best_mode (bitsize, bitnum,
					      align * BITS_PER_UNIT, maxmode,
					      MEM_VOLATILE_P (xop0));
		  else
		    bestmode = GET_MODE (xop0);

		  if (bestmode == VOIDmode)
		    goto extv_loses;

		  /* Compute offset as multiple of this unit,
		     counting in bytes.  */
		  unit = GET_MODE_BITSIZE (bestmode);
		  xoffset = (bitnum / unit) * GET_MODE_SIZE (bestmode);
		  xbitpos = bitnum % unit;
		  xop0 = change_address (xop0, bestmode,
					 plus_constant (XEXP (xop0, 0),
							xoffset));
		  /* Fetch it to a register in that size.  */
		  xop0 = force_reg (bestmode, xop0);

		  /* XBITPOS counts within UNIT, which is what is expected.  */
		}
	      else
		/* Get ref to first byte containing part of the field.  */
		xop0 = change_address (xop0, byte_mode,
				       plus_constant (XEXP (xop0, 0), xoffset));
	    }

	  /* If op0 is a register, we need it in MAXMODE (which is usually
	     SImode) to make it acceptable to the format of extv.  */
	  if (GET_CODE (xop0) == SUBREG && GET_MODE (xop0) != maxmode)
	    abort ();
	  if (GET_CODE (xop0) == REG && GET_MODE (xop0) != maxmode)
	    xop0 = gen_rtx (SUBREG, maxmode, xop0, 0);

	  /* On big-endian machines, we count bits from the most significant.
	     If the bit field insn does not, we must invert.  */
#if BITS_BIG_ENDIAN != BYTES_BIG_ENDIAN
	  xbitpos = unit - bitsize - xbitpos;
#endif
	  /* XBITPOS counts within a size of UNIT.
	     Adjust to count within a size of MAXMODE.  */
#if BITS_BIG_ENDIAN
	  if (GET_CODE (xop0) != MEM)
	    xbitpos += (GET_MODE_BITSIZE (maxmode) - unit);
#endif
	  unit = GET_MODE_BITSIZE (maxmode);

	  if (xtarget == 0
	      || (flag_force_mem && GET_CODE (xtarget) == MEM))
	    xtarget = xspec_target = gen_reg_rtx (tmode);

	  if (GET_MODE (xtarget) != maxmode)
	    {
	      if (GET_CODE (xtarget) == REG)
		{
		  int wider = (GET_MODE_SIZE (maxmode)
			       > GET_MODE_SIZE (GET_MODE (xtarget)));
		  xtarget = gen_lowpart (maxmode, xtarget);
		  if (wider)
		    xspec_target_subreg = xtarget;
		}
	      else
		xtarget = gen_reg_rtx (maxmode);
	    }

	  /* If this machine's extv insists on a register target,
	     make sure we have one.  */
	  if (! ((*insn_operand_predicate[(int) CODE_FOR_extv][0])
		 (xtarget, maxmode)))
	    xtarget = gen_reg_rtx (maxmode);

	  bitsize_rtx = GEN_INT (bitsize);
	  bitpos_rtx = GEN_INT (xbitpos);

	  pat = gen_extv (protect_from_queue (xtarget, 1),
			  xop0, bitsize_rtx, bitpos_rtx);
	  if (pat)
	    {
	      emit_insn (pat);
	      target = xtarget;
	      spec_target = xspec_target;
	      spec_target_subreg = xspec_target_subreg;
	    }
	  else
	    {
	      delete_insns_since (last);
	      target = extract_fixed_bit_field (tmode, op0, offset, bitsize,
						bitpos, target, 0, align);
	    }
	} 
      else
	extv_loses:
#endif
	target = extract_fixed_bit_field (tmode, op0, offset, bitsize, bitpos,
					  target, 0, align);
    }
  if (target == spec_target)
    return target;
  if (target == spec_target_subreg)
    return spec_target;
  if (GET_MODE (target) != tmode && GET_MODE (target) != mode)
    {
      /* If the target mode is floating-point, first convert to the
	 integer mode of that size and then access it as a floating-point
	 value via a SUBREG.  */
      if (GET_MODE_CLASS (tmode) == MODE_FLOAT)
	{
	  target = convert_to_mode (mode_for_size (GET_MODE_BITSIZE (tmode),
						   MODE_INT, 0),
				    target, unsignedp);
	  if (GET_CODE (target) != REG)
	    target = copy_to_reg (target);
	  return gen_rtx (SUBREG, tmode, target, 0);
	}
      else
	return convert_to_mode (tmode, target, unsignedp);
    }
  return target;
}

/* Extract a bit field using shifts and boolean operations
   Returns an rtx to represent the value.
   OP0 addresses a register (word) or memory (byte).
   BITPOS says which bit within the word or byte the bit field starts in.
   OFFSET says how many bytes farther the bit field starts;
    it is 0 if OP0 is a register.
   BITSIZE says how many bits long the bit field is.
    (If OP0 is a register, it may be narrower than a full word,
     but BITPOS still counts within a full word,
     which is significant on bigendian machines.)

   UNSIGNEDP is nonzero for an unsigned bit field (don't sign-extend value).
   If TARGET is nonzero, attempts to store the value there
   and return TARGET, but this is not guaranteed.
   If TARGET is not used, create a pseudo-reg of mode TMODE for the value.

   ALIGN is the alignment that STR_RTX is known to have, measured in bytes.  */

static rtx
extract_fixed_bit_field (tmode, op0, offset, bitsize, bitpos,
			 target, unsignedp, align)
     enum machine_mode tmode;
     register rtx op0, target;
     register int offset, bitsize, bitpos;
     int unsignedp;
     int align;
{
  int total_bits = BITS_PER_WORD;
  enum machine_mode mode;

  if (GET_CODE (op0) == SUBREG || GET_CODE (op0) == REG)
    {
      /* Special treatment for a bit field split across two registers.  */
      if (bitsize + bitpos > BITS_PER_WORD)
	return extract_split_bit_field (op0, bitsize, bitpos,
					unsignedp, align);
    }
  else
    {
      /* Get the proper mode to use for this field.  We want a mode that
	 includes the entire field.  If such a mode would be larger than
	 a word, we won't be doing the extraction the normal way.  */

      mode = get_best_mode (bitsize, bitpos + offset * BITS_PER_UNIT,
			    align * BITS_PER_UNIT, word_mode,
			    GET_CODE (op0) == MEM && MEM_VOLATILE_P (op0));

      if (mode == VOIDmode)
	/* The only way this should occur is if the field spans word
	   boundaries.  */
	return extract_split_bit_field (op0, bitsize,
					bitpos + offset * BITS_PER_UNIT,
					unsignedp, align);

      total_bits = GET_MODE_BITSIZE (mode);

      /* Make sure bitpos is valid for the chosen mode.  Adjust BITPOS to
	 be be in the range 0 to total_bits-1, and put any excess bytes in
	 OFFSET.  */
      if (bitpos >= total_bits)
	{
	  offset += (bitpos / total_bits) * (total_bits / BITS_PER_UNIT);
	  bitpos -= ((bitpos / total_bits) * (total_bits / BITS_PER_UNIT)
		     * BITS_PER_UNIT);
	}

      /* Get ref to an aligned byte, halfword, or word containing the field.
	 Adjust BITPOS to be position within a word,
	 and OFFSET to be the offset of that word.
	 Then alter OP0 to refer to that word.  */
      bitpos += (offset % (total_bits / BITS_PER_UNIT)) * BITS_PER_UNIT;
      offset -= (offset % (total_bits / BITS_PER_UNIT));
      op0 = change_address (op0, mode,
			    plus_constant (XEXP (op0, 0), offset));
    }

  mode = GET_MODE (op0);

#if BYTES_BIG_ENDIAN
  /* BITPOS is the distance between our msb and that of OP0.
     Convert it to the distance from the lsb.  */

  bitpos = total_bits - bitsize - bitpos;
#endif
  /* Now BITPOS is always the distance between the field's lsb and that of OP0.
     We have reduced the big-endian case to the little-endian case.  */

  if (unsignedp)
    {
      if (bitpos)
	{
	  /* If the field does not already start at the lsb,
	     shift it so it does.  */
	  tree amount = build_int_2 (bitpos, 0);
	  /* Maybe propagate the target for the shift.  */
	  /* But not if we will return it--could confuse integrate.c.  */
	  rtx subtarget = (target != 0 && GET_CODE (target) == REG
			   && !REG_FUNCTION_VALUE_P (target)
			   ? target : 0);
	  if (tmode != mode) subtarget = 0;
	  op0 = expand_shift (RSHIFT_EXPR, mode, op0, amount, subtarget, 1);
	}
      /* Convert the value to the desired mode.  */
      if (mode != tmode)
	op0 = convert_to_mode (tmode, op0, 1);

      /* Unless the msb of the field used to be the msb when we shifted,
	 mask out the upper bits.  */

      if (GET_MODE_BITSIZE (mode) != bitpos + bitsize
#if 0
#ifdef SLOW_ZERO_EXTEND
	  /* Always generate an `and' if
	     we just zero-extended op0 and SLOW_ZERO_EXTEND, since it
	     will combine fruitfully with the zero-extend. */
	  || tmode != mode
#endif
#endif
	  )
	return expand_binop (GET_MODE (op0), and_optab, op0,
			     mask_rtx (GET_MODE (op0), 0, bitsize, 0),
			     target, 1, OPTAB_LIB_WIDEN);
      return op0;
    }

  /* To extract a signed bit-field, first shift its msb to the msb of the word,
     then arithmetic-shift its lsb to the lsb of the word.  */
  op0 = force_reg (mode, op0);
  if (mode != tmode)
    target = 0;

  /* Find the narrowest integer mode that contains the field.  */

  for (mode = GET_CLASS_NARROWEST_MODE (MODE_INT); mode != VOIDmode;
       mode = GET_MODE_WIDER_MODE (mode))
    if (GET_MODE_BITSIZE (mode) >= bitsize + bitpos)
      {
	op0 = convert_to_mode (mode, op0, 0);
	break;
      }

  if (GET_MODE_BITSIZE (mode) != (bitsize + bitpos))
    {
      tree amount = build_int_2 (GET_MODE_BITSIZE (mode) - (bitsize + bitpos), 0);
      /* Maybe propagate the target for the shift.  */
      /* But not if we will return the result--could confuse integrate.c.  */
      rtx subtarget = (target != 0 && GET_CODE (target) == REG
		       && ! REG_FUNCTION_VALUE_P (target)
		       ? target : 0);
      op0 = expand_shift (LSHIFT_EXPR, mode, op0, amount, subtarget, 1);
    }

  return expand_shift (RSHIFT_EXPR, mode, op0,
		       build_int_2 (GET_MODE_BITSIZE (mode) - bitsize, 0), 
		       target, 0);
}

/* Return a constant integer (CONST_INT or CONST_DOUBLE) mask value
   of mode MODE with BITSIZE ones followed by BITPOS zeros, or the
   complement of that if COMPLEMENT.  The mask is truncated if
   necessary to the width of mode MODE.  */

static rtx
mask_rtx (mode, bitpos, bitsize, complement)
     enum machine_mode mode;
     int bitpos, bitsize, complement;
{
  HOST_WIDE_INT masklow, maskhigh;

  if (bitpos < HOST_BITS_PER_WIDE_INT)
    masklow = (HOST_WIDE_INT) -1 << bitpos;
  else
    masklow = 0;

  if (bitpos + bitsize < HOST_BITS_PER_WIDE_INT)
    masklow &= ((unsigned HOST_WIDE_INT) -1
		>> (HOST_BITS_PER_WIDE_INT - bitpos - bitsize));
  
  if (bitpos <= HOST_BITS_PER_WIDE_INT)
    maskhigh = -1;
  else
    maskhigh = (HOST_WIDE_INT) -1 << (bitpos - HOST_BITS_PER_WIDE_INT);

  if (bitpos + bitsize > HOST_BITS_PER_WIDE_INT)
    maskhigh &= ((unsigned HOST_WIDE_INT) -1
		 >> (2 * HOST_BITS_PER_WIDE_INT - bitpos - bitsize));
  else
    maskhigh = 0;

  if (complement)
    {
      maskhigh = ~maskhigh;
      masklow = ~masklow;
    }

  return immed_double_const (masklow, maskhigh, mode);
}

/* Return a constant integer (CONST_INT or CONST_DOUBLE) rtx with the value
   VALUE truncated to BITSIZE bits and then shifted left BITPOS bits.  */

static rtx
lshift_value (mode, value, bitpos, bitsize)
     enum machine_mode mode;
     rtx value;
     int bitpos, bitsize;
{
  unsigned HOST_WIDE_INT v = INTVAL (value);
  HOST_WIDE_INT low, high;

  if (bitsize < HOST_BITS_PER_WIDE_INT)
    v &= ~((HOST_WIDE_INT) -1 << bitsize);

  if (bitpos < HOST_BITS_PER_WIDE_INT)
    {
      low = v << bitpos;
      high = (bitpos > 0 ? (v >> (HOST_BITS_PER_WIDE_INT - bitpos)) : 0);
    }
  else
    {
      low = 0;
      high = v << (bitpos - HOST_BITS_PER_WIDE_INT);
    }

  return immed_double_const (low, high, mode);
}

/* Extract a bit field that is split across two words
   and return an RTX for the result.

   OP0 is the REG, SUBREG or MEM rtx for the first of the two words.
   BITSIZE is the field width; BITPOS, position of its first bit, in the word.
   UNSIGNEDP is 1 if should zero-extend the contents; else sign-extend.  */

static rtx
extract_split_bit_field (op0, bitsize, bitpos, unsignedp, align)
     rtx op0;
     int bitsize, bitpos, unsignedp, align;
{
  /* BITSIZE_1 is size of the part in the first word.  */
  int bitsize_1 = BITS_PER_WORD - bitpos % BITS_PER_WORD;
  /* BITSIZE_2 is size of the rest (in the following word).  */
  int bitsize_2 = bitsize - bitsize_1;
  rtx part1, part2, result;
  int unit = GET_CODE (op0) == MEM ? BITS_PER_UNIT : BITS_PER_WORD;
  int offset = bitpos / unit;
  rtx word;
 
  /* The field must span exactly one word boundary.  */
  if (bitpos / BITS_PER_WORD != (bitpos + bitsize - 1) / BITS_PER_WORD - 1)
    abort ();

  /* Get the part of the bit field from the first word.  If OP0 is a MEM,
     pass OP0 and the offset computed above.  Otherwise, get the proper
     word and pass an offset of zero.  */
  word = (GET_CODE (op0) == MEM ? op0
	  : operand_subword_force (op0, offset, GET_MODE (op0)));
  part1 = extract_fixed_bit_field (word_mode, word,
				   GET_CODE (op0) == MEM ? offset : 0,
				   bitsize_1, bitpos % unit, NULL_RTX,
				   1, align);

  /* Offset op0 by 1 word to get to the following one.  */
  if (GET_CODE (op0) == SUBREG)
    word = operand_subword_force (SUBREG_REG (op0),
				  SUBREG_WORD (op0) + offset + 1, VOIDmode);
  else if (GET_CODE (op0) == MEM)
    word = op0;
  else
    word = operand_subword_force (op0, offset + 1, GET_MODE (op0));

  /* Get the part of the bit field from the second word.  */
  part2 = extract_fixed_bit_field (word_mode, word,
				   (GET_CODE (op0) == MEM
				    ? CEIL (offset + 1, UNITS_PER_WORD) * UNITS_PER_WORD
				    : 0),
				   bitsize_2, 0, NULL_RTX, 1, align);

  /* Shift the more significant part up to fit above the other part.  */
#if BYTES_BIG_ENDIAN
  part1 = expand_shift (LSHIFT_EXPR, word_mode, part1,
			build_int_2 (bitsize_2, 0), 0, 1);
#else
  part2 = expand_shift (LSHIFT_EXPR, word_mode, part2,
			build_int_2 (bitsize_1, 0), 0, 1);
#endif

  /* Combine the two parts with bitwise or.  This works
     because we extracted both parts as unsigned bit fields.  */
  result = expand_binop (word_mode, ior_optab, part1, part2, NULL_RTX, 1,
			 OPTAB_LIB_WIDEN);

  /* Unsigned bit field: we are done.  */
  if (unsignedp)
    return result;
  /* Signed bit field: sign-extend with two arithmetic shifts.  */
  result = expand_shift (LSHIFT_EXPR, word_mode, result,
			 build_int_2 (BITS_PER_WORD - bitsize, 0),
			 NULL_RTX, 0);
  return expand_shift (RSHIFT_EXPR, word_mode, result,
		       build_int_2 (BITS_PER_WORD - bitsize, 0), NULL_RTX, 0);
}

/* Add INC into TARGET.  */

void
expand_inc (target, inc)
     rtx target, inc;
{
  rtx value = expand_binop (GET_MODE (target), add_optab,
			    target, inc,
			    target, 0, OPTAB_LIB_WIDEN);
  if (value != target)
    emit_move_insn (target, value);
}

/* Subtract DEC from TARGET.  */

void
expand_dec (target, dec)
     rtx target, dec;
{
  rtx value = expand_binop (GET_MODE (target), sub_optab,
			    target, dec,
			    target, 0, OPTAB_LIB_WIDEN);
  if (value != target)
    emit_move_insn (target, value);
}

/* Output a shift instruction for expression code CODE,
   with SHIFTED being the rtx for the value to shift,
   and AMOUNT the tree for the amount to shift by.
   Store the result in the rtx TARGET, if that is convenient.
   If UNSIGNEDP is nonzero, do a logical shift; otherwise, arithmetic.
   Return the rtx for where the value is.  */

rtx
expand_shift (code, mode, shifted, amount, target, unsignedp)
     enum tree_code code;
     register enum machine_mode mode;
     rtx shifted;
     tree amount;
     register rtx target;
     int unsignedp;
{
  register rtx op1, temp = 0;
  register int left = (code == LSHIFT_EXPR || code == LROTATE_EXPR);
  register int rotate = (code == LROTATE_EXPR || code == RROTATE_EXPR);
  int try;

  /* Previously detected shift-counts computed by NEGATE_EXPR
     and shifted in the other direction; but that does not work
     on all machines.  */

  op1 = expand_expr (amount, NULL_RTX, VOIDmode, 0);

  if (op1 == const0_rtx)
    return shifted;

  for (try = 0; temp == 0 && try < 3; try++)
    {
      enum optab_methods methods;

      if (try == 0)
	methods = OPTAB_DIRECT;
      else if (try == 1)
	methods = OPTAB_WIDEN;
      else
	methods = OPTAB_LIB_WIDEN;

      if (rotate)
	{
	  /* Widening does not work for rotation.  */
	  if (methods == OPTAB_WIDEN)
	    continue;
	  else if (methods == OPTAB_LIB_WIDEN)
	    {
	      /* If we are rotating by a constant that is valid and
		 we have been unable to open-code this by a rotation,
		 do it as the IOR of two shifts.  I.e., to rotate A
		 by N bits, compute (A << N) | ((unsigned) A >> (C - N))
		 where C is the bitsize of A.

		 It is theoretically possible that the target machine might
		 not be able to perform either shift and hence we would
		 be making two libcalls rather than just the one for the
		 shift (similarly if IOR could not be done).  We will allow
		 this extremely unlikely lossage to avoid complicating the
		 code below.  */

	      if (GET_CODE (op1) == CONST_INT && INTVAL (op1) > 0
		  && INTVAL (op1) < GET_MODE_BITSIZE (mode))
		{
		  rtx subtarget = target == shifted ? 0 : target;
		  rtx temp1;
		  tree other_amount
		    = build_int_2 (GET_MODE_BITSIZE (mode) - INTVAL (op1), 0);

		  shifted = force_reg (mode, shifted);

		  temp = expand_shift (left ? LSHIFT_EXPR : RSHIFT_EXPR,
				       mode, shifted, amount, subtarget, 1);
		  temp1 = expand_shift (left ? RSHIFT_EXPR : LSHIFT_EXPR,
					mode, shifted, other_amount, 0, 1);
		  return expand_binop (mode, ior_optab, temp, temp1, target,
				       unsignedp, methods);
		}
	      else
		methods = OPTAB_LIB;
	    }

	  temp = expand_binop (mode,
			       left ? rotl_optab : rotr_optab,
			       shifted, op1, target, unsignedp, methods);

	  /* If we don't have the rotate, but we are rotating by a constant
	     that is in range, try a rotate in the opposite direction.  */

	  if (temp == 0 && GET_CODE (op1) == CONST_INT
	      && INTVAL (op1) > 0 && INTVAL (op1) < GET_MODE_BITSIZE (mode))
	    temp = expand_binop (mode,
				 left ? rotr_optab : rotl_optab,
				 shifted, 
				 GEN_INT (GET_MODE_BITSIZE (mode)
					  - INTVAL (op1)),
				 target, unsignedp, methods);
	}
      else if (unsignedp)
	{
	  temp = expand_binop (mode,
			       left ? lshl_optab : lshr_optab,
			       shifted, op1, target, unsignedp, methods);
	  if (temp == 0 && left)
	    temp = expand_binop (mode, ashl_optab,
				 shifted, op1, target, unsignedp, methods);
	}

      /* Do arithmetic shifts.
	 Also, if we are going to widen the operand, we can just as well
	 use an arithmetic right-shift instead of a logical one.  */
      if (temp == 0 && ! rotate
	  && (! unsignedp || (! left && methods == OPTAB_WIDEN)))
	{
	  enum optab_methods methods1 = methods;

	  /* If trying to widen a log shift to an arithmetic shift,
	     don't accept an arithmetic shift of the same size.  */
	  if (unsignedp)
	    methods1 = OPTAB_MUST_WIDEN;

	  /* Arithmetic shift */

	  temp = expand_binop (mode,
			       left ? ashl_optab : ashr_optab,
			       shifted, op1, target, unsignedp, methods1);
	}

#ifdef HAVE_extzv
      /* We can do a logical (unsigned) right shift with a bit-field
	 extract insn.  But first check if one of the above methods worked.  */
      if (temp != 0)
	return temp;

      if (unsignedp && code == RSHIFT_EXPR && ! BITS_BIG_ENDIAN && HAVE_extzv)
	{
	  enum machine_mode output_mode
	    = insn_operand_mode[(int) CODE_FOR_extzv][0];

	  if ((methods == OPTAB_DIRECT && mode == output_mode)
	      || (methods == OPTAB_WIDEN
		  && GET_MODE_SIZE (mode) < GET_MODE_SIZE (output_mode)))
	    {
	      rtx shifted1 = convert_to_mode (output_mode,
					      protect_from_queue (shifted, 0),
					      1);
	      enum machine_mode length_mode
		= insn_operand_mode[(int) CODE_FOR_extzv][2];
	      enum machine_mode pos_mode
		= insn_operand_mode[(int) CODE_FOR_extzv][3];
	      rtx target1 = 0;
	      rtx last = get_last_insn ();
	      rtx width;
	      rtx xop1 = op1;
	      rtx pat;

	      if (target != 0)
		target1 = protect_from_queue (target, 1);

	      /* We define extract insns as having OUTPUT_MODE in a register
		 and the mode of operand 1 in memory.  Since we want
		 OUTPUT_MODE, we will always force the operand into a
		 register.  At some point we might want to support MEM
		 directly. */
	      shifted1 = force_reg (output_mode, shifted1);

	      /* If we don't have or cannot use a suggested target,
		 make a place for the result, in the proper mode.  */
	      if (methods == OPTAB_WIDEN || target1 == 0
		  || ! ((*insn_operand_predicate[(int) CODE_FOR_extzv][0])
			(target1, output_mode)))
		target1 = gen_reg_rtx (output_mode);

	      xop1 = protect_from_queue (xop1, 0);
	      xop1 = convert_to_mode (pos_mode, xop1,
				      TREE_UNSIGNED (TREE_TYPE (amount)));

	      /* If this machine's extzv insists on a register for
		 operand 3 (position), arrange for that.  */
	      if (! ((*insn_operand_predicate[(int) CODE_FOR_extzv][3])
		     (xop1, pos_mode)))
		xop1 = force_reg (pos_mode, xop1);

	      /* WIDTH gets the width of the bit field to extract:
		 wordsize minus # bits to shift by.  */
	      if (GET_CODE (xop1) == CONST_INT)
		width = GEN_INT (GET_MODE_BITSIZE (mode) - INTVAL (op1));
	      else
		{
		  /* Now get the width in the proper mode.  */
		  op1 = protect_from_queue (op1, 0);
		  width = convert_to_mode (length_mode, op1,
					   TREE_UNSIGNED (TREE_TYPE (amount)));

		  width = expand_binop (length_mode, sub_optab,
					GEN_INT (GET_MODE_BITSIZE (mode)),
					width, NULL_RTX, 0, OPTAB_LIB_WIDEN);
		}

	      /* If this machine's extzv insists on a register for
		 operand 2 (length), arrange for that.  */
	      if (! ((*insn_operand_predicate[(int) CODE_FOR_extzv][2])
		     (width, length_mode)))
		width = force_reg (length_mode, width);

	      /* Now extract with WIDTH, omitting OP1 least sig bits.  */
	      pat = gen_extzv (target1, shifted1, width, xop1);
	      if (pat)
		{
		  emit_insn (pat);
		  temp = convert_to_mode (mode, target1, 1);
		}
	      else
		delete_insns_since (last);
	    }

	  /* Can also do logical shift with signed bit-field extract
	     followed by inserting the bit-field at a different position.
	     That strategy is not yet implemented.  */
	}
#endif /* HAVE_extzv */
    }

  if (temp == 0)
    abort ();
  return temp;
}

enum alg_code { alg_zero, alg_m, alg_shift,
		  alg_add_t_m2, alg_sub_t_m2,
		  alg_add_factor, alg_sub_factor,
		  alg_add_t2_m, alg_sub_t2_m,
		  alg_add, alg_subtract, alg_factor, alg_shiftop };

/* This structure records a sequence of operations.
   `ops' is the number of operations recorded.
   `cost' is their total cost.
   The operations are stored in `op' and the corresponding
   logarithms of the integer coefficients in `log'.

   These are the operations:
   alg_zero		total := 0;
   alg_m		total := multiplicand;
   alg_shift		total := total * coeff
   alg_add_t_m2		total := total + multiplicand * coeff;
   alg_sub_t_m2		total := total - multiplicand * coeff;
   alg_add_factor	total := total * coeff + total;
   alg_sub_factor	total := total * coeff - total;
   alg_add_t2_m		total := total * coeff + multiplicand;
   alg_sub_t2_m		total := total * coeff - multiplicand;

   The first operand must be either alg_zero or alg_m.  */

struct algorithm
{
  short cost;
  short ops;
  /* The size of the OP and LOG fields are not directly related to the
     word size, but the worst-case algorithms will be if we have few
     consecutive ones or zeros, i.e., a multiplicand like 10101010101...
     In that case we will generate shift-by-2, add, shift-by-2, add,...,
     in total wordsize operations.  */
  enum alg_code op[MAX_BITS_PER_WORD];
  char log[MAX_BITS_PER_WORD];
};

/* Compute and return the best algorithm for multiplying by T.
   The algorithm must cost less than cost_limit
   If retval.cost >= COST_LIMIT, no algorithm was found and all
   other field of the returned struct are undefined.  */

static struct algorithm
synth_mult (t, cost_limit)
     unsigned HOST_WIDE_INT t;
     int cost_limit;
{
  int m;
  struct algorithm *best_alg
    = (struct algorithm *)alloca (sizeof (struct algorithm));
  struct algorithm *alg_in
    = (struct algorithm *)alloca (sizeof (struct algorithm));
  unsigned int cost;
  unsigned HOST_WIDE_INT q;

  /* Indicate that no algorithm is yet found.  If no algorithm
     is found, this value will be returned and indicate failure.  */
  best_alg->cost = cost_limit;

  if (cost_limit <= 0)
    return *best_alg;

  /* t == 1 can be done in zero cost.  */
  if (t == 1)
    {
      best_alg->ops = 1;
      best_alg->cost = 0;
      best_alg->op[0] = alg_m;
      return *best_alg;
    }

  /* t == 0 sometimes has a cost.  If it does and it exceeds our limit,
     fail now.  */

  else if (t == 0)
    {
      if (zero_cost >= cost_limit)
	return *best_alg;
      else
	{
	  best_alg->ops = 1;
	  best_alg->cost = zero_cost;
	  best_alg->op[0] = alg_zero;
	  return *best_alg;
	}
    }

  /* If we have a group of zero bits at the low-order part of T, try
     multiplying by the remaining bits and then doing a shift.  */

  if ((t & 1) == 0)
    {
      m = floor_log2 (t & -t);	/* m = number of low zero bits */
      q = t >> m;
      cost = shift_cost[m];
      if (cost < cost_limit)
	{
	  *alg_in = synth_mult (q, cost_limit - cost);

	  cost += alg_in->cost;
	  if (cost < best_alg->cost)
	    {
	      struct algorithm *x;
	      x = alg_in, alg_in = best_alg, best_alg = x;
	      best_alg->log[best_alg->ops] = m;
	      best_alg->op[best_alg->ops++] = alg_shift;
	      best_alg->cost = cost_limit = cost;
	    }
	}
    }

  /* If we have an odd number, add or subtract one.  */
  if ((t & 1) != 0)
  {
    unsigned HOST_WIDE_INT w;

    for (w = 1; (w & t) != 0; w <<= 1)
      ;
    if (w > 2
	/* Reject the case where t is 3.
	   Thus we prefer addition in that case.  */
	&& t != 3)
      {
	/* T ends with ...111.  Multiply by (T + 1) and subtract 1.  */

	cost = add_cost;
	*alg_in = synth_mult (t + 1, cost_limit - cost);

	cost += alg_in->cost;
	if (cost < best_alg->cost)
	  {
	    struct algorithm *x;
	    x = alg_in, alg_in = best_alg, best_alg = x;
	    best_alg->log[best_alg->ops] = 0;
	    best_alg->op[best_alg->ops++] = alg_sub_t_m2;
	    best_alg->cost = cost_limit = cost;
	  }
      }
    else
      {
	/* T ends with ...01 or ...011.  Multiply by (T - 1) and add 1.  */

	cost = add_cost;
	*alg_in = synth_mult (t - 1, cost_limit - cost);

	cost += alg_in->cost;
	if (cost < best_alg->cost)
	  {
	    struct algorithm *x;
	    x = alg_in, alg_in = best_alg, best_alg = x;
	    best_alg->log[best_alg->ops] = 0;
	    best_alg->op[best_alg->ops++] = alg_add_t_m2;
	    best_alg->cost = cost_limit = cost;
	  }
      }
  }

  /* Look for factors of t of the form
     t = q(2**m +- 1), 2 <= m <= floor(log2(t - 1)).
     If we find such a factor, we can multiply by t using an algorithm that
     multiplies by q, shift the result by m and add/subtract it to itself.

     We search for large factors first and loop down, even if large factors
     are less probable than small; if we find a large factor we will find a
     good sequence quickly, and therefore be able to prune (by decreasing
     COST_LIMIT) the search.  */

  for (m = floor_log2 (t - 1); m >= 2; m--)
    {
      unsigned HOST_WIDE_INT d;

      d = ((unsigned HOST_WIDE_INT) 1 << m) + 1;
      if (t % d == 0 && t > d)
	{
	  cost = MIN (shiftadd_cost[m], add_cost + shift_cost[m]);
	  *alg_in = synth_mult (t / d, cost_limit - cost);

	  cost += alg_in->cost;
	  if (cost < best_alg->cost)
	    {
	      struct algorithm *x;
	      x = alg_in, alg_in = best_alg, best_alg = x;
	      best_alg->log[best_alg->ops] = m;
	      best_alg->op[best_alg->ops++] = alg_add_factor;
	      best_alg->cost = cost_limit = cost;
	    }
	}

      d = ((unsigned HOST_WIDE_INT) 1 << m) - 1;
      if (t % d == 0 && t > d)
	{
	  cost = MIN (shiftsub_cost[m], add_cost + shift_cost[m]);
	  *alg_in = synth_mult (t / d, cost_limit - cost);

	  cost += alg_in->cost;
	  if (cost < best_alg->cost)
	    {
	      struct algorithm *x;
	      x = alg_in, alg_in = best_alg, best_alg = x;
	      best_alg->log[best_alg->ops] = m;
	      best_alg->op[best_alg->ops++] = alg_sub_factor;
	      best_alg->cost = cost_limit = cost;
	    }
	}
    }

  /* Try shift-and-add (load effective address) instructions,
     i.e. do a*3, a*5, a*9.  */
  if ((t & 1) != 0)
    {
      q = t - 1;
      q = q & -q;
      m = exact_log2 (q);
      if (m >= 0)
	{
	  cost = shiftadd_cost[m];
	  *alg_in = synth_mult ((t - 1) >> m, cost_limit - cost);

	  cost += alg_in->cost;
	  if (cost < best_alg->cost)
	    {
	      struct algorithm *x;
	      x = alg_in, alg_in = best_alg, best_alg = x;
	      best_alg->log[best_alg->ops] = m;
	      best_alg->op[best_alg->ops++] = alg_add_t2_m;
	      best_alg->cost = cost_limit = cost;
	    }
	}

      q = t + 1;
      q = q & -q;
      m = exact_log2 (q);
      if (m >= 0)
	{
	  cost = shiftsub_cost[m];
	  *alg_in = synth_mult ((t + 1) >> m, cost_limit - cost);

	  cost += alg_in->cost;
	  if (cost < best_alg->cost)
	    {
	      struct algorithm *x;
	      x = alg_in, alg_in = best_alg, best_alg = x;
	      best_alg->log[best_alg->ops] = m;
	      best_alg->op[best_alg->ops++] = alg_sub_t2_m;
	      best_alg->cost = cost_limit = cost;
	    }
	}
    }

  /* If we are getting a too long sequence for `struct algorithm'
     to record, store a fake cost to make this search fail.  */
  if (best_alg->ops == MAX_BITS_PER_WORD)
    best_alg->cost = cost_limit;

  return *best_alg;
}

/* Perform a multiplication and return an rtx for the result.
   MODE is mode of value; OP0 and OP1 are what to multiply (rtx's);
   TARGET is a suggestion for where to store the result (an rtx).

   We check specially for a constant integer as OP1.
   If you want this check for OP0 as well, then before calling
   you should swap the two operands if OP0 would be constant.  */

rtx
expand_mult (mode, op0, op1, target, unsignedp)
     enum machine_mode mode;
     register rtx op0, op1, target;
     int unsignedp;
{
  rtx const_op1 = op1;

  /* If we are multiplying in DImode, it may still be a win
     to try to work with shifts and adds.  */
  if (GET_CODE (op1) == CONST_DOUBLE
      && GET_MODE_CLASS (GET_MODE (op1)) == MODE_INT
      && HOST_BITS_PER_INT <= BITS_PER_WORD)
    {
      if ((CONST_DOUBLE_HIGH (op1) == 0 && CONST_DOUBLE_LOW (op1) >= 0)
	  || (CONST_DOUBLE_HIGH (op1) == -1 && CONST_DOUBLE_LOW (op1) < 0))
	const_op1 = GEN_INT (CONST_DOUBLE_LOW (op1));
    }

  /* We used to test optimize here, on the grounds that it's better to
     produce a smaller program when -O is not used.
     But this causes such a terrible slowdown sometimes
     that it seems better to use synth_mult always.  */

  if (GET_CODE (const_op1) == CONST_INT && ! mult_is_very_cheap)
    {
      struct algorithm alg;
      struct algorithm neg_alg;
      int negate = 0;
      HOST_WIDE_INT val = INTVAL (op1);
      HOST_WIDE_INT val_so_far;
      rtx insn;

      /* Try to do the computation two ways: multiply by the negative of OP1
	 and then negate, or do the multiplication directly.  The latter is
	 usually faster for positive numbers and the former for negative
	 numbers, but the opposite can be faster if the original value
	 has a factor of 2**m +/- 1, while the negated value does not or
	 vice versa.  */

      alg = synth_mult (val, mult_cost);
      neg_alg = synth_mult (- val,
			    (alg.cost < mult_cost ? alg.cost : mult_cost)
			    - negate_cost);

      if (neg_alg.cost + negate_cost < alg.cost)
	alg = neg_alg, negate = 1;

      if (alg.cost < mult_cost)
	{
	  /* We found something cheaper than a multiply insn.  */
	  int opno;
	  rtx accum, tem;

	  op0 = protect_from_queue (op0, 0);

	  /* Avoid referencing memory over and over.
	     For speed, but also for correctness when mem is volatile.  */
	  if (GET_CODE (op0) == MEM)
	    op0 = force_reg (mode, op0);

	  /* ACCUM starts out either as OP0 or as a zero, depending on
	     the first operation.  */

	  if (alg.op[0] == alg_zero)
	    {
	      accum = copy_to_mode_reg (mode, const0_rtx);
	      val_so_far = 0;
	    }
	  else if (alg.op[0] == alg_m)
	    {
	      accum  = copy_to_mode_reg (mode, op0);
	      val_so_far = 1;
	    }
	  else
	    abort ();

	  for (opno = 1; opno < alg.ops; opno++)
	    {
	      int log = alg.log[opno];
	      rtx shift_subtarget = preserve_subexpressions_p () ? 0 : accum;
	      rtx add_target = opno == alg.ops - 1 && target != 0 ? target : 0;

	      switch (alg.op[opno])
		{
		case alg_shift:
		  accum = expand_shift (LSHIFT_EXPR, mode, accum,
					build_int_2 (log, 0), NULL_RTX, 0);
		  val_so_far <<= log;
		  break;

		case alg_add_t_m2:
		  tem = expand_shift (LSHIFT_EXPR, mode, op0,
				      build_int_2 (log, 0), NULL_RTX, 0);
		  accum = force_operand (gen_rtx (PLUS, mode, accum, tem),
					 add_target ? add_target : accum);
		  val_so_far += (HOST_WIDE_INT) 1 << log;
		  break;

		case alg_sub_t_m2:
		  tem = expand_shift (LSHIFT_EXPR, mode, op0,
				      build_int_2 (log, 0), NULL_RTX, 0);
		  accum = force_operand (gen_rtx (MINUS, mode, accum, tem),
					 add_target ? add_target : accum);
		  val_so_far -= (HOST_WIDE_INT) 1 << log;
		  break;

		case alg_add_t2_m:
		  accum = expand_shift (LSHIFT_EXPR, mode, accum,
					build_int_2 (log, 0), accum, 0);
		  accum = force_operand (gen_rtx (PLUS, mode, accum, op0),
					 add_target ? add_target : accum);
		  val_so_far = (val_so_far << log) + 1;
		  break;

		case alg_sub_t2_m:
		  accum = expand_shift (LSHIFT_EXPR, mode, accum,
					build_int_2 (log, 0), accum, 0);
		  accum = force_operand (gen_rtx (MINUS, mode, accum, op0),
					 add_target ? add_target : accum);
		  val_so_far = (val_so_far << log) - 1;
		  break;

		case alg_add_factor:
		  tem = expand_shift (LSHIFT_EXPR, mode, accum,
				      build_int_2 (log, 0), NULL_RTX, 0);
		  accum = force_operand (gen_rtx (PLUS, mode, accum, tem),
					 add_target ? add_target : accum);
		  val_so_far += val_so_far << log;
		  break;

		case alg_sub_factor:
		  tem = expand_shift (LSHIFT_EXPR, mode, accum,
				      build_int_2 (log, 0), NULL_RTX, 0);
		  accum = force_operand (gen_rtx (MINUS, mode, tem, accum),
					 add_target ? add_target : tem);
		  val_so_far = (val_so_far << log) - val_so_far;
		  break;

		default:
		  abort ();;
		}

	      /* Write a REG_EQUAL note on the last insn so that we can cse
		 multiplication sequences.  */

	      insn = get_last_insn ();
	      REG_NOTES (insn)
		= gen_rtx (EXPR_LIST, REG_EQUAL,
			   gen_rtx (MULT, mode, op0, GEN_INT (val_so_far)),
			   REG_NOTES (insn));
	    }

	  if (negate)
	    {
	      val_so_far = - val_so_far;
	      accum = expand_unop (mode, neg_optab, accum, target, 0);
	    }

	  if (val != val_so_far)
	    abort ();

	  return accum;
	}
    }

  /* This used to use umul_optab if unsigned,
     but for non-widening multiply there is no difference
     between signed and unsigned.  */
  op0 = expand_binop (mode, smul_optab,
		      op0, op1, target, unsignedp, OPTAB_LIB_WIDEN);
  if (op0 == 0)
    abort ();
  return op0;
}

/* Emit the code to divide OP0 by OP1, putting the result in TARGET
   if that is convenient, and returning where the result is.
   You may request either the quotient or the remainder as the result;
   specify REM_FLAG nonzero to get the remainder.

   CODE is the expression code for which kind of division this is;
   it controls how rounding is done.  MODE is the machine mode to use.
   UNSIGNEDP nonzero means do unsigned division.  */

/* ??? For CEIL_MOD_EXPR, can compute incorrect remainder with ANDI
   and then correct it by or'ing in missing high bits
   if result of ANDI is nonzero.
   For ROUND_MOD_EXPR, can use ANDI and then sign-extend the result.
   This could optimize to a bfexts instruction.
   But C doesn't use these operations, so their optimizations are
   left for later.  */

rtx
expand_divmod (rem_flag, code, mode, op0, op1, target, unsignedp)
     int rem_flag;
     enum tree_code code;
     enum machine_mode mode;
     register rtx op0, op1, target;
     int unsignedp;
{
  register rtx result = 0;
  enum machine_mode compute_mode;
  int log = -1;
  int size;
  int can_clobber_op0;
  int mod_insn_no_good = 0;
  rtx adjusted_op0 = op0;
  optab optab1, optab2;

  /* We shouldn't be called with op1 == const1_rtx, but some of the
     code below will malfunction if we are, so check here and handle
     the special case if so.  */
  if (op1 == const1_rtx)
    return rem_flag ? const0_rtx : op0;

  /* Don't use the function value register as a target
     since we have to read it as well as write it,
     and function-inlining gets confused by this.  */
  if (target && REG_P (target) && REG_FUNCTION_VALUE_P (target))
    target = 0;

  /* Don't clobber an operand while doing a multi-step calculation.  */
  if (target)
    if ((rem_flag && (reg_mentioned_p (target, op0)
		      || (GET_CODE (op0) == MEM && GET_CODE (target) == MEM)))
	|| reg_mentioned_p (target, op1)
	|| (GET_CODE (op1) == MEM && GET_CODE (target) == MEM))
      target = 0;

  can_clobber_op0 = (GET_CODE (op0) == REG && op0 == target);

  if (GET_CODE (op1) == CONST_INT)
    log = exact_log2 (INTVAL (op1));

  /* If log is >= 0, we are dividing by 2**log, and will do it by shifting,
     which is really floor-division.  Otherwise we will really do a divide,
     and we assume that is trunc-division.

     We must correct the dividend by adding or subtracting something
     based on the divisor, in order to do the kind of rounding specified
     by CODE.  The correction depends on what kind of rounding is actually
     available, and that depends on whether we will shift or divide.

     In many of these cases it is possible to perform the operation by a
     clever series of logical operations (shifts and/or exclusive-ors).
     Although avoiding the jump has the advantage that it extends the basic
     block and allows further optimization, the branch-free code is normally
     at least one instruction longer in the (most common) case where the
     dividend is non-negative.  Performance measurements of the two
     alternatives show that the branch-free code is slightly faster on the
     IBM ROMP but slower on CISC processors (significantly slower on the
     VAX).  Accordingly, the jump code has been retained.

     On machines where the jump code is slower, the cost of a DIV or MOD
     operation can be set small (less than twice that of an addition); in 
     that case, we pretend that we don't have a power of two and perform
     a normal division or modulus operation.  */

  if ((code == TRUNC_MOD_EXPR || code == TRUNC_DIV_EXPR)
      && ! unsignedp
      && (rem_flag ? smod_pow2_cheap : sdiv_pow2_cheap))
    log = -1;

  /* Get the mode in which to perform this computation.  Normally it will
     be MODE, but sometimes we can't do the desired operation in MODE.
     If so, pick a wider mode in which we can do the operation.  Convert
     to that mode at the start to avoid repeated conversions.

     First see what operations we need.  These depend on the expression
     we are evaluating.  (We assume that divxx3 insns exist under the
     same conditions that modxx3 insns and that these insns don't normally
     fail.  If these assumptions are not correct, we may generate less
     efficient code in some cases.)

     Then see if we find a mode in which we can open-code that operation
     (either a division, modulus, or shift).  Finally, check for the smallest
     mode for which we can do the operation with a library call.  */

  optab1 = (log >= 0 ? (unsignedp ? lshr_optab : ashr_optab)
	    : (unsignedp ? udiv_optab : sdiv_optab));
  optab2 = (log >= 0 ? optab1 : (unsignedp ? udivmod_optab : sdivmod_optab));

  for (compute_mode = mode; compute_mode != VOIDmode;
       compute_mode = GET_MODE_WIDER_MODE (compute_mode))
    if (optab1->handlers[(int) compute_mode].insn_code != CODE_FOR_nothing
	|| optab2->handlers[(int) compute_mode].insn_code != CODE_FOR_nothing)
      break;

  if (compute_mode == VOIDmode)
    for (compute_mode = mode; compute_mode != VOIDmode;
	 compute_mode = GET_MODE_WIDER_MODE (compute_mode))
      if (optab1->handlers[(int) compute_mode].libfunc
	  || optab2->handlers[(int) compute_mode].libfunc)
	break;

  /* If we still couldn't find a mode, use MODE; we'll probably abort in
     expand_binop.  */
  if (compute_mode == VOIDmode)
    compute_mode = mode;

  size = GET_MODE_BITSIZE (compute_mode);

  /* Now convert to the best mode to use.  Show we made a copy of OP0
     and hence we can clobber it (we cannot use a SUBREG to widen
     something.  */
  if (compute_mode != mode)
    {
      adjusted_op0 = op0 = convert_to_mode (compute_mode, op0, unsignedp);
      can_clobber_op0 = 1;
      op1 = convert_to_mode (compute_mode, op1, unsignedp);
    }

  /* If we are computing the remainder and one of the operands is a volatile
     MEM, copy it into a register.  */

  if (rem_flag && GET_CODE (op0) == MEM && MEM_VOLATILE_P (op0))
    adjusted_op0 = op0 = force_reg (compute_mode, op0), can_clobber_op0 = 1;
  if (rem_flag && GET_CODE (op1) == MEM && MEM_VOLATILE_P (op1))
    op1 = force_reg (compute_mode, op1);

  /* If we are computing the remainder, op0 will be needed later to calculate
     X - Y * (X / Y), therefore cannot be clobbered. */
  if (rem_flag)
    can_clobber_op0 = 0;

  if (target == 0 || GET_MODE (target) != compute_mode)
    target = gen_reg_rtx (compute_mode);

  switch (code)
    {
    case TRUNC_MOD_EXPR:
    case TRUNC_DIV_EXPR:
      if (log >= 0 && ! unsignedp)
	{
	  /* Here we need to add OP1-1 if OP0 is negative, 0 otherwise.
	     This can be computed without jumps by arithmetically shifting
	     OP0 right LOG-1 places and then shifting right logically
	     SIZE-LOG bits.  The resulting value is unconditionally added
	     to OP0.  */
	  if (log == 1 || BRANCH_COST >= 3)
	    {
	      rtx temp = gen_reg_rtx (compute_mode);
	      if (! can_clobber_op0)
		/* Copy op0 to a reg, to play safe,
		   since this is done in the other path.  */
		op0 = force_reg (compute_mode, op0);
	      temp = copy_to_suggested_reg (adjusted_op0, temp, compute_mode);
	      temp = expand_shift (RSHIFT_EXPR, compute_mode, temp,
				   build_int_2 (log - 1, 0), NULL_RTX, 0);
	      temp = expand_shift (RSHIFT_EXPR, compute_mode, temp,
				   build_int_2 (size - log, 0),
				   temp, 1);
	      /* We supply 0 as the target to make a new pseudo
		 for the value; that helps loop.c optimize the result.  */
	      adjusted_op0 = expand_binop (compute_mode, add_optab,
					   adjusted_op0, temp,
					   0, 0, OPTAB_LIB_WIDEN);
	    }
	  else
	    {
	      rtx label = gen_label_rtx ();
	      if (! can_clobber_op0)
		{
		  adjusted_op0 = copy_to_suggested_reg (adjusted_op0, target,
							compute_mode);
		  /* Copy op0 to a reg, since emit_cmp_insn will call emit_queue
		     which will screw up mem refs for autoincrements.  */
		  op0 = force_reg (compute_mode, op0);
		}
	      emit_cmp_insn (adjusted_op0, const0_rtx, GE, 
			     NULL_RTX, compute_mode, 0, 0);
	      emit_jump_insn (gen_bge (label));
	      expand_inc (adjusted_op0, plus_constant (op1, -1));
	      emit_label (label);
	    }
	  mod_insn_no_good = 1;
	}
      break;

    case FLOOR_DIV_EXPR:
    case FLOOR_MOD_EXPR:
      if (log < 0 && ! unsignedp)
	{
	  rtx label = gen_label_rtx ();
	  if (! can_clobber_op0)
	    {
	      adjusted_op0 = copy_to_suggested_reg (adjusted_op0, target,
						    compute_mode);
	      /* Copy op0 to a reg, since emit_cmp_insn will call emit_queue
		 which will screw up mem refs for autoincrements.  */
	      op0 = force_reg (compute_mode, op0);
	    }
	  emit_cmp_insn (adjusted_op0, const0_rtx, GE, 
			 NULL_RTX, compute_mode, 0, 0);
	  emit_jump_insn (gen_bge (label));
	  expand_dec (adjusted_op0, op1);
	  expand_inc (adjusted_op0, const1_rtx);
	  emit_label (label);
	  mod_insn_no_good = 1;
	}
      break;

    case CEIL_DIV_EXPR:
    case CEIL_MOD_EXPR:
      if (! can_clobber_op0)
	{
	  adjusted_op0 = copy_to_suggested_reg (adjusted_op0, target,
						compute_mode);
	  /* Copy op0 to a reg, since emit_cmp_insn will call emit_queue
	     which will screw up mem refs for autoincrements.  */
	  op0 = force_reg (compute_mode, op0);
	}
      if (log < 0)
	{
	  rtx label = 0;
	  if (! unsignedp)
	    {
	      label = gen_label_rtx ();
	      emit_cmp_insn (adjusted_op0, const0_rtx, LE, 
			     NULL_RTX, compute_mode, 0, 0);
	      emit_jump_insn (gen_ble (label));
	    }
	  expand_inc (adjusted_op0, op1);
	  expand_dec (adjusted_op0, const1_rtx);
	  if (! unsignedp)
	    emit_label (label);
	}
      else
	{
	  adjusted_op0 = expand_binop (compute_mode, add_optab,
				       adjusted_op0, plus_constant (op1, -1),
				       NULL_RTX, 0, OPTAB_LIB_WIDEN);
	}
      mod_insn_no_good = 1;
      break;

    case ROUND_DIV_EXPR:
    case ROUND_MOD_EXPR:
      if (! can_clobber_op0)
	{
	  adjusted_op0 = copy_to_suggested_reg (adjusted_op0, target,
						compute_mode);
	  /* Copy op0 to a reg, since emit_cmp_insn will call emit_queue
	     which will screw up mem refs for autoincrements.  */
	  op0 = force_reg (compute_mode, op0);
	}
      if (log < 0)
	{
	  op1 = expand_shift (RSHIFT_EXPR, compute_mode, op1,
			      integer_one_node, NULL_RTX, 0);
	  if (! unsignedp)
	    {
	      if (BRANCH_COST >= 2)
		{
		  /* Negate OP1 if OP0 < 0.  Do this by computing a temporary
		     that has all bits equal to the sign bit and exclusive
		     or-ing it with OP1.  */
		  rtx temp = gen_reg_rtx (compute_mode);
		  temp = copy_to_suggested_reg (adjusted_op0, temp, compute_mode);
		  temp = expand_shift (RSHIFT_EXPR, compute_mode, temp,
				       build_int_2 (size - 1, 0),
				       NULL_RTX, 0);
		  op1 = expand_binop (compute_mode, xor_optab, op1, temp, op1,
				      unsignedp, OPTAB_LIB_WIDEN);
		}
	      else
		{
		  rtx label = gen_label_rtx ();
		  emit_cmp_insn (adjusted_op0, const0_rtx, GE, NULL_RTX,
				 compute_mode, 0, 0);
		  emit_jump_insn (gen_bge (label));
		  expand_unop (compute_mode, neg_optab, op1, op1, 0);
		  emit_label (label);
		}
	    }
	  expand_inc (adjusted_op0, op1);
	}
      else
	{
	  op1 = GEN_INT (((HOST_WIDE_INT) 1 << log) / 2);
	  expand_inc (adjusted_op0, op1);
	}
      mod_insn_no_good = 1;
      break;
    }

  if (rem_flag && !mod_insn_no_good)
    {
      /* Try to produce the remainder directly */
      if (log >= 0)
	result = expand_binop (compute_mode, and_optab, adjusted_op0,
			       GEN_INT (((HOST_WIDE_INT) 1 << log) - 1),
			       target, 1, OPTAB_LIB_WIDEN);
      else
	{
	  /* See if we can do remainder without a library call.  */
	  result = sign_expand_binop (mode, umod_optab, smod_optab,
				      adjusted_op0, op1, target,
				      unsignedp, OPTAB_WIDEN);
	  if (result == 0)
	    {
	      /* No luck there.  Can we do remainder and divide at once
		 without a library call?  */
	      result = gen_reg_rtx (compute_mode);
	      if (! expand_twoval_binop (unsignedp
					 ? udivmod_optab : sdivmod_optab,
					 adjusted_op0, op1,
					 NULL_RTX, result, unsignedp))
		result = 0;
	    }
	}
    }

  if (result)
    return gen_lowpart (mode, result);

  /* Produce the quotient.  */
  if (log >= 0)
    result = expand_shift (RSHIFT_EXPR, compute_mode, adjusted_op0,
			   build_int_2 (log, 0), target, unsignedp);
  else if (rem_flag && !mod_insn_no_good)
    /* If producing quotient in order to subtract for remainder,
       and a remainder subroutine would be ok,
       don't use a divide subroutine.  */
    result = sign_expand_binop (compute_mode, udiv_optab, sdiv_optab,
				adjusted_op0, op1, NULL_RTX, unsignedp,
				OPTAB_WIDEN);
  else
    {
      /* Try a quotient insn, but not a library call.  */
      result = sign_expand_binop (compute_mode, udiv_optab, sdiv_optab,
				  adjusted_op0, op1,
				  rem_flag ? NULL_RTX : target,
				  unsignedp, OPTAB_WIDEN);
      if (result == 0)
	{
	  /* No luck there.  Try a quotient-and-remainder insn,
	     keeping the quotient alone.  */
	  result = gen_reg_rtx (mode);
	  if (! expand_twoval_binop (unsignedp ? udivmod_optab : sdivmod_optab,
				     adjusted_op0, op1,
				     result, NULL_RTX, unsignedp))
	    result = 0;
	}

      /* If still no luck, use a library call.  */
      if (result == 0)
	result = sign_expand_binop (compute_mode, udiv_optab, sdiv_optab,
				    adjusted_op0, op1,
				    rem_flag ? NULL_RTX : target,
				    unsignedp, OPTAB_LIB_WIDEN);
    }

  /* If we really want the remainder, get it by subtraction.  */
  if (rem_flag)
    {
      if (result == 0)
	/* No divide instruction either.  Use library for remainder.  */
	result = sign_expand_binop (compute_mode, umod_optab, smod_optab,
				    op0, op1, target,
				    unsignedp, OPTAB_LIB_WIDEN);
      else
	{
	  /* We divided.  Now finish doing X - Y * (X / Y).  */
	  result = expand_mult (compute_mode, result, op1, target, unsignedp);
	  if (! result) abort ();
	  result = expand_binop (compute_mode, sub_optab, op0,
				 result, target, unsignedp, OPTAB_LIB_WIDEN);
	}
    }

  if (result == 0)
    abort ();

  return gen_lowpart (mode, result);
}

/* Return a tree node with data type TYPE, describing the value of X.
   Usually this is an RTL_EXPR, if there is no obvious better choice.
   X may be an expression, however we only support those expressions
   generated by loop.c.   */

tree
make_tree (type, x)
     tree type;
     rtx x;
{
  tree t;

  switch (GET_CODE (x))
    {
    case CONST_INT:
      t = build_int_2 (INTVAL (x),
		       ! TREE_UNSIGNED (type) && INTVAL (x) >= 0 ? 0 : -1);
      TREE_TYPE (t) = type;
      return t;

    case CONST_DOUBLE:
      if (GET_MODE (x) == VOIDmode)
	{
	  t = build_int_2 (CONST_DOUBLE_LOW (x), CONST_DOUBLE_HIGH (x));
	  TREE_TYPE (t) = type;
	}
      else
	{
	  REAL_VALUE_TYPE d;

	  REAL_VALUE_FROM_CONST_DOUBLE (d, x);
	  t = build_real (type, d);
	}

      return t;
	  
    case PLUS:
      return fold (build (PLUS_EXPR, type, make_tree (type, XEXP (x, 0)),
			  make_tree (type, XEXP (x, 1))));
						       
    case MINUS:
      return fold (build (MINUS_EXPR, type, make_tree (type, XEXP (x, 0)),
			  make_tree (type, XEXP (x, 1))));
						       
    case NEG:
      return fold (build1 (NEGATE_EXPR, type, make_tree (type, XEXP (x, 0))));

    case MULT:
      return fold (build (MULT_EXPR, type, make_tree (type, XEXP (x, 0)),
			  make_tree (type, XEXP (x, 1))));
						      
    case ASHIFT:
      return fold (build (LSHIFT_EXPR, type, make_tree (type, XEXP (x, 0)),
			  make_tree (type, XEXP (x, 1))));
						      
    case LSHIFTRT:
      return fold (convert (type,
			    build (RSHIFT_EXPR, unsigned_type (type),
				   make_tree (unsigned_type (type),
					      XEXP (x, 0)),
				   make_tree (type, XEXP (x, 1)))));
						      
    case ASHIFTRT:
      return fold (convert (type,
			    build (RSHIFT_EXPR, signed_type (type),
				   make_tree (signed_type (type), XEXP (x, 0)),
				   make_tree (type, XEXP (x, 1)))));
						      
    case DIV:
      if (TREE_CODE (type) != REAL_TYPE)
	t = signed_type (type);
      else
	t = type;

      return fold (convert (type,
			    build (TRUNC_DIV_EXPR, t,
				   make_tree (t, XEXP (x, 0)),
				   make_tree (t, XEXP (x, 1)))));
    case UDIV:
      t = unsigned_type (type);
      return fold (convert (type,
			    build (TRUNC_DIV_EXPR, t,
				   make_tree (t, XEXP (x, 0)),
				   make_tree (t, XEXP (x, 1)))));
   default:
      t = make_node (RTL_EXPR);
      TREE_TYPE (t) = type;
      RTL_EXPR_RTL (t) = x;
      /* There are no insns to be output
	 when this rtl_expr is used.  */
      RTL_EXPR_SEQUENCE (t) = 0;
      return t;
    }
}

/* Return an rtx representing the value of X * MULT + ADD.
   TARGET is a suggestion for where to store the result (an rtx).
   MODE is the machine mode for the computation.
   X and MULT must have mode MODE.  ADD may have a different mode.
   So can X (defaults to same as MODE).
   UNSIGNEDP is non-zero to do unsigned multiplication.
   This may emit insns.  */

rtx
expand_mult_add (x, target, mult, add, mode, unsignedp)
     rtx x, target, mult, add;
     enum machine_mode mode;
     int unsignedp;
{
  tree type = type_for_mode (mode, unsignedp);
  tree add_type = (GET_MODE (add) == VOIDmode
		   ? type : type_for_mode (GET_MODE (add), unsignedp));
  tree result =  fold (build (PLUS_EXPR, type,
			      fold (build (MULT_EXPR, type,
					   make_tree (type, x),
					   make_tree (type, mult))),
			      make_tree (add_type, add)));

  return expand_expr (result, target, VOIDmode, 0);
}

/* Compute the logical-and of OP0 and OP1, storing it in TARGET
   and returning TARGET.

   If TARGET is 0, a pseudo-register or constant is returned.  */

rtx
expand_and (op0, op1, target)
     rtx op0, op1, target;
{
  enum machine_mode mode = VOIDmode;
  rtx tem;

  if (GET_MODE (op0) != VOIDmode)
    mode = GET_MODE (op0);
  else if (GET_MODE (op1) != VOIDmode)
    mode = GET_MODE (op1);

  if (mode != VOIDmode)
    tem = expand_binop (mode, and_optab, op0, op1, target, 0, OPTAB_LIB_WIDEN);
  else if (GET_CODE (op0) == CONST_INT && GET_CODE (op1) == CONST_INT)
    tem = GEN_INT (INTVAL (op0) & INTVAL (op1));
  else
    abort ();

  if (target == 0)
    target = tem;
  else if (tem != target)
    emit_move_insn (target, tem);
  return target;
}

/* Emit a store-flags instruction for comparison CODE on OP0 and OP1
   and storing in TARGET.  Normally return TARGET.
   Return 0 if that cannot be done.

   MODE is the mode to use for OP0 and OP1 should they be CONST_INTs.  If
   it is VOIDmode, they cannot both be CONST_INT.  

   UNSIGNEDP is for the case where we have to widen the operands
   to perform the operation.  It says to use zero-extension.

   NORMALIZEP is 1 if we should convert the result to be either zero
   or one one.  Normalize is -1 if we should convert the result to be
   either zero or -1.  If NORMALIZEP is zero, the result will be left
   "raw" out of the scc insn.  */

rtx
emit_store_flag (target, code, op0, op1, mode, unsignedp, normalizep)
     rtx target;
     enum rtx_code code;
     rtx op0, op1;
     enum machine_mode mode;
     int unsignedp;
     int normalizep;
{
  rtx subtarget;
  enum insn_code icode;
  enum machine_mode compare_mode;
  enum machine_mode target_mode = GET_MODE (target);
  rtx tem;
  rtx last = 0;
  rtx pattern, comparison;

  if (mode == VOIDmode)
    mode = GET_MODE (op0);

  /* If one operand is constant, make it the second one.  Only do this
     if the other operand is not constant as well.  */

  if ((CONSTANT_P (op0) && ! CONSTANT_P (op1))
      || (GET_CODE (op0) == CONST_INT && GET_CODE (op1) != CONST_INT))
    {
      tem = op0;
      op0 = op1;
      op1 = tem;
      code = swap_condition (code);
    }

  /* For some comparisons with 1 and -1, we can convert this to 
     comparisons with zero.  This will often produce more opportunities for
     store-flag insns. */

  switch (code)
    {
    case LT:
      if (op1 == const1_rtx)
	op1 = const0_rtx, code = LE;
      break;
    case LE:
      if (op1 == constm1_rtx)
	op1 = const0_rtx, code = LT;
      break;
    case GE:
      if (op1 == const1_rtx)
	op1 = const0_rtx, code = GT;
      break;
    case GT:
      if (op1 == constm1_rtx)
	op1 = const0_rtx, code = GE;
      break;
    case GEU:
      if (op1 == const1_rtx)
	op1 = const0_rtx, code = NE;
      break;
    case LTU:
      if (op1 == const1_rtx)
	op1 = const0_rtx, code = EQ;
      break;
    }

  /* From now on, we won't change CODE, so set ICODE now.  */
  icode = setcc_gen_code[(int) code];

  /* If this is A < 0 or A >= 0, we can do this by taking the ones
     complement of A (for GE) and shifting the sign bit to the low bit.  */
  if (op1 == const0_rtx && (code == LT || code == GE)
      && GET_MODE_CLASS (mode) == MODE_INT
      && (normalizep || STORE_FLAG_VALUE == 1
	  || (GET_MODE_BITSIZE (mode) <= HOST_BITS_PER_WIDE_INT
	      && (STORE_FLAG_VALUE 
		  == (HOST_WIDE_INT) 1 << (GET_MODE_BITSIZE (mode) - 1)))))
    {
      subtarget = target;

      /* If the result is to be wider than OP0, it is best to convert it
	 first.  If it is to be narrower, it is *incorrect* to convert it
	 first.  */
      if (GET_MODE_SIZE (target_mode) > GET_MODE_SIZE (mode))
	{
	  op0 = protect_from_queue (op0, 0);
	  op0 = convert_to_mode (target_mode, op0, 0);
	  mode = target_mode;
	}

      if (target_mode != mode)
	subtarget = 0;

      if (code == GE)
	op0 = expand_unop (mode, one_cmpl_optab, op0, subtarget, 0);

      if (normalizep || STORE_FLAG_VALUE == 1)
	/* If we are supposed to produce a 0/1 value, we want to do
	   a logical shift from the sign bit to the low-order bit; for
	   a -1/0 value, we do an arithmetic shift.  */
	op0 = expand_shift (RSHIFT_EXPR, mode, op0,
			    size_int (GET_MODE_BITSIZE (mode) - 1),
			    subtarget, normalizep != -1);

      if (mode != target_mode)
	op0 = convert_to_mode (target_mode, op0, 0);

      return op0;
    }

  if (icode != CODE_FOR_nothing)
    {
      /* We think we may be able to do this with a scc insn.  Emit the
	 comparison and then the scc insn.

	 compare_from_rtx may call emit_queue, which would be deleted below
	 if the scc insn fails.  So call it ourselves before setting LAST.  */

      emit_queue ();
      last = get_last_insn ();

      comparison
	= compare_from_rtx (op0, op1, code, unsignedp, mode, NULL_RTX, 0);
      if (GET_CODE (comparison) == CONST_INT)
	return (comparison == const0_rtx ? const0_rtx
		: normalizep == 1 ? const1_rtx
		: normalizep == -1 ? constm1_rtx
		: const_true_rtx);

      /* If the code of COMPARISON doesn't match CODE, something is
	 wrong; we can no longer be sure that we have the operation.  
	 We could handle this case, but it should not happen.  */

      if (GET_CODE (comparison) != code)
	abort ();

      /* Get a reference to the target in the proper mode for this insn.  */
      compare_mode = insn_operand_mode[(int) icode][0];
      subtarget = target;
      if (preserve_subexpressions_p ()
	  || ! (*insn_operand_predicate[(int) icode][0]) (subtarget, compare_mode))
	subtarget = gen_reg_rtx (compare_mode);

      pattern = GEN_FCN (icode) (subtarget);
      if (pattern)
	{
	  emit_insn (pattern);

	  /* If we are converting to a wider mode, first convert to
	     TARGET_MODE, then normalize.  This produces better combining
	     opportunities on machines that have a SIGN_EXTRACT when we are
	     testing a single bit.  This mostly benefits the 68k.

	     If STORE_FLAG_VALUE does not have the sign bit set when
	     interpreted in COMPARE_MODE, we can do this conversion as
	     unsigned, which is usually more efficient.  */
	  if (GET_MODE_SIZE (target_mode) > GET_MODE_SIZE (compare_mode))
	    {
	      convert_move (target, subtarget,
			    (GET_MODE_BITSIZE (compare_mode)
			     <= HOST_BITS_PER_WIDE_INT)
			    && 0 == (STORE_FLAG_VALUE
				     & ((HOST_WIDE_INT) 1
					<< (GET_MODE_BITSIZE (compare_mode) -1))));
	      op0 = target;
	      compare_mode = target_mode;
	    }
	  else
	    op0 = subtarget;

	  /* If we want to keep subexpressions around, don't reuse our
	     last target.  */

	  if (preserve_subexpressions_p ())
	    subtarget = 0;

	  /* Now normalize to the proper value in COMPARE_MODE.  Sometimes
	     we don't have to do anything.  */
	  if (normalizep == 0 || normalizep == STORE_FLAG_VALUE)
	    ;
	  else if (normalizep == - STORE_FLAG_VALUE)
	    op0 = expand_unop (compare_mode, neg_optab, op0, subtarget, 0);

	  /* We don't want to use STORE_FLAG_VALUE < 0 below since this
	     makes it hard to use a value of just the sign bit due to
	     ANSI integer constant typing rules.  */
	  else if (GET_MODE_BITSIZE (compare_mode) <= HOST_BITS_PER_WIDE_INT
		   && (STORE_FLAG_VALUE
		       & ((HOST_WIDE_INT) 1
			  << (GET_MODE_BITSIZE (compare_mode) - 1))))
	    op0 = expand_shift (RSHIFT_EXPR, compare_mode, op0,
				size_int (GET_MODE_BITSIZE (compare_mode) - 1),
				subtarget, normalizep == 1);
	  else if (STORE_FLAG_VALUE & 1)
	    {
	      op0 = expand_and (op0, const1_rtx, subtarget);
	      if (normalizep == -1)
		op0 = expand_unop (compare_mode, neg_optab, op0, op0, 0);
	    }
	  else
	    abort ();

	  /* If we were converting to a smaller mode, do the 
	     conversion now.  */
	  if (target_mode != compare_mode)
	    {
	      convert_move (target, op0, 0);
	      return target;
	    }
	  else
	    return op0;
	}
    }

  if (last)
    delete_insns_since (last);

  subtarget = target_mode == mode ? target : 0;

  /* If we reached here, we can't do this with a scc insn.  However, there
     are some comparisons that can be done directly.  For example, if
     this is an equality comparison of integers, we can try to exclusive-or
     (or subtract) the two operands and use a recursive call to try the
     comparison with zero.  Don't do any of these cases if branches are
     very cheap.  */

  if (BRANCH_COST > 0
      && GET_MODE_CLASS (mode) == MODE_INT && (code == EQ || code == NE)
      && op1 != const0_rtx)
    {
      tem = expand_binop (mode, xor_optab, op0, op1, subtarget, 1,
			  OPTAB_WIDEN);

      if (tem == 0)
	tem = expand_binop (mode, sub_optab, op0, op1, subtarget, 1,
			    OPTAB_WIDEN);
      if (tem != 0)
	tem = emit_store_flag (target, code, tem, const0_rtx,
			       mode, unsignedp, normalizep);
      if (tem == 0)
	delete_insns_since (last);
      return tem;
    }

  /* Some other cases we can do are EQ, NE, LE, and GT comparisons with 
     the constant zero.  Reject all other comparisons at this point.  Only
     do LE and GT if branches are expensive since they are expensive on
     2-operand machines.  */

  if (BRANCH_COST == 0
      || GET_MODE_CLASS (mode) != MODE_INT || op1 != const0_rtx
      || (code != EQ && code != NE
	  && (BRANCH_COST <= 1 || (code != LE && code != GT))))
    return 0;

  /* See what we need to return.  We can only return a 1, -1, or the
     sign bit.  */

  if (normalizep == 0)
    {
      if (STORE_FLAG_VALUE == 1 || STORE_FLAG_VALUE == -1)
	normalizep = STORE_FLAG_VALUE;

      else if (GET_MODE_BITSIZE (mode) <= HOST_BITS_PER_WIDE_INT
	       && (STORE_FLAG_VALUE
		   == (HOST_WIDE_INT) 1 << (GET_MODE_BITSIZE (mode) - 1)))
	;
      else
	return 0;
    }

  /* Try to put the result of the comparison in the sign bit.  Assume we can't
     do the necessary operation below.  */

  tem = 0;

  /* To see if A <= 0, compute (A | (A - 1)).  A <= 0 iff that result has
     the sign bit set.  */

  if (code == LE)
    {
      /* This is destructive, so SUBTARGET can't be OP0.  */
      if (rtx_equal_p (subtarget, op0))
	subtarget = 0;

      tem = expand_binop (mode, sub_optab, op0, const1_rtx, subtarget, 0,
			  OPTAB_WIDEN);
      if (tem)
	tem = expand_binop (mode, ior_optab, op0, tem, subtarget, 0,
			    OPTAB_WIDEN);
    }

  /* To see if A > 0, compute (((signed) A) << BITS) - A, where BITS is the
     number of bits in the mode of OP0, minus one.  */

  if (code == GT)
    {
      if (rtx_equal_p (subtarget, op0))
	subtarget = 0;

      tem = expand_shift (RSHIFT_EXPR, mode, op0,
			  size_int (GET_MODE_BITSIZE (mode) - 1),
			  subtarget, 0);
      tem = expand_binop (mode, sub_optab, tem, op0, subtarget, 0,
			  OPTAB_WIDEN);
    }
				    
  if (code == EQ || code == NE)
    {
      /* For EQ or NE, one way to do the comparison is to apply an operation
	 that converts the operand into a positive number if it is non-zero
	 or zero if it was originally zero.  Then, for EQ, we subtract 1 and
	 for NE we negate.  This puts the result in the sign bit.  Then we
	 normalize with a shift, if needed. 

	 Two operations that can do the above actions are ABS and FFS, so try
	 them.  If that doesn't work, and MODE is smaller than a full word,
	 we can use zero-extension to the wider mode (an unsigned conversion)
	 as the operation.  */

      if (abs_optab->handlers[(int) mode].insn_code != CODE_FOR_nothing)
	tem = expand_unop (mode, abs_optab, op0, subtarget, 1);
      else if (ffs_optab->handlers[(int) mode].insn_code != CODE_FOR_nothing)
	tem = expand_unop (mode, ffs_optab, op0, subtarget, 1);
      else if (GET_MODE_SIZE (mode) < UNITS_PER_WORD)
	{
	  mode = word_mode;
	  op0 = protect_from_queue (op0, 0);
	  tem = convert_to_mode (mode, op0, 1);
	}

      if (tem != 0)
	{
	  if (code == EQ)
	    tem = expand_binop (mode, sub_optab, tem, const1_rtx, subtarget,
				0, OPTAB_WIDEN);
	  else
	    tem = expand_unop (mode, neg_optab, tem, subtarget, 0);
	}

      /* If we couldn't do it that way, for NE we can "or" the two's complement
	 of the value with itself.  For EQ, we take the one's complement of
	 that "or", which is an extra insn, so we only handle EQ if branches
	 are expensive.  */

      if (tem == 0 && (code == NE || BRANCH_COST > 1))
	{
	  if (rtx_equal_p (subtarget, op0))
	    subtarget = 0;

	  tem = expand_unop (mode, neg_optab, op0, subtarget, 0);
	  tem = expand_binop (mode, ior_optab, tem, op0, subtarget, 0,
			      OPTAB_WIDEN);

	  if (tem && code == EQ)
	    tem = expand_unop (mode, one_cmpl_optab, tem, subtarget, 0);
	}
    }

  if (tem && normalizep)
    tem = expand_shift (RSHIFT_EXPR, mode, tem,
			size_int (GET_MODE_BITSIZE (mode) - 1),
			tem, normalizep == 1);

  if (tem && GET_MODE (tem) != target_mode)
    {
      convert_move (target, tem, 0);
      tem = target;
    }

  if (tem == 0)
    delete_insns_since (last);

  return tem;
}
