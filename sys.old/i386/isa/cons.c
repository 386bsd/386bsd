/* TODO: split intro keyboard/nga
drivers, add raw keyboard access mode for X11, add display frame
buffer map for X11 process, support for standard color and bold modes, 43/56?
line vga modes, something that determines ega/vga/cga seperately,
laptop external monitor support, FUNCTION KEY DEBUGGING!. */
/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Added support for ibmpc term type and improved keyboard support. -Don Ahn
 *
 * %sccs.include.386.c%
 *
 *	%W% (Berkeley) %G%
 */

/*
 * code to work keyboard & display for console
 */
#include "param.h"
#include "conf.h"
#include "ioctl.h"
#include "user.h"
#include "proc.h"
#include "tty.h"
#include "uio.h"
#include "i386/isa/isa_device.h"
#include "callout.h"
#include "systm.h"
#include "kernel.h"
#include "syslog.h"
#include "malloc.h"
#include "i386/isa/icu.h"
#include "i386/isa/isa.h"

struct	tty cons;

struct	consoftc {
	char	cs_flags;
#define	CSF_ACTIVE	0x1	/* timeout active */
#define	CSF_POLLING	0x2	/* polling for input */
	char	cs_lastc;	/* last char sent */
	int	cs_timo;	/* timeouts since interrupt */
	u_long	cs_wedgecnt;	/* times restarted */
} consoftc;

int cnprobe(), cnattach();

struct	isa_driver cndriver = {
	cnprobe, cnattach, "cn",
};


#define	COL		80
#define	ROW		25
#define	CHR		2
#define MONO_BASE	0x3B4
#define MONO_BUF	0xB0000
#define CGA_BASE	0x3D4
#define CGA_BUF		0xB8000
#define IOPHYSMEM	0xA0000

union	crtbuf {
	struct crtbyte {
		u_char	chr;
		union	att {
			u_char	attr; /* attribute : color, blink, bold, ... */
			struct crtattbit {
				u_char	fg:4;	/* foreground */
				u_char	bg:4;	/* background */
			} c_attrbit;
		} c_att;
	} c_byte;
#define	c_chr	c_byte.chr
#define	c_attr	c_byte.c_att.attr
#define	c_fgat	c_byte.c_att.c_attrbit.fg
#define	c_bgat	c_byte.c_att.c_attrbit.bg
	u_short	c_wd;
} *Crtat = (union crtbuf *)MONO_BUF;

struct crtscreen {
	union crtbuf s_buf[ROW*COL];
	u_short	s_cursor;
/* these are wrong: bgat is only a fill attribute, curat is the
attribute written with the text, dfltat is the previous attribute
prior to a standout mode, and there is no stand out mode attrib.*/
	u_char	s_bgat; /* background or fill attribute */ 
	u_char	s_curat; /* foreground or current attribute */ 
	u_char	s_dfltat; /* default attribute */ 
	struct	crtscreen *s_back;	/* previous screen */
	struct	crtscreen *s_next;	/* next screen */
} *dispsp, /* this is the currently displaying screen */
toplvl, /* this is the top level open process screen */
*conssp,	/* this is the console of the moment */
sysscreen; 	/* this is the system's screen (for system error messages)*/

static unsigned int addr_6845 = MONO_BASE;

/*
 * We check the console periodically to make sure
 * that it hasn't wedged.  Unfortunately, if an XOFF
 * is typed on the console, that can't be distinguished
 * from more catastrophic failure.
 */
#define	CN_TIMERVAL	(hz)		/* frequency at which to check cons */
#define	CN_TIMO		(2*60)		/* intervals to allow for output char */

int	cnstart(), cnparam(), ttrstrt();
char	partab[];

cnprobe(dev)
struct isa_device *dev;
{
	u_char c;
	int again = 0;

