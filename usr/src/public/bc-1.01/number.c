/* number.c: Implements arbitrary precision numbers. */

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

#include "bcdefs.h"
#include "proto.h"

/* Storage used for special numbers. */
bc_num _zero_;
bc_num _one_;
bc_num _two_;


/* "Frees" a bc_num NUM.  Actually decreases reference count and only
   frees the storage if reference count is zero. */

void
free_num (num)
    bc_num *num;
{
  if (*num == NULL) return;
  (*num)->n_refs--; 
  if ((*num)->n_refs == 0) free(*num);
  *num = NULL;
}


/* new_num allocates a number and sets fields to known values. */

bc_num
new_num (length, scale)
     int length, scale;
{
  bc_num temp;

  temp = (bc_num) malloc (sizeof(bc_struct)+length+scale);
  if (temp == NULL) out_of_memory ();
  temp->n_sign = PLUS;
  temp->n_len = length;
  temp->n_scale = scale;
  temp->n_refs = 1;
  temp->n_value[0] = 0;
  return temp;
}


/* Intitialize the number package! */

void
init_numbers ()
{
  _zero_ = new_num (1,0);
  _one_  = new_num (1,0);
  _one_->n_value[0] = 1;
  _two_  = new_num (1,0);
  _two_->n_value[0] = 2;
}


/* Make a copy of a number!  Just increments the reference count! */

bc_num
copy_num (num)
     bc_num num;
{
  num->n_refs++;
  return num;
}


/* Initialize a number NUM by making it a copy of zero. */

void
init_num (num)
     bc_num *num;
{
  *num = copy_num (_zero_);
}


/* Convert an integer VAL to a bc number NUM. */

void
int2num (num, val)
     bc_num *num;
     int val;
{
  char buffer[30];
  char *bptr, *vptr;
  int  ix = 1;
  char neg = 0;
  
  /* Sign. */
  if (val < 0)
    {
      neg = 1;
      val = -val;
    }
  
  /* Get things going. */
  bptr = buffer;
  *bptr++ = val % 10;
  val = val / 10;
  
  /* Extract remaining digits. */
  while (val != 0)
    {
      *bptr++ = val % 10;
      val = val / 10;
      ix++; 		/* Count the digits. */
    }
  
  /* Make the number. */
  free_num (num);
  *num = new_num (ix, 0);
  if (neg) (*num)->n_sign = MINUS;
  
  /* Assign the digits. */
  vptr = (*num)->n_value;
  while (ix-- > 0)
    *vptr++ = *--bptr;
}


/* Convert a number NUM to a long.  The function returns only the integer 
   part of the number.  For numbers that are too large to represent as
   a long, this function returns a zero.  This can be detected by checking
   the NUM for zero after having a zero returned. */

long
num2long (num)
     bc_num num;
{
  long val;
  char *nptr;
  int  index;

  /* Extract the int value, ignore the fraction. */
  val = 0;
  nptr = num->n_value;
  for (index=num->n_len; (index>0) && (val<=(LONG_MAX/10)); index--)
    val = val*10 + *nptr++;
  
  /* Check for overflow.  If overflow, return zero. */
  if (index>0) val = 0;
  if (val < 0) val = 0;
 
  /* Return the value. */
  if (num->n_sign == PLUS)
    return (val);
  else
    return (-val);
}


/* The following are some math routines for numbers. */
_PROTOTYPE(static int _do_compare, (bc_num n1, bc_num n2, int use_sign,
				    int ignore_last));
_PROTOTYPE(static void _rm_leading_zeros, (bc_num num));
_PROTOTYPE(static bc_num _do_add, (bc_num n1, bc_num n2));
_PROTOTYPE(static bc_num _do_sub, (bc_num n1, bc_num n2));
_PROTOTYPE(static void _one_mult, (unsigned char *num, int size, int digit,
				   unsigned char *result));



/* Compare two bc numbers.  Return value is 0 if equal, -1 if N1 is less
   than N2 and +1 if N1 is greater than N2.  If USE_SIGN is false, just
   compare the magnitudes. */

