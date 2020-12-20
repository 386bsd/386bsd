/*
 * Adaptec 1542 SCSI driver for 386bsd
 *
 * $Id: as.c,v 1.1 95/01/21 23:00:53 bill Exp Locker: bill $
 *
 * Pace Willisson     pace@blitz.com    March 28, 1992
 *
 * Placed in the public domain with NO WARRANTIES, not even the
 * implied warranties for MERCHANTABILITY or FITNESS FOR A 
 * PARTICULAR PURPOSE.
 *
 * The the tape stuff still needs a lot of working concerning
 * file marks, end of tape handling and rewinding.
 *
 * minor number bits:
 *
 *      7  6  5  4  3  2  1  0
 *                     +-----+  partition number
 *            +-----+           scsi target number
 *      +--+                    controller number
 *
 * For tape drives, set the partition number to 0 for regular,
 * 1 for no rewind.
 *
 * Only supports LUN 0.
 */

/* manufacturer default configuration: */
static char *as_config =
	"as 4 13 1 (0x330).	# Adaptec 1542 SCSI, $Revision: 1.1 $";
#define	NAS	1

/* maximum configuration, regardless of conflicts: 
	"as 2 13 4 (0x330) (0x334) (0x230) (0x234) (0x130) (0x134).
	# Adaptec 1542 SCSI, $Revision: 1.1 $" */

#include "sys/param.h"
#include "sys/file.h"
#include "sys/stat.h"
#include "sys/ioctl.h"
#include "sys/errno.h"
#include	"proc.h" /* PCATCH et al */
#include "uio.h"
#include "dkbad.h"
#include "systm.h"
#include "disklabel.h"
#include "buf.h"
#include "malloc.h"
#include "prototypes.h"
#include "machine/inline/io.h"
#include "isa_driver.h"
#include "isa_irq.h"
#include "sys/syslog.h"
#include "vm.h"
#include "kmem.h"
#include "kernel.h"
#include "modconfig.h"

#include "asreg.h"

static int asstrategy ();
static int asabort ();
extern int hz;

int asverbose = 0;

/* target id 7 is the controller itself */
#define NTARGETS 8

struct mailbox_entry mailboxes[NAS * NTARGETS * 2];

#define b_cylin	b_resid /* fake cylinder number for disksort */

/* maximum scatter list size for Adaptec controller */
#define NSCATTER 17

/* this array must reside in contiguous physical memory */
struct asinfo {
	dev_t dev;
	struct buf requests;
	struct mailbox_entry *mailbox;
	int active;
	struct ccb ccb;
	unsigned int ccb_phys;
	char scatter_list[NSCATTER * 6];

	struct disklabel label;
	struct dos_partition dospart[NDOSPART];
	int have_label;

	int scsi_lock;
	struct buf *scsi_bp;
	int scsi_cdb_len;
	unsigned char scsi_cdb[MAXCDB];

	int tape;		/* sequential */
	int disk;		/* nonsequential */
	int read_only;		/* CDROM */
	int removable;		/* unsupported and tested */
	char vendor[9];
	char model[17];
	char revision[5];
	int bs;			/* device block size */

	int open_lock;
	int open;
	int units_open;

	int wlabel;

	int retry_count;
	int start_time;
	int restart_pending;
	int mbi_status;		/* mailbox in status */
	vm_offset_t bounce;	/* ISA bounce buffer for systems with > 16MB */
	int isbounced;

} asinfo[NAS * NTARGETS];
extern caddr_t dumpISA;

#define dev_part(dev) (minor (dev) & 7)
#define dev_target(dev) ((minor (dev) >> 3) & 7)
#define dev_rewind(dev) ((minor (dev) & 1) == 0)
#define dev_ctlr(dev) ((minor (dev) >> 6) & 3)
#define dev_to_as(dev)  (asinfo + (dev_ctlr(dev) * NTARGETS + dev_target(dev)))

#define makeasdev(major, ctlr, target, part) \
	makedev ((major), ((ctlr) << 6) || ((target) << 3) | (part))

static int as_port[NAS] = {0};

/* default ports for 1542 controller */
static int def_ports[] = { 0x330, 0x334, 0x230, 0x234, 0x130, 0x134 };

static void asintr (int ctl /* dev_t dev*/);
static  asintr1 (struct asinfo *as, int val);
static int	asprobe(struct isa_device *);
static void	asattach(struct isa_device *);
static  void asstart (struct asinfo *as);
static  asdone (struct asinfo *as, int restart);
static int as_get_byte (int port);
static int as_put_byte (int port, int val);
extern int biomask; /* XXX */

struct	isa_driver asdriver = {
	asprobe, asattach, asintr, "as", &biomask
};

/* default per device */
static struct isa_device as_default_devices[] = {
	{ &asdriver,    0x330, 0, -1,  0x00000,     0,  0 },
	{ &asdriver,    0x334, 0, -1,  0x00000,     0,  0 },
	{ &asdriver,    0x230, 0, -1,  0x00000,     0,  0 },
	{ &asdriver,    0x234, 0, -1,  0x00000,     0,  0 },
	{ &asdriver,    0x130, 0, -1,  0x00000,     0,  0 },
	{ &asdriver,    0x134, 0, -1,  0x00000,     0,  0 },
	{ 0 }
};

static int aspartition(struct asinfo *as, struct buf *bp, int *blknop);
static int asmakescatter(struct asinfo *as, struct buf *bp, int phys);
static struct asinfo *asscan (dev_t dev);
static void asissue(struct asinfo *as);
static void asmakecdb(struct asinfo *as, int blkno, int count, int dir);
static void asmakeccb(struct asinfo *as, int nscatter, dev_t dev, int count, int dir);

static u_char asctlcmd_nop[] = { 0 };
static u_char asctlcmd_mailboxinit[] = { 1, 0, 0, 0, 0 };
static u_char asctlcmd_conf[] = { 0xb };

