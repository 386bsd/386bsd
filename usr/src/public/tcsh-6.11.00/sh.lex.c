/* $Header: /src/pub/tcsh/sh.lex.c,v 3.52 2000/11/11 23:03:37 christos Exp $ */
/*
 * sh.lex.c: Lexical analysis into tokens
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

RCSID("$Id: sh.lex.c,v 3.52 2000/11/11 23:03:37 christos Exp $")

#include "ed.h"
/* #define DEBUG_INP */
/* #define DEBUG_SEEK */

/*
 * C shell
 */

/*
 * These lexical routines read input and form lists of words.
 * There is some involved processing here, because of the complications
 * of input buffering, and especially because of history substitution.
 */
static	Char		*word		__P((void));
static	int	 	 getC1		__P((int));
static	void	 	 getdol		__P((void));
static	void	 	 getexcl	__P((int));
static	struct Hist 	*findev		__P((Char *, bool));
static	void	 	 setexclp	__P((Char *));
static	int	 	 bgetc		__P((void));
static	void		 balloc		__P((int));
static	void	 	 bfree		__P((void));
static	struct wordent	*gethent	__P((int));
static	int	 	 matchs		__P((Char *, Char *));
static	int	 	 getsel		__P((int *, int *, int));
static	struct wordent	*getsub		__P((struct wordent *));
static	Char 		*subword	__P((Char *, int, bool *));
static	struct wordent	*dosub		__P((int, struct wordent *, bool));

/*
 * Peekc is a peek character for getC, peekread for readc.
 * There is a subtlety here in many places... history routines
 * will read ahead and then insert stuff into the input stream.
 * If they push back a character then they must push it behind
 * the text substituted by the history substitution.  On the other
 * hand in several places we need 2 peek characters.  To make this
 * all work, the history routines read with getC, and make use both
 * of ungetC and unreadc.  The key observation is that the state
 * of getC at the call of a history reference is such that calls
 * to getC from the history routines will always yield calls of
 * readc, unless this peeking is involved.  That is to say that during
 * getexcl the variables lap, exclp, and exclnxt are all zero.
 *
 * Getdol invokes history substitution, hence the extra peek, peekd,
 * which it can ungetD to be before history substitutions.
 */
static Char peekc = 0, peekd = 0;
static Char peekread = 0;

/* (Tail of) current word from ! subst */
static Char *exclp = NULL;

/* The rest of the ! subst words */
static struct wordent *exclnxt = NULL;

/* Count of remaining words in ! subst */
static int exclc = 0;

/* "Globp" for alias resubstitution */
int aret = TCSH_F_SEEK;

/*
 * Labuf implements a general buffer for lookahead during lexical operations.
 * Text which is to be placed in the input stream can be stuck here.
 * We stick parsed ahead $ constructs during initial input,
 * process id's from `$$', and modified variable values (from qualifiers
 * during expansion in sh.dol.c) here.
 */
static Char labuf[BUFSIZE];

/*
 * Lex returns to its caller not only a wordlist (as a "var" parameter)
 * but also whether a history substitution occurred.  This is used in
 * the main (process) routine to determine whether to echo, and also
 * when called by the alias routine to determine whether to keep the
 * argument list.
 */
static bool hadhist = 0;

/*
 * Avoid alias expansion recursion via \!#
 */
int     hleft;

Char    histline[BUFSIZE + 2];	/* last line input */

 /* The +2 is to fool hp's optimizer */
bool    histvalid = 0;		/* is histline valid */
static Char *histlinep = NULL;	/* current pointer into histline */

static Char getCtmp;

#define getC(f)		(((getCtmp = peekc) != '\0') ? (peekc = 0, getCtmp) : getC1(f))
#define	ungetC(c)	peekc = (Char) c
#define	ungetD(c)	peekd = (Char) c

/* Use Htime to store timestamps picked up from history file for enthist()
 * if reading saved history (sg)
 */
time_t Htime = (time_t)0;
static time_t a2time_t __P((Char *));

/*
 * for history event processing
 * in the command 'echo !?foo?:1 !$' we want the !$ to expand from the line
 * 'foo' was found instead of the last command
 */
static int uselastevent = 1;

int
lex(hp)
    struct wordent *hp;
{
    struct wordent *wdp;
    int     c;


    uselastevent = 1;
    histvalid = 0;
    histlinep = histline;
    *histlinep = '\0';

    btell(&lineloc);
    hp->next = hp->prev = hp;
    hp->word = STRNULL;
    hadhist = 0;
    do
	c = readc(0);
    while (c == ' ' || c == '\t');
    if (c == HISTSUB && intty)
	/* ^lef^rit	from tty is short !:s^lef^rit */
	getexcl(c);
    else
	unreadc(c);
    wdp = hp;
    /*
     * The following loop is written so that the links needed by freelex will
     * be ready and rarin to go even if it is interrupted.
     */
    do {
	struct wordent *new;

	new = (struct wordent *) xmalloc((size_t) sizeof(*wdp));
	new->word = STRNULL;
	new->prev = wdp;
	new->next = hp;
	wdp->next = new;
	hp->prev = new;
	wdp = new;
	wdp->word = word();
    } while (wdp->word[0] != '\n');
    if (histlinep < histline + BUFSIZE) {
	*histlinep = '\0';
	if (histlinep > histline && histlinep[-1] == '\n')
	    histlinep[-1] = '\0';
	histvalid = 1;
    }
    else {
	histline[BUFSIZE - 1] = '\0';
    }

    return (hadhist);
}

static time_t
a2time_t(word)
    Char *word;
{
    /* Attempt to distinguish timestamps from other possible entries.
     * Format: "+NNNNNNNNNN" (10 digits, left padded with ascii '0') */

    time_t ret;
    Char *s;
    int ct;

    if (!word || *(s = word) != '+')
	return (time_t)0;

    for (++s, ret = 0, ct = 0; *s; ++s, ++ct)
    {
	if (!isdigit((unsigned char)*s))
	    return (time_t)0;
	ret = ret * 10 + (time_t)((unsigned char)*s - '0');
    }

    if (ct != 10)
	return (time_t)0;

    return ret;
}

