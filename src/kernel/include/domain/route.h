/*
 * Copyright (c) 1980, 1986 Regents of the University of California.
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
 *	@(#)route.h	7.13 (Berkeley) 4/25/91
 */

/*
 * Kernel resident routing tables.
 * 
 * The routing tables are initialized when interface addresses
 * are set by making entries for all directly connected interfaces.
 */

/*
 * A route consists of a destination address and a reference
 * to a routing entry.  These are often held by protocols
 * in their control blocks, e.g. inpcb.
 */
struct route {
	struct	rtentry *ro_rt;
	struct	sockaddr ro_dst;
};

/*
 * These numbers are used by reliable protocols for determining
 * retransmission behavior and are included in the routing structure.
 */
struct rt_metrics {
	u_long	rmx_locks;	/* Kernel must leave these values alone */
	u_long	rmx_mtu;	/* MTU for this path */
	u_long	rmx_hopcount;	/* max hops expected */
	u_long	rmx_expire;	/* lifetime for route, e.g. redirect */
	u_long	rmx_recvpipe;	/* inbound delay-bandwith product */
	u_long	rmx_sendpipe;	/* outbound delay-bandwith product */
	u_long	rmx_ssthresh;	/* outbound gateway buffer limit */
	u_long	rmx_rtt;	/* estimated round trip time */
	u_long	rmx_rttvar;	/* estimated rtt variance */
};

/*
 * rmx_rtt and rmx_rttvar are stored as microseconds;
 * RTTTOPRHZ(rtt) converts to a value suitable for use
 * by a protocol slowtimo counter.
 */
#define	RTM_RTTUNIT	1000000	/* units for rtt, rttvar, as units per sec */
#define	RTTTOPRHZ(r)	((r) / (RTM_RTTUNIT / PR_SLOWHZ))

/*
 * We distinguish between routes to hosts and routes to networks,
 * preferring the former if available.  For each route we infer
 * the interface to use from the gateway address supplied when
 * the route was entered.  Routes that forward packets through
 * gateways are marked so that the output routines know to address the
 * gateway rather than the ultimate destination.
 */
#ifndef RNF_NORMAL
#include "radix.h"
#endif
struct rtentry {
	struct	radix_node rt_nodes[2];	/* tree glue, and other values */
#define	rt_key(r)	((struct sockaddr *)((r)->rt_nodes->rn_key))
#define	rt_mask(r)	((struct sockaddr *)((r)->rt_nodes->rn_mask))
	struct	sockaddr *rt_gateway;	/* value */
	short	rt_flags;		/* up/down?, host/net */
	short	rt_refcnt;		/* # held references */
	u_long	rt_use;			/* raw # packets forwarded */
	struct	ifnet *rt_ifp;		/* the answer: interface to use */
	struct	ifaddr *rt_ifa;		/* the answer: interface to use */
	struct	sockaddr *rt_genmask;	/* for generation of cloned routes */
	caddr_t	rt_llinfo;		/* pointer to link level info cache */
	struct	rt_metrics rt_rmx;	/* metrics used by rx'ing protocols */
	short	rt_idle;		/* easy to tell llayer still live */
};

/*
 * Following structure necessary for 4.3 compatibility;
 * We should eventually move it to a compat file.
 */
struct ortentry {
	u_long	rt_hash;		/* to speed lookups */
	struct	sockaddr rt_dst;	/* key */
	struct	sockaddr rt_gateway;	/* value */
	short	rt_flags;		/* up/down?, host/net */
	short	rt_refcnt;		/* # held references */
	u_long	rt_use;			/* raw # packets forwarded */
	struct	ifnet *rt_ifp;		/* the answer: interface to use */
};

#define	RTF_UP		0x1		/* route useable */
#define	RTF_GATEWAY	0x2		/* destination is a gateway */
#define	RTF_HOST	0x4		/* host entry (net otherwise) */
#define	RTF_REJECT	0x8		/* host or net unreachable */
#define	RTF_DYNAMIC	0x10		/* created dynamically (by redirect) */
#define	RTF_MODIFIED	0x20		/* modified dynamically (by redirect) */
#define RTF_DONE	0x40		/* message confirmed */
#define RTF_MASK	0x80		/* subnet mask present */
#define RTF_CLONING	0x100		/* generate new routes on use */
#define RTF_XRESOLVE	0x200		/* external daemon resolves name */
#define RTF_LLINFO	0x400		/* generated by ARP or ESIS */
#define RTF_PROTO2	0x4000		/* protocol specific routing flag */
#define RTF_PROTO1	0x8000		/* protocol specific routing flag */


