/* number.h: Arbitrary precision numbers header file. */

/*  This file is part of bc written for MINIX.
    Copyright (C) 1991, 1992 Free Software Foundation, Inc.

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


typedef enum {PLUS, MINUS} sign;

typedef struct
    {
      sign n_sign;
      int  n_len;	/* The number of digits before the decimal point. */
      int  n_scale;	/* The number of digits after the decimal point. */
      int  n_refs;      /* The number of pointers to this number. */
      char n_value[1];  /* The storage. Not zero char terminated. It is 
      			   allocated with all other fields.  */
    } bc_struct;

typedef bc_struct *bc_num;

/*  Some useful macros and constants. */

#define CH_VAL(c)     (c - '0')
#define BCD_CHAR(d)   (d + '0')

#ifdef MIN
#undef MIN
#undef MAX
#endif
#define MAX(a,b)      (a>b?a:b)
#define MIN(a,b)      (a>b?b:a)
#define ODD(a)        (a&1)

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
