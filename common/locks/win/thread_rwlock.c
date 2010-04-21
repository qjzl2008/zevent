#include "arch_thread_rwlock.h"

int thread_rwlock_create(thread_rwlock_t **rwlock)
{
    *rwlock = (thread_rwlock_t *)malloc(sizeof(**rwlock));

    (*rwlock)->readers     = 0;

    if (! ((*rwlock)->read_event = CreateEvent(NULL, TRUE, FALSE, NULL))) {
        *rwlock = NULL;
        return -1;
    }

    if (! ((*rwlock)->write_mutex = CreateMutex(NULL, FALSE, NULL))) {
        CloseHandle((*rwlock)->read_event);
        *rwlock = NULL;
        return -1;
    }


    return 0;
}

static int thread_rwlock_rdlock_core(thread_rwlock_t *rwlock,
                                                  DWORD  milliseconds)
{
    DWORD   code = WaitForSingleObject(rwlock->write_mutex, milliseconds);

    if (code == WAIT_FAILED || code == WAIT_TIMEOUT)
        return code == WAIT_TIMEOUT ? RWLOCK_TIMEOUT : -1;

    /* We've successfully acquired the writer mutex, we can't be locked
     * for write, so it's OK to add the reader lock.  The writer mutex
     * doubles as race condition protection for the readers counter.   
     */
    InterlockedIncrement(&rwlock->readers);
    
    if (! ResetEvent(rwlock->read_event))
        return -1;
    
    if (! ReleaseMutex(rwlock->write_mutex))
        return -1;
    
    return 0;
}

int thread_rwlock_rdlock(thread_rwlock_t *rwlock)
{
    return thread_rwlock_rdlock_core(rwlock, INFINITE);
}

int thread_rwlock_tryrdlock(thread_rwlock_t *rwlock)
{
    return thread_rwlock_rdlock_core(rwlock, 0);
}

int thread_rwlock_wrlock_core(thread_rwlock_t *rwlock, DWORD milliseconds)
{
    DWORD   code = WaitForSingleObject(rwlock->write_mutex, milliseconds);

    if (code == WAIT_FAILED || code == WAIT_TIMEOUT)
        return code == WAIT_TIMEOUT ? RWLOCK_TIMEOUT : -1;

    /* We've got the writer lock but we have to wait for all readers to
     * unlock before it's ok to use it.
     */
    if (rwlock->readers) {
        /* Must wait for readers to finish before returning, unless this
         * is an trywrlock (milliseconds == 0):
         */
        code = milliseconds
          ? WaitForSingleObject(rwlock->read_event, milliseconds)
          : WAIT_TIMEOUT;
        
	if (code == WAIT_FAILED || code == WAIT_TIMEOUT) {
		/* Unable to wait for readers to finish, release write lock: */
		if (! ReleaseMutex(rwlock->write_mutex))
			return -1;

		return code == WAIT_TIMEOUT ? RWLOCK_TIMEOUT : -1;
	}
    }

    return 0;
}

int thread_rwlock_wrlock(thread_rwlock_t *rwlock)
{
    return thread_rwlock_wrlock_core(rwlock, INFINITE);
}

int thread_rwlock_trywrlock(thread_rwlock_t *rwlock)
{
    return thread_rwlock_wrlock_core(rwlock, 0);
}

int thread_rwlock_unlock(thread_rwlock_t *rwlock)
{
    int rv = 0;
    DWORD ret = 0;

    /* First, guess that we're unlocking a writer */
    if (! ReleaseMutex(rwlock->write_mutex))
        ret = GetLastError();
    
    if (ret == ERROR_NOT_OWNER) {
        /* Nope, we must have a read lock */
        if (rwlock->readers &&
            ! InterlockedDecrement(&rwlock->readers) &&
            ! SetEvent(rwlock->read_event)) {
            ret = GetLastError();
	    rv = (ret == 0) ? 0 : -1;
        }
        else {
            rv = 0;
        }
    }

    return rv;
}

int thread_rwlock_destroy(thread_rwlock_t *rwlock)
{
    if (! CloseHandle(rwlock->read_event))
        return -1;

    if (! CloseHandle(rwlock->write_mutex))
        return -1;
    
    return 0;

}

