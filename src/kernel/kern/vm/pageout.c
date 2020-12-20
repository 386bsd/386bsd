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
 *	$Id: pageout.c,v 1.1 94/10/19 17:37:21 bill Exp $
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

/* Page replacement mechnism of the virtul memory subsystem. */

#include "sys/param.h"
#include "proc.h"
#include "kernel.h"	/* hz */
#include "resourcevar.h"
#include "vmmeter.h"
#include "vm.h"
#include "vm_pageout.h"

int vm_pages_needed;		/* event on which pageout daemon sleeps */
int vm_page_free_min_sanity = 40;
int vm_scan_min, vm_scan_max;	/* thresholds for pageout scan */
vm_page_t lastscan;		/* last inactive page scanned */

/* make a scan of the page queues to reclaim pages for the free page queue. */
static void
vm_pageout_scan(void)
{
	vm_page_t m, next;
	int deficit, pending = 0, freed = 0;

	/*
	 * Inactive pages will be scanned. force inactive page status
         * (modify,reference) to be valid.
	 */
	cnt.v_scan++;
	pmap_update();

	/*
	 * Check if the page we were last at is still inactive. If it
	 * is, continue from that point, otherwise, start at the last
	 * inactive page.
	 */
	if (lastscan && lastscan->inactive && lastscan->active == 0)
		m = lastscan;
	else
		m = (vm_page_t) queue_first(&vm_page_queue_inactive);
	lastscan = 0;

	/* scan inactive page queue backwards and reclaim pages */
	for (;!queue_end(&vm_page_queue_inactive, (queue_entry_t) m); m = next) {

		/* XXX if queue cross threaded, start over */
		if (m->active || m->inactive == 0) {
			next = (vm_page_t) queue_first(&vm_page_queue_inactive);
			continue;
		}

		/* check following page after */
		next = (vm_page_t) queue_next(&m->pageq);

		/* pageout in progress, bypass */
		if (m->busy)
			continue;

		/* if page is clean, reactivate if referenced, otherwise free */
		if (m->clean && m->laundry == 0)  {
			if (pmap_is_referenced(VM_PAGE_TO_PHYS(m))) {
				vm_page_activate(m);
				cnt.v_pgfrec++;
			} else {
			freepage:
				pmap_page_protect(VM_PAGE_TO_PHYS(m),
						  VM_PROT_NONE);
				vm_page_free(m);
				cnt.v_dfree++;
				freed++;
			}

			/* if queue exhausted, end scan */
			if (queue_empty(&vm_page_queue_inactive))
				break;

			/* if goal reached, end scan, ready to restart */
			if (vm_page_free_count + pending > vm_scan_max) {
				lastscan = next;
				break;
			}

			/* otherwise, continue scan */
			continue;
		}

		/*
		 * If page was dirtied when deactivated, it must be laundered
		 * before it can be reused.
		 */
		if (m->laundry) {
			vm_object_t object = m->object;
			vm_pager_t pager;
			int pageout_status;

			/* remove any adr. trans. mappings to page */
			pmap_page_protect(VM_PAGE_TO_PHYS(m), VM_PROT_NONE);

			/* gain exclusive access to page */
			m->busy = TRUE;

			/* attempt to consolidate object with shadow(s) */
			vm_object_collapse(object);

			/* anonymous memory needs to allocate pager? */
			if ((pager = object->pager) == NULL) {
				pager = vm_pager_allocate(PG_DFLT,
				    (caddr_t)0, object->size, VM_PROT_ALL);
				object->pager = pager;
				object->paging_offset = 0;
			}

			/* if pager, output, else retry later */
			if (pager) {
				object->paging_in_progress++;
				cnt.v_pgout++;
				cnt.v_pgpgout++;
				pageout_status = vm_pager_put(pager, m, FALSE);
			} else 
				pageout_status = VM_PAGER_FAIL;

			switch (pageout_status) {

			case VM_PAGER_OK:
			case VM_PAGER_PEND:
				/* will eventually be clean and reusable */
				m->laundry = FALSE;
				break;

			case VM_PAGER_BAD:
				/* free truncated page of object. */
				pmap_clear_modify(VM_PAGE_TO_PHYS(m));
				object->paging_in_progress--;
				goto freepage;

			case VM_PAGER_FAIL:
				/* postpone pageout retry */
				vm_page_activate(m);
				break;
			}

			/* if referenced after being cleaned, will reactivate */
			pmap_clear_reference(VM_PAGE_TO_PHYS(m));

			/* if pageout not pending, release page */
			if (pageout_status != VM_PAGER_PEND) {
				m->busy = FALSE;
				PAGE_WAKEUP(m);
				if (pager)
					object->paging_in_progress--;
			} else
				pending++;

			/* alert object of the absence of pageout operations */
			if (pager && object->paging_in_progress == 0)
				wakeup((caddr_t) object);
		}
	}
	
	/* have we made a complete pass through the page queue? */
	if (queue_end(&vm_page_queue_inactive, (queue_entry_t) m))
		cnt.v_rev++;

	/* always deactivate at least one active page per scan */
	deficit = max(vm_pages_needed - freed, 1);

	/* if memory resource still insufficent determine deficit */
	if (vm_page_free_count < vm_page_free_min)
		deficit = max(vm_page_free_min - vm_page_free_count, deficit);
	if (vm_page_free_count + pending < vm_page_free_target)
		deficit = max(vm_page_free_target -
		    (vm_page_free_count + pending), deficit);
    	if (vm_page_inactive_count - pending < vm_page_inactive_target) 
		deficit = max(vm_page_inactive_target -
		    (vm_page_inactive_count - pending), deficit);

	/* scan active queue deactivating unreferenced pages since last scan */
	for (m = (vm_page_t) queue_first(&vm_page_queue_active);
	    !queue_end(&vm_page_queue_active, (queue_entry_t) m); m = next) {

		/* checking from oldest to youngest */
		next = (vm_page_t) queue_next(&m->pageq);

		/* if page was not referenced since last scan, deactivate */
		if (pmap_is_referenced(VM_PAGE_TO_PHYS(m)) == 0 &&
		    deficit-- > 0) {
			vm_page_deactivate(m);

			if (queue_empty(&vm_page_queue_active))
				break;
		} else
			pmap_clear_reference(VM_PAGE_TO_PHYS(m));
	}

	/* deactivate active pages unconditionally to remedy deficit */
	while (deficit-- > 0)
	{
		if (queue_empty(&vm_page_queue_active))
			break;
		m = (vm_page_t) queue_first(&vm_page_queue_active);
		vm_page_deactivate(m);
	}
}