/* */
static int
asctlcmd(int ctl, u_char *cmdstring, int n, u_char *results) {
	int s, i, c, port = as_port[ctl] ;

	c = *cmdstring;
	s = splbio ();

	if (inb (port + AS_INTR) & AS_INTR_HACC)
		outb (port + AS_CONTROL, AS_CONTROL_IRST);
	/* if startscsi or enable mailbox out interrupt, don't wait for idle */
	if (c != 2 && c != 5) {
		for (i = 1000; i > 0; i--) {
			if ((inb (port + AS_STATUS) & AS_STATUS_IDLE))
				break;
			DELAY (100);
		}
		/* timeout waiting for idle */
		if (i == 0) {
printf("\nlost idle %x\n", c);
			splx(s);
			return(-1);
		}

		/* if command expects results, obtain them */
		if (results) {
			int tmp;

			/* get bytes until idle */
			while ((inb (port + AS_STATUS) & AS_STATUS_DF)) {
				if(as_get_byte(port) < 0) {
printf("\nno prior results %x\n", c);
					splx(s);
					return(-1);
				}
	
			}
		}
	}

	/* issue command to controller */
	for (i = 0; i < n ; i++) {
		if (as_put_byte (port, *cmdstring++) < 0) {
printf("\nfailed command %x\n", c);
			splx(s);
			return(-1);
		}
	}

	/* if command expects results, obtain them */
	if (results) {
		int tmp;

		/* get bytes until idle */
		while((tmp = as_get_byte(port)) > 0) {
			*results++ = tmp;
			DELAY(1000);
			if ((inb (port + AS_STATUS) & (AS_STATUS_IDLE|AS_STATUS_DF)) == AS_STATUS_IDLE)
				break;
		}
		if (tmp < 0) {
printf("\nno results %x\n", c);
			splx(s);
			return(-1);
		}
	}

DELAY (100000);
/*printf("intr %x sts %x ", inb(port+ AS_INTR), inb(port+AS_STATUS));*/
	/* if startscsi or enable mailbox out interrupt, don't wait for hacc */
	if (c != 2 && c != 5 /* && c != 0 */) {
		for (i = 10000; i > 0; i--) {
			if (inb(port + AS_INTR) & AS_INTR_HACC)
				break;
			DELAY (100);
		}
		if (inb (port + AS_INTR) & AS_INTR_HACC)
			outb (port + AS_CONTROL, AS_CONTROL_IRST);
		/* timeout waiting for idle */
		if (i == 0) {
printf("\nno hacc %x\n", c);
#ifdef nope
1542C generates a HACC on NOP command, B does not.
#endif
			splx(s);
			return(-1);
		}
	}
	splx(s);
	return(0);
}

static int
asprobe (struct isa_device *dvp)
{
	int i;
	u_char cfg[3];
	unsigned int physaddr;
	int val;
	int s, ctl, ap;

	/* wildcard ? */
	if (dvp->id_unit == '?') {
		/* first, find a controller index */
		for (i = 0; as_port[i] != 0; i++) ;
		if (i > NAS)
			return (0);
		dvp->id_unit = i;

		/* next, look for possible controllers */
		for (i = 0 ; i < sizeof(def_ports)/sizeof(def_ports[0]) ; i++) {
			if (def_ports[i]) {
				dvp->id_iobase = def_ports[i];
				if (asprobe(dvp))
					return (1);
			}
			def_ports[i] = 0;
		}
		return (0);
	}

	ctl = dvp->id_unit;
	ap = as_port[ctl] = dvp->id_iobase;

	if (inb(ap + AS_STATUS) == 0xff)
		goto fail;
	outb (ap + AS_CONTROL, AS_CONTROL_SRST);
	DELAY (30000);

	/* for (i = 10000; i > 0; i--) {
		if ((inb (ap + AS_STATUS) & (AS_STATUS_IDLE | AS_STATUS_INIT))
			!= AS_STATUS_IDLE | AS_STATUS_INIT)
			break;
		DELAY (100);
	}*/

	for (i = 0; i < NTARGETS; i++) {
		asinfo[ctl*NTARGETS+i].mailbox = &mailboxes[ctl*NTARGETS+i];
		asinfo[ctl*NTARGETS+i].ccb_phys = vtophys (&asinfo[ctl*NTARGETS+i].ccb);
	}

	
	physaddr = vtophys (mailboxes);
	
	asctlcmd_mailboxinit[1] = NTARGETS;
	asctlcmd_mailboxinit[2] = physaddr >> 16;
	asctlcmd_mailboxinit[3] = physaddr >> 8;
	asctlcmd_mailboxinit[4] = physaddr;
/*printf("intr %x sts %x ", inb(ap+ AS_INTR), inb(ap+AS_STATUS));*/
	if (asctlcmd(ctl, asctlcmd_nop, sizeof(asctlcmd_nop), 0) == 0 &&
	    asctlcmd(ctl, asctlcmd_mailboxinit, sizeof(asctlcmd_mailboxinit), 0) == 0) {

		if ((val = inb (ap + AS_STATUS)) & AS_STATUS_INIT) {
			printf ("as: mailbox init error: 0x%x\n", val);
			goto fail;
		}
		if (asctlcmd(ctl, asctlcmd_nop, sizeof(asctlcmd_nop), 0) == 0 &&
	    		asctlcmd(ctl, asctlcmd_conf, sizeof(asctlcmd_conf), cfg) == 0) {
			dvp->id_drq = ffs(cfg[0]) - 1;
			dvp->id_irq = cfg[1]<<9;
			return(1);
		}
	}
fail:
	as_port[ctl] = 0;
	return (0);
}

static void
asattach (struct isa_device *dvp)
{
	int i;
	unsigned int physaddr;
	int val;
	int s, ctl;

	ctl = dvp->id_unit;
	if (ctl > 0)
		printf("controller %d ", ctl + 1);
	isa_dmacascade(dvp->id_drq);

	/* take out of the available list */
	for (i = 0 ; i < sizeof(def_ports)/sizeof(def_ports[0]) ; i++)
		if (dvp->id_iobase == def_ports[i])
			def_ports[i] = 0;
}

static int
ascmd (as, bp, direction, count, retrycount)
struct asinfo *as;
struct buf *bp;
int direction;
int count;
int retrycount;
{
	int err;

	do {
		if (asverbose)
			printf ("ascmd ");
		bp->b_bcount = count;
		bp->b_error = 0;
		bp->b_flags &= ~(B_READ | B_ERROR | B_DONE);
		if (direction == B_READ)
			bp->b_flags |= B_READ;

		bp->b_dev = as->dev;
		bp->b_blkno = 0;

		as->scsi_bp = bp;
		/* scsi_cdb, scsi_cdb_len set up by caller */

		asstrategy (bp);
		err = biowait (bp);
		as->scsi_bp = NULL;
		
	} while (err && --retrycount);

	return (err);
}

static asstring (dest, src, size)
char *dest;
char *src;
int size;
{
	size--;
	memcpy (dest, src, size);
	while (size > 0 && dest[size - 1] == ' ')
		size--;
	dest[size] = 0;
}

