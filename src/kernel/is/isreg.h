/* Declarations specific to this driver */
#define NTBUF 2
#define TLEN 1
#define NRBUF 8
#define RLEN 3
#define BUFSIZE 1518
#define RAP 0xe
#define RDP 0xc

/* Control and status register 0 flags */

#define ERR	0x8000
#define BABL	0x4000
#define CERR	0x2000
#define MISS	0x1000
#define MERR	0x0800
#define RINT	0x0400
#define TINT	0x0200
#define IDON	0x0100
#define INTR	0x0080
#define INEA	0x0040
#define RXON	0x0020
#define TXON	0x0010
#define TDMD	0x0008
#define STOP	0x0004
#define STRT	0x0002
#define INIT	0x0001

/* Coontrol and status register 3 flags */

#define BSWP	0x0004
#define ACON	0x0002
#define BCON	0x0001

/* Initialisation block (must be on word boundary) */

struct init_block {
       u_short mode;		/* Mode register 			*/
       u_char  padr[6];		/* Ethernet address 			*/
       u_char ladrf[8];		/* Logical address filter (multicast) 	*/
       u_short rdra;		/* Low order pointer to receive ring 	*/
       u_short rlen;		/* High order pointer and no. rings 	*/
       u_short tdra;		/* Low order pointer to transmit ring 	*/
       u_short tlen;		/* High order pointer and no rings 	*/
       };

/* Mode settings */

#define PROM	0x8000		/* Promiscuous		*/
#define INTL	0x0040		/* Internal loopback	*/
#define DRTY	0x0020		/* Disable retry 	*/
#define COLL	0x0010		/* Force collision 	*/
#define DTCR	0x0008		/* Disable transmit crc	*/
#define LOOP	0x0004		/* Loop back 		*/
#define DTX	0x0002		/* Disable transmitter 	*/
#define DRX	0x0001		/* Disable receiver 	*/

/* Message descriptor structure */

struct mds {
       u_short addr;
       u_short flags;
       u_short bcnt;
       u_short mcnt;
       };

/* Receive ring status flags */

#define OWN	0x8000		/* Owner bit, 0=host, 1=Lance 	*/
#define MDERR	0x4000		/* Error 			*/	
#define FRAM	0x2000		/* Framing error error 		*/
#define OFLO	0x1000		/* Silo overflow 		*/
#define CRC	0x0800		/* CRC error 			*/
#define RBUFF	0x0400		/* Buffer error 		*/
#define STP	0x0200		/* Start of packet 		*/
#define ENP	0x0100		/* End of packet 		*/

/* Transmit ring flags */

#define MORE	0x1000		/* More than 1 retry 	*/
#define ONE	0x0800		/* One retry 		*/
#define DEF	0x0400		/* Deferred transmit 	*/

/* Transmit errors */

#define TBUFF	0x8000		/* Buffer error 	*/
#define UFLO	0x4000		/* Silo underflow	*/
#define LCOL	0x1000		/* Late collision 	*/
#define LCAR	0x0800		/* Loss of carrier 	*/
#define RTRY	0x0400		/* Tried 16 times 	*/
