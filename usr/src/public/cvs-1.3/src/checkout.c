/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.3 kit.
 * 
 * Create Version
 * 
 * "checkout" creates a "version" of an RCS repository.  This version is owned
 * totally by the user and is actually an independent copy, to be dealt with
 * as seen fit.  Once "checkout" has been called in a given directory, it
 * never needs to be called again.  The user can keep up-to-date by calling
 * "update" when he feels like it; this will supply him with a merge of his
 * own modifications and the changes made in the RCS original.  See "update"
 * for details.
 * 
 * "checkout" can be given a list of directories or files to be updated and in
 * the case of a directory, will recursivley create any sub-directories that
 * exist in the repository.
 * 
 * When the user is satisfied with his own modifications, the present version
 * can be committed by "commit"; this keeps the present version in tact,
 * usually.
 * 
 * The call is cvs checkout [options] <module-name>...
 * 
 * "checkout" creates a directory ./CVS, in which it keeps its administration,
 * in two files, Repository and Entries. The first contains the name of the
 * repository.  The second contains one line for each registered file,
 * consisting of the version number it derives from, its time stamp at
 * derivation time and its name.  Both files are normal files and can be
 * edited by the user, if necessary (when the repository is moved, e.g.)
 */

#include "cvs.h"

#ifndef lint
static char rcsid[] = "@(#)checkout.c 1.67 92/04/10";
#endif

#if __STDC__
static char *findslash (char *start, char *p);
static int build_dirs_and_chdir (char *dir, char *prepath, char *realdir,
				 int sticky);
static int checkout_proc (int *pargc, char *argv[], char *where,
		          char *mwhere, char *mfile, int shorten,
		          int local_specified, char *omodule,
		          char *msg);
#else
static int checkout_proc ();
static char *findslash ();
static int build_dirs_and_chdir ();
#endif				/* __STDC__ */

static char *checkout_usage[] =
{
    "Usage:\n  %s %s [-ANPQcflnpqs] [-r rev | -D date] [-d dir] [-k kopt] modules...\n",
    "\t-A\tReset any sticky tags/date/kopts.\n",
    "\t-N\tDon't shorten module paths if -d specified.\n",
    "\t-P\tPrune empty directories.\n",
    "\t-Q\tReally quiet.\n",
    "\t-c\t\"cat\" the module database.\n",
    "\t-f\tForce a head revision match if tag/date not found.\n",
    "\t-l\tLocal directory only, not recursive\n",
    "\t-n\tDo not run module program (if any).\n",
    "\t-p\tCheck out files to standard output.\n",
    "\t-q\tSomewhat quiet.\n",
    "\t-s\tLike -c, but include module status.\n",
    "\t-r rev\tCheck out revision or tag. (implies -P)\n",
    "\t-D date\tCheck out revisions as of date. (implies -P)\n",
    "\t-d dir\tCheck out into dir instead of module name.\n",
    "\t-k kopt\tUse RCS kopt -k option on checkout.\n",
    "\t-j rev\tMerge in changes made between current revision and rev.\n",
    NULL
};

static char *export_usage[] =
{
    "Usage: %s %s [-NPQflnq] [-r rev | -D date] [-d dir] module...\n",
    "\t-N\tDon't shorten module paths if -d specified.\n",
    "\t-Q\tReally quiet.\n",
    "\t-f\tForce a head revision match if tag/date not found.\n",
    "\t-l\tLocal directory only, not recursive\n",
    "\t-n\tDo not run module program (if any).\n",
    "\t-q\tSomewhat quiet.\n",
    "\t-r rev\tCheck out revision or tag. (implies -P)\n",
    "\t-D date\tCheck out revisions as of date. (implies -P)\n",
    "\t-d dir\tCheck out into dir instead of module name.\n",
    NULL
};

static int checkout_prune_dirs;
static int force_tag_match = 1;
static int pipeout;
static int aflag;
static char *options = NULL;
static char *tag = NULL;
static char *date = NULL;
static char *join_rev1 = NULL;
static char *join_rev2 = NULL;
static char *preload_update_dir = NULL;

