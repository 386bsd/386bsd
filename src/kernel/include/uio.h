
/*
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
 *	@(#)uio.h	7.8 (Berkeley) 4/15/91
 */

#ifndef _UIO_H_
#define	_UIO_H_

struct iovec {
	caddr_t	iov_base;
	int	iov_len;
};

enum	uio_rw { UIO_READ, UIO_WRITE };

/*
 * Segment flag values.
 */
enum	uio_seg {
	UIO_USERSPACE,		/* from user data space */
	UIO_SYSSPACE,		/* from system space */
	UIO_USERISPACE		/* from user I space */
};

struct uio {
	struct	iovec *uio_iov;
	int	uio_iovcnt;
	off_t	uio_offset;
	int	uio_resid;
	enum	uio_seg uio_segflg;
	enum	uio_rw uio_rw;
	struct	proc *uio_procp;
};

 /*
  * Limits
  */
#define UIO_MAXIOV	1024		/* max 1K of iov's */
#define UIO_SMALLIOV	8		/* 8 on stack, else malloc */

#ifndef	KERNEL

#include <sys/cdefs.h>

__BEGIN_DECLS
int	readv __P((int, const struct iovec *, int));
int	writev __P((int, const struct iovec *, int));
__END_DECLS
#else
struct iovec *uio_advance(struct uio *uio, struct iovec *iov, int cnt);

/* interface symbols */
#define	__ISYM_VERSION__ "1"	/* XXX RCS major revision number of hdr file */
#include "isym.h"		/* this header has interface symbols */

/* functions used in modules */
__ISYM__(int, uiomove, (caddr_t c, int len, struct uio *uio))

#undef __ISYM__
#undef __ISYM_ALIAS__
#undef __ISYM_VERSION__

/*
 * Advance a uio and its iov by cnt bytes, returning next iov.
 */
extern inline struct iovec *
uio_advance(struct uio *uio, struct iovec *iov, int cnt) {

	/* consume an iov, possibly fetching the next one */
	iov->iov_base += cnt;
	iov->iov_len -= cnt;
	if (iov->iov_len == 0) {
		iov = ++uio->uio_iov;
		uio->uio_iovcnt--;
	}

	uio->uio_resid -= cnt;
	uio->uio_offset += cnt;

	return (iov);
}

#define	uio_base(uio)	(uio)->uio_iov->iov_base
#define	uio_len(uio)	(uio)->uio_iov->iov_len
#endif	/* !KERNEL */

#endif /* !_UIO_H_ */
