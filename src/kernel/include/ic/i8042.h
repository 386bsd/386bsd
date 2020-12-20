#define	KBSTATP		0x64	/* kbd controller status port (I) */
#define	 KBS_DIB	0x01	/* kbd data in buffer */
#define	 KBS_IBF	0x02	/* kbd input buffer low */
#define	 KBS_WARM	0x04	/* kbd input buffer low */
#define	 KBS_OCMD	0x08	/* kbd output buffer has command */
#define	 KBS_NOSEC	0x10	/* kbd security lock not engaged */
/* #define	 KBS_TERR	0x20	/* kbd transmission error */
#define	 KBS_AUXDIB	0x20	/* kbd aux port data in buffer */
#define	 KBS_TOERR	0x40	/* kbd timeout error */
#define	 KBS_PERR	0x80	/* kbd parity error */

#define	KBCMDP		0x64	/* kbd controller port (O) */
#define	KBDATAP		0x60	/* kbd data port (I) */
#define	KBOUTP		0x60	/* kbd data port (O) */

/*
 * Keyboard controller commands
 */
#define	K_READ		0x20	/* read keyboard controller RAM (0x20-0x3f) */
#define	K_WRITE		0x60	/* write keyboard controller RAM (0x20-0x3f) */
#define	 K__CMDBYTE	0x0	/* cmd byte address */
#define	 K__SECONBYTE	0x13	/* security on byte address */
#define	 K__SECOFFBYTE	0x14	/* security off byte address */
#define	 K__MAKE1BYTE	0x16	/* first make strip byte address */
#define	 K__MAKE2BYTE	0x17	/* second make strip byte address */
#define	 K__XXX		0x1D	/* byte address */
#define	 K__YYY		0x1F	/* byte address */
#define	K_ISPASSWD	0xA4	/* is there a passwd in keyboard controller ? */
#define	 K__YESPASSWD	0xFA	/* passwd is installed */
#define	 K__NOPASSWD	0xF1	/* no passwd is installed */
#define	K_LDPASSWD	0xA5	/* load a passwd into keyboard controller */
#define	K_ENAPASSWD	0xA6	/* enable a passwd into keyboard controller */
#define	K_DISAUX	0xA7	/* disable aux device on keyboard controller */
#define	K_ENAAUX	0xA8	/* enable aux device on keyboard controller */
#define	K_DISKEY	0xAD	/* disable keyboard on keyboard controller */
#define	K_ENAKEY	0xAE	/* enable keyboard on keyboard controller */
#define	K_READINP	0xC0	/* read input port */
#define	K_READOUTP	0xD0	/* read output port */
#define	K_WRITEOUTP	0xD1	/* write output port */
#define	K_SIMKEYIN	0xD2	/* simulate keyboard input */
#define	K_SIMAUXIN	0xD3	/* simulate aux input */
#define	K_AUXOUT	0xD4	/* send next byte to aux device */
#define	K_TEST		0xE0	/* read test inputs, bits 0 & 1 */
#define	K_PULSE		0xF0	/* pulse output port (0xf0-ff) */
#define	 K__RESET	0x0E	/* reset system */

/*
 * Keyboard controller command byte (K__CMDBYTE)
 */
#define	KC8_TRANS	0x40	/* convert to old scan codes */
/*#define KC8_OLDPC	0x20	/* old 9bit codes instead of new 11bit */
#define	KC8_AUXDIS	0x20	/* disable aux interrupt */
#define	KC8_KEYDIS	0x10	/* disable keyboard */
#define	KC8_IGNSEC	0x08	/* ignore security lock */
#define	KC8_CPU		0x04	/* exit from protected mode reset */
#define	KC8_AUXIEN	0x02	/* enable aux interrupt */
#define	KC8_KEYIEN	0x01	/* enable keyboard interrupt */

#define	ENABLE_CMDBYTE	(KC8_TRANS|KC8_IGNSEC|KC8_CPU|KC8_AUXIEN|KC8_KEYIEN)
#define	DISABLE_CMDBYTE	(KC8_TRANS|KC8_AUXDIS|KC8_KEYDIS|KC8_IGNSEC|KC8_CPU)

/*
 * Output Port Definitions
 */
#define	KC8_KOUT	0x80	/* keyboard data line state */
#define	KC8_KCLK	0x40	/* keyboard clock line state */
#define	KC8_IRQ12	0x20	/* aux interrupt sent */
#define	KC8_IRQ1	0x10	/* keyboard interrupt sent */
#define	KC8_AOUT	0x08	/* aux data line state */
#define	KC8_ACLK	0x04	/* aux clock line state */
#define	KC8_GATEA20	0x02	/* gate a20  */
#define	KC8_RESET	0x01	/* reset */

