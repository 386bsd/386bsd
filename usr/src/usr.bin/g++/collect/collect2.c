/* Collect static initialization info into data structures
   that can be traversed by C++ initialization and finalization
   routines.

   Copyright (C) 1992 Free Software Foundation, Inc.
   Contributed by Chris Smith (csmith@convex.com).
   Heavily modified by Michael Meissner (meissner@osf.org),
   Per Bothner (bothner@cygnus.com), and John Gilmore (gnu@cygnus.com).

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


/* Build tables of static constructors and destructors and run ld. */

#include <sys/types.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/stat.h>
#ifdef NO_WAIT_H
#include <sys/wait.h>
#endif

#ifndef errno
extern int errno;
#endif

#define COLLECT

#include "config.h"

#ifndef __STDC__
#define generic char
#define const

#else
#define generic void
#endif

#ifdef USG
#define vfork fork
#endif

#ifndef R_OK
#define R_OK 4
#define W_OK 2
#define X_OK 1
#endif

/* On MSDOS, write temp files in current dir
   because there's no place else we can expect to use.  */
#if __MSDOS__
#ifndef P_tmpdir
#define P_tmpdir "./"
#endif
#endif

/* On certain systems, we have code that works by scanning the object file
   directly.  But this code uses system-specific header files and library
   functions, so turn it off in a cross-compiler.  */

#ifdef CROSS_COMPILE
#undef OBJECT_FORMAT_COFF
#undef OBJECT_FORMAT_ROSE
#endif

/* If we can't use a special method, use the ordinary one:
   run nm to find what symbols are present.
   In a cross-compiler, this means you need a cross nm,
   but that isn't quite as unpleasant as special headers.  */

#if !defined (OBJECT_FORMAT_COFF) && !defined (OBJECT_FORMAT_ROSE)
#define OBJECT_FORMAT_NONE
#endif

#ifdef OBJECT_FORMAT_COFF

#include <a.out.h>
#include <ar.h>

#ifdef UMAX
#include <sgs.h>
#endif

#ifdef _AIX
#define ISCOFF(magic) \
  ((magic) == U802WRMAGIC || (magic) == U802ROMAGIC || (magic) == U802TOCMAGIC)
#endif

/* Many versions of ldfcn.h define these.  */
#ifdef FREAD
#undef FREAD
#undef FWRITE
#endif

#include <ldfcn.h>

/* Mips-news overrides this macro.  */
#ifndef MY_ISCOFF
#define MY_ISCOFF(X) ISCOFF (X)
#endif

#endif /* OBJECT_FORMAT_COFF */

#ifdef OBJECT_FORMAT_ROSE

#ifdef _OSF_SOURCE
#define USE_MMAP
#endif

#ifdef USE_MMAP
#include <sys/mman.h>
#endif

#include <unistd.h>
#include <mach_o_format.h>
#include <mach_o_header.h>
#include <mach_o_vals.h>
#include <mach_o_types.h>

#endif /* OBJECT_FORMAT_ROSE */

#ifdef OBJECT_FORMAT_NONE

/* Default flags to pass to nm.  */
#ifndef NM_FLAGS
#define NM_FLAGS "-p"
#endif

#endif /* OBJECT_FORMAT_NONE */

/* Linked lists of constructor and destructor names. */

struct id 
{
  struct id *next;
  int sequence;
  char name[1];
};

struct head
{
  struct id *first;
  struct id *last;
  int number;
};

/* Enumeration giving which pass this is for scanning the program file.  */

enum pass {
  PASS_FIRST,				/* without constructors */
  PASS_SECOND				/* with constructors linked in */
};

#ifndef NO_SYS_SIGLIST
extern char *sys_siglist[];
#endif
extern char *version_string;

static int vflag;			/* true if -v */
static int rflag;			/* true if -r */
static int strip_flag;			/* true if -s */

static int debug;			/* true if -debug */

static int   temp_filename_length;	/* Length of temp_filename */
static char *temp_filename;		/* Base of temp filenames */
static char *c_file;			/* <xxx>.c for constructor/destructor list. */
static char *o_file;			/* <xxx>.o for constructor/destructor list. */
static char *nm_file_name;		/* pathname of nm */
static char *strip_file_name;		/* pathname of strip */

static struct head constructors;	/* list of constructors found */
static struct head destructors;		/* list of destructors found */

extern char *getenv ();
extern char *mktemp ();
static void  add_to_list ();
static void  scan_prog_file ();
static void  fork_execute ();
static void  do_wait ();
static void  write_c_file ();
static void  my_exit ();
static void  handler ();
static void  maybe_unlink ();
static void  choose_temp_base ();

generic *xcalloc ();
generic *xmalloc ();

extern char *index ();
extern char *rindex ();

#ifdef NO_DUP2
dup2 (oldfd, newfd)
     int oldfd;
     int newfd;
{
  int fdtmp[256];
  int fdx = 0;
  int fd;
 
  if (oldfd == newfd)
    return 0;
  close (newfd);
  while ((fd = dup (oldfd)) != newfd) /* good enough for low fd's */
    fdtmp[fdx++] = fd;
  while (fdx > 0)
    close (fdtmp[--fdx]);
}
#endif

char *
my_strerror (e)
     int e;
{
  extern char *sys_errlist[];
  extern int sys_nerr;
  static char buffer[30];

  if (!e)
    return "";

  if (e > 0 && e < sys_nerr)
    return sys_errlist[e];

  sprintf (buffer, "Unknown error %d", e);
  return buffer;
}

/* Delete tempfiles and exit function.  */

static void
my_exit (status)
     int status;
{
  if (c_file != 0 && c_file[0])
    maybe_unlink (c_file);

  if (o_file != 0 && o_file[0])
    maybe_unlink (o_file);

  exit (status);
}


/* Die when sys call fails. */

static void
fatal_perror (string, arg1, arg2, arg3)
     char *string;
{
  int e = errno;

  fprintf (stderr, "collect: ");
  fprintf (stderr, string, arg1, arg2, arg3);
  fprintf (stderr, ": %s\n", my_strerror (e));
  my_exit (1);
}

/* Just die. */

static void
fatal (string, arg1, arg2, arg3)
     char *string;
{
  fprintf (stderr, "collect: ");
  fprintf (stderr, string, arg1, arg2, arg3);
  fprintf (stderr, "\n");
  my_exit (1);
}

/* Write error message.  */

static void
error (string, arg1, arg2, arg3, arg4)
     char *string;
{
  fprintf (stderr, "collect: ");
  fprintf (stderr, string, arg1, arg2, arg3, arg4);
  fprintf (stderr, "\n");
}

/* In case obstack is linked in, and abort is defined to fancy_abort,
   provide a default entry.  */

void
fancy_abort ()
{
  fatal ("internal error");
}


static void
handler (signo)
     int signo;
{
  if (c_file[0])
    maybe_unlink (c_file);

  if (o_file[0])
    maybe_unlink (o_file);

  signal (signo, SIG_DFL);
  kill (getpid (), signo);
}


generic *
xcalloc (size1, size2)
     int size1, size2;
{
  generic *ptr = (generic *) calloc (size1, size2);
  if (ptr)
    return ptr;

  fatal ("out of memory");
  return (generic *)0;
}

generic *
xmalloc (size)
     int size;
{
  generic *ptr = (generic *) malloc (size);
  if (ptr)
    return ptr;

  fatal ("out of memory");
  return (generic *)0;
}

