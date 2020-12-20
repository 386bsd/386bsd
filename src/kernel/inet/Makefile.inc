# MODULE=inet
.PATH:	$S/inet
DOMAIN_SRCS+= \
	if_ether.c in.c in_pcb.c in_proto.c ip_icmp.c \
	ip_input.c ip_output.c raw_ip.c tcp_debug.c tcp_input.c \
	tcp_output.c tcp_subr.c tcp_timer.c tcp_usrreq.c udp_usrreq.c
MAN4+=	icmp.0 inet.0 ip.0 tcp.0 udp.0
IDENT+=	-DINET	# XXX -- eliminate conditional compilation dependancies

# machine dependant checksum, or portable version?
.if exists(in_cksum_${MACHINE}.c)
MACH_SRCS+= in_cksum_${MACHINE}.c
.elif exists(in_cksum_${MACHINE}.s)
MACH_SRCS+= in_cksum_${MACHINE}.s
.else
DOMAIN_SRCS+= in_cksum.c
.endif

# symbols
SYMBOLS += inet.symbols

# .include <bsd.kernel.domain.mk>
