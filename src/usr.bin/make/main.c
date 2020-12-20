/*
 * Copyright (c) 1988, 1989, 1990 The Regents of the University of California.
 * Copyright (c) 1988, 1989 by Adam de Boor
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1989 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)main.c	5.25 (Berkeley) 4/1/91";
#endif /* not lint */

/*-
 * main.c --
 *	The main file for this entire program. Exit routines etc
 *	reside here.
 *
 * Utility functions defined in this file:
 *	Main_ParseArgLine	Takes a line of arguments, breaks them and
 *				treats them as if they were given when first
 *				invoked. Used by the parse module to implement
 *				the .MFLAGS target.
 *
 *	Error			Print a tagged error message. The global
 *				MAKE variable must have been defined. This
 *				takes a format string and two optional
 *				arguments for it.
 *
 *	Fatal			Print an error message and exit. Also takes
 *				a format string and two arguments.
 *
 *	Punt			Aborts all jobs and exits with a message. Also
 *				takes a format string and two arguments.
 *
 *	Finish			Finish things up by printing the number of
 *				errors which occured, as passed to it, and
 *				exiting.
 */

#include <sys/param.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <varargs.h>
#include "make.h"
#include "pathnames.h"

#ifndef	DEFMAXLOCAL
#define	DEFMAXLOCAL DEFMAXJOBS
#endif	DEFMAXLOCAL

#define	MAKEFLAGS	".MAKEFLAGS"

Lst			create;		/* Targets to be made */
time_t			now;		/* Time at start of make */
GNode			*DEFAULT;	/* .DEFAULT node */
Boolean			allPrecious;	/* .PRECIOUS given on line by itself */

static Boolean		noBuiltins;	/* -r flag */
static Lst		makefiles;	/* ordered list of makefiles to read */
int			maxJobs;	/* -J argument */
static int		maxLocal;	/* -L argument */
Boolean			debug;		/* -d flag */
Boolean			noExecute;	/* -n flag */
Boolean			keepgoing;	/* -k flag */
Boolean			queryFlag;	/* -q flag */
Boolean			touchFlag;	/* -t flag */
Boolean			usePipes;	/* !-P flag */
Boolean			ignoreErrors;	/* -i flag */
Boolean			beSilent;	/* -s flag */
Boolean			oldVars;	/* variable substitution style */
Boolean			checkEnvFirst;	/* -e flag */
static Boolean		jobsRunning;	/* TRUE if the jobs might be running */

static Boolean		ReadMakefile();

static char *curdir;			/* if chdir'd for an architecture */

/*-
 * MainParseArgs --
 *	Parse a given argument vector. Called from main() and from
 *	Main_ParseArgLine() when the .MAKEFLAGS target is used.
 *
 *	XXX: Deal with command line overriding .MAKEFLAGS in makefile
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Various global and local flags will be set depending on the flags
 *	given
 */
