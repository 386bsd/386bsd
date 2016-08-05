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
 * Operations on sockets, the core of the socket implementation.
 * These routines are called generically by the routines in
 * the top half, or from a system process, or from a protocol and
 * they implement the abstract semantics of socket operations.
 *
 * $Id: sockops.c,v 1.2 95/02/24 10:40:28 bill Exp Locker: bill $
 */

#include "sys/param.h"
#include "sys/stat.h"
#include "sys/file.h"
#include "uio.h"
#include "sys/errno.h"
#include "proc.h"
#include "malloc.h"
#include "signalvar.h"
#include "mbuf.h"
#include "domain.h"
#include "kernel.h" /* hz/tick */
#include "socketvar.h"
#include "protosw.h"
#include "resourcevar.h"
#include "prototypes.h"

/* strings for sleep message: */
static char	netcls[] = "netcls";


/* Create a socket */
int
socreate(int dom, struct socket **aso, int type, int proto)
{
	struct protosw *prp;
	struct socket *so;
	int error;

	/* valid protocol and type for domain */
	if (proto)
		prp = pffindproto(dom, proto, type);
	else
		prp = pffindtype(dom, type);
	if (prp == 0)
		return (EPROTONOSUPPORT);
	if (prp->pr_type != type)
		return (EPROTOTYPE);

	/* allocate a socket */
	MALLOC(so, struct socket *, sizeof(*so), M_SOCKET, M_WAIT | M_ZERO_IT);
	so->so_type = type;
	if (curproc->p_ucred->cr_uid == 0)	/* XXX */
		so->so_state = SS_PRIV;
	so->so_proto = prp;


	/* attach socket to protocol */
	error =
	    (*prp->pr_usrreq)(so, PRU_ATTACH,
		(struct mbuf *)0, (struct mbuf *)proto, (struct mbuf *)0);
	if (error) {
		so->so_state |= SS_NOFDREF;
		sofree(so);
		return (error);
	}

	*aso = so;
	return (0);
}

/* attach a name to a socket */
int
sobind(struct socket *so, struct mbuf *sockaddrnam)
{
	int error, s = splnet();

	/* request protocol for it to name a socket with an addr */
	error =
	    (*so->so_proto->pr_usrreq)(so, PRU_BIND,
		(struct mbuf *)0, sockaddrnam, (struct mbuf *)0);
	splx(s);
	return (error);
}

