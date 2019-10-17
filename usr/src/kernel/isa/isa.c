/*
 * Copyright (c) 1989, 1990, 1991, 1992, 1994 William F. Jolitz, TeleMuse
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This software is a component of "386BSD" developed by 
 *	William F. Jolitz, TeleMuse.
 * 4. Neither the name of the developer nor the name "386BSD"
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 5. Non-commercial distribution of this complete file in either source and/or
 *    binary form at no charge to the user (such as from an official Internet 
 *    archive site) is permitted. 
 * 6. Commercial distribution and sale of this complete file in either source
 *    and/or binary form on any media, including that of floppies, tape, or 
 *    CD-ROM, or through a per-charge download such as that of a BBS, is not 
 *    permitted without specific prior written permission.
 * 7. Non-commercial and/or commercial distribution of an incomplete, altered, 
 *    or otherwise modified file in either source and/or binary form is not 
 *    permitted.
 *
 * THIS SOFTWARE IS A COMPONENT OF 386BSD DEVELOPED BY WILLIAM F. JOLITZ 
 * AND IS INTENDED FOR RESEARCH AND EDUCATIONAL PURPOSES ONLY. THIS 
 * SOFTWARE SHOULD NOT BE CONSIDERED TO BE A COMMERCIAL PRODUCT. 
 * THE DEVELOPER URGES THAT USERS WHO REQUIRE A COMMERCIAL PRODUCT 
 * NOT MAKE USE OF THIS WORK.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPER ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPER BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: isa.c,v 1.1 94/06/24 18:49:31 bill Exp Locker: bill $
 *
 * ISA Bus adapter.
 */
static char *isa_config =
	"isa	32.	# bus device (/dev/isa) $Revision$";

#include "sys/param.h"
#include "sys/errno.h"
#include "sys/time.h"
#include "sys/file.h"
#include "sys/syslog.h"
#include "kernel.h"	/* hz */
#include "proc.h"
#include "buf.h"
#include "uio.h"
#include "malloc.h"
#include "rlist.h"
#include "modconfig.h"
#include "prototypes.h"

#include "vm.h"

#include "machine/cpu.h"
#include "machine/pcb.h"
#include "machine/psl.h"


#include "isa_driver.h"
#include "isa_stdports.h"
#include "isa_irq.h"
#include "isa_mem.h"
#include "machine/icu.h"
#include "i8237.h"
#include "i8042.h"


#include "machine/inline/io.h"

int config_isadev(struct isa_device *);
u_short getit(int unit, int timer);

#define	IDTVEC(name)	__CONCAT(X,name)
/* assignable interrupt vector table entries */
extern	IDTVEC(irq0), IDTVEC(irq1), IDTVEC(irq2), IDTVEC(irq3),
	IDTVEC(irq4), IDTVEC(irq5), IDTVEC(irq6), IDTVEC(irq7),
	IDTVEC(irq8), IDTVEC(irq9), IDTVEC(irq10), IDTVEC(irq11),
	IDTVEC(irq12), IDTVEC(irq13), IDTVEC(irq14), IDTVEC(irq15);

static *irqvec[16] = {
	&IDTVEC(irq0), &IDTVEC(irq1), &IDTVEC(irq2), &IDTVEC(irq3),
	&IDTVEC(irq4), &IDTVEC(irq5), &IDTVEC(irq6), &IDTVEC(irq7),
	&IDTVEC(irq8), &IDTVEC(irq9), &IDTVEC(irq10), &IDTVEC(irq11),
	&IDTVEC(irq12), &IDTVEC(irq13), &IDTVEC(irq14), &IDTVEC(irq15) };

struct isa_driver *driver_for_intr[16];
u_short isa_mask[16];
void	(*isa_vec[16])();
int	isa_unit[16];
extern	char *intrnames[16];

unsigned volatile it_ticks;
unsigned it_ticksperintr;
int loops_per_usec;

