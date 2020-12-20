/*
 * Copyright (c) 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: copyinstr.h,v 1.1 95/01/21 16:03:44 bill Exp Locker: bill $
 * Copy a variable length segment from a user process into a kernel
 * buffer of finite size.
 */

__INLINE int
copyinstr(struct proc *p, void *from, void *to, u_int size,
	u_int *lencopied) {
	extern const int zero;		/* compiler bug workaround */
	/* const void *f = from + zero;	/* compiler bug workaround */
	u_int req /* = size + zero */;
	int rv;

	/* is this in the range of a valid user process address? */
	/* XXX problem: what if a valid short (< size) string is copied at the
	 * very top (> ENDUSERMEM - size) of the address space. 
	 * Currently, no size test is made, and the worst that happens
	 * is we read the bottom portion of the kernel. -wfj
	 */
	if ((unsigned)from > ENDUSERMEM /* || (unsigned)from + size > ENDUSERMEM */)
		return(EFAULT);

	lencopied += zero;
	from += zero;
	size += zero;
	req = size;

	/* set fault vector */
	asm volatile ("movl $4f, %0" : "=m" (p->p_md.md_onfault));

	/* copy the string */
	asm volatile (" \
		cld ;			\
	1:	cmpb	$0,(%1) ;	\
		movsb ;			\
		loopne	1b ;		\
		jne     3f ;		"
		: "=D" (to), "=S" (from), "=c" (size)
		: "0" (to), "1" (from), "c" (size));

	/* set the return value, and catch the possible fault */
	asm volatile ("\
	2:	xorl %0, %0 ;		\
		jmp 5f ;		\
	3:	movl %2, %0 ;		\
		jmp 5f ;		\
	4:	movl %1, %0 ;		\
	5:				"
		: "=r" (rv)
		: "i" (EFAULT), "i" (ENAMETOOLONG));

	/* clear the fault vector */
	p->p_md.md_onfault = 0;

	/* was the return of the length found desired? */
	if (lencopied)
		*lencopied = req - size;

	return (rv);
}
