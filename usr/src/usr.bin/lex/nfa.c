/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Vern Paxson of Lawrence Berkeley Laboratory.
 * 
 * The United States Government has rights in this work pursuant
 * to contract no. DE-AC03-76SF00098 between the United States
 * Department of Energy and the University of California.
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
static char sccsid[] = "@(#)nfa.c	5.2 (Berkeley) 6/18/90";
#endif /* not lint */

/* nfa - NFA construction routines */

#include "flexdef.h"

/* declare functions that have forward references */

int dupmachine PROTO((int));
void mkxtion PROTO((int, int));


/* add_accept - add an accepting state to a machine
 *
 * synopsis
 *
 *   add_accept( mach, accepting_number );
 *
 * accepting_number becomes mach's accepting number.
 */

void add_accept( mach, accepting_number )
int mach, accepting_number;

    {
    /* hang the accepting number off an epsilon state.  if it is associated
     * with a state that has a non-epsilon out-transition, then the state
     * will accept BEFORE it makes that transition, i.e., one character
     * too soon
     */

    if ( transchar[finalst[mach]] == SYM_EPSILON )
	accptnum[finalst[mach]] = accepting_number;

    else
	{
	int astate = mkstate( SYM_EPSILON );
	accptnum[astate] = accepting_number;
	mach = link_machines( mach, astate );
	}
    }


/* copysingl - make a given number of copies of a singleton machine
 *
 * synopsis
 *
 *   newsng = copysingl( singl, num );
 *
 *     newsng - a new singleton composed of num copies of singl
 *     singl  - a singleton machine
 *     num    - the number of copies of singl to be present in newsng
 */

int copysingl( singl, num )
int singl, num;

    {
    int copy, i;

    copy = mkstate( SYM_EPSILON );

    for ( i = 1; i <= num; ++i )
	copy = link_machines( copy, dupmachine( singl ) );

    return ( copy );
    }


/* dumpnfa - debugging routine to write out an nfa
 *
 * synopsis
 *    int state1;
 *    dumpnfa( state1 );
 */

void dumpnfa( state1 )
int state1;

    {
    int sym, tsp1, tsp2, anum, ns;

    fprintf( stderr, "\n\n********** beginning dump of nfa with start state %d\n",
	     state1 );

    /* we probably should loop starting at firstst[state1] and going to
     * lastst[state1], but they're not maintained properly when we "or"
     * all of the rules together.  So we use our knowledge that the machine
     * starts at state 1 and ends at lastnfa.
     */

    /* for ( ns = firstst[state1]; ns <= lastst[state1]; ++ns ) */
    for ( ns = 1; ns <= lastnfa; ++ns )
	{
	fprintf( stderr, "state # %4d\t", ns );

	sym = transchar[ns];
	tsp1 = trans1[ns];
	tsp2 = trans2[ns];
	anum = accptnum[ns];

	fprintf( stderr, "%3d:  %4d, %4d", sym, tsp1, tsp2 );

	if ( anum != NIL )
	    fprintf( stderr, "  [%d]", anum );

	fprintf( stderr, "\n" );
	}

    fprintf( stderr, "********** end of dump\n" );
    }


/* dupmachine - make a duplicate of a given machine
 *
 * synopsis
 *
 *   copy = dupmachine( mach );
 *
 *     copy - holds duplicate of mach
 *     mach - machine to be duplicated
 *
 * note that the copy of mach is NOT an exact duplicate; rather, all the
 * transition states values are adjusted so that the copy is self-contained,
 * as the original should have been.
 *
 * also note that the original MUST be contiguous, with its low and high
 * states accessible by the arrays firstst and lastst
 */

int dupmachine( mach )
int mach;

    {
    int i, init, state_offset;
    int state = 0;
    int last = lastst[mach];

    for ( i = firstst[mach]; i <= last; ++i )
	{
	state = mkstate( transchar[i] );

	if ( trans1[i] != NO_TRANSITION )
	    {
	    mkxtion( finalst[state], trans1[i] + state - i );

	    if ( transchar[i] == SYM_EPSILON && trans2[i] != NO_TRANSITION )
		mkxtion( finalst[state], trans2[i] + state - i );
	    }

	accptnum[state] = accptnum[i];
	}

    if ( state == 0 )
	flexfatal( "empty machine in dupmachine()" );

    state_offset = state - i + 1;

    init = mach + state_offset;
    firstst[init] = firstst[mach] + state_offset;
    finalst[init] = finalst[mach] + state_offset;
    lastst[init] = lastst[mach] + state_offset;

    return ( init );
    }


