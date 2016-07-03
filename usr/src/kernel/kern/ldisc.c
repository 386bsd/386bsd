/*-
 * Copyright (c) 1982, 1986, 1990 The Regents of the University of California.
 * Copyright (c) 1991 The Regents of the University of California.
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
 *
 * $Id: ldisc.c,v 1.1 94/10/20 00:02:58 bill Exp $
 */

#include "sys/param.h"
#include "sys/file.h"
#include "sys/ioctl.h"
#include "sys/syslog.h"
#include "sys/errno.h"

#include "systm.h"	/* selwait */
#include "proc.h"
#include "privilege.h"
#include "uio.h"
#define TTYDEFCHARS
#include "tty.h"
#undef TTYDEFCHARS
#include "dkstat.h"	/* tkn_... */
#include "kernel.h"	/* lbolt */
#include "vm.h"
#include "vmspace.h"
#include "modconfig.h"

#include "vnode.h"

#include "prototypes.h"


static int proc_compare (struct proc *p1, struct proc *p2);
static int ttnread(struct tty *tp);
static void ttyrubo(struct tty *tp, int cnt);
static void ttyretype(struct tty *tp);
void ttypend(struct tty *tp);
static void ttyrub(int c, struct tty *tp);

/* symbolic sleep message strings */
char ttyin[] = "ttyin";
char ttyout[] = "ttyout";
char ttopen[] = "ttyopn";
char ttclos[] = "ttycls";
char ttybg[] = "ttybg";
char ttybuf[] = "ttybuf";
char ttyopa[] = "ttyopa";

/*
 * Table giving parity for characters and indicating
 * character classes to tty driver. The 8th bit
 * indicates parity, the 7th bit indicates the character
 * is an alphameric or underscore (for ALTWERASE), and the 
 * low 6 bits indicate delay type.  If the low 6 bits are 0
 * then the character needs no special processing on output;
 * classes other than 0 might be translated or (not currently)
 * require delays.
 */
#define	PARITY(c)	(partab[c] & 0x80)
#define	ISALPHA(c)	(partab[(c)&TTY_CHARMASK] & 0x40)
#define	CCLASSMASK	0x3f
#define	CCLASS(c)	(partab[c] & CCLASSMASK)

#define	E	0x00	/* even parity */
#define	O	0x80	/* odd parity */
#define	ALPHA	0x40	/* alpha or underscore */

#define	NO	ORDINARY
#define	NA	ORDINARY|ALPHA
#define	CC	CONTROL
#define	BS	BACKSPACE
#define	NL	NEWLINE
#define	TB	TAB
#define	VT	VTAB
#define	CR	RETURN

char partab[] = {
	E|CC, O|CC, O|CC, E|CC, O|CC, E|CC, E|CC, O|CC,	/* nul - bel */
	O|BS, E|TB, E|NL, O|CC, E|VT, O|CR, O|CC, E|CC, /* bs - si */
	O|CC, E|CC, E|CC, O|CC, E|CC, O|CC, O|CC, E|CC, /* dle - etb */
	E|CC, O|CC, O|CC, E|CC, O|CC, E|CC, E|CC, O|CC, /* can - us */
	O|NO, E|NO, E|NO, O|NO, E|NO, O|NO, O|NO, E|NO, /* sp - ' */
	E|NO, O|NO, O|NO, E|NO, O|NO, E|NO, E|NO, O|NO, /* ( - / */
	E|NA, O|NA, O|NA, E|NA, O|NA, E|NA, E|NA, O|NA, /* 0 - 7 */
	O|NA, E|NA, E|NO, O|NO, E|NO, O|NO, O|NO, E|NO, /* 8 - ? */
	O|NO, E|NA, E|NA, O|NA, E|NA, O|NA, O|NA, E|NA, /* @ - G */
	E|NA, O|NA, O|NA, E|NA, O|NA, E|NA, E|NA, O|NA, /* H - O */
	E|NA, O|NA, O|NA, E|NA, O|NA, E|NA, E|NA, O|NA, /* P - W */
	O|NA, E|NA, E|NA, O|NO, E|NO, O|NO, O|NO, O|NA, /* X - _ */
	E|NO, O|NA, O|NA, E|NA, O|NA, E|NA, E|NA, O|NA, /* ` - g */
	O|NA, E|NA, E|NA, O|NA, E|NA, O|NA, O|NA, E|NA, /* h - o */
	O|NA, E|NA, E|NA, O|NA, E|NA, O|NA, O|NA, E|NA, /* p - w */
	E|NA, O|NA, O|NA, E|NO, O|NO, E|NO, E|NO, O|CC, /* x - del */
	/*
	 * "meta" chars; should be settable per charset.
	 * For now, treat all as normal characters.
	 */
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
};
#undef	NO
#undef	NA
#undef	CC
#undef	BS
#undef	NL
#undef	TB
#undef	VT
#undef	CR

