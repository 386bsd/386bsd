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
 *	$Id: termios.c,v 1.1 94/07/28 14:01:04 bill Exp $
 */
static char *tty_config = "termios 0. # POSIX/BSD line discipline $Revision: 1.1 $";

#include "sys/param.h"
#include "sys/file.h"
#include "sys/ioctl.h"
#include "sys/syslog.h"
#include "uio.h"
#include "sys/errno.h"

#include "proc.h"
#include "tty.h"
#include "kernel.h"	/* lbolt */
#include "dkstat.h"	/* tkn_... */
#include "vm.h"
#include "vmspace.h"
#include "modconfig.h"

#include "vnode.h"

#include "prototypes.h"

static void ttyrubo(struct tty *tp, int cnt);
static void ttyretype(struct tty *tp);
static void ttyrub(int c, struct tty *tp);

/* --- termios --- */
static int ttyoutput(unsigned c, struct tty *tp);
static void ttyecho(int c, struct tty *tp);
static void ttyoutstr(char *cp, struct tty *tp);

#define	PARITY(c)	(partab[c] & 0x80)
#define	ISALPHA(c)	(partab[(c)&TTY_CHARMASK] & 0x40)
#define	CCLASSMASK	0x3f
#define	CCLASS(c)	(partab[c] & CCLASSMASK)
extern char partab[];
int nullioctl(struct tty *tp, int cmd, char *data, int flags, struct proc *p);

/*
 * Is 'c' a line delimiter ("break" character)?
 */
#define ttbreakc(c) ((c) == '\n' || ((c) == cc[VEOF] || \
	(c) == cc[VEOL] || (c) == cc[VEOL2]) && (c) != _POSIX_VDISABLE)

/*
 * Initial open of tty, or (re)entry to standard tty line discipline.
 */
static int
ttyopen(dev_t dev, struct tty *tp, int flags)
{

	tp->t_dev = dev;

	tp->t_state &= ~TS_WOPEN;
	if ((tp->t_state & TS_ISOPEN) == 0) {
		tp->t_state |= TS_ISOPEN;
		initrb(&tp->t_raw);
		initrb(&tp->t_can);
		initrb(&tp->t_out);
		(void) memset((caddr_t)&tp->t_winsize, 0, sizeof(tp->t_winsize));
	}
	return (0);
}

/*
 * "close" a line discipline
 */
static void
ttylclose(struct tty *tp, int flag)
{

	if (flag&IO_NDELAY)
		ttyflush(tp, FREAD|FWRITE);
	else
		ttywflush(tp);
}

/*
 * Process a write call on a tty device.
 */
