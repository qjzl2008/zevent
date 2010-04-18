#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "reslist.h"

static int res_construct(void **res, void *params)    
{    
	*res = malloc(sizeof(int));
	return 0;    
}    

static int res_destruct(void *res, void *params)    
{    
	free(res);    
	return 0;    
}    


int main(void)
{
	int rv;
	reslist_t *reslist = NULL;
	reslist_create(&reslist,
			5, 10, 32,
			0,
			res_construct,
			res_destruct,
			NULL
			/*,allocator_t *allocator*/);
//	reslist_destory(reslist);

	return 0;
}
