/*-
 * Copyright (c) 1990 The Regents of the University of California.
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
static char sccsid[] = "@(#)edit.c	5.2 (Berkeley) 3/3/91";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <pwd.h>
#include <errno.h>
#include <stdio.h>
#include <paths.h>
#include <stdlib.h>
#include <string.h>
#include "chpass.h"

extern char *tempname;

void
edit(pw)
	struct passwd *pw;
{
	struct stat begin, end;

	for (;;) {
		if (stat(tempname, &begin))
			pw_error(tempname, 1, 1);
		pw_edit(1);
		if (stat(tempname, &end))
			pw_error(tempname, 1, 1);
		if (begin.st_mtime == end.st_mtime) {
			(void)fprintf(stderr, "chpass: no changes made\n");
			pw_error((char *)NULL, 0, 0);
		}
		if (verify(pw))
			break;
		pw_prompt();
	}
}

/*
 * display --
 *	print out the file for the user to edit; strange side-effect:
 *	set conditional flag if the user gets to edit the shell.
 */
display(fd, pw)
	int fd;
	struct passwd *pw;
{
	register char *p;
	FILE *fp;
	char *bp, *ok_shell(), *ttoa();

	if (!(fp = fdopen(fd, "w")))
		pw_error(tempname, 1, 1);

	(void)fprintf(fp,
	    "#Changing user database information for %s.\n", pw->pw_name);
	if (!uid) {
		(void)fprintf(fp, "Login: %s\n", pw->pw_name);
		(void)fprintf(fp, "Password: %s\n", pw->pw_passwd);
		(void)fprintf(fp, "Uid [#]: %d\n", pw->pw_uid);
		(void)fprintf(fp, "Gid [# or name]: %d\n", pw->pw_gid);
		(void)fprintf(fp, "Change [month day year]: %s\n",
		    ttoa(pw->pw_change));
		(void)fprintf(fp, "Expire [month day year]: %s\n",
		    ttoa(pw->pw_expire));
		(void)fprintf(fp, "Class: %s\n", pw->pw_class);
		(void)fprintf(fp, "Home directory: %s\n", pw->pw_dir);
		(void)fprintf(fp, "Shell: %s\n",
		    *pw->pw_shell ? pw->pw_shell : _PATH_BSHELL);
	}
	/* Only admin can change "restricted" shells. */
	else if (ok_shell(pw->pw_shell))
		/*
		 * Make shell a restricted field.  Ugly with a
		 * necklace, but there's not much else to do.
		 */
		(void)fprintf(fp, "Shell: %s\n",
		    *pw->pw_shell ? pw->pw_shell : _PATH_BSHELL);
	else
		list[E_SHELL].restricted = 1;
	bp = pw->pw_gecos;
	p = strsep(&bp, ",");
	(void)fprintf(fp, "Full Name: %s\n", p ? p : "");
	p = strsep(&bp, ",");
	(void)fprintf(fp, "Location: %s\n", p ? p : "");
	p = strsep(&bp, ",");
	(void)fprintf(fp, "Office Phone: %s\n", p ? p : "");
	p = strsep(&bp, ",");
	(void)fprintf(fp, "Home Phone: %s\n", p ? p : "");

	(void)fchown(fd, getuid(), getgid());
	(void)fclose(fp);
}

verify(pw)
	struct passwd *pw;
{
	register ENTRY *ep;
	register char *p;
	FILE *fp;
	int len;
	char buf[LINE_MAX];

	if (!(fp = fopen(tempname, "r")))
		pw_error(tempname, 1, 1);
	while (fgets(buf, sizeof(buf), fp)) {
		if (!buf[0] || buf[0] == '#')
			continue;
		if (!(p = index(buf, '\n'))) {
			(void)fprintf(stderr, "chpass: line too long.\n");
			goto bad;
		}
		*p = '\0';
		for (ep = list;; ++ep) {
			if (!ep->prompt) {
				(void)fprintf(stderr,
				    "chpass: unrecognized field.\n");
				goto bad;
			}
			if (!strncasecmp(buf, ep->prompt, ep->len)) {
				if (ep->restricted && uid) {
					(void)fprintf(stderr,
			    "chpass: you may not change the %s field.\n",
					    ep->prompt);
					goto bad;
				}
				if (!(p = index(buf, ':'))) {
					(void)fprintf(stderr,
					    "chpass: line corrupted.\n");
					goto bad;
				}
				while (isspace(*++p));
				if (ep->except && strpbrk(p, ep->except)) {
					(void)fprintf(stderr,
			    "chpass: illegal character in the \"%s\" field.\n",
					    ep->prompt);
					goto bad;
				}
				if ((ep->func)(p, pw, ep)) {
bad:					(void)fclose(fp);
					return(0);
				}
				break;
			}
		}
	}
	(void)fclose(fp);

	/* Build the gecos field. */
	len = strlen(list[E_NAME].save) + strlen(list[E_BPHONE].save) +
	    strlen(list[E_HPHONE].save) + strlen(list[E_LOCATE].save) + 4;
	if (!(p = malloc(len))) {
		(void)fprintf(stderr, "chpass: %s\n", strerror(errno));
		exit(1);
	}
	(void)sprintf(pw->pw_gecos = p, "%s,%s,%s,%s", list[E_NAME].save,
	    list[E_LOCATE].save, list[E_BPHONE].save, list[E_HPHONE].save);

	if (snprintf(buf, sizeof(buf),
	    "%s:%s:%d:%d:%s:%ld:%ld:%s:%s:%s",
	    pw->pw_name, pw->pw_passwd, pw->pw_uid, pw->pw_gid, pw->pw_class,
	    pw->pw_change, pw->pw_expire, pw->pw_gecos, pw->pw_dir,
	    pw->pw_shell) >= sizeof(buf)) {
		(void)fprintf(stderr, "chpass: entries too long\n");
		return(0);
	}
	return(pw_scan(buf, pw));
}
