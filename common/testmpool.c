#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "allocator.h"
#include "thread_mutex.h"

int main(void)
{
	allocator_t *pallocator;
	allocator_create(&pallocator);

	thread_mutex_t *thread_mutex = NULL;
	thread_mutex_create(&thread_mutex,THREAD_MUTEX_DEFAULT);
	allocator_mutex_set(pallocator,thread_mutex);

	while(1)
	{
		memnode_t *mnode = allocator_alloc(pallocator,4096);
		char buf[1024] = {0};
		memcpy(mnode->first_avail,buf,sizeof(buf));
		allocator_free(pallocator,mnode);
		break;
	}

	allocator_destroy(pallocator);
	thread_mutex_destroy(thread_mutex);

	return 0;
}
