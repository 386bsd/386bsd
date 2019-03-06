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
 *	$Id: mmap.c,v 1.1 94/10/19 17:37:17 bill Exp $
 */

/*
 * Mapped file (mmap) interface to VM
 */

#include "sys/param.h"
#include "sys/mman.h"
#include "uio.h"
#include "sys/errno.h"
#include "filedesc.h"
#include "proc.h"
#include "namei.h"	/* specdev.h */
#include "specdev.h"
#include "vm.h"
#include "vmspace.h"
#ifdef	KTRACE
#include "sys/ktrace.h"
#endif

#include "vnode.h"

#include "prototypes.h"

#ifdef DEBUG
int mmapdebug = 0;
#define MDB_FOLLOW	0x01
#define MDB_SYNC	0x02
#define MDB_MAPIT	0x04
#endif

void
munmapfd(struct proc *p, int fd)
{
#ifdef DEBUG
	if (mmapdebug & MDB_FOLLOW)
		printf("munmapfd(%d): fd %d\n", p->p_pid, fd);
#endif

	/*
	 * XXX -- should vmspace_delete any regions mapped to this file
missing the data structure to translate fd into ([addr,size),))
	 */
	p->p_fd->fd_ofileflags[fd] &= ~UF_MAPPED;
}

/*
 * Internal version of mmap.
 * Currently used by mmap, exec, and sys5 shared memory.
 * Handle is:
 *	MAP_FILE: a vnode pointer
 *	MAP_ANON: NULL or a file pointer
 */
