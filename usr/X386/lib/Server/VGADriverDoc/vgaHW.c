/*
 * $XFree86: mit/server/ddx/x386/vga256/vga/vgaHW.c,v 2.14 1993/10/12 15:52:40 dawes Exp $
 * $XConsortium: vgaHW.c,v 1.3 91/08/26 15:40:56 gildea Exp $
 *
 * Copyright 1990,91 by Thomas Roell, Dinkelscherben, Germany.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Thomas Roell not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Thomas Roell makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * THOMAS ROELL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THOMAS ROELL BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Thomas Roell, roell@informatik.tu-muenchen.de
 */

#if !defined(AMOEBA) && !defined(_MINIX)
#define _NEED_SYSI86
#endif

#ifndef MONOVGA
#ifndef SCO
#ifndef SAVE_FONT1
#define SAVE_FONT1
#endif
#endif
#endif

#if defined(__386BSD__) || defined(MACH386) || defined(linux) || defined(AMOEBA) || defined(_MINIX)
#ifndef NEED_SAVED_CMAP
#define NEED_SAVED_CMAP
#endif
#ifndef MONOVGA
#ifndef SAVE_TEXT
#define SAVE_TEXT
#endif
#endif
#ifndef SAVE_FONT2
#define SAVE_FONT2
#endif
#endif

/* bytes per plane to save for text */
#if defined(linux) || defined(_MINIX)
#define TEXT_AMOUNT 16384
#else
#define TEXT_AMOUNT 4096
#endif

/* bytes per plane to save for font data */
#define FONT_AMOUNT 8192

#if defined(__386BSD__) || defined(MACH386)
#include <sys/time.h>
#endif

#ifdef MACH386
#define WEXITSTATUS(x) (x.w_retcode)
#define WTERMSIG(x) (x.w_termsig)
#define WSTOPSIG(x) (x.w_stopsig)
#endif

#ifdef ISC202
#include <sys/types.h>
#define WIFEXITED(a)  ((a & 0x00ff) == 0)  /* LSB will be 0 */
#define WEXITSTATUS(a) ((a & 0xff00) >> 8)
#define WIFSIGNALED(a) ((a & 0xff00) == 0) /* MSB will be 0 */
#define WTERMSIG(a) (a & 0x00ff)
#else
#if defined(ISC) && !defined(_POSIX_SOURCE)
#define _POSIX_SOURCE
#include <sys/types.h>
#include <sys/wait.h>
#undef _POSIX_SOURCE
#else
#if defined(_MINIX) || defined(AMOEBA)
#include <sys/types.h>
#endif
#include <sys/wait.h>
#endif
#endif

#include "X.h"
#include "input.h"
#include "scrnintstr.h"

#include "compiler.h"

#include "x386.h"
#include "x386Priv.h"
#include "xf86_OSlib.h"
#include "vga.h"

#ifdef MONOVGA
/* DAC indices for white and black */
#define WHITE_VALUE 0x3F
#define BLACK_VALUE 0x00
#define OVERSCAN_VALUE 0x01
#endif

static int currentGraphicsClock = -1;
static int currentExternClock = -1;

#define new ((vgaHWPtr)vgaNewVideoState)

unsigned VGA_IOPorts[] = {
	0x3B4, 0x3B5, 0x3BA, 0x3C0, 0x3C1, 0x3C2, 0x3C4, 0x3C5, 0x3C6, 0x3C7, 
	0x3C8, 0x3C9, 0x3CA, 0x3CB, 0x3CC, 0x3CE, 0x3CF, 0x3D4, 0x3D5, 0x3DA,
};
int Num_VGA_IOPorts = (sizeof(VGA_IOPorts)/sizeof(VGA_IOPorts[0]));

#ifdef NEED_SAVED_CMAP
/* This default colourmap is used only when it can't be read from the VGA */