static int
_do_compare (n1, n2, use_sign, ignore_last)
     bc_num n1, n2;
     int use_sign;
     int ignore_last;
{
  char *n1ptr, *n2ptr;
  int  count;
  
  /* First, compare signs. */
  if (use_sign && n1->n_sign != n2->n_sign)
    {
      if (n1->n_sign == PLUS)
	return (1);	/* Positive N1 > Negative N2 */
      else
	return (-1);	/* Negative N1 < Positive N1 */
    }
  
  /* Now compare the magnitude. */
  if (n1->n_len != n2->n_len)
    {
      if (n1->n_len > n2->n_len)
	{
	  /* Magnitude of n1 > n2. */
	  if (!use_sign || n1->n_sign == PLUS)
	    return (1);
	  else
	    return (-1);
	}
      else
	{
	  /* Magnitude of n1 < n2. */
	  if (!use_sign || n1->n_sign == PLUS)
	    return (-1);
	  else
	    return (1);
	}
    }

  /* If we get here, they have the same number of integer digits.
     check the integer part and the equal length part of the fraction. */
  count = n1->n_len + MIN (n1->n_scale, n2->n_scale);
  n1ptr = n1->n_value;
  n2ptr = n2->n_value;

  while ((count > 0) && (*n1ptr == *n2ptr))
    {
      n1ptr++;
      n2ptr++;
      count--;
    }
  if (ignore_last && count == 1 && n1->n_scale == n2->n_scale)
    return (0);
  if (count != 0)
    {
      if (*n1ptr > *n2ptr)
	{
	  /* Magnitude of n1 > n2. */
	  if (!use_sign || n1->n_sign == PLUS)
	    return (1);
	  else
	    return (-1);
	}
      else
	{
	  /* Magnitude of n1 < n2. */
	  if (!use_sign || n1->n_sign == PLUS)
	    return (-1);
	  else
	    return (1);
	}
    }

  /* They are equal up to the last part of the equal part of the fraction. */
  if (n1->n_scale != n2->n_scale) 
    if (n1->n_scale > n2->n_scale)
      {
	for (count = n1->n_scale-n2->n_scale; count>0; count--)
	  if (*n1ptr++ != 0)
	    {
	      /* Magnitude of n1 > n2. */
	      if (!use_sign || n1->n_sign == PLUS)
		return (1);
	      else
		return (-1);
	    }
      }
    else
      {
	for (count = n2->n_scale-n1->n_scale; count>0; count--)
	  if (*n2ptr++ != 0)
	    {
	      /* Magnitude of n1 < n2. */
	      if (!use_sign || n1->n_sign == PLUS)
		return (-1);
	      else
		return (1);
	    }
      }
  
  /* They must be equal! */
  return (0);
}


/* This is the "user callable" routine to compare numbers N1 and N2. */

int
bc_compare (n1, n2)
     bc_num n1, n2;
{
  return _do_compare (n1, n2, TRUE, FALSE);
}


/* In some places we need to check if the number NUM is zero. */

char
is_zero (num)
     bc_num num;
{
  int  count;
  char *nptr;

  /* Quick check. */
  if (num == _zero_) return TRUE;

  /* Initialize */
  count = num->n_len + num->n_scale;
  nptr = num->n_value;

  /* The check */
  while ((count > 0) && (*nptr++ == 0)) count--;

  if (count != 0)
    return FALSE;
  else 
    return TRUE;
}


/* In some places we need to check if the number is negative. */

char
is_neg (num)
     bc_num num;
{
  return num->n_sign == MINUS;
}


/* For many things, we may have leading zeros in a number NUM.
   _rm_leading_zeros just moves the data to the correct
   place and adjusts the length. */

static void
_rm_leading_zeros (num)
     bc_num num;
{
  int bytes;
  char *dst, *src;

  /* Do a quick check to see if we need to do it. */
  if (*num->n_value != 0) return;

  /* The first digit is 0, find the first non-zero digit in the 10's or
     greater place. */
  bytes = num->n_len;
  src = num->n_value;
  while (bytes > 1 && *src == 0) src++, bytes--;
  num->n_len = bytes;
  bytes += num->n_scale;
  dst = num->n_value;
  while (bytes-- > 0) *dst++ = *src++;
  
}


/* Perform addition: N1 is added to N2 and the value is
   returned.  The signs of N1 and N2 are ignored. */

