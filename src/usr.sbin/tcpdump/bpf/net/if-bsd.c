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
 * Set/clear promiscuous mode on interface 'ifp' based on the truth value`
 * of pswitch.  The calls are reference counted so that only the first
 * 'on' request actually has an effect, as does the final 'off' request.
 * Results are undefined if the 'off' and 'on' requests are not matched.
 * This code works only with the BSD drivers.
 */
int
ifpromisc(ifp, pswitch)
	struct ifnet *ifp;
	int pswitch;
{
	/* 
	 * If the device is not configured up, we cannot put it in
	 * promiscuous mode.
	 */
	if ((ifp->if_flags & IFF_UP) == 0)
		return ENETDOWN;

	if (pswitch) {
		if (ifp->if_pcount++ != 0)
			return 0;
		/* turn on driver */
		ifp->if_flags |= IFF_PROMISC;
	} else {
		if (--ifp->if_pcount > 0)
			return 0;
		/* turn off driver */
		ifp->if_flags &= ~IFF_PROMISC;
	}
	/*
	 * Clear the running bit -- this tricks the driver's ioctl
	 * into reinitilizing the interface so that the promiscuous
	 * bit will have its effect.
	 */
	ifp->if_flags &=~ IFF_RUNNING;
	return (*ifp->if_ioctl)(ifp, SIOCSIFFLAGS, (caddr_t)0);
}
