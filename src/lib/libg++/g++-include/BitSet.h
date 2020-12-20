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

#ifndef _BitSet_h
#ifdef __GNUG__
#pragma once
#pragma interface
#endif

#define _BitSet_h 1

#include <stream.h>
#include <values.h>

#define BITSETBITS  BITS(short)

struct BitSetRep
{
  unsigned short  len;          // number of shorts in s
  unsigned short  sz;           // allocated slots
  unsigned short  virt;         // virtual 0 or 1
  unsigned short  s[1];         // bits start here
};

extern BitSetRep*   BitSetalloc(BitSetRep*, const unsigned short*, 
                                int, int, int);
extern BitSetRep*   BitSetcopy(BitSetRep*, const BitSetRep*);
extern BitSetRep*   BitSetresize(BitSetRep*, int);
extern BitSetRep*   BitSetop(const BitSetRep*, const BitSetRep*, 
                             BitSetRep*, char);
extern BitSetRep*   BitSetcmpl(const BitSetRep*, BitSetRep*);


extern BitSetRep    _nilBitSetRep;

class BitSet;

class BitSetBit
{
protected:
  BitSet*            src;
  unsigned long      pos;

 public:
                     BitSetBit(BitSet* v, int p);
                     BitSetBit(const BitSetBit& b);
                    ~BitSetBit();
                     operator int();
  int                operator = (int b);
  int                operator == (int b);
  int                operator != (int b);
};

class BitSet
{
protected:
  BitSetRep*          rep;

  
public:

// constructors
                     BitSet();
                     BitSet(const BitSet&);

                    ~BitSet();

  void               operator =  (const BitSet& y);

// equality & subset tests

  friend int         operator == (const BitSet& x, const BitSet& y);
  friend int         operator != (const BitSet& x, const BitSet& y);
  friend int         operator <  (const BitSet& x, const BitSet& y);
  friend int         operator <= (const BitSet& x, const BitSet& y);
  friend int         operator >  (const BitSet& x, const BitSet& y);
  friend int         operator >= (const BitSet& x, const BitSet& y);


// operations on self

  void               operator |= (const BitSet& y);
  void               operator &= (const BitSet& y);
  void               operator -= (const BitSet& y);
  void               operator ^= (const BitSet& y);

  void               complement();

// individual bit manipulation

  void               set(int pos);
  void               set(int from, int to);
  void               set(); // set all

  void               clear(int pos);
  void               clear(int from, int to);
  void               clear(); // clear all

  void               invert(int pos);
  void               invert(int from, int to);

  int                test(int pos) const;
  int                test(int from, int to) const;

  BitSetBit          operator [] (int i);
  
// iterators

  int                first(int b = 1) const;
  int                last(int b = 1) const;

  int                next(int pos, int b = 1) const;
  int                previous(int pos, int b = 1) const;

// status

  int                empty() const;
  int                virtual_bit() const;
  int                count(int b = 1) const;
  
// convertors & IO

  friend BitSet      atoBitSet(const char* s, 
                               char f='0', char t='1', char star='*');
  friend const char* BitSettoa(const BitSet& x, 
                               char f='0', char t='1', char star='*');

  friend BitSet      shorttoBitSet(unsigned short w);
  friend BitSet      longtoBitSet(unsigned long w);

  friend ostream&    operator << (ostream& s, const BitSet& x);

// procedural versions of operators

  friend void        and(const BitSet& x, const BitSet& y, BitSet& r);
  friend void        or(const BitSet& x, const BitSet& y, BitSet& r);
  friend void        xor(const BitSet& x, const BitSet& y, BitSet& r);
  friend void        diff(const BitSet& x, const BitSet& y, BitSet& r);
  friend void        complement(const BitSet& x, BitSet& r);

// misc

  volatile void      error(const char* msg) const;
  int                OK() const;
};


typedef BitSet BitSetTmp;


  BitSet      operator |  (const BitSet& x, const BitSet& y);
  BitSet      operator &  (const BitSet& x, const BitSet& y);
  BitSet      operator -  (const BitSet& x, const BitSet& y);
  BitSet      operator ^  (const BitSet& x, const BitSet& y);

  BitSet      operator ~  (const BitSet& x);

// These are inlined regardless of optimization

inline int BitSet_index(int l)
{
  return (unsigned)(l) / BITSETBITS;
}

inline int BitSet_pos(int l)
{
  return l & (BITSETBITS - 1);
}

#if defined(__OPTIMIZE__) || defined(USE_LIBGXX_INLINES)


inline BitSet::BitSet() : rep(&_nilBitSetRep) {}

