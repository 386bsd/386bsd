/* general.c - general utilities for emulation of 4.2BSD */

#ifndef	lint
static char *rcsid = "$Header: /f/osi/compat/RCS/general.c,v 7.3 91/02/22 09:15:10 mrose Interim $";
#endif

/* 
 * $Header: /f/osi/compat/RCS/general.c,v 7.3 91/02/22 09:15:10 mrose Interim $
 *
 *
 * $Log:	general.c,v $
 * Revision 7.3  91/02/22  09:15:10  mrose
 * Interim 6.8
 * 
 * Revision 7.2  91/02/12  18:30:43  mrose
 * update
 * 
 * Revision 7.1  91/01/14  13:33:48  mrose
 * loader
 * 
 * Revision 7.0  89/11/23  21:23:00  mrose
 * Release 6.0
 * 
 */

/*
 *				  NOTICE
 *
 *    Acquisition, use, and distribution of this module and related
 *    materials are subject to the restrictions of a license agreement.
 *    Consult the Preface in the User's Manual for the full terms of
 *    this agreement.
 *
 */


/* LINTLIBRARY */

#include <stdio.h>
#include "general.h"
#include "manifest.h"

/*    Berkeley UNIX: 4.2 */

#ifdef	BSD42

/* Simply including "general.h" is sufficient. */

int	_general_stub () {};

#endif
#ifdef BSD44
#undef BSDLIBC
#endif

/*    non-Berkeley UNIX */

#ifndef	BSDLIBC

#ifndef	lint

struct qelem {
    struct qelem   *q_forw;
    struct qelem   *q_back;
    char	    q_data[1];	/* extensible */
};


insque (elem, pred)
struct qelem   *elem,
	       *pred;
{
    pred -> q_forw -> q_back = elem;
    elem -> q_forw = pred -> q_forw;
    elem -> q_back = pred;
    pred -> q_forw = elem;
}


remque (elem)
struct qelem   *elem;
{
    elem -> q_forw -> q_back = elem -> q_back;
    elem -> q_back -> q_forw = elem -> q_forw;
}

#endif
#endif

/*    DUP2 */

#ifndef	BSD42
#ifdef	SYS5
#include <fcntl.h>
#endif


extern int errno;


int     dup2 (d1, d2)
register int    d1,
                d2;
{
    int     d;

    if (d1 == d2)
	return OK;

    (void) close (d2);
#ifdef	F_DUPFD
    if ((d = fcntl (d1, F_DUPFD, d2)) == NOTOK)
	return NOTOK;
#else
    if ((d = dup2_aux (d1, d2)) == NOTOK)
	return NOTOK;
#endif
    if (d == d2)
	return OK;

    errno = 0;
    return NOTOK;
}


#ifndef	F_DUPFD
dup2_aux (d1, d2)
int	d1,
	d2;
{
    int     fd,
	    result;

    if ((fd = dup (d1)) == NOTOK || fd == d2)
	return fd;

    result = dup2_aux (d1, d2);

    (void) close (fd);

    return result;
}
#endif
#endif

/*     BYTEORDER */

#ifndef	SWABLIB

/* ROS and HP-UX don't seem to have these in libc.a */

#undef	ntohs
u_short	ntohs (netshort) u_short netshort; { return netshort; }

#undef	htons
u_short	htons (hostshort) u_short hostshort; { return hostshort; }

#undef	ntohl
u_long	ntohl (netlong) u_long netlong; { return netlong; }

#undef	htonl
u_long	htonl (hostlong) u_long hostlong; { return hostlong; }

#endif
