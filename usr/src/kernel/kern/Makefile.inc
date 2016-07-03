#
# Makefile for the kernel of the kernel (kern) module. -wfj
#
# $Id: Makefile.inc,v 1.1 94/10/20 00:02:43 bill Exp $

.PATH:	$S/kern
KERN_SRCS+= \
	clock.c config.c cred.c descrip.c execve.c exit.c fork.c sysent.c \
	host.c kinfo.c lock.c main.c malloc.c physio.c priv.c \
	proc.c prof.c ptrace.c reboot.c resource.c sig.c synch.c time.c \
	ldisc.c

SYMBOLS+= kern.symbols
MAN9+=	malloc.0 # kinfo.0 io.0 priv.0 process.0 config.0 lock.0
#	host.0 time.0 memory.0
MAN5+=	core.0
MAN4+=	fd.0 # ldisc.0 kern.0

.include "$S/kern/fs/Makefile.inc"
.include "$S/kern/$(MACHINE)/Makefile.inc"
.include "$S/kern/net/Makefile.inc"
.include "$S/kern/socket/Makefile.inc"
.include "$S/kern/subr/Makefile.inc"
.include "$S/kern/vm/Makefile.inc"

# .include <bsd.kernel.mk>
