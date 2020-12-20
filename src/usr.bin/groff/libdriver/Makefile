#	@(#)Makefile	6.2 (Berkeley) 4/30/91

.include "../Makefile.g++"

LIB=		driver
SRCS=		input.cc printer.cc
GXXFLAGS+=	-I$(.CURDIR) -I$(.CURDIR)/../include \
		-I$(.CURDIR)/../../../lib/libg++/g++-include
NOMAN=		noman
NOPROFILE=	noprofile

install:

.include <bsd.lib.mk>

.SUFFIXES: .cc
