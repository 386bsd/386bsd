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
 * AT/386 Interrupt Control constants
 * W. Jolitz 8/89
 */

#ifndef	__ICU__
#define	__ICU__

#ifndef	LOCORE

/*
 * Interrupt "level" mechanism variables, masks, and macros
 */

#define	INTREN(s)	__nonemask__ &= ~(s)
#define	INTRDIS(s)	__nonemask__ |= (s)
#define	INTRMASK(msk,s)	msk |= (s)

#else

/*
 * Macro's for interrupt level priority masks (used in interrupt vector entry)
 */

/* Mask a group of interrupts atomically */
#define	INTR(unit,mask,offst) \
	pushl	$0 ; \
	pushl	$ T_ASTFLT ; \
	pushal ; \
	nop ; \
	movb	$0x20, %al ; \
	outb	%al, $ IO_ICU1 ; \
	inb	$0x84, %al ; \
	movb	$0x20, %al ; \
	outb	%al,$ IO_ICU2 ; \
	inb	$0x84,%al ; \
	pushl	%ds ; \
	pushl	%es ; \
	movw	$0x10, %ax ; \
	movw	%ax, %ds ; \
	movw	%ax, %es ; \
	incl	_cnt+V_INTR ; \
	incl	_isa_intr + offst * 4 ; \
	movw	mask , %dx ; \
	inb	$ IO_ICU1+1, %al ;	/* next, get low order mask */ \
	xchgb	%dl, %al ; 		/* switch the old with the new */ \
	orb	%dl, %al ; 		/* finally, or it in! */ \
	outb	%al, $ IO_ICU1+1 ; \
	inb	$0x84,%al ; \
	inb	$ IO_ICU2+1, %al ;	/* next, get high order mask */ \
	xchgb	%dh, %al ; 		/* switch the old with the new */ \
	orb	%dh, %al ; 		/* finally, or it in! */ \
	outb	%al, $ IO_ICU2+1	; \
	inb	$0x84, %al ; \
	pushl	%edx ; \
	pushl	$ unit ; \
	sti ;

/* Interrupt vector exit macros */

/* First eight interrupts (ICU1) */
#define	INTREXIT1	\
	jmp	doreti

/* Second eight interrupts (ICU2) */
#define	INTREXIT2	\
	jmp	doreti

#define	WRITEPL()	\
	outb	%al, $ IO_ICU1+1 ;	/* update primary icu */ \
	inb	$0x84, %al ;	/* flush write buffer, delay bus cycle */ \
	movb	%ah, %al ; \
	outb	%al, $ IO_ICU2+1 ;	/* update secondary icu */ \
	inb	$0x84, %al ;	/* flush write buffer, delay bus cycle */ \

#define	ORPL(m)	\
	cli ;				/* disable interrupts */ \
	movw	m , %dx ; 		/* get the mask */ \
	inb	$ IO_ICU1+1, %al ;	/* next, get low order mask */ \
	xchgb	%dl, %al ; 		/* switch the old with the new */ \
	orb	%dl, %al ; 		/* finally, or it in! */ \
	outb	%al, $ IO_ICU1+1 ; \
	inb	$0x84, %al ; \
	inb	$ IO_ICU2+1, %al ;	/* next, get high order mask */ \
	xchgb	%dh, %al ; 		/* switch the old with the new */ \
	orb	%dh, %al ; 		/* finally, or it in! */ \
	outb	%al, $ IO_ICU2+1	; \
	inb	$0x84, %al ;	/* flush write buffer, delay bus cycle */ \
	movzwl	%dx, %eax ;		/* return old priority */ \
	sti ;				/* enable interrupts */

#define	SETPL(v)	\
	cli ;				/* disable interrupts */ \
	movw	v , %dx ; \
	inb	$ IO_ICU1+1, %al ;	/* next, get low order mask */ \
	xchgb	%dl, %al ; 		/* switch the old with the new */ \
	outb	%al, $ IO_ICU1+1 ; \
	inb	$0x84, %al ; \
	inb	$ IO_ICU2+1, %al ;	/* next, get high order mask */ \
	xchgb	%dh, %al ; 		/* switch the copy with the new */ \
	outb	%al, $ IO_ICU2+1	; \
	inb	$0x84, %al ;	/* flush write buffer, delay bus cycle */ \
	movzwl	%dx, %eax ;		/* return old priority */ \
	sti ;				/* enable interrupts */

#endif

/*
 * Interrupt enable bits -- in order of priority
 */
#define	IRQ0		0x0001		/* highest priority - timer */
#define	IRQ1		0x0002
#define	IRQ_SLAVE	0x0004
#define	IRQ8		0x0100
#define	IRQ9		0x0200
#define	IRQ2		IRQ9
#define	IRQ10		0x0400
#define	IRQ11		0x0800
#define	IRQ12		0x1000
#define	IRQ13		0x2000
#define	IRQ14		0x4000
#define	IRQ15		0x8000
#define	IRQ3		0x0008
#define	IRQ4		0x0010
#define	IRQ5		0x0020
#define	IRQ6		0x0040
#define	IRQ7		0x0080		/* lowest - parallel printer */

/*
 * Interrupt Control offset into Interrupt descriptor table (IDT)
 */
#define	ICU_OFFSET	32		/* 0-31 are processor exceptions */
#define	ICU_LEN		16		/* 32-47 are ISA interrupts */

#endif	__ICU__
