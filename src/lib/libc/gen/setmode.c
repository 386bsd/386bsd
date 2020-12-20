/*
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

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)setmode.c	5.6 (Berkeley) 5/27/91";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/errno.h>
#ifdef SETMODE_DEBUG
#include <stdio.h>
#endif
#include <stdlib.h>
#include <ctype.h>

#define	SET_LEN	6		/* initial # of bitcmd struct to malloc */
#define	SET_LEN_INCR 4		/* # of bitcmd structs to add as needed */

struct bitcmd {
	char	cmd;
	char	cmd2;
	mode_t	bits;
};

#define	CMD2_CLR	0x01
#define	CMD2_SET	0x02
#define	CMD2_GBITS	0x04
#define	CMD2_OBITS	0x08
#define	CMD2_UBITS	0x10

/*
 * Given the old mode and an array of bitcmd structures, apply the operations
 * described in the bitcmd structures to the old mode, and return the new mode.
 * Note that there is no '=' command; a strict assignment is just a '-' (clear
 * bits) followed by a '+' (set bits).
 */
mode_t
getmode(bbox, omode)
	void *bbox;
	mode_t omode;
{
	register struct bitcmd *set;
	register mode_t newmode, value;

	set = (struct bitcmd *)bbox;
	newmode = omode;
	for (value = 0;; set++)
		switch(set->cmd) {
		/*
		 * When copying the user, group or other bits around, we "know"
		 * where the bit are in the mode so that we can do shifts to
		 * copy them around.  If we don't use shifts, it gets real
		 * grundgy with lots of single bit checks and bit sets.
		 */
		case 'u':
			value = (newmode & S_IRWXU) >> 6;
			goto common;

		case 'g':
			value = (newmode & S_IRWXG) >> 3;
			goto common;

		case 'o':
			value = newmode & S_IRWXO;
		common:
			if (set->cmd2 & CMD2_CLR) {
				if (set->cmd2 & CMD2_UBITS)
					newmode &= ~(S_IRWXU & set->bits);
				if (set->cmd2 & CMD2_GBITS)
					newmode &= ~(S_IRWXG & set->bits);
				if (set->cmd2 & CMD2_OBITS)
					newmode &= ~(S_IRWXO & set->bits);
			}
			if (set->cmd2 & CMD2_SET) {
				if (set->cmd2 & CMD2_UBITS)
					newmode |= (value<<6) & set->bits;
				if (set->cmd2 & CMD2_GBITS)
					newmode |= (value<<3) & set->bits;
				if (set->cmd2 & CMD2_OBITS)
					newmode |= value & set->bits;
			}
			break;

		case '+':
			newmode |= set->bits;
			break;

		case '-':
			newmode &= ~set->bits;
			break;

		case 'X':
			if (omode & (S_IFDIR|S_IXUSR|S_IXGRP|S_IXOTH))
				newmode |= set->bits;
			break;

		case '\0':
		default:
#ifdef SETMODE_DEBUG
			(void)printf("getmode(, %04o) -> %04o\n",
			    omode, newmode);
#endif
			return(newmode);
		}
}

#define	STANDARD_BITS	(S_ISUID|S_ISGID|S_IRWXU|S_IRWXG|S_IRWXO)

