/*
 * Copyright (c) 1982, 1986, 1989 Regents of the University of California.
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
 *	$Id: ip_icmp.c,v 1.1 94/10/20 10:53:32 root Exp $
 */

#include "sys/param.h"
#include "sys/errno.h"
#include "systm.h"
#include "malloc.h"
#include "mbuf.h"
#include "protosw.h"
#include "sys/socket.h"
#include "sys/time.h"
#include "kernel.h"
#include "prototypes.h"

#include "route.h"
#include "if.h"

#include "in.h"
#include "in_systm.h"
#include "in_var.h"
#include "ip.h"
#include "ip_icmp.h"
#include "icmp_var.h"

/*
 * ICMP routines: error generation, receive packet processing, and
 * routines to turnaround packets back to the originator, and
 * host table maintenance routines.
 */
#ifdef ICMPPRINTFS
int	icmpprintfs = 0;
#endif

extern	struct protosw inetsw[];

/*
 * Generate an error packet of type error
 * in response to bad packet ip.
 */
/*VARARGS3*/
icmp_error(n, type, code, dest)
	struct mbuf *n;
	int type, code;
	struct in_addr dest;
{
	register struct ip *oip = mtod(n, struct ip *), *nip;
	register unsigned oiplen = oip->ip_hl << 2;
	register struct icmp *icp;
	register struct mbuf *m;
	unsigned icmplen;

#ifdef ICMPPRINTFS
	if (icmpprintfs)
		printf("icmp_error(%x, %d, %d)\n", oip, type, code);
#endif
	if (type != ICMP_REDIRECT)
		icmpstat.icps_error++;
	/*
	 * Don't send error if not the first fragment of message.
	 * Don't error if the old packet protocol was ICMP
	 * error message, only known informational types.
	 */
	if (oip->ip_off &~ (IP_MF|IP_DF))
		goto freeit;
	if (oip->ip_p == IPPROTO_ICMP && type != ICMP_REDIRECT &&
	  n->m_len >= oiplen + ICMP_MINLEN &&
	  !ICMP_INFOTYPE(((struct icmp *)((caddr_t)oip + oiplen))->icmp_type)) {
		icmpstat.icps_oldicmp++;
		goto freeit;
	}

	/*
	 * First, formulate icmp message
	 */
	m = m_gethdr(M_DONTWAIT, MT_HEADER);
	if (m == NULL)
		goto freeit;
	icmplen = oiplen + min(8, oip->ip_len);
	m->m_len = icmplen + ICMP_MINLEN;
	MH_ALIGN(m, m->m_len);
	icp = mtod(m, struct icmp *);
	if ((u_int)type > ICMP_MAXTYPE)
		panic("icmp_error");
	icmpstat.icps_outhist[type]++;
	icp->icmp_type = type;
	if (type == ICMP_REDIRECT)
		icp->icmp_gwaddr = dest;
	else
		icp->icmp_void = 0;
	if (type == ICMP_PARAMPROB) {
		icp->icmp_pptr = code;
		code = 0;
	}
	icp->icmp_code = code;
	memcpy((caddr_t)&icp->icmp_ip, (caddr_t)oip, icmplen);
	nip = &icp->icmp_ip;
	nip->ip_len = htons((u_short)(nip->ip_len + oiplen));

	/*
	 * Now, copy old ip header (without options)
	 * in front of icmp message.
	 */
	if (m->m_data - sizeof(struct ip) < m->m_pktdat)
		panic("icmp len");
	m->m_data -= sizeof(struct ip);
	m->m_len += sizeof(struct ip);
	m->m_pkthdr.len = m->m_len;
	m->m_pkthdr.rcvif = n->m_pkthdr.rcvif;
	nip = mtod(m, struct ip *);
	memcpy((caddr_t)nip, (caddr_t)oip, oiplen);
	nip->ip_len = m->m_len;
	nip->ip_hl = sizeof(struct ip) >> 2;
	nip->ip_p = IPPROTO_ICMP;
	icmp_reflect(m);

freeit:
	m_freem(n);
}

static struct sockproto icmproto = { AF_INET, IPPROTO_ICMP };
static struct sockaddr_in icmpsrc = { sizeof (struct sockaddr_in), AF_INET };
static struct sockaddr_in icmpdst = { sizeof (struct sockaddr_in), AF_INET };
static struct sockaddr_in icmpgw = { sizeof (struct sockaddr_in), AF_INET };
struct sockaddr_in icmpmask = { 8, 0 };
struct in_ifaddr *ifptoia();

