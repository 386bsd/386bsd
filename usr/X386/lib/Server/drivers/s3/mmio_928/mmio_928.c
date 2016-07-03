/*
 * Copyright 1993 by David Dawes <dawes@physics.su.oz.au>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of David Dawes 
 * not be used in advertising or publicity pertaining to distribution of 
 * the software without specific, written prior permission.
 * David Dawes makes no representations about the suitability of this 
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * DAVID DAWES DISCLAIMS ALL WARRANTIES WITH REGARD TO 
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND 
 * FITNESS, IN NO EVENT SHALL DAVID DAWES BE LIABLE FOR 
 * ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER 
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF 
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

/* $XFree86: mit/server/ddx/x386/accel/s3/drivers/mmio_928/mmio_928.c,v 2.3 1993/09/29 11:11:23 dawes Exp $ */

#include "s3.h"
#include "regs3.h"

static Bool MMIO_928Probe();
static char *MMIO_928Ident();
extern void mmio928_s3EnterLeaveVT();
extern Bool mmio928_s3Initialize();
extern void mmio928_s3AdjustFrame();
extern Bool mmio928_s3SwitchMode();
extern int s3ChipId;
extern Bool s3Mmio928;

/*
 * If 'volatile' isn't available, we disable the MMIO code 
 */

#if defined(__STDC__) || defined(__GNUC__)
#define HAVE_VOLATILE
#endif

s3VideoChipRec MMIO_928 = {
  MMIO_928Probe,
  MMIO_928Ident,
#ifdef HAVE_VOLATILE
  mmio928_s3EnterLeaveVT,
  mmio928_s3Initialize,
  mmio928_s3AdjustFrame,
  mmio928_s3SwitchMode,
#else
  NULL,
  NULL,
  NULL,
  NULL,
#endif
};

static char *
MMIO_928Ident(n)
     int n;
{
#ifdef HAVE_VOLATILE
   static char *chipsets[] = {"mmio_928"};

   if (n + 1 > sizeof(chipsets) / sizeof(char *))
      return(NULL);
   else
      return(chipsets[n]);
#else
   return(NULL);
#endif
}


static Bool
MMIO_928Probe()
{
#ifdef HAVE_VOLATILE
   if (s3InfoRec.chipset) {
      if (StrCaseCmp(s3InfoRec.chipset, MMIO_928Ident(0)))
	 return(FALSE);
      else {
	 s3Mmio928 = TRUE;
	 return(TRUE);
      }
   }

   if (S3_928_SERIES(s3ChipId)) {
      s3InfoRec.chipset = MMIO_928Ident(0);
      s3Mmio928 = TRUE;
      return(TRUE);
   } else {
      return(FALSE);
   }
#else
   return(FALSE);
#endif
}
