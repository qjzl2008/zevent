#ifndef ZNET_SERVER_H
#define ZNET_SERVER_H

#include "znet.h"
#include "common/include/queue.h"
#include "evloop/evloop.h"
#include "znet_types.h"

#define MAX_WORKERS (64)
#define MAX_QUEUE_CAPACITY (20000)

struct net_server_t {
    int endgame;
    int epfd;
    int fd;
    struct fdev ev; 

    unsigned long rate_up, rate_dwn;
    unsigned long long uploaded, downloaded;

    unsigned npeers;
    unsigned max_peers;

    struct ptbl *ptbl;//peer hash table
    struct thread_mutex_t *ptbl_mutex;
    
//    struct msg_tq recv_queue;
//    struct thread_mutex_t *recv_mutex;
    queue_t *recv_queue;

    pthread_t td_start;
    pthread_t td_evloop;

    data_process_fp func;

    struct allocator_t *allocator;
    struct thread_mutex_t *mpool_mutex;
};

#endif

