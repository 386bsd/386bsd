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
 * $Log: db_examine.c,v $
 * Revision 1.1  1992/03/25  21:45:07  pace
 * Initial revision
 *
 * Revision 2.4  91/02/05  17:06:20  mrt
 * 	Changed to new Mach copyright
 * 	[91/01/31  16:17:37  mrt]
 * 
 * Revision 2.3  90/11/07  16:49:23  rpd
 * 	Added db_search_cmd, db_search.
 * 	[90/11/06            rpd]
 * 
 * Revision 2.2  90/08/27  21:50:38  dbg
 * 	Add 'r', 'z' to print and examine formats.
 * 	Change calling sequence of db_disasm.
 * 	db_examine sets db_prev and db_next instead of explicitly
 * 	advancing dot.
 * 	[90/08/20            dbg]
 * 	Reflected changes in db_printsym()'s calling seq.
 * 	[90/08/20            af]
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
#include "machine/db/db_machdep.h"		/* type definitions */

#include "db_lex.h"
#include "db_output.h"
#include "db_command.h"
#include "db_sym.h"

char	db_examine_format[TOK_STRING_SIZE] = "x";

extern	db_addr_t db_disasm(/* db_addr_t, boolean_t */);
			/* instruction disassembler */

/*
 * Examine (print) data.
 */
/*ARGSUSED*/
void
db_examine_cmd(addr, have_addr, count, modif)
	db_expr_t	addr;
	int		have_addr;
	db_expr_t	count;
	char *		modif;
{
	if (modif[0] != '\0')
	    db_strcpy(db_examine_format, modif);

	if (count == -1)
	    count = 1;

	db_examine((db_addr_t) addr, db_examine_format, count);
}

db_examine(addr, fmt, count)
	register
	db_addr_t	addr;
	char *		fmt;	/* format string */
	int		count;	/* repeat count */
{
	int		c;
	db_expr_t	value;
	int		size;
	int		width;
	char *		fp;

	while (--count >= 0) {
	    fp = fmt;
	    size = 4;
	    width = 16;
	    while ((c = *fp++) != 0) {
		switch (c) {
		    case 'b':
			size = 1;
			width = 4;
			break;
		    case 'h':
			size = 2;
			width = 8;
			break;
		    case 'l':
			size = 4;
			width = 16;
			break;
		    case 'a':	/* address */
			/* always forces a new line */
			if (db_print_position() != 0)
			    db_printf("\n");
			db_prev = addr;
			db_printsym(addr, DB_STGY_ANY);
			db_printf(":\t");
			break;
		    default:
			if (db_print_position() == 0) {
			    /* If we hit a new symbol, print it */
			    char *	name;
			    db_expr_t	off;

			    /*db_find_sym_and_offset(addr, &name, &off);
			    if (off == 0)
				db_printf("%s:\t", name);
			    else
				db_printf("\t\t");*/
			db_printsym(addr, DB_STGY_ANY);
			db_printf(":");
			while(db_print_position() < 15)
				db_putchar(' ');

			    db_prev = addr;
			}

			switch (c) {
			    case 'r':	/* signed, current radix */
				value = db_get_value(addr, size, TRUE);
				addr += size;
				db_printf("%-*r", width, value);
				break;
			    case 'x':	/* unsigned hex */
				value = db_get_value(addr, size, FALSE);
				addr += size;
				width = size * 2 + 1;
				db_printf("%-*x", width, value);
				break;
			    case 'z':	/* signed hex */
				value = db_get_value(addr, size, TRUE);
				addr += size;
				width = size * 2 + 2;
				db_printf("%-*z", width, value);
				break;
			    case 'd':	/* signed decimal */
				value = db_get_value(addr, size, TRUE);
				addr += size;
				width = size * 3 + 2;
				db_printf("%-*d", width, value);
				break;
			    case 'u':	/* unsigned decimal */
				value = db_get_value(addr, size, FALSE);
				addr += size;
				width = size * 3 + 1;
				db_printf("%-*u", width, value);
				break;
			    case 'o':	/* unsigned octal */
				value = db_get_value(addr, size, FALSE);
				addr += size;
				width = size * 3 + 1;
				db_printf("%-*o", width, value);
				break;
			    case 'c':	/* character */
				value = db_get_value(addr, 1, FALSE);
				addr += 1;
				if (value >= ' ' && value <= '~') {
				    db_printf("%c", value);
				    width = 1;
				} else {
				    db_printf("\\%03o", value);
				    width = 4;
				}
				break;
			    case 's':	/* null-terminated string */
				for (;;) {
				    value = db_get_value(addr, 1, FALSE);
				    addr += 1;
				    if (value == 0)
					break;
				    if (value >= ' ' && value <= '~')
					db_printf("%c", value);
				    else
					db_printf("\\%03o", value);
				}
				break;
			    case 'i':	/* instruction */
				addr = db_disasm(addr, FALSE);
				break;
			    case 'I':	/* instruction, alternate form */
				addr = db_disasm(addr, TRUE);
				break;
			    default:
				break;
			}
			/*if (db_print_position() != 0)*/
			    db_end_line(width);
			break;
		}
	    }
	}
	db_next = addr;
}

