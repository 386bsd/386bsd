/*
 * Machine dependent constants for Intel 386.
 */

#define MACHINE "i386"

#ifndef BYTE_ORDER
#include <machine/endian.h>
#endif

#include <machine/machlimits.h>

#define	NBPG		4096		/* bytes/page */
#define	PGOFSET		(NBPG-1)	/* byte offset into page */
#define	PGSHIFT		12		/* LOG2(NBPG) */
#define	NPTEPG		(NBPG/(sizeof (struct pte)))

#define NBPDR		(1024*NBPG)	/* bytes/page dir */
#define	PDROFSET	(NBPDR-1)	/* byte offset into page dir */
#define	PDRSHIFT	22		/* LOG2(NBPDR) */

#define	KERNBASE	0xFE000000	/* start of kernel virtual */
#define	BTOPKERNBASE	((u_long)KERNBASE >> PGSHIFT)

#define	DEV_BSIZE	512
#define	DEV_BSHIFT	9		/* log2(DEV_BSIZE) */
#define BLKDEV_IOSIZE	2048
#define	MAXPHYS		(64 * 1024)	/* max raw I/O transfer size */

#define	CLSIZE		1
#define	CLSIZELOG2	0

#define	SSIZE	1		/* initial stack size/NBPG */
#define	SINCR	1		/* increment of stack/NBPG */

#define	UPAGES	2		/* pages of u-area */

/*
 * Constants related to network buffer management.
 * MCLBYTES must be no larger than CLBYTES (the software page size), and,
 * on machines that exchange pages of input or output buffers with mbuf
 * clusters (MAPPED_MBUFS), MCLBYTES must also be an integral multiple
 * of the hardware page size.
 */
#define	MSIZE		128		/* size of an mbuf */
#define	MCLBYTES	1024
#define	MCLSHIFT	10
#define	MCLOFSET	(MCLBYTES - 1)
#ifndef NMBCLUSTERS
#ifdef GATEWAY
#define	NMBCLUSTERS	512		/* map size, max cluster allocation */
#else
#define	NMBCLUSTERS	256		/* map size, max cluster allocation */
#endif
#endif

/*
 * Size of kernel malloc arena in CLBYTES-sized logical pages
 */ 
#ifndef NKMEMCLUSTERS
#define	NKMEMCLUSTERS	(512*1024/CLBYTES)
#endif
/*
 * Some macros for units conversion
 */
/* Core clicks (4096 bytes) to segments and vice versa */
#define	ctos(x)	(x)
#define	stoc(x)	(x)

/* Core clicks (4096 bytes) to disk blocks */
#define	ctod(x)	((x)<<(PGSHIFT-DEV_BSHIFT))
#define	dtoc(x)	((x)>>(PGSHIFT-DEV_BSHIFT))
#define	dtob(x)	((x)<<DEV_BSHIFT)

/* clicks to bytes */
#define	ctob(x)	((x)<<PGSHIFT)

/* bytes to clicks */
#define	btoc(x)	(((unsigned)(x)+(NBPG-1))>>PGSHIFT)

#define	btodb(bytes)	 		/* calculates (bytes / DEV_BSIZE) */ \
	((unsigned)(bytes) >> DEV_BSHIFT)
#define	dbtob(db)			/* calculates (db * DEV_BSIZE) */ \
	((unsigned)(db) << DEV_BSHIFT)

/*
 * Map a ``block device block'' to a file system block.
 * This should be device dependent, and will be if we
 * add an entry to cdevsw/bdevsw for that purpose.
 * For now though just use DEV_BSIZE.
 */
#define	bdbtofsb(bn)	((bn) / (BLKDEV_IOSIZE/DEV_BSIZE))

#ifdef KERNEL
#ifndef LOCORE
int	cpuspeed;
#ifndef __ORPL__
/*
 * Interrupt Group Masks
 */
