/*
 * Copyright (c) 1982, 1986, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution is only permitted until one year after the first shipment
 * of 4.4BSD by the Regents.  Otherwise, redistribution and use in source and
 * binary forms are permitted provided that: (1) source distributions retain
 * this entire copyright notice and comment, and (2) distributions including
 * binaries display the following acknowledgement:  This product includes
 * software developed by the University of California, Berkeley and its
 * contributors'' in the documentation or other materials provided with the
 * distribution and in all advertising materials mentioning features or use
 * of this software.  Neither the name of the University nor the names of
 * its contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	@(#)com.c	7.7 (Berkeley) 6/30/90
 */

#include "com.h"
#if NCOM > 0
/*
 * COM driver, from
 *  98626/98644/internal serial interface
 */
#include "param.h"
#include "systm.h"
#include "ioctl.h"
#include "tty.h"
#include "user.h"
#include "conf.h"
#include "file.h"
#include "uio.h"
#include "kernel.h"
#include "syslog.h"

#include "machine/isa/isa_device.h"
#include "machine/isa/comreg.h"
#include "machine/isa/ic/ns16450.h"
#include "machine/isa/icu.h"

int 	comprobe(), comattach(), comintr(), comstart(), comparam();

struct	isa_driver comdriver = {
	comprobe, comattach, "com"
};

int	comsoftCAR;
int	com_active;
int	ncom = NCOM;
int	comconsole = -1;
int	comdefaultrate = TTYDEF_SPEED;
short com_addr[NCOM];
struct	tty com_tty[NCOM];

struct speedtab comspeedtab[] = {
	0,	0,
	50,	COMBRD(50),
	75,	COMBRD(75),
	110,	COMBRD(110),
	134,	COMBRD(134),
	150,	COMBRD(150),
	200,	COMBRD(200),
	300,	COMBRD(300),
	600,	COMBRD(600),
	1200,	COMBRD(1200),
	1800,	COMBRD(1800),
	2400,	COMBRD(2400),
	4800,	COMBRD(4800),
	9600,	COMBRD(9600),
	19200,	COMBRD(19200),
	38400,	COMBRD(38400),
	57600,	COMBRD(57600),
	-1,	-1
};

extern	struct tty *constty;
#ifdef KGDB
extern int kgdb_dev;
extern int kgdb_rate;
extern int kgdb_debug_init;
#endif

#define	UNIT(x)		minor(x)

comprobe(dev)
struct isa_device *dev;
{
	if ((inb(dev->id_iobase+com_iir) & 0xf8) == 0)
		return(1);
	return(0);

}


int
comattach(isdp)
struct isa_device *isdp;
{
	struct	tty	*tp;
	u_char		unit;
	int		port = isdp->id_iobase;

	if (unit == comconsole)
		DELAY(100000);
	unit = isdp->id_unit;
	com_addr[unit] = port;
	com_active |= 1 << unit;
	/* comsoftCAR = isdp->id_flags; */
	outb(port+com_ier, 0);
	outb(port+com_mcr, 0 | MCR_IENABLE);
#ifdef KGDB
	if (kgdb_dev == makedev(1, unit)) {
		if (comconsole == unit)
			kgdb_dev = -1;	/* can't debug over console port */
		else {
			(void) cominit(unit);
			comconsole = -2; /* XXX */
			if (kgdb_debug_init) {
				printf("com%d: kgdb waiting...", unit);
				/* trap into kgdb */
				asm("trap #15;");
				printf("connected.\n");
			} else
				printf("com%d: kgdb enabled\n", unit);
		}
	}
#endif
	/*
	 * Need to reset baud rate, etc. of next print so reset comconsole.
	 * Also make sure console is always "hardwired"
	 */
	if (unit == comconsole) {
		comconsole = -1;
		comsoftCAR |= (1 << unit);
	}
comsoftCAR |= (1 << unit);
	return (1);
}

