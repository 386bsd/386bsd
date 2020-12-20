/* PS2 mouse */
#define MSE_RESET_SCALE	0xe6	/* back to 1:1 ratio for X/Y */
#define MSE_SET_SCALE	0xe7	/* set 2:1 ratio for X/Y */
#define MSE_SET_RES	0xe8	/* set resolution (counts per mm), second byte */
#define  MSE_RES1	0x00	/* 1 counts per mm */
#define  MSE_RES2	0x01	/* 2 counts per mm */
#define  MSE_RES4	0x10	/* 4 counts per mm */
#define  MSE_RES8	0x11	/* 8 counts per mm */
#define MSE_STS_REQ	0xe9	/* status request (rate, res, status) */
#define  MSE_MDREM	0x40	/* remote, instead of stream mode */
#define  MSE_ENAB	0x20	/* eanbled */
#define  MSE_1TO1	0x10	/* 1:1, instead of 1:2 scale */
#define  MSE_LEFTBUT	0x04	/* left mouse button pressed */
#define  MSE_RIGHTBUT	0x01	/* right mouse button pressed */
#define MSE_STREAM	0xea	/* set stream mode */
#define MSE_READDATA	0xeb	/* send a mouse data packet */
#define MSE_READWRAP	0xec	/* cancel mouse echo mode */
#define MSE_SETWRAP	0xee	/* set mouse echo mode */
#define MSE_SETREMOTE	0xf0	/* set mouse remote mode */
#define MSE_READREMOTE	0xf2	/* send mouse type ID byte (0) */
#define MSE_SETSAMPLE	0xf3	/* set sample rate , second byte */
#define  MSE_RATE10	0x0a	/* 10 samples per second */
#define  MSE_RATE20	0x14	/* 10 samples per second */
#define  MSE_RATE200	0xc8	/* 200 samples per second */
#define MSE_ENABLE	0xf4	/* enable stream mode */
#define MSE_DISABLE	0xf5	/* disable stream mode */
#define MSE_SETDEF	0xf6	/* set defaults */
#define MSE_RESEND	0xfe	/* resend last packet */
#define MSE_RESET	0xff	/* reset mouse */
