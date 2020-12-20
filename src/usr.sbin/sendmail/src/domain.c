/*
 * Copyright (c) 1986 Eric P. Allman
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 */

#include "sendmail.h"

#ifndef lint
#if NAMED_BIND
static char sccsid[] = "@(#)domain.c	8.19 (Berkeley) 3/11/94 (with name server)";
#else
static char sccsid[] = "@(#)domain.c	8.19 (Berkeley) 3/11/94 (without name server)";
#endif
#endif /* not lint */

#if NAMED_BIND

#include <errno.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <netdb.h>

typedef union
{
	HEADER	qb1;
	char	qb2[PACKETSZ];
} querybuf;

static char	MXHostBuf[MAXMXHOSTS*PACKETSZ];

#ifndef MAXDNSRCH
#define MAXDNSRCH	6	/* number of possible domains to search */
#endif

#ifndef MAX
#define MAX(a, b)	((a) > (b) ? (a) : (b))
#endif

#ifndef NO_DATA
# define NO_DATA	NO_ADDRESS
#endif

#ifndef HEADERSZ
# define HEADERSZ	sizeof(HEADER)
#endif

/* don't use sizeof because sizeof(long) is different on 64-bit machines */
#define SHORTSIZE	2	/* size of a short (really, must be 2) */
#define LONGSIZE	4	/* size of a long (really, must be 4) */

#define MAXCNAMEDEPTH	10	/* maximum depth of CNAME recursion */
/*
**  GETMXRR -- get MX resource records for a domain
**
**	Parameters:
**		host -- the name of the host to MX.
**		mxhosts -- a pointer to a return buffer of MX records.
**		droplocalhost -- If TRUE, all MX records less preferred
**			than the local host (as determined by $=w) will
**			be discarded.
**		rcode -- a pointer to an EX_ status code.
**
**	Returns:
**		The number of MX records found.
**		-1 if there is an internal failure.
**		If no MX records are found, mxhosts[0] is set to host
**			and 1 is returned.
*/

