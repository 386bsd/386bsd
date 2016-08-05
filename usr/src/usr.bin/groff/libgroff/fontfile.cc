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

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include "font.h"
#include "lib.h"
#include "searchpath.h"
#include "device.h"

const char *const FONT_ENV_VAR = "GROFF_FONT_PATH";

static search_path font_path(FONT_ENV_VAR, FONTPATH);

int font::res = 0;
int font::hor = 1;
int font::vert = 1;
int font::unitwidth = 0;
int font::paperwidth = 0;
int font::paperlength = 0;
int font::biggestfont = 0;
int font::spare2 = 0;
int font::sizescale = 1;
int font::tcommand = 0;
const char **font::font_name_table = 0;
int *font::sizes = 0;
const char *font::family = 0;
const char **font::style_table = 0;
FONT_COMMAND_HANDLER font::unknown_desc_command_handler = 0;

void font::command_line_font_dir(const char *dir)
{
  font_path.command_line_dir(dir);
}

FILE *font::open_file(const char *name, char **pathp)
{
  char *filename = new char[strlen(name) + strlen(device) + 5];
  sprintf(filename, "dev%s/%s", device, name);
  FILE *fp = font_path.open_file(filename, pathp);
  a_delete filename;
  return fp;
}
