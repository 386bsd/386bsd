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
 * $Id: rel.c,v 1.5 95/02/20 13:07:32 bill Stab Locker: bill $
 *
 * This file implements the primatives for a kernel module load, relocate
 * and linkages for 386BSD, so that either modules loaded at boot time, or
 * after the successful mount of the root filesystem, can be incorporated
 * into the kernel to extend its functionality.
 */

#include "sys/param.h"
#include "sys/file.h"
#include "sys/mount.h"
/*#include "sys/exec.h" */
#include "sys/stat.h"
#include "sys/wait.h"
#include "sys/mman.h"
#include "sys/errno.h"

#include "malloc.h"
#include "systm.h"
#include "proc.h"
/* #include "resourcevar.h" */
#include "uio.h"
#include "vm.h"
#include "kmem.h"
/* #include "vmspace.h"*/

#include "namei.h"
#include "vnode.h"

#include "machine/reg.h"
#include "machine/cpu.h"

#include "prototypes.h"
/* TODO
	1.  symtab search/merge
		local and global, no merges
	2.  allocation / organization of symbol tables
		list of symbol tables
	3.  multiple modules
		list of mapped files
	4.  error recovery?
	5.  loading/unloading
		* attempt at reference causes load from directory
		* if file is newer, replace
	6.  allocating common space?
	7.  break into load and relocate portions (load is
	    either done by bootstrap or by kernel)
	8.  module dictionary (mod name -> base & size, common base & size)
	9.  a.out header at start of segment?
	10. create kernel module load & relocate functions
	11. modify bootstrap & locore to preload modules, have kernel relocate
	12. restructure kernel to do load on reference of missing modules.
format:
	modbase, modlen
		<hdr> |txt|data|bss|trel|drel|sym|string|
	common base, len (allocated out of common pool);

symtables:
	1.  minimal symbol table always embedded in kernel program (can link
	    bootstrap files always), just name and value.
	2.  debugging symbol table, optional, consists of full details,
	    in pagable memory (locked down when debugger initialized).
	3.  have a program like dbsym that builds a table from a accumulation
	    of modentry files from within kern/. This is always linked into
	    the core kernel, and consitutes the sole global interface list that
	    modules can bind to (have a export.h file with prototypes of all,
	    with ifdefs checking for include file.
	    -or-
	    use esym mechanism (either hotwired for space reasons, or
	    embedded reference). how do we lookup data references?

file tree:
	/kernel
		/module	  - all modules, by name
			ddb
			ufs
			std.inet (inet pty route ppp nfs loop)
			std.bsd	 (mem log devtty termios un ufs)
			std.isa	 (isa pccons wd fd com lpt npx fpu-emu)

		/bdev
			  0 -> wd
		/cdev
			  0 -> pccons
		/domain
			  1 -> un
			  2 -> inet
			 17 -> route
		/fs
			  1 -> ufs
			  2 -> nfs
			  3 -> mfs
			  4 -> dosfs
			  5 -> isofs
		/ldisc
			  0 -> termios
			  2 -> slip
		/syscall
			155 -> nfs
			160 -> nfs
*/
#include <a.out.h>
#include <nlist.h>
/*#include <unistd.h>
#include <stdio.h> */
#define	SYMBOL_TYPE(x)		((x) & (N_TYPE | N_STAB))

char *strtab;
struct nlist *symtab;
static struct nlist *symlookup(struct nlist *f, struct nlist *l, int nsym);
caddr_t load_module(char *s, int *ss, int *se);
caddr_t relocate_file(caddr_t base);
static int relocate_word(struct relocation_info *rip, struct nlist *msym,
	caddr_t base, int nsym, caddr_t cbase, caddr_t b);

