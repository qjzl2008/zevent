#ifndef ZNET_PEER_H
#define ZNET_PEER_H

#include "hashtable.h"
#include "znet_types.h"
#include "znet_server.h"
#include "evloop/evloop.h"
#include "iobuf.h"

enum {
	PEER_DISCONNECTED = 0,
	PEER_CONNECTED = 1
};

struct peer {
    SOCKET sd;
    struct net_server_t *ns;
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

HTBL_TYPE(ptbl, peer, uint64_t, id, chain);

int peer_create_in(SOCKET fd,struct sockaddr_in addr,struct net_server_t *ns);
int peer_kill(struct peer *p);

#endif
