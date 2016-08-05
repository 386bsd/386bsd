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

#include "groff.h"
#include "symbol.h"
#include "dictionary.h"
#include "hvunits.h"
#include "env.h"
#include "request.h"
#include "node.h"
#include "token.h"
#include "div.h"
#include "reg.h"
#include "charinfo.h"
#include <math.h>

symbol default_family("T");

enum { ADJUST_LEFT = 0, ADJUST_BOTH = 1, ADJUST_CENTER = 3, ADJUST_RIGHT = 5 };

enum { HYPHEN_LAST_LINE = 2, HYPHEN_LAST_CHARS = 4, HYPHEN_FIRST_CHARS = 8 };

struct env_list {
  environment *env;
  env_list *next;
  env_list(environment *e, env_list *p) : env(e), next(p) {}
};

env_list *env_stack;
const int NENVIRONMENTS = 10;
environment *env_table[NENVIRONMENTS];
dictionary env_dictionary(10);
environment *curenv;
static int next_line_number = 0;

charinfo *field_delimiter_char;
charinfo *padding_indicator_char;



class pending_output_line {
  node *nd;
  int no_fill;
  vunits vs;
  int ls;
  hunits width;
#ifdef WIDOW_CONTROL
  int last_line;		// Is it the last line of the paragraph?
#endif /* WIDOW_CONTROL */
public:
  pending_output_line *next;

  pending_output_line(node *, int, vunits, int, hunits,
		      pending_output_line * = 0);
  ~pending_output_line();
  int output();

#ifdef WIDOW_CONTROL
  friend void environment::mark_last_line();
  friend void environment::output(node *, int, vunits, int, hunits);
#endif /* WIDOW_CONTROL */
};

pending_output_line::pending_output_line(node *n, int nf, vunits v, int l,
					 hunits w, pending_output_line *p)
: nd(n), no_fill(nf), vs(v), ls(l), width(w),
#ifdef WIDOW_CONTROL
  last_line(0),
#endif /* WIDOW_CONTROL */
  next(p)
{
}

pending_output_line::~pending_output_line()
{
  delete_node_list(nd);
}

int pending_output_line::output()
{
  if (trap_sprung_flag)
    return 0;
#ifdef WIDOW_CONTROL
  if (next && next->last_line && !no_fill) {
    curdiv->need(vs*ls + vunits(vresolution));
    if (trap_sprung_flag) {
      next->last_line = 0;	// Try to avoid infinite loops.
      return 0;
    }
  }
#endif
  curdiv->output(nd, no_fill, vs, ls, width);
  nd = 0;
  return 1;
}

void environment::output(node *nd, int no_fill, vunits vs, int ls,
			 hunits width)
{
#ifdef WIDOW_CONTROL
  while (pending_lines) {
    if (widow_control && !pending_lines->no_fill && !pending_lines->next)
      break;
    if (!pending_lines->output())
      break;
    pending_output_line *tem = pending_lines;
    pending_lines = pending_lines->next;
    delete tem;
  }
#else /* WIDOW_CONTROL */
  output_pending_lines();
#endif /* WIDOW_CONTROL */
  if (!trap_sprung_flag && !pending_lines
#ifdef WIDOW_CONTROL
      && (!widow_control || no_fill)
#endif /* WIDOW_CONTROL */
      )
    curdiv->output(nd, no_fill, vs, ls, width);
  else {
    for (pending_output_line **p = &pending_lines; *p; p = &(*p)->next)
      ;
    *p = new pending_output_line(nd, no_fill, vs, ls, width);
  }
}

// a line from .tl goes at the head of the queue

void environment::output_title(node *nd, int no_fill, vunits vs, int ls,
			       hunits width)
{
  if (!trap_sprung_flag)
    curdiv->output(nd, no_fill, vs, ls, width);
  else
    pending_lines = new pending_output_line(nd, no_fill, vs, ls, width,
					    pending_lines);
}

void environment::output_pending_lines()
{
  while (pending_lines && pending_lines->output()) {
    pending_output_line *tem = pending_lines;
    pending_lines = pending_lines->next;
    delete tem;
  }
}

#ifdef WIDOW_CONTROL

void environment::mark_last_line()
{
  if (!widow_control || !pending_lines)
    return;
  for (pending_output_line *p = pending_lines; p->next; p = p->next)
    ;
  if (!p->no_fill)
    p->last_line = 1;
}

void widow_control_request()
{
  if (has_arg()) {
    int n;
    if (get_integer(&n))
      curenv->widow_control = n != 0;
  }
  else
    curenv->widow_control = 1;
  skip_line();
}

#endif /* WIDOW_CONTROL */

/* font_size functions */

size_range *font_size::size_table = 0;
int font_size::nranges = 0;

static int compare_ranges(void *p1, void *p2)
{
  return ((size_range *)p1)->min - ((size_range *)p2)->min;
}

void font_size::init_size_table(int *sizes)
{
  nranges = 0;
  while (sizes[nranges*2] != 0)
    nranges++;
  assert(nranges > 0);
  size_table = new size_range[nranges];
  for (int i = 0; i < nranges; i++) {
    size_table[i].min = sizes[i*2];
    size_table[i].max = sizes[i*2 + 1];
  }
  qsort(size_table, nranges, sizeof(size_range), compare_ranges);
}

font_size::font_size(int sp)
{
  for (int i = 0; i < nranges; i++) {
    if (sp < size_table[i].min) {
      if (i > 0 && size_table[i].min - sp >= sp - size_table[i - 1].max)
	p = size_table[i - 1].max;
      else
	p = size_table[i].min;
      return;
    }
    if (sp <= size_table[i].max) {
      p = sp;
      return;
    }
  }
  p = size_table[nranges - 1].max;
}

int font_size::to_units()
{
  return scale(p, units_per_inch, sizescale*72);
}

// we can't do this in a static constructor because various dictionaries
// have to get initialized first

void init_environments()
{
  curenv = env_table[0] = new environment("0");
}

void tab_character()
{
  curenv->tab_char = get_optional_char();
  skip_line();
}

void leader_character()
{
  curenv->leader_char = get_optional_char();
  skip_line();
}

void environment::add_char(charinfo *ci)
{
  if (interrupted)
    ;
  // don't allow fields in dummy environments
  else if (ci == field_delimiter_char && !dummy) {
    if (current_field)
      wrap_up_field();
    else
      start_field();
  }
  else if (current_field && ci == padding_indicator_char) {
    if (current_tab) {
      tab_contents = new space_node(H0, tab_contents);
      tab_field_spaces++;
    }
    else {
      line = new space_node(H0, line);
      field_spaces++;
    }
  }
  else if (current_tab) {
    if (tab_contents == 0)
      tab_contents = new line_start_node;
    if (ci != hyphen_indicator_char)
      tab_contents = tab_contents->add_char(ci, this, &tab_width);
    else
      tab_contents = tab_contents->add_discretionary_hyphen();
  }
  else {
    if (line == 0)
      start_line();
    if (ci != hyphen_indicator_char)
      line = line->add_char(ci, this, &width_total);
    else
      line = line->add_discretionary_hyphen();
  }
}

node *environment::make_char_node(charinfo *ci)
{
  return make_node(ci, this);
}

void environment::add_node(node *n)
{
  assert(n != 0);
  if (current_tab || current_field)
    n->freeze_space();
  if (interrupted) {
    delete n;
  }
  else if (current_tab) {
    n->next = tab_contents;
    tab_contents = n;
    tab_width += n->width();
  }
  else {
    if (line == 0) {
      if (discarding && n->discardable()) {
	delete n;
	return;
      }
      start_line();
    }
    width_total += n->width();
    space_total += n->nspaces();
    n->next = line;
    line = n;
  }
}


void environment::add_hyphen_indicator()
{
  if (current_tab || interrupted || current_field
      || hyphen_indicator_char != 0)
    return;
  if (line == 0)
    start_line();
  line = line->add_discretionary_hyphen();
}

int environment::get_hyphenation_flags()
{
  return hyphenation_flags;
}

int environment::get_hyphen_line_max()
{
  return hyphen_line_max;
}

int environment::get_hyphen_line_count()
{
  return hyphen_line_count;
}

int environment::get_center_lines()
{
  return center_lines;
}

int environment::get_right_justify_lines()
{
  return right_justify_lines;
}

void environment::add_italic_correction()
{
  if (current_tab) {
    if (tab_contents)
      tab_contents = tab_contents->add_italic_correction(&tab_width);
  }
  else if (line)
    line = line->add_italic_correction(&width_total);
}

int environment::get_space_size()
{
  return space_size;
}

int environment::get_sentence_space_size()
{
  return sentence_space_size;
}

hunits environment::get_space_width()
{ 
  return scale(env_space_width(this), space_size, 12);
}

hunits environment::get_narrow_space_width()
{
  return env_narrow_space_width(this);
}

hunits environment::get_half_narrow_space_width()
{
  return env_half_narrow_space_width(this);
}

void environment::space_newline()
{
  assert(!current_tab && !current_field);
  if (interrupted)
    return;
  int ss = space_size;
  if (node_list_ends_sentence(line) == 1)
    ss += sentence_space_size;
  hunits x = scale(env_space_width(this), ss, 12);
  if (line != 0 && line->merge_space(x)) {
    width_total += x;
    return;
  }
  add_node(new word_space_node(x));
  possibly_break_line(spread_flag);
  spread_flag = 0;
}