unsigned char defaultDAC[768] =
{
     0,  0,  0,    0,  0, 42,    0, 42,  0,    0, 42, 42,
    42,  0,  0,   42,  0, 42,   42, 21,  0,   42, 42, 42,
    21, 21, 21,   21, 21, 63,   21, 63, 21,   21, 63, 63,
    63, 21, 21,   63, 21, 63,   63, 63, 21,   63, 63, 63,
     0,  0,  0,    5,  5,  5,    8,  8,  8,   11, 11, 11,
    14, 14, 14,   17, 17, 17,   20, 20, 20,   24, 24, 24,
    28, 28, 28,   32, 32, 32,   36, 36, 36,   40, 40, 40,
    45, 45, 45,   50, 50, 50,   56, 56, 56,   63, 63, 63,
     0,  0, 63,   16,  0, 63,   31,  0, 63,   47,  0, 63,
    63,  0, 63,   63,  0, 47,   63,  0, 31,   63,  0, 16,
    63,  0,  0,   63, 16,  0,   63, 31,  0,   63, 47,  0,
    63, 63,  0,   47, 63,  0,   31, 63,  0,   16, 63,  0,
     0, 63,  0,    0, 63, 16,    0, 63, 31,    0, 63, 47,
     0, 63, 63,    0, 47, 63,    0, 31, 63,    0, 16, 63,
    31, 31, 63,   39, 31, 63,   47, 31, 63,   55, 31, 63,
    63, 31, 63,   63, 31, 55,   63, 31, 47,   63, 31, 39,
    63, 31, 31,   63, 39, 31,   63, 47, 31,   63, 55, 31,
    63, 63, 31,   55, 63, 31,   47, 63, 31,   39, 63, 31,
    31, 63, 31,   31, 63, 39,   31, 63, 47,   31, 63, 55,
    31, 63, 63,   31, 55, 63,   31, 47, 63,   31, 39, 63,
    45, 45, 63,   49, 45, 63,   54, 45, 63,   58, 45, 63,
    63, 45, 63,   63, 45, 58,   63, 45, 54,   63, 45, 49,
    63, 45, 45,   63, 49, 45,   63, 54, 45,   63, 58, 45,
    63, 63, 45,   58, 63, 45,   54, 63, 45,   49, 63, 45,
    45, 63, 45,   45, 63, 49,   45, 63, 54,   45, 63, 58,
    45, 63, 63,   45, 58, 63,   45, 54, 63,   45, 49, 63,
     0,  0, 28,    7,  0, 28,   14,  0, 28,   21,  0, 28,
    28,  0, 28,   28,  0, 21,   28,  0, 14,   28,  0,  7,
    28,  0,  0,   28,  7,  0,   28, 14,  0,   28, 21,  0,
    28, 28,  0,   21, 28,  0,   14, 28,  0,    7, 28,  0,
     0, 28,  0,    0, 28,  7,    0, 28, 14,    0, 28, 21,
     0, 28, 28,    0, 21, 28,    0, 14, 28,    0,  7, 28,
    14, 14, 28,   17, 14, 28,   21, 14, 28,   24, 14, 28,
    28, 14, 28,   28, 14, 24,   28, 14, 21,   28, 14, 17,
    28, 14, 14,   28, 17, 14,   28, 21, 14,   28, 24, 14,
    28, 28, 14,   24, 28, 14,   21, 28, 14,   17, 28, 14,
    14, 28, 14,   14, 28, 17,   14, 28, 21,   14, 28, 24,
    14, 28, 28,   14, 24, 28,   14, 21, 28,   14, 17, 28,
    20, 20, 28,   22, 20, 28,   24, 20, 28,   26, 20, 28,
    28, 20, 28,   28, 20, 26,   28, 20, 24,   28, 20, 22,
    28, 20, 20,   28, 22, 20,   28, 24, 20,   28, 26, 20,
    28, 28, 20,   26, 28, 20,   24, 28, 20,   22, 28, 20,
    20, 28, 20,   20, 28, 22,   20, 28, 24,   20, 28, 26,
    20, 28, 28,   20, 26, 28,   20, 24, 28,   20, 22, 28,
     0,  0, 16,    4,  0, 16,    8,  0, 16,   12,  0, 16,
    16,  0, 16,   16,  0, 12,   16,  0,  8,   16,  0,  4,
    16,  0,  0,   16,  4,  0,   16,  8,  0,   16, 12,  0,
    16, 16,  0,   12, 16,  0,    8, 16,  0,    4, 16,  0,
     0, 16,  0,    0, 16,  4,    0, 16,  8,    0, 16, 12,
     0, 16, 16,    0, 12, 16,    0,  8, 16,    0,  4, 16,
     8,  8, 16,   10,  8, 16,   12,  8, 16,   14,  8, 16,
    16,  8, 16,   16,  8, 14,   16,  8, 12,   16,  8, 10,
    16,  8,  8,   16, 10,  8,   16, 12,  8,   16, 14,  8,
    16, 16,  8,   14, 16,  8,   12, 16,  8,   10, 16,  8,
     8, 16,  8,    8, 16, 10,    8, 16, 12,    8, 16, 14,
     8, 16, 16,    8, 14, 16,    8, 12, 16,    8, 10, 16,
    11, 11, 16,   12, 11, 16,   13, 11, 16,   15, 11, 16,
    16, 11, 16,   16, 11, 15,   16, 11, 13,   16, 11, 12,
    16, 11, 11,   16, 12, 11,   16, 13, 11,   16, 15, 11,
    16, 16, 11,   15, 16, 11,   13, 16, 11,   12, 16, 11,
    11, 16, 11,   11, 16, 12,   11, 16, 13,   11, 16, 15,
    11, 16, 16,   11, 15, 16,   11, 13, 16,   11, 12, 16,
     0,  0,  0,    0,  0,  0,    0,  0,  0,    0,  0,  0,
     0,  0,  0,    0,  0,  0,    0,  0,  0,    0,  0,  0,
};
#endif /* NEED_SAVED_CMAP */

