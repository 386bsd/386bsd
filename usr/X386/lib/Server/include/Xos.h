/*
 * $XFree86: mit/include/Xos.h,v 2.3 1993/09/22 15:31:01 dawes Exp $
 * $XConsortium: Xos.h,v 1.47 91/08/17 17:14:38 rws Exp $
 * 
 * Copyright 1987 by the Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided 
 * that the above copyright notice appear in all copies and that both that 
 * copyright notice and this permission notice appear in supporting 
 * documentation, and that the name of M.I.T. not be used in advertising
 * or publicity pertaining to distribution of the software without specific, 
 * written prior permission. M.I.T. makes no representations about the 
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * The X Window System is a Trademark of MIT.
 *
 */

/* This is a collection of things to try and minimize system dependencies
 * in a "signficant" number of source files.
 */

#ifndef _XOS_H_
#define _XOS_H_

#ifdef _MINIX
#include <minix/posix.h>
#endif /* _MINIX */

#ifdef SCO
#include <stdio.h>
#endif

#include <X11/Xosdefs.h>

/*
 * Get major data types (esp. caddr_t)
 */

#if defined(USG) || defined(SYSV)
#ifndef __TYPES__
#ifdef CRAY
#define word word_t
#endif /* CRAY */
#ifdef ESIX
#define unchar u_char
#endif
#include <sys/types.h>			/* forgot to protect it... */
#define __TYPES__
#endif /* __TYPES__ */
#else /* USG || SYSV */
#if defined(_POSIX_SOURCE) && (defined(MOTOROLA) || defined(AMOEBA))
#undef _POSIX_SOURCE
#include <sys/types.h>
#define _POSIX_SOURCE
#else
#include <sys/types.h>
#endif
#endif /* USG || SYSV */


/*
 * Just about everyone needs the strings routines.  We provide both forms here,
 * index/rindex and strchr/strrchr, so any systems that don't provide them all
 * need to have #defines here.
 */

#ifndef X_NOT_STDC_ENV
#include <string.h>
#define index strchr
#define rindex strrchr
#else
#ifdef SYSV
#include <string.h>
#define index strchr
#define rindex strrchr
#else
#include <strings.h>
#define strchr index
#define strrchr rindex
#endif
#endif


/*
 * Get open(2) constants
 */
#ifdef X_NOT_POSIX
#include <fcntl.h>
#ifdef USL
#include <unistd.h>
#endif /* USL */
#ifdef CRAY
#include <unistd.h>
#endif /* CRAY */
#ifdef MOTOROLA
#include <unistd.h>
#endif /* MOTOROLA */
#ifdef SYSV386
#include <unistd.h>
#endif /* SYSV386 */

#if defined(ISC) && !defined(O_NDELAY)
#define O_NDELAY O_NONBLOCK
#endif /* ISC */

#include <sys/file.h>
#else /* X_NOT_POSIX */
#if !defined(_POSIX_SOURCE) && defined(macII)
#define _POSIX_SOURCE
#include <fcntl.h>
#undef _POSIX_SOURCE
#else
#include <fcntl.h>
#endif
#include <unistd.h>
#endif /* X_NOT_POSIX else */

/*
 * Get struct timeval
 */

#ifdef SYSV

#ifndef USL
#include <sys/time.h>
#endif
#include <time.h>
#ifdef CRAY
#undef word
#endif /* CRAY */
#if defined(USG) && !defined(CRAY) && !defined(MOTOROLA)
struct timeval {
    long tv_sec;
    long tv_usec;
};
#ifndef USL_SHARELIB
struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};
#endif /* USL_SHARELIB */
#endif /* USG */

#if defined(SCO) && !defined(SCO324)
/*
 * Interval timer (for PEX)
 * a dummy implementation lives in
 * Berklib.c
 */

#define	ITIMER_REAL		0
#define	ITIMER_VIRTUAL	1
#define	ITIMER_PROF		2

struct itimerval {
    struct timeval it_interval;		/* timer interval */
    struct timeval it_value;		/* current timer value */
};
#endif /* defined(SCO) && !defined(SCO324) */

#else /* not SYSV */

#if defined(_POSIX_SOURCE) && (defined(SVR4) || defined(AMOEBA))
/* need to omit _POSIX_SOURCE in order to get what we want in SVR4 */
#undef _POSIX_SOURCE
#include <sys/time.h>
#ifdef AMOEBA
#include <time.h>

/*
 * Interval timer (for PEX)
 */
struct itimerval {
    struct timeval it_interval;		/* timer interval */
    struct timeval it_value;		/* current timer value */
};
#endif /* AMOEBA */
#define _POSIX_SOURCE
#else
#ifdef _MINIX
#include <time.h>
#else
#include <sys/time.h>
#endif
#endif

#endif /* SYSV */

/* use POSIX name for signal */
#if defined(X_NOT_POSIX) && defined(SYSV) && !defined(SIGCHLD)
#define SIGCHLD SIGCLD
#endif

#ifdef ISC
#include <sys/bsdtypes.h>
#endif

#ifdef __386BSD__
#include <machine/endian.h>
#endif

#ifdef _MINIX
#include <net/hton.h>
#include <net/gen/in.h>
#include <net/gen/socket.h>   /* Get socket types. */

#ifndef IOVEC_DEFINED
#define IOVEC_DEFINED
struct iovec
{
	char *iov_base;
	size_t iov_len;
};
#endif

struct sockaddr
{
	union
	{
		int sa_family;
		struct sockaddr_in
		{
			int sin_family;
			ipaddr_t sin_addr;
			u16_t sin_port;
		} sa_in;
	} sa_u;
};

#endif /* _MINIX */

#endif /* _XOS_H_ */