/* Make a copy of a string INPUT with size SIZE.  */

char *
savestring (input, size)
     char *input;
     int size;
{
  char *output = (char *) xmalloc (size + 1);
  bcopy (input, output, size);
  output[size] = 0;
  return output;
}

/* Decide whether the given symbol is:
   a constructor (1), a destructor (2), or neither (0).  */

static int
is_ctor_dtor (s)
     char *s;
{
  struct names { char *name; int len; int ret; int two_underscores; };

  register struct names *p;
  register int ch;
  register char *orig_s = s;

  static struct names special[] = {
#ifdef NO_DOLLAR_IN_LABEL
    { "GLOBAL_.I.", sizeof ("GLOBAL_.I.")-1, 1, 0 },
    { "GLOBAL_.D.", sizeof ("GLOBAL_.D.")-1, 2, 0 },
#else
    { "GLOBAL_$I$", sizeof ("GLOBAL_$I$")-1, 1, 0 },
    { "GLOBAL_$D$", sizeof ("GLOBAL_$I$")-1, 2, 0 },
#endif
#ifdef CFRONT_LOSSAGE /* Don't collect cfront initialization functions.
			 cfront has its own linker procedure to collect them;
			 if collect2 gets them too, they get collected twice
			 when the cfront procedure is run and the compiler used
			 for linking happens to be GCC.  */
    { "sti__", sizeof ("sti__")-1, 1, 1 },
    { "std__", sizeof ("std__")-1, 2, 1 },
#endif /* CFRONT_LOSSAGE */
    { NULL, 0, 0, 0 }
  };

  while ((ch = *s) == '_')
    ++s;

  if (s == orig_s)
    return 0;

  for (p = &special[0]; p->len > 0; p++)
    {
      if (ch == p->name[0]
	  && (!p->two_underscores || ((s - orig_s) >= 2))
	  && strncmp(s, p->name, p->len) == 0)
	{
	  return p->ret;
	}
    }
  return 0;
}


/* Compute a string to use as the base of all temporary file names.
   It is substituted for %g.  */

static void
choose_temp_base ()
{
  char *base = getenv ("TMPDIR");
  int len;

  if (base == (char *)0)
    {
#ifdef P_tmpdir
      if (access (P_tmpdir, R_OK | W_OK) == 0)
	base = P_tmpdir;
#endif
      if (base == (char *)0)
	{
	  if (access ("/usr/tmp", R_OK | W_OK) == 0)
	    base = "/usr/tmp/";
	  else
	    base = "/tmp/";
	}
    }

  len = strlen (base);
  temp_filename = xmalloc (len + sizeof("/ccXXXXXX") + 1);
  strcpy (temp_filename, base);
  if (len > 0 && temp_filename[len-1] != '/')
    temp_filename[len++] = '/';
  strcpy (temp_filename + len, "ccXXXXXX");

  mktemp (temp_filename);
  temp_filename_length = strlen (temp_filename);
}


/* Main program. */

