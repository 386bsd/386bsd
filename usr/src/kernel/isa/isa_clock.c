/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz and Don Ahn.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)clock.c	7.2 (Berkeley) 5/12/91
 */

/*
 * Primitive clock interrupt routines.
 */
/* standard AT configuration: (will always be configured if loaded) */
static	char *clk_config =
	"clock (0 0).	# process timeslice clock $Revision$";

#include "sys/param.h"
#include "sys/time.h"
#include "sys/errno.h"
#include "tzfile.h"
#include "kernel.h"
#include "malloc.h"
#include "modconfig.h"
#include "prototypes.h"

#include "machine/cpu.h"
#include "machine/pcb.h"
#include "machine/inline/io.h"

#include "machine/icu.h"
#include "isa_stdports.h"
#include "isa_irq.h"
#include "rtc.h"
#include "isa_driver.h"

#define DAYST 119
#define DAYEN 303
unsigned long it_ticks, it_ticksperintr;


startrtclock() {
	int s;

	/* initialize 8253 clock */
	/* it_ticksperintr = 1193182/hz;
	outb (IO_TIMER1+3, 0x34);
	outb (IO_TIMER1, it_ticksperintr);
	outb (IO_TIMER1, it_ticksperintr/256); */
	

	/* initialize brain-dead battery powered clock */
	outb (IO_RTC, RTC_STATUSA);
	outb (IO_RTC+1, 0x26);
	outb (IO_RTC, RTC_STATUSB);
	outb (IO_RTC+1, 2);

	outb (IO_RTC, RTC_DIAG);
	if (s = inb (IO_RTC+1))
		printf("RTC BIOS diagnostic error %b\n", s, RTCDG_BITS);
	outb (IO_RTC, RTC_DIAG);
	outb (IO_RTC+1, 0);
}

static ena;
/*
 * Wire clock interrupt in.
 */
enablertclock() {
	ena=1;
}

static int hi = 0xffff;
static int clkprobe(struct isa_device *dvp);
static void clkattach(struct isa_device *dvp);
static void clkintr(struct intrframe f);
struct	isa_driver clkdriver = {
	clkprobe, clkattach, (void (*)(int))clkintr, "clk", &hi
};

/*
 * Probe routine - look device, otherwise set emulator bit
 */
static int
clkprobe(struct isa_device *dvp)
{
	return (1);
}

static void
clkattach(struct isa_device *dvp)
{
	splx(0xf); /* XXX */
}

static void
clkintr(struct intrframe f) {

	if (!ena) return;
	splhigh();
	it_ticks += it_ticksperintr;
	hardclock(f.if_ppl, f.if_eip, f.if_cs);
}

DRIVER_MODCONFIG() {
	char *cfg_string;
	
	/* find configuration string. */
	if (!config_scan(clk_config, &cfg_string)) 
		return;

	/* probe for hardware */
	new_isa_configure(&cfg_string, &clkdriver);
}
