/*
 * Copyright (c) 1989 The Regents of the University of California.
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
 *	$Id$
 */

typedef quad fsid_t;			/* file system id type */

/*
 * File identifier.
 * These are unique per filesystem on a single machine.
 */
#define	MAXFIDSZ	16

struct fid {
	u_short		fid_len;		/* length of data in bytes */
	u_short		fid_reserved;		/* force longword alignment */
	char		fid_data[MAXFIDSZ];	/* data (variable length) */
};

/*
 * file system statistics
 */

#define MNAMELEN 90	/* length of buffer for returned name */

struct statfs {
	short	f_type;			/* type of filesystem (see below) */
	short	f_flags;		/* copy of mount flags */
	long	f_fsize;		/* fundamental file system block size */
	long	f_bsize;		/* optimal transfer block size */
	long	f_blocks;		/* total data blocks in file system */
	long	f_bfree;		/* free blocks in fs */
	long	f_bavail;		/* free blocks avail to non-superuser */
	long	f_files;		/* total file nodes in file system */
	long	f_ffree;		/* free file nodes in fs */
	fsid_t	f_fsid;			/* file system id */
	long	f_spare[9];		/* spare for later */
	char	f_mntonname[MNAMELEN];	/* directory on which mounted */
	char	f_mntfromname[MNAMELEN];/* mounted filesystem */
};

/*
 * File system types.
 */
#define	MOUNT_NONE	0
#define	MOUNT_UFS	1		/* UNIX "Fast" Filesystem */
#define	MOUNT_NFS	2		/* Network Filesystem */
#define	MOUNT_MFS	3		/* Memory Filesystem */
#define	MOUNT_MSDOS	4		/* MSDOS Filesystem */
#define MOUNT_ISOFS	5		/* iso9660 cdrom */
/*#define	MOUNT_MAXTYPE	5*/

/*
 * Structure per mounted file system.
 * Each mounted file system has an array of
 * operations and an instance record.
 * The file systems are put on a doubly linked list.
 */
struct mount {
	struct mount	*mnt_next;		/* next in mount list */
	struct mount	*mnt_prev;		/* prev in mount list */
	struct vfsops	*mnt_op;		/* operations on fs */
	struct vnode	*mnt_vnodecovered;	/* vnode we mounted on */
	struct vnode	*mnt_mounth;		/* list of vnodes this mount */
	int		mnt_flag;		/* flags */
	uid_t		mnt_exroot;		/* exported mapping for uid 0 */
	struct statfs	mnt_stat;		/* cache of filesystem stats */
	qaddr_t		mnt_data;		/* private data */
};

/*
 * Mount flags.
 */
#define	MNT_RDONLY	0x00000001	/* read only filesystem */
#define	MNT_SYNCHRONOUS	0x00000002	/* file system written synchronously */
#define	MNT_NOEXEC	0x00000004	/* can't exec from filesystem */
#define	MNT_NOSUID	0x00000008	/* don't honor setuid bits on fs */
#define	MNT_NODEV	0x00000010	/* don't interpret special files */
#define	MNT_WCHR	0x00000020	/* use wide characters instead */

/*
 * exported mount flags.
 */
#define	MNT_EXPORTED	0x00000100	/* file system is exported */
#define	MNT_EXRDONLY	0x00000200	/* exported read only */

/*
 * Flags set by internal operations.
 */
#define	MNT_LOCAL	0x00001000	/* filesystem is stored locally */
#define	MNT_QUOTA	0x00002000	/* quotas are enabled on filesystem */

/*
 * Mask of flags that are visible to statfs()
 */
#define	MNT_VISFLAGMASK	0x0000ffff

/*
 * filesystem control flags.
 *
 * MNT_MLOCK lock the mount entry so that name lookup cannot proceed
 * past the mount point.  This keeps the subtree stable during mounts
 * and unmounts.
 */
#define	MNT_UPDATE	0x00010000	/* not a real mount, just an update */
#define	MNT_MLOCK	0x00100000	/* lock so that subtree is stable */
#define	MNT_MWAIT	0x00200000	/* someone is waiting for lock */
#define MNT_MPBUSY	0x00400000	/* scan of mount point in progress */
#define MNT_MPWANT	0x00800000	/* waiting for mount point */
#define MNT_UNMOUNT	0x01000000	/* unmount in progress */

