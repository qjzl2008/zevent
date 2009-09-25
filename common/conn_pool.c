#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <arpa/inet.h> 
#include "apr_reslist.h"
#include "apr_strings.h"
#include "apr_pools.h"
#include "conn_pool.h"

static apr_status_t conn_pool_construct(void **con, void *params, apr_pool_t *pool)
{
	conn_svr_cfg *svr = (conn_svr_cfg*) params;
	//int *sockfd = apr_pcalloc(pool, sizeof(int));
	int *sockfd = (int *)malloc(sizeof(int));

	struct sockaddr_in serv_addr;    
	*sockfd = socket(AF_INET, SOCK_STREAM, 0);    
	if (*sockfd < 0)    
		return -1;    
	bzero((char *) &serv_addr, sizeof(serv_addr));    
	serv_addr.sin_family = AF_INET;    
	serv_addr.sin_port = htons(svr->port);    
	serv_addr.sin_addr.s_addr = inet_addr(svr->host);    
	if (connect(*sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0)    
		return -1;  

	*con = sockfd;

	return APR_SUCCESS;
}

static apr_status_t conn_pool_destruct(void *con, void *params, apr_pool_t *pool)
{
	int *sockfd = (int *)con;
	close(*sockfd);
	free(sockfd);
	sockfd = NULL;
	return APR_SUCCESS;
}

static apr_status_t ap_memory_pool_cleanup(void *svr)
{
	conn_svr_cfg *svrCfg = svr;
	apr_reslist_t *rl = svrCfg->connpool;

	if(rl)
		apr_reslist_destroy(rl);

	return APR_SUCCESS;
}

static apr_status_t conn_pool_setup(apr_pool_t *pool, conn_svr_cfg *svr)
{
	apr_status_t rv;

	rv = apr_pool_create(&svr->pool, pool);
	if (rv != APR_SUCCESS) {
		//ap_log_perror(APLOG_MARK, APLOG_CRIT, rv, pool,
		//              "MYSQL POOL: Failed to create reslist memory pool");
		return rv;
	}

	rv = apr_reslist_create(&svr->connpool, svr->nmin, svr->nkeep, svr->nmax,
			apr_time_from_sec(svr->exptime),
			conn_pool_construct, conn_pool_destruct, svr, svr->pool);

	if (rv == APR_SUCCESS) {
		apr_interval_time_t timeout = svr->timeout;
                apr_reslist_timeout_set(svr->connpool,timeout);
		apr_pool_cleanup_register(svr->pool, svr, ap_memory_pool_cleanup,
				apr_pool_cleanup_null);				  
	}
	else {
		//ap_log_perror(APLOG_MARK, APLOG_CRIT, rv, svr->pool,
		//             "DBD: failed to initialise");
		apr_pool_destroy(svr->pool);
		svr->pool = NULL;
	}

	return rv;
}

apr_status_t conn_pool_init(apr_pool_t *pool, conn_svr_cfg *s)
{
	conn_svr_cfg *svr = s;
	apr_status_t rv;

	rv = conn_pool_setup(pool, svr);
	if (rv == APR_SUCCESS) {
		return rv;
	}

	rv = apr_thread_mutex_create(&svr->mutex, APR_THREAD_MUTEX_DEFAULT, pool);
	if (rv != APR_SUCCESS) {
		//ap_log_perror(APLOG_MARK, APLOG_CRIT, rv, pool,
		//            "DBD: Failed to create thread mutex");
	}
	return rv;
}

static apr_status_t conn_pool_setup_lock(apr_pool_t *pool, conn_svr_cfg *s)
{
	conn_svr_cfg *svr = s;
	apr_status_t rv, rv2 = APR_SUCCESS;

	if (!svr->mutex) {
		/* we already logged an error when the mutex couldn't be created */
		return APR_EGENERAL;
	}

	rv = apr_thread_mutex_lock(svr->mutex);
	if (rv != APR_SUCCESS) {
		//ap_log_perror(APLOG_MARK, APLOG_CRIT, rv, pool,
		//            "DBD: Failed to acquire thread mutex");
		return rv;
	}

	if (!svr->connpool) {
		rv2 = conn_pool_setup(pool, svr);
	}

	rv = apr_thread_mutex_unlock(svr->mutex);
	if (rv != APR_SUCCESS) {
		//ap_log_perror(APLOG_MARK, APLOG_CRIT, rv, pool,
		//            "DBD: Failed to release thread mutex");
		if (rv2 == APR_SUCCESS) {
			rv2 = rv;
		}
	}
	return rv2;
}

#if APR_HAS_THREADS
CONN_DECLARE_NONSTD(void) conn_pool_release(conn_svr_cfg *s, int *con)
{
	conn_svr_cfg *svr = s;
	apr_reslist_release(svr->connpool, con);
}
#else
CONN_DECLARE_NONSTD(void) conn_pool_release(conn_svr_cfg *s, int *con)
{
	con = NULL;
}
#endif

#if APR_HAS_THREADS
CONN_DECLARE_NONSTD(void) conn_pool_remove(conn_svr_cfg *s, int *con)
{
	conn_svr_cfg *svr = s;
	apr_reslist_invalidate(svr->connpool, con);
}
#else
CONN_DECLARE_NONSTD(void) conn_pool_remove(conn_svr_cfg *s, int *con)
{
	con = NULL;
}
#endif

#if APR_HAS_THREADS
CONN_DECLARE_NONSTD(int *) conn_pool_acquire(apr_pool_t *pool, conn_svr_cfg *s)
{
	int *rec = NULL;
	conn_svr_cfg *svr = s;
	apr_status_t rv = APR_SUCCESS;

	if (!svr->connpool) {
		if (conn_pool_setup_lock(pool, s) != APR_SUCCESS) {
			return NULL;
		}
	}

	rv = apr_reslist_acquire(svr->connpool, (void**)&rec);
	if (rv != APR_SUCCESS) {
		//ap_log_perror(APLOG_MARK, APLOG_ERR, rv, pool,
		//            "Failed to acquire DBD connection from pool!");
		return NULL;
	}
	/* Keep the connectoin alive */  
	return rec;
}
#else
CONN_DECLARE_NONSTD(int *) conn_pool_acquire(apr_pool_t *pool, conn_svr_cfg *s)
{
	return NULL;
}
#endif

apr_status_t conn_pool_fini(apr_pool_t *pool, conn_svr_cfg *s)
{
	conn_svr_cfg *svr = s;
	apr_status_t rv;

        apr_pool_cleanup_kill(svr->pool, svr, ap_memory_pool_cleanup);				  
	rv = ap_memory_pool_cleanup(svr);
	return rv;
}