void
prlex(sp0)
    struct wordent *sp0;
{
    struct wordent *sp = sp0->next;

    for (;;) {
	xprintf("%S", sp->word);
	sp = sp->next;
	if (sp == sp0)
	    break;
	if (sp->word[0] != '\n')
	    xputchar(' ');
    }
}

void
copylex(hp, fp)
    struct wordent *hp;
    struct wordent *fp;
{
    struct wordent *wdp;

    wdp = hp;
    fp = fp->next;
    do {
	struct wordent *new;
	
	new = (struct wordent *) xmalloc((size_t) sizeof(*wdp));
	new->word = STRNULL;
	new->prev = wdp;
	new->next = hp;
	wdp->next = new;
	hp->prev = new;
	wdp = new;
	wdp->word = Strsave(fp->word);
	fp = fp->next;
    } while (wdp->word[0] != '\n');
}

void
freelex(vp)
    struct wordent *vp;
{
    struct wordent *fp;

    while (vp->next != vp) {
	fp = vp->next;
	vp->next = fp->next;
	if (fp->word != STRNULL)
	    xfree((ptr_t) fp->word);
	xfree((ptr_t) fp);
    }
    vp->prev = vp;
}

static Char *
word()
{
    Char c, c1;
    Char *wp;
    Char    wbuf[BUFSIZE];
    Char    hbuf[12];
    int	    h;
    bool dolflg;
    int i;
#if defined(DSPMBYTE)
    int mbytepos = 1;
#endif /* DSPMBYTE */

    wp = wbuf;
    i = BUFSIZE - 4;
loop:
    while ((c = getC(DOALL)) == ' ' || c == '\t')
	continue;
    if (cmap(c, _META | _ESC))
	switch (c) {
	case '&':
	case '|':
	case '<':
	case '>':
	    *wp++ = c;
	    c1 = getC(DOALL);
	    if (c1 == c)
		*wp++ = c1;
	    else
		ungetC(c1);
	    goto ret;

	case '#':
	    if (intty)
		break;
	    c = 0;
	    h = 0;
	    do {
		c1 = c;
		c = getC(0);
		if (h < 12)
		    hbuf[h++] = c;
	    } while (c != '\n');
	    hbuf[11] = '\0';
	    Htime = a2time_t(hbuf); 
	    if (c1 == '\\')
		goto loop;
	    /*FALLTHROUGH*/

	case ';':
	case '(':
	case ')':
	case '\n':
	    *wp++ = c;
	    goto ret;

	case '\\':
	    c = getC(0);
	    if (c == '\n') {
		if (onelflg == 1)
		    onelflg = 2;
		goto loop;
	    }
	    if (c != HIST)
		*wp++ = '\\', --i;
	    c |= QUOTE;
	default:
	    break;
	}
    c1 = 0;
    dolflg = DOALL;
    for (;;) {
#if defined(DSPMBYTE)
        if (mbytepos == 2)
	    mbytepos = 1;
	else if (mbytepos == 1 && Ismbyte1(c) && 2 <= i)
	    mbytepos = 2;
	else
#endif /* DSPMBYTE */
	if (c1) {
	    if (c == c1) {
		c1 = 0;
		dolflg = DOALL;
	    }
	    else if (c == '\\') {
		c = getC(0);
/*
 * PWP: this is dumb, but how all of the other shells work.  If \ quotes
 * a character OUTSIDE of a set of ''s, why shouldn't it quote EVERY
 * following character INSIDE a set of ''s.
 *
 * Actually, all I really want to be able to say is 'foo\'bar' --> foo'bar
 */
		if (c == HIST)
		    c |= QUOTE;
		else {
		    if (bslash_quote &&
			((c == '\'') || (c == '"') ||
			 (c == '\\'))) {
			c |= QUOTE;
		    }
		    else {
			if (c == '\n')
			    /*
			     * if (c1 == '`') c = ' '; else
			     */
			    c |= QUOTE;
			ungetC(c);
			c = '\\';
		    }
		}
	    }
	    else if (c == '\n') {
		seterror(ERR_UNMATCHED, c1);
		ungetC(c);
		break;
	    }
	}
	else if (cmap(c, _META | _QF | _QB | _ESC)) {
	    if (c == '\\') {
		c = getC(0);
		if (c == '\n') {
		    if (onelflg == 1)
			onelflg = 2;
		    break;
		}
		if (c != HIST)
		    *wp++ = '\\', --i;
		c |= QUOTE;
	    }
	    else if (cmap(c, _QF | _QB)) {	/* '"` */
		c1 = c;
		dolflg = c == '"' ? DOALL : DOEXCL;
	    }
	    else if (c != '#' || !intty) {
		ungetC(c);
		break;
	    }
	}
	if (--i > 0) {
	    *wp++ = c;
	    c = getC(dolflg);
	}
	else {
	    seterror(ERR_WTOOLONG);
	    wp = &wbuf[1];
	    break;
	}
    }
ret:
    *wp = 0;
    return (Strsave(wbuf));
}

static int
getC1(flag)
    int flag;
{
    Char c;

    for (;;) {
	if ((c = peekc) != 0) {
	    peekc = 0;
	    return (c);
	}
	if (lap) {
	    if ((c = *lap++) == 0)
		lap = 0;
	    else {
		if (cmap(c, _META | _QF | _QB))
		    c |= QUOTE;
		return (c);
	    }
	}
	if ((c = peekd) != 0) {
	    peekd = 0;
	    return (c);
	}
	if (exclp) {
	    if ((c = *exclp++) != 0)
		return (c);
	    if (exclnxt && --exclc >= 0) {
		exclnxt = exclnxt->next;
		setexclp(exclnxt->word);
		return (' ');
	    }
	    exclp = 0;
	    exclnxt = 0;
	    /* this will throw away the dummy history entries */
	    savehist(NULL, 0);

	}
	if (exclnxt) {
	    exclnxt = exclnxt->next;
	    if (--exclc < 0)
		exclnxt = 0;
	    else
		setexclp(exclnxt->word);
	    continue;
	}
	c = readc(0);
	if (c == '$' && (flag & DODOL)) {
	    getdol();
	    continue;
	}
	if (c == HIST && (flag & DOEXCL)) {
	    getexcl(0);
	    continue;
	}
	break;
    }
    return (c);
}

