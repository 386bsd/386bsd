/*-
 * Copyright (c) 1990, 1991 William F. Jolitz.
 * Copyright (c) 1990 The Regents of the University of California.
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
 *	@(#)if_ne.c	7.4 (Berkeley) 5/21/91
 */

/*
 * Isolink 4110-2 Ethernet driver
 */

#include "is.h"
#if NIS > 0

#include "param.h"
#include "systm.h"
#include "mbuf.h"
#include "buf.h"
#include "protosw.h"
#include "socket.h"
#include "ioctl.h"
#include "errno.h"
#include "syslog.h"

#include "net/if.h"
#include "net/netisr.h"
#include "net/route.h"

#ifdef INET
#include "netinet/in.h"
#include "netinet/in_systm.h"
#include "netinet/in_var.h"
#include "netinet/ip.h"
#include "netinet/if_ether.h"
#endif

#ifdef NS
#include "netns/ns.h"
#include "netns/ns_if.h"
#endif

#include "i386/isa/isa_device.h"
#include "i386/isa/if_isreg.h"
#include "i386/isa/icu.h"

#include "vm/vm.h"

/* Function prototypes */
int isprobe(), isattach();
int isioctl(),isinit(),isstart();

struct	isa_driver isdriver = {
	isprobe, isattach, "is",
};


struct	mbuf *isget();

#define ETHER_MIN_LEN 64

/*
 * Ethernet software status per interface.
 *
 * Each interface is referenced by a network interface structure,
 * ns_if, which the routing code uses to locate the interface.
 * This structure contains the output queue for the interface, its address, ...
 */
struct	is_softc {
	struct	arpcom ns_ac;		/* Ethernet common part */
#define	ns_if	ns_ac.ac_if		/* network-visible interface */
#define	ns_addr	ns_ac.ac_enaddr		/* hardware Ethernet address */
	int	last_rd;
	int	last_td;
	int	no_td;
} is_softc[NIS] ;

struct init_block init_block;
struct mds *td, *rd;
unsigned char  *rbuf,*tbuf;

int isc;

iswrcsr(port,val)
	u_short port;
	u_short val;
{
	outw(isc+RAP,port);
	outw(isc+RDP,val);
}

u_short isrdcsr(port)
	u_short port;
{
	outw(isc+RAP,port);
	return(inw(isc+RDP));
} 

isprobe(dvp)
	struct isa_device *dvp;
{
	int val,i,s;
	register struct is_softc *ns = &is_softc[0];

	isc = dvp->id_iobase;
	s = splimp();

	/* Stop the lance chip, put it known state */	
	iswrcsr(0,STOP);
	DELAY(100);

	/* is there a lance? */
	iswrcsr(3, 0xffff);
	if (isrdcsr(3) != 7) {
		isc = 0;
		return (0);
	}
	iswrcsr(3, 0);

	/* Extract board address */
	for(i=0; i < 6; i++)  ns->ns_addr[i] = inb(isc+(i*2));

	splx(s);
	return (1);
}



/*
 * Reset of interface.
 */
isreset(unit, uban)
	int unit, uban;
{
	if (unit >= NIS)
		return;
	printf("is%d: reset\n", unit);
	isinit(unit);
}
 
/*
 * Interface exists: make available by filling in network interface
 * record.  System will initialize the interface when it is ready
 * to accept packets.  We get the ethernet address here.
 */
isattach(dvp)
	struct isa_device *dvp;
{
	int unit = dvp->id_unit;
	register struct is_softc *is = &is_softc[unit];
	register struct ifnet *ifp = &is->ns_if;

	/* Set up DMA */
	isa_dmacascade(dvp->id_drq);

	ifp->if_unit = unit;
	ifp->if_name = isdriver.name ;
	ifp->if_mtu = ETHERMTU;
	printf (" ethernet address %s", ether_sprintf(is->ns_addr)) ;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS;
	ifp->if_init = isinit;
	ifp->if_output = ether_output;
	ifp->if_start = isstart;
	ifp->if_ioctl = isioctl;
	ifp->if_reset = isreset;
	ifp->if_watchdog = 0;
	if_attach(ifp);
}


