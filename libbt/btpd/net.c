#include "btpd.h"
#include <ws2tcpip.h>


static unsigned long m_bw_bytes_in;
static unsigned long m_bw_bytes_out;

static unsigned long m_rate_up;
static unsigned long m_rate_dwn;

struct net_listener {
	SOCKET sd;
	struct fdev ev;
};

static int m_nlisteners;
static struct net_listener *m_net_listeners;

unsigned net_npeers;

struct peer_tq net_bw_readq = BTPDQ_HEAD_INITIALIZER(net_bw_readq);
struct peer_tq net_bw_writeq = BTPDQ_HEAD_INITIALIZER(net_bw_writeq);
struct peer_tq net_unattached = BTPDQ_HEAD_INITIALIZER(net_unattached);

void
net_ban_peer(struct net *n, struct meta_peer *mp)
{
	if (mp->flags & PF_BANNED)
		return;
	mp_hold(mp); // Keep the meta peer alive
	mp->flags |= PF_BANNED;
	btpd_log(BTPD_L_BAD, "banned peer %p.\r\n", mp);
}

int
net_torrent_has_peer(struct net *n, const uint8_t *id)
{
	return mptbl_find(n->mptbl, id) != NULL;
}

void
net_create(struct torrent *tp)
{
	struct net *n = btpd_calloc(1, sizeof(*n));
	n->tp = tp;
	tp->net = n;

	if ((n->mptbl = mptbl_create(3, btpd_id_eq, btpd_id_hash)) == NULL)
		btpd_err("Out of memory.\r\n");

	BTPDQ_INIT(&n->getlst);

	n->busy_field = btpd_calloc(ceil(tp->npieces / 8.0), 1);
	n->piece_count = btpd_calloc(tp->npieces, sizeof(*n->piece_count));
}

void
net_kill(struct torrent *tp)
{
	struct htbl_iter it;
	struct meta_peer *mp = mptbl_iter_first(tp->net->mptbl, &it);
	while (mp != NULL) {
		struct meta_peer *mps = mp;
		mp = mptbl_iter_del(&it);
		mp_kill(mps);
	}
	mptbl_free(tp->net->mptbl);
	free(tp->net->piece_count);
	free(tp->net->busy_field);
	free(tp->net);
	tp->net = NULL;
}

void
net_start(struct torrent *tp)
{
	struct net *n = tp->net;
	n->active = 1;
}

void
net_stop(struct torrent *tp)
{
	struct net *n = tp->net;
	struct piece *pc;
	struct peer *p;

	n->active = 0;
	n->rate_up = 0;
	n->rate_dwn = 0;

	ul_on_lost_torrent(n);

	while ((pc = BTPDQ_FIRST(&n->getlst)) != NULL)
		piece_free(pc);
	BTPDQ_INIT(&n->getlst);

	p = BTPDQ_FIRST(&net_unattached);
	while (p != NULL) {
		struct peer *next = BTPDQ_NEXT(p, p_entry);
		if (p->n == n)
			peer_kill(p);
		p = next;
	}

	p = BTPDQ_FIRST(&n->peers);
	while (p != NULL) {
		struct peer *next = BTPDQ_NEXT(p, p_entry);
		peer_kill(p);
		p = next;
	}
}

int
net_active(struct torrent *tp)
{
	return tp->net->active;
}

#define DWORD_MAX (0xFFFFFFFFUL)
#define WSABUF_ON_STACK (50)

