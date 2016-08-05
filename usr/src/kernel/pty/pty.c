/*
 * Copyright (c) 1982, 1986, 1989 The Regents of the University of California.
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
 *	$Id: pty.c,v 1.9 94/07/27 14:02:02 bill Exp $
 */

/*
 * Pseudo-teletype Driver
 * (Actually two drivers, requiring two character device interfaces.)
 */
static char *pty_config =
	"pty 5 6	8.		# pseudo tty driver $Revision: 1.9 $";

#include "sys/param.h"
#include "sys/file.h"
#include "sys/ioctl.h"
#include "sys/errno.h"

#include "systm.h"
#include "tty.h"
#include "uio.h"
#include "proc.h"
#include "kernel.h"
#include "malloc.h"
#include "modconfig.h"

#include "vnode.h"

#include "prototypes.h"


#define BUFSIZ 100		/* Chunk size iomoved to/from user */

/*
 * pts == /dev/tty[pqrs]?
 * ptc == /dev/pty[pqrs]?
 */
static struct	tty *pt_tty;
static struct	pt_ioctl {
	int	pt_flags;
	pid_t	pt_selr, pt_selw;
	u_char	pt_send;
	u_char	pt_ucntl;
} *pt_ioctl;
static int	npty;

extern struct tty *constty;		/* temporary virtual console */

#define	PF_RCOLL	0x01
#define	PF_WCOLL	0x02
#define	PF_PKT		0x08		/* packet mode */
#define	PF_STOPPED	0x10		/* user told stopped */
#define	PF_REMOTE	0x20		/* remote and flow controlled input */
#define	PF_NOSTOP	0x40
#define PF_UCNTL	0x80		/* user control mode */

/*ARGSUSED*/
int
ptsopen(dev_t dev, int flag, int devtype, struct proc *p)
{
	register struct tty *tp;
	int error;

	if (minor(dev) >= npty)
		return (ENXIO);

	tp = &pt_tty[minor(dev)];
	if ((tp->t_state & TS_ISOPEN) == 0) {
		tp->t_state |= TS_WOPEN;
		ttychars(tp);		/* Set up default chars */
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_cflag = TTYDEF_CFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
		ttsetwater(tp);		/* would be done in xxparam() */
	} else if (tp->t_state&TS_XCLUDE && p->p_ucred->cr_uid != 0)
		return (EBUSY);

	if (tp->t_oproc)			/* Ctrlr still around. */
		tp->t_state |= TS_CARR_ON;
	tp->t_stop = 0;

	while ((tp->t_state & TS_CARR_ON) == 0) {
		tp->t_state |= TS_WOPEN;
		if (flag&FNONBLOCK)
			break;
		if (error = ttysleep(tp, (caddr_t)&tp->t_raw, TTIPRI | PCATCH,
		    ttopen, 0))
			return (error);
	}

	error = ldiscif_open(dev, tp, flag);
	ptcwakeup(tp, FREAD|FWRITE);
	return (error);
}

int
ptsclose(dev_t dev, int flag, int mode, struct proc *p)
{
	struct tty *tp = &pt_tty[minor(dev)];

	ldiscif_close(tp, flag);
	ttyclose(tp);
	ptcwakeup(tp, FREAD|FWRITE);
	return (0);
}

