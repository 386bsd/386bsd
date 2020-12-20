/*
 * Mount a MS DOS (FAT) filesystem.
 * (code borrowed from Pace Willison's Public Domain mount_isofs)
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/mount.h>

char *progname;

void
usage ()
{
	fprintf (stderr, "usage: %s bdev dir\n", progname);
	exit (1);
}
		
int
main (argc, argv)
int argc;
char **argv;
{
	char *dev;
	char *dir;
	struct ufs_args args;
	int c;
	extern char *optarg;
	extern int optind;
	int opts;

	progname = argv[0];

	opts = 0;

	while ((c = getopt (argc, argv, "F:")) != EOF) {
		switch (c) {
		case 'F':
			opts |= atoi (optarg);
			break;
		default:
			usage ();
		}
	}

	if (optind + 2 != argc)
		usage ();

	dev = argv[optind];
	dir = argv[optind + 1];

	args.fspec = dev;
	args.exflags = 0;
	args.exroot = 0;

	if (mount (MOUNT_MSDOS, dir, opts, &args) < 0) {
		perror ("mount");
		exit (1);
	}

	exit (0);
}
