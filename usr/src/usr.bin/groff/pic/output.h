// -*- C++ -*-
/* Copyright (C) 1989, 1990 Free Software Foundation, Inc.
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

struct line_type {
  enum { invisible, solid, dotted, dashed } type;
  double dash_width;
  double thickness;		// the thickness is in points

  line_type();
};


class output {
protected:
  char *args;
  double desired_height;	// zero if no height specified
  double desired_width;		// zero if no depth specified
  double compute_scale(double, const position &, const position &);
public:
  output();
  virtual ~output();
  void set_desired_width_height(double wid, double ht);
  void set_args(const char *);
  virtual void start_picture(double sc, const position &ll, const position &ur) = 0;
  virtual void finish_picture() = 0;
  virtual void circle(const position &, double rad,
		      const line_type &, double) = 0;
  virtual void text(const position &, text_piece *, int, double) = 0;
  virtual void line(const position &, const position *, int n,
		    const line_type &) = 0;
  virtual void polygon(const position *, int n,
		       const line_type &, double) = 0;
  virtual void spline(const position &, const position *, int n,
		      const line_type &) = 0;
  virtual void arc(const position &, const position &, const position &,
		   const line_type &) = 0;
  virtual void ellipse(const position &, const distance &,
		       const line_type &, double) = 0;
  virtual void rounded_box(const position &, const distance &, double,
			   const line_type &, double) = 0;
  virtual void command(const char *, const char *, int);
  virtual void set_location(const char *, int);
  virtual int supports_filled_polygons();
  virtual void begin_block(const position &ll, const position &ur);
  virtual void end_block();
};

extern output *out;

output *make_troff_output();
output *make_grops_output();
output *make_tex_output();
output *make_tpic_output();
output *make_fig_output();
