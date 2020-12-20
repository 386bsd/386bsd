/*-
 * Copyright (c) 1982, 1986 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: unproto.c,v 2.2 94/07/13 18:30:58 bill Exp Locker: bill $
 */

static char *un_config = "un 1.";	/* AF_UNIX */

#include "sys/param.h"
#include "sys/socket.h"
#include "protosw.h"
#include "domain.h"
#include "mbuf.h"
#include "modconfig.h"

/*
 * Definitions of protocols supported in the UNIX domain.
 */

int	uipc_usrreq();
int	raw_init(), raw_usrreq(), raw_input(), raw_ctlinput();
extern	struct domain unixdomain;		/* or at least forward */

static struct protosw unixsw[] = {
{ SOCK_STREAM,	&unixdomain,	0,	PR_CONNREQUIRED|PR_WANTRCVD|PR_RIGHTS,
  0,		0,		0,		0,
  uipc_usrreq,
  0,		0,		0,		0,
},
{ SOCK_DGRAM,	&unixdomain,	0,		PR_ATOMIC|PR_ADDR|PR_RIGHTS,
  0,		0,		0,		0,
  uipc_usrreq,
  0,		0,		0,		0,
},
{ 0,		0,		0,		0,
  raw_input,	0,		raw_ctlinput,	0,
  raw_usrreq,
  raw_init,	0,		0,		0,
}
};

int	unp_externalize(), unp_dispose();

struct domain unixdomain =
    { 0, "unix", 0, unp_externalize, unp_dispose,
      unixsw, &unixsw[sizeof(unixsw)/sizeof(unixsw[0])] };

NETWORK_MODCONFIG() {
	char *cfg_string = un_config;

	if (config_scan(un_config, &cfg_string) == 0)
		return;

	if (cfg_number(&cfg_string, &unixdomain.dom_family) == 0)
		return;

	adddomain(&unixdomain);
}

