/*
 * $XFree86: mit/server/ddx/x386/vga256/drivers/cirrus/cir_driver.c,v 2.21 1993/10/16 17:32:55 dawes Exp $
 * Header: /usr/local/src/Xaccel/cirrus/RCS/driver.c,v 1.6 1993/04/04 17:57:44 bill Exp
 *
 * Copyright 1993 by Bill Reynolds, Santa Fe, New Mexico
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Bill Reynolds not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Bill Reynolds makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * BILL REYNOLDS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL BILL REYNOLDS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Bill Reynolds, bill@goshawk.lanl.gov
 * Modifications: David Dawes, <dawes@physics.su.oz.au>
 * Modifications: Piercarlo Grandi, Aberystwyth (pcg@aber.ac.uk)
 * Modifications: Simon P. Cooper, <scooper@vizlab.rutgers.edu>
 *
 */

/* 
 * Modifications to this file for the Cirrus 62x5 chips and color LCD
 * displays were made by Prof. Hank Dietz, Purdue U. School of EE, W.
 * Lafayette, IN, 47907-1285.  These modifications were made very
 * quickly and tested only on a Sager 8200 laptop running Linux SLS
 * 1.03, where they appear to work.  In any case, both Hank and Purdue
 * offer these modifications with the same disclaimers and conditions
 * invoked by Bill Reynolds above:  use these modifications at your
 * own risk and don't blame us.  Neither should you infer that Purdue
 * endorses anything.
 *
 *					hankd@ecn.purdue.edu
 */

/*
 * Note: defining ALLOW_OUT_OF_SPEC_CLOCKS will allow the driver to program
 * clock frequencies higher than those recommended in the Cirrus data book.
 * If you enable this, you do so at your OWN risk, and YOU RISK DAMAGING
 * YOUR HARDWARE.  You have been warned.
 */

#undef ALLOW_OUT_OF_SPEC_CLOCKS
#ifdef ALLOW_OUT_OF_SPEC_CLOCKS
#define MAX_OUT_OF_SPEC_CLOCK	100500
#endif

#include "X.h"
#include "input.h"
#include "screenint.h"

#include "compiler.h"

#include "x386.h"
#include "x386Priv.h"
#include "xf86_OSlib.h"
#include "xf86_HWlib.h"
#define XCONFIG_FLAGS_ONLY
#include "xf86_Config.h"
#include "vga.h"

#include "cir_driver.h"
#include "cfbfuncs.h"

static int cirrusChip;

#define CLGD5420_ID 0x22
#define CLGD5422_ID 0x23
#define CLGD5424_ID 0x25
#define CLGD5426_ID 0x24
#define CLGD5428_ID 0x26
#define CLGD6205_ID 0x02
#define CLGD6215_ID 0x22  /* Hmmm... looks like a 5420 or 5422 */
#define CLGD6225_ID 0x32
#define CLGD6235_ID 0x06

#define CLGD5420    0
#define CLGD5422    1
#define CLGD5424    2
#define CLGD5426    3
#define CLGD5428    4
#define CLGD6205    5
#define CLGD6215    6
#define CLGD6225    7
#define CLGD6235    8
#define LASTCLGD    CLGD6235

#define Is_62x5(x)  ((x) >= CLGD6205)

				/* For now, only save a couple of the */
				/* extensions. */
typedef struct {
  vgaHWRec std;               /* good old IBM VGA */
  unsigned char GR9;		/* Graphics Offset1 */
  unsigned char GRA;		/* Graphics Offset2 */
  unsigned char GRB;		/* Graphics Extensions Control */
  unsigned char SR7;		/* Extended Sequencer */
  unsigned char SRE;		/* VCLK Numerator */
  unsigned char SRF;		/* DRAM Control */
  unsigned char SR1E;		/* VCLK Denominator */
  unsigned char CR19;		/* Interlace End */
  unsigned char CR1A;		/* Miscellaneous Control */
  unsigned char CR1B;		/* Extended Display Control */
  unsigned char RAX;		/* 62x5 LCD Timing -- TFT HSYNC */
  } vgacirrusRec, *vgacirrusPtr;

unsigned char SavedExtSeq;

static Bool lcd_is_on = FALSE;	/* for 62x5 */

static Bool     cirrusProbe();
static char *   cirrusIdent();
static Bool     cirrusClockSelect();
static void     cirrusEnterLeave();
static Bool     cirrusInit();
static void *   cirrusSave();
static void     cirrusRestore();
static void     cirrusAdjust();
static void	cirrusFbInit();

extern void     cirrusSetRead();
extern void     cirrusSetWrite();
extern void     cirrusSetReadWrite();

