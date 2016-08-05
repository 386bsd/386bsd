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
 * $Id: sockconn.c,v 1.1 94/10/19 23:49:57 bill Exp Locker: bill $
 * 
 * Connection-oriented socket primatives.
 */

#include "sys/param.h"
#include "sys/file.h"
#include "sys/errno.h"
#include "proc.h"
#include "mbuf.h"
#include "signalvar.h"
#include "malloc.h"
#include "socketvar.h"
#include "protosw.h"
#include "prototypes.h"

static void soqinsque(struct socket *head, struct socket *so, int q);

/* indicate that socket is attempting an active connection */
void
soisconnecting(struct socket *so)
{

	/* adjust state of socket to indicate attempt to connect */
	so->so_state &= ~(SS_ISCONNECTED|SS_ISDISCONNECTING);
	so->so_state |= SS_ISCONNECTING;
}

/* indicate that socket has connected, either active or passive */
void
soisconnected(struct socket *so)
{
	struct socket *head = so->so_head;

	/* adjust state of socket to correspond to connected state */
	so->so_state &=
	    ~(SS_ISCONNECTING | SS_ISDISCONNECTING | SS_ISCONFIRMING);
	so->so_state |= SS_ISCONNECTED;

	/* if in passive listen queue as provisional ... */
	if (head && soqremque(so, 0)) {
		/*  move to connection queue */
		soqinsque(head, so, 1);
		/* alert readers of the connection status of accept()or */
		sorwakeup(head);
		wakeup((caddr_t)&head->so_timeo);
	}

	/* otherwise, alert any processes waiting for knowledge of connection */
	else {
		wakeup((caddr_t)&so->so_timeo);
		sorwakeup(so);
		sowwakeup(so);
	}
}

/* indicate that socket is attempting to disconnect a connection */
void
soisdisconnecting(struct socket *so)
{

	/* adjust state of socket to indicate attempt to disconnect */
	so->so_state &= ~SS_ISCONNECTING;
	so->so_state |=
	    (SS_ISDISCONNECTING | SS_CANTRCVMORE | SS_CANTSENDMORE);

	/* alert any processes waiting for knowledge of disconnection */
	wakeup((caddr_t)&so->so_timeo);
	sowwakeup(so);
	sorwakeup(so);
}

/* indicate that socket has disconnected */
void
soisdisconnected(struct socket *so)
{

	/* adjust state of socket to correspond to disconnected state */
	so->so_state &=
	    ~(SS_ISCONNECTING | SS_ISCONNECTED | SS_ISDISCONNECTING);
	so->so_state |= (SS_CANTRCVMORE|SS_CANTSENDMORE);

	/* alert any processes waiting for knowledge of disconnection */
	wakeup((caddr_t)&so->so_timeo);
	sowwakeup(so);
	sorwakeup(so);
}

/*
 * When an attempt at a new connection is noted on a socket
 * which accepts connections, sonewconn is called.  If the
 * connection is possible (subject to space constraints, etc.)
 * then we allocate a new socket, properly linked into the
 * data structure of the original socket, and return this.
 */
struct socket *
sonewconn(struct socket *head, int connstatus)
{
	struct socket *so;

	/* has queue size limit been reached? */
	if (head->so_qlen + head->so_q0len > 3 * head->so_qlimit / 2)
		return ((struct socket *)0);

	/* allocate a socket to append to the socket connection queue */
	MALLOC(so, struct socket *, sizeof(*so), M_SOCKET,
	    M_NOWAIT | M_ZERO_IT);
	if (so == NULL) 
		return ((struct socket *)0);

	/* make it a clone of the head accept()ing socket */
	so->so_type = head->so_type;
	so->so_options = head->so_options &~ SO_ACCEPTCONN;
	so->so_linger = head->so_linger;
	so->so_state = head->so_state | SS_NOFDREF;
	so->so_proto = head->so_proto;
	so->so_timeo = head->so_timeo;
	so->so_pgid = head->so_pgid;
	(void) soreserve(so, head->so_snd.sb_hiwat, head->so_rcv.sb_hiwat);

	/* attach it to the queue of socket connection instances */
	soqinsque(head, so, connstatus);

	/* inform protocols of new socket presence */
	if ((*so->so_proto->pr_usrreq)(so, PRU_ATTACH,
	    (struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0)) {
		(void) soqremque(so, connstatus);
		(void) free((caddr_t)so, M_SOCKET);
		return ((struct socket *)0);
	}

	/* alert readers of the connection status of the accept()or */
	if (connstatus) {
		sorwakeup(head);
		wakeup((caddr_t)&head->so_timeo);
		so->so_state |= connstatus;
	}

	return (so);
}

/*
 * Enqueue a socket on a connection queue.
 */
static void
soqinsque(struct socket *head, struct socket *so, int q)
{
	struct socket **prev;

	/* associate socket with a connection queue head */
	so->so_head = head;

	/* tack socket on the tail of either provisional or real queue */
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

/*
 * Remove a socket from its connection queue.
 */
int
soqremque(struct socket *so, int acceptq)
{
	struct socket *head, *prev, *next;

	/* search socket connect queue, starting with the head of its queue */
	for (prev = head = so->so_head; ; prev = next) {
		next = acceptq ? prev->so_q : prev->so_q0;

		/* found the socket to remove */
		if (next == so)
			break;

		/* not in the queue */
		if (next == 0)
			return (0);
	}

	/* remove it from the appropriate queue */
	if (acceptq == 0) {
		prev->so_q0 = next->so_q0;
		head->so_q0len--;
	} else {
		prev->so_q = next->so_q;
		head->so_qlen--;
	}

	/* isolate removed entry, indicate sucess */
	next->so_q0 = next->so_q = 0;
	next->so_head = 0;
	return (1);
}