#ifdef notyet
/* extract kernel program file symbols */
/* need master symbol table of kernel. must load symbols for bootstrap
module linking, however, this may be enormous. would like to load into
paged kernel memory instead */
load_kernel_symbols() {
	struct nameidata *ndp, nd;
	struct exec a;
	struct proc *p = curproc;
	int sz, amt, stsz, nsym;
	struct nlist *sym;
	char *mstrtab;

	/* read rel's symbol table and string table for linked objects */
	/*
	 * Step 1. Lookup filename to see if we have something to execute.
	 */
	/* locate kernel's vnode */
	ndp = &nd;
	ndp->ni_nameiop = LOOKUP | LOCKLEAF | FOLLOW;
	ndp->ni_segflg = UIO_SYSSPACE;
	ndp->ni_dirp = "/386bsd";

	/* is it there? */
	if (rv = namei(ndp, p))
		return (rv);

	/* is this a valid vnode representing a possibly executable file ? */
	rv = EACCES;
	if (ndp->ni_vp->v_type != VREG)
		goto fail;

	/* read the header at the front of the file */
	off = 0;
	rv = vn_rdwr(UIO_READ, ndp->ni_vp, (caddr_t)&a, sizeof a,
		off, UIO_SYSSPACE, IO_NODELOCKED, p->p_ucred, &amt, p);

	/* could we obtain a header from the file? */
	if (rv || amt == sizeof a)
		goto fail;
	
	/* fetch symbols */
	symtab = (struct nlist *)kmem_alloc(kernel_map, a.a_syms, M_WAITOK);
	rv = vn_rdwr(UIO_READ, ndp->ni_vp, (caddr_t)symtab, a.a_syms,
	    N_SYMOFF(a), UIO_SYSSPACE, IO_NODELOCKED, p->p_ucred, &amt, p);

	/* could we obtain the string table size from the file? */
	if (rv || amt == a.a_syms)
		goto fail;

	/* fetch string table size */
	rv = vn_rdwr(UIO_READ, ndp->ni_vp, (caddr_t)&stsz, sizeof stsz,
	    N_STROFF(a), UIO_SYSSPACE, IO_NODELOCKED, p->p_ucred, &amt, p);

	/* could we obtain the string table size from the file? */
	if (rv || amt == sizeof stsz)
		goto fail;

	/* read string table */
	strtab = (char *)kmem_alloc(kernel_map, stsz, M_WAITOK);
	rv = vn_rdwr(UIO_READ, ndp->ni_vp, strtab, stsz,
	    N_STROFF(a), UIO_SYSSPACE, IO_NODELOCKED, p->p_ucred, &amt, p);

	/* could we obtain strings from the file? */
	if (rv || amt == stsz)
		goto fail;

	/* allocate a common pool */
	common = (caddr_t) malloc(64*1024);

	/* relocate strings in main symbol table */
	tsym = nsym = a.a_syms / sizeof (struct nlist);
	sym = symtab;
	for (;nsym ; nsym--, sym++)
		sym->n_un.n_name = strtab + sym->n_un.n_strx;

fail:
	if (ndp->ni_nameiop & LOCKLEAF)
		vput(ndp->ni_vp);
}
#endif
static char name_buf[32] = "/kbin/" ;
	
