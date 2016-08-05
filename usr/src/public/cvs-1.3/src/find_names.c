/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.3 kit.
 * 
 * Find Names
 * 
 * Finds all the pertinent file names, both from the administration and from the
 * repository
 * 
 * Find Dirs
 * 
 * Finds all pertinent sub-directories of the checked out instantiation and the
 * repository (and optionally the attic)
 */

#include "cvs.h"

#ifndef lint
static char rcsid[] = "@(#)find_names.c 1.38 92/04/10";
#endif

#if __STDC__
static int find_dirs (char *dir, List * list, int checkadm);
static int find_rcs (char *dir, List * list);
#else
static int find_rcs ();
static int find_dirs ();
#endif				/* __STDC__ */

static List *filelist;

/*
 * add the key from entry on entries list to the files list
 */
static int
add_entries_proc (node)
    Node *node;
{
    Node *fnode;

    fnode = getnode ();
    fnode->type = FILES;
    fnode->key = xstrdup (node->key);
    if (addnode (filelist, fnode) != 0)
	freenode (fnode);
    return (0);
}

/*
 * compare two files list node (for sort)
 */
static int
fsortcmp (p, q)
    Node *p, *q;
{
    return (strcmp (p->key, q->key));
}

List *
Find_Names (repository, which, aflag, optentries)
    char *repository;
    int which;
    int aflag;
    List **optentries;
{
    List *entries;
    List *files;
    char dir[PATH_MAX];

    /* make a list for the files */
    files = filelist = getlist ();

    /* look at entries (if necessary) */
    if (which & W_LOCAL)
    {
	/* parse the entries file (if it exists) */
	entries = ParseEntries (aflag);

	if (entries != NULL)
	{
	    /* walk the entries file adding elements to the files list */
	    (void) walklist (entries, add_entries_proc);

	    /* if our caller wanted the entries list, return it; else free it */
	    if (optentries != NULL)
		*optentries = entries;
	    else
		dellist (&entries);
	}
    }

    if ((which & W_REPOS) && repository && !isreadable (CVSADM_ENTSTAT))
    {
	/* search the repository */
	if (find_rcs (repository, files) != 0)
	    error (1, errno, "cannot open directory %s", repository);

	/* search the attic too */
	if (which & W_ATTIC)
	{
	    (void) sprintf (dir, "%s/%s", repository, CVSATTIC);
	    (void) find_rcs (dir, files);
	}
    }

    /* sort the list into alphabetical order and return it */
    sortlist (files, fsortcmp);
    return (files);
}

/*
 * create a list of directories to traverse from the current directory
 */
List *
Find_Dirs (repository, which)
    char *repository;
    int which;
{
    List *dirlist;

    /* make a list for the directories */
    dirlist = getlist ();

    /* find the local ones */
    if (which & W_LOCAL)
    {
	/* look only for CVS controlled sub-directories */
	if (find_dirs (".", dirlist, 1) != 0)
	    error (1, errno, "cannot open current directory");
    }

    /* look for sub-dirs in the repository */
    if ((which & W_REPOS) && repository)
    {
	/* search the repository */
	if (find_dirs (repository, dirlist, 0) != 0)
	    error (1, errno, "cannot open directory %s", repository);

#ifdef ATTIC_DIR_SUPPORT		/* XXX - FIXME */
	/* search the attic too */
	if (which & W_ATTIC)
	{
	    char dir[PATH_MAX];

	    (void) sprintf (dir, "%s/%s", repository, CVSATTIC);
	    (void) find_dirs (dir, dirlist, 0);
	}
#endif
    }

    /* sort the list into alphabetical order and return it */
    sortlist (dirlist, fsortcmp);
    return (dirlist);
}

/*
 * Finds all the ,v files in the argument directory, and adds them to the
 * files list.  Returns 0 for success and non-zero if the argument directory
 * cannot be opened.
 */
static int
find_rcs (dir, list)
    char *dir;
    List *list;
{
    Node *p;
    CONST char *regex_err;
    char line[50];
    struct direct *dp;
    DIR *dirp;

    /* set up to read the dir */
    if ((dirp = opendir (dir)) == NULL)
	return (1);

    /* set up a regular expression to find the ,v files */
    (void) sprintf (line, ".*%s$", RCSEXT);
    if ((regex_err = re_comp (line)) != NULL)
	error (1, 0, "%s", regex_err);

    /* read the dir, grabbing the ,v files */
    while ((dp = readdir (dirp)) != NULL)
    {
	if (re_exec (dp->d_name))
	{
	    char *comma;

	    comma = rindex (dp->d_name, ',');	/* strip the ,v */
	    *comma = '\0';
	    p = getnode ();
	    p->type = FILES;
	    p->key = xstrdup (dp->d_name);
	    if (addnode (list, p) != 0)
		freenode (p);
	}
    }
    (void) closedir (dirp);
    return (0);
}

/*
 * Finds all the subdirectories of the argument dir and adds them to the
 * specified list.  Sub-directories without a CVS administration directory
 * are optionally ignored  Returns 0 for success or 1 on error.
 */
static int
find_dirs (dir, list, checkadm)
    char *dir;
    List *list;
    int checkadm;
{
    Node *p;
    CONST char *regex_err;
    char tmp[PATH_MAX];
    char admdir[PATH_MAX];
    struct direct *dp;
    DIR *dirp;

    /* build a regex to blow off ,v files */
    (void) sprintf (tmp, ".*%s$", RCSEXT);
    if ((regex_err = re_comp (tmp)) != NULL)
	error (1, 0, "%s", regex_err);

    /* set up to read the dir */
    if ((dirp = opendir (dir)) == NULL)
	return (1);

    /* read the dir, grabbing sub-dirs */
    while ((dp = readdir (dirp)) != NULL)
    {
	if (strcmp (dp->d_name, ".") == 0 ||
	    strcmp (dp->d_name, "..") == 0 ||
	    strcmp (dp->d_name, CVSATTIC) == 0 ||
	    strcmp (dp->d_name, CVSLCK) == 0 ||
	    re_exec (dp->d_name))	/* don't bother stating ,v files */
	    continue;

	(void) sprintf (tmp, "%s/%s", dir, dp->d_name);
	if (isdir (tmp))
	{
	    /* check for administration directories (if needed) */
	    if (checkadm)
	    {
		/* blow off symbolic links to dirs in local dir */
		if (islink (tmp))
		    continue;

		/* check for new style */
		(void) sprintf (admdir, "%s/%s", tmp, CVSADM);
		if (!isdir (admdir))
		{
		    /* and old style */
		    (void) sprintf (admdir, "%s/%s", tmp, OCVSADM);
		    if (!isdir (admdir))
			continue;
		}
	    }

	    /* put it in the list */
	    p = getnode ();
	    p->type = DIRS;
	    p->key = xstrdup (dp->d_name);
	    if (addnode (list, p) != 0)
		freenode (p);
	}
    }
    (void) closedir (dirp);
    return (0);
}
