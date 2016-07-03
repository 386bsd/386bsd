NMODULE=net
NETWORK_C?=	${CC} -c ${CFLAGS} ${PROF} ${DEBUG} ${.IMPSRC} -o ${.TARGET}

.SUFFIXES: .${NMODULE}o

OBJS+=  ${DOMAIN_SRCS:R:S/$/.${NMODULE}o/g}
OBJS+=  ${DOMAIN_SRCS_S:R:S/$/.${NMODULE}o/g}
INCLUDES+= -I$S/include/domain

.c.${NMODULE}o:
	${NETWORK_C}

DEPEND+= depend_domain

depend_domain: ${DOMAIN_SRCS}
	mkdep ${COPTS} ${.ALLSRC}
	mv .depend /tmp/dep
	sed -e "s;.o :;.neto:;" < /tmp/dep >depend_domain
	rm -rf /tmp/dep

