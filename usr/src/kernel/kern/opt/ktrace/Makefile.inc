#
# Kernel trace mechanism
#
# $Id: Makefile.inc,v 1.1 94/10/19 18:36:42 bill Exp $

.PATH:	$S/kern/opt/ktrace
KERN_SRCS+= ktrace.c
IDENT+=	-DKTRACE

# MAN4+= ktrace.0
# MAN9+= ktrace.0
