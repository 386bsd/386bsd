/* $Header: /home/hyperion/mu/christos/src/sys/tcsh-6.01/RCS/tc.sched.c,v 3.6 1991/11/26 04:41:23 christos Exp $ */
/*
 * tc.sched.c: Scheduled command execution
 *
 * Karl Kleinpaste: Computer Consoles Inc. 1984
 */
/*-
 * Copyright (c) 1980, 1991 The Regents of the University of California.
 * All rights reserved.
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
#include "sh.h"

RCSID("$Id: tc.sched.c,v 3.6 1991/11/26 04:41:23 christos Exp $")

#include "ed.h"
#include "tc.h"

extern int just_signaled;

struct sched_event {
    struct sched_event *t_next;
    time_t t_when;
    Char  **t_lex;
};
static struct sched_event *sched_ptr = NULL;


time_t
sched_next()
{
    if (sched_ptr)
	return (sched_ptr->t_when);
    return ((time_t) - 1);
}

/*ARGSUSED*/
void
dosched(v, c)
    register Char **v;
    struct command *c;
{
    register struct sched_event *tp, *tp1, *tp2;
    time_t  cur_time;
    int     count, hours, minutes, dif_hour, dif_min;
    Char   *cp;
    bool    relative;		/* time specified as +hh:mm */
    struct tm *ltp;
    char   *timeline;
    char   *ctime();

/* This is a major kludge because of a gcc linker  */
/* Problem.  It may or may not be needed for you   */
#ifdef _MINIX
    char kludge[10];
    extern char *sprintf();
    sprintf(kludge,"kludge");
#endif /* _MINIX */

    v++;
    cp = *v++;
    if (cp == NULL) {
	/* print list of scheduled events */
	for (count = 1, tp = sched_ptr; tp; count++, tp = tp->t_next) {
	    timeline = ctime(&tp->t_when);
	    timeline[16] = '\0';
	    xprintf("%6d\t%s\t", count, timeline);
	    blkpr(tp->t_lex);
	    xprintf("\n");
	}
	return;
    }

    if (*cp == '-') {
	/* remove item from list */
	if (!sched_ptr)
	    stderror(ERR_NOSCHED);
	if (*v)
	    stderror(ERR_SCHEDUSAGE);
	count = atoi(short2str(++cp));
	if (count <= 0)
	    stderror(ERR_SCHEDUSAGE);
	tp = sched_ptr;
	tp1 = 0;
	while (--count) {
	    if (tp->t_next == 0)
		break;
	    else {
		tp1 = tp;
		tp = tp->t_next;
	    }
	}
	if (count)
	    stderror(ERR_SCHEDEV);
	if (tp1 == 0)
	    sched_ptr = tp->t_next;
	else
	    tp1->t_next = tp->t_next;
	blkfree(tp->t_lex);
	xfree((ptr_t) tp);
	return;
    }

    /* else, add an item to the list */
    if (!*v)
	stderror(ERR_SCHEDCOM);
    relative = 0;
    if (!Isdigit(*cp)) {	/* not abs. time */
	if (*cp != '+')
	    stderror(ERR_SCHEDUSAGE);
	cp++, relative++;
    }
    minutes = 0;
    hours = atoi(short2str(cp));
    while (*cp && *cp != ':' && *cp != 'a' && *cp != 'p')
	cp++;
    if (*cp && *cp == ':')
	minutes = atoi(short2str(++cp));
    if ((hours < 0) || (minutes < 0) ||
	(hours > 23) || (minutes > 59))
	stderror(ERR_SCHEDTIME);
    while (*cp && *cp != 'p' && *cp != 'a')
	cp++;
    if (*cp && relative)
	stderror(ERR_SCHEDREL);
    if (*cp == 'p')
	hours += 12;
    (void) time(&cur_time);
    ltp = localtime(&cur_time);
    if (relative) {
	dif_hour = hours;
	dif_min = minutes;
    }
    else {
	if ((dif_hour = hours - ltp->tm_hour) < 0)
	    dif_hour += 24;
	if ((dif_min = minutes - ltp->tm_min) < 0) {
	    dif_min += 60;
	    if ((--dif_hour) < 0)
		dif_hour = 23;
	}
    }
    tp = (struct sched_event *) xcalloc(1, sizeof *tp);
    tp->t_when = cur_time - ltp->tm_sec + dif_hour * 3600L + dif_min * 60L;
    /* use of tm_sec: get to beginning of minute. */
    if (!sched_ptr || tp->t_when < sched_ptr->t_when) {
	tp->t_next = sched_ptr;
	sched_ptr = tp;
    }
    else {
	tp1 = sched_ptr->t_next;
	tp2 = sched_ptr;
	while (tp1 && tp->t_when >= tp1->t_when) {
	    tp2 = tp1;
	    tp1 = tp1->t_next;
	}
	tp->t_next = tp1;
	tp2->t_next = tp;
    }
    tp->t_lex = saveblk(v);
}

/*
 * Execute scheduled events
 */
void
sched_run()
{
    time_t   cur_time;
    register struct sched_event *tp, *tp1;
    struct wordent cmd, *nextword, *lastword;
    struct command *t;
    Char  **v, *cp;
    extern Char GettingInput;

#ifdef BSDSIGS
    sigmask_t omask;

    omask = sigblock(sigmask(SIGINT)) & ~sigmask(SIGINT);
#else
    (void) sighold(SIGINT);
#endif

    (void) time(&cur_time);
    tp = sched_ptr;

    /* bugfix by: Justin Bur at Universite de Montreal */
    /*
     * this test wouldn't be necessary if this routine were not called before
     * each prompt (in sh.c).  But it is, to catch missed alarms.  Someone
     * ought to fix it all up.  -jbb
     */
    if (!(tp && tp->t_when < cur_time)) {
#ifdef BSDSIGS
	(void) sigsetmask(omask);
#else
	(void) sigrelse(SIGINT);
#endif
	return;
    }

    if (GettingInput)
	(void) Cookedmode();

    while (tp && tp->t_when < cur_time) {
	if (seterr) {
	    xfree((ptr_t) seterr);
	    seterr = NULL;
	}
	cmd.word = STRNULL;
	lastword = &cmd;
	v = tp->t_lex;
	for (cp = *v; cp; cp = *++v) {
	    nextword = (struct wordent *) xcalloc(1, sizeof cmd);
	    nextword->word = Strsave(cp);
	    lastword->next = nextword;
	    nextword->prev = lastword;
	    lastword = nextword;
	}
	lastword->next = &cmd;
	cmd.prev = lastword;
	tp1 = tp;
	sched_ptr = tp = tp1->t_next;	/* looping termination cond: */
	blkfree(tp1->t_lex);	/* straighten out in case of */
	xfree((ptr_t) tp1);	/* command blow-up. */

	/* expand aliases like process() does. */
	alias(&cmd);
	/* build a syntax tree for the command. */
	t = syntax(cmd.next, &cmd, 0);
	if (seterr)
	    stderror(ERR_OLD);
	/* execute the parse tree. */
	execute(t, -1, NULL, NULL);
	/* done. free the lex list and parse tree. */
	freelex(&cmd), freesyn(t);
    }
    if (GettingInput && !just_signaled) {	/* PWP */
	(void) Rawmode();
	ClearLines();		/* do a real refresh since something may */
	ClearDisp();		/* have printed to the screen */
	Refresh();
    }
    just_signaled = 0;

#ifdef BSDSIGS
    (void) sigsetmask(omask);
#else
    (void) sigrelse(SIGINT);
#endif
}
