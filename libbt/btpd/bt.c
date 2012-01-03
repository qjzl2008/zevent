#include "btpd.h"
#include "iobuf.h"
#include "bt.h"
#include "getopt.h"
#include "opts.h"
#include "http_client.h"
#include "upnp_nat.h"

static int upnp_on = 0;
static upnp_param_t upnp_param;
static upnp_nat_t *upnp_nat;

static DWORD WINAPI evloop_td(void *arg)
{
    bt_arg_t *bt_arg = (bt_arg_t *)arg;
    evloop();
    return 0;
}

BT_DECLARE(int) bt_start_daemon(bt_arg_t *bt_arg,bt_t **bt)
{
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2,2),&wsaData);

    log_init();
    net_port = bt_arg->net_port;

    empty_start = bt_arg->empty_start;
    if (evloop_init() != 0)
    {
	btpd_err("Failed to initialize evloop (%s).\r\n",strerror(errno));
	return -1;
    }

	*bt = (bt_t *)malloc(sizeof(bt_t));
	sizeof(*bt,0,sizeof(*bt));

	(*bt)->bt_port = net_port;

    if(btpd_init(*bt) != 0)
	return -1;

	if(!bt_arg->empty_start)
		active_start();
	else
		active_clear();

    (*bt)->th_bt = CreateThread(NULL,0,evloop_td,bt_arg,0,NULL);
	if(bt_arg->use_upnp == 1)
	{//启动upnp服务
		strcpy(upnp_param.ip,"0.0.0.0");
		strcpy(upnp_param.desc,"p2sp");
		upnp_param.port = net_port;
		strcpy(upnp_param.protocol,"TCP");
		upnp_nat_start(&upnp_param,&upnp_nat);
		upnp_on = 1;
	}
    return 0;
}

BT_DECLARE(int) bt_stop_daemon(bt_t *bt)
{
    enum ipc_err code;
    code = btpd_die(bt->cmdpipe);
    if (code != IPC_OK)
    {
	return -1;
    }

    Sleep(1000);//等待1s向tracker发送stop事件   
	ipc_close(bt->cmdpipe);
	btpd_addrinfo_stop();

    daemon_stop = 1;
    WaitForSingleObject(bt->th_bt,10000);
    CloseHandle(bt->th_bt);

	if(upnp_on)
		upnp_nat_stop(&upnp_param,&upnp_nat);
    WSACleanup();
    log_fini();
	free(bt);
    return 0;
}

BT_DECLARE(int) bt_add(const char *dir,const char *torrent,bt_t *bt)
{
    char *mi;
    size_t mi_size;
    enum ipc_err code;
    char dpath[MAX_PATH];
    char *name = NULL;


    if ((mi = mi_load(torrent,&mi_size)) == NULL)
    {
	return -1;
    }

    strcpy(dpath,dir);
    code = btpd_add(bt->cmdpipe,mi,mi_size,dpath,name);
    if (code == IPC_OK) {
	struct ipc_torrent tspec;
	tspec.by_hash = 1;
	mi_info_hash(mi,tspec.u.hash);
	code = btpd_start(bt->cmdpipe,&tspec);
    }
    else if(code == IPC_ETENTEXIST)
    {
	free(mi);
	return 1;
    }
    else
    {
	free(mi);
	return -1;
    }
    free(mi);
    return 0;
}

typedef struct{
    char *mi_data;
    size_t mi_size;
}mi_t;

static void
http_cb(struct http_req *req,struct http_response *res,void *arg)
{
    mi_t *mi = (mi_t *)arg;
    switch (res->type) {
	case HTTP_T_ERR:
	    break;
	case HTTP_T_DATA:
	    if(!mi->mi_data)
	    {
		mi->mi_data = malloc(res->v.data.l);
	    }
	    memcpy(mi->mi_data,res->v.data.p,res->v.data.l);
	    mi->mi_size = res->v.data.l;
	    break;
	case HTTP_T_DONE:
	    break;
	default:
	    break;
    }
}

static int
write_torrent(const char *mi, size_t mi_size, const char *path)
{
    FILE *fp;
    if((fp = fopen(path,"wb")) == NULL)
	goto err;
    if(fwrite(mi,mi_size,1,fp) != 1) {
	fclose(fp);
	errno = EIO;
	goto err;
    }

    if(fclose(fp) != 0)
	goto err;
    return 0;
err:
    return -1;
}

