
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
 *	@(#)vm_meter.c	7.11 (Berkeley) 4/20/91
 */

#include "param.h"
#include "proc.h"
#include "systm.h"
#include "kernel.h"

#include "vm_param.h"
#include "vmmeter.h"

fixpt_t	averunnable[3];		/* load average, of runnable procs */

int	maxslp = MAXSLP;
int	saferss = SAFERSS;


vmmeter()
{
	register unsigned *cp, *rp, *sp;

	if (time.tv_sec % 5 == 0)
		vmtotal();
	if (proc0.p_slptime > maxslp/2)
		wakeup((caddr_t)&proc0);
}

vmtotal()
{
	register struct proc *p;
	int nrun = 0;

	total.t_vm = 0;
	total.t_avm = 0;
	total.t_rm = 0;
	total.t_arm = 0;
	total.t_rq = 0;
	total.t_dw = 0;
	total.t_pw = 0;
	total.t_sl = 0;
	total.t_sw = 0;
	for (p = allproc; p != NULL; p = p->p_nxt) {
		if (p->p_flag & SSYS)
			continue;
		if (p->p_stat) {
			switch (p->p_stat) {

			case SSLEEP:
				if (p->p_pri <= PZERO && p->p_slptime == 0)
					nrun++;
				/* fall through */
			case SSTOP:
#ifdef notdef
				if (p->p_flag & SPAGE)
					total.t_pw++;
				else
#endif
				if (p->p_flag & SLOAD) {
					if (p->p_pri <= PZERO)
						total.t_dw++;
					else if (p->p_slptime < maxslp)
						total.t_sl++;
				} else if (p->p_slptime < maxslp)
					total.t_sw++;
				if (p->p_slptime < maxslp)
					goto active;
				break;

			case SRUN:
			case SIDL:
				nrun++;
				if (p->p_flag & SLOAD)
					total.t_rq++;
				else
					total.t_sw++;
active:
				break;
			}
		}
	}
	loadav(averunnable, nrun);
}

/*
 * Constants for averages over 1, 5, and 15 minutes
 * when sampling at 5 second intervals.
 */
fixpt_t	cexp[3] = {
	0.9200444146293232 * FSCALE,	/* exp(-1/12) */
	0.9834714538216174 * FSCALE,	/* exp(-1/60) */
	0.9944598480048967 * FSCALE,	/* exp(-1/180) */
};

/*
 * Compute a tenex style load average of a quantity on
 * 1, 5 and 15 minute intervals.
 */
loadav(avg, n)
	register fixpt_t *avg;
	int n;
{
	register int i;

	for (i = 0; i < 3; i++)
		avg[i] = (cexp[i] * avg[i] + n * FSCALE * (FSCALE - cexp[i]))
		         >> FSHIFT;
#if defined(COMPAT_43) && (defined(vax) || defined(tahoe))
	for (i = 0; i < 3; i++)
		avenrun[i] = (double) averunnable[i] / FSCALE;
#endif /* COMPAT_43 */
}










