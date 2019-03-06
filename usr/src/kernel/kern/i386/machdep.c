/*
 * Copyright (c) 1989, 1990, 1991, 1994 William F. Jolitz, TeleMuse
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
 *	$Id: machdep.c,v 1.1 94/10/19 17:40:00 bill Exp $
 *
 * Machine dependant initialization XXX.
 */


#include "sys/param.h"
#include "sys/file.h"
#include "sys/user.h"
#include "sys/reboot.h"
#include "systm.h"
#include "kernel.h"
#include "proc.h"
#include "signalvar.h"
#include "buf.h"
#include "callout.h"
#include "malloc.h"
#include "mbuf.h"
#include "msgbuf.h"
#include "modconfig.h"
#include "netisr.h"
#define	__NO_INLINES
#include "prototypes.h"
#undef	__NO_INLINES

#include "vm.h"
#include "kmem.h"

extern vm_offset_t avail_end, virtual_end;

#include "machine/cpu.h"
#include "machine/reg.h"
#include "machine/psl.h"
#include "segments.h"
#include "specialreg.h"
#include "rtc.h"
#ifdef SYSVSHMx
#include "sys/shm.h"
#endif

/*
 * If no inlines, define the code for inclusion singularly in this module.
 */
/*#ifdef __NO_INLINES*/
#define	__NO_INLINES_BUT_EMIT_CODE
/*#undef __NO_INLINES*/
#include "prototypes.h"
#include "machine/inline/io.h"
#include "machine/inline/inet.h"
#undef	__NO_INLINES_BUT_EMIT_CODE
/* #endif */

extern int bootdev, bootsp;	/* XXX */

/*
 * Declare these as initialized data so we can patch them.
 */
int	nswbuf = 0;
#ifdef	NBUF
int	nbuf = NBUF;
#else
int	nbuf = 0;
#endif
#ifdef	BUFPAGES
int	bufpages = BUFPAGES;
#else
int	bufpages = 0;
#endif
int	msgbufmapped;		/* set when safe to use msgbuf */
extern int freebufspace;
extern char copyright1[], copyright2[];

/*
 * Machine-dependent startup code
 */
extern int boothowto;
long dumplo;
int physmem, maxmem;

