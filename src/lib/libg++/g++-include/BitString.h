// This may look like C code, but it is really -*- C++ -*-
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

#ifndef _BitString_h
#ifdef __GNUG__
#pragma once
#pragma interface
#endif

#define _BitString_h 1

#include <stream.h>
#include <values.h>

#define BITSTRBITS   BITS(short)

struct BitStrRep
{
  unsigned int    len;          // length in bits
  unsigned short  sz;           // allocated slots
  unsigned short  s[1];         // bits start here
};

extern BitStrRep*  BStr_alloc(BitStrRep*, const unsigned short*, int, int,int);
extern BitStrRep*  BStr_resize(BitStrRep*, int);
extern BitStrRep*  BStr_copy(BitStrRep*, const BitStrRep*);
extern BitStrRep*  cmpl(const BitStrRep*, BitStrRep*);
extern BitStrRep*  and(const BitStrRep*, const BitStrRep*, BitStrRep*);
extern BitStrRep*  or(const BitStrRep*, const BitStrRep*, BitStrRep*);
extern BitStrRep*  xor(const BitStrRep*, const BitStrRep*, BitStrRep*);
extern BitStrRep*  diff(const BitStrRep*, const BitStrRep*, BitStrRep*);
extern BitStrRep*  cat(const BitStrRep*, const BitStrRep*, BitStrRep*);
extern BitStrRep*  cat(const BitStrRep*, unsigned int, BitStrRep*);
extern BitStrRep*  lshift(const BitStrRep*, int, BitStrRep*);


class BitString;
class BitPattern;

class BitStrBit
{
protected:
  BitString&        src;
  unsigned int      pos;

 public:
                    BitStrBit(BitString& v, int p);
                    BitStrBit(const BitStrBit& b);
                   ~BitStrBit();
                    operator unsigned int() const;
  int               operator =  (unsigned int b);
  int               operator == (unsigned int b) const ;
  int               operator != (unsigned int b) const ;
};

class BitSubString
{
  friend class      BitString;
  friend class      BitPattern;

protected:

  BitString&        S;
  int               pos;
  int               len;

                    BitSubString(BitString& x, int p, int l);
                    BitSubString(const BitSubString& x);
public:
                    ~BitSubString();

  void              operator =  (const BitString&);
  void              operator =  (const BitSubString&);

  int               length() const;
  int               empty() const;

  int               OK() const;
};

class BitString
{
  friend class       BitSubString;
  friend class       BitPattern;
protected:
  BitStrRep*         rep;

  int                search(int, int, const unsigned short*, int, int) const;
  int                match(int, int, int, const unsigned short*,int,int) const;
  BitSubString       _substr(int first, int l);

public:

// constructors
                     BitString();
                     BitString(const BitString&);
                     BitString(const BitSubString& y);

                    ~BitString();

  void               operator =  (unsigned int bit);
  void               operator =  (const BitString& y);
  void               operator =  (const BitSubString& y);

// equality & subset tests

  friend int         operator == (const BitString&, const BitString&);
  friend int         operator != (const BitString&, const BitString&);
  friend int         operator <  (const BitString&, const BitString&);
  friend int         operator <= (const BitString&, const BitString&);
  friend int         operator >  (const BitString&, const BitString&);
  friend int         operator >= (const BitString&, const BitString&);

// procedural versions of operators


  friend void        and(const BitString&, const BitString&, BitString&);
  friend void        or(const BitString&, const BitString&, BitString&);
  friend void        xor(const BitString&, const BitString&, BitString&);
  friend void        diff(const BitString&, const BitString&, BitString&);
  friend void        cat(const BitString&, const BitString&, BitString&);
  friend void        cat(const BitString&, unsigned int, BitString&);
  friend void        lshift(const BitString&, int, BitString&);
  friend void        rshift(const BitString&, int, BitString&);

  friend void        complement(const BitString&, BitString&);

  friend int         lcompare(const BitString&, const BitString&); 

// assignment-based operators
// (constuctive versions decalred inline below