int
main (argc, argv)
     int argc;
     char *argv[];
{
  char *outfile		= "a.out";
  char *arg;
  FILE *outf;
  char *ld_file_name;
  char *c_file_name;
  char *p;
  char *prefix;
  char **c_argv;
  char **c_ptr;
  char **ld1_argv	= (char **) xcalloc (sizeof (char *), argc+2);
  char **ld1		= ld1_argv;
  char **ld2_argv	= (char **) xcalloc (sizeof (char *), argc+5);
  char **ld2		= ld2_argv;
  int first_file;
  int num_c_args	= argc+7;
  int len;
  int clen;

#ifdef DEBUG
  debug = 1;
  vflag = 1;
#endif

  p = (char *) getenv ("COLLECT_GCC_OPTIONS");
  if (p)
    while (*p)
      {
	char *q = p;
	while (*q && *q != ' ') q++;
	if (*p == '-' && p[1] == 'm')
	  num_c_args++;

	if (*q) q++;
	p = q;
      }

  c_ptr = c_argv = (char **) xcalloc (sizeof (char *), num_c_args);

  if (argc < 2)
    fatal ("no arguments");

  if (signal (SIGQUIT, SIG_IGN) != SIG_IGN)
    signal (SIGQUIT, handler);
  if (signal (SIGINT, SIG_IGN) != SIG_IGN)
    signal (SIGINT, handler);
  if (signal (SIGALRM, SIG_IGN) != SIG_IGN)
    signal (SIGALRM, handler);
  if (signal (SIGHUP, SIG_IGN) != SIG_IGN)
    signal (SIGHUP, handler);
  if (signal (SIGSEGV, SIG_IGN) != SIG_IGN)
    signal (SIGSEGV, handler);
  if (signal (SIGBUS, SIG_IGN) != SIG_IGN)
    signal (SIGBUS, handler);

  /* Try to discover a valid linker/assembler/nm/strip to use.  */
  len = strlen (argv[0]);
  prefix = (char *)0;
  if (len >= sizeof ("ld")-1)
    {
      p = argv[0] + len - sizeof ("ld") + 1;
      if (strcmp (p, "ld") == 0)
	{
	  prefix = argv[0];
	  *p = '\0';
	}
    }

  if (prefix == (char *)0)
    {
      p = rindex (argv[0], '/');
      if (p != (char *)0)
	{
	  prefix = argv[0];
	  p[1] = '\0';
	}

#ifdef STANDARD_EXEC_PREFIX
      else if (access (STANDARD_EXEC_PREFIX, X_OK) == 0)
	prefix = STANDARD_EXEC_PREFIX;
#endif

#ifdef MD_EXEC_PREFIX
      else if (access (MD_EXEC_PREFIX, X_OK) == 0)
	prefix = MD_EXEC_PREFIX;
#endif

      else if (access ("/usr/ccs/gcc", X_OK) == 0)
	prefix = "/usr/ccs/gcc/";

      else if (access ("/usr/ccs/bin", X_OK) == 0)
	prefix = "/usr/ccs/bin/";

      else
	prefix = "/bin/";
    }

  clen = len = strlen (prefix);

#ifdef STANDARD_BIN_PREFIX
  if (clen < sizeof (STANDARD_BIN_PREFIX) - 1)
    clen = sizeof (STANDARD_BIN_PREFIX) - 1;
#endif

#ifdef STANDARD_EXEC_PREFIX
  if (clen < sizeof (STANDARD_EXEC_PREFIX) - 1)
    clen = sizeof (STANDARD_EXEC_PREFIX) - 1;
#endif

  /* Allocate enough string space for the longest possible pathnames.  */
  ld_file_name = xcalloc (len + sizeof ("real-ld"), 1);
  nm_file_name = xcalloc (len + sizeof ("gnm"), 1);
  strip_file_name = xcalloc (len + sizeof ("gstrip"), 1);

  /* Determine the full path name of the ld program to use.  */
  bcopy (prefix, ld_file_name, len);
  strcpy (ld_file_name + len, "real-ld");
  if (access (ld_file_name, X_OK) < 0)
    {
      strcpy (ld_file_name + len, "gld");
      if (access (ld_file_name, X_OK) < 0)
	{
	  free (ld_file_name);
#ifdef REAL_LD_FILE_NAME
	  ld_file_name = REAL_LD_FILE_NAME;
#else
	  ld_file_name = (access ("/usr/bin/ld", X_OK) == 0
			  ? "/usr/bin/ld" : "/bin/ld");
#endif
	}
    }

  /* Determine the full path name of the C compiler to use.  */
  c_file_name = getenv ("COLLECT_GCC");
  /* If this is absolute, it must be a file that exists.
     If it is relative, it must be something that execvp was able to find.
     Either way, we can pass it to execvp and find the same executable.  */
  if (c_file_name == 0)
    {
      c_file_name = xcalloc (clen + sizeof ("gcc"), 1);
      bcopy (prefix, c_file_name, len);
      strcpy (c_file_name + len, "gcc");
      if (access (c_file_name, X_OK) < 0)
	{
#ifdef STANDARD_BIN_PREFIX
	  strcpy (c_file_name, STANDARD_BIN_PREFIX);
	  strcat (c_file_name, "gcc");
	  if (access (c_file_name, X_OK) < 0)
#endif
	    {
#ifdef STANDARD_EXEC_PREFIX
	      strcpy (c_file_name, STANDARD_EXEC_PREFIX);
	      strcat (c_file_name, "gcc");
	      if (access (c_file_name, X_OK) < 0)
#endif
		{
		  strcpy (c_file_name, "gcc");
		}
	    }
	}
    }

  /* Determine the full path name of the nm to use.  */
  bcopy (prefix, nm_file_name, len);
  strcpy (nm_file_name + len, "nm");
  if (access (nm_file_name, X_OK) < 0)
    {
      strcpy (nm_file_name + len, "gnm");
      if (access (nm_file_name, X_OK) < 0)
	{
	  free (nm_file_name);
#ifdef REAL_NM_FILE_NAME
	  nm_file_name = REAL_NM_FILE_NAME;
#else
	  nm_file_name = (access ("/usr/bin/nm", X_OK) == 0
			  ? "/usr/bin/nm" : "/bin/nm");
#endif
	}
    }

  /* Determine the full pathname of the strip to use.  */
  bcopy (prefix, strip_file_name, len);
  strcpy (strip_file_name + len, "strip");
  if (access (strip_file_name, X_OK) < 0)
    {
      strcpy (strip_file_name + len, "gstrip");
      if (access (strip_file_name, X_OK) < 0)
	{
	  free (strip_file_name);
#ifdef REAL_STRIP_FILE_NAME
	  strip_file_name = REAL_STRIP_FILE_NAME;
#else
	  strip_file_name = (access ("/usr/bin/strip", X_OK) == 0
			     ? "/usr/bin/strip" : "/bin/strip");
#endif
	}
    }

  *ld1++ = *ld2++ = "ld";

  /* Make temp file names. */
  choose_temp_base ();
  c_file = xcalloc (temp_filename_length + sizeof (".c"), 1);
  o_file = xcalloc (temp_filename_length + sizeof (".o"), 1);
  sprintf (c_file, "%s.c", temp_filename);
  sprintf (o_file, "%s.o", temp_filename);
  *c_ptr++ = c_file_name;
  *c_ptr++ = "-c";
  *c_ptr++ = "-o";
  *c_ptr++ = o_file;

  /* !!! When GCC calls collect2,
     it does not know whether it is calling collect2 or ld.
     So collect2 cannot meaningfully understand any options
     except those ld understands.
     If you propose to make GCC pass some other option,
     just imagine what will happen if ld is really ld!!!  */

  /* Parse arguments.  Remember output file spec, pass the rest to ld. */
  /* After the first file, put in the c++ rt0.  */

  first_file = 1;
  while ((arg = *++argv) != (char *)0)
    {
      *ld1++ = *ld2++ = arg;

      if (arg[0] == '-')
	  switch (arg[1])
	    {
	    case 'd':
	      if (!strcmp (arg, "-debug"))
		{
		  debug = 1;
		  vflag = 1;
		  ld1--;
		  ld2--;
		}
	      break;

	    case 'o':
	      outfile = (arg[2] == '\0') ? argv[1] : &arg[2];
	      break;

	    case 'r':
	      if (arg[2] == '\0')
		rflag = 1;
	      break;

	    case 's':
	      if (arg[2] == '\0')
		{
		  /* We must strip after the nm run, otherwise C++ linking
		     won't work.  Thus we strip in the second ld run, or
		     else with strip if there is no second ld run.  */
		  strip_flag = 1;
		  ld1--;
		}
	      break;

	    case 'v':
	      if (arg[2] == '\0')
		vflag = 1;
	      break;
	    }

      else if (first_file
	       && (p = rindex (arg, '.')) != (char *)0
	       && strcmp (p, ".R") == 0)
	{
	  first_file = 0;
	  *ld2++ = o_file;
	}
    }

  /* Get any options that the upper GCC wants to pass to the sub-GCC.  */
  p = (char *) getenv ("COLLECT_GCC_OPTIONS");
  if (p)
    while (*p)
      {
	char *q = p;
	while (*q && *q != ' ') q++;
	if (*p == '-' && (p[1] == 'm' || p[1] == 'f'))
	  *c_ptr++ = savestring (p, q - p);

	if (*q) q++;
	p = q;
      }

  *c_ptr++ = c_file;
  *c_ptr = *ld1 = *ld2 = (char *)0;

  if (vflag)
    {
      fprintf (stderr, "collect2 version %s", version_string);
#ifdef TARGET_VERSION
      TARGET_VERSION;
#endif
      fprintf (stderr, "\n");
    }

  if (debug)
    {
      char *ptr;
      fprintf (stderr, "prefix              = %s\n", prefix);
      fprintf (stderr, "ld_file_name        = %s\n", ld_file_name);
      fprintf (stderr, "c_file_name         = %s\n", c_file_name);
      fprintf (stderr, "nm_file_name        = %s\n", nm_file_name);
      fprintf (stderr, "c_file              = %s\n", c_file);
      fprintf (stderr, "o_file              = %s\n", o_file);

      ptr = getenv ("COLLECT_GCC_OPTIONS");
      if (ptr)
	fprintf (stderr, "COLLECT_GCC_OPTIONS = %s\n", ptr);

      ptr = getenv ("COLLECT_GCC");
      if (ptr)
	fprintf (stderr, "COLLECT_GCC         = %s\n", ptr);

      ptr = getenv ("COMPILER_PATH");
      if (ptr)
	fprintf (stderr, "COMPILER_PATH       = %s\n", ptr);

      ptr = getenv ("LIBRARY_PATH");
      if (ptr)
	fprintf (stderr, "LIBRARY_PATH        = %s\n", ptr);

      fprintf (stderr, "\n");
    }

  /* Load the program, searching all libraries.
     Examine the namelist with nm and search it for static constructors
     and destructors to call.
     Write the constructor and destructor tables to a .s file and reload. */

  fork_execute (ld_file_name, ld1_argv);

  /* If -r, don't build the constructor or destructor list, just return now.  */
  if (rflag)
    return 0;

  scan_prog_file (outfile, PASS_FIRST);

  if (debug)
    {
      fprintf (stderr, "%d constructor(s) found\n", constructors.number);
      fprintf (stderr, "%d destructor(s)  found\n", destructors.number);
    }

  if (constructors.number == 0 && destructors.number == 0)
    {
      /* Strip now if it was requested on the command line.  */
      if (strip_flag)
	{
	  char **strip_argv = (char **) xcalloc (sizeof (char *), 3);
	  strip_argv[0] = "strip";
	  strip_argv[1] = outfile;
	  strip_argv[2] = (char *) 0;
	  fork_execute (strip_file_name, strip_argv);
	}
      return 0;
    }

  outf = fopen (c_file, "w");
  if (outf == (FILE *)0)
    fatal_perror ("%s", c_file);

  write_c_file (outf, c_file);

  if (fclose (outf))
    fatal_perror ("closing %s", c_file);

  if (debug)
    {
      fprintf (stderr, "\n========== outfile = %s, c_file = %s\n", outfile, c_file);
      write_c_file (stderr, "stderr");
      fprintf (stderr, "========== end of c_file\n\n");
    }

  /* Assemble the constructor and destructor tables.
     Link the tables in with the rest of the program. */

  fork_execute (c_file_name,  c_argv);
  fork_execute (ld_file_name, ld2_argv);

  /* Let scan_prog_file do any final mods (OSF/rose needs this for
     constructors/destructors in shared libraries.  */
  scan_prog_file (outfile, PASS_SECOND);

  maybe_unlink (c_file);
  maybe_unlink (o_file);
  return 0;
}


