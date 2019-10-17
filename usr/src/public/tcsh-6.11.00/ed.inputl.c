/* $Header: /src/pub/tcsh/ed.inputl.c,v 3.49 2000/11/11 23:03:34 christos Exp $ */
/*
 * ed.inputl.c: Input line handling.
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

RCSID("$Id: ed.inputl.c,v 3.49 2000/11/11 23:03:34 christos Exp $")

#include "ed.h"
#include "ed.defns.h"		/* for the function names */
#include "tw.h"			/* for twenex stuff */

#define OKCMD (INBUFSIZE+INBUFSIZE)

/* ed.inputl -- routines to get a single line from the input. */

extern bool tellwhat;
extern bool MapsAreInited;
extern bool Tty_raw_mode;

/* mismatched first character */
static Char mismatch[] = 
    {'!', '^' , '\\', '-', '%', '\0', '"', '\'', '`', '\0' };

static	int	Repair		__P((void));
static	int	GetNextCommand	__P((KEYCMD *, Char *));
static	int	SpellLine	__P((int));
static	int	CompleteLine	__P((void));
static	void	RunCommand	__P((Char *));
static  void 	doeval1		__P((Char **));

static bool rotate = 0;


static int
Repair()
{
    if (NeedsRedraw) {
	ClearLines();
	ClearDisp();
	NeedsRedraw = 0;
    }
    Refresh();
    Argument = 1;
    DoingArg = 0;
    curchoice = -1;
    return (int) (LastChar - InputBuf);
}