/*
 * Process a received ICMP message.
 */
icmp_input(m, hlen)
	register struct mbuf *m;
	int hlen;
{
	register struct icmp *icp;
	register struct ip *ip = mtod(m, struct ip *);
	int icmplen = ip->ip_len;
	register int i;
	struct in_ifaddr *ia;
	int (*ctlfunc)(), code;
	extern u_char ip_protox[];
	extern struct in_addr in_makeaddr();

	/*
	 * Locate icmp structure in mbuf, and check
	 * that not corrupted and of at least minimum length.
	 */
#ifdef ICMPPRINTFS
	if (icmpprintfs)
		printf("icmp_input from %x, len %d\n", ip->ip_src, icmplen);
#endif
	if (icmplen < ICMP_MINLEN) {
		icmpstat.icps_tooshort++;
		goto freeit;
	}
	i = hlen + min(icmplen, ICMP_ADVLENMIN);
 	if (m->m_len < i && (m = m_pullup(m, i)) == 0)  {
		icmpstat.icps_tooshort++;
		return;
	}
 	ip = mtod(m, struct ip *);
	m->m_len -= hlen;
	m->m_data += hlen;
	icp = mtod(m, struct icmp *);
	if (in_cksum(m, icmplen)) {
		icmpstat.icps_checksum++;
		goto freeit;
	}
	m->m_len += hlen;
	m->m_data -= hlen;

#ifdef ICMPPRINTFS
	/*
	 * Message type specific processing.
	 */
	if (icmpprintfs)
		printf("icmp_input, type %d code %d\n", icp->icmp_type,
		    icp->icmp_code);
#endif
	if (icp->icmp_type > ICMP_MAXTYPE)
		goto raw;
	icmpstat.icps_inhist[icp->icmp_type]++;
	code = icp->icmp_code;
	switch (icp->icmp_type) {

	case ICMP_UNREACH:
		if (code > 5)
			goto badcode;
		code += PRC_UNREACH_NET;
		goto deliver;

	case ICMP_TIMXCEED:
		if (code > 1)
			goto badcode;
		code += PRC_TIMXCEED_INTRANS;
		goto deliver;

	case ICMP_PARAMPROB:
		if (code)
			goto badcode;
		code = PRC_PARAMPROB;
		goto deliver;

	case ICMP_SOURCEQUENCH:
		if (code)
			goto badcode;
		code = PRC_QUENCH;
	deliver:
		/*
		 * Problem with datagram; advise higher level routines.
		 */
		if (icmplen < ICMP_ADVLENMIN || icmplen < ICMP_ADVLEN(icp) ||
		    icp->icmp_ip.ip_hl < (sizeof(struct ip) >> 2)) {
			icmpstat.icps_badlen++;
			goto freeit;
		}
		NTOHS(icp->icmp_ip.ip_len);
#ifdef ICMPPRINTFS
		if (icmpprintfs)
			printf("deliver to protocol %d\n", icp->icmp_ip.ip_p);
#endif
		icmpsrc.sin_addr = icp->icmp_ip.ip_dst;
		if (ctlfunc = inetsw[ip_protox[icp->icmp_ip.ip_p]].pr_ctlinput)
			(*ctlfunc)(code, (struct sockaddr *)&icmpsrc,
			    (caddr_t) &icp->icmp_ip);
		break;

	badcode:
		icmpstat.icps_badcode++;
		break;

	case ICMP_ECHO:
		icp->icmp_type = ICMP_ECHOREPLY;
		goto reflect;

	case ICMP_TSTAMP:
		if (icmplen < ICMP_TSLEN) {
			icmpstat.icps_badlen++;
			break;
		}
		icp->icmp_type = ICMP_TSTAMPREPLY;
		icp->icmp_rtime = iptime();
		icp->icmp_ttime = icp->icmp_rtime;	/* bogus, do later! */
		goto reflect;
		
	case ICMP_IREQ:
#define	satosin(sa)	((struct sockaddr_in *)(sa))
		if (in_netof(ip->ip_src) == 0 &&
		    (ia = ifptoia(m->m_pkthdr.rcvif)))
			ip->ip_src = in_makeaddr(in_netof(IA_SIN(ia)->sin_addr),
			    in_lnaof(ip->ip_src));
		icp->icmp_type = ICMP_IREQREPLY;
		goto reflect;

	case ICMP_MASKREQ:
		if (icmplen < ICMP_MASKLEN ||
		    (ia = ifptoia(m->m_pkthdr.rcvif)) == 0)
			break;
		icp->icmp_type = ICMP_MASKREPLY;
		icp->icmp_mask = ia->ia_sockmask.sin_addr.s_addr;
		if (ip->ip_src.s_addr == 0) {
			if (ia->ia_ifp->if_flags & IFF_BROADCAST)
			    ip->ip_src = satosin(&ia->ia_broadaddr)->sin_addr;
			else if (ia->ia_ifp->if_flags & IFF_POINTOPOINT)
			    ip->ip_src = satosin(&ia->ia_dstaddr)->sin_addr;
		}
reflect:
		ip->ip_len += hlen;	/* since ip_input deducts this */
		icmpstat.icps_reflect++;
		icmpstat.icps_outhist[icp->icmp_type]++;
		icmp_reflect(m);
		return;

	case ICMP_REDIRECT:
		if (icmplen < ICMP_ADVLENMIN || icmplen < ICMP_ADVLEN(icp)) {
			icmpstat.icps_badlen++;
			break;
		}
		/*
		 * Short circuit routing redirects to force
		 * immediate change in the kernel's routing
		 * tables.  The message is also handed to anyone
		 * listening on a raw socket (e.g. the routing
		 * daemon for use in updating its tables).
		 */
		icmpgw.sin_addr = ip->ip_src;
		icmpdst.sin_addr = icp->icmp_gwaddr;
#ifdef	ICMPPRINTFS
		if (icmpprintfs)
			printf("redirect dst %x to %x\n", icp->icmp_ip.ip_dst,
				icp->icmp_gwaddr);
#endif
		if (code == ICMP_REDIRECT_NET || code == ICMP_REDIRECT_TOSNET) {
			u_long in_netof();
			icmpsrc.sin_addr =
			 in_makeaddr(in_netof(icp->icmp_ip.ip_dst), INADDR_ANY);
			in_sockmaskof(icp->icmp_ip.ip_dst, &icmpmask);
			RTREDIRECT((struct sockaddr *)&icmpsrc,
			  (struct sockaddr *)&icmpdst,
			  (struct sockaddr *)&icmpmask, RTF_GATEWAY,
			  (struct sockaddr *)&icmpgw, (struct rtentry **)0);
			icmpsrc.sin_addr = icp->icmp_ip.ip_dst;
			pfctlinput(PRC_REDIRECT_NET,
			  (struct sockaddr *)&icmpsrc);
		} else {
			icmpsrc.sin_addr = icp->icmp_ip.ip_dst;
			RTREDIRECT((struct sockaddr *)&icmpsrc,
			  (struct sockaddr *)&icmpdst,
			  (struct sockaddr *)0, RTF_GATEWAY | RTF_HOST,
			  (struct sockaddr *)&icmpgw, (struct rtentry **)0);
			pfctlinput(PRC_REDIRECT_HOST,
			  (struct sockaddr *)&icmpsrc);
		}
		break;

	/*
	 * No kernel processing for the following;
	 * just fall through to send to raw listener.
	 */
	case ICMP_ECHOREPLY:
	case ICMP_TSTAMPREPLY:
	case ICMP_IREQREPLY:
	case ICMP_MASKREPLY:
	default:
		break;
	}

raw:
	icmpsrc.sin_addr = ip->ip_src;
	icmpdst.sin_addr = ip->ip_dst;
	(void) raw_input(m, &icmproto, (struct sockaddr *)&icmpsrc,
	    (struct sockaddr *)&icmpdst);
	return;

freeit:
	m_freem(m);
}

