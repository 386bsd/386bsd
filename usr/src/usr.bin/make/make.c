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
static char sccsid[] = "@(#)make.c	5.3 (Berkeley) 6/1/90";
#endif /* not lint */

/*-
 * make.c --
 *	The functions which perform the examination of targets and
 *	their suitability for creation
 *
 * Interface:
 *	Make_Run 	    	Initialize things for the module and recreate
 *	    	  	    	whatever needs recreating. Returns TRUE if
 *	    	    	    	work was (or would have been) done and FALSE
 *	    	  	    	otherwise.
 *
 *	Make_Update	    	Update all parents of a given child. Performs
 *	    	  	    	various bookkeeping chores like the updating
 *	    	  	    	of the cmtime field of the parent, filling
 *	    	  	    	of the IMPSRC context variable, etc. It will
 *	    	  	    	place the parent on the toBeMade queue if it
 *	    	  	    	should be.
 *
 *	Make_TimeStamp	    	Function to set the parent's cmtime field
 *	    	  	    	based on a child's modification time.
 *
 *	Make_DoAllVar	    	Set up the various local variables for a
 *	    	  	    	target, including the .ALLSRC variable, making
 *	    	  	    	sure that any variable that needs to exist
 *	    	  	    	at the very least has the empty value.
 *
 *	Make_OODate 	    	Determine if a target is out-of-date.
 *
 *	Make_HandleUse	    	See if a child is a .USE node for a parent
 *				and perform the .USE actions if so.
 */

#include    "make.h"

static Lst     	toBeMade;	/* The current fringe of the graph. These
				 * are nodes which await examination by
				 * MakeOODate. It is added to by
				 * Make_Update and subtracted from by
				 * MakeStartJobs */
static int  	numNodes;   	/* Number of nodes to be processed. If this
				 * is non-zero when Job_Empty() returns
				 * TRUE, there's a cycle in the graph */

/*-
 *-----------------------------------------------------------------------
 * Make_TimeStamp --
 *	Set the cmtime field of a parent node based on the mtime stamp in its
 *	child. Called from MakeOODate via Lst_ForEach. 
 *
 * Results:
 *	Always returns 0. 
 *
 * Side Effects:
 *	The cmtime of the parent node will be changed if the mtime
 *	field of the child is greater than it.
 *-----------------------------------------------------------------------
 */
int
Make_TimeStamp (pgn, cgn)
    register GNode *pgn;	/* the current parent */
    register GNode *cgn;	/* the child we've just examined */
{
    if (cgn->mtime > pgn->cmtime) {
	pgn->cmtime = cgn->mtime;
    }
    return (0);
}

/*-
 *-----------------------------------------------------------------------
 * Make_OODate --
 *	See if a given node is out of date with respect to its sources.
 *	Used by Make_Run when deciding which nodes to place on the
 *	toBeMade queue initially and by Make_Update to screen out USE and
 *	EXEC nodes. In the latter case, however, any other sort of node
 *	must be considered out-of-date since at least one of its children
 *	will have been recreated.
 *
 * Results:
 *	TRUE if the node is out of date. FALSE otherwise. 
 *
 * Side Effects:
 *	The mtime field of the node and the cmtime field of its parents
 *	will/may be changed.
 *-----------------------------------------------------------------------
 */