/* finish_rule - finish up the processing for a rule
 *
 * synopsis
 *
 *   finish_rule( mach, variable_trail_rule, headcnt, trailcnt );
 *
 * An accepting number is added to the given machine.  If variable_trail_rule
 * is true then the rule has trailing context and both the head and trail
 * are variable size.  Otherwise if headcnt or trailcnt is non-zero then
 * the machine recognizes a pattern with trailing context and headcnt is
 * the number of characters in the matched part of the pattern, or zero
 * if the matched part has variable length.  trailcnt is the number of
 * trailing context characters in the pattern, or zero if the trailing
 * context has variable length.
 */

void finish_rule( mach, variable_trail_rule, headcnt, trailcnt )
int mach, variable_trail_rule, headcnt, trailcnt;

    {
    add_accept( mach, num_rules );

    /* we did this in new_rule(), but it often gets the wrong
     * number because we do it before we start parsing the current rule
     */
    rule_linenum[num_rules] = linenum;

    /* if this is a continued action, then the line-number has
     * already been updated, giving us the wrong number
     */
    if ( continued_action )
	--rule_linenum[num_rules];

    fprintf( temp_action_file, "case %d:\n", num_rules );

    if ( variable_trail_rule )
	{
	rule_type[num_rules] = RULE_VARIABLE;

	if ( performance_report )
	    fprintf( stderr, "Variable trailing context rule at line %d\n",
		     rule_linenum[num_rules] );

	variable_trailing_context_rules = true;
	}

    else
	{
	rule_type[num_rules] = RULE_NORMAL;

	if ( headcnt > 0 || trailcnt > 0 )
	    {
	    /* do trailing context magic to not match the trailing characters */
	    char *scanner_cp = "yy_c_buf_p = yy_cp";
	    char *scanner_bp = "yy_bp";

	    fprintf( temp_action_file,
	"*yy_cp = yy_hold_char; /* undo effects of setting up yytext */\n" );

	    if ( headcnt > 0 )
		fprintf( temp_action_file, "%s = %s + %d;\n",
			 scanner_cp, scanner_bp, headcnt );

	    else
		fprintf( temp_action_file,
			 "%s -= %d;\n", scanner_cp, trailcnt );
	
	    fprintf( temp_action_file,
		     "YY_DO_BEFORE_ACTION; /* set up yytext again */\n" );
	    }
	}

    line_directive_out( temp_action_file );
    }


/* link_machines - connect two machines together
 *
 * synopsis
 *
 *   new = link_machines( first, last );
 *
 *     new    - a machine constructed by connecting first to last
 *     first  - the machine whose successor is to be last
 *     last   - the machine whose predecessor is to be first
 *
 * note: this routine concatenates the machine first with the machine
 *  last to produce a machine new which will pattern-match first first
 *  and then last, and will fail if either of the sub-patterns fails.
 *  FIRST is set to new by the operation.  last is unmolested.
 */

int link_machines( first, last )
int first, last;

    {
    if ( first == NIL )
	return ( last );

    else if ( last == NIL )
	return ( first );

    else
	{
	mkxtion( finalst[first], last );
	finalst[first] = finalst[last];
	lastst[first] = max( lastst[first], lastst[last] );
	firstst[first] = min( firstst[first], firstst[last] );

	return ( first );
	}
    }


/* mark_beginning_as_normal - mark each "beginning" state in a machine
 *                            as being a "normal" (i.e., not trailing context-
 *                            associated) states
 *
 * synopsis
 *
 *   mark_beginning_as_normal( mach )
 *
 *     mach - machine to mark
 *
 * The "beginning" states are the epsilon closure of the first state
 */

void mark_beginning_as_normal( mach )
register int mach;

    {
    switch ( state_type[mach] )
	{
	case STATE_NORMAL:
	    /* oh, we've already visited here */
	    return;

	case STATE_TRAILING_CONTEXT:
	    state_type[mach] = STATE_NORMAL;

	    if ( transchar[mach] == SYM_EPSILON )
		{
		if ( trans1[mach] != NO_TRANSITION )
		    mark_beginning_as_normal( trans1[mach] );

		if ( trans2[mach] != NO_TRANSITION )
		    mark_beginning_as_normal( trans2[mach] );
		}
	    break;

	default:
	    flexerror( "bad state type in mark_beginning_as_normal()" );
	    break;
	}
    }