getmxrr(host, mxhosts, droplocalhost, rcode)
	char *host;
	char **mxhosts;
	bool droplocalhost;
	int *rcode;
{
	extern int h_errno;
	register u_char *eom, *cp;
	register int i, j, n;
	int nmx = 0;
	register char *bp;
	HEADER *hp;
	querybuf answer;
	int ancount, qdcount, buflen;
	bool seenlocal = FALSE;
	u_short pref, localpref, type;
	char *fallbackMX = FallBackMX;
	static bool firsttime = TRUE;
	STAB *st;
	bool trycanon = FALSE;
	u_short prefer[MAXMXHOSTS];
	int weight[MAXMXHOSTS];
	extern bool getcanonname();

	if (tTd(8, 2))
		printf("getmxrr(%s, droplocalhost=%d)\n", host, droplocalhost);

	if (fallbackMX != NULL)
	{
		if (firsttime && res_query(FallBackMX, C_IN, T_A,
					   (char *) &answer, sizeof answer) < 0)
		{
			/* this entry is bogus */
			fallbackMX = FallBackMX = NULL;
		}
		else if (droplocalhost &&
			 (st = stab(fallbackMX, ST_CLASS, ST_FIND)) != NULL &&
			 bitnset('w', st->s_class))
		{
			/* don't use fallback for this pass */
			fallbackMX = NULL;
		}
		firsttime = FALSE;
	}

	/* efficiency hack -- numeric or non-MX lookups */
	if (host[0] == '[')
		goto punt;

	errno = 0;
	n = res_search(host, C_IN, T_MX, (char *)&answer, sizeof(answer));
	if (n < 0)
	{
		if (tTd(8, 1))
			printf("getmxrr: res_search(%s) failed (errno=%d, h_errno=%d)\n",
			    (host == NULL) ? "<NULL>" : host, errno, h_errno);
		switch (h_errno)
		{
		  case NO_DATA:
			trycanon = TRUE;
			/* fall through */

		  case NO_RECOVERY:
			/* no MX data on this host */
			goto punt;

		  case HOST_NOT_FOUND:
#ifdef BROKEN_RES_SEARCH
			/* Ultrix resolver returns failure w/ h_errno=0 */
		  case 0:
#endif
			/* the host just doesn't exist */
			*rcode = EX_NOHOST;

			if (!UseNameServer)
			{
				/* might exist in /etc/hosts */
				goto punt;
			}
			break;

		  case TRY_AGAIN:
			/* couldn't connect to the name server */
			if (!UseNameServer && errno == ECONNREFUSED)
				goto punt;

			/* it might come up later; better queue it up */
			*rcode = EX_TEMPFAIL;
			break;

		  default:
			syserr("getmxrr: res_search (%s) failed with impossible h_errno (%d)\n",
				host, h_errno);
			*rcode = EX_OSERR;
			break;
		}

		/* irreconcilable differences */
		return (-1);
	}

	/* find first satisfactory answer */
	hp = (HEADER *)&answer;
	cp = (u_char *)&answer + HEADERSZ;
	eom = (u_char *)&answer + n;
	for (qdcount = ntohs(hp->qdcount); qdcount--; cp += n + QFIXEDSZ)
		if ((n = dn_skipname(cp, eom)) < 0)
			goto punt;
	buflen = sizeof(MXHostBuf) - 1;
	bp = MXHostBuf;
	ancount = ntohs(hp->ancount);
	while (--ancount >= 0 && cp < eom && nmx < MAXMXHOSTS - 1)
	{
		if ((n = dn_expand((u_char *)&answer,
		    eom, cp, (u_char *)bp, buflen)) < 0)
			break;
		cp += n;
		GETSHORT(type, cp);
 		cp += SHORTSIZE + LONGSIZE;
		GETSHORT(n, cp);
		if (type != T_MX)
		{
			if (tTd(8, 8) || _res.options & RES_DEBUG)
				printf("unexpected answer type %d, size %d\n",
				    type, n);
			cp += n;
			continue;
		}
		GETSHORT(pref, cp);
		if ((n = dn_expand((u_char *)&answer, eom, cp,
				   (u_char *)bp, buflen)) < 0)
			break;
		cp += n;
		if (droplocalhost &&
		    (st = stab(bp, ST_CLASS, ST_FIND)) != NULL &&
		    bitnset('w', st->s_class))
		{
			if (tTd(8, 3))
				printf("found localhost (%s) in MX list, pref=%d\n",
					bp, pref);
			if (!seenlocal || pref < localpref)
				localpref = pref;
			seenlocal = TRUE;
			continue;
		}
		weight[nmx] = mxrand(bp);
		prefer[nmx] = pref;
		mxhosts[nmx++] = bp;
		n = strlen(bp);
		bp += n;
		if (bp[-1] != '.')
		{
			*bp++ = '.';
			n++;
		}
		*bp++ = '\0';
		buflen -= n + 1;
	}

	/* sort the records */
	for (i = 0; i < nmx; i++)
	{
		for (j = i + 1; j < nmx; j++)
		{
			if (prefer[i] > prefer[j] ||
			    (prefer[i] == prefer[j] && weight[i] > weight[j]))
			{
				register int temp;
				register char *temp1;

				temp = prefer[i];
				prefer[i] = prefer[j];
				prefer[j] = temp;
				temp1 = mxhosts[i];
				mxhosts[i] = mxhosts[j];
				mxhosts[j] = temp1;
				temp = weight[i];
				weight[i] = weight[j];
				weight[j] = temp;
			}
		}
		if (seenlocal && prefer[i] >= localpref)
		{
			/* truncate higher preference part of list */
			nmx = i;
		}
	}

	if (nmx == 0)
	{
punt:
		if (seenlocal &&
		    (!TryNullMXList || gethostbyname(host) == NULL))
		{
			/*
			**  If we have deleted all MX entries, this is
			**  an error -- we should NEVER send to a host that
			**  has an MX, and this should have been caught
			**  earlier in the config file.
			**
			**  Some sites prefer to go ahead and try the
			**  A record anyway; that case is handled by
			**  setting TryNullMXList.  I believe this is a
			**  bad idea, but it's up to you....
			*/

			*rcode = EX_CONFIG;
			syserr("MX list for %s points back to %s",
				host, MyHostName);
			return -1;
		}
		strcpy(MXHostBuf, host);
		mxhosts[0] = MXHostBuf;
		if (host[0] == '[')
		{
			register char *p;

			/* this may be an MX suppression-style address */
			p = strchr(MXHostBuf, ']');
			if (p != NULL)
			{
				*p = '\0';
				if (inet_addr(&MXHostBuf[1]) != -1)
					*p = ']';
				else
				{
					trycanon = TRUE;
					mxhosts[0]++;
				}
			}
		}
		if (trycanon &&
		    getcanonname(mxhosts[0], sizeof MXHostBuf - 2, FALSE))
		{
			bp = &MXHostBuf[strlen(MXHostBuf)];
			if (bp[-1] != '.')
			{
				*bp++ = '.';
				*bp = '\0';
			}
		}
		nmx = 1;
	}

	/* if we have a default lowest preference, include that */
	if (fallbackMX != NULL && !seenlocal)
		mxhosts[nmx++] = fallbackMX;

	return (nmx);
}
/*
**  MXRAND -- create a randomizer for equal MX preferences
**
**	If two MX hosts have equal preferences we want to randomize
**	the selection.  But in order for signatures to be the same,
**	we need to randomize the same way each time.  This function
**	computes a pseudo-random hash function from the host name.
**
**	Parameters:
**		host -- the name of the host.
**
**	Returns:
**		A random but repeatable value based on the host name.
**
**	Side Effects:
**		none.
*/