static void
MainParseArgs(argc, argv)
	int argc;
	char **argv;
{
	extern int optind;
	extern char *optarg;
	register int i;
	register char *cp;
	char c;

	optind = 1;	/* since we're called more than once */
rearg:	while((c = getopt(argc, argv, "D:I:d:ef:ij:knqrst-")) != EOF) {
		switch(c) {
		case 'D':
			Var_Set(optarg, "1", VAR_GLOBAL);
			Var_Append(MAKEFLAGS, "-D", VAR_GLOBAL);
			Var_Append(MAKEFLAGS, optarg, VAR_GLOBAL);
			break;
		case 'I':
			Parse_AddIncludeDir(optarg);
			Var_Append(MAKEFLAGS, "-I", VAR_GLOBAL);
			Var_Append(MAKEFLAGS, optarg, VAR_GLOBAL);
			break;
#ifdef notdef
		case 'L':
			maxLocal = atoi(optarg);
			Var_Append(MAKEFLAGS, "-L", VAR_GLOBAL);
			Var_Append(MAKEFLAGS, optarg, VAR_GLOBAL);
			break;
		case 'P':
			usePipes = FALSE;
			Var_Append(MAKEFLAGS, "-P", VAR_GLOBAL);
			break;
		case 'S':
			keepgoing = FALSE;
			Var_Append(MAKEFLAGS, "-S", VAR_GLOBAL);
			break;
#endif
		case 'd': {
			char *modules = optarg;

			for (; *modules; ++modules)
				switch (*modules) {
				case 'A':
					debug = ~0;
					break;
				case 'a':
					debug |= DEBUG_ARCH;
					break;
				case 'c':
					debug |= DEBUG_COND;
					break;
				case 'd':
					debug |= DEBUG_DIR;
					break;
				case 'g':
					if (modules[1] == '1') {
						debug |= DEBUG_GRAPH1;
						++modules;
					}
					else if (modules[1] == '2') {
						debug |= DEBUG_GRAPH2;
						++modules;
					}
					break;
				case 'j':
					debug |= DEBUG_JOB;
					break;
				case 'm':
					debug |= DEBUG_MAKE;
					break;
				case 's':
					debug |= DEBUG_SUFF;
					break;
				case 't':
					debug |= DEBUG_TARG;
					break;
				case 'v':
					debug |= DEBUG_VAR;
					break;
				default:
					(void)fprintf(stderr,
				"make: illegal argument to d option -- %c\n",
					    *modules);
					usage();
				}
			Var_Append(MAKEFLAGS, "-d", VAR_GLOBAL);
			Var_Append(MAKEFLAGS, optarg, VAR_GLOBAL);
			break;
		}
		case 'e':
			checkEnvFirst = TRUE;
			Var_Append(MAKEFLAGS, "-e", VAR_GLOBAL);
			break;
		case 'f':
			(void)Lst_AtEnd(makefiles, (ClientData)optarg);
			break;
		case 'i':
			ignoreErrors = TRUE;
			Var_Append(MAKEFLAGS, "-i", VAR_GLOBAL);
			break;
		case 'j':
			maxJobs = atoi(optarg);
			Var_Append(MAKEFLAGS, "-J", VAR_GLOBAL);
			Var_Append(MAKEFLAGS, optarg, VAR_GLOBAL);
			break;
		case 'k':
			keepgoing = TRUE;
			Var_Append(MAKEFLAGS, "-k", VAR_GLOBAL);
			break;
		case 'n':
			noExecute = TRUE;
			Var_Append(MAKEFLAGS, "-n", VAR_GLOBAL);
			break;
		case 'q':
			queryFlag = TRUE;
			/* Kind of nonsensical, wot? */
			Var_Append(MAKEFLAGS, "-q", VAR_GLOBAL);
			break;
		case 'r':
			noBuiltins = TRUE;
			Var_Append(MAKEFLAGS, "-r", VAR_GLOBAL);
			break;
		case 's':
			beSilent = TRUE;
			Var_Append(MAKEFLAGS, "-s", VAR_GLOBAL);
			break;
		case 't':
			touchFlag = TRUE;
			Var_Append(MAKEFLAGS, "-t", VAR_GLOBAL);
			break;
		case '-':
			break;
		default:
		case '?':
			usage();
		}
	}

	oldVars = TRUE;

	/*
	 * See if the rest of the arguments are variable assignments and
	 * perform them if so. Else take them to be targets and stuff them
	 * on the end of the "create" list.
	 */
	for (argv += optind, argc -= optind; *argv; ++argv, --argc)
		if (Parse_IsVar(*argv))
			Parse_DoVar(*argv, VAR_CMD);
		else {
			if (!**argv)
				Punt("illegal (null) argument.");
			if (**argv == '-') {
				optind = 0;
				goto rearg;
			}
			(void)Lst_AtEnd(create, (ClientData)*argv);
		}
}

/*-
 * Main_ParseArgLine --
 *  	Used by the parse module when a .MFLAGS or .MAKEFLAGS target
 *	is encountered and by main() when reading the .MAKEFLAGS envariable.
 *	Takes a line of arguments and breaks it into its
 * 	component words and passes those words and the number of them to the
 *	MainParseArgs function.
 *	The line should have all its leading whitespace removed.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Only those that come from the various arguments.
 */
void
Main_ParseArgLine(line)
	char *line;			/* Line to fracture */
{
	char **argv;			/* Manufactured argument vector */
	int argc;			/* Number of arguments in argv */

	if (line == NULL)
		return;
	for (; *line == ' '; ++line);
	if (!*line)
		return;

	argv = brk_string(line, &argc);
	MainParseArgs(argc, argv);
}

