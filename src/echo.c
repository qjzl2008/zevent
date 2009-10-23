/*
 * just an echo server
 */

#include "zevent.h"
#include "zevent_hooks.h"
#include "log.h"
#include <stdlib.h>

const char *file = "config.ini";
static apr_pool_t *pglobal;

static void zevent_init_child(apr_pool_t *pchild)
{
     /*
      * add what you want to initialize for one child process,eg:database connection..
      */
     zevent_log_error(APLOG_MARK,NULL,"init one child");
}
static void zevent_fini_child(apr_pool_t *pchild)
{
     /*
      * add what you want to do when one child process exit,eg:close database connection..
      */
     zevent_log_error(APLOG_MARK,NULL,"fini one child");
}
static int zevent_process_connection(conn_state_t *cs)
{
	/*
	 * code for your app,this just an example for echo test.
	 */
	apr_bucket *b;
	char *msg;
	apr_size_t len=0;
	int olen = 0;
	const char *buf;
	apr_status_t rv;

	cs->pfd->reqevents = APR_POLLIN;

	if(cs->pfd->rtnevents & APR_POLLIN){
		len = 4096;
		msg = (char *)apr_bucket_alloc(len,cs->baout);
		if (msg == NULL) {
			return -1;
		}
		rv = apr_socket_recv(cs->pfd->desc.s,msg,&len);
		if(rv != APR_SUCCESS)
		{
			zevent_log_error(APLOG_MARK,NULL,"close socket!");
			return -1;
		}

		zevent_log_error(APLOG_MARK,NULL,"recv:%s",msg);

		b = apr_bucket_heap_create(msg,len,NULL,cs->baout);
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
	//				zevent_log_error(APLOG_MARK,NULL,"send:%d bytes\n",
	//						len);
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

	if(zevent_init(file,&pglobal)==-1)
		return -1;

	zevent_hook_child_init(zevent_init_child,NULL,NULL,APR_HOOK_MIDDLE);
	zevent_hook_child_fini(zevent_fini_child,NULL,NULL,APR_HOOK_MIDDLE);

	zevent_hook_process_connection(zevent_process_connection,NULL,NULL,APR_HOOK_REALLY_LAST);
	zevent_run(pglobal);

	zevent_fini(&pglobal);
	return 0;
}