/*
 * slowbcopy --
 *	slow version of bcopy for save/restore of font and text data.
 */
static void
slowbcopy(src, dst, count)
     char *src, *dst;
     int count;
{
  int i;
  unsigned char temp;

  for (i = 0; i < count; i++)
  {
    *dst++ = *src++;
    temp = inb(vgaIOBase + 0x0A);
  }
}

/*
 * vgaProtect --
 *	Protect VGA registers and memory from corruption during loads.
 */
void
vgaProtect(on)
     Bool on;
{
  unsigned char tmp;

  if (x386VTSema) {
    if (on) {
      /*
       * Turn off screen and disable sequencer.
       */
      outb(0x3C4, 0x01);
      tmp = inb(0x3C5);

      (*vgaSaveScreenFunc)(SS_START);
      outw(0x3C4, 0x0100);			/* start synchronous reset */
      outw(0x3C4, ((tmp | 0x20) << 8) | 0x01);	/* disable the display */

      tmp = inb(vgaIOBase + 0x0A);
      outb(0x3C0, 0x00);			/* enable pallete access */
    }
    else {
      /*
       * Reenable sequencer, then turn on screen.
       */
      outb(0x3C4, 0x01);
      tmp = inb(0x3C5);

      outw(0x3C4, ((tmp & 0xDF) << 8) | 0x01);	/* reenable display */
      outw(0x3C4, 0x0300);			/* clear synchronousreset */
      (*vgaSaveScreenFunc)(SS_FINISH);

      tmp = inb(vgaIOBase + 0x0A);
      outb(0x3C0, 0x20);			/* disable pallete access */
    }
  }
  return;
}

/*
 * vgaSaveScreen -- 
 *      Disable the video on the frame buffer to save the screen.
 */
Bool
vgaSaveScreen (pScreen, on)
     ScreenPtr     pScreen;
     Bool          on;
{
  unsigned char   state;

  if (on)
    SetTimeSinceLastInputEvent();
  if (x386VTSema) {
    outb(0x3C4,1);
    state = inb(0x3C5);
  
    if (on) state &= 0xDF;
    else    state |= 0x20;
    
    /*
     * turn off srceen if necessary
     */
    (*vgaSaveScreenFunc)(SS_START);
    outw(0x3C4, 0x0100);              /* syncronous reset */
    outw(0x3C4, (state << 8) | 0x01); /* change mode */
    outw(0x3C4, 0x0300);              /* syncronous reset */
    (*vgaSaveScreenFunc)(SS_FINISH);

  } else {
    if (on)
      ((vgaHWPtr)vgaNewVideoState)->Sequencer[1] &= 0xDF;
    else
      ((vgaHWPtr)vgaNewVideoState)->Sequencer[1] |= 0x20;
  }

  return(TRUE);
}


/*
 * setExternClock
 *      call the external clock program
 *
 */

static Bool
setExternClock(clock)
     int clock;       /* the Clock index */
{
    int i;
#ifdef MACH386
    union wait exit_status;
#else
    int exit_status;
#endif

    if (clock == currentExternClock)
      return(TRUE);
    switch(fork())
    {
      case -1:
        ErrorF("Fork failed for ClockProg (%s)\n", strerror(errno));
        return(FALSE);
      case 0:  /* child */
	/* 
	 * Make sure that the child doesn't inherit any I/O permissions it
	 * shouldn't have.  It's better to put constraints on the development
	 * of a clock program than to give I/O permissions to a bogus program
	 * in someone's Xconfig file
	 */
	for (i = 0; i < MAXSCREENS; i++)
	  xf86DisableIOPorts(i);
        setuid(getuid());
#if !defined(AMOEBA) && !defined(_MINIX)
        /* set stdin, stdout to the consoleFD, and leave stderr alone */
        for (i = 0; i < 2; i++)
        {
          if (x386Info.consoleFd != i)
          {
            close(i);
            dup(x386Info.consoleFd);
          }
        }
#endif
        {
          char *progname, clockarg[8], clockindex[3];

          if (progname = rindex(vga256InfoRec.clockprog, '/'))
            progname++;
          else
            progname = vga256InfoRec.clockprog;
          sprintf(clockarg, "%.3f", vga256InfoRec.clock[clock] / 1000.0);
          sprintf(clockindex, "%d", clock);
          execl(vga256InfoRec.clockprog, progname, clockarg, clockindex, NULL);
          ErrorF("Exec failed for ClockProg command \"%s\" (%s)\n",
                 vga256InfoRec.clockprog, strerror(errno));
          exit(255);
        }
        break;
      default:  /* parent */
        wait(&exit_status);
        if (WIFEXITED(exit_status))
        {
          switch (WEXITSTATUS(exit_status))
          {
            case 0:     /* OK */
              break;
            case 255:   /* exec() failed */
              return(FALSE);
            default:    /* bad exit status */
              ErrorF("ClockProg \"%s\" had bad exit status %d\n",
                     vga256InfoRec.clockprog, WEXITSTATUS(exit_status));
              return(FALSE);
          }
        }
        else if (WIFSIGNALED(exit_status))
        {
          ErrorF("ClockProg \"%s\" died on signal %d\n",
                 vga256InfoRec.clockprog, WTERMSIG(exit_status));
          return(FALSE);
        }
#ifdef WIFSTOPPED
        else if (WIFSTOPPED(exit_status))
        {
          ErrorF("ClockProg \"%s\" stopped by signal %d\n",
                 vga256InfoRec.clockprog, WSTOPSIG(exit_status));
          return(FALSE);
        }
#endif
        else /* should never get to this point */
        {
          ErrorF("ClockProg \"%s\" has unknown exit condition\n",
                 vga256InfoRec.clockprog);
          return(FALSE);
        }
    }
    currentExternClock = clock;
    return(TRUE);
}


