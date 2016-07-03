/*
 * Interrupt "level" mechanism variables, masks, and macros
 */
extern	unsigned short	imen;	/* interrupt mask enable */
extern int	cpl;	/* current priority level mask */

extern	int highmask; /* group of interrupts masked with splhigh() */
extern	int ttymask; /* group of interrupts masked with spltty() */
extern	int biomask; /* group of interrupts masked with splbio() */
extern	int netmask; /* group of interrupts masked with splimp() */

#define	INTREN(s)	imen &= ~(s)
#define	INTRDIS(s)	imen |= (s)
#define	INTRMASK(msk,s)	msk |= (s)
