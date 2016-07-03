
PROG=	ls
SRCS=	cmp.c ls.c print.c util.c

# I can here the howls already ...
LINKS=	$(BINDIR)/ls $(BINDIR)/dir

DPADD=	${LIBUTIL}
LDADD=	-lutil

.include <bsd.prog.mk>
