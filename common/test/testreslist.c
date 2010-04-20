#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "reslist.h"

static int res_construct(void **res, void *params)    
{    
	printf("cons\n");
	*res = malloc(sizeof(int));
	int *p = *res;
	(*p) = 1;
	return 0;    
}    

static int res_destruct(void *res, void *params)    
{    

	printf("des\n");
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
	void *res = NULL;
	reslist_acquire(reslist,&res);
	printf("res:%d\n",*(int *)(res));

	reslist_release(reslist,res);
	reslist_destroy(reslist);

	return 0;
}
