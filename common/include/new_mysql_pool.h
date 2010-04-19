#ifndef MYSQL_POOL_H
#define MYSQL_POOL_H

#ifdef __cplusplus
extern "C"{
#endif

#if !defined(WIN32)
#define MYSQL_DECLARE(type)            type
#define MYSQL_DECLARE_NONSTD(type)     type
#define MYSQL_DECLARE_DATA
#elif defined(MYSQL_DECLARE_STATIC)
#define MYSQL_DECLARE(type)            type __stdcall
#define MYSQL_DECLARE_NONSTD(type)     type
#define MYSQL_DECLARE_DATA
#elif defined(MYSQL_DECLARE_EXPORT)
#define MYSQL_DECLARE(type)            __declspec(dllexport) type __stdcall
#define MYSQL_DECLARE_NONSTD(type)     __declspec(dllexport) type
#define MYSQL_DECLARE_DATA             __declspec(dllexport)
#else
#define MYSQL_DECLARE(type)            __declspec(dllimport) type __stdcall
#define MYSQL_DECLARE_NONSTD(type)     __declspec(dllimport) type
#define MYSQL_DECLARE_DATA             __declspec(dllimport)
#endif

#define DB_DEFAULT_PORT 3306
/* A default nmin of >0 will help with generating meaningful
 * startup error messages if the database is down.
 */
#define DB_DEFAULT_NMIN    5
#define DB_DEFAULT_NKEEP   10
#define DB_DEFAULT_NMAX    30
#define DB_DEFAULT_EXPTIME 300/*second*/
#define DB_DEFAULT_TIMEOUT 3000/*ms*/
#define DB_DEFAULT_CHARSET "utf8"


#include <reslist.h>
#include <my_global.h>
#include <mysql.h>

#include "thread_mutex.h"

/************ svr cfg: manage db connection pool ****************/

typedef struct svr_cfg {
	char host[256];
	char user[256];
	char password[256];
	char db[256];
	char charset[64];
	int port;

	thread_mutex_t *mutex;
	reslist_t *dbpool;
	int nmin;
	int nkeep;
	int nmax;
	int exptime;
	int timeout;

	unsigned int set;
} svr_cfg;

/*setup init*/
MYSQL_DECLARE_NONSTD(int) mysql_pool_init(svr_cfg *s);
/* acquire a connection that MUST be explicitly closed.
 * Returns NULL on error
 */
MYSQL_DECLARE_NONSTD(MYSQL*) mysql_pool_acquire(svr_cfg*);
/* release a connection acquired with ap_dbd_open */
MYSQL_DECLARE_NONSTD(void) mysql_pool_release(svr_cfg*, MYSQL*);

/*fini*/
MYSQL_DECLARE_NONSTD(int) mysql_pool_fini(svr_cfg *s);

/*sql op function*/
MYSQL_DECLARE_NONSTD(int) mysql_pool_query(svr_cfg *s,
		const char *sql,
		size_t len,
		MYSQL_RES **rs);

MYSQL_DECLARE_NONSTD(int) mysql_pool_exec(svr_cfg *s,
		const char *sql,
		size_t len,
		unsigned long *rows);



#ifdef __cplusplus
}
#endif

#endif

