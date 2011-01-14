#ifndef WIN_THREAD_COND_H
#define WIN_THREAD_COND_H

#include "thread_cond.h"

struct thread_cond_t {
    HANDLE semaphore;
    CRITICAL_SECTION csection;
    unsigned long num_waiting;
    unsigned long num_wake;
    unsigned long generation;
};

#endif  /* THREAD_COND_H */