static struct bitcmd *
addcmd(set, op, who, oparg, mask)
	struct bitcmd *set;
	register int oparg, who;
	register int op;
	mode_t mask;
{
	switch (op) {
	case '+':
	case 'X':
		set->cmd = op;
		set->bits = (who ? who : mask) & oparg;
		break;

	case '-':
		set->cmd = '-';
		set->bits = (who ? who : (S_IRWXU|S_IRWXG|S_IRWXO)) & oparg;
		break;

	case '=':
		set->cmd = '-';
		if (!who) {
			set->bits = STANDARD_BITS;
			who = mask;
		} else
			set->bits = who;
		set++;

		set->cmd = '+';
		set->bits = who & oparg;
		break;
	case 'u':
	case 'g':
	case 'o':
		set->cmd = op;
		if (who) {
			set->cmd2 = ((who & S_IRUSR) ? CMD2_UBITS : 0) |
				    ((who & S_IRGRP) ? CMD2_GBITS : 0) |
				    ((who & S_IROTH) ? CMD2_OBITS : 0);
			set->bits = ~0;
		} else {
			set->cmd2 = CMD2_UBITS | CMD2_GBITS | CMD2_OBITS;
			set->bits = mask;
		}
	
		if (oparg == '+')
			set->cmd2 |= CMD2_SET;
		else if (oparg == '-')
			set->cmd2 |= CMD2_CLR;
		else if (oparg == '=')
			set->cmd2 |= CMD2_SET|CMD2_CLR;
		break;
	}
	return(set+1);
}

#define	ADDCMD(a, b, c, d) \
	if (set >= endset) { \
		register struct bitcmd *newset; \
		setlen += SET_LEN_INCR; \
		newset = realloc(saveset, sizeof(struct bitcmd) * setlen); \
		if (!saveset) \
			return(NULL); \
		set = newset + (set - saveset); \
		saveset = newset; \
		endset = newset + (setlen - 2); \
	} \
	set = addcmd(set, (a), (b), (c), (d))

void *
setmode(p)
	register char *p;
{
	register int perm, who;
	register char op;
	mode_t mask;
	struct bitcmd *set, *saveset, *endset;
	int permXbits, setlen;
	static int compress_mode();

	/*
	 * Get a copy of the mask for the permissions that are mask relative.
	 * Flip the bits, we want what's not set.
	 */
	(void)umask(mask = umask(0));
	mask = ~mask;

	setlen = SET_LEN + 2;
	
	set = (struct bitcmd *)malloc((u_int)(sizeof(struct bitcmd) * setlen));
	if (!set)
		return(NULL);
	saveset = set;
	endset = set + (setlen - 2);

	/*
	 * If an absolute number, get it and return; disallow non-octal digits
	 * or illegal bits.
	 */
	if (isdigit(*p)) {
		perm = (mode_t)strtol(p, (char **)0, 8);
		if (perm & ~(STANDARD_BITS|S_ISTXT)) {
			free(saveset);
			return(NULL);
		}
		while (*++p)
			if (*p < '0' || *p > '7') {
				free(saveset);
				return(NULL);
			}
		ADDCMD('=', (STANDARD_BITS|S_ISTXT), perm, mask);
		return((void *)saveset);
	}

	if (!*p) {
		free(saveset);
		return(NULL);
	}
	/*
	 * Build list of structures to set/clear/copy bits as described by
	 * each clause of the symbolic mode.
	 */
	for (;;) {
		/* First, find out which bits might be modified. */
		for (who = 0;; ++p) {
			switch (*p) {
			case 'a':
				who |= STANDARD_BITS;
				break;
			case 'u':
				who |= S_ISUID|S_IRWXU;
				break;
			case 'g':
				who |= S_ISGID|S_IRWXG;
				break;
			case 'o':
				who |= S_IRWXO;
				break;
			default:
				goto getop;
			}
		}
	getop:		

		if ((op = *p++) != '+' && op != '-' && op != '=') {
			free(saveset);
			return(NULL);
		}

		who &= ~S_ISTXT;
		for (perm = 0, permXbits = 0;; ++p) {
			switch (*p) {
			case 'r':
				perm |= S_IRUSR|S_IRGRP|S_IROTH;
				break;
			case 's':
				/* If only "other" bits ignore set-id. */
				if (who & ~S_IRWXO)
					perm |= S_ISUID|S_ISGID;
				break;
			case 't':
				/* If only "other" bits ignore sticky. */
				if (who & ~S_IRWXO) {
					who |= S_ISTXT;
					perm |= S_ISTXT;
				}
				break;
			case 'w':
				perm |= S_IWUSR|S_IWGRP|S_IWOTH;
				break;
			case 'X':
				permXbits = S_IXUSR|S_IXGRP|S_IXOTH;
				break;
			case 'x':
				perm |= S_IXUSR|S_IXGRP|S_IXOTH;
				break;
			case 'u':
			case 'g':
			case 'o':
				/*
				 * When ever we hit 'u', 'g', or 'o', we have
				 * to flush out any partial mode that we have,
				 * and then do the copying of the mode bits.
				 */
				if (perm) {
					ADDCMD(op, who, perm, mask);
					perm = 0;
				}
				if (op == '+' && permXbits) {
					ADDCMD('X', who, permXbits, mask);
					permXbits = 0;
				}
				ADDCMD(*p, who, op, mask);
				break;

			default:
				/*
				 * Add any permissions that we haven't already
				 * done.
				 */
				if (perm) {
					ADDCMD(op, who, perm, mask);
					perm = 0;
				}
				if (permXbits) {
					ADDCMD('X', who, permXbits, mask);
					permXbits = 0;
				}
				goto apply;
			}
		}

apply:		if (!*p)
			break;
		if (*p != ',')
			goto getop;
		++p;
	}
	set->cmd = 0;
#ifdef SETMODE_DEBUG
	(void)printf("Before compress_mode()\n");
	dumpmode(saveset);
#endif
	compress_mode(saveset);
#ifdef SETMODE_DEBUG
	(void)printf("After compress_mode()\n");
	dumpmode(saveset);
#endif
	return((void *)saveset);
}

