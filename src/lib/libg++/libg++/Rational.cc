/* 
Copyright (C) 1988 Free Software Foundation
    written by Doug Lea (dl@rocky.oswego.edu)

This file is part of GNU CC.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY.  No author or distributor
accepts responsibility to anyone for the consequences of using it
or for whether it serves any particular purpose or works at all,
unless he says so in writing.  Refer to the GNU CC General Public
License for full details.

Everyone is granted permission to copy, modify and redistribute
GNU CC, but only under the conditions described in the
GNU CC General Public License.   A copy of this license is
supposed to have been given to you along with GNU CC so you
can know your rights and responsibilities.  It should be in a
file named COPYING.  Among other things, the copyright notice
and this notice must be preserved on all copies.  
*/

#ifdef __GNUG__
#pragma implementation
#endif
#include <Rational.h>
#include <std.h>
#include <math.h>
#include <values.h>



volatile void Rational::error(const char* msg) const
{
  (*lib_error_handler)("Rational", msg);
}

static const Integer _Int_One(1);

void Rational::normalize()
{
  int s = sign(den);
  if (s == 0)
    error("Zero denominator.");
  else if (s < 0)
  {
    den.negate();
    num.negate();
  }

  Integer g = gcd(num, den);
  if (ucompare(g, _Int_One) != 0)
  {
    num /= g;
    den /= g;
  }
}

void      add(const Rational& x, const Rational& y, Rational& r)
{
  if (&r != &x && &r != &y)
  {
    mul(x.num, y.den, r.num);
    mul(x.den, y.num, r.den);
    add(r.num, r.den, r.num);
    mul(x.den, y.den, r.den);
  }
  else
  {
    Integer tmp;
    mul(x.den, y.num, tmp);
    mul(x.num, y.den, r.num);
    add(r.num, tmp, r.num);
    mul(x.den, y.den, r.den);
  }
  r.normalize();
}

void      sub(const Rational& x, const Rational& y, Rational& r)
{
  if (&r != &x && &r != &y)
  {
    mul(x.num, y.den, r.num);
    mul(x.den, y.num, r.den);
    sub(r.num, r.den, r.num);
    mul(x.den, y.den, r.den);
  }
  else
  {
    Integer tmp;
    mul(x.den, y.num, tmp);
    mul(x.num, y.den, r.num);
    sub(r.num, tmp, r.num);
    mul(x.den, y.den, r.den);
  }
  r.normalize();
}

void      mul(const Rational& x, const Rational& y, Rational& r)
{
  mul(x.num, y.num, r.num);
  mul(x.den, y.den, r.den);
  r.normalize();
}

void      div(const Rational& x, const Rational& y, Rational& r)
{
  if (&r != &x && &r != &y)
  {
    mul(x.num, y.den, r.num);
    mul(x.den, y.num, r.den);
  }
  else
  {
    Integer tmp;
    mul(x.num, y.den, tmp);
    mul(y.num, x.den, r.den);
    r.num = tmp;
  }
  r.normalize();
}




void Rational::invert()
{
  Integer tmp = num;  
  num = den;  
  den = tmp;  
  int s = sign(den);
  if (s == 0)
    error("Zero denominator.");
  else if (s < 0)
  {
    den.negate();
    num.negate();
  }
}

int compare(const Rational& x, const Rational& y)
{
  int xsgn = sign(x.num);
  int ysgn = sign(y.num);
  int d = xsgn - ysgn;
  if (d == 0 && xsgn != 0) d = compare(x.num * y.den, x.den * y.num);
  return d;
}

Rational::Rational(double x)
{
  num = 0;
  den = 1;
  if (x != 0.0)
  {
    int neg = x < 0;
    if (neg)
      x = -x;

    const long shift = 15;         // a safe shift per step
    const double width = 32768.0;  // = 2^shift
    const int maxiter = 20;        // ought not be necessary, but just in case,
                                   // max 300 bits of precision
    int expt;
    double mantissa = frexp(x, &expt);
    long exponent = expt;
    double intpart;
    int k = 0;
    while (mantissa != 0.0 && k++ < maxiter)
    {
      mantissa *= width;
      mantissa = modf(mantissa, &intpart);
      num <<= shift;
      num += (long)intpart;
      exponent -= shift;
    }
    if (exponent > 0)
      num <<= exponent;
    else if (exponent < 0)
      den <<= -exponent;
    if (neg)
      num.negate();
  }
  normalize();
}


