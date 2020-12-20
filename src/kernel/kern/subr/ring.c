/*
 * Copyright (c) 1989, 1990, 1991, 1992 William F. Jolitz, TeleMuse
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
 *	This software is a component of "386BSD" developed by 
	William F. Jolitz, TeleMuse.
 * 4. Neither the name of the developer nor the name "386BSD"
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS A COMPONENT OF 386BSD DEVELOPED BY WILLIAM F. JOLITZ 
 * AND IS INTENDED FOR RESEARCH AND EDUCATIONAL PURPOSES ONLY. THIS 
 * SOFTWARE SHOULD NOT BE CONSIDERED TO BE A COMMERCIAL PRODUCT. 
 * THE DEVELOPER URGES THAT USERS WHO REQUIRE A COMMERCIAL PRODUCT 
 * NOT MAKE USE THIS WORK.
 *
 * FOR USERS WHO WISH TO UNDERSTAND THE 386BSD SYSTEM DEVELOPED
 * BY WILLIAM F. JOLITZ, WE RECOMMEND THE USER STUDY WRITTEN 
 * REFERENCES SUCH AS THE  "PORTING UNIX TO THE 386" SERIES 
 * (BEGINNING JANUARY 1991 "DR. DOBBS JOURNAL", USA AND BEGINNING 
 * JUNE 1991 "UNIX MAGAZIN", GERMANY) BY WILLIAM F. JOLITZ AND 
 * LYNNE GREER JOLITZ, AS WELL AS OTHER BOOKS ON UNIX AND THE 
 * ON-LINE 386BSD USER MANUAL BEFORE USE. A BOOK DISCUSSING THE INTERNALS 
 * OF 386BSD ENTITLED "386BSD FROM THE INSIDE OUT" WILL BE AVAILABLE LATE 1992.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPER ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPER BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: ring.c,v 1.1 94/10/19 18:33:26 bill Exp $
 */
#include "sys/param.h"
#include "sys/errno.h"
#include "ringbuf.h"
#include "prototypes.h"

/* add a character to a buffer */
int
putc(unsigned c, struct ringb *rbp)
{
	char *nxtp;

	/* ring buffer full? */
	if ((nxtp = RB_SUCC(rbp, rbp->rb_tl)) == rbp->rb_hd)
		return (-1);

	/* stuff character */
	*rbp->rb_tl = (char) c;
	rbp->rb_tl = nxtp;
	return(0);
}

/* pull a character off a buffer */
int
getc(struct ringb *rbp)
{
	u_char c;

	/* ring buffer empty? */
	if (rbp->rb_hd == rbp->rb_tl)
		return(-1);

	/* fetch character, locate next character */
	c = *(u_char *) rbp->rb_hd;
	rbp->rb_hd = RB_SUCC(rbp, rbp->rb_hd);
	return (c);
}

int
nextc(char **cpp, struct ringb *rbp) {
	char *cp;

	if (*cpp == rbp->rb_tl)
		return (0);
	else {
		cp = *cpp;
		*cpp = RB_SUCC(rbp, cp);
		return(*cp);
	}
}

int
ungetc(unsigned c, struct ringb *rbp)
{
	char	*backp;

	/* ring buffer full? */
	if ( (backp = RB_PRED(rbp, rbp->rb_hd)) == rbp->rb_tl) return (-1);
	rbp->rb_hd = backp;

	/* stuff character */
	*rbp->rb_hd = c;
	return(0);
}

int
unputc(struct ringb *rbp)
{
	char	*backp;
	u_char c;

	/* ring buffer empty? */
	if (rbp->rb_hd == rbp->rb_tl) return(-1);

	/* backup buffer and dig out previous character */
	backp = RB_PRED(rbp, rbp->rb_tl);
	c = *(u_char *)backp;
	rbp->rb_tl = backp;

	return(c);
}

#define	peekc(rbp)	(*(rbp)->rb_hd)

void
initrb(struct ringb *rbp) {
	rbp->rb_hd = rbp->rb_tl = rbp->rb_buf;
}

/*
 * Example code for contiguous operations:
	...
	nc = RB_CONTIGPUT(&rb);
	if (nc) {
	if (nc > 9) nc = 9;
		(void) memcpy(rb.rb_tl, "ABCDEFGHI", nc);
		rb.rb_tl += nc;
		rb.rb_tl = RB_ROLLOVER(&rb, rb.rb_tl);
	}
	...
	...
	nc = RB_CONTIGGET(&rb);
	if (nc) {
		if (nc > 79) nc = 79;
		(void) memcpy(stringbuf, rb.rb_hd, nc);
		rb.rb_hd += nc;
		rb.rb_hd = RB_ROLLOVER(&rb, rb.rb_hd);
		stringbuf[nc] = 0;
		printf("%s|", stringbuf);
	}
	...
 */

/*
 * Concatinate ring buffers.
 */
void
catb(struct ringb *from, struct ringb *to)
{
	int c;

	while ((c = getc(from)) >= 0)
		putc((unsigned)c, to);
}
