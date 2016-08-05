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

/* 
  String class implementation
 */

#ifdef __GNUG__
#pragma implementation
#endif
#include <String.h>
#include <std.h>
#include <ctype.h>
#include <values.h>
#include <new.h>

// extern "C" {
#include <regex.h>
// }

volatile void String::error(const char* msg) const
{
  (void)((*lib_error_handler)("String", msg));
}

//  globals

StrRep  _nilStrRep = { 0, 1, { 0 } }; // nil strings point here
String _nilString;               // nil SubStrings point here




/*
 the following inline fcts are specially designed to work
 in support of String classes, and are not meant as generic replacements
 for libc "str" functions.

 inline copy fcts -  I like left-to-right from->to arguments.
 all versions assume that `to' argument is non-null

 These are worth doing inline, rather than through calls because,
 via procedural integration, adjacent copy calls can be smushed
 together by the optimizer.
*/

// copy n bytes
inline static void ncopy(const char* from, char* to, int n)
{
  if (from != to) while (--n >= 0) *to++ = *from++;
}

// copy n bytes, null-terminate
inline static void ncopy0(const char* from, char* to, int n)
{
  if (from != to) 
  {
    while (--n >= 0) *to++ = *from++;
    *to = 0;
  }
  else
    to[n] = 0;
}

// copy until null
inline static void scopy(const char* from, char* to)
{
  if (from != 0) while((*to++ = *from++) != 0);
}

// copy right-to-left
inline static void revcopy(const char* from, char* to, short n)
{
  if (from != 0) while (--n >= 0) *to-- = *from--;
}


inline static int slen(const char* t) // inline  strlen
{
  if (t == 0)
    return 0;
  else
  {
    const char* a = t;
    while (*a++ != 0);
    return a - 1 - t;
  }
}

// minimum & maximum representable rep size

#define MAXStrRep_SIZE   ((1 << (SHORTBITS - 1)) - 1)
#define MINStrRep_SIZE   16

#ifndef MALLOC_MIN_OVERHEAD
#define MALLOC_MIN_OVERHEAD  4
#endif

// The basic allocation primitive:
// Always round request to something close to a power of two.
// This ensures a bit of padding, which often means that
// concatenations don't have to realloc. Plus it tends to
// be faster when lots of Strings are created and discarded,
// since just about any version of malloc (op new()) will
// be faster when it can reuse identically-sized chunks

inline static StrRep* Snew(int newsiz)
{
  unsigned int siz = sizeof(StrRep) + newsiz + MALLOC_MIN_OVERHEAD;
  unsigned int allocsiz = MINStrRep_SIZE;
  while (allocsiz < siz) allocsiz <<= 1;
  allocsiz -= MALLOC_MIN_OVERHEAD;
  if (allocsiz >= MAXStrRep_SIZE)
    (*lib_error_handler)("String", "Requested length out of range");
    
  StrRep* rep = (StrRep *) new char[allocsiz];
  rep->sz = allocsiz - sizeof(StrRep);
  return rep;
}

// Do-something-while-allocating routines.

// We live with two ways to signify empty Sreps: either the
// null pointer (0) or a pointer to the nilStrRep.

// We always signify unknown source lengths (usually when fed a char*)
// via len == -1, in which case it is computed.

// allocate, copying src if nonull

StrRep* Salloc(StrRep* old, const char* src, int srclen, int newlen)
{
  if (old == &_nilStrRep) old = 0;
  if (srclen < 0) srclen = slen(src);
  if (newlen < srclen) newlen = srclen;
  StrRep* rep;
  if (old == 0 || newlen > old->sz)
    rep = Snew(newlen);
  else
    rep = old;

  rep->len = newlen;
  ncopy0(src, rep->s, srclen);

  if (old != rep && old != 0) delete old;

  return rep;
}

// reallocate: Given the initial allocation scheme, it will
// generally be faster in the long run to get new space & copy
// than to call realloc

