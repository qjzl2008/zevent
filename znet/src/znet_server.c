#include <unistd.h>
#include <linux/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/include/allocator.h"
#include "common/include/thread_mutex.h"
#include "hashtable.h"
#include "queue.h"

#include "util.h"
#include "znet_types.h"
#include "znet_peer.h"
#include "znet_server.h"

static void
net_connection_cb(int sd, short type, void *arg)
{
    int nsd;
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    net_server_t *ns = (net_server_t *)arg;
    nsd = accept(sd, (struct sockaddr *)&addr, (socklen_t *)&len);
    if (nsd < 0) {
        if (errno == EWOULDBLOCK || errno == ECONNABORTED)
            return;
        else
	{
		//error
		printf("Failed to accept new connection (%s).\n", strerror(errno));   
		return;
	}
    }
    if (set_nonblocking(nsd) != 0) {
        close(nsd);
        return;
    }

    int on=1;
    setsockopt(nsd, IPPROTO_TCP, TCP_NODELAY,(void *)&on,(socklen_t)sizeof(on));

    if (ns->npeers >= ns->max_peers-1) {
        close(nsd);
        return;
    }
    peer_create_in(nsd,addr,ns);
}

static void *event_loop(void *arg)
{
	net_server_t *ns = (net_server_t *)arg;

	evloop(ns->epfd,&ns->endgame);
	return NULL;
}

static int start_listen(net_server_t *ns,const ns_arg_t *ns_arg)
{
	int rv;
	int flag = 1;
	int fd = socket(AF_INET,SOCK_STREAM,0);
	if(fd < 0)
		return -1;
	struct sockaddr_in saddr;
	memset(&saddr,0,sizeof(struct sockaddr_in));
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(ns_arg->port);
	saddr.sin_addr.s_addr = inet_addr(ns_arg->ip);
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));   
        rv = bind(fd,(struct sockaddr *)&saddr,sizeof(struct sockaddr_in));
        if(rv < 0)
	{
		return -1;
	}
	rv = listen(fd,500);
	set_nonblocking(fd);
	ns->fd = fd;
	fdev_new(ns->epfd,&ns->ev,fd,EV_READ,net_connection_cb,ns);
	return 0;
}

static void* start_threads(void *arg)
{
	//启动工作线程
	net_server_t *ns = (net_server_t *)arg;
	pthread_t td;

	//启动事件循环
	pthread_create(&td,NULL,event_loop,ns);
	ns->td_evloop = td;

	pthread_join(ns->td_evloop,NULL);
	return 0;
}

#define HEADER_LEN (4)
static int default_process_func(uint8_t *buf,uint32_t len,uint32_t *off)
{
        if(len < HEADER_LEN)
	    return -1;
	uint32_t msg_len = dec_be32(buf);

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
int ns_start_daemon(net_server_t **ns,const ns_arg_t *ns_arg)
{
	//忽略SIGPIPE 信号 
	struct sigaction sa;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;        
	sa.sa_handler = SIG_IGN;
	sigaction(SIGPIPE,&sa,NULL); 

	allocator_t *allocator;
	if(allocator_create(&allocator) < 0)
		return -1;

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
	if(ns_arg->data_func)
		(*ns)->data_func = ns_arg->data_func;
	else
		(*ns)->data_func = default_process_func;

	if(ns_arg->msg_func)
		(*ns)->msg_func = ns_arg->msg_func;

	int epfd = evloop_init();
	(*ns)->epfd = epfd;
	int rv = start_listen(*ns,ns_arg);
	if(rv < 0)
	{
	    queue_destroy((*ns)->recv_queue);
	    thread_mutex_destroy((*ns)->ptbl_mutex);
	    thread_mutex_destroy((*ns)->mpool_mutex);
	    allocator_destroy((*ns)->allocator);

	    free(*ns);
	    return -1;
	}
	//pthread_attr_t attr;
	pthread_t td;
	//pthread_attr_init(&attr);
	//pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&td,NULL,&start_threads,(*ns));
	//pthread_attr_destroy(&attr);
	(*ns)->td_start = td;

	return 0;
}

int ns_stop_daemon(net_server_t *ns)
{
	fdev_del(&ns->ev);
	close(ns->fd);
	ns->endgame = 1;
	pthread_join(ns->td_start,NULL);

	struct htbl_iter it; 
	struct peer *p = ptbl_iter_first(ns->ptbl, &it);
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

int ns_getpeeraddr(net_server_t *ns,uint64_t peer_id,char *ip)
{
    struct peer *p = NULL;

    if(!ip)
	return -1;
    thread_mutex_lock(ns->ptbl_mutex);
    p = ptbl_find(ns->ptbl,&peer_id);

    if(!p || p->status == PEER_DISCONNECTED)
    {
	thread_mutex_unlock(ns->ptbl_mutex);
	return -1;
    }

    __sync_fetch_and_add(&p->refcount,1);
    thread_mutex_unlock(ns->ptbl_mutex);
    char *peer_ip = inet_ntoa(p->addr.sin_addr);

    __sync_fetch_and_sub(&p->refcount,1);
    strcpy(ip,peer_ip);
    return 0;
}
int ns_sendmsg(net_server_t *ns,uint64_t peer_id,void *msg,uint32_t len)
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

        __sync_fetch_and_add(&p->refcount,1);
	thread_mutex_unlock(ns->ptbl_mutex);

	message = (struct msg_t *)mmalloc(p->allocator,
			sizeof(struct msg_t));
	message->buf = (uint8_t *)mmalloc(p->allocator,len);
	bcopy((char *)msg,message->buf,len);
	message->len = len;
	message->peer_id = p->id;

	thread_mutex_lock(p->sq_mutex);
	BTPDQ_INSERT_TAIL(&p->send_queue, message, msg_entry);  
	thread_mutex_unlock(p->sq_mutex);

	rv = fdev_enable(&p->ioev,EV_WRITE);
	if(rv != 0)
	{
		__sync_fetch_and_sub(&p->refcount,1);
		peer_kill(p);
		return -1;
	}
	else
	{
		__sync_fetch_and_sub(&p->refcount,1);
		return 0;
	}
}

int ns_recvmsg(net_server_t *ns,void **msg,uint32_t *len,uint64_t *peer_id,
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

int ns_tryrecvmsg(net_server_t *ns,void **msg,uint32_t *len,uint64_t *peer_id)
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


int ns_free(net_server_t *ns,void *buf)
{
	mfree(ns->allocator,buf);
	return 0;
}

int ns_disconnect(net_server_t *ns,uint64_t id)
{
	thread_mutex_lock(ns->ptbl_mutex);
	struct peer *p = ptbl_find(ns->ptbl,&id);
	if(!p || p->status == PEER_DISCONNECTED)
	{
		thread_mutex_unlock(ns->ptbl_mutex);
		return -1;
	}

	shutdown(p->sd,SHUT_RDWR);
	thread_mutex_unlock(ns->ptbl_mutex);
	return 0;
}