int
ptsread(dev_t dev, struct uio *uio, int flag)
{
	struct proc *p = curproc;
	register struct tty *tp = &pt_tty[minor(dev)];
	register struct pt_ioctl *pti = &pt_ioctl[minor(dev)];
	int error = 0;

again:
	if (pti->pt_flags & PF_REMOTE) {

		while (isbackground(p, tp)) {
			if ((p->p_sigignore & sigmask(SIGTTIN)) ||
			    (p->p_sigmask & sigmask(SIGTTIN)) ||
			    p->p_pgrp->pg_jobc == 0 ||
			    p->p_flag&SPPWAIT)
				return (EIO);
			pgsignal(p->p_pgrp, SIGTTIN, 1);
			if (error = ttysleep(tp, (caddr_t)&lbolt, 
			    TTIPRI | PCATCH, ttybg, 0))
				return (error);
		}

		if (RB_LEN(&tp->t_can) == 0) {
			if (flag & IO_NDELAY)
				return (EWOULDBLOCK);
			if (error = ttysleep(tp, (caddr_t)&tp->t_can,
			    TTIPRI | PCATCH, ttyin, 0))
				return (error);
			goto again;
		}

		while (RB_LEN(&tp->t_can) > 1 && uio->uio_resid > 0)
		{
			unsigned ch = getc(&tp->t_can);

			if (error = copyout_(uio->uio_procp, &ch, uio_base(uio), 1))
				break;
			/* if (ureadc(getc(&tp->t_can), uio) < 0) {
				error = EFAULT;
				break;
			}*/
			else
				(void)uio_advance(uio, uio->uio_iov, 1);
		}

		if (RB_LEN(&tp->t_can) == 1)
			(void) getc(&tp->t_can);

		if (RB_LEN(&tp->t_can))
			return (error);
	} else
		if (tp->t_oproc)
			error = ldiscif_read(tp, uio, flag);

	ptcwakeup(tp, FWRITE);
	return (error);
}

/*
 * Write to pseudo-tty.
 * Wakeups of controlling tty will happen
 * indirectly, when tty driver calls ptsstart.
 */
int
ptswrite(dev_t dev, struct uio *uio, int flag)
{
	struct tty *tp = &pt_tty[minor(dev)];

	if (tp->t_oproc == 0)
		return (EIO);

	return (ldiscif_write(tp, uio, flag));
}

/*
 * Start output on pseudo-tty.
 * Wake up process selecting or sleeping for input from controlling tty.
 */
ptsstart(struct tty *tp)
{
	register struct pt_ioctl *pti = &pt_ioctl[minor(tp->t_dev)];

	if (tp->t_state & TS_TTSTOP)
		return;

	if (pti->pt_flags & PF_STOPPED) {
		pti->pt_flags &= ~PF_STOPPED;
		pti->pt_send = TIOCPKT_START;
	}

	ptcwakeup(tp, FREAD);
}

int
ptsselect(dev_t dev, int rw, struct proc *p) {

	return (ttselect(pt_tty + minor(dev), rw, p));
}

ptcwakeup(struct tty *tp, int flag)
{
	struct pt_ioctl *pti = &pt_ioctl[minor(tp->t_dev)];

	if (flag & FREAD) {
		if (pti->pt_selr) {
			selwakeup(pti->pt_selr, pti->pt_flags & PF_RCOLL);
			pti->pt_selr = 0;
			pti->pt_flags &= ~PF_RCOLL;
		}
		wakeup((caddr_t)&tp->t_out.rb_hd);
	}

	if (flag & FWRITE) {
		if (pti->pt_selw) {
			selwakeup(pti->pt_selw, pti->pt_flags & PF_WCOLL);
			pti->pt_selw = 0;
			pti->pt_flags &= ~PF_WCOLL;
		}
		wakeup((caddr_t)&tp->t_raw.rb_hd);
	}
}

int
ptcopen(dev_t dev, int flag, int devtype, struct proc *p)
{
	register struct tty *tp;
	struct pt_ioctl *pti;

	if (minor(dev) >= npty)
		return (ENXIO);

	tp = &pt_tty[minor(dev)];
	if (tp->t_oproc)
		return (EIO);

	tp->t_oproc = ptsstart;
	tp->t_stop = 0;
	(void) ldiscif_modem(tp, 1);
	tp->t_lflag &= ~EXTPROC;

	pti = &pt_ioctl[minor(dev)];
	pti->pt_flags = 0;
	pti->pt_send = 0;
	pti->pt_ucntl = 0;
	return (0);
}

int
ptcclose(dev_t dev, int flag, int mode, struct proc *p)
{
	struct tty *tp = &pt_tty[minor(dev)];

	(void) ldiscif_modem(tp, 0);
	
	tp->t_state &= ~TS_CARR_ON;
	tp->t_oproc = 0;		/* mark closed */
	tp->t_session = 0;

	if (constty == tp)
		constty = NULL;

	return (0);
}

