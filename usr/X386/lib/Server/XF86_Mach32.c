/*
 * $XFree86: mit/server/ddx/x386/common/XF86_Mach32.c,v 2.1 1993/10/07 13:55:56 dawes Exp $
 */

#include "X.h"
#include "os.h"
#include "x386.h"
#include "xf86_Config.h"

extern ScrnInfoRec mach32InfoRec;

/*
 * This limit is currently set to 80MHz because this is the limit of many
 * ramdacs when running in 1:1 mode.  It will be increased when support
 * is added for using the ramdacs in 2:1 mode.  Increasing this limit
 * could result in damage to your hardware.
 */
#define MAX_MACH32_CLOCK	80000

int mach32MaxClock = MAX_MACH32_CLOCK;

ScrnInfoPtr x386Screens[] = 
{
  &mach32InfoRec,
};

int  x386MaxScreens = sizeof(x386Screens) / sizeof(ScrnInfoPtr);

int xf86ScreenNames[] =
{
  ACCEL,
  -1
};

int mach32ValidTokens[] =
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
