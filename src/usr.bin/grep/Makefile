PROG=	grep
CFLAGS+= -I${.CURDIR}
SRCS=	dfa.c grep.c kwset.c obstack.c regex.c search.c
MLINKS= grep.1 egrep.1
LINKS=	${BINDIR}/grep ${BINDIR}/fgrep ${BINDIR}/egrep ${BINDIR}/posix-egrep

.include <gnu.prog.mk>
.include <bsd.prog.mk>