/*
 * Routing statistics.
 */
struct	rtstat {
	short	rts_badredirect;	/* bogus redirect calls */
	short	rts_dynamic;		/* routes created by redirects */
	short	rts_newgateway;		/* routes modified by redirects */
	short	rts_unreach;		/* lookups which failed */
	short	rts_wildcard;		/* lookups satisfied by a wildcard */
};
/*
 * Structures for routing messages.
 */
struct rt_msghdr {
	u_short	rtm_msglen;	/* to skip over non-understood messages */
	u_char	rtm_version;	/* future binary compatability */
	u_char	rtm_type;	/* message type */
	u_short	rtm_index;	/* index for associated ifp */
	pid_t	rtm_pid;	/* identify sender */
	int	rtm_addrs;	/* bitmask identifying sockaddrs in msg */
	int	rtm_seq;	/* for sender to identify action */
	int	rtm_errno;	/* why failed */
	int	rtm_flags;	/* flags, incl. kern & message, e.g. DONE */
	int	rtm_use;	/* from rtentry */
	u_long	rtm_inits;	/* which metrics we are initializing */
	struct	rt_metrics rtm_rmx; /* metrics themselves */
};

struct route_cb {
	int	ip_count;
	int	ns_count;
	int	iso_count;
	int	any_count;
};

#define RTM_VERSION	2	/* Up the ante and ignore older versions */

#define RTM_ADD		0x1	/* Add Route */
#define RTM_DELETE	0x2	/* Delete Route */
#define RTM_CHANGE	0x3	/* Change Metrics or flags */
#define RTM_GET		0x4	/* Report Metrics */
#define RTM_LOSING	0x5	/* Kernel Suspects Partitioning */
#define RTM_REDIRECT	0x6	/* Told to use different route */
#define RTM_MISS	0x7	/* Lookup failed on this address */
#define RTM_LOCK	0x8	/* fix specified metrics */
#define RTM_OLDADD	0x9	/* caused by SIOCADDRT */
#define RTM_OLDDEL	0xa	/* caused by SIOCDELRT */
#define RTM_RESOLVE	0xb	/* req to resolve dst to LL addr */

#define RTV_MTU		0x1	/* init or lock _mtu */
#define RTV_HOPCOUNT	0x2	/* init or lock _hopcount */
#define RTV_EXPIRE	0x4	/* init or lock _hopcount */
#define RTV_RPIPE	0x8	/* init or lock _recvpipe */
#define RTV_SPIPE	0x10	/* init or lock _sendpipe */
#define RTV_SSTHRESH	0x20	/* init or lock _ssthresh */
#define RTV_RTT		0x40	/* init or lock _rtt */
#define RTV_RTTVAR	0x80	/* init or lock _rttvar */

#define RTA_DST		0x1	/* destination sockaddr present */
#define RTA_GATEWAY	0x2	/* gateway sockaddr present */
#define RTA_NETMASK	0x4	/* netmask sockaddr present */
#define RTA_GENMASK	0x8	/* cloning mask sockaddr present */
#define RTA_IFP		0x10	/* interface name sockaddr present */
#define RTA_IFA		0x20	/* interface addr sockaddr present */
#define RTA_AUTHOR	0x40	/* sockaddr for author of redirect */

#ifdef KERNEL
#ifndef _ROUTE_PROTOTYPES
/* private route functions, accessed via external symbol stub when loaded */
static int rtinit(struct ifaddr *ifa, int cmd, int flags);
static void rtalloc(struct route *ro);
static void rtfree(struct rtentry *rt);
static int rtrequest(int req, struct sockaddr *dst,
	struct sockaddr *gateway, struct sockaddr *netmask,
	int flags, struct rtentry **ret_nrt);
static void rtredirect(struct sockaddr *dst, struct sockaddr *gateway,
	struct sockaddr *netmask, int flags, struct sockaddr *src,
    	struct rtentry **rtp);
static void rtmissmsg(int type, struct sockaddr *dst,
	struct sockaddr *gate, struct sockaddr *mask,
	struct sockaddr *src, int flags, int error);
static int rtioctl(int req, caddr_t data, struct proc *p);

/* loopback interface */
static int looutput(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
	struct rtentry *rt);

/* inline external symbol table function stubs */
#include "esym.h"
extern inline int
rtinit(struct ifaddr *ifa, int cmd, int flags) {
	int (*f)(struct ifaddr *, int, int);

	(const void *) f = esym_fetch(rtinit);
	if (f == 0) {
		ifa->ifa_rt = 0;
		return (0);
	}
	return ((*f)(ifa, cmd, flags));
}

