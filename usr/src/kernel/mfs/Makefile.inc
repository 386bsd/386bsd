# MODULE=mfs
.PATH:	$S/mfs
FS_SRCS+= \
	mfs_vfsops.c mfs_vnops.c
IDENT+=	-DMFS	# XXX -- eliminate conditional compilation dependancies
# MAN4+=	mfs.0

# .include <bsd.kernel.fs.mk>
