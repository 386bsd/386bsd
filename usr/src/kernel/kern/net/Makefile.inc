#
# Makefile for the network interface subsystem of the kernel. -wfj
# $Id: Makefile.inc,v 1.1 94/10/20 00:01:28 bill Exp $
#

.PATH:	$S/kern/net
KERN_SRCS+= \
	if.c ethersubr.c raw_cb.c raw_usrreq.c rt.c

SYMBOLS+= net.symbols

# MAN9+=	if.0 raw.0 ethersubr.0
MAN4+= netintro.0