StrRep* Sresize(StrRep* old, int newlen)
{
  if (old == &_nilStrRep) old = 0;
  StrRep* rep;
  if (old == 0)
    rep = Snew(newlen);
  else if (newlen > old->sz)
  {
    rep = Snew(newlen);
    ncopy0(old->s, rep->s, old->len);
    delete old;
  }
  else
    rep = old;

  rep->len = newlen;

  return rep;
}

// like allocate, but we know that src is a StrRep

StrRep* Scopy(StrRep* old, StrRep* s)
{
  if (old == &_nilStrRep) old = 0;
  if (s == &_nilStrRep) s = 0;
  if (old == s) 
    return (old == 0)? &_nilStrRep : old;
  else if (s == 0)
  {
    old->s[0] = 0;
    old->len = 0;
    return old;
  }
  else 
  {
    StrRep* rep;
    int newlen = s->len;
    if (old == 0 || newlen > old->sz)
    {
      if (old != 0) delete old;
      rep = Snew(newlen);
    }
    else
      rep = old;
    rep->len = newlen;
    ncopy0(s->s, rep->s, newlen);
    return rep;
  }
}

// allocate & concatenate

StrRep* Scat(StrRep* old, const char* s, int srclen, const char* t, int tlen)
{
  if (old == &_nilStrRep) old = 0;
  if (srclen < 0) srclen = slen(s);
  if (tlen < 0) tlen = slen(t);
  int newlen = srclen + tlen;
  StrRep* rep;

  if (old == 0 || newlen > old->sz || 
      (t >= old->s && t < &(old->s[old->len]))) // beware of aliasing
    rep = Snew(newlen);
  else
    rep = old;

  rep->len = newlen;

  ncopy(s, rep->s, srclen);
  ncopy0(t, &(rep->s[srclen]), tlen);

  if (old != rep && old != 0) delete old;

  return rep;
}

// double-concatenate

StrRep* Scat(StrRep* old, const char* s, int srclen, const char* t, int tlen,
             const char* u, int ulen)
{
  if (old == &_nilStrRep) old = 0;
  if (srclen < 0) srclen = slen(s);
  if (tlen < 0) tlen = slen(t);
  if (ulen < 0) tlen = slen(u);
  int newlen = srclen + tlen + ulen;
  StrRep* rep;
  if (old == 0 || newlen > old->sz || 
      (t >= old->s && t < &(old->s[old->len])) ||
      (u >= old->s && u < &(old->s[old->len])))
    rep = Snew(newlen);
  else
    rep = old;

  rep->len = newlen;

  ncopy(s, rep->s, srclen);
  ncopy(t, &(rep->s[srclen]), tlen);
  ncopy0(u, &(rep->s[srclen+tlen]), ulen);

  if (old != rep && old != 0) delete old;

  return rep;
}

// like cat, but we know that new stuff goes in the front of existing rep

StrRep* Sprepend(StrRep* old, const char* t, int tlen)
{
  char* s;
  int srclen;
  if (old == &_nilStrRep || old == 0)
  {
    s = 0; old = 0; srclen = 0;
  }
  else
  {
    s = old->s; srclen = old->len;
  }
  if (tlen < 0) tlen = slen(t);
  int newlen = srclen + tlen;
  StrRep* rep;
  if (old == 0 || newlen > old->sz || 
      (t >= old->s && t < &(old->s[old->len])))
    rep = Snew(newlen);
  else
    rep = old;

  rep->len = newlen;

  revcopy(&(s[srclen]), &(rep->s[newlen]), srclen+1);
  ncopy(t, rep->s, tlen);

  if (old != rep && old != 0) delete old;

  return rep;
}


// string compare: first argument is known to be non-null

inline static int scmp(const char* a, const char* b)
{
  if (b == 0)
    return *a != 0;
  else
  {
    signed char diff = 0;
    while ((diff = *a - *b++) == 0 && *a++ != 0);
    return diff;
  }
}


inline static int ncmp(const char* a, int al, const char* b, int bl)
{
  int n = (al <= bl)? al : bl;
  signed char diff;
  while (n-- > 0) if ((diff = *a++ - *b++) != 0) return diff;
  return al - bl;
}