void environment::space()
{
  if (interrupted)
    return;
  if (current_field && padding_indicator_char == 0) {
    node **pp = current_tab ? &tab_contents : &line;
    int *hp = current_tab ? &tab_field_spaces : &field_spaces;
    *pp = new space_node(H0, *pp);
    *hp += 1;
    return;
  }
  hunits x = scale(env_space_width(this), space_size, 12);
  node *p = current_tab ? tab_contents : line;
  hunits *tp = current_tab ? &tab_width : &width_total;
  if (p && p->nspaces() == 1 && p->width() == x
      && node_list_ends_sentence(p->next) == 1) {
    hunits xx = scale(env_space_width(this), sentence_space_size, 12);
    if (p->merge_space(xx)) {
      *tp += xx;
      return;
    }
  }
  if (p && p->merge_space(x)) {
    *tp += x;
    return;
  }
  add_node(new word_space_node(x));
  possibly_break_line(spread_flag);
  spread_flag = 0;
}

void environment::set_font(symbol nm)
{
  if (interrupted)
    return;
  if (nm == symbol("P")) {
    if (family->make_definite(prev_fontno) < 0)
      return;
    int tem = fontno;
    fontno = prev_fontno;
    prev_fontno = tem;
  }
  else {
    int n = symbol_fontno(nm);
    if (n < 0) {
      n = next_available_font_position();
      if (!mount_font(n, nm))
	return;
    }
    if (family->make_definite(n) < 0)
      return;
    prev_fontno = fontno;
    fontno = n;
  }
}

void environment::set_font(int n)
{
  if (interrupted)
    return;
  if (is_good_fontno(n)) {
    prev_fontno = fontno;
    fontno = n;
  }
  else
    error("bad font number");
}

void environment::set_family(symbol fam)
{
  if (fam.is_null()) {
    if (prev_family->make_definite(fontno) < 0)
      return;
    font_family *tem = family;
    family = prev_family;
    prev_family = tem;
  }
  else {
    font_family *f = lookup_family(fam);
    if (f->make_definite(fontno) < 0)
      return;
    prev_family = family;
    family = f;
  }
}

void environment::set_size(int n)
{
  if (interrupted)
    return;
  if (n == 0) {
    font_size temp = prev_size;
    prev_size = size;
    size = temp;
    int temp2 = prev_requested_size;
    prev_requested_size = requested_size;
    requested_size = temp2;
  }
  else {
    prev_size = size;
    size = font_size(n);
    prev_requested_size = requested_size;
    requested_size = n;
  }
}

void environment::set_char_height(int n)
{
  if (interrupted)
    return;
  if (n == requested_size || n <= 0)
    char_height = 0;
  else
    char_height = n;
}

void environment::set_char_slant(int n)
{
  if (interrupted)
    return;
  char_slant = n;
}

environment::environment(symbol nm)
: name(nm),
  prev_line_length((units_per_inch*13)/2),
  line_length((units_per_inch*13)/2),
  prev_title_length((units_per_inch*13)/2),
  title_length((units_per_inch*13)/2),
  prev_size(sizescale*10),
  size(sizescale*10),
  requested_size(sizescale*10),
  prev_requested_size(sizescale*10),
  char_height(0),
  char_slant(0),
  space_size(12),
  sentence_space_size(12),
  adjust_mode(ADJUST_BOTH),
  fill(1),
  interrupted(0),
  prev_line_interrupted(0),
  center_lines(0),
  right_justify_lines(0),
  prev_vertical_spacing(points_to_units(12)),
  vertical_spacing(points_to_units(12)),
  prev_line_spacing(1),
  line_spacing(1),
  prev_indent(0),
  indent(0),
  have_temporary_indent(0),
  temporary_indent(0),
  underline_lines(0),
  input_trap_count(0),
  prev_text_length(0),
  width_total(0),
  space_total(0),
  input_line_start(0),
  control_char('.'),
  no_break_control_char('\''),
  hyphen_indicator_char(0),
  spread_flag(0),
  line(0),
  pending_lines(0),
  discarding(0),
  tabs(units_per_inch/2, TAB_LEFT),
  current_tab(TAB_NONE),
  current_field(0),
  margin_character_node(0),
  margin_character_distance(points_to_units(10)),
  numbering_nodes(0),
  number_text_separation(1),
  line_number_multiple(1),
  line_number_indent(0),
  no_number_count(0),
  tab_char(0),
  leader_char(charset_table['.']),
  hyphenation_flags(1),
  dummy(0),
  leader_node(0),
#ifdef WIDOW_CONTROL
  widow_control(0),
#endif /* WIDOW_CONTROL */
  hyphen_line_count(0),
  hyphen_line_max(-1),
  hyphenation_space(H0),
  hyphenation_margin(H0)
{
  prev_family = family = lookup_family(default_family);
  prev_fontno = fontno = 1;
  if (!is_good_fontno(1))
    fatal("font number 1 not a valid font");
  if (family->make_definite(1) < 0)
    fatal("invalid default family `%1'", default_family.contents());
  prev_fontno = fontno;
}

environment::environment(const environment *e)
: name(e->name),		// so that eg `.if "\n[.ev]"0"' works
  prev_line_length(e->prev_line_length),
  line_length(e->line_length),
  prev_title_length(e->prev_title_length),
  title_length(e->title_length),
  prev_size(e->prev_size),
  size(e->size),
  prev_requested_size(e->prev_requested_size),
  requested_size(e->requested_size),
  char_height(e->char_height),
  char_slant(e->char_slant),
  space_size(e->space_size),
  sentence_space_size(e->sentence_space_size),
  adjust_mode(e->adjust_mode),
  fill(e->fill),
  interrupted(0),
  prev_line_interrupted(0),
  center_lines(0),
  right_justify_lines(0),
  prev_vertical_spacing(e->prev_vertical_spacing),
  vertical_spacing(e->vertical_spacing),
  prev_line_spacing(e->prev_line_spacing),
  line_spacing(e->line_spacing),
  prev_indent(e->prev_indent),
  indent(e->indent),
  have_temporary_indent(0),
  temporary_indent(0),
  underline_lines(0),
  input_trap_count(0),
  prev_text_length(e->prev_text_length),
  width_total(0),
  space_total(0),
  input_line_start(0),
  control_char(e->control_char),
  no_break_control_char(e->no_break_control_char),
  hyphen_indicator_char(e->hyphen_indicator_char),
  spread_flag(0),
  line(0),
  pending_lines(0),
  discarding(0),
  tabs(e->tabs),
  current_tab(TAB_NONE),
  current_field(0),
  margin_character_node(e->margin_character_node),
  margin_character_distance(e->margin_character_distance),
  numbering_nodes(0),
  number_text_separation(e->number_text_separation),
  line_number_multiple(e->line_number_multiple),
  line_number_indent(e->line_number_indent),
  no_number_count(e->no_number_count),
  tab_char(e->tab_char),
  leader_char(e->leader_char),
  hyphenation_flags(e->hyphenation_flags),
  fontno(e->fontno),
  prev_fontno(e->prev_fontno),
  dummy(1),
  family(e->family),
  prev_family(e->prev_family),
  leader_node(0),
#ifdef WIDOW_CONTROL
  widow_control(e->widow_control),
#endif /* WIDOW_CONTROL */
  hyphen_line_max(e->hyphen_line_max),
  hyphen_line_count(0),
  hyphenation_space(e->hyphenation_space),
  hyphenation_margin(e->hyphenation_margin)
{
}

environment::~environment()
{
  delete leader_node;
  delete_node_list(line);
  delete_node_list(numbering_nodes);
}

hunits environment::get_input_line_position()
{
  hunits n;
  if (line == 0)
    n = -input_line_start;
  else
    n = width_total - input_line_start;
  if (current_tab)
    n += tab_width;
  return n;
}

void environment::set_input_line_position(hunits n)
{
  input_line_start = line == 0 ? -n : width_total - n;
  if (current_tab)
    input_line_start += tab_width;
}

hunits environment::get_line_length()
{
  return line_length;
}

hunits environment::get_saved_line_length()
{
  if (line)
    return target_text_length + saved_indent;
  else
    return line_length;
}

vunits environment::get_vertical_spacing()
{
  return vertical_spacing;
}

int environment::get_line_spacing()
{
  return line_spacing;
}

int environment::get_bold()
{
  return get_bold_fontno(fontno);
}

hunits environment::get_digit_width()
{
  return env_digit_width(this);
} 

int environment::get_adjust_mode()
{
  return adjust_mode;
}

int environment::get_fill()
{
  return fill;
}

hunits environment::get_indent()
{
  return indent;
}

hunits environment::get_saved_indent()
{
  if (line)
    return saved_indent;
  else if (have_temporary_indent)
    return temporary_indent;
  else
    return indent;
}

hunits environment::get_temporary_indent()
{
  return temporary_indent;
}

hunits environment::get_title_length()
{
  return title_length;
}

node *environment::get_prev_char()
{
  for (node *n = current_tab ? tab_contents : line; n; n = n->next) {
    node *last = n->last_char_node();
    if (last)
      return last;
  }
  return 0;
}

hunits environment::get_prev_char_width()
{
  node *last = get_prev_char();
  if (!last)
    return H0;
  return last->width();
}

hunits environment::get_prev_char_skew()
{
  node *last = get_prev_char();
  if (!last)
    return H0;
  return last->skew();
}

