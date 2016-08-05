/* Define per-register tables for data flow info and register allocation.
   Copyright (C) 1987 Free Software Foundation, Inc.

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */



#define REG_BYTES(R) mode_size[(int) GET_MODE (R)]

/* Get the number of consecutive hard regs required to hold the REG rtx R.
   When something may be an explicit hard reg, REG_SIZE is the only
   valid way to get this value.  You cannot get it from the regno.  */

#define REG_SIZE(R) \
  ((mode_size[(int) GET_MODE (R)] + UNITS_PER_WORD - 1) / UNITS_PER_WORD)

/* Maximum register number used in this function, plus one.  */

extern int max_regno;

/* Indexed by n, gives number of times (REG n) is used or set.
   References within loops may be counted more times.  */

extern short *reg_n_refs;

/* Indexed by n, gives number of times (REG n) is set.  */

extern short *reg_n_sets;

/* Indexed by N, gives number of insns in which register N dies.
   Note that if register N is live around loops, it can die
   in transitions between basic blocks, and that is not counted here.
   So this is only a reliable indicator of how many regions of life there are
   for registers that are contained in one basic block.  */

extern short *reg_n_deaths;

/* Indexed by N, gives the first insn that mentions reg N,
   provided that reg is local to one basic block.
   The value here is undefined otherwise.  */

extern rtx *reg_first_use;

/* Get the number of consecutive words required to hold pseudo-reg N.  */

#define PSEUDO_REGNO_SIZE(N) \
  ((GET_MODE_SIZE (PSEUDO_REGNO_MODE (N)) + UNITS_PER_WORD - 1)		\
   / UNITS_PER_WORD)

/* Get the number of bytes required to hold pseudo-reg N.  */

#define PSEUDO_REGNO_BYTES(N) \
  GET_MODE_SIZE (PSEUDO_REGNO_MODE (N))

/* Get the machine mode of pseudo-reg N.  */

#define PSEUDO_REGNO_MODE(N) GET_MODE (regno_reg_rtx[N])

/* Indexed by N, gives number of CALL_INSNS across which (REG n) is live.  */

extern int *reg_n_calls_crossed;

/* Total number of instructions at which (REG n) is live.
   The larger this is, the less priority (REG n) gets for
   allocation in a hard register (in global-alloc).
   This is set in flow.c and remains valid for the rest of the compilation
   of the function; it is used to control register allocation.

   local-alloc.c may alter this number to change the priority.

   Negative values are special.
   -1 is used to mark a pseudo reg which has a constant or memory equivalent
   and is used infrequently enough that it should not get a hard register.
   -2 is used to mark a pseudo reg for a parameter, when a frame pointer
   is not required.  global-alloc.c makes an allocno for this but does
   not try to assign a hard register to it.  */

extern int *reg_live_length;

/* Vector of substitutions of register numbers,
   used to map pseudo regs into hardware regs.  */

extern short *reg_renumber;

/* Vector indexed by hardware reg
   saying whether that reg is ever used.  */

extern char regs_ever_live[FIRST_PSEUDO_REGISTER];

/* Vector indexed by hardware reg giving its name.  */

extern char *reg_names[FIRST_PSEUDO_REGISTER];

/* Vector indexed by regno; gives uid of first insn using that reg.
   This is computed by reg_scan for use by cse and loop.
   It is sometimes adjusted for subsequent changes during loop,
   but not adjusted by cse even if cse invalidates it.  */

extern short *regno_first_uid;

/* Vector indexed by regno; gives uid of last insn using that reg.
   This is computed by reg_scan for use by cse and loop.
   It is sometimes adjusted for subsequent changes during loop,
   but not adjusted by cse even if cse invalidates it.
   This is harmless since cse won't scan through a loop end.  */

extern short *regno_last_uid;

/* Vector indexed by regno; contains 1 for a register is considered a pointer.
   Reloading, etc. will use a pointer register rather than a non-pointer
   as the base register in an address, when there is a choice of two regs.  */

extern char *regno_pointer_flag;
#define REGNO_POINTER_FLAG(REGNO) regno_pointer_flag[REGNO]

/* Vector mapping pseudo regno into the REG rtx for that register.
   This is computed by reg_scan.  */

extern rtx *regno_reg_rtx;

/* Flag set by local-alloc or global-alloc if they decide to allocate
   something in a call-clobbered register.  */

extern int caller_save_needed;

/* Predicate to decide whether to give a hard reg to a pseudo which
   is referenced REFS times and would need to be saved and restored
   around a call CALLS times.  */

#ifndef CALLER_SAVE_PROFITABLE
#define CALLER_SAVE_PROFITABLE(REFS, CALLS)  (4 * (CALLS) < (REFS))
#endif
