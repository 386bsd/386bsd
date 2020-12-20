#	@(#)Makefile	8.1 (Berkeley) 6/7/93

PROG=	makemap
NONSTDINC=$(INCOLDBSD) $(INCDB)
MAN8=	makemap.0
CFLAGS+=-I${.CURDIR}/../src -DNDBM -DNEWDB

.include "../../Makefile.inc"
.include <bsd.prog.mk>
