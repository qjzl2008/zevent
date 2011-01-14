#ifndef WIN_THREAD_RWLOCK_H
#define WIN_THREAD_RWLOCK_H

#include <windows.h>
#include "thread_rwlock.h"

struct thread_rwlock_t {
    HANDLE      write_mutex;
    HANDLE      read_event;
    LONG        readers;
};

#endif  /* THREAD_RWLOCK_H */

