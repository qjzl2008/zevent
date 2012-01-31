#include <time.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "queue.h"
#include "znet_peer.h"

#include "allocator.h"

#define BUF_SIZE (4096)
#define MAX_SENDBUF (1452)

static void net_io_cb(int sd, short type, void *arg);

static int gen_peerid(struct net_server_t *ns,uint64_t *peer_id)
{
	static time_t tm_last;
	static uint32_t fudge;
	time_t tm_now = time(NULL);
	if(tm_last != tm_now)
	{
		fudge = 0;
		tm_last = tm_now;
	}
	else
	{
		if(fudge >= UINT_MAX - 1)
			return -1;
		++fudge;
	}
	*peer_id = ((uint64_t)tm_now)<<32 | fudge;
	return 0;
}

int peer_create_in(SOCKET fd,struct sockaddr_in addr,struct net_server_t *ns)
{
	struct peer *p = NULL;
	allocator_t *allocator;
	if(allocator_create(&allocator) < 0)
		return -1;

	p = (struct peer *)mmalloc(ns->allocator,sizeof(struct peer));
	memset(p,0,sizeof(struct peer));
	p->sd = fd;
	gen_peerid(ns,&p->id);
	p->refcount = 0;
	p->status = PEER_CONNECTED;
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

static int peer_read_cb(struct peer *p)
{
	ssize_t rv;
	int need_kill = 0;
	char buf[BUF_SIZE];
    struct msg_t *msg;
	uint32_t off = 0,size;

	rv = recv(p->sd,buf,BUF_SIZE,0);
	while((rv > 0))
	{
		if(rv > 0)
			iobuf_write(p->allocator,&p->recvbuf,buf,rv);
		if(rv == BUF_SIZE)
		{
			break;
		}
		rv = recv(p->sd,buf,BUF_SIZE,0);
	}
	if(rv == 0)
	{
		if(p->recvbuf.off <=0)
		{
			return -1;
		}
		else
			need_kill = 1;
	}
	if(rv == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
	{
		return -1;
	}

	if(p->ns->func && p->recvbuf.off > 0)
	{
		while((rv = p->ns->func(p->recvbuf.buf,(uint32_t)(p->recvbuf.off),&off)) == 0)
		{
			if(off > p->recvbuf.off)
			{
				return -1;
			}

			msg = (struct msg_t *)mmalloc(p->ns->allocator,
					sizeof(struct msg_t));
			size = off + 1;
			msg->buf = (uint8_t *)mmalloc(p->ns->allocator,size);
			memset(msg->buf,0,size);
			memcpy(msg->buf,p->recvbuf.buf,off);
			msg->len = off;
			msg->peer_id = p->id;
			msg->type = MSG_DATA;
			
			queue_push(p->ns->recv_queue,msg);
		/*	thread_mutex_lock(p->ns->recv_mutex);
			BTPDQ_INSERT_TAIL(&p->ns->recv_queue, msg, msg_entry);  
			thread_mutex_unlock(p->ns->recv_mutex);*/

			iobuf_consumed(&p->recvbuf,off);
			
		}
	}
	if(need_kill)
	{
		return -1;
	}
	return 0;
}

static int peer_write_cb(struct peer *p)
{
	int rv;

	struct msg_t *msg = NULL,*next = NULL;

	thread_mutex_lock(p->sq_mutex);
	msg = BTPDQ_FIRST(&p->send_queue);
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
		fdev_disable(&p->ioev,EV_WRITE);
		return 0;
	}
	rv = send(p->sd,p->sendbuf.buf,(int)(p->sendbuf.off),0);
	while((rv >= 0))
	{
		if(rv > 0)
			iobuf_consumed(&p->sendbuf,rv);
		if(p->sendbuf.off <= 0)
			break;
		rv = send(p->sd,p->sendbuf.buf,(int)(p->sendbuf.off),0);
	}
	if(rv == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
	{
		return -1;
	}

	thread_mutex_lock(p->sq_mutex);
	if(BTPDQ_EMPTY(&p->send_queue) && p->sendbuf.off <= 0)
	{
		fdev_disable(&p->ioev,EV_WRITE);
	}
	thread_mutex_unlock(p->sq_mutex);
	return 0;
}

static void net_io_cb(int sd, short type, void *arg)
{	
	int rv = 0;
	struct peer *p = (struct peer *)arg;

	switch(type) {
		case EV_READ:
			rv = peer_read_cb(p);
			break;
		case EV_WRITE:
			rv = peer_write_cb(p);
			break;
		default:
			break;
	}
	if(rv < 0)
	{
		peer_kill(p);
	}
}


int peer_kill(struct peer *p)
{
	struct msg_t *msg,*next;
    if(!p)
	    return 0;
	if(InterlockedCompareExchange((volatile LONG *)(&p->status),PEER_CONNECTED,PEER_DISCONNECTED))
	{
		printf("ref:%d,close peer id:%llu,peer:%p,ns:%p\n",p->refcount,p->id,p,p->ns);
		closesocket(p->sd);
		fdev_del(&p->ioev);
		//push one msg to notify disconnect
		msg = (struct msg_t *)mmalloc(p->ns->allocator,
			sizeof(struct msg_t));
		msg->buf = NULL;
		msg->len = 0;
		msg->peer_id = p->id;
		msg->type = MSG_DISCONNECT;

		queue_push(p->ns->recv_queue,msg);
	}

	if(!InterlockedCompareExchange(&p->refcount,0,1))
	{
		printf("free peer id:%llu\n",p->id);
		iobuf_free(p->allocator,&p->recvbuf);
		iobuf_free(p->allocator,&p->sendbuf);
		//free send queue
		thread_mutex_lock(p->sq_mutex);
		msg = BTPDQ_FIRST(&p->send_queue);
		while(msg){
			next = BTPDQ_NEXT(msg,msg_entry);
			BTPDQ_REMOVE(&p->send_queue,msg,msg_entry);

			mfree(p->allocator,msg->buf);
			mfree(p->allocator,msg);
			msg = next;
		}
		thread_mutex_unlock(p->sq_mutex);

		thread_mutex_destroy(p->mpool_mutex);
		thread_mutex_destroy(p->sq_mutex);

		allocator_destroy(p->allocator);

		mfree(p->ns->allocator,p);

		thread_mutex_lock(p->ns->ptbl_mutex);
		ptbl_remove(p->ns->ptbl,&p->id);
		p->ns->npeers--;
		thread_mutex_unlock(p->ns->ptbl_mutex);
	}
	return 0;
}
