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
 * $Id: main.c,v 1.1 94/10/20 00:03:02 bill Exp Locker: bill $
 */
static char *kern_config =
	"kernel maxusers 5 hz 100.";

#include "sys/param.h"
#include "sys/user.h"		/* ... for signals in kstack ... */
#include "sys/mount.h"
#include "sys/stat.h"
#include "sys/reboot.h"
#include "filedesc.h"
#include "kernel.h"		/* for boot time and time */
#include "resourcevar.h"
#include "uio.h"
#include "malloc.h"
#include "modconfig.h"

#include "vnode.h"
/*#include "quota.h" ??? */

#include "prototypes.h"


char	copyright1[] =
"386BSD Release 1.0 by William & Lynne Jolitz.";
char	copyright2[] =
"Copyright (c) 1989-1994 William F. Jolitz. All rights reserved.\n";

/* kernel starts out with a single, statically created process */
int	nprocs = 1;

/*
 * Components of process 0;
 * never freed.
 */
struct	session session0;
struct	pgrp pgrp0;
struct	proc proc0;
/* credentials never to be freed, superuser, role all, group wheel */
struct	ucred ucred0 = { 2, 0, 0, 1, { 0 } };
struct	pcred cred0 = { &ucred0, 0, 0, 0, 0, 1 };
struct	filedesc0 filedesc0;
struct	plimit limit0;
struct	vmspace vmspace0, kernspace;
struct	pstats pstat0;
struct	proc *curproc = &proc0;
struct	proc *initproc, *pageproc;

int	cmask = CMASK;
extern	struct user *proc0paddr;
extern	int (*mountroot)();

struct vfsops *rootvfs;
struct vnode *rootvp;
struct vnode *rootdir;
extern int boothowto;
extern int bootdev;
extern int cold;

/*
 * Configurable parameters
 */
int maxusers, hz, maxproc, maxfiles, ncallout, tzh, dst;
long desiredvnodes;

int tick, tickadj;
struct timezone tz;
struct proc *pidhash[64];
struct pgrp *pgrphash[64];
int pidhashmask = 64 -1;

/* virtual memory */
static int pgsz, kmemsz, kmemall;

struct namelist kern_options[] =
{
	"maxusers",		&maxusers,	NUMBER,
	"hz", 			&hz,		NUMBER,
	"maxproc",		&maxproc,	NUMBER,
	"desiredvnodes",	&desiredvnodes,	NUMBER,
	"maxfiles",		&maxfiles,	NUMBER,
	"ncallout",		&ncallout,	NUMBER,
	"tz",			&tzh,		NUMBER,
	"dst",			&dst,		NUMBER,
	"pagesize",		&pgsz,		NUMBER,
	"kmemsize",		&kmemsz,	NUMBER,
	"kmemalloc",		&kmemall,	NUMBER,
	0,			0,		0
};

/*
 * Usually not configurable parameters
 */
int fscale = FSCALE;		/* kernel uses `FSCALE', user uses `fscale' */
int nmbclusters = NMBCLUSTERS;	/* number of mbuf clusters in free pool*/

/*
 * System startup; initialize the world, create process 0,
 * mount root filesystem, and fork to create init and pagedaemon.
 * Most of the hard work is done in the lower-level initialization
 * routines including startup(), which does memory initialization
 * and autoconfiguration.
 */