static int
asopen(dev_t dev, int flag, int fmt, struct proc *pr)
{
	struct asinfo *as;
	unsigned int physaddr;
	struct buf *bp = NULL;
	int retry;
	unsigned char *cdb;
	char *p, *q;
	int n;
	int error;
	char vendor[9];
	char model[17];
	int disksize, ctl;

	ctl = dev_ctlr(dev);
/*printf("as %x: ctl %d tgt %d ", dev, ctl, dev_target(dev));*/
	if (ctl > NAS || as_port[ctl] == 0 || dev_target (dev) >= NTARGETS)
		return (ENXIO);

	as = dev_to_as(dev);
	as->dev = dev;

	while (as->open_lock)
		if (error = tsleep ((caddr_t)as, PWAIT|PCATCH, "scsiopen", 0))
			return (error);

	if (as->open) {
		if (as->tape)
			return (EBUSY);

/*printf("as part %d ", dev_part(dev));*/
		if (as->have_label == 0 && dev_part (dev) != 3)
			return (ENXIO);
		
		as->units_open |= 1 << dev_part (dev);
		return (0);
	}

	as->open_lock = 1;

	/* it seems like we might have to block here in case someone
	 * opens the device just after someone else closes
	 */
	while (as->scsi_lock)
		if (error = tsleep ((caddr_t)as, PWAIT|PCATCH, "scsicmd", 0))
			return (error);

	as->scsi_lock = 1;

	error = EIO;

	as->have_label = 0;
	as->tape = 0;
	as->disk = 0;
	as->read_only = 0;
	as->removable = 0;
	memcpy(vendor, as->vendor, sizeof(vendor));
	memcpy(model, as->model, sizeof(model));
	as->vendor[0] = 0;
	as->model[0] = 0;
	as->revision[0] = 0;

	bp = geteblk (DEV_BSIZE);

	if (asverbose) {
		printf ("openbuf = 0x%x phys 0x%x\n",
			bp->b_un.b_addr, vtophys (bp->b_un.b_addr));
		printf ("mailboxes = 0x%x\n", mailboxes);
	}

	/* first, find out if a device is present, and just what it is */
	as->scsi_cdb_len = 6;
	cdb = as->scsi_cdb;
	(void)memset (cdb, 0, 6);
	cdb[0] = 0x12; /* INQUIRY */
	cdb[4] = 255; /* allocation length */
	if (error = ascmd (as, bp, B_READ, DEV_BSIZE, 2))
		/* does not respond to inquiry, obviously not CCS, give up */
		goto done;

	
	/* blather on console about it */
	p = bp->b_un.b_addr;
	if (asverbose) {
		printf ("inquiry: ");
		for (n = 0; n < 20; n++)
			printf ("%x ", p[n] & 0xff);
		printf ("\n");
		for (n = 0; n < 40; n++) {
			if (p[n] >= ' ' && p[n] < 0177)
				printf ("%c", p[n]);
			else
				printf (".");
		}
		printf ("\n");
	}

	switch (p[0]) {
	case 0: /* normal disk */
	case 4: /* write once disk */
		as->disk = 1;
		break;
	case 5: /* read only disk */
		as->read_only = 1;
		as->disk = 1;
		break;
	case 1: /* tape */
		as->tape = 1;
		break;
	case 0x7f:
		printf ("logical unit not present\n");
		goto done;
	default:
		printf ("unknown peripheral device type: 0x%x\n", p[0]);
		goto done;
	}

	as->removable = (p[1] & 0x80) ? 1 : 0;

	n = p[4] & 0xff;
	if (n >= 31) {
		asstring (as->vendor, p + 8, sizeof as->vendor);
		asstring (as->model, p + 16, sizeof as->model);
		asstring (as->revision, p + 32, sizeof as->revision);
	}

	if(memcmp(as->vendor,vendor, sizeof(vendor)) != 0 ||
		memcmp(as->model,model, sizeof(model)) != 0) {
		printf("as: attached tgt %d <%s %s %s> ", 
			dev_target(dev), as->vendor, as->model, as->revision);
		if (as->read_only) printf("readonly ");
		if (!as->removable) printf("winchester ");
		if (as->tape) printf("tape ");
		if (as->disk) printf("disk ");
		printf("(as%d)", (as->dev & 0xff) >> 3);
		printf("\n");
	}

	/* probe for desired block size */

	/* assume default of 512, except if CDROM (2048) */
	if (as->read_only)
		as->bs = 2048;
	else
		as->bs = 512;

	(void)memset(cdb, 0, 6);
	cdb[0] = 0x1A;  /* SCSI_MDSENSE */
	cdb[4] = 255;
	if (as->tape && ascmd (as, bp, B_READ, 12, 2) == 0)  {
		int minblk, maxblk;

#ifdef notdef
		/* blather about device more */
		if(memcmp(as->vendor,vendor, sizeof(vendor)) != 0 ||
			memcmp(as->model,model, sizeof(model)) != 0) {
			p = bp->b_un.b_addr;
			printf("as%d: data len %d medium %d speed/bufmode 0x%x desc len %d\n",
				dev_target(dev), p[0], p[1], p[2], p[3]);
			printf("as%d: density %d nblocks %d block len %d\n",
				dev_target(dev), p[4],
				(long)p[5]*65536+p[6]*256+p[7],
				(long)p[9]*65536+p[10]*256+p[11]);
		}
#endif
		
		/* obtain possible block sizes */
		(void)memset(cdb, 0, 6);
		cdb[0] = 0x05; /* SCSI_RDLIMITS; */
		if (ascmd (as, bp, B_READ, 12, 2) == 0) {
			p = bp->b_un.b_addr;
			minblk = p[4]*256+p[5];
			maxblk = p[1]*65536+p[2]*256+p[3];
#ifdef notdef
			if(memcmp(as->vendor,vendor, sizeof(vendor)) != 0 ||
				memcmp(as->model,model, sizeof(model)) != 0) {
				printf("as%d: limits: min block len %ld  max block len %ld\n",
					dev_target(dev), minblk, maxblk);
			}
#endif
			if ( minblk == maxblk )
				as->bs = minblk;
			else if (as->tape)
				as->bs = 1;
		}
	}
	
	if (as->tape &&  dev_part(dev)) {
		error = EIO;
		goto done;
	}

	as->scsi_cdb_len = 10;
	(void)memset(cdb, 0, 10);
	cdb[0] = 0x25;  /* SCSI_READCAPACITY */
	disksize = 0;
	if (as->disk && ascmd (as, bp, B_READ, 12, 2) == 0)  {
		p = bp->b_un.b_addr;
		disksize = ntohl(*(long *)p);
		as->bs = ntohl(*(long *)(p+4));
		
	}

if(asverbose)
	printf("block size %d disksize %d ", as->bs, disksize);


	/* for standard disk, negotiate block size */
	if (as->read_only == 0 && as->disk) {
		/* do mode select to set the logical block size */
		as->scsi_cdb_len = 6;
		cdb = as->scsi_cdb;
		(void)memset(cdb, 0, 6);
		cdb[0] = 0x15; /* MODE SELECT */
		cdb[4] = 12; /* parameter list length */
	
		p = bp->b_un.b_addr;
		(void)memset(p, 0, 12);
		p[3] = 8; /* block descriptor length */
		n = as->bs == 1 ? 0 : as->bs;
		p[9] = n >> 16;
		p[10] = n >> 8;
		p[11] = n;
	
		(void) ascmd (as, bp, B_WRITE, 12, 2);
	}

	/* device online and ready? */
	as->scsi_cdb_len = 6;
	(void)memset(cdb, 0, 6);
	cdb[0] = 0x00;  /* SCSI_UNITRDY */
	if (error = ascmd (as, bp, B_READ, 12, 2)) {
		printf("as%d: drive not online\n", dev_target(dev));
		goto done;
	}

	if (as->disk && as->read_only == 0) {
		/* read disk label */
		(void)memset ((caddr_t)&as->label, 0, sizeof as->label);
		as->label.d_secsize = as->bs;
		as->label.d_secpercyl = 64*32;
		as->label.d_type = DTYPE_SCSI;

		
		/* read label using "d" partition */
		if ((p = readdisklabel (dev
			/* makeasdev (major (dev), dev_ctlr (dev), dev_target (dev), 3)*/ ,
			asstrategy, &as->label, as->dospart, 0, 0)) == NULL){
			as->have_label = 1;
		} else {
			if (disksize) {
				as->label.d_subtype = DSTYPE_GEOMETRY;
				as->label.d_npartitions = 3;
				/* partition 0  holds bios, partition 1 ESDI */
				as->label.d_partitions[2].p_size = disksize;
				as->label.d_partitions[2].p_offset = 0;
			}
			if (asverbose || dev_part (dev) != 3)
				printf ("error reading label: %s\n", p);
			if (dev_part (dev) != 3) {
				error = EINVAL;
				goto done;
			}
		}
	}

	/* may want to set logical block size here ? */
	error = 0;

 done:
	if (bp) {
		bp->b_flags |= B_INVAL | B_AGE;
		brelse (bp);
	}

	if (error == 0) {
		as->open = 1;
		/* if more memory than adapter supports, allocate a bounce buf */
		if (physmem > (16*1024*1024/NBPG))
			as->bounce = kmem_alloc(kmem_map, 64*1024, 0);
		else
			as->bounce = 0;
	}

	as->open_lock = 0;
	as->scsi_lock = 0;
	wakeup ((caddr_t)as);

	return (error);
}

