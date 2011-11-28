#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>
#include "znet.h"

static int stop_daemon = 0;

static void handler(int sig)
{
    if(sig == SIGINT)
    {
	stop_daemon = 1;
    }
}

int main(void)
{
    //process signal
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = handler;
    sigaction(SIGINT, &sa, NULL);

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
    while(!stop_daemon && count < 100000)
    {
	rv = nc_recvmsg(nc,&msg,&len,1000000);
	//rv = nc_tryrecvmsg(nc,&msg,&len);
	if(rv == 0)
	{
	    memcpy(buf,(char *)msg,len);
	    nc_sendmsg(nc,buf,len);
	    //nc_disconnect(nc);
	    ++count;
	    printf("count:%d,%u\n",count,time(NULL));
	    nc_free(nc,msg);
	}
    }
    nc_disconnect(nc);
    return 0;
}
