/*
 * Copyright (c) 1980,1986,1988 Regents of the University of California.
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
"@(#) Copyright (c) 1980,1986,1988 Regents of the University of California.\n\
 All rights reserved.\n";
#endif not lint

#ifndef lint
static char sccsid[] = "@(#)bad144.c	5.19 (Berkeley) 4/11/91";
#endif not lint

/*
 * bad144
 *
 * This program prints and/or initializes a bad block record for a pack,
 * in the format used by the DEC standard 144.
 * It can also add bad sector(s) to the record, moving the sector
 * replacements as necessary.
 *
 * It is preferable to write the bad information with a standard formatter,
 * but this program will do.
 * 
 * RP06 sectors are marked as bad by inverting the format bit in the
 * header; on other drives the valid-sector bit is cleared.
 */
#include <sys/param.h>
#include <sys/dkbad.h>
#include <sys/ioctl.h>
#include <ufs/fs.h>
#include <sys/file.h>
#include <sys/disklabel.h>

#include <stdio.h>
#include <paths.h>

#define RETRIES	10		/* number of retries on reading old sectors */
#ifdef __386BSD__
#define	RAWPART	"d"		/* disk partition containing badsector tables */
#else
#define	RAWPART	"c"		/* disk partition containing badsector tables */
#endif

int	fflag, add, copy, verbose, nflag;
int	compare();
int	dups;
int	badfile = -1;		/* copy of badsector table to use, -1 if any */
#define MAXSECSIZE	1024
struct	dkbad curbad, oldbad;
#define	DKBAD_MAGIC	0x4321

char	label[BBSIZE];
daddr_t	size, getold(), badsn();
struct	disklabel *dp;
char	name[BUFSIZ];
char	*malloc();
off_t	lseek();

