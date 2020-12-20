/*-
 * Copyright (c) 1982, 1986, 1990 The Regents of the University of California.
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
 *	$Id: socketvar.h,v 1.2 95/02/07 21:02:09 bill Exp Locker: bill $
 */

#include "sys/socket.h"

/*
 * Kernel structure per socket.
 * Contains send and receive buffer queues,
 * handle on protocol and pointer to protocol
 * private data and error information.
 */
struct socket {
	short	so_type;		/* generic type, see socket.h */
	short	so_options;		/* from socket call, see socket.h */
	short	so_linger;		/* time to linger while closing */
	short	so_state;		/* internal state flags SS_*, below */
	caddr_t	so_pcb;			/* protocol control block */
	struct	protosw *so_proto;	/* protocol handle */
/*
 * Variables for connection queueing.
 * Socket where accepts occur is so_head in all subsidiary sockets.
 * If so_head is 0, socket is not related to an accept.
 * For head socket so_q0 queues partially completed connections,
 * while so_q is a queue of connections ready to be accepted.
 * If a connection is aborted and it has so_head set, then
 * it has to be pulled out of either so_q0 or so_q.
 * We allow connections to queue up based on current queue lengths
 * and limit on number of queued connections for this socket.
 */
	struct	socket *so_head;	/* back pointer to accept socket */
	struct	socket *so_q0;		/* queue of partial connections */
	struct	socket *so_q;		/* queue of incoming connections */
	short	so_q0len;		/* partials on so_q0 */
	short	so_qlen;		/* number of connections on so_q */
	short	so_qlimit;		/* max number queued connections */
	short	so_timeo;		/* connection timeout */
	u_short	so_error;		/* error affecting connection */
	pid_t	so_pgid;		/* pgid for signals */
	u_long	so_oobmark;		/* chars to oob mark */
/*
 * Variables for socket buffering.
 * Each socket contains two socket buffers: one for sending data and
 * one for receiving data.  Each buffer contains a queue of mbufs,
 * information about the number of mbufs and amount of data in the
 * queue, and other fields allowing select() statements and notification
 * on data availability to be implemented.
 */
	struct	sockbuf {
		u_long	sb_cc;		/* logical bytes in queue */
		u_long	sb_hiwat;	/* max logical bytes queued */
		u_long	sb_mbcnt;	/* physical bytes of mbufs in queue */
		u_long	sb_mbmax;	/* max physical bytes of mbufs queued */
		long	sb_lowat;	/* low water mark of logical data */
		struct	mbuf *sb_mb;	/* the mbuf queue chain */
		pid_t	sb_sel;		/* process selecting read/write */
		short	sb_flags;	/* buffer flags: */
#define	SB_LOCK		0x01		/*   process has lock on data queue */
#define	SB_WANT		0x02		/*   process is waiting to lock queue */
#define	SB_WAIT		0x04		/*   someone is waiting for contents */
#define	SB_SEL		0x08		/*   process is select()ing */
#define	SB_ASYNC	0x10		/*   ASYNC I/O, process needs signals */
#define	SB_COLL		0x20		/*   process collision select()ing */
#define	SB_NOINTR	0x40		/*   operations is not interruptible */
		short	sb_timeo;	/* timeout for read/write */
	} so_rcv, so_snd;
#define	SB_MAX		(64*1024)	/* default for max chars in sockbuf */
#define	SB_NOTIFY	(SB_WAIT|SB_SEL|SB_ASYNC)
};

/*
 * Socket state bits.
 */
#define	SS_NOFDREF		0x001	/* no file table ref any more */
#define	SS_ISCONNECTED		0x002	/* socket connected to a peer */
#define	SS_ISCONNECTING		0x004	/* in process of connecting to peer */
#define	SS_ISDISCONNECTING	0x008	/* in process of disconnecting */
#define	SS_CANTSENDMORE		0x010	/* can't send more data to peer */
#define	SS_CANTRCVMORE		0x020	/* can't receive more data from peer */
#define	SS_RCVATMARK		0x040	/* at mark on input */

#define	SS_PRIV			0x080	/* privileged for broadcast, raw... */
#define	SS_NBIO			0x100	/* non-blocking ops */
#define	SS_ASYNC		0x200	/* async i/o notify */
#define	SS_ISCONFIRMING		0x400	/* deciding to accept connection req */


