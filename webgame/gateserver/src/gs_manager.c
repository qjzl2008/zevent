#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <pthread.h>
#include "znet.h"
#include "gs_manager.h"
#include "json_helper.h"
#include "thread_mutex.h"
#include "hash.h"
#include "client.h"

typedef struct gs_manager_t gs_manager_t;
struct gs_manager_t{
    net_server_t *ns;

    thread_mutex_t *mutex;
    hash_t *gs2peer;
    hash_t *peer2gs;

    pthread_t thread_id;
    int stop_daemon;
};

static gs_manager_t *gm = NULL;

static int init_ns_arg(ns_arg_t *ns_arg)
{
    ns_arg->func = NULL;
    json_object *jfile = json_object_from_file("config.json");
    json_object *jip = json_util_get(jfile,"CONFIG.gs-server-address");
    if(!jip)
	return -1;
    const char *ip = json_object_get_string(jip);
    snprintf(ns_arg->ip,sizeof(ns_arg->ip),ip);

    json_object *jport = json_util_get(jfile,"CONFIG.gs-server-port");
    if(!jport)
	return -1;
    int port = json_object_get_int(jport);
    ns_arg->port = port;
    json_object_put(jfile);
    return 0;
}

#define HEADER_LEN (4)
static int process_msg(void *msg,int len,uint64_t peerid)
{
    char *body = (char *)msg + HEADER_LEN;
    json_object *jmsg = json_tokener_parse(body);
    if(!jmsg)
	return -1;

    json_object_put(jmsg);
    return 0;
}

#define MS_PER_SECOND (1000000)
static void *thread_entry(void *arg)
{
    int rv;
    gs_manager_t *gm = (gs_manager_t *)arg;
    net_server_t *ns = gm->ns;
    void *msg;uint32_t len;

    while(!gm->stop_daemon)
    {
	uint64_t peer_id;
	rv = ns_recvmsg(ns,&msg,&len,&peer_id,MS_PER_SECOND);
	if(rv < 0)
	    continue;
	if(rv == 0)
	{
	    process_msg(msg,len,peer_id);
	    //process msg
	    ns_free(ns,msg);
	}
	if(rv == 1)
	{
	    //disconnect
	}
    }
    return NULL;
}

int gm_start()
{
    gm = (gs_manager_t *)malloc(sizeof(gs_manager_t));
    ns_arg_t ns_arg;
    int rv = init_ns_arg(&ns_arg);
    if(rv < 0)
	return -1;
    printf("gm ip:%s,port:%d\n",ns_arg.ip,ns_arg.port);
    rv = ns_start_daemon(&gm->ns,&ns_arg);
    if(rv < 0)
	return -1;

    thread_mutex_create(&gm->mutex,THREAD_MUTEX_DEFAULT);
    gm->peer2gs = hash_make();
    gm->gs2peer = hash_make();
    gm->stop_daemon = 0;
    //start the main loop thread
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_create(&gm->thread_id, &attr, thread_entry, gm);
    pthread_attr_destroy(&attr);

    return 0;
}

int gm_stop()
{
    gm->stop_daemon = 1;
    pthread_join(gm->thread_id,NULL);
    ns_stop_daemon(gm->ns);
    return 0;
}

int gm_destroy()
{
    hash_destroy(gm->peer2gs);
    hash_destroy(gm->gs2peer);

    thread_mutex_destroy(gm->mutex);
    free(gm);
    gm = NULL;
    return 0;
}
