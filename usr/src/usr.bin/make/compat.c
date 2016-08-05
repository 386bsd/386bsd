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
static char sccsid[] = "@(#)compat.c	5.7 (Berkeley) 3/1/91";
#endif /* not lint */

/*-
 * compat.c --
 *	The routines in this file implement the full-compatibility
 *	mode of PMake. Most of the special functionality of PMake
 *	is available in this mode. Things not supported:
 *	    - different shells.
 *	    - friendly variable substitution.
 *
 * Interface:
 *	Compat_Run	    Initialize things for this module and recreate
 *	    	  	    thems as need creatin'
 */

#include    <stdio.h>
#include    <sys/types.h>
#include    <sys/signal.h>
#include    <sys/wait.h>
#include    <sys/errno.h>
#include    <ctype.h>
#include    "make.h"
extern int errno;

/*
 * The following array is used to make a fast determination of which
 * characters are interpreted specially by the shell.  If a command
 * contains any of these characters, it is executed by the shell, not
 * directly by us.
 */

static char 	    meta[256];

static GNode	    *curTarg = NILGNODE;
static GNode	    *ENDNode;
static int  	    CompatRunCommand();

/*-
 *-----------------------------------------------------------------------
 * CompatInterrupt --
 *	Interrupt the creation of the current target and remove it if
 *	it ain't precious.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The target is removed and the process exits. If .INTERRUPT exists,
 *	its commands are run first WITH INTERRUPTS IGNORED..
 *
 *-----------------------------------------------------------------------
 */
static void
CompatInterrupt (signo)
    int	    signo;
{
    GNode   *gn;
    
    if ((curTarg != NILGNODE) && !Targ_Precious (curTarg)) {
	char 	  *file = Var_Value (TARGET, curTarg);

	if (unlink (file) == SUCCESS) {
	    printf ("*** %s removed\n", file);
	}

	/*
	 * Run .INTERRUPT only if hit with interrupt signal
	 */
	if (signo == SIGINT) {
	    gn = Targ_FindNode(".INTERRUPT", TARG_NOCREATE);
	    if (gn != NILGNODE) {
		Lst_ForEach(gn->commands, CompatRunCommand, (ClientData)gn);
	    }
	}
    }
    exit (0);
}

/*-
 *-----------------------------------------------------------------------
 * CompatRunCommand --
 *	Execute the next command for a target. If the command returns an
 *	error, the node's made field is set to ERROR and creation stops.
 *
 * Results:
 *	0 if the command succeeded, 1 if an error occurred.
 *
 * Side Effects:
 *	The node's 'made' field may be set to ERROR.
 *
 *-----------------------------------------------------------------------
 */
