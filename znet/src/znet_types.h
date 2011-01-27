#ifndef ZNET_TYPES_H
#define ZNET_TYPES_H

#include <stdint.h>
#include "mqueue.h"

typedef struct ev_state_t ev_state_t;
struct ev_state_t{
	int fd;
	short ev_type;
	void *arg;
};

struct msg_t {
	uint32_t peer_id;
	uint32_t len;
	char *buf;
        BTPDQ_ENTRY(msg_t) msg_entry;
};

BTPDQ_HEAD(msg_tq, msg_t);

#endif
