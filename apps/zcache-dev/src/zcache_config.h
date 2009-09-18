#ifndef ZCACHE_CONFIG_H
#define ZCACHE_CONFIG_H

/* ARP headers */
#include "apr.h"
#include "apr_strings.h"
#define APR_WANT_STRFUNC
#include "apr_want.h"
#include "apr_tables.h"
#include "apr_lib.h"
#include "apr_fnmatch.h"
#include "apr_strings.h"
#include "apr_dbm.h"
#include "apr_rmm.h"
#include "apr_shm.h"
#include "apr_global_mutex.h"
#include "apr_optional.h"
#include "apr_queue.h"
#include "apr_strings.h"
#include "apr_env.h"
#include "apr_thread_mutex.h"
#include "apr_thread_cond.h"


/* The #ifdef macros are only defined AFTER including the above
* therefore we cannot include these system files at the top  :-(
*/
#if APR_HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#if APR_HAVE_UNISTD_H
#include <unistd.h> /* needed for STDIN_FILENO et.al., at least on FreeBSD */
#endif

/*
* Provide reasonable default for some defines
*/
#ifndef FALSE
#define FALSE (0)
#endif
#ifndef TRUE
#define TRUE (!FALSE)
#endif
#ifndef PFALSE
#define PFALSE ((void *)FALSE)
#endif
#ifndef PTRUE
#define PTRUE ((void *)TRUE)
#endif
#ifndef UNSET
#define UNSET (-1)
#endif
#ifndef NUL
#define NUL '\0'
#endif
#ifndef RAND_MAX
#include <limits.h>
#define RAND_MAX INT_MAX
#endif

#define APR_SHM_MAXSIZE (64 * 1024 * 1024)
#define DEF_IDXNUMS_PERDIVISION (1024)
#define DEF_DIVISION_NUMS (256)

typedef enum {
	ZCACHE_SCMODE_UNSET = UNSET,
	ZCACHE_SCMODE_NONE  = 0,
	ZCACHE_SCMODE_SHMCB = 1,
} zcache_scmode_t;

/*
* Define the STORAGE mutex modes
*/
typedef enum {
	ZCACHE_MUTEXMODE_UNSET  = UNSET,
	ZCACHE_MUTEXMODE_NONE   = 0,
	ZCACHE_MUTEXMODE_USED   = 1
} zcache_mutexmode_t;

typedef struct {
	pid_t           pid;
	apr_pool_t     *pPool;
	int            bFixed;
	int             nStorageMode;
	char           *szStorageDataFile;
	int             nStorageDataSize;
	apr_shm_t      *pStorageDataMM;
	apr_rmm_t      *pStorageDataRMM;
	void    *tStorageDataTable;
	zcache_mutexmode_t  nMutexMode;
	apr_lockmech_e  nMutexMech;
	const char     *szMutexFile;
	apr_global_mutex_t   *pMutex;
	unsigned int idx_nums_perdivision;
	unsigned int division_nums;
}MCConfigRecord;


#ifndef BOOL
#define BOOL unsigned int
#endif
#ifndef UCHAR
#define UCHAR unsigned char
#endif

#endif
