#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "thread_rwlock.h"

int main(void)
{
	int rv;
	thread_rwlock_t *thread_rwlock = NULL;
	rv = thread_rwlock_create(&thread_rwlock);
	rv = thread_rwlock_wrlock(thread_rwlock);
	rv = thread_rwlock_unlock(thread_rwlock);
	thread_rwlock_destroy(thread_rwlock);
	return 0;
}
