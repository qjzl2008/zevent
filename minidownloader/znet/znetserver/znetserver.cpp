// testserver.cpp : 定义控制台应用程序的入口点。
#include "stdafx.h"
#include <conio.h>
extern "C"
{
#include "../znet.h"
};
#include <windows.h>
#include <stdlib.h>
#include <string.h>


int _tmain(int argc, _TCHAR* argv[])
{
	int key;
	ns_arg_t ns_arg;
	ns_arg.max_peers = 1000;
	strcpy_s(ns_arg.ip,sizeof(ns_arg.ip),"127.0.0.1");
	ns_arg.port = 0;
	ns_arg.func = NULL;

	net_server_t *ns;
	int rv = ns_start_daemon(&ns,&ns_arg);
	if(rv < 0)
		return -1;

	void *msg;uint32_t len;
	char buf[64];
	memset(buf,0,sizeof(buf));
	int count = 0;
	uint64_t peer_id;
	while(1)
	{
		rv = ns_recvmsg(ns,&msg,&len,&peer_id,1000000);
		//	rv = ns_tryrecvmsg(ns,&msg,&len,&peer_id);
		if(rv == 0)
		{
			memcpy(buf,(char *)msg,len);
			ns_sendmsg(ns,peer_id,buf,len);
			//ns_disconnect(ns,peer_id);
			++count;
			printf("server:msg:%s,count:%d\n",(char *)msg+4,count);
			ns_free(ns,msg);
		}
		if(_kbhit())
		{
			if((key =_getch()) == 115/*s key*/)
				break;
		}
		//Sleep(1000);
	}
	ns_stop_daemon(ns);
	return 0;
}

