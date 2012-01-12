#include "btpd.h"
#include "http_client.h"

#include <iobuf.h>

struct cli {
    SOCKET sd;
    struct fdev read;
};

struct cli *cli;
static SOCKET m_listen_sd;
static struct fdev m_cli_incoming;

static int
write_buffer(struct cli *cli, struct iobuf *iob)
{
    int err = 0;
    if (!iob->error) {
        uint32_t len = iob->off;
        write_fully(cli->sd, &len, sizeof(len));
        err = write_fully(cli->sd, iob->buf, iob->off);
    } else
        btpd_err("Out of memory.\r\n");
    iobuf_free(iob);
    return err;
}

static int
write_code_buffer(struct cli *cli, enum ipc_err code)
{
    struct iobuf iob = iobuf_init(16);
    iobuf_print(&iob, "d4:codei%uee", code);
    return write_buffer(cli, &iob);
}

static int
write_add_buffer(struct cli *cli, unsigned num)
{
    struct iobuf iob = iobuf_init(32);
    iobuf_print(&iob, "d4:codei%ue3:numi%uee", IPC_OK, num);
    return write_buffer(cli, &iob);
}

static void
write_ans(struct iobuf *iob, struct tlib *tl, enum ipc_tval val)
{
    enum ipc_tstate ts = IPC_TSTATE_INACTIVE;
    unsigned long pcseen = 0;
    unsigned long i = 0;

    switch (val) {
    case IPC_TVAL_CGOT:
        iobuf_print(iob, "i%dei%llde", IPC_TYPE_NUM,
            tl->tp == NULL ? tl->content_have : (long long)cm_content(tl->tp));
        return;
    case IPC_TVAL_CSIZE:
        iobuf_print(iob, "i%dei%llde", IPC_TYPE_NUM,
            (long long)tl->content_size);
        return;
    case IPC_TVAL_PCCOUNT:
        if (tl->tp == NULL)
            iobuf_print(iob, "i%dei%de", IPC_TYPE_ERR, IPC_ETINACTIVE);
        else
            iobuf_print(iob, "i%dei%lue", IPC_TYPE_NUM,
                (unsigned long)tl->tp->npieces);
        return;
    case IPC_TVAL_PCGOT:
        if (tl->tp == NULL)
            iobuf_print(iob, "i%dei%de", IPC_TYPE_ERR, IPC_ETINACTIVE);
        else
            iobuf_print(iob, "i%dei%lue", IPC_TYPE_NUM,
                (unsigned long)cm_pieces(tl->tp));
        return;
    case IPC_TVAL_PCSEEN:
        if (tl->tp == NULL)
            iobuf_print(iob, "i%dei%de", IPC_TYPE_NUM, 0);
        else {
            pcseen = 0;
            for (i = 0; i < tl->tp->npieces; i++)
                if (tl->tp->net->piece_count[i] > 0)
                    pcseen++;
            iobuf_print(iob, "i%dei%lue", IPC_TYPE_NUM, pcseen);
        }
        return;
    case IPC_TVAL_RATEDWN:
        iobuf_print(iob, "i%dei%lue", IPC_TYPE_NUM,
            tl->tp == NULL ? 0UL : tl->tp->net->rate_dwn / RATEHISTORY);
        return;
    case IPC_TVAL_RATEUP:
        iobuf_print(iob, "i%dei%lue", IPC_TYPE_NUM,
            tl->tp == NULL ? 0UL : tl->tp->net->rate_up / RATEHISTORY);
        return;
    case IPC_TVAL_SESSDWN:
        iobuf_print(iob, "i%dei%llde", IPC_TYPE_NUM,
            tl->tp == NULL ? 0LL : tl->tp->net->downloaded);
        return;
    case IPC_TVAL_SESSUP:
        iobuf_print(iob, "i%dei%llde", IPC_TYPE_NUM,
            tl->tp == NULL ? 0LL : tl->tp->net->uploaded);
        return;
    case IPC_TVAL_DIR:
        if (tl->dir != NULL)
            iobuf_print(iob, "i%de%d:%s", IPC_TYPE_STR, (int)strlen(tl->dir),
                tl->dir);
        else
            iobuf_print(iob, "i%dei%de", IPC_TYPE_ERR, IPC_EBADTENT);
        return;
    case IPC_TVAL_NAME:
        if (tl->name != NULL)
            iobuf_print(iob, "i%de%d:%s", IPC_TYPE_STR, (int)strlen(tl->name),
                tl->name);
        else
            iobuf_print(iob, "i%dei%de", IPC_TYPE_ERR, IPC_EBADTENT);
        return;
    case IPC_TVAL_IHASH:
        iobuf_print(iob, "i%de20:", IPC_TYPE_BIN);
        iobuf_write(iob, tl->hash, 20);
        return;
    case IPC_TVAL_NUM:
        iobuf_print(iob, "i%dei%ue", IPC_TYPE_NUM, tl->num);
        return;
    case IPC_TVAL_PCOUNT:
        iobuf_print(iob, "i%dei%ue", IPC_TYPE_NUM,
            tl->tp == NULL ? 0 : tl->tp->net->npeers);
        return;
    case IPC_TVAL_STATE:
        iobuf_print(iob, "i%de", IPC_TYPE_NUM);
        if (tl->tp != NULL) {
            switch (tl->tp->state) {
            case T_STARTING:
                ts = IPC_TSTATE_START;
                break;
            case T_STOPPING:
                ts = IPC_TSTATE_STOP;
                break;
            case T_SEED:
                ts = IPC_TSTATE_SEED;
                break;
            case T_LEECH:
                ts = IPC_TSTATE_LEECH;
                break;
            case T_GHOST:
                break;
            }
        }
        iobuf_print(iob, "i%de", ts);
        return;
    case IPC_TVAL_TOTDWN:
        iobuf_print(iob, "i%dei%llde", IPC_TYPE_NUM, tl->tot_down +
            (tl->tp == NULL ? 0 : tl->tp->net->downloaded));
        return;
    case IPC_TVAL_TOTUP:
        iobuf_print(iob, "i%dei%llde", IPC_TYPE_NUM, tl->tot_up +
            (tl->tp == NULL ? 0 : tl->tp->net->uploaded));
        return;
    case IPC_TVAL_TRERR:
        iobuf_print(iob, "i%dei%ue", IPC_TYPE_NUM, 0);
        return;
    case IPC_TVAL_TRGOOD:
        iobuf_print(iob, "i%dei%de", IPC_TYPE_NUM,
            tl->tp == NULL ? 0 : tr_good_count(tl->tp));
        return;
    case IPC_TVALCOUNT:
        break;
    }
    iobuf_print(iob, "i%dei%de", IPC_TYPE_ERR, IPC_ENOKEY);
}

