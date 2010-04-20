#include "thread_mutex.h"
#include "thread_cond.h"
#include "queue.h"

#if HAS_THREADS

struct queue_t {
    void              **data;
    unsigned int        nelts; /**< # elements */
    unsigned int        in;    /**< next empty location */
    unsigned int        out;   /**< next filled location */
    unsigned int        bounds;/**< max size of queue */
    unsigned int        full_waiters;
    unsigned int        empty_waiters;
    thread_mutex_t *one_big_mutex;
    thread_cond_t  *not_empty;
    thread_cond_t  *not_full;
    int                 terminated;
};

/**
 * Detects when the queue_t is full. This utility function is expected
 * to be called from within critical sections, and is not threadsafe.
 */
#define queue_full(queue) ((queue)->nelts == (queue)->bounds)

/**
 * Detects when the queue_t is empty. This utility function is expected
 * to be called from within critical sections, and is not threadsafe.
 */
#define queue_empty(queue) ((queue)->nelts == 0)

/**
 * queue_t destroy
 */
int queue_destroy(queue_t *queue) 
{
    /* Ignore errors here, we can't do anything about them anyway. */

    thread_cond_destroy(queue->not_empty);
    thread_cond_destroy(queue->not_full);
    thread_mutex_destroy(queue->one_big_mutex);

    return 0;
}

/**
 * Initialize the queue_t.
 */
int queue_create(queue_t **q, 
                                           unsigned int queue_capacity)
{
    int rv;
    queue_t *queue;
    queue = (queue_t*)malloc(sizeof(queue_t));
    *q = queue;

    /* nested doesn't work ;( */
    rv = thread_mutex_create(&queue->one_big_mutex,
                                 THREAD_MUTEX_UNNESTED);
    if (rv != 0) {
        return rv;
    }

    rv = thread_cond_create(&queue->not_empty);
    if (rv != 0) {
        return rv;
    }

    rv = thread_cond_create(&queue->not_full);
    if (rv != 0) {
        return rv;
    }

    /* Set all the data in the queue to NULL */
    queue->data = malloc(queue_capacity * sizeof(void*));
    queue->bounds = queue_capacity;
    queue->nelts = 0;
    queue->in = 0;
    queue->out = 0;
    queue->terminated = 0;
    queue->full_waiters = 0;
    queue->empty_waiters = 0;

    return 0;
}

/**
 * Push new data onto the queue. Blocks if the queue is full. Once
 * the push operation has completed, it signals other threads waiting
 * in queue_pop() that they may continue consuming sockets.
 */
int queue_push(queue_t *queue, void *data)
{
    int rv;

    if (queue->terminated) {
        return QUEUE_EOF; /* no more elements ever again */
    }

    rv = thread_mutex_lock(queue->one_big_mutex);
    if (rv != 0) {
        return rv;
    }

    if (queue_full(queue)) {
        if (!queue->terminated) {
            queue->full_waiters++;
            rv = thread_cond_wait(queue->not_full, queue->one_big_mutex);
            queue->full_waiters--;
            if (rv != 0) {
                thread_mutex_unlock(queue->one_big_mutex);
                return rv;
            }
        }
        /* If we wake up and it's still empty, then we were interrupted */
        if (queue_full(queue)) {
            rv = thread_mutex_unlock(queue->one_big_mutex);
            if (rv != 0) {
                return rv;
            }
            if (queue->terminated) {
                return QUEUE_EOF; /* no more elements ever again */
            }
            else {
                return QUEUE_EINTR;
            }
        }
    }

    queue->data[queue->in] = data;
    queue->in = (queue->in + 1) % queue->bounds;
    queue->nelts++;

    if (queue->empty_waiters) {
        rv = thread_cond_signal(queue->not_empty);
        if (rv != 0) {
            thread_mutex_unlock(queue->one_big_mutex);
            return rv;
        }
    }

    rv = thread_mutex_unlock(queue->one_big_mutex);
    return rv;
}

/**
 * Push new data onto the queue. If the queue is full, return QUEUE_EAGAIN. If
 * the push operation completes successfully, it signals other threads
 * waiting in queue_pop() that they may continue consuming sockets.
 */