static int sendv(SOCKET sd,const struct iovec *vec,int32_t in_vec,size_t *nbytes)
{
	int rc = 0;
	ssize_t rv;
	size_t cur_len;
	int32_t nvec = 0;
	int i,j = 0;
	DWORD dwBytes = 0;
	WSABUF *pWsaBuf;

	for(i = 0; i < in_vec; i++) {
		cur_len = vec[i].iov_len;
		nvec++;
		while(cur_len > DWORD_MAX) {
			nvec++;
			cur_len -= DWORD_MAX;
		}
	}

	pWsaBuf = (nvec <= WSABUF_ON_STACK) ? _alloca(sizeof(WSABUF) * (nvec))
		: malloc(sizeof(WSABUF) * (nvec));
	if(!pWsaBuf)
		return -1;

	for(i = 0; i < in_vec; i++) {
		char *base = vec[i].iov_base;
		cur_len = vec[i].iov_len;

		do {
			if (cur_len > DWORD_MAX) {
				pWsaBuf[j].buf = base;
				pWsaBuf[j].len = DWORD_MAX;
				cur_len -= DWORD_MAX;
				base += DWORD_MAX;
			}
			else {
				pWsaBuf[j].buf = base;
				pWsaBuf[j].len = (DWORD)cur_len;
				cur_len = 0;
			}
			j++;
		} while(cur_len > 0);
	}
	rv = WSASend(sd, pWsaBuf, nvec, &dwBytes, 0, NULL, NULL);
	if(rv == SOCKET_ERROR) {
		rc = rv;
	}

	if (nvec > WSABUF_ON_STACK)
		free(pWsaBuf);

	*nbytes = dwBytes;
	return rc;
}

#define BLOCK_MEM_COUNT 1
#define IOV_MAX (1024)

static unsigned long
net_write(struct peer *p, unsigned long wmax)
{
	struct nb_link *nl;
	struct net_buf *tdata;
	struct iovec iov[IOV_MAX];
	int niov;
	int limited;
	size_t nwritten = 0;
	int rc,err;
	unsigned long bcount;
	int block_count = 0;

	limited = wmax > 0;

	niov = 0;
	nl = BTPDQ_FIRST(&p->outq);
	assert(nl != NULL);

	if (nl->nb->type == NB_TORRENTDATA)
		block_count = 1;
	while ((niov < IOV_MAX && nl != NULL
		&& (!limited || (limited && wmax > 0)))) {
			if (nl->nb->type == NB_PIECE) {
				if (block_count >= BLOCK_MEM_COUNT)
					break;
				tdata = BTPDQ_NEXT(nl, entry)->nb;
				if (tdata->buf == NULL) {
					if (nb_torrentdata_fill(tdata, p->n->tp, nb_get_index(nl->nb),
						nb_get_begin(nl->nb), nb_get_length(nl->nb)) != 0) {
							peer_kill(p);
							return 0;
					}
				}
				block_count++;
			}
			if (niov > 0) {
				iov[niov].iov_base = nl->nb->buf;
				iov[niov].iov_len = nl->nb->len;
			} else {
				iov[niov].iov_base = nl->nb->buf + p->outq_off;
				iov[niov].iov_len = nl->nb->len - p->outq_off;
			}
			if (limited && p->ptype == BT_PEER) {
				if (iov[niov].iov_len > wmax)
					iov[niov].iov_len = wmax;
				wmax -= iov[niov].iov_len;
			}
			niov++;
			nl = BTPDQ_NEXT(nl, entry);
	}

	rc = sendv(p->sd, iov, niov,&nwritten);
	if (rc == SOCKET_ERROR) {
		err = WSAGetLastError();
		if (err == WSAEWOULDBLOCK) {
			p->t_wantwrite = btpd_seconds;
			return 0;
		} else {
			btpd_log(BTPD_L_CONN, "write error: %d\r\n", WSAGetLastError());
			peer_kill(p);
			return 0;
		}
	} 
	bcount = nwritten;

	nl = BTPDQ_FIRST(&p->outq);
	while (bcount > 0) {
		unsigned long bufdelta = nl->nb->len - p->outq_off;
		if (bcount >= bufdelta) {
			peer_sent(p, nl->nb);
			if (nl->nb->type == NB_TORRENTDATA) {
				p->n->uploaded += bufdelta;
				p->count_up += bufdelta;
			}
			bcount -= bufdelta;
			BTPDQ_REMOVE(&p->outq, nl, entry);
			nb_drop(nl->nb);
			free(nl);
			p->outq_off = 0;
			nl = BTPDQ_FIRST(&p->outq);
		} else {
			if (nl->nb->type == NB_TORRENTDATA) {
				p->n->uploaded += bcount;
				p->count_up += bcount;
			}
			p->outq_off +=  bcount;
			bcount = 0;
		}
	}
	if (!BTPDQ_EMPTY(&p->outq))
		p->t_wantwrite = btpd_seconds;
	else
		btpd_ev_disable(&p->ioev, EV_WRITE);
	p->t_lastwrite = btpd_seconds;

	return nwritten;
}