mxrand(host)
	register char *host;
{
	int hfunc;
	static unsigned int seed;

	if (seed == 0)
	{
		seed = (int) curtime() & 0xffff;
		if (seed == 0)
			seed++;
	}

	if (tTd(17, 9))
		printf("mxrand(%s)", host);

	hfunc = seed;
	while (*host != '\0')
	{
		int c = *host++;

		if (isascii(c) && isupper(c))
			c = tolower(c);
		hfunc = ((hfunc << 1) ^ c) % 2003;
	}

	hfunc &= 0xff;

	if (tTd(17, 9))
		printf(" = %d\n", hfunc);
	return hfunc;
}
/*
**  GETCANONNAME -- get the canonical name for named host
**
**	This algorithm tries to be smart about wildcard MX records.
**	This is hard to do because DNS doesn't tell is if we matched
**	against a wildcard or a specific MX.
**	
**	We always prefer A & CNAME records, since these are presumed
**	to be specific.
**
**	If we match an MX in one pass and lose it in the next, we use
**	the old one.  For example, consider an MX matching *.FOO.BAR.COM.
**	A hostname bletch.foo.bar.com will match against this MX, but
**	will stop matching when we try bletch.bar.com -- so we know
**	that bletch.foo.bar.com must have been right.  This fails if
**	there was also an MX record matching *.BAR.COM, but there are
**	some things that just can't be fixed.
**
**	Parameters:
**		host -- a buffer containing the name of the host.
**			This is a value-result parameter.
**		hbsize -- the size of the host buffer.
**		trymx -- if set, try MX records as well as A and CNAME.
**
**	Returns:
**		TRUE -- if the host matched.
**		FALSE -- otherwise.
*/

