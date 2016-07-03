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
 * $Id: $
 *
 * External symbol table (esym):
 *  name space of symbols visable globally to modules. Symbols are
 *  defined on one list (_esym_elist_), and referenced on another (_esym_llist).
 *  Symbols must be unbound before being rebound differently.
 */

/* both lookup and existance instances use the same entries */
struct _esym_entry_ {
	const char *es_name;
	const void *es_address;
	struct _esym_entry_ *es_next;
} *_esym_elist_, *_esym_llist_;

#ifdef KERNEL
extern struct _esym_entry_ *_esym_elist_, *_esym_llist_;

/* hidden symbol search and unbind functions */ 
const void *_esymtab_search_(struct _esym_entry_ *es);
void _esymtab_unbind_(const char *name);
void _esym_check_(struct _esym_entry_ *os);
void _esym_dump_(void);
#endif /* KERNEL */

/* enter new symbol into the external symbol table into the kernel */
#define esym_bind(s) { \
	static struct _esym_entry_  __instance__ = { "" # s, & s, }; \
	__instance__.es_next = _esym_elist_; \
	_esym_elist_ = &__instance__; \
}

/* delete all references to the symbol from the external symbol table */
#define esym_unbind(s) { \
	_esymtab_unbind_( "" # s ); \
}

/* fetch address symbolically */
#define esym_fetch(s) ({ \
	static struct _esym_entry_  __instance__ = { "" # s, 0, }; \
	if (__instance__.es_address == 0) \
		__instance__.es_address = _esymtab_search_(&__instance__); \
	__instance__.es_address; \
})
