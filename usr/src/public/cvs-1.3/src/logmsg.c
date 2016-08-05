/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.3 kit.
 */

#include "cvs.h"

#ifndef lint
static char rcsid[] = "@(#)logmsg.c 1.40 92/04/10";
#endif

#if __STDC__
static int find_type (Node * p);
static int fmt_proc (Node * p);
static int logfile_write (char *repository, char *filter, char *title,
			  char *message, char *revision, FILE * logfp,
			  List * changes);
static int rcsinfo_proc (char *repository, char *template);
static int title_proc (Node * p);
static int update_logfile_proc (char *repository, char *filter);
static void setup_tmpfile (FILE * xfp, char *xprefix, List * changes);
static int editinfo_proc (char *repository, char *template);
#else
static void setup_tmpfile ();
static int find_type ();
static int fmt_proc ();
static int rcsinfo_proc ();
static int update_logfile_proc ();
static int title_proc ();
static int logfile_write ();
static int editinfo_proc ();
#endif				/* __STDC__ */

static FILE *fp;
static char *strlist;
static char *editinfo_editor;
static Ctype type;

/*
 * Puts a standard header on the output which is either being prepared for an
 * editor session, or being sent to a logfile program.  The modified, added,
 * and removed files are included (if any) and formatted to look pretty.
 */
static char *prefix;
static int col;
static void
setup_tmpfile (xfp, xprefix, changes)
    FILE *xfp;
    char *xprefix;
    List *changes;
{
    /* set up statics */
    fp = xfp;
    prefix = xprefix;

    type = T_MODIFIED;
    if (walklist (changes, find_type) != 0)
    {
	(void) fprintf (fp, "%sModified Files:\n", prefix);
	(void) fprintf (fp, "%s\t", prefix);
	col = 8;
	(void) walklist (changes, fmt_proc);
	(void) fprintf (fp, "\n");
    }
    type = T_ADDED;
    if (walklist (changes, find_type) != 0)
    {
	(void) fprintf (fp, "%sAdded Files:\n", prefix);
	(void) fprintf (fp, "%s\t", prefix);
	col = 8;
	(void) walklist (changes, fmt_proc);
	(void) fprintf (fp, "\n");
    }
    type = T_REMOVED;
    if (walklist (changes, find_type) != 0)
    {
	(void) fprintf (fp, "%sRemoved Files:\n", prefix);
	(void) fprintf (fp, "%s\t", prefix);
	col = 8;
	(void) walklist (changes, fmt_proc);
	(void) fprintf (fp, "\n");
    }
}

/*
 * Looks for nodes of a specified type and returns 1 if found
 */
static int
find_type (p)
    Node *p;
{
    if (p->data == (char *) type)
	return (1);
    else
	return (0);
}

/*
 * Breaks the files list into reasonable sized lines to avoid line wrap...
 * all in the name of pretty output.  It only works on nodes whose types
 * match the one we're looking for
 */
static int
fmt_proc (p)
    Node *p;
{
    if (p->data == (char *) type)
    {
	if ((col + (int) strlen (p->key)) > 70)
	{
	    (void) fprintf (fp, "\n%s\t", prefix);
	    col = 8;
	}
	(void) fprintf (fp, "%s ", p->key);
	col += strlen (p->key) + 1;
    }
    return (0);
}

/*
 * Builds a temporary file using setup_tmpfile() and invokes the user's
 * editor on the file.  The header garbage in the resultant file is then
 * stripped and the log message is stored in the "message" argument.
 * 
 * rcsinfo - is the name of a file containing lines tacked onto the end of the
 * RCS info offered to the user for editing. If specified, the '-m' flag to
 * "commit" is disabled -- users are forced to run the editor.
 * 
 */
