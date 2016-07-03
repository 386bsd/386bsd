
PROG=	sed
SRCS=	sed.c utils.c rx.c getopt.c getopt1.c
CFLAGS+=-I${.CURDIR}  -D_POSIX_SOURCE
NOMAN=noman

sed.o rx.o: rx.h
sed.o getopt1.o: getopt.h

.include <gnu.prog.mk>
.include <bsd.prog.mk>