void
cpu_startup(void)
{
	register unsigned i;
	register caddr_t v;
	int maxbufs, base, residual;
	vm_size_t size;
	int firstaddr;
	int kernel_virtual_pages_remaining = atop(virtual_end);

	/*
	 * Initialize error message buffer (at end of core).
	 */

	/* avail_end was pre-decremented in pmap_bootstrap to compensate */
	for (i = 0; i < btoc(sizeof (struct msgbuf)); i++)
		pmap_enter(pmap_kernel(), (vm_offset_t)msgbufp,
			avail_end + (i+1) * NBPG, VM_PROT_ALL, TRUE, AM_NONE);
	msgbufmapped = 1;

#ifdef KDB
	kdb_init();			/* startup kernel debugger */
#endif
	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	printf("%s [1.0.%s]\n", copyright1, version+9);
	printf(copyright2);

	/*
	 * Allocate space for system data structures.
	 * The first available kernel virtual address is in "v".
	 * As pages of kernel virtual memory are allocated, "v" is incremented.
	 * As pages of memory are allocated and cleared,
	 * "firstaddr" is incremented.
	 */

	/*
	 * Make two passes.  The first pass calculates how much memory is
	 * needed and allocates it.  The second pass assigns virtual
	 * addresses to the various data structures.
	 */
	firstaddr = 0;
again:
	v = (caddr_t)firstaddr;

#define	valloc(name, type, num) \
	    (name) = (type *)v; v = (caddr_t)((name)+(num))
#define	valloclim(name, type, num, lim) \
	    (name) = (type *)v; v = (caddr_t)((lim) = ((name)+(num)))
	valloc(callout, struct callout, ncallout);

	/*
	 * Determine how many buffers to allocate.
	 * Use 10% of memory for the first 2 Meg, 5% of the remaining
	 * memory. Insure a minimum of 16 buffers.
	 * We allocate 1/2 as many swap buffer headers as file i/o buffers.
	 */
	if (bufpages == 0)
		if (physmem < ((2 * 1024 * 1024)/NBPG))
			bufpages = physmem / 10 / CLSIZE;
		else
			bufpages = (((2 * 1024 * 1024)/NBPG + 2*physmem) / 10) / CLSIZE;

	if (nbuf == 0) {
		nbuf = bufpages / 2;
		if (nbuf < 16)
			nbuf = 16;
	}
	freebufspace = ptoa(bufpages);
	valloc(buf, struct buf, nbuf);

	/*
	 * End of first pass, size has been calculated so allocate memory
	 */
	if (firstaddr == 0) {
		size = (vm_size_t)(v - firstaddr);
		firstaddr = (int)kmem_alloc(kernel_map, round_page(size),
			M_ZERO_IT);
		if (firstaddr == 0)
			panic("startup: no room for tables");
		 /*(void) memset((void *)firstaddr, 0, size); /* paranoia, sheer */
		goto again;
	}
	kernel_virtual_pages_remaining -= atop(firstaddr+round_page(size));

	/*
	 * End of second pass, addresses have been assigned
	 */
	if ((vm_size_t)(v - firstaddr) != size)
		panic("startup: table size inconsistency");
	/*
	 * Allocate a submap for physio
	 */
#define USRIOSIZE  300	/*XXX*/
	phys_map = kmem_suballoc(VM_PHYS_SIZE, TRUE);
	kernel_virtual_pages_remaining -= atop(VM_PHYS_SIZE);

	/* maximum buffer space window */
	if (atop(freebufspace) > kernel_virtual_pages_remaining)
		buf_map = kmem_suballoc(kernel_virtual_pages_remaining, TRUE);
	else
		buf_map = kmem_suballoc(freebufspace, TRUE);
printf("kva %d physmem %d bufpages %d nbuf %d\n", kernel_virtual_pages_remaining, physmem, bufpages, nbuf);

	/*
	 * Finally, allocate mbuf pool.  Since mclrefcnt is an off-size
	 * we use the more space efficient malloc in place of kmem_alloc.
	 */
	mclrefcnt = (char *)malloc(NMBCLUSTERS+CLBYTES/MCLBYTES,
		M_MBUF, M_NOWAIT);
	(void) memset(mclrefcnt, 0, NMBCLUSTERS+CLBYTES/MCLBYTES);
	mb_map = kmem_suballoc(VM_MBUF_SIZE, FALSE);
	mbutl = (struct mbuf *) vm_map_min(mb_map);

	/*
	 * Initialize callouts
	 */
	callfree = callout;
	for (i = 1; i < ncallout; i++)
		callout[i-1].c_next = &callout[i];

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();
}

extern int bootdev, bootsp;     /* XXX */

extern struct pcb dumppcb;

extern int	dumpsize;
/*
 * Attempt to dump the system's memory image to disk, so
 * that the /sbin/savecore program can recover it after reboot.
 */
