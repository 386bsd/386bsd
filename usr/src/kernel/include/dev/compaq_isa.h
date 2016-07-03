/*-
 * Compaq ISA bus extensions.
 *
 *	$Id$
 *
 * Copyright (C) 1989-1994, William F. Jolitz. All Rights Reserved.
 */

/*
 * Input / Output Port Assignments
 */
#ifndef IO_TIMER2
#define IO_TIMER2	0x048		/* 8252 Timer #2 */
#endif	/* IO_TIMER2 */

/*
 * Physical Memory assignments
 */
#ifndef	COMPAQ_RAMRELOC
#define	COMPAQ_RAMRELOC	0x80c00000	/* Compaq RAM relocation/diag */
#define	COMPAQ_RAMSETUP	0x80c00002	/* Compaq RAM setup */
#define	WEITEK_FPU	0xC0000000	/* WTL 2167 */
#define	CYRIX_EMC	0xC0000000	/* Cyrix EMC */
#endif	/* COMPAQ_RAMRELOC */