int fcompare(const String& x, const String& y)
{
  const char* a = x.chars();
  const char* b = y.chars();
  int al = x.length();
  int bl = y.length();
  int n = (al <= bl)? al : bl;
  signed char diff = 0;
  while (n-- > 0)
  {
    char ac = *a++;
    char bc = *b++;
    if ((diff = ac - bc) != 0)
    {
      if (ac >= 'a' && ac <= 'z')
        ac = ac - 'a' + 'A';
      if (bc >= 'a' && bc <= 'z')
        bc = bc - 'a' + 'A';
      if ((diff = ac - bc) != 0)
        return diff;
    }
  }
  return al - bl;
}

// these are not inline, but pull in the above inlines, so are 
// pretty fast

int compare(const String& x, const char* b)
{
  return scmp(x.chars(), b);
}

int compare(const String& x, const String& y)
{
  return scmp(x.chars(), y.chars());
}

int compare(const String& x, const SubString& y)
{
  return ncmp(x.chars(), x.length(), y.chars(), y.length());
}

int compare(const SubString& x, const String& y)
{
  return ncmp(x.chars(), x.length(), y.chars(), y.length());
}

int compare(const SubString& x, const SubString& y)
{
  return ncmp(x.chars(), x.length(), y.chars(), y.length());
}

int compare(const SubString& x, const char* b)
{
  if (b == 0)
    return x.length();
  else
  {
    const char* a = x.chars();
    int n = x.length();
    signed char diff;
    while (n-- > 0) if ((diff = *a++ - *b++) != 0) return diff;
    return (*b == 0) ? 0 : -1;
  }
}

/*
 index fcts
*/

int String::search(int start, int sl, char c) const
{
  const char* s = chars();
  if (sl > 0)
  {
    if (start >= 0)
    {
      const char* a = &(s[start]);
      const char* lasta = &(s[sl]);
      while (a < lasta) if (*a++ == c) return --a - s;
    }
    else
    {
      const char* a = &(s[sl + start + 1]);
      while (--a >= s) if (*a == c) return a - s;
    }
  }
  return -1;
}

int String::search(int start, int sl, const char* t, int tl) const
{
  const char* s = chars();
  if (tl < 0) tl = slen(t);
  if (sl > 0 && tl > 0)
  {
    if (start >= 0)
    {
      const char* lasts = &(s[sl - tl]);
      const char* lastt = &(t[tl]);
      const char* p = &(s[start]);

      while (p <= lasts)
      {
        const char* x = p++;
        const char* y = t;
        while (*x++ == *y++) if (y >= lastt) return --p - s;
      }
    }
    else
    {
      const char* firsts = &(s[tl - 1]);
      const char* lastt =  &(t[tl - 1]);
      const char* p = &(s[sl + start + 1]); 

      while (--p >= firsts)
      {
        const char* x = p;
        const char* y = lastt;
        while (*x-- == *y--) if (y < t) return ++x - s;
      }
    }
  }
  return -1;
}

int String::match(int start, int sl, int exact, const char* t, int tl) const
{
  if (tl < 0) tl = slen(t);

  if (start < 0)
  {
    start = sl + start - tl + 1;
    if (start < 0 || (exact && start != 0))
      return -1;
  }
  else if (exact && sl - start != tl)
    return -1;

  if (sl == 0 || tl == 0 || sl - start < tl || start >= sl)
    return -1;

  int n = tl;
  const char* s = &(rep->s[start]);
  while (--n >= 0) if (*s++ != *t++) return -1;
  return tl;
}

