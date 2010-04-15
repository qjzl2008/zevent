#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "allocator.h"

int main(void)
{
	allocator_t *pallocator;
	allocator_create(&pallocator);

	while(1)
	{
		memnode_t *mnode = allocator_alloc(pallocator,4096);
		char buf[1024] = {0};
		memcpy(mnode->first_avail,buf,sizeof(buf));
		allocator_free(pallocator,mnode);
	}

	allocator_destroy(pallocator);

	return 0;
}
