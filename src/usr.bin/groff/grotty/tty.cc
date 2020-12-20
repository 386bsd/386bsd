// -*- C++ -*-
/* Copyright (C) 1989, 1990, 1991, 1992 Free Software Foundation, Inc.
     Written by James Clark (jjc@jclark.com)

This file is part of groff.

groff is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

groff is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with groff; see the file COPYING.  If not, write to the Free Software
Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. */

#include "driver.h"

#ifndef USHRT_MAX
#define USHRT_MAX 65535
#endif

#define TAB_WIDTH 8

static int horizontal_tab_flag = 0;
static int form_feed_flag = 0;
static int bold_flag = 1;
static int underline_flag = 1;
static int overstrike_flag = 1;
static int draw_flag = 1;

enum {
  UNDERLINE_MODE = 01,
  BOLD_MODE = 02,
  VDRAW_MODE = 04,
  HDRAW_MODE = 010
};

// Mode to use for bold-underlining.
static unsigned char bold_underline_mode = BOLD_MODE|UNDERLINE_MODE;

class tty_font : public font {
  tty_font(const char *);
  unsigned char mode;
public:
  ~tty_font();
  unsigned char get_mode() { return mode; }
#if 0
  void handle_x_command(int argc, const char **argv);
#endif
  static tty_font *load_tty_font(const char *);
};

tty_font *tty_font::load_tty_font(const char *s)
{
  tty_font *f = new tty_font(s);
  if (!f->load()) {
    delete f;
    return 0;
  }
  const char *num = f->get_internal_name();
  long n;
  if (num != 0 && (n = strtol(num, 0, 0)) != 0)
    f->mode = int(n & (BOLD_MODE|UNDERLINE_MODE));
  if (!underline_flag)
    f->mode &= ~UNDERLINE_MODE;
  if (!bold_flag)
    f->mode &= ~BOLD_MODE;
  if ((f->mode & (BOLD_MODE|UNDERLINE_MODE)) == (BOLD_MODE|UNDERLINE_MODE))
    f->mode = (f->mode & ~(BOLD_MODE|UNDERLINE_MODE)) | bold_underline_mode;
  return f;
}

tty_font::tty_font(const char *nm)
: font(nm), mode(0)
{
}

tty_font::~tty_font()
{
}

#if 0
void tty_font::handle_x_command(int argc, const char **argv)
{
  if (argc >= 1 && strcmp(argv[0], "bold") == 0)
    mode |= BOLD_MODE;
  else if (argc >= 1 && strcmp(argv[0], "underline") == 0)
    mode |= UNDERLINE_MODE;
}
#endif

class glyph {
  static glyph *free_list;
public:
  glyph *next;
  unsigned short hpos;
  unsigned char code;
  unsigned char mode;
  void *operator new(size_t);
  void operator delete(void *);
  inline int draw_mode() { return mode & (VDRAW_MODE|HDRAW_MODE); }
};

glyph *glyph::free_list = 0;

void *glyph::operator new(size_t)
{
  if (!free_list) {
    const int BLOCK = 1024;
    free_list = (glyph *)new char[sizeof(glyph)*BLOCK];
    for (int i = 0; i < BLOCK - 1; i++)
      free_list[i].next = free_list + i + 1;
    free_list[BLOCK - 1].next = 0;
  }
  glyph *p = free_list;
  free_list = free_list->next;
  p->next = 0;
  return p;
}

void glyph::operator delete(void *p)
{
  if (p) {
    ((glyph *)p)->next = free_list;
    free_list = (glyph *)p;
  }
}

class tty_printer : public printer {
  glyph **lines;
  int nlines;
  int cached_v;
  int cached_vpos;
  void add_char(unsigned char, int, int, unsigned char);
public:
  tty_printer();
  ~tty_printer();
  void set_char(int, font *, const environment *, int);
  void draw(int code, int *p, int np, const environment *env);
  void begin_page(int) { }
  void end_page(int page_length);
  font *make_font(const char *);
};

tty_printer::tty_printer() : cached_v(0)
{
  nlines = 66;
  lines = new glyph *[nlines];
  for (int i = 0; i < nlines; i++)
    lines[i] = 0;
}

tty_printer::~tty_printer()
{
  a_delete lines;
}

void tty_printer::set_char(int i, font *f, const environment *env, int w)
{
  if (w != font::hor)
    fatal("width of character not equal to horizontal resolution");
  add_char(f->get_code(i), env->hpos, env->vpos, ((tty_font *)f)->get_mode());
}

void tty_printer::add_char(unsigned char c, int h, int v, unsigned char mode)
{
#if 0
  // This is too expensive.
  if (h % font::hor != 0)
    fatal("horizontal position not a multiple of horizontal resolution");
#endif
  int hpos = h / font::hor;
  if (hpos < 0) {
    error("character to the left of first column discarded");
    return;
  }
  if (hpos > USHRT_MAX) {
    error("character with ridiculously large horizontal position discarded");
    return;
  }
  int vpos;
  if (v == cached_v && cached_v != 0)
    vpos = cached_vpos;
  else {
    if (v % font::vert != 0)
      fatal("vertical position not a multiple of vertical resolution");
    vpos = v / font::vert;
    if (vpos > nlines) {
      glyph **old_lines = lines;
      lines = new glyph *[vpos + 1];
      memcpy(lines, old_lines, nlines*sizeof(glyph *));
      for (int i = nlines; i <= vpos; i++)
	lines[i] = 0;
      a_delete old_lines;
      nlines = vpos + 1;
    }
    // Note that the first output line corresponds to groff
    // position font::vert.
    if (vpos <= 0) {
      error("character above first line discarded");
      return;
    }
    cached_v = v;
    cached_vpos = vpos;
  }
  glyph *g = new glyph;
  g->hpos = hpos;
  g->code = c;
  g->mode = mode;

  // The list will be reversed later.  After reversal, it must be in
  // increasing order of hpos, with HDRAW characters before VDRAW
  // characters before normal characters at each hpos, and otherwise
  // in order of occurrence.

  for (glyph **pp = lines + (vpos - 1); *pp; pp = &(*pp)->next)
    if (int((*pp)->hpos) < hpos
	|| ((*pp)->hpos == hpos && (*pp)->draw_mode() >= g->draw_mode()))
      break;

  g->next = *pp;
  *pp = g;
}

