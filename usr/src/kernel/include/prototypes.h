/*
 * Copyright (c) 1993 William Jolitz. All rights reserved.
 * Written by William Jolitz 1/93
 *
 * Redistribution and use in source and binary forms are freely permitted
 * provided that the above copyright notice and attribution and date of work
 * and this paragraph are duplicated in all such forms.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *
 *	$Id$
 *
 * Machine independant kernel function prototypes.
 */

/* forward declarations */
struct proc;
struct user;
struct pcb;

int	nullop (void);
int	_ENODEV_ (void);	/* XXX: need to generate dynamically */
int	_ENOSYS_ (void);
int	_ENOTTY_ (void);
int	_ENXIO_ (void);
int	_EOPNOTSUPP_ (void);
int	seltrue(dev_t dev, int which, struct proc *p);
void	selwakeup(pid_t pid, int coll);

void	panic (char *);
void	tablefull (char *);
void	addlog (const char *, ...);
void	log (int, const char *, ...);
void	printf (const char *, ...);
int	sprintf (char *buf, const char *, ...);
void	ttyprintf (struct tty *, const char *, ...);

int	 memcmp (const void *, const void *, size_t);
void	*memcpy (void *, const void *, size_t);
void	*memmove (void *, const void *, size_t);
void	*memset (void *, int, size_t);

char	*strcat (char *, const char *);
char	*strcpy (char *, const char *);
size_t	 strlen (const char *);
char	*strncpy (char *, const char *, size_t);

int	copystr (void *kfaddr, void *kdaddr, u_int len, u_int *done);
int	copyinstr (struct proc *, void *udaddr, void *kaddr, u_int len, u_int *done);
int	copyoutstr (struct proc *, void *kaddr, void *udaddr, u_int len, u_int *done);
int	copyin (struct proc *, void *udaddr, void *kaddr, u_int len);
int	copyout (struct proc *, void *kaddr, void *udaddr, u_int len);

int	fubyte (void *base);
#ifdef notdef
int	fuibyte (void *base);
#endif
int	subyte (void *base, int byte);
int	suibyte (void *base, int byte);
int	fuword (void *base);
int	fuiword (void *base);
int	suword (void *base, int word);
int	suiword (void *base, int word);

int	scanc (unsigned size, u_char *cp, u_char *table, int mask);
int	skpc (int mask, int size, char *cp);
int	locc (int mask, char *cp, unsigned size);
int	ffs (long value);

#ifdef _CPU_H_
void hardclock(clockframe frame);
void gatherstats(clockframe frame);
void softclock(clockframe frame);
#endif

void timeout(int (*func)(), caddr_t arg, int t);
void untimeout(int (*func)(), caddr_t arg);

#ifdef _SYS_TIME_H_
int hzto(struct timeval *tv);
void realitexpire(struct proc *p);
int itimerfix(struct timeval *tv);
int itimerdecr(struct itimerval *itp, int usec);
void timevaladd(struct timeval *t1, struct timeval *t2);
void timevalsub(struct timeval *t1, struct timeval *t2);
void timevalfix(struct timeval *t1);
#endif

/*
 * Machine dependant function prototypes
 */
#include "machine/prototypes.h"

/*
 * Inline functions
 */
#include "queue.h"
#define	insque(n, h)	_insque((queue_t) n, (queue_t) h)
#define	remque(n)	_remque((queue_t) n)

/*__BEGIN_DECLS
__END_DECLS */

#include "machine/inline/string.h"
#include "machine/inline/kernel.h"
