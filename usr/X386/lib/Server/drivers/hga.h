/*
 *
 * Author:  Davor Matic, dmatic@athena.mit.edu
 *
 * $XFree86: mit/server/ddx/x386/hga2/hga/hga.h,v 2.3 1993/08/16 14:39:43 dawes Exp $
 */

#define HGA2_PATCHLEVEL "0"

#include "X.h"
#include "misc.h"
#include "x386.h"

extern Bool    hgaProbe();
extern void    hgaPrintIdent();
extern Bool    hgaScreenInit();
extern void    hgaEnterLeaveVT();

extern Bool    hgaSaveScreen();
extern Bool    hgaCloseScreen();

extern Bool    hgaHWInit();
extern void    hgaHWRestore();
extern void *  hgaHWSave();

extern int    hga2ValidTokens[];

/*
 * structure for accessing the video chip`s functions
 */
typedef struct {
  Bool (* ChipProbe)();
  char * (* ChipIdent)();
  void (* ChipEnterLeave)();
  Bool (* ChipInit)();
  void * (* ChipSave)();
  void (* ChipRestore)();
} hgaVideoChipRec, *hgaVideoChipPtr;

/*
 * hooks for communicating with the VideoChip on the HGA
 */
extern Bool (* hgaInitFunc)();
extern void (* hgaEnterLeaveFunc)();
extern void * (* hgaSaveFunc)();
extern void (* hgaRestoreFunc)();

extern pointer hgaOrigVideoState;    /* buffers for all video information */
extern pointer hgaNewVideoState;
extern pointer hgaBase;              /* the framebuffer himself */

typedef struct {
  unsigned char conf; /* write only conf register at port 0x3BF */
  unsigned char mode; /* write only mode register at port 0x3B8 */
} hgaHWRec, *hgaHWPtr;

#define BITS_PER_GUN 1
#define COLORMAP_SIZE 2

extern ScrnInfoRec hga2InfoRec;

/*
 * List of I/O ports for generic HGA
 */
extern unsigned HGA_IOPorts[];
extern int Num_HGA_IOPorts;
