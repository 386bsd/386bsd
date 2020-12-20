#	@(#)Makefile	5.3 (Berkeley) 12/4/90

PROG=	date
SRCS=	date.c netdate.c
NONSTDINC+= ${INCOLDBSD}
DPADD=	${LIBUTIL}
LDADD=	-lutil

.include <bsd.prog.mk>
