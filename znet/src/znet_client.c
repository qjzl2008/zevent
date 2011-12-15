#include <unistd.h>
#include <linux/tcp.h>
#include <fcntl.h>
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
#include "znet_client.h"

static void *event_loop(void *arg)
{
	net_client_t *nc = (net_client_t *)arg;

	evloop(nc->epfd,&nc->endgame);
	return NULL;
}

static void* start_threads(void *arg)
{
	//启动工作线程
	net_client_t *nc = (net_client_t *)arg;
	pthread_t td;

	//启动事件循环
	pthread_create(&td,NULL,event_loop,nc);
	nc->td_evloop = td;

	pthread_join(nc->td_evloop,NULL);
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


static int nc_nbconnect(int *fd, const nc_arg_t *nc_arg)
{
    struct timeval tm;
    fd_set wset,eset;
    FD_ZERO(&wset);
    FD_ZERO(&eset);
    tm.tv_sec=nc_arg->timeout;
    tm.tv_usec=0;

    struct sockaddr_in addr;
    int addrsize=sizeof(struct sockaddr);
    bzero((struct sockaddr*)&addr,addrsize);
    addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=inet_addr(nc_arg->ip);
    addr.sin_port=htons((short)nc_arg->port);

    int sd;
    if((sd=socket(AF_INET,SOCK_STREAM,0))==-1)
    {
	return -1;
    }
    int flags=fcntl(sd,F_GETFL,0);
    fcntl(sd,F_SETFL,flags|O_NONBLOCK);

    int rv=connect(sd,(struct sockaddr *)&addr,addrsize);
    if(rv < 0)
    {
	if(errno != EINPROGRESS)
	{
	    close(sd);
	    return -1;
	}
	else
	{
	    FD_SET(sd,&wset);
	    FD_SET(sd,&eset);
	    rv = select(sd+1,NULL,&wset,&eset,&tm);
	    if(rv < 0){
		close(sd);
		return -1;
	    }
	    if(rv == 0)
	    {
		close(sd);
		return -2;//timeout
	    }
	    if(FD_ISSET(sd,&eset))
	    {
		close(sd);
		return -1;
	    }
	    if(FD_ISSET(sd,&wset))
	    {
		int err=0;

		socklen_t len=sizeof(err);
		rv = getsockopt(sd,SOL_SOCKET,SO_ERROR,&err,&len);
		if(rv < 0 || (rv ==0 && err))
		{
		    close(sd);
		    return -1;
		}
		*fd = sd;
		return 0;
	    }
	}
    }
    else
    {
	*fd = sd;
	return 0;
    }
    return 0;
}

int nc_connect(net_client_t **nc,const nc_arg_t *nc_arg)
{
	//忽略SIGPIPE 信号 
	struct sigaction sa;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;        
	sa.sa_handler = SIG_IGN;
	sigaction(SIGPIPE,&sa,NULL); 

	int rv,sd;
	rv = nc_nbconnect(&sd,nc_arg);
	if(rv < 0)
	    return -1;

	int on=1;
	setsockopt(sd, IPPROTO_TCP, TCP_NODELAY,(void *)&on,(socklen_t)sizeof(on));

	allocator_t *allocator;
	if(allocator_create(&allocator) < 0)
	{
	    close(sd);
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

	int epfd = evloop_init();
	(*nc)->epfd = epfd;
	cpeer_create_out(sd,*nc);

	pthread_t td;
	pthread_create(&td,NULL,&start_threads,(*nc));
	(*nc)->td_start = td;
	return 0;
}

int nc_disconnect(net_client_t *nc)
{
    if(!nc)
	return -1;
    fdev_del(&nc->ev);
    close(nc->sd);
    nc->endgame = 1;
    pthread_join(nc->td_start,NULL);

    cpeer_kill(nc->peer);
    queue_destroy(nc->recv_queue);
    thread_mutex_destroy(nc->peer_mutex);
    //thread_mutex_destroy(nc->recv_mutex);
    thread_mutex_destroy(nc->mpool_mutex);
    allocator_destroy(nc->allocator);
    free(nc);

    /*
       thread_mutex_lock(nc->peer_mutex);
       struct cpeer *p = nc->peer;
       if(!p || p->status == CPEER_DISCONNECTED)
       {
       thread_mutex_unlock(nc->peer_mutex);
       return -1;
       }
       shutdown(p->sd,SHUT_RDWR);
       thread_mutex_unlock(nc->peer_mutex);
       */
    return 0;
}

int nc_sendmsg(net_client_t *nc,void *msg,uint32_t len)
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
        __sync_fetch_and_add(&p->refcount,1);
	thread_mutex_unlock(nc->peer_mutex);

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
		cpeer_kill(p);
		return -1;
	}
	else
	{
		__sync_fetch_and_sub(&p->refcount,1);
		return 0;
	}
}

int nc_recvmsg(net_client_t *nc,void **msg,uint32_t *len,uint32_t timeout)
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

int nc_tryrecvmsg(net_client_t *nc,void **msg,uint32_t *len)
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

int nc_free(net_client_t *nc,void *buf)
{
	mfree(nc->allocator,buf);
	return 0;
}

