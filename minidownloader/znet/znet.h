#ifndef ZNET_H
#define ZNET_H

#include "pstdint.h"

#ifdef __cplusplus
	extern "C"{
#endif

#ifdef _MSC_VER
		typedef unsigned __int64 uint64_t;
		typedef unsigned __int32 uint32_t;
		typedef unsigned char uint8_t;
		typedef unsigned short uint16_t;
#else
#include <stdint.h>
#endif

#if !defined(WIN32)
#define ZNET_DECLARE(type)	type
#define ZNET_DECLARE_NONSTD(type)	type
#define ZNET_DECLARE_DATA
#else
#define ZNET_DECLARE(type)	__declspec(dllexport) type __stdcall
#define ZNET_DECLARE_NONSTD(type)	__declspec(dllexport) type
#define ZNET_DECLARE_DATA		__declspec(dllexport)
#endif

typedef int (*data_process_fp)(uint8_t *buf,uint32_t len,uint32_t *off);

typedef struct{
	char ip[32];
	uint16_t port;
	uint32_t max_peers;
	data_process_fp func;
}ns_arg_t;

typedef struct{
	char ip[32];
	uint16_t port;
	int timeout;
	data_process_fp func;
}nc_arg_t;

//////////////////////////////////////////////////////////////////////
//                 server interface
/////////////////////////////////////////////////////////////////////

typedef struct net_server_t net_server_t;

ZNET_DECLARE(int) ns_start_daemon(net_server_t **ns,const ns_arg_t *ns_arg);
ZNET_DECLARE(int) ns_stop_daemon(net_server_t *ns);
ZNET_DECLARE(int) ns_getpeeraddr(net_server_t *ns,uint64_t peer_id,char *ip);
ZNET_DECLARE(int) ns_sendmsg(net_server_t *ns,uint64_t peer_id,void *msg,uint32_t len);
/*
 * @param timeout milliseconds
 * rv: 0 recv one data message
 *    -1 no message
 *    -2 one disconnect message.
 */
ZNET_DECLARE(int) ns_recvmsg(net_server_t *ns,void **msg,uint32_t *len,uint64_t *peer_id,
	uint32_t timeout);
ZNET_DECLARE(int) ns_tryrecvmsg(net_server_t *ns,void **msg,uint32_t *len,uint64_t *peer_id);
ZNET_DECLARE(int) ns_free(net_server_t *ns,void *buf);
ZNET_DECLARE(int) ns_disconnect(net_server_t *ns,uint64_t id);

//////////////////////////////////////////////////////////////////////
//                 client interface
/////////////////////////////////////////////////////////////////////

typedef struct net_client_t net_client_t;

ZNET_DECLARE(int) nc_connect(net_client_t **nc,const nc_arg_t *nc_arg);
ZNET_DECLARE(int) nc_disconnect(net_client_t *nc);
ZNET_DECLARE(int) nc_sendmsg(net_client_t *nc,void *msg,uint32_t len);
ZNET_DECLARE(int) nc_recvmsg(net_client_t *nc,void **msg,uint32_t *len,uint32_t timeout);
ZNET_DECLARE(int) nc_tryrecvmsg(net_client_t *nc,void **msg,uint32_t *len);
ZNET_DECLARE(int) nc_free(net_client_t *nc,void *buf);

#ifdef __cplusplus
	}
#endif

#endif