/*
 * Reflect the ip packet back to the source
 */
icmp_reflect(m)
	struct mbuf *m;
{
	register struct ip *ip = mtod(m, struct ip *);
	register struct in_ifaddr *ia;
	struct in_addr t;
	struct mbuf *opts = 0, *ip_srcroute();
	int optlen = (ip->ip_hl << 2) - sizeof(struct ip);

	t = ip->ip_dst;
	ip->ip_dst = ip->ip_src;
	/*
	 * If the incoming packet was addressed directly to us,
	 * use dst as the src for the reply.  Otherwise (broadcast
	 * or anonymous), use the address which corresponds
	 * to the incoming interface.
	 */
	for (ia = in_ifaddr; ia; ia = ia->ia_next) {
		if (t.s_addr == IA_SIN(ia)->sin_addr.s_addr)
			break;
		if ((ia->ia_ifp->if_flags & IFF_BROADCAST) &&
		    t.s_addr == satosin(&ia->ia_broadaddr)->sin_addr.s_addr)
			break;
	}
	if (ia == (struct in_ifaddr *)0)
		ia = ifptoia(m->m_pkthdr.rcvif);
	if (ia == (struct in_ifaddr *)0)
		ia = in_ifaddr;
	t = IA_SIN(ia)->sin_addr;
	ip->ip_src = t;
	ip->ip_ttl = MAXTTL;

