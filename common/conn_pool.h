#ifndef CONN_POOL_H
#define CONN_POOL_H

#ifdef __cplusplus
extern "C"{
#endif

#if !defined(WIN32)
#define CONN_DECLARE(type)            type
#define CONN_DECLARE_NONSTD(type)     type
#define CONN_DECLARE_DATA
#elif defined(CONN_DECLARE_STATIC)
#define CONN_DECLARE(type)            type __stdcall
#define CONN_DECLARE_NONSTD(type)     type
#define CONN_DECLARE_DATA
#elif defined(CONN_DECLARE_EXPORT)
#define CONN_DECLARE(type)            __declspec(dllexport) type __stdcall
#define CONN_DECLARE_NONSTD(type)     __declspec(dllexport) type
#define CONN_DECLARE_DATA             __declspec(dllexport)
#else
#define CONN_DECLARE(type)            __declspec(dllimport) type __stdcall
#define CONN_DECLARE_NONSTD(type)     __declspec(dllimport) type
#define CONN_DECLARE_DATA             __declspec(dllimport)
#endif

/* A default nmin of >0 will help with generating meaningful
 * startup error messages if the database is down.
 */
#define DEFAULT_NMIN    5
#define DEFAULT_NKEEP   10
#define DEFAULT_NMAX    30
#define DEFAULT_EXPTIME 300/*second*/
#define DEFAULT_TIMEOUT 3000/*ms*/


#include <apr_hash.h>
#include <apr_reslist.h>

/************ svr cfg: manage connection pool ****************/

typedef struct conn_svr_cfg {
	char host[64];
	int port;

	apr_thread_mutex_t *mutex;
	apr_pool_t *pool;
	apr_reslist_t *connpool;
	int nmin;
	int nkeep;
	int nmax;
	int exptime;/*second ttl*/
	int timeout;/*ms*/

	unsigned int set;
} conn_svr_cfg;

/*setup init*/
CONN_DECLARE_NONSTD(apr_status_t) conn_pool_init(apr_pool_t *pool, conn_svr_cfg *s);
/* acquire a connection that MUST be explicitly closed.
 * Returns NULL on error
 */
CONN_DECLARE_NONSTD(int *) conn_pool_acquire(apr_pool_t*, conn_svr_cfg*);
/* release a connection acquired with ap_dbd_open */
CONN_DECLARE_NONSTD(void) conn_pool_release(conn_svr_cfg*, int *);

/*fini*/
CONN_DECLARE_NONSTD(apr_status_t) conn_pool_fini(apr_pool_t *pool, conn_svr_cfg *s);

#ifdef __cplusplus
}
#endif

#endif

