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

#ifndef lint
static char sccsid[] = "@(#)lex.c	5.16 (Berkeley) 6/8/91";
#endif /* not lint */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if __STDC__
# include <stdarg.h>
#else
# include <varargs.h>
#endif

#include "csh.h"
#include "extern.h"

/*
 * These lexical routines read input and form lists of words.
 * There is some involved processing here, because of the complications
 * of input buffering, and especially because of history substitution.
 */

static Char	*word __P((void));
static int	 getC1 __P((int));
static void	 getdol __P((void));
static void	 getexcl __P((int));
static struct Hist 
		*findev __P((Char *, bool));
static void	 setexclp __P((Char *));
static int	 bgetc __P((void));
static void	 bfree __P((void));
static struct wordent 
		*gethent __P((int));
static int	 matchs __P((Char *, Char *));
static int	 getsel __P((int *, int *, int));
static struct wordent 
		*getsub __P((struct wordent *));
static Char 	*subword __P((Char *, int, bool *));
static struct wordent 
		*dosub __P((int, struct wordent *, bool));

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
static Char *alvecp = NULL;

/*
 * Labuf implements a general buffer for lookahead during lexical operations.
 * Text which is to be placed in the input stream can be stuck here.
 * We stick parsed ahead $ constructs during initial input,
 * process id's from `$$', and modified variable values (from qualifiers
 * during expansion in sh.dol.c) here.
 */
static Char labuf[BUFSIZ];

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

static Char getCtmp;

#define getC(f)		((getCtmp = peekc) ? (peekc = 0, getCtmp) : getC1(f))
#define	ungetC(c)	peekc = c
#define	ungetD(c)	peekd = c

int
lex(hp)
    register struct wordent *hp;
{
    register struct wordent *wdp;
    int     c;

    lineloc = fseekp;
    hp->next = hp->prev = hp;
    hp->word = STRNULL;
    alvecp = 0, hadhist = 0;
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
	register struct wordent *new;

	new = (struct wordent *) xmalloc((size_t) sizeof(*wdp));
	new->word = 0;
	new->prev = wdp;
	new->next = hp;
	wdp->next = new;
	wdp = new;
	wdp->word = word();
    } while (wdp->word[0] != '\n');
    hp->prev = wdp;
    return (hadhist);
}

void
prlex(sp0)
    struct wordent *sp0;
{
    register struct wordent *sp = sp0->next;

    for (;;) {
	xprintf("%s", short2str(sp->word));
	sp = sp->next;
	if (sp == sp0)
	    break;
	if (sp->word[0] != '\n')
	    xputchar(' ');
    }
}

void
copylex(hp, fp)
    register struct wordent *hp;
    register struct wordent *fp;
{
    register struct wordent *wdp;

    wdp = hp;
    fp = fp->next;
    do {
	register struct wordent *new;

	new = (struct wordent *) xmalloc((size_t) sizeof(*wdp));
	new->prev = wdp;
	new->next = hp;
	wdp->next = new;
	wdp = new;
	wdp->word = Strsave(fp->word);
	fp = fp->next;
    } while (wdp->word[0] != '\n');
    hp->prev = wdp;
}

void
freelex(vp)
    register struct wordent *vp;
{
    register struct wordent *fp;

    while (vp->next != vp) {
	fp = vp->next;
	vp->next = fp->next;
	xfree((ptr_t) fp->word);
	xfree((ptr_t) fp);
    }
    vp->prev = vp;
}