bool
getcanonname(host, hbsize, trymx)
	char *host;
	int hbsize;
	bool trymx;
{
	extern int h_errno;
	register u_char *eom, *ap;
	register char *cp;
	register int n; 
	HEADER *hp;
	querybuf answer;
	int ancount, qdcount;
	int ret;
	char **domain;
	int type;
	char **dp;
	char *mxmatch;
	bool amatch;
	bool gotmx;
	int qtype;
	int loopcnt;
	char *xp;
	char nbuf[MAX(PACKETSZ, MAXDNAME*2+2)];
	char *searchlist[MAXDNSRCH+2];
	extern char *gethostalias();

	if (tTd(8, 2))
		printf("getcanonname(%s)\n", host);

	if ((_res.options & RES_INIT) == 0 && res_init() == -1)
		return (FALSE);

	/*
	**  Initialize domain search list.  If there is at least one
	**  dot in the name, search the unmodified name first so we
	**  find "vse.CS" in Czechoslovakia instead of in the local
	**  domain (e.g., vse.CS.Berkeley.EDU).
	**
	**  Older versions of the resolver could create this
	**  list by tearing apart the host name.
	*/

	loopcnt = 0;
cnameloop:
	for (cp = host, n = 0; *cp; cp++)
		if (*cp == '.')
			n++;

	if (n == 0 && (xp = gethostalias(host)) != NULL)
	{
		if (loopcnt++ > MAXCNAMEDEPTH)
		{
			syserr("loop in ${HOSTALIASES} file");
		}
		else
		{
			strncpy(host, xp, hbsize);
			host[hbsize - 1] = '\0';
			goto cnameloop;
		}
	}

	dp = searchlist;
	if (n > 0)
		*dp++ = "";
	if (n >= 0 && *--cp != '.' && bitset(RES_DNSRCH, _res.options))
	{
		for (domain = _res.dnsrch; *domain != NULL; )
			*dp++ = *domain++;
	}
	else if (n == 0 && bitset(RES_DEFNAMES, _res.options))
	{
		*dp++ = _res.defdname;
	}
	else if (*cp == '.')
	{
		*cp = '\0';
	}
	*dp = NULL;

	/*
	**  Now run through the search list for the name in question.
	*/

	mxmatch = NULL;
	qtype = T_ANY;

	for (dp = searchlist; *dp != NULL; )
	{
		if (qtype == T_ANY)
			gotmx = FALSE;
		if (tTd(8, 5))
			printf("getcanonname: trying %s.%s (%s)\n", host, *dp,
				qtype == T_ANY ? "ANY" : qtype == T_A ? "A" :
				qtype == T_MX ? "MX" : "???");
		ret = res_querydomain(host, *dp, C_IN, qtype,
				      &answer, sizeof(answer));
		if (ret <= 0)
		{
			if (tTd(8, 7))
				printf("\tNO: errno=%d, h_errno=%d\n",
					errno, h_errno);

			if (errno == ECONNREFUSED || h_errno == TRY_AGAIN)
			{
				/* the name server seems to be down */
				h_errno = TRY_AGAIN;
				return FALSE;
			}

			if (h_errno != HOST_NOT_FOUND)
			{
				/* might have another type of interest */
				if (qtype == T_ANY)
				{
					qtype = T_A;
					continue;
				}
				else if (qtype == T_A && !gotmx && trymx)
				{
					qtype = T_MX;
					continue;
				}
			}

			if (mxmatch != NULL)
			{
				/* we matched before -- use that one */
				break;
			}

			/* otherwise, try the next name */
			dp++;
			qtype = T_ANY;
			continue;
		}
		else if (tTd(8, 7))
			printf("\tYES\n");

		/*
		**  This might be a bogus match.  Search for A or
		**  CNAME records.  If we don't have a matching
		**  wild card MX record, we will accept MX as well.
		*/

		hp = (HEADER *) &answer;
		ap = (u_char *) &answer + HEADERSZ;
		eom = (u_char *) &answer + ret;

		/* skip question part of response -- we know what we asked */
		for (qdcount = ntohs(hp->qdcount); qdcount--; ap += ret + QFIXEDSZ)
		{
			if ((ret = dn_skipname(ap, eom)) < 0)
			{
				if (tTd(8, 20))
					printf("qdcount failure (%d)\n",
						ntohs(hp->qdcount));
				return FALSE;		/* ???XXX??? */
			}
		}

		amatch = FALSE;
		for (ancount = ntohs(hp->ancount); --ancount >= 0 && ap < eom; ap += n)
		{
			n = dn_expand((u_char *) &answer, eom, ap,
				      (u_char *) nbuf, sizeof nbuf);
			if (n < 0)
				break;
			ap += n;
			GETSHORT(type, ap);
			ap += SHORTSIZE + LONGSIZE;
			GETSHORT(n, ap);
			switch (type)
			{
			  case T_MX:
				gotmx = TRUE;
				if (**dp != '\0')
				{
					/* got a match -- save that info */
					if (trymx && mxmatch == NULL)
						mxmatch = *dp;
					continue;
				}

				/* exact MX matches are as good as an A match */
				/* fall through */

			  case T_A:
				/* good show */
				amatch = TRUE;

				/* continue in case a CNAME also exists */
				continue;

			  case T_CNAME:
				if (loopcnt++ > MAXCNAMEDEPTH)
				{
					/*XXX should notify postmaster XXX*/
					message("DNS failure: CNAME loop for %s",
						host);
					if (CurEnv->e_message == NULL)
					{
						char ebuf[MAXLINE];

						sprintf(ebuf, "Deferred: DNS failure: CNAME loop for %s",
							host);
						CurEnv->e_message = newstr(ebuf);
					}
					h_errno = NO_RECOVERY;
					return FALSE;
				}

				/* value points at name */
				if ((ret = dn_expand((u_char *)&answer,
				    eom, ap, (u_char *)nbuf, sizeof(nbuf))) < 0)
					break;
				(void)strncpy(host, nbuf, hbsize); /* XXX */
				host[hbsize - 1] = '\0';

				/*
				**  RFC 1034 section 3.6 specifies that CNAME
				**  should point at the canonical name -- but
				**  urges software to try again anyway.
				*/

				goto cnameloop;

			  default:
				/* not a record of interest */
				continue;
			}
		}

		if (amatch)
		{
			/* got an A record and no CNAME */
			mxmatch = *dp;
			break;
		}

		/*
		**  If this was a T_ANY query, we may have the info but
		**  need an explicit query.  Try T_A, then T_MX.
		*/

		if (qtype == T_ANY)
			qtype = T_A;
		else if (qtype == T_A && !gotmx && trymx)
			qtype = T_MX;
		else
		{
			/* really nothing in this domain; try the next */
			qtype = T_ANY;
			dp++;
		}
	}

	if (mxmatch == NULL)
		return FALSE;

	/* create matching name and return */
	(void) sprintf(nbuf, "%.*s%s%.*s", MAXDNAME, host,
			*mxmatch == '\0' ? "" : ".",
			MAXDNAME, mxmatch);
	strncpy(host, nbuf, hbsize);
	host[hbsize - 1] = '\0';
	return TRUE;
}


