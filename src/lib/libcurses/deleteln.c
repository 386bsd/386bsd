/*
 * Copyright (c) 1981, 1993
 *	The Regents of the University of California.  All rights reserved.
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
static char sccsid[] = "@(#)deleteln.c	8.1 (Berkeley) 6/4/93";
#endif	/* not lint */

#include <curses.h>
#include <string.h>

/*
 * wdeleteln --
 *	Delete a line from the screen.  It leaves (cury, curx) unchanged.
 */
int
wdeleteln(win)
	register WINDOW *win;
{
	register int y, i;
	register __LINE *temp;

#ifdef DEBUG
	__CTRACE("deleteln: (%0.2o)\n", win);
#endif
	temp = win->lines[win->cury];
	for (y = win->cury; y < win->maxy - 1; y++) {
		win->lines[y]->flags &= ~__ISPASTEOL;
		win->lines[y + 1]->flags &= ~__ISPASTEOL;
		if (win->orig == NULL)
			win->lines[y] = win->lines[y + 1];
		else
			(void) memcpy(win->lines[y]->line, 
			    win->lines[y + 1]->line, 
			    win->maxx * __LDATASIZE);
		__touchline(win, y, 0, win->maxx - 1, 0);
	}

	if (win->orig == NULL)
		win->lines[y] = temp;
	else
		temp = win->lines[y];

	for(i = 0; i < win->maxx; i++) {
		temp->line[i].ch = ' ';
		temp->line[i].attr = 0;
	}
	__touchline(win, y, 0, win->maxx - 1, 0);
	if (win->orig == NULL)
		__id_subwins(win);
	return (OK);
}