void
do_editor (dir, message, repository, changes)
    char *dir;
    char *message;
    char *repository;
    List *changes;
{
    static int reuse_log_message = 0;
    char line[MAXLINELEN], fname[L_tmpnam+1];
    char *orig_message;
    struct stat pre_stbuf, post_stbuf;
    int retcode = 0;

    if (noexec || reuse_log_message)
	return;

    orig_message = xstrdup (message);	/* save it for later */

    /* Create a temporary file */
    (void) tmpnam (fname);
  again:
    if ((fp = fopen (fname, "w+")) == NULL)
	error (1, 0, "cannot create temporary file %s", fname);

    /* set up the file so that the first line is blank if no msg specified */
    if (*orig_message)
    {
	(void) fprintf (fp, "%s", orig_message);
	if (orig_message[strlen (orig_message) - 1] != '\n')
	    (void) fprintf (fp, "\n");
    }
    else
	(void) fprintf (fp, "\n");

    /* tack templates on if necessary */
    (void) Parse_Info (CVSROOTADM_RCSINFO, repository, rcsinfo_proc, 1);

    (void) fprintf (fp,
  "%s----------------------------------------------------------------------\n",
		    CVSEDITPREFIX);
    (void) fprintf (fp,
  "%sEnter Log.  Lines beginning with `%s' are removed automatically\n%s\n",
		    CVSEDITPREFIX, CVSEDITPREFIX, CVSEDITPREFIX);
    if (dir != NULL)
	(void) fprintf (fp, "%sCommitting in %s\n%s\n", CVSEDITPREFIX,
			dir, CVSEDITPREFIX);
    setup_tmpfile (fp, CVSEDITPREFIX, changes);
    (void) fprintf (fp,
  "%s----------------------------------------------------------------------\n",
		    CVSEDITPREFIX);

    /* finish off the temp file */
    (void) fclose (fp);
    if (stat (fname, &pre_stbuf) == -1)
	pre_stbuf.st_mtime = 0;

    if (editinfo_editor)
	free (editinfo_editor);
    editinfo_editor = (char *) NULL;
    (void) Parse_Info (CVSROOTADM_EDITINFO, repository, editinfo_proc, 0);

    /* run the editor */
    run_setup ("%s", editinfo_editor ? editinfo_editor : Editor);
    run_arg (fname);
    if ((retcode = run_exec (RUN_TTY, RUN_TTY, RUN_TTY,
			     RUN_NORMAL | RUN_SIGIGNORE)) != 0)
	error (editinfo_editor ? 1 : 0, retcode == -1 ? errno : 0,
	       editinfo_editor ? "Logfile verification failed" :
	       "warning: editor session failed");

    /* put the entire message back into the message variable */
    fp = open_file (fname, "r");
    *message = '\0';
    while (fgets (line, sizeof (line), fp) != NULL)
    {
	if (strncmp (line, CVSEDITPREFIX, sizeof (CVSEDITPREFIX) - 1) == 0)
	    continue;
	if (((int) strlen (message) + (int) strlen (line)) >= MAXMESGLEN)
	{
	    error (0, 0, "warning: log message truncated!");
	    break;
	}
	(void) strcat (message, line);
    }
    (void) fclose (fp);
    if ((stat (fname, &post_stbuf) == 0 &&
	 pre_stbuf.st_mtime == post_stbuf.st_mtime) ||
	(*message == '\0' || strcmp (message, "\n") == 0))
    {
	for (;;)
	{
	    (void) printf ("\nLog message unchanged or not specified\n");
	    (void) printf ("a)bort, c)continue, e)dit, !)reuse this message unchanged for remaining dirs\n");
	    (void) printf ("Action: (continue) ");
	    (void) fflush (stdout);
	    *line = '\0';
	    (void) fgets (line, sizeof (line), stdin);
	    if (*line == '\0' || *line == '\n' || *line == 'c' || *line == 'C')
		break;
	    if (*line == 'a' || *line == 'A')
		error (1, 0, "aborted by user");
	    if (*line == 'e' || *line == 'E')
		goto again;
	    if (*line == '!')
	    {
		reuse_log_message = 1;
		break;
	    }
	    (void) printf ("Unknown input\n");
	}
    }
    free (orig_message);
    (void) unlink_file (fname);
}

/*
 * callback proc for Parse_Info for rcsinfo templates this routine basically
 * copies the matching template onto the end of the tempfile we are setting
 * up
 */
/* ARGSUSED */
static int
rcsinfo_proc (repository, template)
    char *repository;
    char *template;
{
    static char *last_template;
    FILE *tfp;
    char line[MAXLINELEN];

    /* nothing to do if the last one included is the same as this one */
    if (last_template && strcmp (last_template, template) == 0)
	return (0);
    if (last_template)
	free (last_template);
    last_template = xstrdup (template);

    if ((tfp = fopen (template, "r")) != NULL)
    {
	while (fgets (line, sizeof (line), tfp) != NULL)
	    (void) fputs (line, fp);
	(void) fclose (tfp);
	return (0);
    }
    else
    {
	error (0, 0, "Couldn't open rcsinfo template file %s", template);
	return (1);
    }
}

/*
 * Uses setup_tmpfile() to pass the updated message on directly to any
 * logfile programs that have a regular expression match for the checked in
 * directory in the source repository.  The log information is fed into the
 * specified program as standard input.
 */