/*
 * vgaHWRestore --
 *      restore a video mode
 */

void
vgaHWRestore(restore)
     vgaHWPtr restore;
{
  int i,tmp;

  tmp = inb(vgaIOBase + 0x0A);		/* Reset flip-flop */
  outb(0x3C0, 0x00);			/* Enables pallete access */

  /*
   * This here is a workaround a bug in the kd-driver. We MUST explicitely
   * restore the font we got, when we entered graphics mode.
   * The bug was seen on ESIX, and ISC 2.0.2 when using a monochrome
   * monitor. 
   *
   * BTW, also GIO_FONT seems to have a bug, so we cannot use it, to get
   * a font.
   */
  
  vgaSaveScreen(NULL, FALSE);
  if(restore->FontInfo1 || restore->FontInfo2 || restore->TextInfo) {
    /*
     * here we switch temporary to 16 color-plane-mode, to simply
     * copy the font-info and saved text
     *
     * BUGALLERT: The vga's segment-select register MUST be set appropriate !
     */
    tmp = inb(vgaIOBase + 0x0A); /* reset flip-flop */
    outb(0x3C0,0x30); outb(0x3C0, 0x01); /* graphics mode */
#ifdef XF86VGA16
    outw(0x3CE,0x0003); /* GJA - don't rotate, write unmodified */
    outw(0x3CE,0xFF08); /* GJA - write all bits in a byte */
    outw(0x3CE,0x0001); /* GJA - all planes come from CPU */
#endif
    if (restore->FontInfo1) {
      outw(0x3C4, 0x0402);    /* write to plane 2 */
      outw(0x3C4, 0x0604);    /* enable plane graphics */
      outw(0x3CE, 0x0204);    /* read plane 2 */
      outw(0x3CE, 0x0005);    /* write mode 0, read mode 0 */
      outw(0x3CE, 0x0506);    /* set graphics */
      slowbcopy(restore->FontInfo1, vgaBase, FONT_AMOUNT);
    }
    if (restore->FontInfo2) {
      outw(0x3C4, 0x0802);    /* write to plane 3 */
      outw(0x3C4, 0x0604);    /* enable plane graphics */
      outw(0x3CE, 0x0304);    /* read plane 3 */
      outw(0x3CE, 0x0005);    /* write mode 0, read mode 0 */
      outw(0x3CE, 0x0506);    /* set graphics */
      slowbcopy(restore->FontInfo2, vgaBase, FONT_AMOUNT);
    }
    if (restore->TextInfo) {
      outw(0x3C4, 0x0102);    /* write to plane 0 */
      outw(0x3C4, 0x0604);    /* enable plane graphics */
      outw(0x3CE, 0x0004);    /* read plane 0 */
      outw(0x3CE, 0x0005);    /* write mode 0, read mode 0 */
      outw(0x3CE, 0x0506);    /* set graphics */
      slowbcopy(restore->TextInfo, vgaBase, TEXT_AMOUNT);
      outw(0x3C4, 0x0202);    /* write to plane 1 */
      outw(0x3C4, 0x0604);    /* enable plane graphics */
      outw(0x3CE, 0x0104);    /* read plane 1 */
      outw(0x3CE, 0x0005);    /* write mode 0, read mode 0 */
      outw(0x3CE, 0x0506);    /* set graphics */
      slowbcopy(restore->TextInfo + TEXT_AMOUNT, vgaBase, TEXT_AMOUNT);
    }
  }
  vgaSaveScreen(NULL, TRUE);

  tmp = inb(vgaIOBase + 0x0A);			/* Reset flip-flop */
  outb(0x3C0, 0x00);				/* Enables pallete access */

  if (vgaIOBase == 0x3B0)
    restore->MiscOutReg &= 0xFE;
  else
    restore->MiscOutReg |= 0x01;

  /* Don't change the clock bits when using an external clock program */
  if (restore->NoClock < 0)
  {
    tmp = inb(0x3CC);
    restore->MiscOutReg = (restore->MiscOutReg & 0xF3) | (tmp & 0x0C);
  }
  outb(0x3C2, restore->MiscOutReg);

  for (i=1; i<5;  i++) outw(0x3C4, (restore->Sequencer[i] << 8) | i);
  
  /* Ensure CRTC registers 0-7 are unlocked by clearing bit 7 or CRTC[17] */

  outw(vgaIOBase + 4, ((restore->CRTC[17] & 0x7F) << 8) | 17);

  for (i=0; i<25; i++) outw(vgaIOBase + 4,(restore->CRTC[i] << 8) | i);

  for (i=0; i<9;  i++) outw(0x3CE, (restore->Graphics[i] << 8) | i);

  for (i=0; i<21; i++) {
    tmp = inb(vgaIOBase + 0x0A);
    outb(0x3C0,i); outb(0x3C0, restore->Attribute[i]);
  }
  
  outb(0x3C6,0xFF);
  outb(0x3C8,0x00);
  for (i=0; i<768; i++)
  {
     outb(0x3C9, restore->DAC[i]);
     DACDelay;
  }

  /* Turn on PAS bit */
  tmp = inb(vgaIOBase + 0x0A);
  outb(0x3C0, 0x20);

  /* If restoring text mode, and a text clock is specified with clkprog */
  if (((restore->Attribute[0x10] & 0x01) == 0))
  {
    if (vga256InfoRec.clockprog)
    {
      if (vga256InfoRec.textclock >= 0)
        setExternClock(vga256InfoRec.textclock);
      /*
       * Invalidate the saved extern clock when switching to text mode
       * because I think some systems modify this when VT switching.
       */
      currentExternClock = -1;
    }
  }
  else
  /* If restoring a graphics mode */
  {
    if (currentGraphicsClock >= 0)
      setExternClock(currentGraphicsClock);
  }
}

