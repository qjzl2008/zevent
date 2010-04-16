#ifndef THREAD_MUTEX_H
#define THREAD_MUTEX_H

/**
 * @file thread_mutex.h
 * @brief Thread Mutex Routines
 */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @defgroup thread_mutex Thread Mutex Routines
 * @ingroup mutex 
 * @{
 */

/** Opaque thread-local mutex structure */
typedef struct thread_mutex_t thread_mutex_t;

#define THREAD_MUTEX_DEFAULT  0x0   /**< platform-optimal lock behavior */
#define THREAD_MUTEX_NESTED   0x1   /**< enable nested (recursive) locks */
#define THREAD_MUTEX_UNNESTED 0x2   /**< disable nested locks */


/**
 * Create and initialize a mutex that can be used to synchronize threads.
 * @param mutex the memory address where the newly created mutex will be
 *        stored.
 * @param flags Or'ed value of:
 * <PRE>
 *           THREAD_MUTEX_DEFAULT   platform-optimal lock behavior.
 *           THREAD_MUTEX_NESTED    enable nested (recursive) locks.
 *           THREAD_MUTEX_UNNESTED  disable nested locks (non-recursive).
 * </PRE>
 * @warning Be cautious in using THREAD_MUTEX_DEFAULT.  While this is the
 * most optimial mutex based on a given platform's performance charateristics,
 * it will behave as either a nested or an unnested lock.
 */
int thread_mutex_create(thread_mutex_t *mutex,
                                                  unsigned int flags);
/**
 * Acquire the lock for the given mutex. If the mutex is already locked,
 * the current thread will be put to sleep until the lock becomes available.
 * @param mutex the mutex on which to acquire the lock.
 */
int thread_mutex_lock(thread_mutex_t *mutex);

/**
 * Attempt to acquire the lock for the given mutex. If the mutex has already
 * been acquired, the call returns immediately with APR_EBUSY. Note: it
 * is important that the APR_STATUS_IS_EBUSY(s) macro be used to determine
 * if the return value was APR_EBUSY, for portability reasons.
 * @param mutex the mutex on which to attempt the lock acquiring.
 */
int thread_mutex_trylock(thread_mutex_t *mutex);

/**
 * Release the lock for the given mutex.
 * @param mutex the mutex from which to release the lock.
 */
int thread_mutex_unlock(thread_mutex_t *mutex);

/**
 * Destroy the mutex and free the memory associated with the lock.
 * @param mutex the mutex to destroy.
 */
int thread_mutex_destroy(thread_mutex_t *mutex);


/** @} */

#ifdef __cplusplus
}
#endif
#endif

