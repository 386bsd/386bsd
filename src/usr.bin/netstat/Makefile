#	@(#)Makefile	5.14 (Berkeley) 6/18/90

PROG=	netstat
NONSTDINC+= $(INCOLDBSD)
SRCS=	inet.c if.c main.c mbuf.c route.c unix.c
BINGRP=	kmem
BINMODE=2555
LDADD=	-lutil
DPADD=	${LIBUTIL}

.include <bsd.prog.mk>