static int
ttwrite(register struct tty *tp, register struct uio *uio, int flag)
{
	register char *cp;
	register int cc = 0, ce;
	struct proc *p = uio->uio_procp;
	int i, hiwat, cnt, error, s;
	char obuf[OBUFSIZ];

	hiwat = tp->t_hiwat;
	cnt = uio->uio_resid;
	error = 0;
loop:
	s = spltty();
	if ((tp->t_state&TS_CARR_ON) == 0 && (tp->t_cflag&CLOCAL) == 0) {
		if (tp->t_state&TS_ISOPEN) {
			splx(s);
			return (EIO);
		} else if (flag & IO_NDELAY) {
			splx(s);
			error = EWOULDBLOCK;
			goto out;
		} else {
			/*
			 * sleep awaiting carrier
			 */
			error = ttysleep(tp, (caddr_t)&tp->t_raw, 
					TTIPRI | PCATCH,ttopen, 0);
			splx(s);
			if (error)
				goto out;
			goto loop;
		}
	}
	splx(s);
	/*
	 * Hang the process if it's in the background.
	 */
	if (isbackground(p, tp) && 
	    tp->t_lflag&TOSTOP && (p->p_flag&SPPWAIT) == 0 &&
	    (p->p_sigignore & sigmask(SIGTTOU)) == 0 &&
	    (p->p_sigmask & sigmask(SIGTTOU)) == 0 &&
	     p->p_pgrp->pg_jobc) {
		pgsignal(p->p_pgrp, SIGTTOU, 1);
		if (error = ttysleep(tp, (caddr_t)&lbolt, TTIPRI | PCATCH, 
		    ttybg, 0))
			goto out;
		goto loop;
	}
	/*
	 * Process the user's data in at most OBUFSIZ
	 * chunks.  Perform any output translation.
	 * Keep track of high water mark, sleep on overflow
	 * awaiting device aid in acquiring new space.
	 */
	while (uio->uio_resid > 0 || cc > 0) {
		if (tp->t_lflag&FLUSHO) {
			uio->uio_resid = 0;
			return (0);
		}
		if (RB_LEN(&tp->t_out) > hiwat)
			goto ovhiwat;
		/*
		 * Grab a hunk of data from the user,
		 * unless we have some leftover from last time.
		 */
		if (cc == 0) {
			cc = min(uio->uio_resid, OBUFSIZ);
			cp = obuf;
			error = uiomove(cp, cc, uio);
			if (error) {
				cc = 0;
				break;
			}
		}
		/*
		 * If nothing fancy need be done, grab those characters we
		 * can handle without any of ttyoutput's processing and
		 * just transfer them to the output q.  For those chars
		 * which require special processing (as indicated by the
		 * bits in partab), call ttyoutput.  After processing
		 * a hunk of data, look for FLUSHO so ^O's will take effect
		 * immediately.
		 */
		while (cc > 0) {
			if ((tp->t_oflag&OPOST) == 0)
				ce = cc;
			else {
				ce = cc - scanc((unsigned)cc, (u_char *)cp,
				   (u_char *)partab, CCLASSMASK);
				/*
				 * If ce is zero, then we're processing
				 * a special character through ttyoutput.
				 */
				if (ce == 0) {
					tp->t_rocount = 0;
					if (ttyoutput(*cp, tp) >= 0) {
					    /* no c-lists, wait a bit */
					    ttstart(tp);
					    if (error = ttysleep(tp, 
						(caddr_t)&lbolt,
						 TTOPRI | PCATCH, ttybuf, 0))
						    break;
					    goto loop;
					}
					cp++, cc--;
					if ((tp->t_lflag&FLUSHO) ||
					    RB_LEN(&tp->t_out) > hiwat)
						goto ovhiwat;
					continue;
				}
			}
			/*
			 * A bunch of normal characters have been found,
			 * transfer them en masse to the output queue and
			 * continue processing at the top of the loop.
			 * If there are any further characters in this
			 * <= OBUFSIZ chunk, the first should be a character
			 * requiring special handling by ttyoutput.
			 */
			tp->t_rocount = 0;
#ifdef was
			i = b_to_q(cp, ce, &tp->t_outq);
			ce -= i;
#else
			i = ce;
			ce = min (ce, RB_CONTIGPUT(&tp->t_out));
			(void) memcpy(tp->t_out.rb_tl, cp, ce);
			tp->t_out.rb_tl = RB_ROLLOVER(&tp->t_out,
				tp->t_out.rb_tl + ce);
			i -= ce;
			if (i > 0) {
				int ii;

				ii = min (i, RB_CONTIGPUT(&tp->t_out));
				(void) memcpy(tp->t_out.rb_tl, cp + ce, ii);
				tp->t_out.rb_tl = RB_ROLLOVER(&tp->t_out,
					tp->t_out.rb_tl + ii);
				i -= ii;
				ce += ii;
			}
#endif
			tp->t_col += ce;
			cp += ce, cc -= ce, tk_nout += ce;
			tp->t_outcc += ce;
			if (i > 0) {
				/* out of space, wait a bit */
				ttstart(tp);
				if (error = ttysleep(tp, (caddr_t)&lbolt,
					    TTOPRI | PCATCH, ttybuf, 0))
					break;
				goto loop;
			}
			if (tp->t_lflag&FLUSHO || RB_LEN(&tp->t_out) > hiwat)
				break;
		}
		ttstart(tp);
	}
out:
	/*
	 * If cc is nonzero, we leave the uio structure inconsistent,
	 * as the offset and iov pointers have moved forward,
	 * but it doesn't matter (the call will either return short
	 * or restart with a new uio).
	 */
	uio->uio_resid += cc;
	return (error);

ovhiwat:
	ttstart(tp);
	s = spltty();
	/*
	 * This can only occur if FLUSHO is set in t_lflag,
	 * or if ttstart/oproc is synchronous (or very fast).
	 */
	if (RB_LEN(&tp->t_out) <= hiwat) {
		splx(s);
		goto loop;
	}
	if (flag & IO_NDELAY) {
		splx(s);
		uio->uio_resid += cc;
		if (uio->uio_resid == cnt)
			return (EWOULDBLOCK);
		return (0);
	}
	tp->t_state |= TS_ASLEEP;
	error = ttysleep(tp, (caddr_t)&tp->t_out, TTOPRI | PCATCH, ttyout, 0);
	splx(s);
	if (error)
		goto out;
	goto loop;
}

