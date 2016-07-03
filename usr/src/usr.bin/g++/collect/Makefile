PROG=		collect
SRCS=		collect2.c version.c
CFLAGS+=	-g -I. -I$(.CURDIR)/../../gcc/cc1 \
		-I$(.CURDIR)/../../gcc/cc1/config \
		-DSTANDARD_EXEC_PREFIX=\"/usr/libexec/\"
CLEANFILES+=	g++.0
NOMAN=		noman
.PATH:		$(.CURDIR)/../../gcc/cc1

depend:

.include "../../Makefile.inc"
.include <bsd.prog.mk>
.include "../Makefile.symlinks"