Boolean
Make_OODate (gn)
    register GNode *gn;	      /* the node to check */
{
    Boolean         oodate;

    /*
     * Certain types of targets needn't even be sought as their datedness
     * doesn't depend on their modification time...
     */
    if ((gn->type & (OP_JOIN|OP_USE|OP_EXEC)) == 0) {
	(void) Dir_MTime (gn);
	if (DEBUG(MAKE)) {
	    if (gn->mtime != 0) {
		printf ("modified %s...", Targ_FmtTime(gn->mtime));
	    } else {
		printf ("non-existent...");
	    }
	}
    }

    /*
     * A target is remade in one of the following circumstances:
     *	its modification time is smaller than that of its youngest child
     *	    and it would actually be run (has commands or type OP_NOP)
     *	it's the object of a force operator
     *	it has no children, was on the lhs of an operator and doesn't exist
     *	    already.
     *
     * Libraries are only considered out-of-date if the archive module says
     * they are.
     *
     * These weird rules are brought to you by Backward-Compatability and
     * the strange people who wrote 'Make'.
     */
    if (gn->type & OP_USE) {
	/*
	 * If the node is a USE node it is *never* out of date
	 * no matter *what*.
	 */
	if (DEBUG(MAKE)) {
	    printf(".USE node...");
	}
	oodate = FALSE;
    } else if (gn->type & OP_LIB) {
	if (DEBUG(MAKE)) {
	    printf("library...");
	}
	oodate = Arch_LibOODate (gn);
    } else if (gn->type & OP_JOIN) {
	/*
	 * A target with the .JOIN attribute is only considered
	 * out-of-date if any of its children was out-of-date.
	 */
	if (DEBUG(MAKE)) {
	    printf(".JOIN node...");
	}
	oodate = gn->childMade;
    } else if (gn->type & (OP_FORCE|OP_EXEC)) {
	/*
	 * A node which is the object of the force (!) operator or which has
	 * the .EXEC attribute is always considered out-of-date.
	 */
	if (DEBUG(MAKE)) {
	    if (gn->type & OP_FORCE) {
		printf("! operator...");
	    } else {
		printf(".EXEC node...");
	    }
	}
	oodate = TRUE;
    } else if ((gn->mtime < gn->cmtime) ||
	       ((gn->cmtime == 0) &&
		((gn->mtime==0) || (gn->type & OP_DOUBLEDEP))))
    {
	/*
	 * A node whose modification time is less than that of its
	 * youngest child or that has no children (cmtime == 0) and
	 * either doesn't exist (mtime == 0) or was the object of a
	 * :: operator is out-of-date. Why? Because that's the way Make does
	 * it.
	 */
	if (DEBUG(MAKE)) {
	    if (gn->mtime < gn->cmtime) {
		printf("modified before source...");
	    } else if (gn->mtime == 0) {
		printf("non-existent and no sources...");
	    } else {
		printf(":: operator and no sources...");
	    }
	}
	oodate = TRUE;
    } else {
#if 0
	/* WHY? */
	if (DEBUG(MAKE)) {
	    printf("source %smade...", gn->childMade ? "" : "not ");
	}
	oodate = gn->childMade;
#else
	oodate = FALSE;
#endif /* 0 */
    }

    /*
     * If the target isn't out-of-date, the parents need to know its
     * modification time. Note that targets that appear to be out-of-date
     * but aren't, because they have no commands and aren't of type OP_NOP,
     * have their mtime stay below their children's mtime to keep parents from
     * thinking they're out-of-date.
     */
    if (!oodate) {
	Lst_ForEach (gn->parents, Make_TimeStamp, (ClientData)gn);
    }

    return (oodate);
}

/*-
 *-----------------------------------------------------------------------
 * MakeAddChild  --
 *	Function used by Make_Run to add a child to the list l.
 *	It will only add the child if its make field is FALSE.
 *
 * Results:
 *	Always returns 0
 *
 * Side Effects:
 *	The given list is extended
 *-----------------------------------------------------------------------
 */
static int
MakeAddChild (gn, l)
    GNode          *gn;		/* the node to add */
    Lst            l;		/* the list to which to add it */
{
    if (!gn->make && !(gn->type & OP_USE)) {
	(void)Lst_EnQueue (l, (ClientData)gn);
    }
    return (0);
}

