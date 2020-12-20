#ifndef lint
static char yysccsid[] = "@(#)yaccpar	1.8 (Berkeley) 01/20/90";
#endif
#define YYBYACC 1
#line 2 "tcpgram.y"
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

#line 56 "tcpgram.y"
typedef union {
	int i;
	u_long h;
	u_char *e;
	char *s;
	struct stmt *stmt;
	struct block *blk;
	struct arth *a;
} YYSTYPE;
#line 69 "y.tab.c"
#define DST 257
#define SRC 258
#define HOST 259
#define GATEWAY 260
#define NET 261
#define PORT 262
#define LESS 263
#define GREATER 264
#define PROTO 265
#define BYTE 266
#define ARP 267
#define RARP 268
#define IP 269
#define TCP 270
#define UDP 271
#define ICMP 272
#define BROADCAST 273
#define NUM 274
#define ETHER 275
#define GEQ 276
#define LEQ 277
#define NEQ 278
#define ID 279
#define EID 280
#define HID 281
#define LSH 282
#define RSH 283
#define LEN 284
#define OR 285
#define AND 286
#define UMINUS 287
#define YYERRCODE 256
short yylhs[] = {                                        -1,
    0,    0,    1,    1,    1,    1,    1,    2,    2,    2,
    3,    3,    3,    3,    4,    4,    4,    8,    8,    5,
    5,   16,   16,   16,   16,   16,    6,    6,    6,    6,
    6,    6,   17,   17,   18,   18,   18,   18,   18,   18,
   19,   19,   19,   20,   12,   12,   12,   12,   12,   12,
   12,    7,    7,    7,    7,   14,   14,   14,   15,   15,
   15,    9,    9,   10,   10,   10,   10,   10,   10,   10,
   10,   10,   10,   10,   10,   10,   11,   11,   11,   11,
   11,   13,   13,
};
short yylen[] = {                                         2,
    1,    0,    1,    3,    3,    3,    3,    1,    1,    3,
    1,    1,    1,    2,    1,    3,    3,    1,    1,    1,
    2,    3,    2,    2,    2,    2,    2,    3,    1,    3,
    3,    1,    1,    0,    1,    1,    3,    3,    3,    3,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    2,    2,    4,    1,    1,    1,    1,    1,
    1,    1,    1,    4,    6,    3,    3,    3,    3,    3,
    3,    3,    3,    2,    3,    1,    1,    1,    1,    1,
    1,    1,    3,
};
short yydefred[] = {                                      0,
    0,    0,    0,   47,   48,   46,   49,   50,   51,   52,
   82,   45,   76,    0,    0,    0,    0,    0,    3,   20,
   32,    0,   63,    0,   62,    0,    0,   53,   54,    0,
   21,    0,   74,    0,    0,    0,    0,    0,    0,   57,
   59,   61,    0,    0,    0,    0,    0,    0,    0,    0,
   56,   58,   60,    0,    0,    0,   11,   13,   12,    0,
    0,   27,    8,    9,    0,    0,   41,   44,   42,   43,
   25,    0,   24,   26,   78,   77,   80,   81,   79,    0,
    0,   28,   75,   83,    0,    0,    7,    5,    0,    6,
    4,    0,    0,    0,    0,    0,    0,   68,   69,    0,
    0,    0,   14,    0,   15,    0,    0,    0,    0,    0,
    0,    0,   22,   55,    0,   64,    0,    0,   10,    0,
    0,   38,   40,   37,   39,    0,   17,   16,   65,
};
short yydgoto[] = {                                      17,
   35,  103,   63,  106,   19,   20,   21,  107,   22,   23,
   80,   34,   25,   54,   55,   26,   27,   72,   73,   74,
};
short yysindex[] = {                                    131,
 -271, -263, -261,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,  131,  240,  131,    0, -268,    0,    0,
    0,  407,    0,  -72,    0,  151,  -36,    0,    0,  -30,
    0,  240,    0,  -72,  -34,  -19,  -17,  -31,  -31,    0,
    0,    0,  240,  240,  240,  240,  240,  240,  240,  240,
    0,    0,    0,  240,  240,  240,    0,    0,    0,  151,
  159,    0,    0,    0, -226, -206,    0,    0,    0,    0,
    0, -119,    0,    0,    0,    0,    0,    0,    0, -247,
  -22,    0,    0,    0,  -31,  112,    0,    0,    0,    0,
    0,  120,  120,  173,  106,  -37,  -37,    0,    0,  -22,
  -22,  442,    0,  -40,    0,   -7, -195,  -17, -222, -220,
 -216, -212,    0,    0,  -17,    0, -209,  -17,    0,  151,
  151,    0,    0,    0,    0,  -41,    0,    0,    0,
};
short yyrindex[] = {                                    108,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,   99,    0,   99,    0,   54,    0,    0,
    0,    0,    0,    6,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,  415,  436,   99,   99,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,   -5,  148,    0,    0,    0,    0,
    0,  167,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,   99,   99,    0,    0,    1,    0,
    0,   37,   51,   77,   66,   12,   26,    0,    0,   15,
   40,    0,    0,    0,    0, -185,    0, -171,    0,    0,
    0,    0,    0,    0,  135,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,
};
short yygindex[] = {                                      0,
   68,   84,  -57,    0,   44,    0,    0,    0,  508,   17,
    0,  117,  487,    0,    0,    0,    0,    0,    4,    0,
};
#define YYTABLESIZE 725
short yytable[] = {                                     104,
    9,   85,   28,  105,   49,   29,   82,   76,   86,   50,
   29,   66,   30,   15,   30,   46,   38,   39,   56,   49,
   47,   83,   48,   84,   50,   67,  114,   36,  105,   79,
   78,   77,   36,  119,   36,  122,   72,  123,   62,   31,
  124,    9,   62,   62,  125,   62,   29,   62,   36,   66,
   73,  129,   66,    1,   66,   30,   66,   31,  109,  110,
   62,   62,   62,   67,  126,   70,   67,   18,   67,   66,
   67,   66,   66,   66,   72,  113,   71,   72,  111,  112,
   31,   88,   91,   67,    0,   67,   67,   67,   73,  120,
  121,   73,    0,   75,   72,    0,   72,   72,   72,   19,
   19,   45,   36,   70,   66,    0,   70,    2,   73,   62,
   73,   73,   73,   18,   18,    0,   24,   71,   67,    0,
    0,   87,   90,   70,   62,   70,   70,   70,   31,   72,
   24,    0,   24,    0,   71,   66,   71,   71,   71,   67,
    0,   69,   70,   73,   85,    0,    0,   49,   47,   67,
   48,   16,   50,    0,   24,   24,   15,    0,   70,    0,
   72,   49,   47,   14,   48,    0,   50,    0,    0,   71,
   16,    0,   62,    0,   73,   15,   62,   62,    0,   62,
   35,   62,    0,   60,    0,    0,    0,   35,    0,   70,
   61,   60,    0,    0,   62,   62,   62,    0,  104,   23,
   71,   24,   24,  127,  128,    0,   23,    0,    0,    0,
   46,    0,    0,    0,   49,   47,    0,   48,    0,   50,
   65,   66,   67,   68,   69,   70,    0,    0,   71,    0,
    0,    1,    2,   11,    3,    4,    5,    6,    7,    8,
    9,   10,   11,   12,    0,    0,    0,   57,   58,   59,
   38,   39,   13,   36,    0,   36,   36,    0,   62,   43,
   44,    0,   33,   33,   33,   33,   33,   33,   36,    0,
   33,    0,    0,   36,   36,   36,   62,   62,   62,   32,
    0,    0,   62,   62,   15,    9,    9,   66,   66,   66,
   29,   29,    0,   66,   66,    0,   66,   66,    0,   30,
   30,   67,   67,   67,    0,    0,    0,   67,   67,    0,
   67,   67,   72,   72,   72,    0,    0,    0,   72,   72,
    0,   72,   72,    0,   31,   31,   73,   73,   73,    0,
    0,    0,   73,   73,    0,   73,   73,    0,    0,    0,
    0,   70,   70,   70,    0,    0,    0,    0,    0,    0,
   70,   70,   71,   71,   71,   34,   34,   34,   34,   34,
   34,   71,   71,   34,   34,   34,   34,   34,   34,   34,
    0,    0,   34,    0,    1,    2,    0,    3,    4,    5,
    6,    7,    8,    9,   10,   11,   12,   43,   44,    0,
   57,   58,   59,    1,    2,   13,    3,    4,    5,    6,
    7,    8,    9,   10,   11,   12,   35,    0,   35,   35,
   62,   62,   62,    0,   13,    0,   62,   62,    0,   18,
   18,   35,    0,    0,   11,    0,   35,   35,   35,   57,
   58,   59,   11,    0,    0,    0,    0,   57,   58,   59,
   23,    0,    0,    0,   46,   23,   23,   23,   49,   47,
    0,   48,   63,   50,   43,   44,   63,   63,    0,   63,
    0,   63,    0,    0,    0,    0,   53,   52,   51,    0,
    0,    0,    0,   62,   63,   63,   63,   62,   62,   46,
   62,    0,   62,   49,   47,    0,   48,    0,   50,    0,
    0,    0,    0,    0,    0,   62,   62,   62,    0,  117,
    0,    0,   37,    0,    0,    0,    4,    5,    6,    7,
    8,    9,   64,   11,   12,    0,    0,    0,   37,    0,
    0,    0,   33,   13,   89,   89,    0,    0,    0,    0,
   45,    0,    0,    0,  116,    0,    0,    0,   63,   81,
    0,    0,    0,    0,    0,    0,   64,  108,    0,    0,
   92,   93,   94,   95,   96,   97,   98,   99,    0,   62,
    0,  100,  101,  102,    0,   45,    0,    0,    0,    0,
    0,   89,  115,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
  118,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,   64,   64,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,   40,   41,   42,    0,    0,    0,   43,   44,
   63,   63,   63,    0,    0,    0,   63,   63,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,   62,   62,   62,    0,    0,    0,   62,   62,    0,
    0,    0,    0,   43,   44,
};
short yycheck[] = {                                      40,
    0,   33,  274,   61,   42,    0,   41,   38,   40,   47,
  274,    0,  274,   45,    0,   38,  285,  286,   91,   42,
   43,   41,   45,   41,   47,    0,  274,   33,   86,   60,
   61,   62,   16,   41,   40,  258,    0,  258,   38,    0,
  257,   41,   42,   43,  257,   45,   41,   47,   32,   38,
    0,   93,   41,    0,   43,   41,   45,   14,  285,  286,
   60,   61,   62,   38,  274,    0,   41,    0,   43,   58,
   45,   60,   61,   62,   38,   72,    0,   41,  285,  286,
   41,   38,   39,   58,   -1,   60,   61,   62,   38,  285,
  286,   41,   -1,  124,   58,   -1,   60,   61,   62,  285,
  286,  124,   86,   38,   93,   -1,   41,    0,   58,   26,
   60,   61,   62,  285,  286,   -1,    0,   41,   93,   -1,
   -1,   38,   39,   58,  124,   60,   61,   62,   85,   93,
   14,   -1,   16,   -1,   58,  124,   60,   61,   62,  259,
   -1,  261,  262,   93,   33,   -1,   -1,   42,   43,  124,
   45,   40,   47,   -1,   38,   39,   45,   -1,   93,   -1,
  124,   42,   43,   33,   45,   -1,   47,   -1,   -1,   93,
   40,   -1,   38,   -1,  124,   45,   42,   43,   -1,   45,
   33,   47,   -1,   33,   -1,   -1,   -1,   40,   -1,  124,
   40,   33,   -1,   -1,   60,   61,   62,   -1,   40,   33,
  124,   85,   86,  120,  121,   -1,   40,   -1,   -1,   -1,
   38,   -1,   -1,   -1,   42,   43,   -1,   45,   -1,   47,
  257,  258,  259,  260,  261,  262,   -1,   -1,  265,   -1,
   -1,  263,  264,  274,  266,  267,  268,  269,  270,  271,
  272,  273,  274,  275,   -1,   -1,   -1,  279,  280,  281,
  285,  286,  284,  259,   -1,  261,  262,   -1,  124,  282,
  283,   -1,  257,  258,  259,  260,  261,  262,  274,   -1,
  265,   -1,   -1,  279,  280,  281,  276,  277,  278,   40,
   -1,   -1,  282,  283,   45,  285,  286,  276,  277,  278,
  285,  286,   -1,  282,  283,   -1,  285,  286,   -1,  285,
  286,  276,  277,  278,   -1,   -1,   -1,  282,  283,   -1,
  285,  286,  276,  277,  278,   -1,   -1,   -1,  282,  283,
   -1,  285,  286,   -1,  285,  286,  276,  277,  278,   -1,
   -1,   -1,  282,  283,   -1,  285,  286,   -1,   -1,   -1,
   -1,  276,  277,  278,   -1,   -1,   -1,   -1,   -1,   -1,
  285,  286,  276,  277,  278,  257,  258,  259,  260,  261,
  262,  285,  286,  265,  257,  258,  259,  260,  261,  262,
   -1,   -1,  265,   -1,  263,  264,   -1,  266,  267,  268,
  269,  270,  271,  272,  273,  274,  275,  282,  283,   -1,
  279,  280,  281,  263,  264,  284,  266,  267,  268,  269,
  270,  271,  272,  273,  274,  275,  259,   -1,  261,  262,
  276,  277,  278,   -1,  284,   -1,  282,  283,   -1,  285,
  286,  274,   -1,   -1,  274,   -1,  279,  280,  281,  279,
  280,  281,  274,   -1,   -1,   -1,   -1,  279,  280,  281,
  274,   -1,   -1,   -1,   38,  279,  280,  281,   42,   43,
   -1,   45,   38,   47,  282,  283,   42,   43,   -1,   45,
   -1,   47,   -1,   -1,   -1,   -1,   60,   61,   62,   -1,
   -1,   -1,   -1,   38,   60,   61,   62,   42,   43,   38,
   45,   -1,   47,   42,   43,   -1,   45,   -1,   47,   -1,
   -1,   -1,   -1,   -1,   -1,   60,   61,   62,   -1,   58,
   -1,   -1,   16,   -1,   -1,   -1,  267,  268,  269,  270,
  271,  272,   26,  274,  275,   -1,   -1,   -1,   32,   -1,
   -1,   -1,   15,  284,   38,   39,   -1,   -1,   -1,   -1,
  124,   -1,   -1,   -1,   93,   -1,   -1,   -1,  124,   32,
   -1,   -1,   -1,   -1,   -1,   -1,   60,   61,   -1,   -1,
   43,   44,   45,   46,   47,   48,   49,   50,   -1,  124,
   -1,   54,   55,   56,   -1,  124,   -1,   -1,   -1,   -1,
   -1,   85,   86,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
  104,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,  120,  121,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,  276,  277,  278,   -1,   -1,   -1,  282,  283,
  276,  277,  278,   -1,   -1,   -1,  282,  283,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,  276,  277,  278,   -1,   -1,   -1,  282,  283,   -1,
   -1,   -1,   -1,  282,  283,
};
#define YYFINAL 17
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#define YYMAXTOKEN 287
#if YYDEBUG
char *yyname[] = {
"end-of-file",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
"'!'",0,0,0,0,"'&'",0,"'('","')'","'*'","'+'",0,"'-'",0,"'/'",0,0,0,0,0,0,0,0,0,
0,"':'",0,"'<'","'='","'>'",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,"'['",0,"']'",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
"'|'",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"DST","SRC","HOST","GATEWAY","NET","PORT","LESS",
"GREATER","PROTO","BYTE","ARP","RARP","IP","TCP","UDP","ICMP","BROADCAST","NUM",
"ETHER","GEQ","LEQ","NEQ","ID","EID","HID","LSH","RSH","LEN","OR","AND",
"UMINUS",
};
char *yyrule[] = {
"$accept : prog",
"prog : expr",
"prog :",
"expr : term",
"expr : expr AND term",
"expr : expr OR term",
"expr : expr AND id",
"expr : expr OR id",
"id : nid",
"id : pnum",
"id : '(' pid ')'",
"nid : ID",
"nid : HID",
"nid : EID",
"nid : '!' id",
"pid : nid",
"pid : qid AND id",
"pid : qid OR id",
"qid : pnum",
"qid : pid",
"term : rterm",
"term : '!' term",
"head : pqual dqual aqual",
"head : pqual dqual",
"head : pqual aqual",
"head : pqual PROTO",
"head : pqual ndaqual",
"rterm : head id",
"rterm : '(' expr ')'",
"rterm : pname",
"rterm : arth relop arth",
"rterm : arth irelop arth",
"rterm : other",
"pqual : pname",
"pqual :",
"dqual : SRC",
"dqual : DST",
"dqual : SRC OR DST",
"dqual : DST OR SRC",
"dqual : SRC AND DST",
"dqual : DST AND SRC",
"aqual : HOST",
"aqual : NET",
"aqual : PORT",
"ndaqual : GATEWAY",
"pname : ETHER",
"pname : IP",
"pname : ARP",
"pname : RARP",
"pname : TCP",
"pname : UDP",
"pname : ICMP",
"other : BROADCAST",
"other : LESS NUM",
"other : GREATER NUM",
"other : BYTE NUM byteop NUM",
"relop : '>'",
"relop : GEQ",
"relop : '='",
"irelop : LEQ",
"irelop : '<'",
"irelop : NEQ",
"arth : pnum",
"arth : narth",
"narth : pname '[' arth ']'",
"narth : pname '[' arth ':' NUM ']'",
"narth : arth '+' arth",
"narth : arth '-' arth",
"narth : arth '*' arth",
"narth : arth '/' arth",
"narth : arth '&' arth",
"narth : arth '|' arth",
"narth : arth LSH arth",
"narth : arth RSH arth",
"narth : '-' arth",
"narth : '(' narth ')'",
"narth : LEN",
"byteop : '&'",
"byteop : '|'",
"byteop : '<'",
"byteop : '>'",
"byteop : '='",
"pnum : NUM",
"pnum : '(' pnum ')'",
};
#endif
#define yyclearin (yychar=(-1))
#define yyerrok (yyerrflag=0)
#ifdef YYSTACKSIZE
#ifndef YYMAXDEPTH
#define YYMAXDEPTH YYSTACKSIZE
#endif
#else
#ifdef YYMAXDEPTH
#define YYSTACKSIZE YYMAXDEPTH
#else
#define YYSTACKSIZE 500
#define YYMAXDEPTH 500
#endif
#endif
int yydebug;
int yynerrs;
int yyerrflag;
int yychar;
short *yyssp;
YYSTYPE *yyvsp;
YYSTYPE yyval;
YYSTYPE yylval;
short yyss[YYSTACKSIZE];
YYSTYPE yyvs[YYSTACKSIZE];
#define yystacksize YYSTACKSIZE
#define YYABORT goto yyabort
#define YYACCEPT goto yyaccept
#define YYERROR goto yyerrlab
int
yyparse()
{
    register int yym, yyn, yystate;
#if YYDEBUG
    register char *yys;
    extern char *getenv();

    if (yys = getenv("YYDEBUG"))
    {
        yyn = *yys;
        if (yyn >= '0' && yyn <= '9')
            yydebug = yyn - '0';
    }
#endif

    yynerrs = 0;
    yyerrflag = 0;
    yychar = (-1);

    yyssp = yyss;
    yyvsp = yyvs;
    *yyssp = yystate = 0;

yyloop:
    if (yyn = yydefred[yystate]) goto yyreduce;
    if (yychar < 0)
    {
        if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("yydebug: state %d, reading %d (%s)\n", yystate,
                    yychar, yys);
        }
#endif
    }
    if ((yyn = yysindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
#if YYDEBUG
        if (yydebug)
            printf("yydebug: state %d, shifting to state %d\n",
                    yystate, yytable[yyn]);
#endif
        if (yyssp >= yyss + yystacksize - 1)
        {
            goto yyoverflow;
        }
        *++yyssp = yystate = yytable[yyn];
        *++yyvsp = yylval;
        yychar = (-1);
        if (yyerrflag > 0)  --yyerrflag;
        goto yyloop;
    }
    if ((yyn = yyrindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
        yyn = yytable[yyn];
        goto yyreduce;
    }
    if (yyerrflag) goto yyinrecovery;
#ifdef lint
    goto yynewerror;
#endif
yynewerror:
    yyerror("syntax error");
#ifdef lint
    goto yyerrlab;
#endif
yyerrlab:
    ++yynerrs;
yyinrecovery:
    if (yyerrflag < 3)
    {
        yyerrflag = 3;
        for (;;)
        {
            if ((yyn = yysindex[*yyssp]) && (yyn += YYERRCODE) >= 0 &&
                    yyn <= YYTABLESIZE && yycheck[yyn] == YYERRCODE)
            {
#if YYDEBUG
                if (yydebug)
                    printf("yydebug: state %d, error recovery shifting\
 to state %d\n", *yyssp, yytable[yyn]);
#endif
                if (yyssp >= yyss + yystacksize - 1)
                {
                    goto yyoverflow;
                }
                *++yyssp = yystate = yytable[yyn];
                *++yyvsp = yylval;
                goto yyloop;
            }
            else
            {
#if YYDEBUG
                if (yydebug)
                    printf("yydebug: error recovery discarding state %d\n",
                            *yyssp);
#endif
                if (yyssp <= yyss) goto yyabort;
                --yyssp;
                --yyvsp;
            }
        }
    }
    else
    {
        if (yychar == 0) goto yyabort;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("yydebug: state %d, error recovery discards token %d (%s)\n",
                    yystate, yychar, yys);
        }
#endif
        yychar = (-1);
        goto yyloop;
    }
yyreduce:
#if YYDEBUG
    if (yydebug)
        printf("yydebug: state %d, reducing by rule %d (%s)\n",
                yystate, yyn, yyrule[yyn]);
#endif
    yym = yylen[yyn];
    yyval = yyvsp[1-yym];
    switch (yyn)
    {
case 1:
#line 96 "tcpgram.y"
{ 
	finish_parse(yyvsp[0].blk);
}
break;
case 4:
#line 102 "tcpgram.y"
{ gen_and(yyvsp[-2].blk, yyval.blk = yyvsp[0].blk); }
break;
case 5:
#line 103 "tcpgram.y"
{ gen_or(yyvsp[-2].blk, yyval.blk = yyvsp[0].blk); }
break;
case 6:
#line 104 "tcpgram.y"
{ gen_and(yyvsp[-2].blk, yyval.blk = yyvsp[0].blk); }
break;
case 7:
#line 105 "tcpgram.y"
{ gen_or(yyvsp[-2].blk, yyval.blk = yyvsp[0].blk); }
break;
case 9:
#line 108 "tcpgram.y"
{ yyval.blk = gen_ncode((u_long)yyvsp[0].i, qualifier); }
break;
case 10:
#line 109 "tcpgram.y"
{ yyval.blk = yyvsp[-1].blk; }
break;
case 11:
#line 111 "tcpgram.y"
{ yyval.blk = gen_scode(yyvsp[0].s, qualifier); }
break;
case 12:
#line 112 "tcpgram.y"
{ yyval.blk = gen_ncode(yyvsp[0].h, qualifier); }
break;
case 13:
#line 113 "tcpgram.y"
{ yyval.blk = gen_ecode(yyvsp[0].e, qualifier); }
break;
case 14:
#line 114 "tcpgram.y"
{ gen_not(yyvsp[0].blk); yyval.blk = yyvsp[0].blk; }
break;
case 16:
#line 117 "tcpgram.y"
{ gen_and(yyvsp[-2].blk, yyval.blk = yyvsp[0].blk); }
break;
case 17:
#line 118 "tcpgram.y"
{ gen_or(yyvsp[-2].blk, yyval.blk = yyvsp[0].blk); }
break;
case 18:
#line 120 "tcpgram.y"
{ yyval.blk = gen_ncode((u_long)yyvsp[0].i, qualifier); }
break;
case 21:
#line 124 "tcpgram.y"
{ gen_not(yyvsp[0].blk); yyval.blk = yyvsp[0].blk; }
break;
case 25:
#line 129 "tcpgram.y"
{ qualifier.primary = Q_PROTO; }
break;
case 27:
#line 132 "tcpgram.y"
{ yyval.blk = yyvsp[0].blk; }
break;
case 28:
#line 133 "tcpgram.y"
{ yyval.blk = yyvsp[-1].blk; qualifier = default_qual; }
break;
case 29:
#line 134 "tcpgram.y"
{ yyval.blk = gen_proto_abbrev(yyvsp[0].i); 
				  qualifier = default_qual; }
break;
case 30:
#line 136 "tcpgram.y"
{ yyval.blk = gen_relation(yyvsp[-1].i, yyvsp[-2].a, yyvsp[0].a, 0);
				  qualifier = default_qual; }
break;
case 31:
#line 138 "tcpgram.y"
{ yyval.blk = gen_relation(yyvsp[-1].i, yyvsp[-2].a, yyvsp[0].a, 1);
				  qualifier = default_qual; }
break;
case 33:
#line 143 "tcpgram.y"
{ qualifier.protocol = yyvsp[0].i; }
break;
case 34:
#line 144 "tcpgram.y"
{ qualifier.protocol = 0; }
break;
case 35:
#line 147 "tcpgram.y"
{ qualifier.dir = Q_SRC; }
break;
case 36:
#line 148 "tcpgram.y"
{ qualifier.dir = Q_DST; }
break;
case 37:
#line 149 "tcpgram.y"
{ qualifier.dir = Q_OR; }
break;
case 38:
#line 150 "tcpgram.y"
{ qualifier.dir = Q_OR; }
break;
case 39:
#line 151 "tcpgram.y"
{ qualifier.dir = Q_AND; }
break;
case 40:
#line 152 "tcpgram.y"
{ qualifier.dir = Q_AND; }
break;
case 41:
#line 155 "tcpgram.y"
{ qualifier.primary = Q_HOST; }
break;
case 42:
#line 156 "tcpgram.y"
{ qualifier.primary = Q_NET; }
break;
case 43:
#line 157 "tcpgram.y"
{ qualifier.primary = Q_PORT; }
break;
case 44:
#line 160 "tcpgram.y"
{ qualifier.primary = Q_GATEWAY; }
break;
case 45:
#line 162 "tcpgram.y"
{ yyval.i = Q_ETHER; }
break;
case 46:
#line 163 "tcpgram.y"
{ yyval.i = Q_IP; }
break;
case 47:
#line 164 "tcpgram.y"
{ yyval.i = Q_ARP; }
break;
case 48:
#line 165 "tcpgram.y"
{ yyval.i = Q_RARP; }
break;
case 49:
#line 166 "tcpgram.y"
{ yyval.i = Q_TCP; }
break;
case 50:
#line 167 "tcpgram.y"
{ yyval.i = Q_UDP; }
break;
case 51:
#line 168 "tcpgram.y"
{ yyval.i = Q_ICMP; }
break;
case 52:
#line 170 "tcpgram.y"
{ yyval.blk = gen_broadcast(); }
break;
case 53:
#line 171 "tcpgram.y"
{ yyval.blk = gen_less(yyvsp[0].i); }
break;
case 54:
#line 172 "tcpgram.y"
{ yyval.blk = gen_greater(yyvsp[0].i); }
break;
case 55:
#line 173 "tcpgram.y"
{ yyval.blk = gen_byteop(yyvsp[-1].i, yyvsp[-2].i, yyvsp[0].i); }
break;
case 56:
#line 175 "tcpgram.y"
{ yyval.i = BPF_JGT; }
break;
case 57:
#line 176 "tcpgram.y"
{ yyval.i = BPF_JGE; }
break;
case 58:
#line 177 "tcpgram.y"
{ yyval.i = BPF_JEQ; }
break;
case 59:
#line 179 "tcpgram.y"
{ yyval.i = BPF_JGT; }
break;
case 60:
#line 180 "tcpgram.y"
{ yyval.i = BPF_JGE; }
break;
case 61:
#line 181 "tcpgram.y"
{ yyval.i = BPF_JEQ; }
break;
case 62:
#line 183 "tcpgram.y"
{ yyval.a = gen_loadi(yyvsp[0].i); }
break;
case 64:
#line 186 "tcpgram.y"
{ yyval.a = gen_load(yyvsp[-3].i, yyvsp[-1].a, 1); }
break;
case 65:
#line 187 "tcpgram.y"
{ yyval.a = gen_load(yyvsp[-5].i, yyvsp[-3].a, yyvsp[-1].i); }
break;
case 66:
#line 188 "tcpgram.y"
{ yyval.a = gen_arth(BPF_ADD, yyvsp[-2].a, yyvsp[0].a); }
break;
case 67:
#line 189 "tcpgram.y"
{ yyval.a = gen_arth(BPF_SUB, yyvsp[-2].a, yyvsp[0].a); }
break;
case 68:
#line 190 "tcpgram.y"
{ yyval.a = gen_arth(BPF_MUL, yyvsp[-2].a, yyvsp[0].a); }
break;
case 69:
#line 191 "tcpgram.y"
{ yyval.a = gen_arth(BPF_DIV, yyvsp[-2].a, yyvsp[0].a); }
break;
case 70:
#line 192 "tcpgram.y"
{ yyval.a = gen_arth(BPF_AND, yyvsp[-2].a, yyvsp[0].a); }
break;
case 71:
#line 193 "tcpgram.y"
{ yyval.a = gen_arth(BPF_OR, yyvsp[-2].a, yyvsp[0].a); }
break;
case 72:
#line 194 "tcpgram.y"
{ yyval.a = gen_arth(BPF_LSH, yyvsp[-2].a, yyvsp[0].a); }
break;
case 73:
#line 195 "tcpgram.y"
{ yyval.a = gen_arth(BPF_RSH, yyvsp[-2].a, yyvsp[0].a); }
break;
case 74:
#line 196 "tcpgram.y"
{ yyval.a = gen_neg(yyvsp[0].a); }
break;
case 75:
#line 197 "tcpgram.y"
{ yyval.a = yyvsp[-1].a; }
break;
case 76:
#line 198 "tcpgram.y"
{ yyval.a = gen_loadlen(); }
break;
case 77:
#line 200 "tcpgram.y"
{ yyval.i = '&'; }
break;
case 78:
#line 201 "tcpgram.y"
{ yyval.i = '|'; }
break;
case 79:
#line 202 "tcpgram.y"
{ yyval.i = '<'; }
break;
case 80:
#line 203 "tcpgram.y"
{ yyval.i = '>'; }
break;
case 81:
#line 204 "tcpgram.y"
{ yyval.i = '='; }
break;
case 83:
#line 207 "tcpgram.y"
{ yyval.i = yyvsp[-1].i; }
break;
#line 883 "y.tab.c"
    }
    yyssp -= yym;
    yystate = *yyssp;
    yyvsp -= yym;
    yym = yylhs[yyn];
    if (yystate == 0 && yym == 0)
    {
#if YYDEBUG
        if (yydebug)
            printf("yydebug: after reduction, shifting from state 0 to\
 state %d\n", YYFINAL);
#endif
        yystate = YYFINAL;
        *++yyssp = YYFINAL;
        *++yyvsp = yyval;
        if (yychar < 0)
        {
            if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
            if (yydebug)
            {
                yys = 0;
                if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
                if (!yys) yys = "illegal-symbol";
                printf("yydebug: state %d, reading %d (%s)\n",
                        YYFINAL, yychar, yys);
            }
#endif
        }
        if (yychar == 0) goto yyaccept;
        goto yyloop;
    }
    if ((yyn = yygindex[yym]) && (yyn += yystate) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yystate)
        yystate = yytable[yyn];
    else
        yystate = yydgoto[yym];
#if YYDEBUG
    if (yydebug)
        printf("yydebug: after reduction, shifting from state %d \
to state %d\n", *yyssp, yystate);
#endif
    if (yyssp >= yyss + yystacksize - 1)
    {
        goto yyoverflow;
    }
    *++yyssp = yystate;
    *++yyvsp = yyval;
    goto yyloop;
yyoverflow:
    yyerror("yacc stack overflow");
yyabort:
    return (1);
yyaccept:
    return (0);
}