static int
CompatRunCommand (cmd, gn)
    char    	  *cmd;	    	/* Command to execute */
    GNode   	  *gn;    	/* Node from which the command came */
{
    char    	  *cmdStart;	/* Start of expanded command */
    register char *cp;
    Boolean 	  silent,   	/* Don't print command */
		  errCheck; 	/* Check errors */
    union wait 	  reason;   	/* Reason for child's death */
    int	    	  status;   	/* Description of child's death */
    int	    	  cpid;	    	/* Child actually found */
    int	    	  numWritten;	/* Number of bytes written for error message */
    ReturnStatus  stat;	    	/* Status of fork */
    LstNode 	  cmdNode;  	/* Node where current command is located */
    char    	  **av;	    	/* Argument vector for thing to exec */
    int	    	  argc;	    	/* Number of arguments in av or 0 if not
				 * dynamically allocated */
    Boolean 	  local;    	/* TRUE if command should be executed
				 * locally */

    silent = gn->type & OP_SILENT;
    errCheck = !(gn->type & OP_IGNORE);

    cmdNode = Lst_Member (gn->commands, (ClientData)cmd);
    cmdStart = Var_Subst (cmd, gn, FALSE);

    /*
     * brk_string will return an argv with a NULL in av[1], thus causing
     * execvp to choke and die horribly. Besides, how can we execute a null
     * command? In any case, we warn the user that the command expanded to
     * nothing (is this the right thing to do?).
     */
     
    if (*cmdStart == '\0') {
	Error("%s expands to empty string", cmd);
	return(0);
    } else {
	cmd = cmdStart;
    }
    Lst_Replace (cmdNode, (ClientData)cmdStart);

    if ((gn->type & OP_SAVE_CMDS) && (gn != ENDNode)) {
	(void)Lst_AtEnd(ENDNode->commands, (ClientData)cmdStart);
	return(0);
    } else if (strcmp(cmdStart, "...") == 0) {
	gn->type |= OP_SAVE_CMDS;
	return(0);
    }

    while ((*cmd == '@') || (*cmd == '-')) {
	if (*cmd == '@') {
	    silent = TRUE;
	} else {
	    errCheck = FALSE;
	}
	cmd++;
    }

    while (isspace(*cmd)) cmd++;
    
    /*
     * Search for meta characters in the command. If there are no meta
     * characters, there's no need to execute a shell to execute the
     * command.
     */
    for (cp = cmd; !meta[*cp]; cp++) {
	continue;
    }

    /*
     * Print the command before echoing if we're not supposed to be quiet for
     * this one. We also print the command if -n given.
     */
    if (!silent || noExecute) {
	printf ("%s\n", cmd);
	fflush(stdout);
    }

    /*
     * If we're not supposed to execute any commands, this is as far as
     * we go...
     */
    if (noExecute) {
	return (0);
    }
    
    if (*cp != '\0') {
	/*
	 * If *cp isn't the null character, we hit a "meta" character and
	 * need to pass the command off to the shell. We give the shell the
	 * -e flag as well as -c if it's supposed to exit when it hits an
	 * error.
	 */
	static char	*shargv[4] = { "/bin/sh" };

	shargv[1] = (errCheck ? "-ec" : "-c");
	shargv[2] = cmd;
	shargv[3] = (char *)NULL;
	av = shargv;
	argc = 0;
    } else {
	/*
	 * No meta-characters, so no need to exec a shell. Break the command
	 * into words to form an argument vector we can execute.
	 * brk_string sticks our name in av[0], so we have to
	 * skip over it...
	 */
	av = brk_string(cmd, &argc);
	av += 1;
    }
    
    local = TRUE;

    /*
     * Fork and execute the single command. If the fork fails, we abort.
     */
    cpid = vfork();
    if (cpid < 0) {
	Fatal("Could not fork");
    }
    if (cpid == 0) {
	if (local) {
	    execvp(av[0], av);
	    numWritten = write (2, av[0], strlen (av[0]));
	    numWritten = write (2, ": not found\n", sizeof(": not found"));
	} else {
	    (void)execv(av[0], av);
	}
	exit(1);
    }
    
    /*
     * The child is off and running. Now all we can do is wait...
     */
    while (1) {
	int 	  id;

	if (!local) {
	    id = 0;
	}

	while ((stat = wait((int *)&reason)) != cpid) {
	    if (stat == -1 && errno != EINTR) {
		break;
	    }
	}
	
	if (stat > -1) {
	    if (WIFSTOPPED(reason)) {
		status = reason.w_stopval;		/* stopped */
	    } else if (WIFEXITED(reason)) {
		status = reason.w_retcode;		/* exited */
		if (status != 0) {
		    printf ("*** Error code %d", status);
		}
	    } else {
		status = reason.w_termsig;		/* signaled */
		printf ("*** Signal %d", status);
	    } 

	    
	    if (!WIFEXITED(reason) || (status != 0)) {
		if (errCheck) {
		    gn->made = ERROR;
		    if (keepgoing) {
			/*
			 * Abort the current target, but let others
			 * continue.
			 */
			printf (" (continuing)\n");
		    }
		} else {
		    /*
		     * Continue executing commands for this target.
		     * If we return 0, this will happen...
		     */
		    printf (" (ignored)\n");
		    status = 0;
		}
	    }
	    break;
	} else {
	    Fatal ("error in wait: %d", stat);
	    /*NOTREACHED*/
	}
    }

    return (status);
}

/*-
 *-----------------------------------------------------------------------
 * CompatMake --
 *	Make a target.
 *
 * Results:
 *	0
 *
 * Side Effects:
 *	If an error is detected and not being ignored, the process exits.
 *
 *-----------------------------------------------------------------------
 */
