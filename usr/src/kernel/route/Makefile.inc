# MODULE=route
.PATH:	$S/route
DEV_SRCS+=  radix.c route.c rtsock.c

SYMBOLS+= route.symbols
MAN4+=	route.0

# .include <bsd.kernel.dev.mk>
