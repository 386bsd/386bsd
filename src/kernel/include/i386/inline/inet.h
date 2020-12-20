/*
 * Copyright (c) 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: inet.h,v 1.1 94/06/09 18:11:09 bill Exp Locker: bill $
 * This file contains potential inline procedures that implement functions
 * that are used by the inet network protocols.
 *
 * These procedures can be "non" inlined to allow for debugging,
 * profiling, and tracing.
 */

/*__BEGIN_DECLS
__END_DECLS */

#ifndef __NO_INLINES

#undef	__INLINE
#ifndef __NO_INLINES_BUT_EMIT_CODE
#define	__INLINE	extern inline
#else
#define	__INLINE
#endif

#ifndef KERNEL
#include <machine/inline/inet/htonl.h>
#include <machine/inline/inet/htons.h>
#include <machine/inline/inet/ntohl.h>
#include <machine/inline/inet/ntohs.h>
#else
#include "machine/inline/inet/htonl.h"
#include "machine/inline/inet/htons.h"
#include "machine/inline/inet/ntohl.h"
#include "machine/inline/inet/ntohs.h"

u_short in_cksumiphdr(void *ip);

#include "machine/inline/inet/in_cksumiphdr.h"
#endif

#undef	__INLINE
#endif