  void               operator |= (const BitString&);
  void               operator &= (const BitString&);
  void               operator -= (const BitString&);
  void               operator ^= (const BitString&);
  void               operator += (const BitString&);
  void               operator += (unsigned int b);
  void               operator <<=(int s);
  void               operator >>=(int s);

  void               complement();

// individual bit manipulation

  void               set(int pos);
  void               set(int from, int to);
  void               set();

  void               clear(int pos);
  void               clear(int from, int to);
  void               clear(); 

  void               invert(int pos);
  void               invert(int from, int to);

  int                test(int pos) const;
  int                test(int from, int to) const;

  void               assign(int p, unsigned int bit);

// indexing

  BitStrBit          operator [] (int pos);

// iterators

  int                first(unsigned int bit = 1) const;
  int                last(unsigned int b = 1) const;

  int                next(int pos, unsigned int b = 1) const;
  int                previous(int pos, unsigned int b = 1) const;

// searching & matching

  int                index(unsigned int bit, int startpos = 0) const ;      
  int                index(const BitString&, int startpos = 0) const;
  int                index(const BitSubString&, int startpos = 0) const;
  int                index(const BitPattern&, int startpos = 0) const;

  int                contains(const BitString&) const;
  int                contains(const BitSubString&) const;
  int                contains(const BitPattern&) const;

  int                contains(const BitString&, int pos) const;
  int                contains(const BitSubString&, int pos) const;
  int                contains(const BitPattern&, int pos) const;

  int                matches(const BitString&, int pos = 0) const;
  int                matches(const BitSubString&, int pos = 0) const;
  int                matches(const BitPattern&, int pos = 0) const;

// BitSubString extraction

  BitSubString       at(int pos, int len);
  BitSubString       at(const BitString&, int startpos = 0); 
  BitSubString       at(const BitSubString&, int startpos = 0); 
  BitSubString       at(const BitPattern&, int startpos = 0); 

  BitSubString       before(int pos);
  BitSubString       before(const BitString&, int startpos = 0);
  BitSubString       before(const BitSubString&, int startpos = 0);
  BitSubString       before(const BitPattern&, int startpos = 0);

  BitSubString       after(int pos);
  BitSubString       after(const BitString&, int startpos = 0);
  BitSubString       after(const BitSubString&, int startpos = 0);
  BitSubString       after(const BitPattern&, int startpos = 0);

// other friends & utilities

  friend BitString   common_prefix(const BitString&, const BitString&, 
                                   int pos = 0);
  friend BitString   common_suffix(const BitString&, const BitString&, 
                                   int pos = -1);
  friend BitString   reverse(const BitString&);

  void               right_trim(unsigned int bit);
  void               left_trim(unsigned int bit);

// status

  int                empty() const ;
  int                count(unsigned int bit = 1) const;
  int                length() const;

// convertors & IO

  friend BitString   atoBitString(const char* s, char f='0', char t='1');
  friend const char* BitStringtoa(const BitString&, char f='0', char t='1');

  friend BitString   shorttoBitString(unsigned short);
  friend BitString   longtoBitString(unsigned long);

  friend ostream&    operator << (ostream& s, const BitString&);

// misc

  volatile void      error(const char* msg) const;

// indirect friends

  friend const char* BitPatterntoa(const BitPattern& p, 
                                  char f='0',char t='1',char x='X');
  friend BitPattern  atoBitPattern(const char* s,
                                  char f='0',char t='1',char x='X');

  int                OK() const;
};


class BitPattern
{
public:
  BitString          pattern;
  BitString          mask;

                     BitPattern();
                     BitPattern(const BitPattern&);
                     BitPattern(const BitString& p, const BitString& m);

                    ~BitPattern();

  friend const char* BitPatterntoa(const BitPattern&, char f, char t, char x);
  friend BitPattern atoBitPattern(const char* s, char f,char t, char x);
  friend ostream&   operator << (ostream& s, const BitPattern&);

  int               search(const unsigned short*, int, int) const;
  int               match(const unsigned short* xs, int, int, int) const;

  int               OK() const;
};

