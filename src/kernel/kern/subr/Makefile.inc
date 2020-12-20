#
# Makefile for the kernel's subroutine library. -wfj
#
# $Id: Makefile.inc,v 1.1 94/10/19 18:33:10 bill Exp $

.PATH:	$S/kern/subr
KERN_SRCS+= \
	disklabel.c disksort.c esym.c nullop.c printf.c ring.c rlist.c uio.c

KERN_SRCS+= strcat.c strcpy.c strncpy.c strcmp.c # strlen.c
# XXX should be conditional.
MACH_SRCS_S+= \
	memmove_${MACHINE}.s scanc_${MACHINE}.s skpc_${MACHINE}.s locc_${MACHINE}.s
	
# check for possible inline, C, or assembler machine-dependant versions first.

.if exists(addupc_${MACHINE}.c)
MACH_SRCS+= addupc_${MACHINE}.c
.elif exists(addupc_${MACHINE}.s)
MACH_SRCS_S+= addupc_${MACHINE}.s
.else
KERN_SRCS+= addupc.c
.endif

# .if exists(../../include/${MACHINE}/inline/kernel/copyinstr.h)
# do nothing
# .elif exists(copyinstr_${MACHINE}.c)
# MACH_SRCS+= copyinstr_${MACHINE}.c
# .elif exists(copyinstr_${MACHINE}.s)
# MACH_SRCS_S+= copyinstr_${MACHINE}.s
# .else
# KERN_SRCS+= copyinstr.c
# .endif
# copyoutstr.c copystr.c ffs.c strcat.c strcpy.c strlen.c strncpy.c


ERROR=	ENXIO.o ENODEV.o EOPNOTSUPP.o ENOSYS.o ENOTTY.o

OBJS+=	${ERROR}

${ERROR}: $S/include/sys/errno.h
	@echo creating ${.PREFIX}.o
	@printf '#include "sys/errno.h"\n_${.PREFIX}_(){ return(${.PREFIX});}\n' \
	    > ${.PREFIX}.c
	@$(CC) -c ${.PREFIX}.c
	@rm ${.PREFIX}.c

# symbols
SYMBOLS += subr.symbols

MAN9+=	esym.0 rlist.0 # uio.0 printf.0 ringb.0
