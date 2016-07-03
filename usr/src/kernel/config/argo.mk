#
# 386BSD operating system kernel for network host "argo".
#

KERNEL=	386bsd
IDENT=	-Di486 -DTCP_COMPAT_42
#PARAM=	-DTIMEZONE=480 -DDST=1 -DMAXUSERS=4

# standard kernel with INTERNET protocols
.include "$S/config/config.std.mk"
.include "$S/config/config.inet.mk"

# additional options
.include "$S/math/Makefile.inc"			# NPX emulator
.include "$S/as/Makefile.inc"			# Adaptec 1542 SCSI
.include "$S/ed/Makefile.inc"			# NE2000 ethernet
.include "$S/ddb/Makefile.inc"			# kernel debugger
.include "$S/kern/opt/ktrace/Makefile.inc"	# BSD ktrace mechanism
.include "$S/isofs/Makefile.inc"		# ISO-9660 CDROM filesystem
.include "$S/mfs/Makefile.inc"			# BSD memory-based filesystem

.include "$S/config/kernel.mk"
