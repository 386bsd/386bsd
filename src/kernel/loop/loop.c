/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
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
 *	@(#)if_loop.c	7.13 (Berkeley) 4/26/91
 */

/*
 * Loopback interface driver for protocol testing and timing.
 */

#include "sys/param.h"
#include "sys/ioctl.h"
#include "sys/errno.h"
#include "mbuf.h"
#include "modconfig.h"
#include "esym.h"

#include "machine/cpu.h"

#include "socketvar.h"
#include "if.h"
#include "if_types.h"
#include "netisr.h"
#define	_ROUTE_PROTOTYPES /* XXX */
#include "route.h"
#undef	_ROUTE_PROTOTYPES /* XXX */

#ifdef	INET
#include "in.h"
#include "in_systm.h"
#include "in_var.h"
#include "ip.h"
#endif

#ifdef NS
#include "ns.h"
#include "ns_if.h"
#endif

#ifdef ISO
#include "iso.h"
#include "iso_var.h"
#endif

#define	LOMTU	(1024+512)

static struct	ifnet loif;
static int looutput(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
	struct rtentry *rt);
static int loioctl(struct ifnet *ifp, int cmd, caddr_t data);
static void loattach(void);
static int lortrequest(int cmd, struct rtentry *rt, struct sockaddr *sa);

static void
loattach(void)
{
	register struct ifnet *ifp = &loif;

	ifp->if_name = "lo";
	ifp->if_mtu = LOMTU;
	ifp->if_flags = IFF_LOOPBACK;
	ifp->if_ioctl = loioctl;
	ifp->if_output = looutput;
	ifp->if_type = IFT_LOOP;
	ifp->if_hdrlen = 0;
	ifp->if_addrlen = 0;
	if_attach(ifp);
}

static int
looutput(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
	struct rtentry *rt)
{
	int s, isr;
	register struct ifqueue *ifq = 0;

/* printf("\nlo:%d ", getit(0,0)); */
	if ((m->m_flags & M_PKTHDR) == 0)
		panic("looutput no HDR");
	m->m_pkthdr.rcvif = ifp;
#ifdef	PKT_TRACE
{ unsigned x = getticks();
printf("lo:%d ", x - m->m_pkthdr.time);
x = m->m_pkthdr.time;
}
#endif

	if (rt && rt->rt_flags & RTF_REJECT) {
		m_freem(m);
		return (rt->rt_flags & RTF_HOST ? EHOSTUNREACH : ENETUNREACH);
	}
	ifp->if_opackets++;
	ifp->if_obytes += m->m_pkthdr.len;
	switch (dst->sa_family) {

#ifdef INET
	case AF_INET:
		ifq = &ipintrq;
		isr = NETISR_IP;
		break;
#endif
#ifdef NS
	case AF_NS:
		ifq = &nsintrq;
		isr = NETISR_NS;
		break;
#endif
#ifdef ISO
	case AF_ISO:
		ifq = &clnlintrq;
		isr = NETISR_ISO;
		break;
#endif
	default:
		printf("lo%d: can't handle af%d\n", ifp->if_unit,
			dst->sa_family);
		m_freem(m);
		return (EAFNOSUPPORT);
	}
	s = splimp();
	if (IF_QFULL(ifq)) {
		IF_DROP(ifq);
		m_freem(m);
		splx(s);
		return (ENOBUFS);
	}
	IF_ENQUEUE(ifq, m);
	schednetisr(isr);
	ifp->if_ipackets++;
	ifp->if_ibytes += m->m_pkthdr.len;
	splx(s);
	return (0);
}

static int
lortrequest(int cmd, struct rtentry *rt, struct sockaddr *sa)
{
	if (rt)
		rt->rt_rmx.rmx_mtu = LOMTU;
}

/*
 * Process an ioctl request.
 */
static int
loioctl(struct ifnet *ifp, int cmd, caddr_t data)
{
	struct ifaddr *ifa;
	int error = 0;

	switch (cmd) {

	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		ifa = (struct ifaddr *)data;
		if (ifa != 0 && ifa->ifa_addr->sa_family == AF_ISO)
			ifa->ifa_rtrequest = lortrequest;
		/*
		 * Everything else is done at a higher level.
		 */
		break;

	default:
		error = EINVAL;
	}
	return (error);
}

LOCALNET_MODCONFIG() {

	loattach();
	esym_bind(looutput);
	esym_bind(loif);
}

