/*
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @(#) $Header$ (LBL)
 */

#ifdef ALIGN
#if BYTEORDER == LITTLE_ENDIAN
#define EXTRACT_SHORT(p)\
	((u_short)\
		(*((u_char *)p+1)<<8|\
		 *((u_char *)p+0)<<0))
#define EXTRACT_LONG(p)\
		(*((u_char *)p+3)<<24|\
		 *((u_char *)p+2)<<16|\
		 *((u_char *)p+1)<<8|\
		 *((u_char *)p+0)<<0)
#else
#define EXTRACT_SHORT(p)\
	((u_short)\
		(*((u_char *)p+0)<<8|\
		 *((u_char *)p+1)<<0))
#define EXTRACT_LONG(p)\
		(*((u_char *)p+0)<<24|\
		 *((u_char *)p+1)<<16|\
		 *((u_char *)p+2)<<8|\
		 *((u_char *)p+3)<<0)
#endif
#else
#define EXTRACT_SHORT(p)	(ntohs(*(u_short *)p))
#define EXTRACT_LONG(p)		(ntohl(*(u_long *)p))
#endif