/*-
 * main --
 *	The main function, for obvious reasons. Initializes variables
 *	and a few modules, then parses the arguments give it in the
 *	environment and on the command line. Reads the system makefile
 *	followed by either Makefile, makefile or the file given by the
 *	-f argument. Sets the .MAKEFLAGS PMake variable based on all the
 *	flags it has received by then uses either the Make or the Compat
 *	module to create the initial list of targets.
 *
 * Results:
 *	If -q was given, exits -1 if anything was out-of-date. Else it exits
 *	0.
 *
 * Side Effects:
 *	The program exits when done. Targets are created. etc. etc. etc.
 */
main(argc, argv)
	int argc;
	char **argv;
{
	Lst targs;	/* target nodes to create -- passed to Make_Init */
	Boolean outOfDate; 	/* FALSE if all targets up to date */
	struct stat sb;
	char *p, *path, *getenv();

	/*
	 * if the MAKEOBJDIR (or by default, the _PATH_OBJDIR) directory
	 * exists, change into it and build there.  Once things are
	 * initted, have to add the original directory to the search path,
	 * and modify the paths for the Makefiles apropriately.  The
	 * current directory is also placed as a variable for make scripts.
	 */
	if (!(path = getenv("MAKEOBJDIR")))
		path = _PATH_OBJDIR;
	if (!lstat(path, &sb)) {
		if (S_ISDIR(sb.st_mode))
			curdir = "..";
		else {
			curdir = emalloc((u_int)PATH_MAX + 1);
			if (!getwd(curdir)) {
				(void)fprintf(stderr, "make: %s.\n", curdir);
				exit(2);
			}
		}
		if (chdir(path)) {
			(void)fprintf(stderr, "make: %s: %s.\n",
			    path, strerror(errno));
			exit(2);
		}
	}

	create = Lst_Init(FALSE);
	makefiles = Lst_Init(FALSE);
	beSilent = FALSE;		/* Print commands as executed */
	ignoreErrors = FALSE;		/* Pay attention to non-zero returns */
	noExecute = FALSE;		/* Execute all commands */
	keepgoing = FALSE;		/* Stop on error */
	allPrecious = FALSE;		/* Remove targets when interrupted */
	queryFlag = FALSE;		/* This is not just a check-run */
	noBuiltins = FALSE;		/* Read the built-in rules */
	touchFlag = FALSE;		/* Actually update targets */
	usePipes = TRUE;		/* Catch child output in pipes */
	debug = 0;			/* No debug verbosity, please. */
	jobsRunning = FALSE;

	maxJobs = DEFMAXJOBS;		/* Set default max concurrency */
	maxLocal = DEFMAXLOCAL;		/* Set default local max concurrency */
    
	/*
	 * Initialize the parsing, directory and variable modules to prepare
	 * for the reading of inclusion paths and variable settings on the
	 * command line
	 */
	Dir_Init();		/* Initialize directory structures so -I flags
				 * can be processed correctly */
	Parse_Init();		/* Need to initialize the paths of #include
				 * directories */
	Var_Init();		/* As well as the lists of variables for
				 * parsing arguments */

	if (curdir) {
		Dir_AddDir(dirSearchPath, curdir);
		Var_Set(".CURDIR", curdir, VAR_GLOBAL);
	} else
		Var_Set(".CURDIR", ".", VAR_GLOBAL);

	/*
	 * Initialize various variables.
	 *	MAKE also gets this name, for compatibility
	 *	.MAKEFLAGS gets set to the empty string just in case.
	 *	MFLAGS also gets initialized empty, for compatibility.
	 */
	Var_Set("MAKE", argv[0], VAR_GLOBAL);
	Var_Set(MAKEFLAGS, "", VAR_GLOBAL);
	Var_Set("MFLAGS", "", VAR_GLOBAL);
	Var_Set("MACHINE", MACHINE, VAR_GLOBAL);

	/*
	 * First snag any flags out of the MAKE environment variable.
	 * (Note this is *not* MAKEFLAGS since /bin/make uses that and it's
	 * in a different format).
	 */
#ifdef POSIX
	Main_ParseArgLine(getenv("MAKEFLAGS"));
#else
	Main_ParseArgLine(getenv("MAKE"));
#endif
    
	MainParseArgs(argc, argv);

	/*
	 * Initialize archive, target and suffix modules in preparation for
	 * parsing the makefile(s)
	 */
	Arch_Init();
	Targ_Init();
	Suff_Init();

	DEFAULT = NILGNODE;
	(void)time(&now);

	/*
	 * Set up the .TARGETS variable to contain the list of targets to be
	 * created. If none specified, make the variable empty -- the parser
	 * will fill the thing in with the default or .MAIN target.
	 */
	if (!Lst_IsEmpty(create)) {
		LstNode ln;

		for (ln = Lst_First(create); ln != NILLNODE;
		    ln = Lst_Succ(ln)) {
			char *name = (char *)Lst_Datum(ln);

			Var_Append(".TARGETS", name, VAR_GLOBAL);
		}
	} else
		Var_Set(".TARGETS", "", VAR_GLOBAL);

	/*
	 * Read in the built-in rules first, followed by the specified makefile,
	 * if it was (makefile != (char *) NULL), or the default Makefile and
	 * makefile, in that order, if it wasn't.
	 */
	 if (!noBuiltins && !ReadMakefile(_PATH_DEFSYSMK))
		Fatal("make: no system rules (%s).", _PATH_DEFSYSMK);

	if (!Lst_IsEmpty(makefiles)) {
		LstNode ln;

		ln = Lst_Find(makefiles, (ClientData)NULL, ReadMakefile);
		if (ln != NILLNODE)
			Fatal("make: cannot open %s.", (char *)Lst_Datum(ln));
	} else if (!ReadMakefile("makefile"))
		(void)ReadMakefile("Makefile");

	(void)ReadMakefile(".depend");

	Var_Append("MFLAGS", Var_Value(MAKEFLAGS, VAR_GLOBAL), VAR_GLOBAL);

	/* Install all the flags into the MAKE envariable. */
	if ((p = Var_Value(MAKEFLAGS, VAR_GLOBAL)) && *p)
#ifdef POSIX
		setenv("MAKEFLAGS", p, 1);
#else
		setenv("MAKE", p, 1);
#endif

	/*
	 * For compatibility, look at the directories in the VPATH variable
	 * and add them to the search path, if the variable is defined. The
	 * variable's value is in the same format as the PATH envariable, i.e.
	 * <directory>:<directory>:<directory>...
	 */
	if (Var_Exists("VPATH", VAR_CMD)) {
		char *vpath, *path, *cp, savec;
		/*
		 * GCC stores string constants in read-only memory, but
		 * Var_Subst will want to write this thing, so store it
		 * in an array
		 */
		static char VPATH[] = "${VPATH}";

		vpath = Var_Subst(VPATH, VAR_CMD, FALSE);
		path = vpath;
		do {
			/* skip to end of directory */
			for (cp = path; *cp != ':' && *cp != '\0'; cp++);
			/* Save terminator character so know when to stop */
			savec = *cp;
			*cp = '\0';
			/* Add directory to search path */
			Dir_AddDir(dirSearchPath, path);
			*cp = savec;
			path = cp + 1;
		} while (savec == ':');
		(void)free((Address)vpath);
	}

	/*
	 * Now that all search paths have been read for suffixes et al, it's
	 * time to add the default search path to their lists...
	 */
	Suff_DoPaths();

	/* print the initial graph, if the user requested it */
	if (DEBUG(GRAPH1))
		Targ_PrintGraph(1);

	/*
	 * Have now read the entire graph and need to make a list of targets
	 * to create. If none was given on the command line, we consult the
	 * parsing module to find the main target(s) to create.
	 */
	if (Lst_IsEmpty(create))
		targs = Parse_MainName();
	else
		targs = Targ_FindList(create, TARG_CREATE);

/*
 * this was original amMake -- want to allow parallelism, so put this
 * back in, eventually.
 */
	if (0) {
		/*
		 * Initialize job module before traversing the graph, now that
		 * any .BEGIN and .END targets have been read.  This is done
		 * only if the -q flag wasn't given (to prevent the .BEGIN from
		 * being executed should it exist).
		 */
		if (!queryFlag) {
			if (maxLocal == -1)
				maxLocal = maxJobs;
			Job_Init(maxJobs, maxLocal);
			jobsRunning = TRUE;
		}

		/* Traverse the graph, checking on all the targets */
		outOfDate = Make_Run(targs);
	} else
		/*
		 * Compat_Init will take care of creating all the targets as
		 * well as initializing the module.
		 */
		Compat_Run(targs);
    
	/* print the graph now it's been processed if the user requested it */
	if (DEBUG(GRAPH2))
		Targ_PrintGraph(2);

	if (queryFlag && outOfDate)
		exit(1);
	else
		exit(0);
}

