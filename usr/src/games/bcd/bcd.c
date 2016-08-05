/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Steve Hayman of the Indiana University Computer Science Dept..
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
char copyright[] =
"@(#) Copyright (c) 1989 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)bcd.c	4.4 (Berkeley) 6/1/90";
#endif /* not lint */

/*
 * bcd --
 *
 * Read one line of standard input and produce something that looks like a
 * punch card.  An attempt to reimplement /usr/games/bcd.  All I looked at
 * was the man page.
 *
 * I couldn't find a BCD table handy so I wrote a shell script to deduce what
 * the patterns were that the old bcd was using for each possible 8-bit
 * character.  These are the results -- the low order 12 bits represent the
 * holes.  (A 1 bit is a hole.)  These may be wrong, but they match the old
 * program!
 *
 * Steve Hayman
 * sahayman@iuvax.cs.indiana.edu
 * 1989 11 30
 */

#include <sys/types.h>
#include <stdio.h>
#include <ctype.h>

u_short holes[256] = {
    0x0,	 0x0,	  0x0,	   0x0,	    0x0,     0x0,     0x0,     0x0,
    0x0,	 0x0,	  0x0,	   0x0,	    0x0,     0x0,     0x0,     0x0,
    0x0,	 0x0,	  0x0,	   0x0,	    0x0,     0x0,     0x0,     0x0,
    0x0,	 0x0,	  0x0,	   0x0,	    0x0,     0x0,     0x0,     0x0,
    0x0,	 0x206,	  0x20a,   0x042,   0x442,   0x222,   0x800,   0x406,
    0x812,	 0x412,	  0x422,   0xa00,   0x242,   0x400,   0x842,   0x300,
    0x200,	 0x100,	  0x080,   0x040,   0x020,   0x010,   0x008,   0x004,
    0x002,	 0x001,	  0x012,   0x40a,   0x80a,   0x212,   0x00a,   0x006,
    0x022,	 0x900,	  0x880,   0x840,   0x820,   0x810,   0x808,   0x804,
    0x802,	 0x801,	  0x500,   0x480,   0x440,   0x420,   0x410,   0x408,
    0x404,	 0x402,	  0x402,   0x280,   0x240,   0x220,   0x210,   0x208,
    0x204,	 0x202,	  0x201,   0x082,   0x822,   0x600,   0x282,   0x30f,
    0x900,	 0x880,	  0x840,   0x820,   0x810,   0x808,   0x804,   0x802,
    0x801,	 0x500,	  0x480,   0x440,   0x420,   0x410,   0x408,   0x404,
    0x402,	 0x402,	  0x280,   0x240,   0x220,   0x210,   0x208,   0x204,
    0x202,	 0x201,	  0x082,   0x806,   0x822,   0x600,   0x282,   0x0,
    0x0,	 0x0,	  0x0,	   0x0,	    0x0,     0x0,     0x0,     0x0,
    0x0,	 0x0,	  0x0,	   0x0,	    0x0,     0x0,     0x0,     0x0,
    0x0,	 0x0,	  0x0,	   0x0,	    0x0,     0x0,     0x0,     0x0,
    0x0,	 0x0,	  0x0,	   0x0,	    0x0,     0x0,     0x0,     0x0,
    0x206,	 0x20a,	  0x042,   0x442,   0x222,   0x800,   0x406,   0x812,
    0x412,	 0x422,	  0xa00,   0x242,   0x400,   0x842,   0x300,   0x200,
    0x100,	 0x080,	  0x040,   0x020,   0x010,   0x008,   0x004,   0x002,
    0x001,	 0x012,	  0x40a,   0x80a,   0x212,   0x00a,   0x006,   0x022,
    0x900,	 0x880,	  0x840,   0x820,   0x810,   0x808,   0x804,   0x802,
    0x801,	 0x500,	  0x480,   0x440,   0x420,   0x410,   0x408,   0x404,
    0x402,	 0x402,	  0x280,   0x240,   0x220,   0x210,   0x208,   0x204,
    0x202,	 0x201,	  0x082,   0x806,   0x822,   0x600,   0x282,   0x30f,
    0x900,	 0x880,	  0x840,   0x820,   0x810,   0x808,   0x804,   0x802,
    0x801,	 0x500,	  0x480,   0x440,   0x420,   0x410,   0x408,   0x404,
    0x402,	 0x402,	  0x280,   0x240,   0x220,   0x210,   0x208,   0x204,
    0x202,	 0x201,	  0x082,   0x806,   0x822,   0x600,   0x282,   0x0
};

/*
 * i'th bit of w.
 */
#define	bit(w,i)	((w)&(1<<(i)))

main(argc, argv)
	int argc;
	char **argv;
{
	char cardline[80];

	/*
	 * The original bcd prompts with a "%" when reading from stdin,
	 * but this seems kind of silly.  So this one doesn't.
	 */

	if (argc > 1) {
		while (--argc)
			printcard(*++argv);
	} else
		while (fgets(cardline, sizeof(cardline), stdin))
			printcard(cardline);
	exit(0);
}

#define	COLUMNS	48

printcard(str)
	register char *str;
{
	static char rowchars[] = "   123456789";
	register int i, row;
	register char *p;
	char *index();

	/* ruthlessly remove newlines and truncate at 48 characters. */
	if ((p = index(str, '\n')))
		*p = '\0';

	if (strlen(str) > COLUMNS)
		str[COLUMNS] = '\0';

	/* make string upper case. */
	for (p = str; *p; ++p)
		if (isascii(*p) && islower(*p))
			*p = toupper(*p);

	 /* top of card */
	putchar(' ');
	for (i = 1; i <= COLUMNS; ++i)
		putchar('_');
	putchar('\n');

	/*
	 * line of text.  Leave a blank if the character doesn't have
	 * a hole pattern.
	 */
	p = str;
	putchar('/');
	for (i = 1; *p; i++, p++)
		if (holes[*p])
			putchar(*p);
		else
			putchar(' ');
	while (i++ <= COLUMNS)
		putchar(' ');
	putchar('|');
	putchar('\n');

	/*
	 * 12 rows of potential holes; output a ']', which looks kind of
	 * like a hole, if the appropriate bit is set in the holes[] table.
	 * The original bcd output a '[', a backspace, five control A's,
	 * and then a ']'.  This seems a little excessive.
	 */
	for (row = 0; row <= 11; ++row) {
		putchar('|');
		for (i = 0, p = str; *p; i++, p++) {
			if (bit(holes[*p], 11 - row))
				putchar(']');
			else
				putchar(rowchars[row]);
		}
		while (i++ < COLUMNS)
			putchar(rowchars[row]);
		putchar('|');
		putchar('\n');
	}

	/* bottom of card */
	putchar('|');
	for (i = 1; i <= COLUMNS; i++)
		putchar('_');
	putchar('|');
	putchar('\n');
}