static int
net_dispatch_msg(struct peer *p, const char *buf)
{
	uint32_t index, begin, length;
	int res = 0;

	switch (p->in.msg_num) {
case MSG_CHOKE:
	peer_on_choke(p);
	break;
case MSG_UNCHOKE:
	peer_on_unchoke(p);
	break;
case MSG_INTEREST:
	peer_on_interest(p);
	break;
case MSG_UNINTEREST:
	peer_on_uninterest(p);
	break;
case MSG_HAVE:
	peer_on_have(p, dec_be32(buf));
	break;
case MSG_BITFIELD:
	if (p->npieces == 0)
		peer_on_bitfield(p, buf);
	else
		res = 1;
	break;
case MSG_REQUEST:
	if ((p->mp->flags & (PF_P_WANT|PF_I_CHOKE)) == PF_P_WANT) {
		index = dec_be32(buf);
		begin = dec_be32(buf + 4);
		length = dec_be32(buf + 8);
		if ((length > PIECE_BLOCKLEN
			|| index >= p->n->tp->npieces
			|| !cm_has_piece(p->n->tp, index)
			|| begin + length > torrent_piece_size(p->n->tp, index))) {
				btpd_log(BTPD_L_MSG, "bad request: (%u, %u, %u) from %p\r\n",
					index, begin, length, p);
				res = 1;
				break;
		}
		peer_on_request(p, index, begin, length);
	}
	break;
case MSG_CANCEL:
	index = dec_be32(buf);
	begin = dec_be32(buf + 4);
	length = dec_be32(buf + 8);
	peer_on_cancel(p, index, begin, length);
	break;
case MSG_PIECE:
	length = p->in.msg_len - 9;
	peer_on_piece(p, p->in.pc_index, p->in.pc_begin, length, buf);
	break;
case P2SP_RESP:
	length = p->in.msg_len;
	if(length == p->in.st_bytes)
	{
		peer_on_piece_p2sp(p, p->in.pc_index, p->in.pc_begin, length, buf);
	}
	else
	{
		dl_assign_request_http(p,dl_find_piece(p->n,p->in.pc_index),
			p->in.pc_begin+length,p->in.st_bytes-length);
	}
	break;
default:
	abort();
	}
	return res;
}

static int
net_mh_ok(struct peer *p)
{
	uint32_t mlen = p->in.msg_len;
	switch (p->in.msg_num) {
case MSG_CHOKE:
case MSG_UNCHOKE:
case MSG_INTEREST:
case MSG_UNINTEREST:
	return mlen == 1;
case MSG_HAVE:
	return mlen == 5;
case MSG_BITFIELD:
	return mlen == (uint32_t)ceil(p->n->tp->npieces / 8.0) + 1;
case MSG_REQUEST:
case MSG_CANCEL:
	return mlen == 13;
case MSG_PIECE:
	return mlen <= PIECE_BLOCKLEN + 9;
default:
	return 0;
	}
}

static void
net_progress(struct peer *p, size_t length)
{
	if ((p->in.state == BTP_MSGBODY && p->in.msg_num == MSG_PIECE) ||
		(p->in.state == HTTP_RESP && p->in.msg_num == P2SP_RESP)){
			p->n->downloaded += length;
			p->count_dwn += length;
	}
}

