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
 *	$Id: locore.s,v 1.1 94/10/19 17:39:58 bill Exp Locker: bill $
 *
 * Assembly code for kernel program entry from bootstrap and kernel
 * primatives.
 */

#include "assym.s"
#include "sys/errno.h"

#include "machine/psl.h"
#include "machine/pmap.h"
#include "machine/trap.h"

#include "specialreg.h"
#include "sel.h"
#include "asm.h"

#include "isa_stdports.h"
#include "isa_irq.h"

	.globl	_KERNBASE
	.set	_KERNBASE, KERNBASE
	.set	IDXSHIFT, 10	/* size of page table indicies */
#define VTOPDR(s)	(((s)>>22) & 0x3FF)
#define PDRTOV(s)	((s)<<22)
	.set	SYSPDROFF, VTOPDR(KERNBASE) /* page dir index of kernel base */

/*
 * PTmap is recursive pagemap at top of virtual address space.
 * Within PTmap, the page directory can be found (third indirection).
 */
	.set	PDRPDROFF, SYSPDROFF - 1 /* page dir index of page dir */
	.globl	_PTmap, _PTD, _PTDpde, _Sysmap
	.set	_PTmap, PDRTOV(PDRPDROFF)
	.set	_PTD, _PTmap | (PDRPDROFF<<12)
	.set	_Sysmap, _PTD + NBPG  /* historical: start of kernel's map */
	.set	_PTDpde, _PTD + 4*PDRPDROFF

/*
 * APTmap is the alternate recursive pagemap.
 * It's used when modifying a process's page tables from a different process.
 */
	.set	APDRPDROFF, 0x3FF	/* page dir index of alt page dir */
	.globl	_APTmap, _APTD, _APTDpde
	.set	_APTmap, 0xFFC00000
	.set	_APTD, 0xFFFFF000
	.set	_APTDpde, _PTD + 4*APDRPDROFF

/*
 * Initialization
 */
	.data
	.globl _Exit_stack, _cold, _boothowto, _bootdev, _atdevbase
	.globl _atdevphys, _zero, _VAKernelPTD, _KernelPTD, _KPTphys

	.space	NBPG
_Exit_stack:	/* Temporary stack to take interrupts on during exit */

_cold:		.long 1	/* cold till we are not */
_atdevbase:	.long 0	/* location of start of iomem in virtual */
_atdevphys:	.long 0	/* location of device mapping ptes (phys) */
_zero:		.long 0
_VAKernelPTD:	.long 0
_KernelPTD:	.long 0
_KPTphys:	.long 0
_bootdev:	.long 0
_boothowto:	.long 0

/*
 * run-time start-off of the kernel. the bootstrap has placed the
 * kernel into consecutive physical memory starting with the beginning
 * of RAM. The processor is in 32-bit flat, protected mode with its 
 * segment selectors referencing segments that correspond to the
 * full 32 bit physical address space. The stack pointer is above
 * the kernel, and it contains three arguments passed in from the
 * bootstrap. All interrupts are disabled.
 */
	.text
	.globl	start
start:
	movw	$0x1234, %ax
	movw	%ax, 0x472	/* warm boot */
	jmp	1f
	.space	0x500		/* skip over bios data region */
 1:

/*
 * pass parameters on stack (howto, bootdev, holestart)
 * note: (%esp) is return address of boot
 * (if we want to hold onto /boot, it's physical %esp up to _end)
 */

	/* movl	12(%esp), %ebp	/* must be obtained first */
	movl	$0x90000, %ebp
	movl	4(%esp), %edx
	movl	$_boothowto-KERNBASE, %eax
	call	reloc
	movl	%edx, (%eax)
	movl	8(%esp), %edx
	movl	$_bootdev-KERNBASE, %eax
	call	reloc
	movl	%edx, (%eax)
	/* movl	$_Exit_stack-KERNBASE, %esp */

	/* count up memory after kernel image */
	movl	$_end - KERNBASE, %eax
	addl	$2*NBPG, %eax	/* add a page for .config --> at _end */
	andl	$~(NBPG - 1), %eax
	call	reloc
	movl	%eax, %edx

	movl	$0, 0		/* insure bottom does not hold pattern */
	movl 	$-NBPG, %ecx
	cmpl	$640*1024 - NBPG, %eax
	jg	1f
	movl 	$640*1024, %ecx

#define	PAT	0xa55a5aa5
1:	movl	$PAT, (%eax)	/* write test pattern */
	cmpl	$PAT, (%eax)	/* does test pattern work? */
	jne	2f
	cmpl	$PAT, 0		/* have we rolled over to bottom of memory ? */
	je	2f
	movl	$0, (%eax)	/* force word to zero */
	addl	$NBPG, %eax
	cmpl	%ecx, %eax
	jne	1b
2:
	cmpl	%eax, %edx	/* nothing in this segment? */
	je	3f

	/* save last page frame */
	shrl	$12, %eax
	movl	%eax, %edx
	movl	$_maxmem - KERNBASE, %eax
	call	reloc
	movl	%edx, (%eax)
3:
	/* need to do extended mem? */
	cmpl	$640*1024, %ecx
	movl	$1*1024*1024, %eax
	movl	%eax, %edx
	movl	$-NBPG, %ecx
	je	1b

	/* find end of kernel text, round to integral page size */
	movl	$_etext-KERNBASE, %edi
	andl	$~(NBPG-1), %edi

	/* find end of kernel image, round to integral page size */
	movl	$_end-KERNBASE,%esi
	addl	$2*NBPG-1,%esi	/* add a page for .config --> at _end */
	andl	$~(NBPG-1),%esi

/* number of pages of system map. n.b. APTmap steals one. */
#define	NSYSMAP	(1024 - SYSPDROFF - 1)

/*
 * initial virtual address space of kernel program:
 * <-(PDRPDR)-> | <--- (SYSPDR) --------------------------------------->
 *  <---maps---> <-kernel program--> <-- initialization/mapping state ->
 * +------------+-------------------+-----------------------------------+
 * |   PTmap    | text | data | bss | page dir | proc0 pcb/stk | Sysmap |
 * +------------+-------------------+-----------------------------------+
 *              ^      ^            ^          ^               ^        ^
 *              |      |            |          |               |        |
 *              |      |            +-- NBPG --+               |        |
 *              |      |            +-- (UPAGES + 1) * NBPG ---+        |
 *              |      |            +-- (UPAGES + NSYSMAP + 1) * NBPG --+
 * KERNBASE: ---+      |            |                                   |
 * offset from %edi: --+            |                                   |
 * offset from %esi: ---------------+                                   |
 * start of managed memory(firstpa/vmfirstsva): ------------------------+
 *
 * n.b: physical pages allocated successively, skipping hole.
 */

