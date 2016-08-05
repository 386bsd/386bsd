/*
 * Copyright (c) 1989, 1990, 1991, 1992 William F. Jolitz, TeleMuse
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
 *
 * THIS SOFTWARE IS A COMPONENT OF 386BSD DEVELOPED BY WILLIAM F. JOLITZ 
 * AND IS INTENDED FOR RESEARCH AND EDUCATIONAL PURPOSES ONLY. THIS 
 * SOFTWARE SHOULD NOT BE CONSIDERED TO BE A COMMERCIAL PRODUCT. 
 * THE DEVELOPER URGES THAT USERS WHO REQUIRE A COMMERCIAL PRODUCT 
 * NOT MAKE USE OF THIS WORK.
 *
 * FOR USERS WHO WISH TO UNDERSTAND THE 386BSD SYSTEM DEVELOPED
 * BY WILLIAM F. JOLITZ, WE RECOMMEND THE USER STUDY WRITTEN 
 * REFERENCES SUCH AS THE  "PORTING UNIX TO THE 386" SERIES 
 * (BEGINNING JANUARY 1991 "DR. DOBBS JOURNAL", USA AND BEGINNING 
 * JUNE 1991 "UNIX MAGAZIN", GERMANY) BY WILLIAM F. JOLITZ AND 
 * LYNNE GREER JOLITZ, AS WELL AS OTHER BOOKS ON UNIX AND THE 
 * ON-LINE 386BSD USER MANUAL BEFORE USE. A BOOK DISCUSSING THE INTERNALS 
 * OF 386BSD ENTITLED "386BSD FROM THE INSIDE OUT" WILL BE AVAILABLE LATE 1992.
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
 * This procedure implements a minimal program execution facility for
 * 386BSD. It interfaces to the BSD kernel as the execve system call.
 * Significant limitations and lack of compatiblity with POSIX are
 * present with this version, to make its basic operation more clear.
 *
 */

#include "param.h"
#include "systm.h"
#include "signalvar.h"
#include "resourcevar.h"
#include "proc.h"
#include "mount.h"
#include "namei.h"
#include "vnode.h"
#include "file.h"
#include "exec.h"
#include "stat.h"
#include "wait.h"
#include "mman.h"
#include "malloc.h"

#include "vm/vm.h"
#include "vm/vm_param.h"
#include "vm/vm_map.h"
#include "vm/vm_kern.h"

#include "machine/reg.h"

extern int dostacklimits;
#define	copyinoutstr	copyinstr

/*
 * execve() system call.
 */

