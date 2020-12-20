/*
 * National Semiconductor DS8390 NIC register definitions 
 *
 * Id: if_edreg.h,v 2.2 1993/11/29 16:33:39 davidg Exp davidg
 * $Id: if_edreg.h,v 1.3 93/12/12 19:14:22 root Exp $
 */

/*
 * Page 0 register offsets
 */
#define ED_P0_CR	0x00	/* Command Register */

#define ED_P0_CLDA0	0x01	/* Current Local DMA Addr low (read) */
#define ED_P0_PSTART	0x01	/* Page Start register (write) */

#define ED_P0_CLDA1	0x02	/* Current Local DMA Addr high (read) */
#define ED_P0_PSTOP	0x02	/* Page Stop register (write) */

#define ED_P0_BNRY	0x03	/* Boundary Pointer */

#define ED_P0_TSR	0x04	/* Transmit Status Register (read) */
#define ED_P0_TPSR	0x04	/* Transmit Page Start (write) */

#define ED_P0_NCR	0x05	/* Number of Collisions Reg (read) */
#define ED_P0_TBCR0	0x05	/* Transmit Byte count, low (write) */

#define ED_P0_FIFO	0x06	/* FIFO register (read) */
#define ED_P0_TBCR1	0x06	/* Transmit Byte count, high (write) */

#define ED_P0_ISR	0x07	/* Interrupt Status Register */

#define ED_P0_CRDA0	0x08	/* Current Remote DMA Addr low (read) */
#define ED_P0_RSAR0	0x08	/* Remote Start Address low (write) */

#define ED_P0_CRDA1	0x09	/* Current Remote DMA Addr high (read) */
#define ED_P0_RSAR1	0x09	/* Remote Start Address high (write) */

#define ED_P0_RBCR0	0x0a	/* Remote Byte Count low (write) */

#define ED_P0_RBCR1	0x0b	/* Remote Byte Count high (write) */

#define ED_P0_RSR	0x0c	/* Receive Status (read) */
#define ED_P0_RCR	0x0c	/* Receive Configuration Reg (write) */

#define ED_P0_CNTR0	0x0d	/* frame alignment error counter (read) */
#define ED_P0_TCR	0x0d	/* Transmit Configuration Reg (write) */

#define ED_P0_CNTR1	0x0e	/* CRC error counter (read) */
#define ED_P0_DCR	0x0e	/* Data Configuration Reg (write) */

#define ED_P0_CNTR2	0x0f	/* missed packet counter (read) */
#define ED_P0_IMR	0x0f	/* Interrupt Mask Register (write) */

/*
 * Page 1 register offsets
 */
#define ED_P1_CR	0x00	/* Command Register */
#define ED_P1_PAR0	0x01	/* Physical Address Register 0 */
#define ED_P1_PAR1	0x02	/* Physical Address Register 1 */
#define ED_P1_PAR2	0x03	/* Physical Address Register 2 */
#define ED_P1_PAR3	0x04	/* Physical Address Register 3 */
#define ED_P1_PAR4	0x05	/* Physical Address Register 4 */
#define ED_P1_PAR5	0x06	/* Physical Address Register 5 */
#define ED_P1_CURR	0x07	/* Current RX ring-buffer page */
#define ED_P1_MAR0	0x08	/* Multicast Address Register 0 */
#define ED_P1_MAR1	0x09	/* Multicast Address Register 1 */
#define ED_P1_MAR2	0x0a	/* Multicast Address Register 2 */
#define ED_P1_MAR3	0x0b	/* Multicast Address Register 3 */
#define ED_P1_MAR4	0x0c	/* Multicast Address Register 4 */
#define ED_P1_MAR5	0x0d	/* Multicast Address Register 5 */
#define ED_P1_MAR6	0x0e	/* Multicast Address Register 6 */
#define ED_P1_MAR7	0x0f	/* Multicast Address Register 7 */

/*
 * Page 2 register offsets
 */
