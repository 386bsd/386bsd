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
static char sccsid[] = "@(#)edit.c	5.15 (Berkeley) 6/25/90";
#endif /* not lint */

#include "rcv.h"
#include <sys/stat.h>

/*
 * Mail -- a mail program
 *
 * Perform message editing functions.
 */

/*
 * Edit a message list.
 */

editor(msgvec)
	int *msgvec;
{

	return edit1(msgvec, 'e');
}

/*
 * Invoke the visual editor on a message list.
 */

visual(msgvec)
	int *msgvec;
{

	return edit1(msgvec, 'v');
}

/*
 * Edit a message by writing the message into a funnily-named file
 * (which should not exist) and forking an editor on it.
 * We get the editor from the stuff above.
 */
edit1(msgvec, type)
	int *msgvec;
	char type;
{
	register int c;
	int i;
	FILE *fp;
	register struct message *mp;
	off_t size;

	/*
	 * Deal with each message to be edited . . .
	 */
	for (i = 0; msgvec[i] && i < msgCount; i++) {
		sig_t sigint;

		if (i > 0) {
			char buf[100];
			char *p;

			printf("Edit message %d [ynq]? ", msgvec[i]);
			if (fgets(buf, sizeof buf, stdin) == 0)
				break;
			for (p = buf; *p == ' ' || *p == '\t'; p++)
				;
			if (*p == 'q')
				break;
			if (*p == 'n')
				continue;
		}
		dot = mp = &message[msgvec[i] - 1];
		touch(mp);
		sigint = signal(SIGINT, SIG_IGN);
		fp = run_editor(setinput(mp), mp->m_size, type, readonly);
		if (fp != NULL) {
			(void) fseek(otf, (long) 0, 2);
			size = ftell(otf);
			mp->m_block = blockof(size);
			mp->m_offset = offsetof(size);
			mp->m_size = fsize(fp);
			mp->m_lines = 0;
			mp->m_flag |= MODIFY;
			rewind(fp);
			while ((c = getc(fp)) != EOF) {
				if (c == '\n')
					mp->m_lines++;
				if (putc(c, otf) == EOF)
					break;
			}
			if (ferror(otf))
				perror("/tmp");
			(void) Fclose(fp);
		}
		(void) signal(SIGINT, sigint);
	}
	return 0;
}

/*
 * Run an editor on the file at "fpp" of "size" bytes,
 * and return a new file pointer.
 * Signals must be handled by the caller.
 * "Type" is 'e' for _PATH_EX, 'v' for _PATH_VI.
 */
FILE *
run_editor(fp, size, type, readonly)
	register FILE *fp;
	off_t size;
	char type;
{
	register FILE *nf = NULL;
	register int t;
	time_t modtime;
	char *edit;
	struct stat statb;
	extern char tempEdit[];

	if ((t = creat(tempEdit, readonly ? 0400 : 0600)) < 0) {
		perror(tempEdit);
		goto out;
	}
	if ((nf = Fdopen(t, "w")) == NULL) {
		perror(tempEdit);
		(void) unlink(tempEdit);
		goto out;
	}
	if (size >= 0)
		while (--size >= 0 && (t = getc(fp)) != EOF)
			(void) putc(t, nf);
	else
		while ((t = getc(fp)) != EOF)
			(void) putc(t, nf);
	(void) fflush(nf);
	if (fstat(fileno(nf), &statb) < 0)
		modtime = 0;
	else
		modtime = statb.st_mtime;
	if (ferror(nf)) {
		(void) Fclose(nf);
		perror(tempEdit);
		(void) unlink(tempEdit);
		nf = NULL;
		goto out;
	}
	if (Fclose(nf) < 0) {
		perror(tempEdit);
		(void) unlink(tempEdit);
		nf = NULL;
		goto out;
	}
	nf = NULL;
	if ((edit = value(type == 'e' ? "EDITOR" : "VISUAL")) == NOSTR)
		edit = type == 'e' ? _PATH_EX : _PATH_VI;
	if (run_command(edit, 0, -1, -1, tempEdit, NOSTR) < 0) {
		(void) unlink(tempEdit);
		goto out;
	}
	/*
	 * If in read only mode or file unchanged, just remove the editor
	 * temporary and return.
	 */
	if (readonly) {
		(void) unlink(tempEdit);
		goto out;
	}
	if (stat(tempEdit, &statb) < 0) {
		perror(tempEdit);
		goto out;
	}
	if (modtime == statb.st_mtime) {
		(void) unlink(tempEdit);
		goto out;
	}
	/*
	 * Now switch to new file.
	 */
	if ((nf = Fopen(tempEdit, "a+")) == NULL) {
		perror(tempEdit);
		(void) unlink(tempEdit);
		goto out;
	}
	(void) unlink(tempEdit);
out:
	return nf;
}