/*
 * record details of initial kernel map in global variables
 */

	/* physical address of Kernel PTD */
	movl	%esi, %eax
	call	reloc
	movl	%eax, %edx
	movl	$_KernelPTD-KERNBASE, %eax
	call	reloc
	movl	%edx, (%eax)

	/* virtual address of Kernel PTD */
	movl	%esi, %edx
	addl	$KERNBASE, %edx
	movl	$_VAKernelPTD-KERNBASE, %eax
	call	reloc
	movl	%edx, (%eax)

	/* logical address of proc 0 pcb/stk */
	lea	NBPG(%esi), %eax
	lea	KERNBASE(%eax), %edx
	movl	$_proc0paddr - KERNBASE, %eax
	call	reloc
	movl	%edx, (%eax)

	/* logical address of kernel PT */
	lea	(UPAGES+1)*NBPG(%esi), %ebx
	movl	$_KPTphys - KERNBASE, %eax
	call	reloc
	movl	%ebx, (%eax)

/*
 * map kernel. first step - build page tables.
 */

	/* kernel text pages */
	movl	%edi, %ecx		/* this much virtual address space */
	shrl	$PGSHIFT, %ecx
	movl	$PG_V|PG_KR, %edx	/* read-only (for 486) */
	xorl	%eax, %eax		/*  starting at this address - 0 XXX */
	call	rfillkpt

	/* kernel data pages - covers data, bss, and initial state */
	movl	%esi, %ecx
	subl	%edi, %ecx		/* remaining data pages */
	shrl	$PGSHIFT, %ecx
	addl	$UPAGES+1+NSYSMAP, %ecx /* include initial state */
	movl	$PG_V|PG_KW, %edx	/* read-write */
	call	rfillkpt

	/* map I/O memory map XXX */
	movl	$_atdevphys-KERNBASE, %eax /* record relative pte base */
	call	reloc
	movl	%ebx, (%eax)
	movl	$0x100-0xa0, %ecx	/* pages to map hole */
	movl	$PG_V|PG_KW, %edx	/* read-write */
	movl	$0xa0000, %eax		/* beginning with the base of the hole */
	call	fillkpt

/*
 * Construct a page table directory
 */
	/* install an entry for a temporary double map of bottom of VA */
	lea	(UPAGES+1)*NBPG(%esi), %eax /* logical address of kernel page table */
	movl	$PG_V|PG_KW, %edx	/* read-write */
	movl	$1, %ecx		/* a single entry */
	movl	%esi, %ebx		/* at the start of the page directory */
	call	rfillkpt

	/* install a set of entries for the kernel's page tables */
	lea	(UPAGES+1)*NBPG(%esi), %eax /* logical address of kernel page table */
	movl	$NSYSMAP, %ecx		/* for this many directory entries */
	lea	SYSPDROFF*4(%esi), %ebx	/* offset into page directory of kernel program */
	call	rfillkpt

	/* install an entry recursively mapping the page directory as a page table */
	movl	%esi, %eax		/* logical address of PTD in proc 0 */
	movl	$1, %ecx		/* a single entry */
	lea	PDRPDROFF*4(%esi), %ebx	/* below the kernel entries */
	call	rfillkpt

	/* load base of page directory, and enable mapping */
	movl	%esi, %eax		/* logical address of PTD in proc 0 */
	call	reloc
	movl	%eax, %ebx
	movl	%eax, %cr3		/* load ptd addr into mmu */
	movl	%cr0, %eax		/* get control word */
	orl	$CR0_PG, %eax		/* turn on paging mode bit */
	movl	%eax, %cr0		/* enter paged mode with this instr. */

	/* assign stack for proc 0, at the end of it's pcb */
	movl	_proc0paddr, %eax
 	addl	$UPAGES*NBPG-4*12, %eax /* XXX 4*12 is for printf's sake */
 	movl	%eax, %esp

	/* relocate program counter to high addresses */
	pushl	$begin
	ret

begin:
	/* now running relocated at KERNBASE where the system is linked to run */

	/* XXX shuffle console address mapping */
	.globl _Crtat
	movl	_Crtat, %eax
	subl	$0xfe0a0000, %eax
	movl	_atdevphys, %edx	/* get pte PA */
	subl	_KPTphys, %edx		/* remove virt base of ptes */
	shll	$PGSHIFT-2, %edx
	addl	$KERNBASE, %edx
	movl	%edx, _atdevbase	/* save virt addr */
	addl	%eax, %edx
	movl	%edx, _Crtat

	/* clear bss */
	movl	$_end, %ecx /* find end of kernel image */
	movl	$_edata, %edi
	subl	%edi, %ecx
	xorl	%eax, %eax
	cld
	rep
	stosb

	/* clear first kernel stack */
	movl	$(UPAGES * NBPG)/4, %ecx
	movl	_proc0paddr, %edi
	xorl	%eax, %eax
	cld
	rep
	stosl

	/* locate first page after intialization state, pass as argument */
	lea	(UPAGES+1+NSYSMAP)*NBPG(%esi), %eax
	call	reloc
	pushl	%eax

	/* set up bootstrap frame */
	xorl	%eax, %eax
	movl	%eax, %ebp	/* top most frame pointer points to unmapped */
	movw	%ax, %fs	/* bootstrap could have used these, zero */
	movw	%ax, %gs

	movl	$0, _PTD /* blow away temporary "low" directory entry? */

	/* bootstrap processor - void init386(int firstphys) */
	call	_init386
	popl	%esi

	/* void main(void) */
	call 	_main

	.globl	__ucodesel, __udatasel
	movzwl	__ucodesel, %eax
	movzwl	__udatasel, %ecx

	/* build an outer stack frame to enter user mode to execute proc 0 */
	pushl	%ecx		/* user ss */
	pushl	$USRSTACK - 4	/* user esp */
	pushl	%eax		/* user cs */
	pushl	$0		/* user ip */
	movw	%cx, %ds	/* user ds */
	movw	%cx, %es	/* user es */
	movw	%cx, %gs	/* XXX and ds to gs */
 	lret			/* goto user */

#ifdef	DIAGNOSTIC
	pushl	$1f	/* lret failed - "should never get here!" */
	call	_panic
1:
	.asciz	"lret"
#endif

/*
 * build and install a portion of the initial kernel page table.
 */

#define	ENDHOLE	0x100000
	/* relocate a logical address if "hole" is encountered */
reloc:
	cmpl	%eax, %ebp /* if at or above start of hole, */
	ja	1f
	subl	%ebp, %eax /* use physical pages above hole */
	addl	$ENDHOLE, %eax
1:	ret

	/* fill a set of pagetables or page directory entrys using
	   logical addresses for both page frame and pte addresses */
rfillkpt:
	pushl	%ebx		/* save relative physical address "pointers" */
	pushl	%eax

	movl	%ebx, %eax	/* do logical address reloc for pte addr */
	call	reloc
	movl	%eax, %ebx

	popl	%eax		/* restore page address */
	pushl	%eax		/* and relocate logical page address */
	call	reloc

	movl	%eax, (%ebx)	/* stuff pte */
	orl	%edx, (%ebx)	/* add protection bits to pte */

	popl	%eax
	popl	%ebx
	addl	$NBPG, %eax	/* increment logical page frame address */
	addl	$4, %ebx	/* and look to next logical pte */
	loop	rfillkpt
	ret

	/* fill a set of pagetables or page directory entrys using
	   physical address for the page frame and logical pte address */
