# MODULE=dosfs
# $Id: Makefile.inc,v 1.1 94/10/19 23:46:30 bill Exp $
.PATH:	$S/dosfs
FS_SRCS+= \
	dosfs_conv.c dosfs_denode.c dosfs_fat.c dosfs_lookup.c \
	dosfs_vfsops.c dosfs_vnops.c

MAN4+=	dosfs.0

# .include <bsd.kernel.fs.mk>