static int
CompatMake (gn, pgn)
    GNode   	  *gn;	    /* The node to make */
    GNode   	  *pgn;	    /* Parent to abort if necessary */
{
    if (gn->type & OP_USE) {
	Make_HandleUse(gn, pgn);
    } else if (gn->made == UNMADE) {
	/*
	 * First mark ourselves to be made, then apply whatever transformations
	 * the suffix module thinks are necessary. Once that's done, we can
	 * descend and make all our children. If any of them has an error
	 * but the -k flag was given, our 'make' field will be set FALSE again.
	 * This is our signal to not attempt to do anything but abort our
	 * parent as well.
	 */
	gn->make = TRUE;
	gn->made = BEINGMADE;
	Suff_FindDeps (gn);
	Lst_ForEach (gn->children, CompatMake, (ClientData)gn);
	if (!gn->make) {
	    gn->made = ABORTED;
	    pgn->make = FALSE;
	    return (0);
	}

	if (Lst_Member (gn->iParents, pgn) != NILLNODE) {
	    Var_Set (IMPSRC, Var_Value(TARGET, gn), pgn);
	}
	
	/*
	 * All the children were made ok. Now cmtime contains the modification
	 * time of the newest child, we need to find out if we exist and when
	 * we were modified last. The criteria for datedness are defined by the
	 * Make_OODate function.
	 */
	if (DEBUG(MAKE)) {
	    printf("Examining %s...", gn->name);
	}
	if (! Make_OODate(gn)) {
	    gn->made = UPTODATE;
	    if (DEBUG(MAKE)) {
		printf("up-to-date.\n");
	    }
	    return (0);
	} else if (DEBUG(MAKE)) {
	    printf("out-of-date.\n");
	}

	/*
	 * If the user is just seeing if something is out-of-date, exit now
	 * to tell him/her "yes".
	 */
	if (queryFlag) {
	    exit (-1);
	}

	/*
	 * We need to be re-made. We also have to make sure we've got a $?
	 * variable. To be nice, we also define the $> variable using
	 * Make_DoAllVar().
	 */
	Make_DoAllVar(gn);
		    
	/*
	 * Alter our type to tell if errors should be ignored or things
	 * should not be printed so CompatRunCommand knows what to do.
	 */
	if (Targ_Ignore (gn)) {
	    gn->type |= OP_IGNORE;
	}
	if (Targ_Silent (gn)) {
	    gn->type |= OP_SILENT;
	}

	if (Job_CheckCommands (gn, Fatal)) {
	    /*
	     * Our commands are ok, but we still have to worry about the -t
	     * flag...
	     */
	    if (!touchFlag) {
		curTarg = gn;
		Lst_ForEach (gn->commands, CompatRunCommand, (ClientData)gn);
		curTarg = NILGNODE;
	    } else {
		Job_Touch (gn, gn->type & OP_SILENT);
	    }
	} else {
	    gn->made = ERROR;
	}

	if (gn->made != ERROR) {
	    /*
	     * If the node was made successfully, mark it so, update
	     * its modification time and timestamp all its parents. Note
	     * that for .ZEROTIME targets, the timestamping isn't done.
	     * This is to keep its state from affecting that of its parent.
	     */
	    gn->made = MADE;
#ifndef RECHECK
	    /*
	     * We can't re-stat the thing, but we can at least take care of
	     * rules where a target depends on a source that actually creates
	     * the target, but only if it has changed, e.g.
	     *
	     * parse.h : parse.o
	     *
	     * parse.o : parse.y
	     *  	yacc -d parse.y
	     *  	cc -c y.tab.c
	     *  	mv y.tab.o parse.o
	     *  	cmp -s y.tab.h parse.h || mv y.tab.h parse.h
	     *
	     * In this case, if the definitions produced by yacc haven't
	     * changed from before, parse.h won't have been updated and
	     * gn->mtime will reflect the current modification time for
	     * parse.h. This is something of a kludge, I admit, but it's a
	     * useful one..
	     *
	     * XXX: People like to use a rule like
	     *
	     * FRC:
	     *
	     * To force things that depend on FRC to be made, so we have to
	     * check for gn->children being empty as well...
	     */
	    if (!Lst_IsEmpty(gn->commands) || Lst_IsEmpty(gn->children)) {
		gn->mtime = now;
	    }
#else
	    /*
	     * This is what Make does and it's actually a good thing, as it
	     * allows rules like
	     *
	     *	cmp -s y.tab.h parse.h || cp y.tab.h parse.h
	     *
	     * to function as intended. Unfortunately, thanks to the stateless
	     * nature of NFS (and the speed of this program), there are times
	     * when the modification time of a file created on a remote
	     * machine will not be modified before the stat() implied by
	     * the Dir_MTime occurs, thus leading us to believe that the file
	     * is unchanged, wreaking havoc with files that depend on this one.
	     *
	     * I have decided it is better to make too much than to make too
	     * little, so this stuff is commented out unless you're sure it's
	     * ok.
	     * -- ardeb 1/12/88
	     */
	    if (noExecute || Dir_MTime(gn) == 0) {
		gn->mtime = now;
	    }
	    if (DEBUG(MAKE)) {
		printf("update time: %s\n", Targ_FmtTime(gn->mtime));
	    }
#endif
	    if (!(gn->type & OP_EXEC)) {
		pgn->childMade = TRUE;
		Make_TimeStamp(pgn, gn);
	    }
	} else if (keepgoing) {
	    pgn->make = FALSE;
	} else {
	    printf ("\n\nStop.\n");
	    exit (1);
	}
    } else if (gn->made == ERROR) {
	/*
	 * Already had an error when making this beastie. Tell the parent
	 * to abort.
	 */
	pgn->make = FALSE;
    } else {
	if (Lst_Member (gn->iParents, pgn) != NILLNODE) {
	    Var_Set (IMPSRC, Var_Value(TARGET, gn), pgn);
	}
	switch(gn->made) {
	    case BEINGMADE:
		Error("Graph cycles through %s\n", gn->name);
		gn->made = ERROR;
		pgn->make = FALSE;
		break;
	    case MADE:
		if ((gn->type & OP_EXEC) == 0) {
		    pgn->childMade = TRUE;
		    Make_TimeStamp(pgn, gn);
		}
		break;
	    case UPTODATE:
		if ((gn->type & OP_EXEC) == 0) {
		    Make_TimeStamp(pgn, gn);
		}
		break;
	}
    }

    return (0);
}
	