/*
 * Print value.
 */
char	db_print_format = 'x';

/*ARGSUSED*/
void
db_print_cmd(addr, have_addr, count, modif)
	db_expr_t	addr;
	int		have_addr;
	db_expr_t	count;
	char *		modif;
{
	db_expr_t	value;

	if (modif[0] != '\0')
	    db_print_format = modif[0];

	switch (db_print_format) {
	    case 'a':
		db_printsym((db_addr_t)addr, DB_STGY_ANY);
		break;
	    case 'r':
		db_printf("%11r", addr);
		break;
	    case 'x':
		db_printf("%8x", addr);
		break;
	    case 'z':
		db_printf("%8z", addr);
		break;
	    case 'd':
		db_printf("%11d", addr);
		break;
	    case 'u':
		db_printf("%11u", addr);
		break;
	    case 'o':
		db_printf("%16o", addr);
		break;
	    case 'c':
		value = addr & 0xFF;
		if (value >= ' ' && value <= '~')
		    db_printf("%c", value);
		else
		    db_printf("\\%03o", value);
		break;
	}
	db_printf("\n");
}

db_print_loc_and_inst(loc)
	db_addr_t	loc;
{
	db_printsym(loc, DB_STGY_PROC);
	db_printf(":\t");
	(void) db_disasm(loc, TRUE);
}

db_strcpy(dst, src)
	register char *dst;
	register char *src;
{
	while (*dst++ = *src++)
	    ;
}

/*
 * Search for a value in memory.
 * Syntax: search [/bhl] addr value [mask] [,count]
 */
void
db_search_cmd()
{
	int		t;
	db_addr_t	addr;
	int		size;
	db_expr_t	value;
	db_expr_t	mask;
	unsigned int	count;

	t = db_read_token();
	if (t == tSLASH) {
	    t = db_read_token();
	    if (t != tIDENT) {
	      bad_modifier:
		db_printf("Bad modifier\n");
		db_flush_lex();
		return;
	    }

	    if (!strcmp(db_tok_string, "b"))
		size = 1;
	    else if (!strcmp(db_tok_string, "h"))
		size = 2;
	    else if (!strcmp(db_tok_string, "l"))
		size = 4;
	    else
		goto bad_modifier;
	} else {
	    db_unread_token(t);
	    size = 4;
	}

	if (!db_expression(&addr)) {
	    db_printf("Address missing\n");
	    db_flush_lex();
	    return;
	}

	if (!db_expression(&value)) {
	    db_printf("Value missing\n");
	    db_flush_lex();
	    return;
	}

	if (!db_expression(&mask))
	    mask = 0xffffffff;

	t = db_read_token();
	if (t == tCOMMA) {
	    if (!db_expression(&count)) {
		db_printf("Count missing\n");
		db_flush_lex();
		return;
	    }
	} else {
	    db_unread_token(t);
	    count = -1;		/* effectively forever */
	}
	db_skip_to_eol();

	db_search(addr, size, value, mask, count);
}

db_search(addr, size, value, mask, count)
	register
	db_addr_t	addr;
	int		size;
	db_expr_t	value;
	db_expr_t	mask;
	unsigned int	count;
{
	while (count-- != 0) {
		db_prev = addr;
		if ((db_get_value(addr, size, FALSE) & mask) == value)
			break;
		addr += size;
	}
	db_next = addr;
}
