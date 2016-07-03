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
 * $Id: ptrace.c,v 1.1 94/10/20 00:03:11 bill Exp $
 *
 * Un*x ptrace(2) debugging mechanism, holdover from release 0.1.
 */

#define IPCREG
#include "sys/param.h"
#include "sys/user.h"
#include "sys/file.h"
#include "sys/ptrace.h"
#include "sys/mman.h"
#include "resourcevar.h"
#include "vm.h"
#include "vm_page.h"

#include "machine/reg.h"
#include "machine/psl.h"
#include "machine/cpu.h"

#include "prototypes.h"

/*
 * NOTES.
 *
 * The following ptrace calls have been defined in addition to
 * the standard ones found in original <sys/ptrace.h>:
 *
 * PT_ATTACH		-	attach to running process
 * PT_DETACH		-	detach from running process
 * PT_SYSCALL		-	trace system calls
 * PT_GETREG		-	get register file
 * PT_SETREG		-	set register file
 * PT_BREAD_[IDU]	-	block read from process (not yet implemented)
 * PT_BWRITE_[IDU]	-	block write	"		"
 * PT_INHERIT		-	make forked processes inherit trace flags
 *
 */

/* Define to prevent extraneous clutter in source */
#ifndef SSTRC
#define	SSTRC	0
#endif
#ifndef SFTRC
#define	SFTRC	0
#endif

/*
 * `ipcreg' defined in "machine/reg.h"
 * Should we define a structure with all regs?
 */
extern int sipcreg[], ipcreg[];

struct {
	int	flag;
#define IPC_BUSY	1
#define IPC_WANT	2
#define IPC_DONE	4
	int	req;			/* copy of ptrace request */
	int	*addr;			/* copy of ptrace address */
	int	data;			/* copy of ptrace data */
	int	error;			/* errno from `procxmt' */
	int	regs[NIPCREG];		/* PT_[GS]ETREG */
	caddr_t	buf;			/* PT_BREAD/WRITE */
	int	buflen;			/* 	"	*/
} ipc;

/*
 * Process debugging system call.
 */