int
checkout (argc, argv)
    int argc;
    char *argv[];
{
    register int i;
    int c;
    DBM *db;
    int cat = 0, err = 0, status = 0;
    int run_module_prog = 1;
    int local = 0;
    int shorten = -1;
    char *where = NULL;
    char *valid_options, **valid_usage;

    /*
     * A smaller subset of options are allowed for the export command, which
     * is essentially like checkout, except that it hard-codes certain
     * options to be on (like -kv) and takes care to remove the CVS directory
     * when it has done its duty
     */
    if (strcmp (command_name, "export") == 0)
    {
	valid_options = "Nnd:flRQqr:D:";
	valid_usage = export_usage;
    }
    else
    {
	valid_options = "ANnk:d:flRpQqcsr:D:j:P";
	valid_usage = checkout_usage;
    }

    if (argc == -1)
	usage (valid_usage);

    ign_setup ();

    optind = 1;
    while ((c = gnu_getopt (argc, argv, valid_options)) != -1)
    {
	switch (c)
	{
	    case 'A':
		aflag = 1;
		break;
	    case 'N':
		shorten = 0;
		break;
	    case 'k':
		if (options)
		    free (options);
		options = RCS_check_kflag (optarg);
		break;
	    case 'n':
		run_module_prog = 0;
		break;
	    case 'Q':
		really_quiet = 1;
		/* FALL THROUGH */
	    case 'q':
		quiet = 1;
		break;
	    case 'l':
		local = 1;
		break;
	    case 'R':
		local = 0;
		break;
	    case 'P':
		checkout_prune_dirs = 1;
		break;
	    case 'p':
		pipeout = 1;
		run_module_prog = 0;	/* don't run module prog when piping */
		noexec = 1;		/* so no locks will be created */
		break;
	    case 'c':
		cat = 1;
		break;
	    case 'd':
		where = optarg;
		if (shorten == -1)
		    shorten = 1;
		break;
	    case 's':
		status = 1;
		break;
	    case 'f':
		force_tag_match = 0;
		break;
	    case 'r':
		tag = optarg;
		checkout_prune_dirs = 1;
		break;
	    case 'D':
		date = Make_Date (optarg);
		checkout_prune_dirs = 1;
		break;
	    case 'j':
		if (join_rev2)
		    error (1, 0, "only two -j options can be specified");
		if (join_rev1)
		    join_rev2 = optarg;
		else
		    join_rev1 = optarg;
		break;
	    case '?':
	    default:
		usage (valid_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

    if (shorten == -1)
	shorten = 0;

    if ((!(cat + status) && argc == 0) || ((cat + status) && argc != 0)
	|| (tag && date))
	usage (valid_usage);

    if (where && pipeout)
	error (1, 0, "-d and -p are mutually exclusive");

    if (strcmp (command_name, "export") == 0)
    {
	if (!tag && !date)
	{
	    error (0, 0, "must specify a tag or date");
	    usage (valid_usage);
	}
	if (tag && isdigit (tag[0]))
	    error (1, 0, "tag `%s' must be a symbolic tag", tag);
	options = RCS_check_kflag ("v");/* -kv must be on */
    }

    if (cat || status)
    {
	cat_module (status);
	return (0);
    }
    db = open_module ();

    /*
     * if we have more than one argument and where was specified, we make the
     * where, cd into it, and try to shorten names as much as possible.
     * Otherwise, we pass the where as a single argument to do_module.
     */
    if (argc > 1 && where != NULL)
    {
	char repository[PATH_MAX];

	(void) mkdir (where, 0777);
	if (chdir (where) < 0)
	    error (1, errno, "cannot chdir to %s", where);
	preload_update_dir = xstrdup (where);
	where = (char *) NULL;
	if (!isfile (CVSADM) && !isfile (OCVSADM))
	{
	    (void) sprintf (repository, "%s/%s", CVSroot, CVSNULLREPOS);
	    if (!isfile (repository))
		(void) mkdir (repository, 0777);
	    Create_Admin (".", repository, (char *) NULL, (char *) NULL);
	    if (!noexec)
	    {
		FILE *fp;

		fp = open_file (CVSADM_ENTSTAT, "w+");
		if (fclose(fp) == EOF)
		    error(1, errno, "cannot close %s", CVSADM_ENTSTAT);
	    }
	}
    }

    /*
     * if where was specified (-d) and we have not taken care of it already
     * with the multiple arg stuff, and it was not a simple directory name
     * but rather a path, we strip off everything but the last component and
     * attempt to cd to the indicated place.  where then becomes simply the
     * last component
     */
    if (where != NULL && index (where, '/') != NULL)
    {
	char *slash;

	slash = rindex (where, '/');
	*slash = '\0';

	if (chdir (where) < 0)
	    error (1, errno, "cannot chdir to %s", where);

	preload_update_dir = xstrdup (where);

	where = slash + 1;
	if (*where == '\0')
	    where = NULL;
    }

    for (i = 0; i < argc; i++)
	err += do_module (db, argv[i], CHECKOUT, "Updating", checkout_proc,
			  where, shorten, local, run_module_prog,
			  (char *) NULL);
    close_module (db);
    return (err);
}

/*
 * process_module calls us back here so we do the actual checkout stuff
 */
/* ARGSUSED */
static int
checkout_proc (pargc, argv, where, mwhere, mfile, shorten,
	       local_specified, omodule, msg)
    int *pargc;
    char *argv[];
    char *where;
    char *mwhere;
    char *mfile;
    int shorten;
    int local_specified;
    char *omodule;
    char *msg;
{
    int err = 0;
    int which;
    char *cp;
    char *cp2;
    char repository[PATH_MAX];
    char xwhere[PATH_MAX];
    char *oldupdate = NULL;
    char *prepath;
    char *realdirs;

    /*
     * OK, so we're doing the checkout! Our args are as follows: 
     *  argc,argv contain either dir or dir followed by a list of files 
     *  where contains where to put it (if supplied by checkout) 
     *  mwhere contains the module name or -d from module file 
     *  mfile says do only that part of the module
     *  shorten = TRUE says shorten as much as possible 
     *  omodule is the original arg to do_module()
     */

    /* set up the repository (maybe) for the bottom directory */
    (void) sprintf (repository, "%s/%s", CVSroot, argv[0]);

    /* save the original value of preload_update_dir */
    if (preload_update_dir != NULL)
	oldupdate = xstrdup (preload_update_dir);

    /* fix up argv[] for the case of partial modules */
    if (mfile != NULL)
    {
	char file[PATH_MAX];

	/* if mfile is really a path, straighten it out first */
	if ((cp = rindex (mfile, '/')) != NULL)
	{
	    *cp = 0;
	    (void) strcat (repository, "/");
	    (void) strcat (repository, mfile);

	    /*
	     * Now we need to fill in the where correctly. if !shorten, tack
	     * the rest of the path onto where if where is filled in
	     * otherwise tack the rest of the path onto mwhere and make that
	     * the where
	     * 
	     * If shorten is enabled, we might use mwhere to set where if 
	     * nobody set it yet, so we'll need to setup mwhere as the last
	     * component of the path we are tacking onto repository
	     */
	    if (!shorten)
	    {
		if (where != NULL)
		    (void) sprintf (xwhere, "%s/%s", where, mfile);
		else
		    (void) sprintf (xwhere, "%s/%s", mwhere, mfile);
		where = xwhere;
	    }
	    else
	    {
		char *slash;

		if ((slash = rindex (mfile, '/')) != NULL)
		    mwhere = slash + 1;
		else
		    mwhere = mfile;
	    }
	    mfile = cp + 1;
	}

	(void) sprintf (file, "%s/%s", repository, mfile);
	if (isdir (file))
	{

	    /*
	     * The portion of a module was a directory, so kludge up where to
	     * be the subdir, and fix up repository
	     */
	    (void) strcpy (repository, file);

	    /*
	     * At this point, if shorten is not enabled, we make where either
	     * where with mfile concatenated, or if where hadn't been set we
	     * set it to mwhere with mfile concatenated.
	     * 
	     * If shorten is enabled and where hasn't been set yet, then where
	     * becomes mfile
	     */
	    if (!shorten)
	    {
		if (where != NULL)
		    (void) sprintf (xwhere, "%s/%s", where, mfile);
		else
		    (void) sprintf (xwhere, "%s/%s", mwhere, mfile);
		where = xwhere;
	    }
	    else if (where == NULL)
		where = mfile;
	}
	else
	{
	    int i;

	    /*
	     * The portion of a module was a file, so kludge up argv to be
	     * correct
	     */
	    for (i = 1; i < *pargc; i++)/* free the old ones */
		free (argv[i]);
	    argv[1] = xstrdup (mfile);	/* set up the new one */
	    *pargc = 2;

	    /* where gets mwhere if where isn't set */
	    if (where == NULL)
		where = mwhere;
	}
    }

    /*
     * if shorten is enabled and where isn't specified yet, we pluck the last
     * directory component of argv[0] and make it the where
     */
    if (shorten && where == NULL)
    {
	if ((cp = rindex (argv[0], '/')) != NULL)
	{
	    (void) strcpy (xwhere, cp + 1);
	    where = xwhere;
	}
    }

    /* if where is still NULL, use mwhere if set or the argv[0] dir */
    if (where == NULL)
    {
	if (mwhere)
	    where = mwhere;
	else
	{
	    (void) strcpy (xwhere, argv[0]);
	    where = xwhere;
	}
    }

    if (preload_update_dir != NULL)
    {
	char tmp[PATH_MAX];

	(void) sprintf (tmp, "%s/%s", preload_update_dir, where);
	free (preload_update_dir);
	preload_update_dir = xstrdup (tmp);
    }
    else
	preload_update_dir = xstrdup (where);

    /*
     * At this point, where is the directory we want to build, repository is
     * the repository for the lowest level of the path.
     */

    /*
     * If we are sending everything to stdout, we can skip a whole bunch of
     * work from here
     */
    if (!pipeout)
    {

	/*
	 * We need to tell build_dirs not only the path we want it to build,
	 * but also the repositories we want it to populate the path with. To
	 * accomplish this, we pass build_dirs a ``real path'' with valid
	 * repositories and a string to pre-pend based on how many path
	 * elements exist in where. Big Black Magic
	 */
	prepath = xstrdup (repository);
	cp = rindex (where, '/');
	cp2 = rindex (prepath, '/');
	while (cp != NULL)
	{
	    cp = findslash (where, cp - 1);
	    cp2 = findslash (prepath, cp2 - 1);
	}
	*cp2 = '\0';
	realdirs = cp2 + 1;

	/*
	 * build dirs on the path if necessary and leave us in the bottom
	 * directory (where if where was specified) doesn't contain a CVS
	 * subdir yet, but all the others contain CVS and Entries.Static
	 * files
	 */
	if (build_dirs_and_chdir (where, prepath, realdirs, *pargc <= 1) != 0)
	{
	    error (0, 0, "ignoring module %s", omodule);
	    free (prepath);
	    free (preload_update_dir);
	    preload_update_dir = oldupdate;
	    return (1);
	}

	/* clean up */
	free (prepath);

	/* set up the repository (or make sure the old one matches) */
	if (!isfile (CVSADM) && !isfile (OCVSADM))
	{
	    FILE *fp;

	    if (!noexec && *pargc > 1)
	    {
		Create_Admin (".", repository, (char *) NULL, (char *) NULL);
		fp = open_file (CVSADM_ENTSTAT, "w+");
		if (fclose(fp) == EOF)
		    error(1, errno, "cannot close %s", CVSADM_ENTSTAT);
	    }
	    else
		Create_Admin (".", repository, tag, date);
	}
	else
	{
	    char *repos;

	    /* get the contents of the previously existing repository */
	    repos = Name_Repository ((char *) NULL, preload_update_dir);
	    if (strcmp (repository, repos) != 0)
	    {
		error (0, 0, "existing repository %s does not match %s",
		       repos, repository);
		error (0, 0, "ignoring module %s", omodule);
		free (repos);
		free (preload_update_dir);
		preload_update_dir = oldupdate;
		return (1);
	    }
	    free (repos);
	}
    }

    /*
     * If we are going to be updating to stdout, we need to cd to the
     * repository directory so the recursion processor can use the current
     * directory as the place to find repository information
     */
    if (pipeout)
    {
	if (chdir (repository) < 0)
	{
	    error (0, errno, "cannot chdir to %s", repository);
	    free (preload_update_dir);
	    preload_update_dir = oldupdate;
	    return (1);
	}
	which = W_REPOS;
    }
    else
	which = W_LOCAL | W_REPOS;

    if (tag != NULL || date != NULL)
	which |= W_ATTIC;

    /*
     * if we are going to be recursive (building dirs), go ahead and call the
     * update recursion processor.  We will be recursive unless either local
     * only was specified, or we were passed arguments
     */
    if (!(local_specified || *pargc > 1))
    {
	if (strcmp (command_name, "export") != 0 && !pipeout)
	    history_write ('O', preload_update_dir, tag ? tag : date, where,
			   repository);
	err += do_update (0, (char **) NULL, options, tag, date,
			  force_tag_match, 0 /* !local */ ,
			  1 /* update -d */ , aflag, checkout_prune_dirs,
			  pipeout, which, join_rev1, join_rev2,
			  preload_update_dir);
	free (preload_update_dir);
	preload_update_dir = oldupdate;
	return (err);
    }

    if (!pipeout)
    {
	int i;
	List *entries;

	/* we are only doing files, so register them */
	entries = ParseEntries (0);
	for (i = 1; i < *pargc; i++)
	{
	    char line[MAXLINELEN];
	    char *user;
	    Vers_TS *vers;

	    user = argv[i];
	    vers = Version_TS (repository, options, tag, date, user,
			       force_tag_match, 0, entries, (List *) NULL);
	    if (vers->ts_user == NULL)
	    {
		(void) sprintf (line, "Initial %s", user);
		Register (entries, user, vers->vn_rcs, line, vers->options,
			  vers->tag, vers->date);
	    }
	    freevers_ts (&vers);
	}
	dellist (&entries);
    }

    /* Don't log "export", just regular "checkouts" */
    if (strcmp (command_name, "export") != 0 && !pipeout)
	history_write ('O', preload_update_dir, (tag ? tag : date), where,
		       repository);

    /* go ahead and call update now that everything is set */
    err += do_update (*pargc - 1, argv + 1, options, tag, date,
		      force_tag_match, local_specified, 1 /* update -d */,
		      aflag, checkout_prune_dirs, pipeout, which, join_rev1,
		      join_rev2, preload_update_dir);
    free (preload_update_dir);
    preload_update_dir = oldupdate;
    return (err);
}

static char *
findslash (start, p)
    char *start;
    char *p;
{
    while ((int) p >= (int) start && *p != '/')
	p--;
    if ((int) p < (int) start)
	return (NULL);
    else
	return (p);
}

/*
 * build all the dirs along the path to dir with CVS subdirs with appropriate
 * repositories and Entries.Static files
 */
static int
build_dirs_and_chdir (dir, prepath, realdir, sticky)
    char *dir;
    char *prepath;
    char *realdir;
    int sticky;
{
    FILE *fp;
    char repository[PATH_MAX];
    char path[PATH_MAX];
    char path2[PATH_MAX];
    char *slash;
    char *slash2;
    char *cp;
    char *cp2;

    (void) strcpy (path, dir);
    (void) strcpy (path2, realdir);
    for (cp = path, cp2 = path2;
    (slash = index (cp, '/')) != NULL && (slash2 = index (cp2, '/')) != NULL;
	 cp = slash + 1, cp2 = slash2 + 1)
    {
	*slash = '\0';
	*slash2 = '\0';
	(void) mkdir (cp, 0777);
	if (chdir (cp) < 0)
	{
	    error (0, errno, "cannot chdir to %s", cp);
	    return (1);
	}
	if (!isfile (CVSADM) && !isfile (OCVSADM) &&
	    strcmp (command_name, "export") != 0)
	{
	    (void) sprintf (repository, "%s/%s", prepath, path2);
	    Create_Admin (".", repository, sticky ? (char *) NULL : tag,
			  sticky ? (char *) NULL : date);
	    if (!noexec)
	    {
		fp = open_file (CVSADM_ENTSTAT, "w+");
		if (fclose(fp) == EOF)
		    error(1, errno, "cannot close %s", CVSADM_ENTSTAT);
	    }
	}
	*slash = '/';
	*slash2 = '/';
    }
    (void) mkdir (cp, 0777);
    if (chdir (cp) < 0)
    {
	error (0, errno, "cannot chdir to %s", cp);
	return (1);
    }
    return (0);
}