/*
 * Rubout one character from the rawq of tp
 * as cleanly as possible.
 */
static void
ttyrub(register int c, register struct tty *tp)
{
	char *cp;
	register int savecol;
	int s;

	if ((tp->t_lflag&ECHO) == 0 || (tp->t_lflag&EXTPROC))
		return;
	tp->t_lflag &= ~FLUSHO;	
	if (tp->t_lflag&ECHOE) {
		if (tp->t_rocount == 0) {
			/*
			 * Screwed by ttwrite; retype
			 */
			ttyretype(tp);
			return;
		}
		if (c == ('\t'|TTY_QUOTE) || c == ('\n'|TTY_QUOTE))
			ttyrubo(tp, 2);
		else switch (CCLASS(c &= TTY_CHARMASK)) {

		case ORDINARY:
			ttyrubo(tp, 1);
			break;

		case VTAB:
		case BACKSPACE:
		case CONTROL:
		case RETURN:
		case NEWLINE:
			if (tp->t_lflag&ECHOCTL)
				ttyrubo(tp, 2);
			break;

		case TAB: {
			int c;

			if (tp->t_rocount < RB_LEN(&tp->t_raw)) {
				ttyretype(tp);
				return;
			}
			s = spltty();
			savecol = tp->t_col;
			tp->t_state |= TS_CNTTB;
			tp->t_lflag |= FLUSHO;
			tp->t_col = tp->t_rocol;
			cp = tp->t_raw.rb_hd;
			for (c = nextc(&cp, &tp->t_raw); c ;
				c = nextc(&cp, &tp->t_raw))
				ttyecho(c, tp);
			tp->t_lflag &= ~FLUSHO;
			tp->t_state &= ~TS_CNTTB;
			splx(s);
			/*
			 * savecol will now be length of the tab
			 */
			savecol -= tp->t_col;
			tp->t_col += savecol;
			if (savecol > 8)
				savecol = 8;		/* overflow screw */
			while (--savecol >= 0)
				(void) ttyoutput('\b', tp);
			break;
		}

		default:
			/* XXX */
			printf("ttyrub: would panic c = %d, val = %d\n",
				c, CCLASS(c));
			/*panic("ttyrub");*/
		}
	} else if (tp->t_lflag&ECHOPRT) {
		if ((tp->t_state&TS_ERASE) == 0) {
			(void) ttyoutput('\\', tp);
			tp->t_state |= TS_ERASE;
		}
		ttyecho(c, tp);
	} else
		ttyecho(tp->t_cc[VERASE], tp);
	tp->t_rocount--;
}

/*
 * Crt back over cnt chars perhaps
 * erasing them.
 */
static void
ttyrubo(register struct tty *tp, int cnt)
{

	while (--cnt >= 0)
		ttyoutstr("\b \b", tp);
}

/*
 * Reprint the rawq line.
 * We assume c_cc has already been checked.
 */
