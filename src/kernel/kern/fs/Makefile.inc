#
# Makefile for the filesystem subsystem of the kernel. -wfj
#
# $Id: Makefile.inc,v 1.1 94/10/19 17:09:13 bill Exp Locker: bill $

.PATH:	$S/kern/fs
KERN_SRCS+= \
	bio.c deadfs.c fs_fops.c fs_sys.c lookup.c namcache.c specfs.c \
	vfs.c vnode.c

# MAN9+=	fs.0 bio.0 vfs.0 vnode.0  namei.0