extern struct tty *constty;		/* temporary virtual console */


void
ttychars(struct tty *tp)
{

	(void) memcpy(tp->t_cc, ttydefchars, sizeof(ttydefchars));
}

/* Flush tty after output has drained. */
int
ttywflush(struct tty *tp)
{
	int error;

	if ((error = ttywait(tp)) == 0)
		ttyflush(tp, FREAD);
	return (error);
}

/* Wait for output to drain. */
int
ttywait(register struct tty *tp)
{
	int error = 0, s = spltty();

	while ((RB_LEN(&tp->t_out) || tp->t_state&TS_BUSY) &&
	    (tp->t_state&TS_CARR_ON || tp->t_cflag&CLOCAL) && 
	    tp->t_oproc) {
		(*tp->t_oproc)(tp);
		tp->t_state |= TS_ASLEEP;
		if (error = ttysleep(tp, (caddr_t)&tp->t_out, 
		    TTOPRI | PCATCH, ttyout, 0))
			break;
	}
	splx(s);
	return (error);
}

#define	flushq(qq) { \
	register struct ringb *r = qq; \
	r->rb_hd = r->rb_tl; \
}

/*
 * Flush TTY read and/or write queues,
 * notifying anyone waiting.
 */
void
ttyflush(struct tty *tp, int rw)
{
	int s;

	s = spltty();
	if (rw & FREAD) {
		flushq(&tp->t_can);
		flushq(&tp->t_raw);
		tp->t_rocount = 0;
		tp->t_rocol = 0;
		tp->t_state &= ~TS_LOCAL;
		ttwakeup(tp);
	}
	if (rw & FWRITE) {
		tp->t_state &= ~TS_TTSTOP;
		if (tp->t_stop)
			(*tp->t_stop)(tp, rw);
		flushq(&tp->t_out);
		wakeup((caddr_t)&tp->t_out);
		if (tp->t_wsel) {
			selwakeup(tp->t_wsel, tp->t_state & TS_WCOLL);
			tp->t_wsel = 0;
			tp->t_state &= ~TS_WCOLL;
		}
	}
	splx(s);
}


/* (re)start output on a tty */
void
ttstart(struct tty *tp)
{

	if (tp->t_oproc)		/* kludge for pty */
		(*tp->t_oproc)(tp);
}



/*
 * Common code for ioctls on tty devices.
 * Called after line-discipline-specific ioctl
 * has been called to do discipline-specific functions
 * and/or reject any of these ioctl commands.
 */