static void
getdol()
{
    Char *np, *ep;
    Char    name[4 * MAXVARLEN + 1];
    int c;
    int     sc;
    bool    special = 0, toolong;

    np = name, *np++ = '$';
    c = sc = getC(DOEXCL);
    if (any("\t \n", c)) {
	ungetD(c);
	ungetC('$' | QUOTE);
	return;
    }
    if (c == '{')
	*np++ = (Char) c, c = getC(DOEXCL);
    if (c == '#' || c == '?' || c == '%')
	special++, *np++ = (Char) c, c = getC(DOEXCL);
    *np++ = (Char) c;
    switch (c) {

    case '<':
    case '$':
    case '!':
	if (special)
	    seterror(ERR_SPDOLLT);
	*np = 0;
	addla(name);
	return;

    case '\n':
	ungetD(c);
	np--;
	if (!special)
	    seterror(ERR_NEWLINE);
	*np = 0;
	addla(name);
	return;

    case '*':
	if (special)
	    seterror(ERR_SPSTAR);
	*np = 0;
	addla(name);
	return;

    default:
	toolong = 0;
	if (Isdigit(c)) {
#ifdef notdef
	    /* let $?0 pass for now */
	    if (special) {
		seterror(ERR_DIGIT);
		*np = 0;
		addla(name);
		return;
	    }
#endif
	    /* we know that np < &name[4] */
	    ep = &np[MAXVARLEN];
	    while ((c = getC(DOEXCL)) != 0) {
		if (!Isdigit(c))
		    break;
		if (np < ep)
		    *np++ = (Char) c;
		else
		    toolong = 1;
	    }
	}
	else if (letter(c)) {
	    /* we know that np < &name[4] */
	    ep = &np[MAXVARLEN];
	    toolong = 0;
	    while ((c = getC(DOEXCL)) != 0) {
		/* Bugfix for ${v123x} from Chris Torek, DAS DEC-90. */
		if (!letter(c) && !Isdigit(c))
		    break;
		if (np < ep)
		    *np++ = (Char) c;
		else
		    toolong = 1;
	    }
	}
	else {
	    if (!special)
		seterror(ERR_VARILL);
	    else {
		ungetD(c);
		--np;
	    }
	    *np = 0;
	    addla(name);
	    return;
	}
	if (toolong) {
	    seterror(ERR_VARTOOLONG);
	    *np = 0;
	    addla(name);
	    return;
	}
	break;
    }
    if (c == '[') {
	*np++ = (Char) c;
	/*
	 * Name up to here is a max of MAXVARLEN + 8.
	 */
	ep = &np[2 * MAXVARLEN + 8];
	do {
	    /*
	     * Michael Greim: Allow $ expansion to take place in selector
	     * expressions. (limits the number of characters returned)
	     */
	    c = getC(DOEXCL | DODOL);
	    if (c == '\n') {
		ungetD(c);
		np--;
		seterror(ERR_NLINDEX);
		*np = 0;
		addla(name);
		return;
	    }
	    if (np < ep)
		*np++ = (Char) c;
	} while (c != ']');
	*np = '\0';
	if (np >= ep) {
	    seterror(ERR_SELOVFL);
	    addla(name);
	    return;
	}
	c = getC(DOEXCL);
    }
    /*
     * Name up to here is a max of 2 * MAXVARLEN + 8.
     */
    if (c == ':') {
	/*
	 * if the :g modifier is followed by a newline, then error right away!
	 * -strike
	 */

	int     gmodflag = 0, amodflag = 0;

#ifndef COMPAT
	do {
#endif /* COMPAT */
	    *np++ = (Char) c, c = getC(DOEXCL);
	    if (c == 'g' || c == 'a') {
		if (c == 'g')
		    gmodflag++;
		else
		    amodflag++;
		*np++ = (Char) c; c = getC(DOEXCL);
	    }
	    if ((c == 'g' && !gmodflag) || (c == 'a' && !amodflag)) {
		if (c == 'g')
		    gmodflag++;
		else
		    amodflag++;
		*np++ = (Char) c; c = getC(DOEXCL);
	    }
	    *np++ = (Char) c;
	    /* scan s// [eichin:19910926.0512EST] */
	    if (c == 's') {
		int delimcnt = 2;
		int delim = getC(0);
		*np++ = (Char) delim;
		
		if (!delim || letter(delim)
		    || Isdigit(delim) || any(" \t\n", delim)) {
		    seterror(ERR_BADSUBST);
		    break;
		}	
		while ((c = getC(0)) != (-1)) {
		    *np++ = (Char) c;
		    if(c == delim) delimcnt--;
		    if(!delimcnt) break;
		}
		if(delimcnt) {
		    seterror(ERR_BADSUBST);
		    break;
		}
		c = 's';
	    }
	    if (!any("htrqxesul", c)) {
		if ((amodflag || gmodflag) && c == '\n')
		    stderror(ERR_VARSYN);	/* strike */
		seterror(ERR_BADMOD, c);
		*np = 0;
		addla(name);
		return;
	    }
#ifndef COMPAT
	}
	while ((c = getC(DOEXCL)) == ':');
	ungetD(c);
#endif /* COMPAT */
    }
    else
	ungetD(c);
    if (sc == '{') {
	c = getC(DOEXCL);
	if (c != '}') {
	    ungetD(c);
	    seterror(ERR_MISSING, '}');
	    *np = 0;
	    addla(name);
	    return;
	}
	*np++ = (Char) c;
    }
    *np = 0;
    addla(name);
    return;
}