/*-
 *-----------------------------------------------------------------------
 * Make_HandleUse --
 *	Function called by Make_Run and SuffApplyTransform on the downward
 *	pass to handle .USE and transformation nodes. A callback function
 *	for Lst_ForEach, it implements the .USE and transformation
 *	functionality by copying the node's commands, type flags
 *	and children to the parent node. Should be called before the
 *	children are enqueued to be looked at by MakeAddChild.
 *
 *	A .USE node is much like an explicit transformation rule, except
 *	its commands are always added to the target node, even if the
 *	target already has commands.
 *
 * Results:
 *	returns 0.
 *
 * Side Effects:
 *	Children and commands may be added to the parent and the parent's
 *	type may be changed.
 *
 *-----------------------------------------------------------------------
 */
int
Make_HandleUse (cgn, pgn)
    register GNode	*cgn;	/* The .USE node */
    register GNode   	*pgn;	/* The target of the .USE node */
{
    register GNode	*gn;	/* A child of the .USE node */
    register LstNode	ln; 	/* An element in the children list */

    if (cgn->type & (OP_USE|OP_TRANSFORM)) {
	if ((cgn->type & OP_USE) || Lst_IsEmpty(pgn->commands)) {
	    /*
	     * .USE or transformation and target has no commands -- append
	     * the child's commands to the parent.
	     */
	    (void) Lst_Concat (pgn->commands, cgn->commands, LST_CONCNEW);
	}
	
	if (Lst_Open (cgn->children) == SUCCESS) {
	    while ((ln = Lst_Next (cgn->children)) != NILLNODE) {
		gn = (GNode *)Lst_Datum (ln);

		if (Lst_Member (pgn->children, gn) == NILLNODE) {
		    (void) Lst_AtEnd (pgn->children, gn);
		    (void) Lst_AtEnd (gn->parents, pgn);
		    pgn->unmade += 1;
		}
	    }
	    Lst_Close (cgn->children);
	}
	
	pgn->type |= cgn->type & ~(OP_OPMASK|OP_USE|OP_TRANSFORM);

	/*
	 * This child node is now "made", so we decrement the count of
	 * unmade children in the parent... We also remove the child
	 * from the parent's list to accurately reflect the number of decent
	 * children the parent has. This is used by Make_Run to decide
	 * whether to queue the parent or examine its children...
	 */
	if (cgn->type & OP_USE) {
	    pgn->unmade -= 1;
	}
    }
    return (0);
}

/*-
 *-----------------------------------------------------------------------
 * Make_Update  --
 *	Perform update on the parents of a node. Used by JobFinish once
 *	a node has been dealt with and by MakeStartJobs if it finds an
 *	up-to-date node. 
 *
 * Results:
 *	Always returns 0
 *
 * Side Effects:
 *	The unmade field of pgn is decremented and pgn may be placed on
 *	the toBeMade queue if this field becomes 0.
 *
 * 	If the child was made, the parent's childMade field will be set true
 *	and its cmtime set to now.
 *
 *	If the child wasn't made, the cmtime field of the parent will be
 *	altered if the child's mtime is big enough.
 *
 *	Finally, if the child is the implied source for the parent, the
 *	parent's IMPSRC variable is set appropriately.
 *
 *-----------------------------------------------------------------------
 */