comopen(dev, flag)
	dev_t dev;
{
	register struct tty *tp;
	register int unit;
	int error = 0;
 
	unit = UNIT(dev);
	if (unit >= NCOM || (com_active & (1 << unit)) == 0)
		return (ENXIO);
	tp = &com_tty[unit];
	tp->t_oproc = comstart;
	tp->t_param = comparam;
	tp->t_dev = dev;
	if ((tp->t_state & TS_ISOPEN) == 0) {
		tp->t_state |= TS_WOPEN;
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = TTYDEF_CFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = comdefaultrate;
		comparam(tp, &tp->t_termios);
		ttsetwater(tp);
	} else if (tp->t_state&TS_XCLUDE && u.u_uid != 0)
		return (EBUSY);
	(void) spltty();
	(void) commctl(dev, MCR_DTR | MCR_RTS, DMSET);
	if ((comsoftCAR & (1 << unit)) || (commctl(dev, 0, DMGET) & MSR_DCD))
		tp->t_state |= TS_CARR_ON;
	while ((flag&O_NONBLOCK) == 0 && (tp->t_cflag&CLOCAL) == 0 &&
	       (tp->t_state & TS_CARR_ON) == 0) {
		tp->t_state |= TS_WOPEN;
		if (error = ttysleep(tp, (caddr_t)&tp->t_raw, TTIPRI | PCATCH,
		    ttopen, 0))
			break;
	}
	(void) spl0();
	if (error == 0)
		error = (*linesw[tp->t_line].l_open)(dev, tp);
	return (error);
}
 
/*ARGSUSED*/
comclose(dev, flag)
	dev_t dev;
{
	register struct tty *tp;
	register com;
	register int unit;
 
	unit = UNIT(dev);
	com = com_addr[unit];
	tp = &com_tty[unit];
	(*linesw[tp->t_line].l_close)(tp);
	outb(com+com_cfcr, inb(com+com_cfcr) & ~CFCR_SBREAK);
#ifdef KGDB
	/* do not disable interrupts if debugging */
	if (kgdb_dev != makedev(1, unit))
#endif
	outb(com+com_ier, 0);
	if (tp->t_cflag&HUPCL || tp->t_state&TS_WOPEN || 
	    (tp->t_state&TS_ISOPEN) == 0)
		(void) commctl(dev, 0, DMSET);
	ttyclose(tp);
	return(0);
}
 
comread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
{
	register struct tty *tp = &com_tty[UNIT(dev)];
 
	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}
 
comwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
{
	int unit = UNIT(dev);
	register struct tty *tp = &com_tty[unit];
 
#ifdef notyet
	/*
	 * (XXX) We disallow virtual consoles if the physical console is
	 * a serial port.  This is in case there is a display attached that
	 * is not the console.  In that situation we don't need/want the X
	 * server taking over the console.
	 */
	if (constty && unit == comconsole)
		constty = NULL;
#endif
	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}
 
comintr(unit)
	register int unit;
{
	register com;
	register u_char code;
	register struct tty *tp;

	com = com_addr[unit];
	while (1) {
		code = inb(com+com_iir);
		switch (code) {
		case IIR_NOPEND:
			return (1);
		case IIR_RXRDY:
			/* do time-critical read in-line */
			tp = &com_tty[unit];
			code = inb(com+com_data);
			if ((tp->t_state & TS_ISOPEN) == 0) {
#ifdef KGDB
				if (kgdb_dev == makedev(1, unit) &&
				    code == '!') {
					printf("kgdb trap from com%d\n", unit);
					/* trap into kgdb */
					asm("trap #15;");
				}
#endif
			} else
				(*linesw[tp->t_line].l_rint)(code, tp);
			break;
		case IIR_TXRDY:
			tp = &com_tty[unit];
			tp->t_state &=~ (TS_BUSY|TS_FLUSH);
			if (tp->t_line)
				(*linesw[tp->t_line].l_start)(tp);
			else
				comstart(tp);
			break;
		case IIR_RLS:
			comeint(unit, com);
			break;
		default:
			if (code & IIR_NOPEND)
				return (1);
			log(LOG_WARNING, "com%d: weird interrupt: 0x%x\n",
			    unit, code);
			/* fall through */
		case IIR_MLSC:
			commint(unit, com);
			break;
		}
	}
}

comeint(unit, com)
	register int unit;
	register com;
{
	register struct tty *tp;
	register int stat, c;

	tp = &com_tty[unit];
	stat = inb(com+com_lsr);
	c = inb(com+com_data);
	if ((tp->t_state & TS_ISOPEN) == 0) {
#ifdef KGDB
		/* we don't care about parity errors */
		if (((stat & (LSR_BI|LSR_FE|LSR_PE)) == LSR_PE) &&
		    kgdb_dev == makedev(1, unit) && c == '!') {
			printf("kgdb trap from com%d\n", unit);
			/* trap into kgdb */
			asm("trap #15;");
		}
#endif
		return;
	}
	if (stat & (LSR_BI | LSR_FE))
		c |= TTY_FE;
	else if (stat & LSR_PE)
		c |= TTY_PE;
	else if (stat & LSR_OE)
		log(LOG_WARNING, "com%d: silo overflow\n", unit);
	(*linesw[tp->t_line].l_rint)(c, tp);
}

