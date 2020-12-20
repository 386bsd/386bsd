/* Front-end tree definitions for GNU compiler.
   Copyright (C) 1989, 1991 Free Software Foundation, Inc.

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

#ifndef REAL_H_INCLUDED
#define REAL_H_INCLUDED

/* Define codes for all the float formats that we know of.  */
#define UNKNOWN_FLOAT_FORMAT 0
#define IEEE_FLOAT_FORMAT 1
#define VAX_FLOAT_FORMAT 2
#define IBM_FLOAT_FORMAT 3

/* Default to IEEE float if not specified.  Nearly all machines use it.  */

#ifndef TARGET_FLOAT_FORMAT
#define	TARGET_FLOAT_FORMAT	IEEE_FLOAT_FORMAT
#endif

#ifndef HOST_FLOAT_FORMAT
#define	HOST_FLOAT_FORMAT	IEEE_FLOAT_FORMAT
#endif

#if TARGET_FLOAT_FORMAT == IEEE_FLOAT_FORMAT
#define REAL_INFINITY
#endif

/* Defining REAL_ARITHMETIC invokes a floating point emulator
   that can produce a target machine format differing by more
   than just endian-ness from the host's format.  The emulator
   is also used to support extended real XFmode.  */
#ifndef LONG_DOUBLE_TYPE_SIZE
#define LONG_DOUBLE_TYPE_SIZE 64
#endif
#if (LONG_DOUBLE_TYPE_SIZE == 96) || defined (REAL_ARITHMETIC)
/* **** Start of software floating point emulator interface macros **** */

/* Support 80-bit extended real XFmode if LONG_DOUBLE_TYPE_SIZE
   has been defined to be 96 in the tm.h machine file. */
#if (LONG_DOUBLE_TYPE_SIZE == 96)
#define REAL_IS_NOT_DOUBLE
#define REAL_ARITHMETIC
typedef struct {
  HOST_WIDE_INT r[(11 + sizeof (HOST_WIDE_INT))/(sizeof (HOST_WIDE_INT))];
} realvaluetype;
#define REAL_VALUE_TYPE realvaluetype

#else /* no XFmode support */

#if HOST_FLOAT_FORMAT != TARGET_FLOAT_FORMAT
/* If no XFmode support, then a REAL_VALUE_TYPE is 64 bits wide
   but it is not necessarily a host machine double. */
#define REAL_IS_NOT_DOUBLE
typedef struct {
  HOST_WIDE_INT r[(7 + sizeof (HOST_WIDE_INT))/(sizeof (HOST_WIDE_INT))];
} realvaluetype;
#define REAL_VALUE_TYPE realvaluetype
#else
/* If host and target formats are compatible, then a REAL_VALUE_TYPE
   is actually a host machine double. */
#define REAL_VALUE_TYPE double
#endif
#endif /* no XFmode support */

/* If emulation has been enabled by defining REAL_ARITHMETIC or by
   setting LONG_DOUBLE_TYPE_SIZE to 96, then define macros so that
   they invoke emulator functions. This will succeed only if the machine
   files have been updated to use these macros in place of any
   references to host machine `double' or `float' types.  */
#ifdef REAL_ARITHMETIC
#undef REAL_ARITHMETIC
#define REAL_ARITHMETIC(value, code, d1, d2) \
  earith (&(value), (code), &(d1), &(d2))

/* Declare functions in real.c that are referenced here. */
void earith (), ereal_from_uint (), ereal_from_int (), ereal_to_int ();
void etarldouble (), etardouble ();
long etarsingle ();
int ereal_cmp (), eroundi (), ereal_isneg ();
unsigned int eroundui ();
REAL_VALUE_TYPE etrunci (), etruncui (), ereal_ldexp (), ereal_atof ();
REAL_VALUE_TYPE ereal_negate (), ereal_truncate ();
REAL_VALUE_TYPE ereal_from_float (), ereal_from_double ();

