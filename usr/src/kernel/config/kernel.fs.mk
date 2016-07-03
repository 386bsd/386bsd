FMODULE=fs
FILESYSTEM_C?=	${CC} -c ${CFLAGS} ${PROF} ${DEBUG} ${.IMPSRC} -o ${.TARGET}

.SUFFIXES: .${FMODULE}o

OBJS+=  ${FS_SRCS:R:S/$/.${FMODULE}o/g}
INCLUDES+= -I$S/include/fs

.c.${FMODULE}o:
	${FILESYSTEM_C}

DEPEND+= depend_fs

depend_fs: ${FS_SRCS}
	mkdep ${COPTS} ${.ALLSRC}
	mv .depend /tmp/dep
	sed -e "s;.o :;.fso:;" < /tmp/dep >depend_fs
	rm -rf /tmp/dep