	/* Enable interrupts and keyboard controller */
	while (inb(0x64)&2); outb(0x64,0x60);
	while (inb(0x64)&2); outb(0x60,0x4D);

	/* Start keyboard stuff RESET */
	while (inb(0x64)&2);	/* wait input ready */
	outb(0x60,0xFF);	/* RESET */
	while((c=inb(0x60))!=0xFA) {
		if ((c == 0xFE) ||  (c == 0xFF)) {
			if(!again)printf("KEYBOARD disconnected: RECONNECT \n");
			while (inb(0x64)&2);	/* wait input ready */
			outb(0x60,0xFF);	/* RESET */
			again = 1;
		}
	}
	/* pick up keyboard reset return code */
	while((c=inb(0x60))!=0xAA)nulldev();
	return 1;
}

cnattach(dev)
struct isa_device *dev;
{
	union crtbuf *cp = Crtat + (CGA_BUF-MONO_BUF)/CHR;
	u_short was;

	/* Crtat initialized to point to MONO buffer   */
	/* if not present change to CGA_BUF offset     */
	/* ONLY ADD the difference since locore.s adds */
	/* in the remapped offset at the right time    */

	was = Crtat->c_wd;
	Crtat->c_wd = 0xA55A;
	if (Crtat->c_wd != 0xA55A)
		printf("<mono>");
	else	printf("<color>");
	Crtat->c_wd = was;
	update_cursor(dispsp);
}
static char openf;

/*ARGSUSED*/
cnopen(dev, flag)
	dev_t dev;
{
	register struct tty *tp;

	tp = &cons;
	tp->t_oproc = cnstart;
	tp->t_param = cnparam;
	tp->t_dev = dev;

	/* (put this in consinit code)
		if no top level screen allocated, allocate */
	if (openf++ == 0) {
		initscrn(&toplvl);
		conssp = &toplvl;
		gotoscrn(conssp);
	}

	if ((tp->t_state & TS_ISOPEN) == 0) {
		tp->t_state |= TS_WOPEN;
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = TTYDEF_CFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
		cnparam(tp, &tp->t_termios);
		ttsetwater(tp);
	} else if (tp->t_state&TS_XCLUDE && u.u_uid != 0)
		return (EBUSY);
	tp->t_state |= TS_CARR_ON;
	return ((*linesw[tp->t_line].l_open)(dev, tp));
}

cnclose(dev, flag)
	dev_t dev;
{
	(*linesw[cons.t_line].l_close)(&cons);
	ttyclose(&cons);
	return(0);
}

/*ARGSUSED*/
cnread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
{
	return ((*linesw[cons.t_line].l_read)(&cons, uio, flag));
}

/*ARGSUSED*/
cnwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
{
	return ((*linesw[cons.t_line].l_write)(&cons, uio, flag));
}

/*
 * Got a console receive interrupt -
 * the console processor wants to give us a character.
 * Catch the character, and see who it goes to.
 */
cnrint(dev, irq, cpl)
	dev_t dev;
{
	int c;

	c = sgetc(1);
	if (c&0x100) return;
	/*if (consoftc.cs_flags&CSF_POLLING)
		return;*/
#ifdef KDB
	if (kdbrintr(c, &cons))
		return;
#endif
	if(dispsp != conssp) gotoscrn(conssp);
	(*linesw[cons.t_line].l_rint)(c&0xff, &cons);
}

cnioctl(dev, cmd, data, flag)
	dev_t dev;
	caddr_t data;
{
	register struct tty *tp = &cons;
	register error;
 
	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag);
	if (error >= 0)
		return (error);
	error = ttioctl(tp, cmd, data, flag);
	if (error >= 0)
		return (error);
	return (ENOTTY);
}

int	consintr = 1;
/*
 * Got a console transmission interrupt -
 * the console processor wants another character.
 */
