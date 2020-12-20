#
# Makefile for the kernel's virtual memory subsystem. -wfj
# $Id: Makefile.inc,v 1.1 94/10/19 17:37:06 bill Exp $
#

.PATH:	$S/kern/vm
KERN_SRCS+= \
	dev_pgr.c fault.c kmem.c map.c mmap.c object.c page.c pageout.c \
	pager.c swap.c swap_pgr.c vm_meter.c vm_sys.c vnod_pgr.c

SYMBOLS+= vm.symbols
MAN9+=	kmem.0 vmspace.0
MAN4+=	drum.0
