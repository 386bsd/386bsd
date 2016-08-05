/* const.h: Constants for bc. */

/*  This file is part of bc written for MINIX.
    Copyright (C) 1991 Free Software Foundation, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License , or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; see the file COPYING.  If not, write to
    the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

    You may contact the author by:
       e-mail:  phil@cs.wwu.edu
      us-mail:  Philip A. Nelson
                Computer Science Department, 9062
                Western Washington University
                Bellingham, WA 98226-9062
       
*************************************************************************/


/* Define INT_MAX and LONG_MAX if not defined.  Assuming 32 bits... */

#ifdef NO_LIMITS
#define INT_MAX 0x7FFFFFFF
#define LONG_MAX 0x7FFFFFFF
#endif


/* Define constants in some reasonable size.  The next 4 constants are
   POSIX constants. */

#define BC_BASE_MAX       999	/* No good reason for this value. */
#define BC_SCALE_MAX  INT_MAX
#define BC_STRING_MAX INT_MAX


/* Definitions for arrays. */

#define BC_DIM_MAX    65535       /* this should be NODE_SIZE^NODE_DEPTH-1 */

#define   NODE_SIZE        16     /* Must be a power of 2. */
#define   NODE_MASK       0xf     /* Must be NODE_SIZE-1. */
#define   NODE_SHIFT        4     /* Number of 1 bits in NODE_MASK. */
#define   NODE_DEPTH        4


/* Other BC limits defined but not part of POSIX. */

#define BC_LABEL_GROUP 64
#define BC_LABEL_LOG    6
#define BC_MAX_SEGS    10    /* Code segments. */
#define BC_SEG_SIZE  1024
#define BC_SEG_LOG     10

/* Maximum number of variables, arrays and functions and the
   allocation increment for the dynamic arrays. */

#define MAX_STORE   32767
#define STORE_INCR     32

/* Other interesting constants. */

#define FALSE 0
#define TRUE  1
#define SIMPLE 0
#define ARRAY  1
#define FUNCT  2
#define EXTERN extern
#ifdef __STDC__
#define CONST const
#define VOID  void
#else
#define CONST
#define VOID
#endif

/* Include the version definition. */
#include "version.h"
