/*
 * Copyright (c) 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: copyin.h,v 1.1 95/01/21 16:03:41 bill Exp Locker: bill $
 * Copy a fixed length segment from a user process to the kernel.
 */

__INLINE int
copyin(struct proc *p, void *from, void *toaddr, u_int size)
{
	extern const int zero;		/* compiler bug workaround */
	const void *f = from + zero;	/* compiler bug workaround */
	int rv;

	/* is this in the range of a valid user process address? */
	if ((unsigned)from > ENDUSERMEM || (unsigned)from + size > ENDUSERMEM)
		return(EFAULT);

	/* set fault vector */
	asm volatile ("movl $4f, %0" : "=m" (p->p_md.md_onfault));

	/* copy the string */
	asm volatile ("cld ; repe ; movsl"
		 : "=D" (toaddr), "=S" (f)
		 : "0" (toaddr), "1" (f), "c" (size / 4));
	asm volatile ("repe ; movsb"
		 : "=D" (toaddr), "=S" (f)
		 : "0" (toaddr), "1" (f), "c" (size & 3));

	/* catch the possible fault */
	asm volatile ("\
		xorl %0, %0 ;		\
		jmp 5f ;		\
	4:	movl %1, %0 ;		\
	5:				"
		: "=r" (rv)
		: "i" (EFAULT));

	/* clear the fault vector */
	p->p_md.md_onfault = 0;

	return (rv);
}