#define REAL_VALUES_EQUAL(x, y) (ereal_cmp ((x), (y)) == 0)
/* true if x < y : */
#define REAL_VALUES_LESS(x, y) (ereal_cmp ((x), (y)) == -1)
#define REAL_VALUE_LDEXP(x, n) ereal_ldexp (x, n)

/* These return REAL_VALUE_TYPE: */
#define REAL_VALUE_RNDZINT(x) (etrunci (x))
#define REAL_VALUE_UNSIGNED_RNDZINT(x) (etruncui (x))
extern REAL_VALUE_TYPE real_value_truncate ();
#define REAL_VALUE_TRUNCATE(mode, x)  real_value_truncate (mode, x)

/* These return int: */
#define REAL_VALUE_FIX(x) (eroundi (x))
#define REAL_VALUE_UNSIGNED_FIX(x) ((unsigned int) eroundui (x))

#define REAL_VALUE_ATOF ereal_atof
#define REAL_VALUE_NEGATE ereal_negate

#define REAL_VALUE_MINUS_ZERO(x) \
 ((ereal_cmp (x, dconst0) == 0) && (ereal_isneg (x) != 0 ))

#define REAL_VALUE_TO_INT ereal_to_int
#define REAL_VALUE_FROM_INT(d, i, j) (ereal_from_int (&d, i, j))
#define REAL_VALUE_FROM_UNSIGNED_INT(d, i, j) (ereal_from_uint (&d, i, j))

/* IN is a REAL_VALUE_TYPE.  OUT is an array of longs. */
#define REAL_VALUE_TO_TARGET_LONG_DOUBLE(IN, OUT) (etarldouble ((IN), (OUT)))
#define REAL_VALUE_TO_TARGET_DOUBLE(IN, OUT) (etardouble ((IN), (OUT)))
/* d is an array of longs. */
#define REAL_VALUE_FROM_TARGET_DOUBLE(d)  (ereal_from_double (d))
/* IN is a REAL_VALUE_TYPE.  OUT is a long. */
#define REAL_VALUE_TO_TARGET_SINGLE(IN, OUT) ((OUT) = etarsingle ((IN)))
/* f is a long. */
#define REAL_VALUE_FROM_TARGET_SINGLE(f)  (ereal_from_float (f))

/* Conversions to decimal ASCII string.  */
#define REAL_VALUE_TO_DECIMAL(r, fmt, s) (ereal_to_decimal (r, s))

#endif /* REAL_ARITHMETIC defined */

/* **** End of software floating point emulator interface macros **** */
#else /* LONG_DOUBLE_TYPE_SIZE != 96 and REAL_ARITHMETIC not defined */

/* old interface */
#ifdef REAL_ARITHMETIC
/* Defining REAL_IS_NOT_DOUBLE breaks certain initializations
   when REAL_ARITHMETIC etc. are not defined.  */

/* Now see if the host and target machines use the same format. 
   If not, define REAL_IS_NOT_DOUBLE (even if we end up representing
   reals as doubles because we have no better way in this cross compiler.)
   This turns off various optimizations that can happen when we know the
   compiler's float format matches the target's float format.
   */
#if HOST_FLOAT_FORMAT != TARGET_FLOAT_FORMAT
#define	REAL_IS_NOT_DOUBLE
#ifndef REAL_VALUE_TYPE
typedef struct {
    HOST_WIDE_INT r[sizeof (double)/sizeof (HOST_WIDE_INT)];
  } realvaluetype;
#define REAL_VALUE_TYPE realvaluetype
#endif /* no REAL_VALUE_TYPE */
#endif /* formats differ */
#endif /* 0 */

#endif /* emulator not used */

/* If we are not cross-compiling, use a `double' to represent the
   floating-point value.  Otherwise, use some other type
   (probably a struct containing an array of longs).  */
#ifndef REAL_VALUE_TYPE
#define REAL_VALUE_TYPE double
#else
#define REAL_IS_NOT_DOUBLE
#endif

#if HOST_FLOAT_FORMAT == TARGET_FLOAT_FORMAT

/* Convert a type `double' value in host format first to a type `float'
   value in host format and then to a single type `long' value which
   is the bitwise equivalent of the `float' value.  */