#define ED_P2_CR	0x00	/* Command Register */
#define ED_P2_PSTART	0x01	/* Page Start (read) */
#define ED_P2_CLDA0	0x01	/* Current Local DMA Addr 0 (write) */
#define ED_P2_PSTOP	0x02	/* Page Stop (read) */
#define ED_P2_CLDA1	0x02	/* Current Local DMA Addr 1 (write) */
#define ED_P2_RNPP	0x03	/* Remote Next Packet Pointer */
#define ED_P2_TPSR	0x04	/* Transmit Page Start (read) */
#define ED_P2_LNPP	0x05	/* Local Next Packet Pointer */
#define ED_P2_ACU	0x06	/* Address Counter Upper */
#define ED_P2_ACL	0x07	/* Address Counter Lower */
#define ED_P2_RCR	0x0c	/* Receive Configuration Register (read) */
#define ED_P2_TCR	0x0d	/* Transmit Configuration Register (read) */
#define ED_P2_DCR	0x0e	/* Data Configuration Register (read) */
#define ED_P2_IMR	0x0f	/* Interrupt Mask Register (read) */

/*
 *		Command Register (CR) definitions
 */

/*
 * STP: SToP. Software reset command. Takes the controller offline. No
 *	packets will be received or transmitted. Any reception or
 *	transmission in progress will continue to completion before
 *	entering reset state. To exit this state, the STP bit must
 *	reset and the STA bit must be set. The software reset has
 *	executed only when indicated by the RST bit in the ISR being
 *	set.
 */
#define ED_CR_STP	0x01

/*
 * STA: STArt. This bit is used to activate the NIC after either power-up,
 *	or when the NIC has been put in reset mode by software command
 *	or error.
 */
#define ED_CR_STA	0x02

/*
 * TXP: Transmit Packet. This bit must be set to indicate transmission of
 *	a packet. TXP is internally reset either after the transmission is
 *	completed or aborted. This bit should be set only after the Transmit
 *	Byte Count and Transmit Page Start register have been programmed.
 */
#define ED_CR_TXP	0x04

/*
 * RD0, RD1, RD2: Remote DMA Command. These three bits control the operation
 *	of the remote DMA channel. RD2 can be set to abort any remote DMA
 *	command in progress. The Remote Byte Count registers should be cleared
 *	when a remote DMA has been aborted. The Remote Start Addresses are not
 *	restored to the starting address if the remote DMA is aborted.
 *
 *	RD2 RD1 RD0	function
 *	 0   0   0	not allowed
 *	 0   0   1	remote read
 *	 0   1   0	remote write
 *	 0   1   1	send packet
 *	 1   X   X	abort
 */
#define ED_CR_RD0	0x08
#define ED_CR_RD1	0x10
#define ED_CR_RD2	0x20

/*
 * PS0, PS1: Page Select. The two bits select which register set or 'page' to
 *	access.
 *
 *	PS1 PS0		page
 *	 0   0		0
 *	 0   1		1
 *	 1   0		2
 *	 1   1		reserved
 */
#define ED_CR_PS0	0x40
#define ED_CR_PS1	0x80
/* bit encoded aliases */
#define ED_CR_PAGE_0	0x00 /* (for consistency) */
#define ED_CR_PAGE_1	0x40
#define ED_CR_PAGE_2	0x80

/*
 *		Interrupt Status Register (ISR) definitions
 */

/*
 * PRX: Packet Received. Indicates packet received with no errors.
 */
#define ED_ISR_PRX	0x01

/*
 * PTX: Packet Transmitted. Indicates packet transmitted with no errors.
 */
#define ED_ISR_PTX	0x02

/*
 * RXE: Receive Error. Indicates that a packet was received with one or more
 *	the following errors: CRC error, frame alignment error, FIFO overrun,
 *	missed packet.
 */
#define ED_ISR_RXE	0x04

/*
 * TXE: Transmission Error. Indicates that an attempt to transmit a packet
 *	resulted in one or more of the following errors: excessive
 *	collisions, FIFO underrun.
 */
#define ED_ISR_TXE	0x08

