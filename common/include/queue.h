#ifndef QUEUE_H
#define QUEUE_H


#if HAS_THREADS

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/**
 * opaque structure
 */
typedef struct queue_t queue_t;

typedef enum {
	QUEUE_EINTR = 1,
	QUEUE_AGAIN,
	QUEUE_EOF,
}QUEUE_ERROR_TYPE;

/** 
 * create a FIFO queue
 * @param queue The new queue
 * @param queue_capacity maximum size of the queue
 * @param a pool to allocate queue from
 */
int queue_create(queue_t **queue, unsigned int queue_capacity);

/**
 * push/add an object to the queue, blocking if the queue is already full
 *
 * @param queue the queue
 * @param data the data
 * @returns QUEUE_EINTR the blocking was interrupted (try again)
 * @returns QUEUE_EOF the queue has been terminated
 * @returns 0 on a successful push
 */
int queue_push(queue_t *queue, void *data);

/**
 * pop/get an object from the queue, blocking if the queue is already empty
 *
 * @param queue the queue
 * @param data the data
 * @returns QUEUE_EINTR the blocking was interrupted (try again)
 * @returns QUEUE_EOF if the queue has been terminated
 * @returns 0 on a successful pop
 */
int queue_pop(queue_t *queue, void **data);

/**
 * push/add an object to the queue, returning immediately if the queue is full
 *
 * @param queue the queue
 * @param data the data
 * @returns QUEUE_EINTR the blocking operation was interrupted (try again)
 * @returns QUEUE_EAGAIN the queue is full
 * @returns QUEUE_EOF the queue has been terminated
 * @returns 0 on a successful push
 */
int queue_trypush(queue_t *queue, void *data);

/**
 * pop/get an object to the queue, returning immediately if the queue is empty
 *
 * @param queue the queue
 * @param data the data
 * @returns QUEUE_EINTR the blocking operation was interrupted (try again)
 * @returns QUEUE_EAGAIN the queue is empty
 * @returns QUEUE_EOF the queue has been terminated
 * @returns 0 on a successful push
 */
int queue_trypop(queue_t *queue, void **data);

/**
 * returns the size of the queue.
 *
 * @warning this is not threadsafe, and is intended for reporting/monitoring
 * of the queue.
 * @param queue the queue
 * @returns the size of the queue
 */
unsigned int queue_size(queue_t *queue);

/**
 * interrupt all the threads blocking on this queue.
 *
 * @param queue the queue
 */
int queue_interrupt_all(queue_t *queue);

/**
 * terminate the queue, sending an interrupt to all the
 * blocking threads
 *
 * @param queue the queue
 */
int queue_term(queue_t *queue);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* HAS_THREADS */

#endif /* QUEUE_H */