static bc_num
_do_add (n1, n2)
     bc_num n1, n2;
{
  bc_num sum;
  int sum_scale, sum_digits;
  char *n1ptr, *n2ptr, *sumptr;
  int carry, n1bytes, n2bytes;

  /* Prepare sum. */
  sum_scale = MAX (n1->n_scale, n2->n_scale);
  sum_digits = MAX (n1->n_len, n2->n_len) + 1;
  sum = new_num (sum_digits,sum_scale);

  /* Start with the fraction part.  Initialize the pointers. */
  n1bytes = n1->n_scale;
  n2bytes = n2->n_scale;
  n1ptr = (char *) (n1->n_value + n1->n_len + n1bytes - 1);
  n2ptr = (char *) (n2->n_value + n2->n_len + n2bytes - 1);
  sumptr = (char *) (sum->n_value + sum_scale + sum_digits - 1);

  /* Add the fraction part.  First copy the longer fraction.*/
  if (n1bytes != n2bytes)
    {
      if (n1bytes > n2bytes)
	while (n1bytes>n2bytes)
	  { *sumptr-- = *n1ptr--; n1bytes--;}
      else
	while (n2bytes>n1bytes)
	  { *sumptr-- = *n2ptr--; n2bytes--;}
    }

  /* Now add the remaining fraction part and equal size integer parts. */
  n1bytes += n1->n_len;
  n2bytes += n2->n_len;
  carry = 0;
  while ((n1bytes > 0) && (n2bytes > 0))
    {
      *sumptr = *n1ptr-- + *n2ptr-- + carry;
      if (*sumptr > 9)
	{
	   carry = 1;
	   *sumptr -= 10;
	}
      else
	carry = 0;
      sumptr--;
      n1bytes--;
      n2bytes--;
    }

  /* Now add carry the longer integer part. */
  if (n1bytes == 0)
    { n1bytes = n2bytes; n1ptr = n2ptr; }
  while (n1bytes-- > 0)
    {
      *sumptr = *n1ptr-- + carry;
      if (*sumptr > 9)
	{
	   carry = 1;
	   *sumptr -= 10;
	 }
      else
	carry = 0;
      sumptr--;
    }

  /* Set final carry. */
  if (carry == 1)
    *sumptr += 1;
  
  /* Adjust sum and return. */
  _rm_leading_zeros (sum);
  return sum;  
}


/* Perform subtraction: N2 is subtracted from N1 and the value is
   returned.  The signs of N1 and N2 are ignored.  Also, N1 is
   assumed to be larger than N2.  */

static bc_num
_do_sub (n1, n2)
     bc_num n1, n2;
{
  bc_num diff;
  int diff_scale, diff_len;
  int min_scale, min_len;
  char *n1ptr, *n2ptr, *diffptr;
  int borrow, count, val;

  /* Allocate temporary storage. */
  diff_len = MAX (n1->n_len, n2->n_len);
  diff_scale = MAX (n1->n_scale, n2->n_scale);
  min_len = MIN  (n1->n_len, n2->n_len);
  min_scale = MIN (n1->n_scale, n2->n_scale);
  diff = new_num (diff_len, diff_scale);

  /* Initialize the subtract. */
  n1ptr = (char *) (n1->n_value + n1->n_len + n1->n_scale -1);
  n2ptr = (char *) (n2->n_value + n2->n_len + n2->n_scale -1);
  diffptr = (char *) (diff->n_value + diff_len + diff_scale -1);

  /* Subtract the numbers. */
  borrow = 0;
  
  /* Take care of the longer scaled number. */
  if (n1->n_scale != min_scale)
    {
      /* n1 has the longer scale */
      for (count = n1->n_scale - min_scale; count > 0; count--)
	*diffptr-- = *n1ptr--;
    }
  else
    {
      /* n2 has the longer scale */
      for (count = n2->n_scale - min_scale; count > 0; count--)
	{
	  val = - *n2ptr-- - borrow;
	  if (val < 0)
	    {
	      val += 10;
	      borrow = 1;
	    }
	  else
	    borrow = 0;
	  *diffptr-- = val;
	}
    }
  
  /* Now do the equal length scale and integer parts. */
  
  for (count = 0; count < min_len + min_scale; count++)
    {
      val = *n1ptr-- - *n2ptr-- - borrow;
      if (val < 0)
	{
	  val += 10;
	  borrow = 1;
	}
      else
	borrow = 0;
      *diffptr-- = val;
    }

  /* If n1 has more digits then n2, we now do that subtract. */
  if (diff_len != min_len)
    {
      for (count = diff_len - min_len; count > 0; count--)
	{
	  val = *n1ptr-- - borrow;
	  if (val < 0)
	    {
	      val += 10;
	      borrow = 1;
	    }
	  else
	    borrow = 0;
	  *diffptr-- = val;
	}
    }

  /* Clean up and return. */
  _rm_leading_zeros (diff);
  return diff;
}


/* Here is the full add routine that takes care of negative numbers.
   N1 is added to N2 and the result placed into RESULT. */

void
bc_add ( n1, n2, result)
     bc_num n1, n2, *result;
{
  bc_num sum;
  int cmp_res;

  if (n1->n_sign == n2->n_sign)
    {
      sum = _do_add (n1, n2);
      sum->n_sign = n1->n_sign;
    }
  else
    {
      /* subtraction must be done. */
      cmp_res = _do_compare (n1, n2, FALSE, FALSE);  /* Compare magnitudes. */
      switch (cmp_res)
	{
	case -1:
	  /* n1 is less than n2, subtract n1 from n2. */
	  sum = _do_sub (n2, n1);
	  sum->n_sign = n2->n_sign;
	  break;
	case  0:
	  /* They are equal! return zero! */
	  sum = copy_num (_zero_);   
	  break;
	case  1:
	  /* n2 is less than n1, subtract n2 from n1. */
	  sum = _do_sub (n1, n2);
	  sum->n_sign = n1->n_sign;
	}
    }

  /* Clean up and return. */
  free_num (result);
  *result = sum;
}