cnxint(dev)
	dev_t dev;
{
	register struct tty *tp;
	register int unit;

	if (!consintr)
		return;
	cons.t_state &= ~TS_BUSY;
	consoftc.cs_timo = 0;
	if (cons.t_line)
		(*linesw[cons.t_line].l_start)(&cons);
	else
		cnstart(&cons);
}

cnstart(tp)
	register struct tty *tp;
{
	register c, s;

	s = spltty();
	if (tp->t_state & (TS_TIMEOUT|TS_BUSY|TS_TTSTOP))
		goto out;
	do {
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
	c = getc(&tp->t_out);
	splx(s);
	sput(c, conssp);
	s = spltty();
	} while(1);
out:
	splx(s);
}

static __color;

cnputc(c)
	char c;
{
	if (c == '\n')
		sput('\r', &sysscreen);
	sput(c, &sysscreen);
	if(dispsp != &sysscreen) gotoscrn(&sysscreen);
}

/*
 * Set line parameters
 */
cnparam(tp, t)
	register struct tty *tp;
	register struct termios *t;
{
	register int cflag = t->c_cflag;
        /* and copy to tty */
        tp->t_ispeed = t->c_ispeed;
        tp->t_ospeed = t->c_ospeed;
        tp->t_cflag = cflag;

	return(0);
}

extern int hz;

static beeping;
sysbeepstop()
{
	/* disable counter 2 */
	outb(0x61,inb(0x61)&0xFC);
	beeping = 0;
}

sysbeep()
{

	/* enable counter 2 */
	outb(0x61,inb(0x61)|3);
	/* set command for counter 2, 2 byte write */
	outb(0x43,0xB6);
	/* send 0x637 for 750 HZ */
	outb(0x42,0x37);
	outb(0x42,0x06);
	if(!beeping)timeout(sysbeepstop,0,hz/4);
	beeping = 1;
}

/* Sync software's cursor to hardware */
update_cursor(sp) register struct crtscreen *sp;
{ 	int pos = dispsp->s_cursor;

	outb(addr_6845, 14);
	outb(addr_6845+1, pos>> 8);
	outb(addr_6845, 15);
	outb(addr_6845+1, pos);
	outb(addr_6845, 10);
	outb(addr_6845+1, 0);
	outb(addr_6845, 11);
	outb(addr_6845+1, 18);
	timeout(update_cursor, sp, hz/10);
}

/* Extract cursor location */
get_cursor() {
	unsigned cursorat;

	outb(addr_6845,14);
	cursorat = inb(addr_6845+1)<<8 ;
	outb(addr_6845,15);
	cursorat |= inb(addr_6845+1);
	return(cursorat);
}

u_char shfts, ctls, alts, caps, num, stp, scroll;

/*
 * Compensate for abysmally stupid frame buffer aribitration with macro
 */
#define VGA_16bit
#ifdef VGA_16bit
#define	wrtchar(c,att) { \
	crt[cursor].c_wd = ((att) << 8) + (c); \
	cursor++; \
}
#define fill(chr,att,where,cnt) fillw(((att) << 8) + (chr), (where), (cnt))
#else
#define	wrtchar(c, att) { \
	do {	crt[cursor].c_chr = (c); \
		crt[cursor].c_attr = (att); \
	} while ((c) != crt[cursor].c_chr || (att) != crt[cursor].c_attr); \
	cursor++; \
}
#define	bcopy(a,b,c)	bcopyb(a,b,c)
#endif

/* sput has support for emulation of the 'ibmpc' termcap entry. */
/* This is a bare-bones implementation of a bare-bones entry    */
/* One modification: Change li#24 to li#25 to reflect 25 lines  */

sput(c, sp)
struct crtscreen *sp;
{
	register union crtbuf *crt;

	static int esc,ebrac,eparm,cx,cy,row, cursor;