/* make socket a passive listener with a empty queue of a given size */
int
solisten(struct socket *so, int backlog)
{
	int s = splnet(), error;

	/* condition socket */
	error =
	    (*so->so_proto->pr_usrreq)(so, PRU_LISTEN,
		(struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0);
	if (error) {
		splx(s);
		return (error);
	}

	/* */
	if (so->so_q == 0)
		so->so_options |= SO_ACCEPTCONN;
	if (backlog < 0)
		backlog = 0;
	so->so_qlimit = min(backlog, SOMAXCONN);
	splx(s);
	return (0);
}

/* release (and possibly reclaim) a socket. */
void
sofree(struct socket *so)
{

	if (so->so_pcb || (so->so_state & SS_NOFDREF) == 0)
		return;
	if (so->so_head) {
		if (!soqremque(so, 0) && !soqremque(so, 1))
			panic("sofree");
		so->so_head = 0;
	}

	sbrelease(&so->so_snd);
	sorflush(so);
	FREE(so, M_SOCKET);
}

/*
 * Close a socket on last file table reference removal.
 * Initiate disconnect if connected.
 * Free socket when disconnect complete.
 */
int
soclose(struct socket *so)
{
	int s = splnet();		/* conservative */
	int error = 0;

	if (so->so_options & SO_ACCEPTCONN) {
		while (so->so_q0)
			(void) soabort(so->so_q0);
		while (so->so_q)
			(void) soabort(so->so_q);
	}


	if (so->so_pcb == 0)
		goto discard;

	if (so->so_state & SS_ISCONNECTED) {
		if ((so->so_state & SS_ISDISCONNECTING) == 0) {
			error = sodisconnect(so);
			if (error)
				goto drop;
		}
		if (so->so_options & SO_LINGER) {
			if ((so->so_state & SS_ISDISCONNECTING) &&
			    (so->so_state & SS_NBIO))
				goto drop;
			while (so->so_state & SS_ISCONNECTED)
				if (error = tsleep((caddr_t)&so->so_timeo,
				    PSOCK | PCATCH, netcls, so->so_linger))
					break;
		}
	}
drop:
	if (so->so_pcb) {
		int error2 =
		    (*so->so_proto->pr_usrreq)(so, PRU_DETACH,
			(struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0);
		if (error == 0)
			error = error2;
	}
discard:
	if (so->so_state & SS_NOFDREF)
		panic("soclose");
	so->so_state |= SS_NOFDREF;
	sofree(so);
	splx(s);
	return (error);
}


/*
 * abort any connections to the socket (called at splnet).
 */
int
soabort(struct socket *so)
{

	return (
	    (*so->so_proto->pr_usrreq)(so, PRU_ABORT,
		(struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0));
}

/* */
int
soaccept(struct socket *so, struct mbuf *sockaddrnam)
{
	int s, error;

#ifdef DIAGNOSTIC
	if ((so->so_state & SS_NOFDREF) == 0)
		panic("soaccept: !NOFDREF");
#endif
	s = splnet();
	so->so_state &= ~SS_NOFDREF;
	error =
	    (*so->so_proto->pr_usrreq)(so, PRU_ACCEPT,
		(struct mbuf *)0, sockaddrnam, (struct mbuf *)0);
	splx(s);
	return (error);
}

/* attempt an active open on a socket to a socket address */
int
soconnect(struct socket *so, struct mbuf *sockaddrnam)
{
	int s, error;

	if (so->so_options & SO_ACCEPTCONN)
		return (EOPNOTSUPP);

	/*
	 * If protocol is connection-based, can only connect once.
	 * Otherwise, if connected, try to disconnect first.
	 * This allows user to disconnect by connecting to, e.g.,
	 * a null address.
	 */
	s = splnet();
	if (so->so_state & (SS_ISCONNECTED|SS_ISCONNECTING) &&
	    ((so->so_proto->pr_flags & PR_CONNREQUIRED) ||
		(error = sodisconnect(so))))
		/* error = EISCONN */;
	else
		error =
		    (*so->so_proto->pr_usrreq)(so, PRU_CONNECT,
		    	(struct mbuf *)0, sockaddrnam, (struct mbuf *)0);
	splx(s);
	return (error);
}

/* connect a socket pair via a protocol(e.g. a pipe()) */
int
soconnect2(struct socket *so1, struct socket *so2)
{
	int error, s = splnet();

	error = (*so1->so_proto->pr_usrreq)(so1, PRU_CONNECT2,
	    (struct mbuf *)0, (struct mbuf *)so2, (struct mbuf *)0);
	splx(s);
	return (error);
}

/* disconnect a connected socket */
int
sodisconnect(struct socket *so)
{
	int error, s = splnet();

	/* if not connected or disconnecting, don't bother */
	if ((so->so_state & SS_ISCONNECTED) == 0) {
		error = ENOTCONN;
		goto bad;
	}
	if (so->so_state & SS_ISDISCONNECTING) {
		error = EALREADY;
		goto bad;
	}

	error = (*so->so_proto->pr_usrreq)(so, PRU_DISCONNECT,
	    (struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0);
bad:
	splx(s);
	return (error);
}

/*
 * Send data via a socket.
 *
 * Returns nonzero on error, timeout or signal; callers
 * must check for short counts if EINTR/ERESTART are returned.
 * Data and control buffers are freed on return.
 */
int
sosend(struct socket *so, struct mbuf *addr, struct uio *uio,
	struct mbuf *top, struct mbuf *control, int flags)
{
	struct mbuf **mp, *m;
	long space, len, resid;
	int clen, error, s, dontroute, mlen;
	int atomic = sosendallatonce(so) || top;

	/* should routing be bypassed? */
	dontroute =
	    (flags & MSG_DONTROUTE) &&
		(so->so_options & SO_DONTROUTE) == 0 &&
		    (so->so_proto->pr_flags & PR_ATOMIC);

	/*
	 * Account for transfer and obtain total size of transfer
	 * The data to be sent is described by "uio" if nonzero,
	 * otherwise by the mbuf chain "top" (which must be null
	 * if uio is not).
	 */
	if (uio) {
		uio->uio_procp->p_stats->p_ru.ru_msgsnd++;
		resid = uio->uio_resid;
	} else {
		curproc->p_stats->p_ru.ru_msgsnd++;
		resid = top->m_pkthdr.len;
	}

	/* any control info as well? */
	if (control)
		clen = control->m_len;
	else
		clen = 0;


	/* lock socket send buffer to block any other senders */
restart:
	if (error = sblock(&so->so_snd))
		goto out;

	/* */
	do {
		s = splnet();
#define	snderr(errno)	{ error = errno; splx(s); goto release; }

		/* any pending error to terminate send operation? */
		if (so->so_state & SS_CANTSENDMORE)
			snderr(EPIPE);
		if (so->so_error)
			snderr(so->so_error);
		if ((so->so_state & SS_ISCONNECTED) == 0) {
			if (so->so_proto->pr_flags & PR_CONNREQUIRED) {
				if ((so->so_state & SS_ISCONFIRMING) == 0 &&
				    !(resid == 0 && clen != 0))
					snderr(ENOTCONN);
			} else if (addr == 0)
				snderr(EDESTADDRREQ);
		}

		/* is there enough socket buffer space? */
		space = sbspace(&so->so_snd);
		if (space < resid + clen &&
		    (atomic || space < so->so_snd.sb_lowat
			|| space < clen)) {
			/*
 			 * If the send must go all at once and
			 * the message is larger than can be buffered,
 			 * indicate an hard error.
			 */
			if (atomic && resid > so->so_snd.sb_hiwat ||
			    clen > so->so_snd.sb_hiwat)
				snderr(EMSGSIZE);
 			/*
			 * If can't block, and not enough buffer
			 * space, then indicate that this would
			 * might block.
			 */
			if (so->so_state & SS_NBIO)
				snderr(EWOULDBLOCK);

			/* unlock buffer before waiting, then restart */
			sbunlock(&so->so_snd);
			error = sbwait(&so->so_snd);
			splx(s);
			if (error)
				goto out;
			goto restart;
		}
		splx(s);

		/* move contents into socket buffer */
/* Otherwise, if nonblocking, send as much as possible.
 * Data provided in mbuf chain must be small
 * enough to send all at once.
 */
		mp = &top;
		space -= clen;
		do {
			/* contents are already prepackaged in "top". */
		    if (uio == NULL) {
			resid = 0;
			if (flags & MSG_EOR)
				top->m_flags |= M_EOR;
		    } else
		    do {

			/* at beginning of list, obtain a packet header */ 
			if (top == 0) {
				MGETHDR(m, M_WAIT, MT_DATA);
				mlen = MHLEN;
				m->m_pkthdr.len = 0;
				m->m_pkthdr.rcvif = (struct ifnet *)0;
			} else {
				MGET(m, M_WAIT, MT_DATA);
				mlen = MLEN;
			}

			/* upgrade a mbuf to a cluster */
			if (resid >= MINCLSIZE && space >= MCLBYTES) {
				MCLGET(m, M_WAIT);

				/* no upgrade? */
				if ((m->m_flags & M_EXT) == 0)
					goto nopages;
				mlen = MCLBYTES;
#ifdef	MAPPED_MBUFS
				len = min(MCLBYTES, resid);
#else
				if (top == 0) {
					len = min(MCLBYTES - max_hdr, resid);
					m->m_data += max_hdr;
				} else
					len = min(MCLBYTES, resid);
#endif
				space -= MCLBYTES;
			} else {
nopages:
				len = min(min(mlen, resid), space);
				space -= len;
				/*
				 * For datagram protocols, leave room
				 * for protocol headers in first mbuf.
				 */
				if (atomic && top == 0 && len < mlen)
					MH_ALIGN(m, len);
			}

			/* copy send contents into buffer list */
			error = uiomove(mtod(m, caddr_t), (int)len, uio);
			m->m_len = len;
			*mp = m;
			top->m_pkthdr.len += len;
			mp = &m->m_next;

			/* reconcile bookkeeping */
			if (error)
				goto release;
			resid = uio->uio_resid;
			if (resid <= 0) {
				if (flags & MSG_EOR)
					top->m_flags |= M_EOR;
				break;
			}
		    } while (space > 0 && atomic);

		    /* pass to a protocol */
		    if (dontroute)
			    so->so_options |= SO_DONTROUTE;
		    s = splnet();				/* XXX */
		    error = (*so->so_proto->pr_usrreq)(so,
			(flags & MSG_OOB) ? PRU_SENDOOB : PRU_SEND,
			top, addr, control);
		    splx(s);
		    if (dontroute)
			    so->so_options &= ~SO_DONTROUTE;

		    /* prepare for next iteration, check for error */
		    clen = 0;
		    control = 0;
		    top = 0;
		    mp = &top;
		    if (error)
			goto release;

		} while (resid && space > 0);
#undef snderr
	} while (resid);

	/* unlock socket buffer */
release:
	sbunlock(&so->so_snd);

	/* free buffers */
out:
	if (top)
		m_freem(top);
	if (control)
		m_freem(control);
	return (error);
}

/*
 * Implement receive operations on a socket.
 * We depend on the way that records are added to the sockbuf
 * by sbappend*.  In particular, each record (mbufs linked through m_next)
 * must begin with an address if the protocol so specifies,
 * followed by an optional mbuf or mbufs containing ancillary data,
 * and then zero or more mbufs of data.
 * In order to avoid blocking network interrupts for the entire time here,
 * we splx() while doing the actual copy to user space.
 * Although the sockbuf is locked, new data may still be appended,
 * and thus we must maintain consistency of the sockbuf during that time.
 *
 * The caller may receive the data as a single mbuf chain by supplying
 * an mbuf **mp0 for use in returning the chain.  The uio is then used
 * only for the count in uio_resid.
 */
int
soreceive(struct socket *so, struct mbuf **paddr, struct uio *uio,
	struct mbuf **mp0, struct mbuf **controlp, int *flagsp)
{
	struct mbuf *m, **mp, *nextrecord;
	struct protosw *pr = so->so_proto;
	int flags, len, error, s, offset, moff, type,
	    orig_resid = uio->uio_resid;

	/* canononicalize arguments */
	mp = mp0;
	if (mp)
		*mp = (struct mbuf *)0;
	if (paddr)
		*paddr = 0;
	if (controlp)
		*controlp = 0;
	if (flagsp)
		flags = *flagsp &~ MSG_EOR;
	else
		flags = 0;

	/* probe for any OOB data first */
	if (flags & MSG_OOB) {
		m = m_get(M_WAIT, MT_DATA);
		error = (*pr->pr_usrreq)(so, PRU_RCVOOB,
		    m, (struct mbuf *)(flags & MSG_PEEK), (struct mbuf *)0);
		if (error)
			goto bad;
		do {
			error = uiomove(mtod(m, caddr_t),
			    (int) min(uio->uio_resid, m->m_len), uio);
			m = m_free(m);
		} while (uio->uio_resid && error == 0 && m);
bad:
		if (m)
			m_freem(m);
		return (error);
	}

	/* first receive on confirming socket, socket not yet open */
	if (so->so_state & SS_ISCONFIRMING && uio->uio_resid)
		(*pr->pr_usrreq)(so, PRU_RCVD, (struct mbuf *)0,
		    (struct mbuf *)0, (struct mbuf *)0);

	/* lock receive buffer */
restart:
	if (error = sblock(&so->so_rcv))
		return (error);

	/*
	 * If we have less data than requested, block awaiting more
	 * (subject to any timeout) if:
	 *   1. the current count is less than the low water mark, or
	 *   2. MSG_WAITALL is set, and it is possible to do the entire
	 *	receive operation at once if we block (resid <= hiwat).
	 * If MSG_WAITALL is set but resid is larger than the receive buffer,
	 * we have to do the receive in sections, and thus risk returning
	 * a short count if a timeout or signal occurs after we start.
	 */
	s = splnet();
	m = so->so_rcv.sb_mb;
	while (m == 0 || so->so_rcv.sb_cc < uio->uio_resid &&
	    (so->so_rcv.sb_cc < so->so_rcv.sb_lowat ||
	    ((flags & MSG_WAITALL) && uio->uio_resid <= so->so_rcv.sb_hiwat)) &&
	    m->m_nextpkt == 0 && (pr->pr_flags & PR_ATOMIC) == 0) {
#ifdef DIAGNOSTIC
		if (m == 0 && so->so_rcv.sb_cc)
			panic("receive 1");
#endif
		/* check for errors */
		if (so->so_error) {
			if (m)
				break;
			error = so->so_error;
			if ((flags & MSG_PEEK) == 0)
				so->so_error = 0;
			goto release;
		}
		if (so->so_state & SS_CANTRCVMORE) {
			if (m)
				break;
			else
				goto release;
		}

		/* scan chain for */
		for (; m; m = m->m_next)
			if (m->m_type == MT_OOBDATA  || (m->m_flags & M_EOR)) {
				m = so->so_rcv.sb_mb;
				goto dontblock;
			}

		if ((so->so_state & (SS_ISCONNECTED|SS_ISCONNECTING)) == 0 &&
		    (so->so_proto->pr_flags & PR_CONNREQUIRED)) {
			error = ENOTCONN;
			goto release;
		}
		if (uio->uio_resid == 0)
			goto release;
		if (so->so_state & SS_NBIO) {
			error = EWOULDBLOCK;
			goto release;
		}

		sbunlock(&so->so_rcv);
		error = sbwait(&so->so_rcv);
		splx(s);
		if (error)
			return (error);
		goto restart;
	}

dontblock:
	uio->uio_procp->p_stats->p_ru.ru_msgrcv++;
	nextrecord = m->m_nextpkt;
	if (pr->pr_flags & PR_ADDR) {
#ifdef DIAGNOSTIC
		if (m->m_type != MT_SONAME)
			panic("receive 1a");
#endif
		orig_resid = 0;
		if (flags & MSG_PEEK) {
			if (paddr)
				*paddr = m_copym(m, 0, m->m_len, M_WAIT);
			m = m->m_next;
		} else {
			sbfree(&so->so_rcv, m);
			if (paddr) {
				*paddr = m;
				so->so_rcv.sb_mb = m->m_next;
				m->m_next = 0;
				m = so->so_rcv.sb_mb;
			} else {
				MFREE(m, so->so_rcv.sb_mb);
				m = so->so_rcv.sb_mb;
			}
		}
	}

	while (m && m->m_type == MT_CONTROL && error == 0) {

		if (flags & MSG_PEEK) {
			if (controlp)
				*controlp = m_copym(m, 0, m->m_len, M_WAIT);
			m = m->m_next;
		} else {
			sbfree(&so->so_rcv, m);
			if (controlp) {
				if (pr->pr_domain->dom_externalize &&
				    mtod(m, struct cmsghdr *)->cmsg_type ==
				    SCM_RIGHTS)
				   error = (*pr->pr_domain->dom_externalize)
					(m, uio->uio_procp);
				*controlp = m;
				so->so_rcv.sb_mb = m->m_next;
				m->m_next = 0;
				m = so->so_rcv.sb_mb;
			} else {
				MFREE(m, so->so_rcv.sb_mb);
				m = so->so_rcv.sb_mb;
			}
		}

		if (controlp) {
			orig_resid = 0;
			controlp = &(*controlp)->m_next;
		}
	}

	if (m) {
		if ((flags & MSG_PEEK) == 0)
			m->m_nextpkt = nextrecord;
		type = m->m_type;
		if (type == MT_OOBDATA)
			flags |= MSG_OOB;
	}

	moff = 0;
	offset = 0;
	while (m && uio->uio_resid > 0 && error == 0) {
		if (m->m_type == MT_OOBDATA) {
			if (type != MT_OOBDATA)
				break;
		} else if (type == MT_OOBDATA)
			break;
#ifdef DIAGNOSTIC
		else if (m->m_type != MT_DATA && m->m_type != MT_HEADER)
			panic("receive 3");
#endif
		so->so_state &= ~SS_RCVATMARK;
		len = uio->uio_resid;
		if (so->so_oobmark && len > so->so_oobmark - offset)
			len = so->so_oobmark - offset;
		if (len > m->m_len - moff)
			len = m->m_len - moff;
		/*
		 * If mp is set, just pass back the mbufs.
		 * Otherwise copy them out via the uio, then free.
		 * Sockbuf must be consistent here (points to current mbuf,
		 * it points to next record) when we drop priority;
		 * we must note any additions to the sockbuf when we
		 * block interrupts again.
		 */
		if (mp == 0) {
			splx(s);
			error = uiomove(mtod(m, caddr_t) + moff, (int)len, uio);
			s = splnet();
		} else
			uio->uio_resid -= len;

		if (len == m->m_len - moff) {
			if (m->m_flags & M_EOR)
				flags |= MSG_EOR;
			if (flags & MSG_PEEK) {
				m = m->m_next;
				moff = 0;
			} else {
				nextrecord = m->m_nextpkt;
				sbfree(&so->so_rcv, m);
				if (mp) {
					*mp = m;
					mp = &m->m_next;
					so->so_rcv.sb_mb = m = m->m_next;
					*mp = (struct mbuf *)0;
				} else {
					MFREE(m, so->so_rcv.sb_mb);
					m = so->so_rcv.sb_mb;
				}
				if (m)
					m->m_nextpkt = nextrecord;
			}
		} else {
			if (flags & MSG_PEEK)
				moff += len;
			else {
				if (mp)
					*mp = m_copym(m, 0, len, M_WAIT);
				m->m_data += len;
				m->m_len -= len;
				so->so_rcv.sb_cc -= len;
			}
		}

		if (so->so_oobmark) {
			if ((flags & MSG_PEEK) == 0) {
				so->so_oobmark -= len;
				if (so->so_oobmark == 0) {
					so->so_state |= SS_RCVATMARK;
					break;
				}
			} else
				offset += len;
		}

		if (flags & MSG_EOR)
			break;
		/*
		 * If the MSG_WAITALL flag is set (for non-atomic socket),
		 * we must not quit until "uio->uio_resid == 0" or an error
		 * termination.  If a signal/timeout occurs, return
		 * with a short count but without error.
		 * Keep sockbuf locked against other readers.
		 */
		while (flags & MSG_WAITALL && m == 0 && uio->uio_resid > 0 &&
		    !sosendallatonce(so) && !nextrecord) {
			if (so->so_error || so->so_state & SS_CANTRCVMORE)
				break;
			error = sbwait(&so->so_rcv);
			if (error) {
				sbunlock(&so->so_rcv);
				splx(s);
				return (0);
			}
			if (m = so->so_rcv.sb_mb)
				nextrecord = m->m_nextpkt;
		}
	}

	if (m && pr->pr_flags & PR_ATOMIC) {
		flags |= MSG_TRUNC;
		if ((flags & MSG_PEEK) == 0)
			(void) sbdroprecord(&so->so_rcv);
	}

	if ((flags & MSG_PEEK) == 0) {
		if (m == 0)
			so->so_rcv.sb_mb = nextrecord;
		if (pr->pr_flags & PR_WANTRCVD && so->so_pcb)
			(*pr->pr_usrreq)(so, PRU_RCVD, (struct mbuf *)0,
			    (struct mbuf *)flags, (struct mbuf *)0,
			    (struct mbuf *)0);
	}

	if (orig_resid == uio->uio_resid && orig_resid &&
	    (flags & MSG_EOR) == 0 && (so->so_state & SS_CANTRCVMORE) == 0) {
		sbunlock(&so->so_rcv);
		splx(s);
		goto restart;
	}
		
	if (flagsp)
		*flagsp |= flags;
release:
	sbunlock(&so->so_rcv);
	splx(s);
	return (error);
}

/* shutdown a socket */
int
soshutdown(struct socket *so, int how)
{
	struct protosw *pr = so->so_proto;

	how++;
	if (how & FREAD)
		sorflush(so);
	if (how & FWRITE)
		return ((*pr->pr_usrreq)(so, PRU_SHUTDOWN,
		    (struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0));
	return (0);
}

/* */
void
sorflush(struct socket *so)
{
	struct sockbuf *sb = &so->so_rcv, asb;
	struct protosw *pr = so->so_proto;
	int s;

	sb->sb_flags |= SB_NOINTR;

	(void) sblock(sb);
	s = splimp();
	socantrcvmore(so);
	sbunlock(sb);
	asb = *sb;
	(void) memset((caddr_t)sb, 0, sizeof (*sb));
	splx(s);

	if (pr->pr_flags & PR_RIGHTS && pr->pr_domain->dom_dispose)
		(*pr->pr_domain->dom_dispose)(asb.sb_mb);

	sbrelease(&asb);
}

/**/
int
sosetopt(struct socket *so, int level, int optname, struct mbuf *m0)
{
	int error = 0;
	struct mbuf *m = m0;

	/* if not a option implemented by the socket layer, pass down */
	if (level != SOL_SOCKET) {
		if (so->so_proto && so->so_proto->pr_ctloutput)
			return ((*so->so_proto->pr_ctloutput)
				  (PRCO_SETOPT, so, level, optname, &m0));
		error = ENOPROTOOPT;
	} else {

		switch (optname) {

			/* */
		case SO_LINGER:
			if (m == NULL || m->m_len != sizeof (struct linger)) {
				error = EINVAL;
				goto bad;
			}
			so->so_linger = mtod(m, struct linger *)->l_linger;
			/* fall thru... */

			/* set/clear bits in socket option field */
		case SO_DEBUG:
		case SO_KEEPALIVE:
		case SO_DONTROUTE:
		case SO_USELOOPBACK:
		case SO_BROADCAST:
		case SO_REUSEADDR:
		case SO_OOBINLINE:
			if (m == NULL || m->m_len < sizeof (int)) {
				error = EINVAL;
				goto bad;
			}
			if (*mtod(m, int *))
				so->so_options |= optname;
			else
				so->so_options &= ~optname;
			break;

			/* options with single integer parameter */
		case SO_SNDBUF:
		case SO_RCVBUF:
		case SO_SNDLOWAT:
		case SO_RCVLOWAT:
			if (m == NULL || m->m_len < sizeof (int)) {
				error = EINVAL;
				goto bad;
			}
			switch (optname) {

				/* set recieve or send buffer length */
			case SO_SNDBUF:
			case SO_RCVBUF:
				if (sbreserve(optname == SO_SNDBUF ?
				    &so->so_snd : &so->so_rcv,
				    (u_long) *mtod(m, int *)) == 0) {
					error = ENOBUFS;
					goto bad;
				}
				break;

				/* */
			case SO_SNDLOWAT:
				so->so_snd.sb_lowat = *mtod(m, int *);
				break;
			case SO_RCVLOWAT:
				so->so_rcv.sb_lowat = *mtod(m, int *);
				break;
			}
			break;

		case SO_SNDTIMEO:
		case SO_RCVTIMEO:
		    {
			struct timeval *tv;
			short val;

			if (m == NULL || m->m_len < sizeof (*tv)) {
				error = EINVAL;
				goto bad;
			}
			tv = mtod(m, struct timeval *);
			if (tv->tv_sec > SHRT_MAX / hz - hz) {
				error = EDOM;
				goto bad;
			}
			val = tv->tv_sec * hz + tv->tv_usec / tick;

			switch (optname) {

			case SO_SNDTIMEO:
				so->so_snd.sb_timeo = val;
				break;
			case SO_RCVTIMEO:
				so->so_rcv.sb_timeo = val;
				break;
			}
			break;
		    }

		default:
			error = ENOPROTOOPT;
			break;
		}
	}
bad:
	if (m)
		(void) m_free(m);
	return (error);
}

int
sogetopt(struct socket *so, int level, int optname, struct mbuf **mp)
{
	struct mbuf *m;

	if (level != SOL_SOCKET) {
		if (so->so_proto && so->so_proto->pr_ctloutput) {
			return ((*so->so_proto->pr_ctloutput)
				  (PRCO_GETOPT, so, level, optname, mp));
		} else
			return (ENOPROTOOPT);
	} else {
		m = m_get(M_WAIT, MT_SOOPTS);
		m->m_len = sizeof (int);

		switch (optname) {

		case SO_LINGER:
			m->m_len = sizeof (struct linger);
			mtod(m, struct linger *)->l_onoff =
				so->so_options & SO_LINGER;
			mtod(m, struct linger *)->l_linger = so->so_linger;
			break;

		case SO_USELOOPBACK:
		case SO_DONTROUTE:
		case SO_DEBUG:
		case SO_KEEPALIVE:
		case SO_REUSEADDR:
		case SO_BROADCAST:
		case SO_OOBINLINE:
			*mtod(m, int *) = so->so_options & optname;
			break;

		case SO_TYPE:
			*mtod(m, int *) = so->so_type;
			break;

		case SO_ERROR:
			*mtod(m, int *) = so->so_error;
			so->so_error = 0;
			break;

		case SO_SNDBUF:
			*mtod(m, int *) = so->so_snd.sb_hiwat;
			break;

		case SO_RCVBUF:
			*mtod(m, int *) = so->so_rcv.sb_hiwat;
			break;

		case SO_SNDLOWAT:
			*mtod(m, int *) = so->so_snd.sb_lowat;
			break;

		case SO_RCVLOWAT:
			*mtod(m, int *) = so->so_rcv.sb_lowat;
			break;

		case SO_SNDTIMEO:
		case SO_RCVTIMEO:
		    {
			int val = (optname == SO_SNDTIMEO ?
			     so->so_snd.sb_timeo : so->so_rcv.sb_timeo);

			m->m_len = sizeof(struct timeval);
			mtod(m, struct timeval *)->tv_sec = val / hz;
			mtod(m, struct timeval *)->tv_usec =
			    (val % hz) / tick;
			break;
		    }

		default:
			(void)m_free(m);
			return (ENOPROTOOPT);
		}
		*mp = m;
		return (0);
	}
}

void
sohasoutofband(struct socket *so)
{

	/* signal process group */
	signalpid(so->so_pgid);

	/* if selecting on socket for read, poke process */
	if (so->so_rcv.sb_sel) {
		selwakeup(so->so_rcv.sb_sel, so->so_rcv.sb_flags & SB_COLL);
		so->so_rcv.sb_sel = 0;
		so->so_rcv.sb_flags &= ~SB_COLL;
	}
}

/*
 * Wakeup processes waiting on a socket buffer.
 * Do asynchronous notification via SIGIO
 * if the socket has the SS_ASYNC flag set.
 */
void
sowakeup(struct socket *so) 
{

	if (so->so_state & SS_ASYNC)
		signalpid(so->so_pgid);
}

/*
 * Socantsendmore indicates that no more data will be sent on the
 * socket; it would normally be applied to a socket when the user
 * informs the system that no more data is to be sent, by the protocol
 * code (in case PRU_SHUTDOWN).  Socantrcvmore indicates that no more data
 * will be received, and will normally be applied to the socket by a
 * protocol when it detects that the peer will send no more data.
 * Data queued for reading in the socket may yet be read.
 */
void
socantsendmore(struct socket *so)
{

	so->so_state |= SS_CANTSENDMORE;
	sowwakeup(so);
}

void
socantrcvmore(struct socket *so)
{

	so->so_state |= SS_CANTRCVMORE;
	sorwakeup(so);
}

/*
 * Adjust both of a socket's buffers so that it can hold the
 * desired amount of logical data in its receive and send buffers.
 */
int
soreserve(struct socket *so, u_long sndcc, u_long rcvcc)
{

	/* reserve the requested logical and physical resource */
	if (sbreserve(&so->so_snd, sndcc) == 0)
		return (ENOBUFS);
	if (sbreserve(&so->so_rcv, rcvcc) == 0) {
		sbrelease(&so->so_snd);
		return (ENOBUFS);
	}

	/* sanity check the water marks */
	if (so->so_rcv.sb_lowat == 0)
		so->so_rcv.sb_lowat = 1;
	if (so->so_snd.sb_lowat == 0)
		so->so_snd.sb_lowat = MCLBYTES;
	if (so->so_snd.sb_lowat > so->so_snd.sb_hiwat)
		so->so_snd.sb_lowat = so->so_snd.sb_hiwat;

	return (0);
}
