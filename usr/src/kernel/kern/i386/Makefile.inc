#
# Makefile for machine-dependant kernel
# $Id: Makefile.inc,v 1.1 94/10/19 17:39:51 bill Exp $
#

.PATH:		$S/kern/i386
MACH_SRCS+=	cpu.c machdep.c pmap.c trap.c
FIRSTOBJ=	locore.o

I386?=		$S/kern/i386

locore.o: assym.s ${I386}/locore.s $S/include/${MACHINE}/trap.h \
	$S/include/${MACHINE}/psl.h \
	$S/include/${MACHINE}/pmap.h $S/include/dev/isa_stdports.h \
	$S/include/dev/isa_irq.h
	${CPP} -I. -DLOCORE ${COPTS} ${I386}/locore.s | \
		${AS} ${ASFLAGS} -o locore.o

# symbols
SYMBOLS += i386.symbols
# MAN9+=	i386.0
