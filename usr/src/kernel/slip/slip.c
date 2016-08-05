/*
 * Copyright (c) 1987, 1989 Regents of the University of California.
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
 */

/*
 * Serial Line interface
 *
 * Rick Adams
 * Center for Seismic Studies
 * 1300 N 17th Street, Suite 1450
 * Arlington, Virginia 22209
 * (703)276-7900
 * rick@seismo.ARPA
 * seismo!rick
 *
 * Pounded on heavily by Chris Torek (chris@mimsy.umd.edu, umcp-cs!chris).
 * N.B.: this belongs in netinet, not net, the way it stands now.
 * Should have a link-layer type designation, but wouldn't be
 * backwards-compatible.
 *
 * Converted to 4.3BSD Beta by Chris Torek.
 * Other changes made at Berkeley, based in part on code by Kirk Smith.
 * W. Jolitz added slip abort.
 *
 * Hacked almost beyond recognition by Van Jacobson (van@helios.ee.lbl.gov).
 * Added priority queuing for "interactive" traffic; hooks for TCP
 * header compression; ICMP filtering (at 2400 baud, some cretin
 * pinging you can use up all your bandwidth).  Made low clist behavior
 * more robust and slightly less likely to hang serial line.
 * Sped up a bunch of things.
 * 
 * Note that splimp() is used throughout to block both (tty) input
 * interrupts and network activity; thus, splimp must be >= spltty.
 */

static char *sl_config =
	"slip 4.	# serial line IP (sl0)";

#include "sys/param.h"
/*#include "sys/ucred.h"*/
#include "sys/socket.h"
#include "sys/file.h"
#include "sys/ioctl.h"
#include "privilege.h"
#include "sys/errno.h"
#include "proc.h"
#include "mbuf.h"
#include "dkstat.h"
#include "tty.h"
#include "kernel.h"
#include "modconfig.h"

#include "machine/cpu.h"

#include "if.h"
#include "if_types.h"
#include "netisr.h"
#include "route.h"
#if INET
#include "in.h"
#include "in_systm.h"
#include "in_var.h"
#include "ip.h"
#else
Huh? Slip without inet?
#endif

#include "slcompress.h"
#include "if_slvar.h"

#include "prototypes.h"

/*
 * SLMAX is a hard limit on input packet size.  To simplify the code
 * and improve performance, we require that packets fit in an mbuf
 * cluster, and if we get a compressed packet, there's enough extra
 * room to expand the header into a max length tcp/ip header (128
 * bytes).  So, SLMAX can be at most
 *	MCLBYTES - 128
 *
 * SLMTU is a hard limit on output packet size.  To insure good
 * interactive response, SLMTU wants to be the smallest size that
 * amortizes the header cost.  (Remember that even with
 * type-of-service queuing, we have to wait for any in-progress
 * packet to finish.  I.e., we wait, on the average, 1/2 * mtu /
 * cps, where cps is the line speed in characters per second.
 * E.g., 533ms wait for a 1024 byte MTU on a 9600 baud line.  The
 * average compressed header size is 6-8 bytes so any MTU > 90
 * bytes will give us 90% of the line bandwidth.  A 100ms wait is
 * tolerable (500ms is not), so want an MTU around 296.  (Since TCP
 * will send 256 byte segments (to allow for 40 byte headers), the
 * typical packet size on the wire will be around 260 bytes).  In
 * 4.3tahoe+ systems, we can set an MTU in a route so we do that &
 * leave the interface MTU relatively high (so we don't IP fragment
 * when acting as a gateway to someone using a stupid MTU).
 *
 * Similar considerations apply to SLIP_HIWAT:  It's the amount of
 * data that will be queued 'downstream' of us (i.e., in clists
 * waiting to be picked up by the tty output interrupt).  If we
 * queue a lot of data downstream, it's immune to our t.o.s. queuing.
 * E.g., if SLIP_HIWAT is 1024, the interactive traffic in mixed
 * telnet/ftp will see a 1 sec wait, independent of the mtu (the
 * wait is dependent on the ftp window size but that's typically
 * 1k - 4k).  So, we want SLIP_HIWAT just big enough to amortize
 * the cost (in idle time on the wire) of the tty driver running
 * off the end of its clists & having to call back slstart for a
 * new packet.  For a tty interface with any buffering at all, this
 * cost will be zero.  Even with a totally brain dead interface (like
 * the one on a typical workstation), the cost will be <= 1 character
 * time.  So, setting SLIP_HIWAT to ~100 guarantees that we'll lose
 * at most 1% while maintaining good interactive response.
 */
