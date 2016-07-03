#	@(#)Makefile	5.2 (Berkeley) 9/27/90

PROG=	portmap
NONSTDINC=$(INCOLDBSD)
DPADD=	${LIBRPC} ${LIBUTIL}
LDADD=	-lrpc -lutil
MAN8=	portmap.0

.include <bsd.prog.mk>