int
vmspace_mmap(struct vmspace *vs, vm_offset_t *addr, vm_size_t size, vm_prot_t prot,
	int flags, caddr_t handle, vm_offset_t foff)
	/* caddr_t handle; XXX should be vp */
{
	register vm_pager_t pager;
	boolean_t fitit;
	vm_object_t object;
	vm_map_t map = &vs->vm_map;
	struct vnode *vp;
	int type;
	int rv = KERN_SUCCESS;

#ifdef KTRACE
	if (curproc && KTRPOINT(curproc, KTR_VM))
		ktrvm(curproc->p_tracep, KTVM_MMAP, addr, size, flags, foff);
#endif
	if (size == 0)
		return (0);

/*pg("vm_mmap %x %d", *addr, size);*/
	/*
	 * If we cannot allocate any pages of memory, don't
	 * allow any additional address space allocations.
	 */
	if (curproc && curproc->p_vmspace == vs && chk4space(atop(size)) == 0)
		return(ENOMEM);

	if ((flags & MAP_FIXED) == 0) {
		fitit = TRUE;
		*addr = round_page(*addr);
	} else {
		fitit = FALSE;
		(void) vmspace_delete(vs, (caddr_t)*addr, size);
	}

	/*
	 * Lookup/allocate pager.  All except an unnamed anonymous lookup
	 * gain a reference to ensure continued existance of the object.
	 * (XXX the exception is to appease the pageout daemon)
	 */
	if ((flags & MAP_TYPE) == MAP_ANON)
		type = PG_DFLT;
	else {
		vp = (struct vnode *)handle;
		if (vp->v_type == VCHR) {
			int xxx = (int)vp->v_rdev + 1; /* XXX */
			type = PG_DEVICE;
			handle = (caddr_t)xxx;
		} else
			type = PG_VNODE;
	}
	if ((flags & MAP_TYPE) == MAP_ANON && handle == NULL) {
		rv = vmspace_allocate(vs, addr, size, fitit); 
/*pg("vm_mmap:a1 %x %d", *addr, size);*/
		if (rv != KERN_SUCCESS)
			goto out;
		goto yadayada;
	}
	pager = vm_pager_allocate(type, handle, size, prot);
	if (pager == NULL)
		return (type == PG_DEVICE ? EINVAL : ENOMEM);
	/*
	 * Find object and release extra reference gained by lookup
	 */
	/* if ((flags & MAP_TYPE) == MAP_ANON && handle != NULL)
		pg("handle %d", handle); */
	if (handle != NULL) {
	object = vm_object_lookup(pager);
	vm_object_deallocate(object);
	} else {
#ifdef nope
	object = vm_object_lookup(pager);
	if (object != 0) {
		printf("obj %x ty %d han %x size %d pgr %x ",
			object, type, handle, size, pager);
		pg("vm_mmap: ");
		vm_object_deallocate(object);
	object = vm_object_lookup(pager);
		pg("vm_mmap: %x", object);
	}
#endif
	object = 0;
	}

	/*
	 * Anonymous memory.
	 */
	if ((flags & MAP_TYPE) == MAP_ANON) {
		rv = vm_allocate_with_pager(map, addr, size, fitit,
					    pager, (vm_offset_t)foff, TRUE);
		if (rv != KERN_SUCCESS) {
			if (handle == NULL)
				vm_pager_deallocate(pager);
			/* else
				vm_object_deallocate(object); */
			goto out;
		}
#ifdef DEBUG
		if (mmapdebug & MDB_MAPIT)
			printf("vm_mmap(%d): ANON *addr %x size %x pager %x\n",
			       curproc->p_pid, *addr, size, pager);
#endif
	}
	/*
	 * Must be type MAP_FILE.
	 * Distinguish between character special and regular files.
	 */
	else if (vp->v_type == VCHR) {
		vm_offset_t off;
		rv = vm_allocate_with_pager(map, addr, size, fitit,
					    pager, (vm_offset_t)foff, FALSE);

		/* pass offset information to the device pager */
		for(off = trunc_page(foff); off < round_page(foff + size);
				off += NBPG)
			if (vm_pager_has_page(pager, off) == FALSE) {
				rv = KERN_INVALID_ADDRESS;
				break;
			}
		/*
		 * Uncache the object and lose the reference gained
		 * by vm_pager_allocate().  If the call to
		 * vm_allocate_with_pager() was sucessful, then we
		 * gained an additional reference ensuring the object
		 * will continue to exist.  If the call failed then
		 * the deallocate call below will terminate the
		 * object which is fine.
		 */
		(void) pager_cache(object, FALSE);
		if (rv != KERN_SUCCESS)
			goto out;
	}
	/*
	 * A regular file
	 */
	else {
#ifdef DEBUG
		if (object == NULL)
			printf("vm_mmap: no object: vp %x, pager %x\n",
			       vp, pager);
#endif
		/*
		 * Map it directly.
		 * Allows modifications to go out to the vnode.
		 */
		if (flags & MAP_SHARED) {
			rv = vm_allocate_with_pager(map, addr, size,
						    fitit, pager,
						    (vm_offset_t)foff, FALSE);
			if (rv != KERN_SUCCESS) {
				vm_object_deallocate(object);
				goto out;
			}
			/*
			 * Don't cache the object.  This is the easiest way
			 * of ensuring that data gets back to the filesystem
			 * because vnode_pager_deallocate() will fsync the
			 * vnode.  pager_cache() will lose the extra ref.
			 */
			if (prot & VM_PROT_WRITE)
				pager_cache(object, FALSE);
			else
				vm_object_deallocate(object);
		}
		/*
		 * Copy-on-write of file.  Two flavors.
		 * MAP_COPY is true COW, you essentially get a snapshot of
		 * the region at the time of mapping.  MAP_PRIVATE means only
		 * that your changes are not reflected back to the object.
		 * Changes made by others will be seen.
		 */
		else {
			vm_map_t tmap;
			vm_offset_t off = VM_MIN_ADDRESS;
			struct vmspace tspace;

			/* locate and allocate the target address space */
			/*rv = vm_map_find(map, NULL, (vm_offset_t)0,
					 addr, size, fitit);*/
			vm_map_lock(map);
			if (fitit) {
				rv = vm_map_find(map, addr, size);
				if (rv != KERN_SUCCESS)
					goto xxxxx;
			}
			/*rv = vm_map_insert(map, NULL, (vm_offset_t)0,
					 *addr, *addr + size);*/
xxxxx:
			vm_map_unlock(map);
			if (rv != KERN_SUCCESS) {
				vm_object_deallocate(object);
				goto out;
			}

			tmap = &tspace.vm_map;
			vm_map_init(tmap, off, off + size, TRUE);
					     
			rv = vm_allocate_with_pager(tmap, &off, size,
						    TRUE, pager,
						    (vm_offset_t)foff, FALSE);
			if (rv != KERN_SUCCESS) {
				vm_object_deallocate(object);
				vmspace_delete(&tspace, (caddr_t)off, size);
				goto out;
			}
			/*
			 * (XXX)
			 * MAP_PRIVATE implies that we see changes made by
			 * others.  To ensure that we need to guarentee that
			 * no copy object is created (otherwise original
			 * pages would be pushed to the copy object and we
			 * would never see changes made by others).  We
			 * totally sleeze it right now by marking the object
			 * internal temporarily.
			 */
			if ((flags & MAP_COPY) == 0)
				object->internal = TRUE;
			rv = vm_map_copy(map, tmap, *addr, size, off,
					 TRUE, FALSE);
			object->internal = FALSE;
			/*
			 * (XXX)
			 * My oh my, this only gets worse...
			 * Force creation of a shadow object so that
			 * vmspace_fork() will do the right thing.
			 */
			if ((flags & MAP_COPY) == 0) {
				vm_map_t tmap;
				vm_map_entry_t tentry;
				vm_object_t tobject;
				vm_offset_t toffset;
				vm_prot_t tprot;
				boolean_t twired, tsu;

				tmap = map;
				vm_map_lookup(&tmap, *addr, VM_PROT_WRITE,
					      &tentry, &tobject, &toffset,
					      &tprot, &twired, &tsu);
				vm_map_lookup_done(tmap, tentry);
			}
			/*
			 * (XXX)
			 * Map copy code cannot detect sharing unless a
			 * sharing map is involved.  So we cheat and write
			 * protect everything ourselves.
			 */
			vm_object_pmap_copy(object, (vm_offset_t)foff,
					    (vm_offset_t)foff+size);
			vm_object_deallocate(object);
			vmspace_delete(&tspace, (caddr_t)off, size);
			if (rv != KERN_SUCCESS)
				goto out;
		}
#ifdef DEBUG
		if (mmapdebug & MDB_MAPIT)
			printf("vm_mmap(%d): FILE *addr %x size %x pager %x\n",
			       curproc->p_pid, *addr, size, pager);
#endif
	}
yadayada:
	/*
	 * Correct protection (default is VM_PROT_ALL).
	 * Note that we set the maximum protection.  This may not be
	 * entirely correct.  Maybe the maximum protection should be based
	 * on the object permissions where it makes sense (e.g. a vnode).
	 *
	 * Changed my mind: leave max prot at VM_PROT_ALL.
	 */
	if (prot != VM_PROT_ALL) {
		rv = vmspace_protect(vs, (caddr_t)*addr, size, FALSE, prot);
		if (rv != KERN_SUCCESS) {
			(void) vmspace_delete(vs, (caddr_t)*addr, size);
			goto out;
		}
	}

	/*
	 * Shared memory is also shared with children.
	 */
	if (flags & MAP_SHARED) {
		rv = vmspace_inherit(vs, (caddr_t)*addr, size, VM_INHERIT_SHARE);
		if (rv != KERN_SUCCESS) {
			(void) vmspace_delete(vs, (caddr_t)*addr, size);
			goto out;
		}
	}

	/*
	 * Inherit region.
	 */
	if (flags & MAP_INHERIT) {
		rv = vmspace_inherit(vs, (caddr_t)*addr, size, VM_INHERIT_COPY);
		if (rv != KERN_SUCCESS) {
			(void) vmspace_delete(vs, (caddr_t)*addr, size);
			goto out;
		}
	}
out:
#ifdef DEBUG
	if (mmapdebug & MDB_MAPIT)
		printf("vm_mmap: rv %d\n", rv);
#endif
	switch (rv) {
	case KERN_SUCCESS:
		return (0);
	case KERN_INVALID_ADDRESS:
	case KERN_NO_SPACE:
		return (ENOMEM);
	case KERN_PROTECTION_FAILURE:
		return (EACCES);
	default:
		return (EINVAL);
	}
}