/* Here is the full subtract routine that takes care of negative numbers.
   N2 is subtracted from N1 and the result placed in RESULT. */

void
bc_sub ( n1, n2, result)
     bc_num n1, n2, *result;
{
  bc_num diff;
  int cmp_res;

  if (n1->n_sign != n2->n_sign)
    {
      diff = _do_add (n1, n2);
      diff->n_sign = n1->n_sign;
    }
  else
    {
      /* subtraction must be done. */
      cmp_res = _do_compare (n1, n2, FALSE, FALSE);  /* Compare magnitudes. */
      switch (cmp_res)
	{
	case -1:
	  /* n1 is less than n2, subtract n1 from n2. */
	  diff = _do_sub (n2, n1);
	  diff->n_sign = (n2->n_sign == PLUS ? MINUS : PLUS);
	  break;
	case  0:
	  /* They are equal! return zero! */
	  diff = copy_num (_zero_);   
	  break;
	case  1:
	  /* n2 is less than n1, subtract n2 from n1. */
	  diff = _do_sub (n1, n2);
	  diff->n_sign = n1->n_sign;
	  break;
	}
    }
  
  /* Clean up and return. */
  free_num (result);
  *result = diff;
}


/* The multiply routine.  N2 time N1 is put int PROD with the scale of
   the result being MIN(N2 scale+N1 scale, MAX (SCALE, N2 scale, N1 scale)).
   */

void
bc_multiply (n1, n2, prod, scale)
     bc_num n1, n2, *prod;
     int scale;
{
  bc_num pval;			/* For the working storage. */
  char *n1ptr, *n2ptr, *pvptr;	/* Work pointers. */
  char *n1end, *n2end;		/* To the end of n1 and n2. */

  int indx;
  int len1, len2, total_digits;
  long sum;
  int full_scale, prod_scale;
  int toss;

  /* Initialize things. */
  len1 = n1->n_len + n1->n_scale;
  len2 = n2->n_len + n2->n_scale;
  total_digits = len1 + len2;
  full_scale = n1->n_scale + n2->n_scale;
  prod_scale = MIN(full_scale,MAX(scale,MAX(n1->n_scale,n2->n_scale)));
  toss = full_scale - prod_scale;
  pval =  new_num (total_digits-full_scale, prod_scale);
  pval->n_sign = ( n1->n_sign == n2->n_sign ? PLUS : MINUS );
  n1end = (char *) (n1->n_value + len1 - 1);
  n2end = (char *) (n2->n_value + len2 - 1);
  pvptr = (char *) (pval->n_value + total_digits - toss - 1);
  sum = 0;

  /* Here are the loops... */
  for (indx = 0; indx < toss; indx++)
    {
      n1ptr = (char *) (n1end - MAX(0, indx-len2+1));
      n2ptr = (char *) (n2end - MIN(indx, len2-1));
      while ((n1ptr >= n1->n_value) && (n2ptr <= n2end))
	sum += *n1ptr-- * *n2ptr++;
      sum = sum / 10;
    }
  for ( ; indx < total_digits-1; indx++)
    {
      n1ptr = (char *) (n1end - MAX(0, indx-len2+1));
      n2ptr = (char *) (n2end - MIN(indx, len2-1));
      while ((n1ptr >= n1->n_value) && (n2ptr <= n2end))
	sum += *n1ptr-- * *n2ptr++;
      *pvptr-- = sum % 10;
      sum = sum / 10;
    }
  *pvptr-- = sum;

  /* Assign to prod and clean up the number. */
  free_num (prod);
  *prod = pval;
  _rm_leading_zeros (*prod);
  if (is_zero (*prod)) 
    (*prod)->n_sign = PLUS;
}


/* Some utility routines for the divide:  First a one digit multiply.
   NUM (with SIZE digits) is multiplied by DIGIT and the result is
   placed into RESULT.  It is written so that NUM and RESULT can be
   the same pointers.  */

static void
_one_mult (num, size, digit, result)
     unsigned char *num;
     int size, digit;
     unsigned char *result;
{
  int carry, value;
  unsigned char *nptr, *rptr;

  if (digit == 0)
    memset (result, 0, size);
  else
    {
      if (digit == 1)
	memcpy (result, num, size);
      else
	{
	  /* Initialize */
	  nptr = (unsigned char *) (num+size-1);
	  rptr = (unsigned char *) (result+size-1);
	  carry = 0;

	  while (size-- > 0)
	    {
	      value = *nptr-- * digit + carry;
	      *rptr-- = value % 10;
	      carry = value / 10;
	    }
  
	  if (carry != 0) *rptr = carry;
	}
    }
}