int
ptcread(dev_t dev, struct uio *uio, int flag)
{
	struct tty *tp = &pt_tty[minor(dev)];
	struct pt_ioctl *pti = &pt_ioctl[minor(dev)];
	char buf[BUFSIZ];
	int error = 0, cc;

	/*
	 * We want to block until the slave
	 * is open, and there's something to read;
	 * but if we lost the slave or we're NBIO,
	 * then return the appropriate error instead.
	 */
	for (;;) {
		if (tp->t_state&TS_ISOPEN) {

			if (pti->pt_flags&PF_PKT && pti->pt_send) {
				if (error = copyout_(uio->uio_procp, (caddr_t)&pti->pt_send,
					uio_base(uio), 1))
				/* error = ureadc((int)pti->pt_send, uio);
				if (error) */
					return (error);
				else
					(void)uio_advance(uio, uio->uio_iov, 1);
				if (pti->pt_send & TIOCPKT_IOCTL) {
					cc = min(uio->uio_resid,
						sizeof(tp->t_termios));
					uiomove(&tp->t_termios, cc, uio);
				}
				pti->pt_send = 0;
				return (0);
			}

			if (pti->pt_flags&PF_UCNTL && pti->pt_ucntl) {
				if (error = copyout_(uio->uio_procp, (caddr_t)&pti->pt_send,
					uio_base(uio), 1))
				/*error = ureadc((int)pti->pt_ucntl, uio);
				if (error)*/
					return (error);
				else
					(void)uio_advance(uio, uio->uio_iov, 1);
				pti->pt_ucntl = 0;
				return (0);
			}

			if (RB_LEN(&tp->t_out) && (tp->t_state&TS_TTSTOP) == 0)
				break;
		}

		if ((tp->t_state&TS_CARR_ON) == 0)
			return (0);	/* EOF */

		if (flag & IO_NDELAY)
			return (EWOULDBLOCK);

		if (error = tsleep((caddr_t)&tp->t_out.rb_hd, TTIPRI | PCATCH,
		    ttyin, 0))
			return (error);

	}

	if (pti->pt_flags & (PF_PKT|PF_UCNTL))
	/*	error = ureadc(0, uio);*/
	{
		int zero = 0;
		if ((error = copyout_(uio->uio_procp, &zero, uio_base(uio), 1)) == 0)
			(void)uio_advance(uio, uio->uio_iov, 1);
	}

	while (uio->uio_resid > 0 && error == 0) {
		cc = min(min(uio->uio_resid, BUFSIZ), RB_CONTIGGET(&tp->t_out));
		if (cc) {
			(void) memcpy(buf, tp->t_out.rb_hd, cc);
			tp->t_out.rb_hd =
				RB_ROLLOVER(&tp->t_out, tp->t_out.rb_hd+cc);
		}
		if (cc <= 0)
			break;
		error = uiomove(buf, cc, uio);
	}

	if (RB_LEN(&tp->t_out) <= tp->t_lowat) {
		if (tp->t_state&TS_ASLEEP) {
			tp->t_state &= ~TS_ASLEEP;
			wakeup((caddr_t)&tp->t_out);
		}
		if (tp->t_wsel) {
			selwakeup(tp->t_wsel, tp->t_state & TS_WCOLL);
			tp->t_wsel = 0;
			tp->t_state &= ~TS_WCOLL;
		}
	}

	return (error);
}

ptsstop(struct tty *tp, int flush)
{
	struct pt_ioctl *pti = &pt_ioctl[minor(tp->t_dev)];
	int flag;

	/* note: FLUSHREAD and FLUSHWRITE already ok */
	if (flush == 0) {
		flush = TIOCPKT_STOP;
		pti->pt_flags |= PF_STOPPED;
	} else
		pti->pt_flags &= ~PF_STOPPED;
	pti->pt_send |= flush;

	/* change of perspective */
	flag = 0;
	if (flush & FREAD)
		flag |= FWRITE;
	if (flush & FWRITE)
		flag |= FREAD;
	ptcwakeup(tp, flag);
}