fillkpt:
	pushl	%ebx		/* save relative physical address "pointers" */
	pushl	%eax

	movl	%ebx, %eax	/* do physical address reloc for pte addr */
	call	reloc
	movl	%eax, %ebx
	popl	%eax

	movl	%eax, (%ebx)	/* stuff pte */
	orl	%edx, (%ebx)	/* add protection bits to pte */

	addl	$NBPG, %eax	/* increment physical address */
	popl	%ebx
	addl	$4, %ebx	/* next pte */
	loop	fillkpt
	ret

.globl	___main
___main:	ret		/* gcc 2 errata */

	.set	execve, 59
	.set	exit, 1
	.globl	_icode
	.globl	_szicode

/*
 * Icode is copied out to process 1 to exec /sbin/init.
 * If the execve() fails, process 1 exits.
 */
_icode:
	pushl	$0		/* empty env, for now */
	movl	$argv, %eax
	subl	$_icode, %eax
	pushl	%eax

	movl	$init, %eax
	subl	$_icode, %eax
	pushl	%eax

	pushl	%eax		/* place holder for return address */

	movl	%esp, %ebp
	movl	$execve, %eax
	lcall   $iBCSSYSCALLSEL, $0	/* execve("/sbin/init", argv, 0); */
	pushl	$0
	movl	$exit, %eax
	pushl	%eax		/* place holder for return address */
	lcall	$iBCSSYSCALLSEL, $0	/* exit(0); */

init:
	.asciz	"/sbin/init"
	.align	2
argv:
	.long	init+6-_icode	/* argv[0] = "init" ("/sbin/init" + 6) */
	.long	eicode - _icode	/* argv[1] follows icode after copyout */
	.long	0
eicode:	/* main appends a string(s) here ... */

_szicode:
	.long	_szicode - _icode

/*
 * Code placed in each process (top of stack) to bounce signal into C,
 * and force a re-entry  into the kernel with a system call.
 */
	.globl	_sigcode, _szsigcode
_sigcode:
	call	12(%esp)
	xorl	%eax, %eax	/* smaller movl $103, %eax */
	movb	$103, %al
	/* enter kernel with args on stack */
	lcall	$iBCSSYSCALLSEL, $0	/* sigreturn(); */
	hlt			/* never gets here */

_szsigcode:
	.long	_szsigcode - _sigcode

/*
 * Convienent function to convert a descriptor from a software palitable
 * form into a hardware usable form:
 *         ssdtosd(*ssdp, *sdp)
 */
ENTRY(ssdtosd)
	pushl	%ebx
	movl	8(%esp),%ecx
	movl	8(%ecx),%ebx
	shll	$16,%ebx
	movl	(%ecx),%edx
	roll	$16,%edx
	movb	%dh,%bl
	movb	%dl,%bh
	rorl	$8,%ebx
	movl	4(%ecx),%eax
	movw	%ax,%dx
	andl	$0xf0000,%eax
	orl	%eax,%ebx
	movl	12(%esp),%ecx
	movl	%edx,(%ecx)
	movl	%ebx,4(%ecx)
	popl	%ebx
	ret

#define	PROC_ENTRY	pushl %ebp ; movl %esp,%ebp
#define	PROC_EXIT	leave
/*
 * Make up for the lack of write protection from the kernel by
 * simulating the correct fault in writes to the user process.
 * Otherwise, use the inline functions for access to the user
 * process (see machine/inline/kernel.h).
 */
#if !defined(i486)

/*
 * Copy out a 4 byte value to a user process:
 * int copyout_4(int value, void *addr);
 */
ENTRY(copyout_4)
	movl	_curproc, %ecx
	movl	$copyout_fault1, PMD_ONFAULT(%ecx)	/* in case of fault */
	movl	8(%esp), %edx
	cmpl	$_PTmap, %edx	/* out of user space */
	jae	copyout_fault1

	shrl	$IDXSHIFT, %edx	/* fetch pte associated with address */
	andb	$0xfc, %dl
	movl	_PTmap(%edx), %edx

	andb	$7, %dl		/* if we are the one case that won't trap... */
	cmpb	$5, %dl
	jne	1f
				/* ... then simulate the trap! */
	pushl	8(%esp)
	call	_trapwrite	/* trapwrite(addr) */
	popl	%edx
	movl	_curproc, %ecx
	cmpl	$0, %eax	/* if not ok, return */
	jne	copyout_fault1
1:
	movl	8(%esp), %edx
	movl	4(%esp), %eax
	movl	%eax, (%edx)
	xorl	%eax, %eax
	movl	%eax, PMD_ONFAULT(%ecx)
	ret
	
/*
 * Copy out a 2 byte value to a user process:
 * int copyout_2(short value, void *addr);
 */
ENTRY(copyout_2)
	movl	_curproc, %ecx
	movl	$copyout_fault1, PMD_ONFAULT(%ecx)	/* in case of fault */
	movl	8(%esp), %edx
	cmpl	$_PTmap, %edx	/* out of user space */
	jae	copyout_fault1

	shrl	$IDXSHIFT, %edx	/* calculate pte address */
	andb	$0xfc, %dl
	movl	_PTmap(%edx), %edx
	andb	$7, %dl		/* if we are the one case that won't trap... */
	cmpb	$5, %dl
	jne	1f
				/* ..., then simulate the trap! */
	pushl	8(%esp)
	call	_trapwrite	/* trapwrite(addr) */
	popl	%edx
	movl	_curproc, %ecx	# restore trashed registers
	cmpl	$0, %eax	/* if not ok, return */
	jne	copyout_fault1
1:
	movl	8(%esp), %edx
	movl	4(%esp), %eax
	movw	%ax, (%edx)
	xorl	%eax, %eax
	movl	%eax, PMD_ONFAULT(%ecx)
	ret

/*
 * Copy out a 1 byte value to a user process:
 * int copyout_1(short value, void *addr);
 */
ENTRY(copyout_1)
	movl	_curproc,%ecx
	movl	$copyout_fault1, PMD_ONFAULT(%ecx)	/* in case of fault */
	movl	8(%esp),%edx
	cmpl	$_PTmap, %edx	/* out of user space */
	jae	copyout_fault1

	shrl	$IDXSHIFT, %edx	/* calculate pte address */
	andb	$0xfc, %dl
	movl	_PTmap(%edx), %edx
	andb	$7, %dl		/* if we are the one case that won't trap... */
	cmpb	$5, %dl
	jne	1f
				/* ..., then simulate the trap! */
	pushl	8(%esp)
	call	_trapwrite	/* trapwrite(addr) */
	popl	%edx
	movl	_curproc, %ecx	# restore trashed registers
	cmpl	$0, %eax	/* if not ok, return */
	jne	copyout_fault1
1:
	movl	8(%esp),%edx
	movl	4(%esp),%eax
	movb	%al, (%edx)
	xorl	%eax, %eax
	movl	%eax, PMD_ONFAULT(%ecx)
	ret

copyout_fault1:
	xorl	%eax, %eax
	movl	%eax, PMD_ONFAULT(%ecx) 
	movb	$EFAULT, %al
	ret

/*
 * Copy a kernel string to a user process:
 * int copyoutstr(struct proc *p, void *from, void *to, u_int size,
 * 	u_int *lencopied);
 */
