#ifndef REDIS_POOL_H
#define REDIS_POOL_H

#ifdef __cplusplus
extern "C"{
#endif

#if !defined(WIN32)
#define REDIS_DECLARE(type)            type
#define REDIS_DECLARE_NONSTD(type)     type
#define REDIS_DECLARE_DATA
#elif defined(REDIS_DECLARE_STATIC)
#define REDIS_DECLARE(type)            type __stdcall
#define REDIS_DECLARE_NONSTD(type)     type
#define REDIS_DECLARE_DATA
#elif defined(REDIS_DECLARE_EXPORT)
#define REDIS_DECLARE(type)            __declspec(dllexport) type __stdcall
#define REDIS_DECLARE_NONSTD(type)     __declspec(dllexport) type
#define REDIS_DECLARE_DATA             __declspec(dllexport)
#else
#define REDIS_DECLARE(type)            __declspec(dllimport) type __stdcall
#define REDIS_DECLARE_NONSTD(type)     __declspec(dllimport) type
#define REDIS_DECLARE_DATA             __declspec(dllimport)
#endif

/* A default nmin of >0 will help with generating meaningful
 * startup error messages if the database is down.
 */
#define DEFAULT_NMIN    5
#define DEFAULT_NKEEP   10
#define DEFAULT_NMAX    30
#define DEFAULT_EXPTIME 300/*second*/
#define DEFAULT_TIMEOUT 3000/*ms*/


#include "reslist.h"
#include "thread_mutex.h"

/************ svr cfg: manage redisection pool ****************/

typedef struct redis_svr_cfg {
	char host[64];
	int port;

	thread_mutex_t *mutex;
	reslist_t *connpool;
	int nmin;
	int nkeep;
	int nmax;
	int exptime;/*second ttl*/
	int timeout;/*ms*/

	unsigned int set;
} redis_svr_cfg;

/*setup init*/
REDIS_DECLARE_NONSTD(int) redis_pool_init(redis_svr_cfg *s);
/* acquire a redisection that MUST be explicitly closed.
 * Returns NULL on error
 */
REDIS_DECLARE_NONSTD(void *) redis_pool_acquire(redis_svr_cfg*);
REDIS_DECLARE_NONSTD(void) redis_pool_release(redis_svr_cfg*, void *);

/* remove a redisection*/
REDIS_DECLARE_NONSTD(void) redis_pool_remove(redis_svr_cfg*, void *);

/*fini*/
REDIS_DECLARE_NONSTD(int) redis_pool_fini(redis_svr_cfg *s);

#ifdef __cplusplus
}
#endif

#endif