/*
 * OVW: OverWrite. Indicates a receive ring-buffer overrun. Incoming network
 *	would exceed (has exceeded?) the boundry pointer, resulting in data
 *	that was previously received and not yet read from the buffer to be
 *	overwritten.
 */
#define ED_ISR_OVW	0x10

/*
 * CNT: Counter Overflow. Set when the MSB of one or more of the Network Talley
 *	Counters has been set.
 */
#define ED_ISR_CNT	0x20

/*
 * RDC: Remote Data Complete. Indicates that a Remote DMA operation has completed.
 */
#define ED_ISR_RDC	0x40

/*
 * RST: Reset status. Set when the NIC enters the reset state and cleared when a
 *	Start Command is issued to the CR. This bit is also set when a receive
 *	ring-buffer overrun (OverWrite) occurs and is cleared when one or more
 *	packets have been removed from the ring. This is a read-only bit.
 */
#define ED_ISR_RST	0x80

/*
 *		Interrupt Mask Register (IMR) definitions
 */

/*
 * PRXE: Packet Received interrupt Enable. If set, a received packet will cause
 *	an interrupt.
 */
#define ED_IMR_PRXE	0x01

/*
 * PTXE: Packet Transmit interrupt Enable. If set, an interrupt is generated when
 *	a packet transmission completes.
 */
#define ED_IMR_PTXE	0x02

/*
 * RXEE: Receive Error interrupt Enable. If set, an interrupt will occur whenever a
 *	packet is received with an error.
 */
#define ED_IMR_RXEE 	0x04

/*
 * TXEE: Transmit Error interrupt Enable. If set, an interrupt will occur whenever
 *	a transmission results in an error.
 */
#define ED_IMR_TXEE	0x08

/*
 * OVWE: OverWrite error interrupt Enable. If set, an interrupt is generated whenever
 *	the receive ring-buffer is overrun. i.e. when the boundry pointer is exceeded.
 */
#define ED_IMR_OVWE	0x10

/*
 * CNTE: Counter overflow interrupt Enable. If set, an interrupt is generated whenever
 *	the MSB of one or more of the Network Statistics counters has been set.
 */
#define ED_IMR_CNTE	0x20

/*
 * RDCE: Remote DMA Complete interrupt Enable. If set, an interrupt is generated
 *	when a remote DMA transfer has completed.
 */
#define ED_IMR_RDCE	0x40

/*
 * bit 7 is unused/reserved
 */

/*
 *		Data Configuration Register (DCR) definitions
 */

/*
 * WTS: Word Transfer Select. WTS establishes byte or word transfers for
 *	both remote and local DMA transfers
 */
#define ED_DCR_WTS	0x01

/*
 * BOS: Byte Order Select. BOS sets the byte order for the host.
 *	Should be 0 for 80x86, and 1 for 68000 series processors
 */
#define ED_DCR_BOS	0x02

/*
 * LAS: Long Address Select. When LAS is 1, the contents of the remote
 *	DMA registers RSAR0 and RSAR1 are used to provide A16-A31
 */
#define ED_DCR_LAS	0x04

/*
 * LS: Loopback Select. When 0, loopback mode is selected. Bits D1 and D2
 *	of the TCR must also be programmed for loopback operation.
 *	When 1, normal operation is selected.
 */
#define ED_DCR_LS	0x08

/*
 * AR: Auto-initialize Remote. When 0, data must be removed from ring-buffer
 *	under program control. When 1, remote DMA is automatically initiated
 *	and the boundry pointer is automatically updated
 */
#define ED_DCR_AR	0x10

/*
 * FT0, FT1: Fifo Threshold select.
 *		FT1	FT0	Word-width	Byte-width
 *		 0	 0	1 word		2 bytes
 *		 0	 1	2 words		4 bytes
 *		 1	 0	4 words		8 bytes
 *		 1	 1	8 words		12 bytes
 *
 *	During transmission, the FIFO threshold indicates the number of bytes
 *	or words that the FIFO has filled from the local DMA before BREQ is
 *	asserted. The transmission threshold is 16 bytes minus the receiver
 *	threshold.
 */