extern void     cirrusSetRead2MB();
extern void     cirrusSetWrite2MB();
extern void     cirrusSetReadWrite2MB();

int	CirrusMemTop;

vgaVideoChipRec CIRRUS = {
  cirrusProbe,			/* ChipProbe()*/
  cirrusIdent,			/* ChipIdent(); */
  cirrusEnterLeave,		/* ChipEnterLeave() */
  cirrusInit,			/* ChipInit() */
  cirrusSave,			/* ChipSave() */
  cirrusRestore,		/* ChipRestore() */
  cirrusAdjust,			/* ChipAdjust() */
  NoopDDA,			/* ChipSaveScreen() */
  NoopDDA,			/* ChipGetMode() */
  cirrusFbInit,			/* ChipFbInit() */
  cirrusSetRead,		/* ChipSetRead() */
  cirrusSetWrite,		/* ChipSetWrite() */
  cirrusSetReadWrite,	        /* ChipSetReadWrite() */
  0x10000,			/* ChipMapSize */
  0x08000,			/* ChipSegmentSize, 32k*/
  15,				/* ChipSegmentShift */
  0x7FFF,			/* ChipSegmentMask */
  0x00000, 0x08000,		/* ChipReadBottom, ChipReadTop  */
  0x08000, 0x10000,		/* ChipWriteBottom,ChipWriteTop */
  TRUE,				/* ChipUse2Banks, Uses 2 bank */
  VGA_DIVIDE_VERT,		/* ChipInterlaceType -- don't divide verts */
  {0,},				/* ChipOptionFlags */
  8,				/* ChipRounding */
};

typedef struct {
  unsigned char numer;
  unsigned char denom;
  } cirrusClockRec;

static cirrusClockRec cirrusClockTab[] = {
  { 0x4A, 0x2B },		/* 25.227 */
  { 0x5B, 0x2F },		/* 28.325 */
  { 0x45, 0x30 }, 		/* 41.164 */
  { 0x7E, 0x33 },		/* 36.082 */
  { 0x42, 0x1F },		/* 31.500 */
  { 0x51, 0x3A },		/* 39.992 */
  { 0x55, 0x36 },		/* 45.076 */
  { 0x65, 0x3A },		/* 49.867 */
  { 0x76, 0x34 },		/* 64.983 */
  { 0x7E, 0x32 },		/* 72.163 */
  { 0x6E, 0x2A },		/* 75.000 */
  { 0x5F, 0x22 },		/* 80.013 */   /* all except 5420 */
  { 0x7D, 0x2A },		/* 85.226 */   /* 5426 and 5428 */
  /* These are all too high according to the databook.  They can be enabled
     with the "16clocks" option  *if* this driver has been compiled with
     ALLOW_OUT_OF_SPEC_CLOCKS defined. */
  { 0x7E, 0x28 },		/* 90.203 */
  { 0x7E, 0x26 },		/* 94.950 */
  { 0x7E, 0x24 },		/* 100.226 */

};

#define NUM_CIRRUS_CLOCKS (sizeof(cirrusClockTab)/sizeof(cirrusClockRec))

/* CLOCK_FACTOR is double the osc freq in kHz (osc = 14.31818 MHz) */
#define CLOCK_FACTOR 28636

/* clock in kHz is (numer * CLOCK_FACTOR / (denom & 0x3E)) >> (denom & 1) */
#define CLOCKVAL(n, d) \
     ((((n) & 0x7F) * CLOCK_FACTOR / ((d) & 0x3E)) >> ((d) & 1))

int cirrusClockLimit[] = {
  75200,	/* 5420 */
  80100,	/* 5422 */
  80100,	/* 5424 */
  85500,	/* 5426 */
  85500,	/* 5428 */

/* just guessing.... dwex */
  75200,	/* 6205 */
  75200,	/* 6215 */
  75200,	/* 6225 */
  75200,	/* 6235 */
};

#define new ((vgacirrusPtr)vgaNewVideoState)

/*
 * cirrusIdent -- 
 */
static char *
cirrusIdent(n)
     int n;
{
  static char *chipsets[] = {"clgd5420", "clgd5422", "clgd5424", "clgd5426",
			     "clgd5428",
			     "clgd6205", "clgd6215", "clgd6225", "clgd6235",
			    };

  if (n + 1 > sizeof(chipsets) / sizeof(char *))
    return(NULL);
  else
    return(chipsets[n]);
}

/*
 * cirrusCheckClock --
 *	check if the clock is supported by the chipset
 */