/*
 * Unfinished:
 *
 * Bus discovery causes motherboard low-level configuration. Drivers
 * are discovered by kernel, which do depth-first resource discovery and
 * low-level configuration, signalling changes to the higher levels. On
 * arrival to higher-level kernel layer, message is discovered and
 * configuration is reconciled (top down) via symbolic call to registered
 * bus configuration function(s).
 *
 * Bus drivers provide services via registered interfaces to the kernel,
 * and to drivers via symbolic calls only. Bus resources must be recorded
 * as being assigned to the arc linking the bus driver to a ordinary driver
 * so that if either is unloaded/reloaded, resources can be reclaimed/reconciled
 * in a stateless manner (e.g. if still referenced, the attempt to unload
 * is followed by an immediate reload of the same driver -- resources are
 * independant of the driver instance/implementation itself).
 */

BUS_MODCONFIG() {

	/*printf("isa: "); */
	/*isa_configure(init++) */
}

/*
 * Configure all ISA devices
 */
isa_configure()
{
	struct isa_device *dvp;
	struct isa_driver *dp;
	register cnt;
	int tick, x;

	/*
	 * Configure bus interrupts with processor
	 */
	isa_defaultirq();
	splhigh();	/* XXX */

	/*
	 * Configure motherboard
	 */

	/* initialize 8253 clock */
	it_ticksperintr = 1193182/hz;
	outb (IO_TIMER1+3, 0x34);
	outb (IO_TIMER1, it_ticksperintr);
	outb (IO_TIMER1, it_ticksperintr/256);
	
	/*
	 * Find out how many loops we need to DELAY() a microsecond
	 */

	/* wait till overflow to insure no wrap around */
	do
		tick = getit(0,0);
	while (tick < getit(0,0));

	/* time a while loop */
	cnt = 1000;
	tick = getit(0,0);
	while (cnt-- > 0)
		;
	tick -= getit(0,0);

	/* scale to microseconds per 1000 "loops" */
	tick *= 1000000;
	tick /= 1193182;
	loops_per_usec = (10000/tick + 5) / 10;

	/*
	 * Configure ISA devices. XXX
	 */
	splhigh();
	INTREN(IRQ_SLAVE);
	modscaninit(MODT_DRIVER);

	/*
	 * Finish configuration of ISA devices.
	 */
	/*ttymask |= biomask + netmask;
	netmask |= biomask;
	 biomask |= ttymask ;  can some tty devices use buffers? */
	printf("biomask %x ttymask %x netmask %x\n", biomask, ttymask, netmask);

	/* splat masks into place */
	for (x = 0; x < 16; x++)
	{
		if (driver_for_intr[x] && driver_for_intr[x]->mask)
			isa_mask[x] = *(driver_for_intr[x]->mask);
		if (intrnames[x] == 0)
			intrnames[x] = "** unassigned **";
	}
	splnone();
}

/* parse and evaluate an ISA device */
cfg_isadev(char **ptr, char *modname, struct isa_device *idp) {
	char *lp = *ptr;
	int val;


	/* default fields */
	idp->id_iobase = 0;
	idp->id_irq = 0;
	idp->id_drq = -1; 
	idp->id_maddr= (caddr_t) 0;
	idp->id_msize = 0;
	idp->id_unit = '?';
	idp->id_flags = 0;

	/* list of values or '?' */
	if (cfg_char(&lp, '(') && cfg_number(&lp, &val)) {
		/* specify all or partial list (if device can guess rest) */
		idp->id_iobase = val;

		if (cfg_number(&lp, &val)) {	/* XXX use "," to spec multiples */
			if (val >= 0)
				idp->id_irq = 1 << val;
		}

		if (cfg_number(&lp, &val))
			idp->id_drq = val;

		if (cfg_number(&lp, &val))
			idp->id_maddr = (caddr_t)val;

		if (cfg_number(&lp, &val))
			idp->id_msize = val;

		if (cfg_number(&lp, &val))
			idp->id_unit = val;

		if (cfg_number(&lp, &val))
			idp->id_flags = val;

		if (cfg_char(&lp, ')')) {
			*ptr = lp;
			return (1);
		} 
		goto nope;
	
	}
	else if (cfg_char(&lp, '?')) {
		/* driver attempts to do everything for device */
		idp->id_iobase = '?';
		*ptr = lp;
		return (1);
	}
nope:
	if (modname)
		printf("module %s: cannot find ISA port spec\n", modname);

	return (0);
}