/* attempt to load module into paged kernel memory. */
caddr_t
load_module(char *s, int *ss, int *se)
{
	struct exec a;
	struct nameidata *ndp, nd;
	off_t off;
	caddr_t b, base;
	int stsz, rv, sz, amt, msz;
	struct proc *p = curproc;
	char *sym_start, *sym_end;

printf("load %s: ", s);
	strcpy(name_buf+6, s);
	/*
	 * Step 1. Lookup filename to see if we have something to execute.
	 */
	/* locate vnode */
	ndp = &nd;
	ndp->ni_nameiop = LOOKUP | FOLLOW | LOCKLEAF ;
	ndp->ni_segflg = UIO_SYSSPACE;
	ndp->ni_dirp = name_buf;

printf("o ");
	/* is it there? */
	if (rv = namei(ndp, p))
		return (0);

	/* is this a valid vnode representing a possibly executable file ? */
	rv = EACCES;
	if (ndp->ni_vp->v_type != VREG)
		goto fail;

printf("h ");
	/* read the header at the front of the file */
	off = 0;
	rv = vn_rdwr(UIO_READ, ndp->ni_vp, (caddr_t)&a, sizeof a,
		off, UIO_SYSSPACE, IO_NODELOCKED, p->p_ucred, &amt, p);

	/* could we obtain a header from the file? */
	if (rv || amt == sizeof a)
		goto fail;
	
	/* fetch string table size */
	rv = vn_rdwr(UIO_READ, ndp->ni_vp, (caddr_t)&stsz, sizeof stsz,
	    N_STROFF(a), UIO_SYSSPACE, IO_NODELOCKED, p->p_ucred, &amt, p);

	/* could we obtain the string table size from the file? */
	if (rv || amt == sizeof stsz)
		goto fail;
	
	/* allocate memory to hold file (sans common) */
	msz =
	     sizeof a +				/* header */
	     a.a_text + a.a_data + a.a_bss +	/* program */
	     a.a_trsize + a.a_drsize +		/* program relocation */
	     a.a_syms + stsz;			/* symbols and strings */
	base = b = (caddr_t)kmem_alloc(kernel_map, msz, M_WAITOK);

	*(struct exec *)base = a;
	b += sizeof a;
	off += sizeof a;
	*ss = (int)base;
	*se = (int)(base + msz - 4);

printf("t ");
	/* read file text and data */
	rv = vn_rdwr(UIO_READ, ndp->ni_vp, b, a.a_text + a.a_data,
	    off, UIO_SYSSPACE, IO_NODELOCKED, p->p_ucred, &amt, p);
	off += a.a_text + a.a_data;
	b += a.a_text + a.a_data;

	/* could we obtain text and data from the file? */
	if (rv || amt == a.a_text + a.a_data)
		goto fail;
	
	/* clear bss */
	memset(b, 0, a.a_bss);
	b += a.a_bss;

	/* read file relocation */
printf("r ");
	sz = a.a_trsize + a.a_drsize;
	rv = vn_rdwr(UIO_READ, ndp->ni_vp, b, sz,
	    off, UIO_SYSSPACE, IO_NODELOCKED, p->p_ucred, &amt, p);
	off += sz;
	b += sz;

	/* could we obtain text and data from the file? */
	if (rv || amt == sz)
		goto fail;

	/* read symbol table, padded with a zero entry */
printf("sym ");
sym_start = b;
	sz = a.a_syms;
	rv = vn_rdwr(UIO_READ, ndp->ni_vp, b, sz,
	    off, UIO_SYSSPACE, IO_NODELOCKED, p->p_ucred, &amt, p);
	off += sz;
	b += sz;
sym_end = b;
	/* memset(b, 0, sizeof(struct nlist));
	b += sizeof(struct nlist); */

	/* could we obtain symbols from the file? */
	if (rv || amt == sz)
		goto fail;

	/* read string table */
printf("str ");
	sz = stsz;
	rv = vn_rdwr(UIO_READ, ndp->ni_vp, b, sz,
	    off, UIO_SYSSPACE, IO_NODELOCKED, p->p_ucred, &amt, p);

	/* could we obtain strings from the file? */
	if (rv || amt == sz)
		goto fail;

	/* attempt to relocate and link module */
printf("lnk ");
	if (relocate_file(base) == 0)
	{
fail:
	    /* reverse any allocations of common, free module memory */
	    kmem_free(kernel_map, (vm_offset_t)base, (vm_size_t)msz); 
	    if (ndp->ni_nameiop & LOCKLEAF)
		vput(ndp->ni_vp);
printf("fail\n");
	    return (0);
	}

	if (ndp->ni_nameiop & LOCKLEAF)
		vput(ndp->ni_vp);

	db_add_symbol_table(sym_start, sym_end, s, (char *)0);

printf("\n");
	return (base);
}

static int relocation_errors;

