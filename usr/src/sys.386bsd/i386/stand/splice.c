/*
 * splice wdboot, dl, bootwd together
 */
#include <sys/file.h>

char buf[8*1024];
main(argc, argv) char *argv[]; {
	int wdboot, dl, bootwd, newdl;

	wdboot = open("wdboot", O_RDONLY, 0);
	dl = open("0.1dl", O_RDONLY, 0);
	bootwd = open("bootwd", O_RDONLY, 0);
	newdl = open("newdl", O_WRONLY|O_CREAT, 0644);
	if (wdboot < 0 || dl < 0 ||  bootwd < 0 || newdl < 0)
	{
		printf("files can't be opened\n");
		exit(1);
	}
	read(wdboot, buf, 512);
	read(dl, buf+512, 512);	/* skip over bootstrap */
	read(dl, buf+512, 512);
	read(bootwd, buf+1024, 512); /* skip over dl */
	read(bootwd, buf+1024, 14*512); 
	write(newdl, buf, 8*1024);
	return (1);
}
