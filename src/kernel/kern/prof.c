/*
 * Copyright (c) 1994 William F. Jolitz, TeleMuse
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
 *	William F. Jolitz, TeleMuse.
 * 4. Neither the name of the developer nor the name "386BSD"
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 5. Non-commercial distribution of this complete file in either source and/or
 *    binary form at no charge to the user (such as from an official Internet 
 *    archive site) is permitted. 
 * 6. Commercial distribution and sale of this complete file in either source
 *    and/or binary form on any media, including that of floppies, tape, or 
 *    CD-ROM, or through a per-charge download such as that of a BBS, is not 
 *    permitted without specific prior written permission.
 * 7. Non-commercial and/or commercial distribution of an incomplete, altered, 
 *    or otherwise modified file in either source and/or binary form is not 
 *    permitted.
 *
 * THIS SOFTWARE IS A COMPONENT OF 386BSD DEVELOPED BY WILLIAM F. JOLITZ 
 * AND IS INTENDED FOR RESEARCH AND EDUCATIONAL PURPOSES ONLY. THIS 
 * SOFTWARE SHOULD NOT BE CONSIDERED TO BE A COMMERCIAL PRODUCT. 
 * THE DEVELOPER URGES THAT USERS WHO REQUIRE A COMMERCIAL PRODUCT 
 * NOT MAKE USE OF THIS WORK.
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
 * $Id: prof.c,v 1.1 94/10/20 00:03:08 bill Exp $
 *
 * Process profiling control.
 */

#include "sys/param.h"
#include "sys/user.h"
#include "sys/file.h"
#include "sys/mman.h"
#include "resourcevar.h"
#include "vm.h"
#include "vmspace.h"

#include "machine/reg.h"
#include "machine/psl.h"
#include "machine/cpu.h"

#include "prototypes.h"

/*
 * Request process profiling system call handler.
 */
int
profil(p, args, retval)
	struct proc *p;
	struct uprof *args;
	int *retval;
{
	caddr_t base;
	int sz;
	
	/* insure offset never exceeds user address space size */
	if (args->pr_off > VM_MAX_ADDRESS)
		return (EFAULT);

	/* next, check that the buffer is writable */
	base = (caddr_t)args->pr_base;
	sz = args->pr_size * sizeof (short);
	if (vmspace_access(p->p_vmspace, base, sz, PROT_WRITE) == 0)
		return EFAULT;

	/* force allocated and resident the statitics buffer */
	vmspace_notpageable(p->p_vmspace, base, sz);

	/* force mapped the buffer to avoid skewed results */
	vmspace_activate(p->p_vmspace, base, sz);

	/* load profiling arguements, start profiling on next clock tick */
	p->p_stats->p_prof = *args;

	return (0);
}
