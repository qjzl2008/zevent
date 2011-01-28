#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>

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
    socklen_t len;
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
    /*assert(net_npeers <= net_max_peers);
    if (net_npeers == net_max_peers) {
        close(nsd);
        return;
    }*/
    peer_create_in(nsd,addr,ns);
}

static void *event_loop(void *arg)
{
	net_server_t *ns = (net_server_t *)arg;

	evloop(&ns->endgame);
	return NULL;
}

static void *worker_thread(void *arg)
{
	int rv;
	void *data;
	net_server_t *ns = (net_server_t *)arg;
	while(!ns->endgame)
	{
		rv = queue_pop(ns->fd_queue,&data);
		if(rv != 0)
		{
			if(rv == QUEUE_EOF)
				break;
		}
		else
		{
			struct ev_state_t *ev = (struct ev_state_t *)data;
			peer_io_process(ev);
			mfree(ns->allocator,ev);
		}
	}
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
	rv = listen(fd,100);
	set_nonblocking(fd);
	ns->fd = fd;
	fdev_new(&ns->ev,fd,EV_READ,net_connection_cb,ns);
	return 0;
}

static void* start_threads(void *arg)
{
	//启动工作线程
	net_server_t *ns = (net_server_t *)arg;
	int i = 0;
	pthread_t td;
	for(i=0;i<ns->nworkers;++i)
	{
		pthread_create(&td,NULL,worker_thread,ns);
		ns->td_workers[i] = td;
	}

	//启动事件循环
	pthread_create(&td,NULL,event_loop,ns);
	ns->td_evloop = td;

	pthread_join(ns->td_evloop,NULL);
	for(i=0;i<ns->nworkers;++i)
	{
		pthread_join(ns->td_workers[i],NULL);
	}
	return 0;
}

static int default_process_func(uint8_t *buf,uint32_t len,uint32_t *off)
{
	if(len < 10)
		return -1;
	*off = 10;
	return 0;
}

//server interface
int ns_start_daemon(net_server_t **ns,const ns_arg_t *ns_arg)
{
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
		free(*ns);
		return -1;
	}

	BTPDQ_INIT(&(*ns)->recv_queue);

	queue_create(&(*ns)->fd_queue,1000);//一些
        
	thread_mutex_create(&((*ns)->ptbl_mutex),0);
	thread_mutex_create(&((*ns)->recv_mutex),0);
	
	(*ns)->nworkers = ns_arg->nworkers;
	if(ns_arg->func)
		(*ns)->func = ns_arg->func;
	else
		(*ns)->func = default_process_func;

	evloop_init();
	start_listen(*ns,ns_arg);
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
	queue_interrupt_all(ns->fd_queue);
	pthread_join(ns->td_start,NULL);

	void *data = NULL;
	unsigned int nelts = queue_size(ns->fd_queue);
	if(nelts > 0)
	{
		int rv = queue_pop(ns->fd_queue,&data);
		while(rv == 0)
		{
			ev_state_t *ev = (ev_state_t *)data;
			mfree(ns->allocator,ev);
		}
	}

	queue_destroy(ns->fd_queue);
	struct htbl_iter it; 
	struct peer *p = ptbl_iter_first(ns->ptbl, &it);
	while (p != NULL) { 
		struct peer *ps = p;
		p = ptbl_iter_del(&it); 
		peer_kill(ps); 
	}
	ptbl_free(ns->ptbl);   

	thread_mutex_destroy(ns->ptbl_mutex);
	thread_mutex_destroy(ns->recv_mutex);
	thread_mutex_destroy(ns->mpool_mutex);

	allocator_destroy(ns->allocator);
	free(ns);
}

int ns_sendmsg(net_server_t *ns,uint32_t peer_id,void *msg,uint32_t len)
{
	struct peer *p = NULL;
	struct msg_t *message = NULL;
	int enable_write = 0;

	thread_mutex_lock(ns->ptbl_mutex);
	p = ptbl_find(ns->ptbl,&peer_id);
	thread_mutex_unlock(ns->ptbl_mutex);
	if(!p)
	{
		return -1;
	}
	message = (struct msg_t *)mmalloc(p->allocator,
			sizeof(struct msg_t));
	message->buf = (uint8_t *)mmalloc(p->allocator,len);
	bcopy((char *)msg,message->buf,len);
	message->len = len;
	message->peer_id = p->id;

	thread_mutex_lock(p->sq_mutex);

	if (p->sendbuf.off <=0 && BTPDQ_EMPTY(&p->send_queue)) {  
		enable_write = 1;
	}

	BTPDQ_INSERT_TAIL(&p->send_queue, message, msg_entry);  
	if(enable_write)
	{
		printf("enable write\n");
		fdev_enable(&p->ioev,EV_WRITE);
	}

	thread_mutex_unlock(p->sq_mutex);

	return 0;
}

int ns_recvmsg(net_server_t *ns,void **msg,uint32_t *len,uint32_t *peer_id)
{
	struct msg_t *message;
	thread_mutex_lock(ns->recv_mutex);
	message = BTPDQ_FIRST(&ns->recv_queue);
	if(message)
		BTPDQ_REMOVE(&ns->recv_queue,message,msg_entry);
	thread_mutex_unlock(ns->recv_mutex);
	if(message)
	{
		*msg = message->buf;
		*len = message->len;
		*peer_id = message->peer_id;
		mfree(ns->allocator,message);
		return 0;
	}
	else
		return -1;
}


int ns_free(net_server_t *ns,void *buf)
{
	mfree(ns->allocator,buf);
	return 0;
}

int ns_disconnect(net_server_t *ns,uint32_t id)
{

}


