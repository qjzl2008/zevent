#ifndef THREAD_COND_H
#define THREAD_COND_H

#include "thread_mutex.h"
#include "thread_cond.h"

#include <pthread.h>

#if HAS_THREADS
struct thread_cond_t {
    pthread_cond_t cond;
};
#endif

#endif  