main(argc, argv)
	int argc;
	char *argv[];
{
	register struct bt_bad *bt;
	daddr_t	sn, bn[126];
	int i, f, nbad, new, bad, errs;

	argc--, argv++;
	while (argc > 0 && **argv == '-') {
		(*argv)++;
		while (**argv) {
			switch (**argv) {
#if vax
			    case 'f':
				fflag++;
				break;
#endif
			    case 'a':
				add++;
				break;
			    case 'c':
				copy++;
				break;
			    case 'v':
				verbose++;
				break;
			    case 'n':
				nflag++;
				verbose++;
				break;
			    default:
				if (**argv >= '0' && **argv <= '4') {
					badfile = **argv - '0';
					break;
				}
				goto usage;
			}
			(*argv)++;
		}
		argc--, argv++;
	}
	if (argc < 1) {
usage:
		fprintf(stderr,
		  "usage: bad144 [ -f ] disk [ snum [ bn ... ] ]\n");
		fprintf(stderr,
	      "to read or overwrite bad-sector table, e.g.: bad144 hp0\n");
		fprintf(stderr,
		  "or bad144 -a [ -f ] [ -c ] disk  bn ...\n");
		fprintf(stderr, "where options are:\n");
		fprintf(stderr, "\t-a  add new bad sectors to the table\n");
		fprintf(stderr, "\t-f  reformat listed sectors as bad\n");
		fprintf(stderr, "\t-c  copy original sector to replacement\n");
		exit(1);
	}
	if (argv[0][0] != '/')
		(void)sprintf(name, "%s/r%s%s", _PATH_DEV, argv[0], RAWPART);
	else
		strcpy(name, argv[0]);
	f = open(name, argc == 1? O_RDONLY : O_RDWR);
	if (f < 0)
		Perror(name);
#ifdef was
	if (read(f, label, sizeof(label)) < 0) 
		Perror("read");
	for (dp = (struct disklabel *)(label + LABELOFFSET);
	    dp < (struct disklabel *)
		(label + sizeof(label) - sizeof(struct disklabel));
	    dp = (struct disklabel *)((char *)dp + 64))
		if (dp->d_magic == DISKMAGIC && dp->d_magic2 == DISKMAGIC)
			break;
#else
	/* obtain label and adjust to fit */
	dp = &label;
	if (ioctl(f, DIOCGDINFO, dp) < 0)
		Perror("ioctl DIOCGDINFO");
#endif
	if (dp->d_magic != DISKMAGIC || dp->d_magic2 != DISKMAGIC
		/* dkcksum(lp) != 0 */ ) {
		fprintf(stderr, "Bad pack magic number (pack is unlabeled)\n");
		exit(1);
	}
	if (dp->d_secsize > MAXSECSIZE || dp->d_secsize <= 0) {
		fprintf(stderr, "Disk sector size too large/small (%d)\n",
			dp->d_secsize);
		exit(7);
	}
#ifdef __386BSD__
	if (dp->d_type == DTYPE_SCSI) {
		fprintf(stderr, "SCSI disks don't use bad144!\n");
		exit(1);
	}
	/* are we inside a DOS partition? */
	if (dp->d_partitions[0].p_offset) {
		/* yes, rules change. assume bad tables at end of partition C,
		   which maps all of DOS partition we are within -wfj */
		size = dp->d_partitions[2].p_offset + dp->d_partitions[2].p_size;
	} else
#endif
	size = dp->d_nsectors * dp->d_ntracks * dp->d_ncylinders; 
	argc--;
	argv++;
	if (argc == 0) {
		sn = getold(f, &oldbad);
		printf("bad block information at sector %d in %s:\n",
		    sn, name);
		printf("cartridge serial number: %d(10)\n", oldbad.bt_csn);
		switch (oldbad.bt_flag) {

		case (u_short)-1:
			printf("alignment cartridge\n");
			break;

		case DKBAD_MAGIC:
			break;

		default:
			printf("bt_flag=%x(16)?\n", oldbad.bt_flag);
			break;
		}
		bt = oldbad.bt_bad;
		for (i = 0; i < 126; i++) {
			bad = (bt->bt_cyl<<16) + bt->bt_trksec;
			if (bad < 0)
				break;
			printf("sn=%d, cn=%d, tn=%d, sn=%d\n", badsn(bt),
			    bt->bt_cyl, bt->bt_trksec>>8, bt->bt_trksec&0xff);
			bt++;
		}
		(void) checkold(&oldbad);
		exit(0);
	}
	if (add) {
		/*
		 * Read in the old badsector table.
		 * Verify that it makes sense, and the bad sectors
		 * are in order.  Copy the old table to the new one.
		 */
		(void) getold(f, &oldbad);
		i = checkold(&oldbad);
		if (verbose)
			printf("Had %d bad sectors, adding %d\n", i, argc);
		if (i + argc > 126) {
			printf("bad144: not enough room for %d more sectors\n",
				argc);
			printf("limited to 126 by information format\n");
			exit(1);
		}
		curbad = oldbad;
	} else {
		curbad.bt_csn = atoi(*argv++);
		argc--;
		curbad.bt_mbz = 0;
		curbad.bt_flag = DKBAD_MAGIC;
		if (argc > 126) {
			printf("bad144: too many bad sectors specified\n");
			printf("limited to 126 by information format\n");
			exit(1);
		}
		i = 0;
	}
	errs = 0;
	new = argc;
	while (argc > 0) {
		daddr_t sn = atoi(*argv++);
		argc--;
		if (sn < 0 || sn >= size) {
			printf("%d: out of range [0,%d) for disk %s\n",
			    sn, size, dp->d_typename);
			errs++;
			continue;
		}
		bn[i] = sn;
		curbad.bt_bad[i].bt_cyl = sn / (dp->d_nsectors*dp->d_ntracks);
		sn %= (dp->d_nsectors*dp->d_ntracks);
		curbad.bt_bad[i].bt_trksec =
		    ((sn/dp->d_nsectors) << 8) + (sn%dp->d_nsectors);
		i++;
	}
	if (errs)
		exit(1);
	nbad = i;
	while (i < 126) {
		curbad.bt_bad[i].bt_trksec = -1;
		curbad.bt_bad[i].bt_cyl = -1;
		i++;
	}
	if (add) {
		/*
		 * Sort the new bad sectors into the list.
		 * Then shuffle the replacement sectors so that
		 * the previous bad sectors get the same replacement data.
		 */
		qsort((char *)curbad.bt_bad, nbad, sizeof (struct bt_bad),
		    compare);
		if (dups) {
			fprintf(stderr,
"bad144: bad sectors have been duplicated; can't add existing sectors\n");
			exit(3);
		}
		shift(f, nbad, nbad-new);
	}
	if (badfile == -1)
		i = 0;
	else
		i = badfile * 2;
	for (; i < 10 && i < dp->d_nsectors; i += 2) {
		if (lseek(f, dp->d_secsize * (size - dp->d_nsectors + i),
		    L_SET) < 0)
			Perror("lseek");
		if (verbose)
			printf("write badsect file at %d\n",
				size - dp->d_nsectors + i);
		if (nflag == 0 && write(f, (caddr_t)&curbad, sizeof(curbad)) !=
		    sizeof(curbad)) {
			char msg[80];
			(void)sprintf(msg, "bad144: write bad sector file %d",
			    i/2);
			perror(msg);
		}
		if (badfile != -1)
			break;
	}
#ifdef vax
	if (nflag == 0 && fflag)
		for (i = nbad - new; i < nbad; i++)
			format(f, bn[i]);
#endif
#ifdef DIOCSBAD
	if (nflag == 0 && ioctl(f, DIOCSBAD, (caddr_t)&curbad) < 0)
		fprintf(stderr,
	"Can't sync bad-sector file; reboot for changes to take effect\n");
#endif
	if ((dp->d_flags & D_BADSECT) == 0 && nflag == 0) {
		dp->d_flags |= D_BADSECT;
		if (ioctl(f, DIOCWDINFO, dp) < 0) {
			perror("label");
			fprintf(stderr, "Can't write disklabel to enable bad secctor handling by the drive\n");
			exit(1);
		}
	}
	exit(0);
}

