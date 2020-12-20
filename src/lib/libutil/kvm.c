/*-
 * Copyright (c) 1989 The Regents of the University of California.
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
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)kvm.c	5.18 (Berkeley) 5/7/91";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <sys/user.h>
#include "proc.h"
#include <sys/ioctl.h>
#include <sys/kinfo.h>
#include "tty.h"
#include <fcntl.h>
#include <nlist.h>
#include <kvm.h>
#include <ndbm.h>
#include <limits.h>
#include <paths.h>
#include <stdio.h>
#include <string.h>

#define	btop(x)		(((unsigned)(x)) >> PGSHIFT)	/* XXX */
#define	ptob(x)		((caddr_t)((x) << PGSHIFT))	/* XXX */
#include "vmspace.h"	/* ??? kinfo_proc currently includes this*/
#include <sys/kinfo_proc.h>
#ifdef hp300
#include <hp300/hp300/pte.h>
#endif

/*
 * files
 */
static	const char *unixf, *memf, *kmemf, *swapf;
static	int unixx, mem, kmem, swap;
static	DBM *db;
/*
 * flags
 */
static	int deadkernel;
static	int kvminit = 0;
static	int kvmfilesopen = 0;
/*
 * state
 */
static	struct kinfo_proc *kvmprocbase, *kvmprocptr;
static	int kvmnprocs;
/*
 * u. buffer
 */
static union {
	struct	user user;
	char	upages[UPAGES][NBPG];
} user;
/*
 * random other stuff
 */
static	int	dmmin, dmmax;
static	int	argaddr[howmany(ARG_MAX, NBPG)];	/* XXX */
static	int	nswap;
static	char	*tmp;
#if defined(hp300)
static	int	lowram;
static	struct ste *Sysseg;
#endif
#if defined(i386)
static	struct pde *PTD;
#endif

#define basename(cp)	((tmp=rindex((cp), '/')) ? tmp+1 : (cp))
#define	MAXSYMSIZE	256

#if defined(hp300)
#define pftoc(f)	((f) - lowram)
#define KERNBASE	0
#endif

#ifndef pftoc
#define pftoc(f)	(f)
#endif

u_long kernbase = KERNBASE;

#undef iskva
#define iskva(v)	((u_long)(v) >= kernbase)

static struct nlist nl[] = {
	{ "_Usrptmap" },
#define	X_USRPTMAP	0
	{ "_usrpt" },
#define	X_USRPT		1
	{ "_nswap" },
#define	X_NSWAP		2
	{ "_dmmin" },
#define	X_DMMIN		3
	{ "_dmmax" },
#define	X_DMMAX		4
	{ "_KERNBASE" },
#define	X_KERNBASE	5
	/*
	 * everything here and down, only if a dead kernel
	 */
	{ "_allproc" },
#define X_ALLPROC	6
#define	X_DEADKERNEL	X_ALLPROC
	{ "_zombproc" },
#define X_ZOMBPROC	7
	{ "_nproc" },
#define	X_NPROC		8
#define	X_LAST		8
#if defined(hp300)
	{ "_Sysseg" },
#define	X_SYSSEG	(X_LAST+1)
	{ "_lowram" },
#define	X_LOWRAM	(X_LAST+2)
#endif
#if defined(i386)
	{ "_KernelPTD" },
#define	X_KernelPTD	(X_LAST+1)
#endif
	{ "" },
};

static off_t Vtophys();
static void klseek(), seterr(), setsyserr(), vstodb();
static int getkvars(), kvm_doprocs(), kvm_init();

/*
 * returns 	0 if files were opened now,
 * 		1 if files were already opened,
 *		-1 if files could not be opened.
 */