static int
net_state(struct peer *p, const char *buf)
{
	switch (p->in.state) {
case SHAKE_PSTR:
	if (memcmp(buf, "\x13""BitTorrent protocol", 20) != 0)
		goto bad;
	peer_set_in_state(p, SHAKE_INFO, 20);
	break;
case SHAKE_INFO:
	if (p->mp->flags & PF_INCOMING) {
		struct torrent *tp = torrent_by_hash(buf);
		if (tp == NULL || !net_active(tp))
			goto bad;
		p->n = tp->net;
		peer_send(p, nb_create_shake(tp));
	} else if (memcmp(buf, p->n->tp->tl->hash, 20) != 0)
		goto bad;
	peer_set_in_state(p, SHAKE_ID, 20);
	break;
case SHAKE_ID:
	if ((net_torrent_has_peer(p->n, buf)
		|| memcmp(buf, btpd_get_peer_id(), 20) == 0))
		goto bad;
	memcpy(p->mp->id, buf, 20);
	peer_on_shake(p);
	peer_set_in_state(p, BTP_MSGSIZE, 4);
	break;
case BTP_MSGSIZE:
	p->in.msg_len = dec_be32(buf);
	if (p->in.msg_len == 0)
		peer_on_keepalive(p);
	else
		peer_set_in_state(p, BTP_MSGHEAD, 1);
	break;
case BTP_MSGHEAD:
	p->in.msg_num = buf[0];
	if (!net_mh_ok(p))
		goto bad;
	else if (p->in.msg_len == 1) {
		if (net_dispatch_msg(p, buf) != 0)
			goto bad;
		peer_set_in_state(p, BTP_MSGSIZE, 4);
	} else if (p->in.msg_num == MSG_PIECE)
		peer_set_in_state(p, BTP_PIECEMETA, 8);
	else
		peer_set_in_state(p, BTP_MSGBODY, p->in.msg_len - 1);
	break;
case BTP_PIECEMETA:
	p->in.pc_index = dec_be32(buf);
	p->in.pc_begin = dec_be32(buf + 4);
	peer_set_in_state(p, BTP_MSGBODY, p->in.msg_len - 9);
	break;
case BTP_MSGBODY:
	if (net_dispatch_msg(p, buf) != 0)
		goto bad;
	peer_set_in_state(p, BTP_MSGSIZE, 4);
	break;
case HTTP_RESP:
	if (net_dispatch_msg(p,buf) != 0)
		goto bad;
	break;
default:
	abort();
	}

	return 0;
bad:
	btpd_log(BTPD_L_CONN, "bad data from %p (%u, %u, %u).\r\n",
		p, p->in.state, p->in.msg_len, p->in.msg_num);
	peer_kill(p);
	return -1;
}

static int http_get_offset(struct iovec *iov, unsigned long *off, size_t *body_len)
{
	const char *http_ok = "HTTP/1.1 206";
	
	const char *tok = "Content-Length:";
	char plen[64];
	int hlen = 0;
	char *buf = iov->iov_base;
	char *start = strstr(buf,tok);
	char *end = NULL;

	char *status = strstr(buf,http_ok);
	if(!status)
	{
		http_ok = "HTTP/1.0 206";
		status = strstr(buf,http_ok);
		if(!status)
		{
			return -1;
		}
	}
	
	if(start)
	{
		end = strstr(start,"\r\n");
		if(!end)
		{
			end = strstr(start,"\n");
		}
		if(end)
		{
			memcpy(plen,start+strlen(tok),end-start-strlen(tok));
			(*body_len) = atoi(plen);
			///////////////璁＄畻宸茬粡璇诲彇鐨刢ontent闀垮害//////////////
			end = strstr(start,"\r\n\r\n");
			if(end)
			{
				hlen = end - buf + 4;
			}
			else
			{
				end = strstr(start, "\n\n");
				if(end)
				{
					hlen = end-buf+2;
				}
				else
				{
					return -1;
				}
			}
		}
		else
		{
			return -1;
		}
	}
	else
	{
		return -1;
	}
	*off = hlen;
	return 0;
}

