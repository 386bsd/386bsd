/*
 * Copyright (c) 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: sigframe.h,v 1.1 94/10/19 17:40:08 bill Exp $
 * Assembly macros.
 */

struct sigframe {
	int	sf_signum;
	int	sf_code;
	struct	sigcontext *sf_scp;
	sig_t	sf_handler;
	int	sf_eax;	
	int	sf_edx;	
	int	sf_ecx;	
	char	sf_sigcode[16];
	struct	sigcontext sf_sc;
} ;
