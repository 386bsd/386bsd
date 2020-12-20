PROG=	pppstats
SRCS=	pppstats.c
NONSTDINC+= ${INCOLDBSD}
CFLAGS+=-I.. -DBSD4_4
LDADD=	-lutil
NOMAN=	noman
BINMODE=2555
BINGRP=	kmem

.include <bsd.prog.mk>
