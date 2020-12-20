#
# 4.3 compatibility.
#
# $Id: Makefile.inc,v 1.1 94/10/19 18:38:41 bill Exp $

.PATH:	$S/kern/opt/compat43
KERN_SRCS+= tty_compat.c
IDENT+=	-DCOMPAT_43

# MAN4+= tty43.0

