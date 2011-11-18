#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>
#include "znet.h"

int main(void)
{
    net_client_t *nc;
    nc_arg_t cinfo;
    cinfo.func = NULL;
    strcpy(cinfo.ip,"127.0.0.1");
    cinfo.port = 8899;
    cinfo.timeout = 10;
    int rv = nc_connect(&nc,&cinfo);
    if(rv < 0)
    {
	printf("Connect to server failed!ip:%s,port:%d\n",cinfo.ip,cinfo.port);
	return -1;
    }
    int stop_daemon = 0;

    void *msg;uint32_t len;
    char buf[64];
    memset(buf,0,sizeof(buf));
    char *str = "hello baby!";
    len = strlen(str);
    int nlen = htonl(len);
    memcpy(buf,&nlen,sizeof(nlen));
    memcpy(buf+sizeof(len),str,len);
    len+= sizeof(len);
    int count = 0;

    rv = nc_sendmsg(nc,buf,len);
    while(!stop_daemon)
    {
	rv = nc_recvmsg(nc,&msg,&len);
	if(rv == 0)
	{
	    memcpy(buf,(char *)msg,len);
	    nc_sendmsg(nc,buf,len);
	    //ns_disconnect(ns,peer_id);
	    ++count;
	    printf("count:%d,%u\n",count,time(NULL));
	    nc_free(nc,msg);
	}
    }
    nc_disconnect(nc);
    return 0;
}