/*
 * Internal bastardized version of MACHs vm_region system call.
 * Given address and size it returns map attributes as well
 * as the (locked) object mapped at that location. 
 */
int
vm_region(vm_map_t map, vm_offset_t *addr, vm_size_t *size, vm_prot_t *prot,
	vm_prot_t *max_prot, vm_inherit_t *inheritance, boolean_t *shared,
	vm_object_t *object, vm_offset_t *objoff)
{
	vm_map_entry_t	tmp_entry;
	register
	vm_map_entry_t	entry;
	register
	vm_offset_t	tmp_offset;
	vm_offset_t	start;

	if (map == NULL)
		return(KERN_INVALID_ARGUMENT);
	
	start = *addr;

	vm_map_lock_read(map);
	if (!vm_map_lookup_entry(map, start, &tmp_entry)) {
		if ((entry = tmp_entry->next) == &map->header) {
			vm_map_unlock_read(map);
		   	return(KERN_NO_SPACE);
		}
		start = entry->start;
		*addr = start;
	} else
		entry = tmp_entry;

	*prot = entry->protection;
	*max_prot = entry->max_protection;
	*inheritance = entry->inheritance;

	tmp_offset = entry->offset + (start - entry->start);
	*size = (entry->end - start);

	if (entry->is_a_map) {
		register vm_map_t share_map;
		vm_size_t share_size;

		share_map = entry->object.share_map;

		vm_map_lock_read(share_map);
		(void) vm_map_lookup_entry(share_map, tmp_offset, &tmp_entry);

		if ((share_size = (tmp_entry->end - tmp_offset)) < *size)
			*size = share_size;

		*object = tmp_entry->object.vm_object;
		*objoff = tmp_entry->offset + (tmp_offset - tmp_entry->start);

		*shared = (share_map->ref_count != 1);
		vm_map_unlock_read(share_map);
	} else {
		*object = entry->object.vm_object;
		*objoff = tmp_offset;

		*shared = FALSE;
	}

	vm_map_unlock_read(map);

	return(KERN_SUCCESS);
}