#ifndef REAL_VALUE_TO_TARGET_SINGLE
#define REAL_VALUE_TO_TARGET_SINGLE(IN, OUT)				\
do { float f = (float) (IN);						\
     (OUT) = *(long *) &f;						\
   } while (0)
#endif

/* Convert a type `double' value in host format to a pair of type `long'
   values which is its bitwise equivalent, but put the two words into
   proper word order for the target.  */
#ifndef REAL_VALUE_TO_TARGET_DOUBLE
#if defined (HOST_WORDS_BIG_ENDIAN) == WORDS_BIG_ENDIAN
#define REAL_VALUE_TO_TARGET_DOUBLE(IN, OUT)				\
do { REAL_VALUE_TYPE in = (IN);  /* Make sure it's not in a register.  */\
     (OUT)[0] = ((long *) &in)[0];					\
     (OUT)[1] = ((long *) &in)[1];					\
   } while (0)
#else
#define REAL_VALUE_TO_TARGET_DOUBLE(IN, OUT)				\
do { REAL_VALUE_TYPE in = (IN);  /* Make sure it's not in a register.  */\
     (OUT)[1] = ((long *) &in)[0];					\
     (OUT)[0] = ((long *) &in)[1];					\
   } while (0)
#endif
#endif
#endif /* HOST_FLOAT_FORMAT == TARGET_FLOAT_FORMAT */

/* In this configuration, double and long double are the same. */
#ifndef REAL_VALUE_TO_TARGET_LONG_DOUBLE
#define REAL_VALUE_TO_TARGET_LONG_DOUBLE(a, b) REAL_VALUE_TO_TARGET_DOUBLE (a, b)
#endif

/* Compare two floating-point values for equality.  */
#ifndef REAL_VALUES_EQUAL
#define REAL_VALUES_EQUAL(x, y) ((x) == (y))
#endif

/* Compare two floating-point values for less than.  */
#ifndef REAL_VALUES_LESS
#define REAL_VALUES_LESS(x, y) ((x) < (y))
#endif

/* Truncate toward zero to an integer floating-point value.  */
#ifndef REAL_VALUE_RNDZINT
#define REAL_VALUE_RNDZINT(x) ((double) ((int) (x)))
#endif

/* Truncate toward zero to an unsigned integer floating-point value.  */
#ifndef REAL_VALUE_UNSIGNED_RNDZINT
#define REAL_VALUE_UNSIGNED_RNDZINT(x) ((double) ((unsigned int) (x)))
#endif

/* Convert a floating-point value to integer, using any rounding mode.  */
#ifndef REAL_VALUE_FIX
#define REAL_VALUE_FIX(x) ((int) (x))
#endif

/* Convert a floating-point value to unsigned integer, using any rounding
   mode.  */
#ifndef REAL_VALUE_UNSIGNED_FIX
#define REAL_VALUE_UNSIGNED_FIX(x) ((unsigned int) (x))
#endif

/* Scale X by Y powers of 2.  */
#ifndef REAL_VALUE_LDEXP
#define REAL_VALUE_LDEXP(x, y) ldexp (x, y)
extern double ldexp ();
#endif

/* Convert the string X to a floating-point value.  */
#ifndef REAL_VALUE_ATOF
#if 1
/* Use real.c to convert decimal numbers to binary, ... */
REAL_VALUE_TYPE ereal_atof ();
#define REAL_VALUE_ATOF(x, s) ereal_atof (x, s)
#else
/* ... or, if you like the host computer's atof, go ahead and use it: */
#define REAL_VALUE_ATOF(x, s) atof (x)
#if defined (MIPSEL) || defined (MIPSEB)
/* MIPS compiler can't handle parens around the function name.
   This problem *does not* appear to be connected with any
   macro definition for atof.  It does not seem there is one.  */
extern double atof ();
#else
extern double (atof) ();
#endif
#endif
#endif

/* Negate the floating-point value X.  */
#ifndef REAL_VALUE_NEGATE
#define REAL_VALUE_NEGATE(x) (- (x))
#endif

