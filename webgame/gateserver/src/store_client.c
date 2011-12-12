#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <pthread.h>
#include "znet.h"
#include "client_manager.h"
#include "json_helper.h"
#include "thread_mutex.h"
#include "hash.h"
#include "store_client.h"

typedef struct store_client_t store_client_t;
struct store_client_t{
    net_client_t *nc;

    pthread_t thread_id;
    int stop_daemon;
};

static store_client_t *sc = NULL;

static int init_nc_arg(nc_arg_t *nc_arg)
{
    nc_arg->func = NULL;
    json_object *jfile = json_object_from_file("config.json");
    json_object *jip = json_util_get(jfile,"CONFIG.sqlstore-address");
    if(!jip)
	return -1;
    const char *ip = json_object_get_string(jip);
    snprintf(nc_arg->ip,sizeof(nc_arg->ip),ip);
    json_object_put(jip);

    json_object *jport = json_util_get(jfile,"CONFIG.sqlstore-port");
    if(!jport)
	return -1;
    int port = json_object_get_int(jport);
    nc_arg->port = port;
    json_object_put(jport);
    nc_arg->timeout = 3;
    return 0;
}

#define HEADER_LEN (4)
static int process_msg(void *msg,int len)
{
    char *body = (char *)msg + HEADER_LEN;
    json_object *jobj = json_tokener_parse(body);
    if(!jobj)
	return -1;
    return 0;
}

#define MS_PER_SECOND (1000000)
static void *thread_entry(void *arg)
{
    int rv;
    store_client_t *sc = (store_client_t *)arg;
    net_client_t *nc = sc->nc;
    void *msg;uint32_t len;

    while(!sc->stop_daemon)
    {
	rv = nc_recvmsg(nc,&msg,&len,MS_PER_SECOND);
	if(rv < 0)
	    continue;
	if(rv == 0)
	{
	    process_msg(msg,len);
	    //process msg
	    nc_free(nc,msg);
	}
	if(rv == 1)
	{
	    //disconnect
	}
    }
    return NULL;
}

int sc_start()
{
    sc = (store_client_t *)malloc(sizeof(store_client_t));
    nc_arg_t nc_arg;
    int rv = init_nc_arg(&nc_arg);
    if(rv < 0)
	return -1;
    printf("sc ip:%s,port:%d\n",nc_arg.ip,nc_arg.port);
    rv = nc_connect(&sc->nc,&nc_arg);
    if(rv < 0)
	return -1;

    sc->stop_daemon = 0;
    //start the main loop thread
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_create(&sc->thread_id, &attr, thread_entry, sc);
    pthread_attr_destroy(&attr);

    return 0;
}

int sc_stop()
{
    sc->stop_daemon = 1;
    pthread_join(sc->thread_id,NULL);
    nc_disconnect(sc->nc);
    return 0;
}

int sc_destroy()
{
    free(sc);
    sc = NULL;
    return 0;
}