/* Lance initialisation block set up */
init_mem()
{
	int i;
	u_long temp;

	/* Allocate memory */
/* Temporary hack, will use kmem_alloc in future */
#define MAXMEM ((NRBUF+NTBUF)*(BUFSIZE) + (NRBUF+NTBUF)*sizeof(struct mds) + 8)
static u_char lance_mem[MAXMEM];


	/* Align message descriptors on quad word boundary 
		(this is essential) */

	temp = (u_long) &lance_mem;
	temp = (temp+8) - (temp%8);
	rd = (struct mds *) temp;
	td = (struct mds *) (temp + (NRBUF*sizeof(struct mds)));
	temp += (NRBUF+NTBUF) * sizeof(struct mds);

	init_block.mode = 0;
	
	/* Get ethernet address */
	for (i=0; i<6; i++) 
		init_block.padr[i] = inb(isc+(i*2));

	/* Clear multicast address for now */
	for (i=0; i<8; i++)
		init_block.ladrf[i] = 0;

	init_block.rdra = kvtop(rd);
	init_block.rlen = ((kvtop(rd) >> 16) & 0xff) | (RLEN<<13);
	init_block.tdra = kvtop(td);
	init_block.tlen = ((kvtop(td) >> 16) & 0xff) | (TLEN<<13);

	/* Set up receive ring descriptors */
	rbuf = (unsigned char *)temp;
	for (i=0; i<NRBUF; i++) {
		(rd+i)->addr = kvtop(temp);
		(rd+i)->flags= ((kvtop(temp) >> 16) & 0xff) | OWN;
		(rd+i)->bcnt = -BUFSIZE;
		(rd+i)->mcnt = 0;
		temp += BUFSIZE;
	}

	/* Set up transmit ring descriptors */
	tbuf = (unsigned char *)temp;
#ifdef ISDEBUG
	printf("rd = %x,td = %x, rbuf = %x, tbuf = %x,td+1=%x\n",rd,td,rbuf,tbuf,td+1);
#endif
	for (i=0; i<NTBUF; i++) {
		(td+i)->addr = kvtop(temp);
		(td+i)->flags= ((kvtop(temp) >> 16) & 0xff);
		(td+i)->bcnt = 0;
		(td+i)->mcnt = 0;
		temp += BUFSIZE;
	}

}

/*
 * Initialization of interface; set up initialization block
 * and transmit/receive descriptor rings.
 */
isinit(unit)
	int unit;
{
	register struct is_softc *ns = &is_softc[unit];
	struct ifnet *ifp = &ns->ns_if;
	int s;
	register i;

 	if (ifp->if_addrlist == (struct ifaddr *)0) return;

	ns->last_rd = ns->last_td = ns->no_td = 0;
	s = splimp();

	/* Set up lance's memory area */
	init_mem();

	/* Stop Lance to get access to other registers */
	iswrcsr(0,STOP);

	/* I wish I knew what this was */
	iswrcsr(3,0);

	/* Give lance the physical address of its memory area */
	iswrcsr(1,kvtop(&init_block));
	iswrcsr(2,(kvtop(&init_block) >> 16) & 0xff);

	/* OK, let's try and initialise the Lance */
	iswrcsr(0,INIT);

	/* Wait for initialisation to finish */
	for(i=0; i<1000; i++){
		if (isrdcsr(0)&IDON)
			break;
	}
	if (isrdcsr(0)&IDON) {
		/* Start lance */
		iswrcsr(0,STRT|IDON|INEA);
		ns->ns_if.if_flags |= IFF_RUNNING;
		isstart(ifp);
	}
	else 
		printf("Isolink card failed to initialise\n");
	
	splx(s);
}

/*
 * Setup output on interface.
 * Get another datagram to send off of the interface queue,
 * and map it to the interface before starting the output.
 * called only at splimp or interrupt level.
 */