	if (dispsp == 0) {
		union crtbuf *cp = Crtat + (CGA_BUF-MONO_BUF)/CHR;
		u_short was;

		/* Crtat initialized to point to MONO buffer   */
		/* if not present change to CGA_BUF offset     */
		/* ONLY ADD the difference since locore.s adds */
		/* in the remapped offset at the right time    */

		was = cp->c_wd;
		cp->c_wd = 0xA55A;
		if (cp->c_wd != 0xA55A) {
			addr_6845 = MONO_BASE;
		} else {
			cp->c_wd = was;
			addr_6845 = CGA_BASE;
			Crtat = Crtat + (CGA_BUF-MONO_BUF)/CHR;
		}

		sp = dispsp = &sysscreen;
		initscrn(sp);
		sp->s_cursor = get_cursor();
		fill(' ', sp->s_curat, Crtat + sp->s_cursor,
			COL*ROW - sp->s_cursor);
	}

	/* are we the currently displaying screen or just a buffer? */
	if(dispsp == sp) {
		crt = Crtat;
		cursor = sp->s_cursor;
	} else {
		crt = sp->s_buf;
		cursor = sp->s_cursor;
	}

	/* display a character */
	switch(c) {
	case 0x1B:
		esc = 1; ebrac = 0; eparm = 0;
		break;

	case '\t':
		/*do {
			wrtchar(' ', sp->s_curat);
		} while (cursor % 8);*/
		cursor += (8 - cursor % 8);
		break;

	case '\010':
		cursor--; /* non-destructive backspace */
		break;

	case '\r':
		cursor -= cursor % COL;
		break;

	case '\n':
		cursor += COL ;
		break;

	default:
		if (esc) {
			if (ebrac) {
				switch(c) {
				case 'x': /* experimental */
					if (cx == 0){
						sp = &toplvl;
						gotoscrn(sp);
						sp = conssp = dispsp;
					} else if (cx == 1) {
						newscrn(sp);
						sp = conssp = dispsp;
					} else if (cx == 2) {
						sp = sp->s_back;
						gotoscrn(sp);
						sp = conssp = dispsp;
					}
					if (eparm) {
					    sp->s_bgat = cy;
					    fillbgat(cy, crt, COL*ROW);
					}
					esc = 0; ebrac = 0; eparm = 0;
					break;
				case 'm': /* support for standout */
					if (cx == 0)sp->s_curat = sp->s_dfltat;
					else sp->s_curat = cx;
					if (eparm) sp->s_dfltat = cy;
					esc = 0; ebrac = 0; eparm = 0;
					break;
				case 'A': /* back one row */
					cursor -= COL;
					esc = 0; ebrac = 0; eparm = 0;
					break;
				case 'B': /* down one row */
					cursor += COL;
					esc = 0; ebrac = 0; eparm = 0;
					break;
				case 'C': /* right cursor */
					cursor++;
					esc = 0; ebrac = 0; eparm = 0;
					break;
				case 'J': /* Clear to end of display */
					fill(' ', sp->s_curat, crt+cursor,
						COL*ROW-cursor);
					esc = 0; ebrac = 0; eparm = 0;
					break;
				case 'K': /* Clear to EOL */
					fill(' ', sp->s_curat, crt+cursor,
						COL-cursor%COL);
					esc = 0; ebrac = 0; eparm = 0;
					break;
				case 'H': /* Cursor move */
					if (!cx||!cy)
						cursor = 0;
					else
						cursor = (cx-1)*COL+cy-1;
					esc = 0; ebrac = 0; eparm = 0;
					break;
				case ';': /* Switch params in cursor def */
					eparm = 1;
					return;
				default: /* Only numbers valid here */
					if ((c >= '0')&&(c <= '9')) {
						if (eparm) {
							cy *= 10;
							cy += c - '0';
						} else {
							cx *= 10;
							cx += c - '0';
						}
					} else {
						esc = 0; ebrac = 0; eparm = 0;
					}
					return;
				}
				break;
			} else if (c == 'c') { /* Clear screen & home */
				fill(' ', sp->s_curat, crt,COL*ROW);
				cursor = 0;
				esc = 0; ebrac = 0; eparm = 0;
			} else if (c == '[') { /* Start ESC [ sequence */
				ebrac = 1; cx = 0; cy = 0; eparm = 0;
			} else if (c == 0x1b) { /* ESC ESC sequence */
				esc = 0;
				wrtchar(c, sp->s_curat); 
			} else { /* Invalid, clear state */
				 esc = 0; ebrac = 0; eparm = 0;
			}
		} else {
			if (c == 7)
				sysbeep();
			/* Print only printables */
			else
				wrtchar(c, sp->s_curat); 
			break;
		}
	}