/*ARGSUSED*/
int
ttioctl(struct tty *tp, int com, caddr_t data, int flag, struct proc *p)
{
	/* struct proc *p = curproc;		/* XXX */
	int s, error;

	/*
	 * If the ioctl involves modification,
	 * hang if in the background.
	 */
	switch (com) {

	case TIOCSETD: 
	case TIOCFLUSH:
	case TIOCSTI:
	case TIOCSWINSZ:
	case TIOCSETA:
	case TIOCSETAW:
	case TIOCSETAF:
		while (isbackground(curproc, tp) && 
		   p->p_pgrp->pg_jobc && (p->p_flag&SPPWAIT) == 0 &&
		   (p->p_sigignore & sigmask(SIGTTOU)) == 0 &&
		   (p->p_sigmask & sigmask(SIGTTOU)) == 0) {
			pgsignal(p->p_pgrp, SIGTTOU, 1);
			if (error = ttysleep(tp, (caddr_t)&lbolt, 
			    TTOPRI | PCATCH, ttybg, 0)) 
				return (error);
		}
		break;
	}

	/*
	 * Process the ioctl.
	 */
	switch (com) {

	/* get discipline number */
	case TIOCGETD:
		*(int *)data = tp->t_line;
		break;

	/* set line discipline */
	case TIOCSETD: {
		register int t = *(int *)data;
		dev_t dev = tp->t_dev;

		/* if ((unsigned)t >= nldisp)
			return (ENXIO); */
		if (t != tp->t_line) {
			int ot = tp->t_line;
			s = spltty();
			ldiscif_close(tp, flag);
			tp->t_line = t;
			error = ldiscif_open(dev, tp, flag);
			if (error) {
				tp->t_line = ot;
				(void)ldiscif_open(dev, tp, flag);
				splx(s);
				return (error);
			}
			tp->t_line = t;
			splx(s);
		}
		break;
	}

	/* prevent more opens on channel */
	case TIOCEXCL:
		tp->t_state |= TS_XCLUDE;
		break;

	case TIOCNXCL:
		tp->t_state &= ~TS_XCLUDE;
		break;

	case TIOCFLUSH: {
		register int flags = *(int *)data;

		if (flags == 0)
			flags = FREAD|FWRITE;
		else
			flags &= FREAD|FWRITE;
		ttyflush(tp, flags);
		break;
	}

	case FIOASYNC:
		if (*(int *)data)
			tp->t_state |= TS_ASYNC;
		else
			tp->t_state &= ~TS_ASYNC;
		break;

	case FIONBIO:
		break;	/* XXX remove */

	/* return number of characters immediately available */
	case FIONREAD:
		*(off_t *)data = ttnread(tp);
		break;

	case TIOCOUTQ:
		*(int *)data = RB_LEN(&tp->t_out);
		break;

	case TIOCSTOP:
		s = spltty();
		if ((tp->t_state&TS_TTSTOP) == 0) {
			tp->t_state |= TS_TTSTOP;
			if (tp->t_stop)
				(*tp->t_stop)(tp, 0);
		}
		splx(s);
		break;

	case TIOCSTART:
		s = spltty();
		if ((tp->t_state&TS_TTSTOP) || (tp->t_lflag&FLUSHO)) {
			tp->t_state &= ~TS_TTSTOP;
			tp->t_lflag &= ~FLUSHO;
			ttstart(tp);
		}
		splx(s);
		break;

	/*
	 * Simulate typing of a character at the terminal.
	 */
	case TIOCSTI:
		if (p->p_ucred->cr_uid && (flag & FREAD) == 0)
			return (EPERM);
		if (p->p_ucred->cr_uid && !isctty(p, tp))
			return (EACCES);
		(void)ldiscif_rint(*(u_char *)data, tp);
		break;

	case TIOCGETA: {
		struct termios *t = (struct termios *)data;

		(void) memcpy(t, &tp->t_termios, sizeof(struct termios));
		break;
	}

	case TIOCSETA:
	case TIOCSETAW:
	case TIOCSETAF: {
		register struct termios *t = (struct termios *)data;

		s = spltty();
		if (com == TIOCSETAW || com == TIOCSETAF) {
			if (error = ttywait(tp)) {
				splx(s);
				return (error);
			}
			if (com == TIOCSETAF)
				ttyflush(tp, FREAD);
		}
		if ((t->c_cflag&CIGNORE) == 0) {
			/*
			 * set device hardware
			 */
			if (tp->t_param && (error = (*tp->t_param)(tp, t))) {
				splx(s);
				return (error);
			} else {
				if ((tp->t_state&TS_CARR_ON) == 0 &&
				    (tp->t_cflag&CLOCAL) &&
				    (t->c_cflag&CLOCAL) == 0) {
					tp->t_state &= ~TS_ISOPEN;
					tp->t_state |= TS_WOPEN;
					ttwakeup(tp);
				}
				tp->t_cflag = t->c_cflag;
				tp->t_ispeed = t->c_ispeed;
				tp->t_ospeed = t->c_ospeed;
			}
			ttsetwater(tp);
		}
		if (com != TIOCSETAF) {
			if ((t->c_lflag&ICANON) != (tp->t_lflag&ICANON))
				if (t->c_lflag&ICANON) {	
					tp->t_lflag |= PENDIN;
					ttwakeup(tp);
				}
				else {
					catb(&tp->t_raw, &tp->t_can);
					catb(&tp->t_can, &tp->t_raw);
				}
		}
		tp->t_iflag = t->c_iflag;
		tp->t_oflag = t->c_oflag;
		/*
		 * Make the EXTPROC bit read only.
		 */
		if (tp->t_lflag&EXTPROC)
			t->c_lflag |= EXTPROC;
		else
			t->c_lflag &= ~EXTPROC;
		tp->t_lflag = t->c_lflag;
		(void) memcpy(tp->t_cc, t->c_cc, sizeof(t->c_cc));
		splx(s);
		break;
	}

	/*
	 * Set controlling terminal.
	 * Session ctty vnode pointer set in vnode layer.
	 */
	case TIOCSCTTY:
		if (!SESS_LEADER(p) || 
		   (p->p_session->s_ttyvp || tp->t_session) &&
		   (tp->t_session != p->p_session))
			return (EPERM);
		tp->t_session = p->p_session;
		tp->t_pgrp = p->p_pgrp;
		p->p_session->s_ttyp = tp;
		p->p_flag |= SCTTY;
		break;
		
	/*
	 * Set terminal process group.
	 */
	case TIOCSPGRP: {
		register struct pgrp *pgrp = pgfind(*(int *)data);

		if (!isctty(p, tp))
			return (ENOTTY);
		else if (pgrp == NULL || pgrp->pg_session != p->p_session)
			return (EPERM);
		tp->t_pgrp = pgrp;
		break;
	}

	case TIOCGPGRP:
		if (!isctty(p, tp))
			return (ENOTTY);
		*(int *)data = tp->t_pgrp ? tp->t_pgrp->pg_id : NO_PID;
		break;

	case TIOCSWINSZ:
		if (memcmp((caddr_t)&tp->t_winsize, data,
		    sizeof (struct winsize))) {
			tp->t_winsize = *(struct winsize *)data;
			pgsignal(tp->t_pgrp, SIGWINCH, 1);
		}
		break;

	case TIOCGWINSZ:
		*(struct winsize *)data = tp->t_winsize;
		break;

	case TIOCCONS:
		if (*(int *)data) {
			if (constty && constty != tp &&
			    (constty->t_state & (TS_CARR_ON|TS_ISOPEN)) ==
			    (TS_CARR_ON|TS_ISOPEN))
				return (EBUSY);
			if (error = use_priv(p->p_ucred, PRV_TIOCCONS1, p))
				return (error);
			constty = tp;
		} else if (tp == constty)
			constty = NULL;
		break;

	case TIOCDRAIN:
		if (error = ttywait(tp))
			return (error);
		break;

	default:
#ifdef COMPAT_43
		return (ttcompat(tp, com, data, flag, p));
#else
		return (-1);
#endif
	}
	return (0);
}

