/*
 * Copyright (c) 1982, 1986, 1988, 1990 Regents of the University of California.
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
 * $Id: sockbuf.c,v 1.3 95/02/24 10:43:16 bill Exp Locker: bill $
 *
 * Socket buffer queue utility routines, including allocation / deallocation
 * of buffer space to a socket buffer, add data to the tail of the queue,
 * remove data from the head of the queue, and allow exclusive access to
 * the contents of the buffer by potentially multiple processes.
 */

#include "sys/param.h"
#include "sys/file.h"
#include "sys/errno.h"
#include "systm.h"	/* selwait */
#include "proc.h"
#include "signalvar.h"
#include "buf.h"
#include "malloc.h"
#include "mbuf.h"
#include "socketvar.h"
#include "protosw.h"
#include "prototypes.h"

/* strings for sleep message: */
static char	netio[] = "netio";

u_long	sb_max = SB_MAX;	/* patchable administrative size limit */


/*
 * Commit storage to sockbuf to allow the desired largest logical
 * message to be queued(unimplemented). 
 */
int
sbreserve(struct sockbuf *sb, u_long cc)
{

	/* bound maximum size by memory commitment of cluster + header mbuf */
	if (cc > sb_max * MCLBYTES / (MSIZE + MCLBYTES))
		return (0);

	/* presume that mbufs are at least half - full on the average */
	sb->sb_mbmax = min(cc * 2, sb_max);

	/* adjust watermarks to sane levels */
	sb->sb_hiwat = cc;
	if (sb->sb_lowat > sb->sb_hiwat)
		sb->sb_lowat = sb->sb_hiwat;

	/* account for mbuf memory reserved for use in socket buffer */
	/* m_commit_reserve(sb->sb_mbmax); */

	return (1);
}

/* Free any mbufs held by a socket, and release reserved mbuf resource. */
void
sbrelease(struct sockbuf *sb)
{

	/* m_uncommit_reserve(sb->sb_mbmax); */
	sb->sb_hiwat = sb->sb_mbmax = 0;
	sbflush(sb);
}

/* Free all mbufs in a socket buffer. */
void
sbflush(struct sockbuf *sb)
{

	/* cannot be locked */
	if (sb->sb_flags & SB_LOCK)
		panic("sbflush");

	/* drop any and all mbuf's present in the socket buffer */
	while (sb->sb_mbcnt)
		sbdrop(sb, (int)sb->sb_cc);

#ifdef DIAGNOSTIC
	/* socket must be empty */
	if (sb->sb_cc || sb->sb_mb)
		panic("sbflush 2");
#endif
}

/*
 * The following append functions add information to the tail of the queue,
 * and the following drop functions discard data from the head of queue.
 * Data stored in a socket buffer queue is maintained as a structured list of
 * records, each of which is a chain of mbufs linked together with the m_next
 * field.  Records are themselves linked together with the m_nextpkt field,
 * and are strictly ordered into sucessive groups (see soreceive()):
 *
 *     <group> ::= [[<sockaddr>] <control>] [<oob>] <data>
 *
 * Different versions of sbappend() exist to insert a new group correctly.
 * Record linking exists to allow potentially zero length chains to be
 * preserved as record marks.
 */

/*
 * Append mbuf chain to the trailing record in the socket buffer,
 * thus extending the record. Used by a protocol to deliver
 * a new portion of a record to an existing record.
 */
void
sbappend(struct sockbuf *sb, struct mbuf *m)
{
	struct mbuf *n;

	/* nothing to add */
	if (m == 0)
		return;

	/* socket buffer queue chain present already? */
	if (n = sb->sb_mb) {

		/* locate trailing record */
		while (n->m_nextpkt)
			n = n->m_nextpkt;

		/* locate trailing mbuf in trailing record */
		do {
			/* ug. new record ending? then append as new record! */
			if (n->m_flags & M_EOR) {
				/* redo everything from scratch */
				sbappendrecord(sb, m);
				return;
			}
		} while (n->m_next && (n = n->m_next));
	}

	/* clean & reduce mbufs in list, tack on our mbuf chain */
	sbcompress(sb, m, n);
}