static Bool
cirrusCheckClock(chip, clockno)
  int chip;
  int clockno;
{
  unsigned clockval;

  clockval = CLOCKVAL(cirrusClockTab[clockno].numer,
		      cirrusClockTab[clockno].denom);

  if (clockval > cirrusClockLimit[chip])
  {
    ErrorF("CIRRUS: clock %7.3f is too high for %s (max is %7.3f)\n",
	   clockval / 1000.0, cirrusIdent(chip),
	   cirrusClockLimit[chip] / 1000.0);

#ifdef ALLOW_OUT_OF_SPEC_CLOCKS
    if (OFLG_ISSET(OPTION_16CLKS, &vga256InfoRec.options))
      {
	ErrorF ("CIRRUS: Out of spec. clocks option is enabled\n");
	return (TRUE);
      }
#endif

    return(FALSE);
  }
  return(TRUE);
}

/*
 * cirrusClockSelect --
 *      select one of the possible clocks ...
 */
static Bool
cirrusClockSelect(no)
     int no;
{
  static unsigned char save1, save2, save3;
  unsigned char temp;
  int SR,SR1;


#ifdef DEBUG_CIRRUS
  fprintf(stderr,"Clock NO = %d\n",no);
#endif

#if 0
  SR = 0x7E; SR1 = 0x33;	/* Just in case.... */
#endif

  switch(no)
       {
     case CLK_REG_SAVE:
       save1 = inb(0x3CC);
       outb(0x3C4, 0x0E);
       save2 = inb(0x3C5);
       outb(0x3C4, 0x1E);
       save3 = inb(0x3C5);
       break;
     case CLK_REG_RESTORE:
       outb(0x3C2, save1);
       outw(0x3C4, (save2 << 8) | 0x0E);
       outw(0x3C4, (save3 << 8) | 0x1E);
       break;
     default:
       if ( no >= NUM_CIRRUS_CLOCKS )
	    return(FALSE);
       if (!cirrusCheckClock(cirrusChip, no))
	    return(FALSE);

       SR = cirrusClockTab[no].numer;
       SR1 = cirrusClockTab[no].denom;

				/*  Use VCLK3 for these extended clocks */
       temp = inb(0x3CC);
       outb(0x3C2, temp | 0x0C );
  
#ifdef DEBUG_CIRRUS
       fprintf(stderr,"Misc = %x\n",temp);
       fprintf(stderr,"Miscactual = %x\n",(temp & 0xF3) | 0x0C);
#endif
  
				/* Set SRE and SR1E */
       outb(0x3C4,0x0E);
       temp = inb(0x3C5);
       outb(0x3C5,(temp & 0x80) | (SR & 0x7F));
  
#ifdef DEBUG_CIRRUS
       fprintf(stderr,"SR = %x\n",temp);
       fprintf(stderr,"SRactual = %x\n",(temp & 0x80) | (SR & 0x7F));
#endif

       outb(0x3C4,0x1E);
       temp = inb(0x3C5);
       outb(0x3C5,(temp & 0xC0) | (SR1 & 0x3F));
  
#ifdef DEBUG_CIRRUS
       fprintf(stderr,"SR1 = %x\n",temp);
       fprintf(stderr,"SR1actual = %x\n",(temp & 0xC0) | (SR1 & 0x3F));
#endif
       break;
       }
       return(TRUE);
}

/*
 * cirrusNumClocks --
 *	returns the number of clocks available for the chip
 */
static int
cirrusNumClocks(chip)
{
     cirrusClockRec *rec, *end = cirrusClockTab + NUM_CIRRUS_CLOCKS;

     /* 
      * The 62x5 chips can do marvelous things, but the
      * LCD panels connected to them don't leave much
      * option.  The following forces the cirrus chip to
      * use the slowest clock -- which appears to be what
      * my LCD panel likes best.  Faster clocks seem to
      * cause the LCD display to show noise when things are
      * moved around on the screen.
      */
     /* XXXX might be better/safer to reduce the value in clock limit tab */
     if (lcd_is_on) 
       {
	 return(1);
       }

#ifdef ALLOW_OUT_OF_SPEC_CLOCKS
     if (OFLG_ISSET(OPTION_16CLKS, &vga256InfoRec.options))
       {
	 return (NUM_CIRRUS_CLOCKS);
       }
#endif
     
     for (rec = cirrusClockTab; rec < end; rec++)
          if (CLOCKVAL(rec->numer, rec->denom) > cirrusClockLimit[chip])
               return(rec - cirrusClockTab);
     return(NUM_CIRRUS_CLOCKS);
}

/*
 * cirrusProbe -- 
 *      check up whether a cirrus based board is installed
 */
