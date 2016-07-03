/*
 * Copyright (c) 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: rt.c,v 1.1 94/10/20 00:01:37 bill Exp $
 *
 * Dummy, do nothing routing mechanism.
 */

#include "sys/param.h"
#include "sys/socket.h"
#include "sys/errno.h"
#include "if.h"
#include "route.h"

static void
nortalloc(struct route *ro)
{
	ro->ro_rt = 0;
}

static void
nortfree(struct rtentry *rt)
{
	panic("nortfree");
}

static void
nortredirect(struct sockaddr *dst, struct sockaddr *gateway,
    struct sockaddr *netmask, int flags, struct sockaddr *src,
    struct rtentry **rtp)
{
	panic("nortredirect");
}

static int
nortioctl(int req, caddr_t data, struct proc *p)
{
#ifndef COMPAT_43
	return (EOPNOTSUPP);
#else
	return (EINVAL);
#endif
}

static int
nortrequest(int req, struct sockaddr *dst, struct sockaddr *gateway,
   struct sockaddr *netmask, int flags, struct rtentry **ret_nrt)
{
	return (EINVAL);
}

static int
nortinit(struct ifaddr *ifa, int cmd, int flags)
{
	ifa->ifa_rt = 0;
	return (0);
}

static void
nortmissmsg(int type, struct sockaddr *dst, struct sockaddr *gate,
	struct sockaddr *mask, struct sockaddr *src, int flags, int error){

	panic("nortmissmsg");
}

struct route_ops _router_ = {
	0,
	nortinit, nortalloc, nortfree,
	nortrequest, nortredirect, nortmissmsg, nortioctl
};
