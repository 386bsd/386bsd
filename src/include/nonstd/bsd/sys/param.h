#include "/usr/include/sys/param.h"
#define	BSD	200012			/* "new" BSDs */

#define	NOFILE		OPEN_MAX	/* max number of open files */
#define	MAXPATHLEN	PATH_MAX	/* max pathname length */
#define	MAXINTERP	32		/* max interpreter file name length */
#define	NCARGS		ARG_MAX		/* max number of arg characters */
#define NGROUPS		30
#define PZERO		22		/* XXX unreal, but programs expect it */