/* The full division routine. This computes N1 / N2.  It returns
   0 if the division is ok and the result is in QUOT.  The number of
   digits after the decimal point is SCALE. It returns -1 if division
   by zero is tried.  The algorithm is found in Knuth Vol 2. p237. */

int
bc_divide (n1, n2, quot, scale)
     bc_num n1, n2, *quot;
     int scale;
{ 
  bc_num qval;
  unsigned char *num1, *num2;
  unsigned char *ptr1, *ptr2, *n2ptr, *qptr;
  int  scale1, val;
  unsigned int  len1, len2, scale2, qdigits, extra, count;
  unsigned int  qdig, qguess, borrow, carry;
  unsigned char *mval;
  char zero;
  unsigned int  norm;

  /* Test for divide by zero. */
  if (is_zero (n2)) return -1;

  /* Test for divide by 1.  If it is we must truncate. */
  if (n2->n_scale == 0)
    {
      if (n2->n_len == 1 && *n2->n_value == 1)
	{
	  qval = new_num (n1->n_len, scale);
	  qval->n_sign = (n1->n_sign == n2->n_sign ? PLUS : MINUS);
	  memset (&qval->n_value[n1->n_len],0,scale);
	  memcpy (qval->n_value, n1->n_value,
		  n1->n_len + MIN(n1->n_scale,scale));
	  free_num (quot);
	  *quot = qval;
	}
    }
  
  /* Set up the divide.  Move the decimal point on n1 by n2's scale.
     Remember, zeros on the end of num2 are wasted effort for dividing. */
  scale2 = n2->n_scale;
  n2ptr = (unsigned char *) n2->n_value+n2->n_len+scale2-1;
  while ((scale2 > 0) && (*n2ptr-- == 0)) scale2--;

  len1 = n1->n_len + scale2;
  scale1 = n1->n_scale - scale2;
  if (scale1 < scale)
    extra = scale - scale1;
  else
    extra = 0;
  num1 = (unsigned char *) malloc (n1->n_len+n1->n_scale+extra+2);
  if (num1 == NULL) out_of_memory();
  memset (num1, 0, n1->n_len+n1->n_scale+extra+2);
  memcpy (num1+1, n1->n_value, n1->n_len+n1->n_scale);

  len2 = n2->n_len + scale2;
  num2 = (unsigned char *) malloc (len2+1);
  if (num2 == NULL) out_of_memory();
  memcpy (num2, n2->n_value, len2);
  *(num2+len2) = 0;
  n2ptr = num2;
  while (*n2ptr == 0)
    {
      n2ptr++;
      len2--;
    }

  /* Calculate the number of quotient digits. */
  if (len2 > len1+scale)
    {
      qdigits = scale+1;
      zero = TRUE;
    }
  else
    {
      zero = FALSE;
      if (len2>len1)
	qdigits = scale+1;  	/* One for the zero integer part. */
      else
	qdigits = len1-len2+scale+1;
    }

  /* Allocate and zero the storage for the quotient. */
  qval = new_num (qdigits-scale,scale);
  memset (qval->n_value, 0, qdigits);

  /* Allocate storage for the temporary storage mval. */
  mval = (unsigned char *) malloc (len2+1);
  if (mval == NULL) out_of_memory ();

  /* Now for the full divide algorithm. */
  if (!zero)
    {
      /* Normalize */
      norm =  10 / ((int)*n2ptr + 1);
      if (norm != 1)
	{
	  _one_mult (num1, len1+scale1+extra+1, norm, num1);
	  _one_mult (n2ptr, len2, norm, n2ptr);
	}

      /* Initialize divide loop. */
      qdig = 0;
      if (len2 > len1)
	qptr = (unsigned char *) qval->n_value+len2-len1;
      else
	qptr = (unsigned char *) qval->n_value;

      /* Loop */
      while (qdig <= len1+scale-len2)
	{
	  /* Calculate the quotient digit guess. */
	  if (*n2ptr == num1[qdig])
	    qguess = 9;
	  else
	    qguess = (num1[qdig]*10 + num1[qdig+1]) / *n2ptr;

	  /* Test qguess. */
	  if (n2ptr[1]*qguess >
	      (num1[qdig]*10 + num1[qdig+1] - *n2ptr*qguess)*10
	       + num1[qdig+2])
	    {
	      qguess--;
	      /* And again. */
	      if (n2ptr[1]*qguess >
		  (num1[qdig]*10 + num1[qdig+1] - *n2ptr*qguess)*10
		  + num1[qdig+2])
		qguess--;
	    }
 
	  /* Multiply and subtract. */
	  borrow = 0;
	  if (qguess != 0)
	    {
	      *mval = 0;
	      _one_mult (n2ptr, len2, qguess, mval+1);
	      ptr1 = (unsigned char *) num1+qdig+len2;
	      ptr2 = (unsigned char *) mval+len2;
	      for (count = 0; count < len2+1; count++)
		{
		  val = (int) *ptr1 - (int) *ptr2-- - borrow;
		  if (val < 0)
		    {
		      val += 10;
		      borrow = 1;
		    }
		  else
		    borrow = 0;
		  *ptr1-- = val;
		}
	    }

	  /* Test for negative result. */
	  if (borrow == 1)
	    {
	      qguess--;
	      ptr1 = (unsigned char *) num1+qdig+len2;
	      ptr2 = (unsigned char *) n2ptr+len2-1;
	      carry = 0;
	      for (count = 0; count < len2; count++)
		{
		  val = (int) *ptr1 + (int) *ptr2-- + carry;
		  if (val > 9)
		    {
		      val -= 10;
		      carry = 1;
		    }
		  else
		    carry = 0;
		  *ptr1-- = val;
		}
	      if (carry == 1) *ptr1 = (*ptr1 + 1) % 10;
	    }
       
	  /* We now know the quotient digit. */
	  *qptr++ =  qguess;
	  qdig++;
	}
    }

  /* Clean up and return the number. */
  qval->n_sign = ( n1->n_sign == n2->n_sign ? PLUS : MINUS );
  if (is_zero (qval)) qval->n_sign = PLUS;
  _rm_leading_zeros (qval);
  free_num (quot);
  *quot = qval;

  /* Clean up temporary storage. */
  free (mval);
  free (num1);
  free (num2);

  return 0;	/* Everything is OK. */
}


