/* $XConsortium: input.h,v 1.12 92/08/21 15:09:38 rws Exp $ */
/************************************************************
Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts,
and the Massachusetts Institute of Technology, Cambridge, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the names of Digital or MIT not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

********************************************************/
#ifndef INPUT_H
#define INPUT_H

#include "misc.h"

#define DEVICE_INIT	0
#define DEVICE_ON	1
#define DEVICE_OFF	2
#define DEVICE_CLOSE	3

#define MAP_LENGTH	256
#define DOWN_LENGTH	32	/* 256/8 => number of bytes to hold 256 bits */
#define NullGrab ((GrabPtr)NULL)
#define PointerRootWin ((WindowPtr)PointerRoot)
#define NoneWin ((WindowPtr)None)
#define NullDevice ((DevicePtr)NULL)

#ifndef FollowKeyboard
#define FollowKeyboard 		3
#endif
#ifndef FollowKeyboardWin
#define FollowKeyboardWin  ((WindowPtr) FollowKeyboard)
#endif
#ifndef RevertToFollowKeyboard
#define RevertToFollowKeyboard	3
#endif

typedef unsigned long Leds;
typedef struct _OtherClients *OtherClientsPtr;
typedef struct _InputClients *InputClientsPtr;
typedef struct _GrabRec *GrabPtr;

typedef int (*DeviceProc)();
typedef void (*ProcessInputProc)();

typedef struct _DeviceRec {
    pointer	devicePrivate;
    ProcessInputProc processInputProc;	/* current */
    ProcessInputProc realInputProc;	/* deliver */
    ProcessInputProc enqueueInputProc;	/* enqueue */
    Bool	on;			/* used by DDX to keep state */
} DeviceRec, *DevicePtr;

typedef struct {
    int			click, bell, bell_pitch, bell_duration;
    Bool		autoRepeat;
    unsigned char	autoRepeats[32];
    Leds		leds;
    unsigned char	id;
} KeybdCtrl;

typedef struct {
    KeySym  *map;
    KeyCode minKeyCode,
	    maxKeyCode;
    int     mapWidth;
} KeySymsRec, *KeySymsPtr;

typedef struct {
    int		num, den, threshold;
    unsigned char id;
} PtrCtrl;

typedef struct {
    int         resolution, min_value, max_value;
    int         integer_displayed;
    unsigned char id;
} IntegerCtrl;

typedef struct {
    int         max_symbols, num_symbols_supported;
    int         num_symbols_displayed;
    KeySym      *symbols_supported;
    KeySym      *symbols_displayed;
    unsigned char id;
} StringCtrl;

typedef struct {
    int         percent, pitch, duration;
    unsigned char id;
} BellCtrl;

typedef struct {
    Leds        led_values;
    Mask        led_mask;
    unsigned char id;
} LedCtrl;

extern KeybdCtrl	defaultKeyboardControl;
extern PtrCtrl		defaultPointerControl;

extern DevicePtr AddInputDevice();
extern Bool EnableDevice();
extern Bool DisableDevice();
extern void RegisterPointerDevice();
extern void RegisterKeyboardDevice();
extern DevicePtr LookupKeyboardDevice();
extern DevicePtr LookupPointerDevice();

extern void ProcessPointerEvent();
extern void ProcessKeyboardEvent();

extern Bool InitKeyClassDeviceStruct();
extern Bool InitButtonClassDeviceStruct();
extern Bool InitFocusClassDeviceStruct();
extern Bool InitKbdFeedbackClassDeviceStruct();
extern Bool InitPtrFeedbackClassDeviceStruct();
extern Bool InitValuatorClassDeviceStruct();
extern Bool InitPointerDeviceStruct();
extern Bool InitKeyboardDeviceStruct();

extern void CloseDownDevices();

extern void WriteEventsToClient();

#endif /* INPUT_H */
