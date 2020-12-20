/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */
/*
 * HISTORY
 * $Log: db_input.c,v $
 * Revision 1.1  1992/03/25  21:45:10  pace
 * Initial revision
 *
 * Revision 2.4  91/02/14  14:41:53  mrt
 * 	Add input line editing.
 * 	[90/11/11            dbg]
 * 
 * Revision 2.3  91/02/05  17:06:32  mrt
 * 	Changed to new Mach copyright
 * 	[91/01/31  16:18:13  mrt]
 * 
 * Revision 2.2  90/08/27  21:51:03  dbg
 * 	Reduce lint.
 * 	[90/08/07            dbg]
 * 	Created.
 * 	[90/07/25            dbg]
 * 
 */
/*
 *	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */

#include "sys/param.h"
#include "proc.h"
#include "db_output.h"

/*
 * Character input and editing.
 */

/*
 * We don't track output position while editing input,
 * since input always ends with a new-line.  We just
 * reset the line position at the end.
 */
char *	db_lbuf_start;	/* start of input line buffer */
char *	db_lbuf_end;	/* end of input line buffer */
char *	db_lc;		/* current character */
char *	db_le;		/* one past last character */

#define	CTRL(c)		((c) & 0x1f)
#define	isspace(c)	((c) == ' ' || (c) == '\t')
#define	BLANK		' '
#define	BACKUP		'\b'

void
db_putstring(s, count)
	char	*s;
	int	count;
{
	while (--count >= 0)
	    /*cnputc(*s++);*/
	    console_putchar(*s++);
}

void
db_putnchars(c, count)
	int	c;
	int	count;
{
	while (--count >= 0)
	    console_putchar(c);
	    /*cnputc(c);*/
}

/*
 * Delete N characters, forward or backward
 */
#define	DEL_FWD		0
#define	DEL_BWD		1
void
db_delete(n, bwd)
	int	n;
	int	bwd;
{
	register char *p;

	if (bwd) {
	    db_lc -= n;
	    db_putnchars(BACKUP, n);
	}
	for (p = db_lc; p < db_le-n; p++) {
	    *p = *(p+n);
	    console_putchar(*p);
	    /* cnputc(*p);*/
	}
	db_putnchars(BLANK, n);
	db_putnchars(BACKUP, db_le - db_lc);
	db_le -= n;
}

/* returns TRUE at end-of-line */
int
db_inputchar(c)
	int	c;
{
	switch (c) {
	    case CTRL('b'):
		/* back up one character */
		if (db_lc > db_lbuf_start) {
		    console_putchar(BACKUP);
		    /*cnputc(BACKUP);*/
		    db_lc--;
		}
		break;
	    case CTRL('f'):
		/* forward one character */
		if (db_lc < db_le) {
		    console_putchar(*db_lc);
		    /*cnputc(*db_lc);*/
		    db_lc++;
		}
		break;
	    case CTRL('a'):
		/* beginning of line */
		while (db_lc > db_lbuf_start) {
		    console_putchar(BACKUP);
		    /*cnputc(BACKUP);*/
		    db_lc--;
		}
		break;
	    case CTRL('e'):
		/* end of line */
		while (db_lc < db_le) {
		    console_putchar(*db_lc);
		    /*cnputc(*db_lc);*/
		    db_lc++;
		}
		break;
	    case CTRL('h'):
	    case 0177:
		/* erase previous character */
		if (db_lc > db_lbuf_start)
		    db_delete(1, DEL_BWD);
		break;
	    case CTRL('d'):
		/* erase next character */
		if (db_lc < db_le)
		    db_delete(1, DEL_FWD);
		break;
	    case CTRL('k'):
		/* delete to end of line */
		if (db_lc < db_le)
		    db_delete(db_le - db_lc, DEL_FWD);
		break;
	    case CTRL('t'):
		/* twiddle last 2 characters */
		if (db_lc >= db_lbuf_start + 2) {
		    c = db_lc[-2];
		    db_lc[-2] = db_lc[-1];
		    db_lc[-1] = c;
		    /*cnputc(BACKUP);
		    cnputc(BACKUP);
		    cnputc(db_lc[-2]);
		    cnputc(db_lc[-1]); */
		    console_putchar(BACKUP);
		    console_putchar(BACKUP);
		    console_putchar(db_lc[-2]);
		    console_putchar(db_lc[-1]);
		}
		break;
	    case CTRL('r'):
		db_putstring("^R\n", 3);
		if (db_le > db_lbuf_start) {
		    db_putstring(db_lbuf_start, db_le - db_lbuf_start);
		    db_putnchars(BACKUP, db_le - db_lc);
		}
		break;
	    case '\n':
	    case '\r':
		*db_le++ = c;
		return (1);
	    default:
		if (db_le == db_lbuf_end) {
		    console_putchar('\007');
		    /*cnputc('\007');*/
		}
		else if (c >= ' ' && c <= '~') {
		    register char *p;

		    for (p = db_le; p > db_lc; p--)
			*p = *(p-1);
		    *db_lc++ = c;
		    db_le++;
		    /* cnputc(c);*/
		    console_putchar(c);
		    db_putstring(db_lc, db_le - db_lc);
		    db_putnchars(BACKUP, db_le - db_lc);
		}
		break;
	}
	return (0);
}

int
db_readline(lstart, lsize)
	char *	lstart;
	int	lsize;
{
	db_force_whitespace();	/* synch output position */

	db_lbuf_start = lstart;
	db_lbuf_end   = lstart + lsize;
	db_lc = lstart;
	db_le = lstart;

	while (!db_inputchar(console_getchar()))
	/*while (!db_inputchar(cngetc()))*/
	    continue;

/* 	db_putchar('\r');	/* synch output position */
	db_printf("\r                              \r");

	*db_le = 0;
	return (db_le - db_lbuf_start);
}

void
db_check_interrupt()
{
	register int	c;

	c = cnmaygetc();
	switch (c) {
	    case -1:		/* no character */
		return;

	    case CTRL('c'):
		db_error((char *)0);
		/*NOTREACHED*/

	    case CTRL('s'):
		do {
		    c = cnmaygetc();
		    if (c == CTRL('c'))
			db_error((char *)0);
		} while (c != CTRL('q'));
		break;

	    default:
		/* drop on floor */
		break;
	}
}

cnmaygetc ()
{
	return (-1);
}

/* called from kdb_trap in db_interface.c */
cnpollc (flag)
{
}	
