#include "btpd.h"

#include <http_client.h>
#include <iobuf.h>

#define MAX_DOWNLOAD (1 << 18)  // 256kB

static const char *m_tr_events[] = { "started", "stopped", "completed", "" };

struct httptr_req {
    struct torrent *tp;
    struct tr_tier *tr;
    struct http_req *req;
    struct iobuf buf;
    struct fdev ioev;
    struct timeout timer;
    nameconn_t nc;
    SOCKET sd;
    enum tr_event event;
	char aurl[256];
};

static void
httptr_free(struct httptr_req *treq)
{
    if (treq->sd != -1) {
        btpd_ev_del(&treq->ioev);
        closesocket(treq->sd);
    }
    btpd_timer_del(&treq->timer);
    iobuf_free(&treq->buf);
    free(treq);
}

static void
maybe_connect_to(struct torrent *tp, const char *pinfo)
{
    const char *pid;
    char *ip;
    int port;
    size_t len;

    if ((pid = benc_dget_mem(pinfo, "peer id", &len)) == NULL || len != 20)
        return;

    if (memcmp(btpd_get_peer_id(), pid, 20) == 0)
        return;

    if (net_torrent_has_peer(tp->net, pid))
        return;

    if ((ip = benc_dget_str(pinfo, "ip", NULL)) == NULL)
        return;

    port = benc_dget_int(pinfo, "port");
    peer_create_out(tp->net, pid, ip, port);

    if (ip != NULL)
        free(ip);
}

static void
parse_reply(struct torrent *tp, struct tr_response *res, const char *content,
    size_t size)
{
    const char *buf;
    size_t len;
    const char *peers;
    const char *v6key[] = {"peers6", "peers_ipv6"};
    size_t i;
    int k = 0;

    if (benc_validate(content, size) != 0)
        goto bad_data;

    if ((buf = benc_dget_any(content, "failure reason")) != NULL) {
        if (!benc_isstr(buf))
            goto bad_data;
        res->type = TR_RES_FAIL;
        res->mi_failure = buf;
        return;
    }

    buf = benc_dget_any(content, "interval");
    if (buf != NULL && benc_isint(buf))
        res->interval = benc_int(buf, NULL);

    if ((peers = benc_dget_any(content, "peers")) == NULL)
        goto after_peers;

    if (benc_islst(peers)) {
        for (peers = benc_first(peers);
             peers != NULL && net_npeers < net_max_peers;
             peers = benc_next(peers))
            maybe_connect_to(tp, peers);
    } else if (benc_isstr(peers)) {
        if (net_ipv4) {
            peers = benc_dget_mem(content, "peers", &len);
            for (i = 0; i < len && net_npeers < net_max_peers; i += 6)
                peer_create_out_compact(tp->net, AF_INET, peers + i);
        }
    } else
        goto bad_data;

after_peers:
    if (!net_ipv6)
        goto after_peers6;
    for (k = 0; k < 2; k++) {
        peers = benc_dget_any(content, v6key[k]);
        if (peers != NULL && benc_isstr(peers)) {
            peers = benc_dget_mem(content, v6key[k], &len);
            for (i = 0; i < len && net_npeers < net_max_peers; i += 18)
                peer_create_out_compact(tp->net, AF_INET6, peers + i);
        }
    }
after_peers6:
    res->type = TR_RES_OK;
    return;

bad_data:
    res->type = TR_RES_BAD;
}

static void
http_cb(struct http_req *req, struct http_response *res, void *arg)
{
    struct httptr_req *treq = arg;
    struct tr_response tres = {0, NULL, -1 };
    switch (res->type) {
    case HTTP_T_ERR:
        tres.type = TR_RES_BAD;
        tr_result(treq->tr, &tres);
        httptr_free(treq);
	btpd_err("tr response code:%d,error:%d.\r\n",
		res->v.code,res->v.error);
        break;
    case HTTP_T_DATA:
        if (treq->buf.off + res->v.data.l > MAX_DOWNLOAD) {
            tres.type = TR_RES_BAD;
            tr_result(treq->tr, &tres);
            httptr_cancel(treq);
            break;
        }
        if (!iobuf_write(&treq->buf, res->v.data.p, res->v.data.l))
            btpd_err("Out of memory.\r\n");
        break;
    case HTTP_T_DONE:
        if (treq->event == TR_EV_STOPPED) {
            tres.type = TR_RES_OK;
            tr_result(treq->tr, &tres);
        } else {
            parse_reply(treq->tp, &tres, treq->buf.buf, treq->buf.off);
            tr_result(treq->tr, &tres);
        }
        httptr_free(treq);
        break;
    default:
        break;
    }
}

