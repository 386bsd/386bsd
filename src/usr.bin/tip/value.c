/*
 * Copyright (c) 1983 The Regents of the University of California.
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
static char sccsid[] = "@(#)value.c	5.5 (Berkeley) 3/2/91";
#endif /* not lint */

#include "tip.h"

#define MIDDLE	35

static value_t *vlookup();
static int col = 0;

/*
 * Variable manipulation
 */
vinit()
{
	register value_t *p;
	register char *cp;
	FILE *f;
	char file[256];

	for (p = vtable; p->v_name != NULL; p++) {
		if (p->v_type&ENVIRON)
			if (cp = getenv(p->v_name))
				p->v_value = cp;
		if (p->v_type&IREMOTE)
			number(p->v_value) = *address(p->v_value);
	}
	/*
	 * Read the .tiprc file in the HOME directory
	 *  for sets
	 */
	strcpy(file, value(HOME));
	strcat(file, "/.tiprc");
	if ((f = fopen(file, "r")) != NULL) {
		register char *tp;

		while (fgets(file, sizeof(file)-1, f) != NULL) {
			if (vflag)
				printf("set %s", file);
			if (tp = rindex(file, '\n'))
				*tp = '\0';
			vlex(file);
		}
		fclose(f);
	}
	/*
	 * To allow definition of exception prior to fork
	 */
	vtable[EXCEPTIONS].v_access &= ~(WRITE<<PUBLIC);
}

static int vaccess();

/*VARARGS1*/
vassign(p, v)
	register value_t *p;
	char *v;
{

	if (!vaccess(p->v_access, WRITE)) {
		printf("access denied\r\n");
		return;
	}
	switch (p->v_type&TMASK) {

	case STRING:
		if (p->v_value && equal(p->v_value, v))
			return;
		if (!(p->v_type&(ENVIRON|INIT)))
			free(p->v_value);
		if ((p->v_value = malloc(size(v)+1)) == NOSTR) {
			printf("out of core\r\n");
			return;
		}
		p->v_type &= ~(ENVIRON|INIT);
		strcpy(p->v_value, v);
		break;

	case NUMBER:
		if (number(p->v_value) == number(v))
			return;
		number(p->v_value) = number(v);
		break;

	case BOOL:
		if (boolean(p->v_value) == (*v != '!'))
			return;
		boolean(p->v_value) = (*v != '!');
		break;

	case CHAR:
		if (character(p->v_value) == *v)
			return;
		character(p->v_value) = *v;
	}
	p->v_access |= CHANGED;
}

static void vprint();

vlex(s)
	register char *s;
{
	register value_t *p;
	static void vtoken();

	if (equal(s, "all")) {
		for (p = vtable; p->v_name; p++)
			if (vaccess(p->v_access, READ))
				vprint(p);
	} else {
		register char *cp;

		do {
			if (cp = vinterp(s, ' '))
				cp++;
			vtoken(s);
			s = cp;
		} while (s);
	}
	if (col > 0) {
		printf("\r\n");
		col = 0;
	}
}

static void
vtoken(s)
	register char *s;
{
	register value_t *p;
	register char *cp;
	char *expand();

	if (cp = index(s, '=')) {
		*cp = '\0';
		if (p = vlookup(s)) {
			cp++;
			if (p->v_type&NUMBER)
				vassign(p, atoi(cp));
			else {
				if (strcmp(s, "record") == 0)
					cp = expand(cp);
				vassign(p, cp);
			}
			return;
		}
	} else if (cp = index(s, '?')) {
		*cp = '\0';
		if ((p = vlookup(s)) && vaccess(p->v_access, READ)) {
			vprint(p);
			return;
		}
	} else {
		if (*s != '!')
			p = vlookup(s);
		else
			p = vlookup(s+1);
		if (p != NOVAL) {
			vassign(p, s);
			return;
		}
	}
	printf("%s: unknown variable\r\n", s);
}

static void
vprint(p)
	register value_t *p;
{
	register char *cp;
	extern char *interp(), *ctrl();

	if (col > 0 && col < MIDDLE)
		while (col++ < MIDDLE)
			putchar(' ');
	col += size(p->v_name);
	switch (p->v_type&TMASK) {

	case BOOL:
		if (boolean(p->v_value) == FALSE) {
			col++;
			putchar('!');
		}
		printf("%s", p->v_name);
		break;

	case STRING:
		printf("%s=", p->v_name);
		col++;
		if (p->v_value) {
			cp = interp(p->v_value, NULL);
			col += size(cp);
			printf("%s", cp);
		}
		break;

	case NUMBER:
		col += 6;
		printf("%s=%-5d", p->v_name, number(p->v_value));
		break;

	case CHAR:
		printf("%s=", p->v_name);
		col++;
		if (p->v_value) {
			cp = ctrl(character(p->v_value));
			col += size(cp);
			printf("%s", cp);
		}
		break;
	}
	if (col >= MIDDLE) {
		col = 0;
		printf("\r\n");
		return;
	}
}


static int
vaccess(mode, rw)
	register unsigned mode, rw;
{
	if (mode & (rw<<PUBLIC))
		return (1);
	if (mode & (rw<<PRIVATE))
		return (1);
	return ((mode & (rw<<ROOT)) && getuid() == 0);
}

static value_t *
vlookup(s)
	register char *s;
{
	register value_t *p;

	for (p = vtable; p->v_name; p++)
		if (equal(p->v_name, s) || (p->v_abrev && equal(p->v_abrev, s)))
			return (p);
	return (NULL);
}

char *
vinterp(s, stop)
	register char *s;
	char stop;
{
	register char *p = s, c;
	int num;

	while ((c = *s++) && c != stop)
		switch (c) {

		case '^':
			if (*s)
				*p++ = *s++ - 0100;
			else
				*p++ = c;
			break;

		case '\\':
			num = 0;
			c = *s++;
			if (c >= '0' && c <= '7')
				num = (num<<3)+(c-'0');
			else {
				register char *q = "n\nr\rt\tb\bf\f";

				for (; *q; q++)
					if (c == *q++) {
						*p++ = *q;
						goto cont;
					}
				*p++ = c;
			cont:
				break;
			}
			if ((c = *s++) >= '0' && c <= '7') {
				num = (num<<3)+(c-'0');
				if ((c = *s++) >= '0' && c <= '7')
					num = (num<<3)+(c-'0');
				else
					s--;
			} else
				s--;
			*p++ = num;
			break;

		default:
			*p++ = c;
		}
	*p = '\0';
	return (c == stop ? s-1 : NULL);
}

/*
 * assign variable s with value v (for NUMBER or STRING or CHAR types)
 */

vstring(s,v)
	register char *s;
	register char *v;
{
	register value_t *p;
	char *expand();

	p = vlookup(s); 
	if (p == 0)
		return (1);
	if (p->v_type&NUMBER)
		vassign(p, atoi(v));
	else {
		if (strcmp(s, "record") == 0)
			v = expand(v);
		vassign(p, v);
	}
	return (0);
}
