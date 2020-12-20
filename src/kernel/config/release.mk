#
# 386BSD operating system kernel for final release
#

KERNEL=	386bsd
IDENT+= -DTCP_COMPAT_42

# standard kernel with INTERNET protocols
.include "$S/config/config.std.mk"
.include "$S/config/config.inet.mk"

# additional options
.include "$S/fpu-emu/Makefile.inc"
.include "$S/as/Makefile.inc"			# Adaptec 1540 SCSI
.include "$S/mcd/Makefile.inc"			# Mitsumi CDROM
.include "$S/ed/Makefile.inc"			# NE2000/SMC ethernet
.include "$S/kern/opt/ktrace/Makefile.inc"	# BSD ktrace mechanism
.include "$S/isofs/Makefile.inc"		# ISO-9660 CDROM filesystem
.include "$S/mfs/Makefile.inc"			# BSD memory-based filesystem
.include "$S/dosfs/Makefile.inc"		# MS DOS FAT filesystem

.include "$S/config/kernel.mk"
