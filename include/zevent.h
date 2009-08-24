/**
 * @file  zevent.h
 * @{
 */

#ifndef ZEVENT_MPM_H
#define ZEVENT_MPM_H

#include "apr_thread_proc.h"
#include "zevent_config.h"
#include "apr_buckets.h"
#include "apr_poll.h"
//from evev.c
/* Limit on the total --- clients will be locked out if more servers than
 * this are needed.  It is intended solely to keep the server from crashing
 * when things get out of hand.
 *
 * We keep a hard maximum number of servers, for two reasons --- first off,
 * in case something goes seriously wrong, we want to stop the fork bomb
 * short of actually crashing the machine we're running on by filling some
 * kernel table.  Secondly, it keeps the size of the scoreboard file small
 * enough that we can read the whole thing without worrying too much about
 * the overhead.
 */
#ifndef DEFAULT_SERVER_LIMIT
#define DEFAULT_SERVER_LIMIT 16
#endif

/* Admin can't tune ServerLimit beyond MAX_SERVER_LIMIT.  We want
 * some sort of compile-time limit to help catch typos.
 */
#ifndef MAX_SERVER_LIMIT
#define MAX_SERVER_LIMIT 20000
#endif

/* Limit on the threads per process.  Clients will be locked out if more than
 * this are needed.
 *
 * We keep this for one reason it keeps the size of the scoreboard file small
 * enough that we can read the whole thing without worrying too much about
 * the overhead.
 */
#ifndef DEFAULT_THREAD_LIMIT
#define DEFAULT_THREAD_LIMIT 64
#endif

/* Admin can't tune ThreadLimit beyond MAX_THREAD_LIMIT.  We want
 * some sort of compile-time limit to help catch typos.
 */
#ifndef MAX_THREAD_LIMIT
#define MAX_THREAD_LIMIT 100000
#endif
/////

#ifdef __cplusplus
extern "C" {
#endif

/*
 * * the ones you should need
 * */

ZEVENT_DECLARE(int) zevent_init(const char *inifile,apr_pool_t **pglobal);

ZEVENT_DECLARE(int) zevent_run(apr_pool_t *pconf);

/**
 * predicate indicating if a graceful stop has been requested ...
 * used by the connection loop 
 * @return 1 if a graceful stop has been requested, 0 otherwise
 * @deffunc int zevent_graceful_stop_signalled(*void)
 */
ZEVENT_DECLARE(int) zevent_graceful_stop_signalled(void);

ZEVENT_DECLARE(int) zevent_fini(apr_pool_t **pglobal);
/* Values returned for ZEVENT_MPMQ_MPM_STATE */
#define ZEVENT_MPMQ_STARTING              0
#define ZEVENT_MPMQ_RUNNING               1
#define ZEVENT_MPMQ_STOPPING              2

#define ZEVENT_MPMQ_MAX_DAEMON_USED       1  /* Max # of daemons used so far */
#define ZEVENT_MPMQ_IS_THREADED           2  /* MPM can do threading         */
#define ZEVENT_MPMQ_IS_FORKED             3  /* MPM can do forking           */
#define ZEVENT_MPMQ_HARD_LIMIT_DAEMONS    4  /* The compiled max # daemons   */
#define ZEVENT_MPMQ_HARD_LIMIT_THREADS    5  /* The compiled max # threads   */
#define ZEVENT_MPMQ_MAX_THREADS           6  /* # of threads/child by config */
#define ZEVENT_MPMQ_MIN_SPARE_DAEMONS     7  /* Min # of spare daemons       */
#define ZEVENT_MPMQ_MIN_SPARE_THREADS     8  /* Min # of spare threads       */
#define ZEVENT_MPMQ_MAX_SPARE_DAEMONS     9  /* Max # of spare daemons       */
#define ZEVENT_MPMQ_MAX_SPARE_THREADS    10  /* Max # of spare threads       */
#define ZEVENT_MPMQ_MAX_REQUESTS_DAEMON  11  /* Max # of requests per daemon */
#define ZEVENT_MPMQ_MAX_DAEMONS          12  /* Max # of daemons by config   */
#define ZEVENT_MPMQ_MPM_STATE            13  /* starting, running, stopping  */
#define ZEVENT_MPMQ_IS_ASYNC             14  /* MPM can process async connections  */

ZEVENT_DECLARE(apr_status_t) zevent_mpm_query(int query_code, int *result);

/** A structure that represents the status of the current connection */
typedef struct conn_state_t conn_state_t;

/** 
 *  * @brief A structure to contain connection state information 
 *   */
struct conn_state_t {
	/** memory pool to allocate from */
	apr_pool_t *p;
	/** poll file decriptor information */
	apr_pollfd_t *pfd;

	apr_pollset_t *pollset;

	apr_bucket_alloc_t *bain;
	apr_bucket_brigade *bbin;

	apr_bucket_alloc_t *baout;
	apr_bucket_brigade *bbout;
};

#ifdef __cplusplus
	}
#endif

#endif
/** @} */