void SubString::assign(StrRep* ysrc, const char* ys, int ylen)
{
  if (&S == &_nilString) return;

  if (ylen < 0) ylen = slen(ys);
  StrRep* targ = S.rep;
  int sl = targ->len - len + ylen;

  if (ysrc == targ || sl >= targ->sz)
  {
    StrRep* oldtarg = targ;
    targ = Sresize(0, sl);
    ncopy(oldtarg->s, targ->s, pos);
    ncopy(ys, &(targ->s[pos]), ylen);
    scopy(&(oldtarg->s[pos + len]), &(targ->s[pos + ylen]));
    delete oldtarg;
  }
  else if (len == ylen)
    ncopy(ys, &(targ->s[pos]), len);
  else if (ylen < len)
  {
    ncopy(ys, &(targ->s[pos]), ylen);
    scopy(&(targ->s[pos + len]), &(targ->s[pos + ylen]));
  }
  else
  {
    revcopy(&(targ->s[targ->len]), &(targ->s[sl]), targ->len-pos-len +1);
    ncopy(ys, &(targ->s[pos]), ylen);
  }
  targ->len = sl;
  S.rep = targ;
}



/*
 * substitution
 */


int String::_gsub(const char* pat, int pl, const char* r, int rl)
{
  int nmatches = 0;
  if (pl < 0) pl = slen(pat);
  if (rl < 0) rl = slen(r);
  int sl = length();
  if (sl <= 0 || pl <= 0 || sl < pl)
    return nmatches;
  
  const char* s = chars();

  // prepare to make new rep
  StrRep* nrep = 0;
  int nsz = 0;
  char* x = 0;

  int si = 0;
  int xi = 0;
  int remaining = sl;

  while (remaining >= pl)
  {
    int pos = search(si, sl, pat, pl);
    if (pos < 0)
      break;
    else
    {
      ++nmatches;
      int mustfit = xi + remaining + rl - pl;
      if (mustfit >= nsz)
      {
        if (nrep != 0) nrep->len = xi;
        nrep = Sresize(nrep, mustfit);
        nsz = nrep->sz;
        x = nrep->s;
      }
      pos -= si;
      ncopy(&(s[si]), &(x[xi]), pos);
      ncopy(r, &(x[xi + pos]), rl);
      si += pos + pl;
      remaining -= pos + pl;
      xi += pos + rl;
    }
  }

  if (nrep == 0)
  {
    if (nmatches == 0)
      return nmatches;
    else
      nrep = Sresize(nrep, xi+remaining);
  }

  ncopy0(&(s[si]), &(x[xi]), remaining);
  nrep->len = xi + remaining;

  if (nrep->len <= rep->sz)   // fit back in if possible
  {
    rep->len = nrep->len;
    ncopy0(nrep->s, rep->s, rep->len);
    delete(nrep);
  }
  else
  {
    delete(rep);
    rep = nrep;
  }
  return nmatches;
}

int String::_gsub(const Regex& pat, const char* r, int rl)
{
  int nmatches = 0;
  int sl = length();
  if (sl <= 0)
    return nmatches;

  if (rl < 0) rl = slen(r);

  const char* s = chars();

  StrRep* nrep = 0;
  int nsz = 0;

  char* x = 0;

  int si = 0;
  int xi = 0;
  int remaining = sl;
  int  pos, pl = 0;				  // how long is a regular expression?

  while (remaining > 0)
  {
    pos = pat.search(s, sl, pl, si); // unlike string search, the pos returned here is absolute
    if (pos < 0 || pl <= 0)
      break;
    else
    {
      ++nmatches;
      int mustfit = xi + remaining + rl - pl;
      if (mustfit >= nsz)
      {
        if (nrep != 0) nrep->len = xi;
        nrep = Sresize(nrep, mustfit);
        x = nrep->s;
        nsz = nrep->sz;
      }
      pos -= si;
      ncopy(&(s[si]), &(x[xi]), pos);
      ncopy(r, &(x[xi + pos]), rl);
      si += pos + pl;
      remaining -= pos + pl;
      xi += pos + rl;
    }
  }

  if (nrep == 0)
  {
    if (nmatches == 0)
      return nmatches;
    else
      nrep = Sresize(nrep, xi+remaining);
  }

  ncopy0(&(s[si]), &(x[xi]), remaining);
  nrep->len = xi + remaining;

  if (nrep->len <= rep->sz)   // fit back in if possible
  {
    rep->len = nrep->len;
    ncopy0(nrep->s, rep->s, rep->len);
    delete(nrep);
  }
  else
  {
    delete(rep);
    rep = nrep;
  }
  return nmatches;
}