BitString  operator & (const BitString& x, const BitString& y);
BitString  operator | (const BitString& x, const BitString& y);
BitString  operator ^ (const BitString& x, const BitString& y);
BitString  operator << (const BitString& x, int y);
BitString  operator >> (const BitString& x, int y);
BitString  operator - (const BitString& x, const BitString& y);
BitString  operator + (const BitString& x, const BitString& y);
BitString  operator + (const BitString& x, unsigned int y);
BitString  operator ~ (const BitString& x);
int operator != (const BitString& x, const BitString& y);
int operator>(const BitString& x, const BitString& y);
int operator>=(const BitString& x, const BitString& y);

extern BitStrRep    _nilBitStrRep;
extern BitString    _nil_BitString;

// primitive bit extraction

// These must be inlined regardless of optimization.

inline int BitStr_index(int l) { return (unsigned)(l) / BITSTRBITS; }

inline int BitStr_pos(int l) { return l & (BITSTRBITS - 1); }

#if defined(__OPTIMIZE__) || defined(USE_LIBGXX_INLINES)

// constructors & assignment

inline BitString::BitString() :rep(&_nilBitStrRep) {}

inline BitString::BitString(const BitString& x) :rep(BStr_copy(0, x.rep)) {}

inline BitString::BitString(const BitSubString& y) 
   :rep (BStr_alloc(0, y.S.rep->s, y.pos, y.pos+y.len, y.len)) {}

inline BitString::~BitString()
{ 
  if (rep != &_nilBitStrRep) delete rep;
}

#if defined(__GNUG__) && !defined(NO_NRV)

inline BitString shorttoBitString(unsigned short w) return r
{ 
  r.rep = BStr_alloc(0, &w, 0, BITSTRBITS, BITSTRBITS);
}

inline BitString longtoBitString(unsigned long w) return r
{ 
  unsigned short u[2];
  u[0] = w & ((unsigned short)(~(0)));
  u[1] = w >> BITSTRBITS;
  r.rep = BStr_alloc(0, &u[0], 0, 2*BITSTRBITS, 2*BITSTRBITS);
}

#else

inline BitString shorttoBitString(unsigned short w) 
{ 
  BitString r; r.rep = BStr_alloc(0, &w, 0, BITSTRBITS, BITSTRBITS); return r;
}

inline BitString longtoBitString(unsigned long w) 
{ 
  BitString r;
  unsigned short u[2];
  u[0] = w & ((unsigned short)(~(0)));
  u[1] = w >> BITSTRBITS;
  r.rep = BStr_alloc(0, &u[0], 0, 2*BITSTRBITS, 2*BITSTRBITS);
  return r;
}

#endif

inline void BitString::operator =  (const BitString& y)
{ 
  rep = BStr_copy(rep, y.rep);
}

inline void BitString::operator = (unsigned int b)
{ 
  unsigned short bit = b;
  rep = BStr_alloc(rep, &bit, 0, 1, 1);
}

inline void BitString::operator=(const BitSubString&  y)
{
  rep = BStr_alloc(rep, y.S.rep->s, y.pos, y.pos+y.len, y.len);
}

inline BitSubString::BitSubString(const BitSubString& x) 
    :S(x.S), pos(x.pos), len(x.len) {}

inline BitSubString::BitSubString(BitString& x, int p, int l)
     : S(x), pos(p), len(l) {}

inline BitSubString::~BitSubString() {}

inline BitPattern::BitPattern(const BitString& p, const BitString& m)
    :pattern(p), mask(m) {}

inline BitPattern::BitPattern(const BitPattern& b)
    :pattern(b.pattern), mask(b.mask) {}

inline BitPattern::BitPattern() {}
inline BitPattern::~BitPattern() {}


// procedural versions of operators

inline void and(const BitString& x, const BitString& y, BitString& r)
{
  r.rep = and(x.rep, y.rep, r.rep);
}

inline void or(const BitString& x, const BitString& y, BitString& r)
{
  r.rep = or(x.rep, y.rep, r.rep);
}

inline void xor(const BitString& x, const BitString& y, BitString& r)
{
  r.rep = xor(x.rep, y.rep, r.rep);
}