/*
 * reinput pending characters after state switch
 * call at spltty().
 */
void
ttypend(struct tty *tp)
{
	char *hd, *tl;

	tp->t_lflag &= ~PENDIN;
	tp->t_state |= TS_TYPEN;
	hd = tp->t_raw.rb_hd;
	tl = tp->t_raw.rb_tl;
	flushq(&tp->t_raw);
	while (hd != tl) {
		ldiscif_rint(*(u_char *)hd, tp);
		hd = RB_SUCC(&tp->t_raw, hd);
	}
	tp->t_state &= ~TS_TYPEN;
}

int
ttnread(struct tty *tp)
{
	int nread = 0;

	if (tp->t_lflag & PENDIN)
		ttypend(tp);
	nread = RB_LEN(&tp->t_can);
	if ((tp->t_lflag & ICANON) == 0)
		nread += RB_LEN(&tp->t_raw);
	return (nread);
}

int
ttselect(struct tty *tp, int rw, struct proc *p)
{
	int nread;
	int s = spltty();
	struct proc *selp;

	switch (rw) {

	case FREAD:
		nread = ttnread(tp);
		if (nread > 0 || 
		   ((tp->t_cflag&CLOCAL) == 0 && (tp->t_state&TS_CARR_ON) == 0))
			goto win;
		if (tp->t_rsel && (selp = pfind(tp->t_rsel)) && selp->p_wchan == (caddr_t)&selwait)
			tp->t_state |= TS_RCOLL;
		else
			tp->t_rsel = p->p_pid;
		break;

	case FWRITE:
		if (RB_LEN(&tp->t_out) <= tp->t_lowat)
			goto win;
		if (tp->t_wsel && (selp = pfind(tp->t_wsel)) && selp->p_wchan == (caddr_t)&selwait)
			tp->t_state |= TS_WCOLL;
		else
			tp->t_wsel = p->p_pid;
		break;
	}
	splx(s);
	return (0);
win:
	splx(s);
	return (1);
}


