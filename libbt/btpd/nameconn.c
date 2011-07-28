#include "btpd.h"
#include "Ws2tcpip.h"

struct nameconn {
    struct fdev write_ev;
    void (*cb)(void *, int, SOCKET);
    void *arg;
    aictx_t ai_handle;
    struct addrinfo *ai_res, *ai_cur;
    SOCKET sd;
    enum { NC_AI, NC_CONN } state;
};

static void nc_connect(struct nameconn *nc);

static void
nc_free(struct nameconn *nc)
{
    if (nc->ai_res != NULL)
        freeaddrinfo(nc->ai_res);
    free(nc);
}

static void
nc_done(struct nameconn *nc, int error)
{
    nc->cb(nc->arg, error, nc->sd);
    nc_free(nc);
}

static void
nc_write_cb(int sd, short type, void *arg)
{
    struct nameconn *nc = arg;
    int error;
    socklen_t errsiz = sizeof(int);
    if (getsockopt(nc->sd, SOL_SOCKET, SO_ERROR, &error, &errsiz) < 0)
        btpd_err("getsockopt error (%s).\r\n", strerror(errno));
    if (error == 0) {
        btpd_ev_del(&nc->write_ev);
        nc_done(nc, 0);
    } else {
        btpd_ev_del(&nc->write_ev);
        closesocket(nc->sd);
        nc->ai_cur = nc->ai_cur->ai_next;
        nc_connect(nc);
    }
}

static void
nc_connect(struct nameconn *nc)
{
    struct addrinfo *ai;
    int err;

again:
    if ((ai = nc->ai_cur) == NULL) {
        nc_done(nc, -1);
        return;
    }

    if ((nc->sd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol)) < 0)
        btpd_err("Failed to create socket (%s).\r\n", strerror(errno));

    set_nonblocking(nc->sd);

    err = connect(nc->sd, ai->ai_addr, ai->ai_addrlen);
    if (err == 0)
        nc_done(nc, 0);
    else if (err == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
        btpd_ev_new(&nc->write_ev, nc->sd, EV_WRITE, nc_write_cb, nc);
    else {
        closesocket(nc->sd);
        nc->ai_cur = ai->ai_next;
        goto again;
    }
}

static void
nc_ai_cb(void *arg, int error, struct addrinfo *ai)
{
    struct nameconn *nc = arg;
    if (error == 0) {
        nc->ai_cur = nc->ai_res = ai;
        nc->state = NC_CONN;
        nc_connect(nc);
    } else
        nc_done(nc, error);
}

nameconn_t
btpd_name_connect(const char *name, short port, void (*cb)(void *, int, SOCKET),
    void *arg)
{
    struct addrinfo hints;
    struct nameconn *nc = btpd_calloc(1, sizeof(*nc));
    nc->cb = cb;
    nc->arg = arg;
    nc->sd = -1;
    memset(&hints,0, sizeof(hints));
    hints.ai_flags = AI_ADDRCONFIG;
    hints.ai_family = net_af_spec();
    hints.ai_socktype = SOCK_STREAM;
    nc->ai_handle = btpd_addrinfo(name, port, &hints, nc_ai_cb, nc);
    return nc;
}

void
btpd_name_connect_cancel(struct nameconn *nc)
{
    if (nc->state == NC_AI)
        btpd_addrinfo_cancel(nc->ai_handle);
    else {
        btpd_ev_del(&nc->write_ev);
        closesocket(nc->sd);
    }
    nc_free(nc);
}