static Bool
cirrusProbe()
{  
     int cirrusClockNo, i;
     unsigned char lockreg,IdentVal;
     unsigned char id, rev;
     unsigned char temp;
     
     /*
      * Set up I/O ports to be used by this card
      */
     xf86ClearIOPortList(vga256InfoRec.scrnIndex);
     xf86AddIOPorts(vga256InfoRec.scrnIndex, Num_VGA_IOPorts, VGA_IOPorts);

     if (vga256InfoRec.chipset)
	  {
	  if (!StrCaseCmp(vga256InfoRec.chipset, "cirrus"))
	       {
               ErrorF("\ncirrus is no longer valid.  Use one of\n");
	       ErrorF("the names listed by the -showconfig option\n");
	       return(FALSE);
               }
	  cirrusChip = -1;
	  for (i = CLGD5420; i <= LASTCLGD; i++)
	       {
	       if (!StrCaseCmp(vga256InfoRec.chipset, cirrusIdent(i)))
	            {
	            cirrusChip = i;
	            }
	       }
	  if (cirrusChip >= 0)
	       {
	       cirrusEnterLeave(ENTER); /* Make the timing regs writable */
	       }
	  else
	       {
	       return(FALSE);
	       }
	  }
     else
	  {
	  unsigned char temp, origVal, newVal;
	  
	  cirrusEnterLeave(ENTER); /* Make the timing regs writable */
	  
	  /* Kited the following from the Cirrus */
	  /* Databook */
	  
	  /* If it's a Cirrus at all, we should be */
	  /* able to read back the lock register */
	  /* we wrote in cirrusEnterLeave() */
	  
	  outb(0x3C4,0x06);
	  lockreg = inb(0x3C5);
	  
	  /* Ok, if it's not 0x12, we're not a Cirrus542X or 62x5. */
	  if (lockreg != 0x12)
	       {
	       cirrusEnterLeave(LEAVE);
	       return(FALSE);
	       }
	  
	  /* OK, it's a Cirrus. Now, what kind of */
	  /* Cirrus? We read in the ident reg, */
	  /* CRTC index 27 */
	  
	  
	  outb(vgaIOBase+0x04, 0x27); IdentVal = inb(vgaIOBase+0x05);
	  
	  cirrusChip = -1;
	  id  = (IdentVal & 0xFc) >> 2;
	  rev = (IdentVal & 0x03);

	  switch( id )
	       {
	     case CLGD5420_ID:
	       cirrusChip = CLGD5420;
	       break;
	     case CLGD5422_ID:
	       cirrusChip = CLGD5422;
	       break;
	     case CLGD5424_ID:
	       cirrusChip = CLGD5424;
	       break;
	     case CLGD5426_ID:
	       cirrusChip = CLGD5426;
	       break;
	     case CLGD5428_ID:
	       cirrusChip = CLGD5428;
	       break;

	     /* 
	      * LCD driver chips...  the +1 options are because
	      * these chips have one more bit of chip rev level
	      */
	     case CLGD6205_ID:
	     case CLGD6205_ID + 1:
	       cirrusChip = CLGD6205;
	       break;
#if 0
	     /* looks like a 5420...  oh well...  close enough for now */
	     case CLGD6215_ID:
	     /* looks like a 5422...  oh well...  close enough for now */
	     case CLGD6215_ID + 1:
	       cirrusChip = CLGD6215;
	       break;
#endif
	     case CLGD6225_ID:
	     case CLGD6225_ID + 1:
	       cirrusChip = CLGD6225;
	       break;
	     case CLGD6235_ID:
	     case CLGD6235_ID + 1:
	       cirrusChip = CLGD6235;
	       break;

	     default:
	       ErrorF("Unknown Cirrus chipset: type 0x%02, rev %d\n", id, rev);
	       cirrusEnterLeave(LEAVE);
	       return(FALSE);
	       break;
	       }
	  }
     
     /* OK, we are a Cirrus */

     /* 
      * Try to determine special LCD-oriented stuff...
      *
      * [1] LCD only, CRT only, or simultaneous
      * [2] Panel type (if LCD is enabled)
      *
      * Currently, this isn't used for much, but I've put it
      * into this driver so that you'll at least have a clue
      * that the driver isn't the right one for your chipset
      * if it incorrectly identifies the panel configuration.
      * Later versions of this driver will probably do more
      * with this info than just print it out....
      */
     if (Is_62x5(cirrusChip)) 
	  {
	  /* Unlock the LCD registers... */
	  outb(vgaIOBase + 4, 0x1D);
	  temp = inb(vgaIOBase + 5);
	  outb(vgaIOBase + 5, (temp | 0x80));

	  /* LCD only, CRT only, or simultaneous? */
	  outb(vgaIOBase + 4, 0x20);
	  switch (inb(vgaIOBase + 5) & 0x60) 
	       {
	     case 0x60:
	        lcd_is_on = TRUE;
	        ErrorF("CIRRUS: Simultaneous LCD + VGA display\n");
	        break;
	     case 0x40:
	        ErrorF("CIRRUS: VGA display output only\n");
	        break;
	     case 0x20:
	        lcd_is_on = TRUE;
	        ErrorF("CIRRUS: LCD panel display only\n");
	        break;
	     default:
	        ErrorF("CIRRUS: Neither LCD panel nor VGA display!\n");
	        ErrorF("CIRRUS: Probably not a Cirrus CLGD62x5!\n");
	        ErrorF("CIRRUS: Use this driver at your own risk!\n");
	       }

          /* What type of LCD panel do we have? */
	  if (lcd_is_on) 
	       {
	       outb(vgaIOBase + 4, 0x1c);
	       switch (inb(vgaIOBase + 5) & 0xc0) 
		    {
		  case 0xc0:
		    ErrorF("CIRRUS: TFT active color LCD detected\n");
		    break;
		  case 0x80:
		    ErrorF("CIRRUS: STF passive color LCD detected\n");
		    break;
		  case 0x40:
		    ErrorF("CIRRUS: Grayscale plasma display detected\n");
		    break;
		  default:
		    ErrorF("CIRRUS: Monochrome LCD detected\n");
		    }
	       }

	  /* Lock the LCD registers... */
	  outb(vgaIOBase + 4, 0x1D);
	  temp = inb(vgaIOBase + 5);
	  outb(vgaIOBase + 5, (temp & 0x7f));
          }


     if (!vga256InfoRec.videoRam) 
	  {
	  if (Is_62x5(cirrusChip)) 
	       {
	       /* 
		* According to Ed Strauss at Cirrus, the 62x5 has 512k.
		* That's it.  Period.
		*/
	       vga256InfoRec.videoRam = 512;
	       }
	  else 
	       {
	       unsigned char memreg;

				/* Thanks to Brad Hackenson at Cirrus for */
				/* this bit of undocumented black art....*/
	       outb(0x3C4,0x0A);
	       memreg = inb(0x3C5);
	  
	       switch( (memreg & 0x18) >> 3 )
		    {
		  case 0:
		    vga256InfoRec.videoRam = 256;
		    break;
		  case 1:
		    vga256InfoRec.videoRam = 512;
		    break;
		  case 2:
		    vga256InfoRec.videoRam = 1024;
		    break;
		  case 3:
		    vga256InfoRec.videoRam = 2048;
		    break;
		    }
	       }
	  }
     /* 
      * Banking granularity is 16k for the 5426 or 5428
      * when allowing access to 2MB, and 4k otherwise 
      */
     if (vga256InfoRec.videoRam > 1024)
          {
          CIRRUS.ChipSetRead = cirrusSetRead2MB;
          CIRRUS.ChipSetWrite = cirrusSetWrite2MB;
          CIRRUS.ChipSetReadWrite = cirrusSetReadWrite2MB;
          }


     cirrusClockNo = cirrusNumClocks(cirrusChip);
     if (!vga256InfoRec.clocks)
          if (OFLG_ISSET(OPTION_PROBE_CLKS, &vga256InfoRec.options))
	       vgaGetClocks(cirrusClockNo, cirrusClockSelect);
	  else
	       {
	       vga256InfoRec.clocks = cirrusClockNo;
	       for (i = 0; i < cirrusClockNo; i++)
		   vga256InfoRec.clock[i] =
		     CLOCKVAL(cirrusClockTab[i].numer, cirrusClockTab[i].denom);
	       }
     else
          if (vga256InfoRec.clocks > cirrusClockNo)
	       {
	       ErrorF("Too many Clocks specified in Xconfig.\n");
	       ErrorF("At most %d clocks may be specified\n",
		      cirrusClockNo);
	       }
	  
     vga256InfoRec.chipset = cirrusIdent(cirrusChip);
     vga256InfoRec.bankedMono = FALSE;
#ifdef ALLOW_OUT_OF_SPEC_CLOCKS
     vga256InfoRec.maxClock = MAX_OUT_OF_SPEC_CLOCK;
#else
     vga256InfoRec.maxClock = cirrusClockLimit[cirrusChip];
#endif
     /* Initialize option flags allowed for this driver */
#ifdef ALLOW_OUT_OF_SPEC_CLOCKS
     OFLG_SET(OPTION_16CLKS, &CIRRUS.ChipOptionFlags);
     ErrorF("CIRRUS: Warning: Out of spec clocks can be enabled\n");
#endif
     OFLG_SET(OPTION_NOACCEL, &CIRRUS.ChipOptionFlags);
#ifdef notyet
     OFLG_SET(OPTION_SLOW_DRAM, &CIRRUS.ChipOptionFlags);
#endif
     OFLG_SET(OPTION_PROBE_CLKS, &CIRRUS.ChipOptionFlags);
     return(TRUE);
}