/*
 * Handle close() on a tty line: flush and set to initial state,
 * bumping generation number so that pending read/write calls
 * can detect recycling of the tty.
 */
int
ttyclose(register struct tty *tp)
{
	if (constty == tp)
		constty = NULL;
	ttyflush(tp, FREAD|FWRITE);
	tp->t_session = NULL;
	tp->t_pgrp = NULL;
	tp->t_state = 0;
	tp->t_gen++;
	return (0);
}


/*
 * Default modem control routine (for other line disciplines).
 * Return argument flag, to turn off device on carrier drop.
 */
int
nullmodem(register struct tty *tp, int flag)
{
	
	if (flag)
		tp->t_state |= TS_CARR_ON;
	else {
		tp->t_state &= ~TS_CARR_ON;
		if ((tp->t_cflag&CLOCAL) == 0) {
			if (tp->t_session && tp->t_session->s_leader)
				psignal(tp->t_session->s_leader, SIGHUP);
			return (0);
		}
	}
	return (1);
}

/*
 * Check the output queue on tp for space for a kernel message
 * (from uprintf/tprintf).  Allow some space over the normal
 * hiwater mark so we don't lose messages due to normal flow
 * control, but don't let the tty run amok.
 * Sleeps here are not interruptible, but we return prematurely
 * if new signals come in.
 */
int
ttycheckoutq(register struct tty *tp, int wait)
{
	int hiwat, s, oldsig;

	hiwat = tp->t_hiwat;
	s = spltty();
	if (curproc)
		oldsig = curproc->p_sig;
	else
		oldsig = 0;
	if (RB_LEN(&tp->t_out) > hiwat + 200)
		while (RB_LEN(&tp->t_out) > hiwat) {
			ttstart(tp);
			if (wait == 0 || (curproc && curproc->p_sig != oldsig)) {
				splx(s);
				return (0);
			}
			timeout((int (*)())wakeup, (caddr_t)&tp->t_out, hz);
			tp->t_state |= TS_ASLEEP;
			(void) tsleep((caddr_t)&tp->t_out, PWAIT - 1, ttyopa, 0);
		}
	splx(s);
	return (1);
}