vunits environment::get_prev_char_height()
{
  node *last = get_prev_char();
  if (!last)
    return V0;
  vunits min, max;
  last->vertical_extent(&min, &max);
  return -min;
}

vunits environment::get_prev_char_depth()
{
  node *last = get_prev_char();
  if (!last)
    return V0;
  vunits min, max;
  last->vertical_extent(&min, &max);
  return max;
}

hunits environment::get_text_length()
{
  hunits n = line == 0 ? H0 : width_total;
  if (current_tab)
    n += tab_width;
  return n;
}

hunits environment::get_prev_text_length()
{
  return prev_text_length;
}


static int sb_reg_contents = 0;
static int st_reg_contents = 0;
static int ct_reg_contents = 0;
static int rsb_reg_contents = 0;
static int rst_reg_contents = 0;
static int skw_reg_contents = 0;
static int ssc_reg_contents = 0;

void environment::width_registers()
{
  // this is used to implement \w; it sets the st, sb, ct registers
  vunits min = 0, max = 0, cur = 0;
  int character_type = 0;
  ssc_reg_contents = line ? line->subscript_correction().to_units() : 0;
  skw_reg_contents = line ? line->skew().to_units() : 0;
  line = reverse_node_list(line);
  vunits real_min = V0;
  vunits real_max = V0;
  vunits v1, v2;
  for (node *tem = line; tem; tem = tem->next) {
    tem->vertical_extent(&v1, &v2);
    v1 += cur;
    if (v1 < real_min)
      real_min = v1;
    v2 += cur;
    if (v2 > real_max)
      real_max = v2;
    if ((cur += tem->vertical_width()) < min)
      min = cur;
    else if (cur > max)
      max = cur;
    character_type |= tem->character_type();
  }
  line = reverse_node_list(line);
  st_reg_contents = -min.to_units();
  sb_reg_contents = -max.to_units();
  rst_reg_contents = -real_min.to_units();
  rsb_reg_contents = -real_max.to_units();
  ct_reg_contents = character_type;
}

node *environment::extract_output_line()
{
  if (current_tab)
    wrap_up_tab();
  node *n = line;
  line = 0;
  return n;
}

/* environment related requests */

void environment_switch()
{
  int pop = 0;	// 1 means pop, 2 means pop but no error message on underflow
  if (curenv->is_dummy())
    error("can't switch environments when current environment is dummy");
  else if (!has_arg())
    pop = 1;
  else {
    symbol nm;
    if (!tok.delimiter()) {
      // It looks like a number.
      int n;
      if (get_integer(&n)) {
	if (n >= 0 && n < NENVIRONMENTS) {
	  env_stack = new env_list(curenv, env_stack);
	  if (env_table[n] == 0)
	    env_table[n] = new environment(itoa(n));
	  curenv = env_table[n];
	}
	else
	  nm = itoa(n);
      }
      else
	pop = 2;
    }
    else {
      nm = get_long_name(1);
      if (nm.is_null())
	pop = 2;
    }
    if (!nm.is_null()) {
      environment *e = (environment *)env_dictionary.lookup(nm);
      if (!e) {
	e = new environment(nm);
	(void)env_dictionary.lookup(nm, e);
      }
      env_stack = new env_list(curenv, env_stack);
      curenv = e;
    }
  }
  if (pop) {
    if (env_stack == 0) {
      if (pop == 1)
	error("environment stack underflow");
    }
    else {
      curenv = env_stack->env;
      env_list *tem = env_stack;
      env_stack = env_stack->next;
      delete tem;
    }
  }
  skip_line();
}


static symbol P_symbol("P");

void font_change()
{
  symbol s = get_name();
  int is_number = 1;
  if (s.is_null() || s == P_symbol) {
    s = P_symbol;
    is_number = 0;
  }
  else {
    for (const char *p = s.contents(); p != 0 && *p != 0; p++)
      if (!csdigit(*p)) {
	is_number = 0;
	break;
      }
  }
  if (is_number)
    curenv->set_font(atoi(s.contents()));
  else
    curenv->set_font(s);
  skip_line();
}

void family_change()
{
  symbol s = get_name(1);
  if (!s.is_null())
    curenv->set_family(s);
  skip_line();
}

#if 0
void point_size()
{
  int n;
  if (has_arg()) {
    if (get_number(&n, 0, curenv->get_requested_point_size()/sizescale)) {
      n *= sizescale;
      if (n <= 0)
	n = 1;
      curenv->set_size(n);
    }
  }
  else
    curenv->set_size(0);
  skip_line();
}
#endif

void point_size()
{
  int n;
  if (has_arg()) {
    if (get_number(&n, 'z', curenv->get_requested_point_size())) {
      if (n <= 0)
	n = 1;
      curenv->set_size(n);
    }
  }
  else
    curenv->set_size(0);
  skip_line();
}

void space_size()
{
  int n;
  if (get_integer(&n)) {
    curenv->space_size = n;
    if (has_arg()) {
      if (get_integer(&n))
	curenv->sentence_space_size = n;
    }
    else
      curenv->sentence_space_size = curenv->space_size;
  }
  skip_line();
}

void fill()
{
  while (!tok.newline() && !tok.eof())
    tok.next();
  if (break_flag)
    curenv->do_break();
  curenv->fill = 1;
  tok.next();
}

void no_fill()
{
  while (!tok.newline() && !tok.eof())
    tok.next();
  if (break_flag)
    curenv->do_break();
  curenv->fill = 0;
  tok.next();
}

void center()
{
  int n;
  if (!has_arg() || !get_integer(&n))
    n = 1;
  while (!tok.newline() && !tok.eof())
    tok.next();
  if (break_flag)
    curenv->do_break();
  curenv->right_justify_lines = 0;
  curenv->center_lines = n;
  tok.next();
}

void right_justify()
{
  int n;
  if (!has_arg() || !get_integer(&n))
    n = 1;
  while (!tok.newline() && !tok.eof())
    tok.next();
  if (break_flag)
    curenv->do_break();
  curenv->center_lines = 0;
  curenv->right_justify_lines = n;
  tok.next();
}

void line_length()
{
  hunits temp;
  if (!has_arg()) {
    hunits temp = curenv->line_length;
    curenv->line_length = curenv->prev_line_length;
    curenv->prev_line_length = temp;
  }
  else if (get_hunits(&temp, 'm', curenv->line_length)) {
    if (temp < H0) {
      warning(WARN_RANGE, "bad line length %1u", temp.to_units());
      temp = H0;
    }
    curenv->prev_line_length = curenv->line_length;
    curenv->line_length = temp;
  }
  skip_line();
}

void title_length()
{
  hunits temp;
  if (!has_arg()) {
    hunits temp = curenv->title_length;
    curenv->title_length = curenv->prev_title_length;
    curenv->prev_title_length = temp;
  }
  else if (get_hunits(&temp, 'm', curenv->title_length)) {
    if (temp < H0) {
      warning(WARN_RANGE, "bad title length %1u", temp.to_units());
      temp = H0;
    }
    curenv->prev_title_length = curenv->title_length;
    curenv->title_length = temp;
  }
  skip_line();
}

void vertical_spacing()
{
  if (!has_arg()) {
    vunits temp = curenv->vertical_spacing;
    curenv->vertical_spacing = curenv->prev_vertical_spacing;
    curenv->prev_vertical_spacing = temp;
  }
  else {
    vunits temp;
    if (get_vunits(&temp, 'p', curenv->vertical_spacing)) {
      if (temp <= V0) {
	warning(WARN_RANGE, "vertical spacing must be greater than 0");
	temp = vresolution;
      }
      curenv->prev_vertical_spacing = curenv->vertical_spacing;
      curenv->vertical_spacing = temp;
    }
  }
  skip_line();
}

void line_spacing()
{
  int temp;
  if (!has_arg()) {
    temp = curenv->line_spacing;
    curenv->line_spacing = curenv->prev_line_spacing;
    curenv->prev_line_spacing = temp;
  }
  else if (get_integer(&temp)) {
    if (temp < 1) {
      warning(WARN_RANGE, "value %1 out of range: interpreted as 1", temp);
      temp = 1;
    }
    curenv->prev_line_spacing = curenv->line_spacing;
    curenv->line_spacing = temp;
  }
  skip_line();
}

void indent()
{
  hunits temp;
  int err = 0;
  if (!has_arg())
    temp = curenv->prev_indent;
  else if (!get_hunits(&temp, 'm', curenv->indent))
    err = 1;
  while (!tok.newline() && !tok.eof())
    tok.next();
  if (break_flag)
    curenv->do_break();
  if (temp < H0) {
    warning(WARN_RANGE, "indent cannot be negative");
    temp = H0;
  }
  if (!err) {
    curenv->have_temporary_indent = 0;
    curenv->prev_indent = curenv->indent;
    curenv->indent = temp;
  }
  tok.next();
}

void temporary_indent()
{
  int err = 0;
  hunits temp;
  if (!get_hunits(&temp, 'm', curenv->get_indent()))
    err = 1;
  while (!tok.newline() && !tok.eof())
    tok.next();
  if (break_flag)
    curenv->do_break();
  if (temp < H0) {
    warning(WARN_RANGE, "total indent cannot be negative");
    temp = H0;
  }
  if (!err) {
    curenv->temporary_indent = temp;
    curenv->have_temporary_indent = 1;
  }
  tok.next();
}

