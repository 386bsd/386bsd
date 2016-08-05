/* Code for handling XREF output from GNU C++.
   Copyright (C) 1992, 1993 Free Software Foundation, Inc.
   Contributed by Michael Tiemann (tiemann@cygnus.com)

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */


#include "config.h"
#include "tree.h"
#include <stdio.h>
#include "cp-tree.h"
#include "input.h"

#include <ctype.h>

extern char *getpwd ();

extern char *index ();
extern char *rindex ();

/* The character(s) used to join a directory specification (obtained with
   getwd or equivalent) with a non-absolute file name.  */

#ifndef FILE_NAME_JOINER
#define FILE_NAME_JOINER "/"
#endif

/* Nonzero if NAME as a file name is absolute.  */
#ifndef FILE_NAME_ABSOLUTE_P
#define FILE_NAME_ABSOLUTE_P(NAME) (NAME[0] == '/')
#endif

/* For cross referencing.  */

int flag_gnu_xref;

/************************************************************************/
/*									*/
/*	Common definitions						*/
/*									*/
/************************************************************************/

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef NULL
#define NULL 0
#endif

#define PALLOC(typ) ((typ *) calloc(1,sizeof(typ)))


/* Return a malloc'd copy of STR.  */
#define SALLOC(str) \
 ((char *) ((str) == NULL ? NULL	\
	    : (char *) strcpy ((char *) malloc (strlen ((str)) + 1), (str))))
#define SFREE(str) (str != NULL && (free(str),0))

#define STREQL(s1,s2) (strcmp((s1),(s2)) == 0)
#define STRNEQ(s1,s2) (strcmp((s1),(s2)) != 0)
#define STRLSS(s1,s2) (strcmp((s1),(s2)) < 0)
#define STRLEQ(s1,s2) (strcmp((s1),(s2)) <= 0)
#define STRGTR(s1,s2) (strcmp((s1),(s2)) > 0)
#define STRGEQ(s1,s2) (strcmp((s1),(s2)) >= 0)

/************************************************************************/
/*									*/
/*	Type definitions						*/
/*									*/
/************************************************************************/


typedef struct _XREF_FILE *	XREF_FILE;
typedef struct _XREF_SCOPE *	XREF_SCOPE;

typedef struct _XREF_FILE
{
  char *name;
  char *outname;
  XREF_FILE next;
} XREF_FILE_INFO;

typedef struct _XREF_SCOPE
{
  int gid;
  int lid;
  XREF_FILE file;
  int start;
  XREF_SCOPE outer;
} XREF_SCOPE_INFO;

/************************************************************************/
/*									*/
/*	Local storage							*/
/*									*/
/************************************************************************/

static	char		doing_xref = 0;
static	FILE *		xref_file = NULL;
static	char		xref_name[1024];
static	XREF_FILE	all_files = NULL;
static	char *		wd_name = NULL;
static	XREF_SCOPE	cur_scope = NULL;
static	int 	scope_ctr = 0;
static	XREF_FILE	last_file = NULL;
static	tree		last_fndecl = NULL;

/************************************************************************/
/*									*/
/*	Forward definitions						*/
/*									*/
/************************************************************************/

extern	void		GNU_xref_begin();
extern	void		GNU_xref_end();
extern	void		GNU_xref_file();
extern	void		GNU_xref_start_scope();
extern	void		GNU_xref_end_scope();
extern	void		GNU_xref_ref();
extern	void		GNU_xref_decl();
extern	void		GNU_xref_call();
extern	void		GNU_xref_function();
extern	void		GNU_xref_assign();
extern	void		GNU_xref_hier();
extern	void		GNU_xref_member();

static	void		gen_assign();
static	XREF_FILE	find_file();
static	char *		filename();
static	char *		fctname();
static	char *		declname();
static	void		simplify_type();
static	char *		fixname();
static	void		open_xref_file();

extern	char *		type_as_string();

/* Start cross referencing.  FILE is the name of the file we xref.  */

void
GNU_xref_begin (file)
   char *file;
{
  doing_xref = 1;

  if (file != NULL && STRNEQ (file,"-"))
    {
      open_xref_file(file);
      GNU_xref_file(file);
    }
}

/* Finish cross-referencing.  ERRCNT is the number of errors
   we encountered.  */

