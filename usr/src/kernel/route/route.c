/*
 * Copyright (c) 1980, 1986, 1991 Regents of the University of California.
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
 * $Id: route.c,v 1.1 94/10/20 10:55:41 root Exp Locker: bill $
 */
#include "sys/param.h"
#include "sys/file.h"
#include "sys/ioctl.h"
#include "sys/errno.h"
#include "privilege.h"
#include "proc.h"
#include "mbuf.h"
#include "socketvar.h"
#include "domain.h"
#include "protosw.h"
#include "modconfig.h"
#include "prototypes.h"
#include "esym.h"

#include "if.h"
#include "af.h"
#define	_ROUTE_PROTOTYPES
#include "route.h"
#undef	_ROUTE_PROTOTYPES
#include "raw_cb.h"

#include "in.h"
#include "in_var.h"

#ifdef NS
#include "ns.h"
#endif
#include "netisr.h"

#define	SA(p) ((struct sockaddr *)(p))

int rt_maskedcopy(struct sockaddr *src, struct sockaddr *dst,
	struct sockaddr *netmask);

int	rttrash;		/* routes not in table but not freed */
struct	sockaddr wildcard;	/* zero valued cookie for wildcard searches */
int	rthashsize = RTHASHSIZ;	/* for netstat, etc. */
struct rtstat rtstat;

static int rtinits_done = 0;
struct radix_node_head *ns_rnhead, *in_rnhead;
struct radix_node *rn_match(), *rn_delete(), *rn_addroute();

/*extern void
rt_missmsg(int type, struct sockaddr *dst, struct sockaddr *gate,
	struct sockaddr *mask, struct sockaddr *src, int flags, int error) asm("rtmissmsg");*/


rtinitheads()
{
	if (rtinits_done == 0 &&
#ifdef NS
	    rn_inithead(&ns_rnhead, 16, AF_NS) &&
#endif
	    rn_inithead(&in_rnhead, 32, AF_INET))
		rtinits_done = 1;
}

struct rtentry *rtalloc1(struct sockaddr *dst, int report);

/*
 * Packet routing routines.
 */
void
rtalloc(struct route *ro)
{
	if (ro->ro_rt && ro->ro_rt->rt_ifp && (ro->ro_rt->rt_flags & RTF_UP))
		return;				 /* XXX */
	ro->ro_rt = rtalloc1(&ro->ro_dst, 0);
}

struct rtentry *
rtalloc1(struct sockaddr *dst, int report)
{
	register struct radix_node_head *rnh;
	register struct rtentry *rt;
	register struct radix_node *rn;
	struct rtentry *newrt = 0;
	int  s = splnet(), err = 0, msgtype = RTM_MISS;

	for (rnh = radix_node_head; rnh && (dst->sa_family != rnh->rnh_af); )
		rnh = rnh->rnh_next;
	if (rnh && rnh->rnh_treetop &&
	    (rn = rn_match((caddr_t)dst, rnh->rnh_treetop)) &&
	    ((rn->rn_flags & RNF_ROOT) == 0)) {
		newrt = rt = (struct rtentry *)rn;
		if (report && (rt->rt_flags & RTF_CLONING)) {
			if ((err = rtrequest(RTM_RESOLVE, dst, SA(0),
					      SA(0), 0, &newrt)) ||
			    ((rt->rt_flags & RTF_XRESOLVE)
			      && (msgtype = RTM_RESOLVE))) /* intended! */
			    goto miss;
		} else
			rt->rt_refcnt++;
	} else {
		rtstat.rts_unreach++;
	miss:	if (report)
			rtmissmsg(msgtype, dst, SA(0), SA(0), SA(0), 0, err);
	}
	splx(s);
	return (newrt);
}

void
rtfree(struct rtentry *rt)
{
	struct ifaddr *ifa;

	if (rt == 0)
		panic("rtfree");

	rt->rt_refcnt--;
	if (rt->rt_refcnt <= 0 && (rt->rt_flags & RTF_UP) == 0) {
		rttrash--;
		if (rt->rt_nodes->rn_flags & (RNF_ACTIVE | RNF_ROOT))
			panic ("rtfree 2");
		free((caddr_t)rt, M_RTABLE);
	}
}

/*
 * Force a routing table entry to the specified
 * destination to go through the given gateway.
 * Normally called as a result of a routing redirect
 * message from the network layer.
 *
 * N.B.: must be called at splnet
 *
 */