char *
gethostalias(host)
	char *host;
{
	char *fname;
	FILE *fp;
	register char *p;
	char buf[MAXLINE];
	static char hbuf[MAXDNAME];

	fname = getenv("HOSTALIASES");
	if (fname == NULL || (fp = fopen(fname, "r")) == NULL)
		return NULL;
	while (fgets(buf, sizeof buf, fp) != NULL)
	{
		for (p = buf; p != '\0' && !(isascii(*p) && isspace(*p)); p++)
			continue;
		if (*p == 0)
		{
			/* syntax error */
			continue;
		}
		*p++ = '\0';
		if (strcasecmp(buf, host) == 0)
			break;
	}

	if (feof(fp))
	{
		/* no match */
		fclose(fp);
		return NULL;
	}

	/* got a match; extract the equivalent name */
	while (*p != '\0' && isascii(*p) && isspace(*p))
		p++;
	host = p;
	while (*p != '\0' && !(isascii(*p) && isspace(*p)))
		p++;
	*p = '\0';
	strncpy(hbuf, host, sizeof hbuf - 1);
	hbuf[sizeof hbuf - 1] = '\0';
	return hbuf;
}


#else /* not NAMED_BIND */

#include <netdb.h>

bool
getcanonname(host, hbsize, trymx)
	char *host;
	int hbsize;
	bool trymx;
{
	struct hostent *hp;

	hp = gethostbyname(host);
	if (hp == NULL)
		return (FALSE);

	if (strlen(hp->h_name) >= hbsize)
		return (FALSE);

	(void) strcpy(host, hp->h_name);
	return (TRUE);
}

#endif /* not NAMED_BIND */