static int 
net_connect_block(const char *ip,int port,SOCKET *sd,int tm_sec)
{
    int rv;
	struct addrinfo hints, *res;
	char portstr[6];

	struct timeval tm;
	fd_set wset,eset;
	FD_ZERO(&wset);
	FD_ZERO(&eset);
	tm.tv_sec=tm_sec;
	tm.tv_usec=0;

    *sd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(*sd == INVALID_SOCKET)
    {
	rv = WSAGetLastError();
	return -1;
    }

	set_nonblocking(*sd);

    if(sprintf(portstr,"%d",port) >= sizeof(portstr))
	return EINVAL;
    memset(&hints,0,sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_flags = AI_NUMERICHOST;
    hints.ai_socktype = SOCK_STREAM;

    if(getaddrinfo(ip,portstr,&hints,&res) != 0)
	return -1;

	rv = connect(*sd,res->ai_addr,(int)res->ai_addrlen);
	freeaddrinfo(res);
    if( rv== SOCKET_ERROR)
	{
		if(WSAGetLastError() != WSAEWOULDBLOCK) {
			closesocket(*sd);
			return -1;
		}
		else
		{
			FD_SET(*sd,&wset);
			FD_SET(*sd,&eset);
			rv = select(0,NULL,&wset,&eset,&tm);
			if(rv < 0){
				closesocket(*sd);
				return -1;
			}
			if(rv == 0)
			{
				closesocket(*sd);
				return -2;//timeout
			}
			if(FD_ISSET(*sd,&eset))
			{
				closesocket(*sd);
				return -1;
			}
			if(FD_ISSET(*sd,&wset))
			{
				int err=0;

				socklen_t len=sizeof(err);
				rv = getsockopt(*sd,SOL_SOCKET,SO_ERROR,&err,&len);
				if(rv < 0 || (rv ==0 && err))
				{
					closesocket(*sd);
					return -1;
				}
			}
		}
	}
    set_blocking(*sd);
    return 0;
}

BT_DECLARE(int) bt_add_url(const char *dir,const char *name,const char *url,
	bt_t *bt)
{
	enum ipc_err code;
	char dpath[MAX_PATH];
	int tm_sec = 3;

	SOCKET sd;
	int rv;
	struct http_req *req = NULL;
	mi_t mi = {NULL,0};
	http_get(&req,url,"User-Agent: " "btpd" "\r\n",http_cb,&mi);
	rv = net_connect_block(req->url->host,req->url->port,&sd,webseed_timeout);
	if(rv != 0)
	{
		http_free(req);
		return -1;
	}
	if(!http_write(req,sd))
	{
		return -1;
	}
	http_read(req,sd);
	closesocket(sd);

	if(mi.mi_data)
	{
		write_torrent(mi.mi_data,mi.mi_size,name);

		strcpy(dpath,dir);
		code = btpd_add(bt->cmdpipe,mi.mi_data,mi.mi_size,dpath,NULL);
		if(code == IPC_OK) {
			struct ipc_torrent tspec;
			tspec.by_hash = 1;
			mi_info_hash(mi.mi_data,tspec.u.hash);
			code = btpd_start(bt->cmdpipe,&tspec);
		}
		else if(code == IPC_ETENTEXIST)
		{
			free(mi.mi_data);
			return 1;
		}
		else
		{
			free(mi.mi_data);
			return -1;
		}
		free(mi.mi_data);
		return 0;
	}
	else
		return -1;
}

BT_DECLARE(int) bt_add_p2sp(char *torrent,const char *url,bt_t *bt)
{
    enum ipc_err code;
    struct ipc_torrent t;
    if(torrent_spec(torrent,&t))
    {
	code = btpd_add_p2sp(bt->cmdpipe,&t,url);
	if(code == IPC_OK) {
	    return 0;
	}
	else
	{
	    return -1;
	}
    }
    return 0;
}

BT_DECLARE(int) bt_del(int argc,char **argv,bt_t *bt)
{
    struct ipc_torrent t;
    enum ipc_err err;
    int i = 0;

    if(argc < 1)
	return -1;

    for(i = 0; i < argc; i++)
    {
	if(torrent_spec(argv[i],&t))
	{
	    err = btpd_del(bt->cmdpipe,&t);
	    if(err != IPC_OK)
		return -1;
	}
    }

    return 0;
}

BT_DECLARE(int) bt_stopall(bt_t *bt)
{
    enum ipc_err code;
    
    code = btpd_stop_all(bt->cmdpipe);
    if(code != IPC_OK)
    {
	return -1;
    }

    return 0;
}

BT_DECLARE(int) bt_stop(int argc,char **argv,bt_t *bt)
{
    struct ipc_torrent t;
    enum ipc_err code;
    int i = 0;

    if(argc < 1)
	return -1;

    for(i = 0; i < argc; i++)
    {
	if (torrent_spec(argv[i],&t))
	{
	    code = btpd_stop(bt->cmdpipe,&t);
	    if(code != IPC_OK)
	    {
		return -1;
	    }
	}
    }
    return 0;
}

BT_DECLARE(int) bt_start(int argc,char **argv,bt_t *bt)
{
    struct ipc_torrent t;
    enum ipc_err err;
    int i = 0;
    
    if(argc < 1)
	return -1;
    for(i = 0; i < argc; i++)
    {
	if(torrent_spec(argv[i],&t))
	{
	    err = btpd_start(bt->cmdpipe,&t);
	    if(err != IPC_OK && err != IPC_ETACTIVE)
		return -1;
	}
    }

    return 0;
}

BT_DECLARE(int) bt_rate(unsigned up,unsigned down,bt_t *bt)
{
    enum ipc_err err;
    err = btpd_rate(bt->cmdpipe,up,down);
    if(err != IPC_OK)
	return -1;
    return 0;
}

struct cbarg{
    struct btstat tot;
};

static enum ipc_tval stkeys[] = {
    IPC_TVAL_STATE,
    IPC_TVAL_NUM,
    IPC_TVAL_NAME,
    IPC_TVAL_PCOUNT,
    IPC_TVAL_TRGOOD,
    IPC_TVAL_PCCOUNT,
    IPC_TVAL_PCSEEN,
    IPC_TVAL_SESSUP,
    IPC_TVAL_SESSDWN,
    IPC_TVAL_TOTUP,
    IPC_TVAL_RATEUP,
    IPC_TVAL_RATEDWN,
    IPC_TVAL_CGOT,
    IPC_TVAL_CSIZE
};

static size_t nstkeys = sizeof(stkeys) / sizeof(stkeys[0]);

static void 
stat_cb(int obji,enum ipc_err objerr,struct ipc_get_res *res,void *arg)
{
    struct cbarg *cba = arg;
    struct btstat st,*tot = &cba->tot;
    if (objerr != IPC_OK || res[IPC_TVAL_STATE].v.num == IPC_TSTATE_INACTIVE)
	return;
    memset(&st,0,sizeof(st));
	tot->num = (st.num = res[IPC_TVAL_NUM].v.num);
	tot->state = (st.state = res[IPC_TVAL_STATE].v.num);
    tot->torrent_pieces += (st.torrent_pieces = res[IPC_TVAL_PCCOUNT].v.num);
    tot->pieces_seen += (st.pieces_seen = res[IPC_TVAL_PCSEEN].v.num);
    tot->content_got += (st.content_got = res[IPC_TVAL_CGOT].v.num);
    tot->content_size += (st.content_size = res[IPC_TVAL_CSIZE].v.num);
    tot->downloaded += (st.downloaded = res[IPC_TVAL_SESSDWN].v.num);
    tot->uploaded += (st.uploaded = res[IPC_TVAL_SESSUP].v.num);
    tot->rate_down += (st.rate_down = res[IPC_TVAL_RATEDWN].v.num);
    tot->rate_up += (st.rate_up = res[IPC_TVAL_RATEUP].v.num);
    tot->peers += (st.peers = res[IPC_TVAL_PCOUNT].v.num);
    tot->tot_up += (st.tot_up = res[IPC_TVAL_TOTUP].v.num);
    tot->tr_good += (st.tr_good = res[IPC_TVAL_TRGOOD].v.num);
}

BT_DECLARE(int) bt_stat(char *torrent,bt_t *bt,struct btstat *stat)
{
    struct ipc_torrent t;
    enum ipc_err err;
    struct cbarg cba;

    memset(&cba.tot,0,sizeof(cba.tot));

    if(torrent_spec(torrent,&t))
    {
	err = btpd_tget(bt->cmdpipe,&t,1,stkeys,nstkeys,stat_cb,&cba);
	if(err != IPC_OK)
	    return -1;
	else
	{
	    *stat = cba.tot;
	    return 0;
	}
    }
    else
	return -1;
}

