/*
 * BDM2: Banked dumb monochrome driver
 * Pascal Haible 9/93, haible@izfm.uni-stuttgart.de
 *
 * bdm2/driver/sigma/sigmabank.s
 *
 * derived from:
 * hga2/*
 * Author:  Davor Matic, dmatic@athena.mit.edu
 * and
 * vga256/*
 * Copyright 1990,91 by Thomas Roell, Dinkelscherben, Germany.
 *
 * see bdm2/COPYRIGHT for copyright and disclaimers.
 */

/* $XFree86: mit/server/ddx/x386/bdm2/drivers/sigma/sigmabank.s,v 2.1 1993/09/10 08:11:47 dawes Exp $ */

/*
 * These are here the very lowlevel bankswitching routines.
 * The segment to switch to is passed via %eax. Only %eax and %edx my be used
 * without saving the original contents.
 *
 * WHY ASSEMBLY LANGUAGE ???
 *
 * These routines must be callable by other assembly routines. But I don't
 * want to have the overhead of pushing and poping the normal stack-frame.
 */

/*
 * Special hack for Sigma LaserView [PLUS]
 * - logical bank size 16k
 * - real bank size 16k
 * To bank logical bank e.g n=1 [starting at 0]
 *    (line 64 to 127, mem 16k to 32k-1) to e.g. (logical) BANK1:
 * bank real bank n+13=14 to the first 16k window,
 * bank real bank n+14=15 to the second 16k window.
 * top and bottom are set to include last 12k of the first window and the
 * first 4k of the second window.
 */

#include "assyntax.h"

#include "sigmaPorts.h"

        FILE("sigmabank.s")

        AS_BEGIN

        SEG_DATA

        SEG_TEXT

        ALIGNTEXT4
        GLOBL   GLNAME(SIGMASetRead)
GLNAME(SIGMASetRead):
	/* Add 13 */
	ADD_L	(CONST(13),EAX)
	/* Set bit 7 */
	AND_L	(CONST(0x80),EAX)
	/* Out byte */
	MOV_L	(SLV_BANK0,EDX)
	OUT_B
	/* Next bank */
	MOV_L	(SLV_BANK1,EDX)
	INC_L	(EAX)
	OUT_B
        RET

        ALIGNTEXT4
        GLOBL   GLNAME(SIGMASetWrite)
        GLOBL   GLNAME(SIGMASetReadWrite)
GLNAME(SIGMASetWrite):
GLNAME(SIGMASetReadWrite):
	/* Add 13 */
	ADD_L	(CONST(13),EAX)
	/* Set bit 7 */
	AND_L	(CONST(0x80),EAX)
	/* Out byte */
	MOV_L	(SLV_BANK2,EDX)
	OUT_B
	/* Next bank */
	MOV_L	(SLV_BANK3,EDX)
	INC_L	(EAX)
	OUT_B
        RET