main()
{
	int i;
	struct proc *p;
	struct filedesc0 *fdp;
	int s, fs, rval[2];
	dev_t tdev;
	char *cfg;

	/*
	 * Initialize curproc before any possible traps/probes
	 * to simplify trap processing.
	 */
	p = &proc0;
	curproc = p;
	p->p_addr = proc0paddr;				/* XXX */
	p->p_stats = &pstat0;

	/*
 	 * Configure kernel variables prior to configuring major
	 * kernel subsystems.
	 */
	(void)config_scan(kern_config, &cfg);
	(void)cfg_namelist(&cfg, kern_options);
	if (maxusers < 4)
		maxusers = 4;
	if (maxusers > 200)
		maxusers = 200;
	i = 20 + 16 * maxusers;
	if (maxproc < i)
		maxproc = i;
	i = 2 * maxproc + 100;
	if (desiredvnodes < i)
		desiredvnodes = i;
	i = 3 * maxproc + 100;
	if (maxfiles < i)
		maxfiles = i;
	i = 16 + maxproc;
	if (ncallout < i);
		ncallout = i;

	/* process rescheduling clock interval */
	if (hz < 10 || hz > 1000)
		hz = 100;
	tick = 1000000/hz;	/* microseconds in a clock "tick" */
	tickadj = 240000/(60*hz); /* microseconds of adjustment in a minute */

	/* local time zone & daylight savings -- needed for rtc */
	tz.tz_minuteswest = tzh * 60 * 60;
	tz.tz_dsttime = dst;
	
	startrtclock();

	/*
	 * Initialize the virtual memory system, layer by layer.
	 */

	/* physical layer */
	/* vm_set_page_size(pgsz);  XXX temp moved to init386 */
	vm_page_startup();
	vm_object_init();

	/* virtual layer */
	vm_map_startup(/*VM_KMEM_SIZE + VM_MBUF_SIZE + VM_PHYS_SIZE,
	    MAXALLOCSAVE*/);
	kmem_init();

	/* address translation layer */
	pmap_init();
	/* pager layer */
	vm_pager_init(/*VM_PHYS_SIZE*/);

	kmeminit(/*VM_KMEM_SIZE, MAXALLOCSAVE*/);

	/* configure any extended module services before rest of modules */
	modscaninit(MODT_EXTDMOD);
	cpu_startup();
isa_configure();

	modscaninit(MODT_LDISC);

#ifdef notyet
	/*
	 * Allocate process list hash tables
	 */
	for (i = 64; i < maxproc/2 ; i <<= 1)
		;
	pidhash = (struct proc **) malloc(i * sizeof(struct proc *),
			M_TEMP, M_WAITOK);
	memset((void *)pidhash, 0, i * sizeof(struct proc *));
	pgrphash = (struct pgrp **) malloc(i * sizeof(struct pgrp *),
			M_TEMP, M_WAITOK);
	memset((void *)pgrphash, 0, i * sizeof(struct pgrp *));
	pidhashmask = i - 1;
#endif

	/*
	 * Attach the substructures of the statically allocated
	 * system process 0 (pageout), and insert it into the kernel
	 * global data variables and lists.
	 */
	p = &proc0;
	curproc = p;		/* current running process */

	allproc = p;		/* on the allproc list */
	p->p_prev = &allproc;

	p->p_pgrp = &pgrp0;	/* in the first process group, ... */
	pgrphash[0] = &pgrp0;
	pgrp0.pg_mem = p;
	pgrp0.pg_session = &session0;	/* ... the first session. */
	session0.s_count = 1;
	session0.s_leader = p;

	p->p_flag = SLOAD|SSYS;		/* running system process */
	p->p_stat = SRUN;
	p->p_nice = NZERO;
	memcpy(p->p_comm, "pageout", sizeof ("pageout"));

	/* next, link on credentials, ... */
	p->p_cred = &cred0;

	/* ... a initial file descriptor table, ... */
	fdp = &filedesc0;
	p->p_fd = &fdp->fd_fd;
	fdp->fd_fd.fd_refcnt = 1;
	fdp->fd_fd.fd_cmask = cmask;
	fdp->fd_fd.fd_ofiles = fdp->fd_dfiles;
	fdp->fd_fd.fd_ofileflags = fdp->fd_dfileflags;
	fdp->fd_fd.fd_nfiles = NDFILE;

	/* ... an initial set of resource limits */
	p->p_limit = &limit0;
	for (i = 0; i < sizeof(p->p_rlimit)/sizeof(p->p_rlimit[0]); i++)
		limit0.pl_rlimit[i].rlim_cur =
		    limit0.pl_rlimit[i].rlim_max = RLIM_INFINITY;
	limit0.pl_rlimit[RLIMIT_OFILE].rlim_cur = OPEN_MAX;
	limit0.pl_rlimit[RLIMIT_OFILE].rlim_max = FD_SETSIZE; /* XXX */
	limit0.pl_rlimit[RLIMIT_NPROC].rlim_cur = CHILD_MAX;
	limit0.p_refcnt = 1;

	/*
	 * Allocate a prototype map so we have something to fork
	 */
	p->p_vmspace = &vmspace0;
	vmspace0.vm_refcnt = 1;
	vmspace0.vm_ssize = 1;
	pmap_pinit(&vmspace0.vm_pmap);
	vm_map_init(&p->p_vmspace->vm_map, round_page(VM_MIN_ADDRESS),
	    trunc_page(VM_MAX_ADDRESS), TRUE);
	vmspace0.vm_map.pmap = &vmspace0.vm_pmap;
	p->p_addr = proc0paddr;				/* XXX */
	p->p_stksz = UPAGES * CLBYTES;			/* XXX */

	/* initialize signals */
	p->p_sigacts = &p->p_addr->u_sigacts;
	siginit(p);

	rqinit();
	cold = 0;

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

	/* discover all filesystems and initialize them */
	modscaninit(MODT_FILESYSTEM);
	vfsinit();

	/* probe for boot device */
	if (B_ISVALID(bootdev) == 0 || devif_root(B_TYPE(bootdev),
	    B_UNIT(bootdev), B_PARTITION(bootdev), &tdev) == 0)
		panic("bootdev");

	if (bdevvp(tdev, &rootvp))
		panic("rootvp");

	/*
	 * Initialize tables, protocols, and set up well-known inodes.
	 */
	mbinit();
	modscaninit(MODT_SLIP);
	
	modscaninit(MODT_LOCALNET);

	/*
	 * Block reception of incoming packets
	 * until protocols have been initialized.
	 */
	s = splimp();
	modscaninit(MODT_KERNEL);
	ifinit();
	modscaninit(MODT_NETWORK);
	domaininit();
	splx(s);

#ifdef GPROF
	kmstartup();
#endif

	/* kick off timeout driven events by calling first time */
	roundrobin();
	schedcpu();
	enablertclock();		/* enable realtime clock interrupts */

	fs = B_FS(bootdev);
	/* old bootstrap XXX */
	if (fs == 0)
		fs = 1;	/* assume UFS */

	/* locate requested file system */
	if ((rootvfs = findvfs(fs)) == 0)
		panic("findvfs");

	/* mount the root filesystem, making the root vnode accessible */
	if ((*(rootvfs->vfs_mountroot))())
		panic("cannot mount root");

	/* obtain the root vnode */
	if (VFS_ROOT(rootfs, &rootdir))
		panic("cannot find root vnode");

	/* set default root and current directory of initial process */
	fdp->fd_fd.fd_cdir = rootdir;
	VREF(fdp->fd_fd.fd_cdir);
	VOP_UNLOCK(rootdir);
	fdp->fd_fd.fd_rdir = NULL;

{ volatile int sstart, send;
if (load_module("inet", &sstart, &send)) {
printf("\n init ");
smodscaninit(__MODT_ALL__, sstart, send);
}
if(load_module("ed", &sstart, &send)) {
printf("\n init ");
smodscaninit(__MODT_ALL__, sstart, send);
}
if(load_module("nfs", &sstart, &send)) {
printf("\n init ");
smodscaninit(__MODT_ALL__, sstart, send);
}
printf("after init\n");
/*ifinit();*/
/*domaininit();*/
}
	swapinit();	/* XXX */

	/*
	 * Now can look at time, having had a chance
	 * to verify the time from the file system.
	 */
	boottime = p->p_stats->p_start = time;

	/*
	 * make the init process
	 */
	if (fork(p, (void *) NULL, rval))
		panic("fork init");
	if (rval[1]) {
		static char initflags[] = "-sf";
		char *ip = initflags + 1;
		vm_offset_t addr = 0;
		extern int icode[];		/* user init code */
		extern int szicode;		/* size of icode */

		/*
		 * Now in process 1. SImulate an execve() by setting
		 * init flags into icode,
		 * get a minimal address space, copy out "icode",
		 * and return to it to do an exec of init.
		 */
		initproc = p = curproc;
		if (boothowto&RB_SINGLE)
			*ip++ = 's';
#ifdef notyet
		if (boothowto&RB_FASTBOOT)
			*ip++ = 'f';
#endif
		*ip++ = '\0';

		/* allocate space for user bootstrap program */
		if (vmspace_allocate(p->p_vmspace, &addr,
		    szicode + sizeof(initflags), FALSE) != 0)
			panic("init: couldn't allocate at zero");

		/* need just enough stack to exec from */
		addr = trunc_page(USRSTACK - NBPG);
		if (vmspace_allocate(p->p_vmspace, &addr, NBPG, FALSE)
		    != KERN_SUCCESS)
			panic("init: couldn't allocate init stack");

		p->p_vmspace->vm_maxsaddr = (caddr_t)addr + NBPG - MAXSSIZ;
		(void) copyout(p, (caddr_t)icode, (caddr_t)0, (unsigned)szicode);
		(void) copyout(p, initflags, (caddr_t)szicode, sizeof(initflags));
		return;			/* returns to icode */
	}

	/* initial process 0 becomes pageout process */
	pageproc = p;
	p->p_flag |= SLOAD|SSYS;		/* XXX */
	vm_pageout();
	/* NOTREACHED */
}
