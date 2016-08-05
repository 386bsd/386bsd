/* Declarations for variables relating to reading the source file.
   Used by parsers, lexical analyzers, and error message routines.

   Copyright (C) 1993 Free Software Foundation, Inc.

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

/* Source file current line is coming from.  */
extern char *input_filename;

/* Top-level source file.  */
extern char *main_input_filename;

/* Line number in current source file.  */
extern int lineno;

/* Stream for reading from input file.  */
extern FILE *finput;

struct file_stack
  {
    char *name;
    struct file_stack *next;
    int line;
  };

/* Stack of currently pending input files.
   The line member is not accurate for the innermost file on the stack.  */
extern struct file_stack *input_file_stack;

/* Incremented on each change to input_file_stack.  */
extern int input_file_stack_tick;
