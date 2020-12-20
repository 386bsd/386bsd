#	@(#)Makefile	5.1 (Berkeley) 5/23/90

MAN1=	nroff.0

all nroff: ${MAN1}

clean depend lint tags:

cleandir:
	rm -f ${MAN1}

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/nroff.sh ${DESTDIR}/usr/bin/nroff

.include <bsd.prog.mk>
