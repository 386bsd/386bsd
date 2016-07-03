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
 */

/*
 * This file should be merged into net/if.c.  The 'if_pcount' integer
 * field must be added to 'struct ifnet' in net/if.h.
 */

/*
 * Set/clear promiscuous mode for the hardware interface associated 
 * with 'ifp' based on the truth value of 'pswitch'.  This code
 * works only with the SunOS 3.5 drivers.
 */
ifpromisc(ifp, pswitch)
	struct ifnet *ifp;
	int pswitch;
{
	short flags = ifp->if_flags;

        /*
         * If the device is not configured up, we cannot put it in
         * promiscuous mode.
         */
        if ((flags & IFF_UP) == 0)
                return ENETDOWN;
	if (pswitch) {
		if (ifp->if_pcount++ != 0)
			return 0;
		/* turn on driver */
		flags |= IFF_PROMISC;
	} else {
		if (--ifp->if_pcount > 0)
			return 0;
		/* turn off driver */
		flags &=~ IFF_PROMISC;
	}
	return (*ifp->if_ioctl)(ifp, SIOCSIFFLAGS, (caddr_t)&flags);
}

