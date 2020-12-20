
PROG=	bc
CFLAGS+= -I. -DBC_MATH_FILE='"/usr/lib/libmath.b"'
SRCS=	bc.y scan.l util.c main.c number.c storage.c load.c execute.c  global.c
LDADD=	-lm
DPADD=	$(LIBM)

afterinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 $(.CURDIR)/libmath.b \
	    ${DESTDIR}${LIBDIR}/libmath.b

.include <gnu.prog.mk>
.include <bsd.prog.mk>
