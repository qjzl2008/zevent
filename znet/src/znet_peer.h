#ifndef ZNET_PEER_H
#define ZNET_PEER_H

#include <netinet/in.h> 
#include "hashtable.h"
#include "znet_types.h"
#include "znet_server.h"
#include "evloop/evloop.h"
#include "iobuf.h"

struct peer {
    int sd;
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
    uint16_t flags;
    uint32_t id;
};

HTBL_TYPE(ptbl, peer, uint32_t, id, chain);

int peer_create_in(int fd,struct sockaddr_in addr,struct net_server_t *ns);
int peer_kill(struct peer *p);
int peer_io_process(const ev_state_t *ev);

#endif