int
ptcselect(dev_t dev, int rw, struct proc *p)
{
	register struct tty *tp = &pt_tty[minor(dev)];
	struct pt_ioctl *pti = &pt_ioctl[minor(dev)];
	struct proc *prev;
	int s;

	if ((tp->t_state&TS_CARR_ON) == 0)
		return (1);

	switch (rw) {

	case FREAD:
		/*
		 * Need to block timeouts (ttrstart).
		 */
		s = spltty();
		if ((tp->t_state&TS_ISOPEN) &&
		     RB_LEN(&tp->t_out) && (tp->t_state&TS_TTSTOP) == 0) {
			splx(s);
			return (1);
		}
		splx(s);
		/* FALLTHROUGH */

	case 0:					/* exceptional */
		if ((tp->t_state&TS_ISOPEN) &&
		    (pti->pt_flags&PF_PKT && pti->pt_send ||
		     pti->pt_flags&PF_UCNTL && pti->pt_ucntl))
			return (1);
		if (pti->pt_selr && (prev = pfind(pti->pt_selr)) && prev->p_wchan == (caddr_t)&selwait)
			pti->pt_flags |= PF_RCOLL;
		else
			pti->pt_selr = p->p_pid;
		break;


	case FWRITE:
		if (tp->t_state&TS_ISOPEN) {
			if (pti->pt_flags & PF_REMOTE) {
			    if (RB_LEN(&tp->t_can) == 0)
				return (1);
			} else {
			    if (RB_LEN(&tp->t_raw) + RB_LEN(&tp->t_can) < TTYHOG-2)
				    return (1);
			    if (RB_LEN(&tp->t_can) == 0 && (tp->t_iflag&ICANON))
				    return (1);
			}
		}
		if (pti->pt_selw && (prev = pfind(pti->pt_selw)) && prev->p_wchan == (caddr_t)&selwait)
			pti->pt_flags |= PF_WCOLL;
		else
			pti->pt_selw = p->p_pid;
		break;

	}
	return (0);
}

int
ptcwrite(dev_t dev, struct uio *uio, int flag)
{
	register struct tty *tp = &pt_tty[minor(dev)];
	register u_char *cp;
	register int cc = 0;
	u_char locbuf[BUFSIZ];
	int cnt = 0;
	struct pt_ioctl *pti = &pt_ioctl[minor(dev)];
	int error = 0;

again:

	if ((tp->t_state&TS_ISOPEN) == 0)
		goto block;

	if (pti->pt_flags & PF_REMOTE) {

		if (RB_LEN(&tp->t_can))
			goto block;

		while (uio->uio_resid > 0 && RB_LEN(&tp->t_can) < TTYHOG - 1) {
			if (cc == 0) {
				cc = min(uio->uio_resid, BUFSIZ);
				cc = min(cc, TTYHOG - 1 - RB_CONTIGPUT(&tp->t_can));
				cp = locbuf;
				error = uiomove((caddr_t)cp, cc, uio);
				if (error)
					return (error);
				/* check again for safety */
				if ((tp->t_state&TS_ISOPEN) == 0)
					return (EIO);
			}

			if (cc) {
				(void) memcpy(tp->t_can.rb_tl, cp, cc);
				tp->t_can.rb_tl =
				  RB_ROLLOVER(&tp->t_can, tp->t_can.rb_tl+cc);
			}
			cc = 0;
		}

		(void) putc(0, &tp->t_can);
		ttwakeup(tp);
		wakeup((caddr_t)&tp->t_can);
		return (0);
	}

	while (uio->uio_resid > 0) {

		if (cc == 0) {
			cc = min(uio->uio_resid, BUFSIZ);
			cp = locbuf;
			error = uiomove((caddr_t)cp, cc, uio);
			if (error)
				return (error);
			/* check again for safety */
			if ((tp->t_state&TS_ISOPEN) == 0)
				return (EIO);
		}

		while (cc > 0) {
			if ((RB_LEN(&tp->t_raw) + RB_LEN(&tp->t_can)) >= TTYHOG - 2 &&
			   (RB_LEN(&tp->t_can) > 0 || !(tp->t_iflag&ICANON))) {
				wakeup((caddr_t)&tp->t_raw);
				goto block;
			}
			ldiscif_rint(*cp++, tp);
			cnt++;
			cc--;
		}
		cc = 0;
	}

	return (0);

block:

	/*
	 * Come here to wait for slave to open, for space
	 * in outq, or space in rawq.
	 */
	if ((tp->t_state&TS_CARR_ON) == 0)
		return (EIO);

	if (flag & IO_NDELAY) {
		/* adjust for data copied in but not written */
		uio->uio_resid += cc;
		if (cnt == 0)
			return (EWOULDBLOCK);
		return (0);
	}

	if (error = tsleep((caddr_t)&tp->t_raw.rb_hd, TTOPRI | PCATCH,
	    ttyout, 0)) {
		/* adjust for data copied in but not written */
		uio->uio_resid += cc;
		return (error);
	}

	goto again;
}

