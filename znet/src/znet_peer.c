#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "queue.h"
#include "znet_peer.h"

#include "common/include/allocator.h"

#define BUF_SIZE (4096)

static void net_io_cb(int sd, short type, void *arg)
{	
	struct peer *p = (struct peer *)arg;
	ev_state_t *ev = (ev_state_t *)mmalloc(p->ns->allocator,
			sizeof(ev_state_t));
	ev->fd = sd;
	ev->ev_type = type;
	ev->arg = arg;
	
	queue_push(p->ns->fd_queue,ev);
}

int peer_create_in(int fd,struct sockaddr_in addr,struct net_server_t *ns)
{
	allocator_t *allocator;
	if(allocator_create(&allocator) < 0)
		return -1;

	struct peer *p = (struct peer *)mmalloc(ns->allocator,sizeof(struct peer));
	memset(p,0,sizeof(struct peer));
	p->sd = fd;
	p->id = fd;
	p->ns = ns;
	p->addr = addr;
	p->allocator = allocator;
	thread_mutex_create(&(p->mpool_mutex),0);

	allocator_mutex_set(allocator,p->mpool_mutex);

	p->sendbuf = iobuf_init(p->allocator,512);
	p->recvbuf = iobuf_init(p->allocator,512);

	thread_mutex_create(&(p->sq_mutex),0);
	BTPDQ_INIT(&p->send_queue);

	thread_mutex_lock(ns->ptbl_mutex);
	ptbl_insert(ns->ptbl,p);
	ns->npeers++;
	thread_mutex_unlock(ns->ptbl_mutex);
	
	fdev_new(&p->ioev,fd,EV_READ,net_io_cb,p);
	return 0;
}

static void peer_read_cb(const ev_state_t *ev)
{
	ssize_t rv;
	int need_kill = 0;
	char buf[BUF_SIZE];
	struct peer *p = (struct peer *)ev->arg;

	rv = read(p->sd,buf,BUF_SIZE);
	while((rv > 0) || (rv < 0 && errno == EINTR))
	{
		if(rv > 0)
			iobuf_write(p->allocator,&p->recvbuf,buf,rv);
		if(rv == BUF_SIZE)//分段处理
			break;
		rv = read(p->sd,buf,BUF_SIZE);
	}
	if(rv == 0)
	{
		if(p->recvbuf.off <=0)
		{
			peer_kill(p);
			return;
		}
		else
			need_kill = 1;
	}
	if(rv < 0 && errno != EAGAIN)
	{
		peer_kill(p);
		return;
	}

	uint32_t off = 0;
	if(p->ns->func && p->recvbuf.off > 0)
	{
		while(rv = p->ns->func(p->recvbuf.buf,p->recvbuf.off,&off) == 0)
		{
			if(off > p->recvbuf.off)
			{
				peer_kill(p);
				return;
			}
			
			struct msg_t *msg = (struct msg_t *)mmalloc(p->ns->allocator,
					sizeof(struct msg_t));
			msg->buf = (uint8_t *)mmalloc(p->ns->allocator,off);
			bcopy(p->recvbuf.buf,msg->buf,off);
			msg->len = off;

			thread_mutex_lock(p->ns->recv_mutex);
			BTPDQ_INSERT_TAIL(&p->ns->recv_queue, msg, msg_entry);  
			thread_mutex_unlock(p->ns->recv_mutex);

			iobuf_consumed(&p->recvbuf,off);
		}
	}
	//EPOLLONESHOT所以需要修改
	if(need_kill)
		peer_kill(p);
	else
	{
		fdev_mod(&p->ioev,EV_READ);
	}
}

static void peer_write_cb(const ev_state_t *ev)
{
	struct peer *p = (struct peer *)ev->arg;
	//EPOLLONESHOT所以需要修改
	fdev_mod(&p->ioev,EV_WRITE);
}

int peer_io_process(const ev_state_t *ev)
{
	switch(ev->ev_type) {
		case EV_READ:
			peer_read_cb(ev);
			break;
		case EV_WRITE:
			peer_write_cb(ev);
			break;
		default:
			return -1;
	}
	return 0;
}

int peer_kill(struct peer *p)
{
	fdev_del(&p->ioev);
	close(p->sd);
	iobuf_free(p->allocator,&p->recvbuf);
	iobuf_free(p->allocator,&p->sendbuf);
	//free send queue
	thread_mutex_destroy(p->mpool_mutex);
	thread_mutex_destroy(p->sq_mutex);
	allocator_destroy(p->allocator);

	thread_mutex_lock(p->ns->ptbl_mutex);
	ptbl_remove(p->ns->ptbl,&p->id);
	p->ns->npeers--;
	thread_mutex_unlock(p->ns->ptbl_mutex);
	
	mfree(p->ns->allocator,p);
	return 0;
}
