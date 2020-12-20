# MODULE=ppp
# XXX slip must be loaded with ppp
.PATH:	$S/ppp $S/slip
DEV_SRCS+= ppp.c
# MAN4+=	ppp.0

# .include <bsd.kernel.dev.mk>