isstart(ifp)
	struct ifnet *ifp;
{
	register struct is_softc *ns = &is_softc[ifp->if_unit];
	struct mbuf *m0, *m;
	unsigned char *buffer;
	u_short len;
	int i;
	struct mds *cdm;


	if ((ns->ns_if.if_flags & IFF_RUNNING) == 0)
		return;

	do {
		cdm = (td + ns->last_td);
		if (cdm->flags&OWN)
			return;
	
		IF_DEQUEUE(&ns->ns_if.if_snd, m);

		if (m == 0)
			return;

		/*
	 	* Copy the mbuf chain into the transmit buffer
	 	*/

		buffer = tbuf+(BUFSIZE*ns->last_td);
		len=0;
		for (m0=m; m != 0; m=m->m_next) {
			bcopy(mtod(m,caddr_t),buffer,m->m_len);
			buffer += m->m_len;
			len += m->m_len;
		}
		
		m_freem(m0);
		len = MAX(len,ETHER_MIN_LEN);

		/*
	 	* Init transmit registers, and set transmit start flag.
	 	*/

		cdm->flags |= (OWN|STP|ENP);
		cdm->bcnt = -len;
		cdm->mcnt = 0;
#ifdef ISDEBUG
		xmit_print(ns->last_td);
#endif
		
		iswrcsr(0,TDMD|INEA);
		if (++ns->last_td >= NTBUF)
			ns->last_td=0;
		}while(++ns->no_td < NTBUF);
		ns->no_td = NTBUF;
		ns->ns_if.if_flags |= IFF_OACTIVE;	
#ifdef ISDEBUG 
	printf("no_td = %x, last_td = %x\n",ns->no_td, ns->last_td);
#endif
		return(0);	
}


/*
 * Controller interrupt.
 */
isintr(unit)
{
	register struct is_softc *ns = &is_softc[unit];
	u_short isr;

	while((isr=isrdcsr(0))&INTR) {
		if (isr&ERR) {
			if (isr&BABL)
				printf("BABL\n");
			if (isr&CERR)
				printf("CERR\n");
			if (isr&MISS)
				printf("MISS\n");
			if (isr&MERR)
				printf("MERR\n");
			iswrcsr(0,BABL|CERR|MISS|MERR|INEA);
		}
		if (!(isr&TXON)) {
			isreset(unit);
			return(1);
		}
		if (!(isr&RXON)) {
			isreset(unit);
			return(1);
		}
		if (isr&RINT) {
			isrint(unit);
		}
		if (isr&TINT) {
			iswrcsr(0,TINT|INEA);
			istint(unit);
		}
	}
}

istint(unit) 
	int unit;
{
	struct is_softc *is = &is_softc[unit];
	register struct ifnet *ifp = &is->ns_if;
	int i,loopcount=0;
	struct mds *cdm;

	do {
		if ((i=is->last_td - is->no_td) < 0)
			i+=NTBUF;
		cdm = (td+i);
#ifdef ISDEBUG
	printf("Trans cdm = %x\n",cdm);
#endif
		if (cdm->flags&OWN) {
			if (loopcount)
				break;
			return;
		}
		loopcount++;	
		is->ns_if.if_flags &= ~IFF_OACTIVE;
	}while(--is->no_td > 0);
	isstart(ifp);	
	
}

#define NEXTRDS \
	if (++rmd == NRBUF) rmd=0, cdm=rd; else ++cdm
	
