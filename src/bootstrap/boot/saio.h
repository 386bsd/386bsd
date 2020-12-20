/*
 * Copyright (c) 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: saio.h,v 1.1 94/10/20 16:45:33 root Exp $
 *
 * Mock iob.
 */

/*
 */
struct iob {
	int i_dev;
	int i_adapt;
	int i_ctlr;
	int i_part;
	int i_unit;
	int i_cc;
	int i_bn;
	int i_ma;
	int i_boff;
	int i_flgs;
#define	F_WRITE	0x1
#define	F_FILE	0x2
#define	F_READ	0x4
};

#define	BBSIZE	8192
