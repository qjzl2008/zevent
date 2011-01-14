#ifndef ZNET_SERVER_H
#define ZNET_SERVER_H

#include "znet.h"
#include "common/include/queue.h"
#include "evloop/evloop.h"
#include "znet_types.h"

#define MAX_WORKERS (64)

struct net_server_t {
    int endgame;

    int fd;
    struct fdev ev; 

    unsigned long rate_up, rate_dwn;
    unsigned long long uploaded, downloaded;

    unsigned npeers;

    struct ptbl *ptbl;//peer hash table
    struct thread_mutex_t *ptbl_mutex;
    
    struct msg_tq recv_queue;
    struct thread_mutex_t *recv_mutex;

    queue_t *fd_queue;//epoll事件队列

    pthread_t td_start;
    pthread_t td_evloop;
    pthread_t td_workers[MAX_WORKERS];
    int nworkers;

    data_process_fp func;

    struct allocator_t *allocator;
    struct thread_mutex_t *mpool_mutex;
};

#endif

