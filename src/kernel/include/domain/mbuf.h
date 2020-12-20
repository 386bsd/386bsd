/*
 * Copyright (c) 1982, 1986, 1988 Regents of the University of California.
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
 *	$Id$
 */

#ifndef M_WAITOK
#include "malloc.h"
#endif

/*
 * Mbufs are of a single size, MSIZE (machine/machparam.h), which
 * includes overhead.  An mbuf may add a single "mbuf cluster" of size
 * MCLBYTES (also in machine/machparam.h), which has no additional overhead
 * and is used instead of the internal data area; this is done when
 * at least MINCLSIZE of data must be stored.
 */

#define	MLEN		(MSIZE - sizeof(struct m_hdr))	/* normal data len */
#define	MHLEN		(MLEN - sizeof(struct pkthdr))	/* data len w/pkthdr */

#define	MINCLSIZE	(MHLEN + MLEN)	/* smallest amount to put in cluster */
#define	M_MAXCOMPRESS	(MHLEN / 2)	/* max amount to copy for compression */

/*
 * Macros for type conversion
 * mtod(m,t) -	convert mbuf pointer to data pointer of correct type
 * dtom(x) -	convert data pointer within mbuf to mbuf pointer (XXX)
 * mtocl(x) -	convert pointer within cluster to cluster index #
 * cltom(x) -	convert cluster # to ptr to beginning of cluster
 */
#define mtod(m,t)	((t)((m)->m_data))
#define	dtom(x)		((struct mbuf *)((int)(x) & ~(MSIZE-1)))
#define	mtocl(x)	(((u_int)(x) - (u_int)mbutl) >> MCLSHIFT)
#define	cltom(x)	((caddr_t)((u_int)mbutl + ((u_int)(x) >> MCLSHIFT)))

/* header at beginning of each mbuf: */
struct m_hdr {
	struct	mbuf *mh_next;		/* next buffer in chain */
	struct	mbuf *mh_nextpkt;	/* next chain in queue/record */
	int	mh_len;			/* amount of data in this mbuf */
	caddr_t	mh_data;		/* location of data */
	short	mh_type;		/* type of data in this mbuf */
	short	mh_flags;		/* flags; see below */
};

/* record/packet header in first mbuf of chain; valid if M_PKTHDR set */
struct	pkthdr {
	int	len;		/* total packet length */
	struct	ifnet *rcvif;	/* rcv interface */
#ifdef	PKT_TRACE
	unsigned long time;	/* time header generated */
#ifdef	nope
	unsigned short ary[16];	/* it's */
	unsigned char tag[16];	/* tag */
	unsigned char idx;
#endif
#endif
};

/* description of external storage mapped into mbuf, valid if M_EXT set */
struct m_ext {
	caddr_t	ext_buf;		/* start of buffer */
	void	(*ext_free)();		/* free routine if not the usual */
	u_int	ext_size;		/* size of buffer, for ext_free */
};

struct mbuf {
	struct	m_hdr m_hdr;
	union {
		struct {
			struct	pkthdr MH_pkthdr;	/* M_PKTHDR set */
			union {
				struct	m_ext MH_ext;	/* M_EXT set */
				char	MH_databuf[MHLEN];
			} MH_dat;
		} MH;
		char	M_databuf[MLEN];		/* !M_PKTHDR, !M_EXT */
	} M_dat;
};
#define	m_next		m_hdr.mh_next
#define	m_len		m_hdr.mh_len
#define	m_data		m_hdr.mh_data
#define	m_type		m_hdr.mh_type
#define	m_flags		m_hdr.mh_flags
#define	m_nextpkt	m_hdr.mh_nextpkt
#define	m_act		m_nextpkt
#define	m_pkthdr	M_dat.MH.MH_pkthdr
#define	m_ext		M_dat.MH.MH_dat.MH_ext
#define	m_pktdat	M_dat.MH.MH_dat.MH_databuf
#define	m_dat		M_dat.M_databuf

/* mbuf flags */
#define	M_EXT		0x0001	/* has associated external storage */
#define	M_PKTHDR	0x0002	/* start of record */
#define	M_EOR		0x0004	/* end of record */

