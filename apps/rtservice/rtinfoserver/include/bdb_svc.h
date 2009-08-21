#ifndef BDB_SVC_H
#define BDB_SVC_H

#include "apr.h"
#include "apr_pools.h"
#include "db.h"
#include "store.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CHKPNT_DSIZE (256*1024*1024)
#define CHKPNT_DTIME (10)
#define CHKPNT_CYCLE (15)
#define CACHE_SIZE (512*1024*1024)

	int openenv(DB_ENV **pdb_env,const char *home,const char *data_dir,
			const char *log_dir,FILE *err_file,
			int cachesize,unsigned int flag,apr_pool_t *p);

	int open_db(DB_ENV *pdb_env,DB **pdb,const char *db_name,
			DBTYPE type,unsigned int open_flags,
			unsigned int set_flags,
			int (*bt_compare_fcn)(DB *db,const DBT *dbt1,
				const DBT *dbt2));

	int opendb(const char *cfg,DB_ENV **db_env,DB **dbp,apr_pool_t *p);
	int closedb();
        int closeenv(DB_ENV *db_env);
	
	int set_store(DB *pdb,const PSTORE pstore);
	int get_store(DB *pdb,PSTORE pstore);

#ifdef __cplusplus
}
#endif
#endif
