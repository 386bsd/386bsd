/* interface symbols */
#define	__ISYM_VERSION__ "1"	/* XXX RCS major revision number of hdr file */
#include "isym.h"		/* this header has interface symbols */

/* global variables used in core kernel and other modules */
__ISYM__(int, netmask,)		/* group of interrupts masked with splimp() */

/* functions used in core kernel and modules */
__ISYM__(int, splx, (int))
__ISYM__(int, splimp, (void))

/*
 * Interrupt "level" mechanism variables, masks, and macros
 */
extern	unsigned short	imen;	/* interrupt mask enable */
extern int	cpl;	/* current priority level mask */

extern	int highmask; /* group of interrupts masked with splhigh() */
extern	int ttymask; /* group of interrupts masked with spltty() */
extern	int biomask; /* group of interrupts masked with splbio() */

#define	INTREN(s)	imen &= ~(s)
#define	INTRDIS(s)	imen |= (s)
#define	INTRMASK(msk,s)	msk |= (s)

#undef __ISYM__
#undef __ISYM_ALIAS__
#undef __ISYM_VERSION__
