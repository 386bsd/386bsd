#ifndef lint
static char yysccsid[] = "@(#)yaccpar	1.8 (Berkeley) 01/20/90";
#endif
#define YYBYACC 1
#line 56 "/usr/src/games/atc/grammar.y"
typedef union {
	int	ival;
	char	cval;
} YYSTYPE;
#line 62 "/usr/src/games/atc/grammar.y"
#include "include.h"

#ifndef lint
static char sccsid[] = "@(#)grammar.y	5.2 (Berkeley) 4/30/90";
#endif /* not lint */

int	errors = 0;
int	line = 1;
#line 20 "y.tab.c"
#define HeightOp 257
#define WidthOp 258
#define UpdateOp 259
#define NewplaneOp 260
#define DirOp 261
#define ConstOp 262
#define LineOp 263
#define AirportOp 264
#define BeaconOp 265
#define ExitOp 266
#define YYERRCODE 256
short yylhs[] = {                                        -1,
    3,    0,    1,    1,    4,    4,    4,    4,    5,    6,
    8,    7,    2,    2,    9,    9,    9,    9,   10,   10,
   14,   11,   11,   15,   13,   13,   16,   12,   12,   17,
};
short yylen[] = {                                         2,
    0,    3,    2,    1,    1,    1,    1,    1,    4,    4,
    4,    4,    2,    1,    4,    4,    4,    4,    2,    1,
    4,    2,    1,    5,    2,    1,    5,    2,    1,   10,
};
short yydefred[] = {                                      0,
    0,    0,    0,    0,    0,    1,    0,    5,    6,    7,
    8,    0,    0,    0,    0,    0,    3,    0,    0,    0,
    0,    0,    0,    0,    0,    2,    0,   11,   12,    9,
   10,    0,    0,    0,    0,   13,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,   17,
   28,    0,   18,   25,    0,   15,   19,    0,   16,   22,
    0,    0,    0,    0,    0,    0,   21,    0,    0,   27,
   24,    0,    0,    0,    0,   30,
};
short yydgoto[] = {                                       5,
    6,   26,   16,    7,    8,    9,   10,   11,   27,   44,
   47,   38,   41,   45,   48,   42,   39,
};
short yysindex[] = {                                   -257,
  -49,  -48,  -47,  -46,    0,    0, -257,    0,    0,    0,
    0, -246, -245, -244, -243, -259,    0,  -39,  -38,  -37,
  -36,  -34,  -33,  -32,  -31,    0, -259,    0,    0,    0,
    0,  -63,  -11,  -10,   -9,    0,   -8,  -26,  -63, -228,
  -24,  -11, -226,  -22,  -10, -224,  -20,   -9, -222,    0,
    0, -221,    0,    0, -220,    0,    0, -219,    0,    0,
 -218, -216,    5, -214,    7,    8,    0,    9,   11,    0,
    0, -210, -209,   13,  -35,    0,
};
short yyrindex[] = {                                      0,
    0,    0,    0,    0,    0,    0, -255,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,   55,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,  -38,    0,
    0,   -3,    0,    0,   -2,    0,    0,    1,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,
};
short yygindex[] = {                                      0,
   52,   34,    0,    0,    0,    0,    0,    0,    0,   17,
   15,   25,   23,    0,    0,    0,    0,
};
#define YYTABLESIZE 65
short yytable[] = {                                       1,
    2,    3,    4,   22,   23,   24,   25,    4,    4,    4,
    4,   12,   13,   14,   15,   18,   19,   20,   21,   28,
   29,   30,   31,   32,   33,   34,   35,   37,   40,   43,
   46,   49,   50,   52,   53,   55,   56,   58,   59,   61,
   62,   63,   64,   65,   66,   67,   68,   69,   70,   71,
   72,   73,   74,   75,   14,   26,   20,   76,   17,   23,
   36,   57,   60,   51,   54,
};
short yycheck[] = {                                     257,
  258,  259,  260,  263,  264,  265,  266,  263,  264,  265,
  266,   61,   61,   61,   61,  262,  262,  262,  262,   59,
   59,   59,   59,   58,   58,   58,   58,   91,   40,   40,
   40,   40,   59,  262,   59,  262,   59,  262,   59,  262,
  262,  262,  262,  262,  261,   41,  261,   41,   41,   41,
   40,  262,  262,   41,    0,   59,   59,   93,    7,   59,
   27,   45,   48,   39,   42,
};
#define YYFINAL 5
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#define YYMAXTOKEN 266
#if YYDEBUG
char *yyname[] = {
"end-of-file",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,"'('","')'",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"':'","';'",0,"'='",0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"'['",0,"']'",0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
"HeightOp","WidthOp","UpdateOp","NewplaneOp","DirOp","ConstOp","LineOp",
"AirportOp","BeaconOp","ExitOp",
};
char *yyrule[] = {
"$accept : file",
"$$1 :",
"file : bunch_of_defs $$1 bunch_of_lines",
"bunch_of_defs : def bunch_of_defs",
"bunch_of_defs : def",
"def : udef",
"def : ndef",
"def : wdef",
"def : hdef",
"udef : UpdateOp '=' ConstOp ';'",
"ndef : NewplaneOp '=' ConstOp ';'",
"hdef : HeightOp '=' ConstOp ';'",
"wdef : WidthOp '=' ConstOp ';'",
"bunch_of_lines : line bunch_of_lines",
"bunch_of_lines : line",
"line : BeaconOp ':' Bpoint_list ';'",
"line : ExitOp ':' Epoint_list ';'",
"line : LineOp ':' Lline_list ';'",
"line : AirportOp ':' Apoint_list ';'",
"Bpoint_list : Bpoint Bpoint_list",
"Bpoint_list : Bpoint",
"Bpoint : '(' ConstOp ConstOp ')'",
"Epoint_list : Epoint Epoint_list",
"Epoint_list : Epoint",
"Epoint : '(' ConstOp ConstOp DirOp ')'",
"Apoint_list : Apoint Apoint_list",
"Apoint_list : Apoint",
"Apoint : '(' ConstOp ConstOp DirOp ')'",
"Lline_list : Lline Lline_list",
"Lline_list : Lline",
"Lline : '[' '(' ConstOp ConstOp ')' '(' ConstOp ConstOp ')' ']'",
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
#line 284 "/usr/src/games/atc/grammar.y"

check_edge(x, y)
{
	if (!(x == 0) && !(x == sp->width - 1) && 
	    !(y == 0) && !(y == sp->height - 1))
		yyerror("edge value not on edge.");
}

check_point(x, y)
{
	if (x < 1 || x >= sp->width - 1)
		yyerror("X value out of range.");
	if (y < 1 || y >= sp->height - 1)
		yyerror("Y value out of range.");
}

check_linepoint(x, y)
{
	if (x < 0 || x >= sp->width)
		yyerror("X value out of range.");
	if (y < 0 || y >= sp->height)
		yyerror("Y value out of range.");
}

check_line(x1, y1, x2, y2)
{
	int	d1, d2;

	check_linepoint(x1, y1);
	check_linepoint(x2, y2);

	d1 = ABS(x2 - x1);
	d2 = ABS(y2 - y1);

	if (!(d1 == d2) && !(d1 == 0) && !(d2 == 0))
		yyerror("Bad line endpoints.");
}

yyerror(s)
{
	fprintf(stderr, "\"%s\": line %d: %s\n", file, line, s);
	errors++;

	return (errors);
}

check_edir(x, y, dir)
{
	int	bad = 0;

	if (x == sp->width - 1)
		x = 2;
	else if (x != 0)
		x = 1;
	if (y == sp->height - 1)
		y = 2;
	else if (y != 0)
		y = 1;
	
	switch (x * 10 + y) {
	case 00: if (dir != 3) bad++; break;
	case 01: if (dir < 1 || dir > 3) bad++; break;
	case 02: if (dir != 1) bad++; break;
	case 10: if (dir < 3 || dir > 5) bad++; break;
	case 11: break;
	case 12: if (dir > 1 && dir < 7) bad++; break;
	case 20: if (dir != 5) bad++; break;
	case 21: if (dir < 5) bad++; break;
	case 22: if (dir != 7) bad++; break;
	default:
		yyerror("Unknown value in checkdir!  Get help!");
		break;
	}
	if (bad)
		yyerror("Bad direction for entrance at exit.");
}

check_adir(x, y, dir)
{
}

checkdefs()
{
	int	err = 0;

	if (sp->width == 0) {
		yyerror("'width' undefined.");
		err++;
	}
	if (sp->height == 0) {
		yyerror("'height' undefined.");
		err++;
	}
	if (sp->update_secs == 0) {
		yyerror("'update' undefined.");
		err++;
	}
	if (sp->newplane_time == 0) {
		yyerror("'newplane' undefined.");
		err++;
	}
	if (err)
		return (-1);
	else
		return (0);
}
#line 282 "y.tab.c"
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
#line 74 "/usr/src/games/atc/grammar.y"
{ if (checkdefs() < 0) return (errors); }
break;
case 2:
#line 75 "/usr/src/games/atc/grammar.y"
{ 
		if (sp->num_exits + sp->num_airports < 2)
			yyerror("Need at least 2 airports and/or exits.");
		return (errors);
		}
break;
case 9:
#line 96 "/usr/src/games/atc/grammar.y"
{
		if (sp->update_secs != 0)
			return (yyerror("Redefinition of 'update'."));
		else if (yyvsp[-1].ival < 1)
			return (yyerror("'update' is too small."));
		else
			sp->update_secs = yyvsp[-1].ival;
		}
break;
case 10:
#line 108 "/usr/src/games/atc/grammar.y"
{
		if (sp->newplane_time != 0)
			return (yyerror("Redefinition of 'newplane'."));
		else if (yyvsp[-1].ival < 1)
			return (yyerror("'newplane' is too small."));
		else
			sp->newplane_time = yyvsp[-1].ival;
		}
break;
case 11:
#line 120 "/usr/src/games/atc/grammar.y"
{
		if (sp->height != 0)
			return (yyerror("Redefinition of 'height'."));
		else if (yyvsp[-1].ival < 3)
			return (yyerror("'height' is too small."));
		else
			sp->height = yyvsp[-1].ival; 
		}
break;
case 12:
#line 132 "/usr/src/games/atc/grammar.y"
{
		if (sp->height != 0)
			return (yyerror("Redefinition of 'width'."));
		else if (yyvsp[-1].ival < 3)
			return (yyerror("'width' is too small."));
		else
			sp->width = yyvsp[-1].ival; 
		}
break;
case 13:
#line 144 "/usr/src/games/atc/grammar.y"
{}
break;
case 14:
#line 146 "/usr/src/games/atc/grammar.y"
{}
break;
case 15:
#line 151 "/usr/src/games/atc/grammar.y"
{}
break;
case 16:
#line 153 "/usr/src/games/atc/grammar.y"
{}
break;
case 17:
#line 155 "/usr/src/games/atc/grammar.y"
{}
break;
case 18:
#line 157 "/usr/src/games/atc/grammar.y"
{}
break;
case 19:
#line 162 "/usr/src/games/atc/grammar.y"
{}
break;
case 20:
#line 164 "/usr/src/games/atc/grammar.y"
{}
break;
case 21:
#line 169 "/usr/src/games/atc/grammar.y"
{
		if (sp->num_beacons % REALLOC == 0) {
			if (sp->beacon == NULL)
				sp->beacon = (BEACON *) malloc((sp->num_beacons
					+ REALLOC) * sizeof (BEACON));
			else
				sp->beacon = (BEACON *) realloc(sp->beacon,
					(sp->num_beacons + REALLOC) * 
					sizeof (BEACON));
			if (sp->beacon == NULL)
				return (yyerror("No memory available."));
		}
		sp->beacon[sp->num_beacons].x = yyvsp[-2].ival;
		sp->beacon[sp->num_beacons].y = yyvsp[-1].ival;
		check_point(yyvsp[-2].ival, yyvsp[-1].ival);
		sp->num_beacons++;
		}
break;
case 22:
#line 190 "/usr/src/games/atc/grammar.y"
{}
break;
case 23:
#line 192 "/usr/src/games/atc/grammar.y"
{}
break;
case 24:
#line 197 "/usr/src/games/atc/grammar.y"
{
		int	dir;

		if (sp->num_exits % REALLOC == 0) {
			if (sp->exit == NULL)
				sp->exit = (EXIT *) malloc((sp->num_exits + 
					REALLOC) * sizeof (EXIT));
			else
				sp->exit = (EXIT *) realloc(sp->exit,
					(sp->num_exits + REALLOC) * 
					sizeof (EXIT));
			if (sp->exit == NULL)
				return (yyerror("No memory available."));
		}
		dir = dir_no(yyvsp[-1].cval);
		sp->exit[sp->num_exits].x = yyvsp[-3].ival;
		sp->exit[sp->num_exits].y = yyvsp[-2].ival;
		sp->exit[sp->num_exits].dir = dir;
		check_edge(yyvsp[-3].ival, yyvsp[-2].ival);
		check_edir(yyvsp[-3].ival, yyvsp[-2].ival, dir);
		sp->num_exits++;
		}
break;
case 25:
#line 223 "/usr/src/games/atc/grammar.y"
{}
break;
case 26:
#line 225 "/usr/src/games/atc/grammar.y"
{}
break;
case 27:
#line 230 "/usr/src/games/atc/grammar.y"
{
		int	dir;

		if (sp->num_airports % REALLOC == 0) {
			if (sp->airport == NULL)
				sp->airport=(AIRPORT *)malloc((sp->num_airports
					+ REALLOC) * sizeof(AIRPORT));
			else
				sp->airport = (AIRPORT *) realloc(sp->airport,
					(sp->num_airports + REALLOC) * 
					sizeof(AIRPORT));
			if (sp->airport == NULL)
				return (yyerror("No memory available."));
		}
		dir = dir_no(yyvsp[-1].cval);
		sp->airport[sp->num_airports].x = yyvsp[-3].ival;
		sp->airport[sp->num_airports].y = yyvsp[-2].ival;
		sp->airport[sp->num_airports].dir = dir;
		check_point(yyvsp[-3].ival, yyvsp[-2].ival);
		check_adir(yyvsp[-3].ival, yyvsp[-2].ival, dir);
		sp->num_airports++;
		}
break;
case 28:
#line 256 "/usr/src/games/atc/grammar.y"
{}
break;
case 29:
#line 258 "/usr/src/games/atc/grammar.y"
{}
break;
case 30:
#line 263 "/usr/src/games/atc/grammar.y"
{
		if (sp->num_lines % REALLOC == 0) {
			if (sp->line == NULL)
				sp->line = (LINE *) malloc((sp->num_lines + 
					REALLOC) * sizeof (LINE));
			else
				sp->line = (LINE *) realloc(sp->line,
					(sp->num_lines + REALLOC) *
					sizeof (LINE));
			if (sp->line == NULL)
				return (yyerror("No memory available."));
		}
		sp->line[sp->num_lines].p1.x = yyvsp[-7].ival;
		sp->line[sp->num_lines].p1.y = yyvsp[-6].ival;
		sp->line[sp->num_lines].p2.x = yyvsp[-3].ival;
		sp->line[sp->num_lines].p2.y = yyvsp[-2].ival;
		check_line(yyvsp[-7].ival, yyvsp[-6].ival, yyvsp[-3].ival, yyvsp[-2].ival);
		sp->num_lines++;
		}
break;
#line 626 "y.tab.c"
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
