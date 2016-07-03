/* tuner.c v 0.1 BETA
 * parameter calculation for Xconfig
 *
 * Copyright 1993 by Nye Liu
 * This program is distributed AS IS, under the terms and conditions
 * of the GNU Copyleft.
 *
 * based on the original by Brett Lymn (blymn@awadi.com.au)
 */

/*
                                              Front Porch          Back Porch
						|<--->|            |<------->|
 +---------------------- ~ ---------------------+-----+------------+---------+
 |                                              |   ->| SyncLength |<-       |
 |         Display Time                         |     | ~3.8us (H) |         |
 |                                              |     | ~150us (V) |         |
 +---------------------- ~ ---------------------+-----+------------+---------+
                                              Dots SyncStart    SyncEnd    Frame

*/

#include <stdio.h>

#define RoundTo8(x) ((int)(x) & ~((int)7))
#define VDtoVF 1/.95

int main()
{
  float ClockFreq, MonHFreq;
  int   HDots, VDots;
  int   HFrame, HSyncLength, HSyncStart, HSyncEnd;
  int	VFrame, VSyncLength, VSyncStart, VSyncEnd;
  int	HFrontPorch, MaxHDot;

  printf("VGA Clock Rate (MHz) ? ");
  scanf("%f",&ClockFreq);
  ClockFreq *= 1e6;
  printf("Monitor HFreq (kHz) ? ");
  scanf("%f",&MonHFreq);
  MonHFreq *= 1e3;

  HFrame = RoundTo8(0.5 + ClockFreq / MonHFreq);
  printf("estimated HFrame: %d\n", HFrame);

  HSyncLength = RoundTo8(0.5 + 3.8e-6 * ClockFreq);
  /* make room for at least a front porch of 8 and a back porch of 16 */
  MaxHDot = HFrame - HSyncLength - 24;
  printf("The recommended maximum horizontal resolution is %d\n", MaxHDot);
  printf("Exceeding this will require a lower horizontal scan rate than the one supplied\n");

  printf("Enter the desired resolution: ");
  scanf("%d",&HDots);
  HDots=RoundTo8(HDots);
  /* position H Sync Pulse */
  /* HFrontPorch is typically 1/2 HBackPorch */
  HFrontPorch = RoundTo8(0.5 + (HFrame - HSyncLength - HDots)/3.0);
  if (HFrontPorch < 8) { /* no room for reasonable porches */
      HFrontPorch = 8;
      HFrame = HDots + 3*HFrontPorch + HSyncLength;
      printf("Warning: HFrame has been enlarged to accomodate HDots + sync\n");
      printf("Required refresh rate no longer matches the one supplied\n");
  }
  HSyncStart = RoundTo8(HDots + HFrontPorch);
  HSyncEnd   = RoundTo8(HSyncStart + HSyncLength);
  printf("H refresh rate: %.1fkHz\n", ClockFreq/HFrame*1e-3);

  /* now the vertical calculations */
  printf("The recommended vertical resolution is %d\n",
      VDots = (int)(0.5 + 0.75*HDots));
#if 1
  printf("Enter the desired vertical resolution: ");
  scanf("%d",&VDots);
#endif
  VFrame = VDots * VDtoVF;
  VSyncLength = (int)(0.5 + 150.0e-6 * ClockFreq / HFrame);
  VSyncStart = VDots + 1;  /* give it a pixel's worth of front porch */
  VSyncEnd = VSyncStart + VSyncLength;
  printf("V refresh rate: %.1f Hz\n",ClockFreq/HFrame/VFrame);
  printf("Horizontal front porch: %.2f us\n", HFrontPorch/ClockFreq*1e6);
  printf("The parameters are as follows:\n");
  printf(" \"%dx%d\"     %.0f     %4d %4d %4d %4d   %4d %4d %4d %4d\n",
  				     HDots, VDots, ClockFreq/1e6,
				     HDots, HSyncStart, HSyncEnd, HFrame,
                                     VDots, VSyncStart, VSyncEnd, VFrame);
  return(0);
}