void
rtredirect(struct sockaddr *dst, struct sockaddr *gateway,
    struct sockaddr *netmask, int flags, struct sockaddr *src,
    struct rtentry **rtp)
{
	struct rtentry *rt = 0;
	int error = 0;
	short *stat = 0;

	/* verify the gateway is directly reachable */
	if (ifa_ifwithnet(gateway) == 0) {
		error = ENETUNREACH;
		goto done;
	}
	rt = rtalloc1(dst, 0);
	/*
	 * If the redirect isn't from our current router for this dst,
	 * it's either old or wrong.  If it redirects us to ourselves,
	 * we have a routing loop, perhaps as a result of an interface
	 * going down recently.
	 */
#define	equal(a1, a2) (memcmp((caddr_t)(a1), (caddr_t)(a2), (a1)->sa_len) == 0)
	if (!(flags & RTF_DONE) && rt && !equal(src, rt->rt_gateway))
		error = EINVAL;
	else if (ifa_ifwithaddr(gateway))
		error = EHOSTUNREACH;
	if (error)
		goto done;
	/*
	 * Create a new entry if we just got back a wildcard entry
	 * or the the lookup failed.  This is necessary for hosts
	 * which use routing redirects generated by smart gateways
	 * to dynamically build the routing tables.
	 */
	if ((rt == 0) || (rt_mask(rt) && rt_mask(rt)->sa_len < 2))
		goto create;
	/*
	 * Don't listen to the redirect if it's
	 * for a route to an interface. 
	 */
	if (rt->rt_flags & RTF_GATEWAY) {
		if (((rt->rt_flags & RTF_HOST) == 0) && (flags & RTF_HOST)) {
			/*
			 * Changing from route to net => route to host.
			 * Create new route, rather than smashing route to net.
			 */
		create:
			flags |=  RTF_GATEWAY | RTF_DYNAMIC;
			error = rtrequest((int)RTM_ADD, dst, gateway,
				    SA(0), flags,
				    (struct rtentry **)0);
			stat = &rtstat.rts_dynamic;
		} else {
			/*
			 * Smash the current notion of the gateway to
			 * this destination.  Should check about netmask!!!
			 */
			if (gateway->sa_len <= rt->rt_gateway->sa_len) {
				Bcopy(gateway, rt->rt_gateway, gateway->sa_len);
				rt->rt_flags |= RTF_MODIFIED;
				flags |= RTF_MODIFIED;
				stat = &rtstat.rts_newgateway;
			} else
				error = ENOSPC;
		}
	} else
		error = EHOSTUNREACH;
done:
	if (rt) {
		if (rtp && !error)
			*rtp = rt;
		else
			rtfree(rt);
	}
	if (error)
		rtstat.rts_badredirect++;
	else
		(stat && (*stat)++);
	rtmissmsg(RTM_REDIRECT, dst, gateway, netmask, src, flags, error);
}

/*
* Routing table ioctl interface.
*/
int
rtioctl(int req, caddr_t data, struct proc *p)
{
#ifndef COMPAT_43
	return (EOPNOTSUPP);
#else
	struct ortentry *entry = (struct ortentry *)data;
	int error;
	struct sockaddr *netmask = 0;

	if (req == SIOCADDRT)
		req = RTM_ADD;
	else if (req == SIOCDELRT)
		req = RTM_DELETE;
	else
		return (EINVAL);

	if (error = use_priv(p->p_ucred, PRV_RTIOCTL, p))
		return (error);
#if BYTE_ORDER != BIG_ENDIAN
	if (entry->rt_dst.sa_family == 0 && entry->rt_dst.sa_len < 16) {
		entry->rt_dst.sa_family = entry->rt_dst.sa_len;
		entry->rt_dst.sa_len = 16;
	}
	if (entry->rt_gateway.sa_family == 0 && entry->rt_gateway.sa_len < 16) {
		entry->rt_gateway.sa_family = entry->rt_gateway.sa_len;
		entry->rt_gateway.sa_len = 16;
	}
#else
	if (entry->rt_dst.sa_len == 0)
		entry->rt_dst.sa_len = 16;
	if (entry->rt_gateway.sa_len == 0)
		entry->rt_gateway.sa_len = 16;
#endif
	if ((entry->rt_flags & RTF_HOST) == 0)
		switch (entry->rt_dst.sa_family) {
#ifdef INET
		case AF_INET:
			{
				struct sockaddr_in icmpmask;
				struct sockaddr_in *dst_in = 
					(struct sockaddr_in *)&entry->rt_dst;

				in_sockmaskof(dst_in->sin_addr, &icmpmask);
				netmask = (struct sockaddr *)&icmpmask;
			}
			break;
#endif
#ifdef NS
		case AF_NS:
			{
				extern struct sockaddr_ns ns_netmask;
				netmask = (struct sockaddr *)&ns_netmask;
			}
#endif
		}
	error =  rtrequest(req, &(entry->rt_dst), &(entry->rt_gateway), netmask,
				entry->rt_flags, (struct rtentry **)0);
	rtmissmsg((req == RTM_ADD ? RTM_OLDADD : RTM_OLDDEL),
		   &(entry->rt_dst), &(entry->rt_gateway),
		   netmask, SA(0), entry->rt_flags, error);
	return (error);
#endif
}

