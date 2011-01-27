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

typedef struct{
	char ip[32];
	uint16_t port;
	int nworkers;
	data_process_fp func;
}ns_arg_t;

typedef struct{
	char ip[32];
	uint16_t port;
	data_process_fp func;
}nc_arg_t;

typedef struct net_server_t net_server_t;

//server interface
int ns_start_daemon(net_server_t **ns,const ns_arg_t *ns_arg);
int ns_stop_daemon(net_server_t *ns);
int ns_sendmsg(net_server_t *ns,uint32_t peer_id,void *msg,uint32_t len);
int ns_recvmsg(net_server_t *ns,void **msg,uint32_t *len,uint32_t *peer_id);
int ns_free(net_server_t *ns,void *buf);
int ns_disconnect(net_server_t *ns,uint32_t id);

typedef struct net_client net_client_t;

//client interface
int nc_connect(net_client_t **nc,const nc_arg_t *nc_arg);
int nc_disconnect(net_client_t *nc);
int nc_sendmsg(net_client_t *nc,void *msg,uint32_t len);
int nc_recvmsg(net_client_t *nc,void *msg,uint32_t *len);

#ifdef __cplusplus
	}
#endif

#endif