extern	u_short __highmask__;	/* interrupts masked with splhigh() */
extern	u_short __ttymask__;	/* interrupts masked with spltty() */
extern	u_short __biomask__;	/* interrupts masked with splbio() */
extern	u_short __netmask__;	/* interrupts masked with splimp() */
extern	u_short __protomask__;	/* interrupts masked with splnet() */
extern	u_short __nonemask__;	/* interrupts masked with splnone() */
asm("	.set IO_ICU1, 0x20 ; .set IO_ICU2, 0xa0 "); 

/* adjust priority level to disable a group of interrupts */
#define	__ORPL__(m)	({ u_short oldpl, msk; \
	msk = (msk); \
	asm volatile (" \
	cli ;				/* modify interrupts atomically */ \
	movw	%1, %%dx ; 		/* get mask to OR in */ \
	inb	$ IO_ICU1+1, %%al ;	/* get low order mask */ \
	xchgb	%%dl, %%al ; 		/* switch the old with the new */ \
	orb	%%dl, %%al ; 		/* finally, OR both it in! */ \
	outb	%%al, $ IO_ICU1+1 ; 	/* and stuff it back where it came */ \
	inb	$ 0x84, %%al ;	 	/* post it & handle write recovery */ \
	inb	$ IO_ICU2+1, %%al ;	/* next, get high order mask */ \
	xchgb	%%dh, %%al ; 		/* switch the old with the new */ \
	orb	%%dh, %%al ; 		/* finally, or it in! */ \
	outb	%%al, $ IO_ICU2+1 ; 	/* and stuff it back where it came */ \
	inb	$ 0x84, %%al ;	 	/* post it & handle write recovery */ \
	movw	%%dx, %0 ;		/* return old mask */ \
	sti				/* allow interrupts again */ " \
	: "&=g" (oldpl)			/* return values */ \
	: "g" ((m))			/* arguments */ \
	: "ax", "dx"			/* registers used */ \
	); \
	oldpl; 				/* return the "old" value */ \
})

/* force priority mask to a set value */
#define	__SETPL__(m)	({ u_short oldpl, msk; \
	msk = (msk); \
	asm volatile (" \
	cli ;				/* modify interrupts atomically */ \
	movw	%1, %%dx ; 		/* get mask to OR in */ \
	inb	$ IO_ICU1+1, %%al ;	/* get low order mask */ \
	xchgb	%%dl, %%al ; 		/* switch the old with the new */ \
	outb	%%al, $ IO_ICU1+1 ; 	/* and stuff it back where it came */ \
	inb	$ 0x84, %%al ;	 	/* post it & handle write recovery */ \
	inb	$ IO_ICU2+1, %%al ;	/* next, get high order mask */ \
	xchgb	%%dh, %%al ; 		/* switch the old with the new */ \
	outb	%%al, $ IO_ICU2+1 ; 	/* and stuff it back where it came */ \
	inb	$ 0x84, %%al ;	 	/* post it & handle write recovery */ \
	movw	%%dx, %0 ;		/* return old mask */ \
	sti				/* allow interrupts again */ " \
	: "&=g" (oldpl)			/* return values */ \
	: "g" ((m))			/* arguments */ \
	: "ax", "dx"			/* registers used */ \
	); \
	oldpl; 				/* return the "old" value */ \
})

#define	splhigh()	__ORPL__(__highmask__)
#define	spltty()	__ORPL__(__ttymask__)
#define	splbio()	__ORPL__(__biomask__)
#define	splimp()	__ORPL__(__netmask__)
#define	splnet()	__ORPL__(__protomask__)
#define	splsoftclock()	__ORPL__(__protomask__)

#define splx(v) ({ u_short val; \
	val = (v); \
	if (val == __nonemask__) (void) spl0();	/* zero is special */ \
	else (void) __SETPL__(val); \
})
#endif __ORPL__

#endif
#define	DELAY(n)	{ register int N = (n); while (--N > 0); }

#else KERNEL
#define	DELAY(n)	{ register int N = (n); while (--N > 0); }
#endif KERNEL