#define BUFOFFSET	128
#define	SLMAX		(MCLBYTES - BUFOFFSET)
#define	SLBUFSIZE	(SLMAX + BUFOFFSET)
#define	SLMTU		296
#define	SLIP_HIWAT	roundup(50, 1 /*CBSIZ*/)
#define	CLISTRESERVE	1024	/* Can't let clists get too low */

/*
 * SLIP ABORT ESCAPE MECHANISM:
 *	(inspired by HAYES modem escape arrangement)
 *	1sec escape 1sec escape 1sec escape { 1sec escape 1sec escape }
 *	signals a "soft" exit from slip mode by usermode process
 */

#define	ABT_ESC		'\033'	/* can't be t_intr - distant host must know it*/
#define ABT_WAIT	1	/* in seconds - idle before an escape & after */
#define ABT_RECYCLE	(5*2+2)	/* in seconds - time window processing abort */

#define ABT_SOFT	3	/* count of escapes */


/*
 * The following disgusting hack gets around the problem that IP TOS
 * can't be set yet.  We want to put "interactive" traffic on a high
 * priority queue.  To decide if traffic is interactive, we check that
 * a) it is TCP and b) one of its ports is telnet, rlogin or ftp control.
 */
static u_short interactive_ports[8] = {
	0,	513,	0,	0,
	0,	21,	0,	23,
};
#define INTERACTIVE(p) (interactive_ports[(p) & 7] == (p))

static int nsl;
static struct sl_softc *sl_softc;

#define FRAME_END	 	0xc0		/* Frame End */
#define FRAME_ESCAPE		0xdb		/* Frame Esc */
#define TRANS_FRAME_END	 	0xdc		/* transposed frame end */
#define TRANS_FRAME_ESCAPE 	0xdd		/* transposed frame esc */

#define t_sc T_LINEP

static int sloutput() , slioctl();
static void slstart(struct tty *tp);
static slattach();
extern struct timeval time;

/* execute on load/bootstrap to configure in */
SLIP_MODCONFIG() {
	slattach();
}

/*
 * Called from boot code to establish sl interfaces.
 */
static
slattach()
{
	struct sl_softc *sc;
	int i = 0;

	for (sc = sl_softc; i < nsl; sc++) {
		sc->sc_if.if_name = "sl";
		sc->sc_if.if_unit = i++;
		sc->sc_if.if_mtu = SLMTU;
		sc->sc_if.if_flags = IFF_POINTOPOINT;
		sc->sc_if.if_type = IFT_SLIP;
		sc->sc_if.if_ioctl = slioctl;
		sc->sc_if.if_output = sloutput;
		sc->sc_if.if_snd.ifq_maxlen = 50;
		sc->sc_fastq.ifq_maxlen = 32;
		if_attach(&sc->sc_if);
	}
}

static int
slinit(sc)
	struct sl_softc *sc;
{
	caddr_t p;

	if (sc->sc_ep == (u_char *) 0) {
		MCLALLOC(p, M_WAIT);
		if (p)
			sc->sc_ep = (u_char *)p + SLBUFSIZE;
		else {
			printf("sl%d: can't allocate buffer\n", sc - sl_softc);
			sc->sc_if.if_flags &= ~IFF_UP;
			return (0);
		}
	}
	sc->sc_buf = sc->sc_ep - SLMAX;
	sc->sc_mp = sc->sc_buf;
	sl_compress_init(&sc->sc_comp);
	return (1);
}

/*
 * Line specific open routine.
 * Attach the given tty to the first available sl unit.
 */
