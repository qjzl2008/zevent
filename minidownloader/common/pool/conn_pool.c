#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#ifdef WIN32
#include <WinSock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h> 
#include <string.h>
#endif
//#include <strings.h>
#include "reslist.h"
#include "conn_pool.h"

static int set_nonblocking(SOCKET fd)
{
	u_long one = 1;
	if(ioctlsocket(fd,FIONBIO,&one) == SOCKET_ERROR) {
		return -1;
	}

	return 0;
}

static int set_blocking(SOCKET fd)
{
	u_long one = 0;
	if(ioctlsocket(fd,FIONBIO,&one) == SOCKET_ERROR) {
		return -1;
	}

	return 0;
}

static int net_connect_block(const char *ip,int port,SOCKET *sd,int tm_sec)
{
	int rv;
	struct addrinfo hints, *res;
	char portstr[6];

	struct timeval tm;
	fd_set wset,eset;
	FD_ZERO(&wset);
	FD_ZERO(&eset);
	tm.tv_sec=tm_sec;
	tm.tv_usec=0;

	*sd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if(*sd == INVALID_SOCKET)
	{
		rv = WSAGetLastError();
		return -1;
	}

	set_nonblocking(*sd);

	sprintf_s(portstr,sizeof(portstr),"%d",port);
	memset(&hints,0,sizeof(hints));
	hints.ai_family = AF_INET;
	//hints.ai_flags = AI_NUMERICHOST;
	hints.ai_socktype = SOCK_STREAM;

	if(getaddrinfo(ip,portstr,&hints,&res) != 0)
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
				rv = getsockopt(*sd,SOL_SOCKET,SO_ERROR,&err,&len);
				if(rv < 0 || (rv ==0 && err))
				{
					closesocket(*sd);
					return -1;
				}
			}
		}
	}
	set_blocking(*sd);
	return 0;
}
static int conn_pool_construct(void **con, void *params)
{
	conn_svr_cfg *svr = (conn_svr_cfg*) params;
	int rv;
    SOCKET *sockfd = (SOCKET *)malloc(sizeof(SOCKET));
	rv = net_connect_block(svr->host,svr->port,sockfd,1);
    if(rv < 0)
		return -1;
    *con = sockfd;
	return 0;
}

static int conn_pool_destruct(void *con, void *params)
{
#ifdef WIN32
	SOCKET *sockfd = (SOCKET *)con;
	closesocket(*sockfd);
#else
	int *sockfd = (int *)con;
	close(*sockfd);
#endif
	
	free(sockfd);
	sockfd = NULL;
	return 0;
}

static int conn_pool_cleanup(void *svr)
{
	conn_svr_cfg *svrCfg = svr;
	reslist_t *rl = svrCfg->connpool;

	if(rl)
		reslist_destroy(rl);

	return 0;
}

#define USEC_PER_SEC (long long)(1000000)
#define time_from_sec(sec) ((long long)(sec) * USEC_PER_SEC)
static int conn_pool_setup(conn_svr_cfg *svr)
{
	int rv;

	rv = reslist_create(&svr->connpool, svr->nmin, svr->nkeep, svr->nmax,
			time_from_sec(svr->exptime),
			conn_pool_construct, conn_pool_destruct, svr);

	if (rv == 0) {
		long long timeout = svr->timeout;
                reslist_timeout_set(svr->connpool,timeout);
	}
	else {
		rv = -1;
	}

	return rv;
}

int conn_pool_init(conn_svr_cfg *s)
{
	int rv;

	conn_svr_cfg *svr = s;
	svr->connpool = NULL;

	rv = conn_pool_setup(svr);
	if (rv == 0) {
		return rv;
	}

	rv = thread_mutex_create(&svr->mutex, THREAD_MUTEX_DEFAULT);
	if (rv != 0) {
	}
	return rv;
}

static int conn_pool_setup_lock(conn_svr_cfg *s)
{
	conn_svr_cfg *svr = s;
	int rv, rv2 = 0;

	if (!svr->mutex) {
		return -1;
	}

	rv = thread_mutex_lock(svr->mutex);
	if (rv != 0) {
		return rv;
	}

	if (!svr->connpool) {
		rv2 = conn_pool_setup(svr);
	}

	rv = thread_mutex_unlock(svr->mutex);
	if (rv != 0) {
		if (rv2 == 0) {
			rv2 = rv;
		}
	}
	return rv2;
}

#if HAS_THREADS
CONN_DECLARE_NONSTD(void) conn_pool_release(conn_svr_cfg *s, void *con)
{
	conn_svr_cfg *svr = s;
	reslist_release(svr->connpool, con);
}
#else
CONN_DECLARE_NONSTD(void) conn_pool_release(conn_svr_cfg *s, void *con)
{
	con = NULL;
}
#endif

#if HAS_THREADS
CONN_DECLARE_NONSTD(void) conn_pool_remove(conn_svr_cfg *s, void *con)
{
	conn_svr_cfg *svr = s;
	reslist_invalidate(svr->connpool, con);
}
#else
CONN_DECLARE_NONSTD(void) conn_pool_remove(conn_svr_cfg *s, int *con)
{
	con = NULL;
}
#endif

#if HAS_THREADS
CONN_DECLARE_NONSTD(void *) conn_pool_acquire(conn_svr_cfg *s)
{
	void *rec = NULL;
	conn_svr_cfg *svr = s;
	int rv = 0;

	if (!svr->connpool) {
		if (conn_pool_setup_lock(s) != 0) {
			return NULL;
		}
	}

	rv = reslist_acquire(svr->connpool, (void**)&rec);
	if (rv != 0) {
		return NULL;
	}
	return rec;
}
#else
CONN_DECLARE_NONSTD(void *) conn_pool_acquire(conn_svr_cfg *s)
{
	return NULL;
}
#endif

int conn_pool_fini(conn_svr_cfg *s)
{
	conn_svr_cfg *svr = s;
	int rv;

	rv = conn_pool_cleanup(svr);
	return rv;
}

