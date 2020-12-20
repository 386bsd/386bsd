# $Id: $

.if !defined(NOINCLUDE) && exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif

.SUFFIXES: .out .o .c .y .l .s .9 .8 .7 .6 .5 .4 .3 .2 .1 .0

.9.0 .8.0 .7.0 .6.0 .5.0 .4.0 .3.0 .2.0 .1.0:
	nroff -mandoc ${.IMPSRC} > ${.TARGET}

.if exists(${.CURDIR}/../include)
I=${.CURDIR}/../include
.endif
.if !exists($I)
# No kernel include directory!
I=/usr/include/nonstd/kernel	# XXX
.endif

.if !exists(machine)
FOO!=ln -s $I/$(MACHINE) machine
.endif

CFLAGS+=${COPTS} -D__MODULE__ -DKERNEL -I$I ${NONSTDINC}
LDFLAGS+= -x -r

# STRIP?=	-s

BINGRP?=	bin
BINOWN?=	bin
BINMODE?=	555

#LIBSCSI,LIBCDROM?
# LIBCRT0?=	/usr/lib/crt0.o
# LIBC?=          /usr/lib/libc.a

INCDEV?=	-I$I/dev
INCFS?=		-I$I/fs
INCIC?=		-I$I/ic
INCNET?=	-I$I/net
INCDOMAIN?=	-I$I/domain

.if defined(MODULE) && defined(SRCS)

OBJS+=  ${SRCS:R:S/$/.o/g}

.else defined(MODULE)

SRCS= ${MODULE}.c
OBJS= ${MODULE}.o
MKDEP=	-p

.endif

${MODULE}: ${OBJS} ${DPADD}
	${LD} ${LDFLAGS} -o ${.TARGET} ${OBJS} ${LDADD}

.if	!defined(MAN1) && !defined(MAN2) && !defined(MAN3) && \
	!defined(MAN4) && !defined(MAN5) && !defined(MAN6) && \
	!defined(MAN7) && !defined(MAN8) && !defined(MAN9) && \
	!defined(NOMAN)
MAN4=	${MODULE}.0
.endif
MANALL=	${MAN1} ${MAN2} ${MAN3} ${MAN4} ${MAN5} ${MAN6} ${MAN7} ${MAN8} ${MAN9}

_MODULESUBDIR: .USE
.if defined(SUBDIR) && !empty(SUBDIR)
	@for entry in ${SUBDIR}; do \
		(echo "===> $$entry"; \
		if test -d ${.CURDIR}/$${entry}.${MACHINE}; then \
			cd ${.CURDIR}/$${entry}.${MACHINE}; \
		else \
			cd ${.CURDIR}/$${entry}; \
		fi; \
		${MAKE} ${.TARGET:S/realinstall/install/:S/.depend/depend/}); \
	done
.endif

.MAIN: all
all: ${MODULE} ${MANALL} _MODULESUBDIR

.if !target(clean)
clean: _MODULESUBDIR
	rm -f a.out [Ee]rrs mklog core ${MODULE} ${OBJS} ${CLEANFILES} ${MANALL}
.endif

.if !target(cleandir)
cleandir: _MODULESUBDIR
	rm -f a.out [Ee]rrs mklog core ${MODULE} ${OBJS} ${CLEANFILES}
	rm -f .depend ${MANALL}
.endif

# some of the rules involve .h sources, so remove them from mkdep line
.if !target(depend)
depend: .depend _MODULESUBDIR
.depend: ${SRCS}
.if defined(MODULE)
	mkdep ${MKDEP} ${CFLAGS:M-[ID]*} ${.ALLSRC:M*.c}
.endif
.endif

.if !target(install)
.if !target(beforeinstall)
beforeinstall:
.endif
.if !target(afterinstall)
afterinstall:
.endif

realinstall: _MODULESUBDIR
.if defined(MODULE)
	install ${STRIP} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${MODULE} ${DESTDIR}${BINDIR}
.endif
.if defined(LINKS) && !empty(LINKS)
	@set ${LINKS}; \
	while test $$# -ge 2; do \
		l=${DESTDIR}$$1; \
		shift; \
		t=${DESTDIR}$$1; \
		shift; \
		echo $$t -\> $$l; \
		rm -f $$t; \
		ln $$l $$t; \
	done; true
.endif

install: maninstall
maninstall: afterinstall
afterinstall: realinstall
realinstall: beforeinstall
.endif

.if !target(lint)
lint: ${SRCS} _MODULESUBDIR
.if defined(MODULE)
	@${LINT} ${LINTFLAGS} ${CFLAGS} ${.ALLSRC} | more 2>&1
.endif
.endif

.if !target(obj)
.if defined(NOOBJ)
obj: _MODULESUBDIR
.else
obj: _MODULESUBDIR
	@cd ${.CURDIR}; rm -rf obj; \
	here=`pwd`; dest=/usr/obj/`echo $$here | sed 's,/usr/src/,,'`; \
	echo "$$here -> $$dest"; ln -s $$dest obj; \
	if test -d /usr/obj -a ! -d $$dest; then \
		mkdir -p $$dest; \
	else \
		true; \
	fi;
.endif
.endif

.if !target(tags)
tags: ${SRCS} _MODULESUBDIR
.if defined(MODULE)
	-cd ${.CURDIR}; ctags -f /dev/stdout ${.ALLSRC} | \
	    sed "s;\${.CURDIR}/;;" > tags
.endif
.endif

.if !defined(NOMAN)
.include <bsd.man.mk>
.endif
