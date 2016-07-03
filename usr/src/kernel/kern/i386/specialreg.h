/*
 * Copyright (c) 1989, 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: specialreg.h,v 1.1 94/10/19 17:40:11 bill Exp $
 * Assembly macros.
 */

/*
 * 386 Special registers:
 */

#define	CR0_PE	0x00000001	/* Protected mode Enable */
#define	CR0_MP	0x00000002	/* "Math" Present (e.g. npx), wait for it */
#define	CR0_EM	0x00000004	/* EMulate NPX, e.g. trap, don't execute code */
#define	CR0_TS	0x00000008	/* Process has done Task Switch, do NPX save */
#define	CR0_ET	0x00000010	/* 32 bit (if set) vs 16 bit (387 vs 287) */
#define	CR0_WP	0x00010000	/* (486) write protection enabled for all modes */
#define	CR0_AM	0x00040000	/* (486) allow alignment faults */
#define	CR0_CD	0x40000000	/* (486) lock cache contents */
#define	CR0_PG	0x80000000	/* Paging Enable */

#define rcr(ri) ({						\
	register int rv;					\
								\
	asm volatile ("movl %%cr" # ri ",%0 " : "=r" (rv));	\
	rv;							\
})

#define lcr(ri, s) ({						\
	register int arg = (int) (s) ;				\
								\
	asm volatile ("movl %0,%%cr" # ri : : "r" (arg));	\
})