/*
 * deletion
 */

void String::del(int pos, int len)
{
  if (pos < 0 || len <= 0 || pos + len > length()) return;
  int nlen = length() - len;
  int first = pos + len;
  ncopy0(&(rep->s[first]), &(rep->s[pos]), length() - first);
  rep->len = nlen;
}

void String::del(const Regex& r, int startpos)
{
  int mlen;
  int first = r.search(chars(), length(), mlen, startpos);
  del(first, mlen);
}

void String::del(const char* t, int startpos)
{
  int tlen = slen(t);
  int p = search(startpos, length(), t, tlen);
  del(p, tlen);
}

void String::del(const String& y, int startpos)
{
  del(search(startpos, length(), y.chars(), y.length()), y.length());
}

void String::del(const SubString& y, int startpos)
{
  del(search(startpos, length(), y.chars(), y.length()), y.length());
}

void String::del(char c, int startpos)
{
  del(search(startpos, length(), c), 1);
}

/*
 * substring extraction
 */


SubString String::at(int first, int len)
{
  return _substr(first, len);
}

SubString String::operator() (int first, int len)
{
  return _substr(first, len);
}

SubString String::before(int pos)
{
  return _substr(0, pos);
}

SubString String::through(int pos)
{
  return _substr(0, pos+1);
}

SubString String::after(int pos)
{
  return _substr(pos + 1, length() - (pos + 1));
}

SubString String::from(int pos)
{
  return _substr(pos, length() - pos);
}

SubString String::at(const String& y, int startpos)
{
  int first = search(startpos, length(), y.chars(), y.length());
  return _substr(first,  y.length());
}

SubString String::at(const SubString& y, int startpos)
{
  int first = search(startpos, length(), y.chars(), y.length());
  return _substr(first, y.length());
}

SubString String::at(const Regex& r, int startpos)
{
  int mlen;
  int first = r.search(chars(), length(), mlen, startpos);
  return _substr(first, mlen);
}

SubString String::at(const char* t, int startpos)
{
  int tlen = slen(t);
  int first = search(startpos, length(), t, tlen);
  return _substr(first, tlen);
}

SubString String::at(char c, int startpos)
{
  int first = search(startpos, length(), c);
  return _substr(first, 1);
}

SubString String::before(const String& y, int startpos)
{
  int last = search(startpos, length(), y.chars(), y.length());
  return _substr(0, last);
}

SubString String::before(const SubString& y, int startpos)
{
  int last = search(startpos, length(), y.chars(), y.length());
  return _substr(0, last);
}

SubString String::before(const Regex& r, int startpos)
{
  int mlen;
  int first = r.search(chars(), length(), mlen, startpos);
  return _substr(0, first);
}

SubString String::before(char c, int startpos)
{
  int last = search(startpos, length(), c);
  return _substr(0, last);
}

SubString String::before(const char* t, int startpos)
{
  int tlen = slen(t);
  int last = search(startpos, length(), t, tlen);
  return _substr(0, last);
}

SubString String::through(const String& y, int startpos)
{
  int last = search(startpos, length(), y.chars(), y.length());
  if (last >= 0) last += y.length();
  return _substr(0, last);
}

SubString String::through(const SubString& y, int startpos)
{
  int last = search(startpos, length(), y.chars(), y.length());
  if (last >= 0) last += y.length();
  return _substr(0, last);
}

SubString String::through(const Regex& r, int startpos)
{
  int mlen;
  int first = r.search(chars(), length(), mlen, startpos);
  if (first >= 0) first += mlen;
  return _substr(0, first);
}

SubString String::through(char c, int startpos)
{
  int last = search(startpos, length(), c);
  if (last >= 0) last += 1;
  return _substr(0, last);
}