/*
 * Macros and inlines for sockets and socket buffering.
 */

/* determine the amount of space remaining in the buffer to fill it. */
extern u_long inline
sbspace(struct sockbuf *sb) {
	u_long ulmin(u_long, u_long);

	/* logical or physical buffer overflow ? */
	if (sb->sb_cc > sb->sb_hiwat || sb->sb_mbcnt > sb->sb_mbmax)
		return (0);

	/* remainder to fill? */
	return (ulmin(sb->sb_hiwat - sb->sb_cc, sb->sb_mbmax - sb->sb_mbcnt));
}

/* do we have to send all at once on a socket? */
#define	sosendallatonce(so) \
    ((so)->so_proto->pr_flags & PR_ATOMIC)

/* can we read something from so? */
#define	soreadable(so) \
    ((so)->so_rcv.sb_cc >= (so)->so_rcv.sb_lowat || \
	((so)->so_state & SS_CANTRCVMORE) || \
	(so)->so_qlen || (so)->so_error)

/* can we write something to so? */
#define	sowriteable(so) \
    (sbspace(&(so)->so_snd) >= (so)->so_snd.sb_lowat && \
	(((so)->so_state&SS_ISCONNECTED) || \
	  ((so)->so_proto->pr_flags&PR_CONNREQUIRED)==0) || \
     ((so)->so_state & SS_CANTSENDMORE) || \
     (so)->so_error)

/* adjust counters in sb reflecting allocation of m */
extern inline void
sballoc(struct sockbuf *sb, struct mbuf *m)
{

	sb->sb_cc += m->m_len;
	sb->sb_mbcnt += MSIZE;
	/* m_reserve(MSIZE); */

	if (m->m_flags & M_EXT) {
		sb->sb_mbcnt += m->m_ext.ext_size;
		/* m_reserve(m->m_ext.ext_size); */
	}
}

/* adjust counters in sb reflecting freeing of m */
extern inline void
sbfree(struct sockbuf *sb, struct mbuf *m) {

	sb->sb_cc -= m->m_len;
	sb->sb_mbcnt -= MSIZE;
	/* m_release(MSIZE); */

	if (m->m_flags & M_EXT) {
		sb->sb_mbcnt -= m->m_ext.ext_size;
		/* m_release(m->m_ext.ext_size); */
	}
}

/*
 * Set lock on sockbuf sb; sleep if lock is already held.
 * Unless SB_NOINTR is set on sockbuf, sleep is interruptible.
 * Returns error without lock if sleep is interrupted.
 */
#define sblock(sb) ((sb)->sb_flags & SB_LOCK ? sb_lock(sb) : \
		((sb)->sb_flags |= SB_LOCK, 0))

/* release lock on sockbuf sb */
#define	sbunlock(sb) { \
	(sb)->sb_flags &= ~SB_LOCK; \
	if ((sb)->sb_flags & SB_WANT) { \
		(sb)->sb_flags &= ~SB_WANT; \
		wakeup((caddr_t)&(sb)->sb_flags); \
	} \
}

#ifdef NDFILE
extern inline int
getsock(struct filedesc *fdp, int fdes, struct file **fpp)
{
	struct file *fp;

	if ((unsigned)fdes >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[fdes]) == NULL)
		return (EBADF);

	if (fp->f_type != DTYPE_SOCKET)
		return (ENOTSOCK);

	*fpp = fp;
	return (0);
}
#endif

#ifdef KERNEL

/* strings for sleep message: */
extern	char netio[], netcon[], netcls[];

