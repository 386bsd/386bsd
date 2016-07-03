# MODULE=isofs
.PATH:	$S/isofs
FS_SRCS+= \
	isofs_bmap.c isofs_lookup.c isofs_node.c isofs_rrip.c isofs_util.c \
	isofs_vfsops.c isofs_vnops.c
# MAN4+=	isofs.0

IDENT+=	-DISOFS	# XXX -- eliminate conditional compilation dependancies
# .include <bsd.kernel.fs.mk>
