/*
 * Copyright (c) 1983 Regents of the University of California.
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
static char sccsid[] = "@(#)parse.c	5.3 (Berkeley) 6/1/90";
#endif /* not lint */

#include "externs.h"

wordinit()
{
	register struct wlist *w;

	for (w = wlist; w->string; w++)
		install(w);
}

hash(s)
	register char *s;
{
	register hashval = 0;

	while (*s) {
		hashval += *s++;
		hashval *= HASHMUL;
		hashval &= HASHMASK;
	}
	return hashval;
}

struct wlist *
lookup(s)
	char *s;
{
	register struct wlist *wp;

	for (wp = hashtab[hash(s)]; wp != NULL; wp = wp->next)
		if (*s == *wp->string && strcmp(s, wp->string) == 0)
			return wp;
	return NULL;
}

install(wp)
	register struct wlist *wp;
{
	int hashval;

	if (lookup(wp->string) == NULL) {
		hashval = hash(wp->string);
		wp->next = hashtab[hashval];
		hashtab[hashval] = wp;
	} else
		printf("Multiply defined %s.\n", wp->string);
}

parse()
{
	register struct wlist *wp;
	register n;

	wordnumber = 0;           /* for cypher */
	for (n = 0; n <= wordcount; n++) {
		if ((wp = lookup(words[n])) == NULL) {
			wordvalue[n] = -1;
			wordtype[n] = -1;
		} else {
			wordvalue[n] = wp -> value;
			wordtype[n] = wp -> article;
		}
	}
}
