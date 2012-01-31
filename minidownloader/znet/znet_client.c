#include <fcntl.h>
#include <sys/types.h>
#define WIN32_LEAN_AND_MEAN 
#include <winsock2.h> 
#include <ws2tcpip.h>
#include <windows.h>
#include <ws2tcpip.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "allocator.h"
#include "thread_mutex.h"
#include "hashtable.h"
#include "queue.h"

#include "util.h"
#include "znet_types.h"
#include "znet_client.h"

static DWORD WINAPI event_loop(void *arg)
{
	net_client_t *nc = (net_client_t *)arg;

	evloop(&nc->endgame);
	return 0;
}

static DWORD WINAPI start_threads(void *arg)
{
	net_client_t *nc = (net_client_t *)arg;
	HANDLE td;

	td = CreateThread(NULL,0,event_loop,nc,0,NULL);
	nc->td_evloop = td;

	WaitForSingleObject(td,INFINITE);
	return 0;
}

#define HEADER_LEN (4)
static int default_process_func(uint8_t *buf,uint32_t len,uint32_t *off)
{
	uint32_t msg_len;
    if(len < HEADER_LEN)
	    return -1;
	msg_len = dec_be32(buf);
	if(len < msg_len + HEADER_LEN)
	{
		return -1;
	}
	else
	{
		*off = msg_len + HEADER_LEN;
		return 0;
	}
}


static int nc_nbconnect(SOCKET *sd, const nc_arg_t *nc_arg)
{
	int rv;
	struct addrinfo hints, *res;
	char portstr[6];

	struct timeval tm;
	fd_set wset,eset;
	FD_ZERO(&wset);
	FD_ZERO(&eset);
	tm.tv_sec=nc_arg->timeout;
	tm.tv_usec=0;

	*sd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if(*sd == SOCKET_ERROR)
	{
		rv = WSAGetLastError();
		return -1;
	}

	set_nonblocking(*sd);

	sprintf_s(portstr,sizeof(portstr),"%d",nc_arg->port);
	memset(&hints,0,sizeof(hints));
	hints.ai_family = AF_INET;
	//hints.ai_flags = AI_NUMERICHOST;
	hints.ai_socktype = SOCK_STREAM;

	if(getaddrinfo(nc_arg->ip,portstr,&hints,&res) != 0)
		return -1;

	rv = connect(*sd,res->ai_addr,(int)res->ai_addrlen);
	freeaddrinfo(res);
	if( rv== SOCKET_ERROR)
	{
		if(WSAGetLastError() != WSAEWOULDBLOCK) {
			closesocket(*sd);
			return -1;
		}
		else
		{
			FD_SET(*sd,&wset);
			FD_SET(*sd,&eset);
			rv = select(0,NULL,&wset,&eset,&tm);
			if(rv < 0){
				closesocket(*sd);
				return -1;
			}
			if(rv == 0)
			{
				closesocket(*sd);
				return -2;//timeout
			}
			if(FD_ISSET(*sd,&eset))
			{
				closesocket(*sd);
				return -1;
			}
			if(FD_ISSET(*sd,&wset))
			{
				int err=0;

				socklen_t len=sizeof(err);
				rv = getsockopt(*sd,SOL_SOCKET,SO_ERROR,(char *)&err,&len);
				if(rv < 0 || (rv ==0 && err))
				{
					closesocket(*sd);
					return -1;
				}
			}
		}
	}
	//set_blocking(*sd);
	return 0;
}

ZNET_DECLARE(int) nc_connect(net_client_t **nc,const nc_arg_t *nc_arg)
{
	allocator_t *allocator;
	int on=1;
	int rv;
	SOCKET sd;
	HANDLE td;
	WSADATA wsaData;
#ifdef WIN32
	WSAStartup(MAKEWORD(2,2),&wsaData);
#endif
	rv = nc_nbconnect(&sd,nc_arg);
	if(rv < 0)
	    return -1;

	setsockopt(sd, IPPROTO_TCP, TCP_NODELAY,(void *)&on,(socklen_t)sizeof(on));

	if(allocator_create(&allocator) < 0)
	{
	    closesocket(sd);
	    return -1;
	}

	(*nc) = (net_client_t *)malloc(sizeof(net_client_t));
	memset((*nc),0,sizeof(net_client_t));

	thread_mutex_create(&((*nc)->mpool_mutex),0);
	allocator_mutex_set(allocator,(*nc)->mpool_mutex);

        (*nc)->allocator = allocator;

	queue_create(&((*nc)->recv_queue),C_MAX_QUEUE_CAPACITY);
//	BTPDQ_INIT(&(*nc)->recv_queue);
//	thread_mutex_create(&((*nc)->recv_mutex),0);

	thread_mutex_create(&((*nc)->peer_mutex),0);
	
	if(nc_arg->func)
		(*nc)->func = nc_arg->func;
	else
		(*nc)->func = default_process_func;

	rv = evloop_init();
	cpeer_create_out(sd,*nc);

	td = CreateThread(NULL,0,start_threads,(*nc),0,NULL);
	(*nc)->td_start = td;
	return 0;
}

