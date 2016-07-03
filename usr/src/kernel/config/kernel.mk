#
# This make file incorporates the generic 386BSD kernel program makefile,
# kernel.mk. Copyright (C) 1990-1994 W. Jolitz, All Rights Reserved.
# $Id:$
#

.SUFFIXES: .symbols .9 .8 .7 .6 .5 .4 .3 .2 .1 .0

TOUCH?=	touch -f -c
LD?=	/usr/bin/ld
CC?=	cc 
CPP?=	cpp

# XXX overkill, revise include scheme
INCLUDES= -I$S/include

COPTS+=	${INCLUDES} ${IDENT} -DKERNEL -Di386 -DNPX
DEPEND= depend_mk

ASFLAGS= ${DEBUG}
CFLAGS=	-O ${COPTS}

.if defined(KERNBASE)
CFLAGS+= -DKERNBASE=0x${KERNBASE}
BASE= -DKERNBASE=0x${KERNBASE}
.else
KERNBASE=FE000000
.endif

.if !exists(machine)
FOO!=ln -s $S/include/$(MACHINE) machine
.endif

.MAIN:	all

ALLMAN=	${MAN1} ${MAN2} ${MAN3} ${MAN4} ${MAN5} ${MAN6} ${MAN7} ${MAN8} ${MAN9}

KOBJS = ${FIRSTOBJ} ${OBJS}

_KERNS=		${KERNEL}
.if defined(DDB)
_KERNS+=	${KERNEL}.ddb
.endif
.if defined(PROFILE)
_KERNS+=	${KERNEL}.prof
.endif
.if defined(KGDB)
_KERNS+=	${KERNEL}.kgdb
.endif

all: ${_KERNS}	${ALLMAN}

assym.s: $S/include/sys/param.h $S/include/buf.h $S/include/vmmeter.h \
	$S/include/proc.h $S/include/msgbuf.h machine/vmparam.h \
	$S/config/genassym.c
	${CC} ${INCLUDES} -DKERNEL ${IDENT} ${PARAM} ${BASE} \
		 $S/config/genassym.c -o genassym
	./genassym >assym.s

.include "$S/config/kernel.kern.mk"
.include "$S/config/kernel.dev.mk"
.include "$S/config/kernel.fs.mk"
.include "$S/config/kernel.domain.mk"

SRCS= ${KERN_SRCS} ${MACH_SRCS} ${DEV_SRCS} ${FS_SRCS} ${DOMAIN_SRCS}

${KERNEL}: Makefile symbols.sort ${FIRSTOBJ} ${OBJS}
	@echo loading $@
	@rm -f $@
	@$S/config/newvers.sh
	@${CC} -c ${CFLAGS} ${PROF} ${DEBUG} vers.c
.if defined(DEBUGSYM)
	@${LD} -z -T ${KERNBASE} -o $@ -X ${FIRSTOBJ} ${OBJS} vers.o
.else
	@${LD} -z -T ${KERNBASE} -o $@ -x ${FIRSTOBJ} ${OBJS} vers.o
.endif
	@echo rearranging symbols
	@symorder ${SYMORDER} symbols.sort $@
.if defined(DBSYM)
	@${DBSYM} $@
.endif
	@size $@
	@chmod 755 $@

.9.0 .8.0 .7.0 .6.0 .5.0 .4.0 .3.0 .2.0 .1.0:
	nroff -mandoc ${.IMPSRC} > ${.TARGET}

clean:
	rm -f eddep 386bsd* tags ${OBJS} errs linterrs makelinks

depend: ${DEPEND}
	cat ${DEPEND} >> .depend

depend_mk: assym.s
	mkdep ${COPTS} ${.ALLSRC}
	mkdep -a -p ${INCLUDES} ${IDENT} ${PARAM} $S/config/genassym.c
	mv .depend depend_mk

symbols.sort: ${SYMBOLS}
	grep -hv '^#' ${.ALLSRC} | sed 's/^	//' | sort -u > symbols.sort

.if !target(install)
.if !target(beforeinstall)
beforeinstall:
.endif

realinstall: beforeinstall

install: afterinstall
afterinstall: realinstall maninstall
.endif

.if !target(tags)
tags: ${SRCS}
	-cd ${.CURDIR}; ctags -f /dev/stdout ${.ALLSRC:M*.c} | \
	    sed "s;\${.CURDIR}/;;" > tags
.endif

.include <bsd.man.mk>
