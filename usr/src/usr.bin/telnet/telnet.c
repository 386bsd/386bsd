/*
 * Copyright (c) 1988, 1990 Regents of the University of California.
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
static char sccsid[] = "@(#)telnet.c	5.53 (Berkeley) 3/22/91";
#endif /* not lint */

#include <sys/types.h>

#if	defined(unix)
#include <signal.h>
/* By the way, we need to include curses.h before telnet.h since,
 * among other things, telnet.h #defines 'DO', which is a variable
 * declared in curses.h.
 */
#endif	/* defined(unix) */

#include <arpa/telnet.h>

#include <ctype.h>

#include "ring.h"

#include "defines.h"
#include "externs.h"
#include "types.h"
#include "general.h"


#define	strip(x)	((x)&0x7f)

static unsigned char	subbuffer[SUBBUFSIZE],
			*subpointer, *subend;	 /* buffer for sub-options */
#define	SB_CLEAR()	subpointer = subbuffer;
#define	SB_TERM()	{ subend = subpointer; SB_CLEAR(); }
#define	SB_ACCUM(c)	if (subpointer < (subbuffer+sizeof subbuffer)) { \
				*subpointer++ = (c); \
			}

#define	SB_GET()	((*subpointer++)&0xff)
#define	SB_PEEK()	((*subpointer)&0xff)
#define	SB_EOF()	(subpointer >= subend)
#define	SB_LEN()	(subend - subpointer)

char	options[256];		/* The combined options */
char	do_dont_resp[256];
char	will_wont_resp[256];

int
	eight = 0,
	autologin = 0,	/* Autologin anyone? */
	skiprc = 0,
	connected,
	showoptions,
	In3270,		/* Are we in 3270 mode? */
	ISend,		/* trying to send network data in */
	debug = 0,
	crmod,
	netdata,	/* Print out network data flow */
	crlf,		/* Should '\r' be mapped to <CR><LF> (or <CR><NUL>)? */
#if	defined(TN3270)
	noasynchtty = 0,/* User specified "-noasynch" on command line */
	noasynchnet = 0,/* User specified "-noasynch" on command line */
	askedSGA = 0,	/* We have talked about suppress go ahead */
#endif	/* defined(TN3270) */
	telnetport,
	SYNCHing,	/* we are in TELNET SYNCH mode */
	flushout,	/* flush output */
	autoflush = 0,	/* flush output when interrupting? */
	autosynch,	/* send interrupt characters with SYNCH? */
	localflow,	/* we handle flow control locally */
	localchars,	/* we recognize interrupt/quit */
	donelclchars,	/* the user has set "localchars" */
	donebinarytoggle,	/* the user has put us in binary */
	dontlecho,	/* do we suppress local echoing right now? */
	globalmode;

char *prompt = 0;

cc_t escape;
cc_t rlogin;
#ifdef	KLUDGELINEMODE
cc_t echoc;
#endif

/*
 * Telnet receiver states for fsm
 */
#define	TS_DATA		0
#define	TS_IAC		1
#define	TS_WILL		2
#define	TS_WONT		3
#define	TS_DO		4
#define	TS_DONT		5
#define	TS_CR		6
#define	TS_SB		7		/* sub-option collection */
#define	TS_SE		8		/* looking for sub-option end */

static int	telrcv_state;

jmp_buf	toplevel = { 0 };
jmp_buf	peerdied;

int	flushline;
int	linemode;

#ifdef	KLUDGELINEMODE
int	kludgelinemode = 1;
#endif

/*
 * The following are some clocks used to decide how to interpret
 * the relationship between various variables.
 */

Clocks clocks;

#ifdef	notdef
Modelist modelist[] = {
	{ "telnet command mode", COMMAND_LINE },
	{ "character-at-a-time mode", 0 },
	{ "character-at-a-time mode (local echo)", LOCAL_ECHO|LOCAL_CHARS },
	{ "line-by-line mode (remote echo)", LINE | LOCAL_CHARS },
	{ "line-by-line mode", LINE | LOCAL_ECHO | LOCAL_CHARS },
	{ "line-by-line mode (local echoing suppressed)", LINE | LOCAL_CHARS },
	{ "3270 mode", 0 },
};
#endif


/*
 * Initialize telnet environment.
 */

    void
init_telnet()
{
    env_init();

    SB_CLEAR();
    ClearArray(options);

    connected = In3270 = ISend = localflow = donebinarytoggle = 0;
#if	defined(ENCRYPT) || defined(AUTHENTICATE)
    auth_encrypt_connect(connected);
#endif

    SYNCHing = 0;

    /* Don't change NetTrace */

    escape = CONTROL(']');
    rlogin = _POSIX_VDISABLE;
#ifdef	KLUDGELINEMODE
    echoc = CONTROL('E');
#endif

    flushline = 1;
    telrcv_state = TS_DATA;
}


#ifdef	notdef
#include <varargs.h>

    /*VARARGS*/
    static void
printring(va_alist)
    va_dcl
{
    va_list ap;
    char buffer[100];		/* where things go */
    char *ptr;
    char *format;
    char *string;
    Ring *ring;
    int i;

    va_start(ap);

    ring = va_arg(ap, Ring *);
    format = va_arg(ap, char *);
    ptr = buffer;

    while ((i = *format++) != 0) {
	if (i == '%') {
	    i = *format++;
	    switch (i) {
	    case 'c':
		*ptr++ = va_arg(ap, int);
		break;
	    case 's':
		string = va_arg(ap, char *);
		ring_supply_data(ring, buffer, ptr-buffer);
		ring_supply_data(ring, string, strlen(string));
		ptr = buffer;
		break;
	    case 0:
		ExitString("printring: trailing %%.\n", 1);
		/*NOTREACHED*/
	    default:
		ExitString("printring: unknown format character.\n", 1);
		/*NOTREACHED*/
	    }
	} else {
	    *ptr++ = i;
	}
    }
    ring_supply_data(ring, buffer, ptr-buffer);
}
#endif

/*
 * These routines are in charge of sending option negotiations
 * to the other side.
 *
 * The basic idea is that we send the negotiation if either side
 * is in disagreement as to what the current state should be.
 */

    void
send_do(c, init)
    register int c, init;
{
    if (init) {
	if (((do_dont_resp[c] == 0) && my_state_is_do(c)) ||
				my_want_state_is_do(c))
	    return;
	set_my_want_state_do(c);
	do_dont_resp[c]++;
    }
    NET2ADD(IAC, DO);
    NETADD(c);
    printoption("SENT", DO, c);
}

    void
send_dont(c, init)
    register int c, init;
{
    if (init) {
	if (((do_dont_resp[c] == 0) && my_state_is_dont(c)) ||
				my_want_state_is_dont(c))
	    return;
	set_my_want_state_dont(c);
	do_dont_resp[c]++;
    }
    NET2ADD(IAC, DONT);
    NETADD(c);
    printoption("SENT", DONT, c);
}

    void
send_will(c, init)
    register int c, init;
{
    if (init) {
	if (((will_wont_resp[c] == 0) && my_state_is_will(c)) ||
				my_want_state_is_will(c))
	    return;
	set_my_want_state_will(c);
	will_wont_resp[c]++;
    }
    NET2ADD(IAC, WILL);
    NETADD(c);
    printoption("SENT", WILL, c);
}

    void
send_wont(c, init)
    register int c, init;
{
    if (init) {
	if (((will_wont_resp[c] == 0) && my_state_is_wont(c)) ||
				my_want_state_is_wont(c))
	    return;
	set_my_want_state_wont(c);
	will_wont_resp[c]++;
    }
    NET2ADD(IAC, WONT);
    NETADD(c);
    printoption("SENT", WONT, c);
}


	void