/* Wait for a process to finish, and exit if a non-zero status is found. */

static void
do_wait (prog)
     char *prog;
{
  int status;

  wait (&status);
  if (status)
    {
      int sig = status & 0x7F;
      int ret;

      if (sig != -1 && sig != 0)
	{
#ifdef NO_SYS_SIGLIST
	  error ("%s terminated with signal %d %s",
		 prog,
		 sig,
		 (status & 0200) ? ", core dumped" : "");
#else
	  error ("%s terminated with signal %d [%s]%s",
		 prog,
		 sig,
		 sys_siglist[sig],
		 (status & 0200) ? ", core dumped" : "");
#endif

	  my_exit (127);
	}

      ret = ((status & 0xFF00) >> 8);
      if (ret != -1 && ret != 0)
	{
	  error ("%s returned %d exit status", prog, ret);
	  my_exit (ret);
	}
    }
}


/* Fork and execute a program, and wait for the reply.  */

static void
fork_execute (prog, argv)
     char *prog;
     char **argv;
{
  int pid;

  if (vflag || debug)
    {
      char **p_argv;
      char *str;

      fprintf (stderr, "%s", prog);
      for (p_argv = &argv[1]; (str = *p_argv) != (char *)0; p_argv++)
	fprintf (stderr, " %s", str);

      fprintf (stderr, "\n");
    }

  fflush (stdout);
  fflush (stderr);

  pid = vfork ();
  if (pid == -1)
    fatal_perror ("vfork");

  if (pid == 0)			/* child context */
    {
      execvp (prog, argv);
      fatal_perror ("executing %s", prog);
    }

  do_wait (prog);
}


/* Unlink a file unless we are debugging.  */

static void
maybe_unlink (file)
     char *file;
{
  if (!debug)
    unlink (file);
  else
    fprintf (stderr, "[Leaving %s]\n", file);
}


/* Add a name to a linked list.  */

static void
add_to_list (head_ptr, name)
     struct head *head_ptr;
     char *name;
{
  struct id *newid = (struct id *) xcalloc (sizeof (*newid) + strlen (name), 1);
  static long sequence_number = 0;
  newid->sequence = ++sequence_number;
  strcpy (newid->name, name);

  if (head_ptr->first)
    head_ptr->last->next = newid;
  else
    head_ptr->first = newid;

  head_ptr->last = newid;
  head_ptr->number++;
}

/* Write: `prefix', the names on list LIST, `suffix'.  */

static void
write_list (stream, prefix, list)
     FILE *stream;
     char *prefix;
     struct id *list;
{
  while (list)
    {
      fprintf (stream, "%sx%d,\n", prefix, list->sequence);
      list = list->next;
    }
}

static void
write_list_with_asm (stream, prefix, list)
     FILE *stream;
     char *prefix;
     struct id *list;
{
  while (list)
    {
      fprintf (stream, "%sx%d asm (\"%s\");\n",
	       prefix, list->sequence, list->name);
      list = list->next;
    }
}

/* Write the constructor/destructor tables. */

static void
write_c_file (stream, name)
     FILE *stream;
     char *name;
{
  /* Write the tables as C code  */

  fprintf (stream, "typedef void entry_pt();\n\n");
    
  write_list_with_asm (stream, "extern entry_pt ", constructors.first);
    
  fprintf (stream, "\nentry_pt * __CTOR_LIST__[] = {\n");
  fprintf (stream, "\t(entry_pt *) %d,\n", constructors.number);
  write_list (stream, "\t", constructors.first);
  fprintf (stream, "\t0\n};\n\n");

  write_list_with_asm (stream, "extern entry_pt ", destructors.first);

  fprintf (stream, "\nentry_pt * __DTOR_LIST__[] = {\n");
  fprintf (stream, "\t(entry_pt *) %d,\n", destructors.number);
  write_list (stream, "\t", destructors.first);
  fprintf (stream, "\t0\n};\n\n");

  fprintf (stream, "extern entry_pt __main;\n");
  fprintf (stream, "entry_pt *__main_reference = __main;\n\n");
}


#ifdef OBJECT_FORMAT_NONE

/* Generic version to scan the name list of the loaded program for
   the symbols g++ uses for static constructors and destructors.

   The constructor table begins at __CTOR_LIST__ and contains a count
   of the number of pointers (or -1 if the constructors are built in a
   separate section by the linker), followed by the pointers to the
   constructor functions, terminated with a null pointer.  The
   destructor table has the same format, and begins at __DTOR_LIST__.  */

static void
scan_prog_file (prog_name, which_pass)
     char *prog_name;
     enum pass which_pass;
{
  void (*int_handler) ();
  void (*quit_handler) ();
  char *nm_argv[4];
  int pid;
  int argc = 0;
  int pipe_fd[2];
  char *p, buf[1024];
  FILE *inf;

  if (which_pass != PASS_FIRST)
    return;

  nm_argv[argc++] = "nm";
  if (NM_FLAGS[0] != '\0')
    nm_argv[argc++] = NM_FLAGS;

  nm_argv[argc++] = prog_name;
  nm_argv[argc++] = (char *)0;

  if (pipe (pipe_fd) < 0)
    fatal_perror ("pipe");

  inf = fdopen (pipe_fd[0], "r");
  if (inf == (FILE *)0)
    fatal_perror ("fdopen");

  /* Trace if needed.  */
  if (vflag)
    {
      char **p_argv;
      char *str;

      fprintf (stderr, "%s", nm_file_name);
      for (p_argv = &nm_argv[1]; (str = *p_argv) != (char *)0; p_argv++)
	fprintf (stderr, " %s", str);

      fprintf (stderr, "\n");
    }

  fflush (stdout);
  fflush (stderr);

  /* Spawn child nm on pipe */
  pid = vfork ();
  if (pid == -1)
    fatal_perror ("vfork");

  if (pid == 0)			/* child context */
    {
      /* setup stdout */
      if (dup2 (pipe_fd[1], 1) < 0)
	fatal_perror ("dup2 (%d, 1)", pipe_fd[1]);

      if (close (pipe_fd[0]) < 0)
	fatal_perror ("close (%d)", pipe_fd[0]);

      if (close (pipe_fd[1]) < 0)
	fatal_perror ("close (%d)", pipe_fd[1]);

      execv (nm_file_name, nm_argv);
      fatal_perror ("executing %s", nm_file_name);
    }

