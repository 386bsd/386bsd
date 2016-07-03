/* Author: Mike Tierney <floyd@eng.umd.edu> */

/**
 ** register definitions for WD90C31 hardware accelerator
 **/

/* $XFree86: mit/server/ddx/x386/vga256/drivers/pvga1/paradise.h,v 2.0 1993/09/21 15:24:53 dawes Exp $ */

/** BITBLT Control Part 1, Index 1 **/
#define BLIT_CNTRL1   0x0000  /* blit index 0 */
#define BLT_ACT_STAT 0x0800  /* activation/status bit */
#define BLT_DIRECT   0x0400  /* blit direction 0-top->bottom,left->right */
                             /*                1-bottom->top,right->left */
#define BLT_PLANAR   0x0000  /* planar mode */
#define BLT_PACKED   0x0100  /* packed mode, text and 256-color */
#define BLT_DLINEAR  0x0080  /* blit is dest linear, 0 - blit is rectangular */
#define BLT_SLINEAR  0x0040  /* blit is source linear, 0 - blit is rectangular */
#define BLT_DST_SCR  0x0000  /* blit dest is screen memory */
#define BLT_DST_SYS  0x0020  /* blit dest is system IO location */
#define BLT_SRC_COLR 0x0000  /* color source format */
#define BLT_SRC_MONC 0x0004  /* source format is mono from color comparators */
#define BLT_SRC_FCOL 0x0008  /* source format is fixed color (filled rect) */
#define BLT_SRC_MONH 0x000C  /* source format is mono from host */
#define BLT_SRC_SCR  0x0000  /* blit source is screen memory */
#define BLT_SRC_SYS  0x0002  /* blit source is system IO location */

/** BITBLT Control Part 2, Index 1 **/
#define BLIT_CNTRL2   0x1000  /* blit index 0 */
#define BLT_IENABLE  0X0400  /* interrupt enable when blt completes */
#define BLT_QSTART   0X0080  /* quick start when dest reg written to */
#define BLT_UPDST    0x0040  /* update dest on completion of blit */
#define BLT_NOPAT    0x0000  /* no pattern */
#define BLT_8x8PAT   0x0010  /* 8x8 pattern used for source */
#define BLT_MTRNS    0x0008  /* monochrome transparency enable */
#define BLT_PTRNS    0x0004  /* matching pixels are 1-opaque 0-transparent */
#define BLT_TENABLE  0x0001  /* destination transparency enable */

/** BITBLT SOURCE Low, Index 2 */
#define BLIT_SRC_LOW  0x2000

/** BITBLT SOURCE High, Index 3 */
#define BLIT_SRC_HGH  0x3000

/** BITBLT DEST LOW, Index 4 */
#define BLIT_DST_LOW  0x4000

/** BITBLT DEST LOW, Index 5 */
#define BLIT_DST_HGH  0x5000

/** BITBLT Dim X, Index 6 **/
#define BLIT_DIM_X    0x6000

/** BITBLT Dim Y, Index 7 **/
#define BLIT_DIM_Y    0x7000

/** BITBLT Row Pitch, Index 8 **/
#define BLIT_ROW_PTCH 0x8000

/** BITBLT Raster Op, Index 9 **/
#define BLIT_RAS_OP   0x9000
#define ROP_ZERO     0x0000
#define ROP_AND      0x0100
#define ROP_SAND     0x0200
#define ROP_SRC      0x0300
#define ROP_NSAD     0x0400
#define ROP_DST      0x0500
#define ROP_XOR      0x0600
#define ROP_OR       0x0700
#define ROP_NOR      0x0800
#define ROP_XNOR     0x0900
#define ROP_NDEST    0x0A00
#define ROP_SOND     0x0B00
#define ROP_NSRC     0x0C00
#define ROP_NSOD     0x0D00
#define ROP_NAND     0x0E00
#define ROP_ONE      0x0F00

/** BLT Forground Color, Index 10 **/
#define BLIT_FOR_COLR 0xA000

/** BLT Background Color, Index 11 **/
#define BLIT_BCK_COLR 0xB000

/** BLT Transparency Color, Index 12 **/
#define BLIT_TRN_COLR 0xC000

/** BLT Transparency Mask, Index 13 **/
#define BLIT_TRN_MASK 0xD000

/** BLT MASK, Index 14 **/
#define BLIT_MASK 0xE000

/** wait for current blit operation to finish **/
#define WAIT_BLIT  outw(0x23C0, 0x1 | (0x10 << 8)); \
                   while (inw(0x23C2) & BLT_ACT_STAT);

