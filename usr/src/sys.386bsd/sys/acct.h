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
 *
 *	@(#)acct.h	7.3 (Berkeley) 2/15/91
 */

/*
 * Accounting structures; these use a comp_t type which is a 3 bits base 8
 * exponent, 13 bit fraction ``floating point'' number.  Units are 1/AHZ
 * seconds.
 */
typedef u_short comp_t;

struct acct {
	char	ac_comm[10];	/* command name */
	comp_t	ac_utime;	/* user time */
	comp_t	ac_stime;	/* system time */
	comp_t	ac_etime;	/* elapsed time */
	time_t	ac_btime;	/* starting time */
	uid_t	ac_uid;		/* user id */
	gid_t	ac_gid;		/* group id */
	short	ac_mem;		/* average memory usage */
	comp_t	ac_io;		/* count of IO blocks */
	dev_t	ac_tty;		/* controlling tty */
#define	AFORK	0x01			/* forked but not execed */
#define	ASU	0x02			/* used super-user permissions */
#define	ACOMPAT	0x04			/* used compatibility mode */
#define	ACORE	0x08			/* dumped core */
#define	AXSIG	0x10			/* killed by a signal */
	char	ac_flag;	/* accounting flags */
};

/*
 * 1/AHZ is the granularity of the data encoded in the comp_t fields.
 * This is not necessarily equal to hz.
 */
#define	AHZ	64

#ifdef KERNEL
struct vnode	*acctp;
#endif
