/*
 * Copyright (c) 1981 Regents of the University of California.
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
static char sccsid[] = "@(#)toucholap.c	5.4 (Berkeley) 6/1/90";
#endif /* not lint */

# include	"curses.ext"

# define	min(a,b)	(a < b ? a : b)
# define	max(a,b)	(a > b ? a : b)

/*
 *	Touch, on win2, the part that overlaps with win1.
 *
 */
touchoverlap(win1, win2)
reg WINDOW	*win1, *win2; {

	reg int		x, y, endy, endx, starty, startx;

# ifdef DEBUG
	fprintf(outf, "TOUCHOVERLAP(%0.2o, %0.2o);\n", win1, win2);
# endif
	starty = max(win1->_begy, win2->_begy);
	startx = max(win1->_begx, win2->_begx);
	endy = min(win1->_maxy + win1->_begy, win2->_maxy + win2->_begx);
	endx = min(win1->_maxx + win1->_begx, win2->_maxx + win2->_begx);
# ifdef DEBUG
	fprintf(outf, "TOUCHOVERLAP:from (%d,%d) to (%d,%d)\n", starty, startx, endy, endx);
	fprintf(outf, "TOUCHOVERLAP:win1 (%d,%d) to (%d,%d)\n", win1->_begy, win1->_begx, win1->_begy + win1->_maxy, win1->_begx + win1->_maxx);
	fprintf(outf, "TOUCHOVERLAP:win2 (%d,%d) to (%d,%d)\n", win2->_begy, win2->_begx, win2->_begy + win2->_maxy, win2->_begx + win2->_maxx);
# endif
	if (starty >= endy || startx >= endx)
		return;
	starty -= win2->_begy;
	startx -= win2->_begx;
	endy -= win2->_begy;
	endx -= win2->_begx;
	endx--;
	for (y = starty; y < endy; y++)
		touchline(win2, y, startx, endx);
}
