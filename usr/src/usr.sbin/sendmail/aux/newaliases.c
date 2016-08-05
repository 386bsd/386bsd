/*
 * Copyright (c) 1983 Eric P. Allman
 * Copyright (c) 1988 Regents of the University of California.
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
char copyright[] =
"@(#) Copyright (c) 1988 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)newaliases.c	5.4 (Berkeley) 6/1/90";
#endif /* not lint */

# include <stdio.h>
# include <ctype.h>
# include "sendmail.h"
# include "pathnames.h"

static	char SccsId[] = "@(#)newaliases.c	5.4	6/1/90";

typedef struct { char *dptr; int dsize; } datum;
char *aliases = ALIASFILE;
char dirbuf[100];
char pagbuf[100];
int LineNo;
char *To;
int ExitStat;
int Errors;
HDR *Header;
struct mailer *Mailer[MAXMAILERS+1];
int NextMailer = 0;
# ifdef DEBUG
int Debug;
# endif DEBUG

main(argc, argv)
	int argc;
	char *argv[];
{
	int f;
	char line[BUFSIZ];
	register char *p;
	char *p2;
	char *rhs;
	int naliases, bytes, longest;
	datum key, content;
	bool skipping;
	ADDRESS al, bl;
	extern char *prescan();
	extern ADDRESS *parse();
	bool contin;
	char *cffile = _PATH_SENDMAILCF;

# ifdef DEBUG
	if (argc > 1 && strcmp(argv[1], "-T") == 0)
	{
		Debug = 100;
		argc--;
		argv++;
	}
# endif DEBUG
	if (argc > 1)
		aliases = argv[1];
	if (argc > 2)
		cffile = argv[2];
	readcf(cffile);

	(void) strcpy(dirbuf, aliases);
	(void) strcat(dirbuf, ".dir");
	(void) strcpy(pagbuf, aliases);
	(void) strcat(pagbuf, ".pag");
	f = creat(dirbuf, 0666);
	if (f < 0) {
		perror(dirbuf);
		exit(1);
	}
	(void) close(f);
	f = creat(pagbuf, 0666);
	if (f < 0) {
		perror(pagbuf);
		exit(1);
	}
	(void) close(f);
	if (dbminit(aliases) < 0)
		exit(1);
	if (freopen(aliases, "r", stdin) == 0) {
		perror(aliases);
		exit(1);
	}

	/* read and interpret lines */
	LineNo = 0;
	naliases = 0;
	bytes = 0;
	longest = 0;
	skipping = FALSE;
	while (fgets(line, sizeof (line), stdin) != NULL)
	{
		LineNo++;
		switch (line[0])
		{
		  case '#':
		  case '\n':
		  case '\0':
			skipping = FALSE;
			continue;

		  case ' ':
		  case '\t':
			if (!skipping)
				usrerr("Non-continuation line starts with space");
			skipping = TRUE;
			continue;
		}
		skipping = FALSE;

		/* process the LHS */
		for (p = line; *p != '\0' && *p != ':' && *p != '\n'; p++)
			continue;
		if (*p == '\0' || *p == '\n')
		{
		 syntaxerr:
			usrerr("missing colon");
			continue;
		}
		*p++ = '\0';
		if (parse(line, &al, 1) == NULL)
		{
			*--p = ':';
			goto syntaxerr;
		}
		rhs = p;
		contin = FALSE;
		for (;;)
		{
			register char c;

			/* do parsing & compression of addresses */
			c = *p;
			while (c != '\0')
			{
				p2 = p;
				while (*p != '\n' && *p != ',' && *p != '\0')
					p++;
				c = *p;
				*p++ = '\0';
				if (*p2 == '\0')
				{
					p[-1] = c;
					continue;
				}
				parse(p2, &bl, -1);
				contin = (c == ',');
				p[-1] = c;
				while (isspace(*p))
					p++;
			}

			/* see if there should be a continuation line */
			if (!contin)
				break;

			/* read continuation line */
			p--;
			if (fgets(p, sizeof line - (p - line), stdin) == NULL)
				break;
			LineNo++;

			if (!isspace(*p))
				usrerr("continuation line missing");
		}
		if (al.q_mailer != MN_LOCAL)
		{
			usrerr("cannot alias non-local names");
			continue;
		}
		naliases++;
		key.dsize = strlen(al.q_user) + 1;
		key.dptr = al.q_user;
		content.dsize = strlen(rhs) + 1;
		if (content.dsize > longest)
			longest = content.dsize;
		content.dptr = rhs;
		bytes += key.dsize + content.dsize;
		if (store(key, content), 0)
		/* if (f = store(key, content)) */
			usrerr("Dbm internal error return %d from store\n", f);
	}
	fprintf(stderr, "%d aliases, %d bytes, longest %d bytes, %d errors\n",
	    naliases, bytes, longest, Errors);

	exit(ExitStat);
}

usrerr(fmt, a, b, c, d, e)
	char *fmt;
{
	Errors++;
	fprintf(stderr, "line %d: ", LineNo);
	fprintf(stderr, fmt, a, b, c, d, e);
	fprintf(stderr, "\n");
	return (-1);
}

syserr(fmt, a, b, c, d, e)
	char *fmt;
{
	return (usrerr(fmt, a, b, c, d, e));
}
