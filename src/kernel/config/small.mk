#
# "Small" 386BSD operating system kernel for testing bootstraps.
#

KERNEL=	386bsd
HOST=	small
IDENT=	-DODYSSEUS -Di486 -DSHMMAXPGS=100 -DTCP_COMPAT_42 -D__NO_INLINES
PARAM=	-DTIMEZONE=480 -DDST=1 -DMAXUSERS=4
S=	../..

# minimal kernel
.include "$S/kern/Makefile.inc"
.include "$S/domain/un/Makefile.inc"

# debugger
.include "$S/ddb/Makefile.inc"

# standard pseudo devices
.include "$S/dev/pseudo/mem/Makefile.inc"
.include "$S/dev/pseudo/log/Makefile.inc"
.include "$S/dev/pseudo/cons/Makefile.inc"
.include "$S/dev/pseudo/devtty/Makefile.inc"

# standard filesystems
.include "$S/fs/ufs/Makefile.inc"

.include "$S/dev/isa/isa/Makefile.inc"

.include "$S/dev/isa/clock/Makefile.inc"
.include "$S/dev/isa/pccons/Makefile.inc"
.include "$S/dev/isa/wd/Makefile.inc"
.include "$S/dev/isa/npx/Makefile.inc"

.include "$S/conf/kernel.mk"