willoption(option)
	int option;
{
	int new_state_ok = 0;

	if (do_dont_resp[option]) {
	    --do_dont_resp[option];
	    if (do_dont_resp[option] && my_state_is_do(option))
		--do_dont_resp[option];
	}

	if ((do_dont_resp[option] == 0) && my_want_state_is_dont(option)) {

	    switch (option) {

	    case TELOPT_ECHO:
#	    if defined(TN3270)
		/*
		 * The following is a pain in the rear-end.
		 * Various IBM servers (some versions of Wiscnet,
		 * possibly Fibronics/Spartacus, and who knows who
		 * else) will NOT allow us to send "DO SGA" too early
		 * in the setup proceedings.  On the other hand,
		 * 4.2 servers (telnetd) won't set SGA correctly.
		 * So, we are stuck.  Empirically (but, based on
		 * a VERY small sample), the IBM servers don't send
		 * out anything about ECHO, so we postpone our sending
		 * "DO SGA" until we see "WILL ECHO" (which 4.2 servers
		 * DO send).
		  */
		{
		    if (askedSGA == 0) {
			askedSGA = 1;
			if (my_want_state_is_dont(TELOPT_SGA))
			    send_do(TELOPT_SGA, 1);
		    }
		}
		    /* Fall through */
	    case TELOPT_EOR:
#endif	    /* defined(TN3270) */
	    case TELOPT_BINARY:
	    case TELOPT_SGA:
		settimer(modenegotiated);
		/* FALL THROUGH */
	    case TELOPT_STATUS:
#if	defined(AUTHENTICATE)
	    case TELOPT_AUTHENTICATION:
#endif
#if	defined(ENCRYPT)
	    case TELOPT_ENCRYPT:
#endif
		new_state_ok = 1;
		break;

	    case TELOPT_TM:
		if (flushout)
		    flushout = 0;
		/*
		 * Special case for TM.  If we get back a WILL,
		 * pretend we got back a WONT.
		 */
		set_my_want_state_dont(option);
		set_my_state_dont(option);
		return;			/* Never reply to TM will's/wont's */

	    case TELOPT_LINEMODE:
	    default:
		break;
	    }

	    if (new_state_ok) {
		set_my_want_state_do(option);
		send_do(option, 0);
		setconnmode(0);		/* possibly set new tty mode */
	    } else {
		do_dont_resp[option]++;
		send_dont(option, 0);
	    }
	}
	set_my_state_do(option);
#if	defined(ENCRYPT)
	if (option == TELOPT_ENCRYPT)
		encrypt_send_support();
#endif
}

	void
wontoption(option)
	int option;
{
	if (do_dont_resp[option]) {
	    --do_dont_resp[option];
	    if (do_dont_resp[option] && my_state_is_dont(option))
		--do_dont_resp[option];
	}

	if ((do_dont_resp[option] == 0) && my_want_state_is_do(option)) {

	    switch (option) {

#ifdef	KLUDGELINEMODE
	    case TELOPT_SGA:
		if (!kludgelinemode)
		    break;
		/* FALL THROUGH */
#endif
	    case TELOPT_ECHO:
		settimer(modenegotiated);
		break;

	    case TELOPT_TM:
		if (flushout)
		    flushout = 0;
		set_my_want_state_dont(option);
		set_my_state_dont(option);
		return;		/* Never reply to TM will's/wont's */

	    default:
		break;
	    }
	    set_my_want_state_dont(option);
	    if (my_state_is_do(option))
		send_dont(option, 0);
	    setconnmode(0);			/* Set new tty mode */
	} else if (option == TELOPT_TM) {
	    /*
	     * Special case for TM.
	     */
	    if (flushout)
		flushout = 0;
	    set_my_want_state_dont(option);
	}
	set_my_state_dont(option);
}

	static void
dooption(option)
	int option;
{
	int new_state_ok = 0;

	if (will_wont_resp[option]) {
	    --will_wont_resp[option];
	    if (will_wont_resp[option] && my_state_is_will(option))
		--will_wont_resp[option];
	}

	if (will_wont_resp[option] == 0) {
	  if (my_want_state_is_wont(option)) {

	    switch (option) {

	    case TELOPT_TM:
		/*
		 * Special case for TM.  We send a WILL, but pretend
		 * we sent WONT.
		 */
		send_will(option, 0);
		set_my_want_state_wont(TELOPT_TM);
		set_my_state_wont(TELOPT_TM);
		return;

#	if defined(TN3270)
	    case TELOPT_EOR:		/* end of record */
#	endif	/* defined(TN3270) */
	    case TELOPT_BINARY:		/* binary mode */
	    case TELOPT_NAWS:		/* window size */
	    case TELOPT_TSPEED:		/* terminal speed */
	    case TELOPT_LFLOW:		/* local flow control */
	    case TELOPT_TTYPE:		/* terminal type option */
	    case TELOPT_SGA:		/* no big deal */
	    case TELOPT_ENVIRON:	/* environment variable option */
#if	defined(ENCRYPT)
	    case TELOPT_ENCRYPT:	/* encryption variable option */
#endif
		new_state_ok = 1;
		break;
#if	defined(AUTHENTICATE)
	    case TELOPT_AUTHENTICATION:
		if (autologin)
			new_state_ok = 1;
		break;
#endif

	    case TELOPT_XDISPLOC:	/* X Display location */
		if (env_getvalue((unsigned char *)"DISPLAY"))
		    new_state_ok = 1;
		break;

	    case TELOPT_LINEMODE:
#ifdef	KLUDGELINEMODE
		kludgelinemode = 0;
		send_do(TELOPT_SGA, 1);
#endif
		set_my_want_state_will(TELOPT_LINEMODE);
		send_will(option, 0);
		set_my_state_will(TELOPT_LINEMODE);
		slc_init();
		return;

	    case TELOPT_ECHO:		/* We're never going to echo... */
	    default:
		break;
	    }

	    if (new_state_ok) {
		set_my_want_state_will(option);
		send_will(option, 0);
		setconnmode(0);			/* Set new tty mode */
	    } else {
		will_wont_resp[option]++;
		send_wont(option, 0);
	    }
	  } else {
	    /*
	     * Handle options that need more things done after the
	     * other side has acknowledged the option.
	     */
	    switch (option) {
	    case TELOPT_LINEMODE:
#ifdef	KLUDGELINEMODE
		kludgelinemode = 0;
		send_do(TELOPT_SGA, 1);
#endif
		set_my_state_will(option);
		slc_init();
		send_do(TELOPT_SGA, 0);
		return;
	    }
	  }
	}
	set_my_state_will(option);
}

	static void
dontoption(option)
	int option;
{

	if (will_wont_resp[option]) {
	    --will_wont_resp[option];
	    if (will_wont_resp[option] && my_state_is_wont(option))
		--will_wont_resp[option];
	}

	if ((will_wont_resp[option] == 0) && my_want_state_is_will(option)) {
	    switch (option) {
	    case TELOPT_LINEMODE:
		linemode = 0;	/* put us back to the default state */
		break;
	    }
	    /* we always accept a DONT */
	    set_my_want_state_wont(option);
	    if (my_state_is_will(option))
		send_wont(option, 0);
	    setconnmode(0);			/* Set new tty mode */
	}
	set_my_state_wont(option);
}

/*
 * Given a buffer returned by tgetent(), this routine will turn
 * the pipe seperated list of names in the buffer into an array
 * of pointers to null terminated names.  We toss out any bad,
 * duplicate, or verbose names (names with spaces).
 */

static char *name_unknown = "UNKNOWN";
static char *unknown[] = { 0, 0 };

	char **
mklist(buf, name)
	char *buf, *name;
{
	register int n;
	register char c, *cp, **argvp, *cp2, **argv, **avt;

	if (name) {
		if (strlen(name) > 40) {
			name = 0;
			unknown[0] = name_unknown;
		} else {
			unknown[0] = name;
			upcase(name);
		}
	} else
		unknown[0] = name_unknown;
	/*
	 * Count up the number of names.
	 */
	for (n = 1, cp = buf; *cp && *cp != ':'; cp++) {
		if (*cp == '|')
			n++;
	}
	/*
	 * Allocate an array to put the name pointers into
	 */
	argv = (char **)malloc((n+3)*sizeof(char *));
	if (argv == 0)
		return(unknown);

	/*
	 * Fill up the array of pointers to names.
	 */
	*argv = 0;
	argvp = argv+1;
	n = 0;
	for (cp = cp2 = buf; (c = *cp);  cp++) {
		if (c == '|' || c == ':') {
			*cp++ = '\0';
			/*
			 * Skip entries that have spaces or are over 40
			 * characters long.  If this is our environment
			 * name, then put it up front.  Otherwise, as
			 * long as this is not a duplicate name (case
			 * insensitive) add it to the list.
			 */
			if (n || (cp - cp2 > 41))
				;
			else if (name && (strncasecmp(name, cp2, cp-cp2) == 0))
				*argv = cp2;
			else if (is_unique(cp2, argv+1, argvp))
				*argvp++ = cp2;
			if (c == ':')
				break;
			/*
			 * Skip multiple delimiters. Reset cp2 to
			 * the beginning of the next name. Reset n,
			 * the flag for names with spaces.
			 */
			while ((c = *cp) == '|')
				cp++;
			cp2 = cp;
			n = 0;
		}
		/*
		 * Skip entries with spaces or non-ascii values.
		 * Convert lower case letters to upper case.
		 */
		if ((c == ' ') || !isascii(c))
			n = 1;
		else if (islower(c))
			*cp = toupper(c);
	}
	
	/*
	 * Check for an old V6 2 character name.  If the second
	 * name points to the beginning of the buffer, and is
	 * only 2 characters long, move it to the end of the array.
	 */
	if ((argv[1] == buf) && (strlen(argv[1]) == 2)) {
		--argvp;
		for (avt = &argv[1]; avt < argvp; avt++)
			*avt = *(avt+1);
		*argvp++ = buf;
	}

	/*
	 * Duplicate last name, for TTYPE option, and null
	 * terminate the array.  If we didn't find a match on
	 * our terminal name, put that name at the beginning.
	 */
	cp = *(argvp-1);
	*argvp++ = cp;
	*argvp = 0;

	if (*argv == 0) {
		if (name)
			*argv = name;
		else {
			--argvp;
			for (avt = argv; avt < argvp; avt++)
				*avt = *(avt+1);
		}
	}
	if (*argv)
		return(argv);
	else
		return(unknown);
}

	int