inline void diff(const BitString& x, const BitString& y, BitString& r)
{
  r.rep = diff(x.rep, y.rep, r.rep);
}

inline void cat(const BitString& x, const BitString& y, BitString& r)
{
  r.rep = cat(x.rep, y.rep, r.rep);
}

inline void cat(const BitString& x, unsigned int y, BitString& r)
{
  r.rep = cat(x.rep, y, r.rep);
}

inline void rshift(const BitString& x, int y, BitString& r)
{
  r.rep = lshift(x.rep, -y, r.rep);
}

inline void lshift(const BitString& x, int y, BitString& r)
{
  r.rep = lshift(x.rep, y, r.rep);
}

inline void complement(const BitString& x, BitString& r)
{
  r.rep = cmpl(x.rep, r.rep);
}

// operators


inline void BitString::operator &= (const BitString& y)
{
  and(*this, y, *this);
}


inline void BitString::operator |= (const BitString& y)
{
  or(*this, y, *this);
}

inline void BitString::operator ^= (const BitString& y)
{
  xor(*this, y, *this);
}

inline void BitString::operator <<= (int y)
{
  lshift(*this, y, *this);
}

inline void BitString::operator >>= (int y)
{
  rshift(*this, y, *this);
}

inline void BitString::operator -= (const BitString& y)
{
  diff(*this, y, *this);
}

inline void BitString::operator += (const BitString& y)
{
  cat(*this, y, *this);
}

inline void BitString::operator += (unsigned int y)
{
  cat(*this, y, *this);
}

inline void BitString::complement()
{
  ::complement(*this, *this);
}

#if defined(__GNUG__) && !defined(NO_NRV)

inline BitString  operator & (const BitString& x, const BitString& y) return r
{
  and(x, y, r);
}

inline BitString  operator | (const BitString& x, const BitString& y) return r
{
  or(x, y, r);
}

inline BitString  operator ^ (const BitString& x, const BitString& y) return r
{
  xor(x, y, r);
}

inline BitString  operator << (const BitString& x, int y) return r
{
  lshift(x, y, r);
}

inline BitString  operator >> (const BitString& x, int y) return r
{
  rshift(x, y, r);
}

inline BitString  operator - (const BitString& x, const BitString& y) return r
{
  diff(x, y, r);
}

inline BitString  operator + (const BitString& x, const BitString& y) return r
{
  cat(x, y, r);
}

inline BitString  operator + (const BitString& x, unsigned int y) return r
{
  cat(x, y, r);
}

inline BitString  operator ~ (const BitString& x) return r
{
  complement(x, r);
}

#else /* NO_NRV */

inline BitString  operator & (const BitString& x, const BitString& y) 
{
  BitString r; and(x, y, r); return r;
}

inline BitString  operator | (const BitString& x, const BitString& y) 
{
  BitString r; or(x, y, r); return r;
}

inline BitString  operator ^ (const BitString& x, const BitString& y) 
{
  BitString r; xor(x, y, r); return r;
}

inline BitString  operator << (const BitString& x, int y) 
{
  BitString r; lshift(x, y, r); return r;
}

inline BitString  operator >> (const BitString& x, int y) 
{
  BitString r; rshift(x, y, r); return r;
}

inline BitString  operator - (const BitString& x, const BitString& y) 
{
  BitString r; diff(x, y, r); return r;
}

inline BitString  operator + (const BitString& x, const BitString& y) 
{
  BitString r; cat(x, y, r); return r;
}

inline BitString  operator + (const BitString& x, unsigned int y) 
{
  BitString r; cat(x, y, r); return r;
}

inline BitString  operator ~ (const BitString& x) 
{
  BitString r; complement(x, r); return r;
}

#endif

// status, matching

inline int BitString::length() const
{ 
  return rep->len;
}

inline int BitString::empty() const
{ 
  return rep->len == 0;
}

inline int BitString::index(const BitString& y, int startpos) const
{   
  return search(startpos, rep->len, y.rep->s, 0, y.rep->len);
}

