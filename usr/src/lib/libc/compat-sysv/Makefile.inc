.PATH: ${.CURDIR}/${MACHINE}/compat-sysv ${.CURDIR}/compat-sysv

SRCS+=	 shmat.c shmctl.c shmdt.c shmget.c

MAN2+=	shm.0
MLINKS+= shm.2 shmat.2 shm.2 shmctl.2 shm.2 shmdt.2 shm.2 shmget.2