void
Make_Update (cgn)
    register GNode *cgn;	/* the child node */
{
    register GNode 	*pgn;	/* the parent node */
    register char  	*cname;	/* the child's name */
    register LstNode	ln; 	/* Element in parents and iParents lists */

    cname = Var_Value (TARGET, cgn);

    /*
     * If the child was actually made, see what its modification time is
     * now -- some rules won't actually update the file. If the file still
     * doesn't exist, make its mtime now.
     */
    if (cgn->made != UPTODATE) {
#ifndef RECHECK
	/*
	 * We can't re-stat the thing, but we can at least take care of rules
	 * where a target depends on a source that actually creates the
	 * target, but only if it has changed, e.g.
	 *
	 * parse.h : parse.o
	 *
	 * parse.o : parse.y
	 *  	yacc -d parse.y
	 *  	cc -c y.tab.c
	 *  	mv y.tab.o parse.o
	 *  	cmp -s y.tab.h parse.h || mv y.tab.h parse.h
	 *
	 * In this case, if the definitions produced by yacc haven't changed
	 * from before, parse.h won't have been updated and cgn->mtime will
	 * reflect the current modification time for parse.h. This is
	 * something of a kludge, I admit, but it's a useful one..
	 * XXX: People like to use a rule like
	 *
	 * FRC:
	 *
	 * To force things that depend on FRC to be made, so we have to
	 * check for gn->children being empty as well...
	 */
	if (!Lst_IsEmpty(cgn->commands) || Lst_IsEmpty(cgn->children)) {
	    cgn->mtime = now;
	}
#else
	/*
	 * This is what Make does and it's actually a good thing, as it
	 * allows rules like
	 *
	 *	cmp -s y.tab.h parse.h || cp y.tab.h parse.h
	 *
	 * to function as intended. Unfortunately, thanks to the stateless
	 * nature of NFS (by which I mean the loose coupling of two clients
	 * using the same file from a common server), there are times
	 * when the modification time of a file created on a remote
	 * machine will not be modified before the local stat() implied by
	 * the Dir_MTime occurs, thus leading us to believe that the file
	 * is unchanged, wreaking havoc with files that depend on this one.
	 *
	 * I have decided it is better to make too much than to make too
	 * little, so this stuff is commented out unless you're sure it's ok.
	 * -- ardeb 1/12/88
	 */
	if (noExecute || Dir_MTime(cgn) == 0) {
	    cgn->mtime = now;
	}
	if (DEBUG(MAKE)) {
	    printf("update time: %s\n", Targ_FmtTime(cgn->mtime));
	}
#endif
    }
    
    if (Lst_Open (cgn->parents) == SUCCESS) {
	while ((ln = Lst_Next (cgn->parents)) != NILLNODE) {
	    pgn = (GNode *)Lst_Datum (ln);
	    if (pgn->make) {
		pgn->unmade -= 1;

		if ( ! (cgn->type & (OP_EXEC|OP_USE))) {
		    if (cgn->made == MADE) {
			pgn->childMade = TRUE;
			if (pgn->cmtime < cgn->mtime) {
			    pgn->cmtime = cgn->mtime;
			}
		    } else {
			(void)Make_TimeStamp (pgn, cgn);
		    }
		}
		if (pgn->unmade == 0) {
		    /*
		     * Queue the node up -- any unmade predecessors will
		     * be dealt with in MakeStartJobs.
		     */
		    (void)Lst_EnQueue (toBeMade, (ClientData)pgn);
		} else if (pgn->unmade < 0) {
		    Error ("Graph cycles through %s", pgn->name);
		}
	    }
	}
	Lst_Close (cgn->parents);
    }
    /*
     * Deal with successor nodes. If any is marked for making and has an unmade
     * count of 0, has not been made and isn't in the examination queue,
     * it means we need to place it in the queue as it restrained itself
     * before.
     */
    for (ln = Lst_First(cgn->successors); ln != NILLNODE; ln = Lst_Succ(ln)) {
	GNode	*succ = (GNode *)Lst_Datum(ln);

	if (succ->make && succ->unmade == 0 && succ->made == UNMADE &&
	    Lst_Member(toBeMade, (ClientData)succ) == NILLNODE)
	{
	    (void)Lst_EnQueue(toBeMade, (ClientData)succ);
	}
    }
    
    /*
     * Set the .PREFIX and .IMPSRC variables for all the implied parents
     * of this node.
     */
    if (Lst_Open (cgn->iParents) == SUCCESS) {
	char	*cpref = Var_Value(PREFIX, cgn);

	while ((ln = Lst_Next (cgn->iParents)) != NILLNODE) {
	    pgn = (GNode *)Lst_Datum (ln);
	    if (pgn->make) {
		Var_Set (IMPSRC, cname, pgn);
		Var_Set (PREFIX, cpref, pgn);
	    }
	}
	Lst_Close (cgn->iParents);
    }
}

