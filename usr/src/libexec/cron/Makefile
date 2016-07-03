
PROG=	cron
SRCS=	cron.c database.c do_command.c entry.c env.c job.c \
	user.c popen.c misc.c
CFLAGS+=-I${.CURDIR} -DDEBUGGING=1 -DBSD=0x900000 -DCRONDIR='"/var/cron"' -fstrength-reduce
MAN8=	cron.0

.include <bsd.prog.mk>
