#include <stdlib.h>
#include <ctype.h>
#include "reslist.h"
#include "mysql_pool.h"

static int mysql_pool_construct(void **db, void *params)
{
	svr_cfg *svr = (svr_cfg*) params;
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

	return 0;
}

static int mysql_pool_destruct(void *sql, void *params)
{
	MYSQL *conn = sql;
	mysql_close(conn);
	free(conn);
	conn = NULL;
	return 0;
}

static int mysql_pool_cleanup(void *svr)
{
	svr_cfg *svrCfg = svr;
	reslist_t *rl = svrCfg->dbpool;

	if(rl)
		reslist_destroy(rl);

	return 0;
}

static int mysql_pool_setup(svr_cfg *svr)
{
	int rv;

	rv = reslist_create(&svr->dbpool, svr->nmin, svr->nkeep, svr->nmax,
			apr_time_from_sec(svr->exptime),
			mysql_pool_construct, mysql_pool_destruct, svr);

	if (rv == 0) {
		long long timeout = svr->timeout;
                reslist_timeout_set(svr->dbpool,timeout);
	}
	else {
		svr->pool = NULL;
	}

	return rv;
}

int mysql_pool_init(svr_cfg *s)
{
	svr_cfg *svr = s;
	int rv;

	rv = mysql_pool_setup(svr);
	if (rv == 0) {
		return rv;
	}

	rv = thread_mutex_create(&svr->mutex, THREAD_MUTEX_DEFAULT);
	if (rv != 0) {
	}
	return rv;
}

static int mysql_pool_setup_lock(svr_cfg *s)
{
	svr_cfg *svr = s;
	int rv, rv2 = 0;

	if (!svr->mutex) {
		return -1;
	}

	rv = thread_mutex_lock(svr->mutex);
	if (rv != 0) {
		return rv;
	}

	if (!svr->dbpool) {
		rv2 = mysql_pool_setup(svr);
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
MYSQL_DECLARE_NONSTD(void) mysql_pool_release(svr_cfg *s, MYSQL *sql)
{
	svr_cfg *svr = s;
	reslist_release(svr->dbpool, sql);
}
#else
MYSQL_DECLARE_NONSTD(void) mysql_pool_release(server_rec *s, MYSQL *sql)
{
	sql = NULL;
}
#endif

#if HAS_THREADS
MYSQL_DECLARE_NONSTD(MYSQL*) mysql_pool_acquire(svr_cfg *s)
{
	MYSQL *rec = NULL;
	svr_cfg *svr = s;
	int rv = 0;
	int rv2;

	if (!svr->dbpool) {
		if (mysql_pool_setup_lock(pool, s) != 0) {
			return NULL;
		}
	}

	rv = reslist_acquire(svr->dbpool, (void**)&rec);
	if (rv != 0) {
		return NULL;
	}

	/* Keep the connectoin alive */
	rv2 = mysql_ping(rec);
	if (rv2 != 0) {
		reslist_invalidate(svr->dbpool, rec);
		return NULL;
	}

	return rec;
}
#else
MYSQL_DECLARE_NONSTD(MYSQL*) mysql_pool_acquire(server_rec *s)
{
	return NULL;
}
#endif

apr_status_t mysql_pool_fini(svr_cfg *s)
{
	svr_cfg *svr = s;
	int rv;

	rv = mysql_pool_cleanup(svr);
	return rv;
}

int mysql_pool_query(svr_cfg *s,
		const char *sql,
		size_t len,
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

int mysql_pool_exec(svr_cfg *s,
		const char *sql,
		size_t len,
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