void
GNU_xref_end (ect)
   int ect;
{
  XREF_FILE xf;

  if (!doing_xref) return;

  xf = find_file (input_filename);
  if (xf == NULL) return;

  while (cur_scope != NULL)
    GNU_xref_end_scope(cur_scope->gid,0,0,0,0);

  doing_xref = 0;

  if (xref_file == NULL) return;

  fclose (xref_file);

  xref_file = NULL;
  all_files = NULL;

  if (ect > 0) unlink (xref_name);
}

/* Write out xref for file named NAME.  */

void
GNU_xref_file (name)
   char *name;
{
  XREF_FILE xf;

  if (!doing_xref || name == NULL) return;

  if (xref_file == NULL)
    {
      open_xref_file (name);
      if (!doing_xref) return;
    }

  if (all_files == NULL)
    fprintf(xref_file,"SCP * 0 0 0 0 RESET\n");

  xf = find_file (name);
  if (xf != NULL) return;

  xf = PALLOC (XREF_FILE_INFO);
  xf->name = SALLOC (name);
  xf->next = all_files;
  all_files = xf;

  if (wd_name == NULL)
    wd_name = getpwd ();

  if (FILE_NAME_ABSOLUTE_P (name) || ! wd_name)
    xf->outname = xf->name;
  else
    {
      char *nmbuf
	= (char *) malloc (strlen (wd_name) + strlen (FILE_NAME_JOINER)
			   + strlen (name) + 1);
      sprintf (nmbuf, "%s%s%s", wd_name, FILE_NAME_JOINER, name);
      name = nmbuf;
      xf->outname = nmbuf;
    }

  fprintf (xref_file, "FIL %s %s 0\n", name, wd_name);

  filename (xf);
  fctname (NULL);
}

/* Start a scope identified at level ID.  */

void
GNU_xref_start_scope (id)
   HOST_WIDE_INT id;
{
  XREF_SCOPE xs;
  XREF_FILE xf;

  if (!doing_xref) return;
  xf = find_file (input_filename);

  xs = PALLOC (XREF_SCOPE_INFO);
  xs->file = xf;
  xs->start = lineno;
  if (xs->start <= 0) xs->start = 1;
  xs->gid = id;
  xs->lid = ++scope_ctr;
  xs->outer = cur_scope;
  cur_scope = xs;
}

/* Finish a scope at level ID.
   INID is ???
   PRM is ???
   KEEP is nonzero iff this scope is retained (nonzero if it's
   a compiler-generated invisible scope).
   TRNS is ???  */

void
GNU_xref_end_scope (id,inid,prm,keep,trns)
   HOST_WIDE_INT id;
   HOST_WIDE_INT inid;
   int prm,keep,trns;
{
  XREF_FILE xf;
  XREF_SCOPE xs,lxs,oxs;
  char *stype;

  if (!doing_xref) return;
  xf = find_file (input_filename);
  if (xf == NULL) return;

  lxs = NULL;
  for (xs = cur_scope; xs != NULL; xs = xs->outer)
    {
      if (xs->gid == id) break;
      lxs = xs;
    }
  if (xs == NULL) return;

  if (inid != 0) {
    for (oxs = cur_scope; oxs != NULL; oxs = oxs->outer) {
      if (oxs->gid == inid) break;
    }
    if (oxs == NULL) return;
    inid = oxs->lid;
  }

  if (prm == 2) stype = "SUE";
  else if (prm != 0) stype = "ARGS";
  else if (keep == 2 || inid != 0) stype = "INTERN";
  else stype = "EXTERN";

  fprintf (xref_file,"SCP %s %d %d %d %d %s\n",
	   filename (xf), xs->start, lineno,xs->lid, inid, stype);

  if (lxs == NULL) cur_scope = xs->outer;
  else lxs->outer = xs->outer;

  free (xs);
}

/* Output a reference to NAME in FNDECL.  */

void
GNU_xref_ref (fndecl,name)
   tree fndecl;
   char *name;
{
  XREF_FILE xf;

  if (!doing_xref) return;
  xf = find_file (input_filename);
  if (xf == NULL) return;

  fprintf (xref_file, "REF %s %d %s %s\n",
	   filename (xf), lineno, fctname (fndecl), name);
}

/* Output a reference to DECL in FNDECL.  */

