// znetclient.cpp : 定义控制台应用程序的入口点。
//

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
	nc_arg_t nc_arg;
	strcpy_s(nc_arg.ip,sizeof(nc_arg.ip),"127.0.0.1");
	nc_arg.port = 8090;
	nc_arg.func = NULL;
	nc_arg.timeout = 3;

	net_client_t *nc;
	int rv = nc_connect(&nc,&nc_arg);
	if(rv < 0)
		return -1;
	//send echo msg
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
	while(1)
	{
		rv = nc_recvmsg(nc,&msg,&len,1000000);
		//rv = nc_tryrecvmsg(nc,&msg,&len);
		if(rv == 0)
		{
			memcpy(buf,(char *)msg,len);
			nc_sendmsg(nc,buf,len);
			//nc_disconnect(nc);
			++count;
			printf("client:msg:%s,count:%d\n",(char*)msg + 4,count);
			nc_free(nc,msg);
		}

		if(_kbhit())
		{
			if((key =_getch()) == 115/*s key*/)
				break;
		}
		Sleep(100);
	}
	nc_disconnect(nc);
	return 0;
}