static void
ttyretype(register struct tty *tp)
{
	char *cp;
	int s, c;

	if (tp->t_cc[VREPRINT] != _POSIX_VDISABLE)
		ttyecho(tp->t_cc[VREPRINT], tp);
	(void) ttyoutput('\n', tp);

	s = spltty();
	cp = tp->t_can.rb_hd;
	for (c = nextc(&cp, &tp->t_can); c ; c = nextc(&cp, &tp->t_can))
		ttyecho(c, tp);
	cp = tp->t_raw.rb_hd;
	for (c = nextc(&cp, &tp->t_raw); c ; c = nextc(&cp, &tp->t_raw))
		ttyecho(c, tp);
	tp->t_state &= ~TS_ERASE;
	splx(s);

	tp->t_rocount = RB_LEN(&tp->t_raw);
	tp->t_rocol = 0;
}

/*
 * Echo a typed character to the terminal.
 */
static void
ttyecho(int c, struct tty *tp)
{
	if ((tp->t_state&TS_CNTTB) == 0)
		tp->t_lflag &= ~FLUSHO;
	if (((tp->t_lflag&ECHO) == 0 &&
	    ((tp->t_lflag&ECHONL) == 0 || c == '\n')) || (tp->t_lflag&EXTPROC))
		return;
	if (tp->t_lflag&ECHOCTL) {
		if ((c&TTY_CHARMASK) <= 037 && c != '\t' && c != '\n' ||
		    c == 0177) {
			(void) ttyoutput('^', tp);
			c &= TTY_CHARMASK;
			if (c == 0177)
				c = '?';
			else
				c += 'A' - 1;
		}
	}
	(void) ttyoutput(c, tp);
}

/*
 * send string cp to tp
 */
static void
ttyoutstr(char *cp, struct tty *tp)
{
	unsigned c;

	while (c = *cp++)
		(void) ttyoutput(c, tp);
}

/* Send stop character on input overflow. */
static void
ttyblock(register struct tty *tp)
{
	register x;
	int rawcc, cancc;

	rawcc = RB_LEN(&tp->t_raw);
	cancc = RB_LEN(&tp->t_can);
	x = rawcc + cancc;
	if (rawcc > TTYHOG) {
		ttyflush(tp, FREAD|FWRITE);
		tp->t_state &= ~TS_TBLOCK;
	}
	/*
	 * Block further input iff:
	 * Current input > threshold AND input is available to user program
	 */
	if (x >= TTYHOG/2 && (tp->t_state & TS_TBLOCK) == 0 &&
	    ((tp->t_lflag&ICANON) == 0) || (cancc > 0) &&
	    tp->t_cc[VSTOP] != _POSIX_VDISABLE) {
		if (putc(tp->t_cc[VSTOP], &tp->t_out) == 0) {
			tp->t_state |= TS_TBLOCK;
			ttstart(tp);
		}
	}
}

/*
 * Process input of a single character received on a tty.
 */
