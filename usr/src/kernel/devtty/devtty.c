/*-
 * Copyright (c) 1982, 1986, 1991 The Regents of the University of California.
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
 *	$Id: tty_tty.c,v 1.3 94/07/07 21:13:33 bill Exp Locker: bill $
 */

/*
 * Indirect driver for controlling tty.
 */
static char *ctty_config =
	"devtty 1.	 # tty indirect device (/dev/tty) $Revision: 1.3 $";

#include "sys/param.h"
#include "sys/param.h"
#include "sys/file.h"
#include "sys/ioctl.h"
#include "sys/errno.h"

#include "systm.h"
#include "tty.h"
#include "uio.h"
#include "proc.h"
#include "modconfig.h"

#include "vnode.h"

#include "prototypes.h"

#define cttyvp(p) ((p)->p_flag&SCTTY ? (p)->p_session->s_ttyvp : NULL)

int
cttyopen(dev_t dev, int flag, int mode, struct proc *p)
{
	struct vnode *ttyvp = cttyvp(p);
	int error;

	if (ttyvp == NULL)
		return (ENXIO);
	VOP_LOCK(ttyvp);
	error = VOP_ACCESS(ttyvp,
	  (flag&FREAD ? VREAD : 0) | (flag&FWRITE ? VWRITE : 0), p->p_ucred, p);
	if (!error)
		error = VOP_OPEN(ttyvp, flag, NOCRED, p);
	VOP_UNLOCK(ttyvp);
	return (error);
}

int
cttyread(dev_t dev, struct uio *uio, int flag)
{
	register struct vnode *ttyvp = cttyvp(uio->uio_procp);
	int error;

	if (ttyvp == NULL)
		return (EIO);
	VOP_LOCK(ttyvp);
	error = VOP_READ(ttyvp, uio, flag, NOCRED);
	VOP_UNLOCK(ttyvp);
	return (error);
}

int
cttywrite(dev_t dev, struct uio *uio, int flag)
{
	register struct vnode *ttyvp = cttyvp(uio->uio_procp);
	int error;

	if (ttyvp == NULL)
		return (EIO);
	VOP_LOCK(ttyvp);
	error = VOP_WRITE(ttyvp, uio, flag, NOCRED);
	VOP_UNLOCK(ttyvp);
	return (error);
}

int
cttyioctl(dev_t dev, int cmd, caddr_t addr, int flag, struct proc *p)
{
	struct vnode *ttyvp = cttyvp(p);

	if (ttyvp == NULL)
		return (EIO);
	if (cmd == TIOCNOTTY) {
		if (!SESS_LEADER(p)) {
			p->p_flag &= ~SCTTY;
			return (0);
		} else
			return (EINVAL);
	}
	return (VOP_IOCTL(ttyvp, cmd, addr, flag, NOCRED, p));
}

int
cttyselect(dev_t dev, int flag, struct proc *p)
{
	struct vnode *ttyvp = cttyvp(p);

	if (ttyvp == NULL)
		return (1);	/* try operation to get EOF/failure */
	return (VOP_SELECT(ttyvp, flag, FREAD|FWRITE, NOCRED, p));
}

static struct devif ctty_devif =
{
	{0}, -1, -2, 0, 0, 0, 0, 0, 0,
	cttyopen, 0, cttyioctl, cttyread, cttywrite, cttyselect, 0,
	0, 0, 0, 0,
};

DRIVER_MODCONFIG() {
	char *cfg_string = ctty_config;
	
	/* can configure driver? */
	if (devif_config(&cfg_string, &ctty_devif) == 0)
		return;
}