#define GRBUFLEN (1 << 15)
#define MIN_HTTP_SIZE (2048)
static unsigned long
net_read(struct peer *p, unsigned long rmax)
{
	WSABUF wsaData[2];
	DWORD dwBytes = 0;
	DWORD flags = 0;

	int rv = 0;
	ssize_t nread;
	size_t ndatlen;
	size_t len0,len1;

	size_t rest = p->in.buf != NULL ? p->in.st_bytes - p->in.off : 0;
	size_t iov0_len = ((p->ptype == HTTP_PEER && p->in.body_len != 0 && p->in.buf != NULL)
		|| (p->ptype == BT_PEER && p->in.buf != NULL))
		? p->in.st_bytes - p->in.off : 0;

	char buf[GRBUFLEN];
	struct iovec iov[2] = {
		{
			p->in.buf + p->in.off,
				iov0_len
		}, {
			buf,
				sizeof(buf)
		}
	};

	unsigned long off = 0;
	size_t content_len = 0;

	if (rmax > 0) {
		//进行限速处理
		if (iov[0].iov_len > rmax)
			iov[0].iov_len = rmax;
		iov[1].iov_len = min(rmax - iov[0].iov_len, iov[1].iov_len);
        
		if(p->ptype == HTTP_PEER && p->in.msg_len == 0)
		{
			//如果是http的第一次请求回复，要接满http头，需要放开一定空间
			if(iov[1].iov_len < MIN_HTTP_SIZE)
				iov[1].iov_len = MIN_HTTP_SIZE;
		}
	}
	
	len0 = iov[0].iov_len;
	len1 = iov[1].iov_len;

	wsaData[0].len = (u_long)len0;
	wsaData[0].buf = (char *)iov[0].iov_base;
	wsaData[1].len = (u_long)len1;
	wsaData[1].buf = (char *)iov[1].iov_base;

	rv = WSARecv(p->sd,wsaData,2,&dwBytes,&flags,NULL,NULL);

	if(rv == SOCKET_ERROR && WSAEWOULDBLOCK == WSAGetLastError())
		goto out;
	else if(SOCKET_ERROR == rv)
	{
		btpd_log(BTPD_L_CONN, "Read error (%d) on %p.\r\n", WSAGetLastError(), p);
		peer_kill(p);
		return 0;
	}

	if(rv == 0 && dwBytes == 0)
	{
		btpd_log(BTPD_L_CONN,"Read error (%d) on %p.\r\n",WSAGetLastError(),p);
		peer_kill(p);
		return 0;
	}

	nread = dwBytes;

	if(p->ptype == HTTP_PEER && p->in.body_len == 0)
	{
		//进行http请求回复的特殊处理	
		if(http_get_offset(&iov[1],&off,&content_len) != 0)
		{
			peer_kill(p);
			return 0;
		}
		ndatlen = nread - off;
		if(rest > 0)//对于非初次http请求 将iovec[1]中的数据追加到iovec[0]
		{
			memcpy(p->in.buf + p->in.off,(char *)iov[1].iov_base + off,ndatlen);
		}
		p->in.body_len = content_len - (nread - off);
	}
	else
		ndatlen = nread;

	if(p->ptype == HTTP_PEER)
	{
		p->in.msg_len += ndatlen;
		if(ndatlen == nread)
		{
			p->in.body_len -= nread;
		}
	}

	if(rest > 0) {
		if(ndatlen < rest) {
			p->in.off += ndatlen;
			net_progress(p,ndatlen);
			if(p->ptype == HTTP_PEER && p->in.body_len == 0)
			{
				//请求的文件长度已经不足块长度，需要发送一个新的http请求
				net_state(p,p->in.buf);
			}
			goto out;
		}

		net_progress(p, rest);
		if (net_state(p, p->in.buf) != 0)
			return nread;
		free(p->in.buf);
		p->in.buf = NULL;
		p->in.off = 0;
	}

	iov[1].iov_len = ndatlen - rest;
	while (p->in.st_bytes <= iov[1].iov_len) {
		size_t consumed = p->in.st_bytes;
		net_progress(p, consumed);
		if (net_state(p, (char*)iov[1].iov_base + off) != 0)
			return nread;
		(char *)iov[1].iov_base += consumed;
		iov[1].iov_len -= consumed;
	}

	if (iov[1].iov_len > 0) {
		net_progress(p, iov[1].iov_len);
		p->in.off = iov[1].iov_len;
		p->in.buf = btpd_malloc(p->in.st_bytes);
		memset(p->in.buf,0,p->in.st_bytes);
		memcpy(p->in.buf,(char *)iov[1].iov_base + off,iov[1].iov_len);

		if(p->ptype == HTTP_PEER && p->in.body_len == 0)
		{
			net_state(p,p->in.buf);
		}
	}

out:
	return ndatlen > 0 ? ndatlen : 0;
}