is_unique(name, as, ae)
	register char *name, **as, **ae;
{
	register char **ap;
	register int n;

	n = strlen(name) + 1;
	for (ap = as; ap < ae; ap++)
		if (strncasecmp(*ap, name, n) == 0)
			return(0);
	return (1);
}

#ifdef	TERMCAP
char termbuf[1024];

	/*ARGSUSED*/
	int
setupterm(tname, fd, errp)
	char *tname;
	int fd, *errp;
{
	if (tgetent(termbuf, tname) == 1) {
		termbuf[1023] = '\0';
		if (errp)
			*errp = 1;
		return(0);
	}
	if (errp)
		*errp = 0;
	return(-1);
}
#else
#define	termbuf	ttytype
extern char ttytype[];
#endif

int resettermname = 1;

	char *
gettermname()
{
	char *tname;
	static char **tnamep = 0;
	static char **next;
	int err;

	if (resettermname) {
		resettermname = 0;
		if (tnamep && tnamep != unknown)
			free(tnamep);
		if ((tname = (char *)env_getvalue((unsigned char *)"TERM")) &&
				(setupterm(tname, 1, &err) == 0)) {
			tnamep = mklist(termbuf, tname);
		} else {
			if (tname && (strlen(tname) <= 40)) {
				unknown[0] = tname;
				upcase(tname);
			} else
				unknown[0] = name_unknown;
			tnamep = unknown;
		}
		next = tnamep;
	}
	if (*next == 0)
		next = tnamep;
	return(*next++);
}
/*
 * suboption()
 *
 *	Look at the sub-option buffer, and try to be helpful to the other
 * side.
 *
 *	Currently we recognize:
 *
 *		Terminal type, send request.
 *		Terminal speed (send request).
 *		Local flow control (is request).
 *		Linemode
 */

    static void
suboption()
{
    printsub('<', subbuffer, SB_LEN()+2);
    switch (SB_GET()) {
    case TELOPT_TTYPE:
	if (my_want_state_is_wont(TELOPT_TTYPE))
	    return;
	if (SB_EOF() || SB_GET() != TELQUAL_SEND) {
	    return;
	} else {
	    char *name;
	    unsigned char temp[50];
	    int len;

#if	defined(TN3270)
	    if (tn3270_ttype()) {
		return;
	    }
#endif	/* defined(TN3270) */
	    name = gettermname();
	    len = strlen(name) + 4 + 2;
	    if (len < NETROOM()) {
		sprintf((char *)temp, "%c%c%c%c%s%c%c", IAC, SB, TELOPT_TTYPE,
				TELQUAL_IS, name, IAC, SE);
		ring_supply_data(&netoring, temp, len);
		printsub('>', &temp[2], len-2);
	    } else {
		ExitString("No room in buffer for terminal type.\n", 1);
		/*NOTREACHED*/
	    }
	}
	break;
    case TELOPT_TSPEED:
	if (my_want_state_is_wont(TELOPT_TSPEED))
	    return;
	if (SB_EOF())
	    return;
	if (SB_GET() == TELQUAL_SEND) {
	    long ospeed, ispeed;
	    unsigned char temp[50];
	    int len;

	    TerminalSpeeds(&ispeed, &ospeed);

	    sprintf((char *)temp, "%c%c%c%c%d,%d%c%c", IAC, SB, TELOPT_TSPEED,
		    TELQUAL_IS, ospeed, ispeed, IAC, SE);
	    len = strlen((char *)temp+4) + 4;	/* temp[3] is 0 ... */

	    if (len < NETROOM()) {
		ring_supply_data(&netoring, temp, len);
		printsub('>', temp+2, len - 2);
	    }
/*@*/	    else printf("lm_will: not enough room in buffer\n");
	}
	break;
    case TELOPT_LFLOW:
	if (my_want_state_is_wont(TELOPT_LFLOW))
	    return;
	if (SB_EOF())
	    return;
	switch(SB_GET()) {
	case 1:
	    localflow = 1;
	    break;
	case 0:
	    localflow = 0;
	    break;
	default:
	    return;
	}
	setcommandmode();
	setconnmode(0);
	break;

    case TELOPT_LINEMODE:
	if (my_want_state_is_wont(TELOPT_LINEMODE))
	    return;
	if (SB_EOF())
	    return;
	switch (SB_GET()) {
	case WILL:
	    lm_will(subpointer, SB_LEN());
	    break;
	case WONT:
	    lm_wont(subpointer, SB_LEN());
	    break;
	case DO:
	    lm_do(subpointer, SB_LEN());
	    break;
	case DONT:
	    lm_dont(subpointer, SB_LEN());
	    break;
	case LM_SLC:
	    slc(subpointer, SB_LEN());
	    break;
	case LM_MODE:
	    lm_mode(subpointer, SB_LEN(), 0);
	    break;
	default:
	    break;
	}
	break;

    case TELOPT_ENVIRON:
	if (SB_EOF())
	    return;
	switch(SB_PEEK()) {
	case TELQUAL_IS:
	case TELQUAL_INFO:
	    if (my_want_state_is_dont(TELOPT_ENVIRON))
		return;
	    break;
	case TELQUAL_SEND:
	    if (my_want_state_is_wont(TELOPT_ENVIRON)) {
		return;
	    }
	    break;
	default:
	    return;
	}
	env_opt(subpointer, SB_LEN());
	break;

    case TELOPT_XDISPLOC:
	if (my_want_state_is_wont(TELOPT_XDISPLOC))
	    return;
	if (SB_EOF())
	    return;
	if (SB_GET() == TELQUAL_SEND) {
	    unsigned char temp[50], *dp;
	    int len;

	    if ((dp = env_getvalue((unsigned char *)"DISPLAY")) == NULL) {
		/*
		 * Something happened, we no longer have a DISPLAY
		 * variable.  So, turn off the option.
		 */
		send_wont(TELOPT_XDISPLOC, 1);
		break;
	    }
	    sprintf((char *)temp, "%c%c%c%c%s%c%c", IAC, SB, TELOPT_XDISPLOC,
		    TELQUAL_IS, dp, IAC, SE);
	    len = strlen((char *)temp+4) + 4;	/* temp[3] is 0 ... */

	    if (len < NETROOM()) {
		ring_supply_data(&netoring, temp, len);
		printsub('>', temp+2, len - 2);
	    }
/*@*/	    else printf("lm_will: not enough room in buffer\n");
	}
	break;

#if	defined(AUTHENTICATE)
	case TELOPT_AUTHENTICATION: {
		if (!autologin)
			break;
		if (SB_EOF())
			return;
		switch(SB_GET()) {
		case TELQUAL_IS:
			if (my_want_state_is_dont(TELOPT_AUTHENTICATION))
				return;
			auth_is(subpointer, SB_LEN());
			break;
		case TELQUAL_SEND:
			if (my_want_state_is_wont(TELOPT_AUTHENTICATION))
				return;
			auth_send(subpointer, SB_LEN());
			break;
		case TELQUAL_REPLY:
			if (my_want_state_is_wont(TELOPT_AUTHENTICATION))
				return;
			auth_reply(subpointer, SB_LEN());
			break;
		case TELQUAL_NAME:
			if (my_want_state_is_dont(TELOPT_AUTHENTICATION))
				return;
			auth_name(subpointer, SB_LEN());
			break;
		}
	}
	break;
#endif
#if	defined(ENCRYPT)
	case TELOPT_ENCRYPT:
		if (SB_EOF())
			return;
		switch(SB_GET()) {
		case ENCRYPT_START:
			if (my_want_state_is_dont(TELOPT_ENCRYPT))
				return;
			encrypt_start(subpointer, SB_LEN());
			break;
		case ENCRYPT_END:
			if (my_want_state_is_dont(TELOPT_ENCRYPT))
				return;
			encrypt_end();
			break;
		case ENCRYPT_SUPPORT:
			if (my_want_state_is_wont(TELOPT_ENCRYPT))
				return;
			encrypt_support(subpointer, SB_LEN());
			break;
		case ENCRYPT_REQSTART:
			if (my_want_state_is_wont(TELOPT_ENCRYPT))
				return;
			encrypt_request_start(subpointer, SB_LEN());
			break;
		case ENCRYPT_REQEND:
			if (my_want_state_is_wont(TELOPT_ENCRYPT))
				return;
			/*
			 * We can always send an REQEND so that we cannot
			 * get stuck encrypting.  We should only get this
			 * if we have been able to get in the correct mode
			 * anyhow.
			 */
			encrypt_request_end();
			break;
		case ENCRYPT_IS:
			if (my_want_state_is_dont(TELOPT_ENCRYPT))
				return;
			encrypt_is(subpointer, SB_LEN());
			break;
		case ENCRYPT_REPLY:
			if (my_want_state_is_wont(TELOPT_ENCRYPT))
				return;
			encrypt_reply(subpointer, SB_LEN());
			break;
		case ENCRYPT_ENC_KEYID:
			if (my_want_state_is_dont(TELOPT_ENCRYPT))
				return;
			encrypt_enc_keyid(subpointer, SB_LEN());
			break;
		case ENCRYPT_DEC_KEYID:
			if (my_want_state_is_wont(TELOPT_ENCRYPT))
				return;
			encrypt_dec_keyid(subpointer, SB_LEN());
			break;
		default:
			break;
		}
		break;
#endif
    default:
	break;
    }
}

