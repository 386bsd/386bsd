/*-
 * Copyright (c) 1989 The Regents of the University of California.
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
static char sccsid[] = "@(#)compare.c	5.7 (Berkeley) 5/25/90";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <fts.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include "mtree.h"

#define	LABEL \
	if (!label++) \
		(void)printf("%s: ", RP(p)); \

compare(name, s, p)
	char *name;
	register NODE *s;
	register FTSENT *p;
{
	extern int exitval, uflag;
	int label;
	char *ftype(), *inotype(), *rlink();

	label = 0;
	switch(s->type) {
	case F_BLOCK:
		if (!S_ISBLK(p->fts_statp->st_mode))
			goto typeerr;
		break;
	case F_CHAR:
		if (!S_ISCHR(p->fts_statp->st_mode))
			goto typeerr;
		break;
	case F_DIR:
		if (!S_ISDIR(p->fts_statp->st_mode))
			goto typeerr;
		break;
	case F_FIFO:
		if (!S_ISFIFO(p->fts_statp->st_mode))
			goto typeerr;
		break;
	case F_FILE:
		if (!S_ISREG(p->fts_statp->st_mode))
			goto typeerr;
		break;
	case F_LINK:
		if (!S_ISLNK(p->fts_statp->st_mode))
			goto typeerr;
		break;
	case F_SOCK:
		if (!S_ISFIFO(p->fts_statp->st_mode)) {
typeerr:		LABEL;
			(void)printf("\n\ttype (%s, %s)",
			    ftype(s->type), inotype(p->fts_statp->st_mode));
		}
		break;
	}
	if (s->flags & F_MODE && s->st_mode != (p->fts_statp->st_mode & MBITS)) {
		LABEL;
		(void)printf("\n\tpermissions (%#o, %#o%s",
		    s->st_mode, p->fts_statp->st_mode & MBITS, uflag ? "" : ")");
		if (uflag)
			if (chmod(p->fts_accpath, s->st_mode))
				(void)printf(", not modified: %s)",
				    strerror(errno));
			else
				(void)printf(", modified)");
	}
	if (s->flags & F_OWNER && s->st_uid != p->fts_statp->st_uid) {
		LABEL;
		(void)printf("\n\towner (%u, %u%s",
		    s->st_uid, p->fts_statp->st_uid, uflag ? "" : ")");
		if (uflag)
			if (chown(p->fts_accpath, s->st_uid, -1))
				(void)printf(", not modified: %s)",
				    strerror(errno));
			else
				(void)printf(", modified)");
	}
	if (s->flags & F_GROUP && s->st_gid != p->fts_statp->st_gid) {
		LABEL;
		(void)printf("\n\tgroup (%u, %u%s",
		    s->st_gid, p->fts_statp->st_gid, uflag ? "" : ")");
		if (uflag)
			if (chown(p->fts_accpath, -1, s->st_gid))
				(void)printf(", not modified: %s)",
				    strerror(errno));
			else
				(void)printf(", modified)");
	}
	if (s->flags & F_NLINK && s->type != F_DIR &&
	    s->st_nlink != p->fts_statp->st_nlink) {
		LABEL;
		(void)printf("\n\tlink count (%u, %u)",
		    s->st_nlink, p->fts_statp->st_nlink);
	}
	if (s->flags & F_SIZE && s->st_size != p->fts_statp->st_size) {
		LABEL;
		(void)printf("\n\tsize (%ld, %ld)",
		    s->st_size, p->fts_statp->st_size);
	}
	if (s->flags & F_SLINK) {
		char *cp;

		if (strcmp(cp = rlink(name), s->slink)) {
			LABEL;
			(void)printf("\n\tlink ref (%s, %s)", cp, s->slink);
		}
	}
	if (s->flags & F_TIME && s->st_mtime != p->fts_statp->st_mtime) {
		LABEL;
		(void)printf("\n\tmodification time (%.24s, ",
		    ctime(&s->st_mtime));
		(void)printf("%.24s)", ctime(&p->fts_statp->st_mtime));
	}
	if (label) {
		exitval = 2;
		putchar('\n');
	}
}

char *
inotype(type)
	mode_t type;
{
	switch(type & S_IFMT) {
	case S_IFBLK:
		return("block");
	case S_IFCHR:
		return("char");
	case S_IFDIR:
		return("dir");
	case S_IFREG:
		return("file");
	case S_IFLNK:
		return("link");
	case S_IFSOCK:
		return("socket");
	default:
		return("unknown");
	}
	/* NOTREACHED */
}

char *
ftype(type)
	u_int type;
{
	switch(type) {
	case F_BLOCK:
		return("block");
	case F_CHAR:
		return("char");
	case F_DIR:
		return("dir");
	case F_FIFO:
		return("fifo");
	case F_FILE:
		return("file");
	case F_LINK:
		return("link");
	case F_SOCK:
		return("socket");
	default:
		return("unknown");
	}
	/* NOTREACHED */
}

char *
rlink(name)
	char *name;
{
	register int len;
	static char lbuf[PATH_MAX];

	len = readlink(name, lbuf, sizeof(lbuf));
	if (len == -1) {
		(void)fprintf(stderr, "mtree: %s: %s.\n",
		    name, strerror(errno));
		exit(1);
	}
	lbuf[len] = '\0';
	return(lbuf);
}
