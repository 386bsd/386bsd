/*
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
#ifndef lint
static char rcsid[] =
    "@(#)$Header: savefile.c,v 1.21 91/06/06 23:07:42 mccanne Exp $ (LBL)";
#endif

/*
 * savefile.c - supports offline use of tcpdump
 *	Extraction/creation by Jeffrey Mogul, DECWRL
 *	Modified by Steve McCanne, LBL.
 *
 * Used to save the received packet headers, after filtering, to
 * a file, and then read them later.
 * The first record in the file contains saved values for the machine
 * dependent values so we can print the dump file on any architecture.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <net/bpf.h>

#include "interface.h"
#include "savefile.h"

#define TCPDUMP_MAGIC 0xa1b2c3d4

/*
 * The first record in the file contains saved values for some
 * of the flags used in the printout phases of tcpdump.
 * Many fields here are longs so compilers won't insert unwanted
 * padding; these files need should be interchangeable across 
 * architectures.
 */
struct file_header {
	u_long magic;
	u_short version_major;
	u_short version_minor;
	long thiszone;		/* gmt to local correction */
	u_long sigfigs;		/* accuracy of timestamps */
	u_long snaplen;		/* max length saved portion of each pkt */
	u_long linktype;
};

int sf_swapped;

FILE *sf_readfile;
FILE *sf_writefile;

static int
sf_write_header(fp, linktype)
	FILE *fp;
	int linktype;
{
	struct file_header hdr;

	hdr.magic = TCPDUMP_MAGIC;
	hdr.version_major = VERSION_MAJOR;
	hdr.version_minor = VERSION_MINOR;

	hdr.thiszone = thiszone;
	hdr.snaplen = snaplen;
	hdr.sigfigs = clock_sigfigs();
	hdr.linktype = linktype;

	if (fwrite((char *)&hdr, sizeof(hdr), 1, fp) != 1)
		return -1;

	return 0;
}

static void
swap_hdr(hp)
	struct file_header *hp;
{
	hp->version_major = SWAPSHORT(hp->version_major);
	hp->version_minor = SWAPSHORT(hp->version_minor);
	hp->thiszone = SWAPLONG(hp->thiszone);
	hp->sigfigs = SWAPLONG(hp->sigfigs);
	hp->snaplen = SWAPLONG(hp->snaplen);
	hp->linktype = SWAPLONG(hp->linktype);
}

int
sf_read_init(fname, linktype)
	char *fname;
	int *linktype;
{
	register FILE *fp;
	struct file_header hdr;

	if (fname[0] == '-' && fname[1] == '\0')
		fp = stdin;
	else {
		fp = fopen(fname, "r");
		if (fp == 0) {
			perror(fname);
			exit(1);
		}
	}
	if (fread((char *)&hdr, sizeof(hdr), 1, fp) != 1) {
		perror(fname);
		exit(1);
	}
	if (hdr.magic != TCPDUMP_MAGIC) {
		if (SWAPLONG(hdr.magic) != TCPDUMP_MAGIC)
			return SFERR_BADF;
		sf_swapped = 1;
		swap_hdr(&hdr);
	}
	if (hdr.version_major < VERSION_MAJOR)
		return SFERR_BADVERSION;

	thiszone = hdr.thiszone;
	snaplen = hdr.snaplen;
	*linktype = hdr.linktype;
	timestampinit((int)hdr.sigfigs);

	sf_readfile = fp;

	return 0;
}

/*
 * Print out packets stored in the file initilized by sf_read_init().
 * If cflag is true, return after 'cnt' packets.
 */
int
sf_read(filtp, cnt, printit)
	struct bpf_program *filtp;
	int cnt;
	void (*printit)();
{
	struct packet_header h;
	u_char *buf;
	struct bpf_insn *fcode = filtp->bf_insns;
	int status = 0;

	buf = (u_char *)malloc(snaplen);

	while (status == 0) {
		status = sf_next_packet(&h, buf, snaplen);

		if (status)
			break;

		if (bpf_filter(fcode, buf, h.len, h.caplen)) {
			if (cflag && --cnt < 0)
				break;
			(*printit)(buf, &h.ts, h.len, h.caplen);
		}
	}

	if (status == SFERR_EOF)
		/* treat EOF's as okay status */
		status = 0;

	free((char *)buf);
	return status;
}

/*
 * Read sf_readfile and return the next packet.  Return the header in hdr 
 * and the contents in buf.  Return 0 on success, SFERR_EOF if there were 
 * no more packets, and SFERR_TRUNC if a partial packet was encountered.
 */
int
sf_next_packet(hdr, buf, buflen)
	struct packet_header *hdr;
	u_char *buf;
	int buflen;
{
	FILE *fp = sf_readfile;

	/* read the stamp */
	if (fread((char *)hdr, sizeof(struct packet_header), 1, fp) != 1) {
		/* probably an EOF, though could be a truncated packet */
		return SFERR_EOF;
	}

	if (sf_swapped) {
		/* these were written in opposite byte order */
		hdr->caplen = SWAPLONG(hdr->caplen);
		hdr->len = SWAPLONG(hdr->len);
		hdr->ts.tv_sec = SWAPLONG(hdr->ts.tv_sec);
		hdr->ts.tv_usec = SWAPLONG(hdr->ts.tv_usec);
	}

	if (hdr->caplen > buflen)
		return SFERR_BADF;

	/* read the packet itself */

	if (fread((char *)buf, hdr->caplen, 1, fp) != 1)
		return SFERR_TRUNC;

	return 0;
}

/*
 * Initialize so that sf_write() will output to the file named 'fname'.
 */
void
sf_write_init(fname, linktype)
	char *fname;
	int linktype;
{
	if (fname[0] == '-' && fname[1] == '\0')
		sf_writefile = stdout;
	else {
		sf_writefile = fopen(fname, "w");
		if (sf_writefile == 0) {
			perror(fname);
			exit(1);
		}
	}
	(void)sf_write_header(sf_writefile, linktype);
}

/*
 * Output a packet to the intialized dump file.
 */
void
sf_write(sp, tvp, length, caplen)
	u_char *sp;
	struct timeval *tvp;
	int length;
	int caplen;
{
	struct packet_header h;

	h.ts.tv_sec = tvp->tv_sec;
	h.ts.tv_usec = tvp->tv_usec;
	h.len = length;
	h.caplen = caplen;

	(void)fwrite((char *)&h, sizeof h, 1, sf_writefile);
	(void)fwrite((char *)sp, caplen, 1, sf_writefile);
}

void
sf_err(code)
	int code;
{
	switch (code) {
	case SFERR_BADVERSION:
		error("archaic file format");
		/* NOTREACHED */

	case SFERR_BADF:
		error("bad dump file format");
		/* NOTREACHED */

	case SFERR_TRUNC:
		error("truncated dump file");
		/* NOTREACHED */

	case SFERR_EOF:
		error("EOF reading dump file");
		/* NOTREACHED */

	default:
		error("unknown dump file error code in sf_err()");
		/* NOTREACHED */
	}
	abort();
}