/*
 * cirrusFbInit --
 *      enable speedups for the chips that support it
 */
static void
cirrusFbInit()
{
  int useSpeedUp;

  useSpeedUp = vga256InfoRec.speedup & SPEEDUP_ANYWIDTH;
  
  if (((cirrusChip == CLGD5426) || (cirrusChip == CLGD5428)) &&
      !OFLG_ISSET(OPTION_NOACCEL, &vga256InfoRec.options))
    {
      if (useSpeedUp && x386Verbose)
	{
	  ErrorF ("%s %s: Using accelerator functions\n",
		  XCONFIG_PROBED, cirrusIdent (cirrusChip) );
	}

      cfbLowlevFuncs.fillBoxSolid = CirrusFillBoxSolid;
      cfbLowlevFuncs.fillRectSolidCopy = CirrusFillRectSolidCopy;
      cfbLowlevFuncs.doBitbltCopy = CirrusDoBitbltCopy;
    }

  CirrusMemTop = vga256InfoRec.virtualX * vga256InfoRec.virtualY;
}  

/*
 * cirrusEnterLeave -- 
 *      enable/disable io-mapping
 */
static void 
cirrusEnterLeave(enter)
     Bool enter;
{
  static unsigned char temp;

  if (enter)
       {

       xf86EnableIOPorts(vga256InfoRec.scrnIndex);

				/* Are we Mono or Color? */
       vgaIOBase = (inb(0x3CC) & 0x01) ? 0x3D0 : 0x3B0;

       outb(0x3C4,0x06);
       outb(0x3C5,0x12);	 /* unlock cirrus special */

				/* Put the Vert. Retrace End Reg in temp */

       outb(vgaIOBase + 4, 0x11); temp = inb(vgaIOBase + 5);

				/* Put it back with PR bit set to 0 */
				/* This unprotects the 0-7 CRTC regs so */
				/* they can be modified, i.e. we can set */
				/* the timing. */

       outb(vgaIOBase + 5, temp & 0x7F);

    }
  else
       {

       outb(0x3C4,0x06);
       outb(0x3C5,0x0F);	 /*relock cirrus special */

       xf86DisableIOPorts(vga256InfoRec.scrnIndex);
    }
}

