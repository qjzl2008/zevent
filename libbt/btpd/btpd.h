#ifndef BTPD_H
#define BTPD_H

#include <sys/types.h>
#include <sys/stat.h>
//#include <sys/time.h>

//#include <sys/socket.h>
//#include <netinet/in.h>
//#include <arpa/inet.h>
//#include <netdb.h>
#ifndef AI_ADDRCONFIG
#define AI_ADDRCONFIG 0
#endif

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
//#include <inttypes.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <strings.h>
//#include <unistd.h>

#include <benc.h>
#include <btpd_if.h>
#include <evloop.h>
#include <metainfo.h>
#include <queue.h>
#include <subr.h>

#include "active.h"
#include "hashtable.h"
#include "net_buf.h"
#include "net_types.h"
#include "net.h"
#include "peer.h"
#include "tlib.h"
#include "torrent.h"
#include "download.h"
#include "upload.h"
#include "content.h"
#include "opts.h"
#include "tracker_req.h"
#include "bt.h"

#define BTPD_VERSION "btpd/0.15"

#define BTPD_L_ALL      0xffffffff
#define BTPD_L_ERROR    0x00000001
#define BTPD_L_TR       0x00000002
#define BTPD_L_CONN     0x00000004
#define BTPD_L_MSG      0x00000008
#define BTPD_L_BTPD     0x00000010
#define BTPD_L_POL      0x00000020
#define BTPD_L_BAD      0x00000040

extern long btpd_seconds;

extern volatile int daemon_stop;

struct bt_t{
	HANDLE th_bt; //bt daemonÏß³Ì¾ä±ú
	struct ipc *cmdpipe;
	short bt_port;//bt¶Ë¿Ú
};

void log_init();
void log_fini();

int btpd_init(bt_t *bt);

int ipc_init(bt_t *bt);
int ipc_fini(bt_t *bt);

void btpd_log(uint32_t type, const char *fmt, ...);

void btpd_err(const char *fmt, ...);

void *btpd_malloc(size_t size);
void *btpd_calloc(size_t nmemb, size_t size);

void btpd_ev_new(struct fdev *ev, SOCKET fd, uint16_t flags, evloop_cb_t cb,
    void *arg);
void btpd_ev_del(struct fdev *ev);
void btpd_ev_enable(struct fdev *ev, uint16_t flags);
void btpd_ev_disable(struct fdev *ev, uint16_t flags);
void btpd_timer_add(struct timeout *to, struct timespec *ts);
void btpd_timer_del(struct timeout *to);

void btpd_shutdown(void);
int btpd_is_stopping(void);

int btpd_id_eq(const void *k1, const void *k2);
uint32_t btpd_id_hash(const void *k);

const uint8_t *btpd_get_peer_id(void);

int btpd_id_eq(const void *id1, const void *id2);
uint32_t btpd_id_hash(const void *id);

void td_acquire_lock(void);
void td_release_lock(void);

void td_post(void (*cb)(void *), void *arg);
void td_post_end();
#define td_post_begin td_acquire_lock

int td_init(void);
int td_fini(void);

typedef struct ai_ctx * aictx_t;
aictx_t btpd_addrinfo(const char *node, uint16_t port, struct addrinfo *hints,
    void (*cb)(void *, int, struct addrinfo *), void *arg);
void btpd_addrinfo_cancel(aictx_t ctx); 
void btpd_addrinfo_stop();


typedef struct nameconn *nameconn_t;
nameconn_t btpd_name_connect(const char *name, short port,
    void (*cb)(void *, int, SOCKET), void *arg);
void btpd_name_connect_cancel(nameconn_t nc);

#endif
