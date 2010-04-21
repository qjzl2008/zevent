#ifndef THREAD_RWLOCK_H
#define THREAD_RWLOCK_H

/**
 * @file thread_rwlock.h
 * @brief Reader/Writer Lock Routines
 */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#if HAS_THREADS

typedef enum{
	RWLOCK_TIMEOUT = 1
}rwlock_err_type;

/**
 * @defgroup thread_rwlock Reader/Writer Lock Routines
 * @{
 */

/** Opaque read-write thread-safe lock. */
typedef struct thread_rwlock_t thread_rwlock_t;

/**
 * Note: The following operations have undefined results: unlocking a
 * read-write lock which is not locked in the calling thread; write
 * locking a read-write lock which is already locked by the calling
 * thread; destroying a read-write lock more than once; clearing or
 * destroying the pool from which a <b>locked</b> read-write lock is
 * allocated.
 */

/**
 * Create and initialize a read-write lock that can be used to synchronize
 * threads.
 * @param rwlock the memory address where the newly created readwrite lock
 *        will be stored.
 */
int thread_rwlock_create(thread_rwlock_t **rwlock);

/**
 * Acquire a shared-read lock on the given read-write lock. This will allow
 * multiple threads to enter the same critical section while they have acquired
 * the read lock.
 * @param rwlock the read-write lock on which to acquire the shared read.
 */
int thread_rwlock_rdlock(thread_rwlock_t *rwlock);

/**
 * Attempt to acquire the shared-read lock on the given read-write lock. This
 * is the same as thread_rwlock_rdlock(), only that the function fails
 * if there is another thread holding the write lock, or if there are any
 * write threads blocking on the lock. If the function fails for this case,
 * RWLOCK_EBUSY will be returned.
 * @param rwlock the rwlock on which to attempt the shared read.
 */
int thread_rwlock_tryrdlock(thread_rwlock_t *rwlock);

/**
 * Acquire an exclusive-write lock on the given read-write lock. This will
 * allow only one single thread to enter the critical sections. If there
 * are any threads currently holding the read-lock, this thread is put to
 * sleep until it can have exclusive access to the lock.
 * @param rwlock the read-write lock on which to acquire the exclusive write.
 */
int thread_rwlock_wrlock(thread_rwlock_t *rwlock);

/**
 * Attempt to acquire the exclusive-write lock on the given read-write lock. 
 * This is the same as thread_rwlock_wrlock(), only that the function fails
 * if there is any other thread holding the lock (for reading or writing),
 * in which case the function will return RWLOCK_EBUSY. Note: it is important
 *  to determine if the return
 * value was RWLOCK_EBUSY, for portability reasons.
 * @param rwlock the rwlock on which to attempt the exclusive write.
 */
int thread_rwlock_trywrlock(thread_rwlock_t *rwlock);

/**
 * Release either the read or write lock currently held by the calling thread
 * associated with the given read-write lock.
 * @param rwlock the read-write lock to be released (unlocked).
 */
int thread_rwlock_unlock(thread_rwlock_t *rwlock);

/**
 * Destroy the read-write lock and free the associated memory.
 * @param rwlock the rwlock to destroy.
 */
int thread_rwlock_destroy(thread_rwlock_t *rwlock);


#endif  /* HAS_THREADS */

/** @} */

#ifdef __cplusplus
}
#endif

#endif  /* ! THREAD_RWLOCK_H */