#define ED_DCR_FT0	0x20
#define ED_DCR_FT1	0x40

/*
 * bit 7 (0x80) is unused/reserved
 */

/*
 *		Transmit Configuration Register (TCR) definitions
 */

/*
 * CRC: Inhibit CRC. If 0, CRC will be appended by the transmitter, if 0, CRC
 *	is not appended by the transmitter.
 */
#define ED_TCR_CRC	0x01

/*
 * LB0, LB1: Loopback control. These two bits set the type of loopback that is
 *	to be performed.
 *
 *	LB1 LB0		mode
 *	 0   0		0 - normal operation (DCR_LS = 0)
 *	 0   1		1 - internal loopback (DCR_LS = 0)
 *	 1   0		2 - external loopback (DCR_LS = 1)
 *	 1   1		3 - external loopback (DCR_LS = 0)
 */
#define ED_TCR_LB0	0x02
#define ED_TCR_LB1	0x04

/*
 * ATD: Auto Transmit Disable. Clear for normal operation. When set, allows
 *	another station to disable the NIC's transmitter by transmitting to
 *	a multicast address hashing to bit 62. Reception of a multicast address
 *	hashing to bit 63 enables the transmitter.
 */
#define ED_TCR_ATD	0x08

/*
 * OFST: Collision Offset enable. This bit when set modifies the backoff
 *	algorithm to allow prioritization of nodes.
 */
#define ED_TCR_OFST	0x10
 
/*
 * bits 5, 6, and 7 are unused/reserved
 */

/*
 *		Transmit Status Register (TSR) definitions
 */

/*
 * PTX: Packet Transmitted. Indicates successful transmission of packet.
 */
#define ED_TSR_PTX	0x01

/*
 * bit 1 (0x02) is unused/reserved
 */

/*
 * COL: Transmit Collided. Indicates that the transmission collided at least
 *	once with another station on the network.
 */
#define ED_TSR_COL	0x04

/*
 * ABT: Transmit aborted. Indicates that the transmission was aborted due to
 *	excessive collisions.
 */
#define ED_TSR_ABT	0x08

/*
 * CRS: Carrier Sense Lost. Indicates that carrier was lost during the
 *	transmission of the packet. (Transmission is not aborted because
 *	of a loss of carrier)
 */
#define ED_TSR_CRS	0x10

/*
 * FU: FIFO Underrun. Indicates that the NIC wasn't able to access bus/
 *	transmission memory before the FIFO emptied. Transmission of the
 *	packet was aborted.
 */
#define ED_TSR_FU	0x20

/*
 * CDH: CD Heartbeat. Indicates that the collision detection circuitry
 *	isn't working correctly during a collision heartbeat test.
 */
#define ED_TSR_CDH	0x40

/*
 * OWC: Out of Window Collision: Indicates that a collision occurred after
 *	a slot time (51.2us). The transmission is rescheduled just as in
 *	normal collisions.
 */
#define ED_TSR_OWC	0x80

/*
 *		Receiver Configuration Register (RCR) definitions
 */

/*
 * SEP: Save Errored Packets. If 0, error packets are discarded. If set to 1,
 *	packets with CRC and frame errors are not discarded.
 */
#define ED_RCR_SEP	0x01

/*
 * AR: Accept Runt packet. If 0, packet with less than 64 byte are discarded.
 *	If set to 1, packets with less than 64 byte are not discarded.
 */
#define ED_RCR_AR	0x02

/*
 * AB: Accept Broadcast. If set, packets sent to the broadcast address will be
 *	accepted.
 */
#define ED_RCR_AB	0x04

/*
 * AM: Accept Multicast. If set, packets sent to a multicast address are checked
 *	for a match in the hashing array. If clear, multicast packets are ignored.
 */
#define ED_RCR_AM	0x08

/*
 * PRO: Promiscuous Physical. If set, all packets with a physical addresses are
 *	accepted. If clear, a physical destination address must match this
 *	station's address. Note: for full promiscuous mode, RCR_AB and RCR_AM
 *	must also be set. In addition, the multicast hashing array must be set
 *	to all 1's so that all multicast addresses are accepted.
 */
