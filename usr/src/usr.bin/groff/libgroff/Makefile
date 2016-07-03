#	@(#)Makefile	6.2 (Berkeley) 4/30/91

.include "../Makefile.g++"
.include "../Makefile.fontdir"

LIB=		groff
SRCS=		assert.cc change_lf.cc cmap.cc cset.cc device.cc errarg.cc \
		error.cc fatal.cc filename.cc font.cc fontfile.cc \
		illegal.cc lf.cc lineno.cc macropath.cc nametoindex.cc \
		new.cc progname.cc ptable.cc searchpath.cc string.cc strsave.cc \
		tmpfile.cc version.cc
OBJS+=	iftoa.o itoa.o
GXXFLAGS+=	-I$(.CURDIR) -I$(.CURDIR)/../include \
		-I$(.CURDIR)/../../../lib/libg++/g++-include \
		-DDEVICE=\"ps\" \
		-DFONTPATH=\"$(FONTDIR)\" \
		-DMACROPATH=\"$(MACRODIR)\"
NOMAN=		noman
NOPROFILE=	noprofile

iftoa.o:iftoa.c
	cc -c $(.CURDIR)/iftoa.c
itoa.o:itoa.c
	cc -c $(.CURDIR)/itoa.c

install:

.include <bsd.lib.mk>

.SUFFIXES: .cc