static unsigned char str_lm[] = { IAC, SB, TELOPT_LINEMODE, 0, 0, IAC, SE };

    void
lm_will(cmd, len)
    unsigned char *cmd;
    int len;
{
    if (len < 1) {
/*@*/	printf("lm_will: no command!!!\n");	/* Should not happen... */
	return;
    }
    switch(cmd[0]) {
    case LM_FORWARDMASK:	/* We shouldn't ever get this... */
    default:
	str_lm[3] = DONT;
	str_lm[4] = cmd[0];
	if (NETROOM() > sizeof(str_lm)) {
	    ring_supply_data(&netoring, str_lm, sizeof(str_lm));
	    printsub('>', &str_lm[2], sizeof(str_lm)-2);
	}
/*@*/	else printf("lm_will: not enough room in buffer\n");
	break;
    }
}

    void
lm_wont(cmd, len)
    unsigned char *cmd;
    int len;
{
    if (len < 1) {
/*@*/	printf("lm_wont: no command!!!\n");	/* Should not happen... */
	return;
    }
    switch(cmd[0]) {
    case LM_FORWARDMASK:	/* We shouldn't ever get this... */
    default:
	/* We are always DONT, so don't respond */
	return;
    }
}

    void
lm_do(cmd, len)
    unsigned char *cmd;
    int len;
{
    if (len < 1) {
/*@*/	printf("lm_do: no command!!!\n");	/* Should not happen... */
	return;
    }
    switch(cmd[0]) {
    case LM_FORWARDMASK:
    default:
	str_lm[3] = WONT;
	str_lm[4] = cmd[0];
	if (NETROOM() > sizeof(str_lm)) {
	    ring_supply_data(&netoring, str_lm, sizeof(str_lm));
	    printsub('>', &str_lm[2], sizeof(str_lm)-2);
	}
/*@*/	else printf("lm_do: not enough room in buffer\n");
	break;
    }
}

    void
lm_dont(cmd, len)
    unsigned char *cmd;
    int len;
{
    if (len < 1) {
/*@*/	printf("lm_dont: no command!!!\n");	/* Should not happen... */
	return;
    }
    switch(cmd[0]) {
    case LM_FORWARDMASK:
    default:
	/* we are always WONT, so don't respond */
	break;
    }
}

static unsigned char str_lm_mode[] = {
	IAC, SB, TELOPT_LINEMODE, LM_MODE, 0, IAC, SE
};

	void
lm_mode(cmd, len, init)
	unsigned char *cmd;
	int len, init;
{
	if (len != 1)
		return;
	if ((linemode&MODE_MASK&~MODE_ACK) == *cmd)
		return;
	if (*cmd&MODE_ACK)
		return;
	linemode = *cmd&(MODE_MASK&~MODE_ACK);
	str_lm_mode[4] = linemode;
	if (!init)
	    str_lm_mode[4] |= MODE_ACK;
	if (NETROOM() > sizeof(str_lm_mode)) {
	    ring_supply_data(&netoring, str_lm_mode, sizeof(str_lm_mode));
	    printsub('>', &str_lm_mode[2], sizeof(str_lm_mode)-2);
	}
/*@*/	else printf("lm_mode: not enough room in buffer\n");
	setconnmode(0);	/* set changed mode */
}



/*
 * slc()
 * Handle special character suboption of LINEMODE.
 */

struct spc {
	cc_t val;
	cc_t *valp;
	char flags;	/* Current flags & level */
	char mylevel;	/* Maximum level & flags */
} spc_data[NSLC+1];

#define SLC_IMPORT	0
#define	SLC_EXPORT	1
#define SLC_RVALUE	2
static int slc_mode = SLC_EXPORT;

	void
slc_init()
{
	register struct spc *spcp;

	localchars = 1;
	for (spcp = spc_data; spcp < &spc_data[NSLC+1]; spcp++) {
		spcp->val = 0;
		spcp->valp = 0;
		spcp->flags = spcp->mylevel = SLC_NOSUPPORT;
	}

#define	initfunc(func, flags) { \
					spcp = &spc_data[func]; \
					if (spcp->valp = tcval(func)) { \
					    spcp->val = *spcp->valp; \
					    spcp->mylevel = SLC_VARIABLE|flags; \
					} else { \
					    spcp->val = 0; \
					    spcp->mylevel = SLC_DEFAULT; \
					} \
				    }

	initfunc(SLC_SYNCH, 0);
	/* No BRK */
	initfunc(SLC_AO, 0);
	initfunc(SLC_AYT, 0);
	/* No EOR */
	initfunc(SLC_ABORT, SLC_FLUSHIN|SLC_FLUSHOUT);
	initfunc(SLC_EOF, 0);
#ifndef	SYSV_TERMIO
	initfunc(SLC_SUSP, SLC_FLUSHIN);
#endif
	initfunc(SLC_EC, 0);
	initfunc(SLC_EL, 0);
#ifndef	SYSV_TERMIO
	initfunc(SLC_EW, 0);
	initfunc(SLC_RP, 0);
	initfunc(SLC_LNEXT, 0);
#endif
	initfunc(SLC_XON, 0);
	initfunc(SLC_XOFF, 0);
#ifdef	SYSV_TERMIO
	spc_data[SLC_XON].mylevel = SLC_CANTCHANGE;
	spc_data[SLC_XOFF].mylevel = SLC_CANTCHANGE;
#endif
	initfunc(SLC_FORW1, 0);
#ifdef	USE_TERMIO
	initfunc(SLC_FORW2, 0);
	/* No FORW2 */
#endif

	initfunc(SLC_IP, SLC_FLUSHIN|SLC_FLUSHOUT);
#undef	initfunc

	if (slc_mode == SLC_EXPORT)
		slc_export();
	else
		slc_import(1);

}

    void
slcstate()
{
    printf("Special characters are %s values\n",
		slc_mode == SLC_IMPORT ? "remote default" :
		slc_mode == SLC_EXPORT ? "local" :
					 "remote");
}

    void
