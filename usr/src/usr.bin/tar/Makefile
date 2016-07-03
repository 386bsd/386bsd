
PROG=	tar
SRCS=	tar.c create.c extract.c buffer.c getoldopt.c update.c gnu.c mangle.c \
	version.c list.c names.c diffarch.c getopt.c getopt1.c \
	port.c regex.c fnmatch.c getdate.y rtapelib.c
CFLAGS+= -DSTDC_MSG -DNEED_TZSET -DNEEDPAD -DDIRENT
CFLAGS+= -DDEF_AR_FILE=\"/dev/rmt8\" -DDEFBLOCKING=20

CLEANFILES=tar.5.0

tar.0: tar.5.0

afterinstall:
	install -c -o ${MANOWN} -g ${MANGRP} -m ${MANMODE} tar.5.0 \
	    ${DESTDIR}${MANDIR}5/tar.0

.include <gnu.prog.mk>
.include <bsd.prog.mk>
