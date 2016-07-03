LIB=	gnuregex
CFLAGS+=-DHAVE_STRING_H=1
SRCS=	regex.c
NOMAN=	noman

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/regex.h \
		${DESTDIR}/usr/include/nonstd/gnu

.include <bsd.lib.mk>
