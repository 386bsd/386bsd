/*-
 * Copyright (c) 1982, 1986 The Regents of the University of California.
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
 *	@(#)dkbad.h	7.2 (Berkeley) 2/15/91
 */

/*
 * Definitions needed to perform bad sector revectoring ala DEC STD 144.
 *
 * The bad sector information is located in the first 5 even numbered
 * sectors of the last track of the disk pack.  There are five identical
 * copies of the information, described by the dkbad structure.
 *
 * Replacement sectors are allocated starting with the first sector before
 * the bad sector information and working backwards towards the beginning of
 * the disk.  A maximum of 126 bad sectors are supported.  The position of
 * the bad sector in the bad sector table determines which replacement sector
 * it corresponds to.
 *
 * The bad sector information and replacement sectors are conventionally
 * only accessible through the 'c' file system partition of the disk.  If
 * that partition is used for a file system, the user is responsible for
 * making sure that it does not overlap the bad sector information or any
 * replacement sectors.
 */
struct dkbad {
	long bt_csn;			/* cartridge serial number */
	u_short bt_mbz;			/* unused; should be 0 */
	u_short bt_flag;		/* -1 => alignment cartridge */
	struct bt_bad {
		u_short bt_cyl;		/* cylinder number of bad sector */
		u_short bt_trksec;	/* track and sector number */
	} bt_bad[126];
};

#define	ECC	0
#define	SSE	1
#define	BSE	2
#define	CONT	3
