/**
 * @file  zevent_network.h
 *
 * @{
 */

#ifndef ZEVENT_LISTEN_H
#define ZEVENT_LISTEN_H

#include "apr_network_io.h"
#include "zevent_config.h"
#ifdef __cplusplus
extern "C" {
#endif

/* The maximum length of the queue of pending connections, as defined
 * by listen(2).  Under some systems, it should be increased if you
 * are experiencing a heavy TCP SYN flood attack.
 *
 * It defaults to 511 instead of 512 because some systems store it 
 * as an 8-bit datatype; 512 truncated to 8-bits is 0, while 511 is 
 * 255 when truncated.
 */
#ifndef DEFAULT_LISTENBACKLOG
#define DEFAULT_LISTENBACKLOG 511
#endif
 

typedef struct zevent_listen_rec zevent_listen_rec;
typedef apr_status_t (*accept_function)(void **csd, zevent_listen_rec *lr, apr_pool_t *ptrans);

/**
 * @brief Apache's listeners record.  
 *
 * These are used in the Multi-Processing Modules
 * to setup all of the sockets for the MPM to listen to and accept on.
 */
struct zevent_listen_rec {
    /**
     * The next listener in the list
     */
    zevent_listen_rec *next;
    /**
     * The actual socket 
     */
    apr_socket_t *sd;
    /**
     * The sockaddr the socket should bind to
     */
    apr_sockaddr_t *bind_addr;
    /**
     * The accept function for this socket
     */
    accept_function accept_func;
    /**
     * Is this socket currently active 
     */
    int active;
    /**
     * The default protocol for this listening socket.
     */
    const char* protocol;
};

/**
 * The global list of zevent_listen_rec structures
 */
ZEVENT_DECLARE_DATA extern zevent_listen_rec *zevent_listeners;

ZEVENT_DECLARE_NONSTD(const char *) zevent_set_listener(apr_pool_t *p,int argc,const char *argv[]);

ZEVENT_DECLARE_NONSTD(void) zevent_close_listeners(void);
ZEVENT_DECLARE(int) zevent_open_listeners(apr_pool_t *pool);

ZEVENT_DECLARE(apr_status_t) unixd_accept(void **accepted, zevent_listen_rec *lr, apr_pool_t *ptrans);

ZEVENT_DECLARE(void) zevent_lingering_close(apr_socket_t *csd);

#if defined(TCP_NODELAY) && !defined(MPE) && !defined(TPF)
/**
 * Turn off the nagle algorithm for the specified socket.  The nagle algorithm
 * says that we should delay sending partial packets in the hopes of getting
 * more data.  There are bad interactions between persistent connections and
 * Nagle's algorithm that have severe performance penalties.
 * @param s The socket to disable nagle for.
 */
void zevent_sock_disable_nagle(apr_socket_t *s);
#else
#define zevent_sock_disable_nagle(s)        /* NOOP */
#endif

#ifdef __cplusplus
}
#endif

#endif
/** @} */
