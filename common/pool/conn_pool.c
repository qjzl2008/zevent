#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#ifdef WIN32
#include <WinSock2.h>
#else
#include <arpa/inet.h> 
#endif
//#include <strings.h>
#include "reslist.h"
#include "conn_pool.h"

static int conn_pool_construct(void **con, void *params)
{
	conn_svr_cfg *svr = (conn_svr_cfg*) params;
#ifdef WIN32
	SOCKET *sockfd = (SOCKET *)malloc(sizeof(SOCKET));
#else
	int *sockfd = (int *)malloc(sizeof(int));
#endif
	struct sockaddr_in serv_addr;    
	*sockfd = socket(AF_INET, SOCK_STREAM, 0);    
	if (*sockfd < 0)    
		return -1;    

	memset(&serv_addr,0,sizeof(serv_addr));

	//bzero((char *) &serv_addr, sizeof(serv_addr));    
	serv_addr.sin_family = AF_INET;    
	serv_addr.sin_port = htons(svr->port);    
	serv_addr.sin_addr.s_addr = inet_addr(svr->host);    
	if (connect(*sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0)    
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
CONN_DECLARE_NONSTD(void) conn_pool_release(conn_svr_cfg *s, int *con)
{
	conn_svr_cfg *svr = s;
	reslist_release(svr->connpool, con);
}
#else
CONN_DECLARE_NONSTD(void) conn_pool_release(conn_svr_cfg *s, int *con)
{
	con = NULL;
}
#endif

#if HAS_THREADS
CONN_DECLARE_NONSTD(void) conn_pool_remove(conn_svr_cfg *s, int *con)
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
CONN_DECLARE_NONSTD(int *) conn_pool_acquire(conn_svr_cfg *s)
{
	int *rec = NULL;
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
CONN_DECLARE_NONSTD(int *) conn_pool_acquire(conn_svr_cfg *s)
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

