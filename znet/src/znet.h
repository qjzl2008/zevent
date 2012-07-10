#ifndef ZNET_H
#define ZNET_H

#include <stdint.h>

#ifdef __cplusplus
	extern "C"{
#endif

/**
 * 消息解析函数
 * @param buf 底层提供数据缓冲
 * @param len 数据长度
 * @param off 上层分析后返回的分割位置
 * rv 返回0表示解析成功有一完整消息,off有效
 *    返回<0表示解析失败，数据还不完整，需要继续接收
 *    等待下次回调处理
 */
typedef int (*data_process_fp)(uint8_t *buf,uint32_t len,uint32_t *off);
typedef int (*msg_process_fp)(void *net,uint64_t peerid, void *buf,uint32_t len);

typedef struct{
	char ip[32];
	uint16_t port;
	uint32_t max_peers;
	data_process_fp data_func;
	msg_process_fp msg_func;
}ns_arg_t;

typedef struct{
	char ip[32];
	uint16_t port;
	int timeout;
	data_process_fp data_func;
	msg_process_fp msg_func;
}nc_arg_t;

//////////////////////////////////////////////////////////////////////
//                 server interface
/////////////////////////////////////////////////////////////////////

typedef struct net_server_t net_server_t;
int ns_start_daemon(net_server_t **ns,const ns_arg_t *ns_arg);
int ns_stop_daemon(net_server_t *ns);
int ns_getpeeraddr(net_server_t *ns,uint64_t peer_id,char *ip);
int ns_sendmsg(net_server_t *ns,uint64_t peer_id,void *msg,uint32_t len);
int ns_broadcastmsg(net_server_t *ns,void *msg,uint32_t len);
/*
 * @param timeout milliseconds
 * rv: 0 recv one data message
 *    -1 no message
 *    -2 one disconnect message.
 */
int ns_recvmsg(net_server_t *ns,void **msg,uint32_t *len,uint64_t *peer_id,
	uint32_t timeout);
int ns_tryrecvmsg(net_server_t *ns,void **msg,uint32_t *len,uint64_t *peer_id);
int ns_free(net_server_t *ns,void *buf);
int ns_disconnect(net_server_t *ns,uint64_t id);

//////////////////////////////////////////////////////////////////////
//                 client interface
/////////////////////////////////////////////////////////////////////

typedef struct net_client_t net_client_t;

int nc_connect(net_client_t **nc,const nc_arg_t *nc_arg);
int nc_disconnect(net_client_t *nc);
int nc_sendmsg(net_client_t *nc,void *msg,uint32_t len);
int nc_recvmsg(net_client_t *nc,void **msg,uint32_t *len,uint32_t timeout);
int nc_tryrecvmsg(net_client_t *nc,void **msg,uint32_t *len);
int nc_free(net_client_t *nc,void *buf);

#ifdef __cplusplus
	}
#endif

#endif
