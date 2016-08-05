/* kwset.h - header declaring the keyword set library.
   Copyright 1989 Free Software Foundation
		  Written August 1989 by Mike Haertel.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 1, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   The author may be reached (Email) at the address mike@ai.mit.edu,
   or (US mail) as Mike Haertel c/o Free Software Foundation. */

struct kwsmatch
{
  int index;			/* Index number of matching keyword. */
  char *beg[1];			/* Begin pointer for each submatch. */
  size_t size[1];		/* Length of each submatch. */
};

#if __STDC__

typedef void *kwset_t;

/* Return an opaque pointer to a newly allocated keyword set, or NULL
   if enough memory cannot be obtained.  The argument if non-NULL
   specifies a table of character translations to be applied to all
   pattern and search text. */
extern kwset_t kwsalloc(const char *);

/* Incrementally extend the keyword set to include the given string.
   Return NULL for success, or an error message.  Remember an index
   number for each keyword included in the set. */
extern const char *kwsincr(kwset_t, const char *, size_t);

/* When the keyword set has been completely built, prepare it for
   use.  Return NULL for success, or an error message. */
extern const char *kwsprep(kwset_t);

/* Search through the given buffer for a member of the keyword set.
   Return a pointer to the leftmost longest match found, or NULL if
   no match is found.  If foundlen is non-NULL, store the length of
   the matching substring in the integer it points to.  Similarly,
   if foundindex is non-NULL, store the index of the particular
   keyword found therein. */
extern char *kwsexec(kwset_t, char *, size_t, struct kwsmatch *);

/* Deallocate the given keyword set and all its associated storage. */
extern void kwsfree(kwset_t);

#else

typedef char *kwset_t;

extern kwset_t kwsalloc();
extern char *kwsincr();
extern char *kwsprep();
extern char *kwsexec();
extern void kwsfree();

#endif
