#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "queue.h"

int main(void)
{
	int rv;
	int *data = NULL;
	void *tmp = NULL;

	queue_t *queue = NULL;
	rv = queue_create(&queue,10);
	data = (int *)malloc(sizeof(int));
	*data = 100;
	queue_push(queue,data);

	queue_pop(queue, &tmp);

	printf("tmp:%d\n",*(int *)tmp);
	free(tmp);
	
	queue_destroy(queue);

	return 0;
}
