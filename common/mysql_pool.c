#include <stdlib.h>
#include <ctype.h>
#include "apr_reslist.h"
#include "apr_strings.h"
#include "apr_pools.h"
#include "mysql_pool.h"
#include "log.h"

static apr_status_t mysql_pool_construct(void **db, void *params, apr_pool_t *pool)
{
	svr_cfg *svr = (svr_cfg*) params;
	//MYSQL *rec = apr_pcalloc(pool, sizeof(MYSQL));
	MYSQL *rec = (MYSQL *)malloc(sizeof(MYSQL));

	mysql_init(rec);
	if (0 != mysql_options(rec,MYSQL_SET_CHARSET_NAME,svr->charset))
	{
		return -1;
	}
	if(!mysql_real_connect(rec, svr->host, svr->user, 
			svr->password, svr->db, svr->port, NULL,0))
	{
		return -1;
	}
	*db = rec;

	return APR_SUCCESS;
}

static apr_status_t mysql_pool_destruct(void *sql, void *params, apr_pool_t *pool)
{
	MYSQL *conn = sql;
	mysql_close(conn);
	free(conn);
	conn = NULL;
	return APR_SUCCESS;
}

static apr_status_t ap_memory_pool_cleanup(void *svr)
{
	svr_cfg *svrCfg = svr;
	apr_reslist_t *rl = svrCfg->dbpool;

	if(rl)
		apr_reslist_destroy(rl);

	return APR_SUCCESS;
}

static apr_status_t mysql_pool_setup(apr_pool_t *pool, svr_cfg *svr)
{
	apr_status_t rv;

	rv = apr_pool_create(&svr->pool, pool);
	if (rv != APR_SUCCESS) {
		//ap_log_perror(APLOG_MARK, APLOG_CRIT, rv, pool,
		//              "MYSQL POOL: Failed to create reslist memory pool");
		return rv;
	}

	rv = apr_reslist_create(&svr->dbpool, svr->nmin, svr->nkeep, svr->nmax,
			apr_time_from_sec(svr->exptime),
			mysql_pool_construct, mysql_pool_destruct, svr, svr->pool);

	if (rv == APR_SUCCESS) {
		apr_interval_time_t timeout = svr->timeout;
                apr_reslist_timeout_set(svr->dbpool,timeout);
		apr_pool_cleanup_register(svr->pool, svr, ap_memory_pool_cleanup,
				apr_pool_cleanup_null);				  
	}
	else {
		ap_log_error(LOG_MARK,NULL,
		             "DB: failed to initialise,will redo when acquire.");
		apr_pool_destroy(svr->pool);
		svr->pool = NULL;
	}

	return rv;
}

apr_status_t mysql_pool_init(apr_pool_t *pool, svr_cfg *s)
{
	svr_cfg *svr = s;
	apr_status_t rv;

	rv = mysql_pool_setup(pool, svr);
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

static apr_status_t mysql_pool_setup_lock(apr_pool_t *pool, svr_cfg *s)
{
	svr_cfg *svr = s;
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

	if (!svr->dbpool) {
		rv2 = mysql_pool_setup(pool, svr);
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
MYSQL_DECLARE_NONSTD(void) mysql_pool_release(svr_cfg *s, MYSQL *sql)
{
	svr_cfg *svr = s;
	apr_reslist_release(svr->dbpool, sql);
}
#else
MYSQL_DECLARE_NONSTD(void) mysql_pool_release(server_rec *s, MYSQL *sql)
{
	sql = NULL;
}
#endif

#if APR_HAS_THREADS
MYSQL_DECLARE_NONSTD(MYSQL*) mysql_pool_acquire(apr_pool_t *pool, svr_cfg *s)
{
	MYSQL *rec = NULL;
	svr_cfg *svr = s;
	apr_status_t rv = APR_SUCCESS;
	int rv2;

	if (!svr->dbpool) {
		if (mysql_pool_setup_lock(pool, s) != APR_SUCCESS) {
			return NULL;
		}
	}

	rv = apr_reslist_acquire(svr->dbpool, (void**)&rec);
	if (rv != APR_SUCCESS) {
		//ap_log_perror(APLOG_MARK, APLOG_ERR, rv, pool,
		//            "Failed to acquire DBD connection from pool!");
		return NULL;
	}

	/* Keep the connectoin alive */
	rv2 = mysql_ping(rec);
	if (rv2 != 0) {
		//ap_log_perror(APLOG_MARK, APLOG_ERR, rv, pool,
		//            "MYSQL Error: mysql_ping");
		apr_reslist_invalidate(svr->dbpool, rec);
		return NULL;
	}

	return rec;
}
#else
MYSQL_DECLARE_NONSTD(MYSQL*) mysql_pool_acquire(apr_pool_t *pool, server_rec *s)
{
	return NULL;
}
#endif

apr_status_t mysql_pool_fini(apr_pool_t *pool, svr_cfg *s)
{
	svr_cfg *svr = s;
	apr_status_t rv;

        apr_pool_cleanup_kill(svr->pool, svr, ap_memory_pool_cleanup);				  
	rv = ap_memory_pool_cleanup(svr);
	return rv;
}

int mysql_pool_query(apr_pool_t *pool,svr_cfg *s,
		const char *sql,
		apr_size_t len,
		MYSQL_RES **rs)
{
	svr_cfg *svr = s;
	MYSQL *mysql = NULL;
	mysql = mysql_pool_acquire(pool,svr);
	if(!mysql)
		return -1;
	if(mysql_real_query(mysql,sql,len)==0)
	{
		*rs = mysql_store_result(mysql); 
	}
	else
	{
		mysql_pool_release(svr,mysql);
		return -1;
	}
	mysql_pool_release(svr,mysql);
	return 0;
}

int mysql_pool_exec(apr_pool_t *pool,svr_cfg *s,
		const char *sql,
		apr_size_t len,
		unsigned long *rows)
{
	svr_cfg *svr = s;
	MYSQL *mysql = NULL;
	mysql = mysql_pool_acquire(pool,svr);
	if(!mysql)
		return -1;
	if(mysql_real_query(mysql,sql,len)==0)
	{
		if(rows)
			*rows = mysql_affected_rows(mysql);
	}
	else
	{
		mysql_pool_release(svr,mysql);
		return -1;
	}
	mysql_pool_release(svr,mysql);
	return 0;
}