/* Truncate the floating-point value X to mode MODE.  This is correct only
   for the most common case where the host and target have objects of the same
   size and where `float' is SFmode.  */

/* Don't use REAL_VALUE_TRUNCATE directly--always call real_value_truncate.  */
extern REAL_VALUE_TYPE real_value_truncate ();

#ifndef REAL_VALUE_TRUNCATE
#define REAL_VALUE_TRUNCATE(mode, x) \
 (GET_MODE_BITSIZE (mode) == sizeof (float) * HOST_BITS_PER_CHAR	\
  ? (float) (x) : (x))
#endif

/* Determine whether a floating-point value X is infinite. */
#ifndef REAL_VALUE_ISINF
#define REAL_VALUE_ISINF(x) (target_isinf (x))
#endif

/* Determine whether a floating-point value X is a NaN. */
#ifndef REAL_VALUE_ISNAN
#define REAL_VALUE_ISNAN(x) (target_isnan (x))
#endif

/* Determine whether a floating-point value X is negative. */
#ifndef REAL_VALUE_NEGATIVE
#define REAL_VALUE_NEGATIVE(x) (target_negative (x))
#endif

/* Determine whether a floating-point value X is minus 0. */
#ifndef REAL_VALUE_MINUS_ZERO
#define REAL_VALUE_MINUS_ZERO(x) ((x) == 0 && REAL_VALUE_NEGATIVE (x))
#endif

/* Constant real values 0, 1, 2, and -1.  */

extern REAL_VALUE_TYPE dconst0;
extern REAL_VALUE_TYPE dconst1;
extern REAL_VALUE_TYPE dconst2;
extern REAL_VALUE_TYPE dconstm1;

/* Union type used for extracting real values from CONST_DOUBLEs
   or putting them in.  */

union real_extract 
{
  REAL_VALUE_TYPE d;
  HOST_WIDE_INT i[sizeof (REAL_VALUE_TYPE) / sizeof (HOST_WIDE_INT)];
};

/* For a CONST_DOUBLE:
   The usual two ints that hold the value.
   For a DImode, that is all there are;
    and CONST_DOUBLE_LOW is the low-order word and ..._HIGH the high-order.
   For a float, the number of ints varies,
    and CONST_DOUBLE_LOW is the one that should come first *in memory*.
    So use &CONST_DOUBLE_LOW(r) as the address of an array of ints.  */
#define CONST_DOUBLE_LOW(r) XWINT (r, 2)
#define CONST_DOUBLE_HIGH(r) XWINT (r, 3)

/* Link for chain of all CONST_DOUBLEs in use in current function.  */
#define CONST_DOUBLE_CHAIN(r) XEXP (r, 1)
/* The MEM which represents this CONST_DOUBLE's value in memory,
   or const0_rtx if no MEM has been made for it yet,
   or cc0_rtx if it is not on the chain.  */
#define CONST_DOUBLE_MEM(r) XEXP (r, 0)

/* Function to return a real value (not a tree node)
   from a given integer constant.  */
REAL_VALUE_TYPE real_value_from_int_cst ();

/* Given a CONST_DOUBLE in FROM, store into TO the value it represents.  */

#define REAL_VALUE_FROM_CONST_DOUBLE(to, from)		\
do { union real_extract u;				\
     bcopy (&CONST_DOUBLE_LOW ((from)), &u, sizeof u);	\
     to = u.d; } while (0)

/* Return a CONST_DOUBLE with value R and mode M.  */

#define CONST_DOUBLE_FROM_REAL_VALUE(r, m) immed_real_const_1 (r,  m)

/* Convert a floating point value `r', that can be interpreted
   as a host machine float or double, to a decimal ASCII string `s'
   using printf format string `fmt'.  */
#ifndef REAL_VALUE_TO_DECIMAL
#define REAL_VALUE_TO_DECIMAL(r, fmt, s) (sprintf (s, fmt, r))
#endif

#endif /* Not REAL_H_INCLUDED */
