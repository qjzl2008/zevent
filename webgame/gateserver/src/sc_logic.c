#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <pthread.h>

#include "sc_logic.h"
#include "client_manager.h"
#include "json_helper.h"
#include "netcmd.h"
#include "uuidservice.h"

int sc_logic_createaccount(json_object *jmsg)
{
    json_object *jcode = json_util_get(jmsg,"code");
    if(!jcode)
	return -1;
    int code = json_object_get_int(jcode);
    
    json_object *jcid = json_util_get(jmsg,"msg.cid");
    if(!jcid)
	return -1;
    const char *hexcid = json_object_get_string(jcid);
    uint64_t cid;
    hex2uuid((unsigned char *)hexcid,&cid);

    char buf[256]={'\0'};
    if(code != DEF_MSGTYPE_CONFIRM)
    {
	snprintf(buf+4,sizeof(buf)-4,"{\"cmd\":%d,\"code\":%d}",MSGID_RESPONSE_NEWACCOUNT,
		code);
    }
    else
    {
	json_object *jsqlrv = json_util_get(jmsg,"msg.sqlout.@rv");
	if(!jsqlrv)
	    return -1;
	int rv = json_object_get_int(jsqlrv);
	if(rv != 0)
	{
	    snprintf(buf+4,sizeof(buf)-4,"{\"cmd\":%d,\"code\":%d}",MSGID_RESPONSE_NEWACCOUNT,
		    rv);
	}
	else
	{
	    snprintf(buf+4,sizeof(buf)-4,"{\"cmd\":%d,\"code\":%d}",MSGID_RESPONSE_NEWACCOUNT,
		    rv);
	}
    }
    int len = strlen(buf+4);
    int nlen = htonl(len);
    memcpy(buf,&nlen,sizeof(nlen));
    cm_send2client(cid,buf,len+4);
    return 0;
}
