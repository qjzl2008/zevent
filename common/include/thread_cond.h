#ifndef THREAD_COND_H
#define THREAD_COND_H

/**
 * @file thread_cond.h
 * @brief Condition Variable Routines
 */

#include "thread_mutex.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#if HAS_THREADS

typedef enum{
	COND_ETIMEUP = 1
}COND_ERR_TYPE;

/**
 * @defgroup thread_cond Condition Variable Routines
 * @ingroup 
 * @{
 */

/** Opaque structure for thread condition variables */
typedef struct thread_cond_t thread_cond_t;

/**
 * Note: destroying a condition variable (or likewise, destroying or
 * clearing the pool from which a condition variable was allocated) if
 * any threads are blocked waiting on it gives undefined results.
 */

/**
 * Create and initialize a condition variable that can be used to signal
 * and schedule threads in a single process.
 * @param cond the memory address where the newly created condition variable
 *        will be stored.
 */
int thread_cond_create(thread_cond_t **cond);

/**
 * Put the active calling thread to sleep until signaled to wake up. Each
 * condition variable must be associated with a mutex, and that mutex must
 * be locked before  calling this function, or the behavior will be
 * undefined. As the calling thread is put to sleep, the given mutex
 * will be simultaneously released; and as this thread wakes up the lock
 * is again simultaneously acquired.
 * @param cond the condition variable on which to block.
 * @param mutex the mutex that must be locked upon entering this function,
 *        is released while the thread is asleep, and is again acquired before
 *        returning from this function.
 * @remark Spurious wakeups may occur. Before and after every call to wait on
 * a condition variable, the caller should test whether the condition is already
 * met.
 */
int thread_cond_wait(thread_cond_t *cond,
                                               thread_mutex_t *mutex);

/**
 * Put the active calling thread to sleep until signaled to wake up or
 * the timeout is reached. Each condition variable must be associated
 * with a mutex, and that mutex must be locked before calling this
 * function, or the behavior will be undefined. As the calling thread
 * is put to sleep, the given mutex will be simultaneously released;
 * and as this thread wakes up the lock is again simultaneously acquired.
 * @param cond the condition variable on which to block.
 * @param mutex the mutex that must be locked upon entering this function,
 *        is released while the thread is asleep, and is again acquired before
 *        returning from this function.
 * @param timeout The amount of time in microseconds to wait. This is 
 *        a maximum, not a minimum. If the condition is signaled, we 
 *        will wake up before this time, otherwise the error TIMEUP
 *        is returned.
 */
int thread_cond_timedwait(thread_cond_t *cond,
                                                    thread_mutex_t *mutex,
                                                    long long timeout);

/**
 * Signals a single thread, if one exists, that is blocking on the given
 * condition variable. That thread is then scheduled to wake up and acquire
 * the associated mutex. Although it is not required, if predictable scheduling
 * is desired, that mutex must be locked while calling this function.
 * @param cond the condition variable on which to produce the signal.
 * @remark If no threads are waiting on the condition variable, nothing happens.
 */
int thread_cond_signal(thread_cond_t *cond);

/**
 * Signals all threads blocking on the given condition variable.
 * Each thread that was signaled is then scheduled to wake up and acquire
 * the associated mutex. This will happen in a serialized manner.
 * @param cond the condition variable on which to produce the broadcast.
 * @remark If no threads are waiting on the condition variable, nothing happens.
 */
int thread_cond_broadcast(thread_cond_t *cond);

/**
 * Destroy the condition variable and free the associated memory.
 * @param cond the condition variable to destroy.
 */
int thread_cond_destroy(thread_cond_t *cond);

#endif /* HAS_THREADS */

/** @} */

#ifdef __cplusplus
}
#endif

#endif  /* ! THREAD_COND_H */