/* ARGSUSED */
static int
slopen(dev_t dev, struct tty *tp, int flag)
{
	struct proc *p = curproc;		/* XXX */
	register struct sl_softc *sc;
	register int ns;
	int error;

	if (error = use_priv(p->p_ucred, PRV_SLIP_OPEN, p))
		return (error);

	if (tp->t_line == SLIPDISC)
		return (0);

	for (ns = nsl, sc = sl_softc; --ns >= 0; sc++)
		if (sc->sc_ttyp == NULL) {
			if (slinit(sc) == 0)
				return (ENOBUFS);
			tp->t_sc = (caddr_t)sc;
			sc->sc_ttyp = tp;
			sc->sc_if.if_baudrate = tp->t_ospeed;
			ttyflush(tp, FREAD | FWRITE);
			return (0);
		}
	return (ENXIO);
}

/*
 * Line specific close routine.
 * Detach the tty from the sl unit.
 * Mimics part of ttyclose().
 */
static void
slclose(struct tty *tp, int flag)
{
	register struct sl_softc *sc;
	int s;

	ttywflush(tp);
	s = splimp();		/* actually, max(spltty, splnet) */
	tp->t_line = 0;
	sc = (struct sl_softc *)tp->t_sc;
	if (sc != NULL) {
		if_down(&sc->sc_if);
		sc->sc_ttyp = NULL;
		tp->t_sc = NULL;
		MCLFREE((caddr_t)(sc->sc_ep - SLBUFSIZE));
		sc->sc_ep = 0;
		sc->sc_mp = 0;
		sc->sc_buf = 0;
	}
	splx(s);
}

/*
 * Line specific (tty) ioctl routine.
 * Provide a way to get the sl unit number.
 */
/* ARGSUSED */
static int
sltioctl(struct tty *tp, int cmd, caddr_t data, int flag, struct proc *p)
{
	struct sl_softc *sc = (struct sl_softc *)tp->t_sc;
	int s;

	switch (cmd) {
	case SLIOCGUNIT:
		*(int *)data = sc->sc_if.if_unit;
		break;

	case SLIOCGFLAGS:
		*(int *)data = sc->sc_flags;
		break;

	case SLIOCSFLAGS:
#define	SC_MASK	0xffff
		s = splimp();
		sc->sc_flags =
		    (sc->sc_flags &~ SC_MASK) | ((*(int *)data) & SC_MASK);
		splx(s);
		break;

	default:
		return (-1);
	}
	return (0);
}

/*
 * Queue a packet.  Start transmission if not active.
 */
static
sloutput(ifp, m, dst)
	struct ifnet *ifp;
	register struct mbuf *m;
	struct sockaddr *dst;
{
	register struct sl_softc *sc = &sl_softc[ifp->if_unit];
	register struct ip *ip;
	register struct ifqueue *ifq;
	int s;

	/*
	 * `Cannot happen' (see slioctl).  Someday we will extend
	 * the line protocol to support other address families.
	 */
	if (dst->sa_family != AF_INET) {
		printf("sl%d: af%d not supported\n", sc->sc_if.if_unit,
			dst->sa_family);
		m_freem(m);
		return (EAFNOSUPPORT);
	}

	if (sc->sc_ttyp == NULL) {
		m_freem(m);
		return (ENETDOWN);	/* sort of */
	}
	if ((sc->sc_ttyp->t_state & TS_CARR_ON) == 0) {
		m_freem(m);
		return (EHOSTUNREACH);
	}
	ifq = &sc->sc_if.if_snd;
	if ((ip = mtod(m, struct ip *))->ip_p == IPPROTO_TCP) {
		register int p = ((int *)ip)[ip->ip_hl];

		if (INTERACTIVE(p & 0xffff) || INTERACTIVE(p >> 16)) {
			ifq = &sc->sc_fastq;
			p = 1;
		} else
			p = 0;

		if (sc->sc_flags & SC_COMPRESS) {
			/*
			 * The last parameter turns off connection id
			 * compression for background traffic:  Since
			 * fastq traffic can jump ahead of the background
			 * traffic, we don't know what order packets will
			 * go on the line.
			 */
			p = sl_compress_tcp(m, ip, &sc->sc_comp, p);
			*mtod(m, u_char *) |= p;
		}
	} else if (sc->sc_flags & SC_NOICMP && ip->ip_p == IPPROTO_ICMP) {
		m_freem(m);
		return (0);
	}
	s = splimp();
	if (IF_QFULL(ifq)) {
		IF_DROP(ifq);
		m_freem(m);
		splx(s);
		sc->sc_if.if_oerrors++;
		return (ENOBUFS);
	}
	IF_ENQUEUE(ifq, m);
	sc->sc_if.if_lastchange = time;
	if (RB_LEN(&sc->sc_ttyp->t_out) == 0)
		slstart(sc->sc_ttyp);
	splx(s);
	return (0);
}

