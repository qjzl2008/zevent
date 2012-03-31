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

static int count = 0;
static int msg_process_func(void *net,uint64_t peerid, void *buf,uint32_t len)
{
    net_client_t *nc = (net_client_t*)net;
    nc_sendmsg(nc,buf,len);
    ++count;
    if(count % 100000 == 0)
	printf("count:%d,tm:%u\n",count,time(NULL));
    return 0;
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
    cinfo.data_func = NULL;
    cinfo.msg_func = msg_process_func;
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
    while(!stop_daemon /*&& count < 100000*/)
    {
	struct timeval outtime;
	outtime.tv_sec=0;
	outtime.tv_usec=100000;
	select(0,NULL,NULL,NULL,&outtime);   
//	sleep(1);
//	rv = nc_recvmsg(nc,&msg,&len,1000000);
//	//rv = nc_tryrecvmsg(nc,&msg,&len);
//	if(rv == 0)
//	{
//	    memcpy(buf,(char *)msg,len);
	    rv = nc_sendmsg(nc,buf,len);
	    //printf("rv:%d\n",rv);
//	    //nc_disconnect(nc);
//	    ++count;
//	    //printf("count:%d,time:%u,data:%s\n",count,time(NULL),(char*)msg+4);
//	    nc_free(nc,msg);
//	}
    }
    nc_disconnect(nc);
    return 0;
}
