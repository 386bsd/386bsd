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
static char sccsid[] = "@(#)hexsyntax.c	5.2 (Berkeley) 5/8/90";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>
#include "hexdump.h"

off_t skip;				/* bytes to skip */

newsyntax(argc, argvp)
	int argc;
	char ***argvp;
{
	extern enum _vflag vflag;
	extern FS *fshead;
	extern char *optarg;
	extern int length, optind;
	int ch;
	char *p, **argv;

	argv = *argvp;
	while ((ch = getopt(argc, argv, "bcde:f:n:os:vx")) != EOF)
		switch (ch) {
		case 'b':
			add("\"%07.7_Ax\n\"");
			add("\"%07.7_ax \" 16/1 \"%03o \" \"\\n\"");
			break;
		case 'c':
			add("\"%07.7_Ax\n\"");
			add("\"%07.7_ax \" 16/1 \"%3_c \" \"\\n\"");
			break;
		case 'd':
			add("\"%07.7_Ax\n\"");
			add("\"%07.7_ax \" 8/2 \"  %05u \" \"\\n\"");
			break;
		case 'e':
			add(optarg);
			break;
		case 'f':
			addfile(optarg);
			break;
		case 'n':
			if ((length = atoi(optarg)) < 0) {
				(void)fprintf(stderr,
				    "hexdump: bad length value.\n");
				exit(1);
			}
			break;
		case 'o':
			add("\"%07.7_Ax\n\"");
			add("\"%07.7_ax \" 8/2 \" %06o \" \"\\n\"");
			break;
		case 's':
			if ((skip = strtol(optarg, &p, 0)) < 0) {
				(void)fprintf(stderr,
				    "hexdump: bad skip value.\n");
				exit(1);
			}
			switch(*p) {
			case 'b':
				skip *= 512;
				break;
			case 'k':
				skip *= 1024;
				break;
			case 'm':
				skip *= 1048576;
				break;
			}
			break;
		case 'v':
			vflag = ALL;
			break;
		case 'x':
			add("\"%07.7_Ax\n\"");
			add("\"%07.7_ax \" 8/2 \"   %04x \" \"\\n\"");
			break;
		case '?':
			usage();
			exit(1);
		}

	if (!fshead) {
		add("\"%07.7_Ax\n\"");
		add("\"%07.7_ax \" 8/2 \"%04x \" \"\\n\"");
	}

	*argvp += optind;
}

usage()
{
	(void)fprintf(stderr,
"hexdump: [-bcdovx] [-e fmt] [-f fmt_file] [-n length] [-s skip] [file ...]\n");
	exit(1);
}
