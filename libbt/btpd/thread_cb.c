#include <sys/types.h>

//#include <pthread.h>
#include <string.h>

#include "btpd.h"

struct td_cb {
    void (*cb)(void *);
    void *arg;
    BTPDQ_ENTRY(td_cb) entry;
};

BTPDQ_HEAD(td_cb_tq, td_cb);

static SOCKET td_sd, m_td_rd, m_td_wr;
static struct fdev m_td_ev;
static struct td_cb_tq m_td_cbs = BTPDQ_HEAD_INITIALIZER(m_td_cbs);
static CRITICAL_SECTION cs;

void
td_acquire_lock(void)
{
    EnterCriticalSection(&cs);
}

void
td_release_lock(void)
{
    LeaveCriticalSection(&cs);
}

void
td_post(void (*fun)(void *), void *arg)
{
    struct td_cb *cb = btpd_calloc(1, sizeof(*cb));
    cb->cb = fun;
    cb->arg = arg;
    BTPDQ_INSERT_TAIL(&m_td_cbs, cb, entry);
}

void
td_post_end(void)
{
    char c = '1';
    td_release_lock();
    send(m_td_wr, &c, sizeof(c),0);
}

static void
td_cb(SOCKET fd, short type, void *arg)
{
    char buf[1024];
    struct td_cb_tq tmpq =  BTPDQ_HEAD_INITIALIZER(tmpq);
    struct td_cb *cb, *next;

    recv(fd, buf, sizeof(buf),0);
    td_acquire_lock();
    BTPDQ_FOREACH_MUTABLE(cb, &m_td_cbs, entry, next)
        BTPDQ_INSERT_TAIL(&tmpq, cb, entry);
    BTPDQ_INIT(&m_td_cbs);
    td_release_lock();

    BTPDQ_FOREACH_MUTABLE(cb, &tmpq, entry, next) {
        cb->cb(cb->arg);
        free(cb);
    }
}

extern void client_connection_cb(SOCKET sd, short type, void *arg);
extern int pipe_port;

int
td_init(void)
{
    SOCKET sd,cd,nsd;
    struct sockaddr_in addr;
    size_t psiz = sizeof(addr.sin_addr);

    InitializeCriticalSection(&cs);

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(pipe_port);

    if((sd = socket(AF_INET,SOCK_STREAM,0)) < 0)
    {
	btpd_err("socket:%s\r\n",strerror(errno));
	return -1;
    }
    if(bind(sd, (struct sockaddr *)&addr,sizeof(addr)) != 0) {
	btpd_err("bind: %s\r\n",strerror(errno));
	return -1;
    }
    
    listen(sd,4);
    td_sd = sd;
    //set_nonblocking(sd);
    
    if((cd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == SOCKET_ERROR)
	return -1;

    if(connect(cd,(struct sockaddr *)&addr,sizeof(addr)) == SOCKET_ERROR) {
	closesocket(cd);
	return -1;
    }

    if((nsd = accept(sd, NULL, NULL)) == INVALID_SOCKET) {
	btpd_err("client accept failed.\r\n");
    }

    if((set_blocking(nsd)) != 0)
	btpd_err("set_blocking failed.\r\n");
    
    m_td_wr = nsd;
    m_td_rd = cd;

    btpd_ev_new(&m_td_ev, m_td_rd, EV_READ, td_cb, NULL);
    return 0;
}

int td_fini(void)
{
    btpd_ev_del(&m_td_ev);
    closesocket(m_td_rd);
    closesocket(m_td_wr);
    closesocket(td_sd);
    return 0;
}
