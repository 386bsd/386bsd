/*
 * Copyright (c) 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: string.h,v 1.1 94/06/09 18:13:38 bill Exp $
 * This file contains potential inline procedures that implement
 * string functions.
 *
 * These procedures can be "non" inlined to allow for debugging,
 * profiling, tracing, and incorporation in the C library.
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

#ifdef KERNEL
/* #include "machine/inline/string/ffs.h" */
/* #include "machine/inline/string/memcmp.h" */
#include "machine/inline/string/memcpy.h"
#include "machine/inline/string/memset.h"
/* #include "machine/inline/string/memmove.h" */
/* #include "machine/inline/string/strlen.h" */
#else
#include <machine/inline/string/bcopy.h>
#include <machine/inline/string/bcmp.h>
#include <machine/inline/string/bzero.h>
#include <machine/inline/string/ffs.h>
#include <machine/inline/string/memcmp.h>
#include <machine/inline/string/memcpy.h>
#include <machine/inline/string/memset.h>
#include <machine/inline/string/memmove.h>
#include <machine/inline/string/strlen.h>
#endif

#undef	__INLINE
#endif