/*
 * vgaHWSave --
 *      save the current video mode
 */
static int saveflag=0;

void *
vgaHWSave(save, size)
     vgaHWPtr save;
     int          size;
{
  int           i,tmp;
  Bool	        first_time = FALSE;

  if (save == NULL) {
    save = (vgaHWPtr)Xcalloc(size);
    /*
     * Here we are, when we first save the videostate. This means we came here
     * to save the original Text mode. Because some drivers may depend
     * on NoClock we set it here to a resonable value.
     */
    first_time = TRUE;
    if (vga256InfoRec.clockprog && vga256InfoRec.textclock >= 0)
      save->NoClock = -1;
    else
      /* This isn't very useful -- it ignores the high order CS bits */
      save->NoClock = (inb(0x3CC) >> 2) & 3;
  }

  /*
   * now get the fuck'in register
   */
  save->MiscOutReg = inb(0x3CC);
  vgaIOBase = (save->MiscOutReg & 0x01) ? 0x3D0 : 0x3B0;

  tmp = inb(vgaIOBase + 0x0A); /* reset flip-flop */
  outb(0x3C0, 0x00);

#ifdef NEED_SAVED_CMAP
  /*
   * Some recent (1991) ET4000 chips have a HW bug that prevents the reading
   * of the color lookup table.  Mask rev 9042EAI is known to have this bug.
   *
   * X386 already keeps track of the contents of the color lookup table so
   * reading the HW isn't needed.  Therefore, as a workaround for this HW
   * bug, the following (correct) code has been #ifdef'ed out.  This is also
   * a valid change for ET4000 chips that don't have the HW bug.  The code
   * is really just being retained for reference.  MWS 22-Aug-91
   *
   * This is *NOT* true for 386BSD, Mach -- the initial colour map must be
   * restored.  When saving the text mode, we check if the colourmap is
   * readable.  If so we read it.  If not, we set the saved map to a
   * default map (taken from Ferraro's "Programmer's Guide to the EGA and
   * VGA Cards" 2nd ed).
   */

  if (first_time)
  {
    int read_error = 0;

    outb(0x3C6,0xFF);
    /*
     * check if we can read the lookup table
     */
    outb(0x3C7,0x00);
    for (i=0; i<3; i++) save->DAC[i] = inb(0x3C9);
    outb(0x3C8,0x00);
    for (i=0; i<3; i++) outb(0x3C9, ~save->DAC[i]);
    outb(0x3C7,0x00);
    for (i=0; i<3; i++)
    {
      unsigned char tmp = inb(0x3C9);
      if (tmp != (~save->DAC[i]&0x3F)) read_error++;
    }
  
    if (read_error)
    {
      /*			 
       * save the default lookup table
       */
      bcopy(defaultDAC, save->DAC, 768);
      ErrorF("VGA256: Cannot read colourmap from VGA.");
      ErrorF("  Will restore with default\n");
    }
    else
    {
      /*			 
       * save the colorlookuptable 
       */
      outb(0x3C7,0x01);
      for (i=3; i<768; i++)
      {
	save->DAC[i] = inb(0x3C9); 
	DACDelay;
      }
    }
  }
#endif /* NEED_SAVED_CMAP */

  for (i=0; i<25; i++) { outb(vgaIOBase + 4,i);
			 save->CRTC[i] = inb(vgaIOBase + 5); }

  for (i=0; i<21; i++) {
    tmp = inb(vgaIOBase + 0x0A);
    outb(0x3C0,i);
    save->Attribute[i] = inb(0x3C1);
  }

  for (i=0; i<9;  i++) { outb(0x3CE,i); save->Graphics[i]  = inb(0x3CF); }

  for (i=0; i<5;  i++) { outb(0x3C4,i); save->Sequencer[i]   = inb(0x3C5); }

  vgaSaveScreen(NULL, FALSE);
  
  /* XXXX Still not sure if this is needed.  It isn't done in the Restore */
  outb(0x3C2, save->MiscOutReg | 0x01);		/* shift to colour emulation */
  /* Since forced to colour mode, must use 0x3Dx instead of (vgaIOBase + x) */

  /*
   * get the character sets, and text screen if required
   */
  if (((save->Attribute[0x10] & 0x01) == 0)) {
#ifdef SAVE_FONT1
    if (save->FontInfo1 == NULL) {
      save->FontInfo1 = (pointer)xalloc(FONT_AMOUNT);
      /*
       * Here we switch temporary to 16 color-plane-mode, to simply
       * copy the font-info
       *
       * BUGALLERT: The vga's segment-select register MUST be set appropriate !
       */
      tmp = inb(0x3D0 + 0x0A); /* reset flip-flop */
      outb(0x3C0,0x30); outb(0x3C0, 0x01); /* graphics mode */
      outw(0x3C4, 0x0402);    /* write to plane 2 */
      outw(0x3C4, 0x0604);    /* enable plane graphics */
      outw(0x3CE, 0x0204);    /* read plane 2 */
      outw(0x3CE, 0x0005);    /* write mode 0, read mode 0 */
      outw(0x3CE, 0x0506);    /* set graphics */
      slowbcopy(vgaBase, save->FontInfo1, FONT_AMOUNT);
    }
#endif /* SAVE_FONT1 */
#ifdef SAVE_FONT2
    if (save->FontInfo2 == NULL) {
      save->FontInfo2 = (pointer)xalloc(FONT_AMOUNT);
      tmp = inb(0x3D0 + 0x0A); /* reset flip-flop */
      outb(0x3C0,0x30); outb(0x3C0, 0x01); /* graphics mode */
      outw(0x3C4, 0x0802);    /* write to plane 3 */
      outw(0x3C4, 0x0604);    /* enable plane graphics */
      outw(0x3CE, 0x0304);    /* read plane 3 */
      outw(0x3CE, 0x0005);    /* write mode 0, read mode 0 */
      outw(0x3CE, 0x0506);    /* set graphics */
      slowbcopy(vgaBase, save->FontInfo2, FONT_AMOUNT);
    }
#endif /* SAVE_FONT2 */
#ifdef SAVE_TEXT
    if (save->TextInfo == NULL) {
      save->TextInfo = (pointer)xalloc(2 * TEXT_AMOUNT);
      tmp = inb(0x3D0 + 0x0A); /* reset flip-flop */
      outb(0x3C0,0x30); outb(0x3C0, 0x01); /* graphics mode */
      /*
       * This is a quick hack to save the text screen for system that don't
       * restore it automatically.
       */
      outw(0x3C4, 0x0102);    /* write to plane 0 */
      outw(0x3C4, 0x0604);    /* enable plane graphics */
      outw(0x3CE, 0x0004);    /* read plane 0 */
      outw(0x3CE, 0x0005);    /* write mode 0, read mode 0 */
      outw(0x3CE, 0x0506);    /* set graphics */
      slowbcopy(vgaBase, save->TextInfo, TEXT_AMOUNT);
      outw(0x3C4, 0x0202);    /* write to plane 1 */
      outw(0x3C4, 0x0604);    /* enable plane graphics */
      outw(0x3CE, 0x0104);    /* read plane 1 */
      outw(0x3CE, 0x0005);    /* write mode 0, read mode 0 */
      outw(0x3CE, 0x0506);    /* set graphics */
      slowbcopy(vgaBase, save->TextInfo + TEXT_AMOUNT, TEXT_AMOUNT);
    }
#endif /* SAVE_TEXT */
  }

  outb(0x3C2, save->MiscOutReg);		/* back to original setting */
  
  vgaSaveScreen(NULL, TRUE);

  /* Turn on PAS bit */
  tmp = inb(vgaIOBase + 0x0A);
  outb(0x3C0, 0x20);
  
  return ((void *) save);
}



