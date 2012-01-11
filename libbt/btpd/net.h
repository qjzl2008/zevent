#ifndef BTPD_NET_H
#define BTPD_NET_H

#include <WinSock2.h>
#include <WS2tcpip.h>

#define MSG_CHOKE       0
#define MSG_UNCHOKE     1
#define MSG_INTEREST    2
#define MSG_UNINTEREST  3
#define MSG_HAVE        4
#define MSG_BITFIELD    5
#define MSG_REQUEST     6
#define MSG_PIECE       7
#define MSG_CANCEL      8
#define P2SP_RESP 	9

#define RATEHISTORY 20

extern struct peer_tq net_unattached;
extern struct peer_tq net_bw_readq;
extern struct peer_tq net_bw_writeq;
extern unsigned net_npeers;

struct iovec
{
    void *iov_base;
    size_t iov_len;
};

int net_init(void);

void net_on_tick(void);

void net_create(struct torrent *tp);
void net_kill(struct torrent *tp);

void net_start(struct torrent *tp);
void net_stop(struct torrent *tp);
int net_active(struct torrent *tp);

void net_ban_peer(struct net *n, struct meta_peer *mp);
int net_torrent_has_peer(struct net *n, const uint8_t *id);

void net_io_cb(SOCKET sd, short type, void *arg);

int net_connect_addr(int family, struct sockaddr *sa, socklen_t salen,
    SOCKET *sd);
int net_connect_name(const char *ip, int port, SOCKET *sd);

int net_connect_block(const char *ip,int port,SOCKET *sd,int tm_sec);

int net_af_spec(void);

#endif