#ifdef notyet
/* print a isa dev entry */
void
print_isadev(struct isa_device *idp) {

	if (idp->id_iobase) {
		printf ("port: ");
		if (idp->id_iobase == '?')
			printf("? ");
		else
			printf("%x ", idp->id_iobase);
	}
	if (idp->id_irq) /* XXX iterate over bitfield */
		printf ("irq%d ", ffs(idp->id_irq)-1);
	if (idp->id_drq != -1)
		printf ("drq%d ",  idp->id_drq);
	if (idp->id_maddr)
		printf ("maddr: %x + %d ",
			idp->id_maddr, idp->id_msize);
	printf ("unit: ");
	if (idp->id_unit == '?')
		printf("? ");
	else
		printf("%d ", idp->id_unit);
		
	/* if (idp->id_flags)
		printf("flags: %x ", idp->id_flags);*/
}
#endif

void
new_isa_configure(char **lp, struct isa_driver *dp) {
	struct isa_device adev ; 

	adev.id_driver = dp;
	for(;;) {
		if (cfg_isadev(lp, 0, &adev)) {
			/*if ( */ (void)config_isadev(&adev) /* )
				print_isadev(&adev)  */ ;
				/* attach */
		} else break;
	}
		
	/*printf("\n");*/
}

/*
 * Configure an ISA device.
 */
config_isadev(isdp)
	struct isa_device *isdp;
{
	struct isa_driver *dp;
 
	if (dp = isdp->id_driver) {
		if (isdp->id_maddr) {
			extern u_int atdevbase;

			isdp->id_maddr -= 0xa0000;
			isdp->id_maddr += atdevbase;
		}
printf("probing for %s port %x: ", dp->name, isdp->id_iobase);
		isdp->id_alive = (*dp->probe)(isdp);
		if (isdp->id_alive) {
printf("\r                                                                  \r");
			printf("%s: ", dp->name);
			(*dp->attach)(isdp);
			printf(" ");
			if (isdp->id_iobase)
				printf("port %x ", isdp->id_iobase);
			/* XXX handle multiple interrupts */
			if(isdp->id_irq) {
				int intrno;

				intrno = ffs(isdp->id_irq)-1;
				printf("irq%d ", intrno);
				INTREN(isdp->id_irq);

				if (dp->mask)
/*{
printf("mask before %x", *(dp->mask));*/
					INTRMASK(*(dp->mask), isdp->id_irq);
/*printf(" after %x ",  *(dp->mask));
} */

				setirq(ICU_OFFSET + intrno, irqvec [intrno]);
				isa_vec[intrno] = dp->intr;
				isa_unit[intrno] = isdp->id_unit;

				driver_for_intr[intrno] = dp;
				intrnames[intrno] = dp->name; /* XXX - allocate seperate string with unit number */
			}
			if (isdp->id_drq != -1)
				printf("drq%d ", isdp->id_drq);
			printf("\n");
		}
		return (1);
	} else {
		DELAY(5000);
		printf("\r                                                       \r");
		return(0);
	}
}

#define	IDTVEC(name)	__CONCAT(X,name)
/* default interrupt vector table entries */
extern	IDTVEC(intr0), IDTVEC(intr1), IDTVEC(intr2), IDTVEC(intr3),
	IDTVEC(intr4), IDTVEC(intr5), IDTVEC(intr6), IDTVEC(intr7),
	IDTVEC(intr8), IDTVEC(intr9), IDTVEC(intr10), IDTVEC(intr11),
	IDTVEC(intr12), IDTVEC(intr13), IDTVEC(intr14), IDTVEC(intr15);

static *defvec[16] = {
	&IDTVEC(intr0), &IDTVEC(intr1), &IDTVEC(intr2), &IDTVEC(intr3),
	&IDTVEC(intr4), &IDTVEC(intr5), &IDTVEC(intr6), &IDTVEC(intr7),
	&IDTVEC(intr8), &IDTVEC(intr9), &IDTVEC(intr10), &IDTVEC(intr11),
	&IDTVEC(intr12), &IDTVEC(intr13), &IDTVEC(intr14), &IDTVEC(intr15) };

/* out of range default interrupt vector gate entry */
extern	IDTVEC(intrdefault);
	
/*
 * Fill in default interrupt table (in case of spuruious interrupt
 * during configuration of kernel, setup interrupt control unit
 */
