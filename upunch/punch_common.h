#ifndef _P2P_H_
#define _P2P_H_

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(WIN32)
#define P2P_DECLARE(type)	type
#define P2P_DECLARE_NONSTD(type)	type
#define P2P_DECLARE_DATA
#else
#define P2P_DECLARE(type)	__declspec(dllexport) type __stdcall
#define P2P_DECLARE_NONSTD(type)	__declspec(dllexport) type
#define P2P_DECLARE_DATA	__declspec(dllexport)
#endif


/* Some capability defaults which can be reset for particular platforms. */
#define P2P_HAVE_DAEMON 1

/* Moved here to define _CRT_SECURE_NO_WARNINGS before all the including takes place */
#ifdef WIN32
#include "win32/p2p_win32.h"
#undef P2P_HAVE_DAEMON
#endif

#include <time.h>
#include <ctype.h>
#include <stdlib.h>

#ifndef WIN32
#include <netdb.h>
#endif

#ifndef _MSC_VER
#include <getopt.h>
#endif /* #ifndef _MSC_VER */

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>

#ifndef WIN32
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <pthread.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>

#define closesocket(a) close(a)
#endif /* #ifndef WIN32 */

#include <string.h>

#include <stdarg.h>

#include "p2p_wire.h"

#ifndef WIN32

#define SOCKET int
#endif /* #ifndef WIN32 */

/* P2P packet header indicators. */
#define MSG_TYPE_REGISTER               1
#define MSG_TYPE_DEREGISTER             2
#define MSG_TYPE_PACKET                 3
#define MSG_TYPE_REGISTER_ACK           4
#define MSG_TYPE_REGISTER_SUPER         5
#define MSG_TYPE_REGISTER_SUPER_ACK     6
#define MSG_TYPE_REGISTER_SUPER_NAK     7

struct peer_info {
    struct peer_info *  next;
    p2p_community_t     community_name;
    p2p_sock_t          sock;
    time_t              last_seen;
};

#define P2P_EDGE_SN_HOST_SIZE 48
typedef char p2p_sn_name_t[P2P_EDGE_SN_HOST_SIZE];
#define P2P_EDGE_NUM_SUPERNODES 2
#define P2P_EDGE_SUP_ATTEMPTS   3

typedef int (*callback_t)(void*);
typedef struct p2p_edge         p2p_edge_t;

/** Main structure type for edge. */
struct p2p_edge
{
	int                 daemon;                 /**< Non-zero if edge should detach and run in the background. */

	p2p_sock_t          supernode;

	size_t              sn_idx;                 /**< Currently active supernode. */
	size_t              sn_num;                 /**< Number of supernode addresses defined. */
	p2p_sn_name_t       sn_ip_array[P2P_EDGE_NUM_SUPERNODES];
	int                 sn_wait;                /**< Whether we are waiting for a supernode response. */

	p2p_community_t     community_name;         /**< The community. 16 full octets. */

	SOCKET                 udp_sock;
	SOCKET                 udp_mgmt_sock;          /**< socket for status info. */

	struct peer_info *  known_peers;            /**< Edges we are connected to. */
	struct peer_info *  pending_peers;          /**< Edges we have tried to register with. */
	time_t              last_register_req;      /**< Check if time to re-register with super*/
	size_t              register_lifetime;      /**< Time distance after last_register_req at which to re-register. */
	time_t              last_p2p;               /**< Last time p2p traffic was received. */
	time_t              last_sup;               /**< Last time a packet arrived from supernode. */
	size_t              sup_attempts;           /**< Number of remaining attempts to this supernode. */
	p2p_cookie_t        last_cookie;            /**< Cookie sent in last REGISTER_SUPER. */

	time_t              start_time;             /**< For calculating uptime */
	int					run_punching;
	HANDLE				hPhunching;

	/* Statistics */
	size_t              tx_p2p;
	size_t              rx_p2p;
	size_t              tx_sup;
	size_t              rx_sup;

	//call back funciont
	callback_t			reg_callback;
	callback_t			puch_callback;
};


/* ************************************** */

#define TRACE_ERROR     0, __FILE__, __LINE__
#define TRACE_WARNING   1, __FILE__, __LINE__
#define TRACE_NORMAL    2, __FILE__, __LINE__
#define TRACE_INFO      3, __FILE__, __LINE__
#define TRACE_DEBUG     4, __FILE__, __LINE__

/* ************************************** */

#define SUPERNODE_IP    "127.0.0.1"
#define SUPERNODE_PORT  1234

/* ************************************** */

#ifndef max
#define max(a, b) ((a < b) ? b : a)
#endif

#ifndef min
#define min(a, b) ((a > b) ? b : a)
#endif

/* ************************************** */

/* Variables */
/* extern TWOFISH *tf; */
extern int traceLevel;
extern int useSyslog;

/* Functions */
extern void traceEvent(int eventTraceLevel, char* file, int line, char * format, ...);

extern SOCKET open_socket(int local_port, int bind_any);

extern char* intoa(uint32_t addr, char* buf, uint16_t buf_len);
extern char * sock_to_cstr( p2p_sock_str_t out,
                            const p2p_sock_t * sock );

extern int sock_equal( const p2p_sock_t * a, 
                       const p2p_sock_t * b );

extern void hexdump(const uint8_t * buf, size_t len);

void print_p2p_version();


/* Operations on peer_info lists. */
struct peer_info * find_peer_by_addr( struct peer_info * list,
                                     const p2p_sock_t *peer );
void   peer_list_add( struct peer_info * * list,
                      struct peer_info * peer );
size_t peer_list_size( const struct peer_info * list );
size_t purge_peer_list( struct peer_info ** peer_list, 
                        time_t purge_before );
size_t clear_peer_list( struct peer_info ** peer_list );
size_t purge_expired_registrations( struct peer_info ** peer_list );

/* version.c */
extern char *p2p_sw_version, *p2p_sw_osName, *p2p_sw_buildDate;

#ifdef __cplusplus
}
#endif

#endif /* _P2P_H_ */