slc_mode_export()
{
    slc_mode = SLC_EXPORT;
    if (my_state_is_will(TELOPT_LINEMODE))
	slc_export();
}

    void
slc_mode_import(def)
    int def;
{
    slc_mode = def ? SLC_IMPORT : SLC_RVALUE;
    if (my_state_is_will(TELOPT_LINEMODE))
	slc_import(def);
}

unsigned char slc_import_val[] = {
	IAC, SB, TELOPT_LINEMODE, LM_SLC, 0, SLC_VARIABLE, 0, IAC, SE
};
unsigned char slc_import_def[] = {
	IAC, SB, TELOPT_LINEMODE, LM_SLC, 0, SLC_DEFAULT, 0, IAC, SE
};

    void
slc_import(def)
    int def;
{
    if (NETROOM() > sizeof(slc_import_val)) {
	if (def) {
	    ring_supply_data(&netoring, slc_import_def, sizeof(slc_import_def));
	    printsub('>', &slc_import_def[2], sizeof(slc_import_def)-2);
	} else {
	    ring_supply_data(&netoring, slc_import_val, sizeof(slc_import_val));
	    printsub('>', &slc_import_val[2], sizeof(slc_import_val)-2);
	}
    }
/*@*/ else printf("slc_import: not enough room\n");
}

    void
slc_export()
{
    register struct spc *spcp;

    TerminalDefaultChars();

    slc_start_reply();
    for (spcp = &spc_data[1]; spcp < &spc_data[NSLC+1]; spcp++) {
	if (spcp->mylevel != SLC_NOSUPPORT) {
	    if (spcp->val == (cc_t)(_POSIX_VDISABLE))
		spcp->flags = SLC_NOSUPPORT;
	    else
		spcp->flags = spcp->mylevel;
	    if (spcp->valp)
		spcp->val = *spcp->valp;
	    slc_add_reply(spcp - spc_data, spcp->flags, spcp->val);
	}
    }
    slc_end_reply();
    (void)slc_update();
    setconnmode(1);	/* Make sure the character values are set */
}

	void
slc(cp, len)
	register unsigned char *cp;
	int len;
{
	register struct spc *spcp;
	register int func,level;

	slc_start_reply();

	for (; len >= 3; len -=3, cp +=3) {

		func = cp[SLC_FUNC];

		if (func == 0) {
			/*
			 * Client side: always ignore 0 function.
			 */
			continue;
		}
		if (func > NSLC) {
			if ((cp[SLC_FLAGS] & SLC_LEVELBITS) != SLC_NOSUPPORT)
				slc_add_reply(func, SLC_NOSUPPORT, 0);
			continue;
		}

		spcp = &spc_data[func];

		level = cp[SLC_FLAGS]&(SLC_LEVELBITS|SLC_ACK);

		if ((cp[SLC_VALUE] == (unsigned char)spcp->val) &&
		    ((level&SLC_LEVELBITS) == (spcp->flags&SLC_LEVELBITS))) {
			continue;
		}

		if (level == (SLC_DEFAULT|SLC_ACK)) {
			/*
			 * This is an error condition, the SLC_ACK
			 * bit should never be set for the SLC_DEFAULT
			 * level.  Our best guess to recover is to
			 * ignore the SLC_ACK bit.
			 */
			cp[SLC_FLAGS] &= ~SLC_ACK;
		}

		if (level == ((spcp->flags&SLC_LEVELBITS)|SLC_ACK)) {
			spcp->val = (cc_t)cp[SLC_VALUE];
			spcp->flags = cp[SLC_FLAGS];	/* include SLC_ACK */
			continue;
		}

		level &= ~SLC_ACK;

		if (level <= (spcp->mylevel&SLC_LEVELBITS)) {
			spcp->flags = cp[SLC_FLAGS]|SLC_ACK;
			spcp->val = (cc_t)cp[SLC_VALUE];
		}
		if (level == SLC_DEFAULT) {
			if ((spcp->mylevel&SLC_LEVELBITS) != SLC_DEFAULT)
				spcp->flags = spcp->mylevel;
			else
				spcp->flags = SLC_NOSUPPORT;
		}
		slc_add_reply(func, spcp->flags, spcp->val);
	}
	slc_end_reply();
	if (slc_update())
		setconnmode(1);	/* set the  new character values */
}

    void
slc_check()
{
    register struct spc *spcp;

    slc_start_reply();
    for (spcp = &spc_data[1]; spcp < &spc_data[NSLC+1]; spcp++) {
	if (spcp->valp && spcp->val != *spcp->valp) {
	    spcp->val = *spcp->valp;
	    if (spcp->val == (cc_t)(_POSIX_VDISABLE))
		spcp->flags = SLC_NOSUPPORT;
	    else
		spcp->flags = spcp->mylevel;
	    slc_add_reply(spcp - spc_data, spcp->flags, spcp->val);
	}
    }
    slc_end_reply();
    setconnmode(1);
}


unsigned char slc_reply[128];
unsigned char *slc_replyp;

	void
slc_start_reply()
{
	slc_replyp = slc_reply;
	*slc_replyp++ = IAC;
	*slc_replyp++ = SB;
	*slc_replyp++ = TELOPT_LINEMODE;
	*slc_replyp++ = LM_SLC;
}

	void
slc_add_reply(func, flags, value)
	unsigned char func;
	unsigned char flags;
	cc_t value;
{
	if ((*slc_replyp++ = func) == IAC)
		*slc_replyp++ = IAC;
	if ((*slc_replyp++ = flags) == IAC)
		*slc_replyp++ = IAC;
	if ((*slc_replyp++ = (unsigned char)value) == IAC)
		*slc_replyp++ = IAC;
}

    void
slc_end_reply()
{
    register int len;

    *slc_replyp++ = IAC;
    *slc_replyp++ = SE;
    len = slc_replyp - slc_reply;
    if (len <= 6)
	return;
    if (NETROOM() > len) {
	ring_supply_data(&netoring, slc_reply, slc_replyp - slc_reply);
	printsub('>', &slc_reply[2], slc_replyp - slc_reply - 2);
    }
/*@*/else printf("slc_end_reply: not enough room\n");
}

	int
slc_update()
{
	register struct spc *spcp;
	int need_update = 0;

	for (spcp = &spc_data[1]; spcp < &spc_data[NSLC+1]; spcp++) {
		if (!(spcp->flags&SLC_ACK))
			continue;
		spcp->flags &= ~SLC_ACK;
		if (spcp->valp && (*spcp->valp != spcp->val)) {
			*spcp->valp = spcp->val;
			need_update = 1;
		}
	}
	return(need_update);
}

	void
env_opt(buf, len)
	register unsigned char *buf;
	register int len;
{
	register unsigned char *ep = 0, *epc = 0;
	register int i;

	switch(buf[0]&0xff) {
	case TELQUAL_SEND:
		env_opt_start();
		if (len == 1) {
			env_opt_add(NULL);
		} else for (i = 1; i < len; i++) {
			switch (buf[i]&0xff) {
			case ENV_VALUE:
				if (ep) {
					*epc = 0;
					env_opt_add(ep);
				}
				ep = epc = &buf[i+1];
				break;
			case ENV_ESC:
				i++;
				/*FALL THROUGH*/
			default:
				if (epc)
					*epc++ = buf[i];
				break;
			}
			if (ep) {
				*epc = 0;
				env_opt_add(ep);
			}
		}
		env_opt_end(1);
		break;

	case TELQUAL_IS:
	case TELQUAL_INFO:
		/* Ignore for now.  We shouldn't get it anyway. */
		break;

	default:
		break;
	}
}

#define	OPT_REPLY_SIZE	256
unsigned char *opt_reply;
unsigned char *opt_replyp;
unsigned char *opt_replyend;

	void
env_opt_start()
{
	if (opt_reply)
		opt_reply = (unsigned char *)realloc(opt_reply, OPT_REPLY_SIZE);
	else
		opt_reply = (unsigned char *)malloc(OPT_REPLY_SIZE);
	if (opt_reply == NULL) {
/*@*/		printf("env_opt_start: malloc()/realloc() failed!!!\n");
		opt_reply = opt_replyp = opt_replyend = NULL;
		return;
	}
	opt_replyp = opt_reply;
	opt_replyend = opt_reply + OPT_REPLY_SIZE;
	*opt_replyp++ = IAC;
	*opt_replyp++ = SB;
	*opt_replyp++ = TELOPT_ENVIRON;
	*opt_replyp++ = TELQUAL_IS;
}

	void
