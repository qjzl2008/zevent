#ifndef MONGO_POOL_H
#define MONGO_POOL_H

#ifdef __cplusplus
extern "C"{
#endif

#if !defined(WIN32)
#define MONGO_DECLARE(type)            type
#define MONGO_DECLARE_NONSTD(type)     type
#define MONGO_DECLARE_DATA
#elif defined(MONGO_DECLARE_STATIC)
#define MONGO_DECLARE(type)            type __stdcall
#define MONGO_DECLARE_NONSTD(type)     type
#define MONGO_DECLARE_DATA
#elif defined(MONGO_DECLARE_EXPORT)
#define MONGO_DECLARE(type)            __declspec(dllexport) type __stdcall
#define MONGO_DECLARE_NONSTD(type)     __declspec(dllexport) type
#define MONGO_DECLARE_DATA             __declspec(dllexport)
#else
#define MONGO_DECLARE(type)            __declspec(dllimport) type __stdcall
#define MONGO_DECLARE_NONSTD(type)     __declspec(dllimport) type
#define MONGO_DECLARE_DATA             __declspec(dllimport)
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

/************ svr cfg: manage connection pool ****************/

typedef enum{
		MONGO_SINGLE = 0,
		MONGO_RS = 1
}mongo_type_t;

typedef struct mongo_svr_cfg {
	char connstr[1024];
	char rsname[64];
	mongo_type_t mongo_type;

	thread_mutex_t *mutex;
	reslist_t *connpool;
	int nmin;
	int nkeep;
	int nmax;
	int exptime;/*second ttl*/
	int timeout;/*ms*/

	unsigned int set;
} mongo_svr_cfg;

/*setup init*/
MONGO_DECLARE_NONSTD(int) mongo_pool_init(mongo_svr_cfg *s);
/* acquire a connection that MUST be explicitly closed.
 * Returns NULL on error
 */
MONGO_DECLARE_NONSTD(void *) mongo_pool_acquire(mongo_svr_cfg*);
MONGO_DECLARE_NONSTD(void) mongo_pool_release(mongo_svr_cfg*, void *);

/* remove a connection*/
MONGO_DECLARE_NONSTD(void) mongo_pool_remove(mongo_svr_cfg*, void *);

/*fini*/
MONGO_DECLARE_NONSTD(int) mongo_pool_fini(mongo_svr_cfg *s);

#ifdef __cplusplus
}
#endif

#endif