/* Wake up any readers on a tty.  */
void
ttwakeup(register struct tty *tp)
{

	if (tp->t_rsel) {
		selwakeup(tp->t_rsel, tp->t_state&TS_RCOLL);
		tp->t_state &= ~TS_RCOLL;
		tp->t_rsel = 0;
	}
	if (tp->t_state & TS_ASYNC)
		pgsignal(tp->t_pgrp, SIGIO, 1); 
	wakeup((caddr_t)&tp->t_raw);
}

/*
 * Look up a code for a specified speed in a conversion table;
 * used by drivers to map software speed values to hardware parameters.
 */
int
ttspeedtab(int speed, register struct speedtab *table)
{

	for ( ; table->sp_speed != -1; table++)
		if (table->sp_speed == speed)
			return (table->sp_code);
	return (-1);
}

/*
 * set tty hi and low water marks
 *
 * Try to arrange the dynamics so there's about one second
 * from hi to low water.
 * 
 */
int
ttsetwater(struct tty *tp)
{
	register cps = tp->t_ospeed / 10;
	register x;

#define clamp(x, h, l) ((x)>h ? h : ((x)<l) ? l : (x))
	tp->t_lowat = x = clamp(cps/2, TTMAXLOWAT, TTMINLOWAT);
	x += cps;
	x = clamp(x, TTMAXHIWAT, TTMINHIWAT);
	tp->t_hiwat = roundup(x, 1 /*CBSIZE*/);
#undef clamp
}

/*
 * Report on state of foreground process group.
 */
void
ttyinfo(register struct tty *tp)
{
	register struct proc *p, *pick;
	struct timeval utime, stime;
	int tmp;

	if (ttycheckoutq(tp,0) == 0) 
		return;

	/* Print load average. */
	tmp = (averunnable[0] * 100 + FSCALE / 2) >> FSHIFT;
	ttyprintf(tp, "load: %d.%02d ", tmp / 100, tmp % 100);

	if (tp->t_session == NULL)
		ttyprintf(tp, "not a controlling terminal\n");
	else if (tp->t_pgrp == NULL)
		ttyprintf(tp, "no foreground process group\n");
	else if ((p = tp->t_pgrp->pg_mem) == NULL)
		ttyprintf(tp, "empty foreground process group\n");
	else {
		/* Pick interesting process. */
		for (pick = NULL; p != NULL; p = p->p_pgrpnxt)
			if (proc_compare(pick, p))
				pick = p;

		ttyprintf(tp, " cmd: %s %d [%s] ", pick->p_comm, pick->p_pid,
		    pick->p_stat == SRUN ? (pick == curproc ? "running" : "run") :
		    pick->p_wmesg ? pick->p_wmesg : "iowait");

		/*
		 * Lock out clock if process is running; get user/system
		 * cpu time.
		 */
		if (curproc == pick)
			tmp = splclock();
		utime = pick->p_utime;
		stime = pick->p_stime;
		if (curproc == pick)
			splx(tmp);

		/* Print user time. */
		ttyprintf(tp, "%d.%02du ",
		    utime.tv_sec, (utime.tv_usec + 5000) / 10000);

		/* Print system time. */
		ttyprintf(tp, "%d.%02ds ",
		    stime.tv_sec, (stime.tv_usec + 5000) / 10000);

#define	pgtok(a)	(((a) * NBPG) / 1024)
		/* Print percentage cpu, resident set size. */
		tmp = pick->p_pctcpu * 10000 + FSCALE / 2 >> FSHIFT;
		ttyprintf(tp, "%d%% %dk\n",
		   tmp / 100, pgtok(pick->p_vmspace->vm_rssize));
	}
	tp->t_rocount = 0;	/* so pending input will be retyped if BS */
}

