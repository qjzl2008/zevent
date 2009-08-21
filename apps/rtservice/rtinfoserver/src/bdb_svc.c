#include <string.h>
#include "bdb_svc.h"
#include "apr_file_io.h"
#include "apr_file_info.h"
#include "apr_thread_proc.h"
#include "log.h"
#include "protocol.h"

static int volatile shutdown_ = 0;
static apr_thread_t *chkpnt_threadid;
static apr_threadattr_t *thread_attr;

static void *APR_THREAD_FUNC chkpnt_thread(apr_thread_t *thd,void *dummy)
{
	int ret;
        DB_ENV *db_env = (DB_ENV *)dummy;
	while(!shutdown_){
		if((db_env) && (ret = db_env->txn_checkpoint(db_env,CHKPNT_DSIZE,
						CHKPNT_DTIME,0)!=0)){
				return NULL;
		}

		apr_sleep(apr_time_from_sec(CHKPNT_CYCLE));
	}
	apr_thread_exit(thd,APR_SUCCESS);
	return NULL;
}

int openenv(DB_ENV **pdb_env,const char *home,const char *data_dir,
		const char *log_dir,FILE *err_file,
		int cachesize,unsigned int flag,apr_pool_t *p)
{
	int rv = -1;
	if((rv = db_env_create(pdb_env,0)) != 0){
		return rv;
	}

	if((rv = (*pdb_env)->set_cachesize((*pdb_env),0,cachesize,0)) != 0){
		return rv;
	}

	if((rv = (*pdb_env)->set_data_dir((*pdb_env),data_dir)) != 0){
		return rv;
	}
	if((rv = (*pdb_env)->set_lg_dir((*pdb_env),log_dir)) != 0){
		return rv;
	}
	if((rv = (*pdb_env)->set_lk_detect((*pdb_env),DB_LOCK_DEFAULT)) != 0){
		return rv;
	}
	if((rv = (*pdb_env)->set_tx_max((*pdb_env),1024)) != 0){
		return rv;
	}
	if((rv = (*pdb_env)->set_lg_bsize((*pdb_env),cachesize)) != 0){
		return rv;
	}
	if((rv =(*pdb_env)->set_flags((*pdb_env),
					DB_TXN_NOSYNC,1) !=0))
	{
		return rv;
	}


	if((rv = (*pdb_env)->open((*pdb_env),home,flag,0)) != 0){

		(*pdb_env)->close((*pdb_env),0);
		return rv;
	}

        if((rv=(*pdb_env)->log_set_config((*pdb_env),DB_LOG_AUTO_REMOVE,1) != 0))
	{
		return rv;
	}

	apr_threadattr_create(&thread_attr,p);
	apr_threadattr_detach_set(thread_attr,1);
	rv = apr_thread_create(&chkpnt_threadid,thread_attr,chkpnt_thread,*pdb_env,p);
	if(rv != APR_SUCCESS){
		return -1;
	}

	return 0;
}

int open_db(DB_ENV *pdb_env,DB **pdb,const char *db_name,
		DBTYPE type,unsigned int open_flags,
		unsigned int set_flags,
		int (*bt_compare_fcn)(DB *db,const DBT *dbt1,
			const DBT *dbt2))
{
	DB_TXN *tid = NULL;
	int ret;
	if((ret = db_create(pdb,pdb_env,0))!= 0)
	{
		return -1;
	}

	if(set_flags)
		(*pdb)->set_flags((*pdb),set_flags);
	if(bt_compare_fcn)
		(*pdb)->set_bt_compare((*pdb),bt_compare_fcn);
	ret = (*pdb)->open((*pdb),tid,db_name,NULL,type,open_flags,0);
	if(ret != 0)
		return -1;
	return 0;
}

int opendb(const char *dbhome,DB_ENV **db_env,DB **dbp,apr_pool_t *p)
{
	int ret;

	DBTYPE db_type = DB_BTREE;
	u_int32_t db_flags = DB_CREATE | DB_THREAD | DB_AUTO_COMMIT;
        if((ret = open_db(*db_env,dbp,"data.db",db_type,db_flags,0,NULL)) != 0)
	{
		return -1;
	}
	
/*	rv = apr_thread_join(&thread_rv,chkpnt_threadid);
	if(rv != APR_SUCCESS)
	{
	}*/
	return 0;
}

int closedb(DB *dbp)
{
	if(dbp)
		dbp->close(dbp,0);
	return 0;
}

int closeenv(DB_ENV *db_env)
{
	shutdown_ = 1;
	if(db_env)
		db_env->close(db_env,0);
	return 0;
}

int set_store(DB *dbp,const PSTORE pstore)
{
	DBT key,data;
        char buf[BUFFER_SIZE];
        int ret;
	memset(buf,0,sizeof(buf));

	memset(&key,0,sizeof(key));
	memset(&data,0,sizeof(data));
	key.data = pstore->key;
	key.size = strlen(pstore->key)+1;
	data.data = buf;
	data.size = data_serialize(&pstore->data,buf,sizeof(buf)) - buf;

	switch(ret = dbp->put(dbp,NULL,&key,&data,0)){
		case 0:
			break;
		default:
			return -1;
	}
	return 0;
}

int get_store(DB *dbp,PSTORE pstore)
{
	DBT key,data;
	char buf[BUFFER_SIZE];
	int ret;

	memset(&key,0,sizeof(key));
	memset(&data,0,sizeof(data));
	key.data = pstore->key;
	key.size = strlen(pstore->key) + 1;
	data.flags = DB_DBT_USERMEM;

	memset(buf,0,sizeof(buf));
	data.data = buf;
	data.ulen = sizeof(buf);

	switch (ret = dbp->get(dbp,NULL,&key,&data,0)){
		case 0:
			data_unserialize(&pstore->data,buf,sizeof(buf));
			return 0;
			break;
		case DB_NOTFOUND:
			return -2;
			break;
		default:
			return -1;
	}
	return 0;
}

