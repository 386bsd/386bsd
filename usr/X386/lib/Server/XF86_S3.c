/*
 * $XFree86: mit/server/ddx/x386/common/XF86_S3.c,v 2.2 1993/10/14 16:05:56 dawes Exp $
 */

#include "X.h"
#include "os.h"
#include "x386.h"
#include "xf86_Config.h"

extern ScrnInfoRec s3InfoRec;

/*
 * This limit is set to 110MHz because this is the limit for
 * the ramdacs used on many S3 cards Increasing this limit
 * could result in damage to your hardware.
 */
/* Clock limit for non-Bt485 cards */
#define MAX_S3_CLOCK    110000

/*
 * This limit is currently set to 85MHz because this is the limit for
 * the Bt485 ramdac when running in 1:1 mode.  It will be increased when
 * support for using the ramdac in 4:1 mode.  Increasing this limit
 * could result in damage to your hardware.
 */

/* Clock limit for cards with a Bt485 */
#define MAX_BT485_CLOCK  85000

int s3MaxClock = MAX_S3_CLOCK;
int s3MaxBt485Clock = MAX_BT485_CLOCK;

ScrnInfoPtr x386Screens[] = 
{
  &s3InfoRec,
};

int  x386MaxScreens = sizeof(x386Screens) / sizeof(ScrnInfoPtr);

int xf86ScreenNames[] =
{
  ACCEL,
  -1
};

int s3ValidTokens[] =
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
