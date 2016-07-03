/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Don Ahn.
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
 *	@(#)fd.c	7.3 (Berkeley) 5/25/91
 */

/****************************************************************************/
/*                        standalone fd driver                               */
/****************************************************************************/
#include "sys/param.h"
#include "disklabel.h"
#include "fdreg.h"
#include "isa_stdports.h"
#include "nec765.h"
#include "saio.h"
#include "machine/inline/io.h"

#define NUMRETRY 10
#define NFD 2
#define FDBLK 512

extern struct disklabel disklabel;

struct fd_type {
	int	sectrac;		/* sectors per track         */
	int	secsize;		/* size code for sectors     */
	int	datalen;		/* data len when secsize = 0 */
	int	gap;			/* gap len between sectors   */
	int	tracks;			/* total num of tracks       */
	int	size;			/* size of disk in sectors   */
	int	steptrac;		/* steps per cylinder        */
	int	trans;			/* transfer speed code       */
};

struct fd_type fd_types[] = {
 	{ 18,2,0xFF,0x1B,80,2880,1,0 },	/* 1.44 meg HD 3.5in floppy    */
	{ 15,2,0xFF,0x1B,80,2400,1,0 },	/* 1.2 meg HD floppy           */
	/* need 720K 3.5in here as well */
#ifdef noway
	{ 9,2,0xFF,0x23,40,720,2,1 },	/* 360k floppy in 1.2meg drive */
	{ 9,2,0xFF,0x2A,40,720,1,1 },	/* 360k floppy in DD drive     */
#endif
};


/* state needed for current transfer */
static int probetype;
static int fd_type;
static int fd_retry;
static int fd_status[7];

static int fdc = IO_FD1;	/* floppy disk base */

/* Make sure DMA buffer doesn't cross 64k boundary */
/* char bounce[FDBLK];*/
static char *bounce = (char *)0x200000;
static int readtrack = -1;


/****************************************************************************/
/*                               fdstrategy                                 */
/****************************************************************************/
int
fdstrategy(io,func)
register struct iob *io;
int func;
{
	char *address;
	long nblocks,blknum;
 	int unit, iosize;

#if	DEBUG > 2
printf("fdstrat ");
#endif
	unit = io->i_unit;

	/*
	 * Set up block calculations.
	 */
        iosize = io->i_cc / FDBLK;
	blknum = (unsigned long) io->i_bn * DEV_BSIZE / FDBLK;
 	nblocks = fd_types[fd_type].size /*  disklabel.d_secperunit */;
	if ((blknum + iosize > nblocks) || blknum < 0) {
#if DEBUG > 8
		printf("bn = %d; sectors = %d; type = %d; fssize = %d ",
			blknum, iosize, fd_type, nblocks);
                printf("fdstrategy - I/O out of filesystem boundaries\n");
#endif
		return(-1);
	}

	address = (char *)io->i_ma;
        while (iosize > 0) {
                if (fdio(func, unit, blknum, address))
                        return(-1);
		iosize--;
		blknum++;
                address += FDBLK;
        }
        return(io->i_cc);
}

static int ccyl = -1;

int
fdio(func, unit, blknum, address)
int func,unit,blknum;
char *address;
{
	int i,j, cyl, sectrac,sec,head,numretry;
	struct fd_type *ft;
	int s;

#if	DEBUG > 1
	printf("fdio ");
#endif
 	ft = &fd_types[fd_type];

 	sectrac = ft->sectrac;
	cyl = blknum / (sectrac*2);
	sec = blknum %  (sectrac * 2);
	head = sec / sectrac;
	sec = sec % sectrac + 1;
#if	DEBUG > 3
	printf("sec %d hd %d cyl %d ", sec, head, cyl);
#endif
	numretry = NUMRETRY;

	if (func == F_WRITE)
		bcopy(address, bounce+FDBLK*(sec-1), FDBLK);

retry:
#if	DEBUG > 3
	printf("%x ", inb(fdc+fdsts));
#endif
	if(numretry != NUMRETRY)
		printf("fdio: bn %d retry\n", blknum);
	if (ccyl != cyl) {
		ccyl = -1;
		/* attempt seek to desired cylinder */
		out_fdc(NE7CMD_SEEK); out_fdc(unit); out_fdc(cyl);

		if (waitseek(cyl) == 0) {
			numretry--;
			recalibrate(unit);
			if (numretry) goto retry;
			printf("fdio: seek to cylinder %d failed\n", cyl);
#if	DEBUG > 3
			printf("unit %d, type %d, sectrac %d, blknum %d\n",
				unit,fd_type,sectrac,blknum);
#endif
			return -1;
		}
	}
	ccyl = cyl;

	/* check if in read track buffer to avoid transfer */
	if (func == F_READ && readtrack == ((cyl<<1)+ head) && sec < 16) {
		bcopy(bounce + FDBLK*(sec-1), address, FDBLK);
		return 0;
	}

	/* set up transfer and issue command */
	if (func == F_READ && numretry == NUMRETRY && !probetype) {
		fd_dma(1, (int)bounce, (int)sectrac*FDBLK);
		readtrack = -1;
		s = 1;
#define	NE7CMD_READTRACK	0x62
	 	out_fdc(NE7CMD_READTRACK);
	} else {
		fd_dma(func == F_READ, (int)bounce + FDBLK*(sec-1), FDBLK);
		s = sec;

		if (func == F_READ)
			out_fdc(NE7CMD_READ);
		else
			out_fdc(NE7CMD_WRITE);
	}
	out_fdc(head << 2 | unit);	/* head & unit */
	out_fdc(cyl);			/* track */
	out_fdc(head);
	out_fdc(s);			/* sector */
	out_fdc(ft->secsize);		/* sector size */
	out_fdc(sectrac);		/* sectors/track */
	out_fdc(ft->gap);		/* gap size */
	out_fdc(ft->datalen);		/* data length */

	waitio('r');
#if	DEBUG > 3
	printf("d ");
#endif
	if (fd_status[0] & 0xf8) {

		if (!probetype) {
#if	DEBUG > 3
			printf("FD err %lx %lx %lx %lx %lx %lx %lx\n",
			fd_status[0], fd_status[1], fd_status[2], fd_status[3],
			fd_status[4], fd_status[5], fd_status[6] );
#endif
		} else
			return(-1);

		numretry--;
		recalibrate(unit);
		if (numretry) goto retry;
		printf("fdio: i/o error bn %d\n", blknum);
		return -1;
	}
#if	DEBUG > 3
	printf("e ");
#endif
	if (func == F_READ) {
		bcopy(bounce+ (FDBLK*(sec-1)), address, FDBLK);
		if(numretry == NUMRETRY) 
			readtrack = (cyl<<1) + head;
	}
	return 0;
}

/*
 * wait for seek to cylinder to  complete. return success.
 */
#define	NE7_ST0SE	0x20	/* seek ended */
int
waitseek(int cyl) {
	int attempts, st0, pcn;

	for (attempts = 100000; attempts > 0; attempts--) {
		/* sense interrupt, get status */
		out_fdc(NE7CMD_SENSEI);
		st0 = in_fdc();

		/* controller ready to talk?, if so get present cylinder */
		if (st0 == -1)
			continue;
		pcn = in_fdc();
		if (pcn == -1)
			continue;

		/* seek end? */
		if ((st0 & NE7_ST0SE) == 0)
			continue;

		/* on cylinder? */
		if (pcn != cyl)
			return (0);
		else
			return (1);
	}
	return (0);
}

/*
 * Force drive to start of unit and re seek.
 */
recalibrate(int unit) {

	/* reset head load, head unload, step rate, and DMA mode */
	out_fdc(NE7CMD_SPECIFY); out_fdc(0xDF); out_fdc(2);

	/* seek to cylinder 0 */
	out_fdc(NE7CMD_RECAL); out_fdc(unit);

	/* wait for results */
	(void)waitseek(0);

	/* force a seek on next transfer */
	ccyl = -1;
}

/****************************************************************************/
/*                             fdc in/out                                   */
/****************************************************************************/
int
in_fdc()
{
	int i;
	/*int cnt = 100000;*/
	while (/*cnt-- > 0 &&*/ (i = inb(fdc+fdsts) & 192) != 192) if (i == 128) return -1;
	/*if(cnt <= 0)
		return(-1);*/
	return inb(0x3f5);
}

waitio(p)
{
char c;
int n;
int i;

#if	DEBUG > 0
	printf("w ");
#endif
	n = in_fdc();
	if (n == -1) return;
	fd_status[0] = n;
#if	DEBUG > 1
	printf("%c%x.", p, n);
#endif
	for(i=1;i<7;i++) {
		fd_status[i] = in_fdc();
#if	DEBUG > 2
		printf("%x.", fd_status[i]);
#endif
	}
#if	DEBUG > 2
	printf(" ");
#endif
}

out_fdc(x)
int x;
{
	int r;
	int cnt=100000;
	do {
		if (cnt-- == 0) return(-1);
		r = (inb(fdc+fdsts) & 192);
		if (r==128) break;
	} while (1);
	outb(0x3f5, x);
}


/****************************************************************************/
/*                           fdopen/fdclose                                 */
/****************************************************************************/
fdopen(io)
	register struct iob *io;
{
	int unit, type, i;
	struct fd_type *ft;
	char buf[512];

	unit = io->i_unit;
	io->i_boff = 0;		/* no disklabels -- tar/dump wont work */
#if DEBUG > 1
	printf("fdopen %d ", unit);
#endif
 	ft = &fd_types[0];

	/* Try a reset, keep motor on */
	outb(0x3f2,0);
	for(i=0; i < 100000; i++);
	outb(0x3f2, unit | (unit  ? 32 : 16) );
	for(i=0; i < 100000; i++);
	outb(0x3f2, unit | 0xC | (unit  ? 32 : 16) );
	outb(0x3f7, ft->trans);

	recalibrate(unit);

	/*
	 * discover type of floppy
	 */
	probetype = 1;
	for (fd_type = 0; fd_type < sizeof(fd_types)/sizeof(fd_types[0]);
		fd_type++, ft++) {
		/*for(i=0; i < 100000; i++);
		outb(0x3f7,ft->trans);
		for(i=0; i < 100000; i++);*/
		if (fdio(F_READ, unit, ft->sectrac - 1, buf) >= 0){
			probetype = 0;
			return(0);
		}
	}

	printf("failed fdopen");
	return(-1);
}


/*
 * set up DMA read/write operation and virtual address addr for nbytes
 */
int
fd_dma(int read, int addr, int nbytes)
{
	/* Set read/write bytes */
	if (read) {
		outb(0xC, 0x46); outb(0xB, 0x46);
	} else {
		outb(0xC, 0x4A); outb(0xB, 0x4A);
	}

	/* Send start address */
	outb(0x4, addr); outb(0x4, (addr>>8)); outb(0x81, (addr>>16));

	/* Send count */
	nbytes--;
	outb(0x5, nbytes); outb(0x5, (nbytes>>8));

	/* set channel 2 */
	outb(0x0A, 2);
}