SubString String::through(const char* t, int startpos)
{
  int tlen = slen(t);
  int last = search(startpos, length(), t, tlen);
  if (last >= 0) last += tlen;
  return _substr(0, last);
}

SubString String::after(const String& y, int startpos)
{
  int first = search(startpos, length(), y.chars(), y.length());
  if (first >= 0) first += y.length();
  return _substr(first, length() - first);
}

SubString String::after(const SubString& y, int startpos)
{
  int first = search(startpos, length(), y.chars(), y.length());
  if (first >= 0) first += y.length();
  return _substr(first, length() - first);
}

SubString String::after(char c, int startpos)
{
  int first = search(startpos, length(), c);
  if (first >= 0) first += 1;
  return _substr(first, length() - first);
}

SubString String::after(const Regex& r, int startpos)
{
  int mlen;
  int first = r.search(chars(), length(), mlen, startpos);
  if (first >= 0) first += mlen;
  return _substr(first, length() - first);
}

SubString String::after(const char* t, int startpos)
{
  int tlen = slen(t);
  int first = search(startpos, length(), t, tlen);
  if (first >= 0) first += tlen;
  return _substr(first, length() - first);
}

SubString String::from(const String& y, int startpos)
{
  int first = search(startpos, length(), y.chars(), y.length());
  return _substr(first, length() - first);
}

SubString String::from(const SubString& y, int startpos)
{
  int first = search(startpos, length(), y.chars(), y.length());
  return _substr(first, length() - first);
}

SubString String::from(const Regex& r, int startpos)
{
  int mlen;
  int first = r.search(chars(), length(), mlen, startpos);
  return _substr(first, length() - first);
}

SubString String::from(char c, int startpos)
{
  int first = search(startpos, length(), c);
  return _substr(first, length() - first);
}

SubString String::from(const char* t, int startpos)
{
  int tlen = slen(t);
  int first = search(startpos, length(), t, tlen);
  return _substr(first, length() - first);
}



/*
 * split/join
 */


int split(const String& src, String results[], int n, const String& sep)
{
  String x = src;
  const char* s = x.chars();
  int sl = x.length();
  int i = 0;
  int pos = 0;
  while (i < n && pos < sl)
  {
    int p = x.search(pos, sl, sep.chars(), sep.length());
    if (p < 0)
      p = sl;
    results[i].rep = Salloc(results[i].rep, &(s[pos]), p - pos, p - pos);
    i++;
    pos = p + sep.length();
  }
  return i;
}

int split(const String& src, String results[], int n, const Regex& r)
{
  String x = src;
  const char* s = x.chars();
  int sl = x.length();
  int i = 0;
  int pos = 0;
  int p, matchlen;
  while (i < n && pos < sl)
  {
    p = r.search(s, sl, matchlen, pos);
    if (p < 0)
      p = sl;
    results[i].rep = Salloc(results[i].rep, &(s[pos]), p - pos, p - pos);
    i++;
    pos = p + matchlen;
  }
  return i;
}


#if defined(__GNUG__) && !defined(NO_NRV)

String join(String src[], int n, const String& separator) return x;
{
  String sep = separator;
  int xlen = 0;
  for (int i = 0; i < n; ++i)
    xlen += src[i].length();
  xlen += (n - 1) * sep.length();

  x.alloc(xlen);

  int j = 0;
  
  for (i = 0; i < n - 1; ++i)
  {
    ncopy(src[i].chars(), &(x.rep->s[j]), src[i].length());
    j += src[i].length();
    ncopy(sep.chars(), &(x.rep->s[j]), sep.length());
    j += sep.length();
  }
  ncopy0(src[i].chars(), &(x.rep->s[j]), src[i].length());
}

#else

String join(String src[], int n, const String& separator)
{
  String x;
  String sep = separator;
  int xlen = 0;
  for (int i = 0; i < n; ++i)
    xlen += src[i].length();
  xlen += (n - 1) * sep.length();

  x.alloc(xlen);

  int j = 0;
  
  for (i = 0; i < n - 1; ++i)
  {
    ncopy(src[i].chars(), &(x.rep->s[j]), src[i].length());
    j += src[i].length();
    ncopy(sep.chars(), &(x.rep->s[j]), sep.length());
    j += sep.length();
  }
  ncopy0(src[i].chars(), &(x.rep->s[j]), src[i].length());
  return x;
}