/*-
 * ReadMakefile  --
 *	Open and parse the given makefile.
 *
 * Results:
 *	TRUE if ok. FALSE if couldn't open file.
 *
 * Side Effects:
 *	lots
 */
static Boolean
ReadMakefile(fname)
	char *fname;		/* makefile to read */
{
	extern Lst parseIncPath, sysIncPath;
	FILE *stream;
	char *name, path[PATH_MAX + 1];

	if (!strcmp(fname, "-")) {
		Parse_File("(stdin)", stdin);
		Var_Set("MAKEFILE", "", VAR_GLOBAL);
	} else {
		if (stream = fopen(fname, "r"))
			goto found;
		/* if we've chdir'd, rebuild the path name */
		if (curdir && *fname != '/') {
			(void)sprintf(path, "%s/%s", curdir, fname);
			if (stream = fopen(path, "r")) {
				fname = path;
				goto found;
			}
		}
		/* look in -I and system include directories. */
		name = Dir_FindFile(fname, parseIncPath);
		if (!name)
			name = Dir_FindFile(fname, sysIncPath);
		if (!name || !(stream = fopen(name, "r")))
			return(FALSE);
		fname = name;
		/*
		 * set the MAKEFILE variable desired by System V fans -- the
		 * placement of the setting here means it gets set to the last
		 * makefile specified, as it is set by SysV make.
		 */
found:		Var_Set("MAKEFILE", fname, VAR_GLOBAL);
		Parse_File(fname, stream);
		(void)fclose(stream);
	}
	return(TRUE);
}

