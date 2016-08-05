/*
 * msdos common header file
 */

#define MSECTOR_SIZE	512		/* MSDOS sector size in bytes */
#define MDIR_SIZE	32		/* MSDOS directory size in bytes */
#define MAX_CLUSTER	8192		/* largest cluster size */
#define MAX_PATH	128		/* largest MSDOS path length */
#define MAX_DIR_SECS	64		/* largest directory (in sectors) */

#define NEW		1
#define OLD		0

struct directory {
	unsigned char name[8];		/* file name */
	unsigned char ext[3];		/* file extension */
	unsigned char attr;		/* attribute byte */
	unsigned char reserved[10];	/* ?? */
	unsigned char time[2];		/* time stamp */
	unsigned char date[2];		/* date stamp */
	unsigned char start[2];		/* starting cluster number */
	unsigned char size[4];		/* size of the file */
};

struct device {
	char drive;			/* the drive letter */
	char *name;			/* full path to device */
	long offset;			/* skip this many bytes */
	int fat_bits;			/* FAT encoding scheme */
	int mode;			/* any special open() flags */
	int (*gioctl) ();		/* gioctl() function if needed */
	int tracks;			/* tracks */
	int heads;			/* heads */
	int sectors;			/* sectors */
};

struct bootsector {
	unsigned char jump[3];		/* Jump to boot code */
	unsigned char banner[8];	/* OEM name & version */
	unsigned char secsiz[2];	/* Bytes per sector hopefully 512 */
	unsigned char clsiz;		/* Cluster size in sectors */
	unsigned char nrsvsect[2];	/* Number of reserved (boot) sectors */
	unsigned char nfat;		/* Number of FAT tables hopefully 2 */
	unsigned char dirents[2];	/* Number of directory slots */
	unsigned char psect[2];		/* Total sectors on disk */
	unsigned char descr;		/* Media descriptor=first byte of FAT */
	unsigned char fatlen[2];	/* Sectors in FAT */
	unsigned char nsect[2];		/* Sectors/track */
	unsigned char nheads[2];	/* Heads */
	unsigned char nhs[4];		/* number of hidden sectors */
	unsigned char bigsect[4];	/* big total sectors */
	unsigned char junk[476];	/* who cares? */
};

typedef void SIG_TYPE;
/* typedef int SIG_TYPE; */

#ifdef BSD
#define strchr index
#define strrchr rindex
#endif /* BSD */
