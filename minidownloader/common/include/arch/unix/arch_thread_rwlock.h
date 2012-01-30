#ifndef UNIX_THREAD_RWLOCK_H
#define UNIX_THREAD_RWLOCK_H

#include "thread_rwlock.h"

#include <pthread.h>

#if HAS_THREADS

struct thread_rwlock_t {
    pthread_rwlock_t rwlock;
};


#endif

#endif  /* THREAD_RWLOCK_H */

