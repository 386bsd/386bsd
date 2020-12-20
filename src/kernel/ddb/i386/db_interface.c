/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */
/*
 * HISTORY
 * $Log:	db_interface.c,v $
 * Revision 1.2  93/10/21  10:46:48  bill
 * 0.2 beta changes
 * 
 * Revision 1.1  1992/03/25  21:42:03  pace
 * Initial revision
 *
 * Revision 2.4  91/02/05  17:11:13  mrt
 * 	Changed to new Mach copyright
 * 	[91/02/01  17:31:17  mrt]
 * 
 * Revision 2.3  90/12/04  14:45:55  jsb
 * 	Changes for merged intel/pmap.{c,h}.
 * 	[90/12/04  11:14:41  jsb]
 * 
 * Revision 2.2  90/10/25  14:44:43  rwd
 * 	Added watchpoint support.
 * 	[90/10/18            rpd]
 * 
 * 	Created.
 * 	[90/07/25            dbg]
 * 
 *
 */

/*
 * Interface to new debugger.
 */
/*#include "specialreg.h"*/
#include "sys/param.h"
#include "proc.h"
#include "machine/db/db_machdep.h"

#include "sys/reboot.h"
#include "vm.h"
#include "vm_statistics.h"
#include "pmap.h"

#include <setjmp.h>
#include "systm.h" /* just for boothowto --eichin */
int	db_active = 0;

/*
 * Received keyboard interrupt sequence.
 */
kdb_kbd_trap(regs)
	struct i386_saved_state *regs;
{
	if (db_active == 0 && (boothowto & RB_KDB)) {
	    printf("\n\nkernel: keyboard interrupt\n");
	    kdb_trap(-1, 0, regs);
	}
}

/*
 *  kdb_trap - field a TRACE or BPT trap
 */

static jmp_buf *db_nofault = 0;

kdb_trap(type, code, regs)
	int	type, code;
	register struct i386_saved_state *regs;
{
int sv;

#if 0
	if ((boothowto&RB_KDB) == 0)
	    return(0);
#endif

asm("lahf ; cli" : "=a" (sv));
	switch (type) {
	    case T_BPTFLT /* T_INT3 */:	/* breakpoint */
	    case T_KDBTRAP /* T_WATCHPOINT */:	/* watchpoint */
	    /* case T_PRIVINFLT /* T_DEBUG :	/* single_step */
	    case T_TRCTRAP /* T_DEBUG */:	/* single_step */

	    case -1:	/* keyboard interrupt */
		break;

	    default:
		kdbprinttrap(type, code);

		if (db_nofault) {
		    jmp_buf *no_fault = db_nofault;
		    db_nofault = 0;
asm("sahf " : : "a" (sv));
		    longjmp(*no_fault, 1);
		}
	}

	/*  Should switch to kdb`s own stack here. */

	ddb_regs = *regs;

	if ((regs->tf_cs & 0x3) == 0) {
	    /*
	     * Kernel mode - esp and ss not saved
	     */
	    ddb_regs.tf_esp = (int)&regs->tf_esp;	/* kernel stack pointer */
#if 0
	    ddb_regs.ss   = KERNEL_DS;
#endif
	    asm(" movw %%ss,%%ax; movl %%eax,%0 " 
		: "=g" (ddb_regs.tf_ss) 
		:
		: "ax");
	}

	db_active++;
	cnpollc(TRUE);
	db_trap(type, code);
	cnpollc(FALSE);
	db_active--;

	regs->tf_eip    = ddb_regs.tf_eip;
	regs->tf_eflags = ddb_regs.tf_eflags;
	regs->tf_eax    = ddb_regs.tf_eax;
	regs->tf_ecx    = ddb_regs.tf_ecx;
	regs->tf_edx    = ddb_regs.tf_edx;
	regs->tf_ebx    = ddb_regs.tf_ebx;
	if (regs->tf_cs & 0x3) {
	    /*
	     * user mode - saved esp and ss valid
	     */
	    regs->tf_esp = ddb_regs.tf_esp;		/* user stack pointer */
	    regs->tf_ss  = ddb_regs.tf_ss & 0xffff;	/* user stack segment */
	}
	regs->tf_ebp    = ddb_regs.tf_ebp;
	regs->tf_esi    = ddb_regs.tf_esi;
	regs->tf_edi    = ddb_regs.tf_edi;
	regs->tf_es     = ddb_regs.tf_es & 0xffff;
	regs->tf_cs     = ddb_regs.tf_cs & 0xffff;
	regs->tf_ds     = ddb_regs.tf_ds & 0xffff;
#if 0
	regs->tf_fs     = ddb_regs.tf_fs & 0xffff;
	regs->tf_gs     = ddb_regs.tf_gs & 0xffff;
#endif

asm("sahf " : : "a" (sv));
	return (1);
}

/*
 * Print trap reason.
 */
kdbprinttrap(type, code)
	int	type, code;
{
	printf("kernel: ");
	printf("type %d", type);
	printf(" trap, code=%x\n", code);
}

/*
 * Read bytes from kernel address space for debugger.
 */

extern jmp_buf	db_jmpbuf;

void
db_read_bytes(addr, size, data)
	vm_offset_t	addr;
	register int	size;
	register char	*data;
{
	register char	*src;

	db_nofault = &db_jmpbuf;

	src = (char *)addr;
	while (--size >= 0)
	    *data++ = *src++;

	db_nofault = 0;
}

struct pte *pmap_pte(pmap_t, vm_offset_t);

/*
 * Write bytes to kernel address space for debugger.
 */
void
db_write_bytes(addr, size, data)
	vm_offset_t	addr;
	register int	size;
	register char	*data;
{
	register char	*dst;

	register pt_entry_t *ptep0 = 0;
	pt_entry_t	oldmap0 = { 0 };
	vm_offset_t	addr1;
	register pt_entry_t *ptep1 = 0;
	pt_entry_t	oldmap1 = { 0 };
	extern char	etext;

	db_nofault = &db_jmpbuf;

	if (addr >= VM_MIN_KERNEL_ADDRESS &&
	    addr <= (vm_offset_t)&etext)
	{
	    ptep0 = pmap_pte(kernel_pmap, addr);
	    oldmap0 = *ptep0;
	    *(int *)ptep0 |= /* INTEL_PTE_WRITE */ PG_RW;

	    addr1 = i386_trunc_page(addr + size - 1);
	    if (i386_trunc_page(addr) != addr1) {
		/* data crosses a page boundary */

		ptep1 = pmap_pte(kernel_pmap, addr1);
		oldmap1 = *ptep1;
		*(int *)ptep1 |= /* INTEL_PTE_WRITE */ PG_RW;
	    }
	    tlbflush();
	}

	dst = (char *)addr;

	while (--size >= 0)
	    *dst++ = *data++;

	db_nofault = 0;

	if (ptep0) {
	    *ptep0 = oldmap0;
	    if (ptep1) {
		*ptep1 = oldmap1;
	    }
	    tlbflush();
	}
}

Debugger (msg)
char *msg;
{
	asm ("int $3");
}
