
PROG=	crontab
SRCS=	crontab.c misc.c entry.c env.c
CFLAGS+=-I${.CURDIR} -I${.CURDIR}/../../libexec/cron -DDEBUGGING=1 -DBSD=0x900000 -DCRONDIR='"/var/cron"' -fstrength-reduce
MAN5=	crontab.0
CLEANFILES=crontab.5.0

crontab.0: crontab.5.0

afterinstall:
	install -c -o ${MANOWN} -g ${MANGRP} -m ${MANMODE} crontab.5.0 \
	    ${DESTDIR}${MANDIR}5/crontab.0

.include <bsd.prog.mk>
.PATH: ${.CURDIR}/../../libexec/cron