static int
asclose(dev_t dev, int flag, int fmt, struct proc *pr)
{
	struct asinfo *as;
	int error = 0;
	unsigned char *cdb;
	struct buf *bp;
	int n;

	as = dev_to_as(dev);

	while (as->open_lock)
		if (error = tsleep ((caddr_t)as, PWAIT|PCATCH, "scsiclose", 0))
			return (error);

	as->open_lock = 1;

	if (as->tape) {
		while (as->scsi_lock)
			if (error = tsleep ((caddr_t)as, PWAIT|PCATCH,
					    "scsicmd", 0))
				return (error);

		as->scsi_lock = 1;
		
		bp = geteblk (DEV_BSIZE);

             if ((flag & FWRITE) != 0) {
                             /* presume user will use tape again */
                     as->scsi_cdb_len = 6;
                     cdb = as->scsi_cdb;
                     (void) memset (cdb, 0, 6);
                     cdb[0] = 0x10; /* write filemarks */
                     cdb[4] = 1; /* one of them */
                     error = ascmd (as, bp, B_READ, 0, 1);
             }
             if (dev_rewind (dev) || error) {
                     if ( error == 0 && (flag & FWRITE) != 0) {
                                     /* presumption error correction */
                             as->scsi_cdb_len = 6;
                             cdb = as->scsi_cdb;
                             (void) memset (cdb, 0, 6);
                             cdb[0] = 0x10; /* write filemarks */
                             cdb[4] = 1; /* one of them */
                             error |= ascmd (as, bp, B_READ, 0, 1);
                     }
                     as->scsi_cdb_len = 6;
                     cdb = as->scsi_cdb;
                     (void) memset (cdb, 0, 6);
                     cdb[0] = 0x1; /* rewind */
                     cdb[1] = 1; /* don't wait until done */
                     error |= ascmd (as, bp, B_READ, 0, 1);
             }
#ifdef notdef
		} else {
			cdb[0] = 0x11; /* backspace */
			cdb[1] = 1; /* look at filemarks (instead of blocks) */
			n = -1;
			cdb[2] = n >> 16;
			cdb[3] = n >> 8;
			cdb[4] = n;
			error = ascmd (as, bp, B_READ, 0, 1);
		} 
#endif

		bp->b_flags |= B_INVAL | B_AGE;
		brelse (bp);

		as->scsi_lock = 0;
	}

	as->units_open &= ~(1 << dev_part (dev));

	if (as->units_open == 0) {
		if (as->bounce)
			kmem_free(kmem_map, as->bounce, 64*1024);
		as->bounce = 0;
		as->open = 0;
	}

	as->open_lock = 0;

	wakeup ((caddr_t)as);

	return (error);
}

static int
asioctl(dev_t dev, int cmd, caddr_t addr, int flag, struct proc *p)
{
	struct scsicmd *cmdp;
	struct asinfo *as;
	int ccblen;
	struct buf *bp;
	int error = 0;
	int direction;
	struct disklabel *dl;
	int old_wlabel;

	as = dev_to_as(dev);

	switch (cmd) {
	case DIOCGDINFO:
		*(struct disklabel *)addr = as->label;
		break;

        case DIOCSDINFO:
                if ((flag & FWRITE) == 0) {
                        error = EBADF;
			break;
		}
		dl = (struct disklabel *)addr;
		if (error = setdisklabel(&as->label, dl, 0, as->dospart))
			break;
		as->have_label = 1;
                break;

        case DIOCGPART:
                ((struct partinfo *)addr)->disklab = &as->label;
                ((struct partinfo *)addr)->part =
                    &as->label.d_partitions[dev_part(dev)];
                break;

        case DIOCWLABEL:
                if ((flag & FWRITE) == 0) {
                        error = EBADF;
			break;
		}
		as->wlabel = *(int *)addr;
                break;

        case DIOCWDINFO:
                if ((flag & FWRITE) == 0) {
                        error = EBADF;
			break;
		}

		dl = (struct disklabel *)addr;

		if (error = setdisklabel (&as->label, dl, 0, as->dospart))
			break;

		as->have_label = 1;

		old_wlabel = as->wlabel;
		as->wlabel = 1;
		error = writedisklabel(dev, asstrategy, &as->label,
				as->dospart);
		as->wlabel = old_wlabel;
                break;

	case SCSICMD:
		cmdp = (struct scsicmd *)addr;

		/* limited by max sizeof of geteblk */
		if (cmdp->datalen >= 8192 
		    || cmdp->cdblen >= MAXCDB) {
			error = EINVAL;
			break;
		}

		ccblen = cmdp->ccblen;
		if (ccblen > sizeof (struct ccb))
			ccblen = sizeof (struct ccb);

		while (as->scsi_lock)
			if (error = tsleep ((caddr_t)as, PWAIT|PCATCH,
					    "scsicmd", 0))
				break;

		as->scsi_lock = 1;
			
		bp = geteblk (cmdp->datalen);

		as->scsi_cdb_len = cmdp->cdblen;
		if (error = copyin (curproc, cmdp->cdb, as->scsi_cdb, cmdp->cdblen))
			goto done;
		
		direction = cmdp->readflag ? B_READ : B_WRITE;

		if (direction == B_WRITE)
			if (error = copyin (curproc, cmdp->data,
					    bp->b_un.b_addr, cmdp->datalen))
				goto done;
		
		ascmd (as, bp, direction, cmdp->datalen, 1);

		copyout (curproc, &as->ccb, cmdp->ccb, ccblen);
		if (direction == B_READ)
			copyout (curproc, bp->b_un.b_addr, cmdp->data, cmdp->datalen);
	done:
		bp->b_flags |= B_INVAL | B_AGE;
		brelse (bp);
		as->scsi_lock = 0;
		wakeup ((caddr_t)as);
		break;
	default:
		error = ENOTTY;
		break;
	}
	return (error);
}