/*
 * vgaHWInit --
 *      Handle the initialization, etc. of a screen.
 *      Return FALSE on failure.
 */

Bool
vgaHWInit(mode, size)
     DisplayModePtr      mode;
     int             size;
{
  int                i;

  /*
   * If we're using an external clock program, the first thing we do is
   * call it.  If it fails for any reason, we abort the mode switch and
   * hope that it hasn't screwed up the old clock setting.
   */
  if (vga256InfoRec.clockprog)
  {
    if (setExternClock(mode->Clock))
      currentGraphicsClock = mode->Clock;
    else
      return(FALSE);
  }

  if (vgaNewVideoState == NULL) {
    vgaNewVideoState = (void *)Xcalloc(size);

    /*
     * initialize default colormap for monochrome
     */
    for (i=0; i<3;   i++) new->DAC[i] = 0x00;
    for (i=3; i<768; i++) new->DAC[i] = 0x3F;
#ifdef MONOVGA
    i = BLACK_VALUE * 3;
    new->DAC[i++] = vga256InfoRec.blackColour.red;
    new->DAC[i++] = vga256InfoRec.blackColour.green;
    new->DAC[i] = vga256InfoRec.blackColour.blue;
    i = WHITE_VALUE * 3;
    new->DAC[i++] = vga256InfoRec.whiteColour.red;
    new->DAC[i++] = vga256InfoRec.whiteColour.green;
    new->DAC[i] = vga256InfoRec.whiteColour.blue;
    i = OVERSCAN_VALUE * 3;
    new->DAC[i++] = 0x00;
    new->DAC[i++] = 0x00;
    new->DAC[i] = 0x00;
#endif
#ifndef MONOVGA
    /* Initialise overscan register */
    new->Attribute[17] = 0xFF;
#endif

  }

  /*
   * Get NoClock.  Set it to -1 if using an external clock setting program
   */
  if (vga256InfoRec.clockprog)
    new->NoClock = -1;
  else
    new->NoClock = mode->Clock;

  /*
   * compute correct Hsync & Vsync polarity 
   */
  if ((mode->Flags & (V_PHSYNC | V_NHSYNC))
      && (mode->Flags & (V_PVSYNC | V_NVSYNC)))
      {
	new->MiscOutReg = 0x23;
	if (mode->Flags & V_NHSYNC) new->MiscOutReg |= 0x40;
	if (mode->Flags & V_NVSYNC) new->MiscOutReg |= 0x80;
      }
      else
      {
	if      (mode->VDisplay < 400) new->MiscOutReg = 0xA3;
	else if (mode->VDisplay < 480) new->MiscOutReg = 0x63;
	else if (mode->VDisplay < 768) new->MiscOutReg = 0xE3;
	else                           new->MiscOutReg = 0x23;
      }
  if (!vga256InfoRec.clockprog)
    new->MiscOutReg |= (new->NoClock & 0x03) << 2;
  
  /*
   * Time Sequencer
   */
#ifdef XF86VGA16
  new->Sequencer[0] = 0x02;
#else
  new->Sequencer[0] = 0x00;
#endif
  new->Sequencer[1] = 0x01;
#ifdef MONOVGA
  new->Sequencer[2] = 1 << BIT_PLANE;
#else
  new->Sequencer[2] = 0x0F;
#endif
  new->Sequencer[3] = 0x00;                             /* Font select */
#if defined(MONOVGA) || defined(XF86VGA16)
  new->Sequencer[4] = 0x06;                             /* Misc */
#else
  new->Sequencer[4] = 0x0E;                             /* Misc */
#endif

  if (vgaInterlaceType == VGA_DIVIDE_VERT && (mode->Flags & V_INTERLACE)) {
    mode->VDisplay >>= 1;
    mode->VSyncStart >>= 1;
    mode->VSyncEnd >>= 1;
    mode->VTotal >>= 1;
  }
    
  /*
   * CRTC Controller
   */
  new->CRTC[0]  = (mode->HTotal >> 3) - 5;
  new->CRTC[1]  = (mode->HDisplay >> 3) - 1;
  new->CRTC[2]  = (mode->HSyncStart >> 3) -1;
  new->CRTC[3]  = ((mode->HSyncEnd >> 3) & 0x1F) | 0x80;
  new->CRTC[4]  = (mode->HSyncStart >> 3);
  new->CRTC[5]  = (((mode->HSyncEnd >> 3) & 0x20 ) << 2 )
    | (((mode->HSyncEnd >> 3)) & 0x1F);
  new->CRTC[6]  = (mode->VTotal - 2) & 0xFF;
  new->CRTC[7]  = (((mode->VTotal -2) & 0x100) >> 8 )
    | (((mode->VDisplay -1) & 0x100) >> 7 )
      | ((mode->VSyncStart & 0x100) >> 6 )
	| (((mode->VSyncStart) & 0x100) >> 5 )
	  | 0x10
	    | (((mode->VTotal -2) & 0x200)   >> 4 )
	      | (((mode->VDisplay -1) & 0x200) >> 3 )
		| ((mode->VSyncStart & 0x200) >> 2 );
  new->CRTC[8]  = 0x00;
  new->CRTC[9]  = ((mode->VSyncStart & 0x200) >>4) | 0x40;
  new->CRTC[10] = 0x00;
  new->CRTC[11] = 0x00;
  new->CRTC[12] = 0x00;
  new->CRTC[13] = 0x00;
  new->CRTC[14] = 0x00;
  new->CRTC[15] = 0x00;
  new->CRTC[16] = mode->VSyncStart & 0xFF;
  new->CRTC[17] = (mode->VSyncEnd & 0x0F) | 0x20;
  new->CRTC[18] = (mode->VDisplay -1) & 0xFF;
  new->CRTC[19] = vga256InfoRec.virtualX >> 4;  /* just a guess */
  new->CRTC[20] = 0x00;
  new->CRTC[21] = mode->VSyncStart & 0xFF; 
  new->CRTC[22] = (mode->VSyncStart +1) & 0xFF;
#if defined(MONOVGA) || defined(XF86VGA16)
  new->CRTC[23] = 0xE3;
#else
  new->CRTC[23] = 0xC3;
#endif
  new->CRTC[24] = 0xFF;

  if (vgaInterlaceType == VGA_DIVIDE_VERT && (mode->Flags & V_INTERLACE)) {
    mode->VDisplay <<= 1;
    mode->VSyncStart <<= 1;
    mode->VSyncEnd <<= 1;
    mode->VTotal <<= 1;
  }
    
  /*
   * Graphics Display Controller
   */
  new->Graphics[0] = 0x00;
  new->Graphics[1] = 0x00;
  new->Graphics[2] = 0x00;
  new->Graphics[3] = 0x00;
#ifdef MONOVGA
  new->Graphics[4] = BIT_PLANE;
  new->Graphics[5] = 0x00;
#else
  new->Graphics[4] = 0x00;
#ifdef XF86VGA16
  new->Graphics[5] = 0x02;
#else
  new->Graphics[5] = 0x40;
#endif
#endif
  new->Graphics[6] = 0x05;   /* only map 64k VGA memory !!!! */
  new->Graphics[7] = 0x0F;
  new->Graphics[8] = 0xFF;
  
#ifdef MONOVGA
  /* Initialise the Mono map according to which bit-plane gets written to */

  for (i=0; i<16; i++)
    if (i & (1<<BIT_PLANE))
      new->Attribute[i] = WHITE_VALUE;
    else
      new->Attribute[i] = BLACK_VALUE;

  new->Attribute[16] = 0x01;  /* -VGA2- */ /* wrong for the ET4000 */
  new->Attribute[17] = OVERSCAN_VALUE;  /* -VGA2- */
#else
  new->Attribute[0]  = 0x00; /* standard colormap translation */
  new->Attribute[1]  = 0x01;
  new->Attribute[2]  = 0x02;
  new->Attribute[3]  = 0x03;
  new->Attribute[4]  = 0x04;
  new->Attribute[5]  = 0x05;
  new->Attribute[6]  = 0x06;
  new->Attribute[7]  = 0x07;
  new->Attribute[8]  = 0x08;
  new->Attribute[9]  = 0x09;
  new->Attribute[10] = 0x0A;
  new->Attribute[11] = 0x0B;
  new->Attribute[12] = 0x0C;
  new->Attribute[13] = 0x0D;
  new->Attribute[14] = 0x0E;
  new->Attribute[15] = 0x0F;
#ifdef XF86VGA16
  new->Attribute[16] = 0x81; /* wrong for the ET4000 */
  new->Attribute[17] = 0x00; /* GJA -- overscan. */
#else
  new->Attribute[16] = 0x41; /* wrong for the ET4000 */
#endif
  /*
   * Attribute[17] is the overscan, and is initalised above only at startup
   * time, and not when mode switching.
   */
#endif
  new->Attribute[18] = 0x0F;
  new->Attribute[19] = 0x00;
  new->Attribute[20] = 0x00;
  return(TRUE);
}


/*
 * vgaGetClocks --
 *      get the dot-clocks via a BIG BAD hack ...
 */

void
vgaGetClocks(num, ClockFunc)
     int num;
     Bool (*ClockFunc)();
{
  xf86GetClocks(num, ClockFunc, vgaProtect, vgaSaveScreen, (vgaIOBase + 0x0A),
		0x08, 1, 28322, &vga256InfoRec);
}