inline BitSet::BitSet(const BitSet& x) :rep(BitSetcopy(0, x.rep)) {}

inline BitSet::~BitSet() { if (rep != &_nilBitSetRep) delete rep; }

inline void BitSet::operator =  (const BitSet& y)
{ 
  rep = BitSetcopy(rep, y.rep);
}

inline int operator != (const BitSet& x, const BitSet& y) { return !(x == y); }

inline int operator >  (const BitSet& x, const BitSet& y) { return y < x; }

inline int operator >= (const BitSet& x, const BitSet& y) { return y <= x; }

inline void and(const BitSet& x, const BitSet& y, BitSet& r)
{
  r.rep =  BitSetop(x.rep, y.rep, r.rep, '&');
}

inline void or(const BitSet& x, const BitSet& y, BitSet& r)
{
  r.rep =  BitSetop(x.rep, y.rep, r.rep, '|');
}

inline void xor(const BitSet& x, const BitSet& y, BitSet& r)
{
  r.rep =  BitSetop(x.rep, y.rep, r.rep, '^');
}

inline void diff(const BitSet& x, const BitSet& y, BitSet& r)
{
  r.rep =  BitSetop(x.rep, y.rep, r.rep, '-');
}

inline void complement(const BitSet& x, BitSet& r)
{
  r.rep = BitSetcmpl(x.rep, r.rep);
}

#if defined(__GNUG__) && !defined(NO_NRV)

inline BitSet operator & (const BitSet& x, const BitSet& y) return r
{
  and(x, y, r);
}

inline BitSet operator | (const BitSet& x, const BitSet& y) return r
{
  or(x, y, r);
}

inline BitSet operator ^ (const BitSet& x, const BitSet& y) return r
{
  xor(x, y, r);
}

inline BitSet operator - (const BitSet& x, const BitSet& y) return r
{
  diff(x, y, r);
}

inline BitSet operator ~ (const BitSet& x) return r
{
  ::complement(x, r);
}

#else /* NO_NRV */

inline BitSet operator & (const BitSet& x, const BitSet& y) 
{
  BitSet r; and(x, y, r); return r;
}

inline BitSet operator | (const BitSet& x, const BitSet& y) 
{
  BitSet r; or(x, y, r); return r;
}

inline BitSet operator ^ (const BitSet& x, const BitSet& y) 
{
  BitSet r; xor(x, y, r); return r;
}

inline BitSet operator - (const BitSet& x, const BitSet& y) 
{
  BitSet r; diff(x, y, r); return r;
}

inline BitSet operator ~ (const BitSet& x) 
{
  BitSet r; ::complement(x, r); return r;
}

#endif

inline void BitSet::operator &= (const BitSet& y)
{
  and(*this, y, *this);
}

inline void BitSet::operator |= (const BitSet& y)
{
  or(*this, y, *this);
}

inline void BitSet::operator ^= (const BitSet& y)
{
  xor(*this, y, *this);
}

inline void BitSet::operator -= (const BitSet& y)
{
  diff(*this, y, *this);
}


inline void BitSet::complement()
{
  ::complement(*this, *this);
}

inline int BitSet::virtual_bit() const
{
  return rep->virt;
}

inline int BitSet::first(int b) const
{
  return next(-1, b);
}

inline int BitSet::test(int p) const
{
  if (p < 0) error("Illegal bit index");
  int index = BitSet_index(p);
  return (index >= rep->len)? rep->virt : 
         ((rep->s[index] & (1 << BitSet_pos(p))) != 0);
}


inline void BitSet::clear()
{
  if (rep->len > 0) bzero(rep->s, rep->sz * sizeof(short));
  rep->len = rep->virt = 0;
}

inline void BitSet::set()
{
  rep = BitSetalloc(rep, 0, 0, 1, 0);
}

inline BitSetBit::BitSetBit(const BitSetBit& b) :src(b.src), pos(b.pos) {}

inline BitSetBit::BitSetBit(BitSet* v, int p)
{
  src = v;  pos = p;
}

inline BitSetBit::~BitSetBit() {}

inline BitSetBit::operator int()
{
  return src->test(pos);
}

inline int BitSetBit::operator = (int b)
{
  if (b) src->set(pos); else src->clear(pos); return b;
}

inline int BitSetBit::operator == (int b)
{
  return src->test(pos) == b;
}

inline int BitSetBit::operator != (int b)
{
  return src->test(pos) != b;
}

inline BitSetBit BitSet::operator [] (int i)
{
  if (i < 0) error("illegal bit index");
  return BitSetBit(this, i);
}



#endif
#endif