/*
 * Yet another bastard routine.
 */
int
vm_allocate_with_pager(vm_map_t map, vm_offset_t *addr, vm_size_t size,
	boolean_t fitit, vm_pager_t pager, vm_offset_t poffset,
	boolean_t internal)
{
	register volatile vm_object_t	object;
	register int		result;

	if (map == NULL)
		return(KERN_INVALID_ARGUMENT);

	*addr = trunc_page(*addr);
	size = round_page(size);

	/* find address */
	if (fitit) {
		result = vm_map_find(map, addr, size);
		if (result != KERN_SUCCESS)
			return (result);
	}

	/*
	 *	Lookup the pager/paging-space in the object cache.
	 *	If it's not there, then create a new object and cache
	 *	it.
	 */
	if (internal)
		object = NULL;
	else {
		object = vm_object_lookup(pager);
		vm_stat.lookups++;
	}
	if (object == NULL) {
		object = vm_object_allocate(size);
		vm_object_reference(object);
		object->pager = pager;
	} else
		vm_stat.hits++;
	object->internal = internal;

	result = vm_map_insert(map, object, poffset,
					 *addr, *addr + size);
	vm_map_unlock(map);
	if (result != KERN_SUCCESS)
		vm_object_deallocate(object);
	else if (pager != NULL && internal)
		vm_object_enter(object);

	/* lose persistance if internal (lose reference gained)*/
	if (internal)
		(void) pager_cache(object, FALSE);
	return (result);
}

/*
 * XXX: this routine belongs in vm_map.c.
 *
 * Returns TRUE if the range [start - end) is allocated in either
 * a single entry (single_entry == TRUE) or multiple contiguous
 * entries (single_entry == FALSE).
 *
 * start and end should be page aligned.
 */
boolean_t
vm_map_is_allocated(vm_map_t map, vm_offset_t start, vm_offset_t end,
	boolean_t single_entry)
{
	vm_map_entry_t mapent;
	register vm_offset_t nend;

	vm_map_lock_read(map);

	/*
	 * Start address not in any entry
	 */
	if (!vm_map_lookup_entry(map, start, &mapent)) {
		vm_map_unlock_read(map);
		return (FALSE);
	}
	/*
	 * Find the maximum stretch of contiguously allocated space
	 */
	nend = mapent->end;
	if (!single_entry) {
		mapent = mapent->next;
		while (mapent != &map->header && mapent->start == nend) {
			nend = mapent->end;
			mapent = mapent->next;
		}
	}

	vm_map_unlock_read(map);
	return (end <= nend);
}
