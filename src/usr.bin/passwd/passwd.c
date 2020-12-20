/*
 * Copyright (c) 1988 The Regents of the University of California.
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
"@(#) Copyright (c) 1988 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)passwd.c	5.5 (Berkeley) 7/6/91";
#endif /* not lint */

#include <stdio.h>
#include <unistd.h>

#ifdef KERBEROS
int use_kerberos = 1;
#endif

main(argc, argv)
	int argc;
	char **argv;
{
	extern int optind;
	register int ch;
	char *uname;

#ifdef KERBEROS
	while ((ch = getopt(argc, argv, "l")) != EOF)
		switch (ch) {
		case 'l':		/* change local password file */
			use_kerberos = 0;
			break;
#else
	while ((ch = getopt(argc, argv, "")) != EOF)
		switch (ch) {
#endif
		default:
		case '?':
			usage();
			exit(1);
		}

	argc -= optind;
	argv += optind;

	uname = getlogin();

	switch(argc) {
	case 0:
		break;
	case 1:
#ifdef	KERBEROS
		if (use_kerberos && strcmp(argv[1], uname)) {
			(void)fprintf(stderr, "passwd: %s\n\t%s\n%s\n",
"to change another user's Kerberos password, do",
"\"kinit user; passwd; kdestroy\";",
"to change a user's local passwd, use \"passwd -l user\"");
			exit(1);
		}
#endif
		uname = argv[0];
		break;
	default:
		usage();
		exit(1);
	}

#ifdef	KERBEROS
	if (use_kerberos)
		exit(krb_passwd());
#endif
	exit(local_passwd(uname));
}

usage()
{
#ifdef	KERBEROS
	(void)fprintf(stderr, "usage: passwd [-l] user\n");
#else
	(void)fprintf(stderr, "usage: passwd user\n");
#endif
}
