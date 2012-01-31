#ifndef ZNET_TYPES_H
#define ZNET_TYPES_H

#include "pstdint.h"
#include "mqueue.h"

enum msg_type{
    MSG_DATA = 0,
    MSG_DISCONNECT = 1
};
struct msg_t {
	uint64_t peer_id;
	uint32_t type;
	uint32_t len;
	uint8_t *buf;
        BTPDQ_ENTRY(msg_t) msg_entry;
};

BTPDQ_HEAD(msg_tq, msg_t);

#endif