#define ED_RCR_PRO	0x10

/*
 * MON: Monitor Mode. If set, packets will be checked for good CRC and framing,
 *	but are not stored in the ring-buffer. If clear, packets are stored (normal
 *	operation).
 */
#define ED_RCR_MON	0x20

/*
 * bits 6 and 7 are unused/reserved.
 */

/*
 *		Receiver Status Register (RSR) definitions
 */

/*
 * PRX: Packet Received without error.
 */
#define ED_RSR_PRX	0x01

/*
 * CRC: CRC error. Indicates that a packet has a CRC error. Also set for frame
 *	alignment errors.
 */
#define ED_RSR_CRC	0x02

/*
 * FAE: Frame Alignment Error. Indicates that the incoming packet did not end on
 *	a byte boundry and the CRC did not match at the last byte boundry.
 */
#define ED_RSR_FAE	0x04

/*
 * FO: FIFO Overrun. Indicates that the FIFO was not serviced (during local DMA)
 *	causing it to overrun. Reception of the packet is aborted.
 */
#define ED_RSR_FO	0x08

/*
 * MPA: Missed Packet. Indicates that the received packet couldn't be stored in
 *	the ring-buffer because of insufficient buffer space (exceeding the
 *	boundry pointer), or because the transfer to the ring-buffer was inhibited
 *	by RCR_MON - monitor mode.
 */
#define ED_RSR_MPA	0x10

/*
 * PHY: Physical address. If 0, the packet received was sent to a physical address.
 *	If 1, the packet was accepted because of a multicast/broadcast address
 *	match.
 */
#define ED_RSR_PHY	0x20

/*
 * DIS: Receiver Disabled. Set to indicate that the receiver has enetered monitor
 *	mode. Cleared when the receiver exits monitor mode.
 */
#define ED_RSR_DIS	0x40

/*
 * DFR: Deferring. Set to indicate a 'jabber' condition. The CRS and COL inputs
 *	are active, and the transceiver has set the CD line as a result of the
 *	jabber.
 */
#define ED_RSR_DFR	0x80

/*
 * receive ring discriptor
 *
 * The National Semiconductor DS8390 Network interface controller uses
 * the following receive ring headers.  The way this works is that the
 * memory on the interface card is chopped up into 256 bytes blocks.
 * A contiguous portion of those blocks are marked for receive packets
 * by setting start and end block #'s in the NIC.  For each packet that
 * is put into the receive ring, one of these headers (4 bytes each) is
 * tacked onto the front.
 */
struct ed_ring	{
	struct edr_status {		/* received packet status	*/
	    u_char rs_prx:1,		/* packet received intack	*/
		   rs_crc:1,		/* crc error		*/
	           rs_fae:1,		/* frame alignment error	*/
	           rs_fo:1,		/* fifo overrun		*/
	           rs_mpa:1,		/* packet received intack	*/
	           rs_phy:1,		/* packet received intack	*/
	           rs_dis:1,		/* packet received intack	*/
	           rs_dfr:1;		/* packet received intack	*/
	} ed_rcv_status;		/* received packet status	*/
	u_char	next_packet;		/* pointer to next packet	*/
	u_short	count;			/* bytes in packet (length + 4)	*/
};

/*
 * 				Common constants
 */
#define ED_PAGE_SIZE		256		/* Size of RAM pages in bytes */
#define ED_TXBUF_SIZE		6		/* Size of TX buffer in pages */

/*
 * Vendor types
 */
#define ED_VENDOR_WD_SMC	0x00		/* Western Digital/SMC */
#define ED_VENDOR_3COM		0x01		/* 3Com */
#define ED_VENDOR_NOVELL	0x02		/* Novell */

/*
 * Compile-time config flags
 */
/*
 * this sets the default for enabling/disablng the tranceiver
 */
#define ED_FLAGS_DISABLE_TRANCEIVER	0x0001

