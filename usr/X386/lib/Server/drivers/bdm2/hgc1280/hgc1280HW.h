/*
 * BDM2: Banked dumb monochrome driver
 * Pascal Haible 8/93, haible@izfm.uni-stuttgart.de
 *
 * bdm2/driver/hgc1280/hgc1280HW.h
 *
 * see bdm2/COPYRIGHT for copyright and disclaimers.
 */

/* $XFree86: mit/server/ddx/x386/bdm2/drivers/hgc1280/hgc1280HW.h,v 2.3 1993/10/02 09:50:42 dawes Exp $ */

/* ********** WARNING: **********
 * Couldn't include defined constants in hgc1280bank.s, so the
 * ports are hardcoded in there!!!
 */

#include "compiler.h"	/*	void outb(port,val);
				void outw(port,val);
				unsigned int inb(port);
				unsigned int inw(port);
				void intr_disable();
				void intr_enable();
				short port;
				short val;
			*/

#define C_STYLE_HEX_CONSTANTS
#include "hgc1280Port.h"

#define HGC_NUM_REGS		(0x40)

#define HGC_MEM_BASE		(0xC8000L)

#define HGC_BANK_SIZE		(0x4000L)

#define HGC_MEM_BASE_BANK1	(0L)
#define HGC_MEM_BASE_BANK2	(HGC_BANK_SIZE)

#define HGC_BANK1_BOTTOM	(HGC_MEM_BASE_BANK1)
#define HGC_BANK1_TOP		(HGC_BANK1_BOTTOM+HGC_BANK_SIZE)
#define HGC_BANK2_BOTTOM        (HGC_MEM_BASE_BANK2)
#define HGC_BANK2_TOP		(HGC_BANK2_BOTTOM+HGC_BANK_SIZE)

#define HGC_MAP_BASE		(HGC_MEM_BASE)

#define HGC_MAP_SIZE		(2*HGC_BANK_SIZE)

#define HGC_SEGMENT_SIZE	(HGC_BANK_SIZE)
#define HGC_SEGMENT_SHIFT	(14)
#define HGC_SEGMENT_MASK	(0x3FFFL)

#define HGC_HDISPLAY		(1280)
#define HGC_VDISPLAY		(1024)
#define HGC_MAX_VIRTUAL_X	(1472)
#define HGC_MAX_VIRTUAL_Y	(1024)
#define HGC_SCAN_LINE_WIDTH	(2048)

/* HGC-1280 needs some delay between the setting of at least some of
 * the registers. I'm not sure if it is the usual I/O recovery or
 * something else. Anyway, on my setup I need both inb() when setting
 * the mode registers. Setting the banking regs seems to work without
 * this delay.
 */
#define IO_RECOVER		{ volatile unsigned char io_recover_dummy; \
				  io_recover_dummy=inb(HGC_PORT_INDEX);    \
				  io_recover_dummy=inb(HGC_PORT_INDEX); }

#define HGC_SET_REG(reg,val)	{ outb(HGC_PORT_INDEX,reg);	\
				  IO_RECOVER;			\
				  outb(HGC_PORT_DATA,val); }	\
				  IO_RECOVER;

#define HGC_GET_REG(reg,pval)	{ outb(HGC_PORT_INDEX,reg);	\
				  IO_RECOVER;			\
				  *(pval)=inb(HGC_PORT_DATA); }	\
				  IO_RECOVER;

#if 0
#define HGC_SHIFT_DISPLAY(offs)	{ HGC_SET_REG(45, 16 - ((offs)>>4) ) }
#endif

#define HGC_SET_BANK1(b)	HGC_SET_REG(HGC_REG_BANK1,b)
#define HGC_SET_BANK2(b)	HGC_SET_REG(HGC_REG_BANK2,b)

#define HGC_PROBE_REG_RW	(56)
#define HGC_PROBE_VAL_WRITE	(0xFF)
#define HGC_PROBE_VAL_READ	(0xFF)
#define HGC_PROBE_VAL_RESET	(0x00)
#define HGC_PROBE_REG_FIX	(61)
#define HGC_PROBE_VAL_FIX1	(16)
#define HGC_PROBE_VAL_FIX2	(85)

unsigned char hgcRegsGraf1280x1024[HGC_NUM_REGS] = {
/***     0    1    2    3    4    5    6    7    8    9 ***/
/*0*/    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
/*1*/    0,   0,   0,   0, 255, 245,   0,   0,   0,   0,
/*2*/    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
/*3*/    0,   0, 214, 212,   0, 212,   1, 212,  10, 213,
/*4*/  107, 153, 138,  90,  33,  16, 191,   2,   9, 192,
/*5*/  224,  68,  65,   1,  65,  79,   0,  85,   3,   0,
/*6*/    0,  85,   3,   0 };
