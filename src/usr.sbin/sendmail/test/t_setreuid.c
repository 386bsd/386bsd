/*
**  This program checks to see if your version of setreuid works.
**  Compile it, make it setuid root, and run it as yourself (NOT as
**  root).  If it won't compile or outputs any MAYDAY messages, don't
**  define HASSETREUID in conf.h.
**
**  Compilation is trivial -- just "cc t_setreuid.c".
*/

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>

#ifdef __hpux
#define setreuid(r, e)	setresuid(r, e, -1)
#endif

main()
{
	uid_t realuid = getuid();

	printuids("initial uids", realuid, 0);

	if (geteuid() != 0)
	{
		printf("re-run setuid root\n");
		exit(1);
	}

	if (setreuid(0, 1) < 0)
		printf("setreuid(0, 1) failure\n");
	printuids("after setreuid(0, 1)", 0, 1);

	if (geteuid() != 1)
		printf("MAYDAY!  Wrong effective uid\n");

	/* do activity here */

	if (setreuid(-1, 0) < 0)
		printf("setreuid(-1, 0) failure\n");
	printuids("after setreuid(-1, 0)", 0, 0);
	if (setreuid(realuid, 0) < 0)
		printf("setreuid(%d, 0) failure\n", realuid);
	printuids("after setreuid(realuid, 0)", realuid, 0);

	if (geteuid() != 0)
		printf("MAYDAY!  Wrong effective uid\n");
	if (getuid() != realuid)
		printf("MAYDAY!  Wrong real uid\n");
	printf("\n");

	if (setreuid(0, 2) < 0)
		printf("setreuid(0, 2) failure\n");
	printuids("after setreuid(0, 2)", 0, 2);

	if (geteuid() != 2)
		printf("MAYDAY!  Wrong effective uid\n");

	/* do activity here */

	if (setreuid(-1, 0) < 0)
		printf("setreuid(-1, 0) failure\n");
	printuids("after setreuid(-1, 0)", 0, 0);
	if (setreuid(realuid, 0) < 0)
		printf("setreuid(%d, 0) failure\n", realuid);
	printuids("after setreuid(realuid, 0)", realuid, 0);

	if (geteuid() != 0)
		printf("MAYDAY!  Wrong effective uid\n");
	if (getuid() != realuid)
		printf("MAYDAY!  Wrong real uid\n");

	exit(0);
}

printuids(str, r, e)
	char *str;
	int r, e;
{
	printf("%s (should be %d/%d): r/euid=%d/%d\n", str, r, e,
		getuid(), geteuid());
}