dumpsys()
{

	/* no dump device at all? */
	if (dumpdev == BLK_NODEV)
		return;

	/* XXX don't dump in case of running off cdrom */
	if ((minor(bootdev)&07) != 3)
		return;

	/* dump to a non-swap partition? */
	if ((minor(dumpdev)&07) != 1)
		return;

	/* save current state in pcb so that debugger can reconstruct the stack */
	asm ("movl %%ebx, %0" : "=m" (dumppcb.pcb_tss.tss_ebx));
	asm ("movl %%esi, %0" : "=m" (dumppcb.pcb_tss.tss_esi));
	asm ("movl %%edi, %0" : "=m" (dumppcb.pcb_tss.tss_edi));
	asm ("movl %%esp, %0" : "=m" (dumppcb.pcb_tss.tss_esp));
	asm ("movl %%ebp, %0" : "=m" (dumppcb.pcb_tss.tss_ebp));
	asm ("movl $1f, %0; 1:" : "=m" (dumppcb.pcb_tss.tss_eip));
	dumppcb.pcb_ptd = rcr(3);

	printf("\nThe operating system is saving a copy of RAM memory to device %x, offset %d\n\
(hit any key to abort): [ amount left to save (MB) ] ", dumpdev, dumplo);

	/* give a second for operator to abort dump */
	DELAY(1000000);

	dumpsize = physmem;
	switch (devif_dump(dumpdev, BLKDEV)) {
	/* switch ((*bdevsw[major(dumpdev)].d_dump)(dumpdev)) {*/

	case ENXIO:
		printf("-- device bad\n");
		break;

	case EFAULT:
		printf("-- device not ready\n");
		break;

	case EINVAL:
		printf("-- area improper\n");
		break;

	case EIO:
		printf("-- i/o error\n");
		break;

	case EINTR:
		printf("-- aborted from console\n");
		break;

	default:
		printf(" succeeded\n");
		break;
	}
	printf("system rebooting.\n\n");
	DELAY(10000);
}

/*
 * Initialize 386 and configure to run kernel
 */

/*
 * Initialize segments & interrupt table
 */


#define	GNULL_SEL	0	/* Null Descriptor */
#define	GCODE_SEL	1	/* Kernel Code Descriptor */
#define	GDATA_SEL	2	/* Kernel Data Descriptor */
#define	GLDT_SEL	3	/* LDT - eventually one per process */
#define	GTGATE_SEL	4	/* Process task switch gate */
#define	GINVTSS_SEL	5	/* Task state to take invalid tss on */
#define	GDBLFLT_SEL	6	/* Task state to take double fault on */
#define	GEXIT_SEL	7	/* Task state to process cpu_texit() on */
#define	GPROC0_SEL	8	/* Task state process slot zero and up */
#define NGDT 		16	/* round to next power of two */

sel_t	_udatasel, _ucodesel, _exit_tss_sel;

union descriptor gdt_bootstrap[NGDT], *gdt = gdt_bootstrap;

struct segment_descriptor *sdfirstp_gdesc = &gdt_bootstrap[GPROC0_SEL].sd,
	*sdlast_gdesc = &gdt_bootstrap[NGDT - 1].sd,
	*sdlastfree_gdesc = &gdt_bootstrap[GPROC0_SEL].sd;
int sdngd_gdesc = NGDT, sdnfree_gdesc = NGDT - GPROC0_SEL;

/* interrupt descriptor table */
struct gate_descriptor idt[NIDT];

/* local descriptor table */
union descriptor ldt[5];
#define	LSYS5CALLS_SEL	0	/* forced by intel BCS */
#define	LSYS5SIGR_SEL	1

#define	L43BSDCALLS_SEL	2	/* notyet */
#define	LUCODE_SEL	3
#define	LUDATA_SEL	4
/* seperate stack, es,fs,gs sels ? */
/* #define	LPOSIXCALLS_SEL	5	/* notyet */

struct	i386tss	inv_tss, dbl_tss, exit_tss;
char alt_stack[1024];

extern  struct user *proc0paddr;

