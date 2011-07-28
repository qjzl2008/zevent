#include "btpd.h"

#include "thread_mutex.h"
#include "thread_cond.h"
#include "arch_thread_mutex.h"
#include "arch_thread_cond.h"

struct ai_ctx {
    BTPDQ_ENTRY(ai_ctx) entry;
    struct addrinfo hints;
    struct addrinfo *res;
    char node[255], service[6];
    void (*cb)(void *, int, struct addrinfo *);
    void *arg;
    int cancel;
    int error;
    uint16_t port;
};

BTPDQ_HEAD(ai_ctx_tq, ai_ctx);

static struct ai_ctx_tq m_aiq = BTPDQ_HEAD_INITIALIZER(m_aiq);
static struct thread_mutex_t *m_aiq_lock;
static struct thread_cond_t *m_aiq_cond;

static HANDLE td;

struct ai_ctx *
btpd_addrinfo(const char *node, uint16_t port, struct addrinfo *hints,
    void (*cb)(void *, int, struct addrinfo *), void *arg)
{
    struct ai_ctx *ctx = btpd_calloc(1, sizeof(*ctx));
    ctx->hints = *hints;
    ctx->cb = cb;
    ctx->arg = arg;    
    sprintf(ctx->node, "%s", node);
    ctx->port = port;
    sprintf(ctx->service, "%hu", port);

    thread_mutex_lock(m_aiq_lock);
    BTPDQ_INSERT_TAIL(&m_aiq, ctx, entry);
    thread_mutex_unlock(m_aiq_lock);
    thread_cond_signal(m_aiq_cond);

    return ctx;
}

void
btpd_addrinfo_cancel(struct ai_ctx *ctx)
{
    ctx->cancel = 1;
}

void btpd_addrinfo_stop()
{
    thread_cond_signal(m_aiq_cond);
    WaitForSingleObject(td,1000);
    CloseHandle(td);
    td_fini();
}

static void
addrinfo_td_cb(void *arg)
{
    struct ai_ctx *ctx = arg;
    if (!ctx->cancel)
        ctx->cb(ctx->arg, ctx->error, ctx->res);
    else if (ctx->error != 0)
        freeaddrinfo(ctx->res);
    free(ctx);
}

static DWORD WINAPI
addrinfo_td(void *arg)
{
    struct ai_ctx *ctx;
    while (!daemon_stop) {
        thread_mutex_lock(m_aiq_lock);
        while (!daemon_stop && BTPDQ_EMPTY(&m_aiq))
            thread_cond_wait(m_aiq_cond, m_aiq_lock);
	if(BTPDQ_EMPTY(&m_aiq))
	{
	    break;
	}
        ctx = BTPDQ_FIRST(&m_aiq);
        BTPDQ_REMOVE(&m_aiq, ctx, entry);
        thread_mutex_unlock(m_aiq_lock);

        ctx->error =
            getaddrinfo(ctx->node, ctx->service, &ctx->hints, &ctx->res);

        td_post_begin();
        td_post(addrinfo_td_cb, ctx);
        td_post_end();
    }
    return 0;
}

static void
errdie(int err, const char *str)
{
    if (err != 0)
        btpd_err("addrinfo_init: %s (%s).\r\n", str, strerror(errno));
}

void
addrinfo_init(void)
{
    errdie(thread_mutex_create(&m_aiq_lock, THREAD_MUTEX_DEFAULT), "thread_mutex_init");
    errdie(thread_cond_create(&m_aiq_cond), "thread_cond_init");
    td = CreateThread(NULL, 0, addrinfo_td, NULL,0,NULL);
}
