#include <stdlib.h>
#include "apr.h"
#include "apr_general.h"
#include "apr_file_io.h"
#include "apr_buckets.h"
#include "apr_strings.h"
#include "zevent_mpm.h"
#include "zevent_hooks.h"
#include "log.h"
#include "db.h"
#include "bdb_svc.h"
#include "iniparser.h"
#include "protocol.h"

//const char *cfg = "/home/zhoubug/dev/svn_work/zsh/zevent/apps/rtservice/rtinfoserver/build/bin/config.ini";
const char *cfg = "config.ini";
static apr_pool_t *pglobal;
DB_ENV *db_env = NULL;
DB *dbp = NULL;

static void svc_init(apr_pool_t *p)
{
	apr_status_t rv;
	int ret;
	char envdir[1024] = {'\0'};
	char datadir[1024] = {'\0'};
	char logdir[1024] = {'\0'};

	dictionary *d = (dictionary *)iniparser_load(cfg);
	if(!d)
	{
		zevent_log_error(APLOG_MARK,NULL,"Call iniparser_load failed!\n");
		return;
	}
	const char *dbhome = (char *)iniparser_getstring(d,"bdb:dbhome",NULL);
	if(!dbhome)
	{
		zevent_log_error(APLOG_MARK,NULL,"Call iniparser_getstring failed!\n");
		return;
	}

	rv = apr_dir_make(dbhome,APR_UREAD | APR_UWRITE | APR_UEXECUTE,p);
	apr_snprintf(envdir,sizeof(envdir),"%s/env",dbhome);
	rv = apr_dir_make(envdir,APR_UREAD | APR_UWRITE |
			APR_UEXECUTE,p);
	apr_snprintf(datadir,sizeof(datadir),"%s/data",dbhome);
	rv = apr_dir_make(datadir,APR_UREAD | APR_UWRITE |
			APR_UEXECUTE,p);
	apr_snprintf(logdir,sizeof(logdir),"%s/log",dbhome);
	rv = apr_dir_make(logdir,APR_UREAD | APR_UWRITE | APR_UEXECUTE,p);

        u_int32_t env_flags = DB_CREATE | DB_INIT_LOCK | DB_INIT_LOG |
		DB_INIT_MPOOL | DB_THREAD | DB_INIT_TXN | DB_RECOVER;


	if((ret = openenv(&db_env,envdir,"../data","../log",
					NULL,CACHE_SIZE,env_flags,p))!=0)
	{
		return ;
	}
	iniparser_freedict(d);
	return ;
}

static void svc_fini(apr_pool_t *p)
{
	closeenv(db_env);
	zevent_log_error(APLOG_MARK,NULL,"fini the app");
}

static void init_child(apr_pool_t *pchild)
{
	/*
	 * add what you want to initialize for one child process,eg:database connection..
	 */

	dictionary *d = (dictionary *)iniparser_load(cfg);
	if(!d)
	{
		zevent_log_error(APLOG_MARK,NULL,"Call iniparser_load failed!\n");
		return;
	}
	const char *dbhome = (char *)iniparser_getstring(d,"bdb:dbhome",NULL);
	if(!dbhome)
	{
		zevent_log_error(APLOG_MARK,NULL,"Call iniparser_getstring failed!\n");
		return;
	}
	int rv = opendb(dbhome,&db_env,&dbp,pchild);
	zevent_log_error(APLOG_MARK,NULL,"init one child %d",rv);
	iniparser_freedict(d);
}

static void fini_child(apr_pool_t *pchild)
{
	closedb(dbp);
	zevent_log_error(APLOG_MARK,NULL,"fini one child");
}

static void dispose(char *msg,int *len)
{
	char *p = NULL;
	int slen = 0,i = 0;
	TDATA tdata;
	memset(&tdata,0,sizeof(TDATA));

	memcpy(&tdata.command,msg+sizeof(unsigned int),sizeof(tdata.command));
	tdata.data = msg + sizeof(unsigned int) + sizeof(tdata.command);
	char *send_info = msg + 3*sizeof(unsigned int);//len+command+result

	STORE store;
	store.data.curTime = time(NULL);

	switch(tdata.command)
	{
		case CMD_DATA_GET:
			store_unserialize(&store,(char*)tdata.data,
					*len-2*sizeof(unsigned int));
			if(get_store(dbp,&store) != 0)
				goto fail;
			if((p = store_serialize(&store,send_info,*len-3*sizeof(unsigned int))) == NULL)
			{
				goto fail;
			}
			slen = p - send_info;
			break;
		case CMD_DATA_SET:
			store_unserialize(&store,(char*)tdata.data,
					*len-2*sizeof(unsigned int));

//			zevent_log_error(APLOG_MARK,NULL,"key:%s,value:%s",store.key,
//					store.data.value);
			if(set_store(dbp,&store) != 0)
				goto fail;
			slen = 0;
			break;
		default:
			goto fail;
	}
	*((unsigned int*)msg+1) = tdata.command;
	*(((unsigned int*)msg)+2) = CMD_SUCCESSFUL;
	slen += 2*sizeof(unsigned int);
	for(i = 0; i < sizeof(unsigned int); i++)
	{
		msg[i] = (slen >> (8 * i)) & 0xff;
	}
	*len = slen + sizeof(unsigned int);

	return;
fail:
	*((unsigned int*)msg+1) = tdata.command;
	*(((unsigned int*)msg)+2) = CMD_FAIL;
	slen = 2*sizeof(unsigned int);
	for(i = 0; i < sizeof(unsigned int); i++)
	{
		msg[i] = (slen >> (8 * i)) & 0xff;
	}

	*len = slen + sizeof(unsigned int);
	return;
}