/*ARGSUSED*/
int
ptyioctl(dev_t dev, int cmd, caddr_t data, int flag, struct proc *p)
{
	register struct tty *tp = &pt_tty[minor(dev)];
	register struct pt_ioctl *pti = &pt_ioctl[minor(dev)];
	register u_char *cc = tp->t_cc;
	int stop, error;
	extern struct devif ptc_devif;

	/*
	 * IF CONTROLLER STTY THEN MUST FLUSH TO PREVENT A HANG.
	 * ttywflush(tp) will hang if there are characters in the outq.
	 */
	if (cmd == TIOCEXT) {

		/*
		 * When the EXTPROC bit is being toggled, we need
		 * to send an TIOCPKT_IOCTL if the packet driver
		 * is turned on.
		 */
		if (*(int *)data) {
			if (pti->pt_flags & PF_PKT) {
				pti->pt_send |= TIOCPKT_IOCTL;
				ptcwakeup(tp, FREAD);
			}
			tp->t_lflag |= EXTPROC;
		} else {
			if ((tp->t_state & EXTPROC) &&
			    (pti->pt_flags & PF_PKT)) {
				pti->pt_send |= TIOCPKT_IOCTL;
				ptcwakeup(tp, FREAD);
			}
			tp->t_lflag &= ~EXTPROC;
		}

		return(0);
	}

	else if (major(dev) == ptc_devif.di_cmajor)
		switch (cmd) {

		case TIOCGPGRP:
			/*
			 * We aviod calling ttioctl on the controller since,
			 * in that case, tp must be the controlling terminal.
			 */
			*(int *)data = tp->t_pgrp ? tp->t_pgrp->pg_id : 0;
			return (0);

		case TIOCPKT:
			if (*(int *)data) {
				if (pti->pt_flags & PF_UCNTL)
					return (EINVAL);
				pti->pt_flags |= PF_PKT;
			} else
				pti->pt_flags &= ~PF_PKT;
			return (0);

		case TIOCUCNTL:
			if (*(int *)data) {
				if (pti->pt_flags & PF_PKT)
					return (EINVAL);
				pti->pt_flags |= PF_UCNTL;
			} else
				pti->pt_flags &= ~PF_UCNTL;
			return (0);

		case TIOCREMOTE:
			if (*(int *)data)
				pti->pt_flags |= PF_REMOTE;
			else
				pti->pt_flags &= ~PF_REMOTE;
			ttyflush(tp, FREAD|FWRITE);
			return (0);

#ifdef	COMPAT_43
		case TIOCSETP:		
		case TIOCSETN:
#endif
		case TIOCSETD:
		case TIOCSETA:
		case TIOCSETAW:
		case TIOCSETAF:
			while (getc(&tp->t_out) >= 0)
				;
			break;

		case TIOCSIG:
			if (*(unsigned int *)data >= NSIG)
				return(EINVAL);
			if ((tp->t_lflag&NOFLSH) == 0)
				ttyflush(tp, FREAD|FWRITE);
			pgsignal(tp->t_pgrp, *(unsigned int *)data, 1);
			if ((*(unsigned int *)data == SIGINFO) &&
			    ((tp->t_lflag&NOKERNINFO) == 0))
				ttyinfo(tp);
			return(0);
		}

	/* XXX controller up before ldiscif_open called by slave XXX */
	if (tp->t_ldiscif == 0)
		return (ENOTTY);

	error = ldiscif_ioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);

	error = ttioctl(tp, cmd, data, flag, p);

	/*
	 * Since we use the tty queues internally,
	 * pty's can't be switched to disciplines which overwrite
	 * the queues.  We can't tell anything about the discipline
	 * from here...
	 */
