/*
 * $XFree86: mit/server/ddx/x386/common/XF86_Mach8.c,v 2.2 1993/10/14 16:05:54 dawes Exp $
 */

#include "X.h"
#include "os.h"
#include "x386.h"
#include "xf86_Config.h"

extern ScrnInfoRec mach8InfoRec;

/*
 * This limit is set to a value which is typical for many of the ramdacs
 * used on Mach8 cards.  Increasing this limit could result in damage to
 * to your hardware.
 */
/* XXXX This value needs to be checked (currently using 80MHz) */
#define MAX_MACH8_CLOCK 80000

int mach8MaxClock = MAX_MACH8_CLOCK;

ScrnInfoPtr x386Screens[] = 
{
  &mach8InfoRec,
};

int  x386MaxScreens = sizeof(x386Screens) / sizeof(ScrnInfoPtr);

int xf86ScreenNames[] =
{
  ACCEL,
  -1
};

int mach8ValidTokens[] =
{
  STATICGRAY,
  GRAYSCALE,
  STATICCOLOR,
  PSEUDOCOLOR,
  TRUECOLOR,
  DIRECTCOLOR,
  CHIPSET,
  CLOCKS,
  DISPLAYSIZE,
  MODES,
  OPTION,
  VIDEORAM,
  VIEWPORT,
  VIRTUAL,
  CLOCKPROG,
  BIOSBASE,
  -1
};

/* Dummy function for PEX in LinkKit and mono server */

#if defined(LINKKIT) && !defined(PEXEXT)
PexExtensionInit() {}
#endif
