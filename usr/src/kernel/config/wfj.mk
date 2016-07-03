#
# 386BSD operating system kernel for all of wfj's local hosts.
#

KERNEL=	386bsd
IDENT=	-Di486

# standard kernel with INTERNET protocols
.include "$S/config/config.std.mk"		# standard configuration
.include "$S/config/config.inet.mk"		# INTERNET

# additional options
.include "$S/kern/opt/ktrace/Makefile.inc"	# BSD ktrace mechanism
.include "$S/fpu-emu/Makefile.inc"		# WM math emulator
.include "$S/ddb/Makefile.inc"			# kernel debugger
.include "$S/as/Makefile.inc"			# adaptec SCSI
.include "$S/mcd/Makefile.inc"			# mitsumi cdrom
.include "$S/wt/Makefile.inc"			# wangtec cartridge tape
.include "$S/ed/Makefile.inc"			# NS/WD/SMC/3COM ethernet
.include "$S/isofs/Makefile.inc"		# ISO-9660 CDROM filesystem
.include "$S/dosfs/Makefile.inc"		# MS/DOS FAT filesystem
.include "$S/mfs/Makefile.inc"			# BSD memory-based filesystem

.include "$S/config/kernel.mk"			# makefile boilerplate
