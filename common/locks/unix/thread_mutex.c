#include "arch_thread_mutex.h"

#if HAS_THREADS

static int thread_mutex_cleanup(void *data)
{
    thread_mutex_t *mutex = data;
    apr_status_t rv;

    rv = pthread_mutex_destroy(&mutex->mutex);
    return rv;
} 

int thread_mutex_create(thread_mutex_t **mutex,
                                                  unsigned int flags,
                                                  allocator_t *allocator)
{
    thread_mutex_t *new_mutex;
    int rv;
    
    //here
    new_mutex = apr_pcalloc(pool, sizeof(apr_thread_mutex_t));
    new_mutex->pool = pool;

#ifdef HAVE_PTHREAD_MUTEX_RECURSIVE
    if (flags & APR_THREAD_MUTEX_NESTED) {
        pthread_mutexattr_t mattr;
        
        rv = pthread_mutexattr_init(&mattr);
        if (rv) return rv;
        
        rv = pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE);
        if (rv) {
            pthread_mutexattr_destroy(&mattr);
            return rv;
        }
         
        rv = pthread_mutex_init(&new_mutex->mutex, &mattr);
        
        pthread_mutexattr_destroy(&mattr);
    } else
#endif
        rv = pthread_mutex_init(&new_mutex->mutex, NULL);

    if (rv) {
#ifdef HAVE_ZOS_PTHREADS
        rv = errno;
#endif
        return rv;
    }

    apr_pool_cleanup_register(new_mutex->pool,
                              new_mutex, thread_mutex_cleanup,
                              apr_pool_cleanup_null);

    *mutex = new_mutex;
    return APR_SUCCESS;
}

APR_DECLARE(apr_status_t) apr_thread_mutex_lock(apr_thread_mutex_t *mutex)
{
    apr_status_t rv;

    rv = pthread_mutex_lock(&mutex->mutex);
#ifdef HAVE_ZOS_PTHREADS
    if (rv) {
        rv = errno;
    }
#endif
    
    return rv;
}

APR_DECLARE(apr_status_t) apr_thread_mutex_trylock(apr_thread_mutex_t *mutex)
{
    apr_status_t rv;

    rv = pthread_mutex_trylock(&mutex->mutex);
    if (rv) {
#ifdef HAVE_ZOS_PTHREADS
        rv = errno;
#endif
        return (rv == EBUSY) ? APR_EBUSY : rv;
    }

    return APR_SUCCESS;
}

APR_DECLARE(apr_status_t) apr_thread_mutex_unlock(apr_thread_mutex_t *mutex)
{
    apr_status_t status;

    status = pthread_mutex_unlock(&mutex->mutex);
#ifdef HAVE_ZOS_PTHREADS
    if (status) {
        status = errno;
    }
#endif

    return status;
}

APR_DECLARE(apr_status_t) apr_thread_mutex_destroy(apr_thread_mutex_t *mutex)
{
    return apr_pool_cleanup_run(mutex->pool, mutex, thread_mutex_cleanup);
}

APR_POOL_IMPLEMENT_ACCESSOR(thread_mutex)

#endif /* HAS_THREADS */
