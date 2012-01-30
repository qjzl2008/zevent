#include <stdlib.h>
#include <errno.h>
#include "arch_thread_rwlock.h"

#if HAS_THREADS

/* The rwlock must be initialized but not locked by any thread when
 * cleanup is called. */
int thread_rwlock_create(thread_rwlock_t **rwlock)
{
    thread_rwlock_t *new_rwlock;
    int stat;

    new_rwlock = (thread_rwlock_t *)malloc(sizeof(thread_rwlock_t));

    if ((stat = pthread_rwlock_init(&new_rwlock->rwlock, NULL))) {
        return stat;
    }

    *rwlock = new_rwlock;
    return 0;
}

int thread_rwlock_rdlock(thread_rwlock_t *rwlock)
{
    int stat;

    stat = pthread_rwlock_rdlock(&rwlock->rwlock);
    return stat;
}

int thread_rwlock_tryrdlock(thread_rwlock_t *rwlock)
{
    int stat;

    stat = pthread_rwlock_tryrdlock(&rwlock->rwlock);
    if (stat == EBUSY)
        stat = RWLOCK_EBUSY;
    return stat;
}

int thread_rwlock_wrlock(thread_rwlock_t *rwlock)
{
    int stat;

    stat = pthread_rwlock_wrlock(&rwlock->rwlock);
    return stat;
}

int thread_rwlock_trywrlock(thread_rwlock_t *rwlock)
{
    int stat;

    stat = pthread_rwlock_trywrlock(&rwlock->rwlock);
    if (stat == EBUSY)
        stat = RWLOCK_EBUSY;
    return stat;
}

int thread_rwlock_unlock(thread_rwlock_t *rwlock)
{
    int stat;

    stat = pthread_rwlock_unlock(&rwlock->rwlock);
    return stat;
}

int thread_rwlock_destroy(thread_rwlock_t *rwlock)
{
    int stat;

    stat = pthread_rwlock_destroy(&rwlock->rwlock);

    free(rwlock);
    return stat;
}

#endif /* HAS_THREADS */
