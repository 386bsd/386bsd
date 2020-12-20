/* Copyright (C) 1989, 1990, 1991, 1992 Free Software Foundation, Inc.
     Written by James Clark (jjc@jclark.com)

This file is part of groff.

groff is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

groff is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with groff; see the file COPYING.  If not, write to the Free Software
Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "posix.h"

extern const char *program_name;

static void ewrite(const char *s)
{
  write(2, s, strlen(s));
}

void *operator new(size_t size)
{
  // Avoid relying on the behaviour of malloc(0).
  if (size == 0)
    size++;
#ifdef COOKIE_BUG
  char *p = (char *)malloc(unsigned(size + 8));
  if (p != 0) {
    ((unsigned *)p)[1] = 0;
    return p + 8;
  }
#else /* not COOKIE_BUG */
  char *p = (char *)malloc(unsigned(size));
  if (p != 0)
    return p;
#endif /* not COOKIE_BUG */
  if (program_name) {
    ewrite(program_name);
    ewrite(": ");
  }
  ewrite("out of memory\n");
  _exit(-1);
}

#ifdef COOKIE_BUG

void operator delete(void *p)
{
  if (p)
    free((void *)((char *)p - 8));
}

#endif /* COOKIE_BUG */