	/* do we need to scroll? */
	if (cursor >= COL*ROW) {
		/* scroll lock check XXX */
/*		if (toplvlsp) do sgetc(1); while (scroll);*/

		bcopy(crt+COL, crt, COL*(ROW-1)*CHR);
		fill(' ', sp->s_curat, crt+COL*(ROW-1),COL) ;
		cursor -= COL ;
	}

	/* update cursor position in data structure */
	sp->s_cursor = cursor;
}

#ifndef VGA_16bit
static 
fill(chr,att,where,cnt) register union crtbuf *where; register cnt; {

	while (cnt--) {
		where->c_attr = att;
		where++->c_chr = chr;
	}
}
#endif

static 
fillatt(att, where, cnt) register union crtbuf *where; register cnt; {

	while (cnt--)
		where++->c_attr = att;
}

static 
fillfgat(att, where, cnt) register union crtbuf *where; register cnt; {

	while (cnt--)
		where++->c_fgat = att;
}

static 
fillbgat(att, where, cnt) register union crtbuf *where; register cnt; {

	while (cnt--)
		where++->c_bgat = att;
}

/* save screen and make argument current screen */
/* three kinds of switches:
	1. undisplayed screen to undisplayed screen (just change pointers)
	2. undisplayed screen to displayed screen (just change pointers)
	3. displayed screen to undisplayed screen (save state)

	why we did this:
	put system messages in a seperate screen 
	save vi screens recursively, preserving old contents
	cycle through screens using pgup/pgdown
	add debugging screens
	at any one time, you can only interact with conssp. dispsp is
	which is on the screen at any time being veiwed. toplvlsp is the
	topmost,i.e the "login" screen
	*/
initscrn (sp) struct crtscreen *sp; {
		sp->s_cursor = 0;
		sp->s_dfltat = 0x7;
		sp->s_curat = 0x7;
		sp->s_bgat = 0x0;
		sp->s_back = sp;
		fill(' ', sp->s_curat, sp->s_buf,
			COL*ROW);
}

static
gotoscrn(newsp) struct crtscreen *newsp; {

	/* save state of current screen to backing object */
	/*dispsp->s_cursor = get_cursor();*/
	bcopy(Crtat, dispsp->s_buf, ROW*COL*CHR);

	/* move into place new screen */
	bcopy(newsp->s_buf, Crtat,  ROW*COL*CHR);
	dispsp = newsp;
}

/* save screen, allocate new one, clean it, and make it current */
newscrn(sp) struct crtscreen *sp; {
	struct crtscreen *newsp;
	
	if (sp->s_next == 0)
		sp->s_next = (struct crtscreen *)
			malloc (sizeof(struct crtscreen), M_DEVBUF);
	newsp = sp->s_next;

	bzero ((caddr_t)newsp, sizeof(struct crtscreen));
	initscrn(newsp);
	newsp->s_back = sp;

	gotoscrn(newsp);
}

