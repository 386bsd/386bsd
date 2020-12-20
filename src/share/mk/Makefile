# $Id$

FILES=	 bsd.README bsd.doc.mk bsd.lib.mk bsd.man.mk bsd.prog.mk \
	 bsd.subdir.mk gnu.prog.mk kernel.mk sys.mk

all clean cleandir depend lint tags:

install:
	install -c -o ${BINOWN} -g ${BINOWN} -m 444 ${FILES} \
	    ${DESTDIR}${BINDIR}/mk

.include <bsd.prog.mk>
