/*
 * Copyright (c) 1994 William F. Jolitz, TeleMuse
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
 *	This software is a component of "386BSD" developed by 
 *	William F. Jolitz, TeleMuse.
 * 4. Neither the name of the developer nor the name "386BSD"
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 5. Non-commercial distribution of this complete file in either source and/or
 *    binary form at no charge to the user (such as from an official Internet 
 *    archive site) is permitted. 
 * 6. Commercial distribution and sale of this complete file in either source
 *    and/or binary form on any media, including that of floppies, tape, or 
 *    CD-ROM, or through a per-charge download such as that of a BBS, is not 
 *    permitted without specific prior written permission.
 * 7. Non-commercial and/or commercial distribution of an incomplete, altered, 
 *    or otherwise modified file in either source and/or binary form is not 
 *    permitted.
 *
 * THIS SOFTWARE IS A COMPONENT OF 386BSD DEVELOPED BY WILLIAM F. JOLITZ 
 * AND IS INTENDED FOR RESEARCH AND EDUCATIONAL PURPOSES ONLY. THIS 
 * SOFTWARE SHOULD NOT BE CONSIDERED TO BE A COMMERCIAL PRODUCT. 
 * THE DEVELOPER URGES THAT USERS WHO REQUIRE A COMMERCIAL PRODUCT 
 * NOT MAKE USE OF THIS WORK.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPER ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPER BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: esym.c,v 1.1 94/10/19 18:33:20 bill Exp $
 *
 * External symbol table (esym):
 *  name space of symbols visable globally to modules. Symbols are
 *  defined on one list (_esym_elist_), and referenced on another (_esym_llist).
 *  Symbols must be unbound before being rebound differently.
 */

#include "esym.h"

struct _esym_entry_ *_esym_elist_, *_esym_llist_;

/* find missing symbol */
const void *_esymtab_search_(struct _esym_entry_ *es) {
	register struct _esym_entry_ *ss = _esym_elist_;

	/* locate the symbol */
	for (; ss && strcmp(es->es_name, ss->es_name) != 0 ; ss = ss->es_next)
		;

	/* if found, add lookup instance to lookup list */
	if (ss) {
		es->es_next = _esym_llist_;
		_esym_llist_ = es;

		/* return located address */
		return (ss->es_address);
	}

	/* otherwise return zero */
	else
		return (0);
}

/* remove a symbol from the external symbol table, so it can be rebound. */
void
_esymtab_unbind_(const char *name) {
	register struct _esym_entry_ *ss, *os, *es;

	/* locate the symbol on the existance instance list */
	os = 0;
	for (ss = _esym_elist_; ss ; ss = ss->es_next)

		/* if found, delete existance instance from existance list */
		if (strcmp(name, ss->es_name) == 0) {
			if (os == 0)
				_esym_elist_ = ss->es_next;
			else
				os->es_next = ss->es_next;
			break;
		} else
			os = ss;

	/* if found, locate the symbol on the lookup instance list */
	os = 0;
	if (ss == 0)
		return;
	else
		es = ss;
	for (ss = _esym_llist_; ss ; ss = ss->es_next)

		/* if found, delete lookup instance from lookup list */
		if (es->es_address == ss->es_address) {

#ifdef DIAGNOSTIC
			if (strcmp(es->es_name, ss->es_name) != 0)
				panic("esym_unbind: duplicate address");
#endif

			if (os == 0)
				_esym_llist_ = ss->es_next;
			else
				os->es_next = ss->es_next;
		} else
			os = ss;

#ifdef DIAGNOSTIC
	_esym_dump_();
	_esym_check_(es);
#endif
}

#ifdef DIAGNOSTIC
void
_esym_dump_(void) {
	struct _esym_entry_ *ss;

	printf("existance: ");
	for (ss = _esym_elist_; ss ; ss = ss->es_next)
		printf("%x:%s:%x ", ss, ss->es_name, ss->es_address);
	printf("\n");

	printf("lookup: ");
	for (ss = _esym_llist_; ss ; ss = ss->es_next)
		printf("%x:%s:%x ", ss, ss->es_name, ss->es_address);
	printf("\n");
}

void
_esym_check_(struct _esym_entry_ *os) {
	struct _esym_entry_ *ss, *es;

	
	for (es = _esym_elist_; es ; es = es->es_next) {

		/* check for aliases */
		for (ss = _esym_elist_; ss ; ss = ss->es_next) {
			if (ss == es)
				continue;
			if (strcmp(es->es_name, ss->es_name) == 0)
				panic("esym_check: duplicate name on elist");
			if (es->es_address == ss->es_address)
				panic("esym_check: duplicate address on elist");
		}

		/* check optional argument */
		if (os && strcmp(es->es_name, os->es_name) == 0)
		 	panic("esym_check: name on elist");
		if (os && es->es_address == os->es_address)
			panic("esym_check: address on elist");
	}

	for (es = _esym_llist_; es ; es = es->es_next) {

		/* check optional argument */
		if (os && strcmp(es->es_name, os->es_name) == 0)
		 	panic("esym_check: name on llist");
		if (os && es->es_address == os->es_address)
			panic("esym_check: address on llist");
	}
}
#endif