/* mkbranch - make a machine that branches to two machines
 *
 * synopsis
 *
 *   branch = mkbranch( first, second );
 *
 *     branch - a machine which matches either first's pattern or second's
 *     first, second - machines whose patterns are to be or'ed (the | operator)
 *
 * note that first and second are NEITHER destroyed by the operation.  Also,
 * the resulting machine CANNOT be used with any other "mk" operation except
 * more mkbranch's.  Compare with mkor()
 */

int mkbranch( first, second )
int first, second;

    {
    int eps;

    if ( first == NO_TRANSITION )
	return ( second );

    else if ( second == NO_TRANSITION )
	return ( first );

    eps = mkstate( SYM_EPSILON );

    mkxtion( eps, first );
    mkxtion( eps, second );

    return ( eps );
    }


/* mkclos - convert a machine into a closure
 *
 * synopsis
 *   new = mkclos( state );
 *
 *     new - a new state which matches the closure of "state"
 */

int mkclos( state )
int state;

    {
    return ( mkopt( mkposcl( state ) ) );
    }


/* mkopt - make a machine optional
 *
 * synopsis
 *
 *   new = mkopt( mach );
 *
 *     new  - a machine which optionally matches whatever mach matched
 *     mach - the machine to make optional
 *
 * notes:
 *     1. mach must be the last machine created
 *     2. mach is destroyed by the call
 */

int mkopt( mach )
int mach;

    {
    int eps;

    if ( ! SUPER_FREE_EPSILON(finalst[mach]) )
	{
	eps = mkstate( SYM_EPSILON );
	mach = link_machines( mach, eps );
	}

    /* can't skimp on the following if FREE_EPSILON(mach) is true because
     * some state interior to "mach" might point back to the beginning
     * for a closure
     */
    eps = mkstate( SYM_EPSILON );
    mach = link_machines( eps, mach );

    mkxtion( mach, finalst[mach] );

    return ( mach );
    }


/* mkor - make a machine that matches either one of two machines
 *
 * synopsis
 *
 *   new = mkor( first, second );
 *
 *     new - a machine which matches either first's pattern or second's
 *     first, second - machines whose patterns are to be or'ed (the | operator)
 *
 * note that first and second are both destroyed by the operation
 * the code is rather convoluted because an attempt is made to minimize
 * the number of epsilon states needed
 */

int mkor( first, second )
int first, second;

    {
    int eps, orend;

    if ( first == NIL )
	return ( second );

    else if ( second == NIL )
	return ( first );

    else
	{
	/* see comment in mkopt() about why we can't use the first state
	 * of "first" or "second" if they satisfy "FREE_EPSILON"
	 */
	eps = mkstate( SYM_EPSILON );

	first = link_machines( eps, first );

	mkxtion( first, second );

	if ( SUPER_FREE_EPSILON(finalst[first]) &&
	     accptnum[finalst[first]] == NIL )
	    {
	    orend = finalst[first];
	    mkxtion( finalst[second], orend );
	    }

	else if ( SUPER_FREE_EPSILON(finalst[second]) &&
		  accptnum[finalst[second]] == NIL )
	    {
	    orend = finalst[second];
	    mkxtion( finalst[first], orend );
	    }

	else
	    {
	    eps = mkstate( SYM_EPSILON );

	    first = link_machines( first, eps );
	    orend = finalst[first];

	    mkxtion( finalst[second], orend );
	    }
	}

    finalst[first] = orend;
    return ( first );
    }


/* mkposcl - convert a machine into a positive closure
 *
 * synopsis
 *   new = mkposcl( state );
 *
 *    new - a machine matching the positive closure of "state"
 */

int mkposcl( state )
int state;

    {
    int eps;

    if ( SUPER_FREE_EPSILON(finalst[state]) )
	{
	mkxtion( finalst[state], state );
	return ( state );
	}

    else
	{
	eps = mkstate( SYM_EPSILON );
	mkxtion( eps, state );
	return ( link_machines( state, eps ) );
	}
    }


/* mkrep - make a replicated machine
 *
 * synopsis
 *   new = mkrep( mach, lb, ub );
 *
 *    new - a machine that matches whatever "mach" matched from "lb"
 *          number of times to "ub" number of times
 *
 * note
 *   if "ub" is INFINITY then "new" matches "lb" or more occurrences of "mach"
 */