int net_connect_block(const char *ip,int port,SOCKET *sd,int tm_sec)
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
	//hints.ai_flags = AI_NUMERICHOST;
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

int
net_connect_addr(int family, struct sockaddr *sa, socklen_t salen, SOCKET *sd)
{
	if ((*sd = socket(family, SOCK_STREAM, 0)) == -1)
		return -1;

	set_nonblocking(*sd);

	if (connect(*sd, sa, salen) == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK) {
		int err = -1;
		btpd_log(BTPD_L_CONN, "Botched connection %s.\r\n", strerror(errno));
		closesocket(*sd);
		return err;
	}

	return 0;
}

int
net_connect_name(const char *ip, int port, SOCKET *sd)
{
	struct addrinfo hints, *res;
	char portstr[6];
	int error;

	assert(net_npeers < net_max_peers);

	if (sprintf(portstr, "%d", port) >= sizeof(portstr))
		return -1;
	memset(&hints, 0,sizeof(hints));
	hints.ai_family = net_af_spec();
	//hints.ai_flags = AI_NUMERICHOST;
	hints.ai_socktype = SOCK_STREAM;
	if (getaddrinfo(ip, portstr, &hints, &res) != 0)
		return -1;

	error =
		net_connect_addr(res->ai_family, res->ai_addr, res->ai_addrlen, sd);
	freeaddrinfo(res);
	return error;
}

void
net_connection_cb(SOCKET sd, short type, void *arg)
{
	SOCKET nsd;

	nsd = accept(sd, NULL, NULL);
	if (nsd == SOCKET_ERROR) {
		btpd_err("accept failed: %d\r\n", WSAGetLastError());
	}

	if (set_nonblocking(nsd) != 0) {
		closesocket(nsd);
		return;
	}

	assert(net_npeers <= net_max_peers);
	if (net_npeers == net_max_peers) {
		closesocket(nsd);
		return;
	}

	peer_create_in(nsd);

	btpd_log(BTPD_L_CONN, "got connection.\r\n");
}

static unsigned long
compute_rate_sub(unsigned long rate)
{
	if (rate > 256 * RATEHISTORY)
		return rate / RATEHISTORY;
	else
		return min(256, rate);
}

static void
compute_rates(void) {
	unsigned long tot_up = 0, tot_dwn = 0;
	struct torrent *tp;
	BTPDQ_FOREACH(tp, torrent_get_all(), entry) {
		unsigned long tp_up = 0, tp_dwn = 0;
		struct net *n = tp->net;
		struct peer *p;
		BTPDQ_FOREACH(p, &n->peers, p_entry) {
			if (p->count_up > 0 || peer_active_up(p)) {
				tp_up += p->count_up;
				p->rate_up += p->count_up - compute_rate_sub(p->rate_up);
				p->count_up = 0;
			}
			if (p->count_dwn > 0 || peer_active_down(p)) {
				tp_dwn += p->count_dwn;
				p->rate_dwn += p->count_dwn - compute_rate_sub(p->rate_dwn);
				p->count_dwn = 0;
			}
		}
		n->rate_up += tp_up - compute_rate_sub(n->rate_up);
		n->rate_dwn += tp_dwn - compute_rate_sub(n->rate_dwn);
		tot_up += tp_up;
		tot_dwn += tp_dwn;
	}
	m_rate_up += tot_up - compute_rate_sub(m_rate_up);
	m_rate_dwn += tot_dwn - compute_rate_sub(m_rate_dwn);
}

