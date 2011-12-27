#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <pthread.h>
#include "znet.h"
#include "allocator.h"
#include "gs_manager.h"
#include "json_helper.h"
#include "thread_mutex.h"
#include "hash.h"
#include "client.h"
#include "netcmd.h"
#include "gm_logic.h"
#include "client_manager.h"
#include "uuidservice.h"

typedef struct gs_manager_t gs_manager_t;
struct gs_manager_t{
    net_server_t *ns;

    thread_mutex_t *mutex_mpool;
    allocator_t *allocator;

    thread_mutex_t *mutex;
    hash_t *gs2peer;
    hash_t *peer2gs;
    hash_t *scene2peer;

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

    json_object *jmaxgs = json_util_get(jfile,"CONFIG.max-gs");
    if(!jmaxgs)
	return -1;
    int maxgs = json_object_get_int(jmaxgs);
    ns_arg->max_peers = (uint32_t)maxgs;

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
    json_object *jcmd = json_util_get(jmsg,"cmd");
    if(!jcmd)
	return -1;
    int cmd = json_object_get_int(jcmd);
    switch(cmd){
	case MSGID_REQUEST_REGGS:
	    gm_logic_reggs(peerid,jmsg);
	    break;
	case MSGID_REQUEST_DATA2CLIENTS:
	    printf("gs send2client:%s\n",body);
	    gm_logic_data2clients(peerid,jmsg);
	    break;
    }

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
	rv = ns_recvmsg(ns,&msg,&len,&peer_id,MS_PER_SECOND/4);
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
	    cm_rmclients(peer_id);
	    gm_unreggs(peer_id);
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
    //init memory pool
    thread_mutex_create(&gm->mutex_mpool,THREAD_MUTEX_DEFAULT);
    allocator_create(&gm->allocator); 
    allocator_mutex_set(gm->allocator,gm->mutex_mpool);

    thread_mutex_create(&gm->mutex,THREAD_MUTEX_DEFAULT);
    gm->peer2gs = hash_make();
    gm->gs2peer = hash_make();
    gm->scene2peer = hash_make();
    gm->stop_daemon = 0;
    //start the main loop thread
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_create(&gm->thread_id, &attr, thread_entry, gm);
    pthread_attr_destroy(&attr);

    return 0;
}

int gm_send2gs(uint64_t peerid,void *buf,uint32_t len)
{
    ns_sendmsg(gm->ns,peerid,buf,len);
    return 0;
}

int gm_reggs(uint64_t peerid,gs_t *gs)
{
    int gsid = gs->gsid;
    void *entry_key,*val;

    int rv = 0;
    thread_mutex_lock(gm->mutex);
    val = hash_get(gm->gs2peer,&gsid,sizeof(int),&entry_key); 
    if(!val)
    {
	//set peerid->gs
	const uint32_t hexidsize = 64;
	unsigned char *hexpeerid = mmalloc(gm->allocator,hexidsize);
	uuid2hex(peerid,hexpeerid,hexidsize);
	hash_set(gm->peer2gs,hexpeerid,HASH_KEY_STRING,gs);

	//set gs->peerid
        hexpeerid = mmalloc(gm->allocator,hexidsize);
	uuid2hex(peerid,hexpeerid,hexidsize);
	int *pgsid = mmalloc(gm->allocator,sizeof(int));
	memcpy(pgsid,&gsid,sizeof(gsid));
	hash_set(gm->gs2peer,pgsid,sizeof(int),hexpeerid);

	//set gs-scene->peerid
	int i = 0;
	for(i = 0; i < gs->nscenes; ++i)
	{
	    char *sceneid = mmalloc(gm->allocator,64);
	    snprintf(sceneid,64,"%d-%d",gsid,gs->scenes[i]);
	    hexpeerid = mmalloc(gm->allocator,hexidsize);
	    uuid2hex(peerid,hexpeerid,hexidsize);
	    hash_set(gm->scene2peer,sceneid,HASH_KEY_STRING,hexpeerid);
	}
	rv = 0;
    }
    else{
	rv = -1;
    }
    thread_mutex_unlock(gm->mutex);
    return rv;
}