isrint(unit)
	int unit;
{
	register struct is_softc *is=&is_softc[unit];
	register int rmd = is->last_rd;
	struct mds *cdm = (rd + rmd);

	/* Out of sync with hardware, should never happen */
	
	if (cdm->flags & OWN) {
		printf("is0 error: out of sync\n");
		iswrcsr(0,RINT|INEA);
		return;
	}

	/* Process all buffers with valid data */
	while (!(cdm->flags&OWN)) {
		/* Clear interrupt to avoid race condition */
		iswrcsr(0,RINT|INEA);
		if (cdm->flags&ERR) {
			if (cdm->flags&FRAM)
				printf("FRAM\n");
			if (cdm->flags&OFLO)
				printf("OFLO\n");
			if (cdm->flags&CRC)
				printf("CRC\n");
			if (cdm->flags&RBUFF)
				printf("RBUFF\n");
		}else 
		if (cdm->flags&(STP|ENP) != (STP|ENP)) {
			do {
				iswrcsr(0,RINT|INEA);
				cdm->mcnt = 0;
				cdm->flags |= OWN;	
				NEXTRDS;
			}while (!(cdm->flags&(OWN|ERR|STP|ENP)));
			is->last_rd = rmd;
			printf("Chained buffer\n");
			if ((cdm->flags & (OWN|ERR|STP|ENP)) != ENP) {
				isreset(unit);
				return;
			}
		}else
			{
#ifdef ISDEBUG 
	recv_print(is->last_rd);
#endif
			isread(is,rbuf+(BUFSIZE*rmd),cdm->mcnt);
			}
			
		cdm->flags |= OWN;
		cdm->mcnt = 0;
		NEXTRDS;
#ifdef ISDEBUG
	printf("is->last_rd = %x, cdm = %x\n",is->last_rd,cdm);
#endif
	} /* while */
	is->last_rd = rmd;
} /* isrint */

/*
 * Pass a packet to the higher levels.
 * We deal with the trailer protocol here.
 */
isread(ns, buf, len)
	register struct is_softc *ns;
	char *buf;
	int len;
{
	register struct ether_header *eh;
    	struct mbuf *m;
	int off, resid;
	register struct ifqueue *inq;

	/*
	 * Deal with trailer protocol: if type is trailer type
	 * get true type from first 16-bit word past data.
	 * Remember that type was trailer by setting off.
	 */
	eh = (struct ether_header *)buf;
	eh->ether_type = ntohs((u_short)eh->ether_type);
	len = len - sizeof(struct ether_header) - 4;
#define	nedataaddr(eh, off, type)	((type)(((caddr_t)((eh)+1)+(off))))
	if (eh->ether_type >= ETHERTYPE_TRAIL &&
	    eh->ether_type < ETHERTYPE_TRAIL+ETHERTYPE_NTRAILER) {
		off = (eh->ether_type - ETHERTYPE_TRAIL) * 512;
		if (off >= ETHERMTU) return;		/* sanity */
		eh->ether_type = ntohs(*nedataaddr(eh, off, u_short *));
		resid = ntohs(*(nedataaddr(eh, off+2, u_short *)));
		if (off + resid > len) return;		/* sanity */
		len = off + resid;
	} else	off = 0;

	if (len == 0) return;

	/*
	 * Pull packet off interface.  Off is nonzero if packet
	 * has trailing header; neget will then force this header
	 * information to be at the front, but we still have to drop
	 * the type and length which are at the front of any trailer data.
	 */
	m = isget(buf, len, off, &ns->ns_if);
	if (m == 0) return;

	ether_input(&ns->ns_if, eh, m);
}

/*
 * Supporting routines
 */

/*
 * Pull read data off a interface.
 * Len is length of data, with local net header stripped.
 * Off is non-zero if a trailer protocol was used, and
 * gives the offset of the trailer information.
 * We copy the trailer information and then all the normal
 * data into mbufs.  When full cluster sized units are present
 * we copy into clusters.
 */
struct mbuf *
isget(buf, totlen, off0, ifp)
	caddr_t buf;
	int totlen, off0;
	struct ifnet *ifp;
{
	struct mbuf *top, **mp, *m, *p;
	int off = off0, len;
	register caddr_t cp = buf;
	char *epkt;

	buf += sizeof(struct ether_header);
	cp = buf;
	epkt = cp + totlen;


