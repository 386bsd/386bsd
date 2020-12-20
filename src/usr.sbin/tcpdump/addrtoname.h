/*
 * Copyright (c) 1988, 1990 The Regents of the University of California.
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
 * @(#) $Header: addrtoname.h,v 1.4 90/09/24 17:02:10 mccanne Exp $ (LBL)
 */

/* Name to address translation routines. */

extern char *etheraddr_string();
extern char *etherproto_string();
extern char *tcpport_string();
extern char *udpport_string();
extern char *getname();
extern char *intoa();

extern void init_addrtoname();
extern void no_foreign_names();

#ifndef ALIGN
#define ipaddr_string(p) getname (*(u_long *)(p))
#define ipaddr_string_bytewise(p) getname (*(u_long *)(p))
#else
#if BYTE_ORDER == LITTLE_ENDIAN
#define ipaddr_string(p) getname ((u_long)\
			  (*(u_short *)(p))|(*(((u_short *)p)+1)<<16))
#define ipaddr_string_bytewise(p)\
	getname((u_long)\
		(*(((u_char *)(p))+3)<<24 | *(((u_char *)(p))+2)<<16 | \
		 *(((u_char *)(p))+1)<<8 | *(((u_char *)(p))+0)<<0))
#else
#define ipaddr_string(p)\
	getname((u_long)\
		(((*(u_short *)(p))<<16)|*(((u_short *)p)+1)))
#define ipaddr_string_bytewise(p)\
	getname((u_long)\
		(((*(u_char *)(p))<<24)|*(((u_char *)(p))+1)<<16|\
		 *(((u_char *)p)+2)<<8|*(((u_char *)p)+3)))
#endif
#endif