ptrace(curp, uap, retval)
	struct proc *curp;
	register struct args {
		int	req;
		int	pid;
		int	*addr;
		int	data;
	} *uap;
	int *retval;
{
	struct proc *p;
	int s, error = 0;

	*retval = 0;
	if (uap->req == PT_TRACE_ME) {
		curp->p_flag |= STRC;
		/*p->p_tptr = p->p_pptr; * What shall we do here ? */
		return 0;
	}
	if ((p = pfind(uap->pid)) == NULL) {
		return ESRCH;
	}

#ifdef notyet
	if (uap->req != PT_ATTACH && (
			(p->p_flag & STRC) == 0 ||
			(p->p_tptr && curp != p->p_tptr) ||
			(!p->p_tptr && curp != p->p_pptr)))

		return ESRCH;
#endif


#ifdef PT_ATTACH
	switch (uap->req) {
	case PT_ATTACH:
		if (curp->p_ucred->cr_uid != 0 && (
			curp->p_ucred->cr_uid != p->p_ucred->cr_uid ||
			curp->p_ucred->cr_uid != p->p_cred->p_svuid))
			return EACCES;

		p->p_tptr = curp;
		p->p_flag |= STRC;
		psignal(p, SIGTRAP);
		return 0;

	case PT_DETACH:
		if ((unsigned)uap->data >= NSIG)
			return EINVAL;
		p->p_flag &= ~(STRC|SSTRC|SFTRC);
		p->p_tptr = NULL;
		psignal(p->p_pptr, SIGCHLD);
		wakeup((caddr_t)p->p_pptr);
		s = splhigh();
		if (p->p_stat == SSTOP) {
			p->p_sigacts->ps_stopsig = uap->data;
			setrun(p);
		} else if (uap->data) {
			psignal(p, uap->data);
		}
		splx(s);
		return 0;

#ifdef PT_INHERIT
	case PT_INHERIT:
		if ((p->p_flag & STRC) == 0)
			return ESRCH;
		p->p_flag |= SFTRC;
		return 0;
#endif

	default:
		break;
	}
#endif

	/* Other ptrace calls require target process to be in stopped state */
	if ((p->p_flag & STRC) == 0 || p->p_stat != SSTOP) {
		return ESRCH;
	}

	/* Acquire the ipc structure */
	while (ipc.flag & IPC_BUSY) {
		ipc.flag |= IPC_WANT;
		error = tsleep((caddr_t)&ipc, PWAIT|PCATCH, "ipc", 0);
		if (error)
			goto out;
	}

	/* Got it, fill it */
	ipc.flag = IPC_BUSY;
	ipc.error = 0;
	ipc.req = uap->req;
	ipc.addr = uap->addr;
	ipc.data = uap->data;

#ifdef PT_GETREGS
	switch (uap->req) {
	case PT_SETREGS:
		error = copyin(curp, (char *)ipc.addr, (char *)ipc.regs, sizeof(ipc.regs));
		if (error)
			goto out;
		break;

#ifdef notyet	/* requires change in number of args to ptrace syscall */
	case PT_BWRITE_I:
	case PT_BWRITE_D:
		ipc.buflen = uap->data;
		ipc.buf = kmem_alloc(kernel_map, uap->data, 0);
		error = copyin(curp, (char *)ipc.addr, (char *)ipc.buf, ipc.buflen);
		if (error) {
			kmem_free(kernelmap, ipc.buf, ipc.buflen);
			goto out;
		}
#endif
	default:
		break;
	}
#endif

	s = splclock();
	setrun(p);
	splx(s);
	while ((ipc.flag & IPC_DONE) == 0) {
		error = tsleep((caddr_t)&ipc, PWAIT|PCATCH, "ipc", 0);
		if (error)
			goto out;
	}

	*retval = ipc.data;
	if (error = ipc.error)
		goto out;

#ifdef PT_GETREGS
	switch (uap->req) {
	case PT_GETREGS:
		error = copyout(curp, (char *)ipc.regs, (char *)ipc.addr, sizeof(ipc.regs));
		break;

	case PT_BREAD_I:
	case PT_BREAD_D:
		/* Not yet */
	default:
		break;
	}
#endif

out:
	/* Release ipc structure */
	ipc.flag &= ~IPC_BUSY;
	if (ipc.flag & IPC_WANT) {
		ipc.flag &= ~IPC_WANT;
		wakeup((caddr_t)&ipc);
	}
	return error;
}