static void
ttyinput(register unsigned c, register struct tty *tp)
{
	register int iflag = tp->t_iflag;
	register int lflag = tp->t_lflag;
	register u_char *cc = tp->t_cc;
	int i, err;

	/*
	 * If input is pending take it first.
	 */
	if (lflag&PENDIN)
		ttypend(tp);
	/*
	 * Gather stats.
	 */
	tk_nin++;
	if (lflag&ICANON) {
		tk_cancc++;
		tp->t_cancc++;
	} else {
		tk_rawcc++;
		tp->t_rawcc++;
	}
	/*
	 * Handle exceptional conditions (break, parity, framing).
	 */
	if (err = (c&TTY_ERRORMASK)) {
		c &= ~TTY_ERRORMASK;
		if (err&TTY_FE && !c) {		/* break */
			if (iflag&IGNBRK)
				goto endcase;
			else if (iflag&BRKINT && lflag&ISIG && 
				(cc[VINTR] != _POSIX_VDISABLE))
				c = cc[VINTR];
			else if (iflag&PARMRK)
				goto parmrk;
		} else if ((err&TTY_PE && iflag&INPCK) || err&TTY_FE) {
			if (iflag&IGNPAR)
				goto endcase;
			else if (iflag&PARMRK) {
parmrk:
				putc(0377|TTY_QUOTE, &tp->t_raw);
				putc(0|TTY_QUOTE, &tp->t_raw);
				putc(c|TTY_QUOTE, &tp->t_raw);
				goto endcase;
			} else
				c = 0;
		}
	}
	/*
	 * In tandem mode, check high water mark.
	 */
	if (iflag&IXOFF)
		ttyblock(tp);
	if ((tp->t_state&TS_TYPEN) == 0 && (iflag&ISTRIP))
		c &= ~0x80;
	if ((tp->t_lflag&EXTPROC) == 0) {
		/*
		 * Check for literal nexting very first
		 */
		if (tp->t_state&TS_LNCH) {
			c |= TTY_QUOTE;
			tp->t_state &= ~TS_LNCH;
		}
		/*
		 * Scan for special characters.  This code
		 * is really just a big case statement with
		 * non-constant cases.  The bottom of the
		 * case statement is labeled ``endcase'', so goto
		 * it after a case match, or similar.
		 */

		/*
		 * Control chars which aren't controlled
		 * by ICANON, ISIG, or IXON.
		 */
		if (lflag&IEXTEN) {
			if (CCEQ(cc[VLNEXT], c)) {
				if (lflag&ECHO) {
					if (lflag&ECHOE)
						ttyoutstr("^\b", tp);
					else
						ttyecho(c, tp);
				}
				tp->t_state |= TS_LNCH;
				goto endcase;
			}
			if (CCEQ(cc[VDISCARD], c)) {
				if (lflag&FLUSHO)
					tp->t_lflag &= ~FLUSHO;
				else {
					ttyflush(tp, FWRITE);
					ttyecho(c, tp);
					if (RB_LEN(&tp->t_raw) + RB_LEN(&tp->t_can))
						ttyretype(tp);
					tp->t_lflag |= FLUSHO;
				}
				goto startoutput;
			}
		}
		/*
		 * Signals.
		 */
		if (lflag&ISIG) {
			if (CCEQ(cc[VINTR], c) || CCEQ(cc[VQUIT], c)) {
				if ((lflag&NOFLSH) == 0)
					ttyflush(tp, FREAD|FWRITE);
				ttyecho(c, tp);
				pgsignal(tp->t_pgrp,
				    CCEQ(cc[VINTR], c) ? SIGINT : SIGQUIT, 1);
				goto endcase;
			}
			if (CCEQ(cc[VSUSP], c)) {
				if ((lflag&NOFLSH) == 0)
					ttyflush(tp, FREAD);
				ttyecho(c, tp);
				pgsignal(tp->t_pgrp, SIGTSTP, 1);
				goto endcase;
			}
		}
		/*
		 * Handle start/stop characters.
		 */
		if (iflag&IXON) {
			if (CCEQ(cc[VSTOP], c)) {
				if ((tp->t_state&TS_TTSTOP) == 0) {
					tp->t_state |= TS_TTSTOP;
					if (tp->t_stop)
						(*tp->t_stop)(tp, 0);
					/*(*cdevsw[major(tp->t_dev)].d_stop)(tp,
					   0);*/
					return;
				}
				if (!CCEQ(cc[VSTART], c))
					return;
				/* 
				 * if VSTART == VSTOP then toggle 
				 */
				goto endcase;
			}
			if (CCEQ(cc[VSTART], c))
				goto restartoutput;
		}
		/*
		 * IGNCR, ICRNL, & INLCR
		 */
		if (c == '\r') {
			if (iflag&IGNCR)
				goto endcase;
			else if (iflag&ICRNL)
				c = '\n';
		} else if (c == '\n' && iflag&INLCR)
			c = '\r';
	}
	if ((tp->t_lflag&EXTPROC) == 0 && lflag&ICANON) {
		/*
		 * From here on down canonical mode character
		 * processing takes place.
		 */
		/*
		 * erase (^H / ^?)
		 */
		if (CCEQ(cc[VERASE], c)) {
			if (RB_LEN(&tp->t_raw))
				ttyrub(unputc(&tp->t_raw), tp);
			goto endcase;
		}
		/*
		 * kill (^U)
		 */
		if (CCEQ(cc[VKILL], c)) {
			if (lflag&ECHOKE && RB_LEN(&tp->t_raw) == tp->t_rocount &&
			    (lflag&ECHOPRT) == 0) {
				while (RB_LEN(&tp->t_raw))
					ttyrub(unputc(&tp->t_raw), tp);
			} else {
				ttyecho(c, tp);
				if (lflag&ECHOK || lflag&ECHOKE)
					ttyecho('\n', tp);
				while (getc(&tp->t_raw) > 0)
					;
				tp->t_rocount = 0;
			}
			tp->t_state &= ~TS_LOCAL;
			goto endcase;
		}
		/*
		 * word erase (^W)
		 */
		if (CCEQ(cc[VWERASE], c)) {	
			int ctype;
			int alt = lflag&ALTWERASE;

			/* 
			 * erase whitespace 
			 */
			while ((c = unputc(&tp->t_raw)) == ' ' || c == '\t')
				ttyrub(c, tp);
			if (c == -1)
				goto endcase;
			/*
			 * erase last char of word and remember the
			 * next chars type (for ALTWERASE)
			 */
			ttyrub(c, tp);
			c = unputc(&tp->t_raw);
			if (c == -1)
				goto endcase;
			ctype = ISALPHA(c);
			/*
			 * erase rest of word
			 */
			do {
				ttyrub(c, tp);
				c = unputc(&tp->t_raw);
				if (c == -1)
					goto endcase;
			} while (c != ' ' && c != '\t' && 
				(alt == 0 || ISALPHA(c) == ctype));
			(void) putc(c, &tp->t_raw);
			goto endcase;
		}
		/*
		 * reprint line (^R)
		 */
		if (CCEQ(cc[VREPRINT], c)) {
			ttyretype(tp);
			goto endcase;
		}
		/*
		 * ^T - kernel info and generate SIGINFO
		 */
		if (CCEQ(cc[VSTATUS], c)) {
			pgsignal(tp->t_pgrp, SIGINFO, 1);
			if ((lflag&NOKERNINFO) == 0)
				ttyinfo(tp);
			goto endcase;
		}
	}
	/*
	 * Check for input buffer overflow
	 */
	if (RB_LEN(&tp->t_raw)+RB_LEN(&tp->t_can) >= TTYHOG) {
		if (iflag&IMAXBEL) {
			if (RB_LEN(&tp->t_out) < tp->t_hiwat)
				(void) ttyoutput(CTRL('g'), tp);
		} else
			ttyflush(tp, FREAD | FWRITE);
		goto endcase;
	}
	/*
	 * Put data char in q for user and
	 * wakeup on seeing a line delimiter.
	 */
	if (putc(c, &tp->t_raw) >= 0) {
		if ((lflag&ICANON) == 0) {
			ttwakeup(tp);
			ttyecho(c, tp);
			goto endcase;
		}
		if (ttbreakc(c)) {
			tp->t_rocount = 0;
			catb(&tp->t_raw, &tp->t_can);
			ttwakeup(tp);
		} else if (tp->t_rocount++ == 0)
			tp->t_rocol = tp->t_col;
		if (tp->t_state&TS_ERASE) {
			/*
			 * end of prterase \.../
			 */
			tp->t_state &= ~TS_ERASE;
			(void) ttyoutput('/', tp);
		}
		i = tp->t_col;
		ttyecho(c, tp);
		if (CCEQ(cc[VEOF], c) && lflag&ECHO) {
			/*
			 * Place the cursor over the '^' of the ^D.
			 */
			i = min(2, tp->t_col - i);
			while (i > 0) {
				(void) ttyoutput('\b', tp);
				i--;
			}
		}
	}
endcase:
	/*
	 * IXANY means allow any character to restart output.
	 */
	if ((tp->t_state&TS_TTSTOP) && (iflag&IXANY) == 0 && 
	    cc[VSTART] != cc[VSTOP])
		return;
restartoutput:
	tp->t_state &= ~TS_TTSTOP;
	tp->t_lflag &= ~FLUSHO;
startoutput:
	ttstart(tp);
}

