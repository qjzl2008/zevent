#if HAS_THREADS

#include <stdlib.h>
#include <errno.h>
#include "thread_cond.h"
#include "arch_thread_mutex.h"
#include "arch_thread_cond.h"

int thread_cond_create(thread_cond_t **cond)
{
    thread_cond_t *new_cond;
    int rv;

    new_cond = malloc(sizeof(thread_cond_t));

    if ((rv = pthread_cond_init(&new_cond->cond, NULL))) {
        return rv;
    }

    *cond = new_cond;
    return 0;
}

int thread_cond_wait(thread_cond_t *cond,
                                               thread_mutex_t *mutex)
{
    int rv;

    rv = pthread_cond_wait(&cond->cond, &mutex->mutex);
    return rv;
}

#define USEC_PER_SEC ((long long)(1000000))

#define time_sec(time) ((time) / USEC_PER_SEC)

#define time_usec(time) ((time) % USEC_PER_SEC)

int thread_cond_timedwait(thread_cond_t *cond,
                                                    thread_mutex_t *mutex,
                                                    long long timeout)
{
    int rv;
    long long then;
    struct timespec abstime;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    then = tv.tv_sec * USEC_PER_SEC + tv.tv_usec + timeout;

    abstime.tv_sec = time_sec(then);
    abstime.tv_nsec = time_usec(then) * 1000; /* nanoseconds */

    rv = pthread_cond_timedwait(&cond->cond, &mutex->mutex, &abstime);
    if (ETIMEDOUT == rv) {
        return rv;
    }
    return rv;
}


int thread_cond_signal(thread_cond_t *cond)
{
    int rv;

    rv = pthread_cond_signal(&cond->cond);
    return rv;
}

int thread_cond_broadcast(thread_cond_t *cond)
{
    int rv;

    rv = pthread_cond_broadcast(&cond->cond);
    return rv;
}

int thread_cond_destroy(thread_cond_t *cond)
{
    int rv;

    rv = pthread_cond_destroy(&cond->cond);
    return rv;
}

#endif /* HAS_THREADS */
