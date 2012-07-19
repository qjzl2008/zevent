#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <pthread.h>
#include "znet.h"
#include "client_manager.h"
#include "thread_mutex.h"
#include "hash.h"
#include "allocator.h"
#include "json/json.h"
#include "json_helper.h"
#include "request.h"
#include "response.h"
#include "cm_logic.h"


extern json_object *json_util_get(json_object *obj, const char *path);

typedef struct client_manager_t client_manager_t;
struct client_manager_t{
    net_server_t *ns;

    thread_mutex_t *mutex_mpool;
    allocator_t *allocator;

    int stop_daemon;
};

static client_manager_t *cm = NULL;

static int init_ns_arg(ns_arg_t *ns_arg)
{
    json_object *jfile = json_object_from_file("config.json");
    if(!jobject_ptr_isvalid(jfile))
    {
        return -1;
    }
 
    json_object *jip = json_util_get(jfile,"CONFIG.clients-server-address");
    if(!jip)
	return -1;
    const char *ip = json_object_get_string(jip);
    snprintf(ns_arg->ip,sizeof(ns_arg->ip),ip);

    json_object *jport = json_util_get(jfile,"CONFIG.clients-server-port");
    if(!jport)
	return -1;
    int port = json_object_get_int(jport);
    ns_arg->port = port;

    json_object *jmaxclients = json_util_get(jfile,"CONFIG.max-clients");
    if(!jmaxclients)
	return -1;
    int maxpeers = json_object_get_int(jmaxclients);
    ns_arg->max_peers = (uint32_t)maxpeers;
    json_object_put(jfile);
    return 0;
}

static int data_process_func(uint8_t *buf,uint32_t len,uint32_t *off)
{
    if(len <= 0)
        return -1;
//    *off = len;
//    return 0;

    struct request *req=request_new((char *)buf);
	int  i;
	char sb[BUF_SIZE]={0};

	if(req_state_len(req,sb)!=STATE_CONTINUE){
		fprintf(stderr,"argc format ***ERROR***,packet:%s",sb);
		return -1;
	}
	req->argc=atoi(sb);

	req->argv=(char**)calloc(req->argc,sizeof(char*));
	for(i=0;i<req->argc;i++){

		int argv_len;
		/*parse argv len*/
		memset(sb,0,BUF_SIZE);
		if(req_state_len(req,sb)!=STATE_CONTINUE){
			fprintf(stderr,"argv's length format ***ERROR***,packet:%s",sb);
			return -1;
		}

		argv_len=atoi(sb);
		req->pos+=(argv_len+2);
    }

    *off = req->pos;
    request_free(req);
    return 0;
}

static int msg_process_func(void *net,uint64_t peerid, void *buf,uint32_t len)
{
    net_server_t *ns = (net_server_t*)net;
    int ret = 0,rv = 0;
    struct request *req=request_new(buf);
    struct response *resp;
    ret=request_parse(req);
    if(ret){
        char sent_buf[MAX_SEND_LEN]={0};
        request_dump(req);
        switch(req->cmd){
            case CMD_PING:{
                              resp=response_new(0,OK_PONG);
                              response_detch(resp,sent_buf);

                              ns_sendmsg(ns,peerid,sent_buf,strlen(sent_buf));
                              response_dump(resp);
                              response_free(resp);
                              break;
                          }

            case CMD_SET:{
                             rv = cm_logic_set(req);
                             if(rv != 0)
                                 resp=response_new(0,ERR);
                             else
                                 resp=response_new(0,OK);
                             response_detch(resp,sent_buf);
                             ns_sendmsg(ns,peerid,sent_buf,strlen(sent_buf));
                             response_dump(resp);
                             response_free(resp);
                             break;
                         }

            case CMD_GET:{
                             char *res = NULL;
                             rv = cm_logic_get(req,&res);
                             if(rv != 0)
                             {
                                 resp=response_new(0,OK_404);
                                 response_detch(resp,sent_buf);
                                 ns_sendmsg(ns,peerid,sent_buf,strlen(sent_buf));
                                 response_dump(resp);
                                 response_free(resp);
                             }
                             else
                             {
                                 resp=response_new(1,OK_200);
                                 resp->argv[0]=res;
                                 response_detch(resp,sent_buf);
                                 ns_sendmsg(ns,peerid,sent_buf,strlen(sent_buf));
                                 response_dump(resp);
                                 response_free(resp);
                                 free(res);
                             }

                             break;
                         }
            case CMD_DEL:{
                             rv = cm_logic_del(req);
                             if(rv != 0)
                                 resp=response_new(0,ERR);
                             else
                                 resp=response_new(0,OK);
                             response_detch(resp,sent_buf);

                             ns_sendmsg(ns,peerid,sent_buf,strlen(sent_buf));
                             //response_dump(resp);
                             response_free(resp);
                             break;
                         }

            default:{
                        resp=response_new(0,ERR);
                        response_detch(resp,sent_buf);
                        ns_sendmsg(ns,peerid,sent_buf,strlen(sent_buf));
                        //response_dump(resp);
                        response_free(resp);
                        break;
                    }
        }

    }
    request_free(req);

    return 0;
}

int cm_start()
{
    cm = (client_manager_t *)malloc(sizeof(client_manager_t));
    ns_arg_t ns_arg;

    ns_arg.data_func = data_process_func;
    ns_arg.msg_func = msg_process_func;
    int rv = init_ns_arg(&ns_arg);
    if(rv < 0)
	return -1;
    printf("cm ip:%s,port:%d\n",ns_arg.ip,ns_arg.port);
    rv = ns_start_daemon(&cm->ns,&ns_arg);
    if(rv < 0)
	return -1;

    //init memory pool
    thread_mutex_create(&cm->mutex_mpool,THREAD_MUTEX_DEFAULT);
    allocator_create(&cm->allocator); 
    allocator_mutex_set(cm->allocator,cm->mutex_mpool);

    cm->stop_daemon = 0;
    return 0;
}

int cm_send2client(uint64_t peerid,void *buf,uint32_t len)
{
    ns_sendmsg(cm->ns,peerid,buf,len);
    return 0;
}

void* cm_malloc(uint32_t size)
{
    return mmalloc(cm->allocator,size);
}

int cm_free(void *memory)
{
    mfree(cm->allocator,memory);
    return 0;
}

int cm_stop()
{
    cm->stop_daemon = 1;
    ns_stop_daemon(cm->ns);
    return 0;
}

int cm_destroy()
{
    thread_mutex_destroy(cm->mutex_mpool);
    allocator_destroy(cm->allocator);

    free(cm);
    cm = NULL;
    return 0;
}

