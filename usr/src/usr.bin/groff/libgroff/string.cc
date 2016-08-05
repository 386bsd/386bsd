// -*- C++ -*-
/* Copyright (C) 1989, 1990, 1991 Free Software Foundation, Inc.
     Written by James Clark (jjc@jclark.uucp)

This file is part of groff.

groff is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 1, or (at your option) any later
version.

groff is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with groff; see the file LICENSE.  If not, write to the Free Software
Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. */

#include <stdio.h>
#include "stringclass.h"
#include "lib.h"

static char *salloc(int len, int *sizep);
static void sfree(char *ptr, int size);
static char *sfree_alloc(char *ptr, int size, int len, int *sizep);
static char *srealloc(char *ptr, int size, int oldlen, int newlen, int *sizep);

static char *salloc(int len, int *sizep)
{
  if (len == 0) {
    *sizep = 0;
    return 0;
  }
  else
    return new char[*sizep = len*2];
}

static void sfree(char *ptr, int)
{
  delete ptr;
}

static char *sfree_alloc(char *ptr, int oldsz, int len, int *sizep)
{
  if (oldsz >= len) {
    *sizep = oldsz;
    return ptr;
  }
  delete ptr;
  if (len == 0) {
    *sizep = 0;
    return 0;
  }
  else
    return new char[*sizep = len*2];
}

static char *srealloc(char *ptr, int oldsz, int oldlen, int newlen, int *sizep)
{
  if (oldsz >= newlen) {
    *sizep = oldsz;
    return ptr;
  }
  if (newlen == 0) {
    delete ptr;
    *sizep = 0;
    return 0;
  }
  else {
    char *p = new char[*sizep = newlen*2];
    if (oldlen < newlen && oldlen != 0)
      memcpy(p, ptr, oldlen);
    delete ptr;
    return p;
  }
}

string::string() : len(0), ptr(0), sz(0)
{
}

string::string(const char *p, int n) : len(n)
{
  assert(n >= 0);
  ptr = salloc(n, &sz);
  if (n != 0)
    memcpy(ptr, p, n);
}

string::string(const char *p)
{
  if (p == 0) {
    len = 0;
    ptr = 0;
    sz = 0;
  }
  else {
    len = strlen(p);
    ptr = salloc(len, &sz);
    memcpy(ptr, p, len);
  }
}

string::string(char c) : len(1)
{
  ptr = salloc(1, &sz);
  *ptr = c;
}

string::string(const string &s) : len(s.len)
{
  ptr = salloc(len, &sz);
  if (len != 0)
    memcpy(ptr, s.ptr, len);
}
  
string::~string()
{
  sfree(ptr, sz);
}

string &string::operator=(const string &s)
{
  ptr = sfree_alloc(ptr, sz, s.len, &sz);
  len = s.len;
  if (len != 0)
    memcpy(ptr, s.ptr, len);
  return *this;
}

string &string::operator=(const char *p)
{
  if (p == 0) {
    sfree(ptr, len);
    len = 0;
    ptr = 0;
    sz = 0;
  }
  else {
    int slen = strlen(p);
    ptr = sfree_alloc(ptr, sz, slen, &sz);
    len = slen;
    memcpy(ptr, p, len);
  }
  return *this;
}

string &string::operator=(char c)
{
  ptr = sfree_alloc(ptr, sz, 1, &sz);
  len = 1;
  *ptr = c;
  return *this;
}

void string::move(string &s)
{
  sfree(ptr, sz);
  ptr = s.ptr;
  len = s.len;
  sz = s.sz;
  s.ptr = 0;
  s.len = 0;
  s.sz = 0;
}

void string::grow1()
{
  ptr = srealloc(ptr, sz, len, len + 1, &sz);
}

string &string::operator+=(const char *p)
{
  if (p != 0) {
    int n = strlen(p);
    int newlen = len + n;
    if (newlen > sz)
      ptr = srealloc(ptr, sz, len, newlen, &sz);
    memcpy(ptr + len, p, n);
    len = newlen;
  }
  return *this;
}

string &string::operator+=(const string &s)
{
  if (s.len != 0) {
    int newlen = len + s.len;
    if (newlen > sz)
      ptr = srealloc(ptr, sz, len, newlen, &sz);
    memcpy(ptr + len, s.ptr, s.len);
    len = newlen;
  }
  return *this;
}

void string::append(const char *p, int n)
{
  if (n > 0) {
    int newlen = len + n;
    if (newlen > sz)
      ptr = srealloc(ptr, sz, len, newlen, &sz);
    memcpy(ptr + len, p, n);
    len = newlen;
  }
}

string::string(const char *s1, int n1, const char *s2, int n2)
{
  assert(n1 >= 0 && n2 >= 0);
  len = n1 + n2;
  if (len == 0) {
    sz = 0;
    ptr = 0;
  }
  else {
    ptr = salloc(len, &sz);
    if (n1 == 0)
      memcpy(ptr, s2, n2);
    else {
      memcpy(ptr, s1, n1);
      if (n2 != 0)
	memcpy(ptr + n1, s2, n2);
    }
  }
}

int operator<=(const string &s1, const string &s2)
{
  return (s1.len <= s2.len
	  ? s1.len == 0 || memcmp(s1.ptr, s2.ptr, s1.len) <= 0
	  : s2.len != 0 && memcmp(s1.ptr, s2.ptr, s2.len) < 0);
}

int operator<(const string &s1, const string &s2)
{
  return (s1.len < s2.len
	  ? s1.len == 0 || memcmp(s1.ptr, s2.ptr, s1.len) <= 0
	  : s2.len != 0 && memcmp(s1.ptr, s2.ptr, s2.len) < 0);
}

int operator>=(const string &s1, const string &s2)
{
  return (s1.len >= s2.len
	  ? s2.len == 0 || memcmp(s1.ptr, s2.ptr, s2.len) >= 0
	  : s1.len != 0 && memcmp(s1.ptr, s2.ptr, s1.len) > 0);
}

int operator>(const string &s1, const string &s2)
{
  return (s1.len > s2.len
	  ? s2.len == 0 || memcmp(s1.ptr, s2.ptr, s2.len) >= 0
	  : s1.len != 0 && memcmp(s1.ptr, s2.ptr, s1.len) > 0);
}

void string::set_length(int i)
{
  assert(i >= 0);
  if (i > sz)
    ptr = srealloc(ptr, sz, len, i, &sz);
  len = i;
}

void string::clear()
{
  len = 0;
}

int string::search(char c) const
{
  char *p = (char *)memchr(ptr, c, len);
  return p ? p - ptr : -1;
}

// we silently strip nuls

char *string::extract() const
{
  char *p = ptr;
  int n = len;
  int nnuls = 0;
  for (int i = 0; i < n; i++)
    if (p[i] == '\0')
      nnuls++;
  char *q = new char[n + 1 - nnuls];
  char *r = q;
  for (i = 0; i < n; i++)
    if (p[i] != '\0')
      *r++ = p[i];
  q[n] = '\0';
  return q;
}

void put_string(const string &s, FILE *fp)
{
  int len = s.length();
  const char *ptr = s.contents();
  for (int i = 0; i < len; i++)
    putc(ptr[i], fp);
}

string as_string(int i)
{
  static char buf[INT_DIGITS + 2];
  sprintf(buf, "%d", i);
  return string(buf);
}

