#include "arch_thread_mutex.h"
#include "arch_thread_cond.h"

#include <limits.h>

int thread_cond_create(thread_cond_t **cond)
{
    thread_cond_t *cv;

    cv = malloc(sizeof(**cond));
    if (cv == NULL) {
        return -1;
    }
    memset(cv,0,sizeof(thread_cond_t));
    cv->semaphore = CreateSemaphore(NULL, 0, LONG_MAX, NULL);
    if (cv->semaphore == NULL) {
        return -1;
    }

    *cond = cv;
    InitializeCriticalSection(&cv->csection);

    return 0;
}

int thread_cond_destroy(thread_cond_t *cond)
{
    CloseHandle(cond->semaphore);
    DeleteCriticalSection(&cond->csection);
    free(cond);
    return 0;
}

static int _thread_cond_timedwait(thread_cond_t *cond,
                                                      thread_mutex_t *mutex,
                                                      DWORD timeout_ms )
{
    DWORD res;
    int rv;
    unsigned int wake = 0;
    unsigned long generation;

    EnterCriticalSection(&cond->csection);
    cond->num_waiting++;
    generation = cond->generation;
    LeaveCriticalSection(&cond->csection);

    thread_mutex_unlock(mutex);

    do {
        res = WaitForSingleObject(cond->semaphore, timeout_ms);

        EnterCriticalSection(&cond->csection);

        if (cond->num_wake) {
            if (cond->generation != generation) {
                cond->num_wake--;
                cond->num_waiting--;
                rv = 0;
                break;
            } else {
                wake = 1;
            }
        }
        else if (res != WAIT_OBJECT_0) {
            cond->num_waiting--;
            rv = COND_ETIMEUP;
            break;
        }

        LeaveCriticalSection(&cond->csection);

        if (wake) {
            wake = 0;
            ReleaseSemaphore(cond->semaphore, 1, NULL);
        }
    } while (1);

    LeaveCriticalSection(&cond->csection);
    thread_mutex_lock(mutex);

    return rv;
}

int thread_cond_wait(thread_cond_t *cond,
                                               thread_mutex_t *mutex)
{
    return _thread_cond_timedwait(cond, mutex, INFINITE);
}

#define time_as_msec(time) ((time) / 1000)
int thread_cond_timedwait(thread_cond_t *cond,
                                                    thread_mutex_t *mutex,
                                                    long long timeout)
{
    DWORD timeout_ms = (DWORD) time_as_msec(timeout);

    return _thread_cond_timedwait(cond, mutex, timeout_ms);
}

int thread_cond_signal(thread_cond_t *cond)
{
    unsigned int wake = 0;

    EnterCriticalSection(&cond->csection);
    if (cond->num_waiting > cond->num_wake) {
        wake = 1;
        cond->num_wake++;
        cond->generation++;
    }
    LeaveCriticalSection(&cond->csection);

    if (wake) {
        ReleaseSemaphore(cond->semaphore, 1, NULL);
    }

    return 0;
}

int thread_cond_broadcast(thread_cond_t *cond)
{
    unsigned long num_wake = 0;

    EnterCriticalSection(&cond->csection);
    if (cond->num_waiting > cond->num_wake) {
        num_wake = cond->num_waiting - cond->num_wake;
        cond->num_wake = cond->num_waiting;
        cond->generation++;
    }
    LeaveCriticalSection(&cond->csection);

    if (num_wake) {
        ReleaseSemaphore(cond->semaphore, num_wake, NULL);
    }

    return 0;
}