/* vm_pageout is the high level pageout daemon. */
volatile void
vm_pageout(void)
{

	/* set absolute minimum free pages to avoid deadlock */
	if (vm_page_free_min == 0) {
		vm_scan_min = vm_page_free_min = vm_page_free_count / 20;
		vm_scan_max = vm_scan_min / 2;
		if (vm_page_free_min > vm_page_free_min_sanity)
			vm_page_free_min = vm_page_free_min_sanity;
	}

	/* set desired number of free pages */
	if (vm_page_free_target == 0)
		vm_page_free_target = (vm_page_free_min * 4) / 3;

	/* set desired number of inactive pages */
	if (vm_page_inactive_target == 0)
		vm_page_inactive_target = vm_page_free_min * 2;

	if (vm_page_free_target <= vm_page_free_min)
		vm_page_free_target = vm_page_free_min + 1;

	if (vm_page_inactive_target <= vm_page_free_target)
		vm_page_inactive_target = vm_page_free_target + 1;

	/* scan for pages to reclaim when awoken */
	for (;;) {
		int rate;

		if (vm_page_free_count <= vm_page_free_target) {

		/* reclaim any outstanding pageouts from previous scan */
		vm_pager_sync();

		/* scan pages */
		vm_pageout_scan();

		/* immediate reclaim from this scan operation */
		vm_pager_sync();

		/* broadcast wakeup */
		wakeup((caddr_t) &vm_page_free_count);

		/* if target not reached, reschedule ourselves */
		if (vm_page_free_count < vm_scan_min) {

			/* determine rate to run based on linear ramp */ 
			if (vm_page_free_count < vm_scan_max)
				rate = max(hz/100, 1); 
			else
				rate = hz * (vm_page_free_count - vm_scan_max)
					/ (vm_scan_min - vm_scan_max);
		}
		else
			rate = 0;
		} else
			rate = 0;

		(void) tsleep((caddr_t)&proc0, PVM, "pageout", rate);
	}
}

/*
 * Gross check for swap space and memory.
 */
extern int swap_empty;

int
chk4space(int pages) {

	if (swap_empty && vm_page_free_count < vm_page_free_target
	    && vm_page_inactive_count < vm_page_inactive_target
	    && vm_page_free_count + vm_page_inactive_count - pages
		- vm_pages_needed < vm_page_free_min)
		return (0);
	return (1);
}

/*
 * Set default limits for VM system.
 * Called for proc 0, and then inherited by all others.
 */
void
vm_init_limits(struct proc *p)
{

	/*
	 * Set up the initial limits on process VM.
	 * Set the maximum resident set size to be all
	 * of (reasonably) available memory.  This causes
	 * any single, large process to start random page
	 * replacement once it fills memory.
	 */
        p->p_rlimit[RLIMIT_STACK].rlim_cur = DFLSSIZ;
        p->p_rlimit[RLIMIT_STACK].rlim_max = MAXSSIZ;
        p->p_rlimit[RLIMIT_DATA].rlim_cur = DFLDSIZ;
        p->p_rlimit[RLIMIT_DATA].rlim_max = MAXDSIZ;
	p->p_rlimit[RLIMIT_RSS].rlim_cur = p->p_rlimit[RLIMIT_RSS].rlim_max =
		(ptoa(vm_page_free_count) * 80)/100;
	p->p_stats->p_ru.ru_maxrss = p->p_rlimit[RLIMIT_RSS].rlim_cur;
}

/*
 * Wait for free pages, jabbing the pageout process.
 */
void
vm_page_wait(char *s, int npages) {

	vm_pages_needed += npages;
	wakeup((caddr_t)&proc0);
	(void) tsleep((caddr_t)&vm_page_free_count, PVM, s, 0);
	vm_pages_needed -= npages;
}
