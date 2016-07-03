/*-
 * ISA RAM Physical Address Space (ignoring the I/O memory "hole")
 *
 * Copyright (C) 1989-1994, William F. Jolitz. All Rights Reserved.
 */

#ifndef	ISA_RAM_BEGIN
#define	ISA_RAM_BEGIN	0x0000000	/* Start of RAM Memory */
#define	ISA_RAM_END	0x1000000	/* End of RAM Memory */
#define	ISA_RAM_SIZE	(ISA_RAM_END - ISA_RAM_BEGIN)
#endif	/* ISA_RAM_BEGIN */
