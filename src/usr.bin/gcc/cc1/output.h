/* Declarations for insn-output.c.  These functions are defined in recog.c,
   final.c, and varasm.c.
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

#ifndef STDIO_PROTO
#ifdef BUFSIZ
#define STDIO_PROTO(ARGS) PROTO(ARGS)
#else
#define STDIO_PROTO(ARGS) ()
#endif
#endif

/* Output a string of assembler code, substituting insn operands.
   Defined in final.c.  */
extern void output_asm_insn PROTO((char *, rtx *));

/* Output a string of assembler code, substituting numbers, strings
   and fixed syntactic prefixes.  */
extern void asm_fprintf ();

/* Print an integer constant expression in assembler syntax.
   Addition and subtraction are the only arithmetic
   that may appear in these expressions.  */
extern void output_addr_const STDIO_PROTO((FILE *, rtx));

/* Output a name (as found inside a symbol_ref) in assembler syntax.  */
extern void assemble_name STDIO_PROTO((FILE *, char *));

/* Replace a SUBREG with a REG or a MEM, based on the thing it is a
   subreg of.  */
extern rtx alter_subreg PROTO((rtx));

/* When outputting assembler code, indicates which alternative
   of the constraints was actually satisfied.  */
extern int which_alternative;

/* When outputting delayed branch sequences, this rtx holds the
   sequence being output.  It is null when no delayed branch
   sequence is being output, so it can be used as a test in the
   insn output code.

   This variable is defined  in final.c.  */
extern rtx final_sequence;

/* Number of bytes of args popped by function being compiled on its return.
   Zero if no bytes are to be popped.
   May affect compilation of return insn or of function epilogue.  */

extern int current_function_pops_args;

/* Nonzero if function being compiled needs to be given an address
   where the value should be stored.  */

extern int current_function_returns_struct;

/* Nonzero if function being compiled needs to
   return the address of where it has put a structure value.  */

extern int current_function_returns_pcc_struct;

/* Nonzero if function being compiled needs to be passed a static chain.  */

extern int current_function_needs_context;

/* Nonzero if function being compiled can call setjmp.  */

extern int current_function_calls_setjmp;

/* Nonzero if function being compiled can call longjmp.  */

extern int current_function_calls_longjmp;

/* Nonzero if function being compiled can call alloca,
   either as a subroutine or builtin.  */

extern int current_function_calls_alloca;

/* Nonzero if function being compiled receives nonlocal gotos
   from nested functions.  */

extern int current_function_has_nonlocal_label;

/* Nonzero if function being compiled contains nested functions.  */

extern int current_function_contains_functions;

/* Nonzero if the current function returns a pointer type */

extern int current_function_returns_pointer;

/* If function's args have a fixed size, this is that size, in bytes.
   Otherwise, it is -1.
   May affect compilation of return insn or of function epilogue.  */

extern int current_function_args_size;

/* # bytes the prologue should push and pretend that the caller pushed them.
   The prologue must do this, but only if parms can be passed in registers.  */

extern int current_function_pretend_args_size;

/* # of bytes of outgoing arguments required to be pushed by the prologue.
   If this is non-zero, it means that ACCUMULATE_OUTGOING_ARGS was defined
   and no stack adjusts will be done on function calls.  */

extern int current_function_outgoing_args_size;

/* Nonzero if current function uses varargs.h or equivalent.
   Zero for functions that use stdarg.h.  */

extern int current_function_varargs;

/* Quantities of various kinds of registers
   used for the current function's args.  */

extern CUMULATIVE_ARGS current_function_args_info;

/* Name of function now being compiled.  */

extern char *current_function_name;

/* If non-zero, an RTL expression for that location at which the current
   function returns its result.  Usually equal to
   DECL_RTL (DECL_RESULT (current_function_decl)).  */

extern rtx current_function_return_rtx;

/* If some insns can be deferred to the delay slots of the epilogue, the
   delay list for them is recorded here.  */

extern rtx current_function_epilogue_delay_list;

/* Nonzero means generate position-independent code.
   This is not fully implemented yet.  */

extern int flag_pic;

/* This is nonzero if the current function uses pic_offset_table_rtx.  */
extern int current_function_uses_pic_offset_table;

/* This is nonzero if the current function uses the constant pool.  */
extern int current_function_uses_const_pool;

/* The line number of the beginning of the current function.
   sdbout.c needs this so that it can output relative linenumbers.  */

#ifdef SDB_DEBUGGING_INFO /* Avoid undef sym in certain broken linkers.  */
extern int sdb_begin_function_line;
#endif

/* File in which assembler code is being written.  */

#ifdef BUFSIZ
extern FILE *asm_out_file;
#endif