  /* Parent context from here on.  */
  int_handler  = (void (*) ())signal (SIGINT,  SIG_IGN);
  quit_handler = (void (*) ())signal (SIGQUIT, SIG_IGN);

  if (close (pipe_fd[1]) < 0)
    fatal_perror ("close (%d)", pipe_fd[1]);

  if (debug)
    fprintf (stderr, "\nnm output with constructors/destructors.\n");

  /* Read each line of nm output.  */
  while (fgets (buf, sizeof buf, inf) != (char *)0)
    {
      int ch, ch2;
      char *name, *end;

      /* If it contains a constructor or destructor name, add the name
	 to the appropriate list. */

      for (p = buf; (ch = *p) != '\0' && ch != '\n' && ch != '_'; p++)
	;

      if (ch == '\0' || ch == '\n')
	continue;
  
      name = p;
      /* Find the end of the symbol name.
	 Don't include `|', because Encore nm can tack that on the end.  */
      for (end = p; (ch2 = *end) != '\0' && !isspace (ch2) && ch2 != '|';
	   end++)
	continue;

      *end = '\0';
      switch (is_ctor_dtor (name))
	{
	case 1:
	  add_to_list (&constructors, name);
	  break;

	case 2:
	  add_to_list (&destructors, name);
	  break;

	default:		/* not a constructor or destructor */
	  continue;
	}

      if (debug)
	fprintf (stderr, "\t%s\n", buf);
    }

  if (debug)
    fprintf (stderr, "\n");

  if (fclose (inf) != 0)
    fatal_perror ("fclose of pipe");

  do_wait (nm_file_name);

  signal (SIGINT,  int_handler);
  signal (SIGQUIT, quit_handler);
}

#endif /* OBJECT_FORMAT_NONE */


/*
 * COFF specific stuff.
 */

#ifdef OBJECT_FORMAT_COFF

#if defined(EXTENDED_COFF)
#   define GCC_SYMBOLS(X)	(SYMHEADER(X).isymMax + SYMHEADER(X).iextMax)
#   define GCC_SYMENT		SYMR
#   define GCC_OK_SYMBOL(X)	((X).st == stProc && (X).sc == scText)
#   define GCC_SYMINC(X)	(1)
#   define GCC_SYMZERO(X)	(SYMHEADER(X).isymMax)
#   define GCC_CHECK_HDR(X)	(PSYMTAB(X) != 0)
#else
#   define GCC_SYMBOLS(X)	(HEADER(ldptr).f_nsyms)
#   define GCC_SYMENT		SYMENT
#   define GCC_OK_SYMBOL(X) \
     (((X).n_sclass == C_EXT) && \
        (((X).n_type & N_TMASK) == (DT_NON << N_BTSHFT) || \
         ((X).n_type & N_TMASK) == (DT_FCN << N_BTSHFT)))
#   define GCC_SYMINC(X)	((X).n_numaux+1)
#   define GCC_SYMZERO(X)	0
#   define GCC_CHECK_HDR(X)	(1)
#endif

extern char *ldgetname ();

/* COFF version to scan the name list of the loaded program for
   the symbols g++ uses for static constructors and destructors.

   The constructor table begins at __CTOR_LIST__ and contains a count
   of the number of pointers (or -1 if the constructors are built in a
   separate section by the linker), followed by the pointers to the
   constructor functions, terminated with a null pointer.  The
   destructor table has the same format, and begins at __DTOR_LIST__.  */

static void
scan_prog_file (prog_name, which_pass)
     char *prog_name;
     enum pass which_pass;
{
  LDFILE *ldptr = NULL;
  int sym_index, sym_count;

  if (which_pass != PASS_FIRST)
    return;

  if ((ldptr = ldopen (prog_name, ldptr)) == NULL)
    fatal ("%s: can't open as COFF file", prog_name);
      
  if (!MY_ISCOFF (HEADER (ldptr).f_magic))
    fatal ("%s: not a COFF file", prog_name);

  if (GCC_CHECK_HDR (ldptr))
    {
      sym_count = GCC_SYMBOLS (ldptr);
      sym_index = GCC_SYMZERO (ldptr);
      while (sym_index < sym_count)
	{
	  GCC_SYMENT symbol;

	  if (ldtbread (ldptr, sym_index, &symbol) <= 0)
	    break;
	  sym_index += GCC_SYMINC (symbol);

	  if (GCC_OK_SYMBOL (symbol))
	    {
	      char *name;

	      if ((name = ldgetname (ldptr, &symbol)) == NULL)
		continue;		/* should never happen */

#ifdef _AIX
	      /* All AIX function names begin with a dot. */
	      if (*name++ != '.')
		continue;
#endif

	      switch (is_ctor_dtor (name))
		{
		case 1:
		  add_to_list (&constructors, name);
		  break;

		case 2:
		  add_to_list (&destructors, name);
		  break;

		default:		/* not a constructor or destructor */
		  continue;
		}

#if !defined(EXTENDED_COFF)
	      if (debug)
		fprintf (stderr, "\tsec=%d class=%d type=%s%o %s\n",
			 symbol.n_scnum, symbol.n_sclass,
			 (symbol.n_type ? "0" : ""), symbol.n_type,
			 name);
#else
	      if (debug)
		fprintf (stderr, "\tiss = %5d, value = %5d, index = %5d, name = %s\n",
			 symbol.iss, symbol.value, symbol.index, name);
#endif
	    }
	}
    }

  (void) ldclose(ldptr);
}

#endif /* OBJECT_FORMAT_COFF */


/*
 * OSF/rose specific stuff.
 */

#ifdef OBJECT_FORMAT_ROSE

/* Union of the various load commands */

typedef union load_union
{
  ldc_header_t			hdr;	/* common header */
  load_cmd_map_command_t	map;	/* map indexing other load cmds */
  interpreter_command_t		iprtr;	/* interpreter pathname */
  strings_command_t		str;	/* load commands strings section */
  region_command_t		region;	/* region load command */
  reloc_command_t		reloc;	/* relocation section */
  package_command_t		pkg;	/* package load command */
  symbols_command_t		sym;	/* symbol sections */
  entry_command_t		ent;	/* program start section */
  gen_info_command_t		info;	/* object information */
  func_table_command_t		func;	/* function constructors/destructors */
} load_union_t;

/* Structure to point to load command and data section in memory.  */

typedef struct load_all
{
  load_union_t *load;			/* load command */
  char *section;			/* pointer to section */
} load_all_t;

/* Structure to contain information about a file mapped into memory.  */

struct file_info
{
  char *start;				/* start of map */
  char *name;				/* filename */
  long	size;				/* size of the file */
  long  rounded_size;			/* size rounded to page boundary */
  int	fd;				/* file descriptor */
  int	rw;				/* != 0 if opened read/write */
  int	use_mmap;			/* != 0 if mmap'ed */
};

extern int decode_mach_o_hdr ();

extern int encode_mach_o_hdr ();

static void bad_header ();

static void print_header ();

static void print_load_command ();

static void add_func_table ();

static struct file_info	*read_file ();

static void end_file ();


/* OSF/rose specific version to scan the name list of the loaded
   program for the symbols g++ uses for static constructors and
   destructors.

   The constructor table begins at __CTOR_LIST__ and contains a count
   of the number of pointers (or -1 if the constructors are built in a
   separate section by the linker), followed by the pointers to the
   constructor functions, terminated with a null pointer.  The
   destructor table has the same format, and begins at __DTOR_LIST__.  */