	if (off) {
		cp += off + 2 * sizeof(u_short);
		totlen -= 2 * sizeof(u_short);
	}

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == 0)
		return (0);
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = totlen;
	m->m_len = MHLEN;

	top = 0;
	mp = &top;
	while (totlen > 0) {
		if (top) {
			MGET(m, M_DONTWAIT, MT_DATA);
			if (m == 0) {
				m_freem(top);
				return (0);
			}
			m->m_len = MLEN;
		}
		len = min(totlen, epkt - cp);
		if (len >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if (m->m_flags & M_EXT)
				m->m_len = len = min(len, MCLBYTES);
			else
				len = m->m_len;
		} else {
			/*
			 * Place initial small packet/header at end of mbuf.
			 */
			if (len < m->m_len) {
				if (top == 0 && len + max_linkhdr <= m->m_len)
					m->m_data += max_linkhdr;
				m->m_len = len;
			} else
				len = m->m_len;
		}
		bcopy(cp, mtod(m, caddr_t), (unsigned)len);
		cp += len;
		*mp = m;
		mp = &m->m_next;
		totlen -= len;
		if (cp == epkt)
			cp = buf;
	}
	return (top);
}

/*
 * Process an ioctl request.
 */
isioctl(ifp, cmd, data)
	register struct ifnet *ifp;
	int cmd;
	caddr_t data;
{
	register struct ifaddr *ifa = (struct ifaddr *)data;
	struct is_softc *ns = &is_softc[ifp->if_unit];
	struct ifreq *ifr = (struct ifreq *)data;
	int s = splimp(), error = 0;


	switch (cmd) {

	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;

		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			isinit(ifp->if_unit);	/* before arpwhohas */
			((struct arpcom *)ifp)->ac_ipaddr =
				IA_SIN(ifa)->sin_addr;
			arpwhohas((struct arpcom *)ifp, &IA_SIN(ifa)->sin_addr);
			break;
#endif
#ifdef NS
		case AF_NS:
		    {
			register struct ns_addr *ina = &(IA_SNS(ifa)->sns_addr);

			if (ns_nullhost(*ina))
				ina->x_host = *(union ns_host *)(ns->ns_addr);
			else {
				/* 
				 * The manual says we can't change the address 
				 * while the receiver is armed,
				 * so reset everything
				 */
				ifp->if_flags &= ~IFF_RUNNING; 
				bcopy((caddr_t)ina->x_host.c_host,
				    (caddr_t)ns->ns_addr, sizeof(ns->ns_addr));
			}
			isinit(ifp->if_unit); /* does ne_setaddr() */
			break;
		    }
#endif
		default:
			isinit(ifp->if_unit);
			break;
		}
		break;

	case SIOCSIFFLAGS:
		if ((ifp->if_flags & IFF_UP) == 0 &&
		    ifp->if_flags & IFF_RUNNING) {
			ifp->if_flags &= ~IFF_RUNNING;
			iswrcsr(0,STOP);
		} else if (ifp->if_flags & IFF_UP &&
		    (ifp->if_flags & IFF_RUNNING) == 0)
			isinit(ifp->if_unit);
		break;

#ifdef notdef
	case SIOCGHWADDR:
		bcopy((caddr_t)ns->ns_addr, (caddr_t) &ifr->ifr_data,
			sizeof(ns->ns_addr));
		break;
#endif

	default:
		error = EINVAL;
	}
	splx(s);
	return (error);
}

recv_print(no)
	int no;
{
	struct mds *rmd;
	int len,i;
	
	rmd = (rd+no);
	len = rmd->mcnt;
	printf("Receive buffer %d, len = %d\n",no,len);
	printf("Status %x\n",isrdcsr(0));
	for (i=0; i<len; i++) 
		printf("%x ",*(rbuf+(BUFSIZE*no)+i));
	printf("\n");
}
		
xmit_print(no)
	int no;
{
	struct mds *rmd;
	int i;
	u_short len;
	
	rmd = (td+no);
	len = -(rmd->bcnt);
	printf("Transmit buffer %d, len = %d\n",no,len);
	printf("Status %x\n",isrdcsr(0));
	printf("addr %x, flags %x, bcnt %x, mcnt %x\n",
		rmd->addr,rmd->flags,rmd->bcnt,rmd->mcnt);
	for (i=0; i<len; i++) 
		printf("%x ",*(tbuf+(BUFSIZE*no)+i));
	printf("\n");
}
		
#endif