/*-
 *-----------------------------------------------------------------------
 * Compat_Run --
 *	Initialize this mode and start making.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Guess what?
 *
 *-----------------------------------------------------------------------
 */
void
Compat_Run(targs)
    Lst	    	  targs;    /* List of target nodes to re-create */
{
    char    	  *cp;	    /* Pointer to string of shell meta-characters */
    GNode   	  *gn;	    /* Current root target */
    int	    	  errors;   /* Number of targets not remade due to errors */

    if (signal(SIGINT, SIG_IGN) != SIG_IGN) {
	signal(SIGINT, CompatInterrupt);
    }
    if (signal(SIGTERM, SIG_IGN) != SIG_IGN) {
	signal(SIGTERM, CompatInterrupt);
    }
    if (signal(SIGHUP, SIG_IGN) != SIG_IGN) {
	signal(SIGHUP, CompatInterrupt);
    }
    if (signal(SIGQUIT, SIG_IGN) != SIG_IGN) {
	signal(SIGQUIT, CompatInterrupt);
    }

    for (cp = "#=|^(){};&<>*?[]:$`\\\n"; *cp != '\0'; cp++) {
	meta[*cp] = 1;
    }
    /*
     * The null character serves as a sentinel in the string.
     */
    meta[0] = 1;

    ENDNode = Targ_FindNode(".END", TARG_CREATE);
    /*
     * If the user has defined a .BEGIN target, execute the commands attached
     * to it.
     */
    if (!queryFlag) {
	gn = Targ_FindNode(".BEGIN", TARG_NOCREATE);
	if (gn != NILGNODE) {
	    Lst_ForEach(gn->commands, CompatRunCommand, (ClientData)gn);
	}
    }

    /*
     * For each entry in the list of targets to create, call CompatMake on
     * it to create the thing. CompatMake will leave the 'made' field of gn
     * in one of several states:
     *	    UPTODATE	    gn was already up-to-date
     *	    MADE  	    gn was recreated successfully
     *	    ERROR 	    An error occurred while gn was being created
     *	    ABORTED	    gn was not remade because one of its inferiors
     *	    	  	    could not be made due to errors.
     */
    errors = 0;
    while (!Lst_IsEmpty (targs)) {
	gn = (GNode *) Lst_DeQueue (targs);
	CompatMake (gn, gn);

	if (gn->made == UPTODATE) {
	    printf ("`%s' is up to date.\n", gn->name);
	} else if (gn->made == ABORTED) {
	    printf ("`%s' not remade because of errors.\n", gn->name);
	    errors += 1;
	}
    }

    /*
     * If the user has defined a .END target, run its commands.
     */
    if (errors == 0) {
	Lst_ForEach(ENDNode->commands, CompatRunCommand, (ClientData)gn);
    }
}