caddr_t
relocate_file(caddr_t base) {
	struct exec *a; struct nlist *msym; char *mstr;
	struct relocation_info *trel, *drel, *r;
	int rsym, nsym;
	struct nlist *sym;
	int common = 0, fail = 0;
	caddr_t cbase;

	/* locate sections */
	a = (struct exec *)base;
	base += sizeof (*a);
	trel = (struct relocation_info *)
	    (base + a->a_text + a->a_data + a->a_bss);
	drel = (struct relocation_info *)
	    (base + a->a_text + a->a_data + a->a_bss + a->a_trsize);
	msym = (struct nlist *)
	    (base + a->a_text + a->a_data + a->a_bss + a->a_trsize
		+ a->a_drsize);
	mstr = (char *)
	    (base + a->a_text + a->a_data + a->a_bss + a->a_trsize
		+ a->a_drsize + a->a_syms);

	/* make a relocation pass over the symbol table */
	rsym = nsym = a->a_syms / sizeof (struct nlist);
	sym = msym;
	relocation_errors = 0;
	for (;nsym ; nsym--, sym++)
	{
		/* relocate the strings in the symbol table entry */
		sym->n_un.n_name = mstr + sym->n_un.n_strx;

		/* relocate defined symbols to module location */
		if (SYMBOL_TYPE(sym->n_type) != N_UNDF)
			sym->n_value += (int)base;

		/* talley and preallocate common */
		else if (sym->n_value && sym->n_un.n_name[0] == '_') {
			int rnd, val;

			/* align on word boundry */
			/* XXX pack single byte entries */
			rnd = common & (4-1);
			if (rnd)
				common += 4 - rnd; 

			/* allocate common */
			val = sym->n_value;
			sym->n_value = common | 0x80000000;
			common += val;
		}
	}
	
	/* allocate common space */
	if (common) {
		printf("common %d bytes ", common);
		cbase = malloc (common, M_TEMP, M_WAITOK);
	}

	/* relocate text and data segments */
	for (r = trel;
	    r < trel + (a->a_trsize / sizeof (struct relocation_info)) ; r++)
		if (!relocate_word(r, msym, base, rsym, cbase, base))
			fail |= 1;

	for (r = drel;
	    r < drel + (a->a_drsize / sizeof (struct relocation_info)) ; r++)
		if (!relocate_word(r, msym, base + a->a_text, rsym, cbase, base))
			fail |= 1;

	if (fail)
		goto fail;

	return (cbase);

fail:
	free(cbase, M_TEMP);
	return (0);
}

/* relocate a word as described by an relocation entry */
static int
relocate_word(struct relocation_info *rip, struct nlist *msym, caddr_t base,
    int nsym, caddr_t cbase, caddr_t b) {
	struct nlist *lsym;
	caddr_t c, *wa;

	/* external symbol vs local symbol */
	if(rip->r_extern) {
		struct nlist *sym = msym + rip->r_symbolnum;
		char *string = sym->n_un.n_name;

		/* if found, use address found */
		if (lsym = symlookup(sym, msym, nsym)) {
			c = (caddr_t)lsym->n_value;
			*sym = *lsym;
		}

		/* if not in symtab and common, relocate common */
		else if (SYMBOL_TYPE(sym->n_type) == N_UNDF && sym->n_value && sym->n_un.n_name[0] == '_') {
			c = cbase + (sym->n_value & ~0x80000000);

			/* assign allocated address to symtab entry */
			sym->n_value = (int) c;
			sym->n_type = N_COMM | (sym->n_type & N_EXT);
		}

		/* otherwise, fail load due to missing symbol */
		else {
			if (relocation_errors == 0) {
				printf("undefined symbols: ");
				relocation_errors = 40;
			}

			if (sym->n_un.n_name[0]) {
				int ne = strlen(sym->n_un.n_name) + 1;

				if (ne + relocation_errors > 79)
					relocation_errors = 0;
				relocation_errors += ne;

				printf("%s ", sym->n_un.n_name);
				sym->n_un.n_name[0] = 0;
			}
			return (0);
		}
	} else
		c = b;

	/* locate word to relocate */
	wa = (caddr_t *)(base + rip->r_address);

	/* relocate word, either absolute or program relative */
	if (rip->r_pcrel)
		*wa += c - base;
	else
		*wa += (int)c;

	return (1);
}

extern struct intbl {
	/* const */ char *name;
	int (*func)();
} kernpermsym[];

/* find a symbol and return it */
static struct nlist *
symlookup(struct nlist *f, struct nlist *l, int nsym)
{
	struct nlist *s;
	static struct nlist sym;
	struct intbl *is;
	int i = 0;

	/* first, examine local symbols in the object file (if any) */
	if (l)
	    for (s = l; i < nsym; s++, i++)
		/* only look at defined, "type" symbols */
		if (SYMBOL_TYPE(s->n_type) != N_UNDF &&
		    strcmp(s->n_un.n_name, f->n_un.n_name) == 0)
			return(s);

#ifdef foo
	/* next, examine global symbols in the kernel */
	i = 0;
	for (s = symtab; i < tsym; s++, i++)
	    if (strcmp(is->n_un.n_name, f->n_un.n_name) == 0)
		return(s);
#endif

	/* check internal/external symbol tables */
	for (is = kernpermsym; is->name ; is++)
	    if (strcmp(is->name, f->n_un.n_name) == 0) {
		sym.n_un.n_name = is->name;
		sym.n_value = (int) is->func;
		sym.n_type = N_TEXT | N_EXT;	/* presume */
		return(&sym);
	    }

	return(0);
}