void
GNU_xref_decl (fndecl,decl)
   tree fndecl;
   tree decl;
{
  XREF_FILE xf,xf1;
  char *cls;
  char *name;
  char buf[10240];
  int uselin;

  if (!doing_xref) return;
  xf = find_file (input_filename);
  if (xf == NULL) return;

  uselin = FALSE;

  if (TREE_CODE (decl) == TYPE_DECL) cls = "TYPEDEF";
  else if (TREE_CODE (decl) == FIELD_DECL) cls = "FIELD";
  else if (TREE_CODE (decl) == VAR_DECL)
    {
      if (fndecl == NULL && TREE_STATIC(decl)
	  && TREE_READONLY(decl) && DECL_INITIAL(decl) != 0
	  && !TREE_PUBLIC(decl) && !DECL_EXTERNAL(decl)
	  && DECL_MODE(decl) != BLKmode) cls = "CONST";
      else if (DECL_EXTERNAL(decl)) cls = "EXTERN";
      else if (TREE_PUBLIC(decl)) cls = "EXTDEF";
      else if (TREE_STATIC(decl)) cls = "STATIC";
      else if (DECL_REGISTER(decl)) cls = "REGISTER";
      else cls = "AUTO";
    }
  else if (TREE_CODE (decl) == PARM_DECL) cls = "PARAM";
  else if (TREE_CODE (decl) == FIELD_DECL) cls = "FIELD";
  else if (TREE_CODE (decl) == CONST_DECL) cls = "CONST";
  else if (TREE_CODE (decl) == FUNCTION_DECL)
    {
      if (DECL_EXTERNAL (decl)) cls = "EXTERN";
      else if (TREE_PUBLIC (decl)) cls = "EFUNCTION";
      else cls = "SFUNCTION";
    }
  else if (TREE_CODE (decl) == LABEL_DECL) cls = "LABEL";
  else if (TREE_CODE (decl) == UNION_TYPE)
    {
      cls = "UNIONID";
      decl = TYPE_NAME (decl);
      uselin = TRUE;
    }
  else if (TREE_CODE (decl) == RECORD_TYPE)
    {
      if (CLASSTYPE_DECLARED_CLASS (decl)) cls = "CLASSID";
      else cls = "STRUCTID";
      decl = TYPE_NAME (decl);
      uselin = TRUE;
    }
  else if (TREE_CODE (decl) == ENUMERAL_TYPE)
    {
      cls = "ENUMID";
      decl = TYPE_NAME (decl);
      uselin = TRUE;
    }
  else cls = "UNKNOWN";

  if (decl == NULL || DECL_NAME (decl) == NULL) return;

  if (uselin && decl->decl.linenum > 0 && decl->decl.filename != NULL)
    {
      xf1 = find_file (decl->decl.filename);
      if (xf1 != NULL)
	{
	  lineno = decl->decl.linenum;
	  xf = xf1;
	}
    }

  if (DECL_ASSEMBLER_NAME (decl))
    name = IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (decl));
  else
    name = IDENTIFIER_POINTER (DECL_NAME (decl));

  strcpy (buf, type_as_string (TREE_TYPE (decl)));
  simplify_type (buf);

  fprintf (xref_file, "DCL %s %d %s %d %s %s %s\n",
	   filename(xf), lineno, name,
	   (cur_scope != NULL ? cur_scope->lid : 0),
	   cls, fctname(fndecl), buf);

  if (STREQL (cls, "STRUCTID") || STREQL (cls, "UNIONID"))
    {
      cls = "CLASSID";
      fprintf (xref_file, "DCL %s %d %s %d %s %s %s\n",
	       filename(xf), lineno,name,
	       (cur_scope != NULL ? cur_scope->lid : 0),
	       cls, fctname(fndecl), buf);
    }
}

/* Output a reference to a call to NAME in FNDECL.  */

void
GNU_xref_call (fndecl, name)
   tree fndecl;
   char *name;
{
  XREF_FILE xf;
  char buf[1024];
  char *s;

  if (!doing_xref) return;
  xf = find_file (input_filename);
  if (xf == NULL) return;
  name = fixname (name, buf);

  for (s = name; *s != 0; ++s)
    if (*s == '_' && s[1] == '_') break;
  if (*s != 0) GNU_xref_ref (fndecl, name);

  fprintf (xref_file, "CAL %s %d %s %s\n",
	   filename (xf), lineno, name, fctname (fndecl));
}

/* Output cross-reference info about FNDECL.  If non-NULL,
   ARGS are the arguments for the function (i.e., before the FUNCTION_DECL
   has been fully built).  */