void underline()
{
  int n = 0;
  if (!has_arg())
    n = 1;
  else if (!get_integer(&n))
    n = 0;
  if (n <= 0) {
    if (curenv->underline_lines > 0) {
      curenv->prev_fontno = curenv->fontno;
      curenv->fontno = curenv->pre_underline_fontno;
    }
    curenv->underline_lines = 0;
  }
  else {
    curenv->underline_lines = n;
    curenv->pre_underline_fontno = curenv->fontno;
    curenv->fontno = get_underline_fontno();
  }
  skip_line();
}


void control_char()
{
  if (!has_arg())
    curenv->control_char = '.';
  else if (tok.ch() == 0)
    error("bad control character");
  else
    curenv->control_char = tok.ch();
  skip_line();
}

void no_break_control_char()
{
  if (!has_arg())
    curenv->no_break_control_char = '\'';
  else if (tok.ch() == 0)
    error("bad control character");
  else
    curenv->no_break_control_char = tok.ch();
  skip_line();
}


void margin_character()
{
  if (curenv->margin_character_node) {
    delete curenv->margin_character_node;
    curenv->margin_character_node = 0;
  }
  charinfo *ci = get_optional_char();
  if (ci) {
    curenv->margin_character_node = curenv->make_char_node(ci);
    hunits d;
    if (curenv->margin_character_node && has_arg() && get_hunits(&d, 'm'))
      curenv->margin_character_distance = d;
  }
  skip_line();
}

void number_lines()
{
  delete_node_list(curenv->numbering_nodes);
  curenv->numbering_nodes = 0;
  int n;
  if (has_arg() && get_integer(&n, next_line_number)) {
    next_line_number = n;
    if (next_line_number < 0) {
      warning(WARN_RANGE, "negative line number");
      next_line_number = 0;
    }
    node *nd = 0;
    for (int i = '9'; i >= '0'; i--) {
      node *tem = make_node(charset_table[i], curenv);
      if (!tem) {
	skip_line();
	return;
      }
      tem->next = nd;
      nd = tem;
    }
    curenv->numbering_nodes = nd;
    curenv->line_number_digit_width = env_digit_width(curenv);
    if (has_arg()) {
      if (!tok.delimiter()) {
	if (get_integer(&n)) {
	  if (n <= 0) {
	    warning(WARN_RANGE, "negative or zero line number multiple");
	  }
	  else
	    curenv->line_number_multiple = n;
	}
      }
      else
	while (!tok.space() && !tok.newline() && !tok.eof())
	  tok.next();
      if (has_arg()) {
	if (!tok.delimiter()) {
	  if (get_integer(&n))
	    curenv->number_text_separation = n;
	}
	else
	  while (!tok.space() && !tok.newline() && !tok.eof())
	    tok.next();
	if (has_arg() && !tok.delimiter() && get_integer(&n))
	  curenv->line_number_indent = n;
      }
    }
  }
  skip_line();
}

void no_number()
{
  if (has_arg()) {
    int n;
    if (get_integer(&n))
      curenv->no_number_count = n > 0 ? n : 0;
  }
  else
    curenv->no_number_count = 1;
  skip_line();
}

void no_hyphenate()
{
  curenv->hyphenation_flags = 0;
  skip_line();
}

void hyphenate_request()
{
  int n;
  if (has_arg()) {
    if (get_integer(&n))
      curenv->hyphenation_flags = n;
  }
  else
    curenv->hyphenation_flags = 1;
  skip_line();
}

void hyphen_char()
{
  curenv->hyphen_indicator_char = get_optional_char();
  skip_line();
}

void hyphen_line_max_request()
{
  if (has_arg()) {
    int n;
    if (get_integer(&n))
      curenv->hyphen_line_max = n;
  }
  else
    curenv->hyphen_line_max = -1;
  skip_line();
}

void environment::interrupt()
{
  if (!dummy) {
    add_node(new transparent_dummy_node);
    interrupted = 1;
  }
}

void environment::newline()
{
  if (underline_lines > 0) {
    if (--underline_lines == 0) {
      prev_fontno = fontno;
      fontno = pre_underline_fontno;
    }
  }
  if (current_field)
    wrap_up_field();
  if (current_tab)
    wrap_up_tab();
  // strip trailing spaces
  while (line != 0 && line->discardable()) {
    width_total -= line->width();
    space_total -= line->nspaces();
    node *tem = line;
    line = line->next;
    delete tem;
  }
  node *to_be_output = 0;
  hunits to_be_output_width;
  prev_line_interrupted = interrupted;
  if (dummy)
    space_newline();
  else if (interrupted)
    interrupted = 0;
  else if (center_lines > 0) {
    --center_lines;
    hunits x = target_text_length - width_total;
    if (x > H0)
      saved_indent += x/2;
    to_be_output = line;
    to_be_output_width = width_total;
    line = 0;
  }
  else if (right_justify_lines > 0) {
    --right_justify_lines;
    hunits x = target_text_length - width_total;
    if (x > H0)
      saved_indent += x;
    to_be_output = line;
    to_be_output_width = width_total;
    line = 0;
  }
  else if (fill)
    space_newline();
  else {
    to_be_output = line;
    to_be_output_width = width_total;
    line = 0;
  }
  input_line_start = line == 0 ? H0 : width_total;
  if (to_be_output) {
    output_line(to_be_output, to_be_output_width);
    hyphen_line_count = 0;
  }
  if (input_trap_count > 0) {
    if (--input_trap_count == 0)
      spring_trap(input_trap);
  }
}

void environment::output_line(node *n, hunits width)
{
  prev_text_length = width;
  if (margin_character_node) {
    hunits d = line_length + margin_character_distance - saved_indent - width;
    if (d > 0) {
      n = new hmotion_node(d, n);
      width += d;
    }
    node *tem = margin_character_node->copy();
    tem->next = n;
    n = tem;
    width += tem->width();
  }
  node *nn = 0;
  while (n != 0) {
    node *tem = n->next;
    n->next = nn;
    nn = n;
    n = tem;
  }
  if (!saved_indent.is_zero())
    nn = new hmotion_node(saved_indent, nn);
  width += saved_indent;
  if (no_number_count > 0)
    --no_number_count;
  else if (numbering_nodes) {
    hunits w = (line_number_digit_width
		*(3+line_number_indent+number_text_separation));
    if (next_line_number % line_number_multiple != 0)
      nn = new hmotion_node(w, nn);
    else {
      hunits x = w;
      nn = new hmotion_node(number_text_separation*line_number_digit_width,
			    nn);
      x -= number_text_separation*line_number_digit_width;
      char buf[30];
      sprintf(buf, "%3d", next_line_number);
      for (char *p = strchr(buf, '\0') - 1; p >= buf && *p != ' '; --p) {
	node *gn = numbering_nodes;
	for (int count = *p - '0'; count > 0; count--)
	  gn = gn->next;
	gn = gn->copy();
	x -= gn->width();
	gn->next = nn;
	nn = gn;
      }
      nn = new hmotion_node(x, nn);
    }
    width += w;
    ++next_line_number;
  }
  output(nn, !fill, vertical_spacing, line_spacing, width);
}

void environment::start_line()
{
  assert(line == 0);
  discarding = 0;
  line = new line_start_node;
  if (have_temporary_indent) {
    saved_indent = temporary_indent;
    have_temporary_indent = 0;
  }
  else
    saved_indent = indent;
  target_text_length = line_length - saved_indent;
  width_total = H0;
  space_total = 0;
}

hunits environment::get_hyphenation_space()
{
  return hyphenation_space;
}

void hyphenation_space_request()
{
  hunits n;
  if (get_hunits(&n, 'm')) {
    if (n < H0) {
      warning(WARN_RANGE, "hyphenation space cannot be negative");
      n = H0;
    }
    curenv->hyphenation_space = n;
  }
  skip_line();
}

hunits environment::get_hyphenation_margin()
{
  return hyphenation_margin;
}

void hyphenation_margin_request()
{
  hunits n;
  if (get_hunits(&n, 'm')) {
    if (n < H0) {
      warning(WARN_RANGE, "hyphenation margin cannot be negative");
      n = H0;
    }
    curenv->hyphenation_margin = n;
  }
  skip_line();
}