/* mbuf pkthdr flags, also in m_flags */
#define	M_BCAST		0x0100	/* send/received as link-level broadcast */
#define	M_MCAST		0x0200	/* send/received as link-level multicast */

/* flags copied when copying m_pkthdr */
#define	M_COPYFLAGS	(M_PKTHDR|M_EOR|M_BCAST|M_MCAST)

/* mbuf types */
#define	MT_FREE		0	/* should be on free list */
#define	MT_DATA		1	/* dynamic (data) allocation */
#define	MT_HEADER	2	/* packet header */
#define	MT_SOCKET	3	/* socket structure */
#define	MT_PCB		4	/* protocol control block */
#define	MT_RTABLE	5	/* routing tables */
#define	MT_HTABLE	6	/* IMP host tables */
#define	MT_ATABLE	7	/* address resolution tables */
#define	MT_SONAME	8	/* socket name */
#define	MT_SOOPTS	10	/* socket options */
#define	MT_FTABLE	11	/* fragment reassembly header */
#define	MT_RIGHTS	12	/* access rights */
#define	MT_IFADDR	13	/* interface address */
#define MT_CONTROL	14	/* extra-data protocol message */
#define MT_OOBDATA	15	/* expedited data  */

/* flags to m_get/MGET */
#define	M_DONTWAIT	M_NOWAIT
#define	M_WAIT		M_WAITOK

/*
 * mbuf allocation/deallocation macros:
 *
 *	MGET(struct mbuf *m, int how, int type)
 * allocates an mbuf and initializes it to contain internal data.
 *
 *	MGETHDR(struct mbuf *m, int how, int type)
 * allocates an mbuf and initializes it to contain a packet header
 * and internal data.
 */
#define	MGET(m, how, type) { \
	MALLOC((m), struct mbuf *, MSIZE, mbtypes[type], (how)); \
	if (m) { \
		(m)->m_type = (type); \
		mbstat.m_mtypes[type]++; \
		(m)->m_next = (struct mbuf *)NULL; \
		(m)->m_nextpkt = (struct mbuf *)NULL; \
		(m)->m_data = (m)->m_dat; \
		(m)->m_flags = 0; \
		/* m_transient_reserve(MSIZE); */ \
	} else \
		(m) = m_retry((how), (type)); \
}

#ifdef PKT_TRACE
#define	MGETHDRT(m, how, type) { \
	MALLOC((m), struct mbuf *, MSIZE, mbtypes[type], (how)); \
	if (m) { \
		(m)->m_type = (type); \
		mbstat.m_mtypes[type]++; \
		(m)->m_next = (struct mbuf *)NULL; \
		(m)->m_nextpkt = (struct mbuf *)NULL; \
		(m)->m_data = (m)->m_pktdat; \
		(m)->m_flags = M_PKTHDR; \
		(m)->m_pkthdr.time = getticks(); \
		/* m_transient_reserve(MSIZE); */ \
	} else \
		(m) = m_retryhdrt((how), (type)); \
}
#define	MGETHDR(m, how, type) { \
	MALLOC((m), struct mbuf *, MSIZE, mbtypes[type], (how)); \
	if (m) { \
		(m)->m_type = (type); \
		mbstat.m_mtypes[type]++; \
		(m)->m_next = (struct mbuf *)NULL; \
		(m)->m_nextpkt = (struct mbuf *)NULL; \
		(m)->m_data = (m)->m_pktdat; \
		(m)->m_flags = M_PKTHDR; \
		(m)->m_pkthdr.time = 0; \
		/* m_transient_reserve(MSIZE); */ \
	} else \
		(m) = m_retryhdr((how), (type)); \
}
#else
#define	MGETHDR(m, how, type) { \
	MALLOC((m), struct mbuf *, MSIZE, mbtypes[type], (how)); \
	if (m) { \
		(m)->m_type = (type); \
		mbstat.m_mtypes[type]++; \
		(m)->m_next = (struct mbuf *)NULL; \
		(m)->m_nextpkt = (struct mbuf *)NULL; \
		(m)->m_data = (m)->m_pktdat; \
		(m)->m_flags = M_PKTHDR; \
		/* m_transient_reserve(MSIZE); */ \
	} else \
		(m) = m_retryhdr((how), (type)); \
}
#endif