int gm_getpidbyid(int gsid,uint64_t *peerid)
{
    void *entry_key,*val;

    int rv = 0;
    thread_mutex_lock(gm->mutex);
    val = hash_get(gm->gs2peer,&gsid,sizeof(int),&entry_key); 
    if(!val)
    {
	rv = -1;
    }
    else{
	unsigned char *pid = (unsigned char *)val;
	hex2uuid(pid,peerid);
	rv = 0;
    }
    thread_mutex_unlock(gm->mutex);
    return rv;
}

int gm_unreggs(uint64_t peerid)
{
    unsigned char hexpeerid[64] = {'\0'};
    uuid2hex(peerid,hexpeerid,sizeof(hexpeerid));

    void *entry_key,*val;

    int rv = 0;
    thread_mutex_lock(gm->mutex);
    val = hash_get(gm->peer2gs,hexpeerid,HASH_KEY_STRING,&entry_key); 
    if(!val)
    {
	rv = -1;
    }
    else
    {
	void *p2gs_key = entry_key;
	void *p2gs_val = val;

	gs_t *gs = (gs_t*)val;
	int gsid = gs->gsid;

	val = hash_get(gm->gs2peer,&gsid,sizeof(int),&entry_key); 
	if(val)
	{
	    hash_set(gm->gs2peer,&gsid,sizeof(int),NULL);
	    mfree(gm->allocator,entry_key);
	    mfree(gm->allocator,val);

	    int i = 0;
	    char sceneid[64]={0};
	    for(i = 0; i < gs->nscenes; ++i)
	    {
		snprintf(sceneid,sizeof(sceneid),"%d-%d",gsid,gs->scenes[i]);
		val = hash_get(gm->scene2peer,sceneid,HASH_KEY_STRING,&entry_key); 
		if(val)
		{
		    hash_set(gm->scene2peer,sceneid,HASH_KEY_STRING,NULL);
		    mfree(gm->allocator,entry_key);
		    mfree(gm->allocator,val);
		}
	    }
	}

	hash_set(gm->peer2gs,hexpeerid,HASH_KEY_STRING,NULL);
	mfree(gm->allocator,p2gs_key);
	mfree(gm->allocator,p2gs_val);
    }
    thread_mutex_unlock(gm->mutex);
    return rv;
}

int gm_getgs(uint64_t *peerid)
{
    thread_mutex_lock(gm->mutex);
    unsigned int count = hash_count(gm->peer2gs);
    if(count == 0)
    {
	thread_mutex_unlock(gm->mutex);
	return -1;
    }
    srandom(time(NULL));
    int idx = random() % count;
    hash_index_t *hi;
    void *key,*val;
    int i = 0,rv = 0;
    for (hi = hash_first(gm->peer2gs); hi && i != idx ; hi = hash_next(hi));

    if(hi)
    {
	hash_this(hi,(const void **)&key,NULL,(void **)&val); 
	unsigned char *pid = (unsigned char *)key;
	hex2uuid(pid,peerid);
	rv = 0;
    }
    else
    {
	rv = -1;
    }

    thread_mutex_unlock(gm->mutex);
    return rv;
}

void* gm_malloc(uint32_t size)
{
    return mmalloc(gm->allocator,size);
}

int gm_free(void *memory)
{
    mfree(gm->allocator,memory);
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
    hash_index_t *hi;
    void *key,*val;
    for (hi = hash_first(gm->peer2gs); hi ; hi = hash_next(hi))
    {
	hash_this(hi,(const void **)&key,NULL,(void **)&val); 
	mfree(gm->allocator,key);
	mfree(gm->allocator,val);
    }
    for (hi = hash_first(gm->gs2peer); hi ; hi = hash_next(hi))
    {
	hash_this(hi,(const void **)&key,NULL,(void **)&val); 
	mfree(gm->allocator,key);
	mfree(gm->allocator,val);
    }
    for (hi = hash_first(gm->scene2peer); hi ; hi = hash_next(hi))
    {
	hash_this(hi,(const void **)&key,NULL,(void **)&val); 
	mfree(gm->allocator,key);
	mfree(gm->allocator,val);
    }

    hash_destroy(gm->peer2gs);
    hash_destroy(gm->gs2peer);
    hash_destroy(gm->scene2peer);

    thread_mutex_destroy(gm->mutex);

    thread_mutex_destroy(gm->mutex_mpool);
    allocator_destroy(gm->allocator);

    free(gm);
    gm = NULL;
    return 0;
}
