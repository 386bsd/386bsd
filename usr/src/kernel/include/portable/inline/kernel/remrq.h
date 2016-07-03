/*
 * Copyright (c) 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: $
 * Function to dequeue a process from the run queue its stuck on.
 * Must be called with rescheduling clock blocked.
 */

__INLINE void
remrq(struct proc *p) {
	int rqidx;
	struct prochd *ph;

	/* Rescale 256 priority levels to fit into 32 queue headers */
	rqidx = p->p_pri / 4;

#ifdef	DIAGNOSTIC
	/* If a run queue is empty, something is definitely wrong */
	if (whichqs & (1<<rqidx) == 0)
		panic("remrq");
#endif

	/* Unlink process off doublely-linked run queue */
	p->p_link->p_rlink = p->p_rlink;
	p->p_rlink->p_link = p->p_link;

	/*
	 * If something is still present on the queue,
	 * set the corresponding bit. Otherwise clear it.
	 */
	ph = qs + rqidx;
	if ((struct prochd *)ph->ph_link == ph)
		whichqs &= ~(1<<rqidx);
	else
		whichqs |= (1<<rqidx);

	/* Mark this process as unlinked */
	p->p_rlink = (struct proc *) 0;
}