daddr_t
getold(f, bad)
struct dkbad *bad;
{
	register int i;
	daddr_t sn;
	char msg[80];

	if (badfile == -1)
		i = 0;
	else
		i = badfile * 2;
	for (; i < 10 && i < dp->d_nsectors; i += 2) {
		sn = size - dp->d_nsectors + i;
		if (lseek(f, sn * dp->d_secsize, L_SET) < 0)
			Perror("lseek");
		if (read(f, (char *) bad, dp->d_secsize) == dp->d_secsize) {
			if (i > 0)
				printf("Using bad-sector file %d\n", i/2);
			return(sn);
		}
		(void)sprintf(msg, "bad144: read bad sector file at sn %d", sn);
		perror(msg);
		if (badfile != -1)
			break;
	}
	fprintf(stderr, "bad144: %s: can't read bad block info\n", name);
	exit(1);
	/*NOTREACHED*/
}

checkold()
{
	register int i;
	register struct bt_bad *bt;
	daddr_t sn, lsn;
	int errors = 0, warned = 0;

	if (oldbad.bt_flag != DKBAD_MAGIC) {
		fprintf(stderr, "bad144: %s: bad flag in bad-sector table\n",
			name);
		errors++;
	}
	if (oldbad.bt_mbz != 0) {
		fprintf(stderr, "bad144: %s: bad magic number\n", name);
		errors++;
	}
	bt = oldbad.bt_bad;
	for (i = 0; i < 126; i++, bt++) {
		if (bt->bt_cyl == 0xffff && bt->bt_trksec == 0xffff)
			break;
		if ((bt->bt_cyl >= dp->d_ncylinders) ||
		    ((bt->bt_trksec >> 8) >= dp->d_ntracks) ||
		    ((bt->bt_trksec & 0xff) >= dp->d_nsectors)) {
			fprintf(stderr,
		     "bad144: cyl/trk/sect out of range in existing entry: ");
			fprintf(stderr, "sn=%d, cn=%d, tn=%d, sn=%d\n",
				badsn(bt), bt->bt_cyl, bt->bt_trksec>>8,
				bt->bt_trksec & 0xff);
			errors++;
		}
		sn = (bt->bt_cyl * dp->d_ntracks +
		    (bt->bt_trksec >> 8)) *
		    dp->d_nsectors + (bt->bt_trksec & 0xff);
		if (i > 0 && sn < lsn && !warned) {
		    fprintf(stderr,
			"bad144: bad sector file is out of order\n");
		    errors++;
		    warned++;
		}
		if (i > 0 && sn == lsn) {
		    fprintf(stderr,
			"bad144: bad sector file contains duplicates (sn %d)\n",
			sn);
		    errors++;
		}
		lsn = sn;
	}
	if (errors)
		exit(1);
	return (i);
}

/*
 * Move the bad sector replacements
 * to make room for the new bad sectors.
 * new is the new number of bad sectors, old is the previous count.
 */
