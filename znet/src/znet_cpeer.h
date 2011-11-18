#ifndef ZNET_CPEER_H
#define ZNET_CPEER_H

#include <netinet/in.h> 
#include "hashtable.h"
#include "znet_types.h"
#include "znet_client.h"
#include "evloop/evloop.h"
#include "iobuf.h"

enum {
	CPEER_DISCONNECTED = 0,
	CPEER_CONNECTED = 1
};

struct cpeer {
    int sd;
    struct net_client_t *nc;
    struct fdev ioev;
    struct sockaddr_in addr;

    struct msg_tq send_queue;
    struct thread_mutex_t *sq_mutex;

    struct iobuf sendbuf;
    struct iobuf recvbuf;

    struct allocator_t *allocator;
    struct thread_mutex_t *mpool_mutex;

    unsigned long rate_up, rate_dwn;
    unsigned long count_up, count_dwn;

    long t_created;
    long t_lastwrite;
    long t_wantwrite;

    HTBL_ENTRY(chain);
    volatile uint16_t status;
    uint64_t id;
    volatile uint32_t refcount;
};

int cpeer_create_out(int fd,struct net_client_t *nc);
int cpeer_kill(struct cpeer *p);

#endif
