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
 * @(#) $Header: os-sunos4.h,v 1.7 90/09/21 02:09:58 mccanne Exp $ (LBL)
 */

#define ETHER_SERVICE
u_char *ETHER_hostton();
char *ETHER_ntohost();

/* Map things in the ether_arp struct */
#define SHA(ap) ((ap)->arp_sha.ether_addr_octet)
#define SPA(ap) ((ap)->arp_spa)
#define THA(ap) ((ap)->arp_tha.ether_addr_octet)
#define TPA(ap) ((ap)->arp_tpa)

#define EDST(ep) ((ep)->ether_dhost.ether_addr_octet)
#define ESRC(ep) ((ep)->ether_shost.ether_addr_octet)

/* Map protocol types */
#define ETHERPUP_IPTYPE ETHERTYPE_IP
#define ETHERPUP_REVARPTYPE ETHERTYPE_REVARP
#define ETHERPUP_ARPTYPE ETHERTYPE_ARP
