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
 * $Log: db_lex.c,v $
 * Revision 1.1  1992/03/25  21:45:13  pace
 * Initial revision
 *
 * Revision 2.3  91/02/05  17:06:36  mrt
 * 	Changed to new Mach copyright
 * 	[91/01/31  16:18:20  mrt]
 * 
 * Revision 2.2  90/08/27  21:51:10  dbg
 * 	Add 'dotdot' token.
 * 	[90/08/22            dbg]
 * 
 * 	Allow backslash to quote any character into an identifier.
 * 	Allow colon in identifier for symbol table qualification.
 * 	[90/08/16            dbg]
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
/*
 * Lexical analyzer.
 */
#include "db_lex.h"

char	db_line[120];
char *	db_lp, *db_endlp;

int
db_read_line()
{
	int	i;

	i = db_readline(db_line, sizeof(db_line));
	if (i == 0)
	    return (0);	/* EOI */
	db_lp = db_line;
	db_endlp = db_lp + i;
	return (i);
}

void
db_flush_line()
{
	db_lp = db_line;
	db_endlp = db_line;
}

int	db_look_char = 0;

int
db_read_char()
{
	int	c;

	if (db_look_char != 0) {
	    c = db_look_char;
	    db_look_char = 0;
	}
	else if (db_lp >= db_endlp)
	    c = -1;
	else 
	    c = *db_lp++;
	return (c);
}

void
db_unread_char(c)
{
	db_look_char = c;
}

int	db_look_token = 0;

void
db_unread_token(t)
	int	t;
{
	db_look_token = t;
}

int
db_read_token()
{
	int	t;

	if (db_look_token) {
	    t = db_look_token;
	    db_look_token = 0;
	}
	else
	    t = db_lex();
	return (t);
}

int	db_tok_number;
char	db_tok_string[TOK_STRING_SIZE];

int	db_radix = 16;

void
db_flush_lex()
{
	db_flush_line();
	db_look_char = 0;
	db_look_token = 0;
}

int
db_lex()
{
	int	c;

	c = db_read_char();
	while (c <= ' ' || c > '~') {
	    if (c == '\n' || c == -1)
		return (tEOL);
	    c = db_read_char();
	}

	if (c >= '0' && c <= '9') {
	    /* number */
	    int	r, digit;

	    if (c > '0')
		r = db_radix;
	    else {
		c = db_read_char();
		if (c == 'O' || c == 'o')
		    r = 8;
		else if (c == 'T' || c == 't')
		    r = 10;
		else if (c == 'X' || c == 'x')
		    r = 16;
		else {
		    r = db_radix;
		    db_unread_char(c);
		}
		c = db_read_char();
	    }
	    db_tok_number = 0;
	    for (;;) {
		if (c >= '0' && c <= ((r == 8) ? '7' : '9'))
		    digit = c - '0';
		else if (r == 16 && ((c >= 'A' && c <= 'F') ||
				     (c >= 'a' && c <= 'f'))) {
		    if (c >= 'a')
			digit = c - 'a' + 10;
		    else if (c >= 'A')
			digit = c - 'A' + 10;
		}
		else
		    break;
		db_tok_number = db_tok_number * r + digit;
		c = db_read_char();
	    }
	    if ((c >= '0' && c <= '9') ||
		(c >= 'A' && c <= 'Z') ||
		(c >= 'a' && c <= 'z') ||
		(c == '_'))
	    {
		db_error("Bad character in number\n");
		db_flush_lex();
		return (tEOF);
	    }
	    db_unread_char(c);
	    return (tNUMBER);
	}
	if ((c >= 'A' && c <= 'Z') ||
	    (c >= 'a' && c <= 'z') ||
	    c == '_' || c == '\\')
	{
	    /* string */
	    char *cp;

	    cp = db_tok_string;
	    if (c == '\\') {
		c = db_read_char();
		if (c == '\n' || c == -1)
		    db_error("Bad escape\n");
	    }
	    *cp++ = c;
	    while (1) {
		c = db_read_char();
		if ((c >= 'A' && c <= 'Z') ||
		    (c >= 'a' && c <= 'z') ||
		    (c >= '0' && c <= '9') ||
		    c == '_' || c == '\\' || c == ':')
		{
		    if (c == '\\') {
			c = db_read_char();
			if (c == '\n' || c == -1)
			    db_error("Bad escape\n");
		    }
		    *cp++ = c;
		    if (cp == db_tok_string+sizeof(db_tok_string)) {
			db_error("String too long\n");
			db_flush_lex();
			return (tEOF);
		    }
		    continue;
		}
		else {
		    *cp = '\0';
		    break;
		}
	    }
	    db_unread_char(c);
	    return (tIDENT);
	}

	switch (c) {
	    case '+':
		return (tPLUS);
	    case '-':
		return (tMINUS);
	    case '.':
		c = db_read_char();
		if (c == '.')
		    return (tDOTDOT);
		db_unread_char(c);
		return (tDOT);
	    case '*':
		return (tSTAR);
	    case '/':
		return (tSLASH);
	    case '=':
		return (tEQ);
	    case '%':
		return (tPCT);
	    case '#':
		return (tHASH);
	    case '(':
		return (tLPAREN);
	    case ')':
		return (tRPAREN);
	    case ',':
		return (tCOMMA);
	    case '"':
		return (tDITTO);
	    case '$':
		return (tDOLLAR);
	    case '!':
		return (tEXCL);
	    case '<':
		c = db_read_char();
		if (c == '<')
		    return (tSHIFT_L);
		db_unread_char(c);
		break;
	    case '>':
		c = db_read_char();
		if (c == '>')
		    return (tSHIFT_R);
		db_unread_char(c);
		break;
	    case -1:
		return (tEOF);
	}
	db_printf("Bad character\n");
	db_flush_lex();
	return (tEOF);
}
