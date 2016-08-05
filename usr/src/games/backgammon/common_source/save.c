/*
 * Copyright (c) 1980 Regents of the University of California.
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
static char sccsid[] = "@(#)save.c	5.4 (Berkeley) 6/1/90";
#endif /* not lint */

#include "back.h"

extern int	errno;

static char	confirm[] = "Are you sure you want to leave now?";
static char	prompt[] = "Enter a file name:  ";
static char	exist1[] = "The file '";
static char	exist2[] =
	"' already exists.\nAre you sure you want to use this file?";
static char	cantuse[] = "\nCan't use ";
static char	saved[] = "This game has been saved on the file '";
static char	type[] = "'.\nType \"backgammon ";
static char	rec[] = "\" to recover your game.\n\n";
static char	cantrec[] = "Can't recover file:  ";

save (n)
register int	n;

{
	register int	fdesc;
	register char	*fs;
	char		fname[50];

	if (n)  {
		if (tflag)  {
			curmove (20,0);
			clend();
		} else
			writec ('\n');
		writel (confirm);
		if (! yorn(0))
			return;
	}
	cflag = 1;
	for (;;)  {
		writel (prompt);
		fs = fname;
		while ((*fs = readc()) != '\n')  {
			if (*fs == tty.sg_erase)  {
				if (fs > fname)  {
					fs--;
					if (tflag)
						curmove (curr,curc-1);
					else
						writec (*fs);
				} else
					writec ('\007');
				continue;
			}
			writec (*fs++);
		}
		*fs = '\0';
		if ((fdesc = open(fname,2)) == -1 && errno == 2)  {
			if ((fdesc = creat (fname,0700)) != -1)
			break;
		}
		if (fdesc != -1)  {
			if (tflag)  {
				curmove (18,0);
				clend();
			} else
				writec ('\n');
			writel (exist1);
			writel (fname);
			writel (exist2);
			cflag = 0;
			close (fdesc);
			if (yorn (0))  {
				unlink (fname);
				fdesc = creat (fname,0700);
				break;
			} else  {
				cflag = 1;
				continue;
			}
		}
		writel (cantuse);
		writel (fname);
		writel (".\n");
		close (fdesc);
		cflag = 1;
	}
	write (fdesc,board,sizeof board);
	write (fdesc,off,sizeof off);
	write (fdesc,in,sizeof in);
	write (fdesc,dice,sizeof dice);
	write (fdesc,&cturn,sizeof cturn);
	write (fdesc,&dlast,sizeof dlast);
	write (fdesc,&pnum,sizeof pnum);
	write (fdesc,&rscore,sizeof rscore);
	write (fdesc,&wscore,sizeof wscore);
	write (fdesc,&gvalue,sizeof gvalue);
	write (fdesc,&raflag,sizeof raflag);
	close (fdesc);
	if (tflag)
		curmove (18,0);
	writel (saved);
	writel (fname);
	writel (type);
	writel (fname);
	writel (rec);
	if (tflag)
		clend();
	getout ();
}

recover (s)
char	*s;

{
	register int	i;
	int		fdesc;

	if ((fdesc = open (s,0)) == -1)
		norec (s);
	read (fdesc,board,sizeof board);
	read (fdesc,off,sizeof off);
	read (fdesc,in,sizeof in);
	read (fdesc,dice,sizeof dice);
	read (fdesc,&cturn,sizeof cturn);
	read (fdesc,&dlast,sizeof dlast);
	read (fdesc,&pnum,sizeof pnum);
	read (fdesc,&rscore,sizeof rscore);
	read (fdesc,&wscore,sizeof wscore);
	read (fdesc,&gvalue,sizeof gvalue);
	read (fdesc,&raflag,sizeof raflag);
	close (fdesc);
	rflag = 1;
}

norec (s)
register char	*s;

{
	register char	*c;

	tflag = 0;
	writel (cantrec);
	c = s;
	while (*c != '\0')
		writec (*c++);
	getout ();
}