ENTRY(copyoutstr)
	movl	4(%esp), %eax	/* p */
	movl	$9f, PMD_ONFAULT(%eax)  /* in case we fault */
	pushl	%esi
	pushl	%edi
	pushl	%ebx
	movl	20(%esp), %esi	/* from */
	movl	24(%esp), %edi	/* to */
	movl	28(%esp), %ebx	/* size */

	cmpl	$_PTmap, %edi	/* out of user space */
	jae	9f
	movl	%edi, %eax
	addl	%ebx, %eax
	cmpl	$_PTmap, %eax
	jae	9f
 				/* first, check to see if "write fault" */
1:	movl	%edi, %eax
	shrl	$IDXSHIFT, %eax	/* fetch pte associated with address */
	andb	$0xfc, %al
	movl	_PTmap(%eax), %eax

	andb	$7, %al		/* if we are the one case that won't trap... */
	cmpb	$5, %al
	jne	2f
				/* ... then simulate the trap! */
	pushl	%edi
	call	_trapwrite	/* trapwrite(addr) */
	popl	%eax

	cmpl	$0, %eax	/* if not ok, return */
	jne	9f
				/* otherwise, continue with reference */
2:	movl	%edi, %eax	/* calculate the remainder in this page */
	andl	$NBPG-1, %eax
	movl	$NBPG, %ecx
	subl	%eax, %ecx
	cmpl	%ecx, %ebx
	jg	3f
	movl	%ebx, %ecx
3:	subl	%ecx, %ebx

	cld			/* process a dest. page fragement */
5:	cmpb	$0, (%esi)
	movsb
	loopne	5b
	lahf
	je	4f		/* null? normal termination. */

	cmpl	$0, %ebx	/* any remainder? */
	jne	1b

	sahf
	jne     7f		/* out of space */

	/* found null, normal termination */
4:	xorl	%eax, %eax

	/* termination processing. */
8:
	popl	%ebx
	popl	%edi
	popl	%esi
	movl	4(%esp), %edx	/* p */
	movl	$0, PMD_ONFAULT(%edx)

	/* termination count needs to be returned? */
	movl	20(%esp), %edx	/* lencopied */
	cmpl	$0, %edx
	je	1f
	subl	%ecx, 16(%esp)	/* size */
	movl	16(%esp), %ecx
	movl	%ecx, (%edx)
1:	ret

7:	movl	$ENAMETOOLONG, %eax /* ran out of space in user process */
	jmp	8b

9:	movl	$EFAULT, %eax /* detected a fault, return error */
	jmp	8b

/*
 * Copy from kernel to user process:
 * int copyout(struct proc *p, void *ksrc, void *udest, u_in sz);
 */
ENTRY(copyout)
	movl	4(%esp), %eax	/* p */
	movl	$9f, PMD_ONFAULT(%eax)  /* in case we fault */
	pushl	%esi
	pushl	%edi
	pushl	%ebx
	movl	20(%esp), %esi	/* ksrc */
	movl	24(%esp), %edi	/* udest */
	movl	28(%esp), %ebx	/* sz */

	cmpl	$_PTmap, %edi	/* out of user space */
	jae	9f
	movl	%edi, %eax
	addl	%ebx, %eax
	cmpl	$_PTmap, %eax
	jae	9f
 				/* first, check to see if "write fault" */
1:	movl	%edi, %eax
	shrl	$IDXSHIFT, %eax	/* fetch pte associated with address */
	andb	$0xfc, %al
	movl	_PTmap(%eax), %eax

	andb	$7, %al		/* if we are the one case that won't trap... */
	cmpb	$5, %al
	jne	2f
				/* ... then simulate the trap! */
	pushl	%edi
	call	_trapwrite	/* trapwrite(addr) */
	popl	%eax

	cmpl	$0, %eax	/* if not ok, return */
	je	9f
				/* otherwise, continue with reference */
2:	movl	%edi, %eax	/* calculate the remainder in this page */
	andl	$NBPG-1, %eax
	movl	$NBPG, %ecx
	subl	%eax, %ecx
	cmpl	%ecx, %ebx
	jg	3f
	movl	%ebx, %ecx
3:	subl	%ecx, %ebx
	movl	%ecx, %edx

	shrl	$2, %ecx	/* move a page fragment */
	cld; rep; movsl
	movl	%edx, %ecx
	andl	$3, %ecx
	rep; movsb

	cmpl	$0, %ebx	/* any remainder? */
	jne	1b

	popl	%ebx
	popl	%edi
	popl	%esi
	xorl	%eax, %eax
	movl	4(%esp), %edx
	movl	%eax, PMD_ONFAULT(%edx)
	ret

9:	popl	%ebx		/* detected a fault, return error */
	popl	%edi
	popl	%esi
	movl	4(%esp), %edx
	xorl	%eax, %eax
	movl	%eax, PMD_ONFAULT(%edx)
	movl	$EFAULT, %eax
	ret
#endif

/*
 * The following primitives manipulate the run queues.
 * _whichqs tells which of the 32 queues _qs
 * have processes in them.  Setrq puts processes into queues, Remrq
 * removes them from queues.  The running process is on no queue,
 * other processes are on a queue related to p->p_pri, divided by 4
 * actually to shrink the 0-127 range of priorities into the 32 available
 * queues.
 */

	.globl	_whichqs, _qs, _cnt, _panic
	.comm	_noproc, 4
/*
 * When no processes are on the runq, swtch() branches to idle
 * to wait for something to come ready.
 */
	.globl	Idle
	.align 4
Idle:
	call	_splnone	/* idle with all interrupts unmasked */
	cmpl	$0, _whichqs	/* if any process ready on queue, */
	jne	swtch_selq	/*  reenter scheduler after saves. */
	cmpl	$0, _dma_active	/* don't use hlt if DMA is active */
	jne	Idle
	hlt			/* wait for interrupt */
	jmp	Idle

/*
 * void swtch(void)
 * volatile void final_swtch(void)
 */
ENTRY(swtch)
ALTENTRY(final_swtch)
	PROC_ENTRY
	incl	_cnt+V_SWTCH

	/* mark as not in a process, remembering "old" process */
	xorl	%eax, %eax
	xchgl	%eax, _curproc
	pushl	%eax

	/* now find highest priority full process queue or idle */
	pushl %ebx
	pushl %esi

swtch_selq:
	bsfl	_whichqs, %eax		/* find a full queue */
	jz      Idle                    /* if none, idle */

	btrl	%eax, _whichqs		/* clear queue full status */
	jnc	swtch_selq		/* if it was clear, look for another */
	cli				/* interrupts off until after swtch() */
	movl	%eax, %ebx		/* save queue head index */
	shll	$3, %eax		/* select associated queue */
	addl	$_qs, %eax
	movl	%eax, %esi		/* save queue head pointer */

#ifdef	DIAGNOSTIC
	/* check if the queue head is linked to itself (empty) */
	cmpl	P_LINK(%eax), %eax
	je	2f
