#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#ifdef WIN32
#include <WinSock2.h>
#else
#include <arpa/inet.h> 
#include <string.h>
#endif
//#include <strings.h>
#include "reslist.h"
#include "redis_pool.h"
#include "hiredis.h"

static int redis_pool_construct(void **con, void *params)
{
	redis_svr_cfg *svr = (redis_svr_cfg*) params;
    redisContext *c = NULL;// = (redisContext *)malloc(sizeof(redisContext));
    struct timeval timeout = { 1, 500000 }; // 1.5 seconds
    c = redisConnectWithTimeout((char*)svr->host, svr->port, timeout);
    if (c->err) {
        redisFree(c);
        return -1;
    }
	*con = c;

	return 0;
}

static int redis_pool_destruct(void *con, void *params)
{
	redisContext *c = (redisContext *)con;
    redisFree(c);
	return 0;
}

static int redis_pool_cleanup(void *svr)
{
	redis_svr_cfg *svrCfg = (redis_svr_cfg *)svr;
	reslist_t *rl = svrCfg->connpool;

	if(rl)
		reslist_destroy(rl);

	return 0;
}

#define USEC_PER_SEC (long long)(1000000)
#define time_from_sec(sec) ((long long)(sec) * USEC_PER_SEC)
static int redis_pool_setup(redis_svr_cfg *svr)
{
	int rv;

	rv = reslist_create(&svr->connpool, svr->nmin, svr->nkeep, svr->nmax,
			time_from_sec(svr->exptime),
			redis_pool_construct, redis_pool_destruct, svr);

	if (rv == 0) {
		long long timeout = svr->timeout;
                reslist_timeout_set(svr->connpool,timeout);
	}
	else {
		rv = -1;
	}

	return rv;
}

int redis_pool_init(redis_svr_cfg *s)
{
	int rv;

	redis_svr_cfg *svr = s;
	svr->connpool = NULL;

	rv = redis_pool_setup(svr);
	if (rv == 0) {
		return rv;
	}

	rv = thread_mutex_create(&svr->mutex, THREAD_MUTEX_DEFAULT);
	if (rv != 0) {
	}
	return rv;
}

static int redis_pool_setup_lock(redis_svr_cfg *s)
{
	redis_svr_cfg *svr = s;
	int rv, rv2 = 0;

	if (!svr->mutex) {
		return -1;
	}

	rv = thread_mutex_lock(svr->mutex);
	if (rv != 0) {
		return rv;
	}

	if (!svr->connpool) {
		rv2 = redis_pool_setup(svr);
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
REDIS_DECLARE_NONSTD(void) redis_pool_release(redis_svr_cfg *s, void *con)
{
	redis_svr_cfg *svr = s;
	reslist_release(svr->connpool, con);
}
#else
REDIS_DECLARE_NONSTD(void) redis_pool_release(redis_svr_cfg *s, void *con)
{
	con = NULL;
}
#endif

#if HAS_THREADS
REDIS_DECLARE_NONSTD(void) redis_pool_remove(redis_svr_cfg *s, void *con)
{
	redis_svr_cfg *svr = s;
	reslist_invalidate(svr->connpool, con);
}
#else
REDIS_DECLARE_NONSTD(void) redis_pool_remove(redis_svr_cfg *s, int *con)
{
	con = NULL;
}
#endif

#if HAS_THREADS
REDIS_DECLARE_NONSTD(void *) redis_pool_acquire(redis_svr_cfg *s)
{
	void *rec = NULL;
	redis_svr_cfg *svr = s;
	int rv = 0;

	if (!svr->connpool) {
		if (redis_pool_setup_lock(s) != 0) {
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
REDIS_DECLARE_NONSTD(void *) redis_pool_acquire(redis_svr_cfg *s)
{
	return NULL;
}
#endif

int redis_pool_fini(redis_svr_cfg *s)
{
	redis_svr_cfg *svr = s;
	int rv;

	rv = redis_pool_cleanup(svr);
	return rv;
}