static Char *
word()
{
    register Char c, c1;
    register Char *wp;
    Char    wbuf[BUFSIZ];
    register bool dolflg;
    register int i;

    wp = wbuf;
    i = BUFSIZ - 4;
loop:
    while ((c = getC(DOALL)) == ' ' || c == '\t');
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
	    do {
		c1 = c;
		c = getC(0);
	    } while (c != '\n');
	    if (c1 == '\\')
		goto loop;
	    /* fall into ... */

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
	}
    c1 = 0;
    dolflg = DOALL;
    for (;;) {
	if (c1) {
	    if (c == c1) {
		c1 = 0;
		dolflg = DOALL;
	    }
	    else if (c == '\\') {
		c = getC(0);
		if (c == HIST)
		    c |= QUOTE;
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
	    else if (c == '\n') {
		seterror(ERR_UNMATCHED, c1);
		ungetC(c);
		break;
	    }
	}
	else if (cmap(c, _META | _Q | _Q1 | _ESC)) {
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
	    else if (cmap(c, _Q | _Q1)) {	/* '"` */
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
    register int flag;
{
    register Char c;

    while (1) {
	if (c = peekc) {
	    peekc = 0;
	    return (c);
	}
	if (lap) {
	    if ((c = *lap++) == 0)
		lap = 0;
	    else {
		if (cmap(c, _META | _Q | _Q1))
		    c |= QUOTE;
		return (c);
	    }
	}
	if (c = peekd) {
	    peekd = 0;
	    return (c);
	}
	if (exclp) {
	    if (c = *exclp++)
		return (c);
	    if (exclnxt && --exclc >= 0) {
		exclnxt = exclnxt->next;
		setexclp(exclnxt->word);
		return (' ');
	    }
	    exclp = 0;
	    exclnxt = 0;
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
    register Char *np, *ep;
    Char    name[4 * MAXVARLEN + 1];
    register int c;
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
	*np++ = c, c = getC(DOEXCL);
    if (c == '#' || c == '?')
	special++, *np++ = c, c = getC(DOEXCL);
    *np++ = c;
    switch (c) {

    case '<':
    case '$':
	if (special)
	    seterror(ERR_SPDOLLT);
	*np = 0;
	addla(name);
	return;

    case '\n':
	ungetD(c);
	np--;
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
	    while (c = getC(DOEXCL)) {
		if (!Isdigit(c))
		    break;
		if (np < ep)
		    *np++ = c;
		else
		    toolong = 1;
	    }
	}
	else if (letter(c)) {
	    /* we know that np < &name[4] */
	    ep = &np[MAXVARLEN];
	    toolong = 0;
	    while (c = getC(DOEXCL)) {
		/* Bugfix for ${v123x} from Chris Torek, DAS DEC-90. */
		if (!letter(c) && !Isdigit(c))
		    break;
		if (np < ep)
		    *np++ = c;
		else
		    toolong = 1;
	    }
	}
	else {
	    *np = 0;
	    seterror(ERR_VARILL);
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
	*np++ = c;
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
		*np++ = c;
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

	int     gmodflag = 0;

	*np++ = c, c = getC(DOEXCL);
	if (c == 'g')
	    gmodflag++, *np++ = c, c = getC(DOEXCL);
	*np++ = c;
	if (!any("htrqxe", c)) {
	    if (gmodflag && c == '\n')
		stderror(ERR_VARSYN);	/* strike */
	    seterror(ERR_VARMOD, c);
	    *np = 0;
	    addla(name);
	    return;
	}
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
	*np++ = c;
    }
    *np = 0;
    addla(name);
    return;
}

void
addla(cp)
    Char   *cp;
{
    Char    buf[BUFSIZ];

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
    register struct wordent *hp, *ip;
    int     left, right, dol;
    register int c;

    if (sc == 0) {
	sc = getC(0);
	if (sc != '{') {
	    ungetC(sc);
	    sc = 0;
	}
    }
    quesarg = -1;
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
    register Char *cp;
    int     delim;
    register int c;
    int     sc;
    bool global = 0;
    Char    orhsb[sizeof(rhsb) / sizeof(Char)];

    exclnxt = 0;
    sc = c = getC(0);
    if (c == 'g')
	global ++, sc = c = getC(0);

    switch (c) {
    case 'p':
	justpr++;
	return (en);

    case 'x':
    case 'q':
	global ++;

	/* fall into ... */

    case 'h':
    case 'r':
    case 't':
    case 'e':
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
	    *cp++ = c;
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
		if (&cp[Strlen(orhsb)] > &rhsb[sizeof(rhsb) / sizeof(Char) - 2])
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
	    *cp++ = c;
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
    return (en);
}

static struct wordent *
dosub(sc, en, global)
    int     sc;
    struct wordent *en;
    bool global;
{
    struct wordent lexi;
    bool    didsub = 0;
    struct wordent *hp = &lexi;
    register struct wordent *wdp;
    register int i = exclc;

    wdp = hp;
    while (--i >= 0) {
	register struct wordent *new;

	new = (struct wordent *) xcalloc(1, sizeof *wdp);
	new->word = 0;
	new->prev = wdp;
	new->next = hp;
	wdp->next = new;
	wdp = new;
	en = en->next;
	wdp->word = (en->word && (global ||didsub == 0)) ?
	    subword(en->word, sc, &didsub) : Strsave(en->word);
    }
    if (didsub == 0)
	seterror(ERR_MODFAIL);
    hp->prev = wdp;
    return (&enthist(-1000, &lexi, 0)->Hlex);
}

static Char *
subword(cp, type, adid)
    Char   *cp;
    int     type;
    bool   *adid;
{
    Char    wbuf[BUFSIZ];
    register Char *wp, *mp, *np;
    register int i;

    switch (type) {

    case 'r':
    case 'e':
    case 'h':
    case 't':
    case 'q':
    case 'x':
	wp = domod(cp, type);
	if (wp == 0)
	    return (Strsave(cp));
	*adid = 1;
	return (wp);

    default:
	wp = wbuf;
	i = BUFSIZ - 4;
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
    register Char *wp, *xp;
    register int c;

    switch (type) {

    case 'x':
    case 'q':
	wp = Strsave(cp);
	for (xp = wp; c = *xp; xp++)
	    if ((c != ' ' && c != '\t') || type == 'q')
		*xp |= QUOTE;
	return (wp);

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
    }
    return (0);
}

static int
matchs(str, pat)
    register Char *str, *pat;
{
    while (*str && *pat && *str == *pat)
	str++, pat++;
    return (*pat == 0);
}

static int
getsel(al, ar, dol)
    register int *al, *ar;
    int     dol;
{
    register int c = getC(0);
    register int i;
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
    register struct Hist *hp;
    register Char *np;
    register int c;
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
	    while (!any(": \t\\\n}", c)) {
		if (event != -1 && Isdigit(c))
		    event = event * 10 + c - '0';
		else
		    event = -1;
		if (np < &lhsb[sizeof(lhsb) / sizeof(Char) - 2])
		    *np++ = c;
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
		    *np++ = c;
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
    register struct Hist *hp;

    for (hp = Histlist.Hnext; hp; hp = hp->Hnext) {
	Char   *dp;
	register Char *p, *q;
	register struct wordent *lp = hp->Hlex.next;
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
    register Char *cp;
{
    if (cp && cp[0] == '\n')
	return;
    exclp = cp;
}

void
unreadc(c)
    int    c;
{
    peekread = c;
}

int
readc(wanteof)
    bool    wanteof;
{
    register int c;
    static  sincereal;

    if (c = peekread) {
	peekread = 0;
	return (c);
    }
top:
    if (alvecp) {
	if (c = *alvecp++)
	    return (c);
	if (*alvec) {
	    alvecp = *alvec++;
	    return (' ');
	}
    }
    if (alvec) {
	if (alvecp = *alvec) {
	    alvec++;
	    goto top;
	}
	/* Infinite source! */
	return ('\n');
    }
    if (evalp) {
	if (c = *evalp++)
	    return (c);
	if (*evalvec) {
	    evalp = *evalvec++;
	    return (' ');
	}
	evalp = 0;
    }
    if (evalvec) {
	if (evalvec == (Char **) 1) {
	    doneinp = 1;
	    reset();
	}
	if (evalp = *evalvec) {
	    evalvec++;
	    goto top;
	}
	evalvec = (Char **) 1;
	return ('\n');
    }
    do {
	if (arginp == (Char *) 1 || onelflg == 1) {
	    if (wanteof)
		return (-1);
	    exitstat();
	}
	if (arginp) {
	    if ((c = *arginp++) == 0) {
		arginp = (Char *) 1;
		return ('\n');
	    }
	    return (c);
	}
reread:
	c = bgetc();
	if (c < 0) {
	    struct termios tty;
	    if (wanteof)
		return (-1);
	    /* was isatty but raw with ignoreeof yields problems */
	    if (tcgetattr(SHIN, &tty) == 0 && (tty.c_lflag & ICANON))
	    {
		/* was 'short' for FILEC */
		int     ctpgrp;

		if (++sincereal > 25)
		    goto oops;
		if (tpgrp != -1 &&
		    (ctpgrp = tcgetpgrp(FSHTTY)) != -1 &&
		    tpgrp != ctpgrp) {
		    (void) tcsetpgrp(FSHTTY, tpgrp);
		    (void) killpg((pid_t) ctpgrp, SIGHUP);
		    xprintf("Reset tty pgrp from %d to %d\n", ctpgrp, tpgrp);
		    goto reread;
		}
		if (adrof(STRignoreeof)) {
		    if (loginsh)
			xprintf("\nUse \"logout\" to logout.\n");
		    else
			xprintf("\nUse \"exit\" to leave csh.\n");
		    reset();
		}
		if (chkstop == 0)
		    panystop(1);
	    }
    oops:
	    doneinp = 1;
	    reset();
	}
	sincereal = 0;
	if (c == '\n' && onelflg)
	    onelflg--;
    } while (c == 0);
    return (c);
}

static int
bgetc()
{
    register int buf, off, c;

#ifdef FILEC
    register int numleft = 0, roomleft;
    Char    ttyline[BUFSIZ];
#endif
    char    tbuf[BUFSIZ + 1];

    if (cantell) {
	if (fseekp < fbobp || fseekp > feobp) {
	    fbobp = feobp = fseekp;
	    (void) lseek(SHIN, fseekp, L_SET);
	}
	if (fseekp == feobp) {
	    int     i;

	    fbobp = feobp;
	    do
		c = read(SHIN, tbuf, BUFSIZ);
	    while (c < 0 && errno == EINTR);
	    if (c <= 0)
		return (-1);
	    for (i = 0; i < c; i++)
		fbuf[0][i] = (unsigned char) tbuf[i];
	    feobp += c;
	}
	c = fbuf[0][fseekp - fbobp];
	fseekp++;
	return (c);
    }

again:
    buf = (int) fseekp / BUFSIZ;
    if (buf >= fblocks) {
	register Char **nfbuf =
	(Char **) xcalloc((size_t) (fblocks + 2),
			  sizeof(Char **));

	if (fbuf) {
	    (void) blkcpy(nfbuf, fbuf);
	    xfree((ptr_t) fbuf);
	}
	fbuf = nfbuf;
	fbuf[fblocks] = (Char *) xcalloc(BUFSIZ, sizeof(Char));
	fblocks++;
	if (!intty)
	    goto again;
    }
    if (fseekp >= feobp) {
	buf = (int) feobp / BUFSIZ;
	off = (int) feobp % BUFSIZ;
	roomleft = BUFSIZ - off;

#ifdef FILEC
	roomleft = BUFSIZ - off;
	for (;;) {
	    if (filec && intty) {
		c = numleft ? numleft : tenex(ttyline, BUFSIZ);
		if (c > roomleft) {
		    /* start with fresh buffer */
		    feobp = fseekp = fblocks * BUFSIZ;
		    numleft = c;
		    goto again;
		}
		if (c > 0)
		    bcopy(ttyline, fbuf[buf] + off, c * sizeof(Char));
		numleft = 0;
	    }
	    else {
#endif
		c = read(SHIN, tbuf, roomleft);
		if (c > 0) {
		    int     i;
		    Char   *ptr = fbuf[buf] + off;

		    for (i = 0; i < c; i++)
			ptr[i] = (unsigned char) tbuf[i];
		}
#ifdef FILEC
	    }
#endif
	    if (c >= 0)
		break;
	    if (errno == EWOULDBLOCK) {
		int     off = 0;

		(void) ioctl(SHIN, FIONBIO, (ioctl_t) & off);
	    }
	    else if (errno != EINTR)
		break;
	}
	if (c <= 0)
	    return (-1);
	feobp += c;
#ifndef FILEC
	goto again;
#else
	if (filec && !intty)
	    goto again;
#endif
    }
    c = fbuf[buf][(int) fseekp % BUFSIZ];
    fseekp++;
    return (c);
}

static void
bfree()
{
    register int sb, i;

    if (cantell)
	return;
    if (whyles)
	return;
    sb = (int) (fseekp - 1) / BUFSIZ;
    if (sb > 0) {
	for (i = 0; i < sb; i++)
	    xfree((ptr_t) fbuf[i]);
	(void) blkcpy(fbuf, &fbuf[sb]);
	fseekp -= BUFSIZ * sb;
	feobp -= BUFSIZ * sb;
	fblocks -= sb;
    }
}

void
bseek(l)
    off_t   l;

{

    fseekp = l;
    if (!cantell) {
#ifdef notdef
	register struct whyle *wp;
#endif

	if (!whyles)
	    return;
#ifdef notdef
	/*
	 * Christos: I don't understand this? both wp and l are local. What is
	 * this used for? I suspect the author meant fseek = wp->w_start
	 * This seek/tell stuff needs to be re-written...
	 */
	for (wp = whyles; wp->w_next; wp = wp->w_next)
	    continue;
	if (wp->w_start > l)
	    l = wp->w_start;
#endif
    }
}

void
btoeof()
{
    (void) lseek(SHIN, (off_t) 0, L_XTND);
    fseekp = feobp;
    wfree();
    bfree();
}

void
settell()
{
    cantell = 0;
    if (arginp || onelflg || intty)
	return;
    if (lseek(SHIN, (off_t) 0, L_INCR) < 0 || errno == ESPIPE)
	return;
    fbuf = (Char **) xcalloc(2, sizeof(Char **));
    fblocks = 1;
    fbuf[0] = (Char *) xcalloc(BUFSIZ, sizeof(Char));
    fseekp = fbobp = feobp = lseek(SHIN, (off_t) 0, L_INCR);
    cantell = 1;
}
