/*
 * Copyright (c) 1982, 1986, 1989 Regents of the University of California.
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
 *	$Id: reboot.c,v 1.1 94/10/20 00:03:12 bill Exp $
 */
#include "sys/param.h"
#include "sys/reboot.h"
#include "privilege.h"
#include "buf.h"
#include "proc.h"
#include "machine/pcb.h"

dev_t	dumpdev = BLK_NODEV;
unsigned	dumpmag = 0x8fca0101;	/* magic number for savecore */
int		dumpsize = 0;		/* also for savecore */
extern int	dumpsize;
int	waittime = -1;
struct pcb dumppcb;

/* BSD force system to boot */
int
reboot(p, uap, retval)
	struct proc *p;
	struct args {
		int	opt;
	} *uap;
	int *retval;
{
	int error;

	if (error = use_priv(p->p_ucred, PRV_REBOOT, p))
		return (error);
	boot(uap->opt);
	return (0);
}

/* implement a boot, either instigated from user or panic */
void
boot(int arghowto)
{
	register int howto;
	register int devtype;
	extern char *panicstr;
	extern int cold;
	int nomsg = 1;

	if (cold) {
		printf("hit reset please");
		for(;;);
	}
	howto = arghowto;
	if ((howto&RB_NOSYNC) == 0 && waittime < 0 && bfreelist[0].b_forw) {
		register struct buf *bp;
		int iter, nbusy;

		waittime = 0;
		(void) splnet();
		/*
		 * Release vnodes held by processes before update.
		 */
		if (panicstr == 0)
			vnode_pager_umount(NULL);
		sync((struct sigcontext *)0);

		for (iter = 0; iter < 20; iter++) {
			nbusy = 0;
			for (bp = &buf[nbuf]; --bp >= buf; )
				if ((bp->b_flags & (B_BUSY|B_INVAL)) == B_BUSY)
					nbusy++;
			if (nbusy == 0)
				break;
			if (nomsg) {
				printf("updating disks before rebooting... ");
				nomsg = 0;
			}
			/* printf("%d ", nbusy); */
			DELAY(40000 * iter);
		}
		if (nbusy)
			printf(" failed!\n");
		else if (nomsg == 0)
			printf("succeded.\n");
		DELAY(10000);			/* wait for printf to finish */
	}

	splhigh();

	if (howto&RB_HALT) {
		pg("\nThe operating system has halted. Please press any key to reboot.\n\n");
	} else {
		if (howto & RB_DUMP)
			dumpsys();	
	}

	cpu_reset(/*howto, devtype*/);
	for(;;)
		;
	/* NOTREACHED */
}
