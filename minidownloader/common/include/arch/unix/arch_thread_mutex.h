#ifndef UNIX_THREAD_MUTEX_H
#define UNIX_THREAD_MUTEX_H

#include "thread_mutex.h"

#include <pthread.h>
struct thread_mutex_t {
    pthread_mutex_t mutex;
};
#endif


