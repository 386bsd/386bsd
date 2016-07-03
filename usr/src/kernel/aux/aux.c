/*
 * Copyright (c) 1993, 1994 William F. Jolitz, TeleMuse
 * All rights reserved.
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
 *	This software is a component of "386BSD" developed by 
	William F. Jolitz, TeleMuse.
 * 4. Neither the name of the developer nor the name "386BSD"
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS A COMPONENT OF 386BSD DEVELOPED BY WILLIAM F. JOLITZ 
 * AND IS INTENDED FOR RESEARCH AND EDUCATIONAL PURPOSES ONLY. THIS 
 * SOFTWARE SHOULD NOT BE CONSIDERED TO BE A COMMERCIAL PRODUCT. 
 * THE DEVELOPER URGES THAT USERS WHO REQUIRE A COMMERCIAL PRODUCT 
 * NOT MAKE USE THIS WORK.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPER ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPER BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id$
 *
 * Aux keyboard port, usually used to connect a mouse.
 */

static char *aux_config =
	"aux 14	(0x310 12).	# mouse port";

#include "sys/param.h"
#include "proc.h"
#include "sys/file.h"
#include "uio.h"
#include "vnode.h"
#include "sys/ioctl.h"
#include "sys/user.h"
#include "tty.h"
#include "isa_driver.h"
#include "systm.h"
#include "kernel.h"
#include "modconfig.h"
/*#include "i386/isa/icu.h"*/
#include "isa_irq.h"
/* #include "i386/isa/isa.h" */
/* #include "i386/isa/ic/82c710.h" */
#include "prototypes.h"
#include "machine/inline/io.h"

int auxprobe(), auxattach(), auxintr();

struct	isa_driver auxdriver = {
	auxprobe, auxattach, auxintr, "aux", 0
};

static struct ringb rcv;
static int  auxopenf, auxflag, auxpid, auxpgid;

extern auxopen(dev_t, int, int, struct proc *);

auxprobe(dev)
struct isa_device *dev;
{
	unsigned c;
	int again = 0;
int i;

/*outb(0x2fa,0x55);
outb(0x3fa,0xaa);
outb(0x3fa,0x36);
outb(0x3fa,0xe4);
outb(0x2fa,0x1b);
outb(0x2fa,0x36);
printf("\n[");
for(i=0; i < 16; i++) {
outb(0x390, i);
printf("%x ", inb(0x391));
}
printf("]\n"); */
/* outb(0x311, 0x88);
DELAY(250);
outb(0x311, 0x80);
DELAY(2500);
while((inb(0x311)&4)==0) ;
outb(0x310, 0xf4);
while((inb(0x311)&4)==0) ;
outb(0x311, 0x95); */
return(1);
}

#ifdef nope
	while((c = key_cmd(KBC_RESET)) != KBR_ACK) {
		if ((c == KBR_RESEND) ||  (c == KBR_OVERRUN)) {
			if(!again)printf("KEYBOARD disconnected: RECONNECT \n");
			DELAY(40);
			again = 1;
		}
	}

	/* pick up keyboard reset return code */
	if((c = kbd_rd()) != KBR_RSTDONE)
		printf("keyboard failed selftest (%x) \n", c);

kbd_drain();

/*printf("6");
	kbd_cmd(K_DISKEY);
	kbd_cmd(K_ENAKEY);*/

/*DELAY(0x10000);
kbd_drain();
	while(aux_cmd(0xff) != KBR_ACK)
		printf("!");
	if((c = kbd_rd()) != KBR_RSTDONE)
		printf("aux failed selftest (%x) \n", c);
kbd_drain();
DELAY(0x10000);
printf("9");
	while(aux_cmd(0xf4) != KBR_ACK);
kbd_drain(); */
/*DELAY(0x10000);
printf("9");
	aux_cmd(0xf6);
kbd_drain(); */
printf("A");

	/* enable interrupts and keyboard controller */
	kbd_cmd_write_param(K_WRITE + K__CMDBYTE, ENABLE_CMDBYTE);
kbd_drain();
printf("B");

	/* enable interrupts and keyboard controller */
	/*kbd_cmd(K_DISKEY);
kbd_drain();
	kbd_cmd(K_DISAUX); */
kbd_drain();
	kbd_cmd_write_param(K_WRITE + K__CMDBYTE, 0x47);
kbd_drain();
printf("C");

/*	kbd_cmd(K_ENAAUX);
kbd_drain(); */
	kbd_cmd(K_ENAKEY);