void
addla(cp)
    Char   *cp;
{
    Char    buf[BUFSIZE];

    if (Strlen(cp) + (lap ? Strlen(lap) : 0) >=
	(sizeof(labuf) - 4) / sizeof(Char)) {
	seterror(ERR_EXPOVFL);
	return;
    }
    if (lap)
	(void) Strcpy(buf, lap);
    (void) Strcpy(labuf, cp);
    if (lap)
	(void) Strcat(labuf, buf);
    lap = labuf;
}

static Char lhsb[32];
static Char slhs[32];
static Char rhsb[64];
static int quesarg;

static void
getexcl(sc)
    int    sc;
{
    struct wordent *hp, *ip;
    int     left, right, dol;
    int c;

    if (sc == 0) {
	sc = getC(0);
	if (sc != '{') {
	    ungetC(sc);
	    sc = 0;
	}
    }
    quesarg = -1;

    if (uselastevent) {
	uselastevent = 0;
	lastev = eventno;
    }
    else
	lastev = eventno;
    hp = gethent(sc);
    if (hp == 0)
	return;
    hadhist = 1;
    dol = 0;
    if (hp == alhistp)
	for (ip = hp->next->next; ip != alhistt; ip = ip->next)
	    dol++;
    else
	for (ip = hp->next->next; ip != hp->prev; ip = ip->next)
	    dol++;
    left = 0, right = dol;
    if (sc == HISTSUB) {
	ungetC('s'), unreadc(HISTSUB), c = ':';
	goto subst;
    }
    c = getC(0);
    if (!any(":^$*-%", c))
	goto subst;
    left = right = -1;
    if (c == ':') {
	c = getC(0);
	unreadc(c);
	if (letter(c) || c == '&') {
	    c = ':';
	    left = 0, right = dol;
	    goto subst;
	}
    }
    else
	ungetC(c);
    if (!getsel(&left, &right, dol))
	return;
    c = getC(0);
    if (c == '*')
	ungetC(c), c = '-';
    if (c == '-') {
	if (!getsel(&left, &right, dol))
	    return;
	c = getC(0);
    }
subst:
    exclc = right - left + 1;
    while (--left >= 0)
	hp = hp->next;
    if (sc == HISTSUB || c == ':') {
	do {
	    hp = getsub(hp);
	    c = getC(0);
	} while (c == ':');
    }
    unreadc(c);
    if (sc == '{') {
	c = getC(0);
	if (c != '}')
	    seterror(ERR_BADBANG);
    }
    exclnxt = hp;
}

static struct wordent *
getsub(en)
    struct wordent *en;
{
    Char *cp;
    int     delim;
    int c;
    int     sc;
    bool global;
    Char    orhsb[sizeof(rhsb) / sizeof(Char)];

#ifndef COMPAT
    do {
#endif /* COMPAT */
	exclnxt = 0;
	global = 0;
	sc = c = getC(0);
	if (c == 'g' || c == 'a') {
	    global |= (c == 'g') ? 1 : 2;
	    sc = c = getC(0);
	}
	if (((c =='g') && !(global & 1)) || ((c == 'a') && !(global & 2))) {
	    global |= (c == 'g') ? 1 : 2;
	    sc = c = getC(0);
	}

	switch (c) {
	case 'p':
	    justpr++;
	    return (en);

	case 'x':
	case 'q':
	    global |= 1;
	    /*FALLTHROUGH*/

	case 'h':
	case 'r':
	case 't':
	case 'e':
	case 'u':
	case 'l':
	    break;

	case '&':
	    if (slhs[0] == 0) {
		seterror(ERR_NOSUBST);
		return (en);
	    }
	    (void) Strcpy(lhsb, slhs);
	    break;

#ifdef notdef
	case '~':
	    if (lhsb[0] == 0)
		goto badlhs;
	    break;
#endif

	case 's':
	    delim = getC(0);
	    if (letter(delim) || Isdigit(delim) || any(" \t\n", delim)) {
		unreadc(delim);
		lhsb[0] = 0;
		seterror(ERR_BADSUBST);
		return (en);
	    }
	    cp = lhsb;
	    for (;;) {
		c = getC(0);
		if (c == '\n') {
		    unreadc(c);
		    break;
		}
		if (c == delim)
		    break;
		if (cp > &lhsb[sizeof(lhsb) / sizeof(Char) - 2]) {
		    lhsb[0] = 0;
		    seterror(ERR_BADSUBST);
		    return (en);
		}
		if (c == '\\') {
		    c = getC(0);
		    if (c != delim && c != '\\')
			*cp++ = '\\';
		}
		*cp++ = (Char) c;
	    }
	    if (cp != lhsb)
		*cp++ = 0;
	    else if (lhsb[0] == 0) {
		seterror(ERR_LHS);
		return (en);
	    }
	    cp = rhsb;
	    (void) Strcpy(orhsb, cp);
	    for (;;) {
		c = getC(0);
		if (c == '\n') {
		    unreadc(c);
		    break;
		}
		if (c == delim)
		    break;
#ifdef notdef
		if (c == '~') {
		    if (&cp[Strlen(orhsb)] > &rhsb[sizeof(rhsb) /
						   sizeof(Char) - 2])
			goto toorhs;
		    (void) Strcpy(cp, orhsb);
		    cp = Strend(cp);
		    continue;
		}
#endif
		if (cp > &rhsb[sizeof(rhsb) / sizeof(Char) - 2]) {
		    seterror(ERR_RHSLONG);
		    return (en);
		}
		if (c == '\\') {
		    c = getC(0);
		    if (c != delim /* && c != '~' */ )
			*cp++ = '\\';
		}
		*cp++ = (Char) c;
	    }
	    *cp++ = 0;
	    break;

	default:
	    if (c == '\n')
		unreadc(c);
	    seterror(ERR_BADBANGMOD, c);
	    return (en);
	}
	(void) Strcpy(slhs, lhsb);
	if (exclc)
	    en = dosub(sc, en, global);
#ifndef COMPAT
    }
    while ((c = getC(0)) == ':');
    unreadc(c);
#endif /* COMPAT */
    return (en);
}

/*
 * 
 * From Beto Appleton (beto@aixwiz.austin.ibm.com)
 *
 * when using history substitution, and the variable
 * 'history' is set to a value higher than 1000,
 * the shell might either freeze (hang) or core-dump.
 * We raise the limit to 50000000
 */

