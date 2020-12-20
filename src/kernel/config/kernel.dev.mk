DMODULE=dev
DRIVER_C?=	${CC} -c ${CFLAGS} ${PROF} ${DEBUG} ${.IMPSRC} -o ${.TARGET}

.SUFFIXES: .${DMODULE}o .S

OBJS+=  ${DEV_SRCS:R:S/$/.${DMODULE}o/g}
INCLUDES+= -I$S/include/dev -I$S/include/ic

.c.${DMODULE}o:
	${DRIVER_C}

.S.${DMODULE}o:
	${DRIVER_C} -D__ASSEMBLER__

DEPEND+= depend_dev

depend_dev: ${DEV_SRCS}
	mkdep ${COPTS} ${.ALLSRC}
	mv .depend /tmp/dep
	sed -e "s;.o :;.devo:;" < /tmp/dep >depend_dev
	rm -rf /tmp/dep