/*
 * cirrusRestore -- 
 *      restore a video mode
 */
static void 
cirrusRestore(restore)
  vgacirrusPtr restore;
{
  unsigned char i;


  outw(0x3CE, 0x0009);	/* select bank 0 */
  outw(0x3CE, 0x000A);

  vgaHWRestore(restore);

/*  unsigned char GR9;		 Graphics Offset1 */
/*  unsigned char GRA;		 Graphics Offset2 */
/*  unsigned char GRB;		 Graphics Extensions Control */
/*  unsigned char SR7;		 Extended Sequencer */
/*  unsigned char SRE;		 VCLK Numerator */
/*  unsigned char SRF;		 DRAM Control */
/*  unsigned char SR1E;		 VCLK Denominator */
/*  unsigned char CR19;		 Interlace End */
/*  unsigned char CR1A;		 Miscellaneous Control */
/*  unsigned char CR1B;		 Extended Display Control */

  outw(0x3C4, 0x0100);				/* disable timing sequencer */

  outb(0x3CE,0x09);
  outb(0x3CF,restore->GR9);

  outb(0x3CE,0x0A);
  outb(0x3CF,restore->GRA);

  outb(0x3CE,0x0B);
  outb(0x3CF,restore->GRB);

  outb(0x3C4,0x07);
  outb(0x3C5,restore->SR7);

  if (restore->std.NoClock >= 0)
       {
       outb(0x3C4,0x0E);
       outb(0x3C5,restore->SRE);
       }

  outb(0x3C4,0x0F);
  outb(0x3C5,restore->SRF);

  if (restore->std.NoClock >= 0)
       {
       outb(0x3C4,0x1E);
       outb(0x3C5,restore->SR1E);
       }

  outb(vgaIOBase + 4,0x19);
  outb(vgaIOBase + 5,restore->CR19);

  outb(vgaIOBase + 4,0x1A);
  outb(vgaIOBase + 5,restore->CR1A);

  outb(vgaIOBase + 4, 0x1B);
  outb(vgaIOBase + 5,restore->CR1B);

  if (cirrusChip == CLGD6225) 
       {
       /* Unlock the LCD registers... */
       outb(vgaIOBase + 4, 0x1D);
       i = inb(vgaIOBase + 5);
       outb(vgaIOBase + 5, (i | 0x80));

       /* Restore LCD HSYNC value */
       outb(vgaIOBase + 4, 0x0A);
       outb(vgaIOBase + 5, restore->RAX);
#if 0
       fprintf(stderr, "RAX restored to %d\n", restore->RAX);
#endif

       /* Lock the LCD registers... */
       outb(vgaIOBase + 4, 0x1D);
       i = inb(vgaIOBase + 5);
       outb(vgaIOBase + 5, (i & 0x7f));
       }
}