kbd_drain();
	printf (" kbd %x ", kbd_cmd_read_param(K_READ + K__CMDBYTE));
kbd_drain();
	kbd_cmd(K_ENAAUX);
	return 1;
#endif

auxattach(dev)
struct isa_device *dev;
{
}

int
auxopen(dev_t dev, int flag, int mode, struct proc *p)
{
	register struct tty *tp;

	if(auxopenf) return(EBUSY);
	initrb(&rcv);
outb(0x311, 0x88);
DELAY(250);
outb(0x311, 0x80);
DELAY(2500);
while((inb(0x311)&4)==0) ;
outb(0x310, 0xf4);
while((inb(0x311)&4)==0) ;
while((inb(0x311)&1)==0) ;
(void)inb(0x310);
outb(0x311, 0x95);
	auxflag=0;
	auxopenf=1;
	return (0);
}

int
auxclose(dev_t dev, int flag, int mode, struct proc *p)
{
	outb(0x311, 0x00);
	auxflag=0;
	auxopenf=0;
	return(0);
}

/* 
 * auxread --copy a buffer to user space.
 */
int
auxread(dev_t dev, struct uio *uio, int flag)
{
	register unsigned n;
	register int s;
	int error = 0;

	s = splhigh();
	while (RB_LEN(&rcv) == 0) {
		if (flag & IO_NDELAY) {
			splx(s);
			return (EWOULDBLOCK);
		}
		auxflag = 1;
		if (error = tsleep((caddr_t)&rcv, PWAIT | PCATCH,
			"auxread", 0)) {
			splx(s);
			return (error);
		}
	}
	splx(s);
	auxflag =0;

	while (uio->uio_resid > 0) {
		n = min(RB_CONTIGGET(&rcv) , uio->uio_resid);
		if (n == 0)
			break;
		error = uiomove(rcv.rb_hd, n, uio);
		if (error)
			break;
		rcv.rb_hd = RB_ROLLOVER(&rcv, rcv.rb_hd+n);
	}
	return (error);
}

int
auxwrite(dev_t dev, struct uio *uio, int flag)
{
	char c;

/* printf("W %x ", inb(0x311)); */
	uiomove(&c, 1, uio);
	outb(0x310, c);
	return (0);
}

auxintr(dev)
	dev_t dev;
{

	if(auxopenf == 0) {
		/* printf("X %x %x ", inb(0x310), inb(0x311)); */
		return;
	}
	if (inb(0x311) & 2) {
	if(RB_LEN(&rcv) < RBSZ - 2)
		putc(inb(0x310), &rcv);
	else
		(void) inb(0x310);
	if(auxflag) wakeup((caddr_t)&rcv);
	auxflag = 0;
	if (auxpid) {
		selwakeup(auxpid, 0);
		auxpid= 0;
	}
#ifdef nope
	if (auxpgid) {
		struct proc *p;
		if (auxpgid < 0)
			gsignal(-auxpgid, SIGIO); 
		else if (p = pfind(auxpgid))
			psignal(p, SIGIO);
	}
#else
	signalpid(auxpgid);
#endif
	}
	return;
}

int
auxioctl(dev_t dev, int cmd, caddr_t data, int flag, struct proc *p)
{

	switch (cmd) {

	/* return number of characters immediately available */
	case FIONREAD:
		*(off_t *)data = RB_LEN(&rcv);
		break;

	case FIONBIO:
		break;

	/* case FIOASYNC:
		if (*(int *)data)
		else
		break; */

	case TIOCSPGRP:
		auxpgid = *(int *)data;
		break;

	case TIOCGPGRP:
		*(int *)data = auxpgid;
		break;

	default:
		return (ENOTTY);
	}
	return (0);
}

int
auxselect(dev_t dev, int rw, struct proc *p)
{
	int s = splhigh();

	switch (rw) {

	case FREAD:
		if (RB_LEN(&rcv)) {
			splx(s);
			return (1);
		}
		auxpid = p->p_pid;
		break;
	}
	splx(s);
	return (0);
}

static struct devif aux_devif =
{
	{0}, -1, -2, 0, 0, 0, 0, 0, 0,
	auxopen, auxclose, auxioctl, auxread, auxwrite, auxselect, 0,
	0, 0, 0, 0,
};

DRIVER_MODCONFIG() {
	char *cfg_string = aux_config;
	
	/* can configure an aux? */
	if (devif_config(&cfg_string, &aux_devif) == 0)
		return;

	/* probe for hardware */
	new_isa_configure(&cfg_string, &auxdriver);
}