kvm_openfiles(uf, mf, sf)
	const char *uf, *mf, *sf; 
{
	if (kvmfilesopen)
		return (1);
	unixx = mem = kmem = swap = -1;
	unixf = (uf == NULL) ? _PATH_UNIX : uf; 
	memf = (mf == NULL) ? _PATH_MEM : mf;

	if ((unixx = open(unixf, O_RDONLY, 0)) == -1) {
		setsyserr("can't open %s", unixf);
		goto failed;
	}
	if ((mem = open(memf, O_RDONLY, 0)) == -1) {
		setsyserr("can't open %s", memf);
		goto failed;
	}
	if (sf != NULL)
		swapf = sf;
	if (mf != NULL) {
		deadkernel++;
		kmemf = mf;
		kmem = mem;
		swap = -1;
	} else {
		kmemf = _PATH_KMEM;
		if ((kmem = open(kmemf, O_RDONLY, 0)) == -1) {
			setsyserr("can't open %s", kmemf);
			goto failed;
		}
		swapf = (sf == NULL) ?  _PATH_DRUM : sf;
		/*
		 * live kernel - avoid looking up nlist entries
		 * past X_DEADKERNEL.
		 */
		nl[X_DEADKERNEL].n_name = "";
	}
	if (swapf != NULL && ((swap = open(swapf, O_RDONLY, 0)) == -1)) {
		seterr("can't open %s", swapf);
		goto failed;
	}
	kvmfilesopen++;
	if (kvminit == 0 && kvm_init(NULL, NULL, NULL, 0) == -1) /*XXX*/
		return (-1);
	return (0);
failed:
	kvm_close();
	return (-1);
}

static
kvm_init(uf, mf, sf)
	char *uf, *mf, *sf;
{
	if (kvmfilesopen == 0 && kvm_openfiles(NULL, NULL, NULL) == -1)
		return (-1);
	if (getkvars() == -1)
		return (-1);
	kvminit = 1;

	return (0);
}

kvm_close()
{
	if (unixx != -1) {
		close(unixx);
		unixx = -1;
	}
	if (kmem != -1) {
		if (kmem != mem)
			close(kmem);
		/* otherwise kmem is a copy of mem, and will be closed below */
		kmem = -1;
	}
	if (mem != -1) {
		close(mem);
		mem = -1;
	}
	if (swap != -1) {
		close(swap);
		swap = -1;
	}
	if (db != NULL) {
		dbm_close(db);
		db = NULL;
	}
	kvminit = 0;
	kvmfilesopen = 0;
	deadkernel = 0;
}

kvm_nlist(nl)
	struct nlist *nl;
{
	datum key, data;
	char dbname[PATH_MAX];
	char dbversion[_POSIX2_LINE_MAX];
	char kversion[_POSIX2_LINE_MAX];
	int dbversionlen;
	char symbuf[MAXSYMSIZE];
	struct nlist nbuf, *n;
	int num, did;

	if (kvmfilesopen == 0 && kvm_openfiles(NULL, NULL, NULL) == -1)
		return (-1);
	if (deadkernel)
		goto hard2;
	/*
	 * initialize key datum
	 */
	key.dptr = symbuf;

	if (db != NULL)
		goto win;	/* off to the races */
	/*
	 * open database
	 */
	sprintf(dbname, "%s/kvm_%s", _PATH_VARRUN, basename(unixf));
	if ((db = dbm_open(dbname, O_RDONLY, 0)) == NULL)
		goto hard2;
	/*
	 * read version out of database
	 */
	bcopy("VERSION", symbuf, sizeof ("VERSION") - 1);
	key.dsize = (sizeof ("VERSION") - 1);
	data = dbm_fetch(db, key);
	if (data.dptr == NULL)
		goto hard1;
	bcopy(data.dptr, dbversion, data.dsize);
	dbversionlen = data.dsize;

	/*
	 * read version string from kernel memory
	 */
	bcopy("_version", symbuf, sizeof ("_version") - 1);
	key.dsize = (sizeof ("_version") - 1);
	data = dbm_fetch(db, key);
	if (data.dptr == NULL)
		goto hard1;
	if (data.dsize != sizeof (struct nlist))
		goto hard1;
	bcopy(data.dptr, &nbuf, sizeof (struct nlist));
	lseek(kmem, nbuf.n_value, 0);
	if (read(kmem, kversion, dbversionlen) != dbversionlen)
		goto hard1;

