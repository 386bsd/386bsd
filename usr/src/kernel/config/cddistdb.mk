#
# 386BSD operating system kernel for CD release testing
#

KERNEL=	386bsd
IDENT+= -DTCP_COMPAT_42
#PARAM=	-DTIMEZONE=480 -DDST=1 -DMAXUSERS=4

# standard kernel with INTERNET protocols
.include "$S/config/config.std.mk"
#.include "$S/config/config.inet.mk"

# additional options
.include "$S/fpu-emu/Makefile.inc"
#.include "$S/as/Makefile.inc"			# mitsumi cdrom
.include "$S/mcd/Makefile.inc"			# Adaptec 154X
#.include "$S/ed/Makefile.inc"			# NE2000 ethernet
#.include "$S/ddb/Makefile.inc"			# kernel debugger
.include "$S/kern/opt/ktrace/Makefile.inc"	# BSD ktrace mechanism
.include "$S/isofs/Makefile.inc"		# ISO-9660 CDROM filesystem
.include "$S/mfs/Makefile.inc"			# BSD memory-based filesystem

.include "$S/config/kernel.mk"