/*-
 * Error --
 *	Print an error message given its format.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The message is printed.
 */
/* VARARGS */
void
Error(va_alist)
	va_dcl
{
	va_list ap;
	char *fmt;

	va_start(ap);
	fmt = va_arg(ap, char *);
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, "\n");
	(void)fflush(stderr);
}

/*-
 * Fatal --
 *	Produce a Fatal error message. If jobs are running, waits for them
 *	to finish.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	The program exits
 */
/* VARARGS */
void
Fatal(va_alist)
	va_dcl
{
	va_list ap;
	char *fmt;

	if (jobsRunning)
		Job_Wait();

	va_start(ap);
	fmt = va_arg(ap, char *);
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, "\n");
	(void)fflush(stderr);

	if (DEBUG(GRAPH2))
		Targ_PrintGraph(2);
	exit(2);		/* Not 1 so -q can distinguish error */
}

/*
 * Punt --
 *	Major exception once jobs are being created. Kills all jobs, prints
 *	a message and exits.
 *
 * Results:
 *	None 
 *
 * Side Effects:
 *	All children are killed indiscriminately and the program Lib_Exits
 */
/* VARARGS */
void
Punt(va_alist)
	va_dcl
{
	va_list ap;
	char *fmt;

	(void)fprintf(stderr, "make: ");
	va_start(ap);
	fmt = va_arg(ap, char *);
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, "\n");
	(void)fflush(stderr);

	DieHorribly();
}

/*-
 * DieHorribly --
 *	Exit without giving a message.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	A big one...
 */
void
DieHorribly()
{
	if (jobsRunning)
		Job_AbortAll();
	if (DEBUG(GRAPH2))
		Targ_PrintGraph(2);
	exit(2);		/* Not 1, so -q can distinguish error */
}

/*
 * Finish --
 *	Called when aborting due to errors in child shell to signal
 *	abnormal exit. 
 *
 * Results:
 *	None 
 *
 * Side Effects:
 *	The program exits
 */
void
Finish(errors)
	int errors;	/* number of errors encountered in Make_Make */
{
	Fatal("%d error%s", errors, errors == 1 ? "" : "s");
}

/*
 * emalloc --
 *	malloc, but die on error.
 */
char *
emalloc(len)
	u_int len;
{
	char *p, *malloc();

	if (!(p = malloc(len)))
		enomem();
	return(p);
}

/*
 * enomem --
 *	die when out of memory.
 */
enomem()
{
	(void)fprintf(stderr, "make: %s.\n", strerror(errno));
	exit(2);
}

/*
 * usage --
 *	exit with usage message
 */
usage()
{
	(void)fprintf(stderr,
"usage: make [-eiknqrst] [-D variable] [-d flags] [-f makefile ]\n\
            [-I directory] [-j max_jobs] [variable=value]\n");
	exit(2);
}
