#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "new_mysql_pool.h"

int main(void)
{
	svr_cfg cfg;
	strcpy(cfg.host,"127.0.0.1");
	strcpy(cfg.user,"root");
	strcpy(cfg.db,"test");
	strcpy(cfg.charset,"utf8");
	cfg.port = 3006;
	thread_mutex_t *thread_mutex = NULL;
	thread_mutex_create(&thread_mutex,THREAD_MUTEX_DEFAULT);
	cfg.mutex = thread_mutex;
	cfg.nmin = 5;
	cfg.nkeep = 10;
	cfg.nmax = 15;
	cfg.exptime = 10;
	cfg.timeout = 1000;

	int rv = mysql_pool_init(&cfg);
	printf("rv:%d\n",rv);
	//	sleep(10);
	mysql_pool_fini(&cfg);
	thread_mutex_destroy(thread_mutex);
	return 0;
}
