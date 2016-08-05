/*-
 * Copyright (c) 1992 The Regents of the University of California.
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
"@(#) Copyright (c) 1992 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)dbtest.c	5.18 (Berkeley) 5/22/93";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <db.h>

enum S { COMMAND, COMPARE, GET, PUT, REMOVE, SEQ, SEQFLAG, KEY, DATA };

void	 compare __P((DBT *, DBT *));
DBTYPE	 dbtype __P((char *));
void	 dump __P((DB *, int));
void	 err __P((const char *, ...));
void	 get __P((DB *, DBT *));
void	 getdata __P((DB *, DBT *, DBT *));
void	 put __P((DB *, DBT *, DBT *));
void	 rem __P((DB *, DBT *));
void	*rfile __P((char *, size_t *));
void	 seq __P((DB *, DBT *));
u_int	 setflags __P((char *));
void	*setinfo __P((DBTYPE, char *));
void	 usage __P((void));
void	*xmalloc __P((char *, size_t));

DBTYPE type;
void *infop;
u_long lineno;
u_int flags;
int ofd = STDOUT_FILENO;

DB *XXdbp;				/* Global for gdb. */

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern int optind;
	extern char *optarg;
	enum S command, state;
	DB *dbp;
	DBT data, key, keydata;
	size_t len;
	int ch;
	char *fname, *infoarg, *p, buf[8 * 1024];

	infoarg = NULL;
	fname = NULL;
	while ((ch = getopt(argc, argv, "f:i:o:")) != EOF)
		switch(ch) {
		case 'f':
			fname = optarg;
			break;
		case 'i':
			infoarg = optarg;
			break;
		case 'o':
			if ((ofd = open(optarg,
			    O_WRONLY|O_CREAT|O_TRUNC, 0666)) < 0)
				err("%s: %s", optarg, strerror(errno));
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 2)
		usage();

	/* Set the type. */
	type = dbtype(*argv++);

	/* Open the descriptor file. */
	if (freopen(*argv, "r", stdin) == NULL)
		err("%s: %s", *argv, strerror(errno));

	/* Set up the db structure as necessary. */
	if (infoarg == NULL)
		infop = NULL;
	else
		for (p = strtok(infoarg, ",\t "); p != NULL;
		    p = strtok(0, ",\t "))
			if (*p != '\0')
				infop = setinfo(type, p);

	/* Open the DB. */
	if (fname == NULL) {
		p = getenv("TMPDIR");
		if (p == NULL)
			p = "/var/tmp";
		(void)sprintf(buf, "%s/__dbtest", p);
		fname = buf;
		(void)unlink(buf);
	}
	if ((dbp = dbopen(fname,
	    O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, type, infop)) == NULL)
		err("dbopen: %s", strerror(errno));
	XXdbp = dbp;

	state = COMMAND;
	for (lineno = 1;
	    (p = fgets(buf, sizeof(buf), stdin)) != NULL; ++lineno) {
		len = strlen(buf);
		switch(*p) {
		case 'c':			/* compare */
			if (state != COMMAND)
				err("line %lu: not expecting command", lineno);
			state = KEY;
			command = COMPARE;
			break;
		case 'e':			/* echo */
			if (state != COMMAND)
				err("line %lu: not expecting command", lineno);
			/* Don't display the newline, if CR at EOL. */
			if (p[len - 2] == '\r')
				--len;
			if (write(ofd, p + 1, len - 1) != len - 1)
				err("write: %s", strerror(errno));
			break;
		case 'g':			/* get */
			if (state != COMMAND)
				err("line %lu: not expecting command", lineno);
			state = KEY;
			command = GET;
			break;
		case 'p':			/* put */
			if (state != COMMAND)
				err("line %lu: not expecting command", lineno);
			state = KEY;
			command = PUT;
			break;
		case 'r':			/* remove */
			if (state != COMMAND)
				err("line %lu: not expecting command", lineno);
			state = KEY;
			command = REMOVE;
			break;
		case 's':			/* seq */
			if (state != COMMAND)
				err("line %lu: not expecting command", lineno);
			if (flags == R_CURSOR) {
				state = KEY;
				command = SEQ;
			} else
				seq(dbp, &key);
			break;
		case 'f':
			flags = setflags(p + 1);
			break;
		case 'D':			/* data file */
			if (state != DATA)
				err("line %lu: not expecting data", lineno);
			data.data = rfile(p + 1, &data.size);
			goto ldata;
		case 'd':			/* data */
			if (state != DATA)
				err("line %lu: not expecting data", lineno);
			data.data = xmalloc(p + 1, len - 1);
			data.size = len - 1;
ldata:			switch(command) {
			case COMPARE:
				compare(&keydata, &data);
				break;
			case PUT:
				put(dbp, &key, &data);
				break;
			default:
				err("line %lu: command doesn't take data",
				    lineno);
			}
			if (type != DB_RECNO)
				free(key.data);
			free(data.data);
			state = COMMAND;
			break;
		case 'K':			/* key file */
			if (state != KEY)
				err("line %lu: not expecting a key", lineno);
			if (type == DB_RECNO)
				err("line %lu: 'K' not available for recno",
				    lineno);
			key.data = rfile(p + 1, &key.size);
			goto lkey;
		case 'k':			/* key */
			if (state != KEY)
				err("line %lu: not expecting a key", lineno);
			if (type == DB_RECNO) {
				static recno_t recno;
				recno = strtol(p + 1, NULL, 0);
				key.data = &recno;
				key.size = sizeof(recno);
			} else {
				key.data = xmalloc(p + 1, len - 1);
				key.size = len - 1;
			}
lkey:			switch(command) {
			case COMPARE:
				getdata(dbp, &key, &keydata);
				state = DATA;
				break;
			case GET:
				get(dbp, &key);
				if (type != DB_RECNO)
					free(key.data);
				state = COMMAND;
				break;
			case PUT:
				state = DATA;
				break;
			case REMOVE:
				rem(dbp, &key);
				if (type != DB_RECNO)
					free(key.data);
				state = COMMAND;
				break;
			case SEQ:
				seq(dbp, &key);
				if (type != DB_RECNO)
					free(key.data);
				state = COMMAND;
				break;
			default:
				err("line %lu: command doesn't take a key",
				    lineno);
			}
			break;
		case 'o':
			dump(dbp, p[1] == 'r');
			break;
		default:
			err("line %lu: %s: unknown command character",
			    p, lineno);
		}
	}
	if (dbp->close(dbp))
		err("db->close: %s", strerror(errno));
	(void)close(ofd);
	exit(0);
}

