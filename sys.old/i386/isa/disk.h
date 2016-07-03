/*
 * @(#)disk.h	1.1 (Symmetric) 6/11/66
 */

#ifndef BSIZE
#define BSIZE	1024			/* the default block size */
#endif

/*
 * Each disk has a label which includes information about the hardware
 * disk geometry, filesystem partitions, and drive specific information.
 * The label is in block 0, offset from the beginning to leave room
 * for a bootstrap, etc.
 */

#define LABELSECTOR	0		/* sector containing label */
/*#define LABELOFFSET	800		/* offset of label in sector */
#define LABELOFFSET	(512-120)	/* offset of label in sector */
#define DISKMAGIC	0xabc		/* The disk magic number */
#define	DTYPE_ST506	1		/* ST506 Winchester */
#define	DTYPE_FLOPPY	2		/* 5-1/4" minifloppy */
#define	DTYPE_SCSI	3		/* SCSI Direct Access Device */

#ifndef LOCORE
struct disklabel {
	short	dk_magic;		/* the magic number */
	short	dk_type;		/* drive type */
	struct dcon {
		short	dc_secsize;	/* # of bytes per sector */
		short	dc_nsectors;	/* # of sectors per track */
		short	dc_ntracks;	/* # of tracks per cylinder */
		short	dc_ncylinders;	/* # of cylinders per unit */
		long	dc_secpercyl;	/* # of sectors per cylinder */
		long	dc_secperunit;	/* # of sectors per unit */
		long	dc_drivedata[4]; /* drive-type specific information */
	} dc;
	struct dpart {			/* the partition table */
		long	nblocks;	/* number of sectors in partition */
		long	cyloff;		/* starting cylinder for partition */
	} dk_partition[8];
	char	dk_name[16];		/* pack identifier */
};

#define dk_secsize		dc.dc_secsize
#define dk_nsectors		dc.dc_nsectors
#define dk_ntracks		dc.dc_ntracks
#define dk_ncylinders		dc.dc_ncylinders
#define dk_secpercyl		dc.dc_secpercyl
#define dk_secperunit		dc.dc_secperunit

/*
 * Drive data for ST506.
 */
#define dk_precompcyl	dc.dc_drivedata[0]
#define dk_ecc		dc.dc_drivedata[1]	/* used only when formatting */
#define dk_gap3		dc.dc_drivedata[2]	/* used only when formatting */

/*
 * Drive data for SCSI
 */
#define dk_blind	dc.dc_drivedata[0]	/* can we work in "blind" i/o */

/*
 *	A convenience for assigning partition tables
 */

typedef struct {
	struct dpart t[8];
} partitions;

struct format_op {
	char	*df_buf;
	int	df_count;		/* value-result */
	daddr_t	df_startblk;
	int	df_reg[4];		/* result */
};

/*
 * Disk-specific ioctls.
 */
		/* get and set disklabel; last form used internally */
#define DIOCGDINFO	_IOR('d', 101, struct disklabel)
#define DIOCSDINFO	_IOW('d', 102, struct disklabel)
#define DIOCGDINFOP	_IOW('d', 103, struct disklabel *) /* get label pointer */

#define DIOCSSTEP	_IOW('d', 104, int)	/* set step rate */
#define DIOCSRETRIES	_IOW('d', 105, int)	/* set # of retries */

/* do format operation, read or write */
#define DIOCRFORMAT	_IOWR('d', 106, struct format_op)
#define DIOCWFORMAT	_IOWR('d', 107, struct format_op)

#else	LOCORE

 # struct disklabel {
 	.set	dk_magic,	  0		# the magic number 
 	.set	dk_type,	  2		# drive type 
	.set	dc,		  4	
 	.set	dk_secsize,	  4		# # of bytes per sector 
 	.set	dk_nsectors,	  6		# # of sectors per track 
 	.set	dk_ntracks,	  8		# # of tracks per cylinder 
 	.set	dk_ncylinders,	 10		# # of cylinders per unit 
 	.set	dk_secpercyl,	 12		# # of sectors per cylinder 
 	.set	dk_secperunit,	 16		# # of sectors per unit 
 	.set	dk_drivedata,	 20	 # drive-type specific information 
 #	struct dpart {			# the partition table 
 	.set	dk_partition,	 36	
 	.set	dpar_nblocksa,	 36	# number of sectors in partition 
 	.set	dpar_cyloffa,	 40	# starting cylinder for partition 
 	.set	dpar_nblocksb,	 44	# number of sectors in partition 
 	.set	dpar_cyloffb,	 48	# starting cylinder for partition 
 	.set	dpar_nblocksc,	 52	# number of sectors in partition 
 	.set	dpar_cyloffc,	 56	# starting cylinder for partition 
 	.set	dpar_nblocksd,	 60	# number of sectors in partition 
 	.set	dpar_cyloffd,	 64	# starting cylinder for partition 
 	.set	dpar_nblockse,	 68	# number of sectors in partition 
 	.set	dpar_cyloffe,	 72	# starting cylinder for partition 
 	.set	dpar_nblocksf,	 76	# number of sectors in partition 
 	.set	dpar_cylofff,	 80	# starting cylinder for partition 
 	.set	dpar_nblocksg,	 84	# number of sectors in partition 
 	.set	dpar_cyloffg,	 90	# starting cylinder for partition 
 	.set	dpar_nblocksh,	 94	# number of sectors in partition 
 	.set	dpar_cyloffh,	 98	# starting cylinder for partition 
 	.set	dk_name,	102	# pack identifier 
 	.set	dk_end_,	118	# end of structure

 #
 # Drive data for ST506.
#define dk_precompcyl dk_drivedata

 # Drive data for SCSI.
#define dk_blind dk_drivedata
#endif
