/*
 * fsm.h - {Link, IP} Control Protocol Finite State Machine definitions.
 *
 * Copyright (c) 1989 Carnegie Mellon University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Carnegie Mellon University.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: fsm.h,v 1.2 1994/04/11 07:18:35 paulus Exp $
 */

/*
 * Packet header = Code, id, length.
 */
#define HEADERLEN	(sizeof (u_char) + sizeof (u_char) + sizeof (u_short))


/*
 *  CP (LCP, IPCP, etc.) codes.
 */
#define CONFREQ		1	/* Configuration Request */
#define CONFACK		2	/* Configuration Ack */
#define CONFNAK		3	/* Configuration Nak */
#define CONFREJ		4	/* Configuration Reject */
#define TERMREQ		5	/* Termination Request */
#define TERMACK		6	/* Termination Ack */
#define CODEREJ		7	/* Code Reject */


/*
 * Each FSM is described by a fsm_callbacks and a fsm structure.
 */
typedef struct fsm_callbacks {
    void (*resetci)();		/* Reset our Configuration Information */
    int  (*cilen)();		/* Length of our Configuration Information */
    void (*addci)();		/* Add our Configuration Information */
    int  (*ackci)();		/* ACK our Configuration Information */
    int  (*nakci)();		/* NAK our Configuration Information */
    int  (*rejci)();		/* Reject our Configuration Information */
    int  (*reqci)();		/* Request peer's Configuration Information */
    void (*up)();		/* Called when fsm reaches OPENED state */
    void (*down)();		/* Called when fsm leaves OPENED state */
    void (*starting)();		/* Called when we want the lower layer */
    void (*finished)();		/* Called when we don't want the lower layer */
    void (*protreject)();	/* Called when Protocol-Reject received */
    void (*retransmit)();	/* Retransmission is necessary */
    int  (*extcode)();		/* Called when unknown code received */
    char *proto_name;		/* String name for protocol (for messages) */
} fsm_callbacks;


typedef struct fsm {
    int unit;			/* Interface unit number */
    int protocol;		/* Data Link Layer Protocol field value */
    int state;			/* State */
    int flags;			/* Contains option bits */
    u_char id;			/* Current id */
    u_char reqid;		/* Current request id */
    int timeouttime;		/* Timeout time in milliseconds */
    int maxconfreqtransmits;	/* Maximum Configure-Request transmissions */
    int retransmits;		/* Number of retransmissions left */
    int maxtermtransmits;	/* Maximum Terminate-Request transmissions */
    int nakloops;		/* Number of nak loops since last ack */
    int maxnakloops;		/* Maximum number of nak loops tolerated */
    fsm_callbacks *callbacks;	/* Callback routines */
} fsm;


/*
 * Link states.
 */
#define INITIAL		0	/* Down, hasn't been opened */
#define STARTING	1	/* Down, been opened */
#define CLOSED		2	/* Up, hasn't been opened */
#define STOPPED		3	/* Open, waiting for down event */
#define CLOSING		4	/* Terminating the connection, not open */
#define STOPPING	5	/* Terminating, but open */
#define REQSENT		6	/* We've sent a Config Request */
#define ACKRCVD		7	/* We've received a Config Ack */
#define ACKSENT		8	/* We've sent a Config Ack */
#define OPENED		9	/* Connection available */


/*
 * Flags - indicate options controlling FSM operation
 */
#define OPT_PASSIVE	1	/* Don't die if we don't get a response */
#define OPT_RESTART	2	/* Treat 2nd OPEN as DOWN, UP */
#define OPT_SILENT	4	/* Wait for peer to speak first */


/*
 * Timeouts.
 */
#define DEFTIMEOUT	3	/* Timeout time in seconds */
#define DEFMAXTERMREQS	2	/* Maximum Terminate-Request transmissions */
#define DEFMAXCONFREQS	10	/* Maximum Configure-Request transmissions */
#define DEFMAXNAKLOOPS	10	/* Maximum number of nak loops */


/*
 * Prototypes
 */
void fsm_init __ARGS((fsm *));
void fsm_lowerup __ARGS((fsm *));
void fsm_lowerdown __ARGS((fsm *));
void fsm_open __ARGS((fsm *));
void fsm_close __ARGS((fsm *));
void fsm_input __ARGS((fsm *, u_char *, int));
void fsm_protreject __ARGS((fsm *));
void fsm_sdata __ARGS((fsm *, int, int, u_char *, int));


/*
 * Variables
 */
extern int peer_mru[];		/* currently negotiated peer MRU (per unit) */
