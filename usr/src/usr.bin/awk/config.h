/********************************************
386bsd.h
copyright 1993, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/


/*
 * $Log: 386bsd.h,v $
 * Revision 1.1  1993/02/05  02:20:10  mike
 * Initial revision
 *
 */

#ifndef   CONFIG_H
#define   CONFIG_H    1

#define   FPE_TRAPS_ON                1
#define   FPE_ZERODIVIDE   FPE_FLTDIV_TRAP
#define   FPE_OVERFLOW     FPE_FLTOVF_TRAP

#define   HAVE_STRTOD		0

#define   HAVE_MATHERR		0

#define   DONT_PROTO_OPEN

#include "config/Idefault.h"

#endif  /* CONFIG_H  */