/*
 * cirrusSave -- 
 *      save the current video mode
 */
static void *
cirrusSave(save)
     vgacirrusPtr save;
{
  unsigned char             temp1, temp2;

  
  vgaIOBase = (inb(0x3CC) & 0x01) ? 0x3D0 : 0x3B0;

  outb(0x3CE, 0x09);
  temp1 = inb(0x3CF);
  outb(0x3CF, 0x00);	/* select bank 0 */
  outb(0x3CE, 0x0A);
  temp2 = inb(0x3CF);
  outb(0x3CF, 0x00);	/* select bank 0 */

  save = (vgacirrusPtr)vgaHWSave(save, sizeof(vgacirrusRec));


/*  unsigned char GR9;		 Graphics Offset1 */
/*  unsigned char GRA;		 Graphics Offset2 */
/*  unsigned char GRB;		 Graphics Extensions Control */
/*  unsigned char SR7;		 Extended Sequencer */
/*  unsigned char SRE;		 VCLK Numerator */
/*  unsigned char SRF;		 DRAM Control */
/*  unsigned char SR1E;		 VCLK Denominator */
/*  unsigned char CR19;		 Interlace End */
/*  unsigned char CR1A;		 Miscellaneous Control */
/*  unsigned char CR1B;		 Extended Display Control */

  save->GR9 = temp1;

  save->GRA = temp2;

  outb(0x3CE,0x0B);		
  save->GRB = inb(0x3CF); 
				
  outb(0x3C4,0x07);
  save->SR7 = inb(0x3C5);

  outb(0x3C4,0x0E);
  save->SRE = inb(0x3C5);

  outb(0x3C4,0x0F);
  save->SRF = inb(0x3C5);

  outb(0x3C4,0x1E);
  save->SR1E = inb(0x3C5);

  outb(vgaIOBase + 4,0x19);
  save->CR19 = inb(vgaIOBase + 5);

  outb(vgaIOBase + 4,0x1A);
  save->CR1A = inb(vgaIOBase + 5);

  outb(vgaIOBase + 4, 0x1B);
  save->CR1B = inb(vgaIOBase + 5);

  if (cirrusChip == CLGD6225) 
       {
       /* Unlock the LCD registers... */
       outb(vgaIOBase + 4, 0x1D);
       temp1 = inb(vgaIOBase + 5);
       outb(vgaIOBase + 5, (temp1 | 0x80));

       /* Save current LCD HSYNC value */
       outb(vgaIOBase + 4, 0x0A);
       save->RAX = inb(vgaIOBase + 5);
#if 0
       fprintf(stderr, "RAX saved as %d\n", save->RAX);
#endif

       /* Lock the LCD registers... */
       outb(vgaIOBase + 4, 0x1D);
       temp1 = inb(vgaIOBase + 5);
       outb(vgaIOBase + 5, (temp1 & 0x7f));
       }

  return ((void *) save);
}

/*
 * cirrusInit -- 
 *      Handle the initialization, etc. of a screen.
 */
