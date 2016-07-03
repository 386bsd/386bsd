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
 * $Id: sockconn.c,v 1.1 94/10/19 23:49:57 bill Exp $
 * 
 * connection-oriented socket primatives.
 */

#include "sys/param.h"
#include "sys/file.h"
#include "sys/errno.h"
#include "proc.h"
#include "signalvar.h"
#include "malloc.h"
#include "protosw.h"
#include "socketvar.h"
#include "prototypes.h"

/*
 * Procedures to manipulate state flags of socket
 * and do appropriate wakeups.  Normal sequence from the
 * active (originating) side is that soisconnecting() is
 * called during processing of connect() call,
 * resulting in an eventual call to soisconnected() if/when the
 * connection is established.  When the connection is torn down
 * soisdisconnecting() is called during processing of disconnect() call,
 * and soisdisconnected() is called when the connection to the peer
 * is totally severed.  The semantics of these routines are such that
 * connectionless protocols can call soisconnected() and soisdisconnected()
 * only, bypassing the in-progress calls when setting up a ``connection''
 * takes no time.
 *
 * From the passive side, a socket is created with
 * two queues of sockets: so_q0 for connections in progress
 * and so_q for connections already made and awaiting user acceptance.
 * As a protocol is preparing incoming connections, it creates a socket
 * structure queued on so_q0 by calling sonewconn().  When the connection
 * is established, soisconnected() is called, and transfers the
 * socket structure to so_q, making it available to accept().
 * 
 * If a socket is closed with sockets on either
 * so_q0 or so_q, these sockets are dropped.
 *
 * If higher level protocols are implemented in
 * the kernel, the wakeups done here will sometimes
 * cause software-interrupt process scheduling.
 */

soisconnecting(so)
	register struct socket *so;
{

	so->so_state &= ~(SS_ISCONNECTED|SS_ISDISCONNECTING);
	so->so_state |= SS_ISCONNECTING;
}

soisconnected(so)
	register struct socket *so;
{
	register struct socket *head = so->so_head;

	so->so_state &= ~(SS_ISCONNECTING|SS_ISDISCONNECTING|SS_ISCONFIRMING);
	so->so_state |= SS_ISCONNECTED;
	if (head && soqremque(so, 0)) {
		soqinsque(head, so, 1);
		sorwakeup(head);
		wakeup((caddr_t)&head->so_timeo);
	} else {
		wakeup((caddr_t)&so->so_timeo);
		sorwakeup(so);
		sowwakeup(so);
	}
}

soisdisconnecting(so)
	register struct socket *so;
{

	so->so_state &= ~SS_ISCONNECTING;
	so->so_state |= (SS_ISDISCONNECTING|SS_CANTRCVMORE|SS_CANTSENDMORE);
	wakeup((caddr_t)&so->so_timeo);
	sowwakeup(so);
	sorwakeup(so);
}

soisdisconnected(so)
	register struct socket *so;
{

	so->so_state &= ~(SS_ISCONNECTING|SS_ISCONNECTED|SS_ISDISCONNECTING);
	so->so_state |= (SS_CANTRCVMORE|SS_CANTSENDMORE);
	wakeup((caddr_t)&so->so_timeo);
	sowwakeup(so);
	sorwakeup(so);
}

/*
 * When an attempt at a new connection is noted on a socket
 * which accepts connections, sonewconn is called.  If the
 * connection is possible (subject to space constraints, etc.)
 * then we allocate a new structure, propoerly linked into the
 * data structure of the original socket, and return this.
 * Connstatus may be 0, or SO_ISCONFIRMING, or SO_ISCONNECTED.
 *
 * Currently, sonewconn() is defined as sonewconn1() in socketvar.h
 * to catch calls that are missing the (new) second parameter.
 */
struct socket *
sonewconn1(head, connstatus)
	register struct socket *head;
	int connstatus;
{
	register struct socket *so;
	int soqueue = connstatus ? 1 : 0;

	if (head->so_qlen + head->so_q0len > 3 * head->so_qlimit / 2)
		return ((struct socket *)0);
	MALLOC(so, struct socket *, sizeof(*so), M_SOCKET, M_NOWAIT);
	if (so == NULL) 
		return ((struct socket *)0);
	(void) memset((caddr_t)so, 0, sizeof(*so));
	so->so_type = head->so_type;
	so->so_options = head->so_options &~ SO_ACCEPTCONN;
	so->so_linger = head->so_linger;
	so->so_state = head->so_state | SS_NOFDREF;
	so->so_proto = head->so_proto;
	so->so_timeo = head->so_timeo;
	so->so_pgid = head->so_pgid;
	(void) soreserve(so, head->so_snd.sb_hiwat, head->so_rcv.sb_hiwat);
	soqinsque(head, so, soqueue);
	if ((*so->so_proto->pr_usrreq)(so, PRU_ATTACH,
	    (struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0)) {
		(void) soqremque(so, soqueue);
		(void) free((caddr_t)so, M_SOCKET);
		return ((struct socket *)0);
	}
	if (connstatus) {
		sorwakeup(head);
		wakeup((caddr_t)&head->so_timeo);
		so->so_state |= connstatus;
	}
	return (so);
}

soqinsque(head, so, q)
	register struct socket *head, *so;
	int q;
{

	register struct socket **prev;
	so->so_head = head;
	if (q == 0) {
		head->so_q0len++;
		so->so_q0 = 0;
		for (prev = &(head->so_q0); *prev; )
			prev = &((*prev)->so_q0);
	} else {
		head->so_qlen++;
		so->so_q = 0;
		for (prev = &(head->so_q); *prev; )
			prev = &((*prev)->so_q);
	}
	*prev = so;
}

soqremque(so, q)
	register struct socket *so;
	int q;
{
	register struct socket *head, *prev, *next;

	head = so->so_head;
	prev = head;
	for (;;) {
		next = q ? prev->so_q : prev->so_q0;
		if (next == so)
			break;
		if (next == 0)
			return (0);
		prev = next;
	}
	if (q == 0) {
		prev->so_q0 = next->so_q0;
		head->so_q0len--;
	} else {
		prev->so_q = next->so_q;
		head->so_qlen--;
	}
	next->so_q0 = next->so_q = 0;
	next->so_head = 0;
	return (1);
}
