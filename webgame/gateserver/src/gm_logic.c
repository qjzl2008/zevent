#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "client_manager.h"
#include "gs_manager.h"
#include "gm_logic.h"
#include "store_client.h"
#include "uuidservice.h"
#include "netcmd.h"
#include "json_helper.h"

static int gm_logic_sendres(uint64_t peerid,int cmd,int code)
{
    char buf[256]={'\0'};
    snprintf(buf+4,sizeof(buf)-4,"{\"cmd\":%d,\"code\":%d}",cmd,code);
    int len = strlen(buf+4);
    int nlen = htonl(len);
    memcpy(buf,&nlen,sizeof(nlen));

    int rv = gm_send2gs(peerid,buf,len+sizeof(len));
    if(rv < 0)
    {
	return -1;
    }
    return 0;
}

int gm_logic_reggs(uint64_t peerid,json_object *jmsg)
{
    json_object *jgsid = json_util_get(jmsg,"gsid");
    if(!jgsid)
	return -1;
    int gsid = json_object_get_int(jgsid);

    json_object *jscenes = json_util_get(jmsg,"scenes");
    if(!jscenes)
    {
	return -1;
    }
    int nsize = json_object_array_length(jscenes);
    if(nsize > MAX_SCENES_PERGS)
    {
	return -1;
    }

    gs_t *gs = (gs_t *)gm_malloc(sizeof(gs_t));
    memset(gs,0,sizeof(gs_t));
    gs->gsid = gsid;

    int i;
    for(i=0; i < nsize; i++)
    {
	json_object *jscene = json_object_array_get_idx(jscenes, i);
	if(jscene)
	{
	    int sceneid = json_object_get_int(jscene);
	    gs->scenes[i++] = sceneid;
	    ++gs->nscenes;
	}
    }

    int rv = gm_reggs(peerid,gs);
    if(rv < 0)
    {
	gm_logic_sendres(peerid,MSGID_RESPONSE_REGGS,DEF_MSGTYPE_REJECT);
	return -1;
    }
    else
    {
	gm_logic_sendres(peerid,MSGID_RESPONSE_REGGS,DEF_MSGTYPE_CONFIRM);
    }
    return 0;
}

int gm_logic_data2clients(uint64_t peerid,json_object *jmsg)
{
    json_object *jmsgs = json_util_get(jmsg,"msgs");
    if(!jmsgs)
    {
	return -1;
    }

    int i;
    for(i=0; i < json_object_array_length(jmsgs); i++)
    {
	json_object *obj = json_object_array_get_idx(jmsgs, i);
	if(obj)
	{
	    json_object *jpeerid = json_util_get(obj,"peerid");
	    if(!jpeerid)
		return -1;
	    const char *hexpeerid = json_object_get_string(jpeerid);
	    uint64_t peerid = 0;
	    hex2uuid((unsigned char *)hexpeerid,&peerid);
	    json_object *jdata = json_util_get(obj,"msg");
	    if(!jdata)
		return -1;
	    const char *data = json_object_get_string(jdata);

	    int len = strlen(data);
	    int nlen = htonl(len);
	    char *buf = (char *)gm_malloc((uint32_t)(len+4));
	    memcpy(buf,&nlen,sizeof(nlen));
	    memcpy(buf+4,data,len);

	    cm_send2client(peerid,(void *)buf,len+4);
	    gm_free(buf);
	}
    }

    return 0;
}
