/*
 * $XFree86: mit/server/ddx/x386/common/XF86_Mono.c,v 2.2 1993/08/30 15:23:13 dawes Exp $
 */

#include "X.h"
#include "os.h"
#include "x386.h"
#include "xf86_Config.h"

extern ScrnInfoRec vga2InfoRec;
extern ScrnInfoRec hga2InfoRec;
extern ScrnInfoRec bdm2InfoRec;

#ifdef BUILD_VGA2
#define SCREEN0 &vga2InfoRec
#else
#define SCREEN0 NULL
#endif

#ifdef BUILD_HGA2
#define SCREEN1 &hga2InfoRec
#else
#define SCREEN1 NULL
#endif

#ifdef BUILD_BDM2
#define SCREEN2 &bdm2InfoRec
#else
#define SCREEN2 NULL
#endif

ScrnInfoPtr x386Screens[] = 
{
  SCREEN0,
  SCREEN1,
  SCREEN2,
};

int  x386MaxScreens = sizeof(x386Screens) / sizeof(ScrnInfoPtr);

int xf86ScreenNames[] =
{
  VGA2,
  HGA2,
  BDM2,
  -1
};

#ifdef BUILD_VGA2
int vga2ValidTokens[] =
{
  STATICGRAY,
  CHIPSET,
  CLOCKS,
  DISPLAYSIZE,
  MODES,
  SCREENNO,
  OPTION,
  VIDEORAM,
  VIEWPORT,
  VIRTUAL,
  CLOCKPROG,
  BIOSBASE,
  BLACK,
  WHITE,
  -1
};
#endif /* BUILDVGA2 */

#ifdef BUILD_HGA2
int hga2ValidTokens[] =
{
  STATICGRAY,
  SCREENNO,
  DISPLAYSIZE,
  -1
};
#endif /* BUILDHGA2 */

#ifdef BUILD_BDM2
int bdm2ValidTokens[] =
{
  STATICGRAY,
  CHIPSET,
  SCREENNO,
  DISPLAYSIZE,
  VIRTUAL,
  VIEWPORT,
  -1
}; 
#endif /* BUILDBDM2 */

/* Dummy function for PEX in LinkKit and mono server */

PexExtensionInit() {}