#define HIST_PURGE -50000000
static struct wordent *
dosub(sc, en, global)
    int     sc;
    struct wordent *en;
    bool global;
{
    struct wordent lexi;
    bool    didsub = 0, didone = 0;
    struct wordent *hp = &lexi;
    struct wordent *wdp;
    int i = exclc;
    struct Hist *hst;

    wdp = hp;
    while (--i >= 0) {
	struct wordent *new = 
		(struct wordent *) xcalloc(1, sizeof *wdp);

	new->word = 0;
	new->prev = wdp;
	new->next = hp;
	wdp->next = new;
	wdp = new;
	en = en->next;
	if (en->word) {
	    Char *tword, *otword;

	    if ((global & 1) || didsub == 0) {
		tword = subword(en->word, sc, &didone);
		if (didone)
		    didsub = 1;
		if (global & 2) {
		    while (didone && tword != STRNULL) {
			otword = tword;
			tword = subword(otword, sc, &didone);
			if (Strcmp(tword, otword) == 0) {
			    xfree((ptr_t) otword);
			    break;
			}
			else
			    xfree((ptr_t) otword);
		    }
		}
	    }
	    else
		tword = Strsave(en->word);
	    wdp->word = tword;
	}
    }
    if (didsub == 0)
	seterror(ERR_MODFAIL);
    hp->prev = wdp;
    /* 
     * ANSI mode HP/UX compiler chokes on
     * return &enthist(HIST_PURGE, &lexi, 0)->Hlex;
     */
    hst = enthist(HIST_PURGE, &lexi, 0, 0);
    return &(hst->Hlex);
}

static Char *
subword(cp, type, adid)
    Char   *cp;
    int     type;
    bool   *adid;
{
    Char    wbuf[BUFSIZE];
    Char *wp, *mp, *np;
    int i;

    *adid = 0;
    switch (type) {

    case 'r':
    case 'e':
    case 'h':
    case 't':
    case 'q':
    case 'x':
    case 'u':
    case 'l':
	wp = domod(cp, type);
	if (wp == 0)
	    return (Strsave(cp));
	*adid = 1;
	return (wp);

    default:
	wp = wbuf;
	i = BUFSIZE - 4;
	for (mp = cp; *mp; mp++)
	    if (matchs(mp, lhsb)) {
		for (np = cp; np < mp;)
		    *wp++ = *np++, --i;
		for (np = rhsb; *np; np++)
		    switch (*np) {

		    case '\\':
			if (np[1] == '&')
			    np++;
			/* fall into ... */

		    default:
			if (--i < 0) {
			    seterror(ERR_SUBOVFL);
			    return (STRNULL);
			}
			*wp++ = *np;
			continue;

		    case '&':
			i -= Strlen(lhsb);
			if (i < 0) {
			    seterror(ERR_SUBOVFL);
			    return (STRNULL);
			}
			*wp = 0;
			(void) Strcat(wp, lhsb);
			wp = Strend(wp);
			continue;
		    }
		mp += Strlen(lhsb);
		i -= Strlen(mp);
		if (i < 0) {
		    seterror(ERR_SUBOVFL);
		    return (STRNULL);
		}
		*wp = 0;
		(void) Strcat(wp, mp);
		*adid = 1;
		return (Strsave(wbuf));
	    }
	return (Strsave(cp));
    }
}

Char   *
domod(cp, type)
    Char   *cp;
    int     type;
{
    Char *wp, *xp;
    int c;

    switch (type) {

    case 'x':
    case 'q':
	wp = Strsave(cp);
	for (xp = wp; (c = *xp) != 0; xp++)
	    if ((c != ' ' && c != '\t') || type == 'q')
		*xp |= QUOTE;
	return (wp);

    case 'l':
	wp = Strsave(cp);
	for (cp = wp; *cp; cp++) 
	    if (Isupper(*cp)) {
		*cp = Tolower(*cp);
		return wp;
	    }
	return wp;

    case 'u':
	wp = Strsave(cp);
	for (cp = wp; *cp; cp++) 
	    if (Islower(*cp)) {
		*cp = Toupper(*cp);
		return wp;
	    }
	return wp;

    case 'h':
    case 't':
	if (!any(short2str(cp), '/'))
	    return (type == 't' ? Strsave(cp) : 0);
	wp = Strend(cp);
	while (*--wp != '/')
	    continue;
	if (type == 'h')
	    xp = Strsave(cp), xp[wp - cp] = 0;
	else
	    xp = Strsave(wp + 1);
	return (xp);

    case 'e':
    case 'r':
	wp = Strend(cp);
	for (wp--; wp >= cp && *wp != '/'; wp--)
	    if (*wp == '.') {
		if (type == 'e')
		    xp = Strsave(wp + 1);
		else
		    xp = Strsave(cp), xp[wp - cp] = 0;
		return (xp);
	    }
	return (Strsave(type == 'e' ? STRNULL : cp));
    default:
	break;
    }
    return (0);
}

static int
matchs(str, pat)
    Char *str, *pat;
{
    while (*str && *pat && *str == *pat)
	str++, pat++;
    return (*pat == 0);
}

static int
getsel(al, ar, dol)
    int *al, *ar;
    int     dol;
{
    int c = getC(0);
    int i;
    bool    first = *al < 0;

    switch (c) {

    case '%':
	if (quesarg == -1) {
	    seterror(ERR_BADBANGARG);
	    return (0);
	}
	if (*al < 0)
	    *al = quesarg;
	*ar = quesarg;
	break;

    case '-':
	if (*al < 0) {
	    *al = 0;
	    *ar = dol - 1;
	    unreadc(c);
	}
	return (1);

    case '^':
	if (*al < 0)
	    *al = 1;
	*ar = 1;
	break;

    case '$':
	if (*al < 0)
	    *al = dol;
	*ar = dol;
	break;

    case '*':
	if (*al < 0)
	    *al = 1;
	*ar = dol;
	if (*ar < *al) {
	    *ar = 0;
	    *al = 1;
	    return (1);
	}
	break;

    default:
	if (Isdigit(c)) {
	    i = 0;
	    while (Isdigit(c)) {
		i = i * 10 + c - '0';
		c = getC(0);
	    }
	    if (i < 0)
		i = dol + 1;
	    if (*al < 0)
		*al = i;
	    *ar = i;
	}
	else if (*al < 0)
	    *al = 0, *ar = dol;
	else
	    *ar = dol - 1;
	unreadc(c);
	break;
    }
    if (first) {
	c = getC(0);
	unreadc(c);
	if (any("-$*", c))
	    return (1);
    }
    if (*al > *ar || *ar > dol) {
	seterror(ERR_BADBANGARG);
	return (0);
    }
    return (1);

}