/*
 * Returns 1 if p2 is "better" than p1
 *
 * The algorithm for picking the "interesting" process is thus:
 *
 *	1) (Only foreground processes are eligable - implied)
 *	2) Runnable processes are favored over anything
 *	   else.  The runner with the highest cpu
 *	   utilization is picked (p_cpu).  Ties are
 *	   broken by picking the highest pid.
 *	3  Next, the sleeper with the shortest sleep
 *	   time is favored.  With ties, we pick out
 *	   just "short-term" sleepers (SSINTR == 0).
 *	   Further ties are broken by picking the highest
 *	   pid.
 *
 */
#define isrun(p)	(((p)->p_stat == SRUN) || ((p)->p_stat == SIDL))
#define TESTAB(a, b)    ((a)<<1 | (b))
#define ONLYA   2
#define ONLYB   1
#define BOTH    3

static int
proc_compare(register struct proc *p1, register struct proc *p2)
{

	if (p1 == NULL)
		return (1);
	/*
	 * see if at least one of them is runnable
	 */
	switch (TESTAB(isrun(p1), isrun(p2))) {
	case ONLYA:
		return (0);
	case ONLYB:
		return (1);
	case BOTH:
		/*
		 * tie - favor one with highest recent cpu utilization
		 */
		if (p2->p_cpu > p1->p_cpu)
			return (1);
		if (p1->p_cpu > p2->p_cpu)
			return (0);
		return (p2->p_pid > p1->p_pid);	/* tie - return highest pid */
	}
	/*
 	 * weed out zombies
	 */
	switch (TESTAB(p1->p_stat == SZOMB, p2->p_stat == SZOMB)) {
	case ONLYA:
		return (1);
	case ONLYB:
		return (0);
	case BOTH:
		return (p2->p_pid > p1->p_pid); /* tie - return highest pid */
	}
	/* 
	 * pick the one with the smallest sleep time
	 */
	if (p2->p_slptime > p1->p_slptime)
		return (0);
	if (p1->p_slptime > p2->p_slptime)
		return (1);
	/*
	 * favor one sleeping in a non-interruptible sleep
	 */
	if (p1->p_flag&SSINTR && (p2->p_flag&SSINTR) == 0)
		return (1);
	if (p2->p_flag&SSINTR && (p1->p_flag&SSINTR) == 0)
		return (0);
	return (p2->p_pid > p1->p_pid);		/* tie - return highest pid */
}

/*
 * Output char to tty; console putchar style.
 */
int
tputchar(int c, struct tty *tp)
{
	register s = spltty();

	if ((tp->t_state & (TS_CARR_ON|TS_ISOPEN)) == (TS_CARR_ON|TS_ISOPEN)) {
		if (c == '\n')
			(void) ldiscif_put('\r', tp);
		(void) ldiscif_put(c, tp);
		/*if (c == '\n')
			(void) ttyoutput('\r', tp);
		(void) ttyoutput(c, tp);*/
		ttstart(tp);
		splx(s);
		return (0);
	}
	splx(s);
	return (-1);
}

/*
 * Sleep on chan, returning ERESTART if tty changed
 * while we napped and returning any errors (e.g. EINTR/ETIMEDOUT)
 * reported by tsleep.  If the tty is revoked, restarting a pending
 * call will redo validation done at the start of the call.
 */
int
ttysleep(struct tty *tp, caddr_t chan, int pri, char *wmesg, int timo)
{
	int error;
	short gen = tp->t_gen;

	if (error = tsleep(chan, pri, wmesg, timo))
		return (error);
	if (tp->t_gen != gen)
		return (ERESTART);
	return (0);
}

int
nullioctl(struct tty *tp, int cmd, char *data, int flags, struct proc *p)
{

	return (-1);
}