static char *title;
static FILE *logfp;
static char *message;
static char *revision;
static List *changes;

void
Update_Logfile (repository, xmessage, xrevision, xlogfp, xchanges)
    char *repository;
    char *xmessage;
    char *xrevision;
    FILE *xlogfp;
    List *xchanges;
{
    char *srepos;

    /* set up static vars for update_logfile_proc */
    message = xmessage;
    revision = xrevision;
    logfp = xlogfp;
    changes = xchanges;

    /* figure out a good title string */
    srepos = Short_Repository (repository);

    /* allocate a chunk of memory to hold the title string */
    if (!strlist)
	strlist = xmalloc (MAXLISTLEN);
    strlist[0] = '\0';

    type = T_TITLE;
    (void) walklist (changes, title_proc);
    type = T_ADDED;
    (void) walklist (changes, title_proc);
    type = T_MODIFIED;
    (void) walklist (changes, title_proc);
    type = T_REMOVED;
    (void) walklist (changes, title_proc);
    title = xmalloc (strlen (srepos) + strlen (strlist) + 1 + 2); /* for 's */
    (void) sprintf (title, "'%s%s'", srepos, strlist);

    /* to be nice, free up this chunk of memory */
    free (strlist);
    strlist = (char *) NULL;

    /* call Parse_Info to do the actual logfile updates */
    (void) Parse_Info (CVSROOTADM_LOGINFO, repository, update_logfile_proc, 1);

    /* clean up */
    free (title);
}

/*
 * callback proc to actually do the logfile write from Update_Logfile
 */
static int
update_logfile_proc (repository, filter)
    char *repository;
    char *filter;
{
    return (logfile_write (repository, filter, title, message, revision,
			   logfp, changes));
}

/*
 * concatenate each name onto strlist
 */
static int
title_proc (p)
    Node *p;
{
    if (p->data == (char *) type)
    {
	(void) strcat (strlist, " ");
	(void) strcat (strlist, p->key);
    }
    return (0);
}

/*
 * Since some systems don't define this...
 */
#ifndef MAXHOSTNAMELEN
#define	MAXHOSTNAMELEN	256
#endif

/*
 * Writes some stuff to the logfile "filter" and returns the status of the
 * filter program.
 */
static int
logfile_write (repository, filter, title, message, revision, logfp, changes)
    char *repository;
    char *filter;
    char *title;
    char *message;
    char *revision;
    FILE *logfp;
    List *changes;
{
    char cwd[PATH_MAX], host[MAXHOSTNAMELEN];
    FILE *pipefp, *Popen ();
    char *prog = xmalloc (MAXPROGLEN);
    char *cp;
    int c;

    /*
     * A maximum of 6 %s arguments are supported in the filter
     */
    (void) sprintf (prog, filter, title, title, title, title, title, title);
    if ((pipefp = Popen (prog, "w")) == NULL)
    {
	if (!noexec)
	    error (0, 0, "cannot write entry to log filter: %s", prog);
	free (prog);
	return (1);
    }
    if (gethostname (host, sizeof (host)) < 0)
	(void) strcpy (host, "(unknown)");
    (void) fprintf (pipefp, "Update of %s\n", repository);
    (void) fprintf (pipefp, "In directory %s:%s\n\n", host,
		    ((cp = getwd (cwd)) != NULL) ? cp : cwd);
    if (revision && *revision)
	(void) fprintf (pipefp, "Revision/Branch: %s\n\n", revision);
    setup_tmpfile (pipefp, "", changes);
    (void) fprintf (pipefp, "Log Message:\n%s\n", message);
    if (logfp != (FILE *) 0)
    {
	(void) fprintf (pipefp, "Status:\n");
	(void) rewind (logfp);
	while ((c = getc (logfp)) != EOF)
	    (void) putc ((char) c, pipefp);
    }
    free (prog);
    return (pclose (pipefp));
}

/*
 * We choose to use the *last* match within the editinfo file for this
 * repository.  This allows us to have a global editinfo program for the
 * root of some hierarchy, for example, and different ones within different
 * sub-directories of the root (like a special checker for changes made to
 * the "src" directory versus changes made to the "doc" or "test"
 * directories.
 */
/* ARGSUSED */
static int
editinfo_proc(repository, editor)
    char *repository;
    char *editor;
{
    /* nothing to do if the last match is the same as this one */
    if (editinfo_editor && strcmp (editinfo_editor, editor) == 0)
	return (0);
    if (editinfo_editor)
	free (editinfo_editor);

    editinfo_editor = xstrdup (editor);
    return (0);
}