int mkrep( mach, lb, ub )
int mach, lb, ub;

    {
    int base_mach, tail, copy, i;

    base_mach = copysingl( mach, lb - 1 );

    if ( ub == INFINITY )
	{
	copy = dupmachine( mach );
	mach = link_machines( mach,
			      link_machines( base_mach, mkclos( copy ) ) );
	}

    else
	{
	tail = mkstate( SYM_EPSILON );

	for ( i = lb; i < ub; ++i )
	    {
	    copy = dupmachine( mach );
	    tail = mkopt( link_machines( copy, tail ) );
	    }

	mach = link_machines( mach, link_machines( base_mach, tail ) );
	}

    return ( mach );
    }


/* mkstate - create a state with a transition on a given symbol
 *
 * synopsis
 *
 *   state = mkstate( sym );
 *
 *     state - a new state matching sym
 *     sym   - the symbol the new state is to have an out-transition on
 *
 * note that this routine makes new states in ascending order through the
 * state array (and increments LASTNFA accordingly).  The routine DUPMACHINE
 * relies on machines being made in ascending order and that they are
 * CONTIGUOUS.  Change it and you will have to rewrite DUPMACHINE (kludge
 * that it admittedly is)
 */

int mkstate( sym )
int sym;

    {
    if ( ++lastnfa >= current_mns )
	{
	if ( (current_mns += MNS_INCREMENT) >= MAXIMUM_MNS )
	    lerrif( "input rules are too complicated (>= %d NFA states)",
		    current_mns );
	
	++num_reallocs;

	firstst = reallocate_integer_array( firstst, current_mns );
	lastst = reallocate_integer_array( lastst, current_mns );
	finalst = reallocate_integer_array( finalst, current_mns );
	transchar = reallocate_integer_array( transchar, current_mns );
	trans1 = reallocate_integer_array( trans1, current_mns );
	trans2 = reallocate_integer_array( trans2, current_mns );
	accptnum = reallocate_integer_array( accptnum, current_mns );
	assoc_rule = reallocate_integer_array( assoc_rule, current_mns );
	state_type = reallocate_integer_array( state_type, current_mns );
	}

    firstst[lastnfa] = lastnfa;
    finalst[lastnfa] = lastnfa;
    lastst[lastnfa] = lastnfa;
    transchar[lastnfa] = sym;
    trans1[lastnfa] = NO_TRANSITION;
    trans2[lastnfa] = NO_TRANSITION;
    accptnum[lastnfa] = NIL;
    assoc_rule[lastnfa] = num_rules;
    state_type[lastnfa] = current_state_type;

    /* fix up equivalence classes base on this transition.  Note that any
     * character which has its own transition gets its own equivalence class.
     * Thus only characters which are only in character classes have a chance
     * at being in the same equivalence class.  E.g. "a|b" puts 'a' and 'b'
     * into two different equivalence classes.  "[ab]" puts them in the same
     * equivalence class (barring other differences elsewhere in the input).
     */

    if ( sym < 0 )
	{
	/* we don't have to update the equivalence classes since that was
	 * already done when the ccl was created for the first time
	 */
	}

    else if ( sym == SYM_EPSILON )
	++numeps;

    else
	{
	if ( useecs )
	    /* map NUL's to csize */
	    mkechar( sym ? sym : csize, nextecm, ecgroup );
	}

    return ( lastnfa );
    }


/* mkxtion - make a transition from one state to another
 *
 * synopsis
 *
 *   mkxtion( statefrom, stateto );
 *
 *     statefrom - the state from which the transition is to be made
 *     stateto   - the state to which the transition is to be made
 */

void mkxtion( statefrom, stateto )
int statefrom, stateto;

    {
    if ( trans1[statefrom] == NO_TRANSITION )
	trans1[statefrom] = stateto;

    else if ( (transchar[statefrom] != SYM_EPSILON) ||
	      (trans2[statefrom] != NO_TRANSITION) )
	flexfatal( "found too many transitions in mkxtion()" );

    else
	{ /* second out-transition for an epsilon state */
	++eps2;
	trans2[statefrom] = stateto;
	}
    }

/* new_rule - initialize for a new rule
 *
 * synopsis
 *
 *   new_rule();
 *
 * the global num_rules is incremented and the any corresponding dynamic
 * arrays (such as rule_type[]) are grown as needed.
 */

void new_rule()

    {
    if ( ++num_rules >= current_max_rules )
	{
	++num_reallocs;
	current_max_rules += MAX_RULES_INCREMENT;
	rule_type = reallocate_integer_array( rule_type, current_max_rules );
	rule_linenum =
	    reallocate_integer_array( rule_linenum, current_max_rules );
	}

    if ( num_rules > MAX_RULE )
	lerrif( "too many rules (> %d)!", MAX_RULE );

    rule_linenum[num_rules] = linenum;
    }