shift(f, new, old)
{
	daddr_t repl;

	/*
	 * First replacement is last sector of second-to-last track.
	 */
	repl = size - dp->d_nsectors - 1;
	new--; old--;
	while (new >= 0 && new != old) {
		if (old < 0 ||
		    compare(&curbad.bt_bad[new], &oldbad.bt_bad[old]) > 0) {
			/*
			 * Insert new replacement here-- copy original
			 * sector if requested and possible,
			 * otherwise write a zero block.
			 */
			if (!copy ||
			    !blkcopy(f, badsn(&curbad.bt_bad[new]), repl - new))
				blkzero(f, repl - new);
		} else {
			if (blkcopy(f, repl - old, repl - new) == 0)
			    fprintf(stderr,
				"Can't copy replacement sector %d to %d\n",
				repl-old, repl-new);
			old--;
		}
		new--;
	}
}

char *buf;

/*
 *  Copy disk sector s1 to s2.
 */
blkcopy(f, s1, s2)
daddr_t s1, s2;
{
	register tries, n;

	if (buf == (char *)NULL) {
		buf = malloc((unsigned)dp->d_secsize);
		if (buf == (char *)NULL) {
			fprintf(stderr, "Out of memory\n");
			exit(20);
		}
	}
	for (tries = 0; tries < RETRIES; tries++) {
		if (lseek(f, dp->d_secsize * s1, L_SET) < 0)
			Perror("lseek");
		if ((n = read(f, buf, dp->d_secsize)) == dp->d_secsize)
			break;
	}
	if (n != dp->d_secsize) {
		fprintf(stderr, "bad144: can't read sector, %d: ", s1);
		if (n < 0)
			perror((char *)0);
		return(0);
	}
	if (lseek(f, dp->d_secsize * s2, L_SET) < 0)
		Perror("lseek");
	if (verbose)
		printf("copying %d to %d\n", s1, s2);
	if (nflag == 0 && write(f, buf, dp->d_secsize) != dp->d_secsize) {
		fprintf(stderr,
		    "bad144: can't write replacement sector, %d: ", s2);
		perror((char *)0);
		return(0);
	}
	return(1);
}

char *zbuf;

blkzero(f, sn)
daddr_t sn;
{

	if (zbuf == (char *)NULL) {
		zbuf = malloc((unsigned)dp->d_secsize);
		if (zbuf == (char *)NULL) {
			fprintf(stderr, "Out of memory\n");
			exit(20);
		}
	}
	if (lseek(f, dp->d_secsize * sn, L_SET) < 0)
		Perror("lseek");
	if (verbose)
		printf("zeroing %d\n", sn);
	if (nflag == 0 && write(f, zbuf, dp->d_secsize) != dp->d_secsize) {
		fprintf(stderr,
		    "bad144: can't write replacement sector, %d: ", sn);
		perror((char *)0);
	}
}

compare(b1, b2)
register struct bt_bad *b1, *b2;
{
	if (b1->bt_cyl > b2->bt_cyl)
		return(1);
	if (b1->bt_cyl < b2->bt_cyl)
		return(-1);
	if (b1->bt_trksec == b2->bt_trksec)
		dups++;
	return (b1->bt_trksec - b2->bt_trksec);
}

daddr_t
badsn(bt)
register struct bt_bad *bt;
{
	return ((bt->bt_cyl*dp->d_ntracks + (bt->bt_trksec>>8)) * dp->d_nsectors
		+ (bt->bt_trksec&0xff));
}

#ifdef vax

struct rp06hdr {
	short	h_cyl;
	short	h_trksec;
	short	h_key1;
	short	h_key2;
	char	h_data[512];
#define	RP06_FMT	010000		/* 1 == 16 bit, 0 == 18 bit */
};

/*
 * Most massbus and unibus drives
 * have headers of this form
 */
struct hpuphdr {
	u_short	hpup_cyl;
	u_char	hpup_sect;
	u_char	hpup_track;
	char	hpup_data[512];
#define	HPUP_OKSECT	0xc000		/* this normally means sector is good */
#define	HPUP_16BIT	0x1000		/* 1 == 16 bit format */
};
int rp06format(), hpupformat();