/*
 * This forces the board to be used in 8/16bit mode even if it
 *	autoconfigs differently
 */
#define ED_FLAGS_FORCE_8BIT_MODE	0x0002
#define ED_FLAGS_FORCE_16BIT_MODE	0x0004

/*
 * This disables the use of double transmit buffers.
 */
#define ED_FLAGS_NO_MULTI_BUFFERING	0x0008

/*
 * This forces all operations with the NIC memory to use Programmed
 *	I/O (i.e. not via shared memory)
 */
#define ED_FLAGS_FORCE_PIO		0x0010

/*
 *		Definitions for Western digital/SMC WD80x3 series ASIC
 */
/*
 * Memory Select Register (MSR)
 */
#define ED_WD_MSR	0

#define ED_WD_MSR_ADDR	0x3f	/* Memory decode bits 18-13 */
#define ED_WD_MSR_MENB	0x40	/* Memory enable */
#define ED_WD_MSR_RST	0x80	/* Reset board */

/*
 * Interface Configuration Register (ICR)
 */
#define ED_WD_ICR	1

#define ED_WD_ICR_16BIT	0x01	/* 16-bit interface */
#define ED_WD_ICR_OAR	0x02	/* select register. 0=BIO 1=EAR */
#define ED_WD_ICR_IR2	0x04	/* high order bit of encoded IRQ */
#define ED_WD_ICR_MSZ	0x08	/* memory size (0=8k 1=32k) */
#define ED_WD_ICR_RLA	0x10	/* recall LAN address */
#define ED_WD_ICR_RX7	0x20	/* recall all but i/o and LAN address */
#define	ED_WD_ICR_RIO	0x40	/* recall i/o address */
#define ED_WD_ICR_STO	0x80	/* store to non-volatile memory */

/*
 * IO Address Register (IAR)
 */
#define ED_WD_IAR	2

/*
 * EEROM Address Register
 */
#define ED_WD_EAR	3

/*
 * Interrupt Request Register (IRR)
 */
#define ED_WD_IRR	4

#define	ED_WD_IRR_0WS	0x01	/* use 0 wait-states on 8 bit bus */
#define ED_WD_IRR_OUT1	0x02	/* WD83C584 pin 1 output */
#define ED_WD_IRR_OUT2	0x04	/* WD83C584 pin 2 output */
#define ED_WD_IRR_OUT3	0x08	/* WD83C584 pin 3 output */
#define ED_WD_IRR_FLASH	0x10	/* Flash RAM is in the ROM socket */

/*
 * The three bit of the encoded IRQ are decoded as follows:
 *
 *	IR2 IR1 IR0	IRQ
 *	 0   0   0	 2/9
 *	 0   0   1	 3
 *	 0   1   0	 5
 *	 0   1   1	 7
 *	 1   0   0	 10
 *	 1   0   1	 11
 *	 1   1   0	 15
 *	 1   1   1	 4
 */
#define ED_WD_IRR_IR0	0x20	/* bit 0 of encoded IRQ */
#define ED_WD_IRR_IR1	0x40	/* bit 1 of encoded IRQ */
#define ED_WD_IRR_IEN	0x80	/* Interrupt enable */

/*
 * LA Address Register (LAAR)
 */
#define ED_WD_LAAR	5

#define ED_WD_LAAR_ADDRHI	0x1f	/* bits 23-19 of RAM address */
#define ED_WD_LAAR_0WS16	0x20	/* enable 0 wait-states on 16 bit bus */
#define ED_WD_LAAR_L16EN	0x40	/* enable 16-bit operation */
#define ED_WD_LAAR_M16EN	0x80	/* enable 16-bit memory access */

/* i/o base offset to station address/card-ID PROM */
#define ED_WD_PROM	8

/* i/o base offset to CARD ID */
#define ED_WD_CARD_ID	ED_WD_PROM+6