/* software prototypes -- in more palitable form */
struct soft_segment_descriptor gdt_segs[] = {
	/* Null Descriptor */
{	0x0,			/* segment base address  */
	0x0,			/* length - all address space */
	0,			/* segment type */
	0,			/* segment descriptor priority level */
	0,			/* segment descriptor present */
	0,0,
	0,			/* default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
	/* Code Descriptor for kernel */
{	0x0,			/* segment base address  */
	0xffffe,		/* length - all address space */
	SDT_MEMERA,		/* segment type */
	0,			/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0,0,
	1,			/* default 32 vs 16 bit size */
	1  			/* limit granularity (byte/page units)*/ },
	/* Data Descriptor for kernel */
{	0x0,			/* segment base address  */
	0xffffe,		/* length - all address space */
	SDT_MEMRWA,		/* segment type */
	0,			/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0,0,
	1,			/* default 32 vs 16 bit size */
	1  			/* limit granularity (byte/page units)*/ },
	/* LDT Descriptor */
{	(int) ldt,			/* segment base address  */
	sizeof(ldt)-1,		/* length - all address space */
	SDT_SYSLDT,		/* segment type */
	0,			/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0,0,
	0,			/* unused - default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
	/* Null Descriptor - Placeholder */
{	0x0,			/* segment base address  */
	0x0,			/* length - all address space */
	0,			/* segment type */
	0,			/* segment descriptor priority level */
	0,			/* segment descriptor present */
	0,0,
	0,			/* default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
	/* Invalid Tss fault Tss Descriptor */
{	(int) &inv_tss,		/* segment base address  */
	sizeof(struct i386tss)-1,		/* length - all address space */
	SDT_SYS386TSS,		/* segment type */
	0,			/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0,0,
	0,			/* unused - default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
	/* Double fault Tss Descriptor */
{	(int) &dbl_tss,		/* segment base address  */
	sizeof(struct i386tss)-1,		/* length - all address space */
	SDT_SYS386TSS,		/* segment type */
	0,			/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0,0,
	0,			/* unused - default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
	/* Exit Tss Descriptor */
{	(int) &exit_tss,	/* segment base address  */
	sizeof(struct i386tss)-1,		/* length - all address space */
	SDT_SYS386TSS,		/* segment type */
	0,			/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0,0,
	0,			/* unused - default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ }};

struct soft_segment_descriptor ldt_segs[] = {
	/* Null Descriptor - overwritten by call gate */
{	0x0,			/* segment base address  */
	0x0,			/* length - all address space */
	0,			/* segment type */
	0,			/* segment descriptor priority level */
	0,			/* segment descriptor present */
	0,0,
	0,			/* default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
	/* Null Descriptor - overwritten by call gate */
{	0x0,			/* segment base address  */
	0x0,			/* length - all address space */
	0,			/* segment type */
	0,			/* segment descriptor priority level */
	0,			/* segment descriptor present */
	0,0,
	0,			/* default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
	/* Null Descriptor - overwritten by call gate */
{	0x0,			/* segment base address  */
	0x0,			/* length - all address space */
	0,			/* segment type */
	0,			/* segment descriptor priority level */
	0,			/* segment descriptor present */
	0,0,
	0,			/* default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
	/* Code Descriptor for user */
{	0x0,			/* segment base address  */
	0xffffe,		/* length - all address space */
	SDT_MEMERA,		/* segment type */
	SEL_UPL,		/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0,0,
	1,			/* default 32 vs 16 bit size */
	1  			/* limit granularity (byte/page units)*/ },
	/* Data Descriptor for user */
{	0x0,			/* segment base address  */
	0xffffe,		/* length - all address space */
	SDT_MEMRWA,		/* segment type */
	SEL_UPL,		/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0,0,
	1,			/* default 32 vs 16 bit size */
	1  			/* limit granularity (byte/page units)*/ } };

void
setidt(int idx, void *func, int typ, int dpl, sel_t sel)
{
	struct gate_descriptor *ip = idt + idx;

	ip->gd_looffset = (int)func;
	ip->gd_selector = sel;
	ip->gd_stkcpy = 0;
	ip->gd_xx = 0;
	ip->gd_type = typ;
	ip->gd_dpl = dpl;
	ip->gd_p = 1;
	ip->gd_hioffset = ((int)func)>>16 ;
}

void
setirq(int idx, void *func) {

	setidt(idx, func, SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
}

/*
 * Expand the Global descriptor table (GDT) in the case of increased
 * demand for descriptors (currently, just TSS's). Since the tables
 * are densly allocated out of linear address space, which is known
 * to be mapped 1:1 with kernel virtual address space, growth is
 * accomplished by allocating a new portion of virtual address space
 * and building a new GDT out of the old one, prior to discarding it.
 * This is a costly operation, but we do this rather than maximally
 * allocating the 64KB GDT when we will infrequently use more than a
 * few hundred descriptors. We grow exponentially as a concession to
 * potential demand for threads, and also to allow for the use of a
 * "buddy" allocator scheme to provide for a future shrinking scheme
 * on deallocation (see freedesc()).
 */
void
expanddesctable(void)
{
	union descriptor *gd;
	struct region_descriptor r_gdt;

	/* obtain a twice larger table for our "new" gdt. */
	gd = (union descriptor *) malloc(2 * sdngd_gdesc
	    * sizeof(union descriptor), M_TEMP, M_WAITOK);

	/* create new one out of "old" table. */
	(void) splhigh();
	sdlast_gdesc = (struct segment_descriptor *)gd + (2 * sdngd_gdesc - 1);
	sdfirstp_gdesc = (struct segment_descriptor *)gd + GPROC0_SEL;
	sdlastfree_gdesc = (struct segment_descriptor *)gd + sdngd_gdesc;
	memcpy(gd, gdt, sdngd_gdesc * sizeof(union descriptor));
	(void)memset(sdlastfree_gdesc, 0,
	    sdngd_gdesc * sizeof(union descriptor));
	sdnfree_gdesc = sdngd_gdesc;
	sdngd_gdesc *= 2;

	/* reload descriptor table */
	r_gdt.rd_limit = sdngd_gdesc * sizeof(union descriptor) - 1;
	r_gdt.rd_base = (int) gd;
	lgdt(r_gdt);

	/* free old descriptor table */
	if (gdt != gdt_bootstrap)
		free((void *)gdt, M_TEMP);

	/* place new descriptor table in place */
	gdt = gd;
	(void) splnone();
}

/* called from debugger*/
struct i386tss *
panictss(){
	int sel;
	struct segment_descriptor *sdp;
	struct i386tss *tsp;

	asm(" cli ; str %w0" : "=q"(sel));
	printf("\nTask Exception\n current context: ");

again:
	printf("selector %x ", sel);
	sdp = &gdt[IDXSEL(sel)].sd;
	tsp = (struct i386tss *) (sdp->sd_lobase + (sdp->sd_hibase<<24));

	printf(" type %d ", sdp->sd_type);
	printf("cs %x eip %x esp %x ebp %x", tsp->tss_cs,
		tsp->tss_eip, tsp->tss_esp, tsp->tss_ebp);

	sel = tsp->tss_link;
	if (sel) {
		printf("\nprevious context: ");
		goto again;
	}
	printf("\n");
	DELAY(100000);
	return (tsp);
}


#define	IDTVEC(name)	__CONCAT(X, name)
extern	IDTVEC(div), IDTVEC(dbg), IDTVEC(nmi), IDTVEC(bpt), IDTVEC(ofl),
	IDTVEC(bnd), IDTVEC(ill), IDTVEC(dna), IDTVEC(dble), IDTVEC(fpusegm),
	IDTVEC(tss), IDTVEC(missing), IDTVEC(stk), IDTVEC(prot),
	IDTVEC(page), IDTVEC(rsvd), IDTVEC(fpu), IDTVEC(rsvd0),
	IDTVEC(rsvd1), IDTVEC(rsvd2), IDTVEC(rsvd3), IDTVEC(rsvd4),
	IDTVEC(rsvd5), IDTVEC(rsvd6), IDTVEC(rsvd7), IDTVEC(rsvd8),
	IDTVEC(rsvd9), IDTVEC(rsvd10), IDTVEC(rsvd11), IDTVEC(rsvd12),
	IDTVEC(rsvd13), IDTVEC(rsvd14), IDTVEC(rsvd14), IDTVEC(syscall);

int	cpu_option;


invtss() {
	struct i386tss *tsp = panictss();
	struct trapframe tf;

	tf.tf_err = *(int *) (alt_stack + 1020);
	tf.tf_trapno = T_TSSFLT;
	tf.tf_cs = tsp->tss_cs;
	tf.tf_eax = tsp->tss_eax;
	tf.tf_ebx = tsp->tss_ebx;
	tf.tf_ecx = tsp->tss_ecx;
	tf.tf_edx = tsp->tss_edx;
	tf.tf_esp = tsp->tss_esp;
	tf.tf_ebp = tsp->tss_ebp;
	tf.tf_eip = tsp->tss_eip;
	tf.tf_eflags = tsp->tss_eflags;
	trap(tf);
	panic("inval tss");
}

dbl() {
	struct i386tss *tsp = panictss();
	struct trapframe tf;

	tf.tf_err = 0;
	tf.tf_trapno = T_DOUBLEFLT;
	tf.tf_cs = tsp->tss_cs;
	tf.tf_eax = tsp->tss_eax;
	tf.tf_ebx = tsp->tss_ebx;
	tf.tf_ecx = tsp->tss_ecx;
	tf.tf_edx = tsp->tss_edx;
	tf.tf_esp = tsp->tss_esp;
	tf.tf_ebp = tsp->tss_ebp;
	tf.tf_eip = tsp->tss_eip;
	tf.tf_eflags = tsp->tss_eflags;
	trap(tf);
	panic("double fault");
}


/* initialize processor, pass first physical page past kernel */
init386(firstphys) {
	int x;
	unsigned biosbasemem, biosextmem;
	struct gate_descriptor *gdp;
	/* table descriptors - used to load tables by microp */
	struct region_descriptor r_gdt, r_idt;

	proc0.p_addr = proc0paddr;

	/*
	 * Initialize the console before we print anything out.
	 * Initialize any busses that the console might be on.
	 */
	modscaninit(MODT_BUS);
	modscaninit(MODT_CONSOLE);

	/* make gdt memory segments */
	for (x=0; x < sizeof(gdt_segs)/sizeof(struct soft_segment_descriptor);
	    x++)
		ssdtosd(gdt_segs+x, gdt+x);

	/* make ldt memory segments */
	ldt_segs[LUCODE_SEL].ssd_limit = btoc(VM_MAX_ADDRESS);
	ldt_segs[LUDATA_SEL].ssd_limit = btoc(VM_MAX_ADDRESS);

	/* Note. eventually want private ldts per process */
	for (x=0; x < 5; x++)
		ssdtosd(ldt_segs+x, ldt+x);

	/* exceptions */
	setidt(0, &IDTVEC(div),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(1, &IDTVEC(dbg),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(2, &IDTVEC(nmi),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
 	setidt(3, &IDTVEC(bpt),  SDT_SYS386TGT, SEL_UPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(4, &IDTVEC(ofl),  SDT_SYS386TGT, SEL_UPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(5, &IDTVEC(bnd),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(6, &IDTVEC(ill),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(7, &IDTVEC(dna),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(8, &IDTVEC(dble),  SDT_SYSTASKGT, SEL_KPL, GSEL(GDBLFLT_SEL, SEL_KPL));
	setidt(9, &IDTVEC(fpusegm),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(10, &IDTVEC(tss),  SDT_SYSTASKGT, SEL_KPL, GSEL(GINVTSS_SEL, SEL_KPL));
	setidt(11, &IDTVEC(missing),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(12, &IDTVEC(stk),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(13, &IDTVEC(prot),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(14, &IDTVEC(page),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(15, &IDTVEC(rsvd),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(16, &IDTVEC(fpu),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(17, &IDTVEC(rsvd0),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(18, &IDTVEC(rsvd1),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(19, &IDTVEC(rsvd2),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(20, &IDTVEC(rsvd3),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(21, &IDTVEC(rsvd4),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(22, &IDTVEC(rsvd5),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(23, &IDTVEC(rsvd6),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(24, &IDTVEC(rsvd7),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(25, &IDTVEC(rsvd8),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(26, &IDTVEC(rsvd9),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(27, &IDTVEC(rsvd10),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(28, &IDTVEC(rsvd11),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(29, &IDTVEC(rsvd12),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(30, &IDTVEC(rsvd13),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(31, &IDTVEC(rsvd14),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));

	r_gdt.rd_limit = NGDT * sizeof(union descriptor) - 1;
	r_gdt.rd_base = (int) gdt;
	lgdt(r_gdt);

	r_idt.rd_limit = sizeof(idt) - 1;
	r_idt.rd_base = (int) idt;
	lidt(r_idt);
	lldt(GSEL(GLDT_SEL, SEL_KPL));

	/*
	 * Use BIOS values stored in RTC CMOS RAM, since probing
	 * breaks certain 386 AT relics.
	 */
	biosbasemem = rtcin(RTC_BASELO)+ (rtcin(RTC_BASEHI)<<8);
#define	RTC_EXTMSB	0x5d
	biosextmem = rtcin(RTC_EXTLO)+ (rtcin(RTC_EXTHI)<<8) + (rtcin(RTC_EXTMSB)<<16);

	/* if either bad, just assume base memory */
	if (biosbasemem == 0xffff /*|| biosextmem == 0xffff*/) {
		maxmem = min (maxmem, 640/4);
	} else if (biosextmem > 0  && biosbasemem == 640) {
		int pagesinbase, pagesinext;

		pagesinbase = biosbasemem/4 - firstphys/NBPG;
		pagesinext = biosextmem/4;

		/*
		 * use greater of either base or extended memory. do this
		 * until I reinstitue discontiguous allocation of vm_page
		 * array.
		 */
		if (pagesinbase > pagesinext)
			maxmem = 640/4;
		else {
			maxmem = pagesinext + 0x100000/NBPG;
			if (firstphys < 0x100000)
				firstphys = 0x100000; /* skip hole */
		}
	} else
		maxmem = biosbasemem/4;

	maxmem -=  1;	/* highest page of usable memory */

#define RAM_MB_TOO_SMALL 2	/* smallest megabytes of memory useable */
#define RAM_MB_TOO_LARGE 64	/* largest megabytes of memory useable - why?? */

	if (maxmem < RAM_MB_TOO_SMALL * 1024/4) {
		printf("Warning: not enough RAM(%d pages), running in degraded mode.\n", maxmem);
		DELAY(500*1000);
	}
	if (maxmem > RAM_MB_TOO_LARGE * 1024/4) {
		printf("Warning: Too much RAM memory, only using first %d MB.\n", RAM_MB_TOO_LARGE);
		maxmem = RAM_MB_TOO_LARGE * 1024/4;
		DELAY(500*1000);
	}

	physmem = maxmem;	/* number of pages of physmem addr space */

	vm_set_page_size(NBPG);			/* XXX */
	/* call pmap initialization to make new kernel address space */
	pmap_bootstrap (firstphys, 0);

	/* make a initial tss so microp can get interrupt stack on syscall! */
	memset(&proc0.p_addr->u_pcb, 0, sizeof(struct pcb));
	proc0.p_addr->u_pcb.pcb_tss.tss_esp0 = (int) proc0paddr + UPAGES*NBPG - 4;
	proc0.p_addr->u_pcb.pcb_tss.tss_ss0 = GSEL(GDATA_SEL, SEL_KPL) ;
	proc0.p_addr->u_pcb.pcb_tss.tss_esp = (int) proc0paddr + UPAGES*NBPG - 4;
	proc0.p_addr->u_pcb.pcb_tss.tss_ss = GSEL(GDATA_SEL, SEL_KPL) ;
	alloctss(&proc0);
	ltr(proc0.p_md.md_tsel);

	proc0.p_addr->u_pcb.pcb_tss.tss_ldt =  GSEL(GLDT_SEL, SEL_KPL);
	proc0.p_addr->u_pcb.pcb_tss.tss_cs =  GSEL(GCODE_SEL, SEL_KPL);
	proc0.p_addr->u_pcb.pcb_tss.tss_ds =  GSEL(GDATA_SEL, SEL_KPL);
	proc0.p_addr->u_pcb.pcb_tss.tss_es =  GSEL(GDATA_SEL, SEL_KPL);
	proc0.p_addr->u_pcb.pcb_tss.tss_ss =  GSEL(GDATA_SEL, SEL_KPL);
	proc0.p_addr->u_pcb.pcb_tss.tss_fs =  GSEL(GDATA_SEL, SEL_KPL);
	proc0.p_addr->u_pcb.pcb_tss.tss_gs =  GSEL(GDATA_SEL, SEL_KPL);

	/* make a call gate to reenter kernel with */
	gdp = &ldt[LSYS5CALLS_SEL].gd;
	x = (int) &IDTVEC(syscall);
	gdp->gd_looffset = x++;
	gdp->gd_selector = GSEL(GCODE_SEL,SEL_KPL);
	gdp->gd_stkcpy = 0;
	gdp->gd_type = SDT_SYS386CGT;
	gdp->gd_dpl = SEL_UPL;
	gdp->gd_p = 1;
	gdp->gd_hioffset = ((int) &IDTVEC(syscall)) >>16;

	/* make a call gate for iBCS signal return gate 
	ldt[LSYS5SIGR_SEL] = ldt[LSYS5CALLS_SEL];
	x = (int) &IDTVEC(i386_sigret);
	gdp->gd_looffset = x++;
	gdp->gd_hioffset = ((int) &IDTVEC(i386_sigret)) >>16; */

	/* transfer to user mode */
	_ucodesel = LSEL(LUCODE_SEL, SEL_UPL);
	_udatasel = LSEL(LUDATA_SEL, SEL_UPL);

	/* setup proc 0's pcb */
	proc0.p_addr->u_pcb.pcb_flags = 0;
	proc0.p_addr->u_pcb.pcb_ptd = KernelPTD;

	/* setup fault tss's */
	inv_tss = proc0.p_addr->u_pcb.pcb_tss;
	inv_tss.tss_esp0 = (int) (alt_stack + 1020);
	inv_tss.tss_esp = (int) (alt_stack + 1020);
	inv_tss.tss_ss =  GSEL(GDATA_SEL, SEL_KPL);
	inv_tss.tss_cs =  GSEL(GCODE_SEL, SEL_KPL);
	inv_tss.tss_eip =  (int) &invtss;
	exit_tss = dbl_tss = inv_tss;
	dbl_tss.tss_eip =  (int) &dbl;
	_exit_tss_sel =  GSEL(GEXIT_SEL, SEL_KPL);

	/* are we a 386 or at least a 486? */
	x = rcr(0);
		lcr(0, x | CR0_WP);

	lcr(0, x & ~ CR0_WP);
	x = rcr(0);
	if (x & CR0_WP) {
/* if compiled for 486 instructions, which will emulate slowly on 386, hint */
#if defined(i486)
		printf("486 optimization mode\n");
#endif
		cpu_option = CPU_386_DEFAULT;
	} else {
		/* put back on write protection */
		lcr(0, x | CR0_WP);
		cpu_option = CPU_486_DEFAULT;
		lcr(0, x | 0x20);
		/* really, truely, a 486 */
		x = rcr(0);
		if(x & 0x20) {
			cpu_option |= CPU_486_INVTLB;
		}
	}

#ifdef DDB
	kdb_init();
	if (boothowto & RB_KDB)
		Debugger();
#endif
}

/* output a byte, allowing for recovery time */
void
__outb(u_short port, u_char val) {

	(void) inb_(0x84);
	(void) inb_(0x84);
	outb(port, val);
	(void) inb_(0x84);
	(void) inb_(0x84);
}

/* input a byte, allowing for recovery time */
u_char
__inb(u_short port) {
	u_char rv;

	(void) inb_(0x84);
	(void) inb_(0x84);
	rv = inb(port);
	(void) inb_(0x84);
	(void) inb_(0x84);

	return (rv);
}