static struct wordent *
gethent(sc)
    int     sc;
{
    struct Hist *hp;
    Char *np;
    int c;
    int     event;
    bool    back = 0;

    c = sc == HISTSUB ? HIST : getC(0);
    if (c == HIST) {
	if (alhistp)
	    return (alhistp);
	event = eventno;
    }
    else
	switch (c) {

	case ':':
	case '^':
	case '$':
	case '*':
	case '%':
	    ungetC(c);
	    if (lastev == eventno && alhistp)
		return (alhistp);
	    event = lastev;
	    break;

	case '#':		/* !# is command being typed in (mrh) */
	    if (--hleft == 0) {
		seterror(ERR_HISTLOOP);
		return (0);
	    }
	    else
		return (&paraml);
	    /* NOTREACHED */

	case '-':
	    back = 1;
	    c = getC(0);
	    /* FALLSTHROUGH */

	default:
	    if (any("(=~", c)) {
		unreadc(c);
		ungetC(HIST);
		return (0);
	    }
	    np = lhsb;
	    event = 0;
	    while (!cmap(c, _ESC | _META | _QF | _QB) && !any("^*-%${}:#", c)) {
		if (event != -1 && Isdigit(c))
		    event = event * 10 + c - '0';
		else
		    event = -1;
		if (np < &lhsb[sizeof(lhsb) / sizeof(Char) - 2])
		    *np++ = (Char) c;
		c = getC(0);
	    }
	    unreadc(c);
	    if (np == lhsb) {
		ungetC(HIST);
		return (0);
	    }
	    *np++ = 0;
	    if (event != -1) {
		/*
		 * History had only digits
		 */
		if (back)
		    event = eventno + (alhistp == 0) - (event ? event : 0);
		break;
	    }
	    if (back) {
		event = sizeof(lhsb) / sizeof(lhsb[0]);
		np = &lhsb[--event];
		*np-- = '\0';
		for (event--; np > lhsb; *np-- = lhsb[--event])
		    continue;
		*np = '-';
	    }
	    hp = findev(lhsb, 0);
	    if (hp)
		lastev = hp->Hnum;
	    return (&hp->Hlex);

	case '?':
	    np = lhsb;
	    for (;;) {
		c = getC(0);
		if (c == '\n') {
		    unreadc(c);
		    break;
		}
		if (c == '?')
		    break;
		if (np < &lhsb[sizeof(lhsb) / sizeof(Char) - 2])
		    *np++ = (Char) c;
	    }
	    if (np == lhsb) {
		if (lhsb[0] == 0) {
		    seterror(ERR_NOSEARCH);
		    return (0);
		}
	    }
	    else
		*np++ = 0;
	    hp = findev(lhsb, 1);
	    if (hp)
		lastev = hp->Hnum;
	    return (&hp->Hlex);
	}

    for (hp = Histlist.Hnext; hp; hp = hp->Hnext)
	if (hp->Hnum == event) {
	    hp->Href = eventno;
	    lastev = hp->Hnum;
	    return (&hp->Hlex);
	}
    np = putn(event);
    seterror(ERR_NOEVENT, short2str(np));
    return (0);
}

static struct Hist *
findev(cp, anyarg)
    Char   *cp;
    bool    anyarg;
{
    struct Hist *hp;

    for (hp = Histlist.Hnext; hp; hp = hp->Hnext) {
	Char   *dp;
	Char *p, *q;
	struct wordent *lp = hp->Hlex.next;
	int     argno = 0;

	/*
	 * The entries added by alias substitution don't have a newline but do
	 * have a negative event number. Savehist() trims off these entries,
	 * but it happens before alias expansion, too early to delete those
	 * from the previous command.
	 */
	if (hp->Hnum < 0)
	    continue;
	if (lp->word[0] == '\n')
	    continue;
	if (!anyarg) {
	    p = cp;
	    q = lp->word;
	    do
		if (!*p)
		    return (hp);
	    while (*p++ == *q++);
	    continue;
	}
	do {
	    for (dp = lp->word; *dp; dp++) {
		p = cp;
		q = dp;
		do
		    if (!*p) {
			quesarg = argno;
			return (hp);
		    }
		while (*p++ == *q++);
	    }
	    lp = lp->next;
	    argno++;
	} while (lp->word[0] != '\n');
    }
    seterror(ERR_NOEVENT, short2str(cp));
    return (0);
}


static void
setexclp(cp)
    Char *cp;
{
    if (cp && cp[0] == '\n')
	return;
    exclp = cp;
}

void
unreadc(c)
    int    c;
{
    peekread = (Char) c;
}

