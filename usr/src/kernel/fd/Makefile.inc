# MODULE=fdc
.PATH:	$S/fd
DEV_SRCS+= fdc.c fd.c
DEVBOOT_SRCS+= fdboot.c
DEVBOOTBLK_SRCS+= fdbootblk.c
# MAN4+=	fdc.0

# .include <bsd.kernel.dev.mk>