/* CCRETVAL */
int
Inputl()
{
    CCRETVAL retval;
    KEYCMD  cmdnum = 0;
    extern KEYCMD NumFuns;
    unsigned char tch;		/* the place where read() goes */
    Char    ch;
    int     num;		/* how many chars we have read at NL */
    int	    expnum;
    struct varent *crct = inheredoc ? NULL : adrof(STRcorrect);
    struct varent *autol = adrof(STRautolist);
    struct varent *matchbeep = adrof(STRmatchbeep);
    struct varent *imode = adrof(STRinputmode);
    Char   *SaveChar, *CorrChar;
    Char    Origin[INBUFSIZE], Change[INBUFSIZE];
    int     matchval;		/* from tenematch() */
    COMMAND fn;
    int curlen = 0;
    int newlen;
    int idx;

    if (!MapsAreInited)		/* double extra just in case */
	ed_InitMaps();

    ClearDisp();		/* reset the display stuff */
    ResetInLine(0);		/* reset the input pointers */
    if (GettingInput)
	MacroLvl = -1;		/* editor was interrupted during input */

    if (imode) {
	if (!Strcmp(*(imode->vec), STRinsert))
	    inputmode = MODE_INSERT;
	else if (!Strcmp(*(imode->vec), STRoverwrite))
	    inputmode = MODE_REPLACE;
    }

#if defined(FIONREAD) && !defined(OREO)
    if (!Tty_raw_mode && MacroLvl < 0) {
# ifdef SUNOS4
	long chrs = 0;
# else /* !SUNOS4 */
	/* 
	 * *Everyone* else has an int, but SunOS wants long!
	 * This breaks where int != long (alpha)
	 */
	int chrs = 0;
# endif /* SUNOS4 */

	(void) ioctl(SHIN, FIONREAD, (ioctl_t) & chrs);
	if (chrs == 0) {
	    if (Rawmode() < 0)
		return 0;
	}
    }
#endif /* FIONREAD && !OREO */

    GettingInput = 1;
    NeedsRedraw = 0;

    if (tellwhat) {
	copyn(InputBuf, WhichBuf, INBUFSIZE);
	LastChar = InputBuf + (LastWhich - WhichBuf);
	Cursor = InputBuf + (CursWhich - WhichBuf);
	tellwhat = 0;
	Hist_num = HistWhich;
    }
    if (Expand) {
	(void) e_up_hist(0);
	Expand = 0;
    }
    Refresh();			/* print the prompt */

    for (num = OKCMD; num == OKCMD;) {	/* while still editing this line */
#ifdef DEBUG_EDIT
	if (Cursor > LastChar)
	    xprintf("Cursor > LastChar\r\n");
	if (Cursor < InputBuf)
	    xprintf("Cursor < InputBuf\r\n");
	if (Cursor > InputLim)
	    xprintf("Cursor > InputLim\r\n");
	if (LastChar > InputLim)
	    xprintf("LastChar > InputLim\r\n");
	if (InputLim != &InputBuf[INBUFSIZE - 2])
	    xprintf("InputLim != &InputBuf[INBUFSIZE-2]\r\n");
	if ((!DoingArg) && (Argument != 1))
	    xprintf("(!DoingArg) && (Argument != 1)\r\n");
	if (CcKeyMap[0] == 0)
	    xprintf("CcKeyMap[0] == 0 (maybe not inited)\r\n");
#endif

	/* if EOF or error */
	if ((num = GetNextCommand(&cmdnum, &ch)) != OKCMD) {
	    break;
	}

	if (cmdnum >= NumFuns) {/* BUG CHECK command */
#ifdef DEBUG_EDIT
	    xprintf(CGETS(6, 1, "ERROR: illegal command from key 0%o\r\n"), ch);
#endif
	    continue;		/* try again */
	}

	/* now do the real command */
	retval = (*CcFuncTbl[cmdnum]) (ch);

	/* save the last command here */
	LastCmd = cmdnum;

	/* make sure fn is initialized */
	fn = (retval == CC_COMPLETE_ALL) ? LIST_ALL : LIST;

	/* use any return value */
	switch (retval) {

	case CC_REFRESH:
	    Refresh();
	    /*FALLTHROUGH*/
	case CC_NORM:		/* normal char */
	    Argument = 1;
	    DoingArg = 0;
	    /*FALLTHROUGH*/
	case CC_ARGHACK:	/* Suggested by Rich Salz */
	    /* <rsalz@pineapple.bbn.com> */
	    curchoice = -1;
	    curlen = (int) (LastChar - InputBuf);
	    break;		/* keep going... */

	case CC_EOF:		/* end of file typed */
	    curchoice = -1;
	    curlen = (int) (LastChar - InputBuf);
	    num = 0;
	    break;

	case CC_WHICH:		/* tell what this command does */
	    tellwhat = 1;
	    copyn(WhichBuf, InputBuf, INBUFSIZE);
	    LastWhich = WhichBuf + (LastChar - InputBuf);
	    CursWhich = WhichBuf + (Cursor - InputBuf);
	    *LastChar++ = '\n';	/* for the benifit of CSH */
	    HistWhich = Hist_num;
	    Hist_num = 0;	/* for the history commands */
	    num = (int) (LastChar - InputBuf);	/* number characters read */
	    break;

	case CC_NEWLINE:	/* normal end of line */
	    curlen = 0;
	    curchoice = -1;
	    matchval = 1;
	    if (crct && (!Strcmp(*(crct->vec), STRcmd) ||
			 !Strcmp(*(crct->vec), STRall))) {
                PastBottom();
		copyn(Origin, InputBuf, INBUFSIZE);
		SaveChar = LastChar;
		if (SpellLine(!Strcmp(*(crct->vec), STRcmd)) == 1) {
                    PastBottom();
		    copyn(Change, InputBuf, INBUFSIZE);
		    *Strchr(Change, '\n') = '\0';
		    CorrChar = LastChar;	/* Save the corrected end */
		    LastChar = InputBuf;	/* Null the current line */
		    SoundBeep();
		    printprompt(2, short2str(Change));
		    Refresh();
		    if (read(SHIN, (char *) &tch, 1) < 0)
#ifdef convex
		        /*
			 * need to print error message in case file
			 * is migrated
			 */
                        if (errno && errno != EINTR)
                            stderror(ERR_SYSTEM, progname, strerror(errno));
#else
			break;
#endif
		    ch = tch;
		    if (ch == 'y' || ch == ' ') {
			LastChar = CorrChar;	/* Restore the corrected end */
			xprintf(CGETS(6, 2, "yes\n"));
		    }
		    else {
			copyn(InputBuf, Origin, INBUFSIZE);
			LastChar = SaveChar;
			if (ch == 'e') {
			    xprintf(CGETS(6, 3, "edit\n"));
			    *LastChar-- = '\0';
			    Cursor = LastChar;
			    printprompt(3, NULL);
			    ClearLines();
			    ClearDisp();
			    Refresh();
			    break;
			}
			else if (ch == 'a') {
			    xprintf(CGETS(6, 4, "abort\n"));
		            LastChar = InputBuf;   /* Null the current line */
			    Cursor = LastChar;
			    printprompt(0, NULL);
			    Refresh();
			    break;
			}
			xprintf(CGETS(6, 5, "no\n"));
		    }
		    flush();
		}
	    } else if (crct && !Strcmp(*(crct->vec), STRcomplete)) {
                if (LastChar > InputBuf && LastChar[-1] == '\n') {
                    LastChar[-1] = '\0';
                    LastChar--;
                    Cursor = LastChar;
                }
                match_unique_match = 1;  /* match unique matches */
		matchval = CompleteLine();
                match_unique_match = 0;
        	curlen = (int) (LastChar - InputBuf);
		if (matchval != 1) {
                    PastBottom();
		}
		if (matchval == 0) {
		    xprintf(CGETS(6, 6, "No matching command\n"));
		} else if (matchval == 2) {
		    xprintf(CGETS(6, 7, "Ambiguous command\n"));
		}
	        if (NeedsRedraw) {
		    ClearLines();
		    ClearDisp();
		    NeedsRedraw = 0;
	        }
	        Refresh();
	        Argument = 1;
	        DoingArg = 0;
		if (matchval == 1) {
                    PastBottom();
                    *LastChar++ = '\n';
                    *LastChar = '\0';
		}
        	curlen = (int) (LastChar - InputBuf);
            }
	    else
		PastBottom();

	    if (matchval == 1) {
	        tellwhat = 0;	/* just in case */
	        Hist_num = 0;	/* for the history commands */
		/* return the number of chars read */
	        num = (int) (LastChar - InputBuf);
	        /*
	         * For continuation lines, we set the prompt to prompt 2
	         */
	        printprompt(1, NULL);
	    }
	    break;

	case CC_CORRECT:
	    if (tenematch(InputBuf, Cursor - InputBuf, SPELL) < 0)
		SoundBeep();		/* Beep = No match/ambiguous */
	    curlen = Repair();
	    break;

	case CC_CORRECT_L:
	    if (SpellLine(FALSE) < 0)
		SoundBeep();		/* Beep = No match/ambiguous */
	    curlen = Repair();
	    break;


	case CC_COMPLETE:
	case CC_COMPLETE_ALL:
	case CC_COMPLETE_FWD:
	case CC_COMPLETE_BACK:
	    switch (retval) {
	    case CC_COMPLETE:
		fn = RECOGNIZE;
		curlen = (int) (LastChar - InputBuf);
		curchoice = -1;
		rotate = 0;
		break;
	    case CC_COMPLETE_ALL:
		fn = RECOGNIZE_ALL;
		curlen = (int) (LastChar - InputBuf);
		curchoice = -1;
		rotate = 0;
		break;
	    case CC_COMPLETE_FWD:
		fn = RECOGNIZE_SCROLL;
		curchoice++;
		rotate = 1;
		break;
	    case CC_COMPLETE_BACK:
		fn = RECOGNIZE_SCROLL;
		curchoice--;
		rotate = 1;
		break;
	    default:
		abort();
	    }
	    if (InputBuf[curlen] && rotate) {
		newlen = (int) (LastChar - InputBuf);
		for (idx = (int) (Cursor - InputBuf); 
		     idx <= newlen; idx++)
			InputBuf[idx - newlen + curlen] =
			InputBuf[idx];
		LastChar = InputBuf + curlen;
		Cursor = Cursor - newlen + curlen;
	    }
	    curlen = (int) (LastChar - InputBuf);


	    if (adrof(STRautoexpand))
		(void) e_expand_history(0);
	    /*
	     * Modified by Martin Boyer (gamin@ireq-robot.hydro.qc.ca):
	     * A separate variable now controls beeping after
	     * completion, independently of autolisting.
	     */
	    expnum = (int) (Cursor - InputBuf);
	    switch (matchval = tenematch(InputBuf, Cursor-InputBuf, fn)){
	    case 1:
		if (non_unique_match && matchbeep &&
		    (Strcmp(*(matchbeep->vec), STRnotunique) == 0))
		    SoundBeep();
		break;
	    case 0:
		if (matchbeep) {
		    if (Strcmp(*(matchbeep->vec), STRnomatch) == 0 ||
			Strcmp(*(matchbeep->vec), STRambiguous) == 0 ||
			Strcmp(*(matchbeep->vec), STRnotunique) == 0)
			SoundBeep();
		}
		else
		    SoundBeep();
		break;
	    default:
		if (matchval < 0) {	/* Error from tenematch */
		    curchoice = -1;
		    SoundBeep();
		    break;
		}
		if (matchbeep) {
		    if ((Strcmp(*(matchbeep->vec), STRambiguous) == 0 ||
			 Strcmp(*(matchbeep->vec), STRnotunique) == 0))
			SoundBeep();
		}
		else
		    SoundBeep();
		/*
		 * Addition by David C Lawrence <tale@pawl.rpi.edu>: If an 
		 * attempted completion is ambiguous, list the choices.  
		 * (PWP: this is the best feature addition to tcsh I have 
		 * seen in many months.)
		 */
		if (autol && (Strcmp(*(autol->vec), STRambiguous) != 0 || 
				     expnum == Cursor - InputBuf)) {
		    PastBottom();
		    fn = (retval == CC_COMPLETE_ALL) ? LIST_ALL : LIST;
		    (void) tenematch(InputBuf, Cursor-InputBuf, fn);
		}
		break;
	    }
	    if (NeedsRedraw) {
		PastBottom();
		ClearLines();
		ClearDisp();
		NeedsRedraw = 0;
	    }
	    Refresh();
	    Argument = 1;
	    DoingArg = 0;
	    break;

	case CC_LIST_CHOICES:
	case CC_LIST_ALL:
	    if (InputBuf[curlen] && rotate) {
		newlen = (int) (LastChar - InputBuf);
		for (idx = (int) (Cursor - InputBuf); 
		     idx <= newlen; idx++)
			InputBuf[idx - newlen + curlen] =
			InputBuf[idx];
		LastChar = InputBuf + curlen;
		Cursor = Cursor - newlen + curlen;
	    }
	    curlen = (int) (LastChar - InputBuf);
	    if (curchoice >= 0)
		curchoice--;

	    fn = (retval == CC_LIST_ALL) ? LIST_ALL : LIST;
	    /* should catch ^C here... */
	    if (tenematch(InputBuf, Cursor - InputBuf, fn) < 0)
		SoundBeep();
	    Refresh();
	    Argument = 1;
	    DoingArg = 0;
	    break;


	case CC_LIST_GLOB:
	    if (tenematch(InputBuf, Cursor - InputBuf, GLOB) < 0)
		SoundBeep();
	    curlen = Repair();
	    break;

	case CC_EXPAND_GLOB:
	    if (tenematch(InputBuf, Cursor - InputBuf, GLOB_EXPAND) <= 0)
		SoundBeep();		/* Beep = No match */
	    curlen = Repair();
	    break;

	case CC_NORMALIZE_PATH:
	    if (tenematch(InputBuf, Cursor - InputBuf, PATH_NORMALIZE) <= 0)
		SoundBeep();		/* Beep = No match */
	    curlen = Repair();
	    break;

	case CC_EXPAND_VARS:
	    if (tenematch(InputBuf, Cursor - InputBuf, VARS_EXPAND) <= 0)
		SoundBeep();		/* Beep = No match */
	    curlen = Repair();
	    break;

	case CC_NORMALIZE_COMMAND:
	    if (tenematch(InputBuf, Cursor - InputBuf, COMMAND_NORMALIZE) <= 0)
		SoundBeep();		/* Beep = No match */
	    curlen = Repair();
	    break;

	case CC_HELPME:
	    xputchar('\n');
	    /* should catch ^C here... */
	    (void) tenematch(InputBuf, LastChar - InputBuf, PRINT_HELP);
	    Refresh();
	    Argument = 1;
	    DoingArg = 0;
	    curchoice = -1;
	    curlen = (int) (LastChar - InputBuf);
	    break;

	case CC_FATAL:		/* fatal error, reset to known state */
#ifdef DEBUG_EDIT
	    xprintf(CGETS(7, 8, "*** editor fatal ERROR ***\r\n\n"));
#endif				/* DEBUG_EDIT */
	    /* put (real) cursor in a known place */
	    ClearDisp();	/* reset the display stuff */
	    ResetInLine(1);	/* reset the input pointers */
	    Refresh();		/* print the prompt again */
	    Argument = 1;
	    DoingArg = 0;
	    curchoice = -1;
	    curlen = (int) (LastChar - InputBuf);
	    break;

	case CC_ERROR:
	default:		/* functions we don't know about */
	    DoingArg = 0;
	    Argument = 1;
	    SoundBeep();
	    flush();
	    curchoice = -1;
	    curlen = (int) (LastChar - InputBuf);
	    break;
	}
    }
    (void) Cookedmode();	/* make sure the tty is set up correctly */
    GettingInput = 0;
    flush();			/* flush any buffered output */
    return num;
}