/* Board type codes in card ID */
#define ED_TYPE_WD8003S		0x02
#define ED_TYPE_WD8003E		0x03
#define ED_TYPE_WD8013EBT	0x05
#define ED_TYPE_WD8013W		0x26
#define ED_TYPE_WD8013EP	0x27
#define ED_TYPE_WD8013WC	0x28
#define ED_TYPE_WD8013EBP	0x2c
#define ED_TYPE_WD8013EPC	0x29
#define ED_TYPE_SMC8216T	0x2a
#define ED_TYPE_SMC8216C	0x2b

/* Bit definitions in card ID */
#define	ED_WD_REV_MASK		0x1f		/* Revision mask */
#define	ED_WD_SOFTCONFIG	0x20		/* Soft config */
#define	ED_WD_LARGERAM		0x40		/* Large RAM */
#define	ED_MICROCHANEL		0x80		/* Microchannel bus (vs. isa) */

/*
 * Checksum total. All 8 bytes in station address PROM will add up to this
 */
#define ED_WD_ROM_CHECKSUM_TOTAL	0xFF

#define ED_WD_NIC_OFFSET	0x10		/* I/O base offset to NIC */
#define ED_WD_ASIC_OFFSET	0		/* I/O base offset to ASIC */
#define ED_WD_IO_PORTS		32		/* # of i/o addresses used */

#define ED_WD_PAGE_OFFSET	0	/* page offset for NIC access to mem */

/*
 *			Definitions for 3Com 3c503
 */
#define ED_3COM_NIC_OFFSET	0
#define ED_3COM_ASIC_OFFSET	0x400		/* offset to nic i/o regs */

/*
 * XXX - The I/O address range is fragmented in the 3c503; this is the
 *	number of regs at iobase.
 */
#define ED_3COM_IO_PORTS	16		/* # of i/o addresses used */

/* tx memory starts in second bank on 8bit cards */
#define ED_3COM_TX_PAGE_OFFSET_8BIT	0x20

/* tx memory starts in first bank on 16bit cards */
#define ED_3COM_TX_PAGE_OFFSET_16BIT	0x0

/* ...and rx memory starts in second bank */
#define ED_3COM_RX_PAGE_OFFSET_16BIT	0x20


/*
 *	Page Start Register. Must match PSTART in NIC
 */
#define ED_3COM_PSTR		0

/*
 *	Page Stop Register. Must match PSTOP in NIC
 */
#define ED_3COM_PSPR		1

/*
 *	Drq Timer Register. Determines number of bytes to be transfered during
 *		a DMA burst.
 */
#define ED_3COM_DQTR		2

/*
 *	Base Configuration Register. Read-only register which contains the
 *		board-configured I/O base address of the adapter. Bit encoded.
 */
#define ED_3COM_BCFR		3

#define ED_3COM_BCFR_2E0	0x01
#define ED_3COM_BCFR_2A0	0x02
#define ED_3COM_BCFR_280	0x04
#define ED_3COM_BCFR_250	0x08
#define ED_3COM_BCFR_350	0x10
#define ED_3COM_BCFR_330	0x20
#define ED_3COM_BCFR_310	0x40
#define ED_3COM_BCFR_300	0x80

/*
 *	EPROM Configuration Register. Read-only register which contains the
 *		board-configured memory base address. Bit encoded.
 */
#define ED_3COM_PCFR		4

#define ED_3COM_PCFR_C8000	0x10
#define ED_3COM_PCFR_CC000	0x20
#define ED_3COM_PCFR_D8000	0x40
#define ED_3COM_PCFR_DC000	0x80

/*
 *	GA Configuration Register. Gate-Array Configuration Register.
 */
#define ED_3COM_GACFR		5

/*
 * mbs2  mbs1  mbs0		start address
 *  0     0     0		0x0000
 *  0     0     1		0x2000
 *  0     1     0		0x4000
 *  0     1     1		0x6000
 *
 *	Note that with adapters with only 8K, the setting for 0x2000 must
 *		always be used.
 */
#define ED_3COM_GACFR_MBS0	0x01
#define ED_3COM_GACFR_MBS1	0x02
#define ED_3COM_GACFR_MBS2	0x04

