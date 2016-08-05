/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.
 *
 * +Redistribution and use in source and binary forms, with or without
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
 * $Id: domain.c,v 1.1 94/10/19 23:49:50 bill Exp Locker: bill $
 */

#include "sys/param.h"
#include "sys/time.h"
#include "sys/socket.h"
#include "sys/errno.h"
#include "protosw.h"
#include "domain.h"
#include "mbuf.h"
#include "kernel.h"	/* hz */
#include "netisr.h"

struct domain *domains;		/* head of list of domains */

/* add a domain module into the domains list */
void
adddomain(struct domain *dp)
{
	struct protosw *pr;

	dp->dom_next = domains;
	domains = dp;

	/* initialize the domain, and any protocol within the domain */
	if (dp->dom_init)
		(*dp->dom_init)();

	for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
		if (pr->pr_init)
			(*pr->pr_init)();

	if (max_linkhdr < 16)		/* XXX */
		max_linkhdr = 16;
	max_hdr = max_linkhdr + max_protohdr;
	max_datalen = MHLEN - max_hdr;
}

/* XXX catch bogus software interrupts */
defaultnetintr() {

	printf("default net interrupt\n");
}

static void pffasttimo(void), pfslowtimo(void);
net_intr_t netintr[32];

/* initialize domains */
void
domaininit(void)
{
	int i;

	/* XXX set default network software interrupts */
	for (i = 0; i < NBBY*sizeof(netisr); i++)
		netintr[i] = (void (*)(void)) defaultnetintr;

	pffasttimo();
	pfslowtimo();
}

/* locate a protocol in a family by type alone */
struct protosw *
pffindtype(int family, int type)
{
	struct domain *dp;
	struct protosw *pr;

	/* walk domain list to find protocol family */
	for (dp = domains; dp; dp = dp->dom_next)
		if (dp->dom_family == family)
			goto found;
	return (0);

	/* examine all protocols to find first type matching */
found:
	for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
		if (pr->pr_type && pr->pr_type == type)
			return (pr);
	return (0);
}

/* locate a protocol in a family by protocol number and type */
struct protosw *
pffindproto(int family, int protocol, int type)
{
	struct domain *dp;
	struct protosw *pr, *maybe = 0;

	/* no unspecified */
	if (family == 0)
		return (0);

	/* walk domain list to find protocol family */
	for (dp = domains; dp; dp = dp->dom_next)
		if (dp->dom_family == family)
			goto found;
	return (0);

	/* locate protocol in domain */
found:
	for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++) {
		if ((pr->pr_protocol == protocol) && (pr->pr_type == type))
			return (pr);

		if (type == SOCK_RAW && pr->pr_type == SOCK_RAW &&
		    pr->pr_protocol == 0 && maybe == (struct protosw *)0)
			maybe = pr;
	}
	return (maybe);
}

/* broadcast a control command to all protocols in the sockets family */
void
pfctlinput(int cmd, struct sockaddr *sa)
{
	struct domain *dp;
	struct protosw *pr;

	/* for all protocols in all domains, pass the message */
	for (dp = domains; dp; dp = dp->dom_next)
		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
			if (pr->pr_ctlinput)
				(*pr->pr_ctlinput)(cmd, sa, (caddr_t) 0);
}

/* poke all protocols slow timeout functions */
static void
pfslowtimo(void)
{
	struct domain *dp;
	struct protosw *pr;

	/* for all protocols in all domains, pass the message */
	for (dp = domains; dp; dp = dp->dom_next)
		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
			if (pr->pr_slowtimo)
				(*pr->pr_slowtimo)();

	timeout(pfslowtimo, (caddr_t)0, hz/2);
}

/* poke all protocols fast timeout functions */
static void
pffasttimo(void)
{
	struct domain *dp;
	struct protosw *pr;

	/* for all protocols in all domains, pass the message */
	for (dp = domains; dp; dp = dp->dom_next)
		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
			if (pr->pr_fasttimo)
				(*pr->pr_fasttimo)();

	timeout(pffasttimo, (caddr_t)0, hz/5);
}