#define	NOOVERWRITE	"put failed, would overwrite key\n"
#define	NOSUCHKEY	"get failed, no such key\n"

void
compare(db1, db2)
	DBT *db1, *db2;
{
	register size_t len;
	register u_char *p1, *p2;

	if (db1->size != db2->size)
		printf("compare failed: key->data len %lu != data len %lu\n",
		    db1->size, db2->size);

	len = MIN(db1->size, db2->size);
	for (p1 = db1->data, p2 = db2->data; len--;)
		if (*p1++ != *p2++) {
			printf("compare failed at offset %d\n",
			    p1 - (u_char *)db1->data);
			break;
		}
}

void
get(dbp, kp)
	DB *dbp;
	DBT *kp;
{
	DBT data;

	switch(dbp->get(dbp, kp, &data, flags)) {
	case 0:
		(void)write(ofd, data.data, data.size);
		break;
	case -1:
		err("line %lu: get: %s", lineno, strerror(errno));
		/* NOTREACHED */
	case 1:
		(void)write(ofd, NOSUCHKEY, sizeof(NOSUCHKEY) - 1);
		(void)fprintf(stderr, "%d: %.*s: %s\n", 
		    lineno, kp->size, kp->data, NOSUCHKEY);
		break;
	}
}

void
getdata(dbp, kp, dp)
	DB *dbp;
	DBT *kp, *dp;
{
	switch(dbp->get(dbp, kp, dp, flags)) {
	case 0:
		return;
	case -1:
		err("line %lu: getdata: %s", lineno, strerror(errno));
		/* NOTREACHED */
	case 1:
		err("line %lu: get failed, no such key", lineno);
		/* NOTREACHED */
	}
}

