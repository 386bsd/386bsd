#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#if __STDC__
int shmctl(int shmid, int cmd, void *buf)
#else
int shmctl(shmid, cmd, buf)
	int shmid;
	int cmd;
	void *buf;
#endif
{
	return (shmsys(1, shmid, cmd, buf));
}
