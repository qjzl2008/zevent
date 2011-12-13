#include <stdio.h>
#include <string.h>
#include "cm_logic.h"
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
    snprintf(buf+4,sizeof(buf),"{\"cmd\":%d,\"msg\":{\"cmd\":%d,\"cid\":\"%s\",\
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
