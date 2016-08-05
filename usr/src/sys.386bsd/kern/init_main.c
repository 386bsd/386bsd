/*
 * Copyright (c) 1982, 1986, 1989, 1991 Regents of the University of California.
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
 *	@(#)init_main.c	7.41 (Berkeley) 5/15/91
 */
static char rcsid[] = "$Header: /usr/src/sys.386bsd/kern/RCS/init_main.c,v 1.3 92/01/21 21:28:49 william Exp Locker: root $";

#include "param.h"
#include "filedesc.h"
#include "kernel.h"
#include "mount.h"
#include "proc.h"
#include "resourcevar.h"
#include "signalvar.h"
#include "systm.h"
#include "vnode.h"
#include "conf.h"
#include "buf.h"
#include "malloc.h"
#include "protosw.h"
#include "reboot.h"
#include "user.h"

#include "ufs/quota.h"

#include "machine/cpu.h"

#include "vm/vm.h"

char	copyright1[] =
"386BSD Release 0.1 by William and Lynne Jolitz.";
char	copyright2[] =
"Copyright (c) 1989,1990,1991,1992 William F. Jolitz. All rights reserved.\n\
Based in part on work by the 386BSD User Community and the\n\
BSD Networking Software, Release 2 by UCB EECS Department.\n";

/*
 * Components of process 0;
 * never freed.
 */
struct	session session0;
struct	pgrp pgrp0;
struct	proc proc0;
struct	pcred cred0;
struct	filedesc0 filedesc0;
struct	plimit limit0;
struct	vmspace vmspace0;
struct	proc *curproc = &proc0;
struct	proc *initproc, *pageproc;

int	cmask = CMASK;
extern	struct user *proc0paddr;
extern	int (*mountroot)();

struct	vnode *rootvp, *swapdev_vp;
int	boothowto;

/*
 * System startup; initialize the world, create process 0,
 * mount root filesystem, and fork to create init and pagedaemon.
 * Most of the hard work is done in the lower-level initialization
 * routines including startup(), which does memory initialization
 * and autoconfiguration.
 */