/*
 * Start output on interface.  Get another datagram
 * to send from the interface queue and map it to
 * the interface before starting output.
 */
static void
slstart(struct tty *tp)
{
	register struct sl_softc *sc = (struct sl_softc *)tp->t_sc;
	register struct mbuf *m;
	register u_char *cp;
	int s;
	struct mbuf *m2;

	for (;;) {
		/*
		 * If there is more in the output queue, just send it now.
		 * We are being called in lieu of ttstart and must do what
		 * it would.
		 */
		if (RB_LEN(&tp->t_out) != 0) {
			(*tp->t_oproc)(tp);
			if (RB_LEN(&tp->t_out) > SLIP_HIWAT)
				return;
		}
		/*
		 * This happens briefly when the line shuts down.
		 */
		if (sc == NULL)
			return;

		/*
		 * Get a packet and send it to the interface.
		 */
		s = splimp();
		IF_DEQUEUE(&sc->sc_fastq, m);
		if (m == NULL)
			IF_DEQUEUE(&sc->sc_if.if_snd, m);
		splx(s);
		if (m == NULL)
			return;
		sc->sc_if.if_lastchange = time;
		/*
		 * If system is getting low on clists, just flush our
		 * output queue (if the stuff was important, it'll get
		 * retransmitted).
		 */
		if (RBSZ - RB_LEN(&tp->t_out) < SLMTU) {
			m_freem(m);
			sc->sc_if.if_collisions++;
			continue;
		}

		/*
		 * The extra FRAME_END will start up a new packet, and thus
		 * will flush any accumulated garbage.  We do this whenever
		 * the line may have been idle for some time.
		 */
		if (RB_LEN(&tp->t_out) == 0) {
			++sc->sc_bytessent;
			(void) putc(FRAME_END, &tp->t_out);
		}

		while (m) {
			register u_char *ep;

			cp = mtod(m, u_char *); ep = cp + m->m_len;
			while (cp < ep) {
				/*
				 * Find out how many bytes in the string we can
				 * handle without doing something special.
				 */
				register u_char *bp = cp;

				while (cp < ep) {
					switch (*cp++) {
					case FRAME_ESCAPE:
					case FRAME_END:
						--cp;
						goto out;
					}
				}
				out:
				if (cp > bp) {
					int cc;
					/*
					 * Put n characters at once
					 * into the tty output queue.
					 */
#ifdef was
					if (b_to_q((char *)bp, cp - bp, &tp->t_outq))
						break;
					sc->sc_bytessent += cp - bp;
#else
#ifdef works
					if (cc = RB_CONTIGPUT(&tp->t_out)) {
						cc = min (cc, cp - bp);
						memcpy(tp->t_out.rb_tl,
						      (char *)bp, cc);
						tp->t_out.rb_tl =
				  RB_ROLLOVER(&tp->t_out, tp->t_out.rb_tl + cc);
						sc->sc_bytessent += cc;
						bp += cc;
					} else
						break;
					if (cp > bp && cc = RB_CONTIGPUT(&tp->t_out)) {
						cc = min (cc, cp - bp);
						memcpy(tp->t_out.rb_tl,
						      (char *)bp, cc);
						tp->t_out.rb_tl =
				  RB_ROLLOVER(&tp->t_out, tp->t_out.rb_tl + cc);
						sc->sc_bytessent += cc;
						bp += cc;
					} else
						break;
#else
					while (cp > bp && (cc = RB_CONTIGPUT(&tp->t_out))) {
						cc = min (cc, cp - bp);
						memcpy(tp->t_out.rb_tl,
						      (char *)bp, cc);
						tp->t_out.rb_tl =
				  RB_ROLLOVER(&tp->t_out, tp->t_out.rb_tl + cc);
						sc->sc_bytessent += cc;
						bp += cc;
					}
#endif
#endif
				}
				/*
				 * If there are characters left in the mbuf,
				 * the first one must be special..
				 * Put it out in a different form.
				 */
				if (cp < ep) {
					if (putc(FRAME_ESCAPE, &tp->t_out))
						break;
					if (putc(*cp++ == FRAME_ESCAPE ?
					   TRANS_FRAME_ESCAPE : TRANS_FRAME_END,
					   &tp->t_out)) {
						(void) unputc(&tp->t_out);
						break;
					}
					sc->sc_bytessent += 2;
				}
			}
			MFREE(m, m2);
			m = m2;
		}

		if (putc(FRAME_END, &tp->t_out)) {
			/*
			 * Not enough room.  Remove a char to make room
			 * and end the packet normally.
			 * If you get many collisions (more than one or two
			 * a day) you probably do not have enough clists
			 * and you should increase "nclist" in param.c.
			 */
			(void) unputc(&tp->t_out);
			(void) putc(FRAME_END, &tp->t_out);
			sc->sc_if.if_collisions++;
		} else {
			++sc->sc_bytessent;
			sc->sc_if.if_opackets++;
		}
		sc->sc_if.if_obytes = sc->sc_bytessent;
	}
}