int
readc(wanteof)
    bool    wanteof;
{
    int c;
    static  int sincereal;	/* Number of real EOFs we've seen */
    Char *ptr;			/* For STRignoreeof */
    int numeof = 0;		/* Value of STRignoreeof */

#ifdef DEBUG_INP
    xprintf("readc\n");
#endif
    if ((c = peekread) != 0) {
	peekread = 0;
	return (c);
    }

    /* Compute the value of EOFs */
    if ((ptr = varval(STRignoreeof)) != STRNULL) {
	while (*ptr) {
	    if (!Isdigit(*ptr)) {
		numeof = 0;
		break;
	    }
	    numeof = numeof * 10 + *ptr++ - '0';
	}
    } 
    if (numeof < 1) numeof = 26;	/* Sanity check */

top:
    aret = TCSH_F_SEEK;
    if (alvecp) {
	arun = 1;
#ifdef DEBUG_INP
	xprintf("alvecp %c\n", *alvecp & 0xff);
#endif
	aret = TCSH_A_SEEK;
	if ((c = *alvecp++) != 0)
	    return (c);
	if (alvec && *alvec) {
		alvecp = *alvec++;
		return (' ');
	}
	else {
	    alvecp = NULL;
	    aret = TCSH_F_SEEK;
	    return('\n');
	}
    }
    if (alvec) {
	arun = 1;
	if ((alvecp = *alvec) != 0) {
	    alvec++;
	    goto top;
	}
	/* Infinite source! */
	return ('\n');
    }
    arun = 0;
    if (evalp) {
	aret = TCSH_E_SEEK;
	if ((c = *evalp++) != 0)
	    return (c);
	if (evalvec && *evalvec) {
	    evalp = *evalvec++;
	    return (' ');
	}
	aret = TCSH_F_SEEK;
	evalp = 0;
    }
    if (evalvec) {
	if (evalvec == INVPPTR) {
	    doneinp = 1;
	    reset();
	}
	if ((evalp = *evalvec) != 0) {
	    evalvec++;
	    goto top;
	}
	evalvec = INVPPTR;
	return ('\n');
    }
    do {
	if (arginp == INVPTR || onelflg == 1) {
	    if (wanteof)
		return (-1);
	    exitstat();
	}
	if (arginp) {
	    if ((c = *arginp++) == 0) {
		arginp = INVPTR;
		return ('\n');
	    }
	    return (c);
	}
#ifdef BSDJOBS
reread:
#endif /* BSDJOBS */
	c = bgetc();
	if (c < 0) {
#ifndef WINNT_NATIVE
# ifndef POSIX
#  ifdef TERMIO
	    struct termio tty;
#  else /* SGTTYB */
	    struct sgttyb tty;
#  endif /* TERMIO */
# else /* POSIX */
	    struct termios tty;
# endif /* POSIX */
#endif /* !WINNT_NATIVE */
	    if (wanteof)
		return (-1);
	    /* was isatty but raw with ignoreeof yields problems */
#ifndef WINNT_NATIVE
# ifndef POSIX
#  ifdef TERMIO
	    if (ioctl(SHIN, TCGETA, (ioctl_t) & tty) == 0 &&
		(tty.c_lflag & ICANON))
#  else /* GSTTYB */
	    if (ioctl(SHIN, TIOCGETP, (ioctl_t) & tty) == 0 &&
		(tty.sg_flags & RAW) == 0)
#  endif /* TERMIO */
# else /* POSIX */
	    if (tcgetattr(SHIN, &tty) == 0 &&
		(tty.c_lflag & ICANON))
# endif /* POSIX */
#else /* WINNT_NATIVE */
	    if (isatty(SHIN))
#endif /* !WINNT_NATIVE */
	    {
		/* was 'short' for FILEC */
#ifdef BSDJOBS
		int     ctpgrp;
#endif /* BSDJOBS */

		if (++sincereal >= numeof)	/* Too many EOFs?  Bye! */
		    goto oops;
#ifdef BSDJOBS
		if (tpgrp != -1 &&
		    (ctpgrp = tcgetpgrp(FSHTTY)) != -1 &&
		    tpgrp != ctpgrp) {
		    (void) tcsetpgrp(FSHTTY, tpgrp);
# ifdef _SEQUENT_
		    if (ctpgrp)
# endif /* _SEQUENT */
		    (void) killpg((pid_t) ctpgrp, SIGHUP);
# ifdef notdef
		    /*
		     * With the walking process group fix, this message
		     * is now obsolete. As the foreground process group
		     * changes, the shell needs to adjust. Well too bad.
		     */
		    xprintf(CGETS(16, 1, "Reset tty pgrp from %d to %d\n"),
			    ctpgrp, tpgrp);
# endif /* notdef */
		    goto reread;
		}
#endif /* BSDJOBS */
		/* What follows is complicated EOF handling -- sterling@netcom.com */
		/* First, we check to see if we have ignoreeof set */
		if (adrof(STRignoreeof)) {
			/* If so, we check for any stopped jobs only on the first EOF */
			if ((sincereal == 1) && (chkstop == 0)) {
				panystop(1);
			}
		} else {
			/* If we don't have ignoreeof set, always check for stopped jobs */
			if (chkstop == 0) {
				panystop(1);
			}
		}
		/* At this point, if there were stopped jobs, we would have already
		 * called reset().  If we got this far, assume we can print an
		 * exit/logout message if we ignoreeof, or just exit.
		 */
		if (adrof(STRignoreeof)) {
			/* If so, tell the user to use exit or logout */
		    if (loginsh) {
				xprintf(CGETS(16, 2,
					"\nUse \"logout\" to logout.\n"));
		   	} else {
				xprintf(CGETS(16, 3,
					"\nUse \"exit\" to leave %s.\n"),
					progname);
			}
			reset();
		} else {
			/* If we don't have ignoreeof set, just fall through */
			;	/* EMPTY */
		}
	    }
    oops:
	    doneinp = 1;
	    reset();
	}
	sincereal = 0;
	if (c == '\n' && onelflg)
	    onelflg--;
    } while (c == 0);
    if (histlinep < histline + BUFSIZE)
	*histlinep++ = (Char) c;
    return (c);
}

static void
balloc(buf)
    int buf;
{
    Char **nfbuf;

    while (buf >= fblocks) {
	nfbuf = (Char **) xcalloc((size_t) (fblocks + 2),
			  sizeof(Char **));
	if (fbuf) {
	    (void) blkcpy(nfbuf, fbuf);
	    xfree((ptr_t) fbuf);
	}
	fbuf = nfbuf;
	fbuf[fblocks] = (Char *) xcalloc(BUFSIZE, sizeof(Char));
	fblocks++;
    }
}

