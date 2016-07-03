#	@(#)Makefile	6.2 (Berkeley) 3/3/91

.include "../Makefile.g++"

PROG=		groff
SRCS=		groff.cc
OBJS+=		pipeline.o
LDADD+=		$(.CURDIR)/../libgroff/obj/libgroff.a -lm
DPADD+=		${LIBM}
# Nuke -Ig++-include when we finally install libg++.
GXXFLAGS+= -DPROG_PREFIX="\"\"" -I$(.CURDIR)/../include \
		-I$(.CURDIR)/../../../lib/libg++/g++-include

pipeline.o: pipeline.c
	cc -c $(.CURDIR)/pipeline.c

afterinstall:
	install -c -o $(BINOWN) -g $(BINGRP) -m $(BINMODE) \
		$(.CURDIR)/nroff.sh $(DESTDIR)$(BINDIR)/nroff
	install -c -o $(BINOWN) -g $(BINGRP) -m $(BINMODE) \
		$(.CURDIR)/psroff.sh $(DESTDIR)$(BINDIR)/psroff

.include "../../Makefile.inc"
.include <bsd.prog.mk>