isa_defaultirq() {
	int i;

	/* icu vectors */
#define	NIDT	256
#define	NRSVIDT	32		/* reserved entries for cpu exceptions */
	for (i = NRSVIDT ; i < NRSVIDT+ICU_LEN ; i++)
		setirq(i, defvec[i]);

	/* out of range vectors */
	for (i = NRSVIDT; i < NIDT; i++)
		setirq(i, &IDTVEC(intrdefault));

asm("cli");
	/* clear npx intr latch */
	outb(0xf1,0);

	/* initialize 8259's */
	outb(IO_ICU1+1, 0xff);		/* mask interrupts */
	__outb(IO_ICU1, 0x11);		/* reset; program device, four bytes */
	__outb(IO_ICU1+1, NRSVIDT);	/* starting at this vector index */
	__outb(IO_ICU1+1, 1<<2);	/* slave on line 2 */
	__outb(IO_ICU1+1, 1);		/* 8086 mode, requires EOI */
	/*__outb(IO_ICU1+1, 0xff);	/* leave interrupts masked */
	/*__outb(IO_ICU1, ?*0x60+*?8+1);/* default to ISR on read */
	/*__outb(IO_ICU1, 2);*/

	outb(IO_ICU2+1, 0xff);		/* mask interrupts */
	__outb(IO_ICU2, 0x11);		/* reset; program device, four bytes */
	__outb(IO_ICU2+1, NRSVIDT+8);	/* staring at this vector index */
	__outb(IO_ICU2+1,2);		/* my slave id is 2 */
	__outb(IO_ICU2+1,1);		/* 8086 mode, requires EOI */
	/*__outb(IO_ICU2+1, 0xff);	/* leave interrupts masked */
	/*__outb(IO_ICU2, ?*0x60+*?8+1);/* default to ISR on read */
	/*__outb(IO_ICU2, 2);*/
}
extern struct isa_driver wddriver;

int reset_8259 = 1;
int restore_8259_cmd = 1;
init_8259() {
	if (reset_8259) {
	/* initialize 8259's */
	__outb(IO_ICU1, 0x11);		/* reset; program device, four bytes */
	__outb(IO_ICU1+1, NRSVIDT);	/* starting at this vector index */
	__outb(IO_ICU1+1, 1<<2);		/* slave on line 2 */
	__outb(IO_ICU1+1, 1);		/* 8086 mode */
	__outb(IO_ICU1+1, 0xff);		/* leave interrupts masked */
	}
	if(restore_8259_cmd) {
	/*__outb(IO_ICU1, ?*0x60+*?8+1);		/* default to ISR on read */
	__outb(IO_ICU1, 2);
	}

	if (reset_8259) {
	__outb(IO_ICU2, 0x11);		/* reset; program device, four bytes */
	__outb(IO_ICU2+1, NRSVIDT+8);	/* staring at this vector index */
	__outb(IO_ICU2+1,2);		/* my slave id is 2 */
	__outb(IO_ICU2+1,1);		/* 8086 mode */
	__outb(IO_ICU2+1, 0xff);		/* leave interrupts masked */
	}
	if(restore_8259_cmd) {
	/*__outb(IO_ICU2, ?*0x60+*?8+1);		/* default to ISR on read */
	__outb(IO_ICU2, 2);
	}
	wdredo();
}

static int wdredocnt = 100;

wdredo() {
	int x;

	if (--wdredocnt <= 0)  {
		x = splbio();
		(*wddriver.intr)(0);
		splx(x);
		wdredocnt = 100;
	}
}
/*
 * XXX unfinished, missing new channel code and pvc_alloc()
 */

/* region of physical memory known to be contiguous */
vm_offset_t isaphysmem;
static caddr_t dma_bounce[8];		/* XXX */
static char bounced[8];		/* XXX */
int dma_active;
#define MAXDMASZ 512		/* XXX */

/* high byte of address is stored in this port for i-th dma channel */
static short dmapageport[8] =
	{ 0x87, 0x83, 0x81, 0x82, 0x8f, 0x8b, 0x89, 0x8a };

/*
 * isa_dmacascade(): program 8237 DMA controller channel to accept
 * external dma control by a board.
 * N.B. drivers must manage "dma_active" manually.
 */