/*
 * Copy data buffer to mbuf chain; add ifnet pointer.
 */
static struct mbuf *
sl_btom(sc, len)
	register struct sl_softc *sc;
	register int len;
{
	register struct mbuf *m;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (NULL);

	/*
	 * If we have more than MHLEN bytes, it's cheaper to
	 * queue the cluster we just filled & allocate a new one
	 * for the input buffer.  Otherwise, fill the mbuf we
	 * allocated above.  Note that code in the input routine
	 * guarantees that packet will fit in a cluster.
	 */
	if (len >= MHLEN) {
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			/*
			 * we couldn't get a cluster - if memory's this
			 * low, it's time to start dropping packets.
			 */
			(void) m_free(m);
			return (NULL);
		}
		sc->sc_ep = mtod(m, u_char *) + SLBUFSIZE;
		m->m_data = (caddr_t)sc->sc_buf;
		m->m_ext.ext_buf = (caddr_t)((int)sc->sc_buf &~ MCLOFSET);
	} else
		memcpy(mtod(m, caddr_t), (caddr_t)sc->sc_buf, len);

	m->m_len = len;
	m->m_pkthdr.len = len;
	m->m_pkthdr.rcvif = &sc->sc_if;
	return (m);
}

/*
 * tty interface receiver interrupt.
 */
