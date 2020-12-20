/*-
 * ISA Bus Driver specification.
 *
 * Copyright (C) 1989-1994, William F. Jolitz. All Rights Reserved.
 *
 * $Id$
 */

/*
 * Per-device structure.
 */
struct isa_device {
	struct	isa_driver *id_driver;
	short	id_iobase;	/* base i/o address */
	short	id_irq;		/* interrupt request */
	short	id_drq;		/* DMA request */
	caddr_t id_maddr;	/* physical i/o memory address on bus (if any)*/
	int	id_msize;	/* size of i/o memory */
	int	id_unit;	/* unit number */
	int	id_alive;	/* device is present */
	int	id_flags;	/* private device configuration flags */
};

/*
 * Per-driver structure.
 *
 * Each device driver defines entries for a set of routines
 * as well as an array of types which are acceptable to it.
 * These are used at boot time by the configuration service.
 */
struct isa_driver {
	/* test whether device is present */
	int	(*probe)(struct isa_device *);
	/* setup driver for a device */
	void	(*attach)(struct isa_device *);
	void	(*intr)(int);		/* driver interrupt */
	char	*name;			/* driver name */
	int	*mask;			/* spl group (if not zero) */
};

/* interface symbols */
#define	__ISYM_VERSION__ "1"	/* XXX RCS major revision number of hdr file */
#include "isym.h"		/* this header has interface symbols */

/* functions used in core kernel and modules */
__ISYM__(void, new_isa_configure, (char **, struct isa_driver *))

#undef __ISYM__
#undef __ISYM_ALIAS__
#undef __ISYM_VERSION__