static int
asstrategy (bp)
struct buf *bp;
{
	struct asinfo *as;
	int s;

	if (asverbose)
		printf ("asstrategy %d %d ", bp->b_blkno, bp->b_bcount);
	s = splbio ();

	as = dev_to_as (bp->b_dev);
	
	if (as->tape) {
		bp->av_forw = NULL;
		if (as->requests.b_actf)
			as->requests.b_actl->av_forw = bp;
		else
			as->requests.b_actf = bp;
		as->requests.b_actl = bp;
	} else {
		if (bp != as->scsi_bp
		    && as->have_label == 0
		    && dev_part (bp->b_dev) != 3)
			goto bad;

		bp->b_cylin = bp->b_blkno;
		disksort (&as->requests, bp);
	}
	
	if (as->active == 0)
		asstart (as);

	splx (s);
	return;

 bad:
	bp->b_flags |= B_ERROR;
	biodone (bp);
	splx (s);
}
static 
asrestart (as)
struct asinfo *as;
{
	int s;
	s = splbio ();
	as->restart_pending = 0;
	as->retry_count++;
	asstart (as);
	splx (s);
}
static void
asstart (as)
struct asinfo *as;
{
	struct buf *bp;
	int blknum;
	unsigned int physaddr, p2;
	struct ccb *ccb;
	unsigned char *cdb;
	int target;
	char *p, *nnext;
	int n;
	char *sp;
	int nscatter;
	int thistime;
	int nbytes;
	struct partition *part;
	int blkno;
	int nblocks;
	int total;
	int bs = as->bs;


	if (as->restart_pending) {
		as->restart_pending = 0;
		untimeout (asrestart, (caddr_t)as);
	}

 again:
	if ((bp = as->requests.b_actf) == NULL)
		return;

	bp->b_error = 0;

	if (asverbose)
		printf ("asstart %x ", bp);

	if (as->mailbox->cmd != 0) {
		/* this can't happen, unless the card flakes */
		printf ("asstart: mailbox not available\n");
		bp->b_error = EIO;
		goto bad;
	}

	if (as->isbounced == 0)
		bp->b_resid = 0;

	if (as->retry_count == 0) {
		as->start_time = time.tv_sec;
	} else {
		if (time.tv_sec - as->start_time > 60) {
			printf ("as: command timed out\n");
			bp->b_error = EIO;
			goto done;
		}
	}

	if (bp != as->scsi_bp) {
		if (bp->b_bcount == 0)
			goto done;

		if ((bp->b_bcount % bs) != 0) {
			printf("as: partial block read\n");
			bp->b_error = EIO;
			goto bad;
		}
	}

	if (bp != as->scsi_bp) {
		nblocks = aspartition(as, bp, &blkno);
		if (nblocks < 0)
			goto bad;
		if (nblocks == 0)
			goto done;
		total = nblocks * bs;
if(asverbose)
printf("total %d nblocks %d ", total, nblocks);
	} else
		total = bp->b_bcount;

	nscatter = asmakescatter(as, bp, 0);
	
	if (nscatter > NSCATTER) {
		printf("out of range, cannot happen?");
		bp->b_error = ENXIO;
		goto bad;
	}


	asmakeccb(as, nscatter, bp->b_dev, bp->b_bcount, bp->b_flags & B_READ);

	ccb = &as->ccb;
	cdb = ccb->ccb_cdb;
	if (bp == as->scsi_bp) {
		ccb->ccb_scsi_command_len = as->scsi_cdb_len;
		(void) memcpy(cdb, as->scsi_cdb, as->scsi_cdb_len);
	}
	else
		asmakecdb(as, blkno, bp->b_bcount,
		    bp->b_flags & B_READ);

 	/* write back cache contents */
 	/*if ((bp->b_flags & B_READ) == 0)
 		asm (".byte 0x0f,0x09 # wbinvd");	*/

	asissue(as);
	timeout (asabort, (caddr_t)as, hz * 60 * 2);
	return;

 bad:
	bp->b_flags |= B_ERROR;
 done:
	asdone (as, 0);
	goto again;
}

static 
asabort (as)
struct asinfo *as;
{
	int s;
	int physaddr;
	struct buf *bp;