env_opt_start_info()
{
	env_opt_start();
	if (opt_replyp)
	    opt_replyp[-1] = TELQUAL_INFO;
}

	void
env_opt_add(ep)
	register unsigned char *ep;
{
	register unsigned char *vp, c;

	if (opt_reply == NULL)		/*XXX*/
		return;			/*XXX*/

	if (ep == NULL || *ep == '\0') {
		env_default(1);
		while (ep = env_default(0))
			env_opt_add(ep);
		return;
	}
	vp = env_getvalue(ep);
	if (opt_replyp + (vp ? strlen((char *)vp) : 0) +
				strlen((char *)ep) + 6 > opt_replyend)
	{
		register int len;
		opt_replyend += OPT_REPLY_SIZE;
		len = opt_replyend - opt_reply;
		opt_reply = (unsigned char *)realloc(opt_reply, len);
		if (opt_reply == NULL) {
/*@*/			printf("env_opt_add: realloc() failed!!!\n");
			opt_reply = opt_replyp = opt_replyend = NULL;
			return;
		}
		opt_replyp = opt_reply + len - (opt_replyend - opt_replyp);
		opt_replyend = opt_reply + len;
	}
	*opt_replyp++ = ENV_VAR;
	for (;;) {
		while (c = *ep++) {
			switch(c&0xff) {
			case IAC:
				*opt_replyp++ = IAC;
				break;
			case ENV_VALUE:
			case ENV_VAR:
			case ENV_ESC:
				*opt_replyp++ = ENV_ESC;
				break;
			}
			*opt_replyp++ = c;
		}
		if (ep = vp) {
			*opt_replyp++ = ENV_VALUE;
			vp = NULL;
		} else
			break;
	}
}

	void
env_opt_end(emptyok)
	register int emptyok;
{
	register int len;

	len = opt_replyp - opt_reply + 2;
	if (emptyok || len > 6) {
		*opt_replyp++ = IAC;
		*opt_replyp++ = SE;
		if (NETROOM() > len) {
			ring_supply_data(&netoring, opt_reply, len);
			printsub('>', &opt_reply[2], len - 2);
		}
/*@*/		else printf("slc_end_reply: not enough room\n");
	}
	if (opt_reply) {
		free(opt_reply);
		opt_reply = opt_replyp = opt_replyend = NULL;
	}
}



    int
telrcv()
{
    register int c;
    register int scc;
    register unsigned char *sbp;
    int count;
    int returnValue = 0;

    scc = 0;
    count = 0;
    while (TTYROOM() > 2) {
	if (scc == 0) {
	    if (count) {
		ring_consumed(&netiring, count);
		returnValue = 1;
		count = 0;
	    }
	    sbp = netiring.consume;
	    scc = ring_full_consecutive(&netiring);
	    if (scc == 0) {
		/* No more data coming in */
		break;
	    }
	}

	c = *sbp++ & 0xff, scc--; count++;
#if	defined(ENCRYPT)
	if (decrypt_input)
		c = (*decrypt_input)(c);
#endif

	switch (telrcv_state) {

	case TS_CR:
	    telrcv_state = TS_DATA;
	    if (c == '\0') {
		break;	/* Ignore \0 after CR */
	    }
	    else if ((c == '\n') && my_want_state_is_dont(TELOPT_ECHO) && !crmod) {
		TTYADD(c);
		break;
	    }
	    /* Else, fall through */

	case TS_DATA:
	    if (c == IAC) {
		telrcv_state = TS_IAC;
		break;
	    }
#	    if defined(TN3270)
	    if (In3270) {
		*Ifrontp++ = c;
		while (scc > 0) {
		    c = *sbp++ & 0377, scc--; count++;
#if	defined(ENCRYPT)
		    if (decrypt_input)
			c = (*decrypt_input)(c);
#endif
		    if (c == IAC) {
			telrcv_state = TS_IAC;
			break;
		    }
		    *Ifrontp++ = c;
		}
	    } else
#	    endif /* defined(TN3270) */
		    /*
		     * The 'crmod' hack (see following) is needed
		     * since we can't * set CRMOD on output only.
		     * Machines like MULTICS like to send \r without
		     * \n; since we must turn off CRMOD to get proper
		     * input, the mapping is done here (sigh).
		     */
	    if ((c == '\r') && my_want_state_is_dont(TELOPT_BINARY)) {
		if (scc > 0) {
		    c = *sbp&0xff;
#if	defined(ENCRYPT)
		    if (decrypt_input)
			c = (*decrypt_input)(c);
#endif
		    if (c == 0) {
			sbp++, scc--; count++;
			/* a "true" CR */
			TTYADD('\r');
		    } else if (my_want_state_is_dont(TELOPT_ECHO) &&
					(c == '\n')) {
			sbp++, scc--; count++;
			TTYADD('\n');
		    } else {
#if	defined(ENCRYPT)
		        if (decrypt_input)
			    (*decrypt_input)(-1);
#endif

			TTYADD('\r');
			if (crmod) {
				TTYADD('\n');
			}
		    }
		} else {
		    telrcv_state = TS_CR;
		    TTYADD('\r');
		    if (crmod) {
			    TTYADD('\n');
		    }
		}
	    } else {
		TTYADD(c);
	    }
	    continue;

	case TS_IAC:
process_iac:
	    switch (c) {
	    
	    case WILL:
		telrcv_state = TS_WILL;
		continue;

	    case WONT:
		telrcv_state = TS_WONT;
		continue;

	    case DO:
		telrcv_state = TS_DO;
		continue;

	    case DONT:
		telrcv_state = TS_DONT;
		continue;

	    case DM:
		    /*
		     * We may have missed an urgent notification,
		     * so make sure we flush whatever is in the
		     * buffer currently.
		     */
		printoption("RCVD", IAC, DM);
		SYNCHing = 1;
		(void) ttyflush(1);
		SYNCHing = stilloob();
		settimer(gotDM);
		break;

	    case SB:
		SB_CLEAR();
		telrcv_state = TS_SB;
		continue;

#	    if defined(TN3270)
	    case EOR:
		if (In3270) {
		    if (Ibackp == Ifrontp) {
			Ibackp = Ifrontp = Ibuf;
			ISend = 0;	/* should have been! */
		    } else {
			Ibackp += DataFromNetwork(Ibackp, Ifrontp-Ibackp, 1);
			ISend = 1;
		    }
		}
		printoption("RCVD", IAC, EOR);
		break;
#	    endif /* defined(TN3270) */

	    case IAC:
#	    if !defined(TN3270)
		TTYADD(IAC);
#	    else /* !defined(TN3270) */
		if (In3270) {
		    *Ifrontp++ = IAC;
		} else {
		    TTYADD(IAC);
		}
#	    endif /* !defined(TN3270) */
		break;

	    case NOP:
	    case GA:
	    default:
		printoption("RCVD", IAC, c);
		break;
	    }
	    telrcv_state = TS_DATA;
	    continue;

	case TS_WILL:
	    printoption("RCVD", WILL, c);
	    willoption(c);
	    SetIn3270();
	    telrcv_state = TS_DATA;
	    continue;

	case TS_WONT:
	    printoption("RCVD", WONT, c);
	    wontoption(c);
	    SetIn3270();
	    telrcv_state = TS_DATA;
	    continue;

	case TS_DO:
	    printoption("RCVD", DO, c);
	    dooption(c);
	    SetIn3270();
	    if (c == TELOPT_NAWS) {
		sendnaws();
	    } else if (c == TELOPT_LFLOW) {
		localflow = 1;
		setcommandmode();
		setconnmode(0);
	    }
	    telrcv_state = TS_DATA;
	    continue;

	case TS_DONT:
	    printoption("RCVD", DONT, c);
	    dontoption(c);
	    flushline = 1;
	    setconnmode(0);	/* set new tty mode (maybe) */
	    SetIn3270();
	    telrcv_state = TS_DATA;
	    continue;

	case TS_SB:
	    if (c == IAC) {
		telrcv_state = TS_SE;
	    } else {
		SB_ACCUM(c);
	    }
	    continue;

	case TS_SE:
	    if (c != SE) {
		if (c != IAC) {
		    /*
		     * This is an error.  We only expect to get
		     * "IAC IAC" or "IAC SE".  Several things may
		     * have happend.  An IAC was not doubled, the
		     * IAC SE was left off, or another option got
		     * inserted into the suboption are all possibilities.
		     * If we assume that the IAC was not doubled,
		     * and really the IAC SE was left off, we could
		     * get into an infinate loop here.  So, instead,
		     * we terminate the suboption, and process the
		     * partial suboption if we can.
		     */
		    SB_ACCUM(IAC);
		    SB_ACCUM(c);
		    subpointer -= 2;
		    SB_TERM();

		    printoption("In SUBOPTION processing, RCVD", IAC, c);
		    suboption();	/* handle sub-option */
		    SetIn3270();
		    telrcv_state = TS_IAC;
		    goto process_iac;
		}
		SB_ACCUM(c);
		telrcv_state = TS_SB;
	    } else {
		SB_ACCUM(IAC);
		SB_ACCUM(SE);
		subpointer -= 2;
		SB_TERM();
		suboption();	/* handle sub-option */
		SetIn3270();
		telrcv_state = TS_DATA;
	    }
	}
    }
    if (count)
	ring_consumed(&netiring, count);
    return returnValue||count;
}

