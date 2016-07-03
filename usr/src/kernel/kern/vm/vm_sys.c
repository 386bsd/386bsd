/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * $Id: vm_sys.c,v 1.1 94/10/19 17:37:27 bill Exp $
 */

/*
 * System calls of the virtual memory system.
 */

#include "sys/param.h"
#include "sys/mman.h"
#include "privilege.h"
#include "uio.h"
#include "sys/errno.h"
#include "systm.h"
#include "filedesc.h"
#include "proc.h"
#include "namei.h"	/* specdev.h */
#include "specdev.h"
/*#include "conf.h"*/
#ifdef	KTRACE
#include "sys/ktrace.h"
#endif
#include "resourcevar.h"
#include "vm.h"
#include "vmspace.h"

#include "namei.h"
#include "vnode.h"

#include "prototypes.h"

#ifdef DEBUG
int mmapdebug = 0;
#define MDB_FOLLOW	0x01
#define MDB_SYNC	0x02
#define MDB_MAPIT	0x04
#endif

/* ARGSUSED */
getpagesize(p, uap, retval)
	struct proc *p;
	void *uap;
	int *retval;
{

	*retval = NBPG * CLSIZE;
	return (0);
}

/* ARGSUSED */
sstk(p, uap, retval)
	struct proc *p;
	struct args {
		int	incr;
	} *uap;
	int *retval;
{

	/* Not yet implemented */
	return (EOPNOTSUPP);
}

smmap(p, uap, retval)
	struct proc *p;
	register struct args {
		caddr_t	addr;
		int	len;
		int	prot;
		int	flags;
		int	fd;
		off_t	pos;
	} *uap;
	int *retval;
{
	register struct filedesc *fdp = p->p_fd;
	register struct file *fp;
	struct vnode *vp;
	vm_offset_t addr;
	vm_size_t size;
	vm_prot_t prot;
	caddr_t handle;
	int mtype, error;

#ifdef DEBUG
	if (mmapdebug & MDB_FOLLOW)
		printf("mmap(%d): addr %x len %x pro %x flg %x fd %d pos %x\n",
		       p->p_pid, uap->addr, uap->len, uap->prot,
		       uap->flags, uap->fd, uap->pos);
#endif
	/*
	 * Make sure one of the sharing types is specified
	 */
	mtype = uap->flags & MAP_TYPE;
	switch (mtype) {
	case MAP_FILE:
	case MAP_ANON:
		break;
	default:
		return(EINVAL);
	}
	/*
	 * Address (if FIXED) must be page aligned.
	 * Size is implicitly rounded to a page boundary.
	 */
	addr = (vm_offset_t) uap->addr;
	if ((uap->flags & MAP_FIXED) && (addr & page_mask) || uap->len < 0)
		return(EINVAL);
	if ((uap->flags & MAP_FIXED) && (addr + size > VM_MAX_ADDRESS))
		return(EINVAL);
	size = (vm_size_t) round_page(uap->len);
	/*
	 * XXX if no hint provided for a non-fixed mapping place it after
	 * the end of the largest possible heap.
	 *
	 * There should really be a pmap call to determine a reasonable
	 * location.
	 */
	if (addr == 0 && (uap->flags & MAP_FIXED) == 0)
		addr = round_page(p->p_vmspace->vm_daddr + MAXDSIZ);
	/*
	 * Mapping file or named anonymous, get fp for validation
	 */
	if (mtype == MAP_FILE || uap->fd != -1) {
		if (((unsigned)uap->fd) >= fdp->fd_nfiles ||
		    (fp = fdp->fd_ofiles[uap->fd]) == NULL)
			return(EBADF);
	}
	/*
	 * If we are mapping a file we need to check various
	 * file/vnode related things.
	 */
	if (mtype == MAP_FILE) {
		/*
		 * Obtain vnode and make sure it is of appropriate type
		 */
		if (fp->f_type != DTYPE_VNODE)
			return(EINVAL);
		vp = (struct vnode *)fp->f_data;
		if (vp->v_type != VREG && vp->v_type != VCHR)
			return(EINVAL);
		/*
		 * Ensure that file protection and desired protection
		 * are compatible.  Note that we only worry about writability
		 * if mapping is shared.
		 */
		if ((uap->prot & PROT_READ) && (fp->f_flag & FREAD) == 0 ||
		    ((uap->flags & MAP_SHARED) &&
		     (uap->prot & PROT_WRITE) && (fp->f_flag & FWRITE) == 0))
			return(EACCES);
		/* XXX restrict r/o file shared by converting into private */
		if ((uap->flags & MAP_SHARED) && (fp->f_flag & FWRITE) == 0)
			uap->flags = (uap->flags & ~MAP_SHARED) | MAP_PRIVATE;
		handle = (caddr_t)vp;
	} else if (uap->fd != -1)
		handle = (caddr_t)fp;
	else
		handle = NULL;
	/*
	 * Map protections to MACH style
	 */
	prot = VM_PROT_NONE;
	if (uap->prot & PROT_READ)
		prot |= VM_PROT_READ;
	if (uap->prot & PROT_WRITE)
		prot |= VM_PROT_WRITE;
	if (uap->prot & PROT_EXEC)
		prot |= VM_PROT_EXECUTE;

	error = vmspace_mmap(p->p_vmspace, &addr, size, prot,
			uap->flags, handle, (vm_offset_t)uap->pos);
	if (error == 0)
		*retval = (int) addr;
	return(error);
}