/*
 * Mbuf cluster macros.
 * MCLALLOC(caddr_t p, int how) allocates an mbuf cluster.
 * MCLGET adds such clusters to a normal mbuf;
 * the flag M_EXT is set upon success.
 * MCLFREE releases a reference to a cluster allocated by MCLALLOC,
 * freeing the cluster if the reference count has reached 0.
 *
 * Normal mbuf clusters are normally treated as character arrays
 * after allocation, but use the first word of the buffer as a free list
 * pointer while on the free list.
 */
union mcluster {
	union	mcluster *mcl_next;
	char	mcl_buf[MCLBYTES];
};

#define	MCLALLOC(p, how) \
	{ int ms = splimp(); \
	  if (mclfree == 0) \
		(void)m_clalloc(1, (how)); \
	  if ((p) = (caddr_t)mclfree) { \
		++mclrefcnt[mtocl(p)]; \
		mbstat.m_clfree--; \
		mclfree = ((union mcluster *)(p))->mcl_next; \
		/* m_transient_reserve(MCLBYTES); */ \
	  } \
	  splx(ms); \
	}

#define	MCLGET(m, how) \
	{ MCLALLOC((m)->m_ext.ext_buf, (how)); \
	  if ((m)->m_ext.ext_buf != NULL) { \
		(m)->m_data = (m)->m_ext.ext_buf; \
		(m)->m_flags |= M_EXT; \
		(m)->m_ext.ext_size = MCLBYTES;  \
	  } \
	}

#define	MCLFREE(p) \
	{ int ms = splimp(); \
	  if (--mclrefcnt[mtocl(p)] == 0) { \
		((union mcluster *)(p))->mcl_next = mclfree; \
		mclfree = (union mcluster *)(p); \
		mbstat.m_clfree++; \
		/* m_transient_release(MCLBYTES); */ \
	  } \
	  splx(ms); \
	}

/*
 * MFREE(struct mbuf *m, struct mbuf *n)
 * Free a single mbuf and associated external storage.
 * Place the successor, if any, in n.
 */
#ifdef notyet
#define	MFREE(m, n) \
	{ mbstat.m_mtypes[(m)->m_type]--; \
	  if ((m)->m_flags & M_EXT) { \
		if ((m)->m_ext.ext_free) \
			(*((m)->m_ext.ext_free))((m)->m_ext.ext_buf, \
			    (m)->m_ext.ext_size); \
		else \
			MCLFREE((m)->m_ext.ext_buf); \
	  } \
	  (n) = (m)->m_next; \
	  /* m_transient_release(MSIZE); */ \
	  FREE((m), mbtypes[(m)->m_type]); \
	}
#else /* notyet */
#ifdef PKT_TRACE
#define	MFREE(m, nn) \
	{ mbstat.m_mtypes[(m)->m_type]--; \
	  if ((m)->m_flags & M_PKTHDR) { \
		m_trace(m); \
	  } \
	  if ((m)->m_flags & M_EXT) { \
		MCLFREE((m)->m_ext.ext_buf); \
	  } \
	  (nn) = (m)->m_next; \
	  /* m_transient_release(MSIZE); */ \
	  FREE((m), mbtypes[(m)->m_type]); \
	}
#else
#define	MFREE(m, nn) \
	{ mbstat.m_mtypes[(m)->m_type]--; \
	  if ((m)->m_flags & M_EXT) { \
		MCLFREE((m)->m_ext.ext_buf); \
	  } \
	  (nn) = (m)->m_next; \
	  /* m_transient_release(MSIZE); */ \
	  FREE((m), mbtypes[(m)->m_type]); \
	}
#endif
#endif

/*
 * Copy mbuf pkthdr from from to to.
 * from must have M_PKTHDR set, and to must be empty.
 */
#define	M_COPY_PKTHDR(to, from) { \
	(to)->m_pkthdr = (from)->m_pkthdr; \
	(to)->m_flags = (from)->m_flags & M_COPYFLAGS; \
	(to)->m_data = (to)->m_pktdat; \
}

