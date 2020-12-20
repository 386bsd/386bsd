/* libmath.b for bc for minix.  */

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


scale = 20

/* Uses the fact that e^x = (e^(x/2))^2
   When x is small enough, we use the series:
     e^x = 1 + x + x^2/2! + x^3/3! + ...
*/

define e(x) {
  auto  a, d, e, f, i, m, v, z

  /* Check the sign of x. */
  if (x<0) {
    m = 1
    x = -x
  } 

  /* Precondition x. */
  z = scale;
  scale = 4 + z + .44*x;
  while (x > 1) {
    f += 1;
    x /= 2;
  }

  /* Initialize the variables. */
  v = 1+x
  a = x
  d = 1

  for (i=2; 1; i++) {
    e = (a *= x) / (d *= i)
    if (e == 0) {
      if (f>0) while (f--)  v = v*v;
      scale = z
      if (m) return (1/v);
      return (v/1);
    }
    v += e
  }
}

/* Natural log. Uses the fact that ln(x^2) = 2*ln(x)
    The series used is:
       ln(x) = 2(a+a^3/3+a^5/5+...) where a=(x-1)/(x+1)
*/

define l(x) {
  auto e, f, i, m, n, v, z

  /* return something for the special case. */
  if (x <= 0) return (1 - 10^scale)

  /* Precondition x to make .5 < x < 2.0. */
  z = scale;
  scale += 4;
  f = 2;
  i=0
  while (x >= 2) {  /* for large numbers */
    f *= 2;
    x = sqrt(x);
  }
  while (x <= .5) {  /* for small numbers */
    f *= 2;
    x = sqrt(x);
  }

  /* Set up the loop. */
  v = n = (x-1)/(x+1)
  m = n*n

  /* Sum the series. */
  for (i=3; 1; i+=2) {
    e = (n *= m) / i
    if (e == 0) {
      v = f*v
      scale = z
      return (v/1)
    }
    v += e
  }
}

/* Sin(x)  uses the standard series:
   sin(x) = x - x^3/3! + x^5/5! - x^7/7! ... */

define s(x) {
  auto  e, i, m, n, s, v, z

  /* precondition x. */
  z = scale 
  scale = 1.1*z + 1;
  v = a(1)
  if (x < 0) {
    m = 1;
    x = -x;
  }
  scale = 0
  n = (x / v + 2 )/4
  x = x - 4*n*v
  if (n%2) x = -x

  /* Do the loop. */
  scale = z + 2;
  v = e = x
  s = -x*x
  for (i=3; 1; i+=2) {
    e *= s/(i*(i-1))
    if (e == 0) {
      scale = z
      if (m) return (-v/1);
      return (v/1);
    }
    v += e
  }
}

/* Cosine : cos(x) = sin(x+pi/2) */
define c(x) {
  auto v;
  scale += 1;
  v = s(x+a(1)*2);
  scale -= 1;
  return (v/1);
}

/* Arctan: Using the formula:
     atan(x) = atan(c) + atan((x-c)/(1+xc)) for a small c (.2 here)
   For under .2, use the series:
     atan(x) = x - x^3/3 + x^5/5 - x^7/7 + ...   */

define a(x) {
  auto a, e, f, i, m, n, s, v, z

  /* Special case and for fast answers */
  if (x==1) {
    if (scale <= 25) return (.7853981633974483096156608/1)
    if (scale <= 40) return (.7853981633974483096156608458198757210492/1)
    if (scale <= 60) \
      return (.785398163397448309615660845819875721049292349843776455243736/1)
  }
  if (x==.2) {
    if (scale <= 25) return (.1973955598498807583700497/1)
    if (scale <= 40) return (.1973955598498807583700497651947902934475/1)
    if (scale <= 60) \
      return (.197395559849880758370049765194790293447585103787852101517688/1)
  }

  /* Negative x? */
  if (x<0) {
    m = 1;
    x = -x;
  }

  /* Save the scale. */
  z = scale;

  /* Note: a and f are known to be zero due to being auto vars. */
  /* Calculate atan of a known number. */ 
  if (x > .2)  {
    scale = z+4;
    a = a(.2);
  }
   
  /* Precondition x. */
  scale = z+2;
  while (x > .2) {
    f += 1;
    x = (x-.2) / (1+x*.2);
  }

  /* Initialize the series. */
  v = n = x;
  s = -x*x;

  /* Calculate the series. */
  for (i=3; 1; i+=2) {
    e = (n *= s) / i;
    if (e == 0) {
      scale = z;
      if (m) return ((f*a+v)/-1);
      return ((f*a+v)/1);
    }
    v += e
  }
}


/* Bessel function of integer order.  Uses the following:
   j(-n,x) = (-1)^n*j(n,x) 
   j(n,x) = x^n/(2^n*n!) * (1 - x^2/(2^2*1!*(n+1)) + x^4/(2^4*2!*(n+1)*(n+2))
            - x^6/(2^6*3!*(n+1)*(n+2)*(n+3)) .... )
*/
define j(n,x) {
  auto a, d, e, f, i, m, s, v, z

  /* Make n an integer and check for negative n. */
  z = scale;
  scale = 0;
  n = n/1;
  if (n<0) {
    n = -n;
    if (n%2 == 1) m = 1;
  }

  /* Compute the factor of x^n/(2^n*n!) */
  f = 1;
  for (i=2; i<=n; i++) f = f*i;
  scale = 1.5*z;
  f = x^n / 2^n / f;

  /* Initialize the loop .*/
  v = e = 1;
  s = -x*x/4
  scale = 1.5*z

  /* The Loop.... */
  for (i=1; 1; i++) {
    e =  e * s / i / (n+i);
    if (e == 0) {
       scale = z
       if (m) return (-f*v/1);
       return (f*v/1);
    }
    v += e;
  }
}