/* Modulo for numbers.  This computes NUM1 % NUM2  and puts the
   result in RESULT.   */

int
bc_modulo (num1, num2, result, scale)
     bc_num num1, num2, *result;
     int scale;
{
  bc_num temp;
  int rscale;

  /* Check for correct numbers. */
  if (is_zero (num2)) return -1;

  /* Calculate final scale. */
  rscale = MAX (num1->n_scale, num2->n_scale+scale);
  init_num (&temp);
  
  /* Calculate it. */
  bc_divide (num1, num2, &temp, scale);
  bc_multiply (temp, num2, &temp, rscale);
  bc_sub (num1, temp, result);
  free_num (&temp);

  return 0;	/* Everything is OK. */
}


/* Raise NUM1 to the NUM2 power.  The result is placed in RESULT.
   Maximum exponent is LONG_MAX.  If a NUM2 is not an integer,
   only the integer part is used.  */

void
bc_raise (num1, num2, result, scale)
     bc_num num1, num2, *result;
     int scale;
{
   bc_num temp, power;
   long exponent;
   int rscale;
   char neg;

   /* Check the exponent for scale digits and convert to a long. */
   if (num2->n_scale != 0)
     rt_warn ("non-zero scale in exponent");
   exponent = num2long (num2);
   if (exponent == 0 && (num2->n_len > 1 || num2->n_value[0] != 0))
       rt_error ("exponent too large in raise");

   /* Special case if exponent is a zero. */
   if (exponent == 0)
     {
       free_num (result);
       *result = copy_num (_one_);
       return;
     }

   /* Other initializations. */
   if (exponent < 0)
     {
       neg = TRUE;
       exponent = -exponent;
       rscale = scale;
     }
   else
     {
       neg = FALSE;
       rscale = MIN (num1->n_scale*exponent, MAX(scale, num1->n_scale));
     }
   temp = copy_num (_one_);
   power = copy_num (num1);

   /* Do the calculation. */
   while (exponent != 0)
     {
       if (exponent & 1 != 0) 
	 bc_multiply (temp, power, &temp, rscale);
       bc_multiply (power, power, &power, rscale);
       exponent = exponent >> 1;
     }
   
   /* Assign the value. */
   if (neg)
     {
       bc_divide (_one_, temp, result, rscale);
       free_num (&temp);
     }
   else
     {
       free_num (result);
       *result = temp;
     }
   free_num (&power);
}


/* Take the square root NUM and return it in NUM with SCALE digits
   after the decimal place. */

