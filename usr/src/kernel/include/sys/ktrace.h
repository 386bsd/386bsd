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
 *
 *	@(#)ktrace.h	7.4 (Berkeley) 5/7/91
 */

/*
 * operations to ktrace system call  (KTROP(op))
 */
#define KTROP_SET		0	/* set trace points */
#define KTROP_CLEAR		1	/* clear trace points */
#define KTROP_CLEARFILE		2	/* stop all tracing to file */
#define	KTROP(o)		((o)&3)	/* macro to extract operation */
/*
 * flags (ORed in with operation)
 */
#define KTRFLAG_DESCEND		4	/* perform op on all children too */

/*
 * ktrace record header
 */
struct ktr_header {
	int	ktr_len;		/* length of buf */
	short	ktr_type;		/* trace record type */
	pid_t	ktr_pid;		/* process id */
	char	ktr_comm[MAXCOMLEN+1];	/* command name */
	struct	timeval ktr_time;	/* timestamp */
	caddr_t	ktr_buf;
};

/*
 * Test for kernel trace point
 */
#define KTRPOINT(p, type)	((p)->p_traceflag & (1<<(type)))

/*
 * ktrace record types
 */

/*
 * KTR_SYSCALL - system call record
 */
#define KTR_SYSCALL	1
struct ktr_syscall {
	short	ktr_code;		/* syscall number */
	short	ktr_narg;		/* number of arguments */
	/*
	 * followed by ktr_narg ints
	 */
};

/*
 * KTR_SYSRET - return from system call record
 */
#define KTR_SYSRET	2
struct ktr_sysret {
	short	ktr_code;
	short	ktr_eosys;
	int	ktr_error;
	int	ktr_retval;
};

/*
 * KTR_NAMEI - namei record
 */
#define KTR_NAMEI	3
	/* record contains pathname */

/*
 * KTR_GENIO - trace generic process i/o
 */
#define KTR_GENIO	4
struct ktr_genio {
	int	ktr_fd;
	enum	uio_rw ktr_rw;
	/*
	 * followed by data successfully read/written
	 */
};

/*
 * KTR_PSIG - trace processed signal
 */
#define	KTR_PSIG	5
struct ktr_psig {
	int	signo;
	sig_t	action;
	int	mask;
	int	code;
};

/*
 * KTR_VM - trace virtual memory operations
 */
#define	KTR_VM		6
struct ktr_vm {
	int	func;
#define KTVM_MMAP	0	/* vm_mmap() */
#define KTVM_ALLOC	1	/* vm_allocate() */
#define KTVM_REMOVE	2	/* vm_map_remove() */
#define KTVM_FAULT	3	/* vm_fault() */
#define KTVM_PAGER	4	/* vm_pager_xxx() */
#define KTVM_PAGE	5	/* vm_page_alloc/free() */
	int args[4];
};

/*
 * KTR_BIO - trace buffer operations
 */
#define	KTR_BIO		7
struct ktr_bio {
	int	func;
#define KTBIO_READ	0	/* bread() */
#define KTBIO_READA1	1	/* breada:1() */
#define KTBIO_READA2	2	/* breada:2() */
#define KTBIO_WRITE	3	/* bwrite() */
#define KTBIO_AWRITE	4	/* bawrite() */
#define KTBIO_WAIT	5	/* biowait() */
/* #define KTBIO_DONE	6	/* biodone() */
#define KTBIO_DTOA	7	/* bdwrite becoming bawrite() */
#define KTBIO_RECYCLE	8	/* free buffer recycled */
#define KTBIO_INVALID	9	/* buffer invalidated and freed */
#define KTBIO_RELEASE	10	/* buffer released */
#define KTBIO_REFERENCE	11	/* buffer referenced (cache hit) */
#define KTBIO_MODIFY	12	/* buffer modified (bdwrite) */
#define KTBIO_BUSY	13	/* buffer busy when requested */
	int args[2];
};

/*
 * kernel trace points (in p_traceflag)
 */
#define KTRFAC_MASK	0x00ffffff
#define KTRFAC_SYSCALL	(1<<KTR_SYSCALL)
#define KTRFAC_SYSRET	(1<<KTR_SYSRET)
#define KTRFAC_NAMEI	(1<<KTR_NAMEI)
#define KTRFAC_GENIO	(1<<KTR_GENIO)
#define	KTRFAC_PSIG	(1<<KTR_PSIG)
#define	KTRFAC_VM	(1<<KTR_VM)
#define	KTRFAC_BIO	(1<<KTR_BIO)

/*
 * trace flags (also in p_traceflags)
 */
#define KTRFAC_ROOT	0x80000000	/* root set this trace */
#define KTRFAC_INHERIT	0x40000000	/* pass trace flags to children */

#ifndef	KERNEL

#include <sys/cdefs.h>

__BEGIN_DECLS
int	ktrace __P((const char *, int, int, pid_t));
__END_DECLS

#endif	/* !KERNEL */
