#ifndef P2P_PUNCH_H
#define P2P_PUNCH_H

#include "punch_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct punch_arg_t punch_arg_t;

struct punch_arg_t{
	char sn[64];//eg:192.168.0.1:7654
	char community_name[P2P_COMMUNITY_SIZE];
	int local_port;
	int traceLevel;
};

P2P_DECLARE(int) start_punching_daemon(punch_arg_t *args, p2p_edge_t *node);
P2P_DECLARE(int) stop_punching_daemon(p2p_edge_t *node);

P2P_DECLARE(int) punching_hole(p2p_edge_t *node, const char *peer_ip,int port);

#ifdef __cplusplus
}
#endif

#endif