/* find the interface to use with a destination or gateway, otherwise return 0 */
struct ifaddr *
ifa_ifwithroute(int flags, struct sockaddr *dst, struct sockaddr *gateway)
{
	struct ifaddr *ifa;

	if ((flags & RTF_GATEWAY) == 0) {
		/*
		 * If we are adding a route to an interface,
		 * and the interface is a pt to pt link
		 * we should search for the destination
		 * as our clue to the interface.  Otherwise
		 * we can use the local address.
		 */
		ifa = 0;
		if (flags & RTF_HOST) 
			ifa = ifa_ifwithdstaddr(dst);
		if (ifa == 0)
			ifa = ifa_ifwithaddr(gateway);
	} else {
		/*
		 * If we are adding a route to a remote net
		 * or host, the gateway may still be on the
		 * other end of a pt to pt link.
		 */
		ifa = ifa_ifwithdstaddr(gateway);
	}
	if (ifa == 0)
		ifa = ifa_ifwithnet(gateway);
	if (ifa == 0) {
		struct rtentry *rt = rtalloc1(dst, 0);
		if (rt == 0)
			return (0);
		rt->rt_refcnt--;
		if ((ifa = rt->rt_ifa) == 0)
			return (0);
	}
	if (ifa->ifa_addr->sa_family != dst->sa_family) {
		struct ifaddr *oifa = ifa, *ifaof_ifpforaddr();
		ifa = ifaof_ifpforaddr(dst, ifa->ifa_ifp);
		if (ifa == 0)
			ifa = oifa;
	}
	return (ifa);
}

#define ROUNDUP(a) (a>0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))

int
rtrequest(int req, struct sockaddr *dst, struct sockaddr *gateway,
   struct sockaddr *netmask, int flags, struct rtentry **ret_nrt)
{
	int s = splnet(), len, error = 0;
	struct rtentry *rt;
	struct radix_node *rn;
	struct radix_node_head *rnh;
	struct ifaddr *ifa, *ifa_ifwithdstaddr();
	struct sockaddr *ndst;
	u_char af = dst->sa_family;
#define senderr(x) { error = x ; goto bad; }

	if (rtinits_done == 0)
		rtinitheads();
	for (rnh = radix_node_head; rnh && (af != rnh->rnh_af); )
		rnh = rnh->rnh_next;
	if (rnh == 0)
		senderr(ESRCH);
	if (flags & RTF_HOST)
		netmask = 0;
	switch (req) {
	case RTM_DELETE:
		if (ret_nrt && (rt = *ret_nrt)) {
			RTFREE_(rt);
			*ret_nrt = 0;
		}
		if ((rn = rn_delete((caddr_t)dst, (caddr_t)netmask, 
					rnh->rnh_treetop)) == 0)
			senderr(ESRCH);
		if (rn->rn_flags & (RNF_ACTIVE | RNF_ROOT))
			panic ("rtrequest delete");
		rt = (struct rtentry *)rn;
		rt->rt_flags &= ~RTF_UP;
		if ((ifa = rt->rt_ifa) && ifa->ifa_rtrequest)
			ifa->ifa_rtrequest(RTM_DELETE, rt, SA(0));
		rttrash++;
		if (rt->rt_refcnt <= 0)
			rtfree(rt);
		break;

	case RTM_RESOLVE:
		if (ret_nrt == 0 || (rt = *ret_nrt) == 0)
			senderr(EINVAL);
		ifa = rt->rt_ifa;
		flags = rt->rt_flags & ~RTF_CLONING;
		gateway = rt->rt_gateway;
		if ((netmask = rt->rt_genmask) == 0)
			flags |= RTF_HOST;
		goto makeroute;

	case RTM_ADD:
		if ((ifa = ifa_ifwithroute(flags, dst, gateway)) == 0)
			senderr(ENETUNREACH);
	makeroute:
		len = sizeof (*rt) + ROUNDUP(gateway->sa_len)
		    + ROUNDUP(dst->sa_len);
		R_Malloc(rt, struct rtentry *, len);
		if (rt == 0)
			senderr(ENOBUFS);
		Bzero(rt, len);
		ndst = (struct sockaddr *)(rt + 1);
		if (netmask) {
			rt_maskedcopy(dst, ndst, netmask);
		} else
			Bcopy(dst, ndst, dst->sa_len);
		rn = rn_addroute((caddr_t)ndst, (caddr_t)netmask,
					rnh->rnh_treetop, rt->rt_nodes);
		if (rn == 0) {
			free((caddr_t)rt, M_RTABLE);
			senderr(EEXIST);
		}
		rt->rt_ifa = ifa;
		rt->rt_ifp = ifa->ifa_ifp;
		rt->rt_flags = RTF_UP | flags;
		rt->rt_gateway = (struct sockaddr *)
					(rn->rn_key + ROUNDUP(dst->sa_len));
		Bcopy(gateway, rt->rt_gateway, gateway->sa_len);
		if (req == RTM_RESOLVE)
			rt->rt_rmx = (*ret_nrt)->rt_rmx; /* copy metrics */
		if (ifa->ifa_rtrequest)
			ifa->ifa_rtrequest(req, rt, SA(ret_nrt ? *ret_nrt : 0));
		if (ret_nrt) {
			*ret_nrt = rt;
			rt->rt_refcnt++;
		}
		break;
	}
bad:
	splx(s);
	return (error);
}