commint(unit, com)
	register int unit;
	register com;
{
	register struct tty *tp;
	register int stat;

	tp = &com_tty[unit];
	stat = inb(com+com_msr);
	if ((stat & MSR_DDCD) && (comsoftCAR & (1 << unit)) == 0) {
		if (stat & MSR_DCD)
			(void)(*linesw[tp->t_line].l_modem)(tp, 1);
		else if ((*linesw[tp->t_line].l_modem)(tp, 0) == 0)
			outb(com+com_mcr,
				inb(com+com_mcr) & ~(MCR_DTR | MCR_RTS) | MCR_IENABLE);
	} else if ((stat & MSR_DCTS) && (tp->t_state & TS_ISOPEN) &&
		   (tp->t_flags & CRTSCTS)) {
		/* the line is up and we want to do rts/cts flow control */
		if (stat & MSR_CTS) {
			tp->t_state &=~ TS_TTSTOP;
			ttstart(tp);
		} else
			tp->t_state |= TS_TTSTOP;
	}
}

comioctl(dev, cmd, data, flag)
	dev_t dev;
	caddr_t data;
{
	register struct tty *tp;
	register int unit = UNIT(dev);
	register com;
	register int error;
 
	tp = &com_tty[unit];
	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag);
	if (error >= 0)
		return (error);
	error = ttioctl(tp, cmd, data, flag);
	if (error >= 0)
		return (error);

	com = com_addr[unit];
	switch (cmd) {

	case TIOCSBRK:
		outb(com+com_cfcr, inb(com+com_cfcr) | CFCR_SBREAK);
		break;

	case TIOCCBRK:
		outb(com+com_cfcr, inb(com+com_cfcr) & ~CFCR_SBREAK);
		break;

	case TIOCSDTR:
		(void) commctl(dev, MCR_DTR | MCR_RTS, DMBIS);
		break;

	case TIOCCDTR:
		(void) commctl(dev, MCR_DTR | MCR_RTS, DMBIC);
		break;

	case TIOCMSET:
		(void) commctl(dev, *(int *)data, DMSET);
		break;

	case TIOCMBIS:
		(void) commctl(dev, *(int *)data, DMBIS);
		break;

	case TIOCMBIC:
		(void) commctl(dev, *(int *)data, DMBIC);
		break;

	case TIOCMGET:
		*(int *)data = commctl(dev, 0, DMGET);
		break;

	default:
		return (ENOTTY);
	}
	return (0);
}

comparam(tp, t)
	register struct tty *tp;
	register struct termios *t;
{
	register com;
	register int cfcr, cflag = t->c_cflag;
	int unit = UNIT(tp->t_dev);
	int ospeed = ttspeedtab(t->c_ospeed, comspeedtab);
 
	/* check requested parameters */
        if (ospeed < 0 || (t->c_ispeed && t->c_ispeed != t->c_ospeed))
                return(EINVAL);
        /* and copy to tty */
        tp->t_ispeed = t->c_ispeed;
        tp->t_ospeed = t->c_ospeed;
        tp->t_cflag = cflag;

	com = com_addr[unit];
	outb(com+com_ier, IER_ERXRDY | IER_ETXRDY | IER_ERLS /*| IER_EMSC*/);
	if (ospeed == 0) {
		(void) commctl(unit, 0, DMSET);	/* hang up line */
		return(0);
	}
	outb(com+com_cfcr, inb(com+com_cfcr) | CFCR_DLAB);
	outb(com+com_data, ospeed & 0xFF);
	outb(com+com_ier, ospeed >> 8);
	switch (cflag&CSIZE) {
	case CS5:
		cfcr = CFCR_5BITS; break;
	case CS6:
		cfcr = CFCR_6BITS; break;
	case CS7:
		cfcr = CFCR_7BITS; break;
	case CS8:
		cfcr = CFCR_8BITS; break;
	}
	if (cflag&PARENB) {
		cfcr |= CFCR_PENAB;
		if ((cflag&PARODD) == 0)
			cfcr |= CFCR_PEVEN;
	}
	if (cflag&CSTOPB)
		cfcr |= CFCR_STOPB;
	outb(com+com_cfcr, cfcr);
	return(0);
}
 
comstart(tp)
	register struct tty *tp;
{
	register com;
	int s, unit, c;
 
	unit = UNIT(tp->t_dev);
	com = com_addr[unit];
	s = spltty();
	if (tp->t_state & (TS_TIMEOUT|TS_TTSTOP))
		goto out;
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
	if (RB_LEN(&tp->t_out) == 0)
		goto out;
	if (inb(com+com_lsr) & LSR_TXRDY) {
		c = getc(&tp->t_out);
		tp->t_state |= TS_BUSY;
		outb(com+com_data, c);
	}
out:
	splx(s);
}
 