extern inline void
rtalloc(struct route *ro)
{
	void (*f)(struct route *);

	(const void *) f = esym_fetch(rtalloc);
	if (f == 0)
		ro->ro_rt = 0;
	else
		(*f)(ro);
}

extern inline void
rtfree(struct rtentry *rt)
{
	void (*f)(struct rtentry *);
	/*extern void panic(const char *);*/

	(const void *) f = esym_fetch(rtfree);
	if (f == 0)
		panic("no rtfree");
	else
		(*f)(rt);
}

extern inline void
rtredirect(struct sockaddr *dst, struct sockaddr *gateway,
    struct sockaddr *netmask, int flags, struct sockaddr *src,
    struct rtentry **rtp)
{
	void (*f) (struct sockaddr *, struct sockaddr *, struct sockaddr *,
		int, struct sockaddr *, struct rtentry **);
	/*extern void panic(const char *);*/

	(const void *) f = esym_fetch(rtredirect);
	if (f == 0)
		panic("no rtredirect");
	else
		(*f)(dst, gateway, netmask, flags, src, rtp);
}

extern inline int
rtioctl(int req, caddr_t data, struct proc *p)
{
	int (*f)(int, caddr_t , struct proc *);

	(const void *) f = esym_fetch(rtioctl);
	if (f == 0)
		return (EOPNOTSUPP);
	else
		return ((*f)(req, data, p));
}

extern inline int
rtrequest(int req, struct sockaddr *dst, struct sockaddr *gateway,
   struct sockaddr *netmask, int flags, struct rtentry **ret_nrt)
{
	int (*f)(int, struct sockaddr *, struct sockaddr *,
		struct sockaddr *, int, struct rtentry **);

	(const void *) f = esym_fetch(rtrequest);
	if (f == 0)
		return (EINVAL);
	else
		return ((*f)(req, dst, gateway, netmask, flags, ret_nrt));
}

extern inline void
rtmissmsg(int type, struct sockaddr *dst, struct sockaddr *gate,
    struct sockaddr *mask, struct sockaddr *src, int flags, int error)
{
	void (*f) (int, struct sockaddr *, struct sockaddr *,
		struct sockaddr *, struct sockaddr *, int, int);
	/*extern void panic(const char *);*/

	(const void *) f = esym_fetch(rtmissmsg);
	if (f == 0)
		panic("no rtmissmsg");
	else
		(*f)(type, dst, gate, mask, src, flags, error);
}

extern inline int
looutput(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
	struct rtentry *rt)
{
	int (*f)(struct ifnet *, struct mbuf *, struct sockaddr *,
		struct rtentry *);

	(const void *) f = esym_fetch(looutput);
	if (f == 0)
		return (EOPNOTSUPP);
	else
		return ((*f)(ifp, m, dst, rt));
}

#define	route_cb	*(struct route_cb *)esym_fetch(route_cb)
#else
extern int rtinit(struct ifaddr *ifa, int cmd, int flags);
extern void rtalloc(struct route *ro);
extern void rtfree(struct rtentry *rt);
extern int rtrequest(int req, struct sockaddr *dst,
	struct sockaddr *gateway, struct sockaddr *netmask,
	int flags, struct rtentry **ret_nrt);
extern void rtredirect(struct sockaddr *dst, struct sockaddr *gateway,
	struct sockaddr *netmask, int flags, struct sockaddr *src,
    	struct rtentry **rtp);
extern void rtmissmsg(int type, struct sockaddr *dst,
	struct sockaddr *gate, struct sockaddr *mask,
	struct sockaddr *src, int flags, int error);
extern int rtioctl(int req, caddr_t data, struct proc *p);

extern struct route_cb route_cb;
#endif /* _ROUTE_PROTOTYPES */


#define	RTFREE_(rt) \
	if ((rt)->rt_refcnt <= 1) \
		rtfree(rt); \
	else \
		(rt)->rt_refcnt--;

#ifdef	GATEWAY
#define	RTHASHSIZ	64
#else
#define	RTHASHSIZ	8
#endif
#if	(RTHASHSIZ & (RTHASHSIZ - 1)) == 0
#define RTHASHMOD(h)	((h) & (RTHASHSIZ - 1))
#else
#define RTHASHMOD(h)	((h) % RTHASHSIZ)
#endif
extern struct	mbuf *rthost[RTHASHSIZ];
extern struct	mbuf *rtnet[RTHASHSIZ];
extern struct	rtstat	rtstat;
#endif