ZNET_DECLARE(int) nc_disconnect(net_client_t *nc)
{
    if(!nc)
	return -1;
    fdev_del(&nc->ev);
    closesocket(nc->sd);
    nc->endgame = 1;
	WaitForSingleObject(nc->td_start,INFINITE);

    cpeer_kill(nc->peer);
    queue_destroy(nc->recv_queue);
    thread_mutex_destroy(nc->peer_mutex);
    //thread_mutex_destroy(nc->recv_mutex);
    thread_mutex_destroy(nc->mpool_mutex);
    allocator_destroy(nc->allocator);
    free(nc);

    return 0;
}

ZNET_DECLARE(int) nc_sendmsg(net_client_t *nc,void *msg,uint32_t len)
{
	int rv;
	struct cpeer *p = nc->peer;
	struct msg_t *message = NULL;

	thread_mutex_lock(nc->peer_mutex);
	if(!p || p->status == CPEER_DISCONNECTED)
	{
		thread_mutex_unlock(nc->peer_mutex);
		return -1;
	}
	InterlockedIncrement(&p->refcount);
	thread_mutex_unlock(nc->peer_mutex);

	message = (struct msg_t *)mmalloc(p->allocator,
			sizeof(struct msg_t));
	message->buf = (uint8_t *)mmalloc(p->allocator,len);
	memcpy(message->buf,(char*)msg,len);
	message->len = len;
	message->peer_id = p->id;

	thread_mutex_lock(p->sq_mutex);
	BTPDQ_INSERT_TAIL(&p->send_queue, message, msg_entry);  
	thread_mutex_unlock(p->sq_mutex);

	rv = fdev_enable(&p->ioev,EV_WRITE);
	if(rv != 0)
	{
		InterlockedDecrement(&p->refcount);
		cpeer_kill(p);
		return -1;
	}
	else
	{
		InterlockedDecrement(&p->refcount);
		return 0;
	}
}

ZNET_DECLARE(int) nc_recvmsg(net_client_t *nc,void **msg,uint32_t *len,uint32_t timeout)
{
	struct msg_t *message;
	int rv = queue_pop(nc->recv_queue,(void *)&message,timeout);
	if(rv != 0)
	    return -1;
/*
	thread_mutex_lock(nc->recv_mutex);
	message = BTPDQ_FIRST(&nc->recv_queue);
	if(message)
		BTPDQ_REMOVE(&nc->recv_queue,message,msg_entry);
	thread_mutex_unlock(nc->recv_mutex);
*/
	if(!message)
	    return -1;
	else
	{
	    *msg = message->buf;
	    *len = message->len;
	    mfree(nc->allocator,message);
	    if(message->type == MSG_DISCONNECT)
	    {
		return 1;
	    }
	    else
		return 0;
	}
}

ZNET_DECLARE(int) nc_tryrecvmsg(net_client_t *nc,void **msg,uint32_t *len)
{
	struct msg_t *message;
	int rv = queue_trypop(nc->recv_queue,(void *)&message);
	if(rv != 0)
	    return -1;
	if(!message)
	    return -1;
	else
	{
	    *msg = message->buf;
	    *len = message->len;
	    mfree(nc->allocator,message);
	    if(message->type == MSG_DISCONNECT)
	    {
		return 1;
	    }
	    else
		return 0;
	}
}

ZNET_DECLARE(int) nc_free(net_client_t *nc,void *buf)
{
	mfree(nc->allocator,buf);
	return 0;
}

