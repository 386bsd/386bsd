/* 
 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * The Mach Operating System project at Carnegie-Mellon University.
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
 * $Id: lock.c,v 1.1 94/10/20 00:03:01 bill Exp $
 *
 *
 * Copyright (c) 1987, 1990 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Authors: Avadis Tevanian, Jr., Michael Wayne Young
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/* Locking primitives implementation. */

#include "sys/param.h"
#include "vm_param.h"
#include "lock.h"
#include "proc.h"
#define __NO_INLINES
#include "prototypes.h"
static char nothread;

/*
 * Some general notes on locks:
 * * A write lock require exclusive access, a read lock doesn't but must
 *   defer to a write lock.
 * * Attempts to upgrade a read lock to a write lock may lose the read
 *   lock if another upgrade is in progress, requiring a read lock to be
 *   reacquired (it will block until write unlock).
 * * Attempts to downgrade a write lock to a read lock are granted
 *   unconditionally.
 * * Recursive mode on locks bind the lock to the thread and grant any
 *   lock request on that thread, blocking all other threads unconditionally.
 * * As implemented, recursive mode presumes a write lock on the map, thus
 *   it is impossible to distinguish the difference between read/write
 *   conditions on a recursive lock.
 * -wfj
 */

/* Initialize a lock; required before use. Locks are statically allocated. */
void
lock_init(lock_t l)
{
	l->want_write = FALSE;
	l->want_upgrade = FALSE;
	l->read_count = 0;
	l->thread = &nothread;
	l->recursion_depth = 0;
}

/* block waiting for write lock */
void
lock_write(lock_t l)
{
	register int	i;

	/* allow recursion? */
	if (((struct proc *)l->thread) == curproc) {
		l->recursion_depth++;
		return;
	}

	/* wait for write lock */
	while (l->want_write) {
		l->waiting = TRUE;
		(void) tsleep((caddr_t)l, PVM, "lckwrw", 0);
	}
	l->want_write = TRUE;

	/* have write lock, wait for readers and upgrades to release. */
	while ((l->read_count != 0) || l->want_upgrade) {
		l->waiting = TRUE;
		(void) tsleep((caddr_t)l, PVM, "lckwru", 0);
	}
}

/* release lock regardless how it was obtained. */
void
lock_done(lock_t l)
{
	/* release the appropriate lock */
	if (l->read_count != 0)		 /* read lock */
		l->read_count--;
	else
	if (l->recursion_depth != 0) /* write lock: recursive lock */
		l->recursion_depth--;
	else
	if (l->want_upgrade)	/* write lock: upgrade attempt */
	 	l->want_upgrade = FALSE;
	else			/* write lock: write attempt */
	 	l->want_write = FALSE;

	/* someone waiting for lock? */
	if (l->waiting) {
		l->waiting = FALSE;
		wakeup((caddr_t) l);
	}
}

/* block waiting for read lock */
void
lock_read(lock_t l)
{
	register int	i;

	/* allow recursive lock? */
	if (((struct proc *)l->thread) == curproc) {
		l->read_count++;
		return;
	}

	/* any write or upgrades request outstanding? */
	while (l->want_write || l->want_upgrade) {
		l->waiting = TRUE;
		(void) tsleep((caddr_t)l, PVM, "lckwru", 0);
	}

	/* grab read lock */
	l->read_count++;
}

/*
 * block on read lock being upgraded to write lock. if another upgrade
 * is already in progress, release read lock to avoid deadlock and return
 * true indicating upgrade failed and read lock must be reacquired.
 */
boolean_t
lock_read_to_write(lock_t l)
{
	register int	i;

	/* lose read lock */
	l->read_count--;

	/* allow recursive lock? */
	if (((struct proc *)l->thread) == curproc) {
		/* write lock is implied */
		l->recursion_depth++;
		return(FALSE);
	}

	/* already an upgrade request? */
	if (l->want_upgrade) {
		/* if waiting for it, let him have it */
		if (l->waiting) {
			l->waiting = FALSE;
			wakeup((caddr_t) l);
		}

		/* we lost our read lock and failed the upgrade */
		return (TRUE);
	}

	/* signal that we want the lock */
	l->want_upgrade = TRUE;

	/* if any other reader locks, wait for them. */
	while (l->read_count != 0) {
		l->waiting = TRUE;
		(void) tsleep((caddr_t)l, PVM, "lkrdwr", 0);
	}

	return (FALSE);
}

/* downgrade write lock to read lock. */
void
lock_write_to_read(lock_t l)
{
	/* since we already have write lock, we can have read lock. */
	l->read_count++;

	/* release write lock */
	if (l->recursion_depth != 0)
		l->recursion_depth--;
	else
	if (l->want_upgrade)
		l->want_upgrade = FALSE;
	else
	 	l->want_write = FALSE;

	/* anyone waiting for a lock? */
	if (l->waiting) {
		l->waiting = FALSE;
		wakeup((caddr_t) l);
	}
}

/* attempt to obtain write lock. */
boolean_t
lock_try_write(lock_t l)
{
	/* allow recursive lock? */
	if (((struct proc *)l->thread) == curproc) {
		l->recursion_depth++;
		return(TRUE);
	}

	/* lock in use? */
	if (l->want_write || l->want_upgrade || l->read_count)
		return(FALSE);

	/* grab write lock */
	l->want_write = TRUE;
	return(TRUE);
}

/* attempt to obtain read lock. */
boolean_t
lock_try_read(lock_t l)
{
	/* allow recursive lock ? */
	if (((struct proc *)l->thread) == curproc) {
		l->read_count++;
		return(TRUE);
	}

	/* write lock in use? */
	if (l->want_write || l->want_upgrade)
		return(FALSE);

	/* grab read lock */
	l->read_count++;
	return(TRUE);
}

/* attempt to upgrade read lock to write lock without losing read lock */
boolean_t
lock_try_read_to_write(lock_t l)
{
	/* allow recursive lock ? */
	if (((struct proc *)l->thread) == curproc) {
		l->read_count--;
		l->recursion_depth++;
		return(TRUE);
	}

	/* can't get upgrade ? */
	if (l->want_upgrade)
		return(FALSE);

	/* grab upgrade, lose read lock. */
	l->want_upgrade = TRUE;
	l->read_count--;

	/* if other read locks, wait for upgrade. */
	while (l->read_count != 0) {
		l->waiting = TRUE;
		(void) tsleep((caddr_t)l, PVM, "lkrdwr", 0);
	}

	return(TRUE);
}

/* change mode of lock to indicate all locks to be honored for this thread. */
void
lock_set_recursive(lock_t l)
{
	/* must already have write lock */
	if (!l->want_write)
		panic("lock_set_recursive: don't have write lock");
	
	/* assign lock to thread */
	if ((struct proc *)l->thread != curproc && l->thread != &nothread)
		panic("lock_set_recursive: already set recursive");
	l->thread = (char *) curproc;
}

/* clear special "recursive" mode of lock breaking association with thread. */
void
lock_clear_recursive(lock_t l)
{
	if (((struct proc *) l->thread) != curproc)
		panic("lock_clear_recursive: wrong thread");
	
	if (l->recursion_depth == 0)
		l->thread = &nothread;
}
