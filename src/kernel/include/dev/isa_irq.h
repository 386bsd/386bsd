/*-
 * ISA Interrupt Request assignments.
 *
 * Copyright (C) 1989-1994, William F. Jolitz. All Rights Reserved.
 */

#ifndef	__ISA_ICU__
#define	__ISA_ICU__

/*
 * Interrupt enable bits -- in order of priority
 */
#define	IRQ0		0x00001		/* highest priority - timer */
#define	IRQ1		0x00002
#define	IRQ_SLAVE	0x00004
#define	IRQ8		0x00100
#define	IRQ9		0x00200
#define	IRQ2		IRQ9
#define	IRQ10		0x00400
#define	IRQ11		0x00800
#define	IRQ12		0x01000
#define	IRQ13		0x02000
#define	IRQ14		0x04000
#define	IRQ15		0x08000
#define	IRQ3		0x00008
#define	IRQ4		0x00010
#define	IRQ5		0x00020
#define	IRQ6		0x00040
#define	IRQ7		0x00080		/* lowest - parallel printer */

#define	IRNET		0x10000		/* software - only, network */
#define	IRSCLK		0x20000		/* software - only, softclock */

#define	IRHIGH		0x3ffff		/* all masked off */

/*
 * Interrupt Control offset into Interrupt descriptor table (IDT)
 * (should be moved to nonstd/pc/irq XXX)
 */
#define	ICU_OFFSET	32		/* 0-31 are processor exceptions */
#define	ICU_LEN		16		/* 32-47 are ISA interrupts */

#endif	/* __ISA_ICU__ */
