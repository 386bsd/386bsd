/*
 * Copyright (c) 1982, 1986, 1990 Regents of the University of California.
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
 * Socket file descriptor operations
 *
 * $Id: sock_fops.c,v 1.1 94/10/19 23:49:52 bill Exp Locker: root $
 */

#include "sys/param.h"
#include "sys/ioctl.h"
#include "sys/stat.h"
#include "sys/file.h"
#include "sys/errno.h"
#include "mbuf.h"
#include "socketvar.h"
#include "protosw.h"
#include "prototypes.h"

#include "if.h"
#include "route.h"


static int
	soo_read(struct file *fp, struct uio *uio, struct ucred *cred),
	soo_write(struct file *fp, struct uio *uio, struct ucred *cred),
	soo_ioctl(struct file *fp, int com, caddr_t data, struct proc *p),
	soo_select(struct file *fp, int which, struct proc *p),
	soo_close(struct file *fp, struct proc *p),
	soo_stat(struct file *fp, struct stat *ub, struct proc *);

struct	fileops socketops =
    { soo_read, soo_write, soo_ioctl, soo_select, soo_close, soo_stat };

/* perform a read() on a socket file descriptor */
static int
soo_read(struct file *fp, struct uio *uio, struct ucred *cred)
{

	return (soreceive((struct socket *)fp->f_data, (struct mbuf **)0,
		uio, (struct mbuf **)0, (struct mbuf **)0, (int *)0));
}

/* perform a write() on a socket file descriptor */
static int
soo_write(struct file *fp, struct uio *uio, struct ucred *cred)
{

	return (sosend((struct socket *)fp->f_data, (struct mbuf *)0,
		uio, (struct mbuf *)0, (struct mbuf *)0, 0));
}

/* perform a ioctl() on a socket file descriptor */
static int
soo_ioctl(struct file *fp, int cmd, caddr_t data, struct proc *p)
{
	struct socket *so = (struct socket *)fp->f_data;

	switch (cmd) {

		/* set/clear nonblocking i/o flag */
	case FIONBIO:
		if (*(int *)data)
			so->so_state |= SS_NBIO;
		else
			so->so_state &= ~SS_NBIO;
		return (0);

		/* set/clear asynchronous i/o flag */
	case FIOASYNC:
		if (*(int *)data) {
			so->so_state |= SS_ASYNC;
			so->so_rcv.sb_flags |= SB_ASYNC;
			so->so_snd.sb_flags |= SB_ASYNC;
		} else {
			so->so_state &= ~SS_ASYNC;
			so->so_rcv.sb_flags &= ~SB_ASYNC;
			so->so_snd.sb_flags &= ~SB_ASYNC;
		}
		return (0);

		/* get outstanding received characters */
	case FIONREAD:
		*(int *)data = so->so_rcv.sb_cc;
		return (0);

		/* set/clear nonblocking i/o flag */
	case SIOCSPGRP:
		so->so_pgid = *(int *)data;
		return (0);

		/* set/clear nonblocking i/o flag */
	case SIOCGPGRP:
		*(int *)data = so->so_pgid;
		return (0);

		/* set/clear nonblocking i/o flag */
	case SIOCATMARK:
		*(int *)data = (so->so_state&SS_RCVATMARK) != 0;
		return (0);
	}

	/*
	 * Interface/routing/protocol specific ioctls:
	 * interface and routing ioctls should have a
	 * different entry since a socket's unnecessary
	 */
	if (IOCGROUP(cmd) == 'i')
		return (ifioctl(so, cmd, data, p));
#ifdef COMPAT_43
	if (IOCGROUP(cmd) == 'r') {
		return rtioctl(cmd, data, p);
/*		if (_router_.rt_ioctl)
			return RTIOCTL(cmd, data, p);
		else
			return(EOPNOTSUPP);*/
	}
#endif

	return ((*so->so_proto->pr_usrreq)(so, PRU_CONTROL, 
	    (struct mbuf *)cmd, (struct mbuf *)data, (struct mbuf *)0));
}

/* examine socket of the file descriptor to see if it is select()ed */
static int
soo_select(struct file *fp, int which, struct proc *p)
{
	struct socket *so = (struct socket *)fp->f_data;
	int s = splnet();

	switch (which) {

		/* is the socket ready for a read()? */
	case FREAD:
		if (soreadable(so)) {
			splx(s);
			return(1);
		}
		sbselqueue(&so->so_rcv, p);
		break;

		/* is the socket ready for a write()? */
	case FWRITE:
		if (sowriteable(so)) {
			splx(s);
			return(1);
		}
		sbselqueue(&so->so_snd, p);
		break;

		/* does the socket have an outstanding exception? */
	case 0:
		if (so->so_oobmark || (so->so_state & SS_RCVATMARK)) {
			splx(s);
			return (1);
		}
		sbselqueue(&so->so_rcv, p);
		break;
	}
	splx(s);
	return (0);
}

/* return the results for a stat() on a socket file descriptor */
static int
soo_stat(struct file *fp, struct stat *ub, struct proc *p)
{
	struct socket *so = (struct socket *)fp->f_data;

	/* clear the status buffer first */
	(void) memset((caddr_t)ub, 0, sizeof (*ub));

	/* ask socket's protocol for contents of status buffer to return */
	return ((*so->so_proto->pr_usrreq)(so, PRU_SENSE,
	    (struct mbuf *)ub, (struct mbuf *)0,  (struct mbuf *)0));
}

/* perform a close() on a socket file descriptor */
static int
soo_close(struct file *fp, struct proc *p)
{
	int error = 0;

	/* if socket is not closed, close it with a socket op */
	if (fp->f_data)
		error = soclose((struct socket *)fp->f_data);

	fp->f_data = 0;
	return (error);
}
