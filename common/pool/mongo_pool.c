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

#include "mongo.h"
#include "mongo_pool.h"

static int mongo_pool_construct(void **con, void *params)
{
		mongo_svr_cfg *svr = (mongo_svr_cfg*) params;
		const char *token = ",";	
		char *state = NULL;	
		char *strret = NULL;
		char *ip = NULL;
		char *port = NULL;	
		const char *token2 = ":";	
		char *state2 = NULL;
		int nport;
		char strmongo[1024] = {0};
		strcpy(strmongo,svr->connstr);
		char *connstr = strmongo;

		int status;
	    mongo *mongo_conn = (mongo *)malloc(sizeof(mongo));
		if(svr->mongo_type == MONGO_SINGLE)
		{
				while((ip = strtok_r(connstr,token2,&state2)))	
				{
						port = strtok_r(NULL,token2,&state2);
						if(!port)
                        {
                            mongo_destroy(mongo_conn);
                            free(mongo_conn);
                            return -1;
                        }
				        nport = atoi(port);
						status = mongo_connect(mongo_conn, ip, nport );
						if( status != MONGO_OK ) 
                        {
                            mongo_destroy(mongo_conn);
                            free(mongo_conn);
                            return -1;
                        }
						connstr = NULL;
						break;
				}

		}
		else
		{
				mongo_replset_init( mongo_conn, svr->rsname );
				while((strret = strtok_r(connstr,token,&state)))	
				{		
						ip = strtok_r(strret,token2,&state2);
						if(!ip)			
                        {
                            mongo_destroy(mongo_conn);
                            free(mongo_conn);
                            return -1;		
                        }
						port = strtok_r(NULL,token2,&state2);
						if(!port)
                        {

                            mongo_destroy(mongo_conn);
                            free(mongo_conn);
                            return -1;		
                        }

						nport = atoi(port);
						mongo_replset_add_seed( mongo_conn, ip, nport );

						connstr = NULL;
				}
				status = mongo_replset_connect( mongo_conn );

                if( status != MONGO_OK ) {
                    mongo_destroy(mongo_conn);
                    free(mongo_conn);
                    return -1;
                }

		}
		*con = mongo_conn;
		return 0;
}

static int mongo_pool_destruct(void *con, void *params)
{
	mongo *mongo_conn = (mongo *)con;
	mongo_destroy(mongo_conn);
	free(mongo_conn);
	return 0;
}

static int mongo_pool_cleanup(void *svr)
{
	mongo_svr_cfg *svrCfg = (mongo_svr_cfg *)svr;
	reslist_t *rl = svrCfg->connpool;

	if(rl)
		reslist_destroy(rl);

	return 0;
}

#define USEC_PER_SEC (long long)(1000000)
#define time_from_sec(sec) ((long long)(sec) * USEC_PER_SEC)
static int mongo_pool_setup(mongo_svr_cfg *svr)
{
	int rv;

	rv = reslist_create(&svr->connpool, svr->nmin, svr->nkeep, svr->nmax,
			time_from_sec(svr->exptime),
			mongo_pool_construct, mongo_pool_destruct, svr);

	if (rv == 0) {
		long long timeout = svr->timeout;
                reslist_timeout_set(svr->connpool,timeout);
	}
	else {
		rv = -1;
	}

	return rv;
}

int mongo_pool_init(mongo_svr_cfg *s)
{
	int rv;

	mongo_svr_cfg *svr = s;
	svr->connpool = NULL;

	rv = mongo_pool_setup(svr);
	if (rv == 0) {
		return rv;
	}

	rv = thread_mutex_create(&svr->mutex, THREAD_MUTEX_DEFAULT);
	if (rv != 0) {
	}
	return rv;
}

static int mongo_pool_setup_lock(mongo_svr_cfg *s)
{
	mongo_svr_cfg *svr = s;
	int rv, rv2 = 0;

	if (!svr->mutex) {
		return -1;
	}

	rv = thread_mutex_lock(svr->mutex);
	if (rv != 0) {
		return rv;
	}

	if (!svr->connpool) {
		rv2 = mongo_pool_setup(svr);
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
MONGO_DECLARE_NONSTD(void) mongo_pool_release(mongo_svr_cfg *s, void *con)
{
	mongo_svr_cfg *svr = s;
	reslist_release(svr->connpool, con);
}
#else
MONGO_DECLARE_NONSTD(void) mongo_pool_release(mongo_svr_cfg *s, void *con)
{
	con = NULL;
}
#endif

#if HAS_THREADS
MONGO_DECLARE_NONSTD(void) mongo_pool_remove(mongo_svr_cfg *s, void *con)
{
	mongo_svr_cfg *svr = s;
	reslist_invalidate(svr->connpool, con);
}
#else
MONGO_DECLARE_NONSTD(void) mongo_pool_remove(mongo_svr_cfg *s, int *con)
{
	con = NULL;
}
#endif

#if HAS_THREADS
MONGO_DECLARE_NONSTD(void *) mongo_pool_acquire(mongo_svr_cfg *s)
{
	void *rec = NULL;
	mongo_svr_cfg *svr = s;
	int rv = 0;

	if (!svr->connpool) {
		if (mongo_pool_setup_lock(s) != 0) {
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
MONGO_DECLARE_NONSTD(void *) mongo_pool_acquire(mongo_svr_cfg *s)
{
	return NULL;
}
#endif

int mongo_pool_fini(mongo_svr_cfg *s)
{
	mongo_svr_cfg *svr = s;
	int rv;

	rv = mongo_pool_cleanup(svr);
	return rv;
}

