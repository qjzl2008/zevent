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

int main()
{
	int rv;
	net_server_t *ns;
	ns_arg_t sinfo;
	sinfo.func = NULL;
	strcpy(sinfo.ip,"127.0.0.1");
	sinfo.port = 8899;
	sinfo.max_peers = 1000;
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
	    rv = ns_recvmsg(ns,&msg,&len,&peer_id);
	    if(rv == 0)
	    {
		pthread_mutex_lock(&mutex);
		void *page = mpool_new(mp,&pgno);
		memcpy(page,msg,len);
		mpool_put(mp,page,MPOOL_DIRTY);
		pthread_mutex_unlock(&mutex);

		memcpy(buf,(char *)msg,len);
		ns_sendmsg(ns,peer_id,buf,len);
		//ns_disconnect(ns,peer_id);
		++count;
		printf("count:%d,%u\n",count,time(NULL));
		ns_free(ns,msg);
	    }
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