/*
 * Output a single character on a tty, doing output processing
 * as needed (expanding tabs, newline processing, etc.).
 * Returns < 0 if putc succeeds, otherwise returns char to resend.
 * Must be recursive.
 */
static int
ttyoutput(unsigned c, struct tty *tp)
{
	register int col;
	register long oflag = tp->t_oflag;
	
	if ((oflag&OPOST) == 0) {
		if (tp->t_lflag&FLUSHO) 
			return (-1);
		if (putc(c, &tp->t_out))
			return (c);
		tk_nout++;
		tp->t_outcc++;
		return (-1);
	}
	c &= TTY_CHARMASK;
	/*
	 * Do tab expansion if OXTABS is set.
	 * Special case if we have external processing, we don't
	 * do the tab expansion because we'll probably get it
	 * wrong.  If tab expansion needs to be done, let it
	 * happen externally.
	 */
	if (c == '\t' && oflag&OXTABS && (tp->t_lflag&EXTPROC) == 0) {
		register int s;

		c = 8 - (tp->t_col&7);
		if ((tp->t_lflag&FLUSHO) == 0) {
			int i;

			s = spltty();		/* don't interrupt tabs */
#ifdef was
			c -= b_to_q("        ", c, &tp->t_outq);
#else
			i = min (c, RB_CONTIGPUT(&tp->t_out));
			(void) memcpy(tp->t_out.rb_tl, "        ", i);
			tp->t_out.rb_tl =
				RB_ROLLOVER(&tp->t_out, tp->t_out.rb_tl+i);
			i = min (c-i, RB_CONTIGPUT(&tp->t_out));

			/* off end and still have space? */
			if (i) {
				(void) memcpy(tp->t_out.rb_tl, "        ", i);
				tp->t_out.rb_tl =
				   RB_ROLLOVER(&tp->t_out, tp->t_out.rb_tl+i);
			}
#endif
			tk_nout += c;
			tp->t_outcc += c;
			splx(s);
		}
		tp->t_col += c;
		return (c ? -1 : '\t');
	}
	if (c == CEOT && oflag&ONOEOT)
		return (-1);
	tk_nout++;
	tp->t_outcc++;
	/*
	 * Newline translation: if ONLCR is set,
	 * translate newline into "\r\n".
	 */
	if (c == '\n' && (tp->t_oflag&ONLCR) && ttyoutput('\r', tp) >= 0)
		return (c);
	if ((tp->t_lflag&FLUSHO) == 0 && putc(c, &tp->t_out))
		return (c);

	col = tp->t_col;
	switch (CCLASS(c)) {

	case ORDINARY:
		col++;

	case CONTROL:
		break;

	case BACKSPACE:
		if (col > 0)
			col--;
		break;

	case NEWLINE:
		col = 0;
		break;

	case TAB:
		col = (col + 8) &~ 0x7;
		break;

	case RETURN:
		col = 0;
	}
	tp->t_col = col;
	return (-1);
}