void isa_dmacascade(unsigned chan)
{	int modeport;

	if (chan > 7)
		panic("isa_dmacascade: impossible request"); 

	/* set dma channel mode, and set dma channel mode */
	if ((chan & 4) == 0)
		modeport = IO_DMA1 + 0xb;
	else
		modeport = IO_DMA2 + 0x16;
	outb(modeport, DMA37MD_CASCADE | (chan & 3));
	if ((chan & 4) == 0)
		outb(modeport - 1, chan & 3);
	else
		outb(modeport - 2, chan & 3);
}

/*
 * isa_dmastart(): program 8237 DMA controller channel, avoid page alignment
 * problems by using a bounce buffer.
 */
void isa_dmastart(int flags, caddr_t addr, unsigned nbytes, unsigned chan)
{	vm_offset_t phys;
	int modeport, waport, mskport;
	caddr_t newaddr;

	if (chan > 7 || nbytes > (1<<16))
		panic("isa_dmastart: impossible request"); 

	if (isa_dmarangecheck(addr, nbytes)) {
		if (dma_bounce[chan] == 0)
			dma_bounce[chan] =
				/*(caddr_t)malloc(MAXDMASZ, M_TEMP, M_WAITOK);*/
				(caddr_t) isaphysmem + NBPG*chan;
		bounced[chan] = 1;
		newaddr = dma_bounce[chan];
		*(int *) newaddr = 0;	/* XXX */

		/* copy bounce buffer on write */
		if (!(flags & B_READ))
			memcpy(newaddr, addr, nbytes);
		addr = newaddr;
	}

	/* translate to physical */
	phys = pmap_extract(pmap_kernel(), (vm_offset_t)addr);
/*printf("dma virt %x phys %x word %x", addr, phys, *(long *) addr);*/

	/* set dma channel mode, and reset address ff */
	if ((chan & 4) == 0)
		modeport = IO_DMA1 + 0xb;
	else
		modeport = IO_DMA2 + 0x16;
	if (flags & B_READ)
		outb(modeport, DMA37MD_SINGLE|DMA37MD_WRITE|(chan&3));
	else
		outb(modeport, DMA37MD_SINGLE|DMA37MD_READ|(chan&3));
	if ((chan & 4) == 0)
		outb(modeport + 1, 0);
	else
		outb(modeport + 2, 0);

	/* send start address */
	if ((chan & 4) == 0) {
		waport =  IO_DMA1 + (chan<<1);
		outb(waport, phys);
		outb(waport, phys>>8);
	} else {
		waport =  IO_DMA2 + ((chan - 4)<<2);
		outb(waport, phys>>1);
		outb(waport, phys>>9);
	}
	outb(dmapageport[chan], phys>>16);

	/* send count */
	if ((chan & 4) == 0) {
		outb(waport + 1, --nbytes);
		outb(waport + 1, nbytes>>8);
	} else {
		nbytes >>= 1;
		outb(waport + 2, --nbytes);
		outb(waport + 2, nbytes>>8);
	}

	/* unmask channel */
	if ((chan & 4) == 0)
		mskport =  IO_DMA1 + 0x0a;
	else
		mskport =  IO_DMA2 + 0x14;
	outb(mskport, chan & 3);
	dma_active |= 1 << chan;
}

void isa_dmadone(int flags, caddr_t addr, int nbytes, int chan)
{

/*printf(" addr %x word %x ", addr, *(long *) addr);*/
	/* copy bounce buffer on read */
	/*if ((flags & (B_PHYS|B_READ)) == (B_PHYS|B_READ))*/
	if (bounced[chan]) {
		memcpy(addr, dma_bounce[chan], nbytes);
		bounced[chan] = 0;
	}
	dma_active &= ~(1 << chan);
}

/*
 * Check for problems with the address range of a DMA transfer
 * (non-contiguous physical pages, outside of bus address space).
 * Return true if special handling needed.
 */