struct	formats {
	char	*f_name;		/* disk name */
	int	f_bufsize;		/* size of sector + header */
	int	f_bic;			/* value to bic in hpup_cyl */
	int	(*f_routine)();		/* routine for special handling */
} formats[] = {
	{ "rp06",	sizeof (struct rp06hdr), RP06_FMT,	rp06format },
	{ "eagle",	sizeof (struct hpuphdr), HPUP_OKSECT,	hpupformat },
	{ "capricorn",	sizeof (struct hpuphdr), HPUP_OKSECT,	hpupformat },
	{ "rm03",	sizeof (struct hpuphdr), HPUP_OKSECT,	hpupformat },
	{ "rm05",	sizeof (struct hpuphdr), HPUP_OKSECT,	hpupformat },
	{ "9300",	sizeof (struct hpuphdr), HPUP_OKSECT,	hpupformat },
	{ "9766",	sizeof (struct hpuphdr), HPUP_OKSECT,	hpupformat },
	{ 0, 0, 0, 0 }
};

/*ARGSUSED*/
hpupformat(fp, dp, blk, buf, count)
	struct formats *fp;
	struct disklabel *dp;
	daddr_t blk;
	char *buf;
	int count;
{
	struct hpuphdr *hdr = (struct hpuphdr *)buf;
	int sect;

	if (count < sizeof(struct hpuphdr)) {
		hdr->hpup_cyl = (HPUP_OKSECT | HPUP_16BIT) |
			(blk / (dp->d_nsectors * dp->d_ntracks));
		sect = blk % (dp->d_nsectors * dp->d_ntracks);
		hdr->hpup_track = (u_char)(sect / dp->d_nsectors);
		hdr->hpup_sect = (u_char)(sect % dp->d_nsectors);
	}
	return (0);
}

/*ARGSUSED*/
rp06format(fp, dp, blk, buf, count)
	struct formats *fp;
	struct disklabel *dp;
	daddr_t blk;
	char *buf;
	int count;
{

	if (count < sizeof(struct rp06hdr)) {
		fprintf(stderr, "Can't read header on blk %d, can't reformat\n",
			blk);
		return (-1);
	}
	return (0);
}

format(fd, blk)
	int fd;
	daddr_t blk;
{
	register struct formats *fp;
	static char *buf;
	static char bufsize;
	struct format_op fop;
	int n;

	for (fp = formats; fp->f_name; fp++)
		if (strcmp(dp->d_typename, fp->f_name) == 0)
			break;
	if (fp->f_name == 0) {
		fprintf(stderr, "bad144: don't know how to format %s disks\n",
			dp->d_typename);
		exit(2);
	}
	if (buf && bufsize < fp->f_bufsize) {
		free(buf);
		buf = NULL;
	}
	if (buf == NULL)
		buf = malloc((unsigned)fp->f_bufsize);
	if (buf == NULL) {
		fprintf(stderr, "bad144: can't allocate sector buffer\n");
		exit(3);
	}
	bufsize = fp->f_bufsize;
	/*
	 * Here we do the actual formatting.  All we really
	 * do is rewrite the sector header and flag the bad sector
	 * according to the format table description.  If a special
	 * purpose format routine is specified, we allow it to
	 * process the sector as well.
	 */
	if (verbose)
		printf("format blk %d\n", blk);
	bzero((char *)&fop, sizeof(fop));
	fop.df_buf = buf;
	fop.df_count = fp->f_bufsize;
	fop.df_startblk = blk;
	bzero(buf, fp->f_bufsize);
	if (ioctl(fd, DIOCRFORMAT, &fop) < 0)
		perror("bad144: read format");
	if (fp->f_routine &&
	    (*fp->f_routine)(fp, dp, blk, buf, fop.df_count) != 0)
		return;
	if (fp->f_bic) {
		struct hpuphdr *xp = (struct hpuphdr *)buf;

		xp->hpup_cyl &= ~fp->f_bic;
	}
	if (nflag)
		return;
	bzero((char *)&fop, sizeof(fop));
	fop.df_buf = buf;
	fop.df_count = fp->f_bufsize;
	fop.df_startblk = blk;
	if (ioctl(fd, DIOCWFORMAT, &fop) < 0)
		Perror("write format");
	if (fop.df_count != fp->f_bufsize) {
		char msg[80];
		(void)sprintf(msg, "bad144: write format %d", blk);
		perror(msg);
	}
}
#endif

Perror(op)
	char *op;
{

	fprintf(stderr, "bad144: "); perror(op);
	exit(4);
}
