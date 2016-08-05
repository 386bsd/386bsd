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
 * $Id: $
 *
 * Ethernet Link Layer.
 */

/*
 * Structure of a 10Mb/s Ethernet header.
 */
struct	ether_header {
	u_char	ether_dhost[6];
	u_char	ether_shost[6];
	u_short	ether_type;
};

#define	ETHERTYPE_PUP	0x0200		/* PUP protocol */
#define	ETHERTYPE_IP	0x0800		/* IP protocol */
#define ETHERTYPE_ARP	0x0806		/* Addr. resolution protocol */

/*
 * The ETHERTYPE_NTRAILER packet types starting at ETHERTYPE_TRAIL have
 * (type-ETHERTYPE_TRAIL)*512 bytes of data followed
 * by an ETHER type (as given above) and then the (variable-length) header.
 */
#define	ETHERTYPE_TRAIL		0x1000		/* Trailer packet */
#define	ETHERTYPE_NTRAILER	16

#define	ETHERMTU	1500
#define	ETHERMIN	(60-14)

/*
 * Ethernet Address Resolution Protocol.
 *
 * See RFC 826 for protocol description.  Structure below is adapted
 * to resolving internet addresses.  Field names used correspond to 
 * RFC 826.
 */
struct	ether_arp {
	struct	arphdr ea_hdr;	/* fixed-size header */
	u_char	arp_sha[6];	/* sender hardware address */
	u_char	arp_spa[4];	/* sender protocol address */
	u_char	arp_tha[6];	/* target hardware address */
	u_char	arp_tpa[4];	/* target protocol address */
};
#define	arp_hrd	ea_hdr.ar_hrd
#define	arp_pro	ea_hdr.ar_pro
#define	arp_hln	ea_hdr.ar_hln
#define	arp_pln	ea_hdr.ar_pln
#define	arp_op	ea_hdr.ar_op


/*
 * Structure shared between the ethernet driver modules and
 * the address resolution code.  For example, each ec_softc or il_softc
 * begins with this structure.
 */
struct	arpcom {
	struct 	ifnet ac_if;		/* network-visible interface */
	u_char	ac_enaddr[6];		/* ethernet hardware address */
	struct in_addr ac_ipaddr;	/* copy of ip address- XXX */
};

/*
 * Internet to ethernet address resolution table.
 */
struct	arptab {
	struct	in_addr at_iaddr;	/* internet address */
	u_char	at_enaddr[6];		/* ethernet address */
	u_char	at_timer;		/* minutes since last reference */
	u_char	at_flags;		/* flags */
	struct	mbuf *at_hold;		/* last packet until resolved/timeout */
};

#ifdef	KERNEL
/*u_char	etherbroadcastaddr[6];
struct	arptab *arptnew();*/
struct	arptab *arptnew(struct in_addr *);

/* interface symbols */
#define	__ISYM_VERSION__ "1"	/* XXX RCS major revision number of hdr file */
#include "isym.h"		/* this header has interface symbols */

/* global variables used in core kernel and other modules */
__ISYM__(u_char, etherbroadcastaddr, [6])

/* functions used in modules */
__ISYM__(void, ether_input, (struct ifnet *, struct ether_header *, struct mbuf *))
__ISYM__(int, ether_output, (struct ifnet *, struct mbuf *, struct sockaddr *, struct rtentry *))
__ISYM__(char *, ether_sprintf, (u_char *))

#undef __ISYM__
#undef __ISYM_ALIAS__
#undef __ISYM_VERSION__

#ifndef _ARP_PROTOTYPES
/* private arp functions, accessed via external symbol stub when loaded */
static void arpwhohas(struct arpcom *, struct in_addr *);
static void arpinput(struct arpcom *ac, struct mbuf *m);
static int arpresolve(struct arpcom *ac, struct mbuf *m, struct in_addr *destip,
	u_char *desten, int *usetrailers);

/* inline external symbol table function stubs */
extern inline void
arpwhohas(struct arpcom *a, struct in_addr *i) {
	void (*f)(struct arpcom *, struct in_addr *);

	(const void *) f = esym_fetch(arpwhohas);
	if (f == 0)
		return;
	(*f)(a, i);
}
extern inline void
arpinput(struct arpcom *a, struct mbuf *m) {
	void (*f)(struct arpcom *, struct mbuf *);

	(const void *) f = esym_fetch(arpinput);
	if (f == 0)
		return;
	(*f)(a, m);
}
extern inline int
arpresolve(struct arpcom *a, struct mbuf *m, struct in_addr *d, u_char *c, int *u) {
	int (*f)(struct arpcom *, struct mbuf *, struct in_addr *, u_char *, int *);

	(const void *) f = esym_fetch(arpresolve);
	if (f == 0)
		return (0);
	return ((*f)(a, m, d, c, u));
}
#else
extern void arpwhohas(struct arpcom *, struct in_addr *);
extern void arpinput(struct arpcom *ac, struct mbuf *m);
extern int arpresolve(struct arpcom *ac, struct mbuf *m, struct in_addr *destip,
	u_char *desten, int *usetrailers);
#endif /* _ARP_PROTOTYPES */
#endif