#endif
  
/*
 misc
*/

    
StrRep* Sreverse(StrRep* src, StrRep* dest)
{
  int n = src->len;
  if (src != dest)
    dest = Salloc(dest, src->s, n, n);
  if (n > 0)
  {
    char* a = dest->s;
    char* b = &(a[n - 1]);
    while (a < b)
    {
      char t = *a;
      *a++ = *b;
      *b-- = t;
    }
  }
  return dest;
}


StrRep* Supcase(StrRep* src, StrRep* dest)
{
  int n = src->len;
  if (src != dest) dest = Salloc(dest, src->s, n, n);
  char* p = dest->s;
  char* e = &(p[n]);
  for (; p < e; ++p) if (islower(*p)) *p = toupper(*p);
  return dest;
}

StrRep* Sdowncase(StrRep* src, StrRep* dest)
{
  int n = src->len;
  if (src != dest) dest = Salloc(dest, src->s, n, n);
  char* p = dest->s;
  char* e = &(p[n]);
  for (; p < e; ++p) if (isupper(*p)) *p = tolower(*p);
  return dest;
}

StrRep* Scapitalize(StrRep* src, StrRep* dest)
{
  int n = src->len;
  if (src != dest) dest = Salloc(dest, src->s, n, n);

  char* p = dest->s;
  char* e = &(p[n]);
  for (; p < e; ++p)
  {
    int at_word;
    if (at_word = islower(*p))
      *p = toupper(*p);
    else 
      at_word = isupper(*p) || isdigit(*p);

    if (at_word)
    {
      while (++p < e)
      {
        if (isupper(*p))
          *p = tolower(*p);
        else if (!islower(*p) && !isdigit(*p))
          break;
      }
    }
  }
  return dest;
}

#if defined(__GNUG__) && !defined(NO_NRV)

String replicate(char c, int n) return w;
{
  w.rep = Sresize(w.rep, n);
  char* p = w.rep->s;
  while (n-- > 0) *p++ = c;
  *p = 0;
}

String replicate(const String& y, int n) return w
{
  int len = y.length();
  w.rep = Sresize(w.rep, n * len);
  char* p = w.rep->s;
  while (n-- > 0)
  {
    ncopy(y.chars(), p, len);
    p += len;
  }
  *p = 0;
}

String common_prefix(const String& x, const String& y, int startpos) return r;
{
  const char* xchars = x.chars();
  const char* ychars = y.chars();
  const char* xs = &(xchars[startpos]);
  const char* ss = xs;
  const char* topx = &(xchars[x.length()]);
  const char* ys = &(ychars[startpos]);
  const char* topy = &(ychars[y.length()]);
  for (int l = 0; xs < topx && ys < topy && *xs++ == *ys++; ++l);
  r.rep = Salloc(r.rep, ss, l, l);
}

String common_suffix(const String& x, const String& y, int startpos) return r;
{
  const char* xchars = x.chars();
  const char* ychars = y.chars();
  const char* xs = &(xchars[x.length() + startpos]);
  const char* botx = xchars;
  const char* ys = &(ychars[y.length() + startpos]);
  const char* boty = ychars;
  for (int l = 0; xs >= botx && ys >= boty && *xs == *ys ; --xs, --ys, ++l);
  r.rep = Salloc(r.rep, ++xs, l, l);
}

#else

String replicate(char c, int n)
{
  String w;
  w.rep = Sresize(w.rep, n);
  char* p = w.rep->s;
  while (n-- > 0) *p++ = c;
  *p = 0;
  return w;
}

String replicate(const String& y, int n)
{
  String w;
  int len = y.length();
  w.rep = Sresize(w.rep, n * len);
  char* p = w.rep->s;
  while (n-- > 0)
  {
    ncopy(y.chars(), p, len);
    p += len;
  }
  *p = 0;
  return w;
}

