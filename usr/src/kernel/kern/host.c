/*
 * Copyright (c) 1982, 1986, 1989 Regents of the University of California.
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
 * $Id: host.c,v 1.1 94/10/20 00:02:55 bill Exp $
 */

#include "sys/param.h"
#include "privilege.h"
#include "sys/errno.h"
#include "kernel.h"	/* hostname .... */
#include "proc.h"
#include "prototypes.h"

/* BSD get host id */
gethostid(p, uap, retval)
	struct proc *p;
	void *uap;
	long *retval;
{

	*retval = hostid;
	return (0);
}

/* BSD set host id */
sethostid(p, uap, retval)
	struct proc *p;
	struct args {
		long	hostid;
	} *uap;
	int *retval;
{
	int error;

	if (error = use_priv(p->p_ucred, PRV_SETHOSTID, p))
		return (error);
	hostid = uap->hostid;
	return (0);
}

/* BSD get host name */
gethostname(p, uap, retval)
	struct proc *p;
	struct args {
		char	*hostname;
		u_int	len;
	} *uap;
	int *retval;
{

	if (uap->len > hostnamelen + 1)
		uap->len = hostnamelen + 1;

	return (copyout(p, (caddr_t)hostname, (caddr_t)uap->hostname, uap->len));
}

/* BSD set host name */
sethostname(p, uap, retval)
	struct proc *p;
	register struct args {
		char	*hostname;
		u_int	len;
	} *uap;
	int *retval;
{
	int error;

	if (error = use_priv(p->p_ucred, PRV_SETHOSTNAME, p))
		return (error);

	if (uap->len > sizeof (hostname) - 1)
		return (EINVAL);

	hostnamelen = uap->len;
	error = copyin(p, (caddr_t)uap->hostname, hostname, uap->len);
	hostname[hostnamelen] = 0;

	return (error);
}
