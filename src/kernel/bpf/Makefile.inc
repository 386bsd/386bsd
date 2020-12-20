# MODULE=inet
.PATH:	$S/bpf
DOMAIN_SRCS+= \
	bpf.c bpf_filter.c
MAN4+=	bpf.0

# .include <bsd.kernel.domain.mk>