	/*
	 * if they match, we win - otherwise do it the hard way
	 */
	if (bcmp(dbversion, kversion, dbversionlen) != 0)
		goto hard1;
	/*
	 * getem from the database.
	 */
win:
	num = did = 0;
	for (n = nl; n->n_name && n->n_name[0]; n++, num++) {
		int len;
		/*
		 * clear out fields from users buffer
		 */
		n->n_type = 0;
		n->n_other = 0;
		n->n_desc = 0;
		n->n_value = 0;
		/*
		 * query db
		 */
		if ((len = strlen(n->n_name)) > MAXSYMSIZE) {
			seterr("symbol too large");
			return (-1);
		}
		(void)strcpy(symbuf, n->n_name);
		key.dsize = len;
		data = dbm_fetch(db, key);
		if (data.dptr == NULL || data.dsize != sizeof (struct nlist))
			continue;
		bcopy(data.dptr, &nbuf, sizeof (struct nlist));
		n->n_value = nbuf.n_value;
		n->n_type = nbuf.n_type;
		n->n_desc = nbuf.n_desc;
		n->n_other = nbuf.n_other;
		did++;
	}
	return (num - did);
hard1:
	dbm_close(db);
	db = NULL;
hard2:
	num = nlist(unixf, nl);
	if (num == -1)
		seterr("nlist (hard way) failed");
	return (num);
}

kvm_getprocs(what, arg)
	int what, arg;
{
	if (kvminit == 0 && kvm_init(NULL, NULL, NULL, 0) == -1)
		return (NULL);
	if (!deadkernel) {
		int ret, copysize;

		if ((ret = getkerninfo(what, NULL, NULL, arg)) == -1) {
			setsyserr("can't get estimate for kerninfo");
			return (-1);
		}
		copysize = ret;
		if ((kvmprocbase = (struct kinfo_proc *)malloc(copysize)) 
		     == NULL) {
			seterr("out of memory");
			return (-1);
		}
		if ((ret = getkerninfo(what, kvmprocbase, &copysize, 
		     arg)) == -1) {
			setsyserr("can't get proc list");
			return (-1);
		}
		if (copysize % sizeof (struct kinfo_proc)) {
			seterr("proc size mismatch (got %d total, kinfo_proc: %d)",
				copysize, sizeof (struct kinfo_proc));
			return (-1);
		}
		kvmnprocs = copysize / sizeof (struct kinfo_proc);
	} else {
		int nproc;

		if (kvm_read((void *) nl[X_NPROC].n_value, &nproc,
		    sizeof (int)) != sizeof (int)) {
			seterr("can't read nproc");
			return (-1);
		}
		if ((kvmprocbase = (struct kinfo_proc *)
		     malloc(nproc * sizeof (struct kinfo_proc))) == NULL) {
			seterr("out of memory (addr: %x nproc = %d)",
				nl[X_NPROC].n_value, nproc);
			return (-1);
		}
		kvmnprocs = kvm_doprocs(what, arg, kvmprocbase);
		realloc(kvmprocbase, kvmnprocs * sizeof (struct kinfo_proc));
	}
	kvmprocptr = kvmprocbase;

	return (kvmnprocs);
}

/*
 * XXX - should NOT give up so easily - especially since the kernel
 * may be corrupt (it died).  Should gather as much information as possible.
 * Follows proc ptrs instead of reading table since table may go
 * away soon.
 */