inline int BitString::index(const BitSubString& y, int startpos) const
{   
  return search(startpos, rep->len, y.S.rep->s, y.pos, y.pos+y.len);
}

inline int BitString::contains(const BitString& y) const
{   
  return search(0, rep->len, y.rep->s, 0, y.rep->len) >= 0;
}

inline int BitString::contains(const BitSubString& y) const
{   
  return search(0, rep->len, y.S.rep->s, y.pos, y.pos+y.len) >= 0;
}

inline int BitString::contains(const BitString& y, int p) const
{
  return match(p, rep->len, 0, y.rep->s, 0, y.rep->len);
}

inline int BitString::matches(const BitString& y, int p) const
{
  return match(p, rep->len, 1, y.rep->s, 0, y.rep->len);
}

inline int BitString::contains(const BitSubString& y, int p) const
{
  return match(p, rep->len, 0, y.S.rep->s, y.pos, y.pos+y.len);
}

inline int BitString::matches(const BitSubString& y, int p) const
{
  return match(p, rep->len, 1, y.S.rep->s, y.pos, y.pos+y.len);
}

inline int BitString::contains(const BitPattern& r) const
{
  return r.search(rep->s, 0, rep->len) >= 0;
}

inline int BitString::contains(const BitPattern& r, int p) const
{
  return r.match(rep->s, p, rep->len, 0);
}

inline int BitString::matches(const BitPattern& r, int p) const
{
  return r.match(rep->s, p, rep->len, 1);
}

inline int BitString::index(const BitPattern& r, int startpos) const
{
  return r.search(rep->s, startpos, rep->len);
}

inline  int BitSubString::length() const
{ 
  return len;
}

inline  int BitSubString::empty() const
{ 
  return len == 0;
}

inline int operator != (const BitString& x, const BitString& y)
{
  return !(x == y);
}

inline int operator>(const BitString& x, const BitString& y)
{
  return y < x;
}

inline int operator>=(const BitString& x, const BitString& y)
{
  return y <= x;
}

inline int BitString::first(unsigned int b) const
{
  return next(-1, b);
}

inline int BitString::last(unsigned int b) const
{
  return previous(rep->len, b);
}

inline int BitString::index(unsigned int bit, int startpos) const
{
  if (startpos >= 0)
    return next(startpos - 1, bit);
  else
    return previous(rep->len + startpos + 1, bit);
}

inline void BitString::right_trim(unsigned int b) 
{
  int nb = (b == 0)? 1 : 0;
  rep = BStr_resize(rep, previous(rep->len, nb) + 1);
}

inline void BitString::left_trim(unsigned int b)
{
  int nb = (b == 0)? 1 : 0;
  int p = next(-1, nb);
  rep = BStr_alloc(rep, rep->s, p, rep->len, rep->len - p);
}

inline int BitString::test(int i) const
{
  return ((unsigned)(i) >= rep->len)? 0 : 
         ((rep->s[BitStr_index(i)] & (1 << (BitStr_pos(i)))) != 0);
}


// subscripting

inline BitStrBit::BitStrBit(const BitStrBit& b) :src(b.src), pos(b.pos) {}

inline BitStrBit::BitStrBit(BitString& v, int p) :src(v), pos(p) {}

inline BitStrBit::~BitStrBit() {}

inline BitStrBit::operator unsigned int() const
{
  return src.test(pos);
}

inline int BitStrBit::operator = (unsigned int b)
{
  src.assign(pos, b); return b;
}

inline int BitStrBit::operator == (unsigned int b) const
{
  return src.test(pos) == b;
}

inline int BitStrBit::operator != (unsigned int b) const
{
  return src.test(pos) != b;
}

inline BitStrBit BitString::operator [] (int i)
{
  if ((unsigned)(i) >= rep->len) error("illegal bit index");
  return BitStrBit(*this, i);
}

inline BitSubString BitString::_substr(int first, int l)
{
  if (first < 0 || l <= 0 || (unsigned)(first + l) > rep->len)
    return BitSubString(_nil_BitString, 0, 0) ;
  else 
    return BitSubString(*this, first, l);
}

#endif

#endif