/*
 * Operations supported on mounted file system.
 */
#ifdef KERNEL
#ifdef __STDC__
struct nameidata;
#endif

/* XXX should be struct vfs */
struct vfsops {
	char *vfs_name; 	/* virtual filesystem module type name */
	int vfs_type; 		/* virtual filesystem module type */
	struct vfsops *vfs_next;	/* next filesystem type */

	int (*vfs_mount)(struct mount *, char *, caddr_t, struct nameidata *, struct proc *);
	int (*vfs_start)(struct mount *, int, struct proc *);
	int (*vfs_unmount)(struct mount *, int, struct proc *);
	int (*vfs_root)(struct mount *, struct vnode **);
	int (*vfs_quotactl)(struct mount *, int, uid_t, caddr_t, struct proc *);
	int (*vfs_statfs)(struct mount *, struct statfs *, struct proc *);
	int (*vfs_sync)(struct mount *, int);
	int (*vfs_fhtovp)(struct mount *, struct fid *, struct vnode **);
	int (*vfs_vptofh)(struct vnode *, struct fid *);
	int (*vfs_init)	(void);
	int (*vfs_mountroot)(void);
};

#define VFS_MOUNT(MP, PATH, DATA, NDP, P) \
	(*(MP)->mnt_op->vfs_mount)(MP, PATH, DATA, NDP, P)
#define VFS_START(MP, FLAGS, P)	  (*(MP)->mnt_op->vfs_start)(MP, FLAGS, P)
#define VFS_UNMOUNT(MP, FORCE, P) (*(MP)->mnt_op->vfs_unmount)(MP, FORCE, P)
#define VFS_ROOT(MP, VPP)	  (*(MP)->mnt_op->vfs_root)(MP, VPP)
#define VFS_QUOTACTL(MP,C,U,A,P)  (*(MP)->mnt_op->vfs_quotactl)(MP, C, U, A, P)
#define VFS_STATFS(MP, SBP, P)	  (*(MP)->mnt_op->vfs_statfs)(MP, SBP, P)
#define VFS_SYNC(MP, WAITFOR)	  (*(MP)->mnt_op->vfs_sync)(MP, WAITFOR)
#define VFS_FHTOVP(MP, FIDP, VPP) (*(MP)->mnt_op->vfs_fhtovp)(MP, FIDP, VPP)
#define	VFS_VPTOFH(VP, FIDP)	  (*(VP)->v_mount->mnt_op->vfs_vptofh)(VP, FIDP)
#endif /* KERNEL */

/*
 * Flags for various system call interfaces.
 *
 * forcibly flags for vfs_umount().
 * waitfor flags to vfs_sync() and getfsstat()
 */
#define MNT_FORCE	1
#define MNT_NOFORCE	2
#define MNT_WAIT	1
#define MNT_NOWAIT	2

/*
 * Generic file handle
 */
struct fhandle {
	fsid_t	fh_fsid;	/* File system id of mount point */
	struct	fid fh_fid;	/* Id of file */
};
typedef struct fhandle	fhandle_t;

/*
 * Arguments to mount UFS
 */
struct ufs_args {
	char	*fspec;		/* block special device to mount */
	int	exflags;	/* export related flags */
	uid_t	exroot;		/* mapping for root uid */
};

#ifdef MFS
/*
 * Arguments to mount MFS
 */
struct mfs_args {
	char	*name;		/* name to export for statfs */
	caddr_t	base;		/* base address of file system in memory */
	u_long size;		/* size of file system */
};
#endif MFS

#ifdef NFS
/*
 * File Handle (32 bytes for version 2), variable up to 1024 for version 3
 */
union nfsv2fh {
	fhandle_t	fh_generic;
	u_char		fh_bytes[32];
};
typedef union nfsv2fh nfsv2fh_t;

/*
 * Arguments to mount NFS
 */
