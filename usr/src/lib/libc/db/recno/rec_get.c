/*-
 * Copyright (c) 1990 The Regents of the University of California.
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
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)rec_get.c	5.10 (Berkeley) 5/16/93";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <db.h>
#include "recno.h"

/*
 * __REC_GET -- Get a record from the btree.
 *
 * Parameters:
 *	dbp:	pointer to access method
 *	key:	key to find
 *	data:	data to return
 *	flag:	currently unused
 *
 * Returns:
 *	RET_ERROR, RET_SUCCESS and RET_SPECIAL if the key not found.
 */
int
__rec_get(dbp, key, data, flags)
	const DB *dbp;
	const DBT *key;
	DBT *data;
	u_int flags;
{
	BTREE *t;
	EPG *e;
	recno_t nrec;
	int status;

	if (flags || (nrec = *(recno_t *)key->data) == 0) {
		errno = EINVAL;
		return (RET_ERROR);
	}

	/*
	 * If we haven't seen this record yet, try to find it in the
	 * original file.
	 */
	t = dbp->internal;
	if (nrec > t->bt_nrecs) {
		if (ISSET(t, R_EOF | R_INMEM))
			return (RET_SPECIAL);
		if ((status = t->bt_irec(t, nrec)) != RET_SUCCESS)
			return (status);
	}

	--nrec;
	if ((e = __rec_search(t, nrec, SEARCH)) == NULL)
		return (RET_ERROR);

	status = __rec_ret(t, e, 0, NULL, data);
	mpool_put(t->bt_mp, e->page, 0);
	return (status);
}

/*
 * __REC_FPIPE -- Get fixed length records from a pipe.
 *
 * Parameters:
 *	t:	tree
 *	cnt:	records to read
 *
 * Returns:
 *	RET_ERROR, RET_SUCCESS
 */
int
__rec_fpipe(t, top)
	BTREE *t;
	recno_t top;
{
	DBT data;
	recno_t nrec;
	size_t len;
	int ch;
	char *p;

	data.data = t->bt_dbuf;
	data.size = t->bt_reclen;

	if (t->bt_dbufsz < t->bt_reclen) {
		if ((t->bt_dbuf = realloc(t->bt_dbuf, t->bt_reclen)) == NULL)
			return (RET_ERROR);
		t->bt_dbufsz = t->bt_reclen;
	}
	for (nrec = t->bt_nrecs; nrec < top; ++nrec) {
		len = t->bt_reclen;
		for (p = t->bt_dbuf;; *p++ = ch)
			if ((ch = getc(t->bt_rfp)) == EOF || !len--) {
				if (__rec_iput(t, nrec, &data, 0)
				    != RET_SUCCESS)
					return (RET_ERROR);
				break;
			}
		if (ch == EOF)
			break;
	}
	if (nrec < top) {
		SET(t, R_EOF);
		return (RET_SPECIAL);
	}
	return (RET_SUCCESS);
}

/*
 * __REC_VPIPE -- Get variable length records from a pipe.
 *
 * Parameters:
 *	t:	tree
 *	cnt:	records to read
 *
 * Returns:
 *	RET_ERROR, RET_SUCCESS
 */
int
__rec_vpipe(t, top)
	BTREE *t;
	recno_t top;
{
	DBT data;
	recno_t nrec;
	indx_t len;
	size_t sz;
	int bval, ch;
	char *p;

	bval = t->bt_bval;
	for (nrec = t->bt_nrecs; nrec < top; ++nrec) {
		for (p = t->bt_dbuf, sz = t->bt_dbufsz;; *p++ = ch, --sz) {
			if ((ch = getc(t->bt_rfp)) == EOF || ch == bval) {
				data.data = t->bt_dbuf;
				data.size = p - t->bt_dbuf;
				if (ch == EOF && data.size == 0)
					break;
				if (__rec_iput(t, nrec, &data, 0)
				    != RET_SUCCESS)
					return (RET_ERROR);
				break;
			}
			if (sz == 0) {
				len = p - t->bt_dbuf;
				t->bt_dbufsz += (sz = 256);
				if ((t->bt_dbuf =
				    realloc(t->bt_dbuf, t->bt_dbufsz)) == NULL)
					return (RET_ERROR);
				p = t->bt_dbuf + len;
			}
		}
		if (ch == EOF)
			break;
	}
	if (nrec < top) {
		SET(t, R_EOF);
		return (RET_SPECIAL);
	}
	return (RET_SUCCESS);
}

/*
 * __REC_FMAP -- Get fixed length records from a file.
 *
 * Parameters:
 *	t:	tree
 *	cnt:	records to read
 *
 * Returns:
 *	RET_ERROR, RET_SUCCESS
 */
int
__rec_fmap(t, top)
	BTREE *t;
	recno_t top;
{
	DBT data;
	recno_t nrec;
	caddr_t sp, ep;
	size_t len;
	char *p;

	sp = t->bt_cmap;
	ep = t->bt_emap;
	data.data = t->bt_dbuf;
	data.size = t->bt_reclen;

	if (t->bt_dbufsz < t->bt_reclen) {
		if ((t->bt_dbuf = realloc(t->bt_dbuf, t->bt_reclen)) == NULL)
			return (RET_ERROR);
		t->bt_dbufsz = t->bt_reclen;
	}
	for (nrec = t->bt_nrecs; nrec < top; ++nrec) {
		if (sp >= ep) {
			SET(t, R_EOF);
			return (RET_SPECIAL);
		}
		len = t->bt_reclen;
		for (p = t->bt_dbuf; sp < ep && len--; *p++ = *sp++);
		memset(p, t->bt_bval, len);
		if (__rec_iput(t, nrec, &data, 0) != RET_SUCCESS)
			return (RET_ERROR);
	}
	t->bt_cmap = sp;
	return (RET_SUCCESS);
}

/*
 * __REC_VMAP -- Get variable length records from a file.
 *
 * Parameters:
 *	t:	tree
 *	cnt:	records to read
 *
 * Returns:
 *	RET_ERROR, RET_SUCCESS
 */
int
__rec_vmap(t, top)
	BTREE *t;
	recno_t top;
{
	DBT data;
	caddr_t sp, ep;
	recno_t nrec;
	int bval;

	sp = t->bt_cmap;
	ep = t->bt_emap;
	bval = t->bt_bval;

	for (nrec = t->bt_nrecs; nrec < top; ++nrec) {
		if (sp >= ep) {
			SET(t, R_EOF);
			return (RET_SPECIAL);
		}
		for (data.data = sp; sp < ep && *sp != bval; ++sp);
		data.size = sp - (caddr_t)data.data;
		if (__rec_iput(t, nrec, &data, 0) != RET_SUCCESS)
			return (RET_ERROR);
		++sp;
	}
	t->bt_cmap = sp;
	return (RET_SUCCESS);
}
