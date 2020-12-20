/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Robert Elz at The University of Melbourne.
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
 *	@(#)quota.h	7.9 (Berkeley) 2/22/91
 */

#ifndef _QUOTA_
#define _QUOTA_

/*
 * Definitions for disk quotas imposed on the average user
 * (big brother finally hits UNIX).
 *
 * The following constants define the amount of time given a user
 * before the soft limits are treated as hard limits (usually resulting
 * in an allocation failure). The timer is started when the user crosses
 * their soft limit, it is reset when they go below their soft limit.
 */
#define	MAX_IQ_TIME	(7*24*60*60)	/* 1 week */
#define	MAX_DQ_TIME	(7*24*60*60)	/* 1 week */

/*
 * The following constants define the usage of the quota file array
 * in the ufsmount structure and dquot array in the inode structure.
 * The semantics of the elements of these arrays are defined in the
 * routine getinoquota; the remainder of the quota code treats them
 * generically and need not be inspected when changing the size of
 * the array.
 */
#define	MAXQUOTAS	2
#define	USRQUOTA	0	/* element used for user quotas */
#define	GRPQUOTA	1	/* element used for group quotas */

/*
 * Definitions for the default names of the quotas files.
 */
#define INITQFNAMES { \
	"user",		/* USRQUOTA */ \
	"group",	/* GRPQUOTA */ \
	"undefined", \
};
#define QUOTAFILENAME "quota"
#define QUOTAGROUP "operator"

/*
 * Command definitions for the 'quotactl' system call.
 * The commands are broken into a main command defined below
 * and a subcommand that is used to convey the type of
 * quota that is being manipulated (see above).
 */
#define SUBCMDMASK	0x00ff
#define SUBCMDSHIFT	8
#define	QCMD(cmd, type)	(((cmd) << SUBCMDSHIFT) | ((type) & SUBCMDMASK))

#define	Q_QUOTAON	0x0100	/* enable quotas */
#define	Q_QUOTAOFF	0x0200	/* disable quotas */
#define	Q_GETQUOTA	0x0300	/* get limits and usage */
#define	Q_SETQUOTA	0x0400	/* set limits and usage */
#define	Q_SETUSE	0x0500	/* set usage */
#define	Q_SYNC		0x0600	/* sync disk copy of a filesystems quotas */

/*
 * The following structure defines the format of the disk quota file
 * (as it appears on disk) - the file is an array of these structures
 * indexed by user or group number.  The setquota system call establishes
 * the vnode for each quota file (a pointer is retained in the ufsmount
 * structure).
 */
struct	dqblk {
	u_long	dqb_bhardlimit;	/* absolute limit on disk blks alloc */
	u_long	dqb_bsoftlimit;	/* preferred limit on disk blks */
	u_long	dqb_curblocks;	/* current block count */
	u_long	dqb_ihardlimit;	/* maximum # allocated inodes + 1 */
	u_long	dqb_isoftlimit;	/* preferred inode limit */
	u_long	dqb_curinodes;	/* current # allocated inodes */
	time_t	dqb_btime;	/* time limit for excessive disk use */
	time_t	dqb_itime;	/* time limit for excessive files */
};

#ifdef KERNEL
/*
 * The following structure records disk usage for a user or group on a
 * filesystem. There is one allocated for each quota that exists on any
 * filesystem for the current user or group. A cache is kept of recently
 * used entries.
 */
struct	dquot {
	struct	dquot *dq_forw, *dq_back;/* MUST be first entry */
	struct	dquot *dq_freef, **dq_freeb; /* free list */
	short	dq_flags;		/* flags, see below */
	short	dq_cnt;			/* count of active references */
	short	dq_spare;		/* unused spare padding */
	short	dq_type;		/* quota type of this dquot */
	u_long	dq_id;			/* identifier this applies to */
	struct	ufsmount *dq_ump;	/* filesystem that this is taken from */
	struct	dqblk dq_dqb;		/* actual usage & quotas */
};
/*
 * Flag values.
 */
#define	DQ_LOCK		0x01		/* this quota locked (no MODS) */
#define	DQ_WANT		0x02		/* wakeup on unlock */
#define	DQ_MOD		0x04		/* this quota modified since read */
#define	DQ_FAKE		0x08		/* no limits here, just usage */
#define	DQ_BLKS		0x10		/* has been warned about blk limit */
#define	DQ_INODS	0x20		/* has been warned about inode limit */
/*
 * Shorthand notation.
 */
#define	dq_bhardlimit	dq_dqb.dqb_bhardlimit
#define	dq_bsoftlimit	dq_dqb.dqb_bsoftlimit
#define	dq_curblocks	dq_dqb.dqb_curblocks
#define	dq_ihardlimit	dq_dqb.dqb_ihardlimit
#define	dq_isoftlimit	dq_dqb.dqb_isoftlimit
#define	dq_curinodes	dq_dqb.dqb_curinodes
#define	dq_btime	dq_dqb.dqb_btime
#define	dq_itime	dq_dqb.dqb_itime

/*
 * If the system has never checked for a quota for this file,
 * then it is set to NODQUOT. Once a write attempt is made
 * the inode pointer is set to reference a dquot structure.
 */
#define	NODQUOT		((struct dquot *) 0)

/*
 * Flags to chkdq() and chkiq()
 */
#define	FORCE	0x01	/* force usage changes independent of limits */
#define	CHOWN	0x02	/* (advisory) change initiated by chown */

/*
 * Macros to avoid subroutine calls to trivial functions.
 */
#ifndef DIAGNOSTIC
#define	DQREF(dq)	(dq)->dq_cnt++
#else
#define	DQREF(dq)	dqref(dq)
#endif /* DIAGNOSTIC */

#else

#include <sys/cdefs.h>

__BEGIN_DECLS
int	quotactl __P((const char *, int, int, void *));
__END_DECLS

#endif /* KERNEL */
#endif /* _QUOTA_ */
