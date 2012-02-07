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
#include "znet_peer.h"
#include "znet_server.h"

static void
net_connection_cb(SOCKET sd, short type, void *arg)
{
    SOCKET nsd;
	int on=1;
	DWORD wsaerr;
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    net_server_t *ns = (net_server_t *)arg;
    nsd = accept(sd, (struct sockaddr *)&addr, (socklen_t *)&len);
	if (nsd < 0) 
	{
		wsaerr = WSAGetLastError();
		if (wsaerr == WSAEWOULDBLOCK || wsaerr == WSAECONNABORTED)
			return;
		else
		{
			//error
			//printf("Failed to accept new connection (%s).\n", strerror(errno));   
			return;
		}
	}
    if (set_nonblocking(nsd) != 0) {
        closesocket(nsd);
        return;
    }

    setsockopt(nsd, IPPROTO_TCP, TCP_NODELAY,(void *)&on,(socklen_t)sizeof(on));

    if (ns->npeers >= ns->max_peers-1) {
        closesocket(nsd);
        return;
    }
    peer_create_in(nsd,addr,ns);
}

static DWORD WINAPI event_loop(void *arg)
{
	net_server_t *ns = (net_server_t *)arg;

	evloop(&ns->endgame);
	return 0;
}

static int start_listen(net_server_t *ns,ns_arg_t *ns_arg)
{
	int rv;
	int flag = 1;
	struct sockaddr_in saddr;
	int len = sizeof(saddr);
	SOCKET fd = socket(AF_INET,SOCK_STREAM,0);
	if(fd == SOCKET_ERROR)
		return -1;
	memset(&saddr,0,sizeof(struct sockaddr_in));
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(ns_arg->port);
	saddr.sin_addr.s_addr = inet_addr(ns_arg->ip);
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *)(&flag), sizeof(flag));   
    rv = bind(fd,(struct sockaddr *)&saddr,sizeof(struct sockaddr_in));
    if(rv < 0)
	{
		closesocket(fd);
		return -1;
	}
	if(ns_arg->port == 0)
	{
		if (getsockname(fd, (struct sockaddr*)&saddr, &len) == -1){
			closesocket(fd);
			return -1;
		}
		ns_arg->port = ntohs(saddr.sin_port);
	}
	rv = listen(fd,500);
	set_nonblocking(fd);
	ns->fd = fd;
	fdev_new(&ns->ev,fd,EV_READ,net_connection_cb,ns);
	return 0;
}

