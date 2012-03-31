#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>
#include "znet.h"
#include "mpool.h"

pthread_mutex_t mutex;
static int stop_daemon = 0;

static void handler(int sig)
{
    if(sig == SIGINT)
    {
	stop_daemon = 1;
    }
}

struct thread_info {    /* Used as argument to thread_start() */
    void *mpool;
};

static void *thread_start(void *arg)
{
    struct thread_info *tinfo = (struct thread_info *) arg;

    struct timeval delay;
    delay.tv_sec = 30;
    delay.tv_usec = 0;//10000;//10ms
    while(!stop_daemon)
    {
	delay.tv_sec = 30;
	delay.tv_usec = 0;//10000;//10ms

	int rv = select(0,NULL,NULL,NULL,&delay); 
	if(rv == 0)
	{
	    pthread_mutex_lock(&mutex);
	    mpool_sync(tinfo->mpool);
	    pthread_mutex_unlock(&mutex);
	}
    }
    printf("Check Data Thread %d exit!\n",getpid());
    return NULL;
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

static uint32_t count = 0;
static int msg_process_func(void *net,uint64_t peerid, void *buf,uint32_t len)
{
    ++count;
    if(count % 10000 == 0)
	printf("count:%u,time:%u\n",count,time(NULL));

    net_server_t *ns = (net_server_t*)net;
    ns_sendmsg(ns,peerid,buf,len);
    return 0;
}

int main()
{
//    daemonize();
    int rv;
    net_server_t *ns;
    ns_arg_t sinfo;
    sinfo.data_func = NULL;
    strcpy(sinfo.ip,"127.0.0.1");
    sinfo.port = 8899;
    sinfo.max_peers = 1000;
    sinfo.msg_func = msg_process_func;
    ns_start_daemon(&ns,&sinfo);

    void *msg;uint32_t len;
    char buf[64];
    memset(buf,0,sizeof(buf));
    int count = 0;

    //init mem db
    int fd = open("msg.mdb",O_CREAT|O_RDWR|O_LARGEFILE,0600);
    MPOOL *mp = mpool_open(NULL,fd,3000,2000);
    mpool_stat(mp);
    pgno_t pgno;

    //process signal
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = handler;
    sigaction(SIGINT, &sa, NULL);

    //start check-thread
    struct thread_info *tinfo;
    tinfo = calloc(1, sizeof(struct thread_info));
    tinfo->mpool = mp;

    pthread_t thread_id;
    pthread_mutex_init(&mutex, NULL);
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_create(&thread_id, &attr, thread_start, tinfo);
    pthread_attr_destroy(&attr);

    uint64_t peer_id;

    while(!stop_daemon)
    {
//	rv = ns_recvmsg(ns,&msg,&len,&peer_id,1000000);
//	rv = ns_tryrecvmsg(ns,&msg,&len,&peer_id);
//	if(rv == 0)
//	{
/*	    pthread_mutex_lock(&mutex);
	    void *page = mpool_new(mp,&pgno);
	    memcpy(page,msg,len);
	    mpool_put(mp,page,MPOOL_DIRTY);
	    pthread_mutex_unlock(&mutex);
*/
//	    memcpy(buf,(char *)msg,len);
//	    ns_sendmsg(ns,peer_id,buf,len);
	    //ns_disconnect(ns,peer_id);
	 //++count;
	 //printf("count:%d,%u\n",count,time(NULL));
//	    ns_free(ns,msg);
//	}
	//printf("time:%d\n",time(NULL));
	sleep(1);
    }
    ns_stop_daemon(ns);
    //tell check data thread to exit
    pthread_kill(thread_id,SIGINT);
    pthread_join(thread_id,NULL);
    pthread_mutex_destroy(&mutex);
    mpool_close(mp);
    printf("Normal exit!\n");
    return 0;
}
