/*
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentionin
g
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
 */

#ifndef	IBMRTPC
#define	ALIGN
#endif	/* IBMRTPC */

#ifndef BYTE_ORDER
#define BYTE_ORDER BIG_ENDIAN
#endif

#ifndef NTOHL
#define NTOHL(x)
#define NTOHS(x)
#define HTONL(x)
#define HTONS(x)
#endif

#ifndef ntohl
#define	ntohl(x) (x)
#define	ntohs(x) (x)
#define	htonl(x) (x)
#define	htons(x) (x)
#endif

