
PROG=	pppd
SRCS=	main.c magic.c fsm.c lcp.c ipcp.c upap.c chap.c md5.c \
	auth.c options.c lock.c sys-bsd.c
CFLAGS+= -DPPPDISC=5 -DNO_DRAND48
NONSTDINC+= ${INCOLDBSD}
MAN8=	pppd.0
BINMODE=4555
BINOWN=	root

LDADD=	-lutil
DPADD=	${LIBUTIL}

.include <bsd.prog.mk>
