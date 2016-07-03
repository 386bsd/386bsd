/*
 * Copyright (c) 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: addupc_i386.s,v 1.1 94/10/19 17:39:53 bill Exp $
 *
 * addupc(int pc, struct uprof *up, int ticks):
 * update profiling information for the user process.
 * [XXX move back to machine indep kernel in C]
 */

#include "asm.h"
#include "assym.s"

ENTRY(addupc)
	pushl %ebp
	movl %esp,%ebp
	movl 12(%ebp),%edx		/* up */
	movl 8(%ebp),%eax		/* pc */

	subl PR_OFF(%edx),%eax		/* pc -= up->pr_off */
	jl L1				/* if (pc < 0) return */

	shrl $1,%eax			/* praddr = pc >> 1 */
	imull PR_SCALE(%edx),%eax	/* praddr *= up->pr_scale */
	shrl $15,%eax			/* praddr = praddr << 15 */
	andl $-2,%eax			/* praddr &= ~1 */

	cmpl PR_SIZE(%edx),%eax		/* if (praddr > up->pr_size) return */
	ja L1

/*	addl %eax,%eax			/* praddr -> word offset */
	addl PR_BASE(%edx),%eax		/* praddr += up-> pr_base */
	movl 16(%ebp),%ecx		/* ticks */

	movl _curproc, %edx
	movl $proffault, PMD_ONFAULT(%edx)
	addw %cx, (%eax)		/* storage location += ticks */
	movl $0, PMD_ONFAULT(%edx)
L1:
	leave
	ret

proffault:
	/* if we get a fault, then kill profiling all together */
	movl $0, PMD_ONFAULT(%edx)	/* squish the fault handler */
 	movl 12(%ebp), %ecx
	movl $0,PR_SCALE(%ecx)		/* up->pr_scale = 0 */
	leave
	ret