/*-
 *-----------------------------------------------------------------------
 * MakeAddAllSrc --
 *	Add a child's name to the ALLSRC and OODATE variables of the given
 *	node. Called from Make_DoAllVar via Lst_ForEach. A child is added only
 *	if it has not been given the .EXEC, .USE or .INVISIBLE attributes.
 *	.EXEC and .USE children are very rarely going to be files, so...
 *	A child is added to the OODATE variable if its modification time is
 *	later than that of its parent, as defined by Make, except if the
 *	parent is a .JOIN node. In that case, it is only added to the OODATE
 *	variable if it was actually made (since .JOIN nodes don't have
 *	modification times, the comparison is rather unfair...)..
 *
 * Results:
 *	Always returns 0
 *
 * Side Effects:
 *	The ALLSRC variable for the given node is extended.
 *-----------------------------------------------------------------------
 */
static int
MakeAddAllSrc (cgn, pgn)
    GNode	*cgn;	/* The child to add */
    GNode	*pgn;	/* The parent to whose ALLSRC variable it should be */
			/* added */
{
    if ((cgn->type & (OP_EXEC|OP_USE|OP_INVISIBLE)) == 0) {
	register char *child;

	child = Var_Value(TARGET, cgn);
	Var_Append (ALLSRC, child, pgn);
	if (pgn->type & OP_JOIN) {
	    if (cgn->made == MADE) {
		Var_Append(OODATE, child, pgn);
	    }
	} else if ((pgn->mtime < cgn->mtime) ||
		   (cgn->mtime >= now && cgn->made == MADE))
	{
	    /*
	     * It goes in the OODATE variable if the parent is younger than the
	     * child or if the child has been modified more recently than
	     * the start of the make. This is to keep pmake from getting
	     * confused if something else updates the parent after the
	     * make starts (shouldn't happen, I know, but sometimes it
	     * does). In such a case, if we've updated the kid, the parent
	     * is likely to have a modification time later than that of
	     * the kid and anything that relies on the OODATE variable will
	     * be hosed.
	     *
	     * XXX: This will cause all made children to go in the OODATE
	     * variable, even if they're not touched, if RECHECK isn't defined,
	     * since cgn->mtime is set to now in Make_Update. According to
	     * some people, this is good...
	     */
	    Var_Append(OODATE, child, pgn);
	}
    }
    return (0);
}

/*-
 *-----------------------------------------------------------------------
 * Make_DoAllVar --
 *	Set up the ALLSRC and OODATE variables. Sad to say, it must be
 *	done separately, rather than while traversing the graph. This is
 *	because Make defined OODATE to contain all sources whose modification
 *	times were later than that of the target, *not* those sources that
 *	were out-of-date. Since in both compatibility and native modes,
 *	the modification time of the parent isn't found until the child
 *	has been dealt with, we have to wait until now to fill in the
 *	variable. As for ALLSRC, the ordering is important and not
 *	guaranteed when in native mode, so it must be set here, too.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	The ALLSRC and OODATE variables of the given node is filled in.
 *	If the node is a .JOIN node, its TARGET variable will be set to
 * 	match its ALLSRC variable.
 *-----------------------------------------------------------------------
 */
void
Make_DoAllVar (gn)
    GNode	*gn;
{
    Lst_ForEach (gn->children, MakeAddAllSrc, gn);

    if (!Var_Exists (OODATE, gn)) {
	Var_Set (OODATE, "", gn);
    }
    if (!Var_Exists (ALLSRC, gn)) {
	Var_Set (ALLSRC, "", gn);
    }

    if (gn->type & OP_JOIN) {
	Var_Set (TARGET, Var_Value (ALLSRC, gn), gn);
    }
}