#ifdef SETMODE_DEBUG
dumpmode(set)
	register struct bitcmd *set;
{
	for (; set->cmd; ++set)
		(void)printf("cmd: '%c' bits %04o%s%s%s%s%s%s\n",
		    set->cmd, set->bits, set->cmd2 ? " cmd2:" : "",
		    set->cmd2 & CMD2_CLR ? " CLR" : "",
		    set->cmd2 & CMD2_SET ? " SET" : "",
		    set->cmd2 & CMD2_UBITS ? " UBITS" : "",
		    set->cmd2 & CMD2_GBITS ? " GBITS" : "",
		    set->cmd2 & CMD2_OBITS ? " OBITS" : "");
}
#endif

/*
 * Given an array of bitcmd structures, compress by compacting consecutive
 * '+', '-' and 'X' commands into at most 3 commands, one of each.  The 'u',
 * 'g' and 'o' commands continue to be separate.  They could probably be 
 * compacted, but it's not worth the effort.
 */
static
compress_mode(set)
	register struct bitcmd *set;
{
	register struct bitcmd *nset;
	register int setbits, clrbits, Xbits, op;

	for (nset = set;;) {
		/* Copy over any 'u', 'g' and 'o' commands. */
		while ((op = nset->cmd) != '+' && op != '-' && op != 'X') {
			*set++ = *nset++;
			if (!op)
				return;
		}

		for (setbits = clrbits = Xbits = 0;; nset++) {
			if ((op = nset->cmd) == '-') {
				clrbits |= nset->bits;
				setbits &= ~nset->bits;
				Xbits &= ~nset->bits;
			} else if (op == '+') {
				setbits |= nset->bits;
				clrbits &= ~nset->bits;
				Xbits &= ~nset->bits;
			} else if (op == 'X')
				Xbits |= nset->bits & ~setbits;
			else
				break;
		}
		if (clrbits) {
			set->cmd = '-';
			set->cmd2 = 0;
			set->bits = clrbits;
			set++;
		}
		if (setbits) {
			set->cmd = '+';
			set->cmd2 = 0;
			set->bits = setbits;
			set++;
		}
		if (Xbits) {
			set->cmd = 'X';
			set->cmd2 = 0;
			set->bits = Xbits;
			set++;
		}
	}
}
