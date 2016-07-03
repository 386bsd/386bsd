#
# 386BSD operating system kernel for network host "odysseus".
#

KERNEL=	386bsd
IDENT=	-Di486 -DTCP_COMPAT_42 -DDIAGNOSTIC
#PARAM=	-DTIMEZONE=480 -DDST=1 -DMAXUSERS=4

# standard kernel with INTERNET protocols
.include "$S/config/config.std.mk"
.include "$S/config/config.inet.mk"

# additional options
.include "$S/ppp/Makefile.inc"			# point-to-point protocol
.include "$S/bpf/Makefile.inc"			# packet filter
#.include "$S/math/Makefile.inc"			# math emulator
#.include "$S/fpu-emu/Makefile.inc"			# math emulator
#.include "$S/mcd/Makefile.inc"			# mitsumi cdrom
#.include "$S/wt/Makefile.inc"			# wangtec cartridge tape
.include "$S/ed/Makefile.inc"			# NE2000 ethernet
.include "$S/ddb/Makefile.inc"			# kernel debugger
.include "$S/kern/opt/ktrace/Makefile.inc"	# BSD ktrace mechanism
.include "$S/isofs/Makefile.inc"		# ISO-9660 CDROM filesystem
.include "$S/dosfs/Makefile.inc"		# DOS FAT filesystem
.include "$S/mfs/Makefile.inc"			# BSD memory-based filesystem

.include "$S/config/kernel.mk"
