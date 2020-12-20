%{
/*
 * Copyright (c) 1988-1990 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Grammar for tcpdump.
 */
#ifndef lint
static char rcsid[] =
    "@(#) $Header: tcpgram.y,v 1.24 91/06/06 22:36:04 mccanne Exp $ (LBL)";
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>

#include "interface.h"

#include <sys/time.h>
#include <net/bpf.h>

#include "gencode.h"

static struct qual qualifier;
static struct qual default_qual = { 0, 0, 0 };

int n_errors = 0;

static void
yyerror()
{
	++n_errors;
}

%}

%union {
	int i;
	u_long h;
	u_char *e;
	char *s;
	struct stmt *stmt;
	struct block *blk;
	struct arth *a;
}

%type	<blk>	expr id nid pid term rterm other qid
%type	<a>	arth narth
%type	<i>	byteop pname pnum relop irelop

%token  DST SRC HOST GATEWAY
%token  NET PORT LESS GREATER PROTO BYTE
%token  ARP RARP IP TCP UDP ICMP
%token  BROADCAST
%token  NUM
%token  ETHER
%token	GEQ LEQ NEQ
%token	ID EID HID
%token	LSH RSH
%token  LEN

%type	<s> ID
%type	<e> EID
%type	<h> HID
%type	<i> NUM

%left OR AND
%nonassoc  '!'
%left '|'
%left '&' 
%left LSH RSH
%left '+' '-'
%left '*' '/'
%nonassoc UMINUS
%%
prog:	  expr
{ 
	finish_parse($1);
}
	| /* null */
	;
expr:	  term
	| expr AND term		{ gen_and($1, $$ = $3); }
	| expr OR term		{ gen_or($1, $$ = $3); }
	| expr AND id		{ gen_and($1, $$ = $3); }
	| expr OR id		{ gen_or($1, $$ = $3); }
	;
id:	  nid
	| pnum			{ $$ = gen_ncode((u_long)$1, qualifier); }
	| '(' pid ')'		{ $$ = $2; }
	;
nid:	  ID			{ $$ = gen_scode($1, qualifier); }
	| HID			{ $$ = gen_ncode($1, qualifier); }
	| EID			{ $$ = gen_ecode($1, qualifier); }
	| '!' id		{ gen_not($2); $$ = $2; }
	;
pid:	  nid
	| qid AND id		{ gen_and($1, $$ = $3); }
	| qid OR id		{ gen_or($1, $$ = $3); }
	;
qid:	  pnum			{ $$ = gen_ncode((u_long)$1, qualifier); }
	| pid
	;
term:	  rterm
	| '!' term		{ gen_not($2); $$ = $2; }
	;
head:	  pqual dqual aqual
	| pqual dqual
	| pqual aqual
	| pqual PROTO		{ qualifier.primary = Q_PROTO; }
	| pqual ndaqual
	;
rterm:	  head id		{ $$ = $2; }
	| '(' expr ')'		{ $$ = $2; qualifier = default_qual; }
	| pname			{ $$ = gen_proto_abbrev($1); 
				  qualifier = default_qual; }
	| arth relop arth	{ $$ = gen_relation($2, $1, $3, 0);
				  qualifier = default_qual; }
	| arth irelop arth	{ $$ = gen_relation($2, $1, $3, 1);
				  qualifier = default_qual; }
	| other
	;
/* protocol level qualifiers */
pqual:	  pname			{ qualifier.protocol = $1; }
	|			{ qualifier.protocol = 0; }
	;
/* 'direction' qualifiers */
dqual:	  SRC			{ qualifier.dir = Q_SRC; }
	| DST			{ qualifier.dir = Q_DST; }
	| SRC OR DST		{ qualifier.dir = Q_OR; }
	| DST OR SRC		{ qualifier.dir = Q_OR; }
	| SRC AND DST		{ qualifier.dir = Q_AND; }
	| DST AND SRC		{ qualifier.dir = Q_AND; }
	;
/* address type qualifiers */
aqual:	  HOST			{ qualifier.primary = Q_HOST; }
	| NET			{ qualifier.primary = Q_NET; }
	| PORT			{ qualifier.primary = Q_PORT; }
	;
/* non-directional address type qualifiers */
ndaqual:  GATEWAY 		{ qualifier.primary = Q_GATEWAY; }
	;
pname:	  ETHER			{ $$ = Q_ETHER; }
	| IP			{ $$ = Q_IP; }
	| ARP			{ $$ = Q_ARP; }
	| RARP			{ $$ = Q_RARP; }
	| TCP			{ $$ = Q_TCP; }
	| UDP			{ $$ = Q_UDP; }
	| ICMP			{ $$ = Q_ICMP; }
	;
other:	  BROADCAST		{ $$ = gen_broadcast(); }
	| LESS NUM		{ $$ = gen_less($2); }
	| GREATER NUM		{ $$ = gen_greater($2); }
	| BYTE NUM byteop NUM	{ $$ = gen_byteop($3, $2, $4); }
	;
relop:	  '>'			{ $$ = BPF_JGT; }
	| GEQ			{ $$ = BPF_JGE; }
	| '='			{ $$ = BPF_JEQ; }
	;
irelop:	  LEQ			{ $$ = BPF_JGT; }
	| '<'			{ $$ = BPF_JGE; }
	| NEQ			{ $$ = BPF_JEQ; }
	;
arth:	  pnum			{ $$ = gen_loadi($1); }
	| narth
	;
narth:	  pname '[' arth ']'		{ $$ = gen_load($1, $3, 1); }
	| pname '[' arth ':' NUM ']'	{ $$ = gen_load($1, $3, $5); }
	| arth '+' arth			{ $$ = gen_arth(BPF_ADD, $1, $3); }
	| arth '-' arth			{ $$ = gen_arth(BPF_SUB, $1, $3); }
	| arth '*' arth			{ $$ = gen_arth(BPF_MUL, $1, $3); }
	| arth '/' arth			{ $$ = gen_arth(BPF_DIV, $1, $3); }
	| arth '&' arth			{ $$ = gen_arth(BPF_AND, $1, $3); }
	| arth '|' arth			{ $$ = gen_arth(BPF_OR, $1, $3); }
	| arth LSH arth			{ $$ = gen_arth(BPF_LSH, $1, $3); }
	| arth RSH arth			{ $$ = gen_arth(BPF_RSH, $1, $3); }
	| '-' arth %prec UMINUS		{ $$ = gen_neg($2); }
	| '(' narth ')'			{ $$ = $2; }
	| LEN				{ $$ = gen_loadlen(); }
	;
byteop:	  '&'			{ $$ = '&'; }
	| '|'			{ $$ = '|'; }
	| '<'			{ $$ = '<'; }
	| '>'			{ $$ = '>'; }
	| '='			{ $$ = '='; }
	;
pnum:	  NUM
	| '(' pnum ')'		{ $$ = $2; }
	;
%%
