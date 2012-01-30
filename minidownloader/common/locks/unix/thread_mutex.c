#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "thread_mutex.h"
#include "arch_thread_mutex.h"

int thread_mutex_create(thread_mutex_t **new_mutex,
                                                  unsigned int flags)
{
    int rv;
    
    *new_mutex = (thread_mutex_t *)malloc(sizeof(thread_mutex_t));
	memset(*new_mutex,0,sizeof(thread_mutex_t));

    /*
    if (flags & THREAD_MUTEX_NESTED) {
        pthread_mutexattr_t mattr;
        
        rv = pthread_mutexattr_init(&mattr);
        if (rv) return rv;
        
        rv = pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE);
        if (rv) {
            pthread_mutexattr_destroy(&mattr);
            return rv;
        }
         
        rv = pthread_mutex_init(&new_mutex->mutex, &mattr);
        
        pthread_mutexattr_destroy(&mattr);
    } else*/
        rv = pthread_mutex_init(&(*new_mutex)->mutex, NULL);

    if (rv) {
        return rv;
    }

    return 0;
}

int thread_mutex_lock(thread_mutex_t *mutex)
{
    int rv;

    rv = pthread_mutex_lock(&mutex->mutex);
    
    return rv;
}

int thread_mutex_trylock(thread_mutex_t *mutex)
{
    int rv;

    rv = pthread_mutex_trylock(&mutex->mutex);
    if (rv) {
        return (rv == EBUSY) ? EBUSY : rv;
    }

    return 0;
}

int thread_mutex_unlock(thread_mutex_t *mutex)
{
    int status;

    status = pthread_mutex_unlock(&mutex->mutex);

    return status;
}

int thread_mutex_destroy(thread_mutex_t *mutex)
{
    int rv;

    rv = pthread_mutex_destroy(&mutex->mutex);

    free(mutex);
    return rv;
}

