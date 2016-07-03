/*
 * BDM2: Banked dumb monochrome driver
 * Pascal Haible 8/93, haible@izfm.uni-stuttgart.de
 *
 * bdm2/bdm/bdm.h
 *
 * derived from:
 * hga2/*
 * Author:  Davor Matic, dmatic@athena.mit.edu
 * and
 * vga256/*
 * Copyright 1990,91 by Thomas Roell, Dinkelscherben, Germany.
 *
 * see bdm2/COPYRIGHT for copyright and disclaimers.
 */

/* $XFree86: mit/server/ddx/x386/bdm2/bdm/bdm.h,v 2.0 1993/08/30 15:21:57 dawes Exp $ */

/* Included from bdm.c, bdmBank.c */

#define BDM2_PATCHLEVEL "0"

#include "X.h"
#include "misc.h"
#include "x386.h"

/* External dummy function (Don't know from where..) */
extern void NoopDDA();

/* Functions in bdm.c */
extern void    bdmPrintIdent();
extern Bool    bdmProbe();
extern Bool    bdmScreenInit();
extern void    bdmEnterLeaveVT();
extern Bool    bdmCloseScreen();
extern void    bdmAdjustFrame();

extern int     bdm2ValidTokens[];

/*
 * structure for accessing the video chip`s functions
 *
 * We are doing a total encapsulation of the driver's functions.
 * Banking (bdmSetReadWrite(p) etc.) is done in bdmBank.c
 *   using the chip's function pointed to by bmpSetReadWriteFunc(bank)
 */
typedef struct {
  char * (* ChipIdent)();
  Bool (* ChipProbe)();
  void (* ChipEnterLeave)();
  void * (* ChipInit)();
  void * (* ChipSave)();
  void (* ChipRestore)();
  void (* ChipAdjust)();
  Bool (* ChipSaveScreen)();
  void (* ChipGetMode)();
  /* These are the chip's banking functions:		*/
  /* they do the real switching to the desired bank	*/
  /* they 'become' bdmSetReadFunc() etc.		*/
  void (* ChipSetRead)();
  void (* ChipSetWrite)();
  void (* ChipSetReadWrite)();
  /* Bottom and top of the banking window (rel. to ChipMapBase)	*/
  /* Note: Top = highest accessable byte + 1			*/
  void *ChipReadBottom;
  void *ChipReadTop;
  void *ChipWriteBottom;
  void *ChipWriteTop;
  /* Memory to map	*/
  int ChipMapBase;
  int ChipMapSize;	/* replaces MEMTOMAP */
  int ChipSegmentSize;
  int ChipSegmentShift;
  int ChipSegmentMask;
  Bool ChipUse2Banks;
  /* Display size is given by the driver */
  int ChipHDisplay;
  int ChipVDisplay;
  /* In case Scan Line in mfb is longer than HDisplay */
  int ChipScanLineWidth;/* in pixels */
} bdmVideoChipRec, *bdmVideoChipPtr;

/*
 * hooks for communicating with the VideoChip on the BDM
 */
extern void (* bdmEnterLeaveFunc)();
extern void * (* bdmInitFunc)();
extern void * (* bdmSaveFunc)();
extern void (* bdmRestoreFunc)();
extern void (* bdmAdjustFunc)();
extern Bool (* bdmSaveScreenFunc)();
extern void (* bdmGetModeFunc)();
extern void (* bdmSetReadFunc)();
extern void (* bdmSetWriteFunc)();
extern void (* bdmSetReadWriteFunc)();

extern pointer bdmOrigVideoState;    /* buffers for all video information */
extern pointer bdmNewVideoState;
extern pointer bdmBase;              /* the framebuffer himself */

#define BITS_PER_GUN 1
#define COLORMAP_SIZE 2

extern ScrnInfoRec bdm2InfoRec;

/*
 * List of I/O ports for ...
 */
/* don't do this here, this really is a low level thing */
/* extern unsigned BDM_IOPorts[]; */
/* extern int Num_BDM_IOPorts; */