static int
cmd_tget(struct cli *cli, int argc, const char *args)
{
    size_t nkeys;
    const char *keys, *p;
    enum ipc_tval *opts;
    struct iobuf iob;
    int i = 0, k= 0;

    if(argc != 1 || !benc_isdct(args))
	return IPC_COMMERR;

    if ((keys = benc_dget_lst(args, "keys")) == NULL)
        return IPC_COMMERR;

    nkeys = benc_nelems(keys);
    opts = btpd_calloc(nkeys, sizeof(*opts));

    p = benc_first(keys);
    for (i = 0; i < nkeys; i++)
        opts[i] = benc_int(p, &p);

    iob = iobuf_init(1 << 15);
    iobuf_swrite(&iob, "d4:codei0e6:resultl");
    p = benc_dget_any(args, "from");
    if (benc_isint(p)) {
        enum ipc_twc from = benc_int(p, NULL);
        struct htbl_iter it;
        struct tlib *tl;
        for (tl = tlib_iter_first(&it); tl != NULL; tl = tlib_iter_next(&it)) {
            if (!torrent_haunting(tl) && (
                    from == IPC_TWC_ALL ||
                    (!torrent_active(tl) && from == IPC_TWC_INACTIVE) ||
                    (torrent_active(tl) && from == IPC_TWC_ACTIVE))) {
                iobuf_swrite(&iob, "l");
                for (k = 0; k < nkeys; k++)
                    write_ans(&iob, tl, opts[k]);
                iobuf_swrite(&iob, "e");
            }
        }
    } else if (benc_islst(p)) {
        for (p = benc_first(p); p != NULL; p = benc_next(p)) {
            struct tlib *tl = NULL;
            if (benc_isint(p))
                tl = tlib_by_num(benc_int(p, NULL));
            else if (benc_isstr(p) && benc_strlen(p) == 20)
                tl = tlib_by_hash(benc_mem(p, NULL, NULL));
            else {
                iobuf_free(&iob);
                free(opts);
                return IPC_COMMERR;
            }
            if (tl != NULL && !torrent_haunting(tl)) {
                iobuf_swrite(&iob, "l");
                for (i = 0; i < nkeys; i++)
                    write_ans(&iob, tl, opts[i]);
                iobuf_swrite(&iob, "e");
            } else
                iobuf_print(&iob, "i%de", IPC_ENOTENT);
        }
    }
    iobuf_swrite(&iob, "ee");
    free(opts);
    return write_buffer(cli, &iob);
}

