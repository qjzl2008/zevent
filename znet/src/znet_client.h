#ifndef ZNET_CLIENT_H
#define ZNET_CLIENT_H

#include "znet.h"
#include "common/include/queue.h"
#include "evloop/evloop.h"
#include "znet_types.h"
#include "znet_cpeer.h"

#define C_MAX_QUEUE_CAPACITY (20000)
struct net_client_t {
    int endgame;
    int epfd;
    int sd;
    struct fdev ev; 

    struct cpeer *peer;
    struct thread_mutex_t *peer_mutex;

    queue_t *recv_queue;
    //struct msg_tq recv_queue;
    //struct thread_mutex_t *recv_mutex;

    pthread_t td_start;
    pthread_t td_evloop;

    data_process_fp func;
    struct allocator_t *allocator;
    struct thread_mutex_t *mpool_mutex;
};

#endif