breakpoint *environment::choose_breakpoint()
{
  hunits x = width_total;
  int s = space_total;
  node *n = line;
  breakpoint *best_bp = 0;	// the best breakpoint so far
  int best_bp_fits = 0;
  while (n != 0) {
    x -= n->width();
    s -= n->nspaces();
    breakpoint *bp = n->get_breakpoints(x, s);
    while (bp != 0) {
      if (bp->width <= target_text_length) {
	if (!bp->hyphenated) {
	  breakpoint *tem = bp->next;
	  bp->next = 0;
	  while (tem != 0) {
	    breakpoint *tem1 = tem;
	    tem = tem->next;
	    delete tem1;
	  }
	  if (best_bp_fits
	      // Decide whether to use the hyphenated breakpoint.
	      && (hyphen_line_max < 0
		  // Only choose the hyphenated breakpoint if it would not
		  // exceed the maximum number of consecutive hyphenated
		  // lines.
		  || hyphen_line_count + 1 <= hyphen_line_max)
	      && !(adjust_mode == ADJUST_BOTH
		   // Don't choose the hyphenated breakpoint if the line
		   // can be justified by adding no more than
		   // hyphenation_space to any word space.
		   ? (bp->nspaces > 0
		      && (((target_text_length - bp->width
			    + (bp->nspaces - 1)*hresolution)/bp->nspaces)
			  <= hyphenation_space))
		   // Don't choose the hyphenated breakpoint if the line
		   // is no more than hyphenation_margin short.
		   : target_text_length - bp->width <= hyphenation_margin)) {
	    delete bp;
	    return best_bp;
	  }
	  if (best_bp)
	    delete best_bp;
	  return bp;
	}
	else {
	  if ((adjust_mode == ADJUST_BOTH
	       ? hyphenation_space == H0
	       : hyphenation_margin == H0)
	      && (hyphen_line_max < 0
		  || hyphen_line_count + 1 <= hyphen_line_max)) {
	    // No need to consider a non-hyphenated breakpoint.
	    if (best_bp)
	      delete best_bp;
	    return bp;
	  }
	  // It fits but it's hyphenated.
	  if (!best_bp_fits) {
	    if (best_bp)
	      delete best_bp;
	    best_bp = bp;
	    bp = bp->next;
	    best_bp_fits = 1;
	  }
	  else {
	    breakpoint *tem = bp;
	    bp = bp->next;
	    delete tem;
	  }
	}
      }
      else {
	if (best_bp)
	  delete best_bp;
	best_bp = bp;
	bp = bp->next;
      }
    }
    n = n->next;
  }
  if (best_bp) {
    if (!best_bp_fits)
      warning(WARN_BREAK, "can't break line");
    return best_bp;
  }
  return 0;
}

void environment::hyphenate_line()
{
  if (line == 0)
    return;
  hyphenation_type prev_type = line->get_hyphenation_type();
  for (node *tem = line->next; tem != 0; tem = tem->next) {
    hyphenation_type this_type = tem->get_hyphenation_type();
    if (prev_type == HYPHEN_BOUNDARY && this_type == HYPHEN_MIDDLE)
      break;
    prev_type = this_type;
  }
  if (tem == 0)
    return;
  node *start = tem;
  int i = 0;
  do {
    ++i;
    tem = tem->next;
  } while (tem != 0 && tem->get_hyphenation_type() == HYPHEN_MIDDLE);
  int inhibit = (tem != 0 && tem->get_hyphenation_type() == HYPHEN_INHIBIT);
  node *end = tem;
  hyphen_list *sl = 0;
  tem = start;
  node *forward = 0;
  while (tem != end) {
    sl = tem->get_hyphen_list(sl);
    node *tem1 = tem;
    tem = tem->next;
    tem1->next = forward;
    forward = tem1;
  }
  // this is for characters like hyphen and emdash
  int prev_code = 0;
  for (hyphen_list *h = sl; h; h = h->next) {
    h->breakable = (prev_code != 0
		    && h->next != 0
		    && h->next->hyphenation_code != 0);
    prev_code = h->hyphenation_code;
  }
  if (hyphenation_flags != 0
      && !inhibit
      // this may not be right if we have extra space on this line
      && !((hyphenation_flags & HYPHEN_LAST_LINE)
	   && curdiv->distance_to_next_trap() <= line_spacing*vertical_spacing)
      && i >= 4)
    hyphenate(sl, hyphenation_flags);
  while (forward != 0) {
    node *tem1 = forward;
    forward = forward->next;
    tem1->next = 0;
    tem = tem1->add_self(tem, &sl);
  }
  node *new_start = tem;
  for (tem = line; tem->next != start; tem = tem->next)
    ;
  tem->next = new_start;
}

static node *node_list_reverse(node *n)
{
  node *res = 0;
  while (n) {
    node *tem = n;
    n = n->next;
    tem->next = res;
    res = tem;
  }
  return res;
}

static void distribute_space(node *n, int nspaces, hunits desired_space,
			     int force_forward = 0)
{
  static int reverse = 0;
  if (!force_forward && reverse)
    n = node_list_reverse(n);
  for (node *tem = n; tem; tem = tem->next)
    tem->spread_space(&nspaces, &desired_space);
  if (!force_forward) {
    if (reverse)
      (void)node_list_reverse(n);
    reverse = !reverse;
  }
  assert(desired_space.is_zero() && nspaces == 0);
}

void environment::possibly_break_line(int forced)
{
  if (!fill || current_tab || current_field || dummy)
    return;
  while (line != 0 && (forced || width_total > target_text_length)) {
    hyphenate_line();
    breakpoint *bp = choose_breakpoint();
    if (bp == 0)
      // we'll find one eventually
      return;
    node *pre, *post;
    bp->nd->split(bp->index, &pre, &post);
    hunits extra_space_width = H0;
    switch(adjust_mode) {
    case ADJUST_BOTH:
      if (bp->nspaces != 0)
	extra_space_width = target_text_length - bp->width;
      break;
    case ADJUST_CENTER:
      saved_indent += (target_text_length - bp->width)/2;
      break;
    case ADJUST_RIGHT:
      saved_indent += target_text_length - bp->width;
      break;
    }
    distribute_space(pre, bp->nspaces, extra_space_width);
    node *tem = line;
    line = 0;
    output_line(pre, bp->width + extra_space_width);
    line = tem;
    input_line_start -= bp->width + extra_space_width;
    if (line != bp->nd) {
      for (tem = line; tem->next != bp->nd; tem = tem->next)
	;
      tem->next = post;
    }
    else
      line = post;
    if (bp->hyphenated)
      hyphen_line_count++;
    else
      hyphen_line_count = 0;
    delete bp;
    space_total = 0;
    width_total = 0;
    node *first_non_discardable = 0;
    for (tem = line; tem != 0; tem = tem->next)
      if (!tem->discardable())
	first_non_discardable = tem;
    node *to_be_discarded;
    if (first_non_discardable) {
      to_be_discarded = first_non_discardable->next;
      first_non_discardable->next = 0;
      for (tem = line; tem != 0; tem = tem->next) {
	width_total += tem->width();
	space_total += tem->nspaces();
      }
      discarding = 0;
    }
    else {
      discarding = 1;
      to_be_discarded = line;
      line = 0;
    }
    while (to_be_discarded != 0) {
      tem = to_be_discarded;
      to_be_discarded = to_be_discarded->next;
      input_line_start -= tem->width();
      delete tem;
    }
    if (line != 0) {
      if (have_temporary_indent) {
	saved_indent = temporary_indent;
	have_temporary_indent = 0;
      }
      else
	saved_indent = indent;
      target_text_length = line_length - saved_indent;
    }
  }
}

void environment::do_break()
{
  if (curdiv == topdiv && !topdiv->first_page_begun) {
    topdiv->begin_page();
    return;
  }
  if (current_tab)
    wrap_up_tab();
  if (line) {
    line = new space_node(H0, line); // this is so that hyphenation works
    space_total++;
    possibly_break_line();
  }
  while (line != 0 && line->discardable()) {
    width_total -= line->width();
    space_total -= line->nspaces();
    node *tem = line;
    line = line->next;
    delete tem;
  }
  discarding = 0;
  input_line_start = H0;
  if (line != 0) {
    if (fill) {
      switch (adjust_mode) {
      case ADJUST_CENTER:
	saved_indent += (target_text_length - width_total)/2;
	break;
      case ADJUST_RIGHT:
	saved_indent += target_text_length - width_total;
	break;
      }
    }
    node *tem = line;
    line = 0;
    output_line(tem, width_total);
    hyphen_line_count = 0;
  }
#ifdef WIDOW_CONTROL
  mark_last_line();
  output_pending_lines();
#endif /* WIDOW_CONTROL */
}

int environment::is_empty()
{
  return !current_tab && line == 0 && pending_lines == 0;
}

void break_request()
{
  while (!tok.newline() && !tok.eof())
    tok.next();
  if (break_flag)
    curenv->do_break();
  tok.next();
}

void title()
{
  if (curdiv == topdiv && !topdiv->first_page_begun) {
    handle_initial_title();
    return;
  }
  node *part[3];
  hunits part_width[3];
  part[0] = part[1] = part[2] = 0;
  environment env(curenv);
  environment *oldenv = curenv;
  curenv = &env;
  read_title_parts(part, part_width);
  curenv = oldenv;
  curenv->size = env.size;
  curenv->prev_size = env.prev_size;
  curenv->requested_size = env.requested_size;
  curenv->prev_requested_size = env.prev_requested_size;
  curenv->char_height = env.char_height;
  curenv->char_slant = env.char_slant;
  curenv->fontno = env.fontno;
  curenv->prev_fontno = env.prev_fontno;
  node *n = 0;
  node *p = part[2];
  while (p != 0) {
    node *tem = p;
    p = p->next;
    tem->next = n;
    n = tem;
  }
  hunits title_length(curenv->title_length);
  hunits f = title_length - part_width[1];
  hunits f2 = f/2;
  n = new hmotion_node(f2 - part_width[2], n);
  p = part[1];
  while (p != 0) {
    node *tem = p;
    p = p->next;
    tem->next = n;
    n = tem;
  }
  n = new hmotion_node(f - f2 - part_width[0], n);
  p = part[0];
  while (p != 0) {
    node *tem = p;
    p = p->next;
    tem->next = n;
    n = tem;
  }
  curenv->output_title(n, !curenv->fill, curenv->vertical_spacing,
		       curenv->line_spacing, title_length);
  curenv->hyphen_line_count = 0;
  tok.next();
}  

