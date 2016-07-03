/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * %sccs.include.386.c%
 *
 *	%W% (Berkeley) %G%
 */

/*
 * AT/386
 * Vector interrupt control section
 * Copyright (C) 1989,90 W. Jolitz
 */

	.data
	.globl	___nonemask__
___nonemask__:	.long	0x7fff	# interrupt mask enable (all off to start)
	.globl	___highmask__
___highmask__:	.long	0xffff
	.globl	___ttymask__
___ttymask__:	.long	0
	.globl	___biomask__
___biomask__:	.long	0
	.globl	___netmask__
___netmask__:	.long	0
	.globl	___protomask__
___protomask__:	.long	0x8000
	.globl	_isa_intr
_isa_intr:	.space	16*4

	.text
/*
 * Handle return from interrupt after device handler finishes
 */
doreti:
	cli
	popl	%ebx			# remove intr number
	popl	%eax			# get previous priority
	# now interrupt frame is a trap frame!
	movw	%ax,%cx
	outb	%al,$ IO_ICU1+1		# re-enable intr?
	inb	$0x84,%al
	movb	%ah,%al
	outb	%al,$ IO_ICU2+1
	inb	$0x84,%al

	cmpw	___nonemask__,%cx	# returning to zero?
	je	1f

2:	popl	%es			# nope, going to non-zero level
	popl	%ds
	popal
	nop
	addl	$8,%esp
	iret

1:	cmpl	$0,_netisr		# check for softint s/traps
	je	2b

#include "../net/netisr.h"

1:

#define DONET(s, c)	; \
	.globl	c ;  \
	btrl	$ s ,_netisr ;  \
	jnb	1f ; \
	call	c ; \
1:

	SETPL(___protomask__)

	DONET(NETISR_RAW,_rawintr)
#ifdef INET
	DONET(NETISR_IP,_ipintr)
#endif
#ifdef IMP
	DONET(NETISR_IMP,_impintr)
#endif
#ifdef NS
	DONET(NETISR_NS,_nsintr)
#endif

	/* restore interrupt state, but don't turn them on just yet */
	btrl	$ NETISR_SCLK,_netisr
	jnb	1f
	# back to an interrupt frame for a moment
	pushl	___nonemask__
	pushl	$0	# dummy unit
	call	_softclock
	popl	%eax
	popl	%eax

1:
	SETPL(___nonemask__)
	cmpw	$0x1f,13*4(%esp)	# to user?
	jne	2f			# nope, leave
	DONET(NETISR_AST,_trap);

2:	pop	%es
	pop	%ds
	popal
	nop
	addl	$8,%esp
	iret

/*
 * Interrupt priority mechanism
 */
	.globl	_splhigh
	.globl	_splclock
_splhigh:
_splclock:
	ORPL(___highmask__)
	ret

	.globl	_spltty
_spltty:
	ORPL(___ttymask__)
	ret

	.globl	_splbio
_splbio:
	ORPL(___biomask__)
	ret

	.globl	_splimp
_splimp:
	ORPL(___netmask__)
	ret

	.globl	_splnet
_splnet:
	ORPL(___protomask__)
	ret

	.globl	_splsoftclock
_splsoftclock:
	ORPL(___protomask__)
	ret

	.globl _splnone
	.globl _spl0
_splnone:
_spl0:
	SETPL(___protomask__)
	DONET(NETISR_RAW,_rawintr)
#ifdef INET
	DONET(NETISR_IP,_ipintr)
#endif
#ifdef IMP
	DONET(NETISR_IMP,_impintr)
#endif
#ifdef NS
	DONET(NETISR_NS,_nsintr)
#endif
	SETPL(___nonemask__)
	ret

	.globl _splx
_splx:
	cli				# disable interrupts
	movw	4(%esp),%ax		# new priority level
	movw	%ax,%cx
	cmpw	___nonemask__,%cx
	je	_spl0			# going to "zero level" is special

	SETPL(%cx)
	ret

	/* hardware interrupt catcher (IDT 32 - 47) */
	.globl	_isa_strayintr

IDTVEC(intr0)
	INTR(0, ___highmask__, 0) ; call	_isa_strayintr ; INTREXIT1

IDTVEC(intr1)
	INTR(1, ___highmask__, 1) ; call	_isa_strayintr ; INTREXIT1

IDTVEC(intr2)
	INTR(2, ___highmask__, 2) ; call	_isa_strayintr ; INTREXIT1

IDTVEC(intr3)
	INTR(3, ___highmask__, 3) ; call	_isa_strayintr ; INTREXIT1

IDTVEC(intr4)
	INTR(4, ___highmask__, 4) ; call	_isa_strayintr ; INTREXIT1

IDTVEC(intr5)
	INTR(5, ___highmask__, 5) ; call	_isa_strayintr ; INTREXIT1

IDTVEC(intr6)
	INTR(6, ___highmask__, 6) ; call	_isa_strayintr ; INTREXIT1

IDTVEC(intr7)
	INTR(7, ___highmask__, 7) ; call	_isa_strayintr ; INTREXIT1


IDTVEC(intr8)
	INTR(8, ___highmask__, 8) ; call	_isa_strayintr ; INTREXIT2

IDTVEC(intr9)
	INTR(9, ___highmask__, 9) ; call	_isa_strayintr ; INTREXIT2

IDTVEC(intr10)
	INTR(10, ___highmask__, 10) ; call	_isa_strayintr ; INTREXIT2

IDTVEC(intr11)
	INTR(11, ___highmask__, 11) ; call	_isa_strayintr ; INTREXIT2

IDTVEC(intr12)
	INTR(12, ___highmask__, 12) ; call	_isa_strayintr ; INTREXIT2

IDTVEC(intr13)
	INTR(13, ___highmask__, 13) ; call	_isa_strayintr ; INTREXIT2

IDTVEC(intr14)
	INTR(14, ___highmask__, 14) ; call	_isa_strayintr ; INTREXIT2

IDTVEC(intr15)
	INTR(15, ___highmask__, 15) ; call	_isa_strayintr ; INTREXIT2