struct nfs_args {
	struct sockaddr	*addr;		/* file server address */
	int		sotype;		/* Socket type */
	int		proto;		/* and Protocol */
	nfsv2fh_t	*fh;		/* File handle to be mounted */
	int		flags;		/* flags */
	int		wsize;		/* write size in bytes */
	int		rsize;		/* read size in bytes */
	int		timeo;		/* initial timeout in .1 secs */
	int		retrans;	/* times to retry send */
	char		*hostname;	/* server's name */
};
/*
 * NFS mount option flags
 */
#define	NFSMNT_SOFT	0x0001	/* soft mount (hard is default) */
#define	NFSMNT_WSIZE	0x0002	/* set write size */
#define	NFSMNT_RSIZE	0x0004	/* set read size */
#define	NFSMNT_TIMEO	0x0008	/* set initial timeout */
#define	NFSMNT_RETRANS	0x0010	/* set number of request retrys */
#define	NFSMNT_HOSTNAME	0x0020	/* set hostname for error printf */
#define	NFSMNT_INT	0x0040	/* allow interrupts on hard mount */
#define	NFSMNT_NOCONN	0x0080	/* Don't Connect the socket */
#define	NFSMNT_SCKLOCK	0x0100	/* Lock socket against others */
#define	NFSMNT_WANTSCK	0x0200	/* Want a socket lock */
#define	NFSMNT_SPONGY	0x0400	/* spongy mount (soft for stat and lookup) */
#define	NFSMNT_COMPRESS	0x0800	/* Compress nfs rpc xdr */
#define	NFSMNT_LOCKBITS	(NFSMNT_SCKLOCK | NFSMNT_WANTSCK)
#endif NFS

#ifdef KERNEL
/*
 * exported vnode operations
 */
void	vfs_remove __P((struct mount *mp)); /* remove a vfs from mount list */

/* interface symbols */
#define	__ISYM_VERSION__ "1"	/* XXX RCS major revision number of hdr file */
#include "isym.h"		/* this header has interface symbols */

/* global variables used in core kernel and other modules */
__ISYM__(struct	mount *, rootfs,)	/* ptr to root mount structure */

/* functions used in modules */
__ISYM__(int, vfs_lock, (struct mount *mp))   /* lock a vfs */
__ISYM__(void, vfs_unlock, (struct mount *mp)) /* unlock a vfs */
__ISYM__(int, vfs_busy, (struct mount *mp))   /* busy a vfs */
__ISYM__(void, vfs_unbusy, (struct mount *mp)) /* unbusy a vfs */
__ISYM__(struct	mount *, getvfs, (fsid_t *fsid)) /* return vfs given fsid */
__ISYM__(void, mntflushbuf, (struct mount *mountp, int flags))
__ISYM__(int, mntinvalbuf, (struct mount *mountp))
__ISYM__(int, vflush, (struct mount *mp, struct vnode *skipvp, int flags))
__ISYM__(void, addvfs, (struct vfsops *))	/* insert filesystem into vfslist */
__ISYM__(void, cache_purgevfs, (struct mount *mp))

#undef __ISYM__
#undef __ISYM_ALIAS__
#undef __ISYM_VERSION__

/* XXX temp vm interface */
void vnode_pager_umount(struct mount *mp);

/* struct	vfsops *vfssw[];	/* mount filesystem type table */
extern struct	vfsops *vfs;		/* head of vfs list */
struct	vfsops *findvfs(int type);	/* return vfs associated with type */
/* use another filesystem to complete a different filesystem */
void incoporatefs(struct vfsops *, struct vfsops *);
/*int	(*mountroot)();		/* perform mount of root filesystem */

#else /* KERNEL */

#include <sys/cdefs.h>

__BEGIN_DECLS
int	fstatfs __P((int, struct statfs *));
int	getfh __P((const char *, fhandle_t *));
int	getfsstat __P((struct statfs *, long, int));
int	getmntinfo __P((struct statfs **, int));
int	mount __P((int, const char *, int, void *));
int	statfs __P((const char *, struct statfs *));
int	unmount __P((const char *, int));
__END_DECLS

#endif /* KERNEL */