int 
bc_sqrt (num, scale)
     bc_num *num;
     int scale;
{
  int rscale, cmp_res, done;
  int cscale;
  bc_num guess, guess1, point5;

  /* Initial checks. */
  cmp_res = bc_compare (*num, _zero_);
  if (cmp_res < 0)
    return 0;		/* error */
  else
    {
      if (cmp_res == 0)
	{
	  free_num (num);
	  *num = copy_num (_zero_);
	  return 1;
	}
    }
  cmp_res = bc_compare (*num, _one_);
  if (cmp_res == 0)
    {
      free_num (num);
      *num = copy_num (_one_);
      return 1;
    }

  /* Initialize the variables. */
  rscale = MAX (scale, (*num)->n_scale);
  cscale = rscale + 2;
  init_num (&guess);
  init_num (&guess1);
  point5 = new_num (1,1);
  point5->n_value[1] = 5;
  
  
  /* Calculate the initial guess. */
  if (cmp_res < 0)
    /* The number is between 0 and 1.  Guess should start at 1. */
    guess = copy_num (_one_);
  else
    {
      /* The number is greater than 1.  Guess should start at 10^(exp/2). */
      int2num (&guess,10);
      int2num (&guess1,(*num)->n_len);
      bc_multiply (guess1, point5, &guess1, rscale);
      guess1->n_scale = 0;
      bc_raise (guess, guess1, &guess, rscale);
      free_num (&guess1);
    }
  
  /* Find the square root using Newton's algorithm. */
  done = FALSE;
  while (!done)
    {
      free_num (&guess1);
      guess1 = copy_num (guess);
      bc_divide (*num,guess,&guess,cscale);
      bc_add (guess,guess1,&guess);
      bc_multiply (guess,point5,&guess,cscale);
      cmp_res = _do_compare (guess,guess1,FALSE,TRUE);
      if (cmp_res == 0) done = TRUE;
    }
  
  /* Assign the number and clean up. */
  free_num (num);
  bc_divide (guess,_one_,num,rscale);
  free_num (&guess);
  free_num (&guess1);
  free_num (&point5);
  return 1;
}


/* The following routines provide output for bcd numbers package
   using the rules of POSIX bc for output. */

/* This structure is used for saving digits in the conversion process. */
typedef struct stk_rec {
	long  digit;
	struct stk_rec *next;
} stk_rec;

/* The reference string for digits. */
char ref_str[] = "0123456789ABCDEF";


/* A special output routine for "multi-character digits."  Exactly
   SIZE characters must be output for the value VAL.  If SPACE is
   non-zero, we must output one space before the number.  OUT_CHAR
   is the actual routine for writing the characters. */

void
out_long (val, size, space, out_char)
     long val;
     int size, space;
#ifdef __STDC__
     void (*out_char)(int);
#else
     void (*out_char)();
#endif
{
  char digits[40];
  int len, ix;

  if (space) (*out_char) (' ');
  sprintf (digits, "%ld", val);
  len = strlen (digits);
  while (size > len)
    {
      (*out_char) ('0');
      size--;
    }
  for (ix=0; ix < len; ix++)
    (*out_char) (digits[ix]);
}

/* Output of a bcd number.  NUM is written in base O_BASE using OUT_CHAR
   as the routine to do the actual output of the characters. */

void
out_num (num, o_base, out_char)
     bc_num num;
     int o_base;
#ifdef __STDC__
     void (*out_char)(int);
#else
     void (*out_char)();
#endif
{
  char *nptr;
  int  index, fdigit, pre_space;
  stk_rec *digits, *temp;
  bc_num int_part, frac_part, base, cur_dig, t_num;

  /* The negative sign if needed. */
  if (num->n_sign == MINUS) (*out_char) ('-');

  /* Output the number. */
  if (is_zero (num))
    (*out_char) ('0');
  else
    if (o_base == 10)
      {
	/* The number is in base 10, do it the fast way. */
	nptr = num->n_value;
	if (num->n_len > 1 || *nptr != 0)
	  for (index=num->n_len; index>0; index--)
	    (*out_char) (BCD_CHAR(*nptr++));
	else
	  nptr++;
	
	/* Now the fraction. */
	if (num->n_scale > 0)
	  {
	    (*out_char) ('.');
	    for (index=0; index<num->n_scale; index++)
	      (*out_char) (BCD_CHAR(*nptr++));
	  }
      }
    else
      {
	/* The number is some other base. */
	digits = NULL;
	init_num (&int_part);
	bc_divide (num, _one_, &int_part, 0);
	init_num (&frac_part);
	init_num (&cur_dig);
	init_num (&base);
	bc_sub (num, int_part, &frac_part);
	int2num (&base, o_base);

	/* Get the digits of the integer part and push them on a stack. */
	while (!is_zero (int_part))
	  {
	    bc_modulo (int_part, base, &cur_dig, 0);
	    temp = (stk_rec *) malloc (sizeof(stk_rec));
	    if (temp == NULL) out_of_memory();
	    temp->digit = num2long (cur_dig);
	    temp->next = digits;
	    digits = temp;
	    bc_divide (int_part, base, &int_part, 0);
	  }

	/* Print the digits on the stack. */
	if (digits != NULL)
	  {
	    /* Output the digits. */
	    while (digits != NULL)
	      {
		temp = digits;
		digits = digits->next;
		if (o_base <= 16) 
		  (*out_char) (ref_str[ (int) temp->digit]);
		else
		  out_long (temp->digit, base->n_len, 1, out_char);
		free (temp);
	      }
	  }

	/* Get and print the digits of the fraction part. */
	if (num->n_scale > 0)
	  {
	    (*out_char) ('.');
	    pre_space = 0;
	    t_num = copy_num (_one_);
	    while (t_num->n_len <= num->n_scale) {
	      bc_multiply (frac_part, base, &frac_part, num->n_scale);
	      fdigit = num2long (frac_part);
	      int2num (&int_part, fdigit);
	      bc_sub (frac_part, int_part, &frac_part);
	      if (o_base <= 16)
		(*out_char) (ref_str[fdigit]);
	      else {
		out_long (fdigit, base->n_len, pre_space, out_char);
		pre_space = 1;
	      }
	      bc_multiply (t_num, base, &t_num, 0);
	    }
	  }
    
	/* Clean up. */
	free_num (&int_part);
	free_num (&frac_part);
	free_num (&base);
	free_num (&cur_dig);
      }
}