/* ARGSUSED */
execve(p, uap, retval)
	struct proc *p;
	register struct args {
		char	*fname;
		char	**argp;
		char	**envp;
	} *uap;
	int *retval;
{
	register struct nameidata *ndp;
	struct nameidata nd;
	struct exec hdr;
	char **argbuf, **argbufp, *stringbuf, *stringbufp;
	char **vectp, *ep;
	int needsenv, limitonargs, stringlen, addr, size, len,
		rv, amt, argc, tsize, dsize, bsize, cnt, foff;
	struct vattr attr;
	struct vmspace *vs;
	caddr_t newframe;

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

	/* does it have any attributes? */
	rv = VOP_GETATTR(ndp->ni_vp, &attr, p->p_ucred, p);
	if (rv)
		goto exec_fail;

	/* is it executable, and a regular file? */
	if ((attr.va_mode & VEXEC) == 0 || attr.va_type != VREG) {
		rv = EACCES;
		goto exec_fail;
	}

	/*
	 * Step 2. Does the file contain a format we can
	 * understand and execute
	 */
	rv = vn_rdwr(UIO_READ, ndp->ni_vp, (caddr_t)&hdr, sizeof(hdr),
		0, UIO_SYSSPACE, IO_NODELOCKED, p->p_ucred, &amt, p);

	/* big enough to hold a header? */
	if (rv)
		goto exec_fail;
	
	/* ... that we recognize? */
	rv = ENOEXEC;
	if (hdr.a_magic != ZMAGIC)
		goto exec_fail;

	/* sanity check  "ain't not such thing as a sanity clause" -groucho */
	rv = ENOMEM;
	if (/*hdr.a_text == 0 || */ hdr.a_text > MAXTSIZ
		|| hdr.a_text % NBPG || hdr.a_text > attr.va_size)
		goto exec_fail;

	if (hdr.a_data == 0 || hdr.a_data > DFLDSIZ
		|| hdr.a_data > attr.va_size
		|| hdr.a_data + hdr.a_text > attr.va_size)
		goto exec_fail;

	if (hdr.a_bss > MAXDSIZ)
		goto exec_fail;
	
	if (hdr.a_text + hdr.a_data + hdr.a_bss > MAXTSIZ + MAXDSIZ)
		goto exec_fail;

	if (hdr.a_data + hdr.a_bss > p->p_rlimit[RLIMIT_DATA].rlim_cur)
		goto exec_fail;

	if (hdr.a_entry > hdr.a_text + hdr.a_data)
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

	/* create anonymous memory region for new stack */
	vs = p->p_vmspace;
	if ((unsigned)vs->vm_maxsaddr + MAXSSIZ < USRSTACK)
		newframe = (caddr_t) USRSTACK - MAXSSIZ;
	else
		vs->vm_maxsaddr = newframe = (caddr_t) USRSTACK - 2*MAXSSIZ;

	/* don't do stack limit checking on traps temporarily XXX*/
	dostacklimits = 0;

	rv = vm_allocate(&vs->vm_map, &newframe, MAXSSIZ, FALSE);
	if (rv) goto exec_fail;

	/* allocate string buffer and arg buffer */
	argbuf = (char **) (newframe + MAXSSIZ - 3*ARG_MAX);
	stringbuf = stringbufp = ((char *)argbuf) + 2*ARG_MAX;
	argbufp = argbuf;

	/* first, do args */
	vectp = uap->argp;
	needsenv = 1;
	limitonargs = ARG_MAX;
	cnt = 0;

do_env_as_well:
	if(vectp == 0) goto dont_bother;

	/* for each envp, copy in string */
	do {
		/* did we outgrow initial argbuf, if so, die */
		if (argbufp == (char **)stringbuf) {
			rv = E2BIG;
			goto exec_dealloc;
		}
	
		/* get an string pointer */
		ep = (char *)fuword(vectp++);
		if (ep == (char *)-1) {
			rv = EFAULT;
			goto exec_dealloc;
		}

		/* if not a null pointer, copy string */
		if (ep) {
			if (rv = copyinoutstr(ep, stringbufp,
				(u_int)limitonargs, (u_int *) &stringlen)) {
				if (rv == ENAMETOOLONG)
					rv = E2BIG;
				goto exec_dealloc;
			}
			suword(argbufp++, (int)stringbufp);
			cnt++;
			stringbufp += stringlen;
			limitonargs -= stringlen;
		} else {
			suword(argbufp++, 0);
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
 
	/* At this point, one could optionally implement a
	 * second pass to condense the strings, arguement vectors,
	 * and stack to fit the fewest pages.
	 *
	 * One might selectively do this when copying was cheaper
	 * than leaving allocated two more pages per process.
	 */

	/* stuff arg count on top of "new" stack */
	argbuf[-1] = (char *)argc;

	/*
	 * Step 4. Build the new processes image.
	 *
	 * At this point, we are committed -- destroy old executable!
	 */

	/* blow away all address space, except the stack */
	rv = vm_deallocate(&vs->vm_map, 0, USRSTACK - 2*MAXSSIZ);
	if (rv)
		goto exec_abort;

	/* destroy "old" stack */
	if ((unsigned)newframe < USRSTACK - MAXSSIZ) {
		rv = vm_deallocate(&vs->vm_map, USRSTACK - MAXSSIZ, MAXSSIZ);
		if (rv)
			goto exec_abort;
	} else {
		rv = vm_deallocate(&vs->vm_map, USRSTACK - 2*MAXSSIZ, MAXSSIZ);
		if (rv)
			goto exec_abort;
	}

	/* build a new address space */
	addr = 0;

	/* screwball mode -- special case of 413 to save space for floppy */
	if (hdr.a_text == 0) {
		foff = tsize = 0;
		hdr.a_data += hdr.a_text;
	} else {
		tsize = roundup(hdr.a_text, NBPG);
		foff = NBPG;
	}

	/* treat text and data in terms of integral page size */
	dsize = roundup(hdr.a_data, NBPG);
	bsize = roundup(hdr.a_bss + dsize, NBPG);
	bsize -= dsize;

	/* map text & data in file, as being "paged in" on demand */
	rv = vm_mmap(&vs->vm_map, &addr, tsize+dsize, VM_PROT_ALL,
		MAP_FILE|MAP_COPY|MAP_FIXED, (caddr_t)ndp->ni_vp, foff);
	if (rv)
		goto exec_abort;

	/* mark pages r/w data, r/o text */
	if (tsize) {
		addr = 0;
		rv = vm_protect(&vs->vm_map, addr, tsize, FALSE,
			VM_PROT_READ|VM_PROT_EXECUTE);
		if (rv)
			goto exec_abort;
	}

	/* create anonymous memory region for bss */
	addr = dsize + tsize;
	rv = vm_allocate(&vs->vm_map, &addr, bsize, FALSE);
	if (rv)
		goto exec_abort;

	/*
	 * Step 5. Prepare process for execution.
	 */

	/* touchup process information -- vm system is unfinished! */
	vs->vm_tsize = tsize/NBPG;		/* text size (pages) XXX */
	vs->vm_dsize = (dsize+bsize)/NBPG;	/* data size (pages) XXX */
	vs->vm_taddr = 0;		/* user virtual address of text XXX */
	vs->vm_daddr = (caddr_t)tsize;	/* user virtual address of data XXX */
	vs->vm_maxsaddr = newframe;	/* user VA at max stack growth XXX */
	vs->vm_ssize =  ((unsigned)vs->vm_maxsaddr + MAXSSIZ
		- (unsigned)argbuf)/ NBPG + 1; /* stack size (pages) */
	dostacklimits = 1;	/* allow stack limits to be enforced XXX */

	/* close files on exec, fixup signals */
	fdcloseexec(p);
	execsigs(p);

	/* name this process - nameiexec(p, ndp) */
	len = MIN(ndp->ni_namelen,MAXCOMLEN);
	bcopy(ndp->ni_ptr, p->p_comm, len);
	p->p_comm[len] = 0;
	
	/* mark as executable, wakeup any process that was vforked and tell
	 * it that it now has it's own resources back */
	p->p_flag |= SEXEC;
	if (p->p_pptr && (p->p_flag & SPPWAIT)) {
	    p->p_flag &= ~SPPWAIT;
	    wakeup(p->p_pptr);
	}
	
	/* implement set userid/groupid */
	if ((attr.va_mode&VSUID) && (p->p_flag & STRC) == 0) {
	    p->p_ucred = crcopy(p->p_ucred);
	    p->p_cred->p_svuid = p->p_ucred->cr_uid = attr.va_uid;
	}
	if ((attr.va_mode&VSGID) && (p->p_flag & STRC) == 0) {
	    p->p_ucred = crcopy(p->p_ucred);
	    p->p_cred->p_svgid = p->p_ucred->cr_groups[0] = attr.va_gid;
	}

	/* setup initial register state */
	p->p_regs[SP] = (unsigned) (argbuf - 1);
	setregs(p, hdr.a_entry);

	vput(ndp->ni_vp);
	FREE(ndp->ni_pnbuf, M_NAMEI);

	/* if tracing process, pass control back to debugger so breakpoints
	   can be set before the program "runs" */
	if (p->p_flag & STRC)
		psignal(p, SIGTRAP);

	return (0);

exec_dealloc:
	/* remove interim "new" stack frame we were building */
	vm_deallocate(&vs->vm_map, newframe, MAXSSIZ);

exec_fail:
	dostacklimits = 1;
	vput(ndp->ni_vp);
	FREE(ndp->ni_pnbuf, M_NAMEI);

	return(rv);

exec_abort:
	/* sorry, no more process anymore. exit gracefully */
	vm_deallocate(&vs->vm_map, newframe, MAXSSIZ);
	vput(ndp->ni_vp);
	FREE(ndp->ni_pnbuf, M_NAMEI);
	exit(p, W_EXITCODE(0, SIGABRT));

	/* NOTREACHED */
	return(0);
}