static DWORD WINAPI start_threads(void *arg)
{
	net_server_t *ns = (net_server_t *)arg;
	HANDLE td;

	td = CreateThread(NULL,0,event_loop,ns,0,NULL);
	ns->td_evloop = td;

	WaitForSingleObject(td,INFINITE);
	CloseHandle(td);
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

//server interface
ZNET_DECLARE(int) ns_start_daemon(net_server_t **ns,ns_arg_t *ns_arg)
{
	int rv;
	HANDLE td;
	WSADATA wsaData;
	allocator_t *allocator;
	if(allocator_create(&allocator) < 0)
		return -1;

#ifdef WIN32
	WSAStartup(MAKEWORD(2,2),&wsaData);
#endif

	(*ns) = (net_server_t *)malloc(sizeof(net_server_t));
	memset((*ns),0,sizeof(net_server_t));

	thread_mutex_create(&((*ns)->mpool_mutex),0);
	allocator_mutex_set(allocator,(*ns)->mpool_mutex);

        (*ns)->allocator = allocator;

	if (((*ns)->ptbl = ptbl_create(3, id_eq, id_hash)) == NULL)
	{
	    thread_mutex_destroy((*ns)->mpool_mutex);
	    allocator_destroy((*ns)->allocator);

	    free(*ns);
	    return -1;
	}

	//BTPDQ_INIT(&(*ns)->recv_queue);
	//thread_mutex_create(&((*ns)->recv_mutex),0);
	
	queue_create(&((*ns)->recv_queue),MAX_QUEUE_CAPACITY);
	thread_mutex_create(&((*ns)->ptbl_mutex),0);
	
	(*ns)->max_peers = ns_arg->max_peers;
	if(ns_arg->func)
		(*ns)->func = ns_arg->func;
	else
		(*ns)->func = default_process_func;

	evloop_init();
	rv = start_listen(*ns,ns_arg);
	if(rv < 0)
	{
	    queue_destroy((*ns)->recv_queue);
	    thread_mutex_destroy((*ns)->ptbl_mutex);
	    thread_mutex_destroy((*ns)->mpool_mutex);
	    allocator_destroy((*ns)->allocator);

	    free(*ns);
	    return -1;
	}

	td = CreateThread(NULL,0,start_threads,(*ns),0,NULL);
	(*ns)->td_start = td;

	return 0;
}

ZNET_DECLARE(int) ns_stop_daemon(net_server_t *ns)
{
	struct htbl_iter it; 
	struct peer *p;

	fdev_del(&ns->ev);
	closesocket(ns->fd);
	ns->endgame = 1;
	WaitForSingleObject(ns->td_start,INFINITE);
	CloseHandle(ns->td_start);

	p = ptbl_iter_first(ns->ptbl, &it);
	while (p != NULL) { 
		struct peer *ps = p;
		p = ptbl_iter_del(&it); 
		peer_kill(ps); 
	}
	ptbl_free(ns->ptbl);   

	queue_destroy(ns->recv_queue);
	thread_mutex_destroy(ns->ptbl_mutex);
	//thread_mutex_destroy(ns->recv_mutex);
	thread_mutex_destroy(ns->mpool_mutex);

	allocator_destroy(ns->allocator);
	free(ns);
	return 0;
}

ZNET_DECLARE(int) ns_getpeeraddr(net_server_t *ns,uint64_t peer_id,char *ip)
{
    struct peer *p = NULL;
	char *peer_ip;

    if(!ip)
	return -1;
    thread_mutex_lock(ns->ptbl_mutex);
    p = ptbl_find(ns->ptbl,&peer_id);

    if(!p || p->status == PEER_DISCONNECTED)
    {
	thread_mutex_unlock(ns->ptbl_mutex);
	return -1;
    }

    InterlockedIncrement(&p->refcount);
    thread_mutex_unlock(ns->ptbl_mutex);
    peer_ip = inet_ntoa(p->addr.sin_addr);

    InterlockedDecrement(&p->refcount);
    strcpy(ip,peer_ip);
    return 0;
}

ZNET_DECLARE(int) ns_broadcastmsg(net_server_t *ns,void *msg,uint32_t len)
{
	int rv;
	struct htbl_iter it;
	struct peer *p = NULL;
	struct msg_t *message = NULL;

	thread_mutex_lock(ns->ptbl_mutex);
	for (p = ptbl_iter_first(ns->ptbl,&it); p != NULL; p = ptbl_iter_next(&it))
	{
		if(!p || p->status == PEER_DISCONNECTED)
		{
			continue;
		}
		InterlockedIncrement(&p->refcount);

		message = (struct msg_t *)mmalloc(p->allocator,
			sizeof(struct msg_t));
		message->buf = (uint8_t *)mmalloc(p->allocator,len);
		memcpy(message->buf,(char *)msg,len);
		message->len = len;
		message->peer_id = p->id;

		thread_mutex_lock(p->sq_mutex);
		BTPDQ_INSERT_TAIL(&p->send_queue, message, msg_entry);  
		thread_mutex_unlock(p->sq_mutex);

		rv = fdev_enable(&p->ioev,EV_WRITE);
		if(rv<0)
		{
			InterlockedDecrement(&p->refcount);
			thread_mutex_unlock(ns->ptbl_mutex);
			peer_kill(p);
			thread_mutex_lock(ns->ptbl_mutex);
			continue;
		}
		InterlockedDecrement(&p->refcount);
		continue;
	}
	thread_mutex_unlock(ns->ptbl_mutex);
	return 0;
}

ZNET_DECLARE(int) ns_sendmsg(net_server_t *ns,uint64_t peer_id,void *msg,uint32_t len)
{
	int rv;
	struct peer *p = NULL;
	struct msg_t *message = NULL;

	thread_mutex_lock(ns->ptbl_mutex);
	p = ptbl_find(ns->ptbl,&peer_id);
	
	if(!p || p->status == PEER_DISCONNECTED)
	{
		thread_mutex_unlock(ns->ptbl_mutex);
		return -1;
	}

    InterlockedIncrement(&p->refcount);
	thread_mutex_unlock(ns->ptbl_mutex);

	message = (struct msg_t *)mmalloc(p->allocator,
			sizeof(struct msg_t));
	message->buf = (uint8_t *)mmalloc(p->allocator,len);
	memcpy(message->buf,(char *)msg,len);
	message->len = len;
	message->peer_id = p->id;

	thread_mutex_lock(p->sq_mutex);
	BTPDQ_INSERT_TAIL(&p->send_queue, message, msg_entry);  
	thread_mutex_unlock(p->sq_mutex);

	rv = fdev_enable(&p->ioev,EV_WRITE);
	if(rv != 0)
	{
		InterlockedDecrement(&p->refcount);
		peer_kill(p);
		return -1;
	}
	else
	{
		InterlockedDecrement(&p->refcount);
		return 0;
	}
}

ZNET_DECLARE(int) ns_recvmsg(net_server_t *ns,void **msg,uint32_t *len,uint64_t *peer_id,
	uint32_t timeout)
{
	struct msg_t *message;
	int rv = queue_pop(ns->recv_queue,(void *)&message,timeout);
	if(rv != 0)
	    return -1;
/*	thread_mutex_lock(ns->recv_mutex);
	message = BTPDQ_FIRST(&ns->recv_queue);
	if(message)
		BTPDQ_REMOVE(&ns->recv_queue,message,msg_entry);
	thread_mutex_unlock(ns->recv_mutex);*/
	if(!message)
	    return -1;
	else
	{
	    *msg = message->buf;
	    *len = message->len;
	    *peer_id = message->peer_id;
	    mfree(ns->allocator,message);
	    if(message->type == MSG_DISCONNECT)
	    {
		return 1;
	    }
	    else
		return 0;
	}
}

ZNET_DECLARE(int) ns_tryrecvmsg(net_server_t *ns,void **msg,uint32_t *len,uint64_t *peer_id)
{
	struct msg_t *message;
	int rv = queue_trypop(ns->recv_queue,(void *)&message);
	if(rv != 0)
	    return -1;

	if(!message)
	    return -1;
	else
	{
	    *msg = message->buf;
	    *len = message->len;
	    *peer_id = message->peer_id;
	    mfree(ns->allocator,message);
	    if(message->type == MSG_DISCONNECT)
	    {
		return 1;
	    }
	    else
		return 0;
	}
}


ZNET_DECLARE(int) ns_free(net_server_t *ns,void *buf)
{
	mfree(ns->allocator,buf);
	return 0;
}

ZNET_DECLARE(int) ns_disconnect(net_server_t *ns,uint64_t id)
{
	struct peer *p;
	thread_mutex_lock(ns->ptbl_mutex);
	p = ptbl_find(ns->ptbl,&id);
	if(!p || p->status == PEER_DISCONNECTED)
	{
		thread_mutex_unlock(ns->ptbl_mutex);
		return -1;
	}

	shutdown(p->sd,SD_BOTH);
	thread_mutex_unlock(ns->ptbl_mutex);
	return 0;
}


