/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * David Hitz of Auspex Systems, Inc.
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
"@(#) Copyright (c) 1991 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)look.c	5.1 (Berkeley) 7/21/91";
#endif /* not lint */

/*
 * look -- find lines in a sorted list.
 * 
 * The man page said that TABs and SPACEs participate in -d comparisons.
 * In fact, they were ignored.  This implements historic practice, not
 * the manual page.
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "pathnames.h"

/*
 * FOLD and DICT convert characters to a normal form for comparison,
 * according to the user specified flags.
 * 
 * DICT expects integers because it uses a non-character value to
 * indicate a character which should not participate in comparisons.
 */
#define	EQUAL		0
#define	GREATER		1
#define	LESS		(-1)
#define NO_COMPARE	(-2)

#define	FOLD(c)	(isascii(c) && isupper(c) ? tolower(c) : (c))
#define	DICT(c)	(isascii(c) && isalnum(c) ? (c) : NO_COMPARE)

int dflag, fflag;

char	*binary_search __P((char *, char *, char *));
int	 compare __P((char *, char *, char *));
void	 err __P((const char *fmt, ...));
char	*linear_search __P((char *, char *, char *));
int	 look __P((char *, char *, char *));
void	 print_from __P((char *, char *, char *));
static void	 usage __P((void));

main(argc, argv)
	int argc;
	char *argv[];
{
	struct stat sb;
	int ch, fd;
	char *back, *file, *front, *string;

	file = _PATH_WORDS;
	while ((ch = getopt(argc, argv, "df")) != EOF)
		switch(ch) {
		case 'd':
			dflag = 1;
			break;
		case 'f':
			fflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	switch (argc) {
	case 2:				/* Don't set -df for user. */
		string = *argv++;
		file = *argv;
		break;
	case 1:				/* But set -df by default. */
		dflag = fflag = 1;
		string = *argv;
		break;
	default:
		usage();
	}

	if ((fd = open(file, O_RDONLY, 0)) < 0 || fstat(fd, &sb) ||
	    (front = mmap(NULL, sb.st_size, PROT_READ, MAP_FILE, fd,
	    (off_t)0)) == NULL)
		err("%s: %s", file, strerror(errno));
	back = front + sb.st_size;
	exit(look(string, front, back));
}

look(string, front, back)
	char *string, *front, *back;
{
	register int ch;
	register char *readp, *writep;

	/* Reformat string string to avoid doing it multiple times later. */
	for (readp = writep = string; ch = *readp++;) {
		if (fflag)
			ch = FOLD(ch);
		if (dflag)
			ch = DICT(ch);
		if (ch != NO_COMPARE)
			*(writep++) = ch;
	}
	*writep = '\0';

	front = binary_search(string, front, back);
	front = linear_search(string, front, back);

	if (front)
		print_from(string, front, back);
	return (front ? 0 : 1);
}


/*
 * Binary search for "string" in memory between "front" and "back".
 * 
 * This routine is expected to return a pointer to the start of a line at
 * *or before* the first word matching "string".  Relaxing the constraint
 * this way simplifies the algorithm.
 * 
 * Invariants:
 * 	front points to the beginning of a line at or before the first 
 *	matching string.
 * 
 * 	back points to the beginning of a line at or after the first 
 *	matching line.
 * 
 * Base of the Invariants.
 * 	front = NULL; 
 *	back = EOF;
 * 
 * Advancing the Invariants:
 * 
 * 	p = first newline after halfway point from front to back.
 * 
 * 	If the string at "p" is not greater than the string to match, 
 *	p is the new front.  Otherwise it is the new back.
 * 
 * Termination:
 * 
 * 	The definition of the routine allows it return at any point, 
 *	since front is always at or before the line to print.
 * 
 * 	In fact, it returns when the chosen "p" equals "back".  This 
 *	implies that there exists a string is least half as long as 
 *	(back - front), which in turn implies that a linear search will 
 *	be no more expensive than the cost of simply printing a string or two.
 * 
 * 	Trying to continue with binary search at this point would be 
 *	more trouble than it's worth.
 */
#define	SKIP_PAST_NEWLINE(p, back) \
	while (p < back && *p++ != '\n');

char *
binary_search(string, front, back)
	register char *string, *front, *back;
{
	register char *p;

	p = front + (back - front) / 2;
	SKIP_PAST_NEWLINE(p, back);

	while (p != back) {
		if (compare(string, p, back) == GREATER)
			front = p;
		else
			back = p;
		p = front + (back - front) / 2;
		SKIP_PAST_NEWLINE(p, back);
	}
	return (front);
}

/*
 * Find the first line that starts with string, linearly searching from front
 * to back.
 * 
 * Return NULL for no such line.
 * 
 * This routine assumes:
 * 
 * 	o front points at the first character in a line. 
 *	o front is before or at the first line to be printed.
 */
char *
linear_search(string, front, back)
	char *string, *front, *back;
{
	while (front < back) {
		switch (compare(string, front, back)) {
		case EQUAL:		/* Found it. */
			return (front);
			break;
		case LESS:		/* No such string. */
			return (NULL);
			break;
		case GREATER:		/* Keep going. */
			break;
		}
		SKIP_PAST_NEWLINE(front, back);
	}
	return (NULL);
}

/*
 * Print as many lines as match string, starting at front.
 */
void 
print_from(string, front, back)
	register char *string, *front, *back;
{
	for (; front < back && compare(string, front, back) == EQUAL; ++front) {
		for (; front < back && *front != '\n'; ++front)
			if (putchar(*front) == EOF)
				err("stdout: %s", strerror(errno));
		if (putchar('\n') == EOF)
			err("stdout: %s", strerror(errno));
	}
}

/*
 * Return LESS, GREATER, or EQUAL depending on how the string1 compares with
 * string2 (s1 ??? s2).
 * 
 * 	o Matches up to len(s1) are EQUAL. 
 *	o Matches up to len(s2) are GREATER.
 * 
 * Compare understands about the -f and -d flags, and treats comparisons
 * appropriately.
 * 
 * The string "s1" is null terminated.  The string s2 is '\n' terminated (or
 * "back" terminated).
 */
int
compare(s1, s2, back)
	register char *s1, *s2, *back;
{
	register int ch;

	for (; *s1 && s2 < back && *s2 != '\n'; ++s1, ++s2) {
		ch = *s2;
		if (fflag)
			ch = FOLD(ch);
		if (dflag)
			ch = DICT(ch);

		if (ch == NO_COMPARE) {
			++s2;		/* Ignore character in comparison. */
			continue;
		}
		if (*s1 != ch)
			return (*s1 < ch ? LESS : GREATER);
	}
	return (*s1 ? GREATER : EQUAL);
}

static void
usage()
{
	(void)fprintf(stderr, "usage: look [-df] string [file]\n");
	exit(2);
}

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

void
#if __STDC__
err(const char *fmt, ...)
#else
err(fmt, va_alist)
	char *fmt;
	va_dcl
#endif
{
	va_list ap;
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void)fprintf(stderr, "look: ");
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, "\n");
	exit(2);
	/* NOTREACHED */
}
