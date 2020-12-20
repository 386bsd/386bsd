/* System dependent declarations.
   Copyright (C) 1988, 1989, 1992, 1993 Free Software Foundation, Inc.

This file is part of GNU DIFF.

GNU DIFF is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU DIFF is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU DIFF; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include <sys/types.h>
#include <sys/stat.h>
#define STDC_HEADERS 1

#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(mode) (((mode) & S_IFMT) == S_IFREG)
#endif

#include <unistd.h>

#include <time.h>

#include <fcntl.h>

#ifndef O_RDONLY
#define O_RDONLY 0
#endif

#include <sys/wait.h>

#define STAT_BLOCKSIZE(s) (s).st_blksize

#include <dirent.h>
#ifdef direct
#undef direct
#endif

#include <string.h>
#ifndef index
#define index	strchr
#endif
#ifndef rindex
#define rindex	strrchr
#endif
#define bcopy(s,d,n)	memcpy (d,s,n)
#define bcmp(s1,s2,n)	memcmp (s1,s2,n)
#define bzero(s,n)	memset (s,0,n)

#include <stdlib.h>
#include <limits.h>

#include <errno.h>

#ifdef TRUE
#undef TRUE
#endif
#ifdef FALSE
#undef FALSE
#endif
#define TRUE		1
#define	FALSE		0

#if !__STDC__
#define volatile
#endif

#define min(a,b) ((a) <= (b) ? (a) : (b))
#define max(a,b) ((a) >= (b) ? (a) : (b))