static int
cmd_add(struct cli *cli, int argc, const char *args)
{
    struct tlib *tl;
    size_t mi_size = 0, csize = 0;
    const char *mi, *cp;
    char content[MAX_PATH];
    uint8_t hash[20];
	char *pname = NULL;

    if(argc != 1 || !benc_isdct(args))
	return IPC_COMMERR;

    if ((mi = benc_dget_mem(args, "torrent", &mi_size)) == NULL)
        return IPC_COMMERR;

    if (!mi_test(mi, mi_size))
        return write_code_buffer(cli, IPC_EBADT);

    if ((cp = benc_dget_mem(args, "content", &csize)) == NULL ||
            csize >= MAX_PATH || csize == 0)
        return write_code_buffer(cli, IPC_EBADCDIR);

    //if (cp[0] != '/')
    //    return write_code_buffer(cli, IPC_EBADCDIR);
    memcpy(content, cp, csize);
    content[csize] = '\0';

    tl = tlib_by_hash(mi_info_hash(mi, hash));
    if (tl != NULL && !torrent_haunting(tl))
        return write_code_buffer(cli, IPC_ETENTEXIST);

	pname = benc_dget_str(args, "name", NULL);
    if (tl != NULL) {
        tl = tlib_readd(tl, hash, mi, mi_size, content,
            benc_dget_str(args, "name", NULL));
    } else {
        tl = tlib_add(hash, mi, mi_size, content,
            benc_dget_str(args, "name", NULL));
    }
	if(pname)
		free(pname);
    return write_add_buffer(cli, tl->num);
}

static int
cmd_add_p2sp(struct cli *cli, int argc, const char *args)
{
    const char *url;
    const uint8_t *hash;
    size_t url_size = 0,hash_size = 0;
    struct http_url *p2sp_url;
    struct tlib *tl;

    if(argc != 1 || !benc_isdct(args))
	return IPC_COMMERR;

    if((url = benc_dget_str(args,"url",&url_size)) == NULL)
	return IPC_COMMERR;
    if((hash = benc_dget_mem(args,"hash",&hash_size)) == NULL)
	return IPC_COMMERR;

    tl = tlib_by_hash(hash);
    if(tl == NULL || torrent_haunting(tl))
	return write_code_buffer(cli,IPC_ENOTENT);
    else if(!torrent_active(tl))
	return write_code_buffer(cli,IPC_ETINACTIVE);
    else
	write_code_buffer(cli,IPC_OK);

    p2sp_url = http_url_parse(url);
    if(p2sp_url)
    {
	strcpy(tl->tp->sinfo[tl->tp->svrnum].host,p2sp_url->host);
	tl->tp->sinfo[tl->tp->svrnum].port = p2sp_url->port;
	strcpy(tl->tp->sinfo[tl->tp->svrnum].uri,p2sp_url->uri);
	tl->tp->sinfo->done = 0;
	++tl->tp->svrnum;
	http_url_free(p2sp_url);
    }
    return 0;
}

