/* Definitions for Intel 386 running system V, using gas.
   Copyright (C) 1992 Free Software Foundation, Inc.

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

#include "gas.h"

/* Add stuff that normally comes from i386v.h */

/* longjmp may fail to restore the registers if called from the same
   function that called setjmp.  To compensate, the compiler avoids
   putting variables in registers in functions that use both setjmp
   and longjmp.  */

#define NON_SAVING_SETJMP \
  (current_function_calls_setjmp && current_function_calls_longjmp)

/* longjmp may fail to restore the stack pointer if the saved frame
   pointer is the same as the caller's frame pointer.  Requiring a frame
   pointer in any function that calls setjmp or longjmp avoids this
   problem, unless setjmp and longjmp are called from the same function.
   Since a frame pointer will be required in such a function, it is OK
   that the stack pointer is not restored.  */

#undef FRAME_POINTER_REQUIRED
#define FRAME_POINTER_REQUIRED \
  (current_function_calls_setjmp || current_function_calls_longjmp)

/* Modify ASM_OUTPUT_LOCAL slightly to test -msvr3-shlib, adapted to gas  */
#undef ASM_OUTPUT_LOCAL
#define ASM_OUTPUT_LOCAL(FILE, NAME, SIZE, ROUNDED)	\
  do {							\
    int align = exact_log2 (ROUNDED);			\
    if (align > 2) align = 2;				\
    if (TARGET_SVR3_SHLIB)				\
      {							\
	data_section ();				\
	ASM_OUTPUT_ALIGN ((FILE), align == -1 ? 2 : align); \
	ASM_OUTPUT_LABEL ((FILE), (NAME));		\
	fprintf ((FILE), "\t.set .,.+%u\n", (ROUNDED));	\
      }							\
    else						\
      {							\
	fputs (".lcomm ", (FILE));			\
	assemble_name ((FILE), (NAME));			\
	fprintf ((FILE), ",%u\n", (ROUNDED));		\
      }							\
  } while (0)

/* Add stuff that normally comes from i386v.h via svr3.h */

/* Define the actual types of some ANSI-mandated types.  These
   definitions should work for most SVR3 systems.  */

#undef SIZE_TYPE
#define SIZE_TYPE "unsigned int"

#undef PTRDIFF_TYPE
#define PTRDIFF_TYPE "int"

#undef WCHAR_TYPE
#define WCHAR_TYPE "long int"

#undef WCHAR_TYPE_SIZE
#define WCHAR_TYPE_SIZE BITS_PER_WORD