#ifdef SOCKBUF_DEBUG
void
sbcheck(struct sockbuf *sb)
{
	register struct mbuf *m;
	register int len = 0, mbcnt = 0;

	for (m = sb->sb_mb; m; m = m->m_next) {
		len += m->m_len;
		mbcnt += MSIZE;
		if (m->m_flags & M_EXT)
			mbcnt += m->m_ext.ext_size;
		if (m->m_nextpkt)
			panic("sbcheck nextpkt");
	}
	if (len != sb->sb_cc || mbcnt != sb->sb_mbcnt) {
		printf("cc %d != %d || mbcnt %d != %d\n", len, sb->sb_cc,
		    mbcnt, sb->sb_mbcnt);
		panic("sbcheck");
	}
}
#endif

/*
 * Append mbuf chain as the start of the new trailing record in a socket
 * buffer. Used by a protocol to deliver data as the start of a new record.
 */
void
sbappendrecord(struct sockbuf *sb, struct mbuf *m0)
{
	struct mbuf *m;

	/* nothing to add. */
	if (m0 == 0)
		return;

	/* if contents in the queue, find the last record */
	if (m = sb->sb_mb)
		while (m->m_nextpkt)
			m = m->m_nextpkt;

	/*
	 * Put the first mbuf on the queue.
	 * Note this permits zero length records.
	 */
	sballoc(sb, m0);
	if (m)
		m->m_nextpkt = m0;
	else
		sb->sb_mb = m0;

	/* isolate remainder of chain past first mbuf */
	m = m0->m_next;
	m0->m_next = 0;

	/* relay record mark to chain past first mbuf if present */
	if (m && (m0->m_flags & M_EOR)) {
		m0->m_flags &= ~M_EOR;
		m->m_flags |= M_EOR;
	}
	sbcompress(sb, m, m0);
}

/*
 * Append a mbuf chain of OOB data to any other OOB data in the
 * trailing record of the socket buffer, which is located ahead of
 * normal data. Used by a protocol when OOB data spontaineously arrives.
 */
void
sbinsertoob(struct sockbuf *sb, struct mbuf *m0)
{
	struct mbuf *m, **mp;

	if (m0 == 0)
		return;

	/* look for trailing OOB packet or ending control chain */ 
	for (mp = &sb->sb_mb; m = *mp; mp = &((*mp)->m_nextpkt)) {
	    again:
		switch (m->m_type) {

		case MT_OOBDATA:
			continue;		/* WANT next train */

		case MT_CONTROL:
			if (m = m->m_next)
				goto again;	/* inspect THIS train further */
		}
		break;
	}

	/*
	 * Put the first mbuf on the queue.
	 * Note this permits zero length records.
	 */
	sballoc(sb, m0);
	m0->m_nextpkt = *mp;
	*mp = m0;

	/* isolate remainder of chain past first mbuf */
	m = m0->m_next;
	m0->m_next = 0;

	/* relay end of record mark */
	if (m && (m0->m_flags & M_EOR)) {
		m0->m_flags &= ~M_EOR;
		m->m_flags |= M_EOR;
	}
	sbcompress(sb, m, m0);
}

/*
 * Append address, optional control, and optional data, into the recieve
 * socket buffer as a new trailing record. Data must have a packet header.
 * Returns 0 if no space in the socket buffer or insufficient mbufs.
 * Used by a protocol to place message contents and peer address into
 * socket buffer efficiently.
 */
int
sbappendaddr(struct sockbuf *sb, struct sockaddr *asa, struct mbuf *m0,
	struct mbuf *control)
{
	struct mbuf *m, *n;
	int space = asa->sa_len;

	/* use packet header to obtain total length without chain walk */ 
	if (m0)
		space += m0->m_pkthdr.len;

	/* find trailing control mbuf, if any control mbufs are supplied */
	for (n = control; n; n = n->m_next) {
		space += n->m_len;
		if (n->m_next == 0)
			break;
	}

