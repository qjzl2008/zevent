#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "queue.h"
#include "znet_peer.h"

#include "common/include/allocator.h"

#define BUF_SIZE (4096)
#define MAX_SENDBUF (1452)

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

	printf("read cb\n");
	if(p->flags & EV_READ)
	{
		printf("read herer\n");
		return;
	}
	p->flags |= EV_READ;

	rv = read(p->sd,buf,BUF_SIZE);
	while((rv > 0) || (rv < 0 && errno == EINTR))
	{
		if(rv > 0)
			iobuf_write(p->allocator,&p->recvbuf,buf,rv);
		/*if(rv == BUF_SIZE)//分段处理
			break;*/
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
	if(rv<0)
	{
		printf("rv:-1,EAGAIN\n");
	}
	

	uint32_t off = 0;
	if(p->ns->func && p->recvbuf.off > 0)
	{
		while((rv = p->ns->func(p->recvbuf.buf,p->recvbuf.off,&off)) == 0)
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
			msg->peer_id = p->id;
			
			thread_mutex_lock(p->ns->recv_mutex);
			BTPDQ_INSERT_TAIL(&p->ns->recv_queue, msg, msg_entry);  
			thread_mutex_unlock(p->ns->recv_mutex);

			iobuf_consumed(&p->recvbuf,off);
			
		}
	}
	if(need_kill)
		peer_kill(p);
	/*
	else
	{
		//EPOLLONESHOT所以需要修改
		thread_mutex_lock(p->sq_mutex);
		uint16_t flags = EV_READ;
		if(!BTPDQ_EMPTY(&p->send_queue) || p->sendbuf.off > 0)
		{
			if(!(p->flags & EV_WRITE))
				flags |= EV_WRITE;
		}
		if(flags > 0)
		{
			printf("flags:%d\n",flags);
			fdev_mod(&p->ioev,flags);
		}
		thread_mutex_unlock(p->sq_mutex);

	}*/

	p->flags &= ~EV_READ;
}

static void peer_write_cb(const ev_state_t *ev)
{
	int rv;
	struct peer *p = (struct peer *)ev->arg;

	printf("write cb\n");
	if(p->flags & EV_WRITE)
	{
		printf("write herer\n");
		return;
	}
	p->flags |= EV_WRITE;

	struct msg_t *msg = NULL,*next = NULL;

	thread_mutex_lock(p->sq_mutex);
	msg = BTPDQ_FIRST(&p->send_queue);
	//拼包
	while(msg && p->sendbuf.off < MAX_SENDBUF){
		next = BTPDQ_NEXT(msg,msg_entry);
		BTPDQ_REMOVE(&p->send_queue,msg,msg_entry);

		iobuf_write(p->allocator,&p->sendbuf,msg->buf,msg->len);
		mfree(p->allocator,msg->buf);
		mfree(p->allocator,msg);
		msg = next;
	}
	thread_mutex_unlock(p->sq_mutex);

	if(p->sendbuf.off <= 0)
	{
		printf("off<=0\n");
	        p->flags &=~EV_WRITE;
		return;
	}
	rv = write(p->sd,p->sendbuf.buf,p->sendbuf.off);
	while((rv >= 0) || (rv < 0 && errno == EINTR))
	{
		if(rv > 0)
			iobuf_consumed(&p->sendbuf,rv);
		if(p->sendbuf.off <= 0)
			break;
		rv = write(p->sd,p->sendbuf.buf,p->sendbuf.off);
	}
	if(rv < 0 && errno != EAGAIN)
	{
		peer_kill(p);
		return;
	}

	//EPOLLONESHOT所以需要修改
	//此处需谨慎啊 不可多次加入EV_READ
	thread_mutex_lock(p->sq_mutex);
	if(BTPDQ_EMPTY(&p->send_queue) && p->sendbuf.off <= 0)
	{
		printf("disable write\n");
		fdev_disable(&p->ioev,EV_WRITE);
		//flags |= EV_WRITE;
		//printf("flags:%d\n",flags);
		//fdev_mod(&p->ioev,flags);
	}
	thread_mutex_unlock(p->sq_mutex);

	p->flags &=~EV_WRITE;

	return;
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
