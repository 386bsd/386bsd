/*
 * $XFree86: mit/server/ddx/x386/common/XF86_SVGA.c,v 2.1 1993/08/01 05:56:02 dawes Exp $
 */

#include "X.h"
#include "os.h"
#include "x386.h"
#include "xf86_Config.h"

extern ScrnInfoRec vga256InfoRec;

ScrnInfoPtr x386Screens[] = 
{
  &vga256InfoRec,
};

int  x386MaxScreens = sizeof(x386Screens) / sizeof(ScrnInfoPtr);

int xf86ScreenNames[] =
{
  VGA256,
  -1
};

int vga256ValidTokens[] =
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
  SPEEDUP,
  NOSPEEDUP,
  CLOCKPROG,
  BIOSBASE,
  -1
};

/* Dummy function for PEX in LinkKit and mono server */

#if defined(LINKKIT) && !defined(PEXEXT)
PexExtensionInit() {}
#endif