/*-
 *-----------------------------------------------------------------------
 * MakeStartJobs --
 *	Start as many jobs as possible.
 *
 * Results:
 *	If the query flag was given to pmake, no job will be started,
 *	but as soon as an out-of-date target is found, this function
 *	returns TRUE. At all other times, this function returns FALSE.
 *
 * Side Effects:
 *	Nodes are removed from the toBeMade queue and job table slots
 *	are filled.
 *
 *-----------------------------------------------------------------------
 */
static Boolean
MakeStartJobs ()
{
    register GNode	*gn;
    
    while (!Job_Full() && !Lst_IsEmpty (toBeMade)) {
	gn = (GNode *) Lst_DeQueue (toBeMade);
	if (DEBUG(MAKE)) {
	    printf ("Examining %s...", gn->name);
	}
	/*
	 * Make sure any and all predecessors that are going to be made,
	 * have been.
	 */
	if (!Lst_IsEmpty(gn->preds)) {
	    LstNode ln;

	    for (ln = Lst_First(gn->preds); ln != NILLNODE; ln = Lst_Succ(ln)){
		GNode	*pgn = (GNode *)Lst_Datum(ln);

		if (pgn->make && pgn->made == UNMADE) {
		    if (DEBUG(MAKE)) {
			printf("predecessor %s not made yet.\n", pgn->name);
		    }
		    break;
		}
	    }
	    /*
	     * If ln isn't nil, there's a predecessor as yet unmade, so we
	     * just drop this node on the floor. When the node in question
	     * has been made, it will notice this node as being ready to
	     * make but as yet unmade and will place the node on the queue.
	     */
	    if (ln != NILLNODE) {
		continue;
	    }
	}
	
	numNodes--;
	if (Make_OODate (gn)) {
	    if (DEBUG(MAKE)) {
		printf ("out-of-date\n");
	    }
	    if (queryFlag) {
		return (TRUE);
	    }
	    Make_DoAllVar (gn);
	    Job_Make (gn);
	} else {
	    if (DEBUG(MAKE)) {
		printf ("up-to-date\n");
	    }
	    gn->made = UPTODATE;
	    if (gn->type & OP_JOIN) {
		/*
		 * Even for an up-to-date .JOIN node, we need it to have its
		 * context variables so references to it get the correct
		 * value for .TARGET when building up the context variables
		 * of its parent(s)...
		 */
		Make_DoAllVar (gn);
	    }
	    
	    Make_Update (gn);
	}
    }
    return (FALSE);
}

/*-
 *-----------------------------------------------------------------------
 * MakePrintStatus --
 *	Print the status of a top-level node, viz. it being up-to-date
 *	already or not created due to an error in a lower level.
 *	Callback function for Make_Run via Lst_ForEach.
 *
 * Results:
 *	Always returns 0.
 *
 * Side Effects:
 *	A message may be printed.
 *
 *-----------------------------------------------------------------------
 */
static int
MakePrintStatus(gn, cycle)
    GNode   	*gn;	    /* Node to examine */
    Boolean 	cycle;	    /* True if gn->unmade being non-zero implies
			     * a cycle in the graph, not an error in an
			     * inferior */
{
    if (gn->made == UPTODATE) {
	printf ("`%s' is up to date.\n", gn->name);
    } else if (gn->unmade != 0) {
	if (cycle) {
	    /*
	     * If printing cycles and came to one that has unmade children,
	     * print out the cycle by recursing on its children. Note a
	     * cycle like:
	     *	a : b
	     *	b : c
	     *	c : b
	     * will cause this to erroneously complain about a being in
	     * the cycle, but this is a good approximation.
	     */
	    if (gn->made == CYCLE) {
		Error("Graph cycles through `%s'", gn->name);
		gn->made = ENDCYCLE;
		Lst_ForEach(gn->children, MakePrintStatus, (ClientData)TRUE);
		gn->made = UNMADE;
	    } else if (gn->made != ENDCYCLE) {
		gn->made = CYCLE;
		Lst_ForEach(gn->children, MakePrintStatus, (ClientData)TRUE);
	    }
	} else {
	    printf ("`%s' not remade because of errors.\n", gn->name);
	}
    }
    return (0);
}

