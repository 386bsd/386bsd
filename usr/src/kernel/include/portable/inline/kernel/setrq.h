/*
 * Copyright (c) 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: $
 * Enqueue a process on a run queue. Process will be on a run queue
 * until run for a time slice (swtch()), or removed by remrq().
 * Should only be called with a running process, and with the
 * processor protecting against rescheduling.
 */
__INLINE void
setrq(struct proc *p) {
	int rqidx;
	struct prochd *ph;
	struct proc *or;

	/* Rescale 256 priority levels to fit into 32 queue headers */
	rqidx = p->p_pri / 4;

#ifdef	DIAGNOSTIC
	/*
	 * If this process is already linked on the run queue,
	 * we are in trouble.
	 */
	if (p->p_rlink != 0)
		panic("setrq: already linked");
#endif

	/* Link this process on the appropriate queue tail */
	ph = qs + rqidx;
	p->p_link = (struct proc *)ph;
	or = p->p_rlink = ph->ph_rlink;
	ph->ph_rlink = or->p_link = p;

	/* Indicate that this queue has at least one process in it */
	whichqs |= (1<<rqidx);
}
