/*
 * Copyright 1993 by David Wexelblat <dwex@goblin.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of David Wexelblat not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  David Wexelblat makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * DAVID WEXELBLAT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL DAVID WEXELBLAT BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 */

/* $XFree86: mit/server/ddx/x386/common/xf86_Option.h,v 2.15 1993/10/16 17:31:31 dawes Exp $ */

#ifndef _XF86_OPTION_H
#define _XF86_OPTION_H

/*
 * Structures and macros for handling option flags.
 */
#define MAX_OFLAGS	128
#define FLAGBITS	sizeof(unsigned long)
typedef struct {
	unsigned long flag_bits[MAX_OFLAGS/FLAGBITS];
} OFlagSet;

#define OFLG_SET(f,p)	((p)->flag_bits[(f)/FLAGBITS] |= (1 << ((f)%FLAGBITS)))
#define OFLG_CLR(f,p)	((p)->flag_bits[(f)/FLAGBITS] &= ~(1 << ((f)%FLAGBITS)))
#define OFLG_ISSET(f,p)	((p)->flag_bits[(f)/FLAGBITS] & (1 << ((f)%FLAGBITS)))
#define OFLG_ZERO(p)	memset((char *)(p), 0, sizeof(*(p)))

/*
 * Option flags.  Define these in numeric order.
 */
#define OPTION_LEGEND		0  /* Legend board with 32 clocks           */
#define OPTION_SWAP_HIBIT	1  /* WD90Cxx-swap high-order clock sel bit */
#define OPTION_INTERN_DISP	2  /* Laptops - enable internal display (WD)*/
#define OPTION_EXTERN_DISP	3  /* Laptops - enable external display (WD)*/
#define OPTION_NOLINEAR_MODE	4  /* chipset has broken linear access mode */
#define OPTION_8CLKS		5  /* Probe for 8 clocks instead of 4 (PVGA1) */
#define OPTION_16CLKS		6  /* probe for 16 clocks instead of 8 */
#define OPTION_PROBE_CLKS	7  /* Force clock probe for cards where a
				      set of preset clocks is used */
#define OPTION_HIBIT_HIGH	8  /* Initial state of high order clock bit */
#define OPTION_HIBIT_LOW	9
#define OPTION_FAST_DRAM	10 /* reduce DRAM access time (for ET4000) */
#define OPTION_SLOW_DRAM	11 /* Allow for slow DRAM (for Cirrus) */
#define OPTION_MEM_ACCESS	12 /* prevent direct access to video ram
				      from being automatically disabled */
#define OPTION_NO_MEM_ACCESS	13 /* Unable to access video ram directly */
#define OPTION_NOACCEL		14 /* Disable accel support in SVGA server */
#define OPTION_HW_CURSOR	15 /* Turn on HW cursor */
#define OPTION_SW_CURSOR	16 /* Turn off HW cursor (Mach32) */
#define OPTION_BT485		17 /* Has BrookTree Bt485 RAMDAC */
#define OPTION_NO_BT485		18 /* Override Bt485 RAMDAC probe */
#define OPTION_BT485_CURS	19 /* Override Bt485 RAMDAC probe */
#define OPTION_SHOWCACHE	20 /* Allow cache to be seen (S3) */

#define CLOCK_OPTION_PROGRAMABLE 0 /* has a programable clock */
#define CLOCK_OPTION_ICD2061A	 1 /* use ICD 2061A programable clocks      */
#define CLOCK_OPTION_ICD2061ASL	 2 /* use slow ICD 2061A programable clocks */


/*
 * Table to map option strings to tokens.
 */
typedef struct {
  char *name;
  int  token;
} OptFlagRec, *OptFlagPtr;

#ifdef INIT_OPTIONS
OptFlagRec xf86_OptionTab[] = {
  { "legend",		OPTION_LEGEND },
  { "swap_hibit",	OPTION_SWAP_HIBIT },
  { "intern_disp",	OPTION_INTERN_DISP },
  { "extern_disp",	OPTION_EXTERN_DISP },
  { "nolinear",		OPTION_NOLINEAR_MODE },
  { "8clocks",		OPTION_8CLKS },
  { "16clocks",		OPTION_16CLKS },
  { "probe_clocks",	OPTION_PROBE_CLKS },
  { "hibit_high",	OPTION_HIBIT_HIGH },
  { "hibit_low",	OPTION_HIBIT_LOW },
  { "fast_dram",	OPTION_FAST_DRAM },
  { "slow_dram",	OPTION_SLOW_DRAM },
  { "memaccess",	OPTION_MEM_ACCESS },
  { "nomemaccess",	OPTION_NO_MEM_ACCESS },
  { "noaccel",		OPTION_NOACCEL },
  { "hw_cursor",	OPTION_HW_CURSOR },
  { "sw_cursor",	OPTION_SW_CURSOR },
  { "bt485",		OPTION_BT485 },
  { "no_bt485",		OPTION_NO_BT485 },
  { "bt485_curs",	OPTION_BT485_CURS },
  { "showcache",	OPTION_SHOWCACHE },
  { "",			-1 },
};

OptFlagRec xf86_ClockOptionTab [] = {
  { "icd2061a",		CLOCK_OPTION_ICD2061A }, /* generic ICD2061A */
  { "icd2061a_slow",	CLOCK_OPTION_ICD2061ASL }, /* slow ICD2061A */
  { "",			-1 },
};

#else
extern OptFlagRec xf86_OptionTab[];
extern OptFlagRec xf86_ClockOptionTab[];
#endif

#endif /* _XF86_OPTION_H */