	s = splbio ();
	if (as->active) {
		printf ("asabort %d\n", as - asinfo);
		physaddr = vtophys (&as->ccb);
		as->mailbox->msb = physaddr >> 16;
		as->mailbox->mid = physaddr >> 8;
		as->mailbox->lsb = physaddr;
		as->mailbox->cmd = 2;
		as_put_byte (as_port[dev_ctlr(as->dev)], AS_CMD_START_SCSI_COMMAND);

		as->active = 0;
		bp = as->requests.b_actf;
		if (bp) {
			bp->b_flags |= B_ERROR;
			asdone (as, 1);
		}
	}
	splx (s);
}
static void
asintr (int ctl)
{
	int didwork;
	int i, j;
	struct mailbox_entry *mp;
	unsigned int physaddr;
	int val;
	struct asinfo *as;

	outb (as_port[ctl /*dev_ctlr(dev)*/] + AS_CONTROL, AS_CONTROL_IRST);
	if (asverbose)
		printf ("asintr %x ", ctl /*dev*/);
 again:
	didwork = 0;
	if (as = asscan((ctl <<6) /*dev*/)) {
		asintr1(as, as->mbi_status);
		didwork = 1;
	}

	if (didwork)
		goto again;
}
static 
asintr1 (as, val)
struct asinfo *as;
int val;
{
	struct buf *bp;
	struct ccb *ccb;
	int n;
	int bad;
	char *msg;
	char msgbuf[100];
	unsigned char *sp;
	int i, key;
	int resid;

	if (asverbose)
		printf ("asintr1 %x ", val);
	if (as->active == 0) {
		printf ("as: stray intr 0x%x\n", as->dev);
		return;
	}

	as->active = 0;
	key = 0;
	untimeout (asabort, (caddr_t)as);
	
	bp = as->requests.b_actf;
	ccb = &as->ccb;

	if (bp == as->scsi_bp) {
		/* no fancy error recovery in this case */
		if (asverbose)
			printf ("asintr1:scsicmd ");
#if 0
		if (val != 1)
			bp->b_flags |= B_ERROR;
		goto next;
#endif
	}

	bad = 0;
	msg = NULL;
	bp->b_error = 0;

	if (val != 1 && val != 4) {
		bad = 1;
		bp->b_error = EIO;
		sprintf (msgbuf, "funny mailbox message 0x%x\n", val);
		msg = msgbuf;
		goto wrapup;
	}

	if (ccb->ccb_host_status != 0) {
		bad = 1;
		/* selection timeout */
		if (ccb->ccb_host_status == 0x11) {
			bp->b_error = ENXIO;
		} else {
			sprintf (msgbuf, "controller error 0x%x",
				ccb->ccb_host_status);
			bp->b_error = EIO;
		}
		msg = msgbuf;
		goto wrapup;
	}

	if (ccb->ccb_target_status == 0)
		/* good transfer */
		goto wrapup;

	if (ccb->ccb_target_status == 8) {
		/* target rejected command because it is busy
		 * and wants us to try again later.  We'll wait 1 second
		 */
		as->restart_pending = 1;
		timeout (asrestart, (caddr_t)as, hz);
		return;
	}

	if (ccb->ccb_target_status != 2) {
		bad = 1;
		bp->b_error = EIO;
		sprintf (msgbuf, "target error 0x%x",
			 ccb->ccb_target_status);
		msg = msgbuf;
		goto wrapup;
	}

	/* normal path for errors */

	sp = ccb_sense (ccb);
	/* check for extended sense information */
	if ((sp[0] & 0x7f) != 0x70) {
		/* none */
		bad = 1;
		bp->b_error = EIO;
		sprintf (msgbuf, "scsi error 0x%x", sp[0] & 0x7f);
		msg = msgbuf;
		goto wrapup;
	}
	
	if (as->tape && (sp[2] & 0xf) == 0) {
		if (sp[2] & 0xe0) {
			/* either we read a file mark, the early warning EOT,
			 * or the block size did not match.  In any case, the
			 * normal residue handling will work (I think)
			 */
			goto wrapup;
		}
	}

	bad = 1;

	switch (key = sp[2] & 0xf) {
	case 1: 
		msg = "soft error";
		bad = 0;
		break;
	case 2:
		msg = "not ready";
		break;
	case 3:
		msg = "hard error";
		break;
	case 4:
		msg = "target hardware error";
		break;
	case 5:
		msg = "illegal request";
		break;
	case 6:
		msg = "unit attention error";
		break;
	case 7: 
		msg = "write protect error";
		break;
	case 0xd:
		msg = "volume overflow";
		break;
	default:
		sprintf (msgbuf, "scsi extended error 0x%x", sp[2] & 0xf);
		msg = msgbuf;
		break;
	}

 wrapup:
	/* unit attention? */
	if (key == 6) {
		asstart (as);
		return;
	}

	if (bad && msg == NULL)
		msg = "unknown error";

	if (msg && key != 6 && bp->b_error != ENXIO) {
		diskerr (bp, "as", msg,
			 LOG_PRINTF,
			 -1, /* number of successful blks */
			 as->have_label ? &as->label : NULL);
		printf ("\n");
	}

	if (bad && key != 6 && bp->b_error != ENXIO) {
		bp->b_flags |= B_ERROR;
		printf ("scsi sense: ");
		sp = ccb_sense (ccb);
		for (i = 0; i < 30; i++)
			printf ("%x ", sp[i] & 0xff);
		printf ("\n");
	}

	/*resid = (ccb->ccb_data_len_msb << 16)
		| (ccb->ccb_data_len_mid << 8)
			| ccb->ccb_data_len_lsb;*/
resid = 0;

 	/* flush cache if read */
 	/*if ((bp->b_flags & B_READ) != 0)
 		asm (".byte 0x0f,0x08 # invd");	*/

	if (bad == 0 && as->isbounced && (bp->b_flags & B_READ) != 0) {
		memcpy(bp->b_un.b_addr, (char *)as->bounce, bp->b_bcount - resid);
		/* as->isbounced = 0;*/
	}

	bp->b_resid += resid;
	if (bp != as->scsi_bp && bp->b_resid != 0)
		printf ("scsi resid = %d\n", bp->b_resid);

 next:
	asdone (as, 1);
}
static 
asdone (as, restart)
struct asinfo *as;
int restart;
{
	struct buf *bp;

	bp = as->requests.b_actf;
	as->requests.b_actf = bp->av_forw;
	biodone (bp);
	as->retry_count = 0;
	if (restart && as->requests.b_actf)
		asstart (as);
}

static int
assize (dev_t dev)
{
	struct asinfo *as;
	struct disklabel *lp;
	int val, ctl;

	ctl = dev_ctlr(dev);
	if (ctl > NAS || as_port[ctl] == 0 || dev_target (dev) > NTARGETS)
		return (ENXIO);

	as = dev_to_as (dev);
	if (as->open == 0
	    && asopen (dev, FREAD, S_IFBLK, NULL) != 0)
		return (0);

	if (as->have_label == 0)
		return (0);

	lp = &as->label;
	val = lp->d_partitions[dev_part (dev)].p_size
		* lp->d_secsize / DEV_BSIZE;

	/* XXX hold open: (void) asclose(dev, FREAD, S_IFBLK, NULL); */
	return (val);
}

static void
asmakeccb(struct asinfo *as, int nscatter, dev_t dev, int count, int dir)
{
	struct ccb *ccb = &as->ccb;
	int target = dev_target (dev);
	int nbytes;
	unsigned int physaddr;

	/* this only needed to make debugging easier */
	(void) memset ((caddr_t)ccb, 0, sizeof *ccb); 

	if (nscatter) {
		ccb->ccb_opcode = 2; /* scatter cmd, return resid */
		nbytes = nscatter * 6;
		physaddr = vtophys (as->scatter_list);
	} else {
		ccb->ccb_opcode = 0;
		nbytes = count;
		physaddr = 0;
	}
	
	ccb->ccb_addr_and_control = target << 5;
	if (count != 0)
		ccb->ccb_addr_and_control |= (dir == B_READ) ? 8 : 0x10;
	else
		ccb->ccb_addr_and_control |= 0x18;

	ccb->ccb_data_len_msb = nbytes >> 16;
	ccb->ccb_data_len_mid = nbytes >> 8;
	ccb->ccb_data_len_lsb = nbytes;

	ccb->ccb_requst_sense_allocation_len = MAXSENSE;

	ccb->ccb_data_ptr_msb = physaddr >> 16;
	ccb->ccb_data_ptr_mid = physaddr >> 8;
	ccb->ccb_data_ptr_lsb = physaddr;

	ccb->ccb_link_msb = 0;
	ccb->ccb_link_mid = 0;
	ccb->ccb_link_lsb = 0;
	ccb->ccb_link_id = 0;
	ccb->ccb_host_status = 0;
	ccb->ccb_target_status = 0;
	ccb->ccb_zero1 = 0;
	ccb->ccb_zero2 = 0;
}