static int
bgetc()
{
    int c, off, buf;
    int numleft = 0, roomleft;
    char    tbuf[BUFSIZE + 1];

    if (cantell) {
	if (fseekp < fbobp || fseekp > feobp) {
	    fbobp = feobp = fseekp;
	    (void) lseek(SHIN, fseekp, L_SET);
	}
	if (fseekp == feobp) {
	    int     i;

	    fbobp = feobp;
	    do
		c = read(SHIN, tbuf, BUFSIZE);
	    while (c < 0 && errno == EINTR);
#ifdef convex
	    if (c < 0)
		stderror(ERR_SYSTEM, progname, strerror(errno));
#endif /* convex */
	    if (c <= 0)
		return (-1);
	    for (i = 0; i < c; i++)
		fbuf[0][i] = (unsigned char) tbuf[i];
	    feobp += c;
	}
#ifndef WINNT_NATIVE
	c = fbuf[0][fseekp - fbobp];
	fseekp++;
#else
	do {
	    c = fbuf[0][fseekp - fbobp];
	    fseekp++;
	} while(c == '\r');
#endif /* !WINNT_NATIVE */
	return (c);
    }

    while (fseekp >= feobp) {
	if (editing && intty) {		/* then use twenex routine */
	    fseekp = feobp;		/* where else? */
	    c = numleft = Inputl();	/* PWP: get a line */
	    while (numleft > 0) {
		off = (int) feobp % BUFSIZE;
		buf = (int) feobp / BUFSIZE;
		balloc(buf);
		roomleft = BUFSIZE - off;
		if (roomleft > numleft)
		    roomleft = numleft;
		(void) memmove((ptr_t) (fbuf[buf] + off), (ptr_t) (InputBuf + c - numleft), (size_t) (roomleft * sizeof(Char)));
		numleft -= roomleft;
		feobp += roomleft;
	    }
	}
	else {
	    off = (int) feobp % BUFSIZE;
	    buf = (int) feobp / BUFSIZE;
	    balloc(buf);
	    roomleft = BUFSIZE - off;
	    c = read(SHIN, tbuf, (size_t) roomleft);
	    if (c > 0) {
		int     i;
		Char   *ptr = fbuf[buf] + off;

		for (i = 0; i < c; i++)
		    ptr[i] = (unsigned char) tbuf[i];
		feobp += c;
	    }
	}
	if (c == 0 || (c < 0 && fixio(SHIN, errno) == -1))
	    return (-1);
    }
#ifndef WINNT_NATIVE
    c = fbuf[(int) fseekp / BUFSIZE][(int) fseekp % BUFSIZE];
    fseekp++;
#else
    do {
	c = fbuf[(int) fseekp / BUFSIZE][(int) fseekp % BUFSIZE];
	fseekp++;
    } while(c == '\r');
#endif /* !WINNT_NATIVE */
    return (c);
}

static void
bfree()
{
    int sb, i;

    if (cantell)
	return;
    if (whyles)
	return;
    sb = (int) (fseekp - 1) / BUFSIZE;
    if (sb > 0) {
	for (i = 0; i < sb; i++)
	    xfree((ptr_t) fbuf[i]);
	(void) blkcpy(fbuf, &fbuf[sb]);
	fseekp -= BUFSIZE * sb;
	feobp -= BUFSIZE * sb;
	fblocks -= sb;
    }
}

void
bseek(l)
    struct Ain   *l;
{
    switch (aret = l->type) {
    case TCSH_E_SEEK:
	evalvec = l->a_seek;
	evalp = l->c_seek;
#ifdef DEBUG_SEEK
	xprintf(CGETS(16, 4, "seek to eval %x %x\n"), evalvec, evalp);
#endif
	return;
    case TCSH_A_SEEK:
	alvec = l->a_seek;
	alvecp = l->c_seek;
#ifdef DEBUG_SEEK
	xprintf(CGETS(16, 5, "seek to alias %x %x\n"), alvec, alvecp);
#endif
	return;
    case TCSH_F_SEEK:	
#ifdef DEBUG_SEEK
	xprintf(CGETS(16, 6, "seek to file %x\n"), fseekp);
#endif
	fseekp = l->f_seek;
	return;
    default:
	xprintf(CGETS(16, 7, "Bad seek type %d\n"), aret);
	abort();
    }
}

/* any similarity to bell telephone is purely accidental */
void
btell(l)
struct Ain *l;
{
    switch (l->type = aret) {
    case TCSH_E_SEEK:
	l->a_seek = evalvec;
	l->c_seek = evalp;
#ifdef DEBUG_SEEK
	xprintf(CGETS(16, 8, "tell eval %x %x\n"), evalvec, evalp);
#endif
	return;
    case TCSH_A_SEEK:
	l->a_seek = alvec;
	l->c_seek = alvecp;
#ifdef DEBUG_SEEK
	xprintf(CGETS(16, 9, "tell alias %x %x\n"), alvec, alvecp);
#endif
	return;
    case TCSH_F_SEEK:
	/*SUPPRESS 112*/
	l->f_seek = fseekp;
	l->a_seek = NULL;
#ifdef DEBUG_SEEK
	xprintf(CGETS(16, 10, "tell file %x\n"), fseekp);
#endif
	return;
    default:
	xprintf(CGETS(16, 7, "Bad seek type %d\n"), aret);
	abort();
    }
}

void
btoeof()
{
    (void) lseek(SHIN, (off_t) 0, L_XTND);
    aret = TCSH_F_SEEK;
    fseekp = feobp;
    alvec = NULL;
    alvecp = NULL;
    evalvec = NULL;
    evalp = NULL;
    wfree();
    bfree();
}

void
settell()
{
    off_t x;
    cantell = 0;
    if (arginp || onelflg || intty)
	return;
    if ((x = lseek(SHIN, (off_t) 0, L_INCR)) == -1)
	return;
    fbuf = (Char **) xcalloc(2, sizeof(Char **));
    fblocks = 1;
    fbuf[0] = (Char *) xcalloc(BUFSIZE, sizeof(Char));
    fseekp = fbobp = feobp = x;
    cantell = 1;
}