/* Process a read call on a tty device. */
static int
ttread(struct tty *tp, struct uio *uio, int flag)
{
	struct ringb *qp;
	int c;
	long lflag;
	u_char *cc = tp->t_cc;
	struct proc *p = uio->uio_procp;
	int s, first, error = 0;

loop:
	lflag = tp->t_lflag;
	s = spltty();
	/*
	 * take pending input first 
	 */
	if (lflag&PENDIN)
		ttypend(tp);
	splx(s);

	/*
	 * Hang process if it's in the background.
	 */
	if (isbackground(p, tp)) {
		if ((p->p_sigignore & sigmask(SIGTTIN)) ||
		   (p->p_sigmask & sigmask(SIGTTIN)) ||
		    p->p_flag&SPPWAIT || p->p_pgrp->pg_jobc == 0)
			return (EIO);
		pgsignal(p->p_pgrp, SIGTTIN, 1);
		if (error = ttysleep(tp, (caddr_t)&lbolt, TTIPRI | PCATCH, 
		    ttybg, 0)) 
			return (error);
		goto loop;
	}

	/*
	 * If canonical, use the canonical queue,
	 * else use the raw queue.
	 */
	qp = lflag&ICANON ? &tp->t_can : &tp->t_raw;

	/*
	 * If there is no input, sleep on rawq
	 * awaiting hardware receipt and notification.
	 * If we have data, we don't need to check for carrier.
	 */
	s = spltty();
	if (RB_LEN(qp) <= 0) {
		int carrier;

		carrier = (tp->t_state&TS_CARR_ON) || (tp->t_cflag&CLOCAL);
		if (!carrier && tp->t_state&TS_ISOPEN) {
			splx(s);
			return (0);	/* EOF */
		}
		if (flag & IO_NDELAY) {
			splx(s);
			return (EWOULDBLOCK);
		}
		error = ttysleep(tp, (caddr_t)&tp->t_raw, TTIPRI | PCATCH,
		    carrier ? ttyin : ttopen, 0);
		splx(s);
		if (error)
			return (error);
		goto loop;
	}
	splx(s);

	/*
	 * Input present, check for input mapping and processing.
	 */
	first = 1;
	while ((c = getc(qp)) >= 0) {
		/*
		 * delayed suspend (^Y)
		 */
		if (CCEQ(cc[VDSUSP], c) && lflag&ISIG) {
			pgsignal(tp->t_pgrp, SIGTSTP, 1);
			if (first) {
				if (error = ttysleep(tp, (caddr_t)&lbolt,
				    TTIPRI | PCATCH, ttybg, 0))
					break;
				goto loop;
			}
			break;
		}
		/*
		 * Interpret EOF only in canonical mode.
		 */
		if (CCEQ(cc[VEOF], c) && lflag&ICANON)
			break;
		/*
		 * Give user character.
		 */
		error = copyout_(p, &c, uio_base(uio), 1);
		(void)uio_advance(uio, uio->uio_iov, 1);
 		/* error = ureadc(c, uio); */
		if (error)
			break;
 		if (uio->uio_resid == 0)
			break;
		/*
		 * In canonical mode check for a "break character"
		 * marking the end of a "line of input".
		 */
		if (lflag&ICANON && ttbreakc(c))
			break;
		first = 0;
	}
	/*
	 * Look to unblock output now that (presumably)
	 * the input queue has gone down.
	 */
	if (tp->t_state&TS_TBLOCK && RB_LEN(&tp->t_raw) < TTYHOG/5) {
		if (cc[VSTART] != _POSIX_VDISABLE &&
		    putc(cc[VSTART], &tp->t_out) == 0) {
			tp->t_state &= ~TS_TBLOCK;
			ttstart(tp);
		}
	}
	return (error);
}