procxmt(p)
	register struct proc *p;
{
	int i, rv = 0;

	/* Are we still being traced? */
	if ((p->p_flag & STRC) == 0)
		return 1;

	p->p_addr->u_kproc.kp_proc = *p;
	fill_eproc(p, &p->p_addr->u_kproc.kp_eproc);

	switch (ipc.req) {
	case PT_READ_I:
	case PT_READ_D:
		ipc.error = copyin(p, (char *)ipc.addr, (char *)&ipc.data, sizeof(ipc.data));
		break;

	case PT_READ_U:
		if ((u_int)ipc.addr > UPAGES * NBPG - sizeof(int)) {
			ipc.error = EFAULT;
			break;
		}
		ipc.data = *(int *)((u_int)p->p_addr + (u_int)ipc.addr);
		break;

	case PT_WRITE_I:
	/*case PT_WRITE_D:*/
		/*printf("prot %d ", vmspace_protect(p->p_vmspace, (vm_offset_t)ipc.addr,
			sizeof(int), FALSE, VM_PROT_WRITE));*/
		/* paul kranenberg's patch */
        case PT_WRITE_D: {
                vm_prot_t prot;         /* current protection of region */
                int cow;                /* ensure copy-on-write happens */
  
                if (cow = (vmspace_access(p->p_vmspace, (caddr_t)ipc.addr, sizeof(ipc.data), PROT_WRITE) == 0)) {
                        vm_offset_t     addr = (vm_offset_t)ipc.addr;
                        vm_size_t       size;
                        vm_prot_t       max_prot;
                        vm_inherit_t    inh;
                        boolean_t       shared;
                        vm_object_t     object;
                        vm_offset_t     objoff;
  
                        if (vm_region(&p->p_vmspace->vm_map, &addr, &size,
                                        &prot, &max_prot, &inh, &shared,
                                        &object, &objoff) != KERN_SUCCESS ||
                            vmspace_protect(p->p_vmspace, (caddr_t)ipc.addr,
                                        sizeof(ipc.data), FALSE,
                                        prot|VM_PROT_WRITE) != KERN_SUCCESS ||
                            vm_fault(&p->p_vmspace->vm_map,trunc_page(ipc.addr),
                                        VM_PROT_WRITE, FALSE) != KERN_SUCCESS) {
  
                                ipc.error = EFAULT;
                                break;
                        }
                }
                ipc.error = copyout(p, (char *)&ipc.data,
                                        (char *)ipc.addr, sizeof(ipc.data));
                if (cow)
                        if (vmspace_protect(p->p_vmspace, (caddr_t)ipc.addr,
                                        sizeof(ipc.data), FALSE,
                                        prot) != KERN_SUCCESS)
                                printf("ptrace: oops\n");
                break;
        }
		/*ipc.error = copyout(p, (char *)&ipc.data, (char *)ipc.addr, sizeof(ipc.data));
		break;*/

	case PT_WRITE_U:
		{
		int flags = p->p_addr->u_pcb.pcb_flags;
		int *uadr = (int *)((u_int)p->p_addr + (u_int)ipc.addr);

		if ((u_int)ipc.addr > UPAGES * NBPG - sizeof(int)) {
			ipc.error = EFAULT;
			break;
		}
		
		for (i = 0; i < NIPCREG; i++)
			if (uadr == cpu_ptracereg(p, i)) {
				*uadr = ipc.data;
#ifdef i386
				if (i == 10)
					*uadr |= SEL_UPL;
				if (i == 8)
					(unsigned)*uadr %= VM_MAX_ADDRESS;
#endif
				break;
			};

		if (i >= NIPCREG) {
			ipc.error = EFAULT;
			break;
		}
		}
		break;

	case PT_CONTINUE:
		if (ipc.addr != (int *)1) {
#ifdef i386
			p->p_md.md_regs[(curproc->p_addr->u_pcb.pcb_flags&FM_TRAP)?tEIP:sEIP] = (int)ipc.addr;
#endif
		}
		p->p_flag &= ~SSTRC;	/* Only set by PT_SYSCALL */
		if ((unsigned)ipc.data >= NSIG) {
			ipc.error = EINVAL;
		} else {
			p->p_sigacts->ps_stopsig = ipc.data;
			rv = 1;
		}
		break;

	case PT_KILL:
		p->p_flag &= ~SSTRC;	/* Only set by PT_SYSCALL */
		rv = 2;
		break;

	case PT_STEP:
#ifdef i386
		if (ipc.addr != (int *)1) {
			p->p_md.md_regs[(curproc->p_addr->u_pcb.pcb_flags&FM_TRAP)?tEIP:sEIP] = (int)ipc.addr;
		}
		p->p_md.md_regs[(curproc->p_addr->u_pcb.pcb_flags&FM_TRAP)?tEFLAGS:sEFLAGS] |= PSL_T;
#endif
		p->p_flag &= ~SSTRC;	/* Only set by PT_SYSCALL */
		p->p_sigacts->ps_stopsig = 0;
		rv = 1;
		break;

#ifdef PT_SYSCALL
	case PT_SYSCALL:
		if (ipc.addr != (int *)1) {
#ifdef i386
			p->p_md.md_regs[(curproc->p_addr->u_pcb.pcb_flags&FM_TRAP)?tEIP:sEIP] = (int)ipc.addr;
#endif
		}
		p->p_flag |= SSTRC;
		p->p_sigacts->ps_stopsig = 0;
		rv = 1;
		break;
#endif
#ifdef PT_GETREGS
	case PT_GETREGS:
		for (i = 0; i < NIPCREG; i++)
			ipc.regs[i] = *cpu_ptracereg(p, i);
		break;

	case PT_SETREGS:
		for (i = 0; i < NIPCREG; i++)
			*cpu_ptracereg(p, i) = ipc.regs[i];
		break;
#endif

#ifdef PT_DUMP
	case PT_DUMP:
		/* Should be able to specify core file name */
		ipc.error = coredump(p);
		break;
#endif

	default:
		ipc.error = EINVAL;
	}
	ipc.flag |= IPC_DONE;
	wakeup((caddr_t)&ipc);

	if (rv == 2)
		exit(p, 0); 	/*???*/

	return rv;
}
