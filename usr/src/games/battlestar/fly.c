/*
 * Copyright (c) 1983 Regents of the University of California.
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
 */

#ifndef lint
static char sccsid[] = "@(#)fly.c	5.6 (Berkeley) 3/4/91";
#endif /* not lint */

#include "externs.h"
#undef UP
#include <curses.h>

#define abs(a)	((a) < 0 ? -(a) : (a))
#define MIDR  (LINES/2 - 1)
#define MIDC  (COLS/2 - 1)

int row, column;
int dr = 0, dc = 0;
char destroyed;
int clock = 120;		/* time for all the flights in the game */
char cross = 0;
sig_t oldsig;

void
succumb()
{
	if (oldsig == SIG_DFL) {
		endfly();
		exit(1);
	}
	if (oldsig != SIG_IGN) {
		endfly();
		(*oldsig)(SIGINT);
	}
}

visual()
{
	void moveenemy();

	destroyed = 0;
	savetty();
	if(initscr() == ERR){
		puts("Whoops!  No more memory...");
		return(0);
	}
	oldsig = signal(SIGINT, succumb);
	crmode();
	noecho();
	screen();
	row = rnd(LINES-3) + 1;
	column = rnd(COLS-2) + 1;
	moveenemy();
	for (;;) {
		switch(getchar()){

			case 'h':
			case 'r':
				dc = -1;
				fuel--;
				break;

			case 'H':
			case 'R':
				dc = -5;
				fuel -= 10;
				break;

			case 'l':
				dc = 1;
				fuel--;
				break;

			case 'L':
				dc = 5;
				fuel -= 10;
				break;

			case 'j':
			case 'u':
				dr = 1;
				fuel--;
				break;

			case 'J':
			case 'U':
				dr = 5;
				fuel -= 10;
				break;

			case 'k':
			case 'd':
				dr = -1;
				fuel--;
				break;

			case 'K':
			case 'D':
				dr = -5;
				fuel -= 10;
				break;

			case '+':
				if (cross){
					cross = 0;
					notarget();
				}
				else
					cross = 1;
				break;

			case ' ':
			case 'f':
				if (torps){
					torps -= 2;
					blast();
					if (row == MIDR && column - MIDC < 2 && MIDC - column < 2){
						destroyed = 1;
						alarm(0);
					}
				}
				else
					mvaddstr(0,0,"*** Out of torpedoes. ***");
				break;

			case 'q':
				endfly();
				return(0);

			default:
				mvaddstr(0,26,"Commands = r,R,l,L,u,U,d,D,f,+,q");
				continue;

			case EOF:
				break;
		}
		if (destroyed){
			endfly();
			return(1);
		}
		if (clock <= 0){
			endfly();
			die();
		}
	}
}

screen()
{
	register int r,c,n;
	int i;

	clear();
	i = rnd(100);
	for (n=0; n < i; n++){
		r = rnd(LINES-3) + 1;
		c = rnd(COLS);
		mvaddch(r, c, '.');
	}
	mvaddstr(LINES-1-1,21,"TORPEDOES           FUEL           TIME");
	refresh();
}

target()
{
	register int n;

	move(MIDR,MIDC-10);
	addstr("-------   +   -------");
	for (n = MIDR-4; n < MIDR-1; n++){
		mvaddch(n,MIDC,'|');
		mvaddch(n+6,MIDC,'|');
	}
}

notarget()
{
	register int n;

	move(MIDR,MIDC-10);
	addstr("                     ");
	for (n = MIDR-4; n < MIDR-1; n++){
		mvaddch(n,MIDC,' ');
		mvaddch(n+6,MIDC,' ');
	}
}

blast()
{
	register int n;

	alarm(0);
	move(LINES-1, 24);
	printw("%3d", torps);
	for(n = LINES-1-2; n >= MIDR + 1; n--){
		mvaddch(n, MIDC+MIDR-n, '/');
		mvaddch(n, MIDC-MIDR+n, '\\');
		refresh();
	}
	mvaddch(MIDR,MIDC,'*');
	for(n = LINES-1-2; n >= MIDR + 1; n--){
		mvaddch(n, MIDC+MIDR-n, ' ');
		mvaddch(n, MIDC-MIDR+n, ' ');
		refresh();
	}
	alarm(1);
}

void
moveenemy()
{
	double d;
	int oldr, oldc;

	oldr = row;
	oldc = column;
	if (fuel > 0){
		if (row + dr <= LINES-3 && row + dr > 0)
			row += dr;
		if (column + dc < COLS-1 && column + dc > 0)
			column += dc;
	} else if (fuel < 0){
		fuel = 0;
		mvaddstr(0,60,"*** Out of fuel ***");
	}
	d = (double) ((row - MIDR)*(row - MIDR) + (column - MIDC)*(column - MIDC));
	if (d < 16){
		row += (rnd(9) - 4) % (4 - abs(row - MIDR));
		column += (rnd(9) - 4) % (4 - abs(column - MIDC));
	}
	clock--;
	mvaddstr(oldr, oldc - 1, "   ");
	if (cross)
		target();
	mvaddstr(row, column - 1, "/-\\");
	move(LINES-1, 24);
	printw("%3d", torps);
	move(LINES-1, 42);
	printw("%3d", fuel);
	move(LINES-1, 57);
	printw("%3d", clock);
	refresh();
	signal(SIGALRM, moveenemy);
	alarm(1);
}

endfly()
{
	alarm(0);
	signal(SIGALRM, SIG_DFL);
	mvcur(0,COLS-1,LINES-1,0);
	endwin();
	signal(SIGTSTP, SIG_DFL);
	signal(SIGINT, oldsig);
}
