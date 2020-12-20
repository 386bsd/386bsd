# MODULE=ufs
.PATH:	$S/ufs
FS_SRCS+= \
	ufs_alloc.c ufs_bmap.c ufs_inode.c ufs_lockf.c ufs_lookup.c \
	ufs_subr.c ufs_tables.c ufs_vfsops.c ufs_vnops.c

MAN5+=	ufs.0

# .include <bsd.kernel.fs.mk>