static void
asmakecdb(struct asinfo *as, int blkno, int count, int dir)
{
	struct ccb *ccb = &as->ccb;
	unsigned char *cdb;
	int nblocks = howmany(count, as->bs);

	cdb = ccb->ccb_cdb;
	if (as->tape) {
		ccb->ccb_scsi_command_len = 6;
		cdb[0] = (dir == B_READ) ? 8 : 0xa;
		if (as->bs == 1)
			cdb[1] = 0; /* logical unit 0, variable block size */
		else
			cdb[1] = 1; /* fixed block size */
		cdb[2] = nblocks >> 16;
		cdb[3] = nblocks >> 8;
		cdb[4] = nblocks;
		cdb[5] = 0; /* control byte (used in linking) */
	} else {
		ccb->ccb_scsi_command_len = 10;
		cdb[0] = (dir == B_READ) ? 0x28 : 0x2a;
		cdb[1] = 0;
		*(long *) (cdb+2) = htonl(blkno);
		*(short *) (cdb+7) = htons(nblocks);
		cdb[9] = 0; /* control byte (used in linking) */
	}
}

static void
asissue(struct asinfo *as) {
	struct ccb *ccb = &as->ccb;
	int n;
	unsigned int physaddr /*= vtophys (ccb);*/;

	physaddr = vtophys (ccb);
	if (asverbose) {
		printf ("ccb: ");
		for (n = 0; n < 48; n++)
			printf ("%02x ", ((unsigned char *)ccb)[n]);
		printf ("\n");
	}

	as->mailbox->msb = physaddr >> 16;
	as->mailbox->mid = physaddr >> 8;
	as->mailbox->lsb = physaddr;
	as->mailbox->cmd = 1;
	
	/* tell controller to look in its mailbox */
	as_put_byte (as_port[dev_ctlr(as->dev)], AS_CMD_START_SCSI_COMMAND);
	as->active = 1;
}

static struct asinfo *
asscan (dev_t dev)
{
	int i, j;
	struct mailbox_entry *mp;
	unsigned int physaddr;
	int val, ctl = dev_ctlr(dev);

	outb (as_port[ctl] + AS_CONTROL, AS_CONTROL_IRST);
	
	ctl *= NTARGETS;
	for (i = NTARGETS; i < NTARGETS * 2; i++) {
		mp = &mailboxes[ctl + i];
		
		if ((val = mp->cmd) == 0)
			continue;
		
		physaddr = (mp->msb << 16)
			| (mp->mid << 8)
				| mp->lsb;
		
		for (j = 0; j < NTARGETS; j++) {
			if (asinfo[ctl + j].ccb_phys == physaddr) {
				mp->cmd = 0;
				(asinfo + ctl + j) ->mbi_status = val;
				return (asinfo + ctl + j);
			}
		}
		if (j == NTARGETS) {
			printf ("as: unknown mailbox paddr 0x%x\n", physaddr);
			mp->cmd = 0;
		}
	}
	return (0);
}

static int
as_put_byte (port, val)
int val;
{
	int i, sts, intr;

	for (i = 10000; i > 0; i--) {
		sts = inb (port + AS_STATUS);
		intr = inb (port + AS_INTR);
		if ((sts & AS_STATUS_CDF) == 0)
			break;
		DELAY (1000);
	}

	/* time out? */
	if (i == 0) {
		printf ("as: put byte timed out %x %x", sts, intr);
		goto fail;
	}

	if ((intr & AS_INTR_HACC) && (sts & AS_STATUS_INVDCMD)) {
		printf ("as: put byte invalid command %x %x", sts, intr);
	fail:
		outb (port + AS_CONTROL, AS_CONTROL_IRST);
		return (-1);
	}

	outb (port + AS_DATA_OUT, val);
	return (0);
}
		
static int
asmakescatter(struct asinfo *as, struct buf *bp, int phys)
{
	int n;
	char *sp;
	int nscatter;
	int thistime;
	unsigned int physaddr, p2;
	int total;
	char *p = bp->b_un.b_addr;

	/* restrict transfer to maximum controller can handle */
	if (bp->b_bcount > (16*1024*1024 - as->bs)) {
		int	amt = 16*1024*1024 - as->bs;
		bp->b_resid  += bp->b_bcount - amt;
		bp->b_bcount -= bp->b_bcount - amt;
	}

	if (bp != as->scsi_bp && as->bs)
		total =  roundup(bp->b_bcount, as->bs);
	else
		total =  bp->b_bcount;

redo:
	if (asverbose)
		printf("%d bytes from %x: ", total, p);

	/* generate scatter list */
	n = 0;
	sp = as->scatter_list;
	nscatter = 0;
	while (n < total && nscatter < NSCATTER) {

		/* physical or virtual */
		if (phys) {
			physaddr = (unsigned int) p;
			thistime = 16*1024*1024 - (int)p;
		} else {
			thistime = page_size - ((vm_offset_t)p - trunc_page (p));
			physaddr = vtophys (p);
		}
		if (n + thistime > total)
			thistime = total - n;

		/* if any page is outside of ISA bus address space, bounce */
		if (physaddr >= 16*1024*1024) {
			/* no buffer? */
			if (as->bounce == 0)
	    			panic(
"as: i/o outside of bus address space and no buffer");
			/* if (asverbose) */
				printf ("as: bounce transfer ");

			/* truncate to max size, */
			if (total > 64*1024) {
				bp->b_resid +=  bp->b_bcount - 64*1024;
				total = 64*1024; 
				bp->b_bcount = 64*1024;
			}

			/* if a write, load bounce buffer */
			if ((bp->b_flags & B_READ) == 0) {

				if (asverbose)
					printf("load bounce: ");
				/* if physical dump, map VA == PA */
				/* XXX should reserve address space on boot for this */
				if (phys) {
					pmap_map((vm_offset_t)dumpISA, 
					    trunc_page((vm_offset_t)bp->b_un.b_addr), 
					    round_page((vm_offset_t)bp->b_un.b_addr + total), 
					    VM_PROT_ALL);
				memcpy((char *)as->bounce,
				    dumpISA + ((int)bp->b_un.b_addr & (NBPG-1)),
				    bp->b_bcount);
				} else
				memcpy((char *)as->bounce, bp->b_un.b_addr, 
				    bp->b_bcount);
			}

			/* if a read, bounce buffer will be unloaded after transfer */
			as->isbounced = 1;
			p = (char *)as->bounce;
			phys = 0;
			goto redo;
		}
		else
			as->isbounced = 0;

		/*
		 * Do we have a run of consecutive pages?
		 */
		if (phys == 0) {
			/* check for consecutive physical pages */
			p2 = physaddr;
			for (; n + thistime < total ;) {
				if (trunc_page(p2) + page_size !=
			    	    trunc_page(vtophys(p + thistime)))
					break;
				p2 = vtophys(p + thistime);
				if (p2 >= 16*1024*1024)
					break;
				thistime += page_size;
				if (n + thistime > total) {
					thistime = total - n;
					break;
				}
			}
		}

		if (asverbose)
			printf ("%d bytes to %x (%x) ", thistime, p, physaddr);
		sp[0] = thistime >> 16;
		sp[1] = thistime >> 8;
		sp[2] = thistime;
		sp[3] = physaddr >> 16;
		sp[4] = physaddr >> 8;
		sp[5] = physaddr;
		
		p += thistime;
		n += thistime;
		sp += 6;
		nscatter++;

		/* only one segment allowed for a phys transfer */
		if (phys && bp->b_bcount > n) {
			bp->b_resid  += bp->b_bcount - n;
			bp->b_bcount -= bp->b_bcount - n;
			if (asverbose)
				printf("restricted to %d bytes: ", 
					bp->b_bcount);
			break;
		}
	}

	return (nscatter);
}

