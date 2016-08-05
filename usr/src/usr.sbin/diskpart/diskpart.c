/*
 * Copyright (c) 1983, 1988 Regents of the University of California.
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

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1983, 1988 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)diskpart.c	5.11 (Berkeley) 6/1/90";
#endif /* not lint */

/*
 * Program to calculate standard disk partition sizes.
 */
#include <sys/param.h>
#define DKTYPENAMES
#include <sys/disklabel.h>

#include <stdio.h>
#include <ctype.h>

#define	for_now			/* show all of `c' partition for disklabel */
#define	NPARTITIONS	8
#define	PART(x)		(x - 'a')

/*
 * Default partition sizes, where they exist.
 */
#define	NDEFAULTS	4
int	defpart[NDEFAULTS][NPARTITIONS] = {
   { 15884, 66880, 0, 15884, 307200, 0, 0, 291346 },	/* ~ 356+ Mbytes */
   { 15884, 33440, 0, 15884, 55936, 0, 0, 291346 },	/* ~ 206-355 Mbytes */
   { 15884, 33440, 0, 15884, 55936, 0, 0, 0 },		/* ~ 61-205 Mbytes */
   { 15884, 10032, 0, 15884, 0, 0, 0, 0 },		/* ~ 20-60 Mbytes */
};

/*
 * Each array defines a layout for a disk;
 * that is, the collection of partitions totally
 * covers the physical space on a disk.
 */
#define	NLAYOUTS	3
char	layouts[NLAYOUTS][NPARTITIONS] = {
   { 'a', 'b', 'h', 'g' },
   { 'a', 'b', 'h', 'd', 'e', 'f' },
   { 'c' },
};

/*
 * Default disk block and disk block fragment
 * sizes for each file system.  Those file systems
 * with zero block and frag sizes are special cases
 * (e.g. swap areas or for access to the entire device).
 */
struct	partition defparam[NPARTITIONS] = {
	{ 0, 0, 1024, FS_UNUSED, 8, 0 },		/* a */
	{ 0, 0, 1024, FS_SWAP,   8, 0 },		/* b */
	{ 0, 0, 1024, FS_UNUSED, 8, 0 },		/* c */
	{ 0, 0,  512, FS_UNUSED, 8, 0 },		/* d */
	{ 0, 0, 1024, FS_UNUSED, 8, 0 },		/* e */
	{ 0, 0, 1024, FS_UNUSED, 8, 0 },		/* f */
	{ 0, 0, 1024, FS_UNUSED, 8, 0 },		/* g */
	{ 0, 0, 1024, FS_UNUSED, 8, 0 }			/* h */
};

/*
 * Each disk has some space reserved for a bad sector
 * forwarding table.  DEC standard 144 uses the first
 * 5 even numbered sectors in the last track of the
 * last cylinder for replicated storage of the bad sector
 * table; another 126 sectors past this is needed as a
 * pool of replacement sectors.
 */
int	badsecttable = 126;	/* # sectors */

int	pflag;			/* print device driver partition tables */
int	dflag;			/* print disktab entry */

struct	disklabel *promptfordisk();