/*
 * Handle modem control transition on a tty.
 * Flag indicates new state of carrier.
 * Returns 0 if the line should be turned off, otherwise 1.
 */
static int
ttymodem(register struct tty *tp, int flag)
{

	if ((tp->t_state&TS_WOPEN) == 0 && (tp->t_lflag&MDMBUF)) {

		/* MDMBUF: do flow control according to carrier flag */
		if (flag) {
			tp->t_state &= ~TS_TTSTOP;
			ttstart(tp);
		} else if ((tp->t_state&TS_TTSTOP) == 0) {
			tp->t_state |= TS_TTSTOP;
			if (tp->t_stop)
				(*tp->t_stop)(tp, 0);
			/*(*cdevsw[major(tp->t_dev)].d_stop)(tp, 0);*/
		}
	} else if (flag == 0) {
		/*
		 * Lost carrier.
		 */
		tp->t_state &= ~TS_CARR_ON;
		if (tp->t_state&TS_ISOPEN && (tp->t_cflag&CLOCAL) == 0) {
			if (tp->t_session && tp->t_session->s_leader)
				psignal(tp->t_session->s_leader, SIGHUP);
			ttyflush(tp, FREAD|FWRITE);
			return (0);
		}
	} else {
		/*
		 * Carrier now on.
		 */
		tp->t_state |= TS_CARR_ON;
		ttwakeup(tp);
	}
	return (1);
}

int ldiscif_config(char **cfg, struct ldiscif *lif);
static struct ldiscif tty_ldiscif =
{
	{0}, -1, 0, 0, 0,
	ttyopen, ttylclose, ttread, ttwrite, nullioctl, ttyinput,
	ttyoutput, ttstart, ttymodem,
};

LDISC_MODCONFIG() {
	char *cfg_string = tty_config;
	
	if (ldiscif_config(&cfg_string, &tty_ldiscif) == 0)
		return;
}