static void
slinput(unsigned c, struct tty *tp)
{
	register struct sl_softc *sc;
	register struct mbuf *m;
	register int len;
	int s;

	tk_nin++;
	sc = (struct sl_softc *)tp->t_sc;
	if (sc == NULL)
		return;
	if (!(tp->t_state&TS_CARR_ON))	/* XXX */
		return;

	++sc->sc_bytesrcvd;
	++sc->sc_if.if_ibytes;
	c &= 0xff;			/* XXX */

#ifdef ABT_ESC
	if (sc->sc_flags & SC_ABORT) {
		/* if we see an abort after "idle" time, count it */
		if (c == ABT_ESC && time.tv_sec >= sc->sc_lasttime + ABT_WAIT) {
			sc->sc_abortcount++;
			/* record when the first abort escape arrived */
			if (sc->sc_abortcount == 1)
				sc->sc_starttime = time.tv_sec;
		}
		/*
		 * if we have an abort, see that we have not run out of time,
		 * or that we have an "idle" time after the complete escape
		 * sequence
		 */
		if (sc->sc_abortcount) {
			if (time.tv_sec >= sc->sc_starttime + ABT_RECYCLE)
				sc->sc_abortcount = 0;
			if (sc->sc_abortcount >= ABT_SOFT &&
			    time.tv_sec >= sc->sc_lasttime + ABT_WAIT) {
				slclose(tp, 0);
				return;
			}
		}
		sc->sc_lasttime = time.tv_sec;
	}
#endif

	switch (c) {

	case TRANS_FRAME_ESCAPE:
		if (sc->sc_escape)
			c = FRAME_ESCAPE;
		break;

	case TRANS_FRAME_END:
		if (sc->sc_escape)
			c = FRAME_END;
		break;

	case FRAME_ESCAPE:
		sc->sc_escape = 1;
		return;

	case FRAME_END:
		len = sc->sc_mp - sc->sc_buf;
		if (len < 3)
			/* less than min length packet - ignore */
			goto newpack;

		if ((c = (*sc->sc_buf & 0xf0)) != (IPVERSION << 4)) {
			if (c & 0x80)
				c = TYPE_COMPRESSED_TCP;
			else if (c == TYPE_UNCOMPRESSED_TCP)
				*sc->sc_buf &= 0x4f; /* XXX */
			/*
			 * We've got something that's not an IP packet.
			 * If compression is enabled, try to decompress it.
			 * Otherwise, if `auto-enable' compression is on and
			 * it's a reasonable packet, decompress it and then
			 * enable compression.  Otherwise, drop it.
			 */
			if (sc->sc_flags & SC_COMPRESS) {
				len = sl_uncompress_tcp(&sc->sc_buf, len, len,
							(u_int)c, &sc->sc_comp);
				if (len <= 0)
					goto error;
			} else if ((sc->sc_flags & SC_AUTOCOMP) &&
			    c == TYPE_UNCOMPRESSED_TCP && len >= 40) {
				len = sl_uncompress_tcp(&sc->sc_buf, len, len,
							(u_int)c, &sc->sc_comp);
				if (len <= 0)
					goto error;
				sc->sc_flags |= SC_COMPRESS;
			} else
				goto error;
		}
		m = sl_btom(sc, len);
		if (m == NULL)
			goto error;

		sc->sc_if.if_ipackets++;
		sc->sc_if.if_lastchange = time;
		s = splimp();
		if (IF_QFULL(&ipintrq)) {
			IF_DROP(&ipintrq);
			sc->sc_if.if_ierrors++;
			sc->sc_if.if_iqdrops++;
			m_freem(m);
		} else {
			IF_ENQUEUE(&ipintrq, m);
			schednetisr(NETISR_IP);
		}
		splx(s);
		goto newpack;
	}
	if (sc->sc_mp < sc->sc_ep) {
		*sc->sc_mp++ = c;
		sc->sc_escape = 0;
		return;
	}
error:
	sc->sc_if.if_ierrors++;
newpack:
	sc->sc_mp = sc->sc_buf = sc->sc_ep - SLMAX;
	sc->sc_escape = 0;
}

/*
 * Process an ioctl request.
 */
static
slioctl(ifp, cmd, data)
	register struct ifnet *ifp;
	int cmd;
	caddr_t data;
{
	register struct ifaddr *ifa = (struct ifaddr *)data;
	int s = splimp(), error = 0;

	switch (cmd) {

	case SIOCSIFADDR:
		if (ifa->ifa_addr->sa_family == AF_INET)
			ifp->if_flags |= IFF_UP;
		else
			error = EAFNOSUPPORT;
		break;

	case SIOCSIFDSTADDR:
		if (ifa->ifa_addr->sa_family != AF_INET)
			error = EAFNOSUPPORT;
		break;

	default:
		error = EINVAL;
	}
	splx(s);
	return (error);
}

int ldiscif_config(char **cfg, struct ldiscif *lif);
static struct ldiscif sl_ldiscif =
{
	{0}, -1, 0, 0, 0,
	slopen, slclose, 0, 0, sltioctl, slinput, 0, slstart, nullmodem,
};

LDISC_MODCONFIG() {
	char *cfg_string = sl_config;
	
	if (ldiscif_config(&cfg_string, &sl_ldiscif) == 0)
		return;

	/* allocate number of slip interfaces */
	nsl = 1;
	(void)cfg_number(&cfg_string, &nsl);

	sl_softc = (struct sl_softc *)
	    malloc(sizeof(struct sl_softc) * nsl, M_TEMP, M_NOWAIT);
	(void)memset((void *)sl_softc, 0, sizeof (struct sl_softc) * nsl);
}