/*
 * Stop output on a line.
 */
/*ARGSUSED*/
comstop(tp, flag)
	register struct tty *tp;
{
	register int s;

	s = spltty();
	if (tp->t_state & TS_BUSY) {
		if ((tp->t_state&TS_TTSTOP)==0)
			tp->t_state |= TS_FLUSH;
	}
	splx(s);
}
 
commctl(dev, bits, how)
	dev_t dev;
	int bits, how;
{
	register com;
	register int unit;
	int s;

	unit = UNIT(dev);
	com = com_addr[unit];
	s = spltty();
	switch (how) {

	case DMSET:
		outb(com+com_mcr, bits | MCR_IENABLE);
		break;

	case DMBIS:
		outb(com+com_mcr, inb(com+com_mcr) | bits | MCR_IENABLE);
		break;

	case DMBIC:
		outb(com+com_mcr, inb(com+com_mcr) & ~bits | MCR_IENABLE);
		break;

	case DMGET:
		bits = inb(com+com_msr);
		break;
	}
	(void) splx(s);
	return(bits);
}

#ifdef notdef
/*
 * Following are all routines needed for COM to act as console
 */
#include "machine/cons.h"

comcnprobe(ip)
	struct isa_device *ip;
{
	int unit, i;
	extern int comopen();

	/* XXX: ick */
	unit = CONUNIT;
	com_addr[CONUNIT] = CONADDR;

	/* make sure hardware exists */
	if (badaddr((short *)com_addr[unit])) {
		cp->cn_pri = CN_DEAD;
		return;
	}

	/* locate the major number */
	for (i = 0; i < nchrdev; i++)
		if (cdevsw[i].d_open == comopen)
			break;

	/* initialize required fields */
	cp->cn_dev = makedev(i, unit);
	cp->cn_tp = &com_tty[unit];
	switch (com_addr[unit]->com_irid) {
	case COMID0:
	case COMID1:
		cp->cn_pri = CN_NORMAL;
		break;
	case COMREMID0:
	case COMREMID1:
		cp->cn_pri = CN_REMOTE;
		break;
	default:
		cp->cn_pri = CN_DEAD;
		break;
	}
}

comcninit(cp)
	struct consdev *cp;
{
	int unit = UNIT(cp->cn_dev);

	cominit(unit);
	comconsole = unit;
}

cominit(unit)
	int unit;
{
	register struct com;
	int s, rate;
	short stat;

#ifdef lint
	stat = unit; if (stat) return;
#endif
	com = com_addr[unit];
	s = splhigh();
	com->com_irid = 0xFF;
	DELAY(100);
	outb(com+com_ic,IC_IE);
	outb(com+com_cfcr, CFCR_DLAB);
	rate = ttspeedtab(comdefaultrate, comspeedtab);
	outb(com+com_data, rate & 0xFF);
	outb(com+com_ier, rate >> 8);
	outb(com+com_cfcr, CFCR_8BITS);
	outb(com+com_ier, IER_ERXRDY | IER_ETXRDY);
	stat = inb(com+com_iir);
	splx(s);
}

comcngetc(dev)
{
	register com = com_addr[UNIT(dev)];
	short stat;
	int c, s;

#ifdef lint
	stat = dev; if (stat) return(0);
#endif
	s = splhigh();
	while (((stat = inb(com+com_iir)) & LSR_RXRDY) == 0)
		;
	c = inb(com+com_data);
	stat = inb(com+com_iir);
	splx(s);
	return(c);
}

/*
 * Console kernel output character routine.
 */
comcnputc(dev, c)
	dev_t dev;
	register int c;
{
	register com = com_addr[UNIT(dev)];
	register int timo;
	short stat;
	int s = splhigh();

#ifdef lint
	stat = dev; if (stat) return;
#endif
	if (comconsole == -1) {
		(void) cominit(UNIT(dev));
		comconsole = UNIT(dev);
	}
	/* wait for any pending transmission to finish */
	timo = 50000;
	while (((stat = inb(com+com_iir)) & LSR_TXRDY) == 0 && --timo)
		;
	outb(com+com_data, c);
	/* wait for this transmission to complete */
	timo = 1500000;
	while (((stat = inb(com+com_iir)) & LSR_TXRDY) == 0 && --timo)
		;
	/* clear any interrupts generated by this transmission */
	stat = inb(com+com_iir);
	splx(s);
}
#endif
#endif