static void
net_bw_tick(void)
{
	struct peer *p;
	unsigned long bytes = 0;

	m_bw_bytes_out = net_bw_limit_out;
	m_bw_bytes_in = net_bw_limit_in;

	if (net_bw_limit_in > 0) {
		while ((p = BTPDQ_FIRST(&net_bw_readq)) != NULL && m_bw_bytes_in > 0) {
			BTPDQ_REMOVE(&net_bw_readq, p, rq_entry);
			btpd_ev_enable(&p->ioev, EV_READ);
			p->mp->flags &= ~PF_ON_READQ;

			//if(p->ptype == BT_PEER)
			bytes = net_read(p, m_bw_bytes_in);
			if(m_bw_bytes_in > bytes)
				m_bw_bytes_in -= bytes;
			else
				m_bw_bytes_in = 0;
		}
	} else {
		while ((p = BTPDQ_FIRST(&net_bw_readq)) != NULL) {
			BTPDQ_REMOVE(&net_bw_readq, p, rq_entry);
			btpd_ev_enable(&p->ioev, EV_READ);
			p->mp->flags &= ~PF_ON_READQ;
			net_read(p, 0);
		}
	}

	if (net_bw_limit_out) {
		while (((p = BTPDQ_FIRST(&net_bw_writeq)) != NULL
			&& m_bw_bytes_out > 0)) {
				BTPDQ_REMOVE(&net_bw_writeq, p, wq_entry);
				btpd_ev_enable(&p->ioev, EV_WRITE);
				p->mp->flags &= ~PF_ON_WRITEQ;
				if(p->ptype == BT_PEER)
					m_bw_bytes_out -=  net_write(p, m_bw_bytes_out);
		}
	} else {
		while ((p = BTPDQ_FIRST(&net_bw_writeq)) != NULL) {
			BTPDQ_REMOVE(&net_bw_writeq, p, wq_entry);
			btpd_ev_enable(&p->ioev, EV_WRITE);
			p->mp->flags &= ~PF_ON_WRITEQ;
			net_write(p, 0);
		}
	}
}

static void
run_peer_ticks(void)
{
	struct torrent *tp;
	struct peer *p, *next;

	BTPDQ_FOREACH_MUTABLE(p, &net_unattached, p_entry, next)
		peer_on_tick(p);

	BTPDQ_FOREACH(tp, torrent_get_all(), entry)
		BTPDQ_FOREACH_MUTABLE(p, &tp->net->peers, p_entry, next)
		peer_on_tick(p);
}

void
net_on_tick(void)
{
	run_peer_ticks();
	compute_rates();
	net_bw_tick();
}

static void
net_read_cb(struct peer *p)
{
	//if(net_bw_limit_in < 0)
	//{
	//	btpd_ev_disable(&p->ioev,EV_READ);
	//	p->mp->flags |= PF_ON_READQ;
	//	BTPDQ_INSERT_TAIL(&net_bw_readq,p,rq_entry);
	//	return;
	//}

	//if(net_bw_limit_in == 0 /*|| p->ptype == HTTP_PEER*/)
	//	net_read(p,0);
	//else if(m_bw_bytes_in > 0)
	//	m_bw_bytes_in -= net_read(p,m_bw_bytes_in);
	if (net_bw_limit_in == 0)
		net_read(p, 0);
	else 
	{
		if (m_bw_bytes_in > 0)
		{
			unsigned long bytes = net_read(p, m_bw_bytes_in);
			if(m_bw_bytes_in > bytes)
				m_bw_bytes_in -= bytes;
			else
				m_bw_bytes_in = 0;
		}
		else{
			btpd_ev_disable(&p->ioev, EV_READ);
			p->mp->flags |= PF_ON_READQ;
			BTPDQ_INSERT_TAIL(&net_bw_readq, p, rq_entry);
		}
	}
}

