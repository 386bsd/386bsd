/*
 * Copyright (c) 1989, 1990, 1991, 1992, 1994 William F. Jolitz, TeleMuse
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
 * $Id: execve.c,v 1.1 94/10/20 00:02:51 bill Exp Locker: bill $
 *
 * This procedure implements a minimal program execution facility for
 * 386BSD. It interfaces to the BSD kernel as the execve() system call.
 */

#include "sys/param.h"
#include "sys/file.h"
#include "sys/mount.h"
#include "sys/exec.h"
#include "sys/stat.h"
#include "sys/wait.h"
#include "sys/mman.h"
#include "sys/errno.h"

#include "malloc.h"
#include "systm.h"
#include "proc.h"
#include "resourcevar.h"
#include "uio.h"
#include "vm.h"
#include "vmspace.h"
#include "vm_pager.h"		/* XXX */

#include "namei.h"
#include "vnode.h"

#undef	KERNEL			/* XXX */
#include "vnode_pager.h"	/* XXX */
#define	KERNEL

#include "machine/reg.h"
#include "machine/cpu.h"

#include "prototypes.h"

#define	copyinoutstr	copyinstr

/*
 * execve() system call.
 */

/* ARGSUSED */
int
execve(p, uap, retval)
	struct proc *p;
	struct args {
		char	*fname;
		char	**argp;
		char	**envp;
	} *uap;
	void *retval;
{
	struct nameidata *ndp;
	struct nameidata nd;
	struct exec hdr;
	char **argbuf, **argbufp, *stringbuf, *stringbufp, **vectp, *ep;
	int needsenv, limitonargs, stringlen, rv, amt, argc, cnt, foff, em = 0;
	vm_offset_t addr, newframe;
	vm_size_t tsize, dsize, bsize;
	struct vattr attr;
	struct vmspace *vs = p->p_vmspace;
	unsigned oss, nss;
	vn_pager_t vnp;
	vm_pager_t pager;


	/*
	 * Step 1. Lookup filename to see if we have something to execute.
	 */
	ndp = &nd;
	ndp->ni_nameiop = LOOKUP | LOCKLEAF | FOLLOW | SAVENAME;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->fname;

	/* is it there? */
	if (rv = namei(ndp, p))
		return (rv);

	/* is this a valid vnode representing a possibly executable file ? */
	rv = EACCES;
	if (ndp->ni_vp->v_type != VREG)
		goto exec_fail;

	/* is the vnode cached already by a previous execve()? */
	pager = (vm_pager_t)ndp->ni_vp->v_vmdata;
	if (pager == NULL)
		vnp = NULL;
	else {
		vnp = (vn_pager_t)pager->pg_data;
		if (vnp->vnp_flags & VNP_EXECVE) {
			/* avoid redundant I/O by using cached info */
			hdr = vnp->vnp_hdr;
			attr = vnp->vnp_attr;
			goto bypass1;
		}
	}

	/* does the filesystem it belongs to allow files to be executed? */
	if ((ndp->ni_vp->v_mount->mnt_flag & MNT_NOEXEC) != 0)
		goto exec_fail;

	/* does it have any attributes? */
	rv = VOP_GETATTR(ndp->ni_vp, &attr, p->p_ucred, p);
	if (rv)
		goto exec_fail;

	/* is it an executable file? */
	if ((attr.va_mode & VANYEXECMASK) == 0)
		goto exec_fail;

	/*
	 * Step 2. Does the file contain a format we can
	 * understand and execute
	 */

	/* read the header at the front of the file */
	rv = vn_rdwr(UIO_READ, ndp->ni_vp, (caddr_t)&hdr, sizeof(hdr),
		0, UIO_SYSSPACE, IO_NODELOCKED, p->p_ucred, &amt, p);

	/* could we obtain a header from the file? */
	if (rv)
		goto exec_fail;
	
	/* ... that we recognize? */
	rv = ENOEXEC;
	if (hdr.a_magic != ZMAGIC) {
		/* if already in emulator, fail */
		/* if (p->p_flag & SINEMULATOR) */
			goto exec_fail;
		/* otherwise attempt use of user-mode loader via emulation mech. */
		/* em = 1;
		goto bypass2; */
	}

	/* sanity check  "ain't not such thing as a sanity clause" -groucho */
	rv = ENOMEM;
	if (hdr.a_text > MAXTSIZ || hdr.a_text % NBPG
	    || hdr.a_text > attr.va_size)
		goto exec_fail;

	if (hdr.a_data == 0 || hdr.a_data > DFLDSIZ
	    || hdr.a_data > attr.va_size
	    || hdr.a_data + hdr.a_text > attr.va_size)
		goto exec_fail;

	if (hdr.a_bss > MAXDSIZ)
		goto exec_fail;
	
	if (hdr.a_text + hdr.a_data + hdr.a_bss > MAXTSIZ + MAXDSIZ)
		goto exec_fail;

	if (hdr.a_entry > hdr.a_text + hdr.a_data)
		goto exec_fail;
	
bypass1:
	rv = ENOMEM;
	if (hdr.a_data + hdr.a_bss > p->p_rlimit[RLIMIT_DATA].rlim_cur)
		goto exec_fail;

	/* don't allow new process contents to exceed memory resources */
	if (chk4space(atop(hdr.a_data + hdr.a_bss + 3*ARG_MAX)) == 0)
		goto exec_fail;

	/*
	 * Step 3.  File and header are valid. Now, dig out the strings
	 * out of the old process image.
	 */

	/*
	 * We implement a single-pass algorithm that builds a new stack
	 * frame within the address space of the "old" process image,
	 * avoiding the second pass entirely. Thus, the new frame is
	 * in position to be run. This consumes much virtual address space,
	 * and two pages more of 'real' memory, such are the costs.
	 * [Also, note the cache wipe that's avoided!]
	 */

	/* can we fit new stack above old one */
	nss = round_page(3*ARG_MAX + NBPG);
	if ((unsigned)vs->vm_maxsaddr + MAXSSIZ + nss < USRSTACK)
		newframe = USRSTACK - nss;
	else
		newframe = ((unsigned)vs->vm_maxsaddr + MAXSSIZ
		    - (vs->vm_ssize*NBPG) - nss);

	/* allocate anonymous memory region for new stack, disable stk limit */
	if (vmspace_allocate(vs, &newframe, nss, FALSE))
		goto exec_fail;
	oss = vs->vm_ssize;
	vs->vm_ssize = 0;


	/* allocate string buffer and arg buffer */
	argbuf = (char **) (newframe + NBPG);
	stringbuf = stringbufp = ((char *)argbuf) + 2*ARG_MAX;
	argbufp = argbuf;

	/* first, do the args */
	vectp = uap->argp;
	needsenv = 1;
	limitonargs = ARG_MAX;
	cnt = 0;

do_env_as_well:
	if (vectp == 0)
		goto dont_bother;

	/* for each argp/envp, copy in string */
	do {
		/* did we outgrow initial argbuf, if so, die */
		if (argbufp == (char **)stringbuf) {
			rv = E2BIG;
			goto exec_dealloc;
		}
	
		/* get an string pointer */
		if (rv = copyin_(p, (void *)vectp++, (void *)&ep, 4))
			goto exec_dealloc; 

		/* if not a null pointer, copy string */
		if (ep) {
			if (rv = copyinoutstr(p, ep, stringbufp,
			    (u_int)limitonargs, (u_int *) &stringlen)) {
				if (rv == ENAMETOOLONG)
					rv = E2BIG;
				goto exec_dealloc;
			}

			(void)copyout_(p, &stringbufp, (void *)argbufp++, 4);
			cnt++;
			stringbufp += stringlen;
			limitonargs -= stringlen;
		} else {
			(void)copyout_(p, &ep, (void *)argbufp++, 4);
			break;
		}
	} while (limitonargs > 0);

dont_bother:
	if (limitonargs <= 0) {
		rv = E2BIG;
		goto exec_dealloc;
	}

	/* have we done the environment yet ? */
	if (needsenv) {
		/* remember the arg count for later */
		argc = cnt;
		vectp = uap->envp;
		needsenv = 0;
		goto do_env_as_well;
	}

	/*
	 * At this point, one could optionally implement a
	 * second pass to condense the strings, arguement vectors,
	 * and stack to fit the fewest pages.
	 *
	 * One might selectively do this when copying was cheaper
	 * than leaving allocated two more pages per process.
	 */

	/*
	 * Step 4. Build the new processes image.
	 *
	 * At this point, we are committed -- destroy old executable!
	 */

	/* stuff arg count on top of "new" stack */
	vs->vm_ssize =  btoc(nss);	/* stack size (pages) XXX */
	vs->vm_maxsaddr = (caddr_t)((unsigned)newframe + nss - MAXSSIZ);
	argbuf--;
	(void)copyout_(p, &argc, (void *)argbuf, 4);

	/* remove anything in address space above our stack */
	if ((unsigned)newframe + nss < VM_MAX_ADDRESS)
		vmspace_delete(vs, (caddr_t)newframe + nss,
			    VM_MAX_ADDRESS - (unsigned)newframe - nss);

	/* blow away all address space, except the stack */
	vmspace_delete(vs, (caddr_t)VM_MIN_ADDRESS, newframe);

	/* build a new address space */
	addr = 0;

	/* screwball mode -- special case of 413 to save space for floppy */
	if (hdr.a_text == 0)
		foff = tsize = 0;
	else {
		tsize = roundup(hdr.a_text, NBPG);
		foff = NBPG;
	}

	/* treat text and data in terms of integral page size */
	dsize = roundup(hdr.a_data, NBPG);
	bsize = roundup(hdr.a_bss + dsize, NBPG);
	bsize -= dsize;

	/* map text & data in file, as being "paged in" on demand */
	rv = vmspace_mmap(vs, &addr, tsize+dsize, VM_PROT_ALL,
	    MAP_FILE|MAP_COPY, (caddr_t)ndp->ni_vp, foff);
	if (rv)
		goto exec_abort;

	/* mark pages r/w data, r/o text */
	if (tsize) {
		addr = 0;
		rv = vmspace_protect(vs, (caddr_t)addr, tsize, FALSE,
		    VM_PROT_READ|VM_PROT_EXECUTE);
		if (rv)
			goto exec_abort;
	}

	/* create anonymous memory region for bss */
	addr = dsize + tsize;
	if (rv = vmspace_allocate(vs, &addr, bsize, FALSE))
		goto exec_abort;

	/*
	 * Step 5. Prepare process for execution.
	 */

	/* touchup process information -- vm system is unfinished! */
	vs->vm_tsize = tsize/NBPG;		/* text size (pages) XXX */
	vs->vm_dsize = (dsize+bsize)/NBPG;	/* data size (pages) XXX */
	vs->vm_taddr = 0;		/* user virtual address of text XXX */
	vs->vm_daddr = (caddr_t)tsize;	/* user virtual address of data XXX */

	/* close files on exec, fixup signals, name this process */
	fdcloseexec(p);
	execsigs(p);
	nameiexec(ndp, p);

	/*
	 * mark as having had an execve(), wakeup any process that was
	 * vforked and tell it that it now has it's own resources back. XXX
	 */
	p->p_flag |= SEXEC;
bypass2:
	if (p->p_pptr && (p->p_flag & SPPWAIT)) {
		p->p_flag &= ~SPPWAIT;
		wakeup((caddr_t)p->p_pptr);
	}
	
	/* implement set userid/groupid */
	if ((attr.va_mode&VSUID) && (p->p_flag & STRC) == 0) {
		p->p_cred = modpcred(p);
		p->p_cred->p_svuid = p->p_ucred->cr_uid = attr.va_uid;
	}
	if ((attr.va_mode&VSGID) && (p->p_flag & STRC) == 0) {
		p->p_cred = modpcred(p);
		p->p_cred->p_svgid = p->p_ucred->cr_groups[0] =
		    attr.va_gid;
	}
	/* if (em)
		return(EEMULATE); */

	/* setup initial register state */
	cpu_execsetregs(p, (caddr_t)hdr.a_entry, (caddr_t) (argbuf));



	/* if tracing process, pass control back to debugger so
	   breakpoints can be set before the program "runs" */
	if (p->p_flag & STRC)
		psignal(p, SIGTRAP);

	/* cache information in vm system. */
	if (vnp == NULL) {
		pager = (vm_pager_t)ndp->ni_vp->v_vmdata;
		vnp = (vn_pager_t)pager->pg_data;
		vnp->vnp_hdr = hdr;
		vnp->vnp_attr = attr;
		vnp->vnp_flags |= VNP_EXECVE;
	}

	return (0);

exec_dealloc:
	/* remove interim "new" stack frame we were building */
	vmspace_delete(vs, (caddr_t)newframe, nss);

	/* reengauge stack limits */
	vs->vm_ssize =  oss;
	
exec_fail:
	/* release namei request */
	nameiexec(ndp, (struct proc *)0);

	return(rv);

exec_abort:
	/* sorry, no more process anymore. exit gracefully */

	/* release namei request */
	nameiexec(ndp, (struct proc *)0);

	exit(p, W_EXITCODE(0, SIGABRT));

	/* NOTREACHED */
	return(0);
}
