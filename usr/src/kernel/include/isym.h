/*
 * Copyright (c) 1995 William F. Jolitz, TeleMuse
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
 * $Id: isym.h,v 1.3 95/02/20 17:26:04 bill Exp Locker: bill $
 *
 * Interface symbol definitions:
 *
 * These symbols are embedded strings used to allow rendezvous by modules
 * to core kernel interface functions bound at compile time. The kernel
 * maintains a table which maps an isym name to an address of the desired
 * function or variable. As the kernel interfaces evolve, incompatible
 * function interfaces or global variables get distinct new names, and
 * reverse compatible isym's can be employed to provide access to the
 * addresses of the functions or variables used to implement compatibility.
 */

/* must have defined a interface version number */
#ifndef __ISYM_VERSION__
#error "no current version"
#endif

#ifdef KERNEL

/* modules use registered, versioned symbol instead of kernel internal name */
#ifdef __MODULE__
#define __ISYM__(d, s, a)		extern d s a asm(# s __ISYM_VERSION__);
#else
#define __ISYM__(d, s, a)		extern d s a;
#endif
#define __ISYM_ALIAS__(d, s, a, o, v)	extern d s a;

#else

/* on generation of interface symbol table, force dummy definition */
#ifdef	__DEFINITION__
#define __ISYM__(d, s, a)		extern s ();
#define __ISYM_ALIAS__(d, s, a, o, v)	extern s ();
#endif

/* on generation of interface symbol table, emit an entry (isym name & addr) */
#ifdef	__ENTRY__
#undef	__ISYM__
#define __ISYM__(d, s, a)		# s ## __ISYM_VERSION__ , s ,
#undef	__ISYM_ALIAS__
#define __ISYM_ALIAS__(d, s, a, o, v)	# o # v , s ,
#endif

#endif