#endif

	/* unlink first process attached to the queue head */
	movl	P_LINK(%eax), %ecx
	movl	P_LINK(%ecx), %edx
	movl	%edx, P_LINK(%eax)
	movl	P_RLINK(%ecx), %eax
	movl	%eax, P_RLINK(%edx)

	/* if queue is still full, set full status in queue status word */
	cmpl	P_LINK(%ecx), %esi
	je	1f
	btsl	%ebx, _whichqs
1:

	xorl	%eax, %eax

#ifdef	DIAGNOSTIC
	/* process should not be sleeping but runnable */
	cmpl	%eax, P_WCHAN(%ecx)
	jne	2f
	cmpb	$SRUN, P_STAT(%ecx)
	jne	2f
#endif

	movl	%eax, P_RLINK(%ecx) /* isolate process to run */

	/* recover state of previous register contents */
	popl %esi
	popl %ebx

	/* recover previous process pointer */
	popl  %eax
	PROC_EXIT

	/* if process is switching to itself then don't ljmp */
	cmpl	%ecx, %eax
	je	1f

	ljmp	PMD_TSEL - 4(%ecx)
1:	movl	%eax, _curproc		/* into next process */
	ret

#ifdef DIAGNOSTIC
2:
	pushl	$3f
	call	_panic
3:
	.asciz	"swtch"
#endif

/*
 * void qswtch(void);
 *
 * Call should be made at splclock(), and p->p_stat should be SRUN.
 */
ENTRY(qswtch)
	PROC_ENTRY
	incl	_cnt+V_SWTCH

	/* mark as not in a process, remembering "old" process */
	xorl	%eax, %eax
	xchgl	%eax, _curproc
	pushl	%eax

	pushl %ebx
	pushl %esi

	cli
	/* stick on a queue */
#ifdef	DIAGNOSTIC
	cmpl	$0, P_RLINK(%eax)	/* should not be on q already */
	jne	1f
#endif
	andl	$~MDP_RESCHED, PMD_FLAGS(%eax)
	movzbl	P_PRI(%eax), %edx
	shrl	$2, %edx
	btsl	%edx, _whichqs		/* set q full bit */
	shll	$3, %edx
	addl	$_qs, %edx		/* locate q hdr */
	movl	%edx, P_LINK(%eax)	/* link process on tail of q */
	movl	P_RLINK(%edx), %ecx
	movl	%ecx, P_RLINK(%eax)
	movl	%eax, P_RLINK(%edx)
	movl	%eax, P_LINK(%ecx)
	jmp	swtch_selq

#ifdef	DIAGNOSTIC
1:	pushl	$2f
	call	_panic
2:
	.asciz	"qswtch"
#endif

	.data
	.globl	_proc0paddr
_proc0paddr:	.long	0	/* process 0 address of kstack. */
	.space 4
	.globl	_intrnames, _eintrnames, _intrcnt, _eintrcnt
_intrnames:	.space 16*4
_eintrnames:
_intrcnt:	.space 16*4
_eintrcnt:

	.text
/*
 * Trap Vector Table (IDT 0 - 31)
 *
 * Transfer from exception to common entry
 * saving fault number. Prepend dummy error code to exceptions
 * lacking a code so that frame is consistant size.
 * These can be in any order, since a trap gate for each needs
 * to be assigned (see machdep.c) to tell the processor how to
 * associate an exception with these entry points.
 */

/* Trap and fault vector macros.  */ 
#define	IDTVEC(name)	.align 4; .globl _X/**/name; _X/**/name:
#define	TRAP(a)		pushl $(a) ; jmp common_traps

IDTVEC(div)	pushl $0; TRAP(T_DIVIDE)
IDTVEC(dbg)	pushl $0; TRAP(T_TRCTRAP)
IDTVEC(nmi)	pushl $0; TRAP(T_NMI)
IDTVEC(bpt)	pushl $0; TRAP(T_BPTFLT)
IDTVEC(ofl)	pushl $0; TRAP(T_OFLOW)
IDTVEC(bnd)	pushl $0; TRAP(T_BOUND)
IDTVEC(ill)	pushl $0; TRAP(T_PRIVINFLT)
IDTVEC(dna)	pushl $0; TRAP(T_DNA)
IDTVEC(dble)	TRAP(T_DOUBLEFLT)
IDTVEC(fpusegm)	pushl $0; TRAP(T_FPOPFLT)
IDTVEC(tss)	TRAP(T_TSSFLT)
IDTVEC(missing)	TRAP(T_SEGNPFLT)
IDTVEC(stk)	TRAP(T_STKFLT)
IDTVEC(prot)	TRAP(T_PROTFLT)
IDTVEC(page)	TRAP(T_PAGEFLT)
IDTVEC(rsvd)	pushl $0; TRAP(T_RESERVED)
IDTVEC(fpu)	pushl $0; TRAP(T_ARITHTRAP)
	/* 17 - 31 reserved for future exp */
IDTVEC(rsvd0)	pushl $0; TRAP(17)
IDTVEC(rsvd1)	pushl $0; TRAP(18)
IDTVEC(rsvd2)	pushl $0; TRAP(19)
IDTVEC(rsvd3)	pushl $0; TRAP(20)
IDTVEC(rsvd4)	pushl $0; TRAP(21)
IDTVEC(rsvd5)	pushl $0; TRAP(22)
IDTVEC(rsvd6)	pushl $0; TRAP(23)
IDTVEC(rsvd7)	pushl $0; TRAP(24)
IDTVEC(rsvd8)	pushl $0; TRAP(25)
IDTVEC(rsvd9)	pushl $0; TRAP(26)
IDTVEC(rsvd10)	pushl $0; TRAP(27)
IDTVEC(rsvd11)	pushl $0; TRAP(28)
IDTVEC(rsvd12)	pushl $0; TRAP(29)
IDTVEC(rsvd13)	pushl $0; TRAP(30)
IDTVEC(rsvd14)	pushl $0; TRAP(31)

/* common code for all traps. */
common_traps:
	pushal
	nop
	push %ds
	push %es
	movw	$KDSEL, %ax
	movw	%ax,%ds
	movw	%ax,%es
	incl	_cnt+V_TRAP
	call	_trap
	movl	_cpl, %ecx
	jmp	trap_exit

/*
 * Interrupts, otherwise known as asynchronous traps.
 */
	/* hardware interrupt catcher (IDT 32 - 47) */
	.globl	_isa_strayintr

/*
 * Macro's for interrupt level priority masks (used in interrupt vector entry)
 */

#define	WUP(n)	\
	pushl %eax  ; \
	pushl $n  ; \
	inb $IO_ICU2+1, %al ; \
	movb %al,%ah ; \
	inb $IO_ICU1+1, %al ; \
	pushl %eax ; \
	call _wpl ; \
	popl %eax ;  popl %eax ;  popl %eax ; 

/* If this is an old 386 that requires recovery time on the 8259, add nops */
#define	NOP	/* none with modern machines */
/* #define	NOP	inb $0x84, %al ; /* some with old machines */

/* Processor/Bus chip set has write buffers to be forced out. */
#define	WRPOST	inb $0x84, %al