void tty_printer::draw(int code, int *p, int np, const environment *env)
{
  if (code != 'l' || !draw_flag)
    return;
  if (np != 2) {
    error("2 arguments required for line");
    return;
  }
  if (p[0] == 0) {
    // vertical line
    int v = env->vpos;
    int len = p[1];
    if (len < 0) {
      v += len;
      len = -len;
    }
    while (len >= 0) {
      add_char('|', env->hpos, v, VDRAW_MODE);
      len -= font::vert;
      v += font::vert;
    }
  }
  if (p[1] == 0) {
    // horizontal line
    int h = env->hpos;
    int len = p[0];
    if (len < 0) {
      h += len;
      len = -len;
    }
    while (len >= 0) {
      add_char('-', h, env->vpos, HDRAW_MODE);
      len -= font::hor;
      h += font::hor;
    }
  }
}

void tty_printer::end_page(int page_length)
{
  if (page_length % font::vert != 0)
    error("vertical position at end of page not multiple of vertical resolution");
  int lines_per_page = page_length / font::vert;
  for (int last_line = nlines; last_line > 0; last_line--)
    if (lines[last_line - 1])
      break;
#if 0
  if (last_line > lines_per_page) {
    error("characters past last line discarded");
    do {
      --last_line;
      while (lines[last_line]) {
	glyph *tem = lines[last_line];
	lines[last_line] = tem->next;
	delete tem;
      }
    } while (last_line > lines_per_page);
  }
#endif
  for (int i = 0; i < last_line; i++) {
    glyph *p = lines[i];
    lines[i] = 0;
    glyph *g = 0;
    while (p) {
      glyph *tem = p->next;
      p->next = g;
      g = p;
      p = tem;
    }
    int hpos = 0;
    
    glyph *nextp;
    for (p = g; p; delete p, p = nextp) {
      nextp = p->next;
      if (nextp && p->hpos == nextp->hpos) {
	if (p->draw_mode() == HDRAW_MODE && nextp->draw_mode() == VDRAW_MODE) {
	  nextp->code = '+';
	  continue;
	}
	if (p->draw_mode() != 0 && p->draw_mode() == nextp->draw_mode()) {
	  nextp->code = p->code;
	  continue;
	}
	if (!overstrike_flag)
	  continue;
      }
      if (hpos > int(p->hpos)) {
	putchar('\b');
	hpos--;
      }
      else {
	if (horizontal_tab_flag) {
	  for (;;) {
	    int next_tab_pos = ((hpos + TAB_WIDTH) / TAB_WIDTH) * TAB_WIDTH;
	    if (next_tab_pos > int(p->hpos))
	      break;
	    putchar('\t');
	    hpos = next_tab_pos;
	  }
	}
	for (; hpos < int(p->hpos); hpos++)
	  putchar(' ');
      }
      assert(hpos == p->hpos);
      if (p->mode & UNDERLINE_MODE) {
	putchar('_');
	putchar('\b');
      }
      if (p->mode & BOLD_MODE) {
	putchar(p->code);
	putchar('\b');
      }
      putchar(p->code);
      hpos++;
    }
    putchar('\n');
  }
  if (form_feed_flag) {
    if (last_line < lines_per_page)
      putchar('\f');
  }
  else {
    for (; last_line < lines_per_page; last_line++)
      putchar('\n');
  }
}

font *tty_printer::make_font(const char *nm)
{
  return tty_font::load_tty_font(nm);
}

printer *make_printer()
{
  return new tty_printer;
}

static void usage();

int main(int argc, char **argv)
{
  program_name = argv[0];
  static char stderr_buf[BUFSIZ];
  setbuf(stderr, stderr_buf);
  int c;
  while ((c = getopt(argc, argv, "F:vhfbuoBUd")) != EOF)
    switch(c) {
    case 'v':
      {
	extern const char *version_string;
	fprintf(stderr, "grotty version %s\n", version_string);
	fflush(stderr);
	break;
      }
    case 'b':
      // Do not embolden by overstriking.
      bold_flag = 0;
      break;
    case 'u':
      // Do not underline.
      underline_flag = 0;
      break;
    case 'o':
      // Do not overstrike (other than emboldening and underlining).
      overstrike_flag = 0;
      break;
    case 'B':
      // Do bold-underlining as bold.
      bold_underline_mode = BOLD_MODE;
      break;
    case 'U':
      // Do bold-underlining as underlining.
      bold_underline_mode = UNDERLINE_MODE;
      break;
    case 'h':
      // Use horizontal tabs.
      horizontal_tab_flag = 1;
      break;
    case 'f':
      form_feed_flag = 1;
      break;
    case 'F':
      font::command_line_font_dir(optarg);
      break;
    case 'd':
      // Ignore \D commands.
      draw_flag = 0;
      break;
    case '?':
      usage();
      break;
    default:
      assert(0);
    }
  if (optind >= argc)
    do_file("-");
  else {
    for (int i = optind; i < argc; i++)
      do_file(argv[i]);
  }
  delete pr;
  exit(0);
}

static void usage()
{
  fprintf(stderr, "usage: %s [-hfvbuodBU] [-F dir] [files ...]\n",
	  program_name);
  exit(1);
}