msync(p, uap, retval)
	struct proc *p;
	struct args {
		caddr_t	addr;
		int	len;
	} *uap;
	int *retval;
{
	vm_offset_t addr, objoff, oaddr;
	vm_size_t size, osize;
	vm_prot_t prot, mprot;
	vm_inherit_t inherit;
	vm_object_t object;
	boolean_t shared;
	int rv;

#ifdef DEBUG
	if (mmapdebug & (MDB_FOLLOW|MDB_SYNC))
		printf("msync(%d): addr %x len %x\n",
		       p->p_pid, uap->addr, uap->len);
#endif
	if (((int)uap->addr & page_mask) || uap->len < 0)
		return(EINVAL);
	addr = oaddr = (vm_offset_t)uap->addr;
	osize = (vm_size_t)uap->len;
	/*
	 * Region must be entirely contained in a single entry
	 */
	if (!vm_map_is_allocated(&p->p_vmspace->vm_map, addr, addr+osize,
	    TRUE))
		return(EINVAL);
	/*
	 * Determine the object associated with that entry
	 * (object is returned locked on KERN_SUCCESS)
	 */
	rv = vm_region(&p->p_vmspace->vm_map, &addr, &size, &prot, &mprot,
		       &inherit, &shared, &object, &objoff);
	if (rv != KERN_SUCCESS)
		return(EINVAL);
#ifdef DEBUG
	if (mmapdebug & MDB_SYNC)
		printf("msync: region: object %x addr %x size %d objoff %d\n",
		       object, addr, size, objoff);
#endif
	/*
	 * Do not msync non-vnoded backed objects.
	 */
	if (object->internal || object->pager == NULL ||
	    object->pager->pg_type != PG_VNODE) {
		return(EINVAL);
	}
	objoff += oaddr - addr;
	if (osize == 0)
		osize = size;
#ifdef DEBUG
	if (mmapdebug & MDB_SYNC)
		printf("msync: cleaning/flushing object range [%x-%x)\n",
		       objoff, objoff+osize);
#endif
	if (prot & VM_PROT_WRITE)
		vm_object_page_clean(object, objoff, objoff+osize);
	/*
	 * (XXX)
	 * Bummer, gotta flush all cached pages to ensure
	 * consistency with the file system cache.
	 */
	vm_object_page_remove(object, objoff, objoff+osize);
	return(0);
}

munmap(p, uap, retval)
	register struct proc *p;
	register struct args {
		caddr_t	addr;
		int	len;
	} *uap;
	int *retval;
{
	vm_offset_t addr;
	vm_size_t size;

#ifdef DEBUG
	if (mmapdebug & MDB_FOLLOW)
		printf("munmap(%d): addr %x len %x\n",
		       p->p_pid, uap->addr, uap->len);
#endif

	addr = (vm_offset_t) uap->addr;
	if ((addr & page_mask) || uap->len < 0)
		return(EINVAL);
	size = (vm_size_t) round_page(uap->len);
	if (size == 0)
		return(0);
	if (!vm_map_is_allocated(&p->p_vmspace->vm_map, addr, addr+size,
	    FALSE))
		return(EINVAL);
	vmspace_delete(p->p_vmspace, (caddr_t)addr, size);
	return(0);
}