void
put(dbp, kp, dp)
	DB *dbp;
	DBT *kp, *dp;
{
	switch(dbp->put(dbp, kp, dp, flags)) {
	case 0:
		break;
	case -1:
		err("line %lu: put: %s", lineno, strerror(errno));
		/* NOTREACHED */
	case 1:
		(void)write(ofd, NOOVERWRITE, sizeof(NOOVERWRITE) - 1);
		break;
	}
}

void
rem(dbp, kp)
	DB *dbp;
	DBT *kp;
{
	switch(dbp->del(dbp, kp, flags)) {
	case 0:
		break;
	case -1:
		err("line %lu: get: %s", lineno, strerror(errno));
		/* NOTREACHED */
	case 1:
		(void)write(ofd, NOSUCHKEY, sizeof(NOSUCHKEY) - 1);
		break;
	}
}

void
seq(dbp, kp)
	DB *dbp;
	DBT *kp;
{
	DBT data;

	switch(dbp->seq(dbp, kp, &data, flags)) {
	case 0:
		(void)write(ofd, data.data, data.size);
		break;
	case -1:
		err("line %lu: seq: %s", lineno, strerror(errno));
		/* NOTREACHED */
	case 1:
		(void)write(ofd, NOSUCHKEY, sizeof(NOSUCHKEY) - 1);
		break;
	}
}

void
dump(dbp, rev)
	DB *dbp;
	int rev;
{
	DBT key, data;
	int flags, nflags;

	if (rev) {
		flags = R_LAST;
		nflags = R_PREV;
	} else {
		flags = R_FIRST;
		nflags = R_NEXT;
	}
	for (;; flags = nflags)
		switch(dbp->seq(dbp, &key, &data, flags)) {
		case 0:
			(void)write(ofd, data.data, data.size);
			break;
		case 1:
			goto done;
		case -1:
			err("line %lu: (dump) seq: %s",
			    lineno, strerror(errno));
			/* NOTREACHED */
		}
done:	return;
}
	
u_int
setflags(s)
	char *s;
{
	char *p;

	for (; isspace(*s); ++s);
	if (*s == '\n')
		return (0);
	if ((p = index(s, '\n')) != NULL)
		*p = '\0';
	if (!strcmp(s, "R_CURSOR"))
		return (R_CURSOR);
	if (!strcmp(s, "R_FIRST"))
		return (R_FIRST);
	if (!strcmp(s, "R_IAFTER"))
		return (R_IAFTER);
	if (!strcmp(s, "R_IBEFORE"))
		return (R_IBEFORE);
	if (!strcmp(s, "R_LAST"))
		return (R_LAST);
	if (!strcmp(s, "R_NEXT"))
		return (R_NEXT);
	if (!strcmp(s, "R_NOOVERWRITE"))
		return (R_NOOVERWRITE);
	if (!strcmp(s, "R_PREV"))
		return (R_PREV);
	if (!strcmp(s, "R_SETCURSOR"))
		return (R_SETCURSOR);
	err("line %lu: %s: unknown flag", lineno, s);
	/* NOTREACHED */
}
	
DBTYPE
dbtype(s)
	char *s;
{
	if (!strcmp(s, "btree"))
		return (DB_BTREE);
	if (!strcmp(s, "hash"))
		return (DB_HASH);
	if (!strcmp(s, "recno"))
		return (DB_RECNO);
	err("%s: unknown type (use btree, hash or recno)", s);
	/* NOTREACHED */
}

