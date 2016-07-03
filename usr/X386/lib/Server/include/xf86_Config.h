/*
 * Copyright 1990,91 by Thomas Roell, Dinkelscherben, Germany
 * Copyright 1993 by David Dawes <dawes@physics.su.oz.au>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the names of Thomas Roell and David Dawes 
 * not be used in advertising or publicity pertaining to distribution of 
 * the software without specific, written prior permission.  Thomas Roell and
 * David Dawes makes no representations about the suitability of this 
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * THOMAS ROELL AND DAVID DAWES DISCLAIMS ALL WARRANTIES WITH REGARD TO 
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND 
 * FITNESS, IN NO EVENT SHALL THOMAS ROELL OR DAVID DAWES BE LIABLE FOR 
 * ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER 
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF 
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

/* $XFree86: mit/server/ddx/x386/common/xf86_Config.h,v 2.7 1993/10/07 13:56:00 dawes Exp $ */

#ifndef XCONFIG_FLAGS_ONLY

#ifdef BLACK
#undef BLACK
#endif
#ifdef WHITE
#undef WHITE
#endif
#ifdef SCROLLLOCK
#undef SCROLLLOCK
#endif

typedef struct {
  int           token;                /* id of the token */
  char          *name;                /* pointer to the LOWERCASED name */
} SymTabRec, *SymTabPtr;

typedef struct {
  int           num;                  /* returned number */
  char          *str;                 /* private copy of the return-string */
  double        realnum;              /* returned number as a real */
} LexRec, *LexPtr;

#define LOCK_TOKEN  -3
#define ERROR_TOKEN -2
#define NUMBER      10000                  
#define STRING      10001

/* XConfig sections */
#define FONTPATH   0
#define RGBPATH    1
#define SHAREDMON  2
#define NOTRAPSIGNALS 3

#define KEYBOARD   10

#define MICROSOFT  20
#define MOUSESYS   21
#define MMSERIES   22
#define LOGITECH   23
#define BUSMOUSE   24
#define LOGIMAN    25
#define PS_2       26
#define MMHITTAB   27
#define XQUE       30
#define OSMOUSE    31

#define VGA256     40
#define VGA2       41
#define HGA2       42
#define BDM2       43
#define VGA16      44
#define ACCEL      45

#define MODEDB     60

#ifdef INIT_CONFIG
static SymTabRec SymTab[] = {
  { FONTPATH,   "fontpath" },
  { RGBPATH,    "rgbpath" },
  { SHAREDMON,  "sharedmonitor" },
  { NOTRAPSIGNALS, "notrapsignals" },

  { KEYBOARD,   "keyboard" },

  { MICROSOFT,  "microsoft" },
  { MOUSESYS,   "mousesystems" },
  { MMSERIES,   "mmseries" },
  { LOGITECH,   "logitech" },
  { BUSMOUSE,   "busmouse" },
  { LOGIMAN,    "mouseman" },
  { PS_2,       "ps/2" },
  { MMHITTAB,   "mmhittab" },
  { XQUE,       "xqueue" },
  { OSMOUSE,    "osmouse" },

  { VGA256,     "vga256" },
  { VGA2,       "vga2" },
  { HGA2,       "hga2" },
  { BDM2,       "bdm2" },
  { VGA16,      "vga16" },
  { ACCEL,      "accel" },


  { MODEDB,     "modedb" },

  { -1,         "" },
};
#endif /* INIT_CONFIG */

#define P_MS		0			/* Microsoft */
#define P_MSC		1			/* Mouse Systems Corp */
#define P_MM		2			/* MMseries */
#define P_LOGI		3			/* Logitech */
#define P_BM		4			/* BusMouse ??? */
#define P_LOGIMAN	5			/* MouseMan / TrackMan
						   [CHRIS-211092] */
#define P_PS2		6			/* PS/2 mouse */
#define P_MMHIT		7			/* MM_HitTab */

/* Keyboard keywords */
#define AUTOREPEAT 0
#define DONTZAP    1
#define SERVERNUM  2
#define XLEDS      3
#define VTINIT     4
#define LEFTALT    5
#define RIGHTALT   6
#define SCROLLLOCK 7
#define RIGHTCTL   8

#ifdef INIT_CONFIG
static SymTabRec KeyboardTab[] = {
  { AUTOREPEAT, "autorepeat" },
  { DONTZAP,    "dontzap" },
  { SERVERNUM,  "servernumlock" },
  { XLEDS,      "xleds" },
  { VTINIT,     "vtinit" },
  { LEFTALT,    "leftalt" },
  { RIGHTALT,   "rightalt" },
  { RIGHTALT,   "altgr" },
  { SCROLLLOCK, "scrolllock" },
  { RIGHTCTL,   "rightctl" },
  { -1,         "" },
};
#endif /* INIT_CONFIG */

/* Indexes for the specialKeyMap array */

