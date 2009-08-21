/**
 * @file  event/fdqueue.h
 * @brief fd queue declarations
 *
 * @addtogroup APACHE_MPM_EVENT
 * @{
 */

#ifndef FDQUEUE_H
#define FDQUEUE_H
#include <stdlib.h>
#if APR_HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <apr_thread_mutex.h>
#include <apr_thread_cond.h>
#include <sys/types.h>
#if APR_HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#include <apr_errno.h>

#include "zevent.h"

typedef struct fd_queue_info_t fd_queue_info_t;

apr_status_t zevent_queue_info_create(fd_queue_info_t ** queue_info,
                                  apr_pool_t * pool, int max_idlers);
apr_status_t zevent_queue_info_set_idle(fd_queue_info_t * queue_info,
                                    apr_pool_t * pool_to_recycle);
apr_status_t zevent_queue_info_wait_for_idler(fd_queue_info_t * queue_info);
apr_status_t zevent_queue_info_term(fd_queue_info_t * queue_info);

struct fd_queue_elem_t
{
    apr_socket_t *sd;
    apr_pool_t *p;
    conn_state_t *cs;
};
typedef struct fd_queue_elem_t fd_queue_elem_t;

struct fd_queue_t
{
    fd_queue_elem_t *data;
    int nelts;
    int bounds;
    apr_thread_mutex_t *one_big_mutex;
    apr_thread_cond_t *not_empty;
    int terminated;
};
typedef struct fd_queue_t fd_queue_t;

void zevent_pop_pool(apr_pool_t ** recycled_pool, fd_queue_info_t * queue_info);
void zevent_push_pool(fd_queue_info_t * queue_info,          
                                    apr_pool_t * pool_to_recycle);

apr_status_t zevent_queue_init(fd_queue_t * queue, int queue_capacity,
                           apr_pool_t * a);
apr_status_t zevent_queue_push(fd_queue_t * queue, apr_socket_t * sd,
                           conn_state_t * cs, apr_pool_t * p);
apr_status_t zevent_queue_pop(fd_queue_t * queue, apr_socket_t ** sd,
                          conn_state_t ** cs, apr_pool_t ** p);
apr_status_t zevent_queue_interrupt_all(fd_queue_t * queue);
apr_status_t zevent_queue_term(fd_queue_t * queue);

#endif /* FDQUEUE_H */
/** @} */