/*
 * Set the m_data pointer of a newly-allocated mbuf (m_get/MGET) to place
 * an object of the specified size at the end of the mbuf, longword aligned.
 */
#define	M_ALIGN(m, len) \
	{ (m)->m_data += (MLEN - (len)) &~ (sizeof(long) - 1); }
/*
 * As above, for mbufs allocated with m_gethdr/MGETHDR
 * or initialized by M_COPY_PKTHDR.
 */
#define	MH_ALIGN(m, len) \
	{ (m)->m_data += (MHLEN - (len)) &~ (sizeof(long) - 1); }

/*
 * Compute the amount of space available
 * before the current start of data in an mbuf.
 */
#define	M_LEADINGSPACE(m) \
	((m)->m_flags & M_EXT ? /* (m)->m_data - (m)->m_ext.ext_buf */ 0 : \
	    (m)->m_flags & M_PKTHDR ? (m)->m_data - (m)->m_pktdat : \
	    (m)->m_data - (m)->m_dat)

/*
 * Compute the amount of space available
 * after the end of data in an mbuf.
 */
#define	M_TRAILINGSPACE(m) \
	((m)->m_flags & M_EXT ? (m)->m_ext.ext_buf + (m)->m_ext.ext_size - \
	    ((m)->m_data + (m)->m_len) : \
	    &(m)->m_dat[MLEN] - ((m)->m_data + (m)->m_len))

/*
 * Arrange to prepend space of size plen to mbuf m.
 * If a new mbuf must be allocated, how specifies whether to wait.
 * If how is M_DONTWAIT and allocation fails, the original mbuf chain
 * is freed and m is set to NULL.
 */
#define	M_PREPEND(m, plen, how) { \
	if (M_LEADINGSPACE(m) >= (plen)) { \
		(m)->m_data -= (plen); \
		(m)->m_len += (plen); \
	} else \
		(m) = m_prepend((m), (plen), (how)); \
	if ((m) && (m)->m_flags & M_PKTHDR) \
		(m)->m_pkthdr.len += (plen); \
}

/* change mbuf to new type */
#define MCHTYPE(m, t) { \
	mbstat.m_mtypes[(m)->m_type]--; \
	mbstat.m_mtypes[t]++; \
	(m)->m_type = t;\
}

/* length to m_copy to copy all */
#define	M_COPYALL	1000000000

/* compatiblity with 4.3 */
#define  m_copy(m, o, l)	m_copym((m), (o), (l), M_DONTWAIT)

/*
 * Mbuf statistics.
 */
struct mbstat {
	u_long	m_mbufs;	/* mbufs obtained from page pool */
	u_long	m_clusters;	/* clusters obtained from page pool */
	u_long	m_spare;	/* spare field */
	u_long	m_clfree;	/* free clusters */
	u_long	m_drops;	/* times failed to find space */
	u_long	m_wait;		/* times waited for space */
	u_long	m_drain;	/* times drained protocols for space */
	u_short	m_mtypes[256];	/* type specific mbuf allocations */
};

#ifdef	KERNEL

/* global variables used in core kernel */
extern int nmbclusters;
#ifdef nope
extern int max_linkhdr;			/* largest link-level header */
extern int max_protohdr;		/* largest protocol header */
extern int max_hdr;			/* largest link+protocol header */
extern int max_datalen;			/* MHLEN - max_hdr */
#endif

/* space */
/* unsigned int mbfspace_free, mbfspace_reserve;
unsigned int mbspace_free, mbspace_reserve, mbspace_transient; */

/* functions used in core kernel */
#ifdef PKT_TRACE
struct	mbuf *m_retryhdrt();
#endif
/* struct	mbuf *m_copym(struct mbuf *, int, int, int);
struct mbuf *m_free(struct mbuf *);
struct	mbuf *m_get(int, int);
struct	mbuf *m_getclr(int, int);
struct	mbuf *m_gethdr(int, int);
struct	mbuf *m_prepend(struct mbuf *, int, int);
struct	mbuf *m_pullup(struct mbuf *, int);
void	m_cat(struct mbuf *m, struct mbuf *n); */
void	mbinit(void);
void	m_reclaim(void);