#define	L		0x0001	/* locking function */
#define	SHF		0x0002	/* keyboard shift */
#define	ALT		0x0004	/* alternate shift -- alternate chars */
#define	NUM		0x0008	/* numeric shift  cursors vs. numeric */
#define	CTL		0x0010	/* control shift  -- allows ctl function */
#define	CPS		0x0020	/* caps shift -- swaps case of letter */
#define	ASCII		0x0040	/* ascii code for this key */
#define	STP		0x0080	/* stop output */
#define	FUNC		0x0100	/* function key */
#define	SCROLL		0x0200	/* scroll lock key */
#define	SYSREQ		0x0400	/* sys reqkey */

unsigned	__debug = 0;
u_short action[] = {
0,     ASCII, ASCII, ASCII, ASCII, ASCII, ASCII, ASCII,		/* scan  0- 7 */
ASCII, ASCII, ASCII, ASCII, ASCII, ASCII, ASCII, ASCII,		/* scan  8-15 */
ASCII, ASCII, ASCII, ASCII, ASCII, ASCII, ASCII, ASCII,		/* scan 16-23 */
ASCII, ASCII, ASCII, ASCII, ASCII,   CTL, ASCII, ASCII,		/* scan 24-31 */
ASCII, ASCII, ASCII, ASCII, ASCII, ASCII, ASCII, ASCII,		/* scan 32-39 */
ASCII, ASCII, SHF  , ASCII, ASCII, ASCII, ASCII, ASCII,		/* scan 40-47 */
ASCII, ASCII, ASCII, ASCII, ASCII, ASCII,  SHF,  ASCII,		/* scan 48-55 */
  ALT, ASCII, CPS  , FUNC , FUNC , FUNC , FUNC , FUNC ,		/* scan 56-63 */
FUNC , FUNC , FUNC , FUNC , FUNC , NUM, SCROLL, ASCII,		/* scan 64-71 */
ASCII, ASCII, ASCII, ASCII, ASCII, ASCII, ASCII, ASCII,		/* scan 72-79 */
ASCII, ASCII, ASCII, ASCII, SYSREQ,    0,     0,     0,		/* scan 80-87 */
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,	} ;

u_char unshift[] = {	/* no shift */
0,     033  , '1'  , '2'  , '3'  , '4'  , '5'  , '6'  ,		/* scan  0- 7 */
'7'  , '8'  , '9'  , '0'  , '-'  , '='  , 0177 ,'\t'  ,		/* scan  8-15 */

'q'  , 'w'  , 'e'  , 'r'  , 't'  , 'y'  , 'u'  , 'i'  ,		/* scan 16-23 */
'o'  , 'p'  , '['  , ']'  , '\r' , CTL  , 'a'  , 's'  ,		/* scan 24-31 */

'd'  , 'f'  , 'g'  , 'h'  , 'j'  , 'k'  , 'l'  , ';'  ,		/* scan 32-39 */
'\'' , '`'  , SHF  , '\\' , 'z'  , 'x'  , 'c'  , 'v'  ,		/* scan 40-47 */

'b'  , 'n'  , 'm'  , ','  , '.'  , '/'  , SHF  ,   '*',		/* scan 48-55 */
ALT  , ' '  , CPS,     1,     2,    3 ,     4,     5,		/* scan 56-63 */

    6,     7,     8,     9,    10, NUM, STP,   '7',		/* scan 64-71 */
  '8',   '9',   '-',   '4',   '5',   '6',   '+',   '1',		/* scan 72-79 */

  '2',   '3',   '0',   '.',     0,     0,     0,     0,		/* scan 80-87 */
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,	} ;