void
GNU_xref_function (fndecl, args)
   tree fndecl;
   tree args;
{
  XREF_FILE xf;
  int ct;
  char buf[1024];

  if (!doing_xref) return;
  xf = find_file (input_filename);
  if (xf == NULL) return;

  ct = 0;
  buf[0] = 0;
  if (args == NULL) args = DECL_ARGUMENTS (fndecl);

  GNU_xref_decl (NULL, fndecl);

  for ( ; args != NULL; args = TREE_CHAIN (args))
    {
      GNU_xref_decl (fndecl,args);
      if (ct != 0) strcat (buf,",");
      strcat (buf, declname (args));
      ++ct;
    }

  fprintf (xref_file, "PRC %s %d %s %d %d %s\n",
	   filename(xf), lineno, declname(fndecl),
	   (cur_scope != NULL ? cur_scope->lid : 0),
	   ct, buf);
}

/* Output cross-reference info about an assignment to NAME.  */

void
GNU_xref_assign(name)
   tree name;
{
  XREF_FILE xf;

  if (!doing_xref) return;
  xf = find_file(input_filename);
  if (xf == NULL) return;

  gen_assign(xf, name);
}

static void
gen_assign(xf, name)
   XREF_FILE xf;
   tree name;
{
  char *s;

  s = NULL;

  switch (TREE_CODE (name))
    {
    case IDENTIFIER_NODE :
      s = IDENTIFIER_POINTER(name);
      break;
    case VAR_DECL :
      s = declname(name);
      break;
    case COMPONENT_REF :
      gen_assign(xf, TREE_OPERAND(name, 0));
      gen_assign(xf, TREE_OPERAND(name, 1));
      break;
    case INDIRECT_REF :
    case OFFSET_REF :
    case ARRAY_REF :
    case BUFFER_REF :
      gen_assign(xf, TREE_OPERAND(name, 0));
      break;
    case COMPOUND_EXPR :
      gen_assign(xf, TREE_OPERAND(name, 1));
      break;
      default :
      break;
    }

  if (s != NULL)
    fprintf(xref_file, "ASG %s %d %s\n", filename(xf), lineno, s);
}

/* Output cross-reference info about a class hierarchy.
   CLS is the class type of interest.  BASE is a baseclass
   for CLS.  PUB and VIRT give the visibility info about
   the class derivation.  FRND is nonzero iff BASE is a friend
   of CLS.

   ??? Needs to handle nested classes.  */
void
GNU_xref_hier(cls, base, pub, virt, frnd)
   char *cls;
   char *base;
   int pub;
   int virt;
   int frnd;
{
  XREF_FILE xf;

  if (!doing_xref) return;
  xf = find_file(input_filename);
  if (xf == NULL) return;

  fprintf(xref_file, "HIE %s %d %s %s %d %d %d\n",
	  filename(xf), lineno, cls, base, pub, virt, frnd);
}

/* Output cross-reference info about class members.  CLS
   is the containing type; FLD is the class member.  */

void
GNU_xref_member(cls, fld)
   tree cls;
   tree fld;
{
  XREF_FILE xf;
  char *prot;
  int confg, pure;
  char *d;
  int i;
  char buf[1024], bufa[1024];

  if (!doing_xref) return;
  xf = find_file(fld->decl.filename);
  if (xf == NULL) return;

  if (TREE_PRIVATE (fld)) prot = "PRIVATE";
  else if (TREE_PROTECTED(fld)) prot = "PROTECTED";
  else prot = "PUBLIC";

  confg = 0;
  if (TREE_CODE (fld) == FUNCTION_DECL && DECL_CONST_MEMFUNC_P(fld))
    confg = 1;
  else if (TREE_CODE (fld) == CONST_DECL)
    confg = 1;

  pure = 0;
  if (TREE_CODE (fld) == FUNCTION_DECL && DECL_ABSTRACT_VIRTUAL_P(fld))
    pure = 1;

  d = IDENTIFIER_POINTER(cls);
  sprintf(buf, "%d%s", strlen(d), d);
  i = strlen(buf);
  strcpy(bufa, declname(fld));

#ifdef XREF_SHORT_MEMBER_NAMES
  for (p = &bufa[1]; *p != 0; ++p)
    {
      if (p[0] == '_' && p[1] == '_' && p[2] >= '0' && p[2] <= '9') {
	if (strncmp(&p[2], buf, i) == 0) *p = 0;
	break;
      }
      else if (p[0] == '_' && p[1] == '_' && p[2] == 'C' && p[3] >= '0' && p[3] <= '9') {
	if (strncmp(&p[3], buf, i) == 0) *p = 0;
	break;
      }
    }
#endif

  fprintf(xref_file, "MEM %s %d %s %s %s %d %d %d %d %d %d %d\n",
	  filename(xf), fld->decl.linenum, d,  bufa,  prot,
	  (TREE_CODE (fld) == FUNCTION_DECL ? 0 : 1),
	  (DECL_INLINE (fld) ? 1 : 0),
	  (DECL_FRIEND_P(fld) ? 1 : 0),
	  (DECL_VINDEX(fld) ? 1 : 0),
	  (TREE_STATIC(fld) ? 1 : 0),
	  pure, confg);
}