#define K_INDEX_LEFTALT    0
#define K_INDEX_RIGHTALT   1
#define K_INDEX_SCROLLLOCK 2
#define K_INDEX_RIGHTCTL   3

/* Values for the specialKeyMap array */
#define K_META             0
#define K_COMPOSE          1
#define K_MODESHIFT        2
#define K_MODELOCK         3
#define K_SCROLLLOCK       4
#define K_CONTROL          5

#ifdef INIT_CONFIG
static SymTabRec KeyMapTab[] = {
  { K_META,       "meta" },
  { K_COMPOSE,    "compose" },
  { K_MODESHIFT,  "modeshift" },
  { K_MODELOCK,   "modelock" },
  { K_SCROLLLOCK, "scrolllock" },
  { K_CONTROL,    "control" },
  { -1,           "" },
};
#endif /* INIT_CONFIG */

/* Mouse keywords */
#define EMULATE3   0
#define BAUDRATE   1
#define SAMPLERATE 2
#define CLEARDTR   3
#define CHORDMIDDLE 4

#ifdef INIT_CONFIG
static SymTabRec MouseTab[] = {
  { BAUDRATE,   "baudrate" },
  { EMULATE3,   "emulate3buttons" },
  { SAMPLERATE, "samplerate" },
  { CLEARDTR,   "cleardtr" },
  { CHORDMIDDLE,"chordmiddle" },
  { -1,         "" },
};
#endif /* INIT_CONFIG */

/* Graphics keywords */
#define STATICGRAY  0
#define GRAYSCALE   1
#define STATICCOLOR 2
#define PSEUDOCOLOR 3
#define TRUECOLOR   4
#define DIRECTCOLOR 5

#define CHIPSET     10
#define CLOCKS      11
#define DISPLAYSIZE 12
#define MODES       13
#define SCREENNO    14
#define OPTION      15
#define VIDEORAM    16
#define VIEWPORT    17
#define VIRTUAL     18
#define SPEEDUP     19
#define NOSPEEDUP   20
#define CLOCKPROG   21
#define BIOSBASE    22
#define BLACK       23
#define WHITE       24

#ifdef INIT_CONFIG
static SymTabRec GraphicsTab[] = {
  { STATICGRAY, "staticgray" },
  { GRAYSCALE,  "grayscale" },
  { STATICCOLOR,"staticcolor" },
  { PSEUDOCOLOR,"pseudocolor" },
  { TRUECOLOR,  "truecolor" },
  { DIRECTCOLOR,"directcolor" },

  { CHIPSET,    "chipset" },
  { CLOCKS,     "clocks" },
  { DISPLAYSIZE,"displaysize" },
  { MODES,      "modes" },
  { SCREENNO,   "screenno" },
  { OPTION,     "option" },
  { VIDEORAM,   "videoram" },
  { VIEWPORT,   "viewport" },
  { VIRTUAL,    "virtual" },
  { SPEEDUP,    "speedup" },
  { NOSPEEDUP,  "nospeedup" },
  { CLOCKPROG,  "clockprog" },
  { BIOSBASE,   "biosbase" },
  { BLACK,	"black" },
  { WHITE,	"white" },
  { -1,         "" },
};
#endif /* INIT_CONFIG */

/* Mode timing keywords */
#define INTERLACE 0
#define PHSYNC    1
#define NHSYNC    2
#define PVSYNC    3
#define NVSYNC    4

#ifdef INIT_CONFIG
static SymTabRec TimingTab[] = {
  { INTERLACE,  "interlace"},
  { PHSYNC,     "+hsync"},
  { NHSYNC,     "-hsync"},
  { PVSYNC,     "+vsync"},
  { NVSYNC,     "-vsync"},
  { -1,         "" },
};
#endif /* INIT_CONFIG */

#endif /* XCONFIG_FLAGS_ONLY */

/* 
 *   Xconfig flags to record which options were defined in the Xconfig file
 */
#define XCONFIG_FONTPATH        1       /* Commandline/Xconfig or default  */
#define XCONFIG_RGBPATH         2       /* Xconfig or default */
#define XCONFIG_CHIPSET         3       /* Xconfig or probed */
#define XCONFIG_CLOCKS          4       /* Xconfig or probed */
#define XCONFIG_DISPLAYSIZE     5       /* Xconfig or default/calculated */
#define XCONFIG_VIDEORAM        6       /* Xconfig or probed */
#define XCONFIG_VIEWPORT        7       /* Xconfig or default */
#define XCONFIG_VIRTUAL         8       /* Xconfig or default/calculated */
#define XCONFIG_SPEEDUP         9       /* Xconfig or default/calculated */
#define XCONFIG_NOMEMACCESS     10      /* set if forced on */

#define XCONFIG_GIVEN		"(**)"
#define XCONFIG_PROBED		"(--)"

#ifdef INIT_CONFIG

OFlagSet  GenericXconfigFlag;

#else

extern OFlagSet  GenericXconfigFlag;

#endif  /* INIT_CONFIG */