main()
{
	register int i;
	register struct proc *p;
	register struct filedesc0 *fdp;
	int s, rval[2];

	/*
	 * Initialize curproc before any possible traps/probes
	 * to simplify trap processing.
	 */
	p = &proc0;
	curproc = p;
	/*
	 * Attempt to find console and initialize
	 * in case of early panic or other messages.
	 */
	startrtclock();
	consinit();
	printf("\033[3;15x%s\033[0x [0.1.%s]\n", copyright1, version+9);
	printf(copyright2);

	vm_mem_init();
	kmeminit();
	cpu_startup();

	/*
	 * set up system process 0 (swapper)
	 */
	p = &proc0;
	curproc = p;

	allproc = p;
	p->p_prev = &allproc;
	p->p_pgrp = &pgrp0;
	pgrphash[0] = &pgrp0;
	pgrp0.pg_mem = p;
	pgrp0.pg_session = &session0;
	session0.s_count = 1;
	session0.s_leader = p;

	p->p_flag = SLOAD|SSYS;
	p->p_stat = SRUN;
	p->p_nice = NZERO;
	bcopy("swapper", p->p_comm, sizeof ("swapper"));

	/*
	 * Setup credentials
	 */
	cred0.p_refcnt = 1;
	p->p_cred = &cred0;
	p->p_ucred = crget();
	p->p_ucred->cr_ngroups = 1;	/* group 0 */

	/*
	 * Create the file descriptor table for process 0.
	 */
	fdp = &filedesc0;
	p->p_fd = &fdp->fd_fd;
	fdp->fd_fd.fd_refcnt = 1;
	fdp->fd_fd.fd_cmask = cmask;
	fdp->fd_fd.fd_ofiles = fdp->fd_dfiles;
	fdp->fd_fd.fd_ofileflags = fdp->fd_dfileflags;
	fdp->fd_fd.fd_nfiles = NDFILE;

	/*
	 * Set initial limits
	 */
	p->p_limit = &limit0;
	for (i = 0; i < sizeof(p->p_rlimit)/sizeof(p->p_rlimit[0]); i++)
		limit0.pl_rlimit[i].rlim_cur =
		    limit0.pl_rlimit[i].rlim_max = RLIM_INFINITY;
	limit0.pl_rlimit[RLIMIT_OFILE].rlim_cur = NOFILE;
	limit0.pl_rlimit[RLIMIT_NPROC].rlim_cur = MAXUPRC;
	limit0.p_refcnt = 1;

	/*
	 * Allocate a prototype map so we have something to fork
	 */
	p->p_vmspace = &vmspace0;
	vmspace0.vm_refcnt = 1;
	pmap_pinit(&vmspace0.vm_pmap);
	vm_map_init(&p->p_vmspace->vm_map, round_page(VM_MIN_ADDRESS),
	    trunc_page(VM_MAX_ADDRESS), TRUE);
	vmspace0.vm_map.pmap = &vmspace0.vm_pmap;
	p->p_addr = proc0paddr;				/* XXX */

	/*
	 * We continue to place resource usage info
	 * and signal actions in the user struct so they're pageable.
	 */
	p->p_stats = &p->p_addr->u_stats;
	p->p_sigacts = &p->p_addr->u_sigacts;

	rqinit();

	/*
	 * configure virtual memory system,
	 * set vm rlimits
	 */
	vm_init_limits(p);

	/*
	 * Initialize the file systems.
	 *
	 * Get vnodes for swapdev and rootdev.
	 */
	vfsinit();
	if (bdevvp(swapdev, &swapdev_vp) || bdevvp(rootdev, &rootvp))
		panic("can't setup bdevvp's");

#if defined(vax)
#include "kg.h"
#if NKG > 0
	startkgclock();
#endif
#endif

	/*
	 * Initialize tables, protocols, and set up well-known inodes.
	 */
	mbinit();
#ifdef SYSVSHM
	shminit();
#endif
#include "sl.h"
#if NSL > 0
	slattach();			/* XXX */
#endif
#include "loop.h"
#if NLOOP > 0
	loattach();			/* XXX */
#endif
	/*
	 * Block reception of incoming packets
	 * until protocols have been initialized.
	 */
	s = splimp();
	ifinit();
	domaininit();
	splx(s);

#ifdef GPROF
	kmstartup();
#endif

	/* kick off timeout driven events by calling first time */
	roundrobin();
	schedcpu();
	enablertclock();		/* enable realtime clock interrupts */

	/*
	 * Set up the root file system and vnode.
	 */
	if ((*mountroot)())
		panic("cannot mount root");
	/*
	 * Get vnode for '/'.
	 * Setup rootdir and fdp->fd_fd.fd_cdir to point to it.
	 */
	if (VFS_ROOT(rootfs, &rootdir))
		panic("cannot find root vnode");
	fdp->fd_fd.fd_cdir = rootdir;
	VREF(fdp->fd_fd.fd_cdir);
	VOP_UNLOCK(rootdir);
	fdp->fd_fd.fd_rdir = NULL;
	swapinit();

	/*
	 * Now can look at time, having had a chance
	 * to verify the time from the file system.
	 */
	boottime = p->p_stats->p_start = time;

	/*
	 * make init process
	 */
	siginit(p);
	if (fork(p, (void *) NULL, rval))
		panic("fork init");
	if (rval[1]) {
		static char initflags[] = "-sf";
		char *ip = initflags + 1;
		vm_offset_t addr = 0;
		extern int icode[];		/* user init code */
		extern int szicode;		/* size of icode */

		/*
		 * Now in process 1.  Set init flags into icode,
		 * get a minimal address space, copy out "icode",
		 * and return to it to do an exec of init.
		 */
		p = curproc;
		initproc = p;
		if (boothowto&RB_SINGLE)
			*ip++ = 's';
#ifdef notyet
		if (boothowto&RB_FASTBOOT)
			*ip++ = 'f';
#endif
		*ip++ = '\0';

		if (vm_allocate(&p->p_vmspace->vm_map, &addr,
		    round_page(szicode + sizeof(initflags)), FALSE) != 0 ||
		    addr != 0)
			panic("init: couldn't allocate at zero");

		/* need just enough stack to exec from */
		addr = trunc_page(USRSTACK - MAXSSIZ);
		if (vm_allocate(&p->p_vmspace->vm_map, &addr,
		    MAXSSIZ, FALSE) != KERN_SUCCESS)
			panic("vm_allocate init stack");
		p->p_vmspace->vm_maxsaddr = (caddr_t)addr;
		p->p_vmspace->vm_ssize = 1;
		(void) copyout((caddr_t)icode, (caddr_t)0, (unsigned)szicode);
		(void) copyout(initflags, (caddr_t)szicode, sizeof(initflags));
		return;			/* returns to icode */
	}

	/*
	 * Start up pageout daemon (process 2).
	 */
	if (fork(p, (void *) NULL, rval))
		panic("fork pager");
	if (rval[1]) {
		/*
		 * Now in process 2.
		 */
		p = curproc;
		pageproc = p;
		p->p_flag |= SLOAD|SSYS;		/* XXX */
		bcopy("pagedaemon", curproc->p_comm, sizeof ("pagedaemon"));
		vm_pageout();
		/*NOTREACHED*/
	}

	/*
	 * enter scheduling loop
	 */
	sched();
}