#define	INTRSTRAY1(unit, mask, offst) \
	pushl	$0 ; \
	pushl	$T_ASTFLT ; \
	pushal ; \
	NOP ;			/* wait for 8259 recovery */ \
	movb	$3, %al ; 	/* select ISR ... */ \
	outb	%al, $IO_ICU1 ; /* in 8259 unit 1 ...*/ \
	NOP ;			/* wait for 8259 recovery */ \
	inb	$IO_ICU1, %al ;	/* grab ISR */ \
	movb	%al, %dl ;	/* save ISR */ \
	NOP ;			/* ... ASAP */ \
	movb	$2, %al ; 	/* reselect IRR ... */ \
	outb	%al, $IO_ICU1 ; /* ... ...*/ \
	NOP ;			/* ... ASAP */ \
	movb	$(0x20 | unit), %al ; 	/* next, as soon as possible send EOI ... */ \
	outb	%al, $IO_ICU1 ; /* ... so in service bit may be cleared ...*/ \
	NOP ;			/* wait for 8259 recovery */ \
	pushl	%ds ; 		/* save our data and extra segments ... */ \
	pushl	%es ; \
	movw	$KDSEL, %ax ;	/* ... and reload with kernel's own */ \
	movw	%ax, %ds ; \
	movw	%ax, %es ; \
	movl	_cpl, %eax ; \
	pushl	%eax ; \
	shll	$8, %edx ; \
	movb	$unit, %dl ; \
	pushl	%edx ; \
	orw	mask, %ax ; \
	movl	%eax,_cpl ; \
	orw	_imen, %ax ; \
	outb	%al, $IO_ICU1+1 ; \
	NOP ;			/* wait for 8259 recovery */ \
	movb	%ah, %al ; \
	outb	%al, $IO_ICU2+1	; \
	NOP ;			/* wait for 8259 recovery */ \
	WRPOST ; /* do write post */ \
	sti ; \
	call	_isa_strayintr ; \
	jmp	common_int_return

#define	INTRSTRAY2(unit, mask, offst) \
	pushl	$0 ; \
	pushl	$T_ASTFLT ; \
	pushal ; \
	NOP ;			/* wait for 8259 recovery */ \
	movb	$3, %al ; 	/* select ISR ... */ \
	outb	%al, $IO_ICU2 ; /* in 8259 unit 2 ...*/ \
	NOP ;			/* wait for 8259 recovery */ \
	inb	$IO_ICU2, %al ;	/* grab ISR */ \
	xorb	%dl, %dl ;	 \
	movb	%al, %dh ;	/* save ISR */ \
	NOP ;			/* ... ASAP */ \
	movb	$2, %al ; 	/* reselect IRR ... */ \
	outb	%al, $IO_ICU2 ; /* ... ...*/ \
	movb	$0x62, %al ; 	/* next, as soon as possible send EOI ... */ \
	outb	%al, $IO_ICU1 ; /* ... so in service bit may be cleared ...*/ \
	NOP ;			/* wait for 8259 recovery */ \
	movb	$(0x60|(unit-8)), %al ; 	/* next, as soon as possible send EOI ... */ \
	outb	%al, $IO_ICU1 ; /* ... so in service bit may be cleared ...*/ \
	outb	%al, $IO_ICU2 ; /* ... ...*/ \
	NOP ;			/* wait for 8259 recovery */ \
	pushl	%ds ; 		/* save our data and extra segments ... */ \
	pushl	%es ; \
	movw	$KDSEL, %ax ;	/* ... and reload with kernel's own */ \
	movw	%ax, %ds ; \
	movw	%ax, %es ; \
	movl	_cpl, %eax ; \
	pushl	%eax ; \
	shll	$8, %edx ; \
	movb	$unit, %dl ; \
	pushl	%edx ; \
	orw	mask, %ax ; \
	movl	%eax,_cpl ; \
	orw	_imen, %ax ; \
	outb	%al, $IO_ICU1+1 ; \
	NOP ;			/* wait for 8259 recovery */ \
	movb	%ah, %al ; \
	outb	%al, $IO_ICU2+1	; \
	WRPOST ; /* do write post */ \
	sti ; \
	call	_isa_strayintr ; \
	jmp	common_int_return

/*
 * Stray Interrupt Catcher Table
 */
IDTVEC(intr0)	INTRSTRAY1(0, _highmask, 0)
IDTVEC(intr1)	INTRSTRAY1(1, _highmask, 1)
IDTVEC(intr2)	INTRSTRAY1(2, _highmask, 2)
IDTVEC(intr3)	INTRSTRAY1(3, _highmask, 3)
IDTVEC(intr4)	INTRSTRAY1(4, _highmask, 4)
IDTVEC(intr5)	INTRSTRAY1(5, _highmask, 5)
IDTVEC(intr6)	INTRSTRAY1(6, _highmask, 6)
IDTVEC(intr7)	INTRSTRAY1(7, _highmask, 7)
IDTVEC(intr8)	INTRSTRAY2(8, _highmask, 8)
IDTVEC(intr9)	INTRSTRAY2(9, _highmask, 9)
IDTVEC(intr10)	INTRSTRAY2(10, _highmask, 10)
IDTVEC(intr11)	INTRSTRAY2(11, _highmask, 11)
IDTVEC(intr12)	INTRSTRAY2(12, _highmask, 12)
IDTVEC(intr13)	INTRSTRAY2(13, _highmask, 13)
IDTVEC(intr14)	INTRSTRAY2(14, _highmask, 14)
IDTVEC(intr15)	INTRSTRAY2(15, _highmask, 15)

/* all interrupts after first 16 */
IDTVEC(intrdefault) INTRSTRAY2(255, _highmask, 255)

/*
 * 386 ISA 
 * Interrupt vector interface.
 */ 

/* Mask a group of interrupts atomically */
#define	INTR1(offst) \
	pushl	$0; \
	pushl	$T_ASTFLT; \
	pushal; \
	NOP ; \
	movb	$(0x60|offst), %al; 	/* next, as soon as possible send EOI ... */ \
	outb	%al, $IO_ICU1; /* ... so in service bit may be cleared ...*/ \
	pushl	%ds; 		/* save our data and extra segments ... */ \
	pushl	%es; \
	movw	$0x10, %ax;	/* ... and reload with kernel's own */ \
	movw	%ax, %ds; \
	movw	%ax, %es; \
	incl	_cnt+V_INTR;	/* tally interrupts */ \
	incl	_intrcnt+offst*4; \
	movl	_cpl, %eax; \
	pushl	%eax; \
	pushl	_isa_unit+offst*4; \
	orw	_isa_mask+offst*2, %ax; \
	movl	%eax, _cpl; \
	orw	_imen, %ax; \
	NOP ; \
	outb	%al, $IO_ICU1+1; \
	movb	%ah, %al; \
	outb	%al, $IO_ICU2+1; \
	WRPOST ; /* do write post */ \
	sti; \
	call	*(_isa_vec+(offst*4)); \
	jmp	common_int_return