void *
setinfo(type, s)
	DBTYPE type;
	char *s;
{
	static BTREEINFO ib;
	static HASHINFO ih;
	static RECNOINFO rh;
	char *eq;

	if ((eq = index(s, '=')) == NULL)
		err("%s: illegal structure set statement", s);
	*eq++ = '\0';
	if (!isdigit(*eq))
		err("%s: structure set statement must be a number", s);
		
	switch(type) {
	case DB_BTREE:
		if (!strcmp("flags", s)) {
			ib.flags = strtoul(eq, NULL, 0);
			return (&ib);
		}
		if (!strcmp("cachesize", s)) {
			ib.cachesize = strtoul(eq, NULL, 0);
			return (&ib);
		}
		if (!strcmp("maxkeypage", s)) {
			ib.maxkeypage = strtoul(eq, NULL, 0);
			return (&ib);
		}
		if (!strcmp("minkeypage", s)) {
			ib.minkeypage = strtoul(eq, NULL, 0);
			return (&ib);
		}
		if (!strcmp("lorder", s)) {
			ib.lorder = strtoul(eq, NULL, 0);
			return (&ib);
		}
		if (!strcmp("psize", s)) {
			ib.psize = strtoul(eq, NULL, 0);
			return (&ib);
		}
		break;
	case DB_HASH:
		if (!strcmp("bsize", s)) {
			ih.bsize = strtoul(eq, NULL, 0);
			return (&ih);
		}
		if (!strcmp("ffactor", s)) {
			ih.ffactor = strtoul(eq, NULL, 0);
			return (&ih);
		}
		if (!strcmp("nelem", s)) {
			ih.nelem = strtoul(eq, NULL, 0);
			return (&ih);
		}
		if (!strcmp("cachesize", s)) {
			ih.cachesize = strtoul(eq, NULL, 0);
			return (&ih);
		}
		if (!strcmp("lorder", s)) {
			ih.lorder = strtoul(eq, NULL, 0);
			return (&ih);
		}
		break;
	case DB_RECNO:
		if (!strcmp("flags", s)) {
			rh.flags = strtoul(eq, NULL, 0);
			return (&rh);
		}
		if (!strcmp("cachesize", s)) {
			rh.cachesize = strtoul(eq, NULL, 0);
			return (&rh);
		}
		if (!strcmp("lorder", s)) {
			rh.lorder = strtoul(eq, NULL, 0);
			return (&rh);
		}
		if (!strcmp("reclen", s)) {
			rh.reclen = strtoul(eq, NULL, 0);
			return (&rh);
		}
		if (!strcmp("bval", s)) {
			rh.bval = strtoul(eq, NULL, 0);
			return (&rh);
		}
		if (!strcmp("psize", s)) {
			rh.psize = strtoul(eq, NULL, 0);
			return (&rh);
		}
		break;
	}
	err("%s: unknown structure value", s);
	/* NOTREACHED */
}

void *
rfile(name, lenp)
	char *name;
	size_t *lenp;
{
	struct stat sb;
	void *p;
	int fd;
	char *np;

	for (; isspace(*name); ++name);
	if ((np = index(name, '\n')) != NULL)
		*np = '\0';
	if ((fd = open(name, O_RDONLY, 0)) < 0 ||
	    fstat(fd, &sb))
		err("%s: %s\n", name, strerror(errno));
	/* if (sb.st_size > (off_t)SIZE_T_MAX)
		err("%s: %s\n", name, strerror(E2BIG)); */
	if ((p = malloc((u_int)sb.st_size)) == NULL)
		err("%s", strerror(errno));
	(void)read(fd, p, (int)sb.st_size);
	*lenp = sb.st_size;
	(void)close(fd);
	return (p);
}

void *
xmalloc(text, len)
	char *text;
	size_t len;
{
	void *p;

	if ((p = malloc(len)) == NULL)
		err("%s", strerror(errno));
	memmove(p, text, len);
	return (p);
}

void
usage()
{
	(void)fprintf(stderr,
	    "usage: dbtest [-f file] [-i info] [-o file] type script\n");
	exit(1);
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
	(void)fprintf(stderr, "dbtest: ");
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, "\n");
	exit(1);
	/* NOTREACHED */
}
