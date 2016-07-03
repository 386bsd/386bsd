/***********************************************************
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

******************************************************************/
/* $XFree86: mit/server/include/dix.h,v 2.0 1993/09/09 06:03:29 dawes Exp $ */
/* $XConsortium: dix.h,v 1.60 91/10/30 14:49:57 rws Exp $ */

#ifndef DIX_H
#define DIX_H

#include "gc.h"
#include "window.h"

#define EARLIER -1
#define SAMETIME 0
#define LATER 1

extern void (* miCacheFreeSlot)();

#define NullClient ((ClientPtr) 0)
#define REQUEST(type) \
	register type *stuff = (type *)client->requestBuffer


#define REQUEST_SIZE_MATCH(req)\
    if ((sizeof(req) >> 2) != stuff->length)\
         return(BadLength)

#define REQUEST_AT_LEAST_SIZE(req) \
    if ((sizeof(req) >> 2) > stuff->length )\
         return(BadLength)

#define REQUEST_FIXED_SIZE(req, n)\
    if (((sizeof(req) >> 2) > stuff->length) || \
        (((sizeof(req) + (n) + 3) >> 2) != stuff->length)) \
         return(BadLength)

#define LEGAL_NEW_RESOURCE(id,client)\
    if (!LegalNewID(id,client)) \
    {\
	client->errorValue = id;\
        return(BadIDChoice);\
    }

#define LOOKUP_DRAWABLE(did, client)\
    ((client->lastDrawableID == did) ? \
     client->lastDrawable : (DrawablePtr)LookupDrawable(did, client))

#define VERIFY_GC(pGC, rid, client)\
    if (client->lastGCID == rid)\
        pGC = client->lastGC;\
    else\
	pGC = (GC *)LookupIDByType(rid, RT_GC);\
    if (!pGC)\
    {\
	client->errorValue = rid;\
	return (BadGC);\
    }

#define VALIDATE_DRAWABLE_AND_GC(drawID, pDraw, pGC, client)\
    if ((stuff->gc == INVALID) || (client->lastGCID != stuff->gc) ||\
	(client->lastDrawableID != drawID))\
    {\
        if (client->lastDrawableID != drawID) {\
	    pDraw = (DrawablePtr)LookupIDByClass(drawID, RC_DRAWABLE);\
    	    if (!pDraw)\
    	    {\
            	client->errorValue = drawID; \
	    	return (BadDrawable);\
    	    }\
        } else\
	    pDraw = client->lastDrawable;\
	VERIFY_GC(pGC, stuff->gc, client);\
	if ((pDraw->type == UNDRAWABLE_WINDOW) ||\
	    (pGC->depth != pDraw->depth) ||\
	    (pGC->pScreen != pDraw->pScreen))\
	    return (BadMatch);\
	client->lastDrawable = pDraw;\
	client->lastDrawableID = drawID;\
	client->lastGC = pGC;\
	client->lastGCID = stuff->gc;\
    }\
    else\
    {\
        pGC = client->lastGC;\
        pDraw = client->lastDrawable;\
    }\
    (*miCacheFreeSlot)(pDraw);\
    if (pGC->serialNumber != pDraw->serialNumber)\
	ValidateGC(pDraw, pGC);

#define WriteReplyToClient(pClient, size, pReply) \
   if ((pClient)->swapped) \
      (*ReplySwapVector[((xReq *)(pClient)->requestBuffer)->reqType]) \
           (pClient, (int)(size), pReply); \
      else (void) WriteToClient(pClient, (int)(size), (char *)(pReply));

#define WriteSwappedDataToClient(pClient, size, pbuf) \
   if ((pClient)->swapped) \
      (*(pClient)->pSwapReplyFunc)(pClient, (int)(size), pbuf); \
   else (void) WriteToClient (pClient, (int)(size), (char *)(pbuf));

typedef struct _TimeStamp *TimeStampPtr;
typedef struct _Client *ClientPtr;

typedef struct _WorkQueue	*WorkQueuePtr;


extern ClientPtr requestingClient;
extern ClientPtr *clients;
extern ClientPtr serverClient;
extern int currentMaxClients;
extern long *checkForInput[2];

extern int ProcAllowEvents();
extern int ProcBell();
extern int ProcChangeActivePointerGrab();
extern int ProcChangeKeyboardControl();
extern int ProcChangePointerControl();
extern int ProcGetKeyboardMapping();
extern int ProcGetPointerMapping();
extern int ProcGetInputFocus();
extern int ProcGetKeyboardControl();
extern int ProcGetMotionEvents();
extern int ProcGetPointerControl();
extern int ProcGrabButton();
extern int ProcGrabKey();
extern int ProcGrabKeyboard();
extern int ProcGrabPointer();
extern int ProcQueryKeymap();
extern int ProcQueryPointer();
extern int ProcSetInputFocus();
extern int ProcSetKeyboardMapping();
extern int ProcSetPointerMapping();
extern int ProcSendEvent();
extern int ProcUngrabButton();
extern int ProcUngrabKey();
extern int ProcUngrabKeyboard();
extern int ProcUngrabPointer();
extern int ProcWarpPointer();
extern int ProcRecolorCursor();

extern WindowPtr LookupWindow();
extern pointer LookupDrawable();

extern void NoopDDA();

#endif /* DIX_H */
