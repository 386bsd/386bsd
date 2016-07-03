KMODULE=kern
KERNEL_C?=	${CC} -c ${CFLAGS} ${PROF} ${DEBUG} ${.IMPSRC} -o ${.TARGET}
KERNEL_C_C?=	${CC} -c ${CFLAGS} ${PROF} ${PARAM} ${DEBUG} ${.IMPSRC} \
			-o ${.TARGET}
MACH_C?= 	${CC} -c -I$S/kern/${MACHINE} ${CFLAGS} ${PROF} ${DEBUG} \
			${.IMPSRC} -o ${.TARGET}
MACH_AS?= 	${CPP} -I. -I$S/kern/${MACHINE} -DLOCORE ${COPTS} ${.IMPSRC} | \
		${AS} ${ASFLAGS} -o ${.TARGET}

.SUFFIXES: .${KMODULE}o .${KMODULE}co .${KMODULE}mo

OBJS+=  ${KERN_SRCS:R:S/$/.${KMODULE}o/g}
OBJS+=  ${KERN_SRCS_C:R:S/$/.${KMODULE}co/g}
OBJS+=  ${MACH_SRCS:R:S/$/.${KMODULE}mo/g}
OBJS+=  ${MACH_SRCS_S:R:S/$/.${KMODULE}mo/g}

.c.${KMODULE}o:
	${KERNEL_C}

.c.${KMODULE}co:
	${KERNEL_C_C}

.c.${KMODULE}mo:
	${MACH_C}

.s.${KMODULE}mo:
	${MACH_AS}

DEPEND+= depend_kern depend_mach

depend_kern: ${KERN_SRCS}
	mkdep ${COPTS} ${.ALLSRC}
	mv .depend /tmp/dep
	sed -e "s;.o :;.kerno:;" < /tmp/dep >depend_kern
	rm -rf /tmp/dep

depend_mach: ${MACH_SRCS}
	mkdep ${COPTS} ${.ALLSRC}
	mv .depend /tmp/dep
	sed -e "s;.o :;.kerno:;" < /tmp/dep >depend_mach
	rm -rf /tmp/dep

