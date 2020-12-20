/*
 * Copyright (c) 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: sel.h,v 1.1 94/10/19 17:40:07 bill Exp $
 *
 * 386BSD kernel segment selector assignments.
 */

/* GDT - global descriptor table */
#define	KCSEL		0x08	/* kernel text selector */
#define	KDSEL		0x10	/* kernel data selector */
#define	LDTSEL		0x18	/* user ldt selector XXX */

#define	PANICSEL        0x28	/* kernel panic selector XXX */
#define	PROC0SEL        0x30	/* process 0, tss selector */
				/* up to N processes */

/* LDT - local descriptor table */
#define	iBCSSYSCALLSEL	0x07	/* iBCS syscall selector */
#define	iBCSSIGRETSEL	0x0f	/* iBCS signal return selector */

#define	_386BSDCODE	0x1f	/* 386BSD user program code selector */
#define	_386BSDDATA	0x27	/* 386BSD user program data selector */