u_char shift[] = {	/* shift shift */
0,     033  , '!'  , '@'  , '#'  , '$'  , '%'  , '^'  ,		/* scan  0- 7 */
'&'  , '*'  , '('  , ')'  , '_'  , '+'  , 0177 ,'\t'  ,		/* scan  8-15 */
'Q'  , 'W'  , 'E'  , 'R'  , 'T'  , 'Y'  , 'U'  , 'I'  ,		/* scan 16-23 */
'O'  , 'P'  , '{'  , '}'  , '\r' , CTL  , 'A'  , 'S'  ,		/* scan 24-31 */
'D'  , 'F'  , 'G'  , 'H'  , 'J'  , 'K'  , 'L'  , ':'  ,		/* scan 32-39 */
'"'  , '~'  , SHF  , '|'  , 'Z'  , 'X'  , 'C'  , 'V'  ,		/* scan 40-47 */
'B'  , 'N'  , 'M'  , '<'  , '>'  , '?'  , SHF  ,   '*',		/* scan 48-55 */
ALT  , ' '  , CPS,     0,     0, ' '  ,     0,     0,		/* scan 56-63 */
    0,     0,     0,     0,     0, NUM, STP,   '7',		/* scan 64-71 */
  '8',   '9',   '-',   '4',   '5',   '6',   '+',   '1',		/* scan 72-79 */
  '2',   '3',   '0',   '.',     0,     0,     0,     0,		/* scan 80-87 */
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,	} ;

u_char ctl[] = {	/* CTL shift */
0,     033  , '!'  , 000  , '#'  , '$'  , '%'  , 036  ,		/* scan  0- 7 */
'&'  , '*'  , '('  , ')'  , 037  , '+'  , 034  ,'\177',		/* scan  8-15 */
021  , 027  , 005  , 022  , 024  , 031  , 025  , 011  ,		/* scan 16-23 */
017  , 020  , 033  , 035  , '\r' , CTL  , 001  , 023  ,		/* scan 24-31 */
004  , 006  , 007  , 010  , 012  , 013  , 014  , ';'  ,		/* scan 32-39 */
'\'' , '`'  , SHF  , 034  , 032  , 030  , 003  , 026  ,		/* scan 40-47 */
002  , 016  , 015  , '<'  , '>'  , '?'  , SHF  ,   '*',		/* scan 48-55 */
ALT  , ' '  , CPS,     0,     0, ' '  ,     0,     0,		/* scan 56-63 */
CPS,     0,     0,     0,     0,     0,     0,     0,		/* scan 64-71 */
    0,     0,     0,     0,     0,     0,     0,     0,		/* scan 72-79 */
    0,     0,     0,     0,SYSREQ,     0,     0,     0,		/* scan 80-87 */
    0,     0,   033, '7'  , '4'  , '1'  ,     0, NUM,		/* scan 88-95 */
'8'  , '5'  , '2'  ,     0, STP, '9'  , '6'  , '3'  ,		/*scan  96-103*/
'.'  ,     0, '*'  , '-'  , '+'  ,     0,     0,     0,		/*scan 104-111*/
0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,	} ;

#ifdef notdef
struct key {
	u_short action;		/* how this key functions */
	char	ascii[8];	/* ascii result character indexed by shifts */
};
#endif


#define	KBSTAT		0x64	/* kbd status port */
#define	KBS_INP_BUF_FUL	0x02	/* kbd char ready */
#define	KBDATA		0x60	/* kbd data port */
#define	KBSTATUSPORT	0x61	/* kbd status */

update_led()
{
	while (inb(0x64)&2);	/* wait input ready */
	outb(0x60,0xED);	/* LED Command */
	while (inb(0x64)&2);	/* wait input ready */
	outb(0x60,scroll | 2*num | 4*caps);
}

reset_cpu() {
	while(1) {
		while (inb(0x64)&2);	/* wait input ready */
		outb(0x64,0xFE);	/* Reset Command */
		DELAY(4000000);
		while (inb(0x64)&2);	/* wait input ready */
		outb(0x64,0xFF);	/* Keyboard Reset Command */
	}
	/* NOTREACHED */
}