isa_dmarangecheck(caddr_t va, unsigned length) {
	vm_offset_t phys, priorpage, endva;

	endva = (vm_offset_t)round_page(va + length);
	priorpage = 0;
	for (; va < (caddr_t) endva ; va += NBPG) {
		phys = trunc_page(pmap_extract(pmap_kernel(), (vm_offset_t)va));
		if (phys == 0)
			panic("isa_dmacheck: no physical page present");
		if (phys > ISA_RAM_END) 
			return (1);
		if (priorpage && priorpage + NBPG != phys)
			return (1);
		priorpage = phys;
	}
	return (0);
}

/* head of queue waiting for physmem to become available */
struct buf isa_physmemq;

/* blocked waiting for resource to become free for exclusive use */
static isaphysmemflag;
/* if waited for and call requested when free (B_CALL) */
static void (*isaphysmemunblock)(); /* needs to be a list */

/*
 * Allocate contiguous physical memory for transfer, returning
 * a *virtual* address to region. May block waiting for resource.
 * (assumed to be called at splbio())
 */
caddr_t
isa_allocphysmem(caddr_t va, unsigned length, void (*func)()) {
	
	isaphysmemunblock = func;
	while (isaphysmemflag & B_BUSY) {
		isaphysmemflag |= B_WANTED;
		tsleep((caddr_t)&isaphysmemflag, PRIBIO, "isaphys", 0);
	}
	isaphysmemflag |= B_BUSY;

	return((caddr_t)isaphysmem);
}

/*
 * Free contiguous physical memory used for transfer.
 * (assumed to be called at splbio())
 */
void
isa_freephysmem(caddr_t va, unsigned length) {

	isaphysmemflag &= ~B_BUSY;
	if (isaphysmemflag & B_WANTED) {
		isaphysmemflag &= B_WANTED;
		wakeup((caddr_t)&isaphysmemflag);
		if (isaphysmemunblock)
			(*isaphysmemunblock)();
	}
}
	
/*
 * Handle a NMI, possibly a machine check.
 * return true to panic system, false to ignore.
 */
isa_nmi(cd) {

	log(LOG_CRIT, "\nNMI port 61 %x, port 70 %x\n", inb(0x61), inb(0x70));
	return(0);
}

/*
 * Caught a stray interrupt, notify
 */
isa_strayintr(d) {


#ifdef notdef
	/* DON'T BOTHER FOR NOW! */
	/* for some reason, we get bursts of intr #7, even if not enabled! */
	log(LOG_ERR,"ISA strayintr %x", d);
#endif
}

u_short
getit(int unit, int timer) {
	int port = (unit ? IO_TIMER2 : IO_TIMER1), val;

	outb(port+ 3, timer<<6); /* emit latch command */
	val = inb(port + timer);
	val = (inb(port + timer) << 8) + val;
	return (val);
}

void
setit(unit, timer, mode, count) {
	int port = (unit ? IO_TIMER2 : IO_TIMER1);

	outb(port+ 3, (timer << 6) + mode); /* emit latch command */
	outb(port + timer, count);
	outb(port + timer, count >> 8);
}

/*
 * get time in absolute it_ticks for tracing.
 */
getticks() {
	register unsigned val;

	/* stop interrupts, emit latch command for timer 0, unit 0 */
	asm (" xorl %eax, %eax ; cli ; outb %al, $0x43 ");

	asm (" inb $0x40, %%al ; xchgb %%al, %%ah ; inb $0x40, %%al ; xchg %%al, %%ah " : "=a" (val));
	/* val = inb(IO_TIMER1);
	val += inb(IO_TIMER1) << 8; */

	if (val >= it_ticksperintr - 1)
		return (it_ticks + it_ticksperintr);
	else
		return (it_ticks + it_ticksperintr - val);
}
	
extern int hz;

/*
 * get fraction of tick in units of microseconds
 * (must be called at splclock)
 * This routine cannot use get_it() because of underflow
 * processing.
 */