void adjust()
{
  if (!has_arg())
    curenv->adjust_mode |= 1;
  else
    switch (tok.ch()) {
    case 'l':
      curenv->adjust_mode = ADJUST_LEFT;
      break;
    case 'r':
      curenv->adjust_mode = ADJUST_RIGHT;
      break;
    case 'c':
      curenv->adjust_mode = ADJUST_CENTER;
      break;
    case 'b':
    case 'n':
      curenv->adjust_mode = ADJUST_BOTH;
      break;
    default:
      int n;
      if (get_integer(&n)) {
	if (0 <= n && n <= 5)
	  curenv->adjust_mode = n;
	else
	  warning(WARN_RANGE, "adjustment mode `%1' out of range", n);
      }
    }
  skip_line();
}

void no_adjust()
{
  curenv->adjust_mode &= ~1;
  skip_line();
}

void input_trap()
{
  int n;
  if (!has_arg())
    curenv->input_trap_count = 0;
  else if (get_integer(&n)) {
    if (n <= 0)
      error("number of lines for input trap must be greater than zero");
    else {
      symbol s = get_name(1);
      if (!s.is_null()) {
	curenv->input_trap_count = n;
	curenv->input_trap = s;
      }
    }
  }
  skip_line();
}

/* tabs */

// must not be R or C or L or a legitimate part of a number expression
const char TAB_REPEAT_CHAR = 'T';

struct tab {
  tab *next;
  hunits pos;
  tab_type type;
  tab(hunits, tab_type);
#if 1
  enum { BLOCK = 1024 };
  static tab *free_list;
  void *operator new(size_t);
  void operator delete(void *);
#endif
};

#if 1
tab *tab::free_list = 0;

void *tab::operator new(size_t n)
{
  assert(n == sizeof(tab));
  if (!free_list) {
    free_list = (tab *)new char[sizeof(tab)*BLOCK];
    for (int i = 0; i < BLOCK - 1; i++)
      free_list[i].next = free_list + i + 1;
    free_list[BLOCK-1].next = 0;
  }
  tab *p = free_list;
  free_list = (tab *)(free_list->next);
  p->next = 0;
  return p;
}

#ifdef __GNUG__
/* cfront can't cope with this. */
inline
#endif
void tab::operator delete(void *p)
{
  if (p) {
    ((tab *)p)->next = free_list;
    free_list = (tab *)p;
  }
}

#endif

tab::tab(hunits x, tab_type t) : next(0), pos(x), type(t)
{
}

tab_stops::tab_stops(hunits distance, tab_type type) 
     : initial_list(0)
{
  repeated_list = new tab(distance, type);
}

tab_stops::~tab_stops()
{
  clear();
}

tab_type tab_stops::distance_to_next_tab(hunits curpos, hunits *distance)
{
  hunits lastpos = 0;
  for (tab *tem = initial_list; tem && tem->pos <= curpos; tem = tem->next)
    lastpos = tem->pos;
  if (tem) {
    *distance = tem->pos - curpos;
    return tem->type;
  }
  if (repeated_list == 0)
    return TAB_NONE;
  hunits base = lastpos;
  for (;;) {
    for (tem = repeated_list; tem && tem->pos + base <= curpos; tem = tem->next)
      lastpos = tem->pos;
    if (tem) {
      *distance = tem->pos + base - curpos;
      return tem->type;
    }
    assert(lastpos > 0);
    base += lastpos;
  }
  return TAB_NONE;
}

const char *tab_stops::to_string()
{
  static char *buf = 0;
  static int buf_size = 0;
  // figure out a maximum on the amount of space we can need
  int count = 0;
  for (tab *p = initial_list; p; p = p->next)
    ++count;
  for (p = repeated_list; p; p = p->next)
    ++count;
  // (10 for digits + 1 for u + 1 for 'C' or 'R') + 2 for ' &' + 1 for '\0'
  int need = count*12 + 3;
  if (buf == 0 || need > buf_size) {
    if (buf)
      delete buf;
    buf_size = need;
    buf = new char[buf_size];
  }
  char *ptr = buf;
  for (p = initial_list; p; p = p->next) {
    strcpy(ptr, itoa(p->pos.to_units()));
    ptr = strchr(ptr, '\0');
    *ptr++ = 'u';
    *ptr = '\0';
    switch (p->type) {
    case TAB_LEFT:
      break;
    case TAB_RIGHT:
      *ptr++ = 'R';
      break;
    case TAB_CENTER:
      *ptr++ = 'C';
      break;
    case TAB_NONE:
    default:
      assert(0);
    }
  }
  if (repeated_list)
    *ptr++ = TAB_REPEAT_CHAR;
  for (p = repeated_list; p; p = p->next) {
    strcpy(ptr, itoa(p->pos.to_units()));
    ptr = strchr(ptr, '\0');
    *ptr++ = 'u';
    *ptr = '\0';
    switch (p->type) {
    case TAB_LEFT:
      break;
    case TAB_RIGHT:
      *ptr++ = 'R';
      break;
    case TAB_CENTER:
      *ptr++ = 'C';
      break;
    case TAB_NONE:
    default:
      assert(0);
    }
  }
  *ptr++ = '\0';
  return buf;
}

tab_stops::tab_stops() : initial_list(0), repeated_list(0)
{
}

tab_stops::tab_stops(const tab_stops &ts) 
     : initial_list(0), repeated_list(0)
{
  tab **p = &initial_list;
  tab *t = ts.initial_list;
  while (t) {
    *p = new tab(t->pos, t->type);
    t = t->next;
    p = &(*p)->next;
  }
  p = &repeated_list;
  t = ts.repeated_list;
  while (t) {
    *p = new tab(t->pos, t->type);
    t = t->next;
    p = &(*p)->next;
  }
}

void tab_stops::clear()
{
  while (initial_list) {
    tab *tem = initial_list;
    initial_list = initial_list->next;
    delete tem;
  }
  while (repeated_list) {
    tab *tem = repeated_list;
    repeated_list = repeated_list->next;
    delete tem;
  }
}

void tab_stops::add_tab(hunits pos, tab_type type, int repeated)
{
  for (tab **p = repeated ? &repeated_list : &initial_list; *p; p = &(*p)->next)
    ;
  *p = new tab(pos, type);
}


void tab_stops::operator=(const tab_stops &ts)
{
  clear();
  tab **p = &initial_list;
  tab *t = ts.initial_list;
  while (t) {
    *p = new tab(t->pos, t->type);
    t = t->next;
    p = &(*p)->next;
  }
  p = &repeated_list;
  t = ts.repeated_list;
  while (t) {
    *p = new tab(t->pos, t->type);
    t = t->next;
    p = &(*p)->next;
  }
}
    
void set_tabs()
{
  hunits pos;
  hunits prev_pos = 0;
  int repeated = 0;
  tab_stops tabs;
  while (has_arg()) {
    if (tok.ch() == TAB_REPEAT_CHAR) {
      tok.next();
      repeated = 1;
      prev_pos = 0;
    }
    if (!get_hunits(&pos, 'm', prev_pos))
      break;
    if (pos <= prev_pos) {
      warning(WARN_RANGE, "positions of tab stops must be strictly increasing");
      continue;
    }
    tab_type type = TAB_LEFT;
    if (tok.ch() == 'C') {
      tok.next();
      type = TAB_CENTER;
    }
    else if (tok.ch() == 'R') {
      tok.next();
      type = TAB_RIGHT;
    }
    else if (tok.ch() == 'L') {
      tok.next();
    }
    tabs.add_tab(pos, type, repeated);
    prev_pos = pos;
  }
  curenv->tabs = tabs;
  skip_line();
}

const char *environment::get_tabs()
{
  return tabs.to_string();
}

#if 0
tab_stops saved_tabs;

void tabs_save()
{
  saved_tabs = curenv->tabs;
  skip_line();
}

void tabs_restore()
{
  curenv->tabs = saved_tabs;
  skip_line();
}
#endif

tab_type environment::distance_to_next_tab(hunits *distance)
{
  return curenv->tabs.distance_to_next_tab(get_input_line_position(), distance);
}

void field_characters()
{
  field_delimiter_char = get_optional_char();
  if (field_delimiter_char)
    padding_indicator_char = get_optional_char();
  else
    padding_indicator_char = 0;
  skip_line();
}

void environment::wrap_up_tab()
{
  if (!current_tab)
    return;
  if (line == 0)
    start_line();
  hunits tab_amount;
  switch (current_tab) {
  case TAB_RIGHT:
    tab_amount = tab_distance - tab_width;
    line = make_tab_node(tab_amount, line);
    break;
  case TAB_CENTER:
    tab_amount = tab_distance - tab_width/2;
    line = make_tab_node(tab_amount, line);
    break;
  case TAB_NONE:
  case TAB_LEFT:
  default:
    assert(0);
  }
  width_total += tab_amount;
  width_total += tab_width;
  if (current_field) {
    if (tab_precedes_field) {
      pre_field_width += tab_amount;
      tab_precedes_field = 0;
    }
    field_distance -= tab_amount;
  }
  field_spaces += tab_field_spaces;
  if (tab_contents != 0) {
    for (node *tem = tab_contents; tem->next != 0; tem = tem->next)
      ;
    tem->next = line;
    line = tab_contents;
  }
  tab_field_spaces = 0;
  tab_contents = 0;
  tab_width =  H0;
  tab_distance = H0;
  current_tab = TAB_NONE;
}