#define ED_3COM_GACFR_RSEL	0x08	/* enable shared memory */
#define ED_3COM_GACFR_TEST	0x10	/* for GA testing */
#define ED_3COM_GACFR_OWS	0x20	/* select 0WS access to GA */
#define ED_3COM_GACFR_TCM	0x40	/* Mask DMA interrupts */
#define ED_3COM_GACFR_NIM	0x80	/* Mask NIC interrupts */

/*
 *	Control Register. Miscellaneous control functions.
 */
#define ED_3COM_CR		6

#define ED_3COM_CR_RST		0x01	/* Reset GA and NIC */
#define ED_3COM_CR_XSEL		0x02	/* Transceiver select. BNC=1(def) AUI=0 */
#define ED_3COM_CR_EALO		0x04	/* window EA PROM 0-15 to I/O base */
#define ED_3COM_CR_EAHI		0x08	/* window EA PROM 16-31 to I/O base */
#define ED_3COM_CR_SHARE	0x10	/* select interrupt sharing option */
#define ED_3COM_CR_DBSEL	0x20	/* Double buffer select */
#define ED_3COM_CR_DDIR		0x40	/* DMA direction select */
#define ED_3COM_CR_START	0x80	/* Start DMA controller */

/*
 *	Status Register. Miscellaneous status information.
 */
#define ED_3COM_STREG		7

#define ED_3COM_STREG_REV	0x07	/* GA revision */
#define ED_3COM_STREG_DIP	0x08	/* DMA in progress */
#define ED_3COM_STREG_DTC	0x10	/* DMA terminal count */
#define ED_3COM_STREG_OFLW	0x20	/* Overflow */
#define ED_3COM_STREG_UFLW	0x40	/* Underflow */
#define ED_3COM_STREG_DPRDY	0x80	/* Data port ready */

/*
 *	Interrupt/DMA Configuration Register
 */
#define ED_3COM_IDCFR		8

#define ED_3COM_IDCFR_DRQ0	0x01	/* DMA request 1 select */
#define ED_3COM_IDCFR_DRQ1	0x02	/* DMA request 2 select */
#define ED_3COM_IDCFR_DRQ2	0x04	/* DMA request 3 select */
#define ED_3COM_IDCFR_UNUSED	0x08	/* not used */
#define ED_3COM_IDCFR_IRQ2	0x10	/* Interrupt request 2 select */
#define ED_3COM_IDCFR_IRQ3	0x20	/* Interrupt request 3 select */
#define ED_3COM_IDCFR_IRQ4	0x40	/* Interrupt request 4 select */
#define ED_3COM_IDCFR_IRQ5	0x80	/* Interrupt request 5 select */

/*
 *	DMA Address Register MSB
 */
#define ED_3COM_DAMSB		9

/*
 *	DMA Address Register LSB
 */
#define ED_3COM_DALSB		0x0a

/*
 *	Vector Pointer Register 2
 */
#define ED_3COM_VPTR2		0x0b

/*
 *	Vector Pointer Register 1
 */
#define ED_3COM_VPTR1		0x0c

/*
 *	Vector Pointer Register 0
 */
#define ED_3COM_VPTR0		0x0d

/*
 *	Register File Access MSB
 */
#define ED_3COM_RFMSB		0x0e

/*
 *	Register File Access LSB
 */
#define ED_3COM_RFLSB		0x0f

/*
 *		 Definitions for Novell NE1000/2000 boards
 */

/*
 * Board type codes
 */
#define ED_TYPE_NE1000		0x01
#define ED_TYPE_NE2000		0x02

/*
 * Register offsets/total
 */
#define ED_NOVELL_NIC_OFFSET	0x00
#define ED_NOVELL_ASIC_OFFSET	0x10
#define ED_NOVELL_IO_PORTS	32

/*
 * Remote DMA data register; for reading or writing to the NIC mem
 *	via programmed I/O (offset from ASIC base)
 */
#define ED_NOVELL_DATA		0x00

/*
 * Reset register; reading from this register causes a board reset
 */
#define ED_NOVELL_RESET		0x0f