/* Mask a group of interrupts atomically */
#define	INTR2(offst) \
	pushl	$0; \
	pushl	$T_ASTFLT; \
	pushal; \
	NOP ; \
	movb	$(0x60|(offst-8)), %al; 	/* next, as soon as possible send EOI ... */ \
	outb	%al, $IO_ICU2; \
	movb	$0x62, %al; 	/* next, as soon as possible send EOI ... */ \
	outb	%al, $IO_ICU1; /* ... so in service bit may be cleared ...*/ \
	pushl	%ds; 		/* save our data and extra segments ... */ \
	pushl	%es; \
	movw	$0x10, %ax;	/* ... and reload with kernel's own */ \
	movw	%ax, %ds; \
	movw	%ax, %es; \
	incl	_cnt+V_INTR;	/* tally interrupts */ \
	incl	_intrcnt+offst*4; \
	movl	_cpl,%eax; \
	pushl	%eax; \
	pushl	_isa_unit+offst*4; \
	orw	_isa_mask+offst*2, %ax; \
	movl	%ax, _cpl; \
	orw	_imen, %ax; \
	outb	%al, $IO_ICU1+1; \
	NOP ; \
	movb	%ah, %al; \
	outb	%al, $IO_ICU2+1; \
	WRPOST ; /* do write post */ \
	sti; \
	call	*(_isa_vec+offst*4); \
	jmp	common_int_return

/*
 * Interrupt vector table:
 */
IDTVEC(irq0)	INTR1(0)
IDTVEC(irq1)	INTR1(1)
IDTVEC(irq2)	INTR1(2)
IDTVEC(irq3)	INTR1(3)
IDTVEC(irq4)	INTR1(4)
IDTVEC(irq5)	INTR1(5)
IDTVEC(irq6)	INTR1(6)
IDTVEC(irq7)	INTR1(7)
IDTVEC(irq8)	INTR2(8)
IDTVEC(irq9)	INTR2(9)
IDTVEC(irq10)	INTR2(10)
IDTVEC(irq11)	INTR2(11)
IDTVEC(irq12)	INTR2(12)
IDTVEC(irq13)	INTR2(13)
IDTVEC(irq14)	INTR2(14)
IDTVEC(irq15)	INTR2(15)

	.data
	.globl	_imen, _cpl
	.long 0
_cpl:	.long	IRHIGH - 2	/* current priority level (all off) */
_imen:	.word	0xffff - 2	/* interrupt mask enable (all off) */

netsirqsem: .word 0	/* network software interrupt request semaphore */

	.globl	_highmask
_highmask:	.long	IRHIGH - 2
	.globl	_ttymask
_ttymask:	.long	0
	.globl	_biomask
_biomask:	.long	0
	.globl	_netmask
_netmask:	.long	0

/*
 * low-level interrupt semaphore operations
 */
#define	P(s, l)	\
	movb	$1, %bl ; \
	xchgb	%bl, s ;  \
	testb	%bl, %bl ; \
	jne	l

#define	V(s)	\
	movb	$0, s

/*
 * Mechanism to discover interrupt latency with NMI failsafe timer
 */
#ifdef IRQFAILSAFE
	.globl	_splstart
_splstart:	.long	0
#define	ENFAILSAFE	cmpl	$0, _splstart ;	\
	jne	3f ;		\
	movl	0(%esp), %eax ; \
	movl	%eax, _splstart ; \
	call	_failsafe_start ; \
3:
#define	DISFAILSAFE	cmpl	$0, _splstart ;	\
	je	4f ;		\
	movl	$0, _splstart ; \
	call	_failsafe_cancel ; \
4:
#else
#define	ENFAILSAFE
#define	DISFAILSAFE
#endif
	.text
	.globl common_int_return
/*
 * Handle return from interrupt after device handler finishes
 */
common_int_return:
	/* turn interrupt frame into a trap frame */
	popl	%eax			/* remove intr number */
	popl	%eax			/* get previous priority */

	/* return to previous priority level */
	movl	%eax, %ecx
	cli
	movl	%eax, _cpl
	orw	_imen, %ax
	outb	%al, $IO_ICU1+1		/* re-enable intr? */
	NOP
	movb	%ah, %al
	outb	%al, $IO_ICU2+1
	WRPOST; /* write post before restoring interrupts */

trap_exit:
	testl	%ecx, %ecx		/* returning to zero? */
	je	9f

	/* returning to greater than basepri */
5:
8:
	/* WUP(1)*/
	pop	%es			/* returning to previous context */
	pop	%ds
	popal
	nop
	addl	$8, %esp
	.globl	_iret
_iret:
	iret

9:
	DISFAILSAFE

	cli
	movl	$0, _cpl	/* force back to ipl 0 */

	/* first, look for asts ... */
	/* testw $0x3, 13*4(%esp) */
	cmpw	$0x1f, 13*4(%esp)
	jne	1f			/* no asts, move on to swintrs */
	movl	_curproc, %edx		/* have we a process? */
	/* testl	%edx, %edx	must have if going back to user 
	je	1f */
/*testl	%edx, %edx	
je	9f*/
	btrl	$0, PMD_FLAGS(%edx)
	jc	6f

	/* then network software interrupts */
1:
	xorl	%ebx, %ebx
	xchgl	%ebx, _netisr
	testl	%ebx, %ebx
	jne	4f

3:	/* next, look for softclocks. */
	xorl	%eax, %eax
	xchgl	%eax, _sclkpending
	testl	%eax, %eax
	jne	5f

	jmp	8b			/* return at base pri */

	
#include "netisr.h"

4:
	movl	$IRNET|IRSCLK, _cpl	/* mask net and softclock */
	sti
	.globl _netintr
#define DONET(s, c)	; \
	.globl	c ;  \
	btrl	$ s , %ebx ;  \
	jnc	1f ; \
	incl	_cnt+V_SOFT ; \
	call	c ; \
1:

#define DONETS(isr, tmp)	;	\
1:					\
	bsfl	isr , tmp ;	/* find a software interrupt */ \
	jz 1f	;			\
	btrl	tmp , isr ; 		\
	jnc	1f ; 			\
	incl	_cnt + V_SOFT ;		\
	call	*_netintr(, tmp ,4) ;	\
	jmp	1b ;			\
1:
	

#ifdef nope
#ifdef INET
	DONET(NETISR_IP, _ipintr)
#endif
#ifdef IMP
	DONET(NETISR_IMP, _impintr)
#endif
#ifdef NS
	DONET(NETISR_NS, _nsintr)
#endif
#else
	DONETS(%ebx, %eax)
#endif
	jmp 9b

5:	/* simulate a hardware interrupt for softclock() */
	movl	$IRSCLK, _cpl	/* mask softclock */
	sti
	incl	_cnt+V_SOFT

	pushl	13*4(%esp)		/* cs */
	pushl	13*4(%esp)		/* eip */
	pushl	$0			/* basepri */
	call	_softclock		/* softclock(clockframe); */
	addl	$12, %esp
	jmp 9b

6:	/* have an AST to deliver */
	sti
	incl	_cnt+V_TRAP
	/* movl	%eax, 11*4(%esp)	/* error code contains md_flags */
	movl	$T_ASTFLT, 10*4(%esp)
	call	_trap			/* trap(struct trapframe); */
	jmp 9b

#ifdef	DIAGNOSTIC
9:	pushl	$1f
	call	_panic
