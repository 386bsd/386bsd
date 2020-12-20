/*
 * Copyright (c) 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id$
 * ATAPI definitions
 */

/* Redefined task file registers for ATAPI devices */
#define	atapi_bcrlo	wd_cyl_lo	/* byte count, low byte (R/W) */
#define	atapi_bcrhi	wd_cyl_hi	/* byte count, high byte (R/W) */
#define	atapi_iir	wd_seccnt	/* interrupt source (R) */
#define ata_featr	wd_error	/* feature register (W) */

/*
 * ATAPI Status register bits.
 */
#define	ATAPI_STS_BUSY	0x80 /* Command Block accessed drive. */
#define	ATAPI_STS_DRDY	0x40 /* Can accept ATA commands. */
#define	ATAPI_STS_DSC	0x10 /* Seek complete. */
#define	ATAPI_STS_DRQ	0x08 /* Data request bit. */
#define	ATAPI_STS_CORR	0x04 /* Correctable error occured. */
#define	ATAPI_STS_CHECK	0x01 /* Error occured, error reg has sense key. */

#define ATAPI_STS_BITS	"\020\010busy\006rdy\005seekdone\004drq\003corr\001check"

/*
 * ATAPI Error register bits.
 */
#define ATAPI_ERR_SENSE(s) ((s) & 0xf0)	/* sense key */
#define ATAPI_ERR_MCR	0x08 /* media change request */
#define ATAPI_ERR_ABORT	0x04 /* ATA command aborted */
#define ATAPI_ERR_EOM	0x02 /* end of media */
#define ATAPI_ERR_ILI	0x01 /* illegal length indicator */

#define ATAPI_ERR_BITS	"\020\005mcr\003abort\002eom\001ili"

/*
 * ATAPI Interrupt Identification register.
 */
#define ATAPI_IIR_CoD	0x01 /* Command(1) or Data(0) */
#define ATAPI_IIR_IO	0x02 /* Data Direction (r/w 1/0) */

/*
 * ATAPI Feature Register
 */
#define ATAPI_FEATR_DMA	0x01 /* DMA(1) or PIO(0) data transfer */

/*
 * Commands for ATAPI devices
 */
#define	ATAPI_PKT	0xA0 /* send SCSI packet command */
#define	ATAPI_IDENT	0xA1 /* do an ATAPI device identify */
#define	ATAPI_SRST	0x08 /* do an ATAPI soft reset */

/*
 * ATAPI Device Identification
 */
struct atapi_params {
	/* drive info */
	short	atp_config;	/* general configuration */
#define	ATAPI_CFG_CMDPKTSZ(s)	((s) & 3)	/* cmd packet size in PKT CMD */
#define	ATAPI_CFG_CMDDRQTYP(s)	((s) & 0x60)	/* kind of DRQ signal */
#define	ATAPI_CFG_DRQSLOW	0x00		/* 3ms (polled) DRQ */
#define	ATAPI_CFG_DRQINTR	0x20		/* DRQ with IRQ */
#define	ATAPI_CFG_DRQACCEL	0x40		/* 50us (accelerated) DRQ */
#define	ATAPI_CFG_RMV		0x80		/* removable media */
#define	ATAPI_CFG_DEVTYP(s)	((s) & 0x1f00)	/* kind of ATAPI device */
#define	ATAPI_CFG_DEVCDROM	0x500		/* ATAPI CDROM device */
#define	ATAPI_CFG_PROTOTYP(s)	((s) & 0xc000)	/* kind of protocol */
#define	ATAPI_CFG_ATAPIPROTO	0x8000		/* ATAPI protocol */
#define	ATAPI_CFG_ATA_1		0x0000		/* ATA protocol */
#define	ATAPI_CFG_ATA_2		0x4000		/* ATA protocol */

	short	_reserved1[9];
	char	atp_serial[20];	/* serial number */
	short	_reserved2[3];
	char	atp_rev[8];	/* firmware revision */
	char	atp_model[40];	/* model name */
	short	_reserved3[2];
	short	atp_cap;	/* device capabilities */
#define	ATAP_CAP_TSTD	0x2000	/* ATA stndard stndby timer values */
#define	ATAP_CAP_IORDY	0x0800	/* IORDY supported */
#define	ATAP_CAP_IORDYD	0x0400	/* IORDY can be disabled */
#define	ATAP_CAP_LBA	0x0200	/* LBA supported */
#define	ATAP_CAP_DMA	0x0100	/* DMA supported */
	short	_reserved4;
	short	atp_piocycle;	/* programmed I/O cycle time, in nanoseconds */
	short	atp_dmacycle;	/* DMA I/O cycle time, in nanoseconds */
	short	atp_fields;	/* fields valid */
#define	ATAP_FLD_ADV	0x0002	/* Advanced PIO/DMA fields valid (DMA > 1)*/
#define	ATAP_FLD_CUR	0x0001	/* Current log. mapping  fields valid(PIO >=3)*/
	short	atap_logcyl;	/* current translations number of cyls. */
	short	atap_logheads;	/* current translations number of heads */
	short	atap_logsec;	/* current translations number of sectors */
	long	atap_lsectors;	/* current capacity of CHS sectors */
	char	atap_cnsecperint;	/* current sectors per interrupt */
	char	atap_cnflags;
#define	ATAP_CNF_VALID	0x01	/* Current sectors per intr. fields valid */
	long	atap_sectors;	/* capacity of LBA sectors */
	char	atp_swdmamds;	/* single-word DMA modes supported */
	char	atp_swdmamda;	/* single-word DMA mode current */
	char	atp_mwdmamds;	/* multiple-word DMA modes supported */
	char	atp_mwdmamda;	/* multiple-word DMA mode current */
	short	atp_blindcycle; /* blind PIO minimum cycle time, in ns */
};

int atapi_cmdpktsz[4] = { 12, 16, -1, -1 };	/* size in bytes */

/* ATAPI SCSI command packets */
struct atapi_read_10 {
	u_char ac_cmd;		/* operation code - 0x28 */
	u_char ac_rsv1;
	u_char ac_lba[4];	/* logical block number address (MSB first) */
	u_char ac_rsv2;
	u_char ac_tfr[2];	/* transfer length (MSB first) */
	u_char ac_rsv3[3];
};
#define ATAPI10OP	0x28