static int
cmd_del(struct cli *cli, int argc, const char *args)
{
    int ret;
    struct tlib *tl;
    if (argc != 1)
        return IPC_COMMERR;

    if (benc_isstr(args) && benc_strlen(args) == 20)
        tl = tlib_by_hash(benc_mem(args, NULL, NULL));
    else if (benc_isint(args))
        tl = tlib_by_num(benc_int(args, NULL));
    else
        return IPC_COMMERR;

    if (tl == NULL || torrent_haunting(tl))
        ret = write_code_buffer(cli, IPC_ENOTENT);
    else {
        ret = write_code_buffer(cli, IPC_OK);
        if (tl->tp != NULL)
            torrent_stop(tl->tp, 1);
        else
            tlib_del(tl);
    }

    return ret;
}

static int
cmd_start(struct cli *cli, int argc, const char *args)
{
    struct tlib *tl;
    enum ipc_err code = IPC_OK;

    if (argc != 1)
        return IPC_COMMERR;
    if (btpd_is_stopping())
        return write_code_buffer(cli, IPC_ESHUTDOWN);

    if (benc_isstr(args) && benc_strlen(args) == 20)
        tl = tlib_by_hash(benc_mem(args, NULL, NULL));
    else if (benc_isint(args))
        tl = tlib_by_num(benc_int(args, NULL));
    else
        return IPC_COMMERR;

    if (tl == NULL || torrent_haunting(tl))
        code = IPC_ENOTENT;
    else if (!torrent_startable(tl))
        code = IPC_ETACTIVE;
    else
        if ((code = torrent_start(tl)) == IPC_OK)
            active_add(tl->hash);
    return write_code_buffer(cli, code);
}

static int
cmd_stop(struct cli *cli, int argc, const char *args)
{
    struct tlib *tl;
    if (argc != 1)
        return IPC_COMMERR;

    if (benc_isstr(args) && benc_strlen(args) == 20)
        tl = tlib_by_hash(benc_mem(args, NULL, NULL));
    else if (benc_isint(args))
        tl = tlib_by_num(benc_int(args, NULL));
    else
        return IPC_COMMERR;

    if (tl == NULL || torrent_haunting(tl))
        return write_code_buffer(cli, IPC_ENOTENT);
    else if (!torrent_active(tl))
        return write_code_buffer(cli, IPC_ETINACTIVE);
    else  {
        // Stopping a torrent may trigger exit so we need to reply before.
        int ret = write_code_buffer(cli, IPC_OK);
        active_del(tl->hash);
        torrent_stop(tl->tp, 0);
        return ret;
    }
}

static int
cmd_stop_all(struct cli *cli, int argc, const char *args)
{
    struct torrent *tp, *next;
    int ret = write_code_buffer(cli, IPC_OK);

    active_clear();
    BTPDQ_FOREACH_MUTABLE(tp, torrent_get_all(), entry, next)
        torrent_stop(tp, 0);
    return ret;
}

static int 
cmd_die(struct cli *cli, int argc, const char *args)
{
    int err = write_code_buffer(cli,IPC_OK);
    if(!btpd_is_stopping()) {
	btpd_log(BTPD_L_BTPD,"Someone wants me dead.\r\n");
	btpd_shutdown();
    }
    return err;
}

static int
cmd_rate(struct cli *cli, int argc, const char *args)
{
    unsigned up, down;

    if (argc != 2)
        return IPC_COMMERR;
    if (btpd_is_stopping())
        return write_code_buffer(cli, IPC_ESHUTDOWN);

    if (benc_isint(args))
        up = (unsigned)benc_int(args, &args);
    else
        return IPC_COMMERR;

    if (benc_isint(args))
        down = (unsigned)benc_int(args, &args);
    else
        return IPC_COMMERR;

    net_bw_limit_out = up;
    net_bw_limit_in  = down;
    ul_set_max_uploads();

    return write_code_buffer(cli, IPC_OK);
}


