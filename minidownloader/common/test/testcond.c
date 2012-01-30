#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "thread_cond.h"

int main(void)
{
	int rv;
	thread_mutex_t *thread_mutex = NULL;
    thread_cond_t *thread_cond = NULL;
	thread_mutex_create(&thread_mutex,THREAD_MUTEX_DEFAULT);
	thread_cond_create(&thread_cond);

	rv = thread_cond_signal(thread_cond);                                                         
	rv = thread_mutex_lock(thread_mutex);                                                         
	rv = thread_cond_timedwait(thread_cond, thread_mutex, 10000);                                        
	printf("rv:%d\n",rv);
	rv = thread_mutex_unlock(thread_mutex);                                                       
	rv = thread_cond_broadcast(thread_cond);                                                      
	rv = thread_mutex_lock(thread_mutex);                                                         
	rv = thread_cond_timedwait(thread_cond, thread_mutex, 10000);                                        
	rv = thread_mutex_unlock(thread_mutex);


	thread_cond_destroy(thread_cond);
	thread_mutex_destroy(thread_mutex);
	return 0;
}
