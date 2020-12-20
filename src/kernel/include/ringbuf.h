/*
 * Copyright (c) 1989, 1990, 1991, 1992 William F. Jolitz, TeleMuse
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
 *	This software is a component of "386BSD" developed by 
	William F. Jolitz, TeleMuse.
 * 4. Neither the name of the developer nor the name "386BSD"
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 *	$Id: ringbuf.h,v 1.1 94/07/27 15:42:02 bill Exp Locker: bill $
 *
 * Ring buffers provide a contiguous, dense storage for
 * character data used by the tty driver.
 */

#define	RBSZ 1024

struct ringb {
	int	rb_sz;		/* ring buffer size */
	char	*rb_hd;	  	/* head of buffer segment to be read */
	char	*rb_tl;	  	/* tail of buffer segment to be written */
	char	rb_buf[RBSZ];	/* segment contents */
};

/* sucessive location in ring buffer */
#define	RB_SUCC(rbp, p) \
		((p) >= (rbp)->rb_buf + RBSZ - 1 ? (rbp)->rb_buf : (p) + 1)

/* roll over end of ring to bottom if needed */
#define	RB_ROLLOVER(rbp, p) \
		((p) > (rbp)->rb_buf + RBSZ - 1 ? (rbp)->rb_buf : (p))

/* predecessor location in ring buffer */
#define	RB_PRED(rbp, p) \
		((p) <= (rbp)->rb_buf ? (rbp)->rb_buf + RBSZ - 1 : (p) - 1)

/* length of data in ring buffer */
#define	RB_LEN(rp) \
		((rp)->rb_hd <= (rp)->rb_tl ? (rp)->rb_tl - (rp)->rb_hd : \
		RBSZ - ((rp)->rb_hd - (rp)->rb_tl))

/* maximum amount of data that can be inserted into the buffer contiguously */
#define	RB_CONTIGPUT(rp) \
		(RB_PRED(rp, (rp)->rb_hd) < (rp)->rb_tl ?  \
			(rp)->rb_buf + RBSZ - (rp)->rb_tl : \
			RB_PRED(rp, (rp)->rb_hd) - (rp)->rb_tl)

/* maximum amount of data that can be remove from buffer contiguously */
#define	RB_CONTIGGET(rp) \
		((rp)->rb_hd <= (rp)->rb_tl ? (rp)->rb_tl - (rp)->rb_hd : \
		(rp)->rb_buf + RBSZ - (rp)->rb_hd)

/* prototypes */
#ifdef KERNEL
int putc(unsigned, struct ringb *);
int getc(struct ringb *);
int nextc(char **, struct ringb *);
int ungetc(unsigned, struct ringb *);
int unputc(struct ringb *);
void initrb(struct ringb *);
void catb(struct ringb *, struct ringb *);
#endif