static int
aspartition(struct asinfo *as, struct buf *bp, int *blknop)
{
	int blkno = bp->b_blkno;
	int nblocks = bp->b_bcount / as->bs;
	struct partition *part;

	if (as->have_label && dev_part(bp->b_dev) != 3) {
		part = &as->label.d_partitions[dev_part(bp->b_dev)];
			
		if (blkno > part->p_size) {
			bp->b_error = EINVAL;
			return (-1);
		}
		if (blkno == part->p_size) {
			bp->b_resid += bp->b_bcount;
			return (0);
		}
			
		if (blkno + nblocks > part->p_size) {
			int remaining = part->p_size - blkno;
			/* nblocks = part->p_size - blkno; */
			bp->b_resid += as->bs * (nblocks - remaining);
			bp->b_bcount -= as->bs * (nblocks - remaining);
			nblocks = remaining;
		}

		blkno += part->p_offset;
	} else if (as->bs != 1)
		blkno = (blkno * DEV_BSIZE)/as->bs;
	if (asverbose)
		printf("trans %d ", blkno);
	*blknop = blkno;
	return (nblocks);
}

static int
as_get_byte (port)
{
	int i;

	while ((inb (port + AS_STATUS) & AS_STATUS_DF) == 0)
		DELAY (100);
	return (inb (port + AS_DATA_OUT) & 0xff);
}

static int
asdump(dev_t dev)			/* dump core after a system crash */
{
	struct asinfo *as;
	long	num;			/* number of sectors to write */
	int	ctl, unit, part, asc;
	int	blkoff, blknum, blkcnt;
	long	nblocks, i;
	extern	int maxmem;
	static  int asdoingadump = 0 ;
	struct buf buf;
	char *sp;
	int nscatter;


	/* toss any characters present prior to dump */
	while (sgetc(1))
		;

	/* size of memory to dump */
	num = maxmem;
	ctl = dev_ctlr(dev);
	unit = dev_target(dev);
	part = dev_part(dev);		/* file system */

	/* check for acceptable controller number */
	if (ctl > NAS)
		return(ENXIO);

	/* check for acceptable drive number */
	if (unit > NTARGETS)
		return(ENXIO);

	as = dev_to_as(dev);
	if (as_port[ctl] == 0) return(ENXIO);

	/* check if controller active */
	/*if (astab.b_active) return(EFAULT); */
	if (asdoingadump) return(EFAULT);
	
	/* create a buffer */
	buf.b_flags = B_WRITE;
	buf.b_dev = dev;
	buf.b_blkno = dumplo;
	buf.b_bcount = maxmem * NBPG;
	buf.b_un.b_addr = 0;
	buf.b_resid = 0;

	for (;;) {
	nblocks = aspartition(as, &buf, &blknum);

/*printf("blkno %d, nblocks %d, dumplo %d num %d\n",
blknum, nblocks,dumplo,num); */

	/* check transfer bounds against partition size */
	/*if ((dumplo < 0) || ((dumplo + num) > nblocks))
		return(EINVAL);*/

	/*astab.b_active = 1;		/* mark controller active for if we
					   panic during the dump */
	/* transfer in progress */
	while(as->active)
		asscan(dev);
	/* otherwise, asabort() */
	asdoingadump = 1;
	
	nscatter = asmakescatter(as, &buf, 1);
/*printf("nscat %d addr %x count %d resid %d\n", nscatter,
	buf.b_un.b_addr, buf.b_bcount, buf.b_resid);*/

	/* build a ccb */
	asmakeccb(as, nscatter, dev, buf.b_bcount, B_WRITE);
	asmakecdb(as, blknum, buf.b_bcount, B_WRITE);
/*printf("blk %d sz %d count %d resid %d\n", blknum, nblocks,
	buf.b_bcount, buf.b_resid); */
	asissue(as);
	while(as->active)
		if (as == asscan(0))
			as->active = 0;
	if (buf.b_resid == 0)
		break;
	buf.b_un.b_addr += buf.b_bcount;
	buf.b_blkno += buf.b_bcount / 512;
	buf.b_bcount = buf.b_resid;
	buf.b_resid = 0;
	}
	printf("done");
DELAY(10000000);
	return(0);
}

struct devif as_devif =
{
	{0}, -1, -1, 0x38, 3, 7, 0, 0, 0,
	asopen,	asclose, asioctl, 0, 0, 0, 0,
	asstrategy,	0, asdump, assize,
};

/*static struct bdevsw as_bdevsw = 
	{ asopen,	asclose,	asstrategy,	asioctl,
	  asdump,	assize,		NULL };

void mkraw(struct bdevsw *bdp, struct cdevsw *cdp);*/

DRIVER_MODCONFIG() {
	int vec[3], nvec = 3, nctl;	/* (bdev, cdev, nctl) */
	char *cfg_string = as_config;
#ifdef foo
	struct cdevsw as_cdevsw;
	
	vec[2] = 1;	/* by default, one controller */
	if (!config_scan(as_config, &cfg_string)) 
		return /*(EINVAL)*/;

	/* no more controllers than 2 */
	if (vec[2] > 2)
		vec[2] = 2;

	/*bdevsw[vec[0]] = as_bdevsw;
	mkraw(&as_bdevsw, &as_cdevsw);
	cdevsw[vec[1]] = as_cdevsw; */

	if (!spec_config(&cfg_string, &as_bdevsw, (struct cdevsw *) -1, 0))
		return;
#else
	if (devif_config(&cfg_string, &as_devif) == 0)
		return;
#endif

	(void)cfg_number(&cfg_string, &nctl);
	/* malloc(vec[2]*controllerresources) ... */

	new_isa_configure(&cfg_string, &asdriver);

	/*return (0);*/
}
