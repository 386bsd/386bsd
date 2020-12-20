#
# Makefile for the socket subsystem of the kernel. -wfj
# $Id: Makefile.inc,v 1.1 94/10/19 23:49:47 bill Exp $
#

.PATH:	$S/kern/socket
KERN_SRCS+= \
	domain.c mbuf.c sock_fops.c sock_sys.c sockbuf.c sockconn.c sockops.c

SYMBOLS+= socket.symbols
# MAN9+=	sockbuf.0 sockops.0 domain.0 protosw.0
