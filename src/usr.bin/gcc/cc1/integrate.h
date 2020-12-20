/* Function integration definitions for GNU C-Compiler
   Copyright (C) 1990 Free Software Foundation, Inc.

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

/* This structure is used to remap objects in the function being inlined to
   those belonging to the calling function.  It is passed by
   expand_inline_function to its children.

   This structure is also used when unrolling loops and otherwise
   replicating code, although not all fields are needed in this case;
   only those fields needed by copy_rtx_and_substitute() and its children
   are used.

   This structure is used instead of static variables because
   expand_inline_function may be called recursively via expand_expr.  */

struct inline_remap
{
  /* True if we are doing function integration, false otherwise.
     Used to control whether RTX_UNCHANGING bits are copied by
     copy_rtx_and_substitute. */
  int integrating;
  /* Definition of function be inlined.  */
  union tree_node *fndecl;
  /* Place to put insns needed at start of function.  */
  rtx insns_at_start;
  /* Mapping from old registers to new registers.
     It is allocated and deallocated in `expand_inline_function' */
  rtx *reg_map;
  /* Mapping from old code-labels to new code-labels.
     The first element of this map is label_map[min_labelno].  */
  rtx *label_map;
  /* Mapping from old insn uid's to copied insns.  The first element
   of this map is insn_map[min_insnno]; the last element is
   insn_map[max_insnno].  We keep the bounds here for when the map
   only covers a partial range of insns (such as loop unrolling or
   code replication).  */
  rtx *insn_map;
  int min_insnno, max_insnno;

  /* Map pseudo reg number in calling function to equivalent constant.  We
     cannot in general substitute constants into parameter pseudo registers,
     since some machine descriptions (many RISCs) won't always handle
     the resulting insns.  So if an incoming parameter has a constant
     equivalent, we record it here, and if the resulting insn is
     recognizable, we go with it.

     We also use this mechanism to convert references to incoming arguments
     and stacked variables.  copy_rtx_and_substitute will replace the virtual
     incoming argument and virtual stacked variables registers with new
     pseudos that contain pointers into the replacement area allocated for
     this inline instance.  These pseudos are then marked as being equivalent
     to the appropriate address and substituted if valid.  */
  rtx *const_equiv_map;
  /* Number of entries in const_equiv_map and const_arg_map.  */
  int const_equiv_map_size;
  /* This is incremented for each new basic block.
     It is used to store in const_age_map to record the domain of validity
     of each entry in const_equiv_map.
     A value of -1 indicates an entry for a reg which is a parm.
     All other values are "positive".  */
#define CONST_AGE_PARM (-1)
  unsigned int const_age;
  /* In parallel with const_equiv_map, record the valid age for each entry.
     The entry is invalid if its age is less than const_age.  */
  unsigned int *const_age_map;
  /* Target of the inline function being expanded, or NULL if none.  */
  rtx inline_target;
  /* When an insn is being copied by copy_rtx_and_substitute,
     this is nonzero if we have copied an ASM_OPERANDS.
     In that case, it is the original input-operand vector.  */
  rtvec orig_asm_operands_vector;
  /* When an insn is being copied by copy_rtx_and_substitute,
     this is nonzero if we have copied an ASM_OPERANDS.
     In that case, it is the copied input-operand vector.  */
  rtvec copy_asm_operands_vector;
  /* Likewise, this is the copied constraints vector.  */
  rtvec copy_asm_constraints_vector;

  /* The next few fields are used for subst_constants to record the SETs
     that it saw.  */
  int num_sets;
  struct equiv_table
    {
      rtx dest;
      rtx equiv;
    }  equiv_sets[MAX_RECOG_OPERANDS];
  /* Record the last thing assigned to pc.  This is used for folded 
     conditional branch insns.  */
  rtx last_pc_value;
#ifdef HAVE_cc0
  /* Record the last thing assigned to cc0.  */
  rtx last_cc0_value;
#endif
};

/* Return a copy of an rtx (as needed), substituting pseudo-register,
   labels, and frame-pointer offsets as necessary.  */
extern rtx copy_rtx_and_substitute PROTO((rtx, struct inline_remap *));

extern void try_constants PROTO((rtx, struct inline_remap *));

extern void mark_stores PROTO((rtx, rtx));

extern rtx *global_const_equiv_map;