/* space reservation */
/* #define	m_commit_reserve(s)	mbfspace_free -= (s); mbfspace_reserve += (s);
#define	m_commit_release(s)	mbfspace_free += (s); mbfspace_reserve -= (s);
#define	m_apply_reserve(s)	mbspace_transient -= (s); mbspace_reserve += (s);
#define	m_apply_release(s)	mbspace_transient += (s); mbspace_reserve -= (s);
#define	m_transient_reserve(s)	mbspace_free -= (s); mbspace_transient += (s);
#define	m_transient_release(s)	mbspace_free += (s); mbspace_transient -= (s); */

/* interface symbols */
#define	__ISYM_VERSION__ "1"	/* XXX RCS major revision number of hdr file */
#include "isym.h"		/* this header has interface symbols */

/* global variables used in core kernel and other modules */
__ISYM__(struct mbuf *, mbutl,)		/* virtual address of mclusters */
__ISYM__(char *, mclrefcnt,)		/* cluster reference counts */
__ISYM__(struct mbstat, mbstat,)	/* statistics */
__ISYM__(union mcluster *, mclfree,)	/* head of cluster free list */
__ISYM__(int, max_linkhdr,)		/* largest link-level header */
__ISYM__(int, max_protohdr,)		/* largest protocol header */
__ISYM__(int, max_hdr,)			/* largest link+protocol header */
__ISYM__(int, max_datalen,)		/* MHLEN - max_hdr */

/* functions used in core kernel and modules */
__ISYM__(int, m_clalloc, (int, int))
__ISYM__(struct	mbuf *, m_get, (int, int))
__ISYM__(struct	mbuf *, m_getclr, (int, int))
__ISYM__(struct	mbuf *, m_gethdr, (int, int))
__ISYM__(struct	mbuf *, m_prepend, (struct mbuf *, int, int))
__ISYM__(struct	mbuf *, m_pullup, (struct mbuf *, int))
__ISYM__(struct mbuf *, m_copym, (struct mbuf *, int, int, int))
__ISYM__(struct mbuf *, m_free, (struct mbuf *))
__ISYM__(struct mbuf *, m_retry, (int, int))
__ISYM__(struct mbuf *, m_retryhdr, (int, int))
__ISYM__(void, m_adj, (struct mbuf *mp, int req_len))
__ISYM__(void, m_cat, (struct mbuf *m, struct mbuf *n))
__ISYM__(void, m_copydata, (struct mbuf *m, int off, int len, caddr_t cp))
__ISYM__(void, m_freem, (struct mbuf *))

__ISYM__(int, mbtypes, [])		/* XXX */
#undef __ISYM__
#undef __ISYM_ALIAS__
#undef __ISYM_VERSION__

#ifdef MBTYPES
int mbtypes[] = {				/* XXX */
	M_FREE,		/* MT_FREE	0	/* should be on free list */
	M_MBUF,		/* MT_DATA	1	/* dynamic (data) allocation */
	M_MBUF,		/* MT_HEADER	2	/* packet header */
	M_SOCKET,	/* MT_SOCKET	3	/* socket structure */
	M_PCB,		/* MT_PCB	4	/* protocol control block */
	M_RTABLE,	/* MT_RTABLE	5	/* routing tables */
	M_HTABLE,	/* MT_HTABLE	6	/* IMP host tables */
	0,		/* MT_ATABLE	7	/* address resolution tables */
	M_MBUF,		/* MT_SONAME	8	/* socket name */
	0,		/* 		9 */
	M_SOOPTS,	/* MT_SOOPTS	10	/* socket options */
	M_FTABLE,	/* MT_FTABLE	11	/* fragment reassembly header */
	M_MBUF,		/* MT_RIGHTS	12	/* access rights */
	M_IFADDR,	/* MT_IFADDR	13	/* interface address */
	M_MBUF,		/* MT_CONTROL	14	/* extra-data protocol message */
	M_MBUF,		/* MT_OOBDATA	15	/* expedited data  */
#ifdef DATAKIT
	25, 26, 27, 28, 29, 30, 31, 32		/* datakit ugliness */
#endif
};
#endif
#endif
