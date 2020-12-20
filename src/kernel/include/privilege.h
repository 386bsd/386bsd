/*
 * Copyright (c) 1994 William Jolitz. All rights reserved.
 * Written by William Jolitz 4/94
 *
 * Redistribution and use in source and binary forms are freely permitted
 * provided that the above copyright notice and attribution and date of work
 * and this paragraph are duplicated in all such forms.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * 386BSD Kernel Privileges and Privilege Classes (roles).
 *
 * $Id: privilege.h,v 1.1 94/07/27 15:33:08 bill Exp Locker: bill $
 */

/*
 * Credential privilege roles (or classes of privilege),
 * larger number means fewer privileges.
 */
enum cr_roles {
	ROLE_ALL,	/* all privileges are possible (must be first) */
	ROLE_MGMT,	/* privileges for system management */
	ROLE_LOCALUSR,	/* privileges for local (non-net) user */
	ROLE_DISTANTUSR, /* privileges for non-local (net) user */
	ROLE_NONE,	/* no privileges (must be last) */
};
typedef enum cr_roles cr_role_t;

#ifdef KERNEL

/*
 * Unique privileges inside of this kernel.
 */
enum cr_priv {
/* core kernel */
PRV_NO_UPROCLIMIT, PRV_SETLOGIN, PRV_SETUID, PRV_SETEUID, PRV_SETGID,
PRV_SETEGID, PRV_SETGROUPS, PRV_EXECSETUID, PRV_EXECSETGID,
PRV_SWAPON, PRV_MOUNT, PRV_UNMOUNT, PRV_CHROOT, PRV_MKNOD, PRV_LINKDIR,
PRV_UNLINKDIR, PRV_REVOKE, PRV_TIOCCONS1, PRV_TIOCCONS2, PRV_SETHOSTID,
PRV_SETHOSTNAME, PRV_REBOOT, PRV_SETTIMEOFDAY, PRV_ADJTIME, PRV_NICE,
/* resources */
PRV_RLIMIT_CPU, PRV_RLIMIT_FSIZE, PRV_RLIMIT_DATA, PRV_RLIMIT_STACK,
PRV_RLIMIT_CORE, PRV_RLIMIT_RSS, PRV_RLIMIT_MEMLOCK, PRV_RLIMIT_NPROC,
PRV_RLIMIT_OFILE,
/* UFS filesystem */
PRV_UFS_SETATTR_TIME, PRV_UFS_SETATTR_UID, PRV_UFS_CHMOD, PRV_UFS_CHOWN,
PRV_UFS_MAKNODE, PRV_UFS_QUOTA_CHANGE,
/* NFS filesystem */
PRV_NFS_GETFH, PRV_NFS_SVC, PRV_NFS_ASYNC_DAEMON, PRV_NFS_SRV_CREATE_SPECIAL,
PRV_NFS_SRV_REMOVE_DIR, PRV_NFS_SRV_LINK_DIR,
/* net */
PRV_SIOCSDARP, PRV_SIOCSIFFLAGS, PRV_SIOCSIFMETRIC, PRV_SLIP_OPEN, PRV_RTIOCTL,
};
typedef	enum cr_priv cr_priv_t;

/* interface symbols */
#define	__ISYM_VERSION__ "1"	/* XXX RCS major revision number of hdr file */
#include "isym.h"		/* this header has interface symbols */

/* functions used in modules */
__ISYM__(int, use_priv, (const struct ucred *, cr_priv_t, struct proc *))

#undef __ISYM__
#undef __ISYM_ALIAS__
#undef __ISYM_VERSION__
#endif
