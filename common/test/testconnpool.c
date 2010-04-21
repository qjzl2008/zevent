#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "conn_pool.h"

#ifdef WIN32
#include <Windows.h>
#pragma comment(lib,"ws2_32.lib")
#endif

int main(void)
{
	thread_mutex_t *thread_mutex = NULL;
	conn_svr_cfg cfg;
#ifdef WIN32
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2,2),&wsaData);
#endif
	
#ifdef WIN32
	strcpy_s(cfg.host,sizeof(cfg.host),"127.0.0.1");
#else
	strcpy(cfg.host,"127.0.0.1");
#endif
	cfg.port = 1029;
	thread_mutex_create(&thread_mutex,THREAD_MUTEX_DEFAULT);
	cfg.mutex = thread_mutex;
	cfg.nmin = 5;
	cfg.nkeep = 10;
	cfg.nmax = 15;
	cfg.exptime = 10;
	cfg.timeout = 1000;

	conn_pool_init(&cfg);
#ifdef WIN32
//	Sleep(100);
#else
	sleep(10);
#endif
	conn_pool_fini(&cfg);
	thread_mutex_destroy(thread_mutex);
	return 0;
}