/*-
 *-----------------------------------------------------------------------
 * Make_Run --
 *	Initialize the nodes to remake and the list of nodes which are
 *	ready to be made by doing a breadth-first traversal of the graph
 *	starting from the nodes in the given list. Once this traversal
 *	is finished, all the 'leaves' of the graph are in the toBeMade
 *	queue.
 *	Using this queue and the Job module, work back up the graph,
 *	calling on MakeStartJobs to keep the job table as full as
 *	possible.
 *
 * Results:
 *	TRUE if work was done. FALSE otherwise.
 *
 * Side Effects:
 *	The make field of all nodes involved in the creation of the given
 *	targets is set to 1. The toBeMade list is set to contain all the
 *	'leaves' of these subgraphs.
 *-----------------------------------------------------------------------
 */
Boolean
Make_Run (targs)
    Lst             targs;	/* the initial list of targets */
{
    register GNode  *gn;	/* a temporary pointer */
    register Lst    examine; 	/* List of targets to examine */
    int	    	    errors; 	/* Number of errors the Job module reports */

    toBeMade = Lst_Init (FALSE);

    examine = Lst_Duplicate(targs, NOCOPY);
    numNodes = 0;
    
    /*
     * Make an initial downward pass over the graph, marking nodes to be made
     * as we go down. We call Suff_FindDeps to find where a node is and
     * to get some children for it if it has none and also has no commands.
     * If the node is a leaf, we stick it on the toBeMade queue to
     * be looked at in a minute, otherwise we add its children to our queue
     * and go on about our business. 
     */
    while (!Lst_IsEmpty (examine)) {
	gn = (GNode *) Lst_DeQueue (examine);
	
	if (!gn->make) {
	    gn->make = TRUE;
	    numNodes++;
	    
	    /*
	     * Apply any .USE rules before looking for implicit dependencies
	     * to make sure everything has commands that should...
	     */
	    Lst_ForEach (gn->children, Make_HandleUse, (ClientData)gn);
	    Suff_FindDeps (gn);

	    if (gn->unmade != 0) {
		Lst_ForEach (gn->children, MakeAddChild, (ClientData)examine);
	    } else {
		(void)Lst_EnQueue (toBeMade, (ClientData)gn);
	    }
	}
    }
    
    Lst_Destroy (examine, NOFREE);

    if (queryFlag) {
	/*
	 * We wouldn't do any work unless we could start some jobs in the
	 * next loop... (we won't actually start any, of course, this is just
	 * to see if any of the targets was out of date)
	 */
	return (MakeStartJobs());
    } else {
	/*
	 * Initialization. At the moment, no jobs are running and until some
	 * get started, nothing will happen since the remaining upward
	 * traversal of the graph is performed by the routines in job.c upon
	 * the finishing of a job. So we fill the Job table as much as we can
	 * before going into our loop. 
	 */
	(void) MakeStartJobs();
    }

    /*
     * Main Loop: The idea here is that the ending of jobs will take
     * care of the maintenance of data structures and the waiting for output
     * will cause us to be idle most of the time while our children run as
     * much as possible. Because the job table is kept as full as possible,
     * the only time when it will be empty is when all the jobs which need
     * running have been run, so that is the end condition of this loop.
     * Note that the Job module will exit if there were any errors unless the
     * keepgoing flag was given.
     */
    while (!Job_Empty ()) {
	Job_CatchOutput ();
	Job_CatchChildren (!usePipes);
	(void)MakeStartJobs();
    }

    errors = Job_End();

    /*
     * Print the final status of each target. E.g. if it wasn't made
     * because some inferior reported an error.
     */
    Lst_ForEach(targs, MakePrintStatus,
		(ClientData)((errors == 0) && (numNodes != 0)));
    
    return (TRUE);
}
