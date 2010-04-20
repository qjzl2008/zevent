#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "conn_pool.h"

int main(void)
{
	conn_svr_cfg cfg;
	strcpy(cfg.host,"127.0.0.1");
	cfg.port = 6889;
	thread_mutex_t *thread_mutex = NULL;
	thread_mutex_create(&thread_mutex,THREAD_MUTEX_DEFAULT);
	cfg.mutex = thread_mutex;
	cfg.nmin = 5;
	cfg.nkeep = 10;
	cfg.nmax = 15;
	cfg.exptime = 10;
	cfg.timeout = 1000;

	conn_pool_init(&cfg);
	sleep(10);
	conn_pool_fini(&cfg);
	thread_mutex_destroy(thread_mutex);
	return 0;
}
