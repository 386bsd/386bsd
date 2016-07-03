/*
 * Copyright (c) 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: kernel.h,v 1.1 94/06/09 18:10:23 bill Exp Locker: bill $
 * Inline function calls for the kernel, specific to the X86 processor
 * family.
 */

__BEGIN_DECLS
/* kernel <-> user process primatives */
int copyin(struct proc *p, void *from, void *toaddr, u_int maxlength);
int copyin_(struct proc *p, const void *from, void *toaddr, const u_int size);
int copyinstr(struct proc *p, void *from, void *to, u_int maxlength, u_int *lencopied);
int copyout(struct proc *p, void *from, void *toaddr, u_int maxlength);
int copyout_(struct proc *p, const void *from, void *toaddr, const u_int size);
int copyoutstr(struct proc *p, void *from, void *to, u_int maxlength, u_int *lencopied);
int copystr(void *from, void *to, u_int maxlength, u_int *lencopied);

#if	!defined(i486)
/* minimized 386 write protection bug workaround functions */
int copyout_4(int value, void *toaddr);
int copyout_2(short value, void *toaddr);
int copyout_1(char value, void *toaddr);
#endif

/* min/max functions */
int imax(int i1, int i2);
int imin(int i1, int i2);
long lmax(long l1, long l2);
long lmin(long l1, long l2);
u_int max(u_int u1, u_int u2);
u_int min(u_int u1, u_int u2);
u_long ulmax(u_long u1, u_long u2);
u_long ulmin(u_long u1, u_long u2);

/* queue functions */
void _insque(queue_t element, queue_t queue);
void _remque(queue_t element);

/* process run queue functions */
void setrq(struct proc *p);
void remrq(struct proc *p);
__END_DECLS

#ifndef __NO_INLINES

#undef	__INLINE
#ifndef __NO_INLINES_BUT_EMIT_CODE
#define	__INLINE	extern inline
#else
#define	__INLINE
#endif

/* standard functions */
#include "kernel/copystr.h"
#include "kernel/imax.h"
#include "kernel/imin.h"
#include "kernel/lmax.h"
#include "kernel/lmin.h"
#include "kernel/max.h"
#include "kernel/min.h"
#include "kernel/ulmax.h"
#include "kernel/ulmin.h"

/* queue functions */
#ifdef _QUEUE_H_
#include "kernel/insque.h"
#include "kernel/remque.h"
#endif

/* process functions */
#ifdef _PROC_H_
#include "kernel/copyin.h"
#include "kernel/copyin_.h"
#include "kernel/copyinstr.h"

/* compensate for 386 lack of write-protection from kernel mode(see locore.s). */
#if defined(i486)
/* 486, Pentium, ... can accept inlines for these */
#include "kernel/copyout.h"
#include "kernel/copyout_.h"
#include "kernel/copyoutstr.h"
#else
/* minimize complexity for asm versions, since compiler can't optimize them */
#define copyout_(p, src, dst, len) \
		copyout_ ## len ((int)*src, dst)
#endif

#include "kernel/remrq.h"
#include "kernel/setrq.h"
#endif

#undef	__INLINE
#endif