/* Find file entry given name.  */

static XREF_FILE
find_file(name)
   char *name;
{
  XREF_FILE xf;

  for (xf = all_files; xf != NULL; xf = xf->next) {
    if (STREQL(name, xf->name)) break;
  }

  return xf;
}

/* Return filename for output purposes.  */

static char *
filename(xf)
   XREF_FILE xf;
{
  if (xf == NULL) {
    last_file = NULL;
    return "*";
  }

  if (last_file == xf) return "*";

  last_file = xf;

  return xf->outname;
}

/* Return function name for output purposes.  */

static char *
fctname(fndecl)
   tree fndecl;
{
  static char fctbuf[1024];
  char *s;

  if (fndecl == NULL && last_fndecl == NULL) return "*";

  if (fndecl == NULL)
    {
      last_fndecl = NULL;
      return "*TOP*";
    }

  if (fndecl == last_fndecl) return "*";

  last_fndecl = fndecl;

  s = declname(fndecl);
  s = fixname(s, fctbuf);

  return s;
}

/* Return decl name for output purposes.  */

static char *
declname(dcl)
   tree dcl;
{
  if (DECL_NAME (dcl) == NULL) return "?";

  if (DECL_ASSEMBLER_NAME (dcl))
    return IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (dcl));
  else
    return IDENTIFIER_POINTER (DECL_NAME (dcl));
}

/* Simplify a type string by removing unneeded parenthesis.  */

static void
simplify_type(typ)
   char *typ;
{
  char *s;
  int lvl, i;

  i = strlen(typ);
  while (i > 0 && isspace(typ[i-1])) typ[--i] = 0;

  if (i > 7 && STREQL(&typ[i-5], "const"))
    {
      typ[i-5] = 0;
      i -= 5;
    }

  if (typ[i-1] != ')') return;

  s = &typ[i-2];
  lvl = 1;
  while (*s != 0) {
    if (*s == ')') ++lvl;
    else if (*s == '(')
      {
	--lvl;
	if (lvl == 0)
	  {
	    s[1] = ')';
	    s[2] = 0;
	    break;
	  }
      }
    --s;
  }

  if (*s != 0 && s[-1] == ')')
    {
      --s;
      --s;
      if (*s == '(') s[2] = 0;
      else if (*s == ':') {
	while (*s != '(') --s;
	s[1] = ')';
	s[2] = 0;
      }
    }
}

/* Fixup a function name (take care of embedded spaces).  */

static char *
fixname(nam, buf)
   char *nam;
   char *buf;
{
  char *s, *t;
  int fg;

  s = nam;
  t = buf;
  fg = 0;

  while (*s != 0)
    {
      if (*s == ' ')
	{
	  *t++ = '\36';
	  ++fg;
	}
      else *t++ = *s;
      ++s;
    }
  *t = 0;

  if (fg == 0) return nam;

  return buf;
}

/* Open file for xrefing.  */

static void
open_xref_file(file)
   char *file;
{
  char *s, *t;

#ifdef XREF_FILE_NAME
  XREF_FILE_NAME (xref_name, file);
#else
  s = rindex (file, '/');
  if (s == NULL)
    sprintf (xref_name, ".%s.gxref", file);
  else
    {
      ++s;
      strcpy (xref_name, file);
      t = rindex (xref_name, '/');
      ++t;
      *t++ = '.';
      strcpy (t, s);
      strcat (t, ".gxref");
    }
#endif /* no XREF_FILE_NAME */

  xref_file = fopen(xref_name, "w");

  if (xref_file == NULL)
    {
      error("Can't create cross-reference file `%s'", xref_name);
      doing_xref = 0;
    }
}