node *environment::make_tab_node(hunits d, node *next)
{
  if (leader_node != 0 && d < 0) {
    error("motion generated by leader cannot be negative");
    delete leader_node;
    leader_node = 0;
  }
  if (!leader_node)
    return new hmotion_node(d, next);
  node *n = new hline_node(d, leader_node, next);
  leader_node = 0;
  return n;
}

void environment::handle_tab(int is_leader)
{
  hunits d;
  if (current_tab)
    wrap_up_tab();
  charinfo *ci = is_leader ? leader_char : tab_char;
  delete leader_node;
  leader_node = ci ? make_char_node(ci) : 0;
  tab_type t = distance_to_next_tab(&d);
  switch (t) {
  case TAB_NONE:
    return;
  case TAB_LEFT:
    add_node(make_tab_node(d));
    return;
  case TAB_RIGHT:
  case TAB_CENTER:
    tab_width = 0;
    tab_distance = d;
    tab_contents = 0;
    current_tab = t;
    tab_field_spaces = 0;
    return;
  default:
    assert(0);
  }
}

void environment::start_field()
{
  assert(!current_field);
  hunits d;
  if (distance_to_next_tab(&d) != TAB_NONE) {
    pre_field_width = get_text_length();
    field_distance = d;
    current_field = 1;
    field_spaces = 0;
    tab_field_spaces = 0;
    for (node *p = line; p; p = p->next)
      p->freeze_space();
    tab_precedes_field = current_tab != TAB_NONE;
  }
  else
    error("zero field width");
}

void environment::wrap_up_field()
{
  hunits padding = field_distance - (get_text_length() - pre_field_width);
  if (current_tab && tab_field_spaces != 0) {
    hunits tab_padding = scale(padding, 
			       tab_field_spaces, 
			       field_spaces + tab_field_spaces);
    padding -= tab_padding;
    distribute_space(tab_contents, tab_field_spaces, tab_padding, 1);
    tab_field_spaces = 0;
    tab_width += tab_padding;
  }
  if (field_spaces != 0) {
    distribute_space(line, field_spaces, padding, 1);
    width_total += padding;
    if (current_tab) {
      // the start of the tab has been moved to the right by padding, so
      tab_distance -= padding;
      if (tab_distance <= H0) {
	// use the next tab stop instead
	current_tab = tabs.distance_to_next_tab(get_input_line_position()
						- tab_width,
						&tab_distance);
	if (current_tab == TAB_NONE || current_tab == TAB_LEFT) {
	  width_total += tab_width;
	  if (current_tab == TAB_LEFT) {
	    line = make_tab_node(tab_distance, line);
	    width_total += tab_distance;
	    current_tab = TAB_NONE;
	  }
	  if (tab_contents != 0) {
	    for (node *tem = tab_contents; tem->next != 0; tem = tem->next)
	      ;
	    tem->next = line;
	    line = tab_contents;
	    tab_contents = 0;
	  }
	  tab_width = H0;
	  tab_distance = H0;
	}
      }
    }
  }
  current_field = 0;
}

typedef int (environment::*INT_FUNCP)();
typedef vunits (environment::*VUNITS_FUNCP)();
typedef hunits (environment::*HUNITS_FUNCP)();
typedef const char *(environment::*STRING_FUNCP)();

class int_env_reg : public reg {
  INT_FUNCP func;
 public:
  int_env_reg(INT_FUNCP);
  const char *get_string();
  int get_value(units *val);
};

class vunits_env_reg : public reg {
  VUNITS_FUNCP func;
 public:
  vunits_env_reg(VUNITS_FUNCP f);
  const char *get_string();
  int get_value(units *val);
};


class hunits_env_reg : public reg {
  HUNITS_FUNCP func;
 public:
  hunits_env_reg(HUNITS_FUNCP f);
  const char *get_string();
  int get_value(units *val);
};

class string_env_reg : public reg {
  STRING_FUNCP func;
public:
  string_env_reg(STRING_FUNCP);
  const char *get_string();
};

int_env_reg::int_env_reg(INT_FUNCP f) : func(f)
{
}

int int_env_reg::get_value(units *val)
{
  *val = (curenv->*func)();
  return 1;
}

const char *int_env_reg::get_string()
{
  return itoa((curenv->*func)());
}
 
vunits_env_reg::vunits_env_reg(VUNITS_FUNCP f) : func(f)
{
}

int vunits_env_reg::get_value(units *val)
{
  *val = (curenv->*func)().to_units();
  return 1;
}

const char *vunits_env_reg::get_string()
{
  return itoa((curenv->*func)().to_units());
}

hunits_env_reg::hunits_env_reg(HUNITS_FUNCP f) : func(f)
{
}

int hunits_env_reg::get_value(units *val)
{
  *val = (curenv->*func)().to_units();
  return 1;
}

const char *hunits_env_reg::get_string()
{
  return itoa((curenv->*func)().to_units());
}

string_env_reg::string_env_reg(STRING_FUNCP f) : func(f)
{
}

const char *string_env_reg::get_string()
{
  return (curenv->*func)();
}

class horizontal_place_reg : public general_reg {
public:
  horizontal_place_reg();
  int get_value(units *);
  void set_value(units);
};

horizontal_place_reg::horizontal_place_reg()
{
}

int horizontal_place_reg::get_value(units *res)
{
  *res = curenv->get_input_line_position().to_units();
  return 1;
}

void horizontal_place_reg::set_value(units n)
{
  curenv->set_input_line_position(hunits(n));
}

const char *environment::get_font_family_string()
{
  return family->nm.contents();
}

const char *environment::get_name_string()
{
  return name.contents();
}

// Convert a quantity in scaled points to ascii decimal fraction.

const char *sptoa(int sp)
{
  assert(sp > 0);
  assert(sizescale > 0);
  if (sizescale == 1)
    return itoa(sp);
  if (sp % sizescale == 0)
    return itoa(sp/sizescale);
  // See if 1/sizescale is exactly representable as a decimal fraction,
  // ie its only prime factors are 2 and 5.
  int n = sizescale;
  int power2 = 0;
  while ((n & 1) == 0) {
    n >>= 1;
    power2++;
  }
  int power5 = 0;
  while ((n % 5) == 0) {
    n /= 5;
    power5++;
  }
  if (n == 1) {
    int decimal_point = power5 > power2 ? power5 : power2;
    if (decimal_point <= 10) {
      int factor = 1;
      int t;
      for (t = decimal_point - power2; --t >= 0;)
	factor *= 2;
      for (t = decimal_point - power5; --t >= 0;)
	factor *= 5;
      if (factor == 1 || sp <= INT_MAX/factor)
	return iftoa(sp*factor, decimal_point);
    }
  }
  double s = double(sp)/double(sizescale);
  double factor = 10.0;
  double val = s;
  int decimal_point = 0;
  do  {
    double v = ceil(s*factor);
    if (v > INT_MAX)
      break;
    val = v;
    factor *= 10.0;
  } while (++decimal_point < 10);
  return iftoa(int(val), decimal_point);
}

const char *environment::get_point_size_string()
{
  return sptoa(curenv->get_point_size());
}

const char *environment::get_requested_point_size_string()
{
  return sptoa(curenv->get_requested_point_size());
}

#define init_int_env_reg(name, func) \
  number_reg_dictionary.define(name, new int_env_reg(&environment::func))

#define init_vunits_env_reg(name, func) \
  number_reg_dictionary.define(name, new vunits_env_reg(&environment::func))

#define init_hunits_env_reg(name, func) \
  number_reg_dictionary.define(name, new hunits_env_reg(&environment::func))

#define init_string_env_reg(name, func) \
  number_reg_dictionary.define(name, new string_env_reg(&environment::func))