microtime(tvp)
	register struct timeval *tvp;
{
	extern unsigned it_ticksperintr;
	register unsigned val;

	/* minimize clock sampling skew by latching counter immediately after blocking interrupts */
	/* stop interrupts, emit latch command for timer 0, unit 0 */
	asm (" xorl %eax, %eax ; cli ; outb %al, $0x43 ");

	/* obtain frozen time - optimize overhead */
	asm (" inb $0x84, %%al ; inb $0x84, %%al ; inb $0x40, %%al ; xchgb %%al, %%ah ; inb $0x84, %%al ; inb $0x84, %%al ; inb $0x40, %%al ; xchgb %%al, %%ah " : "=a" (val));
	*tvp = time;

	/* if overflowed, account for missing clock interrupt */
	/* (clock can be skewed by as much as 3 ticks following interrupt due
	   to bus overhead in processing interrupt request (the counter drops
	   to value 1, signals interrupt, 8259 just misses sampling window
	   (~ 1 I/O bus cycle or 625ns, counter now reset to N, program emits
	   freeze command (another bus cycle) .... */
	if (val < it_ticksperintr - 2)
		/* clock interrupt not pending */
		tvp->tv_usec += (1000000 / hz) * (it_ticksperintr - val)
				/ it_ticksperintr;
	else
		/* clock race; interrupt pending, just add missing tick */
		tvp->tv_usec += 1000000 / hz;

	/* if overflowing microsecond time, carry to seconds time */
	if (tvp->tv_usec > 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}

	/* re-engage interrupts, and take clock interrupt if overflowed */
	asm("sti");
}

static beeping;
static
sysbeepstop(f)
{
	/* disable counter 2 */
	outb(0x61, inb(0x61) & 0xFC);
	if (f)
		timeout(sysbeepstop, 0, f);
	else
		beeping = 0;
}

void sysbeep(int pitch, int period)
{

	outb(0x61, inb(0x61) | 3);	/* enable counter 2 */
	outb(0x43, 0xb6);	/* set command for counter 2, 2 byte write */
	
	outb(0x42, pitch);
	outb(0x42, (pitch>>8));
	
	if (!beeping) {
		beeping = period;
		timeout(sysbeepstop, (caddr_t)(period/2), period);
	}
}

/*
 * Pass command to keyboard controller (8042)
 */
void
kbd_cmd(unsigned val) {
	u_char r;
	
	/* see if we can issue a command. clear data buffer if something present */
	while ((r = inb(KBSTATP)) & (KBS_IBF|KBS_DIB))
		if (r & KBS_DIB) {
			DELAY(7);
			printf("kc: %x", inb(KBDATAP));
		}
	DELAY(7);
	outb(KBCMDP, val);

	/* wait till command is taken */
	while (inb(KBSTATP) & KBS_IBF)
		DELAY(7);
}

/*
 * Pass command thru keyboard controller to keyboard itself
 */
void
kbd_wr(unsigned val) {
	u_char r;
	
	while (inb(KBSTATP) & KBS_IBF)
		DELAY(7);

	while ((r = inb(KBSTATP)) & (KBS_IBF|KBS_DIB))
		if (r & KBS_DIB) {
			DELAY(7);
			printf("kw: %x", inb(KBDATAP));
		}

	DELAY(7);
	outb(KBOUTP, val);

	/* wait till data is taken */
	while (inb(KBSTATP) & KBS_IBF)
		DELAY(7);
}

/*
 * Read a character from keyboard controller 
 */
u_char
kbd_rd() {
	int sts;
	
	while (inb(KBSTATP) & KBS_IBF)
		DELAY(7);
	DELAY(7); 
	while (((sts = inb(KBSTATP)) & KBS_DIB) == 0)
		DELAY(7); 
	DELAY(7);
	return (inb(KBDATAP));
}

kbd_drain() {
	int sts;

	/* do { */
		DELAY(1000); 
		if ((sts = inb(KBSTATP)) & KBS_DIB)
			printf("kd: %x", kbd_rd());
	/* } while (sts & KBS_DIB); */
}

/*
 * Send the keyboard a command, wait for and return status
 */
u_char
key_cmd(unsigned val) {

	kbd_wr(val);
	return(kbd_rd());
}

/*
 * Send the aux port a command, wait for and return status
 */
u_char
aux_cmd(unsigned val) {
	u_char r;
	
	kbd_cmd(K_AUXOUT);
	kbd_wr(val);
	return(kbd_rd());
}

/*
 * Execute a keyboard controller command that passes a parameter
 */
void
kbd_cmd_write_param(unsigned cmd, unsigned val) {
	u_char r;
	
	kbd_cmd(cmd);
	kbd_wr(val);
}

