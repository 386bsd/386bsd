/*-
 * ISA bus conventions input / output memory physical addresses.
 *
 * Copyright (C) 1989-1994, William F. Jolitz. All Rights Reserved.
 */

#ifndef	IOM_BEGIN
#define	IOM_BEGIN	0x0a0000		/* Start of I/O Memory "hole" */
#define	IOM_END		0x100000		/* End of I/O Memory "hole" */
#define	IOM_SIZE	(IOM_END - IOM_BEGIN)
#endif	/* IOM_BEGIN */

