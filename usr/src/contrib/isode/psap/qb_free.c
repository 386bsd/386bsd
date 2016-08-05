/* qb_free.c - free a list of qbufs */

#ifndef	lint
static char *rcsid = "$Header: /f/osi/psap/RCS/qb_free.c,v 7.1 91/02/22 09:36:45 mrose Interim $";
#endif

/* 
 * $Header: /f/osi/psap/RCS/qb_free.c,v 7.1 91/02/22 09:36:45 mrose Interim $
 *
 *
 * $Log:	qb_free.c,v $
 * Revision 7.1  91/02/22  09:36:45  mrose
 * Interim 6.8
 * 
 * Revision 7.0  89/11/23  22:13:31  mrose
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
#include "psap.h"

/*  */

int	qb_free (qb)
register struct qbuf *qb;
{
    QBFREE (qb);

    free ((char *) qb);
}
