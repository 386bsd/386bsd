/*
 * Copyright (c) 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: copyout_.h,v 1.1 95/01/21 16:03:46 bill Exp Locker: bill $
 * Copy a constant length segment from the kernel to a user process.
 */

__INLINE int
copyout_(struct proc *p, const void *from, void *to, const u_int size)
{
	int rv;

	/* is this in the range of a valid user process address? */
	if ((unsigned)to > ENDUSERMEM || (unsigned)to + size > ENDUSERMEM)
		return(EFAULT);

	/* set fault vector */
	asm volatile ("movl $4f, %0" : "=m" (p->p_md.md_onfault));

	/* copy quantity */
	if (size == 1)
		*(char *) to = *(char *) from;
	else if (size == 2)
		*(short *) to = *(short *) from;
	else if (size == 4)
		*(long *) to = *(long *) from;

	/* catch the possible fault */
	asm volatile ("\
		xorl %0, %0 ;		\
		jmp 5f ;		\
	4:	movl %1, %0 ;		\
	5:				"
		: "=r" (rv)
		: "i" (EFAULT));

	/* clear the fault vector and return */
	p->p_md.md_onfault = 0;
	return (rv);
}
