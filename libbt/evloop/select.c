#include <errno.h>
#include <string.h>

#include "evloop.h"
#include "hash.h"

extern volatile int daemon_stop;

static fd_set readset,writeset,exceptset;

hash_t *hfdev;

int evloop_init(void)
{
    if(timeheap_init() != 0)
	return -1;

    FD_ZERO(&readset);
    FD_ZERO(&writeset);
    FD_ZERO(&exceptset);
    hfdev = hash_make();
    return 0;
}

int
fdev_new(struct fdev *ev, SOCKET fd, uint16_t flags,evloop_cb_t cb,void *arg)
{
    struct fdev *newfd;
    int i = 0;
    void *key = NULL;
    ev->fd = fd;
    ev->cb = cb;
    ev->arg = arg;
    ev->flags = 0;

    if(!hfdev)
	return -1;

    newfd = malloc(sizeof(struct fdev));
    newfd->fd = fd;
    newfd->cb = cb;
    newfd->arg = arg;
    newfd->flags = 0;

    key = malloc(sizeof(fd));
    memcpy(key,&fd,sizeof(fd));
    hash_set(hfdev,key,sizeof(fd),newfd);
    return fdev_enable(ev,flags);
}

int 
fdev_enable(struct fdev *ev, uint16_t flags)
{
    struct fdev *hfd = NULL;
    int err = 0;
    uint16_t sf = ev->flags;
	void *entry_key = NULL;

    if(!hfdev)
	return -1;
    ev->flags |= flags;
    if(sf != ev->flags) 
    {
	if(ev->flags & EV_READ)
	{
	    FD_SET(ev->fd,&readset);
	}
	if(ev->flags & EV_WRITE)
	{
	    FD_SET(ev->fd,&writeset);
	}
    }

    hfd = hash_get(hfdev,&ev->fd,sizeof(ev->fd),&entry_key);
    if(!hfd)
	return -1;
    hfd->flags = ev->flags;
    hash_set(hfdev,&ev->fd,sizeof(ev->fd),hfd);
    return err;
}

int
fdev_disable(struct fdev *ev, uint16_t flags)
{
    struct fdev *hfd = NULL;
	void *entry_key = NULL;
    int err = 0;
    uint16_t sf = ev->flags;
    ev->flags &= ~flags;
    if(!hfdev)
	return -1;

    if(flags & EV_READ)
    {
	FD_CLR(ev->fd,&readset);
    }

    if(flags & EV_WRITE)
    {
	FD_CLR(ev->fd,&writeset);
    }
    hfd = hash_get(hfdev,&ev->fd,sizeof(ev->fd),&entry_key);
    if(!hfd)
    {
	return -1;
    }
    hfd->flags = ev->flags;
    hash_set(hfdev,&ev->fd,sizeof(ev->fd),hfd);

    return 0;
}

int fdev_del(struct fdev *ev)
{
	void *entry_key,*val;
    FD_CLR(ev->fd,&readset);
    FD_CLR(ev->fd,&writeset);
    if(hfdev)
	{
		val = hash_get(hfdev,&ev->fd,sizeof(ev->fd),&entry_key); 
		if(val)
		{
			hash_set(hfdev,&ev->fd,sizeof(ev->fd),NULL);
			free(entry_key);
			free(val);
		}

	}
    return 0;
}

int evloop()
{
    int nev,i;
    int err;
    struct timespec delay;
    struct timeval timeout;
    struct timeval *tvptr = NULL;
    fd_set tmp_readset,tmp_writeset,tmp_exceptset;
    struct fdev *ev = NULL;
	void *entry_key = NULL;
	void *key,*val;
	hash_index_t *hi;

    while(!daemon_stop) {
	evtimers_run();
	delay = evtimer_delay();

	if(delay.tv_sec >= 0)
	{
	    timeout.tv_sec = delay.tv_sec;
	    timeout.tv_usec = delay.tv_nsec/1000;
	    tvptr = &timeout;
	}

	tmp_readset = readset;
	tmp_writeset = writeset;
	tmp_exceptset = exceptset;

	if((nev = select(0,&tmp_readset,&tmp_writeset,&tmp_exceptset,tvptr)) < 0) {
	    err = WSAGetLastError();
	    break;
	}

	for(i = 0; i < tmp_readset.fd_count; ++i)
	{
	    ev = hash_get(hfdev,&tmp_readset.fd_array[i],
		    sizeof(tmp_readset.fd_array[i]),&entry_key);
	    if(ev && (ev->flags & EV_READ))
	    {
		ev->cb(ev->fd,EV_READ,ev->arg);
	    }
	}

	for(i = 0; i < tmp_writeset.fd_count; ++i)
	{
	    ev = hash_get(hfdev, &tmp_writeset.fd_array[i],
		    sizeof(tmp_writeset.fd_array[i]),&entry_key);
	    if(ev && (ev->flags & EV_WRITE))
	    {
		ev->cb(ev->fd,EV_WRITE,ev->arg);
	    }
	}

	for(i = 0; i < tmp_exceptset.fd_count; ++i)
	{
	    closesocket(tmp_exceptset.fd_array[i]);
	}

    }
	for (hi = hash_first(hfdev); hi ; hi = hash_next(hi))
	{
		hash_this(hi,(const void **)&key,NULL,(void **)&val); 
		free(key);
		free(val);
	}
    hash_destroy(hfdev);
    hfdev = NULL;
	timeheap_destroy();
    return 0;
}
