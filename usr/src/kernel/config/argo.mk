#
# 386BSD operating system kernel for argo server
#

KERNEL=	386bsd
IDENT+= -Di486 -DTCP_COMPAT_42 -DGATEWAY

# standard kernel with INTERNET protocols
.include "$S/config/config.std.mk"
.include "$S/config/config.inet.mk"

# additional options
.include "$S/ddb/Makefile.inc"
.include "$S/bpf/Makefile.inc"			# packet filter
.include "$S/as/Makefile.inc"			# Adaptec 1540 SCSI
.include "$S/ed/Makefile.inc"			# NE2000/SMC ethernet
.include "$S/kern/opt/ktrace/Makefile.inc"	# BSD ktrace mechanism
.include "$S/isofs/Makefile.inc"		# ISO-9660 CDROM filesystem
.include "$S/mfs/Makefile.inc"			# BSD memory-based filesystem
#.include "$S/dosfs/Makefile.inc"		# MS DOS FAT filesystem

.include "$S/config/kernel.mk"