static
kvm_doprocs(what, arg, buff)
	int what, arg;
	char *buff;
{
	struct proc *p, proc;
	register char *bp = buff;
	int i = 0;
	int doingzomb = 0;
	struct eproc eproc;
	struct pgrp pgrp;
	struct session sess;
	struct tty tty;
#ifdef texts
	struct text text;
#endif

	/* allproc */
	if (kvm_read((void *) nl[X_ALLPROC].n_value, &p, 
	    sizeof (struct proc *)) != sizeof (struct proc *)) {
		seterr("can't read allproc");
		return (-1);
	}

again:
	for (; p; p = proc.p_nxt) {
		if (kvm_read(p, &proc, sizeof (struct proc)) !=
		    sizeof (struct proc)) {
			seterr("can't read proc at %x", p);
			return (-1);
		}
		if (kvm_read(proc.p_cred, &eproc.e_pcred,
		    sizeof (struct pcred)) == sizeof (struct pcred))
			(void) kvm_read(eproc.e_pcred.pc_ucred, &eproc.e_ucred,
			    sizeof (struct ucred));
		switch(ki_op(what)) {
			
		case KINFO_PROC_PID:
			if (proc.p_pid != (pid_t)arg)
				continue;
			break;


		case KINFO_PROC_UID:
			if (eproc.e_ucred.cr_uid != (uid_t)arg)
				continue;
			break;

		case KINFO_PROC_RUID:
			if (eproc.e_pcred.p_ruid != (uid_t)arg)
				continue;
			break;
		}

		/*
		 * gather eproc
		 */
		eproc.e_paddr = p;
		if (kvm_read(proc.p_pgrp, &pgrp, sizeof (struct pgrp)) !=
	            sizeof (struct pgrp)) {
			seterr("can't read pgrp at %x", proc.p_pgrp);
			return (-1);
		}
		eproc.e_sess = pgrp.pg_session;
		eproc.e_pgid = pgrp.pg_id;
		eproc.e_jobc = pgrp.pg_jobc;
		if (kvm_read(pgrp.pg_session, &sess, sizeof (struct session))
		   != sizeof (struct session)) {
			seterr("can't read session at %x", pgrp.pg_session);
			return (-1);
		}
		if ((proc.p_flag&SCTTY) && sess.s_ttyp != NULL) {
			if (kvm_read(sess.s_ttyp, &tty, sizeof (struct tty))
			    != sizeof (struct tty)) {
				seterr("can't read tty at %x", sess.s_ttyp);
				return (-1);
			}
			eproc.e_tdev = tty.t_dev;
			eproc.e_tsess = tty.t_session;
			if (tty.t_pgrp != NULL) {
				if (kvm_read(tty.t_pgrp, &pgrp, sizeof (struct
				    pgrp)) != sizeof (struct pgrp)) {
					seterr("can't read tpgrp at &x", 
						tty.t_pgrp);
					return (-1);
				}
				eproc.e_tpgid = pgrp.pg_id;
			} else
				eproc.e_tpgid = -1;
		} else
			eproc.e_tdev = TTY_NODEV;
		if (proc.p_wmesg)
			kvm_read(proc.p_wmesg, eproc.e_wmesg, WMESGLEN);
#ifndef texts
		(void) kvm_read(proc.p_vmspace, &eproc.e_vm,
		    sizeof (struct vmspace));
		/*eproc.e_xsize = eproc.e_xrssize =
			= eproc.e_xswrss = 0;*/
#else
		if (proc.p_textp) {
			kvm_read(proc.p_textp, &text, sizeof (text));
			eproc.e_xsize = text.x_size;
			eproc.e_xrssize = text.x_rssize;
			eproc.e_xccount = text.x_ccount;
			eproc.e_xswrss = text.x_swrss;
		} else {
			eproc.e_xsize = eproc.e_xrssize =
			  eproc.e_xccount = eproc.e_xswrss = 0;
		}
#endif

		switch(ki_op(what)) {

		case KINFO_PROC_PGRP:
			if (eproc.e_pgid != (pid_t)arg)
				continue;
			break;

		case KINFO_PROC_TTY:
			if ((proc.p_flag&SCTTY) == 0 || 
			     eproc.e_tdev != (dev_t)arg)
				continue;
			break;
		}

		i++;
		bcopy(&proc, bp, sizeof (struct proc));
		bp += sizeof (struct proc);
		bcopy(&eproc, bp, sizeof (struct eproc));
		bp+= sizeof (struct eproc);
	}
	if (!doingzomb) {
		/* zombproc */
		if (kvm_read((void *) nl[X_ZOMBPROC].n_value, &p, 
		    sizeof (struct proc *)) != sizeof (struct proc *)) {
			seterr("can't read zombproc");
			return (-1);
		}
		doingzomb = 1;
		goto again;
	}

	return (i);
}