int
rt_maskedcopy(struct sockaddr *src, struct sockaddr *dst,
	struct sockaddr *netmask)
{
	register u_char *cp1 = (u_char *)src;
	register u_char *cp2 = (u_char *)dst;
	register u_char *cp3 = (u_char *)netmask;
	u_char *cplim = cp2 + *cp3;
	u_char *cplim2 = cp2 + *cp1;

	*cp2++ = *cp1++; *cp2++ = *cp1++; /* copies sa_len & sa_family */
	cp3 += 2;
	if (cplim > cplim2)
		cplim = cplim2;
	while (cp2 < cplim)
		*cp2++ = *cp1++ & *cp3++;
	if (cp2 < cplim2)
		(void) memset((caddr_t)cp2, 0, (unsigned)(cplim2 - cp2));
}
/*
 * Set up a routing table entry, normally
 * for an interface.
 */
int
rtinit(struct ifaddr *ifa, int cmd, int flags)
{
	struct rtentry *rt;
	struct sockaddr *dst, *deldst;
	struct mbuf *m = 0;
	int error;

	dst = flags & RTF_HOST ? ifa->ifa_dstaddr : ifa->ifa_addr;
	if (ifa->ifa_flags & IFA_ROUTE) {
		if ((rt = ifa->ifa_rt) && (rt->rt_flags & RTF_UP) == 0) {
			RTFREE_(rt);
			ifa->ifa_rt = 0;
		}
	}
	if (cmd == RTM_DELETE) {
		if ((flags & RTF_HOST) == 0 && ifa->ifa_netmask) {
			m = m_get(M_WAIT, MT_SONAME);
			deldst = mtod(m, struct sockaddr *);
			rt_maskedcopy(dst, deldst, ifa->ifa_netmask);
			dst = deldst;
		}
		if (rt = rtalloc1(dst, 0)) {
			rt->rt_refcnt--;
			if (rt->rt_ifa != ifa) {
				if (m)
					(void) m_free(m);
				return (flags & RTF_HOST ? EHOSTUNREACH
							: ENETUNREACH);
			}
		}
	}
	error = rtrequest(cmd, dst, ifa->ifa_addr, ifa->ifa_netmask,
			flags | ifa->ifa_flags, &ifa->ifa_rt);
	if (m)
		(void) m_free(m);
	if (cmd == RTM_ADD && error == 0 && (rt = ifa->ifa_rt)
						&& rt->rt_ifa != ifa) {
		rt->rt_ifa = ifa;
		rt->rt_ifp = ifa->ifa_ifp;
	}
	return (error);
}

/*static struct route_ops router = {
	"radix",
	rtinit, rtalloc, rtfree,
	rtrequest, rtredirect, rt_missmsg, rtioctl
}; */

KERNEL_MODCONFIG() {

	/*_router_ = router;*/
	esym_bind(rtinit);
	esym_bind(rtalloc);
	esym_bind(rtfree);
	esym_bind(rtredirect);
	esym_bind(rtmissmsg);
	esym_bind(rtioctl);
}