void
PushMacro(str)
    Char   *str;
{
    if (str != NULL && MacroLvl + 1 < MAXMACROLEVELS) {
	MacroLvl++;
	KeyMacro[MacroLvl] = str;
    }
    else {
	SoundBeep();
	flush();
    }
}

/*
 * Like eval, only using the current file descriptors
 */
static Char **gv = NULL, **gav = NULL;

static void
doeval1(v)
    Char **v;
{
    Char  **oevalvec;
    Char   *oevalp;
    int     my_reenter;
    Char  **savegv;
    jmp_buf_t osetexit;

    oevalvec = evalvec;
    oevalp = evalp;
    savegv = gv;
    gav = v;


    gflag = 0, tglob(gav);
    if (gflag) {
	gv = gav = globall(gav);
	gargv = 0;
	if (gav == 0)
	    stderror(ERR_NOMATCH);
	gav = copyblk(gav);
    }
    else {
	gv = NULL;
	gav = copyblk(gav);
	trim(gav);
    }

    getexit(osetexit);

    /* PWP: setjmp/longjmp bugfix for optimizing compilers */
#ifdef cray
    my_reenter = 1;             /* assume non-zero return val */
    if (setexit() == 0) {
        my_reenter = 0;         /* Oh well, we were wrong */
#else /* !cray */
    if ((my_reenter = setexit()) == 0) {
#endif /* cray */
	evalvec = gav;
	evalp = 0;
	process(0);
    }

    evalvec = oevalvec;
    evalp = oevalp;
    doneinp = 0;

    if (gv)
	blkfree(gv);

    gv = savegv;
    resexit(osetexit);
    if (my_reenter)
	stderror(ERR_SILENT);
}

static void
RunCommand(str)
    Char *str;
{
    Char *cmd[2];

    xputchar('\n');	/* Start on a clean line */

    cmd[0] = str;
    cmd[1] = NULL;

    (void) Cookedmode();
    GettingInput = 0;

    doeval1(cmd);
    
    (void) Rawmode();
    GettingInput = 1;

    ClearLines();
    ClearDisp();
    NeedsRedraw = 0;
    Refresh();
}

static int
GetNextCommand(cmdnum, ch)
    KEYCMD *cmdnum;
    register Char *ch;
{
    KEYCMD  cmd = 0;
    int     num;

    while (cmd == 0 || cmd == F_XKEY) {
	if ((num = GetNextChar(ch)) != 1) {	/* if EOF or error */
	    return num;
	}
#ifdef	KANJI
	if (
#ifdef DSPMBYTE
	     _enable_mbdisp &&
#endif
	     !adrof(STRnokanji) && (*ch & META)) {
	    MetaNext = 0;
	    cmd = F_INSERT;
	    break;
	}
	else
#endif /* KANJI */
	if (MetaNext) {
	    MetaNext = 0;
	    *ch |= META;
	}
	/* XXX: This needs to be fixed so that we don't just truncate
	 * the character, we unquote it.
	 */
	if (*ch < NT_NUM_KEYS)
	    cmd = CurrentKeyMap[*ch];
	else
	    cmd = CurrentKeyMap[(unsigned char) *ch];
	if (cmd == F_XKEY) {
	    XmapVal val;
	    CStr cstr;
	    cstr.buf = ch;
	    cstr.len = Strlen(ch);
	    switch (GetXkey(&cstr, &val)) {
	    case XK_CMD:
		cmd = val.cmd;
		break;
	    case XK_STR:
		PushMacro(val.str.buf);
		break;
	    case XK_EXE:
		RunCommand(val.str.buf);
		break;
	    default:
		abort();
		break;
	    }
	}
	if (!AltKeyMap) 
	    CurrentKeyMap = CcKeyMap;
    }
    *cmdnum = cmd;
    return OKCMD;
}

int
GetNextChar(cp)
    register Char *cp;
{
    register int num_read;
    int     tried = 0;
    unsigned char tcp;

    for (;;) {
	if (MacroLvl < 0) {
	    if (!Load_input_line())
		break;
	}
	if (*KeyMacro[MacroLvl] == 0) {
	    MacroLvl--;
	    continue;
	}
	*cp = *KeyMacro[MacroLvl]++ & CHAR;
	if (*KeyMacro[MacroLvl] == 0) {	/* Needed for QuoteMode On */
	    MacroLvl--;
	}
	return (1);
    }

    if (Rawmode() < 0)		/* make sure the tty is set up correctly */
	return 0;		/* oops: SHIN was closed */

#ifdef WINNT_NATIVE
    __nt_want_vcode = 1;
#endif /* WINNT_NATIVE */
    while ((num_read = read(SHIN, (char *) &tcp, 1)) == -1) {
	if (errno == EINTR)
	    continue;
	if (!tried && fixio(SHIN, errno) != -1)
	    tried = 1;
	else {
#ifdef convex
            /* need to print error message in case the file is migrated */
            if (errno != EINTR)
                stderror(ERR_SYSTEM, progname, strerror(errno));
#endif  /* convex */
#ifdef WINNT_NATIVE
	    __nt_want_vcode = 0;
#endif /* WINNT_NATIVE */
	    *cp = '\0';
	    return -1;
	}
    }
#ifdef WINNT_NATIVE
    if (__nt_want_vcode == 2)
	*cp = __nt_vcode;
    else
	*cp = tcp;
    __nt_want_vcode = 0;
#else
    *cp = tcp;
#endif /* WINNT_NATIVE */
    return num_read;
}

/*
 * SpellLine - do spelling correction on the entire command line
 * (which may have trailing newline).
 * If cmdonly is set, only check spelling of command words.
 * Return value:
 * -1: Something was incorrectible, and nothing was corrected
 *  0: Everything was correct
 *  1: Something was corrected
 */
static int
SpellLine(cmdonly)
    int     cmdonly;
{
    int     endflag, matchval;
    Char   *argptr, *OldCursor, *OldLastChar;

    OldLastChar = LastChar;
    OldCursor = Cursor;
    argptr = InputBuf;
    endflag = 1;
    matchval = 0;
    do {
	while (ismetahash(*argptr) || iscmdmeta(*argptr))
	    argptr++;
	for (Cursor = argptr;
	     *Cursor != '\0' && ((Cursor != argptr && Cursor[-1] == '\\') ||
				 (!ismetahash(*Cursor) && !iscmdmeta(*Cursor)));
	     Cursor++)
	     continue;
	if (*Cursor == '\0') {
	    Cursor = LastChar;
	    if (LastChar[-1] == '\n')
		Cursor--;
	    endflag = 0;
	}
	/* Obey current history character settings */
	mismatch[0] = HIST;
	mismatch[1] = HISTSUB;
	if (!Strchr(mismatch, *argptr) &&
	    (!cmdonly || starting_a_command(argptr, InputBuf))) {
#ifdef WINNT_NATIVE
	    /*
	     * This hack avoids correcting drive letter changes
	     */
	    if((Cursor - InputBuf) != 2 || (char)InputBuf[1] != ':')
#endif /* WINNT_NATIVE */
	    {
#ifdef HASH_SPELL_CHECK
		Char save;
		size_t len = Cursor - InputBuf;

		save = InputBuf[len];
		InputBuf[len] = '\0';
		if (find_cmd(InputBuf, 0) != 0) {
		    InputBuf[len] = save;
		    argptr = Cursor;
		    continue;
		}
		InputBuf[len] = save;
#endif /* HASH_SPELL_CHECK */
		switch (tenematch(InputBuf, Cursor - InputBuf, SPELL)) {
		case 1:		/* corrected */
		    matchval = 1;
		    break;
		case -1:		/* couldn't be corrected */
		    if (!matchval)
			matchval = -1;
		    break;
		default:		/* was correct */
		    break;
		}
	    }
	    if (LastChar != OldLastChar) {
		if (argptr < OldCursor)
		    OldCursor += (LastChar - OldLastChar);
		OldLastChar = LastChar;
	    }
	}
	argptr = Cursor;
    } while (endflag);
    Cursor = OldCursor;
    return matchval;
}

/*
 * CompleteLine - do command completion on the entire command line
 * (which may have trailing newline).
 * Return value:
 *  0: No command matched or failure
 *  1: One command matched
 *  2: Several commands matched
 */
static int
CompleteLine()
{
    int     endflag, tmatch;
    Char   *argptr, *OldCursor, *OldLastChar;

    OldLastChar = LastChar;
    OldCursor = Cursor;
    argptr = InputBuf;
    endflag = 1;
    do {
	while (ismetahash(*argptr) || iscmdmeta(*argptr))
	    argptr++;
	for (Cursor = argptr;
	     *Cursor != '\0' && ((Cursor != argptr && Cursor[-1] == '\\') ||
				 (!ismetahash(*Cursor) && !iscmdmeta(*Cursor)));
	     Cursor++)
	     continue;
	if (*Cursor == '\0') {
	    Cursor = LastChar;
	    if (LastChar[-1] == '\n')
		Cursor--;
	    endflag = 0;
	}
	if (!Strchr(mismatch, *argptr) && starting_a_command(argptr, InputBuf)) {
	    tmatch = tenematch(InputBuf, Cursor - InputBuf, RECOGNIZE);
	    if (tmatch <= 0) {
                return 0;
            } else if (tmatch > 1) {
                return 2;
	    }
	    if (LastChar != OldLastChar) {
		if (argptr < OldCursor)
		    OldCursor += (LastChar - OldLastChar);
		OldLastChar = LastChar;
	    }
	}
	argptr = Cursor;
    } while (endflag);
    Cursor = OldCursor;
    return 1;
}

