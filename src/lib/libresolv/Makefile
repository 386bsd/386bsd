#	@(#)Makefile	5.1 (Berkeley) 6/5/90

NONSTDINC=$(INCOLDBSD)
LIB=resolv
CFLAGS+=-DDEBUG -DLIBC_SCCS

.PATH:	${.CURDIR}/../libc/net

SRCS=gethostnamadr.c res_mkquery.c res_query.c res_send.c

.include <bsd.lib.mk>