	if (optlen > 0) {
		register u_char *cp;
		int opt, cnt;
		u_int len;

		/*
		 * Retrieve any source routing from the incoming packet;
		 * add on any record-route or timestamp options.
		 */
		cp = (u_char *) (ip + 1);
		if ((opts = ip_srcroute()) == 0 &&
		    (opts = m_gethdr(M_DONTWAIT, MT_HEADER))) {
			opts->m_len = sizeof(struct in_addr);
			mtod(opts, struct in_addr *)->s_addr = 0;
		}
		if (opts) {
#ifdef ICMPPRINTFS
		    if (icmpprintfs)
			    printf("icmp_reflect optlen %d rt %d => ",
				optlen, opts->m_len);
#endif
		    for (cnt = optlen; cnt > 0; cnt -= len, cp += len) {
			    opt = cp[IPOPT_OPTVAL];
			    if (opt == IPOPT_EOL)
				    break;
			    if (opt == IPOPT_NOP)
				    len = 1;
			    else {
				    len = cp[IPOPT_OLEN];
				    if (len <= 0 || len > cnt)
					    break;
			    }
			    /*
			     * should check for overflow, but it "can't happen"
			     */
			    if (opt == IPOPT_RR || opt == IPOPT_TS) {
				    memcpy(mtod(opts, caddr_t) + opts->m_len,
					(caddr_t)cp, len);
				    opts->m_len += len;
			    }
		    }
		    if (opts->m_len % 4 != 0) {
			    *(mtod(opts, caddr_t) + opts->m_len) = IPOPT_EOL;
			    opts->m_len++;
		    }
#ifdef ICMPPRINTFS
		    if (icmpprintfs)
			    printf("%d\n", opts->m_len);
#endif
		}
		/*
		 * Now strip out original options by copying rest of first
		 * mbuf's data back, and adjust the IP length.
		 */
		ip->ip_len -= optlen;
		ip->ip_hl = sizeof(struct ip) >> 2;
		m->m_len -= optlen;
		if (m->m_flags & M_PKTHDR)
			m->m_pkthdr.len -= optlen;
		optlen += sizeof(struct ip);
		memcpy((caddr_t)(ip + 1), (caddr_t)ip + optlen,
			 (unsigned)(m->m_len - sizeof(struct ip)));
	}
	icmp_send(m, opts);
	if (opts)
		(void)m_free(opts);
}

struct in_ifaddr *
ifptoia(ifp)
	struct ifnet *ifp;
{
	register struct in_ifaddr *ia;

	for (ia = in_ifaddr; ia; ia = ia->ia_next)
		if (ia->ia_ifp == ifp)
			return (ia);
	return ((struct in_ifaddr *)0);
}

/*
 * Send an icmp packet back to the ip level,
 * after supplying a checksum.
 */
icmp_send(m, opts)
	register struct mbuf *m;
	struct mbuf *opts;
{
	register struct ip *ip = mtod(m, struct ip *);
	register int hlen;
	register struct icmp *icp;

	hlen = ip->ip_hl << 2;
	m->m_data += hlen;
	m->m_len -= hlen;
	icp = mtod(m, struct icmp *);
	icp->icmp_cksum = 0;
	icp->icmp_cksum = in_cksum(m, ip->ip_len - hlen);
	m->m_data -= hlen;
	m->m_len += hlen;
#ifdef ICMPPRINTFS
	if (icmpprintfs)
		printf("icmp_send dst %x src %x\n", ip->ip_dst, ip->ip_src);
#endif
	(void) ip_output(m, opts, (struct route *)0, 0);
}

n_time
iptime()
{
	struct timeval atv;
	u_long t;

	microtime(&atv);
	t = (atv.tv_sec % (24*60*60)) * 1000 + atv.tv_usec / 1000;
	return (htonl(t));
}