int queue_trypush(queue_t *queue, void *data)
{
    int rv;

    if (queue->terminated) {
        return QUEUE_EOF; /* no more elements ever again */
    }

    rv = thread_mutex_lock(queue->one_big_mutex);
    if (rv != 0) {
        return rv;
    }

    if (queue_full(queue)) {
        rv = thread_mutex_unlock(queue->one_big_mutex);
        return QUEUE_EAGAIN;
    }
    
    queue->data[queue->in] = data;
    queue->in = (queue->in + 1) % queue->bounds;
    queue->nelts++;

    if (queue->empty_waiters) {
        rv  = thread_cond_signal(queue->not_empty);
        if (rv != 0) {
            thread_mutex_unlock(queue->one_big_mutex);
            return rv;
        }
    }

    rv = thread_mutex_unlock(queue->one_big_mutex);
    return rv;
}

/**
 * not thread safe
 */
unsigned int queue_size(queue_t *queue) {
    return queue->nelts;
}

/**
 * Retrieves the next item from the queue. If there are no
 * items available, it will block until one becomes available.
 * Once retrieved, the item is placed into the address specified by
 * 'data'.
 */
int queue_pop(queue_t *queue, void **data)
{
    int rv;

    if (queue->terminated) {
        return QUEUE_EOF; /* no more elements ever again */
    }

    rv = thread_mutex_lock(queue->one_big_mutex);
    if (rv != 0) {
        return rv;
    }

    /* Keep waiting until we wake up and find that the queue is not empty. */
    if (queue_empty(queue)) {
        if (!queue->terminated) {
            queue->empty_waiters++;
            rv = thread_cond_wait(queue->not_empty, queue->one_big_mutex);
            queue->empty_waiters--;
            if (rv != 0) {
                thread_mutex_unlock(queue->one_big_mutex);
                return rv;
            }
        }
        /* If we wake up and it's still empty, then we were interrupted */
        if (queue_empty(queue)) {
            rv = thread_mutex_unlock(queue->one_big_mutex);
            if (rv != 0) {
                return rv;
            }
            if (queue->terminated) {
                return QUEUE_EOF; /* no more elements ever again */
            }
            else {
                return QUEUE_EINTR;
            }
        }
    } 

    *data = queue->data[queue->out];
    queue->nelts--;

    queue->out = (queue->out + 1) % queue->bounds;
    if (queue->full_waiters) {
        rv = thread_cond_signal(queue->not_full);
        if (rv != 0) {
            thread_mutex_unlock(queue->one_big_mutex);
            return rv;
        }
    }

    rv = thread_mutex_unlock(queue->one_big_mutex);
    return rv;
}

/**
 * Retrieves the next item from the queue. If there are no
 * items available, return QUEUE_EAGAIN.  Once retrieved,
 * the item is placed into the address specified by 'data'.
 */
int queue_trypop(queue_t *queue, void **data)
{
    int rv;

    if (queue->terminated) {
        return QUEUE_EOF; /* no more elements ever again */
    }

    rv = thread_mutex_lock(queue->one_big_mutex);
    if (rv != 0) {
        return rv;
    }

    if (queue_empty(queue)) {
        rv = thread_mutex_unlock(queue->one_big_mutex);
        return QUEUE_EAGAIN;
    } 

    *data = queue->data[queue->out];
    queue->nelts--;

    queue->out = (queue->out + 1) % queue->bounds;
    if (queue->full_waiters) {
        rv = thread_cond_signal(queue->not_full);
        if (rv != 0) {
            thread_mutex_unlock(queue->one_big_mutex);
            return rv;
        }
    }

    rv = thread_mutex_unlock(queue->one_big_mutex);
    return rv;
}

int queue_interrupt_all(queue_t *queue)
{
    int rv;
    if ((rv = thread_mutex_lock(queue->one_big_mutex)) != 0) {
        return rv;
    }
    thread_cond_broadcast(queue->not_empty);
    thread_cond_broadcast(queue->not_full);

    if ((rv = thread_mutex_unlock(queue->one_big_mutex)) != 0) {
        return rv;
    }

    return 0;
}

int queue_term(queue_t *queue)
{
    int rv;

    if ((rv = thread_mutex_lock(queue->one_big_mutex)) != 0) {
        return rv;
    }

    /* we must hold one_big_mutex when setting this... otherwise,
     * we could end up setting it and waking everybody up just after a 
     * would-be popper checks it but right before they block
     */
    queue->terminated = 1;
    if ((rv = thread_mutex_unlock(queue->one_big_mutex)) != 0) {
        return rv;
    }
    return queue_interrupt_all(queue);
}

#endif /* HAS_THREADS */