/*
sgetc(noblock) : get a character from the keyboard. If noblock = 0 wait until
a key is gotten.  Otherwise return a 0x100 (256).
*/
int sgetc(noblock)
{
	u_char dt; unsigned key;

	/* if system requires input proceed to system screen */
	/* if(!noblock && dispsp != &sysscreen) gotoscrn(dispsp, &sysscreen);*/
loop:
	/* First see if there is something in the keyboard port */
	if (inb(KBSTAT)&1) dt = inb(KBDATA);
	else { if (noblock) return (0x100); else goto loop; }
if(__debug&0x4)printf("|%x.", dt);

	/* Check for cntl-alt-del */
	if ((dt == 83)&&ctls&&alts) _exit();

	/* Check for make/break */
	if (dt & 0x80) {
		/* break */
		dt = dt & 0x7f ;
		switch (action[dt]) {
		case SHF: shfts = 0; break;
		case ALT: alts = 0; break;
		case CTL: ctls = 0; break;
		case FUNC:
			/* Toggle debug flags */
			key = unshift[dt];
			if(__debug & (1<<key)) __debug &= ~(1<<key) ;
			else __debug |= (1<<key) ;
/*if(__debug&0x400)panic("force core");*/
			break;
case SYSREQ:
	gotoscrn(conssp);
		}
	} else {
		/* make */
		dt = dt & 0x7f ;
		switch (action[dt]) {
		/* LOCKING KEYS */
		case NUM: num ^= 1; update_led(); break;
		case CPS: caps ^= 1; update_led(); break;
		case SCROLL: scroll ^= 1; update_led(); break;
		case STP: stp ^= 1; if(stp) goto loop; break;

		/* NON-LOCKING KEYS */
		case SHF: shfts = 1; break;
		case ALT: alts = 1; break;
		case CTL: ctls = 1; break;
		case ASCII:
			if (shfts) dt = shift[dt];
			else if (ctls) dt = ctl[dt];
			else dt = unshift[dt];
			if (caps && (dt >= 'a' && dt <= 'z')) dt -= 'a' - 'A';
			return(dt);
break;
case SYSREQ:
	gotoscrn(&sysscreen);
		}
	}
	if (noblock) return (0x100); else goto loop;
}

/* special characters */
#define bs	8
#define lf	10	
#define cr	13	
#define cntlc	3	
#define del	0177	
#define cntld	4

getchar()
{
	register char thechar;
	register delay;
	int x;

	consoftc.cs_flags |= CSF_POLLING;
	x=splhigh();
	sput('>', &sysscreen);
	/*while (1) {*/
		thechar = (char) sgetc(0);
		consoftc.cs_flags &= ~CSF_POLLING;
		splx(x);
		switch (thechar) {
		    default: if (thechar >= ' ')
			     	sput(thechar,&sysscreen);
			     return(thechar);
		    case cr:
		    case lf: sput(cr,&sysscreen);
			     sput(lf,&sysscreen);
			     return(lf);
		    case bs:
		    case del:
			     sput(bs,&sysscreen);
			     sput(' ',&sysscreen);
			     sput(bs,&sysscreen);
			     return(thechar);
		    case cntld:
			     sput('^', &sysscreen); sput('D', &sysscreen);
			sput('\r', &sysscreen); sput('\n', &sysscreen);
			     return(0);
		}
	 /*}*/
}

#include "machine/dbg.h"
static nrow;

dprintf(flgs, fmt, x1)
	char *fmt;
	unsigned x1, flgs;
{	extern unsigned __debug;

	if((flgs&__debug) > DPAUSE) {
		__color = ffs(flgs&__debug)+1;
		prf(fmt, &x1, 1, (struct tty *)0);
	if (flgs&DPAUSE || nrow%24 == 23) { 
		int x;
		x = splhigh();
		if (nrow%24 == 23) nrow = 0;
		sgetc(0);
		splx(x);
	}
	}
	__color = 0;
}

pg(p,q,r,s,t,u,v,w,x,y,z) char *p; {
	printf(p,q,r,s,t,u,v,w,x,y,z);
	printf("\n");
	return(getchar());
}