#ifdef nope
	if (tp->t_line != TTYDISC) {
		ldiscif_close(tp, flag);
		tp->t_line = TTYDISC;
		(void)ldiscif_open(dev, tp, flag);
		error = ENOTTY;
	}
#endif

	if (error < 0) {
		if (pti->pt_flags & PF_UCNTL &&
		    (cmd & ~0xff) == UIOCCMD(0)) {
			if (cmd & 0xff) {
				pti->pt_ucntl = (u_char)cmd;
				ptcwakeup(tp, FREAD);
			}
			return (0);
		}
		error = ENOTTY;
	}
	/*
	 * If external processing and packet mode send ioctl packet.
	 */
	if ((tp->t_lflag&EXTPROC) && (pti->pt_flags & PF_PKT)) {

		switch(cmd) {
		case TIOCSETA:
		case TIOCSETAW:
		case TIOCSETAF:
#ifdef	COMPAT_43
		case TIOCSETP:
		case TIOCSETN:
		case TIOCSETC:
		case TIOCSLTC:
		case TIOCLBIS:
		case TIOCLBIC:
		case TIOCLSET:
#endif
			pti->pt_send |= TIOCPKT_IOCTL;
		default:
			break;
		}
	}

	stop = (tp->t_iflag & IXON) && CCEQ(cc[VSTOP], CTRL('s')) 
		&& CCEQ(cc[VSTART], CTRL('q'));

	if (pti->pt_flags & PF_NOSTOP) {
		if (stop) {
			pti->pt_send &= ~TIOCPKT_NOSTOP;
			pti->pt_send |= TIOCPKT_DOSTOP;
			pti->pt_flags &= ~PF_NOSTOP;
			ptcwakeup(tp, FREAD);
		}
	} else {
		if (!stop) {
			pti->pt_send &= ~TIOCPKT_DOSTOP;
			pti->pt_send |= TIOCPKT_NOSTOP;
			pti->pt_flags |= PF_NOSTOP;
			ptcwakeup(tp, FREAD);
		}
	}

	return (error);
}

static struct devif pts_devif =
{
	{"pts"}, -1, -2, -1, 0, 0, 0, 0, 0,
	ptsopen, ptsclose, ptyioctl, ptsread, ptswrite, ptsselect, 0,
	0,	0, 0, 0,
};

struct devif ptc_devif =
{
	{"ptc"}, -1, -2, -1, 0, 0, 0, 0, 0,
	ptcopen, ptcclose, ptyioctl, ptcread, ptcwrite, ptcselect, 0,
	0,	0, 0, 0,
};

DRIVER_MODCONFIG() {
	char *cfg_string;
	
	/* find configuration string. */
	if (!config_scan(pty_config, &cfg_string)) 
		return;

	/* configure driver into kernel program */
	if (devif_config(&cfg_string, &pts_devif) == 0 ||
	    devif_config(&cfg_string, &ptc_devif) == 0)
		return;

	/* allocate number of ptys */
	(void)cfg_number(&cfg_string, &npty);
	pt_ioctl = (struct pt_ioctl *)
	    malloc(sizeof(struct pt_ioctl) * npty, M_TEMP, M_NOWAIT);
	(void)memset((void *)pt_ioctl, 0, sizeof (struct pt_ioctl) * npty);
	pt_tty = (struct tty *)
	    malloc(sizeof(struct tty) * npty, M_TEMP, M_NOWAIT);
	(void)memset((void *)pt_tty, 0, sizeof (struct tty) * npty);
}
