/*
 * Definitions that user level programs might need to know to interact
 * with serial line IP (slip) lines.

 * @(#) $Header: slip.h,v 1.4 92/02/02 17:26:02 leres Exp $ (LBL)
 *
 * Copyright (c) 1990 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/*
 * ioctl to get slip interface unit number (e.g., sl0, sl1, etc.)
 * assigned to some terminal line with a slip module pushed on it.
 */
#ifdef __STDC__
#define SLIOGUNIT _IOR('B', 1, int)
#else
#define SLIOGUNIT _IOR(B, 1, int)
#endif

/*
 * definitions of the pseudo- link-level header attached to slip
 * packets grabbed by the packet filter (bpf) traffic monitor.
 */
#define SLIP_HDRLEN 16

#define SLX_DIR 0
#define SLX_CHDR 1
#define CHDR_LEN 15

#define SLIPDIR_IN 0
#define SLIPDIR_OUT 1