	/* can't add to socket buffer? */
	if (space > sbspace(sb))
		return (0);

	/* allocate a mbuf to hold a copy of the sockaddr */
#ifdef DIAGNOSTIC
	if (asa->sa_len > MLEN)
		return (0);
#endif

	MGET(m, M_DONTWAIT, MT_SONAME);
	if (m == 0)
		return (0);

	m->m_len = asa->sa_len;
	(void) memcpy(mtod(m, caddr_t), (caddr_t)asa, asa->sa_len);

	/* concatenate data after control ... */
	if (n)
		n->m_next = m0;
	else
		control = m0;

	/* ... and control after sockaddr */
	m->m_next = control;

	/* put sockaddr, control, and data mbuf chain into socket buffer ... */
	for (n = m; n; n = n->m_next)
		sballoc(sb, n);

	/* ... as next record */
	if (n = sb->sb_mb) {
		while (n->m_nextpkt)
			n = n->m_nextpkt;
		n->m_nextpkt = m;
	} else
		sb->sb_mb = m;

	return (1);
}

/*
 * Append control, and optional data, into the socket buffer as a new
 * trailing record. Returns 0 if no space in the socket buffer.
 * Used by a protocol to place control and message into a socket buffer.
 */
int
sbappendcontrol(struct sockbuf *sb, struct mbuf *m0,
	struct mbuf *control)
{
	struct mbuf *m, *n;
	int space = 0;

#ifdef DIAGNOSTIC
	if (control == 0)
		panic("sbappendcontrol");
#endif
	/* use packet header to obtain total length without chain walk */ 
	if (m0)
		space += m0->m_pkthdr.len;

	/* find trailing control mbuf, talley control size */
	for (m = control; ; m = m->m_next) {
		space += m->m_len;
		if (m->m_next == 0)
			break;
	}

	/* save pointer to last control buffer */
	n = m;

	/* socket buffer too small */
	if (space > sbspace(sb))
		return (0);

	/* concatenate data to control */
	n->m_next = m0;

	/* place control and data into socket buffer ... */
	for (m = control; m; m = m->m_next)
		sballoc(sb, m);

	/* ... as next packet */
	if (n = sb->sb_mb) {
		while (n->m_nextpkt)
			n = n->m_nextpkt;
		n->m_nextpkt = control;
	} else
		sb->sb_mb = control;

	return (1);
}

/*
 * Compress mbuf chain "m" and append into the socket
 * buffer following mbuf "p".  If "p" is null, the buffer
 * is presumed empty.
 */
void
sbcompress(struct sockbuf *sb, struct mbuf *m, struct mbuf *p)
{
	int eor = 0;
	struct mbuf *o;

	/* walk new chain to be added to the socket buffer */
	while (m) {

		/* save end of record mark */
		eor |= m->m_flags & M_EOR;

		/* free any adjacent empty entry not marking a end of record */
		if (m->m_len == 0 &&
		    (eor == 0 ||
		     (((o = m->m_next) || (o = p)) &&
		      o->m_type == m->m_type))) {
			m = m_free(m);
			continue;
		}

		/* copy mbuf contents into previous if possible and free */
		if (p && (p->m_flags & (M_EXT | M_EOR)) == 0 &&
		    (p->m_data + p->m_len + m->m_len) < &p->m_dat[MLEN] &&
		    p->m_type == m->m_type) {
			(void) memcpy(mtod(p, caddr_t) + p->m_len, mtod(m, caddr_t),
			    (unsigned)m->m_len);
			p->m_len += m->m_len;
			sb->sb_cc += m->m_len;
			m = m_free(m);
			continue;
		}

		/* link onto buffer, or become the contents of the empty buffer */
		if (p)
			p->m_next = m;
		else
			sb->sb_mb = m;
		sballoc(sb, m);

		p = m;
		m->m_flags &= ~M_EOR;
		m = m->m_next;
		p->m_next = 0;
	}

	/* encountered and end of record in new chain */
	if (eor) {
		if (p)
			p->m_flags |= eor;
#ifdef DIAGNOSTIC
		/* end of record never in single mbuf additions */
		else
			panic("sbcompress");
#endif
	}
}