/** wait finite time for current blit operation to finish **/
#define WAIT_BLIT_FINITE  \
                   { int count = 0; \
                     do { outw(0x23C0, 0x1 | (0x10 << 8)); \
                         fprintf (fp, "waiting...\n"); \
                     } while ((inw(0x23C2) & BLT_ACT_STAT) && count++ < 100); \
                     if (count >= 200) {vgaCloseScreen(); exit(-1);}\
                   }


/**
 ** used to save off default functions
 **   sometimes the speedup routines need to bow out to the default funtion
 **   most of the speedups only work if the operation is screen->screen
 **/

extern void (*pvga1_stdcfbFillRectSolidCopy)();
extern int  (*pvga1_stdcfbDoBitbltCopy)();
extern void (*pvga1_stdcfbBitblt)();
extern void (*pvga1_stdcfbFillBoxSolid)();


/**
 ** these variables hold the current known state of the accel registers
 **   that way, time isn't wasted outputing a value to a port that already
 **   has that value.
 **/

extern unsigned int pvga1_blt_cntrl2;
extern unsigned int pvga1_blt_src_low;
extern unsigned int pvga1_blt_src_hgh;
extern unsigned int pvga1_blt_dim_x;
extern unsigned int pvga1_blt_dim_y;
extern unsigned int pvga1_blt_row_pitch;
extern unsigned int pvga1_blt_rop;
extern unsigned int pvga1_blt_for_color;
extern unsigned int pvga1_blt_bck_color;
extern unsigned int pvga1_blt_trns_color;
extern unsigned int pvga1_blt_trns_mask;
extern unsigned int pvga1_blt_planemask;


/**
 ** macros to set each hardware register
 **/

#define SELECT_BLT_REG   outb (0x23C0, 0x1);

#define SET_BLT_CNTRL1(value)  outw(0x23C2, BLIT_CNTRL1 | value);

#define SET_BLT_CNTRL2(value) \
              if (pvga1_blt_cntrl2 != (value)) \
                 outw(0x23C2, (BLIT_CNTRL2 | value)); \
              pvga1_blt_cntrl2 = value;

#define SET_BLT_DST_LOW(value) \
              outw (0x23C2, BLIT_DST_LOW | value);

#define SET_BLT_DST_HGH(value) \
              outw (0x23C2, BLIT_DST_HGH | value);

#define SET_BLT_SRC_LOW(value) \
              if (pvga1_blt_src_low != (value)) \
                 outw (0x23C2, BLIT_SRC_LOW | value); \
              pvga1_blt_src_low = value;

#define SET_BLT_SRC_HGH(value) \
              if (pvga1_blt_src_hgh != (value)) \
                 outw (0x23C2, BLIT_SRC_HGH | value); \
              pvga1_blt_src_hgh = value;

#define SET_BLT_DIM_X(value) \
              if (pvga1_blt_dim_x != (value)) \
                 outw (0x23C2, BLIT_DIM_X | value); \
              pvga1_blt_dim_x = value;

#define SET_BLT_DIM_Y(value) \
              if (pvga1_blt_dim_y != (value)) \
                 outw (0x23C2, BLIT_DIM_Y | value); \
              pvga1_blt_dim_y = value;

#define SET_BLT_ROW_PTCH(value) \
              if (pvga1_blt_row_pitch != (value)) \
                 outw (0x23C2, BLIT_ROW_PTCH | value); \
              pvga1_blt_row_pitch = value;

#define SET_BLT_RAS_OP(value) \
              if (pvga1_blt_rop != (value)) \
                 outw (0x23C2, BLIT_RAS_OP | value); \
              pvga1_blt_rop = value;

#define SET_BLT_FOR_COLR(value) \
              if (pvga1_blt_for_color != (value)) \
                 outw (0x23C2, BLIT_FOR_COLR | value); \
              pvga1_blt_for_color = value;

#define SET_BLT_BCK_COLR(value) \
              if (pvga1_blt_bck_color != (value)) \
                 outw (0x23C2, BLIT_BCK_COLR | value); \
              pvga1_blt_bck_color = value;

#define SET_BLT_TRN_COLR(value) \
              if (pvga1_blt_trns_color != (value)) \
                 outw (0x23C2, BLIT_TRN_COLR | value); \
              pvga1_blt_trns_color = value;

#define SET_BLT_TRN_MASK(value) \
              if (pvga1_blt_trn_mask != (value)) \
                 outw (0x23C2, BLIT_TRN_MASK | value); \
              pvga1_blt_trn_mask = value;

#define SET_BLT_MASK(value) \
              if (pvga1_blt_planemask != (value)) \
                 outw (0x23C2, BLIT_MASK | value); \
              pvga1_blt_planemask = value;