static void
scan_prog_file (prog_name, which_pass)
     char *prog_name;
     enum pass which_pass;
{
  char *obj;
  mo_header_t hdr;
  load_all_t *load_array;
  load_all_t *load_end;
  load_all_t *load_cmd;
  int symbol_load_cmds;
  off_t offset;
  int i;
  int num_syms;
  int status;
  char *str_sect;
  struct file_info *obj_file;
  int prog_fd;
  mo_lcid_t cmd_strings	  = -1;
  symbol_info_t *main_sym = 0;
  int rw		  = (which_pass != PASS_FIRST);

  prog_fd = open (prog_name, (rw) ? O_RDWR : O_RDONLY);
  if (prog_fd < 0)
    fatal_perror ("can't read %s", prog_name);

  obj_file = read_file (prog_name, prog_fd, rw);
  obj = obj_file->start;

  status = decode_mach_o_hdr (obj, MO_SIZEOF_RAW_HDR, MOH_HEADER_VERSION, &hdr);
  if (status != MO_HDR_CONV_SUCCESS)
    bad_header (status);


  /* Do some basic sanity checks.  Note we explicitly use the big endian magic number,
     since the hardware will automatically swap bytes for us on loading little endian
     integers.  */

#ifndef CROSS_COMPILE
  if (hdr.moh_magic != MOH_MAGIC_MSB
      || hdr.moh_header_version != MOH_HEADER_VERSION
      || hdr.moh_byte_order != OUR_BYTE_ORDER
      || hdr.moh_data_rep_id != OUR_DATA_REP_ID
      || hdr.moh_cpu_type != OUR_CPU_TYPE
      || hdr.moh_cpu_subtype != OUR_CPU_SUBTYPE
      || hdr.moh_vendor_type != OUR_VENDOR_TYPE)
    {
      fatal ("incompatibilities between object file & expected values");
    }
#endif

  if (debug)
    print_header (&hdr);

  offset = hdr.moh_first_cmd_off;
  load_end = load_array
    = (load_all_t *) xcalloc (sizeof (load_all_t), hdr.moh_n_load_cmds + 2);

  /* Build array of load commands, calculating the offsets */
  for (i = 0; i < hdr.moh_n_load_cmds; i++)
    {
      load_union_t *load_hdr;		/* load command header */

      load_cmd = load_end++;
      load_hdr = (load_union_t *) (obj + offset);

      /* If modifying the program file, copy the header.  */
      if (rw)
	{
	  load_union_t *ptr = (load_union_t *) xmalloc (load_hdr->hdr.ldci_cmd_size);
	  bcopy ((generic *)load_hdr, (generic *)ptr, load_hdr->hdr.ldci_cmd_size);
	  load_hdr = ptr;

	  /* null out old command map, because we will rewrite at the end.  */
	  if (ptr->hdr.ldci_cmd_type == LDC_CMD_MAP)
	    {
	      cmd_strings = ptr->map.lcm_ld_cmd_strings;
	      ptr->hdr.ldci_cmd_type = LDC_UNDEFINED;
	    }
	}

      load_cmd->load = load_hdr;
      if (load_hdr->hdr.ldci_section_off > 0)
	load_cmd->section = obj + load_hdr->hdr.ldci_section_off;

      if (debug)
	print_load_command (load_hdr, offset, i);

      offset += load_hdr->hdr.ldci_cmd_size;
    }

  /* If the last command is the load command map and is not undefined,
     decrement the count of load commands.  */
  if (rw && load_end[-1].load->hdr.ldci_cmd_type == LDC_UNDEFINED)
    {
      load_end--;
      hdr.moh_n_load_cmds--;
    }

  /* Go through and process each symbol table section.  */
  symbol_load_cmds = 0;
  for (load_cmd = load_array; load_cmd < load_end; load_cmd++)
    {
      load_union_t *load_hdr = load_cmd->load;

      if (load_hdr->hdr.ldci_cmd_type == LDC_SYMBOLS)
	{
	  symbol_load_cmds++;

	  if (debug)
	    {
	      char *kind = "unknown";

	      switch (load_hdr->sym.symc_kind)
		{
		case SYMC_IMPORTS:	   kind = "imports"; break;
		case SYMC_DEFINED_SYMBOLS: kind = "defined"; break;
		case SYMC_STABS:	   kind = "stabs";   break;
		}

	      fprintf (stderr, "\nProcessing symbol table #%d, offset = 0x%.8lx, kind = %s\n",
		       symbol_load_cmds, load_hdr->hdr.ldci_section_off, kind);
	    }

	  if (load_hdr->sym.symc_kind != SYMC_DEFINED_SYMBOLS)
	    continue;

	  str_sect = load_array[load_hdr->sym.symc_strings_section].section;
	  if (str_sect == (char *)0)
	    fatal ("string section missing");

	  if (load_cmd->section == (char *)0)
	    fatal ("section pointer missing");

	  num_syms = load_hdr->sym.symc_nentries;
	  for (i = 0; i < num_syms; i++)
	    {
	      symbol_info_t *sym = ((symbol_info_t *) load_cmd->section) + i;
	      char *name = sym->si_name.symbol_name + str_sect;

	      if (name[0] != '_')
		continue;

	      if (rw)
		{
		  char *n = name;
		  while (*n == '_')
		    ++n;
		  if (*n != 'm' || (n - name) < 2 || strcmp (n, "main"))
		    continue;

		  main_sym = sym;
		}
	      else
		{
		  switch (is_ctor_dtor (name))
		    {
		    case 1:
		      add_to_list (&constructors, name);
		      break;

		    case 2:
		      add_to_list (&destructors, name);
		      break;

		    default:	/* not a constructor or destructor */
		      continue;
		    }
		}

	      if (debug)
		fprintf (stderr, "\ttype = 0x%.4x, sc = 0x%.2x, flags = 0x%.8x, name = %.30s\n",
			 sym->si_type, sym->si_sc_type, sym->si_flags, name);
	    }
	}
    }

  if (symbol_load_cmds == 0)
    fatal ("no symbol table found");

  /* Update the program file now, rewrite header and load commands.  At present,
     we assume that there is enough space after the last load command to insert
     one more.  Since the first section written out is page aligned, and the
     number of load commands is small, this is ok for the present.  */