static int zevent_process_connection(conn_state_t *cs)
{
	/*
	 * code for your app,this just an example for echo test.
	 */
	apr_bucket *b;
	unsigned char *msg;
	apr_size_t msg_len = 0;
	apr_size_t len=0;
	int i=0;
	int olen = 0;
	const char *buf;
	apr_status_t rv;

	cs->pfd->reqevents = APR_POLLIN;

	if(cs->pfd->rtnevents & APR_POLLIN){
		unsigned char length[sizeof(unsigned int)+1];
		memset(length,0,sizeof(length));
		len = sizeof(unsigned int);
		rv = apr_socket_recv(cs->pfd->desc.s,(char *)length,&len);
		if(rv != APR_SUCCESS)
		{
			zevent_log_error(APLOG_MARK,NULL,"close socket!");
			return -1;
		}

                for(i = 0; i < sizeof(unsigned int); ++i) {
			msg_len |= (size_t)length[i] << (8 * i);
		}

		if(msg_len <= 0)
			return -1;
		if(msg_len > BUFFER_SIZE - 4)
			return -1;

		msg = (unsigned char *)apr_bucket_alloc(BUFFER_SIZE,cs->baout);
		if (msg == NULL) {
			return -1;
		}
		rv = apr_socket_recv(cs->pfd->desc.s,(char *)(msg+sizeof(unsigned int)),&msg_len);
		if(rv != APR_SUCCESS)
		{
			zevent_log_error(APLOG_MARK,NULL,"close socket!");
			return -1;
		}
	//	zevent_log_error(APLOG_MARK,NULL,"recv:%d\n",len);

                int len = BUFFER_SIZE;
                dispose((char *)msg,&len);

		b = apr_bucket_heap_create((const char *)msg,len,NULL,cs->baout);
		apr_bucket_free(msg);
		APR_BRIGADE_INSERT_TAIL(cs->bbout,b);
		cs->pfd->reqevents |= APR_POLLOUT;
		
	}
	else {
		if(cs->bbout){
			for (b = APR_BRIGADE_FIRST(cs->bbout);
					b != APR_BRIGADE_SENTINEL(cs->bbout);
					b = APR_BUCKET_NEXT(b))
			{
				apr_bucket_read(b,&buf,&len,APR_BLOCK_READ);
				olen = len;
				//apr_brigade_flatten(cs->bbout,buf,&len);
				rv = apr_socket_send(cs->pfd->desc.s,buf,&len);

				if((rv == APR_SUCCESS) && (len>=olen))
				{
				//	zevent_log_error(APLOG_MARK,NULL,"send:%d bytes\n",
				//			len);
					apr_bucket_delete(b);
				}

				if((rv == APR_SUCCESS && len < olen) || 
						(rv != APR_SUCCESS))
				{
					if(rv == APR_SUCCESS){
						apr_bucket_split(b,len);
						apr_bucket *bucket = APR_BUCKET_NEXT(b);
						apr_bucket_delete(b);
						b = bucket;
					}
					break;
				}
			}
			if(b != APR_BRIGADE_SENTINEL(cs->bbout))
				cs->pfd->reqevents |= APR_POLLOUT;
		}
	}

	apr_pollset_add(cs->pollset,cs->pfd);
	return 0;
}

int main(int argc,const char * const argv[])
{
	if(zevent_init(cfg,&pglobal)==-1)
		return -1;

	zevent_hook_zevent_init(svc_init,NULL,NULL,APR_HOOK_MIDDLE);
	zevent_hook_zevent_fini(svc_fini,NULL,NULL,APR_HOOK_MIDDLE);
	zevent_hook_child_init(init_child,NULL,NULL,APR_HOOK_MIDDLE);
	zevent_hook_child_fini(fini_child,NULL,NULL,APR_HOOK_MIDDLE);

	zevent_hook_process_connection(zevent_process_connection,NULL,NULL,APR_HOOK_REALLY_LAST);
	zevent_run(pglobal);

	zevent_fini(&pglobal);
	return 0;
}

