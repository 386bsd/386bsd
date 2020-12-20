/*-
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
 * $Id: priv.c,v 1.1 94/10/20 00:03:06 bill Exp $
 *
 * 386BSD Process Privilege Mechanism.
 */

#include "sys/param.h"
#include "sys/syslog.h"
#include "sys/errno.h"
#include "proc.h"
#include "privilege.h"
#include "prototypes.h"

/* attempt to use a credential with a privilege in a role and account for it */
int
use_priv(const struct ucred *cr, cr_priv_t prv, struct proc *p)
{
	cr_role_t minrole = ROLE_ALL;
	int rv = 1;

	/* evaluate privilege in terms of role */
	switch(prv) {

		/* used by int, login and daemons (rlogind, telnetd, ...) */
	case PRV_SETLOGIN:
	case PRV_REVOKE:
	case PRV_ADJTIME:
	case PRV_NFS_GETFH:
	case PRV_NFS_SVC:
	case PRV_NFS_ASYNC_DAEMON:
	case PRV_NFS_SRV_CREATE_SPECIAL:
	case PRV_NFS_SRV_REMOVE_DIR:
	case PRV_NFS_SRV_LINK_DIR:
		minrole = ROLE_ALL;
		rv = (cr->cr_uid == 0);
		break;

		/* used by root in normal system maintenance */
	case PRV_SWAPON:
	case PRV_MOUNT:
	case PRV_UNMOUNT:
	case PRV_CHROOT:
	case PRV_MKNOD:
	case PRV_LINKDIR:
	case PRV_UNLINKDIR:
	case PRV_TIOCCONS1:
	case PRV_TIOCCONS2:
	case PRV_SETHOSTID:
	case PRV_SETHOSTNAME:
	case PRV_REBOOT:
	case PRV_SETTIMEOFDAY:
	case PRV_NO_UPROCLIMIT:
	case PRV_RLIMIT_RSS:
	case PRV_RLIMIT_MEMLOCK:
	case PRV_UFS_QUOTA_CHANGE:
	case PRV_SIOCSDARP:
	case PRV_SIOCSIFFLAGS:
	case PRV_SIOCSIFMETRIC:
	case PRV_SLIP_OPEN:
	case PRV_RTIOCTL:
		minrole = ROLE_MGMT;
		rv = (cr->cr_uid == 0);
		break;

	case PRV_RLIMIT_CPU:
	case PRV_RLIMIT_FSIZE:
	case PRV_RLIMIT_DATA:
	case PRV_RLIMIT_STACK:
	case PRV_RLIMIT_CORE:
	case PRV_RLIMIT_NPROC:
	case PRV_RLIMIT_OFILE:
	case PRV_UFS_SETATTR_TIME:
	case PRV_UFS_SETATTR_UID:
	case PRV_UFS_CHOWN:
	case PRV_UFS_CHMOD:
	case PRV_NICE:
		minrole = ROLE_LOCALUSR;
		rv = (cr->cr_uid == 0);
		break;

		/* used by everybody */
	case PRV_SETUID:
	case PRV_SETEUID:
	case PRV_SETGID:
	case PRV_SETEGID:
	case PRV_SETGROUPS:
	case PRV_EXECSETUID:
	case PRV_EXECSETGID:
		minrole = ROLE_NONE;
		rv = (cr->cr_uid == 0);
		break;

#ifdef	DIAGNOSTIC
	default:
		panic("use_priv");
#endif
	}
	
	/* failure due to role? */
	if ((cr_role_t)cr->cr_role > minrole) {

		/* access to sensitive privilege, warn system manager */
		if (minrole <= ROLE_MGMT)
			log(LOG_WARNING, "uid %d attempted to use privilege\n",
				cr->cr_uid);

		/* record failure if proc present */
		/* if (p && p->p_acflag)
			acct_priv(p, ACCT_ROLE_FAIL, cr, prv); */

		return (EPERM);
	}

	/* successful priv, record use of privilege if proc present */
	/* if (rv && p && p->p_acflag)
		acct_priv(p, ACCT_PRIV_SUCCESS, cr, prv);
	else
		acct_priv(p, ACCT_PRIV_FAIL, cr, prv); */

	return (rv ? 0 : EPERM);
}