static Bool
cirrusInit(mode)
     DisplayModePtr mode;
{
     unsigned char temp1;
     
  if (!vgaHWInit(mode,sizeof(vgacirrusRec)))
    return(FALSE);

/*  unsigned char GR9;		 Graphics Offset1 */
/*  unsigned char GRA;		 Graphics Offset2 */
/*  unsigned char GRB;		 Graphics Extensions Control */
/*  unsigned char SR7;		 Extended Sequencer */
/*  unsigned char SRE;		 VCLK Numerator */
/*  unsigned char SRF;		 DRAM Control */
/*  unsigned char SR1E;		 VCLK Denominator */
/*  unsigned char CR19;		 Interlace End */
/*  unsigned char CR1A;		 Miscellaneous Control */
/*  unsigned char CR1B;		 Extended Display Control */


				/* Set the clock regs */

     if (new->std.NoClock >= 0)
          {
          unsigned char tempreg;
          int SR,SR1;
          
          if (new->std.NoClock >= NUM_CIRRUS_CLOCKS)
               {
               ErrorF("Invalid clock index -- too many clocks in Xconfig\n");
               return(FALSE);
               }
				/* Always use VLCK3 */

          new->std.MiscOutReg |= 0x0C;

#if 0
          SR = 0x7E; SR1 = 0x33;	/* Just in case.... */
#endif

	  if (!cirrusCheckClock(cirrusChip, new->std.NoClock))
	       return (FALSE);

          SR = cirrusClockTab[new->std.NoClock].numer;
          SR1 = cirrusClockTab[new->std.NoClock].denom;

				/* Be nice to the reserved bits... */
          outb(0x3C4,0x0E);
          tempreg = inb(0x3C5);
          new->SRE = (tempreg & 0x80) | (SR & 0x7F);

          outb(0x3C4,0x1E);
          tempreg = inb(0x3C5);
          new->SR1E = (tempreg & 0xC0) | (SR1 & 0x3F);
          }
     
     
     new->std.CRTC[0x13] = vga256InfoRec.virtualX >> 3;
     
				/* Enable Dual Banking */
     new->GRB = 0x01;

				/* Setting the 5 bit to either 1 or 0 works. */
				/* I don't understand the difference. The */
				/* book says this bit is the "CRT FIFO */
				/* Depth". */
     outb(0x3C4,0x0F);
     new->SRF = inb(0x3C5);

     if (CIRRUS.ChipSetRead != cirrusSetRead)
	  {
	  new->GRB |= 0x20;	/* Set 16k bank granularity */
	  new->SRF |= 0x80;	/* Enable the second MB */
	  }

     new->SR7 = 0x01;		/* Tell it to use 256 Colors */

#if 0
				/* Try Linear Addressing. */
     new->SR7 |= ( ((int) vgaBase) >> 16) & 0x0000F0;
     fprintf(stderr,"vgaBase = %x\n",vgaBase);
/* Doesn't work on systems w/ more than 16M memory. T.S. */
#endif
     

				/* Fill up all the overflows - ugh! */
#ifdef DEBUG_CIRRUS
     fprintf(stderr,"Init: VSyncStart + 1 = %x\n\
HsyncEnd>>3 = %x\n\
HDisplay>>3 -1 = %x\n\
VirtX = %x\n",
	     mode->VSyncStart + 1,
	     mode->HSyncEnd >> 3, 
	     (mode->HDisplay >> 3) - 1,
	     vga256InfoRec.virtualX>>4);
#endif
     
     new->CR1A = (((mode->VSyncStart + 1) & 0x300 ) >> 2)
	  | (((mode->HSyncEnd >> 3) & 0xC0) >> 2);

     if (mode->Flags & V_INTERLACE) 
	    {
				/* ``Half the Horizontal Total'' which is */
				/* really half the value in CR0 */

	    new->CR19 = ((mode->HTotal >> 3) - 5) >> 1;
	    new->CR1A |= 0x01;
	    }
     else new->CR19 = 0x00;
     
     new->CR1B = (((vga256InfoRec.virtualX>>3) & 0x100) >> 4)
	  | 0x22;

     if (cirrusChip == CLGD6225) 
	  {
	  /* Don't ask me why the following number works, but it
	   * does work for a Sager 8200 using the BIOS initialization
	   * of the LCD for all other functions.  Without this, the
	   * Sager's display is 8 pixels left and 1 down from where
	   * it should be....  If things are shifted on your display,
	   * the documentation says to +1 for each 8 columns you want
	   * to move left...  but it seems to work in the opposite
	   * direction on my screen.  Anyway, this works for me, and
	   * it is easy to play with if it doesn't work for you.
	   */
	  new->RAX = 12;
          }

  return(TRUE);
}

/*
 * cirrusAdjust --
 *      adjust the current video frame to display the mousecursor
 */
static void 
cirrusAdjust(x, y)
     int x, y;
{
     unsigned char CR1B;
     int Base = (y * vga256InfoRec.virtualX + x + 1) >> 2;
     outw(vgaIOBase + 4, (Base & 0x00FF00) | 0x0C);
     outw(vgaIOBase + 4, ((Base & 0x00FF) << 8) | 0x0D);

     outb(vgaIOBase + 4,0x1B); CR1B = inb(vgaIOBase + 5);
     outb(vgaIOBase + 5,(CR1B & 0xF2) | ((Base & 0x060000) >> 15)
	  | ((Base & 0x010000) >> 16) );
}