mprotect(p, uap, retval)
	struct proc *p;
	struct args {
		caddr_t	addr;
		int	len;
		int	prot;
	} *uap;
	int *retval;
{
	vm_offset_t addr;
	vm_size_t size;
	register vm_prot_t prot;

#ifdef DEBUG
	if (mmapdebug & MDB_FOLLOW)
		printf("mprotect(%d): addr %x len %x prot %d\n",
		       p->p_pid, uap->addr, uap->len, uap->prot);
#endif

	addr = (vm_offset_t) uap->addr;
	if ((addr & page_mask) || uap->len < 0)
		return(EINVAL);
	size = (vm_size_t) uap->len;
	/*
	 * Map protections
	 */
	prot = VM_PROT_NONE;
	if (uap->prot & PROT_READ)
		prot |= VM_PROT_READ;
	if (uap->prot & PROT_WRITE)
		prot |= VM_PROT_WRITE;
	if (uap->prot & PROT_EXEC)
		prot |= VM_PROT_EXECUTE;

	switch (vmspace_protect(p->p_vmspace, (caddr_t)addr, size, FALSE,
	    prot)) {
	case KERN_SUCCESS:
		return (0);
	case KERN_PROTECTION_FAILURE:
		return (EACCES);
	}
	return (EINVAL);
}

/* ARGSUSED */
madvise(p, uap, retval)
	struct proc *p;
	struct args {
		caddr_t	addr;
		int	len;
		int	behav;
	} *uap;
	int *retval;
{

	/* Not yet implemented */
	return (EOPNOTSUPP);
}

/* ARGSUSED */
mincore(p, uap, retval)
	struct proc *p;
	struct args {
		caddr_t	addr;
		int	len;
		char	*vec;
	} *uap;
	int *retval;
{

	/* Not yet implemented */
	return (EOPNOTSUPP);
}


/*
 * System call swapon(name) enables swapping on device name,
 * which must be in the swdevsw.  Return EBUSY
 * if already swapping on this device.
 */
/* ARGSUSED */
swapon(p, uap, retval)
	struct proc *p;
	struct args {
		char	*name;
	} *uap;
	int *retval;
{
	register struct vnode *vp;
	/* register struct swdevt *sp;*/
	register struct nameidata *ndp;
	dev_t dev;
	int error;
	struct nameidata nd;

	/* have the privledge to attempt swapon ? */
	if (error = use_priv(p->p_ucred, PRV_SWAPON, p))
		return (error);

	ndp = &nd;
	ndp->ni_nameiop = LOOKUP | FOLLOW;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->name;
	if (error = namei(ndp, p))
		return (error);
	vp = ndp->ni_vp;
#ifdef oldway
	if (vp->v_type != VBLK) {
		vrele(vp);
		return (ENOTBLK);
	}
	dev = (dev_t)vp->v_rdev;
	/* if (major(dev) >= nblkdev) {
		vrele(vp);
		return (ENXIO);
	} */
	for (sp = &swdevt[0]; sp->sw_dev; sp++)
		if (sp->sw_dev == dev) {
			if (sp->sw_freed) {
				vrele(vp);
				return (EBUSY);
			}
			sp->sw_vp = vp;
			printf("adding swap: ");
			if (error = swfree(p, sp - swdevt)) {
				printf("failed! (unchanged)\n");
				vrele(vp);
				return (error);
			}
			return (0);
		}
	vrele(vp);
	return (EINVAL);
#else
	error = swfree(p, vp);
	if (error)
		vrele(vp);
	return (error);
#endif
}

/* ARGSUSED */
obreak(p, uap, retval)
	struct proc *p;
	struct args {
		char	*nsiz;
	} *uap;
	int *retval;
{
	register struct vmspace *vm = p->p_vmspace;
	vm_offset_t new, old;
	int rv;
	register int diff;

	old = (vm_offset_t)vm->vm_daddr;
	new = round_page(uap->nsiz);
	if ((int)(new - old) > p->p_rlimit[RLIMIT_DATA].rlim_cur)
		return(ENOMEM);
	old = round_page(old + ctob(vm->vm_dsize));
	diff = new - old;
	if (diff > 0) {
		rv = vmspace_allocate(vm, &old, diff, FALSE);
		if (rv != KERN_SUCCESS) {
			uprintf("sbrk: grow failed, return = %d\n", rv);
			return(ENOMEM);
		}
		vm->vm_dsize += btoc(diff);
	} else if (diff < 0) {
		diff = -diff;
		vmspace_delete(vm, (caddr_t)new, diff);
		vm->vm_dsize -= btoc(diff);
	}
	return(0);
}

/* ARGSUSED */
ovadvise(p, uap, retval)
	struct proc *p;
	struct args {
		int	anom;
	} *uap;
	int *retval;
{

	return (EINVAL);
}