1:
	.asciz	"trapexit"
#endif

/*
 * Interrupt priority mechanism
 */
#define	COMMON_SPL							\
	movl	%eax, %edx ;						\
	orw	_imen, %ax ;	/* mask off those not enabled yet */	\
	outb	%al, $IO_ICU1+1 ;	/* update icu's */		\
	NOP ; \
	movb	%ah, %al ;						\
	outb	%al, $IO_ICU2+1 ;					\
	WRPOST ;	 /* write post before restoring interrupts */	\
	xchgl	%edx, _cpl ;	 /* exchange old/new priority level */	\
	movl %edx, %eax ; \
	sti ;				/* enable interrupts */		\
	/* WUP(3) ; */ \
	ret

	.globl	_splhigh
_splhigh:
	movl	$IRHIGH, %eax	/* set new priority level with interrupts off */
	cli
	COMMON_SPL

	.globl	_splclock
_splclock:
	movl	_cpl, %eax
	orb	$IRQ0, %al	/* set new priority level with interrupts off */
	cli
	COMMON_SPL

	.globl	_spltty
_spltty:
	movl	_cpl, %eax
	orw	_ttymask, %ax
	cli				# disable interrupts
	COMMON_SPL

	.globl	_splimp, __splimp
_splimp:
__splimp:
	cli				# disable interrupts
	DISFAILSAFE
	movl	_cpl, %eax
	orw	_netmask, %ax
	COMMON_SPL

	.globl	_splbio	
_splbio:
	cli				# disable interrupts
	DISFAILSAFE
	movl	_cpl, %eax
	orw	_biomask, %ax
	COMMON_SPL

	.globl	_splsoftclock
_splsoftclock:
	PROC_ENTRY
	cli
	pushl	_cpl			/* save old priority */
	movw	_imen, %ax		/* mask off those not enabled yet */
	outb	%al, $IO_ICU1+1		/* update icu's */
	NOP
	movb	%ah, %al
	outb	%al, $IO_ICU2+1
	WRPOST			 /* write post before restoring interrupts */

#ifdef nope
	pushl	%ebx
9:
	xorl	%ebx, %ebx
	xchgl	%ebx, _netisr
	movl	$IRNET|IRSCLK, _cpl	# set new priority level

	testl	%ebx, %ebx
	je	2f
	
	sti
#ifdef nope
#ifdef INET
	DONET(NETISR_IP, _ipintr)
#endif
#ifdef IMP
	DONET(NETISR_IMP, _impintr)
#endif
#ifdef NS
	DONET(NETISR_NS, _nsintr)
#endif
#else
	DONETS(%ebx, %eax)
#endif
	cli
	jmp 9b
2:
#endif
	movl	$IRSCLK, _cpl		/* set new priority level */
	sti
	/* popl	%ebx */
	popl	%eax			/* return old priority */
	PROC_EXIT
	ret


	.globl	_splnet
_splnet:
	cli
	movl	$IRNET|IRSCLK, %eax
	COMMON_SPL

	.globl _splnone
_splnone:
	PROC_ENTRY
	cli
	pushl	_cpl			/* save old priority */
	movw	_imen, %ax		/* mask off those not enabled yet */
	outb	%al, $IO_ICU1+1		/* update icu's */
	NOP
	movb	%ah, %al
	outb	%al, $IO_ICU2+1
	WRPOST			 /* write post before restoring interrupts */
	/* save */
	pushl	%ebx

9:	xorl	%ebx, %ebx
	xchgl	%ebx, _netisr
	movl	$IRNET|IRSCLK, _cpl	# set new priority level

	testl	%ebx, %ebx
	je	5f
	
	sti
#ifdef	nope
#ifdef INET
	DONET(NETISR_IP, _ipintr)
#endif
#ifdef IMP
	DONET(NETISR_IMP, _impintr)
#endif
#ifdef NS
	DONET(NETISR_NS, _nsintr)
#endif
#else
	DONETS(%ebx, %eax)
#endif
	cli
	jmp	9b

5:
#ifdef notdef
	xorl	%ebx, %ebx
	xchgl	%ebx, _sclkpending
	movl	$IRSCLK, _cpl		/* set new priority level */

	testl	%ebx, %ebx
	je	1f

	/* create a softclock interrupt frame */
	sti
	pushl	$KCSEL			/* cs */
	pushl	3*4(%esp)		/* eip */
	pushl	$0			/* basepri */
	call	_softclock		/* softclock(clockframe); */
	addl	$12, %esp
	cli
	jmp	9b

1:
#endif
	movl	$0, _cpl		/* set new priority level */
	sti
	popl	%ebx
	popl	%eax			/* return old priority */
	PROC_EXIT
	ret

	.globl _splx
_splx:
	movl	4(%esp), %eax		# new priority level
	cmpl	$0, %eax
	je	_splnone		# going to "zero level" is special
	cmpl	$IRSCLK, %eax
	je	_splsoftclock		# going to "softclock level" is special

	cli
	COMMON_SPL
	
	.globl	_syscall_entry, _syscall_lret
/*
 * System call interface (in iBCS2, done via call gates).
 */
IDTVEC(syscall)
_syscall_entry:
	pushfl
	pushal
	nop
	movw	$KDSEL, %ax	/* reload kernel selectors */
	movw	%ax, %ds
	movw	%ax, %es
	movw	__udatasel, %ax	 /* reload user selectors */
	movw	%ax, %gs
	incl	_cnt+V_SYSCALL
	call	_syscall

9:
	/* snatch values for softints */
	cli
	movl	_curproc, %edx
	btrl	$0, PMD_FLAGS(%edx)
	
	/* an ast? */
	jc	7f

	/* any network software interrupts present? */
	xorl	%ebx, %ebx
	xchgl	%ebx, _netisr
	testl	%ebx, %ebx
	jne	5f

	/* need a software clock interrupt? */
	xorl	%edi, %edi
	xchgl	%edi, _sclkpending
	testl	%edi, %edi
	jne	6f

	movl	$0, _cpl
	/* WUP(2) */
	movw	__udatasel, %ax	 /* reload user selectors */
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %gs
	popal
	nop
	popfl

_syscall_lret:
	lret

	/* attempt to process network software interrupts prior to return */
5:
	movl	$IRNET|IRSCLK, _cpl
	sti
#ifdef nope
#ifdef INET
	DONET(NETISR_IP, _ipintr)
#endif
#ifdef IMP
	DONET(NETISR_IMP, _impintr)
#endif
#ifdef NS
	DONET(NETISR_NS, _nsintr)
#endif
#else
	DONETS(%ebx, %eax)
#endif
	jmp	9b

6:	/* simulate a hardware interrupt */
	incl	_cnt+V_SOFT
	movl	$IRSCLK, _cpl
	sti

	pushl	10*4(%esp)		/* cs */
	pushl   10*4(%esp)              /* eip */
	pushl	$0			/* basepri */
	call	_softclock		/* softclock(clockframe); */
	addl	$12, %esp
	jmp	9b

7:
	sti
	incl	_cnt+V_TRAP
	call	_systrap		/* systrap(struct syscframe); */
	jmp 9b