  if (rw)
    {
      load_union_t *load_map;
      size_t size;

      if (cmd_strings == -1)
	fatal ("no cmd_strings found");

      /* Add __main to initializer list.
	 If we are building a program instead of a shared library, don't
	 do anything, since in the current version, you cannot do mallocs
	 and such in the constructors.  */

      if (main_sym != (symbol_info_t *)0
	  && ((hdr.moh_flags & MOH_EXECABLE_F) == 0))
	add_func_table (&hdr, load_array, main_sym, FNTC_INITIALIZATION);

      if (debug)
	fprintf (stderr, "\nUpdating header and load commands.\n\n");

      hdr.moh_n_load_cmds++;
      size = sizeof (load_cmd_map_command_t) + (sizeof (mo_offset_t) * (hdr.moh_n_load_cmds - 1));

      /* Create new load command map.  */
      if (debug)
	fprintf (stderr, "load command map, %d cmds, new size %ld.\n",
		 (int)hdr.moh_n_load_cmds, (long)size);

      load_map = (load_union_t *) xcalloc (1, size);
      load_map->map.ldc_header.ldci_cmd_type = LDC_CMD_MAP;
      load_map->map.ldc_header.ldci_cmd_size = size;
      load_map->map.lcm_ld_cmd_strings = cmd_strings;
      load_map->map.lcm_nentries = hdr.moh_n_load_cmds;
      load_array[hdr.moh_n_load_cmds-1].load = load_map;

      offset = hdr.moh_first_cmd_off;
      for (i = 0; i < hdr.moh_n_load_cmds; i++)
	{
	  load_map->map.lcm_map[i] = offset;
	  if (load_array[i].load->hdr.ldci_cmd_type == LDC_CMD_MAP)
	    hdr.moh_load_map_cmd_off = offset;

	  offset += load_array[i].load->hdr.ldci_cmd_size;
	}

      hdr.moh_sizeofcmds = offset - MO_SIZEOF_RAW_HDR;

      if (debug)
	print_header (&hdr);

      /* Write header */
      status = encode_mach_o_hdr (&hdr, obj, MO_SIZEOF_RAW_HDR);
      if (status != MO_HDR_CONV_SUCCESS)
	bad_header (status);

      if (debug)
	fprintf (stderr, "writing load commands.\n\n");

      /* Write load commands */
      offset = hdr.moh_first_cmd_off;
      for (i = 0; i < hdr.moh_n_load_cmds; i++)
	{
	  load_union_t *load_hdr = load_array[i].load;
	  size_t size = load_hdr->hdr.ldci_cmd_size;

	  if (debug)
	    print_load_command (load_hdr, offset, i);

	  bcopy ((generic *)load_hdr, (generic *)(obj + offset), size);
	  offset += size;
	}
    }

  end_file (obj_file);

  if (close (prog_fd))
    fatal_perror ("closing %s", prog_name);

  if (debug)
    fprintf (stderr, "\n");
}


/* Add a function table to the load commands to call a function
   on initiation or termination of the process.  */

static void
add_func_table (hdr_p, load_array, sym, type)
     mo_header_t *hdr_p;		/* pointer to global header */
     load_all_t *load_array;		/* array of ptrs to load cmds */
     symbol_info_t *sym;		/* pointer to symbol entry */
     int type;				/* fntc_type value */
{
  /* Add a new load command.  */
  int num_cmds = ++hdr_p->moh_n_load_cmds;
  int load_index = num_cmds - 1;
  size_t size = sizeof (func_table_command_t) + sizeof (mo_addr_t);
  load_union_t *ptr = xcalloc (1, size);
  load_all_t *load_cmd;
  int i;

  /* Set the unresolved address bit in the header to force the loader to be
     used, since kernel exec does not call the initialization functions.  */
  hdr_p->moh_flags |= MOH_UNRESOLVED_F;

  load_cmd = &load_array[load_index];
  load_cmd->load = ptr;
  load_cmd->section = (char *)0;

  /* Fill in func table load command.  */
  ptr->func.ldc_header.ldci_cmd_type = LDC_FUNC_TABLE;
  ptr->func.ldc_header.ldci_cmd_size = size;
  ptr->func.ldc_header.ldci_section_off = 0;
  ptr->func.ldc_header.ldci_section_len = 0;
  ptr->func.fntc_type = type;
  ptr->func.fntc_nentries = 1;

  /* copy address, turn it from abs. address to (region,offset) if necessary.  */
  /* Is the symbol already expressed as (region, offset)?  */
  if ((sym->si_flags & SI_ABSOLUTE_VALUE_F) == 0)
    {
      ptr->func.fntc_entry_loc[i].adr_lcid = sym->si_value.def_val.adr_lcid;
      ptr->func.fntc_entry_loc[i].adr_sctoff = sym->si_value.def_val.adr_sctoff;
    }

  /* If not, figure out which region it's in.  */
  else
    {
      mo_vm_addr_t addr = sym->si_value.abs_val;
      int found = 0;

      for (i = 0; i < load_index; i++)
	{
	  if (load_array[i].load->hdr.ldci_cmd_type == LDC_REGION)
	    {
	      region_command_t *region_ptr = &load_array[i].load->region;

	      if ((region_ptr->regc_flags & REG_ABS_ADDR_F) != 0
		  && addr >= region_ptr->regc_addr.vm_addr
		  && addr <= region_ptr->regc_addr.vm_addr + region_ptr->regc_vm_size)
		{
		  ptr->func.fntc_entry_loc[0].adr_lcid = i;
		  ptr->func.fntc_entry_loc[0].adr_sctoff = addr - region_ptr->regc_addr.vm_addr;
		  found++;
		  break;
		}
	    }
	}

      if (!found)
	fatal ("could not convert 0x%l.8x into a region", addr);
    }

  if (debug)
    fprintf (stderr,
	     "%s function, region %d, offset = %ld (0x%.8lx)\n",
	     (type == FNTC_INITIALIZATION) ? "init" : "term",
	     (int)ptr->func.fntc_entry_loc[i].adr_lcid,
	     (long)ptr->func.fntc_entry_loc[i].adr_sctoff,
	     (long)ptr->func.fntc_entry_loc[i].adr_sctoff);

}


/* Print the global header for an OSF/rose object.  */

static void
print_header (hdr_ptr)
     mo_header_t *hdr_ptr;
{
  fprintf (stderr, "\nglobal header:\n");
  fprintf (stderr, "\tmoh_magic            = 0x%.8lx\n", hdr_ptr->moh_magic);
  fprintf (stderr, "\tmoh_major_version    = %d\n", (int)hdr_ptr->moh_major_version);
  fprintf (stderr, "\tmoh_minor_version    = %d\n", (int)hdr_ptr->moh_minor_version);
  fprintf (stderr, "\tmoh_header_version   = %d\n", (int)hdr_ptr->moh_header_version);
  fprintf (stderr, "\tmoh_max_page_size    = %d\n", (int)hdr_ptr->moh_max_page_size);
  fprintf (stderr, "\tmoh_byte_order       = %d\n", (int)hdr_ptr->moh_byte_order);
  fprintf (stderr, "\tmoh_data_rep_id      = %d\n", (int)hdr_ptr->moh_data_rep_id);
  fprintf (stderr, "\tmoh_cpu_type         = %d\n", (int)hdr_ptr->moh_cpu_type);
  fprintf (stderr, "\tmoh_cpu_subtype      = %d\n", (int)hdr_ptr->moh_cpu_subtype);
  fprintf (stderr, "\tmoh_vendor_type      = %d\n", (int)hdr_ptr->moh_vendor_type);
  fprintf (stderr, "\tmoh_load_map_cmd_off = %d\n", (int)hdr_ptr->moh_load_map_cmd_off);
  fprintf (stderr, "\tmoh_first_cmd_off    = %d\n", (int)hdr_ptr->moh_first_cmd_off);
  fprintf (stderr, "\tmoh_sizeofcmds       = %d\n", (int)hdr_ptr->moh_sizeofcmds);
  fprintf (stderr, "\tmon_n_load_cmds      = %d\n", (int)hdr_ptr->moh_n_load_cmds);
  fprintf (stderr, "\tmoh_flags            = 0x%.8lx", (long)hdr_ptr->moh_flags);

  if (hdr_ptr->moh_flags & MOH_RELOCATABLE_F)
    fprintf (stderr, ", relocatable");

  if (hdr_ptr->moh_flags & MOH_LINKABLE_F)
    fprintf (stderr, ", linkable");

  if (hdr_ptr->moh_flags & MOH_EXECABLE_F)
    fprintf (stderr, ", execable");

  if (hdr_ptr->moh_flags & MOH_EXECUTABLE_F)
    fprintf (stderr, ", executable");

  if (hdr_ptr->moh_flags & MOH_UNRESOLVED_F)
    fprintf (stderr, ", unresolved");

  fprintf (stderr, "\n\n");
  return;
}


