/*
 * Copyright (c) 1994 William F. Jolitz, TeleMuse
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
 *	William F. Jolitz, TeleMuse.
 * 4. Neither the name of the developer nor the name "386BSD"
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 5. Non-commercial distribution of this complete file in either source and/or
 *    binary form at no charge to the user (such as from an official Internet 
 *    archive site) is permitted. 
 * 6. Commercial distribution and sale of this complete file in either source
 *    and/or binary form on any media, including that of floppies, tape, or 
 *    CD-ROM, or through a per-charge download such as that of a BBS, is not 
 *    permitted without specific prior written permission.
 * 7. Non-commercial and/or commercial distribution of an incomplete, altered, 
 *    or otherwise modified file in either source and/or binary form is not 
 *    permitted.
 *
 * THIS SOFTWARE IS A COMPONENT OF 386BSD DEVELOPED BY WILLIAM F. JOLITZ 
 * AND IS INTENDED FOR RESEARCH AND EDUCATIONAL PURPOSES ONLY. THIS 
 * SOFTWARE SHOULD NOT BE CONSIDERED TO BE A COMMERCIAL PRODUCT. 
 * THE DEVELOPER URGES THAT USERS WHO REQUIRE A COMMERCIAL PRODUCT 
 * NOT MAKE USE OF THIS WORK.
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
 * $Id: uio.c,v 1.1 94/10/19 18:33:49 bill Exp $
 *
 * Primatives for implementing I/O operations on user process segments.
 */

#include "sys/param.h"
#include "sys/errno.h"
#include "uio.h"
#include "proc.h"
#include "prototypes.h"

/*
 * Apply a function to successive portions of the memory described by
 * a uio.
 */
int
uioapply(int (*func)(), int arg1, struct uio *uio)
{
	register struct iovec *iov;
	u_int cnt, cnt1;
	int error = 0;

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_READ && uio->uio_rw != UIO_WRITE)
		panic("uioapply: mode");
	if (uio->uio_segflg == UIO_USERSPACE && uio->uio_procp != curproc)
		panic("uioapply: proc");
#endif
	for (iov = uio->uio_iov ; uio->uio_resid > 0 ; ) {
		cnt = iov->iov_len;
		cnt1 = cnt;
		error = (*func)(arg1, uio->uio_offset, uio->uio_rw,
			iov->iov_base, &cnt1, uio->uio_procp);
		cnt -= cnt1;
		iov = uio_advance(uio, iov, cnt);
		if (error || cnt1)
			return (error);
	}

	return (0);
}

/* pass len bytes to a uio via a function*/
int
uiotofunc (int (*func)(...), char *cp, int len, struct uio *uio, int order)
{
	struct iovec *iov;
	int cnt, rv;

#ifdef DIAGNOSTICx
	if (uio->uio_rw != UIO_READ)
		panic("uiotofunc: mode");
	if (uio->uio_segflg == UIO_USERSPACE && uio->uio_procp != curproc)
		panic("uiotofunc: proc");
#endif
	for (iov = uio->uio_iov ; len > 0 && uio->uio_resid > 0; len -= cnt)
	{
		cnt = imin (iov->iov_len, len);

		if (order & 1)
			rv = (*func)(iov->iov_base, cp, cnt);
		else
			rv = (*func)(cp, iov->iov_base, cnt);

		if ((order & 2) != 0 && rv)
			return(rv);

		cp += cnt;
		iov = uio_advance(uio, iov, cnt);
	}

	return (rv);
}

/* pass len bytes to a uio */
int
uiotofunc1 (int (*func)(...), int arg, char *cp, int len, struct uio *uio,
	 int order)
{
	struct iovec *iov;
	int cnt, rv;

#ifdef DIAGNOSTICx
	if (uio->uio_rw != UIO_READ)
		panic("uiotofunc: mode");
	if (uio->uio_segflg == UIO_USERSPACE && uio->uio_procp != curproc)
		panic("uiotofunc: proc");
#endif
	for (iov = uio->uio_iov ; len > 0 && uio->uio_resid > 0; len -= cnt)
	{
		cnt = imin (iov->iov_len, len);

		if (order & 1)
			rv = (*func)(arg, iov->iov_base, cp, cnt);
		else
			rv = (*func)(arg, cp, iov->iov_base, cnt);

		if ((order & 2) != 0 && rv)
			return(rv);

		cp += cnt;
		iov = uio_advance(uio, iov, cnt);
	}

	return (0);
}

/*
 * Pass information to/from user process mediated by an uio descriptor.
 */
int
uiomove(caddr_t cp, int len, struct uio *uio)
{ 
	int rv = 0;

	/* user space */
	if (uio->uio_segflg == UIO_USERSPACE) {
		if (uio->uio_rw == UIO_READ)
			rv = uiotofunc1((int(*)())copyout, (int)uio->uio_procp,
			    cp, len, uio, 0);
		else
			rv = uiotofunc1((int(*)())copyin, (int)uio->uio_procp,
			    cp, len, uio, 1);
	}

	/* kernel */
	if (uio->uio_segflg == UIO_SYSSPACE)
		(void)uiotofunc((int(*)())memcpy, cp, len, uio,
		    (uio->uio_rw == UIO_READ));

	/* (add new segment types here) */

	return (rv);
}