/* socket buffer operations */
/*struct	socket *sonewconn(struct socket *head, int connstatus);
int soreserve(struct socket *so, u_long sndcc, u_long rcvcc);
int sbreserve(struct sockbuf *sb, u_long cc); */
void sbrelease(struct sockbuf *sb);
/*void sbappend(struct sockbuf *sb, struct mbuf *m);
void sbappendrecord(struct sockbuf *sb, struct mbuf *m0);
void sbinsertoob(struct sockbuf *sb, struct mbuf *m0);
int sbappendaddr(struct sockbuf *sb, struct sockaddr *asa, struct mbuf *m0,
	struct mbuf *control);
int sbappendcontrol(struct sockbuf *sb, struct mbuf *m0, struct mbuf *control);*/
void sbcompress(struct sockbuf *sb, struct mbuf *m, struct mbuf *n);
/*void sbflush(struct sockbuf *sb);
void sbdrop(struct sockbuf *sb, int len);
void sbdroprecord(struct sockbuf *sb);
void socantsendmore(struct socket *so);
void socantrcvmore(struct socket *so); */
void sbselqueue(struct sockbuf *sb, struct proc *cp);
int sbwait(struct sockbuf *sb);
int sb_lock(struct sockbuf *sb);
/*void sowakeup(struct socket *so);
void sbwakeup(struct sockbuf *sb);


void sofree(struct socket *so); */
void sorflush(struct socket *so);

/* interface symbols */
#define	__ISYM_VERSION__ "1"	/* XXX RCS major revision number of hdr file */
#include "isym.h"		/* this header has interface symbols */

__ISYM__(u_long, sb_max,)
__ISYM__(int,  sbappendaddr, (struct sockbuf *sb, struct sockaddr *asa, struct mbuf *m0, struct mbuf *control))
__ISYM__(int,  sbappendcontrol, (struct sockbuf *sb, struct mbuf *m0, struct mbuf *control))
__ISYM__(int,  sbreserve, (struct sockbuf *sb, u_long cc))
__ISYM__(int,  soreserve, (struct socket *so, u_long sndcc, u_long rcvcc))
__ISYM__(void, sbappend, (struct sockbuf *sb, struct mbuf *m))
__ISYM__(void, sbappendrecord, (struct sockbuf *sb, struct mbuf *m0))
__ISYM__(void, sbdrop, (struct sockbuf *sb, int len))
__ISYM__(void, sbdroprecord, (struct sockbuf *sb))
__ISYM__(void, sbflush, (struct sockbuf *sb))
__ISYM__(void, sbinsertoob, (struct sockbuf *sb, struct mbuf *m0))
__ISYM__(void, sbwakeup, (struct sockbuf *sb))
__ISYM__(int, soabort, (struct socket *so))
__ISYM__(void, socantrcvmore, (struct socket *so))
__ISYM__(void, socantsendmore, (struct socket *so))
__ISYM__(void, sofree, (struct socket *so))
__ISYM__(void, sohasoutofband, (struct socket *so))
__ISYM__(void, soisconnected, (struct socket *so))
__ISYM__(void, soisconnecting, (struct socket *so))
__ISYM__(void, soisdisconnected, (struct socket *so))
__ISYM__(void, soisdisconnecting, (struct socket *so))
__ISYM__(struct	socket *, sonewconn, (struct socket *head, int connstatus))
__ISYM__(void, sowakeup, (struct socket *so))

__ISYM__(int, socreate, (int dom, struct socket **aso, int type, int proto))
__ISYM__(int, sobind, (struct socket *so, struct mbuf *sockaddrnam))
__ISYM__(int, solisten, (struct socket *so, int backlog))
__ISYM__(int, soclose, (struct socket *so))
__ISYM__(int, soaccept, (struct socket *so, struct mbuf *sockaddrnam))
__ISYM__(int, soconnect, (struct socket *so, struct mbuf *sockaddrnam))
__ISYM__(int, sosetopt, (struct socket *so, int level, int optname, struct mbuf *m0))
__ISYM__(int, soshutdown, (struct socket *so, int how))
__ISYM__(int, soreceive, (struct socket *so, struct mbuf **paddr, struct uio *uio, struct mbuf **mp0, struct mbuf **controlp, int *flagsp))
__ISYM__(int, sosend, (struct socket *so, struct mbuf *addr, struct uio *uio, struct mbuf *top, struct mbuf *control, int flags))
__ISYM__(int, sockargs, (struct mbuf **mp, caddr_t buf, int buflen, int type))

#undef __ISYM__
#undef __ISYM_ALIAS__
#undef __ISYM_VERSION__

extern void inline 
sorwakeup(struct socket *so) {

	sbwakeup(&so->so_rcv);
	sowakeup(so);
}

extern void inline 
sowwakeup(struct socket *so) {

	sbwakeup(&so->so_snd);
	sowakeup(so);
}

#endif
