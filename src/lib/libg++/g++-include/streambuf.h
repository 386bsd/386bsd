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

#ifndef _streambuf_h
#ifdef __GNUG__
#pragma once
#pragma interface
#endif
#define _streambuf_h 1

/* streambufs. 
   basic streambufs and filebufs are as in Stroustrup, ch 8,
   but the base class contains virtual extensions that allow
   most capabilities of libg++ Files to be used as streambufs
   via `Filebufs'.
*/

#include <stdio.h>
#include <builtin.h>
#include <Fmodes.h>

// see below for NO_LINE_BUFFER_STREAMBUFS

#ifndef BUFSIZE
#ifdef BUFSIZ
#define BUFSIZE BUFSIZ
#else
#define BUFSIZE 1024
#endif
#endif

enum open_mode // filebuf open modes
{ 
  input=0, 
  output=1, 
  append=2 
}; 

class streambuf
{
public:
  char*       base;          // start of buffer
  char*       pptr;          // put-pointer (and gptr fence)
  char*       gptr;          // get-pointer
  char*       eptr;          // last valid addr in buffer

  char        alloc;         // true if we own freestore alloced buffer

              streambuf();
              streambuf(char* buf, int buflen);
 
  virtual    ~streambuf();

  int         doallocate();
  int         allocate();


  int         must_overflow(int ch); // true if should call overflow

  virtual int overflow(int c = EOF); // flush -- return EOF if fail
  virtual int underflow();           // fill -- return EOF if fail

  int         sgetc();          // get one char (as int) or EOF
  int         snextc();         // get and advance
  void        stossc();         // advance only

  int         sputbackc(char);   // unget

  int         sputc(int c = EOF); // write one char

  virtual streambuf*  setbuf(char* buf, int buflen, int preloaded_count = 0);
                                // (not virtual in AT&T)

// the following aren't in AT&T version:

  int         sputs(const char* s);           // write null-terminated str
  int         sputsn(const char* s, int len); // write len bytes

  virtual const char* name();


  virtual streambuf*  open(const char* name, open_mode m);
  virtual streambuf*  open(const char* filename, io_mode m, access_mode a);
  virtual streambuf*  open(const char* filename, const char* m);
  virtual streambuf*  open(int  filedesc, io_mode m);
  virtual streambuf*  open(FILE* fileptr);

  virtual int         is_open();
  virtual int         close();

  virtual void        error();
};


#if defined(__OPTIMIZE__) || defined(USE_LIBGXX_INLINES)


inline int streambuf::must_overflow(int ch)
{
#ifndef NO_LINE_BUFFER_STREAMBUF
  return pptr >= eptr || ch == '\n';
#else
  return pptr >= eptr;
#endif
}


inline int streambuf::allocate()
{
  return (base == 0)? doallocate() : 0; 
}

inline int streambuf::sgetc()
{
  return (gptr >= pptr)? underflow() : int((unsigned char)(*gptr));
}


inline int streambuf::snextc()
{
  ++gptr;
  return (gptr >= pptr)? underflow() : int((unsigned char)(*gptr));
}


inline void streambuf::stossc()
{
  if (gptr >= pptr) underflow(); else gptr++;
}


inline int streambuf::sputbackc(char ch)
{
  return (gptr > base)? int((unsigned char)(*--gptr = ch)) : EOF;
}

inline int streambuf::sputc(int ch)
{
  return must_overflow(ch)? overflow(ch) : 
                            int((unsigned char)(*pptr++ = char(ch)));
}

#endif

#endif