static struct {
    const char *name;
    int nlen;
    int (*fun)(struct cli *cli, int, const char *);
} cmd_table[] = {
    { "add",    3, cmd_add },
    { "del",    3, cmd_del },
    { "die",    3, cmd_die },
    { "start",  5, cmd_start },
    { "stop",   4, cmd_stop },
    { "rate",   4, cmd_rate },
    { "stop-all", 8, cmd_stop_all},
    { "tget",   4, cmd_tget },
    { "addp2sp",7, cmd_add_p2sp}
};

static int ncmds = sizeof(cmd_table) / sizeof(cmd_table[0]);

static int
cmd_dispatch(struct cli *cli, const char *buf)
{
    size_t cmdlen;
    const char *cmd;
    const char *args;
    int i = 0;

    cmd = benc_mem(benc_first(buf), &cmdlen, &args);

    for (i = 0; i < ncmds; i++) {
        if ((cmdlen == cmd_table[i].nlen &&
                strncmp(cmd_table[i].name, cmd, cmdlen) == 0)) {
            return cmd_table[i].fun(cli, benc_nelems(buf) - 1, args);
        }
    }
    return ENOENT;
}

static void
cli_read_cb(int sd, short type, void *arg)
{
    struct cli *cli = arg;
    uint32_t cmdlen;
    uint8_t *msg = NULL;

    if (read_fully(sd, &cmdlen, sizeof(cmdlen)) != 0)
        goto error;

    msg = btpd_malloc(cmdlen);
    if (read_fully(sd, msg, cmdlen) != 0)
        goto error;

    if (!(benc_validate(msg, cmdlen) == 0 && benc_islst(msg) &&
            benc_first(msg) != NULL && benc_isstr(benc_first(msg))))
        goto error;

    if (cmd_dispatch(cli, msg) != 0)
        goto error;

    free(msg);
    return;

error:
    btpd_ev_del(&cli->read);
    closesocket(cli->sd);
    free(cli);
    cli = NULL;
    if (msg != NULL)
        free(msg);
}

void
client_connection_cb(SOCKET sd, short type, void *arg)
{
    SOCKET nsd;

    if ((nsd = accept(sd, NULL, NULL)) == INVALID_SOCKET) {
	btpd_err("client accept failed.code:%d\r\n", WSAGetLastError());
	return;
    }

    if ((set_blocking(nsd)) != 0)
        btpd_err("set_blocking failed.\r\n");

    cli = btpd_calloc(1, sizeof(*cli));
    cli->sd = nsd;
    btpd_ev_new(&cli->read, cli->sd, EV_READ, cli_read_cb, cli);
}

int
ipc_init(bt_t *bt)
{
	SOCKET sd,cd,nsd;
	struct ipc *cmd_pipe; 
	struct sockaddr_in addr;
	socklen_t len = 0;

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons(0);

	if ((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		btpd_err("sock: %s\r\n", strerror(errno));
		return -1;
	}
	if (bind(sd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
		btpd_err("bind: %s\r\n", strerror(errno));
		return -1;
	}

	len = sizeof(addr);
	if(getsockname(sd, (struct sockaddr *)&addr, &len) != 0)
	{
		closesocket(sd);
		return -1;
	}

	listen(sd, 4);

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

	cli = btpd_calloc(1, sizeof(*cli));
	cli->sd = nsd;
	btpd_ev_new(&cli->read, cli->sd, EV_READ, cli_read_cb, cli);

	if ((cmd_pipe = (struct ipc *)malloc(sizeof(*cmd_pipe))) == NULL) {
		return ENOMEM;
	}

	cmd_pipe->sd = cd;
	bt->cmdpipe = cmd_pipe;

    set_nonblocking(sd);
	btpd_ev_new(&m_cli_incoming, sd, EV_READ, client_connection_cb, NULL);
	m_listen_sd = sd;
	return 0;
}

int ipc_fini(bt_t *bt)
{
	closesocket(m_listen_sd);
	return 0;
}