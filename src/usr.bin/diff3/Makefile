
PROG=	diff3
SRCS=	diff3.c getopt.c getopt1.c version.c
NOMAN=noman
CFLAGS+= -I$(.CURDIR)/../diff -DDIFF_PROGRAM=\"/usr/bin/diff\"

.include <bsd.prog.mk>
.PATH:	${.CURDIR}/../diff