/* Print a short summary of a load command.  */

static void
print_load_command (load_hdr, offset, number)
     load_union_t *load_hdr;
     size_t offset;
     int number;
{
  mo_long_t type = load_hdr->hdr.ldci_cmd_type;
  char *type_str = (char *)0;

  switch (type)
    {
    case LDC_UNDEFINED:   type_str = "UNDEFINED";	break;
    case LDC_CMD_MAP:	  type_str = "CMD_MAP";		break;
    case LDC_INTERPRETER: type_str = "INTERPRETER";	break;
    case LDC_STRINGS:	  type_str = "STRINGS";		break;
    case LDC_REGION:	  type_str = "REGION";		break;
    case LDC_RELOC:	  type_str = "RELOC";		break;
    case LDC_PACKAGE:	  type_str = "PACKAGE";		break;
    case LDC_SYMBOLS:	  type_str = "SYMBOLS";		break;
    case LDC_ENTRY:	  type_str = "ENTRY";		break;
    case LDC_FUNC_TABLE:  type_str = "FUNC_TABLE";	break;
    case LDC_GEN_INFO:	  type_str = "GEN_INFO";	break;
    }

  fprintf (stderr,
	   "cmd %2d, sz: 0x%.2lx, coff: 0x%.3lx, doff: 0x%.6lx, dlen: 0x%.6lx",
	   number,
	   (long) load_hdr->hdr.ldci_cmd_size,
	   (long) offset,
	   (long) load_hdr->hdr.ldci_section_off,
	   (long) load_hdr->hdr.ldci_section_len);

  if (type_str == (char *)0)
    fprintf (stderr, ", ty: unknown (%ld)\n", (long) type);

  else if (type != LDC_REGION)
    fprintf (stderr, ", ty: %s\n", type_str);

  else
    {
      char *region = "";
      switch (load_hdr->region.regc_usage_type)
	{
	case REG_TEXT_T:	region = ", .text";	break;
	case REG_DATA_T:	region = ", .data";	break;
	case REG_BSS_T:		region = ", .bss";	break;
	case REG_GLUE_T:	region = ", .glue";	break;
#if defined (REG_RDATA_T) && defined (REG_SDATA_T) && defined (REG_SBSS_T) /*mips*/
	case REG_RDATA_T:	region = ", .rdata";	break;
	case REG_SDATA_T:	region = ", .sdata";	break;
	case REG_SBSS_T:	region = ", .sbss";	break;
#endif
	}

      fprintf (stderr, ", ty: %s, vaddr: 0x%.8lx, vlen: 0x%.6lx%s\n",
	       type_str,
	       (long) load_hdr->region.regc_vm_addr,
	       (long) load_hdr->region.regc_vm_size,
	       region);
    }

  return;
}


/* Fatal error when {en,de}code_mach_o_header fails.  */

static void
bad_header (status)
     int status;
{
  char *msg = (char *)0;

  switch (status)
    {
    case MO_ERROR_BAD_MAGIC:		msg = "bad magic number";		break;
    case MO_ERROR_BAD_HDR_VERS:		msg = "bad header version";		break;
    case MO_ERROR_BAD_RAW_HDR_VERS:	msg = "bad raw header version";		break;
    case MO_ERROR_BUF2SML:		msg = "raw header buffer too small";	break;
    case MO_ERROR_OLD_RAW_HDR_FILE:	msg = "old raw header file";		break;
    case MO_ERROR_UNSUPPORTED_VERS:	msg = "unsupported version";		break;
    }

  if (msg == (char *)0)
    fatal ("unknown {de,en}code_mach_o_hdr return value %d", status);
  else
    fatal ("%s", msg);
}


/* Read a file into a memory buffer.  */

static struct file_info *
read_file (name, fd, rw)
     char *name;		/* filename */
     int fd;			/* file descriptor */
     int rw;			/* read/write */
{
  struct stat stat_pkt;
  struct file_info *p = (struct file_info *) xcalloc (sizeof (struct file_info), 1);
#ifdef USE_MMAP
  static int page_size;
#endif

  if (fstat (fd, &stat_pkt) < 0)
    fatal_perror ("fstat %s", name);

  p->name	  = name;
  p->size	  = stat_pkt.st_size;
  p->rounded_size = stat_pkt.st_size;
  p->fd		  = fd;
  p->rw		  = rw;

#ifdef USE_MMAP
  if (debug)
    fprintf (stderr, "mmap %s, %s\n", name, (rw) ? "read/write" : "read-only");

  if (page_size == 0)
    page_size = sysconf (_SC_PAGE_SIZE);

  p->rounded_size = ((p->size + page_size - 1) / page_size) * page_size;
  p->start = mmap ((caddr_t)0,
		   (rw) ? p->rounded_size : p->size,
		   (rw) ? (PROT_READ | PROT_WRITE) : PROT_READ,
		   MAP_FILE | MAP_VARIABLE | MAP_SHARED,
		   fd,
		   0L);

  if (p->start != (char *)0 && p->start != (char *)-1)
    p->use_mmap = 1;

  else
#endif /* USE_MMAP */
    {
      long len;

      if (debug)
	fprintf (stderr, "read %s\n", name);

      p->use_mmap = 0;
      p->start = xmalloc (p->size);
      if (lseek (fd, 0L, SEEK_SET) < 0)
	fatal_perror ("lseek to 0 on %s", name);

      len = read (fd, p->start, p->size);
      if (len < 0)
	fatal_perror ("read %s", name);

      if (len != p->size)
	fatal ("read %ld bytes, expected %ld, from %s", len, p->size, name);
    }

  return p;
}


/* Do anything necessary to write a file back from memory.  */

static void
end_file (ptr)
     struct file_info *ptr;	/* file information block */
{
#ifdef USE_MMAP
  if (ptr->use_mmap)
    {
      if (ptr->rw)
	{
	  if (debug)
	    fprintf (stderr, "msync %s\n", ptr->name);

	  if (msync (ptr->start, ptr->rounded_size, MS_ASYNC))
	    fatal_perror ("msync %s", ptr->name);
	}

      if (debug)
	fprintf (stderr, "munmap %s\n", ptr->name);

      if (munmap (ptr->start, ptr->size))
	fatal_perror ("munmap %s", ptr->name);
    }
  else
#endif /* USE_MMAP */
    {
      if (ptr->rw)
	{
	  long len;

	  if (debug)
	    fprintf (stderr, "write %s\n", ptr->name);

	  if (lseek (ptr->fd, 0L, SEEK_SET) < 0)
	    fatal_perror ("lseek to 0 on %s", ptr->name);

	  len = write (ptr->fd, ptr->start, ptr->size);
	  if (len < 0)
	    fatal_perror ("read %s", ptr->name);

	  if (len != ptr->size)
	    fatal ("wrote %ld bytes, expected %ld, to %s", len, ptr->size, ptr->name);
	}

      free ((generic *)ptr->start);
    }

  free ((generic *)ptr);
}

#endif /* OBJECT_FORMAT_ROSE */