/*
 * Reclaim "len" bytes of data from the head of a sockbuf. Used by
 * protocol and socket delivery code to consume queue contents.
 */
void
sbdrop(struct sockbuf *sb, int len)
{
	struct mbuf *m, *mn, *next;

	/* next record to skip to if we consume leading record */
	next = (m = sb->sb_mb) ? m->m_nextpkt : 0;

	/* consume mbufs */
	while (len > 0) {

		/* nothing more to consume in this chain, find another record */
		if (m == 0) {

			/* no more records to consume */
			if (next == 0)
				panic("sbdrop");

			/* consume from next record */
			m = next;
			next = m->m_nextpkt;
		}

		/* consume the partial contents of the mbuf and terminate */
		if (m->m_len > len) {
			m->m_len -= len;
			m->m_data += len;
			sb->sb_cc -= len;
			break;
		}

		/* otherwise, consume the mbuf entirely and find the next */
		len -= m->m_len;
		sbfree(sb, m);
		MFREE(m, mn);
		m = mn;
	}

	/* empty mbuf left on list */
	while (m && m->m_len == 0) {
		sbfree(sb, m);
		MFREE(m, mn);
		m = mn;
	}

	/* return remainder to socket buffer */
	if (m) {
		sb->sb_mb = m;
		m->m_nextpkt = next;
	} else
		sb->sb_mb = next;
}

/* Reclaim the leading record of a socket buffer. */
void
sbdroprecord(struct sockbuf *sb)
{
	struct mbuf *m, *mn;

	/* anything in the socket buffer? */
	m = sb->sb_mb;
	if (m) {

		/* advance to next record */
		sb->sb_mb = m->m_nextpkt;

		/* free any mbufs in the record */
		do {
			sbfree(sb, m);
			MFREE(m, mn);
		} while (m = mn);
	}
}

/*
 * Socket buffer event and exclusive access.
 */

/* Queue a process for a select on a socket buffer. */
void
sbselqueue(struct sockbuf *sb, struct proc *cp)
{
	struct proc *p;

	/* already select()ing by a existing process? */
	if (sb->sb_sel &&
	    (p = pfind(sb->sb_sel)) && p->p_wchan == (caddr_t)&selwait)
		sb->sb_flags |= SB_COLL;
	else {
		sb->sb_sel = cp->p_pid;
		sb->sb_flags |= SB_SEL;
	}
}

/* Wakeup or select() any processes associated with the socket buffer. */
void
sbwakeup(struct sockbuf *sb)
{

	/* is there a process select()ing the socket buffer? */
	if (sb->sb_sel) {
		selwakeup(sb->sb_sel, sb->sb_flags & SB_COLL);
		sb->sb_sel = 0;
		sb->sb_flags &= ~(SB_SEL|SB_COLL);
	}

	/* is there a process waiting for the socket buffer? */
	if (sb->sb_flags & SB_WAIT) {
		sb->sb_flags &= ~SB_WAIT;
		wakeup((caddr_t)&sb->sb_cc);
	}
}

/* Wait for data to arrive at/drain from a socket buffer. */
int
sbwait(struct sockbuf *sb)
{

	sb->sb_flags |= SB_WAIT;
	return (tsleep((caddr_t)&sb->sb_cc,
	    (sb->sb_flags & SB_NOINTR) ? PSOCK : PSOCK | PCATCH, netio,
	    sb->sb_timeo));
}

/* 
 * Lock a sockbuf already known to be locked;
 * return any error returned from sleep (EINTR).
 */
int
sb_lock(struct sockbuf *sb)
{
	int error;

	while (sb->sb_flags & SB_LOCK) {
		sb->sb_flags |= SB_WANT;
		if (error = tsleep((caddr_t)&sb->sb_flags, 
		    (sb->sb_flags & SB_NOINTR) ? PSOCK : PSOCK|PCATCH,
			netio, 0))
			return (error);
	}
	sb->sb_flags |= SB_LOCK;
	return (0);
}