static void
net_write_cb(struct peer *p)
{
	if (net_bw_limit_out == 0 || p->ptype == HTTP_PEER)
		net_write(p, 0);
	else
	{
		if (m_bw_bytes_out > 0)
		{
			unsigned long bytes = net_write(p, m_bw_bytes_out);
			if(m_bw_bytes_out > bytes)
				m_bw_bytes_out -= bytes;
			else
				m_bw_bytes_out = 0;
		}
		else {
			btpd_ev_disable(&p->ioev, EV_WRITE);
			p->mp->flags |= PF_ON_WRITEQ;
			BTPDQ_INSERT_TAIL(&net_bw_writeq, p, wq_entry);
		}
	}
}

void
net_io_cb(SOCKET sd, short type, void *arg)
{
	switch (type) {
case EV_READ:
	net_read_cb(arg);
	break;
case EV_WRITE:
	net_write_cb(arg);
	break;
default:
	abort();
	}
}

int
net_af_spec(void)
{
	if (net_ipv4 && net_ipv6)
		return AF_UNSPEC;
	else if (net_ipv4)
		return AF_INET;
	else
		return AF_INET6;
}

void
net_shutdown(void)
{
	int i = 0;
	for (i = 0; i < m_nlisteners; i++) {
		btpd_ev_del(&m_net_listeners[i].ev);
		closesocket(m_net_listeners[i].sd);
	}
}

extern int safe_max_fds;
int 
net_init(void)
{
	int safe_fds;
	int count = 0, flag = 1, found_ipv4 = 0, found_ipv6 = 0;
	struct sockaddr_in addr;
	socklen_t len;
	SOCKET sd;
	char portstr[6];
	struct addrinfo hints,*res,*ai;

	m_bw_bytes_out = net_bw_limit_out;
	m_bw_bytes_in = net_bw_limit_in;

	safe_fds = safe_max_fds;
	if (net_max_peers == 0 || net_max_peers > safe_fds)
		net_max_peers = safe_fds;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_ADDRCONFIG | AI_PASSIVE;
	hints.ai_family = net_af_spec();
	hints.ai_socktype = SOCK_STREAM;

	sprintf(portstr, "%hu", net_port);
	if ((errno = getaddrinfo(NULL, portstr, &hints, &res)) != 0)
	{
		btpd_err("getaddrinfo failed (%s).\r\n", gai_strerror(errno));
		return -1;
	}
	for (ai = res; ai != NULL; ai = ai->ai_next) {
		count++;
		if (ai->ai_family == AF_INET)
			found_ipv4 = 1;
		else
			found_ipv6 = 1;
	}
	net_ipv4 = found_ipv4;
	net_ipv6 = found_ipv6;
	if (!net_ipv4 && !net_ipv6)
	{
		btpd_err("no usable address found. wrong use of -4/-6 perhaps.\r\n");
		return -1;
	}
	m_nlisteners = count;
	m_net_listeners = btpd_calloc(count, sizeof(*m_net_listeners));
	for (ai = res; ai != NULL; ai = ai->ai_next) {
		count--;
		if ((sd = socket(ai->ai_family, ai->ai_socktype, 0)) == -1)
		{
			btpd_err("failed to create socket (%s).\r\n", strerror(errno));
			return -1;
		}
		setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
#ifdef IPV6_V6ONLY
		if (ai->ai_family == AF_INET6)
			setsockopt(sd, IPPROTO_IPV6, IPV6_V6ONLY, &flag, sizeof(flag));
#endif
		if (bind(sd, ai->ai_addr, ai->ai_addrlen) == -1)
		{
			btpd_err("bind failed (%s).\r\n", strerror(errno));
			return -1;
		}
		len = sizeof(addr);
		if(getsockname(sd, (struct sockaddr *)&addr, &len) != 0)
		{
			return -1;
		}
		net_port = ntohs(addr.sin_port);
		listen(sd, 10);
		set_nonblocking(sd);
		m_net_listeners[count].sd = sd;
		btpd_ev_new(&m_net_listeners[count].ev, sd, EV_READ,
			net_connection_cb, NULL);
	}
	freeaddrinfo(res);
	return 0;
}