static void
httptr_io_cb(int sd, short type, void *arg)
{
    struct tr_response res;
    struct httptr_req *treq = arg;
    switch (type) {
    case EV_READ:
        if (http_read(treq->req, sd) && !http_want_read(treq->req))
            btpd_ev_disable(&treq->ioev, EV_READ);
        break;
    case EV_WRITE:
        if (http_write(treq->req, sd) && !http_want_write(treq->req))
            btpd_ev_disable(&treq->ioev, EV_WRITE);
        break;
    case EV_TIMEOUT:
        res.type = TR_RES_CONN;
        tr_result(treq->tr, &res);
        httptr_cancel(treq);
        break;
    default:
        abort();
    }
}

static int httptr_req_pack(struct httptr_req *treq)
{
	char e_hash[61], e_id[61], url[512], qc;
	const uint8_t *peer_id = btpd_get_peer_id();
	int i;

	char *aurl = treq->aurl;

	qc = (strchr(aurl, '?') == NULL) ? '?' : '&';

	for (i = 0; i < 20; i++)
		sprintf(e_hash + i * 3, "%%%.2x", treq->tp->tl->hash[i]);
	for (i = 0; i < 20; i++)
		sprintf(e_id + i * 3, "%%%.2x", peer_id[i]);

	sprintf(url,
		"%s%cinfo_hash=%s&peer_id=%s&key=%ld%s%s&port=%d&uploaded=%llu"
		"&downloaded=%llu&left=%llu&compact=1%s%s",
		aurl, qc, e_hash, e_id, tr_key,
		tr_ip_arg == NULL ? "" : "&localip=", tr_ip_arg == NULL ? "" : tr_ip_arg,
		net_port, treq->tp->net->uploaded, treq->tp->net->downloaded,
		(long long)treq->tp->total_length - cm_content(treq->tp),
		treq->event == TR_EV_EMPTY ? "" : "&event=", m_tr_events[treq->event]);

	if (!http_get(&treq->req, url, "User-Agent: " BTPD_VERSION "\r\n",
		http_cb, treq)) {
			return -1;
	}
	return 0;
}

static void
httptr_nc_cb(void *arg, int error, SOCKET sd)
{
	struct sockaddr_in addr;
	socklen_t len;
    struct tr_response res;
    struct httptr_req *treq = arg;
    struct timespec ts = {30,0};
    uint16_t flags;
	int rv;
    if (error) {
        res.type = TR_RES_CONN;
        tr_result(treq->tr, &res);
        http_cancel(treq->req);
        httptr_free(treq);
    } else {	
		//此处获取下出口网卡的ip做为报告给tracker的内网ip
		len = sizeof(addr);
		if(getsockname(sd, (struct sockaddr *)&addr, &len) != 0)
		{
			tr_ip_arg = NULL;
		}
		else
			tr_ip_arg = inet_ntoa(addr.sin_addr);

		treq->sd = sd;
		rv = httptr_req_pack(treq);

		if(rv != 0)
			return;
	
        flags =
            (http_want_read(treq->req) ? EV_READ : 0) |
            (http_want_write(treq->req) ? EV_WRITE : 0);
        btpd_ev_new(&treq->ioev, sd, flags, httptr_io_cb, treq);
        btpd_timer_add(&treq->timer,&ts);
    }
}

struct httptr_req *
httptr_req(struct torrent *tp, struct tr_tier *tr, const char *aurl,
    enum tr_event event)
{
    struct http_url *http_url;
    struct httptr_req *treq;
    struct timespec ts;

    treq = btpd_calloc(1, sizeof(*treq));
    treq->tp = tp;
    treq->tr = tr;
    treq->event = event;
    treq->buf = iobuf_init(4096);
    if (treq->buf.error)
        btpd_err("Out of memory.\r\n");
    treq->tr = tr;
    treq->sd = -1;
	memcpy(treq->aurl,aurl,strlen(aurl));
    http_url = http_url_parse(aurl);
    treq->nc = btpd_name_connect(http_url->host, http_url->port,
        httptr_nc_cb, treq);
    evtimer_init(&treq->timer, httptr_io_cb, treq);
    ts.tv_sec = 60;
    ts.tv_nsec = 0;
    btpd_timer_add(&treq->timer, &ts);
	http_url_free(http_url);
    return treq;
}

void
httptr_cancel(struct httptr_req *treq)
{
    if (treq->sd == -1)
        btpd_name_connect_cancel(treq->nc);
	if(treq->req)
		http_cancel(treq->req);
    httptr_free(treq);
}
