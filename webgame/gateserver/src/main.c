#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <stdio.h>
#include "znet.h"
#include "store_client.h"
#include "client_manager.h"
#include "gs_manager.h"
#include "uuidservice.h"

static int stop_daemon = 0;

static void handler(int sig)
{
    if(sig == SIGINT)
    {
	stop_daemon = 1;
    }
}

static int daemonize()
{
    int maxfd, fd;
    switch (fork()) {
	case -1: return -1;
	case 0: break;
	default: _exit(0);
    }
    if (setsid() == -1)
	return -1;
    switch (fork()) {
	case -1: return -1;
	case 0: break;
	default: _exit(0);
    }
    umask(0); 
    maxfd = sysconf(_SC_OPEN_MAX);
    if (maxfd == -1)
	maxfd = 8192;
    for (fd = 0; fd < maxfd; fd++)
	close(fd);

    close(STDIN_FILENO);
    fd = open("/dev/null", O_RDWR);
    if (fd != STDIN_FILENO)
	return -1;
    if (dup2(STDIN_FILENO, STDOUT_FILENO) != STDOUT_FILENO)
	return -1;
    if (dup2(STDIN_FILENO, STDERR_FILENO) != STDERR_FILENO)
	return -1;
    return 0;
}

int main()
{
    //daemonize();
    //process signal
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = handler;
    sigaction(SIGINT, &sa, NULL);

    uuid_init();
    sc_start();
    gm_start();
    cm_start();

    struct timeval delay;
    while(!stop_daemon)
    {
	delay.tv_sec = 1;
	delay.tv_usec = 0;

	int rv = select(0,NULL,NULL,NULL,&delay); 
	if(rv == 0)
	{
	    continue;
	}
    }
    cm_stop();
    gm_stop();
    sc_stop();
    cm_destroy();
    gm_destroy();
    sc_destroy();

    uuid_fini();
    printf("Normal exit!\n");
    return 0;
}
