#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "client_manager.h"
#include "cm_logic.h"
#include "gs_manager.h"
#include "store_client.h"
#include "uuidservice.h"
#include "netcmd.h"
#include "json_helper.h"

int cm_logic_createaccount(uint64_t peerid,json_object *jmsg)
{
    uint64_t uuid;
    int rv = gen_uuid_local(UUID_TYPE_ACCOUNT, &uuid);
    if(rv != 0)
	return -1;
    unsigned char hexid[64];
    uuid2hex(peerid,hexid,sizeof(hexid));

    char sql[256]={'\0'};
    json_object *jname = json_util_get(jmsg,"name");
    if(!jname)
	return -1;
    const char *name = json_object_get_string(jname);

    json_object *jmail = json_util_get(jmsg,"mail");
    if(!jmail)
	return -1;
    const char *mail = json_object_get_string(jmail);

    json_object *jpwd = json_util_get(jmsg,"pwd");
    if(!jpwd)
	return -1;
    const char *pwd = json_object_get_string(jpwd);

    snprintf(sql,sizeof(sql),"call create_account(%llu,'%s','%s','%s',@rv)",uuid,
	    name,mail,pwd);

    int cmd1 = MSGID_REQUEST_EXECPROC;
    int cmd2 = MSGID_REQUEST_NEWACCOUNT;
    char buf[1024] = {'\0'};
    snprintf(buf+4,sizeof(buf)-4,"{\"cmd\":%d,\"msg\":{\"cmd\":%d,\"cid\":\"%s\",\
	    \"sql\":\"%s\",\
	    \"sqlout\":[\"@rv\"]}}",cmd1,cmd2,hexid,
			   sql);
    int len = strlen(buf+4);
    int nlen = htonl(len);
    memcpy(buf,&nlen,sizeof(nlen));

    rv = sc_send2store(buf,len+sizeof(len));
    if(rv < 0)
    {
	return -1;
    }
    return 0;
}

int cm_logic_login(uint64_t peerid,json_object *jmsg)
{
    unsigned char hexid[64];
    uuid2hex(peerid,hexid,sizeof(hexid));
    char sql[256]={'\0'};
    json_object *jname = json_util_get(jmsg,"cnm");
    if(!jname)
	return -1;
    const char *name = json_object_get_string(jname);

    json_object *jpwd = json_util_get(jmsg,"pwd");
    if(!jpwd)
	return -1;
    const char *pwd = json_object_get_string(jpwd);

    snprintf(sql,sizeof(sql),"call login('%s','%s',@rv,@accountid)",
	    name,pwd);

    int cmd1 = MSGID_REQUEST_EXECPROC;
    int cmd2 = MSGID_REQUEST_LOGIN;
    char buf[1024] = {'\0'};
    snprintf(buf+4,sizeof(buf)-4,"{\"cmd\":%d,\"msg\":{\"cmd\":%d,\"cid\":\"%s\",\
	    \"sql\":\"%s\",\
	    \"sqlout\":[\"@rv\",\"@accountid\"]}}",cmd1,cmd2,hexid,
			   sql);
    int len = strlen(buf+4);
    int nlen = htonl(len);
    memcpy(buf,&nlen,sizeof(nlen));

    int rv = sc_send2store(buf,len+sizeof(len));
    if(rv < 0)
    {
	return -1;
    }
    return 0;
}

static int cm_logic_sendres(uint64_t peerid,int cmd,int code)
{
    char buf[256]={'\0'};
    snprintf(buf+4,sizeof(buf)-4,"{\"cmd\":%d,\"code\":%d}",cmd,code);
    int len = strlen(buf+4);
    int nlen = htonl(len);
    memcpy(buf,&nlen,sizeof(nlen));

    int rv = cm_send2client(peerid,buf,len+sizeof(len));
    if(rv < 0)
    {
	return -1;
    }
    return 0;
}

int cm_logic_bindgs(uint64_t peerid,json_object *jmsg)
{
    json_object *jgsid = json_util_get(jmsg,"gsid");
    if(!jgsid)
	return -1;
    int gsid = json_object_get_int(jgsid);

    uint64_t gspeerid;
    int rv = gm_getpidbyid(gsid,&gspeerid);
    if(rv < 0)
    {
	cm_logic_sendres(peerid,MSGID_RESPONSE_BINDGS,DEF_MSGTYPE_REJECT);
	return -1;
    }
    rv = cm_bindgs(peerid,gspeerid);
    if(rv < 0)
    {
	cm_logic_sendres(peerid,MSGID_RESPONSE_BINDGS,DEF_MSGTYPE_REJECT);
	return -1;
    }
    else
    {
	cm_logic_sendres(peerid,MSGID_RESPONSE_BINDGS,DEF_MSGTYPE_CONFIRM);
    }
    return 0;
}

int cm_logic_data2gs(uint64_t peerid,json_object *jmsg)
{
    uint64_t gspeerid,accountid;
    int rv = cm_getidbycid(peerid,&gspeerid,&accountid);
    if(rv < 0 || gspeerid == 0)
    {
	cm_logic_sendres(peerid,MSGID_RESPONSE_DATA2GS,DEF_MSGTYPE_REJECT);
	return -1;
    }
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
	    json_object *jmsg = json_util_get(obj,"msg");
	    if(!jmsg)
		return -1;
	    unsigned char hexid[64];
	    uuid2hex(peerid,hexid,sizeof(hexid));
	    json_object_object_add(jmsg,"peerid",json_object_new_string((const char *)hexid));

	    uuid2hex(accountid,hexid,sizeof(hexid));
	    json_object_object_add(jmsg,"accountid",json_object_new_string((const char *)hexid));
	    const char *msg = json_object_get_string(jmsg);
	    int len = strlen(msg);
	    int nlen = htonl(len);
	    char *buf = (char *)malloc(len+4);
	    memcpy(buf,&nlen,sizeof(nlen));
	    memcpy(buf+4,msg,len);

	    gm_send2gs(gspeerid,(void *)buf,len+4);
	    free(buf);

	}
    }

    return 0;
}
