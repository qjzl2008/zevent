#ifndef THREAD_MUTEX_H
#define THREAD_MUTEX_H

#include "thread_mutex.h"

#if HAVE_PTHREAD_H
#include <pthread.h>
#endif

#if HAS_THREADS
struct apr_thread_mutex_t {
    apr_pool_t *pool;
    pthread_mutex_t mutex;
};
#endif

#endif  /* THREAD_MUTEX_H */