static int bol = 1, local = 0;

    int
rlogin_susp()
{
    if (local) {
	local = 0;
	bol = 1;
	command(0, "z\n", 2);
	return(1);
    }
    return(0);
}

    static int
telsnd()
{
    int tcc;
    int count;
    int returnValue = 0;
    unsigned char *tbp;

    tcc = 0;
    count = 0;
    while (NETROOM() > 2) {
	register int sc;
	register int c;

	if (tcc == 0) {
	    if (count) {
		ring_consumed(&ttyiring, count);
		returnValue = 1;
		count = 0;
	    }
	    tbp = ttyiring.consume;
	    tcc = ring_full_consecutive(&ttyiring);
	    if (tcc == 0) {
		break;
	    }
	}
	c = *tbp++ & 0xff, sc = strip(c), tcc--; count++;
	if (rlogin != _POSIX_VDISABLE) {
		if (bol) {
			bol = 0;
			if (sc == rlogin) {
				local = 1;
				continue;
			}
		} else if (local) {
			local = 0;
			if (sc == '.' || c == termEofChar) {
				bol = 1;
				command(0, "close\n", 6);
				continue;
			}
			if (sc == termSuspChar) {
				bol = 1;
				command(0, "z\n", 2);
				continue;
			}
			if (sc == escape) {
				command(0, (char *)tbp, tcc);
				bol = 1;
				count += tcc;
				tcc = 0;
				flushline = 1;
				break;
			}
			if (sc != rlogin) {
				++tcc;
				--tbp;
				--count;
				c = sc = rlogin;
			}
		}
		if ((sc == '\n') || (sc == '\r'))
			bol = 1;
	} else if (sc == escape) {
	    /*
	     * Double escape is a pass through of a single escape character.
	     */
	    if (tcc && strip(*tbp) == escape) {
		tbp++;
		tcc--;
		count++;
		bol = 0;
	    } else {
		command(0, (char *)tbp, tcc);
		bol = 1;
		count += tcc;
		tcc = 0;
		flushline = 1;
		break;
	    }
	} else
	    bol = 0;
#ifdef	KLUDGELINEMODE
	if (kludgelinemode && (globalmode&MODE_EDIT) && (sc == echoc)) {
	    if (tcc > 0 && strip(*tbp) == echoc) {
		tcc--; tbp++; count++;
	    } else {
		dontlecho = !dontlecho;
		settimer(echotoggle);
		setconnmode(0);
		flushline = 1;
		break;
	    }
	}
#endif
	if (MODE_LOCAL_CHARS(globalmode)) {
	    if (TerminalSpecialChars(sc) == 0) {
		bol = 1;
		break;
	    }
	}
	if (my_want_state_is_wont(TELOPT_BINARY)) {
	    switch (c) {
	    case '\n':
		    /*
		     * If we are in CRMOD mode (\r ==> \n)
		     * on our local machine, then probably
		     * a newline (unix) is CRLF (TELNET).
		     */
		if (MODE_LOCAL_CHARS(globalmode)) {
		    NETADD('\r');
		}
		NETADD('\n');
		bol = flushline = 1;
		break;
	    case '\r':
		if (!crlf) {
		    NET2ADD('\r', '\0');
		} else {
		    NET2ADD('\r', '\n');
		}
		bol = flushline = 1;
		break;
	    case IAC:
		NET2ADD(IAC, IAC);
		break;
	    default:
		NETADD(c);
		break;
	    }
	} else if (c == IAC) {
	    NET2ADD(IAC, IAC);
	} else {
	    NETADD(c);
	}
    }
    if (count)
	ring_consumed(&ttyiring, count);
    return returnValue||count;		/* Non-zero if we did anything */
}

/*
 * Scheduler()
 *
 * Try to do something.
 *
 * If we do something useful, return 1; else return 0.
 *
 */


    int
Scheduler(block)
    int	block;			/* should we block in the select ? */
{
		/* One wants to be a bit careful about setting returnValue
		 * to one, since a one implies we did some useful work,
		 * and therefore probably won't be called to block next
		 * time (TN3270 mode only).
		 */
    int returnValue;
    int netin, netout, netex, ttyin, ttyout;

    /* Decide which rings should be processed */

    netout = ring_full_count(&netoring) &&
	    (flushline ||
		(my_want_state_is_wont(TELOPT_LINEMODE)
#ifdef	KLUDGELINEMODE
			&& (!kludgelinemode || my_want_state_is_do(TELOPT_SGA))
#endif
		) ||
			my_want_state_is_will(TELOPT_BINARY));
    ttyout = ring_full_count(&ttyoring);

#if	defined(TN3270)
    ttyin = ring_empty_count(&ttyiring) && (shell_active == 0);
#else	/* defined(TN3270) */
    ttyin = ring_empty_count(&ttyiring);
#endif	/* defined(TN3270) */

#if	defined(TN3270)
    netin = ring_empty_count(&netiring);
#   else /* !defined(TN3270) */
    netin = !ISend && ring_empty_count(&netiring);
#   endif /* !defined(TN3270) */

    netex = !SYNCHing;

    /* If we have seen a signal recently, reset things */
#   if defined(TN3270) && defined(unix)
    if (HaveInput) {
	HaveInput = 0;
	(void) signal(SIGIO, inputAvailable);
    }
#endif	/* defined(TN3270) && defined(unix) */

    /* Call to system code to process rings */

    returnValue = process_rings(netin, netout, netex, ttyin, ttyout, !block);

    /* Now, look at the input rings, looking for work to do. */

    if (ring_full_count(&ttyiring)) {
#   if defined(TN3270)
	if (In3270) {
	    int c;

	    c = DataFromTerminal(ttyiring.consume,
					ring_full_consecutive(&ttyiring));
	    if (c) {
		returnValue = 1;
	        ring_consumed(&ttyiring, c);
	    }
	} else {
#   endif /* defined(TN3270) */
	    returnValue |= telsnd();
#   if defined(TN3270)
	}
#   endif /* defined(TN3270) */
    }

    if (ring_full_count(&netiring)) {
#	if !defined(TN3270)
	returnValue |= telrcv();
#	else /* !defined(TN3270) */
	returnValue = Push3270();
#	endif /* !defined(TN3270) */
    }
    return returnValue;
}

/*
 * Select from tty and network...
 */
    void