#if DEBUG > 0

/* Debugging procedures.  Some are just so one can call them from the
   debugger.  */

/* p_n prints the number NUM in base 10. */

void
p_n (num)
     bc_num num;
{
  out_num (num, 10, out_char);
  return 0;
}


/* p_b prints a character array as if it was a string of bcd digits. */
void
p_v (name, num, len)
     char *name;
     unsigned char *num;
     int len;
{
  int i;
  printf ("%s=", name);
  for (i=0; i<len; i++) printf ("%c",BCD_CHAR(num[i]));
  printf ("\n");
}


/* Convert strings to bc numbers.  Base 10 only.*/

void
str2num (num, str, scale)
     bc_num *num;
     char *str;
     int scale;
{
  int digits, strscale;
  char *ptr, *nptr;
  char zero_int;

  /* Prepare num. */
  free_num (num);

  /* Check for valid number and count digits. */
  ptr = str;
  digits = 0;
  strscale = 0;
  zero_int = FALSE;
  if ( (*ptr == '+') || (*ptr == '-'))  ptr++;  /* Sign */
  while (*ptr == '0') ptr++;			/* Skip leading zeros. */
  while (isdigit(*ptr)) ptr++, digits++;	/* digits */
  if (*ptr == '.') ptr++;			/* decimal point */
  while (isdigit(*ptr)) ptr++, strscale++;	/* digits */
  if ((*ptr != '\0') || (digits+strscale == 0))
    {
      *num = copy_num (_zero_);
      return;
    }

  /* Adjust numbers and allocate storage and initialize fields. */
  strscale = MIN(strscale, scale);
  if (digits == 0)
    {
      zero_int = TRUE;
      digits = 1;
    }
  *num = new_num (digits, strscale);

  /* Build the whole number. */
  ptr = str;
  if (*ptr == '-')
    {
      (*num)->n_sign = MINUS;
      ptr++;
    }
  else
    {
      (*num)->n_sign = PLUS;
      if (*ptr == '+') ptr++;
    }
  while (*ptr == '0') ptr++;			/* Skip leading zeros. */
  nptr = (*num)->n_value;
  if (zero_int)
    {
      *nptr++ = 0;
      digits = 0;
    }
  for (;digits > 0; digits--)
    *nptr++ = CH_VAL(*ptr++);

  
  /* Build the fractional part. */
  if (strscale > 0)
    {
      ptr++;  /* skip the decimal point! */
      for (;strscale > 0; strscale--)
	*nptr++ = CH_VAL(*ptr++);
    }
}

/* Convert a numbers to a string.  Base 10 only.*/

char
*num2str (num)
      bc_num num;
{
  char *str, *sptr;
  char *nptr;
  int  index, signch;

  /* Allocate the string memory. */
  signch = ( num->n_sign == PLUS ? 0 : 1 );  /* Number of sign chars. */
  if (num->n_scale > 0)
    str = (char *) malloc (num->n_len + num->n_scale + 2 + signch);
  else
    str = (char *) malloc (num->n_len + 1 + signch);
  if (str == NULL) out_of_memory();

  /* The negative sign if needed. */
  sptr = str;
  if (signch) *sptr++ = '-';

  /* Load the whole number. */
  nptr = num->n_value;
  for (index=num->n_len; index>0; index--)
    *sptr++ = BCD_CHAR(*nptr++);

  /* Now the fraction. */
  if (num->n_scale > 0)
    {
      *sptr++ = '.';
      for (index=0; index<num->n_scale; index++)
	*sptr++ = BCD_CHAR(*nptr++);
    }

  /* Terminate the string and return it! */
  *sptr = '\0';
  return (str);
}
#endif