/*
 * Execute a keyboard controller command that returns a parameter
 */
u_char
kbd_cmd_read_param(unsigned cmd) {
	u_char r;

	kbd_cmd(cmd);
	return(kbd_rd());
}

#define	CW54BCD		0x01	/* BCD instead of binary count down mode */
#define	CW54RATE	 0x04	/* rate operating mode */
#define	CW54SQUARE	 0x06	/* square wave output mode */
#define	CW54LSBMSB	 0x30	/* pass counter values in little endian order */

#define	KBC42FAILDIS	 0x04	/* disable failsafe counter NMI */
#define	KBC42IOCHKDIS	 0x08	/* disable IOCHK NMI */
#define	KBC42FAILNMI	 0x40	/* NMI from failsafe timer  */
#define	KBC42IOCHKNMI	 0x80	/* NMI from IOCHK */

/*
 * Enable an NMI failsafe timer for a millisecond
 */
int allownmi;
failsafe_start() {
	if(allownmi) {
	setit(1, 0, CW54LSBMSB|CW54SQUARE, 1193000/100);
	outb(0x61, inb(0x61) & ~KBC42FAILDIS);
	outb(0x70, 0x00);
	}
}

/*
 * Disable the NMI failsafe timer, cancelling the timeout.
 */
failsafe_cancel() {
	outb(0x61, inb(0x61) | KBC42FAILDIS);
	outb(0x70, 0x80);
}

/*
 * Read RTC atomically
 */
u_char
rtcin(u_char  adr)
{
	int x;

	/*
	 * Some RTC's (dallas smart watch) attempt to foil unintentioned
	 * operation of the RTC by requiring back-to-back i/o instructions
	 * to reference the RTC; any interviening i/o operations cancel
	 * the reference to the clock (e.g. the NOP).
	 */
	x = splhigh();
	asm volatile ("out%z0 %b0, %2 ; xorl %0, %0 ; in%z0 %3, %b0"
		: "=a"(adr) : "0"(adr), "i"(IO_RTC), "i"(IO_RTC + 1));
	splx(x);
	return (adr);
}

/*
 * Write RTC atomically.
 */
void
rtcout(u_char adr, u_char val)
{
	int x;

	/*
	 * Some RTC's (dallas smart watch) attempt to foil unintentioned
	 * operation of the RTC by requiring back-to-back i/o instructions
	 * to reference the RTC; any interviening i/o operations cancel
	 * the reference to the clock (e.g. the NOP).
	 */
	x = splhigh();
	asm volatile ("out%z0 %b0, %2 ; movb %1, %b0 ; out%z0 %b0, %3"
		:: "a"(adr), "g"(val), "i"(IO_RTC), "i"(IO_RTC+1));
	splx(x);
}

/* XXX: framework only, unfinished */
static int
isaopen(dev_t dev, int flag, int mode, struct proc *p) {
	struct syscframe *fp = (struct syscframe *)p->p_md.md_regs;

	fp->sf_eflags |= PSL_IOPL;
	return (0);
}

static int
isaclose(dev_t dev, int flag, int mode, struct proc *p) {
	struct syscframe *fp = (struct syscframe *)p->p_md.md_regs;

	fp->sf_eflags &= ~PSL_IOPL;
	return (0);
}

static int
isaioctl(dev_t dev, int cmd, caddr_t addr, int flag, struct proc *p) {

	/* What's missing: enable selection on irq's, controls on
	   [e]isa features, statistics, ... */

	return (ENODEV);
}

static int
isaselect(dev_t dev, int rw, struct proc *p) {

	return (ENODEV);
}

static int
isammap(dev_t dev, int offset, int nprot)
{
	if (offset < ISA_RAM_BEGIN || offset > ISA_RAM_END)
		return -1;

	return i386_btop(offset);
}

static struct devif isa_devif =
{
	{0}, -1, -2, 0, 0, 0, 0, 0, 0,
	isaopen, isaclose, isaioctl, 0, 0, isaselect, isammap,
	0, 0, 0, 0,
	0,  0, 
};

DRIVER_MODCONFIG() {
	char *cfg_string = isa_config;
	
	if (devif_config(&cfg_string, &isa_devif) == 0)
		return;

}