struct proc *
kvm_nextproc()
{

	if (!kvmprocbase && kvm_getprocs(0, 0) == -1)
		return (NULL);
	if (kvmprocptr >= (kvmprocbase + kvmnprocs)) {
		seterr("end of proc list");
		return (NULL);
	}
	return((struct proc *)(kvmprocptr++));
}

struct eproc *
kvm_geteproc(p)
	const struct proc *p;
{
	return ((struct eproc *)(((char *)p) + sizeof (struct proc)));
}

kvm_setproc()
{
	kvmprocptr = kvmprocbase;
}

kvm_freeprocs()
{

	if (kvmprocbase) {
		free(kvmprocbase);
		kvmprocbase = NULL;
	}
}

#ifdef i386
/* See also ./sys/kern/kern_execve.c */
#define ARGSIZE		(roundup(ARG_MAX, NBPG))
#endif

/*
 * Obtain uarea and location of process arguments
 */
struct user *
kvm_getu(p)
	const struct proc *p;
{
	register struct kinfo_proc *kp = (struct kinfo_proc *)p;
	register int i;
	register char *up;

	if (kvminit == 0 && kvm_init(NULL, NULL, NULL, 0) == -1)
		return (NULL);
	if (p->p_stat == SZOMB) {
		seterr("zombie process");
		return (NULL);
	}

	kp->kp_eproc.e_vm.vm_rssize =
	    kp->kp_eproc.e_vm.vm_pmap.pm_stats.resident_count; /* XXX */

	/*
	 * Reading from swap is too complicated right now.
	 */
	if ((p->p_flag & SLOAD) == 0)
		return(NULL);
	/*
	 * Read u-area one page at a time for the benefit of post-mortems
	 */
	up = (char *) p->p_addr;
	for (i = 0; i < UPAGES; i++) {
		klseek(kmem, (long)up, 0);
		if (read(kmem, user.upages[i], CLBYTES) != CLBYTES) {
			seterr("cant read page %x of u of pid %d from %s",
			    up, p->p_pid, kmemf);
			return(NULL);
		}
		up += CLBYTES;
	}

	/*
	 * Conjure up a physical addresses for the arguments.
	 */
	argaddr[0] = 0;

#ifdef i386
	/*
	 * XXX wrong approach: should not use pmap information (pde/pte's),
	 * because page could be resident, and translation not. must
	 * use vm_map/object/page to find resident pages, and pagers/swap
	 * to find secondary pages. -wfj
	 */
	if (kp->kp_eproc.e_vm.vm_pmap.pm_pdir) {
		struct pde pde;
		struct pte pte;
		unsigned vaddr =  (unsigned)kp->kp_eproc.e_vm.vm_maxsaddr
			+ MAXSSIZ - ARG_MAX, i;

		/* do address translation for process stack pages */
		for (i = 0 ; i  < howmany(ARG_MAX, NBPG) ; i++, vaddr+= NBPG) {
			/* get page directory entry(pde) out of the process */
			klseek(kmem,
			(long)(kp->kp_eproc.e_vm.vm_pmap.pm_pdir + pdei(vaddr)),
				0);
			if (read(kmem, (char *)&pde, sizeof pde) != sizeof pde
				|| !pde.pd_v)
				return(NULL);
  
			/* use pde to get page table entry(pte) out of the process */
			lseek(mem,
			   (long)ctob(pde.pd_pfnum) + ptei(vaddr) * sizeof pte,
				0);

			/* note, not all pages may be allocated */
			if (read(mem, (char *)&pte, sizeof pte) != sizeof pte
				|| !pte.pg_v)
				break;

			argaddr[i] = (long)ctob(pte.pg_pfnum);
		}
	}
#endif
	return(&user.user);
}

