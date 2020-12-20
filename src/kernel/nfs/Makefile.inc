# MODULE=nfs
.PATH:	$S/nfs
FS_SRCS+= \
	nfs_bio.c nfs_node.c nfs_serv.c nfs_socket.c nfs_srvcache.c \
	nfs_subs.c nfs_syscalls.c nfs_vfsops.c nfs_vnops.c

SYMBOLS+= ufs.symbols
# MAN4+=	nfs.0
IDENT+=	-DNFS	# XXX -- eliminate conditional compilation dependancies

# .include <bsd.kernel.fs.mk>