String common_prefix(const String& x, const String& y, int startpos)
{
  String r;
  const char* xchars = x.chars();
  const char* ychars = y.chars();
  const char* xs = &(xchars[startpos]);
  const char* ss = xs;
  const char* topx = &(xchars[x.length()]);
  const char* ys = &(ychars[startpos]);
  const char* topy = &(ychars[y.length()]);
  for (int l = 0; xs < topx && ys < topy && *xs++ == *ys++; ++l);
  r.rep = Salloc(r.rep, ss, l, l);
  return r;
}

String common_suffix(const String& x, const String& y, int startpos) 
{
  String r;
  const char* xchars = x.chars();
  const char* ychars = y.chars();
  const char* xs = &(xchars[x.length() + startpos]);
  const char* botx = xchars;
  const char* ys = &(ychars[y.length() + startpos]);
  const char* boty = ychars;
  for (int l = 0; xs >= botx && ys >= boty && *xs == *ys ; --xs, --ys, ++l);
  r.rep = Salloc(r.rep, ++xs, l, l);
  return r;
}

#endif

// IO

istream& operator>>(istream& s, String& x)
{
  if (!s.readable())
  {
    s.set(_bad);
    return s;
  }
  char ch;
  int i = 0;
  x.rep = Sresize(x.rep, 20);
  s >> WS;
  if (!s.good())
  {
    s.set(_bad);
    return s;
  }
  while (s.get(ch))
  {
    if (isspace(ch))
      break;
    if (i >= x.rep->sz - 1)
      x.rep = Sresize(x.rep, i+1);
    x.rep->s[i++] = ch;
  }
  x.rep->s[i] = 0;
  x.rep->len = i;
  s.failif(i == 0);
  return s;
}

int readline(istream& s, String& x, char terminator, int discard)
{
  if (!s.readable())
  {
    s.set(_bad);
    return 0;
  }
  char ch;
  int i = 0;
  x.rep = Sresize(x.rep, 80);
  while (s.get(ch))
  {
    if (ch != terminator || !discard)
    {
      if (i >= x.rep->sz - 1)
        x.rep = Sresize(x.rep, i+1);
      x.rep->s[i++] = ch;
    }
    if (ch == terminator)
      break;
  }
  x.rep->s[i] = 0;
  x.rep->len = i;
  return i;
}


ostream& operator<<(ostream& s, const SubString& x)
{ 
  const char* a = x.chars();
  const char* lasta = &(a[x.length()]);
  while (a < lasta)
    s.put(*a++);
  return(s);
}

// from John.Willis@FAS.RI.CMU.EDU

int String::freq(const SubString& y) const
{
  int found = 0;
  for (int i = 0; i < length(); i++) 
    if (match(i,length(),0,y.chars(), y.length())>= 0) found++;
  return(found);
}

int String::freq(const String& y) const
{
  int found = 0;
  for (int i = 0; i < length(); i++) 
    if (match(i,length(),0,y.chars(),y.length()) >= 0) found++;
  return(found);
}

int String::freq(const char* t) const
{
  int found = 0;
  for (int i = 0; i < length(); i++) if (match(i,length(),0,t) >= 0) found++;
  return(found);
}

int String::freq(char c) const
{
  int found = 0;
  for (int i = 0; i < length(); i++) 
    if (match(i,length(),0,&c,1) >= 0) found++;
  return(found);
}


int String::OK() const
{
  int v = rep != 0;             // have a rep
  v &= rep->len <= rep->sz;     // string within bounds
  v &= rep->s[rep->len] == 0;   // null-terminated
  if (!v) error("invariant failure");
  return v;
}

int SubString::OK() const
{
  int v = S != 0;               // have a String;
  v &= S.OK();                 // that is legal
  v &= pos + len >= S.rep->len;// pos and len within bounds
  if (!v) S.error("SubString invariant failure");
  return v;
}

