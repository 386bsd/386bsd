
/*
 * Disk Controller register definitions.
 */
#define	xd_data		0x0		/* data register (R/W - 16 bits) */
#define xd_error	0x1		/* error register (R) */
#define	xd_precomp	xd_error	/* write precompensation (W) */
#define	xd_seccnt	0x2		/* sector count (R/W) */
#define	xd_sector	0x3		/* first sector number (R/W) */
#define	xd_cyl_lo	0x4		/* cylinder address, low byte (R/W) */
#define	xd_cyl_hi	0x5		/* cylinder address, high byte (R/W)*/
#define	xd_sdh		0x6		/* sector size/drive/head (R/W)*/
#define	xd_command	0x7		/* command register (W)	 */
#define	xd_status xd_command		/* immediate status (R)	 */

#define	xd_altsts	0x206	 /*alternate fixed disk status(via 1015) (R)*/
#define	xd_ctlr		0x206	 /*fixed disk controller control(via 1015) (W)*/
#define	xd_digin	0x207	 /* disk controller input(via 1015) (R)*/

/*
 * Status Bits.
 */
#define	XDCS_BUSY	0x80		/* Controller busy bit. */
#define	XDCS_READY	0x40		/* Selected drive is ready */
#define	XDCS_WRTFLT	0x20		/* Write fault */
#define	XDCS_SEEKCMPLT	0x10		/* Seek complete */
#define	XDCS_DRQ	0x08		/* Data request bit. */
#define	XDCS_ECCCOR	0x04		/* ECC correction made in data */
#define	XDCS_INDEX	0x02		/* Index pulse from selected drive */
#define	XDCS_ERR	0x01		/* Error detect bit. */

#define XDCS_BITS	"\020\010busy\006rdy\006wrtflt\005seekdone\004drq\003ecc_cor\002index\001err"

#define XDERR_BITS	"\020\010badblk\007uncorr\006id_crc\005no_id\003abort\002tr000\001no_dam"

/*
 * Commands for Disk Controller.
 */
#define	XDCC_READ	0x20		/* disk read code */
#define	XDCC_WRITE	0x30		/* disk write code */
#define	XDCC_RESTORE	0x10		/* disk restore code -- resets cntlr */
#define	XDCC_FORMAT	0x50		/* disk format code */

#define	XD_STEP		0		/* winchester- default 35us step */

#define	XDSD_IBM	0xa0		/* forced to 512 byte sector, ecc */