Integer trunc(const Rational& x)
{
  return x.num / x.den ;
}


Rational pow(const Rational& x, const Integer& y)
{
  long yy = long(y);
  return pow(x, yy);
}               

#if defined(__GNUG__) && !defined(NO_NRV)

Rational operator - (const Rational& x) return r(x)
{
  r.negate();
}

Rational abs(const Rational& x) return r(x)
{
  if (sign(r.num) < 0) r.negate();
}


Rational sqr(const Rational& x) return r
{
  mul(x.num, x.num, r.num);
  mul(x.den, x.den, r.den);
  r.normalize();
}

Integer floor(const Rational& x) return q
{
  Integer r;
  divide(x.num, x.den, q, r);
  if (sign(x.num) < 0 && sign(r) != 0) q--;
}

Integer ceil(const Rational& x) return q
{
  Integer  r;
  divide(x.num, x.den, q, r);
  if (sign(x.num) >= 0 && sign(r) != 0) q++;
}

Integer round(const Rational& x) return q
{
  Integer r;
  divide(x.num, x.den, q, r);
  r <<= 1;
  if (ucompare(r, x.den) >= 0)
  {
    if (sign(x.num) >= 0)
      q++;
    else
      q--;
  }
}

// power: no need to normalize since num & den already relatively prime

Rational pow(const Rational& x, long y) return r
{
  if (y >= 0)
  {
    pow(x.num, y, r.num);
    pow(x.den, y, r.den);
  }
  else
  {
    y = -y;
    pow(x.num, y, r.den);
    pow(x.den, y, r.num);
    if (sign(r.den) < 0)
    {
      r.num.negate();
      r.den.negate();
    }
  }
}

#else

Rational operator - (const Rational& x) 
{
  Rational r(x); r.negate(); return r;
}

Rational abs(const Rational& x) 
{
  Rational r(x);
  if (sign(r.num) < 0) r.negate();
  return r;
}


Rational sqr(const Rational& x)
{
  Rational r;
  mul(x.num, x.num, r.num);
  mul(x.den, x.den, r.den);
  r.normalize();
  return r;
}

Integer floor(const Rational& x)
{
  Integer q;
  Integer r;
  divide(x.num, x.den, q, r);
  if (sign(x.num) < 0 && sign(r) != 0) q--;
  return q;
}

Integer ceil(const Rational& x)
{
  Integer q;
  Integer  r;
  divide(x.num, x.den, q, r);
  if (sign(x.num) >= 0 && sign(r) != 0) q++;
  return q;
}

Integer round(const Rational& x) 
{
  Integer q;
  Integer r;
  divide(x.num, x.den, q, r);
  r <<= 1;
  if (ucompare(r, x.den) >= 0)
  {
    if (sign(x.num) >= 0)
      q++;
    else
      q--;
  }
  return q;
}

Rational pow(const Rational& x, long y)
{
  Rational r;
  if (y >= 0)
  {
    pow(x.num, y, r.num);
    pow(x.den, y, r.den);
  }
  else
  {
    y = -y;
    pow(x.num, y, r.den);
    pow(x.den, y, r.num);
    if (sign(r.den) < 0)
    {
      r.num.negate();
      r.den.negate();
    }
  }
  return r;
}

#endif

ostream& operator << (ostream& s, const Rational& y)
{
  if (y.den == 1)
    s << Itoa(y.num);
  else
  {
    s << Itoa(y.num);
    s << "/";
    s << Itoa(y.den);
  }
  return s;
}

istream& operator >> (istream& s, Rational& y)
{
  s >> y.num;
  if (s)
  {
    char ch = 0;
    s.get(ch);
    if (ch == '/')
    {
      s >> y.den;
      y.normalize();
    }
    else
    {
      s.unget(ch);
      y.den = 1;
    }
  }
  return s;
}

int Rational::OK() const
{
  int v = num.OK() && den.OK(); // have valid num and denom
  v &= sign(den) > 0;           // denominator positive;
  v &=  ucompare(gcd(num, den), _Int_One) == 0; // relatively prime
  if (!v) error("invariant failure");
  return v;
}
