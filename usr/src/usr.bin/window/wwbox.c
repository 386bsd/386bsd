/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Edward Wang at The University of California, Berkeley.
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
static char sccsid[] = "@(#)wwbox.c	3.8 (Berkeley) 6/6/90";
#endif /* not lint */

#include "ww.h"
#include "tt.h"

wwbox(w, r, c, nr, nc)
register struct ww *w;
register r, c;
int nr, nc;
{
	register r1, c1;
	register i;

	r1 = r + nr - 1;
	c1 = c + nc - 1;
	wwframec(w, r, c, WWF_D|WWF_R);
	for (i = c + 1; i < c1; i++)
		wwframec(w, r, i, WWF_L|WWF_R);
	wwframec(w, r, i, WWF_L|WWF_D);
	for (i = r + 1; i < r1; i++)
		wwframec(w, i, c1, WWF_U|WWF_D);
	wwframec(w, i, c1, WWF_U|WWF_L);
	for (i = c1 - 1; i > c; i--)
		wwframec(w, r1, i, WWF_R|WWF_L);
	wwframec(w, r1, i, WWF_R|WWF_U);
	for (i = r1 - 1; i > r; i--)
		wwframec(w, i, c, WWF_D|WWF_U);
}