void init_env_requests()
{
  init_request("it", input_trap);
  init_request("ad", adjust);
  init_request("na", no_adjust);
  init_request("ev", environment_switch);
  init_request("lt", title_length);
  init_request("ps", point_size);
  init_request("ft", font_change);
  init_request("fam", family_change);
  init_request("ss", space_size);
  init_request("fi", fill);
  init_request("nf", no_fill);
  init_request("ce", center);
  init_request("rj", right_justify);
  init_request("vs", vertical_spacing);
  init_request("ls", line_spacing);
  init_request("ll", line_length);
  init_request("in", indent);
  init_request("ti", temporary_indent);
  init_request("ul", underline);
  init_request("cu", underline);
  init_request("cc", control_char);
  init_request("c2", no_break_control_char);
  init_request("br", break_request);
  init_request("tl", title);
  init_request("ta", set_tabs);
  init_request("fc", field_characters);
  init_request("mc", margin_character);
  init_request("nn", no_number);
  init_request("nm", number_lines);
  init_request("tc", tab_character);
  init_request("lc", leader_character);
  init_request("hy", hyphenate_request);
  init_request("hc", hyphen_char);
  init_request("nh", no_hyphenate);
  init_request("hlm", hyphen_line_max_request);
#ifdef WIDOW_CONTROL
  init_request("wdc", widow_control_request);
#endif /* WIDOW_CONTROL */
#if 0
  init_request("tas", tabs_save);
  init_request("tar", tabs_restore);
#endif  
  init_request("hys", hyphenation_space_request);
  init_request("hym", hyphenation_margin_request);
  init_int_env_reg(".f", get_font);
  init_int_env_reg(".b", get_bold);
  init_hunits_env_reg(".i", get_indent);
  init_hunits_env_reg(".in", get_saved_indent);
  init_int_env_reg(".j", get_adjust_mode);
  init_hunits_env_reg(".k", get_text_length);
  init_hunits_env_reg(".l", get_line_length);
  init_hunits_env_reg(".ll", get_saved_line_length);
  init_int_env_reg(".L", get_line_spacing);
  init_hunits_env_reg(".n", get_prev_text_length);
  init_string_env_reg(".s", get_point_size_string);
  init_string_env_reg(".sr", get_requested_point_size_string);
  init_int_env_reg(".ps", get_point_size);
  init_int_env_reg(".psr", get_requested_point_size);
  init_int_env_reg(".u", get_fill);
  init_vunits_env_reg(".v", get_vertical_spacing);
  init_hunits_env_reg(".w", get_prev_char_width);
  init_int_env_reg(".ss", get_space_size);
  init_int_env_reg(".sss", get_sentence_space_size);
  init_string_env_reg(".fam", get_font_family_string);
  init_string_env_reg(".ev", get_name_string);
  init_int_env_reg(".hy", get_hyphenation_flags);
  init_int_env_reg(".hlm", get_hyphen_line_max);
  init_int_env_reg(".hlc", get_hyphen_line_count);
  init_hunits_env_reg(".lt", get_title_length);
  init_string_env_reg(".tabs", get_tabs);
  init_hunits_env_reg(".csk", get_prev_char_skew);
  init_vunits_env_reg(".cht", get_prev_char_height);
  init_vunits_env_reg(".cdp", get_prev_char_depth);
  init_int_env_reg(".ce", get_center_lines);
  init_int_env_reg(".rj", get_right_justify_lines);
  init_hunits_env_reg(".hys", get_hyphenation_space);
  init_hunits_env_reg(".hym", get_hyphenation_margin);
  number_reg_dictionary.define("ln", new variable_reg(&next_line_number));
  number_reg_dictionary.define("ct", new variable_reg(&ct_reg_contents));
  number_reg_dictionary.define("sb", new variable_reg(&sb_reg_contents));
  number_reg_dictionary.define("st", new variable_reg(&st_reg_contents));
  number_reg_dictionary.define("rsb", new variable_reg(&rsb_reg_contents));
  number_reg_dictionary.define("rst", new variable_reg(&rst_reg_contents));
  number_reg_dictionary.define("ssc", new variable_reg(&ssc_reg_contents));
  number_reg_dictionary.define("skw", new variable_reg(&skw_reg_contents));
  number_reg_dictionary.define("hp", new horizontal_place_reg);
}

// Hyphenation - TeX's hyphenation algorithm with a less fancy implementation.

const int WORD_MAX = 1024;

dictionary exception_dictionary(501);

static void hyphen_word()
{
  char buf[WORD_MAX + 1];
  unsigned char pos[WORD_MAX + 2];
  for (;;) {
    tok.skip();
    if (tok.newline() || tok.eof())
      break;
    int i = 0;
    int npos = 0;
    while (i < WORD_MAX && !tok.space() && !tok.newline() && !tok.eof()) {
      charinfo *ci = tok.get_char(1);
      if (ci == 0) {
	skip_line();
	return;
      }
      tok.next();
      if (ci->get_ascii_code() == '-') {
	if (i > 0 && (npos == 0 || pos[npos - 1] != i))
	  pos[npos++] = i;
      }
      else {
	int c = ci->get_hyphenation_code();
	if (c == 0)
	  break;
	buf[i++] = c;
      }
    }
    if (i > 0) {
      pos[npos] = 0;
      buf[i] = 0;
      unsigned char *tem = new unsigned char[npos + 1];
      memcpy(tem, pos, npos+1);
      tem = (unsigned char *)exception_dictionary.lookup(symbol(buf), tem);
      if (tem)
	delete tem;
    }
  }
  skip_line();
}

struct trie_node;

class trie {
  trie_node *tp;
  virtual void do_match(int len, void *val) = 0;
public:
  trie() : tp(0) {}
  void insert(unsigned char *, int, void *);
  // find calls do_match for each match it finds
  void find(unsigned char *pat, int patlen);
  void clear();
};

struct trie_node {
  unsigned char c;
  trie_node *down;
  trie_node *right;
  void *val;
  trie_node(unsigned char, trie_node *);
  ~trie_node();
};

trie_node::trie_node(unsigned char ch, trie_node *p) 
: c(ch), right(p), down(0), val(0)
{
}

trie_node::~trie_node()
{
  if (down)
    delete down;
  if (right)
    delete right;
  if (val)
    delete val;
}

void trie::clear()
{
  if (tp) {
    delete tp;
    tp = 0;
  }
}

void trie::insert(unsigned char *pat, int patlen, void *val)
{
  trie_node **p = &tp;
  assert(patlen > 0 && pat != 0);
  for (;;) {
    while (*p != 0 && (*p)->c < pat[0])
      p = &((*p)->right);
    if (*p == 0 || (*p)->c != pat[0])
      *p = new trie_node(pat[0], *p);
    if (--patlen == 0) {
      (*p)->val = val;
      break;
    }
    ++pat;
    p = &((*p)->down);
  }
}

void trie::find(unsigned char *pat, int patlen)
{
  trie_node *p = tp;
  for (int i = 0; p != 0 && i < patlen; i++) {
    while (p != 0 && p->c < pat[i])
      p = p->right;
    if (p != 0 && p->c == pat[i]) {
      if (p->val != 0)
	do_match(i+1, p->val);
      p = p->down;
    }
    else
      break;
  }
}

class hyphen_trie : private trie {
  int *h;
  void do_match(int i, void *v);
  void insert_pattern(unsigned char *pat, int patlen, int *num);
public:
  hyphen_trie() {}
  void hyphenate(unsigned char *word, int len, int *hyphens);
  void read_patterns_file(const char *name);
};

struct operation {
  operation *next;
  short distance;
  short num;
  operation(int, int, operation *);
};

operation::operation(int i, int j, operation *op)
: num(i), distance(j), next(op)
{
}

void hyphen_trie::insert_pattern(unsigned char *pat, int patlen, int *num)
{
  operation *op = 0;
  for (int i = 0; i < patlen+1; i++)
    if (num[i] != 0)
      op = new operation(num[i], patlen - i, op);
  insert(pat, patlen, op);
}

void hyphen_trie::hyphenate(unsigned char *word, int len, int *hyphens)
{
  for (int j = 0; j < len+1; j++)
    hyphens[j] = 0;
  for (j = 0; j < len - 1; j++) {
    h = hyphens + j;
    find(word + j, len - j);
  }
}

inline int max(int m, int n)
{
  return m > n ? m : n;
}

void hyphen_trie::do_match(int i, void *v)
{
  operation *op = (operation *)v;
  while (op != 0) {
    h[i - op->distance] = max(h[i - op->distance], op->num);
    op = op->next;
  }
}


void hyphen_trie::read_patterns_file(const char *name)
{
  clear();
  unsigned char buf[WORD_MAX];
  int num[WORD_MAX+1];
  FILE *fp = fopen(name, "r");
  if (fp == 0)
    fatal("can't open hyphenation patterns file `%1': %2",
	  name, strerror(errno));
  int c = getc(fp);
  for (;;) {
    while (c != EOF && csspace(c))
      c = getc(fp);
    if (c == EOF)
      break;
    int i = 0;
    num[0] = 0;
    do {
      if (csdigit(c))
	num[i] = c - '0';
      else {
	buf[i++] = c;
	num[i] = 0;
      }
      c = getc(fp);
    } while (i < WORD_MAX && c != EOF && !csspace(c));
    insert_pattern(buf, i, num);
  }
  fclose(fp);
}

hyphen_trie ht;

void hyphenate(hyphen_list *h, unsigned flags)
{
  while (h && h->hyphenation_code == 0)
    h = h->next;
  int len = 0;
  char hbuf[WORD_MAX+2];
  char *buf = hbuf + 1;
  for (hyphen_list *tem = h; tem && len < WORD_MAX; tem = tem->next) {
    if (tem->hyphenation_code != 0)
      buf[len++] = tem->hyphenation_code;
    else
      break;
  }
  if (len > 2) {
    buf[len] = 0;
    unsigned char *pos = (unsigned char *)exception_dictionary.lookup(buf);
    if (pos != 0) {
      int j = 0;
      int i = 1;
      for (tem = h; tem != 0; tem = tem->next, i++)
	if (pos[j] == i) {
	  tem->hyphen = 1;
	  j++;
	}
    }
    else {
      hbuf[len+1] = '.';
      int num[WORD_MAX+3];
      ht.hyphenate((unsigned char *)hbuf, len+2, num);
      int i;
      num[2] = 0;
      if (flags & 8)
	num[3] = 0;
      if (flags & 4)
	--len;
      for (i = 2, tem = h; i < len && tem; tem = tem->next, i++)
	if (num[i] & 1)
	  tem->hyphen = 1;
    }
  }
}

void read_hyphen_file(const char *name)
{
  ht.read_patterns_file(name);
}

void init_hyphen_requests()
{
  init_request("hw", hyphen_word);
}