main(argc, argv)
	int argc;
	char *argv[];
{
	struct disklabel *dp;
	register int curcyl, spc, def, part, layout, j;
	int threshhold, numcyls[NPARTITIONS], startcyl[NPARTITIONS];
	int totsize = 0;
	char *lp, *tyname;

	argc--, argv++;
	if (argc < 1) {
		fprintf(stderr,
		    "usage: disktab [ -p ] [ -d ] [ -s size ] disk-type\n");
		exit(1);
	}
	if (argc > 0 && strcmp(*argv, "-p") == 0) {
		pflag++;
		argc--, argv++;
	}
	if (argc > 0 && strcmp(*argv, "-d") == 0) {
		dflag++;
		argc--, argv++;
	}
	if (argc > 1 && strcmp(*argv, "-s") == 0) {
		totsize = atoi(argv[1]);
		argc += 2, argv += 2;
	}
	dp = getdiskbyname(*argv);
	if (dp == NULL) {
		if (isatty(0))
			dp = promptfordisk(*argv);
		if (dp == NULL) {
			fprintf(stderr, "%s: unknown disk type\n", *argv);
			exit(2);
		}
	} else {
		if (dp->d_flags & D_REMOVABLE)
			tyname = "removable";
		else if (dp->d_flags & D_RAMDISK)
			tyname = "simulated";
		else
			tyname = "winchester";
	}
	spc = dp->d_secpercyl;
	/*
	 * Bad sector table contains one track for the replicated
	 * copies of the table and enough full tracks preceding
	 * the last track to hold the pool of free blocks to which
	 * bad sectors are mapped.
	 * If disk size was specified explicitly, use specified size.
	 */
	if (dp->d_type == DTYPE_SMD && dp->d_flags & D_BADSECT &&
	    totsize == 0) {
		badsecttable = dp->d_nsectors +
		    roundup(badsecttable, dp->d_nsectors);
		threshhold = howmany(spc, badsecttable);
	} else {
		badsecttable = 0;
		threshhold = 0;
	}
	/*
	 * If disk size was specified, recompute number of cylinders
	 * that may be used, and set badsecttable to any remaining
	 * fraction of the last cylinder.
	 */
	if (totsize != 0) {
		dp->d_ncylinders = howmany(totsize, spc);
		badsecttable = spc * dp->d_ncylinders - totsize;
	}

	/* 
	 * Figure out if disk is large enough for
	 * expanded swap area and 'd', 'e', and 'f'
	 * partitions.  Otherwise, use smaller defaults
	 * based on RK07.
	 */
	for (def = 0; def < NDEFAULTS; def++) {
		curcyl = 0;
		for (part = PART('a'); part < NPARTITIONS; part++)
			curcyl += howmany(defpart[def][part], spc);
		if (curcyl < dp->d_ncylinders - threshhold)
			break;
	}
	if (def >= NDEFAULTS) {
		fprintf(stderr, "%s: disk too small, calculate by hand\n",
			*argv);
		exit(3);
	}

	/*
	 * Calculate number of cylinders allocated to each disk
	 * partition.  We may waste a bit of space here, but it's
	 * in the interest of (very backward) compatibility
	 * (for mixed disk systems).
	 */
	for (curcyl = 0, part = PART('a'); part < NPARTITIONS; part++) {
		numcyls[part] = 0;
		if (defpart[def][part] != 0) {
			numcyls[part] = howmany(defpart[def][part], spc);
			curcyl += numcyls[part];
		}
	}
	numcyls[PART('f')] = dp->d_ncylinders - curcyl;
	numcyls[PART('g')] =
		numcyls[PART('d')] + numcyls[PART('e')] + numcyls[PART('f')];
	numcyls[PART('c')] = dp->d_ncylinders;
	defpart[def][PART('f')] = numcyls[PART('f')] * spc - badsecttable;
	defpart[def][PART('g')] = numcyls[PART('g')] * spc - badsecttable;
	defpart[def][PART('c')] = numcyls[PART('c')] * spc;
#ifndef for_now
	if (totsize || !pflag)
#else
	if (totsize)
#endif
		defpart[def][PART('c')] -= badsecttable;

	/*
	 * Calculate starting cylinder number for each partition.
	 * Note the 'h' partition is physically located before the
	 * 'g' or 'd' partition.  This is reflected in the layout
	 * arrays defined above.
	 */
	for (layout = 0; layout < NLAYOUTS; layout++) {
		curcyl = 0;
		for (lp = layouts[layout]; *lp != 0; lp++) {
			startcyl[PART(*lp)] = curcyl;
			curcyl += numcyls[PART(*lp)];
		}
	}

	if (pflag) {
		printf("}, %s_sizes[%d] = {\n", dp->d_typename, NPARTITIONS);
		for (part = PART('a'); part < NPARTITIONS; part++) {
			if (numcyls[part] == 0) {
				printf("\t0,\t0,\n");
				continue;
			}
			if (dp->d_type != DTYPE_MSCP) {
			       printf("\t%d,\t%d,\t\t/* %c=cyl %d thru %d */\n",
					defpart[def][part], startcyl[part],
					'A' + part, startcyl[part],
					startcyl[part] + numcyls[part] - 1);
				continue;
			}
			printf("\t%d,\t%d,\t\t/* %c=sectors %d thru %d */\n",
				defpart[def][part], spc * startcyl[part],
				'A' + part, spc * startcyl[part],
				spc * startcyl[part] + defpart[def][part] - 1);
		}
		exit(0);
	}
	if (dflag) {
		int nparts;

		/*
		 * In case the disk is in the ``in-between'' range
		 * where the 'g' partition is smaller than the 'h'
		 * partition, reverse the frag sizes so the /usr partition
		 * is always set up with a frag size larger than the
		 * user's partition.
		 */
		if (defpart[def][PART('g')] < defpart[def][PART('h')]) {
			int temp;

			temp = defparam[PART('h')].p_fsize;
			defparam[PART('h')].p_fsize =
				defparam[PART('g')].p_fsize;
			defparam[PART('g')].p_fsize = temp;
		}
		printf("%s:\\\n", dp->d_typename);
		printf("\t:ty=%s:ns#%d:nt#%d:nc#%d:", tyname,
			dp->d_nsectors, dp->d_ntracks, dp->d_ncylinders);
		if (dp->d_secpercyl != dp->d_nsectors * dp->d_ntracks)
			printf("sc#%d:", dp->d_secpercyl);
		if (dp->d_type == DTYPE_SMD && dp->d_flags & D_BADSECT)
			printf("sf:");
		printf("\\\n\t:dt=%s:", dktypenames[dp->d_type]);
		for (part = NDDATA - 1; part >= 0; part--)
			if (dp->d_drivedata[part])
				break;
		for (j = 0; j <= part; j++)
			printf("d%d#%d:", j, dp->d_drivedata[j]);
		printf("\\\n");
		for (nparts = 0, part = PART('a'); part < NPARTITIONS; part++)
			if (defpart[def][part] != 0)
				nparts++;
		for (part = PART('a'); part < NPARTITIONS; part++) {
			if (defpart[def][part] == 0)
				continue;
			printf("\t:p%c#%d:", 'a' + part, defpart[def][part]);
			printf("o%c#%d:b%c#%d:f%c#%d:",
			    'a' + part, spc * startcyl[part],
			    'a' + part,
			    defparam[part].p_frag * defparam[part].p_fsize,
			    'a' + part, defparam[part].p_fsize);
			if (defparam[part].p_fstype == FS_SWAP)
				printf("t%c=swap:", 'a' + part);
			nparts--;
			printf("%s\n", nparts > 0 ? "\\" : "");
		}
#ifdef for_now
		defpart[def][PART('c')] -= badsecttable;
		part = PART('c');
		printf("#\t:p%c#%d:", 'a' + part, defpart[def][part]);
		printf("o%c#%d:b%c#%d:f%c#%d:\n",
		    'a' + part, spc * startcyl[part],
		    'a' + part,
		    defparam[part].p_frag * defparam[part].p_fsize,
		    'a' + part, defparam[part].p_fsize);
#endif
		exit(0);
	}
	printf("%s: #sectors/track=%d, #tracks/cylinder=%d #cylinders=%d\n",
		dp->d_typename, dp->d_nsectors, dp->d_ntracks,
		dp->d_ncylinders);
	printf("\n    Partition\t   Size\t Offset\t   Range\n");
	for (part = PART('a'); part < NPARTITIONS; part++) {
		printf("\t%c\t", 'a' + part);
		if (numcyls[part] == 0) {
			printf(" unused\n");
			continue;
		}
		printf("%7d\t%7d\t%4d - %d%s\n",
			defpart[def][part], startcyl[part] * spc,
			startcyl[part], startcyl[part] + numcyls[part] - 1,
			defpart[def][part] % spc ? "*" : "");
	}
}