char *
kvm_getargs(p, up)
	const struct proc *p;
	const struct user *up;
{
#ifdef i386
	/* See also ./sys/kern/kern_execve.c */
	static char cmdbuf[ARGSIZE];
	static union {
		char	argc[ARGSIZE];
		int	argi[ARGSIZE/sizeof (int)];
	} argspac;
#else
	static char cmdbuf[CLBYTES*2];
	static union {
		char	argc[CLBYTES*2];
		int	argi[CLBYTES*2/sizeof (int)];
	} argspac;
#endif
	register char *cp;
	register int *ip;
	char c;
	int nbad;
#ifdef digoutofswap
	struct dblock db;
#endif
	const char *file;
	int stkoff = 0;

#if defined(i386)
	stkoff = 64;			/* XXX for sigcode */
#endif
	if (up == NULL || p->p_pid == 0 || p->p_pid == 2)
	if (up == NULL || p->p_pid == 0 || p->p_pid == 2)
		goto retucomm;
	if ((p->p_flag & SLOAD) == 0 || argaddr[0] == 0) {
#ifndef digoutofswap
		goto retucomm;	/* XXX for now */
#else
		if (swap < 0 || p->p_ssize == 0)
			goto retucomm;
		vstodb(0, CLSIZE, &up->u_smap, &db, 1);
		(void) lseek(swap, (long)dtob(db.db_base), 0);
		if (read(swap, (char *)&argspac.argc[CLBYTES], CLBYTES)
			!= CLBYTES)
			goto bad;
		vstodb(1, CLSIZE, &up->u_smap, &db, 1);
		(void) lseek(swap, (long)dtob(db.db_base), 0);
		if (read(swap, (char *)&argspac.argc[0], CLBYTES) != CLBYTES)
			goto bad;
		file = swapf;
#endif
	} else {
#ifdef i386
		char *ap = argspac.argc;
		int i;

		for (i = 0 ; i < howmany(ARG_MAX, NBPG) ; i++) {
			lseek(mem, (long)argaddr[i], 0);
			if (read(mem, ap, NBPG) != NBPG)
				goto bad;
			ap += NBPG;
		}
#endif
		file = (char *) memf;
	}
#ifdef i386
	ip = &argspac.argi[(ARGSIZE-ARG_MAX)/sizeof (int)];

	nbad = 0;
	for (cp = (char *)ip; cp < &argspac.argc[ARGSIZE-stkoff]; cp++) {
#else
  	ip = &argspac.argi[CLBYTES*2/sizeof (int)];
  	ip -= 2;                /* last arg word and .long 0 */
	ip -= stkoff / sizeof (int);
	while (*--ip) {
		if (ip == argspac.argi)
			goto retucomm;
	}
	*(char *)ip = ' ';
	ip++;
	nbad = 0;

  	for (cp = (char *)ip; cp < &argspac.argc[CLBYTES*2-stkoff]; cp++) {
#endif
		c = *cp;
		if (c == 0) {	/* convert null between arguments to space */
			*cp = ' ';
			/* if null argument follows then no more args */
			if (*(cp+1) == 0) break;
		}
		else if (c < ' ' || c > 0176) {
			/* eflg -> 0 XXX */ /* limit number of bad chars to 5 */
			if (++nbad >= 5*(0+1)) {
				*cp++ = '?';
				break;
			}
			*cp = '?';
		}
		else if (0 == 0 && c == '=') {		/* eflg -> 0 XXX */
			while (*--cp != ' ')
				if (cp <= (char *)ip)
					break;
			break;
		}
	}
	*cp = 0;
	while (*--cp == ' ')
		*cp = 0;
	cp = (char *)ip;
	strcpy(cmdbuf, cp);
	if (cp[0] == '-' || cp[0] == '?' || cp[0] <= ' ') {
		(void) strcat(cmdbuf, " (");
		(void) strncat(cmdbuf, p->p_comm, sizeof(p->p_comm));
		(void) strcat(cmdbuf, ")");
	}
	return (cmdbuf);

bad:
	seterr("error locating command name for pid %d from %s",
	    p->p_pid, file);
retucomm:
	(void) strcpy(cmdbuf, " (");
	(void) strncat(cmdbuf, p->p_comm, sizeof (p->p_comm));
	(void) strcat(cmdbuf, ")");
	return (cmdbuf);
}


static
getkvars()
{
	if (kvm_nlist(nl) == -1)
		return (-1);

	/* see if kernel is relocated, possibly at a non-standard value */
	if (nl[X_KERNBASE].n_value)
		kernbase = (long) nl[X_KERNBASE].n_value;

	if (deadkernel) {
		long	addr;

		/*
		 * We must obtain the kernel address translation map
		 * first because klseek uses it.
		 */
#if defined(hp300)
		addr = (long) nl[X_LOWRAM].n_value;
		(void) lseek(kmem, addr, 0);
		if (read(kmem, (char *) &lowram, sizeof (lowram))
		    != sizeof (lowram)) {
			seterr("can't read lowram");
			return (-1);
		}
		lowram = btop(lowram);
		Sysseg = (struct ste *) malloc(NBPG);
		if (Sysseg == NULL) {
			seterr("out of space for Sysseg");
			return (-1);
		}
		addr = (long) nl[X_SYSSEG].n_value;
		(void) lseek(kmem, addr, 0);
		read(kmem, (char *)&addr, sizeof(addr));
		(void) lseek(kmem, (long)addr, 0);
		if (read(kmem, (char *) Sysseg, NBPG) != NBPG) {
			seterr("can't read Sysseg");
			return (-1);
		}
#endif
#if defined(i386)
		PTD = (struct pde *) malloc(NBPG);
		if (PTD == NULL) {
			seterr("out of space for PTD");
			return (-1);
		}
		addr = (long) nl[X_KernelPTD].n_value;
		(void) lseek(kmem, addr, 0);
		read(kmem, (char *)&addr, sizeof(addr));
		(void) lseek(kmem, (long)addr, 0);
		if (read(kmem, (char *) PTD, NBPG) != NBPG) {
			seterr("can't read PTD");
			return (-1);
		}
#endif
	}
	/*
	 * Don't use these yet
	 */
#ifdef digoutofswap
	if (kvm_read((void *) nl[X_NSWAP].n_value, &nswap, sizeof (long)) !=
	    sizeof (long)) {
		seterr("can't read nswap");
		return (-1);
	}
	if (kvm_read((void *) nl[X_DMMIN].n_value, &dmmin, sizeof (long)) !=
	    sizeof (long)) {
		seterr("can't read dmmin");
		return (-1);
	}
	if (kvm_read((void *) nl[X_DMMAX].n_value, &dmmax, sizeof (long)) !=
	    sizeof (long)) {
		seterr("can't read dmmax");
		return (-1);
	}
#endif
	return (0);
}

kvm_read(loc, buf, len)
	void *loc;
	void *buf;
{
	if (kvmfilesopen == 0 && kvm_openfiles(NULL, NULL, NULL) == -1)
		return (-1);
	if (iskva(loc)) {
		klseek(kmem, (off_t) loc, 0);
		if (read(kmem, buf, len) != len) {
			seterr("error reading kmem at %x", loc);
			return (-1);
		}
	} else {
		lseek(mem, (off_t) loc, 0);
		if (read(mem, buf, len) != len) {
			seterr("error reading mem at %x", loc);
			return (-1);
		}
	}
	return (len);
}

static void
klseek(fd, loc, off)
	int fd;
	off_t loc;
	int off;
{

	if (deadkernel) {
		if ((loc = Vtophys(loc)) == -1)
			return;
	}
	(void) lseek(fd, (off_t)loc, off);
}

#ifdef digoutofswap
/*
 * Given a base/size pair in virtual swap area,
 * return a physical base/size pair which is the
 * (largest) initial, physically contiguous block.
 */
static void
vstodb(vsbase, vssize, dmp, dbp, rev)
	register int vsbase;
	int vssize;
	struct dmap *dmp;
	register struct dblock *dbp;
{
	register int blk = dmmin;
	register swblk_t *ip = dmp->dm_map;

	vsbase = ctod(vsbase);
	vssize = ctod(vssize);
	if (vsbase < 0 || vsbase + vssize > dmp->dm_size)
		/*panic("vstodb")*/;
	while (vsbase >= blk) {
		vsbase -= blk;
		if (blk < dmmax)
			blk *= 2;
		ip++;
	}
	if (*ip <= 0 || *ip + blk > nswap)
		/*panic("vstodb")*/;
	dbp->db_size = MIN(vssize, blk - vsbase);
	dbp->db_base = *ip + (rev ? blk - (vsbase + dbp->db_size) : vsbase);
}
#endif

static off_t
Vtophys(loc)
	u_long	loc;
{
	off_t newloc = (off_t) -1;
#ifdef hp300
	int p, ste, pte;

	ste = *(int *)&Sysseg[loc >> SG_ISHIFT];
	if ((ste & SG_V) == 0) {
		seterr("vtophys: segment not valid");
		return((off_t) -1);
	}
	p = btop(loc & SG_PMASK);
	newloc = (ste & SG_FRAME) + (p * sizeof(struct pte));
	(void) lseek(kmem, (long)(newloc-(off_t)ptob(lowram)), 0);
	if (read(kmem, (char *)&pte, sizeof pte) != sizeof pte) {
		seterr("vtophys: cannot locate pte");
		return((off_t) -1);
	}
	newloc = pte & PG_FRAME;
	if (pte == PG_NV || newloc < (off_t)ptob(lowram)) {
		seterr("vtophys: page not valid");
		return((off_t) -1);
	}
	newloc = (newloc - (off_t)ptob(lowram)) + (loc & PGOFSET);
#endif
#ifdef i386
	struct pde pde;
	struct pte pte;
	int p;

	pde = PTD[loc >> PD_SHIFT];
	if (pde.pd_v == 0) {
		seterr("vtophys: page directory entry not valid");
		return((off_t) -1);
	}
	p = btop(loc & PT_MASK);
	newloc = pde.pd_pfnum + (p * sizeof(struct pte));
	(void) lseek(mem, (long)newloc, 0);
	if (read(mem, (char *)&pte, sizeof pte) != sizeof pte) {
		seterr("vtophys: cannot obtain desired pte");
		return((off_t) -1);
	}
	newloc = pte.pg_pfnum;
	if (pte.pg_v == 0) {
		seterr("vtophys: page table entry not valid");
		return((off_t) -1);
	}
	newloc += (loc & PGOFSET);
#endif
	return((off_t) newloc);
}

#include <varargs.h>
static char errbuf[_POSIX2_LINE_MAX];

static void
seterr(va_alist)
	va_dcl
{
	char *fmt;
	va_list ap;

	va_start(ap);
	fmt = va_arg(ap, char *);
	(void) vsnprintf(errbuf, _POSIX2_LINE_MAX, fmt, ap);
	va_end(ap);
}

static void
setsyserr(va_alist)
	va_dcl
{
	char *fmt, *cp;
	va_list ap;
	extern int errno;

	va_start(ap);
	fmt = va_arg(ap, char *);
	(void) vsnprintf(errbuf, _POSIX2_LINE_MAX, fmt, ap);
	for (cp=errbuf; *cp; cp++)
		;
	snprintf(cp, _POSIX2_LINE_MAX - (cp - errbuf), ": %s", strerror(errno));
	va_end(ap);
}

char *
kvm_geterr()
{
	return (errbuf);
}
