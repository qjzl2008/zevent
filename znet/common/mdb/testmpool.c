//gcc -g -DDEBUG -DSTATISTICS testmpool.c mpool.c
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include "mpool.h"
int main(void)
{
	int fd = open("a.data",O_CREAT|O_RDWR,0600);
	MPOOL *mp = mpool_open(NULL,fd,4096,64);
	mpool_stat(mp);
	pgno_t pgno;
	void *page = mpool_new(mp,&pgno);
	//memset(page,0,4096);
	const char *str = "abcd";
	if(page)
	{
		memcpy(page,str,strlen(str));
	}
	mpool_put(mp,page,MPOOL_DIRTY);
	mpool_sync(mp);
	mpool_close(mp);
	close(fd);
	return 0;
}