struct disklabel disk;

struct	field {
	char	*f_name;
	char	*f_defaults;
	u_long	*f_location;
} fields[] = {
	{ "sector size",		"512",	&disk.d_secsize },
	{ "#sectors/track",		0,	&disk.d_nsectors },
	{ "#tracks/cylinder",		0,	&disk.d_ntracks },
	{ "#cylinders",			0,	&disk.d_ncylinders },
	{ 0, 0, 0 },
};

struct disklabel *
promptfordisk(name)
	char *name;
{
	register struct disklabel *dp = &disk;
	register struct field *fp;
	register i;
	char buf[BUFSIZ], **tp, *cp, *gets();

	strncpy(dp->d_typename, name, sizeof(dp->d_typename));
	fprintf(stderr,
		"%s: unknown disk type, want to supply parameters (y/n)? ",
		name);
	(void) gets(buf);
	if (*buf != 'y')
		return ((struct disklabel *)0);
	for (;;) {
		fprintf(stderr, "Disk/controller type (%s)? ", dktypenames[1]);
		(void) gets(buf);
		if (buf[0] == 0)
			dp->d_type = 1;
		else
			dp->d_type = gettype(buf, dktypenames);
		if (dp->d_type >= 0)
			break;
		fprintf(stderr, "%s: unrecognized controller type\n", buf);
		fprintf(stderr, "use one of:\n", buf);
		for (tp = dktypenames; *tp; tp++)
			if (index(*tp, ' ') == 0)
				fprintf(stderr, "\t%s\n", *tp);
	}
gettype:
	dp->d_flags = 0;
	fprintf(stderr, "type (winchester|removable|simulated)? ");
	(void) gets(buf);
	if (strcmp(buf, "removable") == 0)
		dp->d_flags = D_REMOVABLE;
	else if (strcmp(buf, "simulated") == 0)
		dp->d_flags = D_RAMDISK;
	else if (strcmp(buf, "winchester")) {
		fprintf(stderr, "%s: bad disk type\n", buf);
		goto gettype;
	}
	strncpy(dp->d_typename, buf, sizeof(dp->d_typename));
	fprintf(stderr, "(type <cr> to get default value, if only one)\n");
	if (dp->d_type == DTYPE_SMD)
	   fprintf(stderr, "Do %ss support bad144 bad block forwarding (yes)? ",
		dp->d_typename);
	(void) gets(buf);
	if (*buf != 'n')
		dp->d_flags |= D_BADSECT;
	for (fp = fields; fp->f_name != NULL; fp++) {
again:
		fprintf(stderr, "%s ", fp->f_name);
		if (fp->f_defaults != NULL)
			fprintf(stderr, "(%s)", fp->f_defaults);
		fprintf(stderr, "? ");
		cp = gets(buf);
		if (*cp == '\0') {
			if (fp->f_defaults == NULL) {
				fprintf(stderr, "no default value\n");
				goto again;
			}
			cp = fp->f_defaults;
		}
		*fp->f_location = atol(cp);
		if (*fp->f_location == 0) {
			fprintf(stderr, "%s: bad value\n", cp);
			goto again;
		}
	}
	fprintf(stderr, "sectors/cylinder (%d)? ",
	    dp->d_nsectors * dp->d_ntracks);
	(void) gets(buf);
	if (buf[0] == 0)
		dp->d_secpercyl = dp->d_nsectors * dp->d_ntracks;
	else
		dp->d_secpercyl = atol(buf);
	fprintf(stderr, "Drive-type-specific parameters, <cr> to terminate:\n");
	for (i = 0; i < NDDATA; i++) {
		fprintf(stderr, "d%d? ", i);
		(void) gets(buf);
		if (buf[0] == 0)
			break;
		dp->d_drivedata[i] = atol(buf);
	}
	return (dp);
}

gettype(t, names)
	char *t;
	char **names;
{
	register char **nm;

	for (nm = names; *nm; nm++)
		if (ustrcmp(t, *nm) == 0)
			return (nm - names);
	if (isdigit(*t))
		return (atoi(t));
	return (-1);
}

ustrcmp(s1, s2)
	register char *s1, *s2;
{
#define	lower(c)	(islower(c) ? (c) : tolower(c))

	for (; *s1; s1++, s2++) {
		if (*s1 == *s2)
			continue;
		if (isalpha(*s1) && isalpha(*s2) &&
		    lower(*s1) == lower(*s2))
			continue;
		return (*s2 - *s1);
	}
	return (0);
}