telnet(user)
    char *user;
{
    sys_telnet_init();

#if defined(ENCRYPT) || defined(AUTHENTICATE)
    {
	static char local_host[256] = { 0 };
	int len = sizeof(local_host);

	if (!local_host[0]) {
		gethostname(local_host, &len);
		local_host[sizeof(local_host)-1] = 0;
	}
	auth_encrypt_init(local_host, hostname, "TELNET", 0);
	auth_encrypt_user(user);
    }
#endif
#   if !defined(TN3270)
    if (telnetport) {
#if	defined(AUTHENTICATE)
	if (autologin)
		send_will(TELOPT_AUTHENTICATION, 1);
#endif
#if	defined(ENCRYPT)
	send_do(TELOPT_ENCRYPT, 1);
	send_will(TELOPT_ENCRYPT, 1);
#endif
	send_do(TELOPT_SGA, 1);
	send_will(TELOPT_TTYPE, 1);
	send_will(TELOPT_NAWS, 1);
	send_will(TELOPT_TSPEED, 1);
	send_will(TELOPT_LFLOW, 1);
	send_will(TELOPT_LINEMODE, 1);
	send_will(TELOPT_ENVIRON, 1);
	send_do(TELOPT_STATUS, 1);
	if (env_getvalue((unsigned char *)"DISPLAY"))
	    send_will(TELOPT_XDISPLOC, 1);
	if (eight)
	    tel_enter_binary(eight);
    }
#   endif /* !defined(TN3270) */

#   if !defined(TN3270)
    for (;;) {
	int schedValue;

	while ((schedValue = Scheduler(0)) != 0) {
	    if (schedValue == -1) {
		setcommandmode();
		return;
	    }
	}

	if (Scheduler(1) == -1) {
	    setcommandmode();
	    return;
	}
    }
#   else /* !defined(TN3270) */
    for (;;) {
	int schedValue;

	while (!In3270 && !shell_active) {
	    if (Scheduler(1) == -1) {
		setcommandmode();
		return;
	    }
	}

	while ((schedValue = Scheduler(0)) != 0) {
	    if (schedValue == -1) {
		setcommandmode();
		return;
	    }
	}
		/* If there is data waiting to go out to terminal, don't
		 * schedule any more data for the terminal.
		 */
	if (ring_full_count(&ttyoring)) {
	    schedValue = 1;
	} else {
	    if (shell_active) {
		if (shell_continue() == 0) {
		    ConnectScreen();
		}
	    } else if (In3270) {
		schedValue = DoTerminalOutput();
	    }
	}
	if (schedValue && (shell_active == 0)) {
	    if (Scheduler(1) == -1) {
		setcommandmode();
		return;
	    }
	}
    }
#   endif /* !defined(TN3270) */
}

#if	0	/* XXX - this not being in is a bug */
/*
 * nextitem()
 *
 *	Return the address of the next "item" in the TELNET data
 * stream.  This will be the address of the next character if
 * the current address is a user data character, or it will
 * be the address of the character following the TELNET command
 * if the current address is a TELNET IAC ("I Am a Command")
 * character.
 */

    static char *
nextitem(current)
    char *current;
{
    if ((*current&0xff) != IAC) {
	return current+1;
    }
    switch (*(current+1)&0xff) {
    case DO:
    case DONT:
    case WILL:
    case WONT:
	return current+3;
    case SB:		/* loop forever looking for the SE */
	{
	    register char *look = current+2;

	    for (;;) {
		if ((*look++&0xff) == IAC) {
		    if ((*look++&0xff) == SE) {
			return look;
		    }
		}
	    }
	}
    default:
	return current+2;
    }
}
#endif	/* 0 */

/*
 * netclear()
 *
 *	We are about to do a TELNET SYNCH operation.  Clear
 * the path to the network.
 *
 *	Things are a bit tricky since we may have sent the first
 * byte or so of a previous TELNET command into the network.
 * So, we have to scan the network buffer from the beginning
 * until we are up to where we want to be.
 *
 *	A side effect of what we do, just to keep things
 * simple, is to clear the urgent data pointer.  The principal
 * caller should be setting the urgent data pointer AFTER calling
 * us in any case.
 */

    static void
netclear()
{
#if	0	/* XXX */
    register char *thisitem, *next;
    char *good;
#define	wewant(p)	((nfrontp > p) && ((*p&0xff) == IAC) && \
				((*(p+1)&0xff) != EC) && ((*(p+1)&0xff) != EL))

    thisitem = netobuf;

    while ((next = nextitem(thisitem)) <= netobuf.send) {
	thisitem = next;
    }

    /* Now, thisitem is first before/at boundary. */

    good = netobuf;	/* where the good bytes go */

    while (netoring.add > thisitem) {
	if (wewant(thisitem)) {
	    int length;

	    next = thisitem;
	    do {
		next = nextitem(next);
	    } while (wewant(next) && (nfrontp > next));
	    length = next-thisitem;
	    memcpy(good, thisitem, length);
	    good += length;
	    thisitem = next;
	} else {
	    thisitem = nextitem(thisitem);
	}
    }

#endif	/* 0 */
}

/*
 * These routines add various telnet commands to the data stream.
 */

    static void
doflush()
{
    NET2ADD(IAC, DO);
    NETADD(TELOPT_TM);
    flushline = 1;
    flushout = 1;
    (void) ttyflush(1);			/* Flush/drop output */
    /* do printoption AFTER flush, otherwise the output gets tossed... */
    printoption("SENT", DO, TELOPT_TM);
}

    void
xmitAO()
{
    NET2ADD(IAC, AO);
    printoption("SENT", IAC, AO);
    if (autoflush) {
	doflush();
    }
}


    void
xmitEL()
{
    NET2ADD(IAC, EL);
    printoption("SENT", IAC, EL);
}

    void
xmitEC()
{
    NET2ADD(IAC, EC);
    printoption("SENT", IAC, EC);
}


    int
dosynch()
{
    netclear();			/* clear the path to the network */
    NETADD(IAC);
    setneturg();
    NETADD(DM);
    printoption("SENT", IAC, DM);
    return 1;
}

int want_status_response = 0;

    int
get_status()
{
    unsigned char tmp[16];
    register unsigned char *cp;

    if (my_want_state_is_dont(TELOPT_STATUS)) {
	printf("Remote side does not support STATUS option\n");
	return 0;
    }
    cp = tmp;

    *cp++ = IAC;
    *cp++ = SB;
    *cp++ = TELOPT_STATUS;
    *cp++ = TELQUAL_SEND;
    *cp++ = IAC;
    *cp++ = SE;
    if (NETROOM() >= cp - tmp) {
	ring_supply_data(&netoring, tmp, cp-tmp);
	printsub('>', tmp+2, cp - tmp - 2);
    }
    ++want_status_response;
    return 1;
}

    void
intp()
{
    NET2ADD(IAC, IP);
    printoption("SENT", IAC, IP);
    flushline = 1;
    if (autoflush) {
	doflush();
    }
    if (autosynch) {
	dosynch();
    }
}

    void
sendbrk()
{
    NET2ADD(IAC, BREAK);
    printoption("SENT", IAC, BREAK);
    flushline = 1;
    if (autoflush) {
	doflush();
    }
    if (autosynch) {
	dosynch();
    }
}

    void
sendabort()
{
    NET2ADD(IAC, ABORT);
    printoption("SENT", IAC, ABORT);
    flushline = 1;
    if (autoflush) {
	doflush();
    }
    if (autosynch) {
	dosynch();
    }
}

    void
sendsusp()
{
    NET2ADD(IAC, SUSP);
    printoption("SENT", IAC, SUSP);
    flushline = 1;
    if (autoflush) {
	doflush();
    }
    if (autosynch) {
	dosynch();
    }
}

    void
sendeof()
{
    NET2ADD(IAC, xEOF);
    printoption("SENT", IAC, xEOF);
}

    void
sendayt()
{
    NET2ADD(IAC, AYT);
    printoption("SENT", IAC, AYT);
}

/*
 * Send a window size update to the remote system.
 */

    void
sendnaws()
{
    long rows, cols;
    unsigned char tmp[16];
    register unsigned char *cp;

    if (my_state_is_wont(TELOPT_NAWS))
	return;

#define	PUTSHORT(cp, x) { if ((*cp++ = ((x)>>8)&0xff) == IAC) *cp++ = IAC; \
			    if ((*cp++ = ((x))&0xff) == IAC) *cp++ = IAC; }

    if (TerminalWindowSize(&rows, &cols) == 0) {	/* Failed */
	return;
    }

    cp = tmp;

    *cp++ = IAC;
    *cp++ = SB;
    *cp++ = TELOPT_NAWS;
    PUTSHORT(cp, cols);
    PUTSHORT(cp, rows);
    *cp++ = IAC;
    *cp++ = SE;
    if (NETROOM() >= cp - tmp) {
	ring_supply_data(&netoring, tmp, cp-tmp);
	printsub('>', tmp+2, cp - tmp - 2);
    }
}

    void
tel_enter_binary(rw)
    int rw;
{
    if (rw&1)
	send_do(TELOPT_BINARY, 1);
    if (rw&2)
	send_will(TELOPT_BINARY, 1);
}

    void
tel_leave_binary(rw)
    int rw;
{
    if (rw&1)
	send_dont(TELOPT_BINARY, 1);
    if (rw&2)
	send_wont(TELOPT_BINARY, 1);
}
