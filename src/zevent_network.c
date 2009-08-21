#include "apr_network_io.h"
#include "apr_strings.h"
#include "apr_lib.h"
#include "stdlib.h"

#define APR_WANT_STRFUNC
#include "apr_want.h"

#include "zevent_network.h"
//#include "mpm_common.h"

ZEVENT_DECLARE_DATA zevent_listen_rec *zevent_listeners = NULL;

static zevent_listen_rec *old_listeners;
static int zevent_listenbacklog;
static int send_buffer_size;
static int receive_buffer_size;

/* TODO: make_sock is just begging and screaming for APR abstraction */
static apr_status_t make_sock(apr_pool_t *p, zevent_listen_rec *server)
{
    apr_socket_t *s = server->sd;
    int one = 1;
    apr_status_t stat;

#ifndef WIN32
    stat = apr_socket_opt_set(s, APR_SO_REUSEADDR, one);
    if (stat != APR_SUCCESS && stat != APR_ENOTIMPL) {
        apr_socket_close(s);
        return stat;
    }
#endif

    stat = apr_socket_opt_set(s, APR_SO_KEEPALIVE, one);
    if (stat != APR_SUCCESS && stat != APR_ENOTIMPL) {
        apr_socket_close(s);
        return stat;
    }

    /*
     * To send data over high bandwidth-delay connections at full
     * speed we must force the TCP window to open wide enough to keep the
     * pipe full.  The default window size on many systems
     * is only 4kB.  Cross-country WAN connections of 100ms
     * at 1Mb/s are not impossible for well connected sites.
     * If we assume 100ms cross-country latency,
     * a 4kB buffer limits throughput to 40kB/s.
     *
     * To avoid this problem I've added the SendBufferSize directive
     * to allow the web master to configure send buffer size.
     *
     * The trade-off of larger buffers is that more kernel memory
     * is consumed.  YMMV, know your customers and your network!
     *
     * -John Heidemann <johnh@isi.edu> 25-Oct-96
     *
     * If no size is specified, use the kernel default.
     */
    if (send_buffer_size) {
        stat = apr_socket_opt_set(s, APR_SO_SNDBUF,  send_buffer_size);
        if (stat != APR_SUCCESS && stat != APR_ENOTIMPL) {
		;
            /* not a fatal error */
        }
    }
    if (receive_buffer_size) {
        stat = apr_socket_opt_set(s, APR_SO_RCVBUF, receive_buffer_size);
        if (stat != APR_SUCCESS && stat != APR_ENOTIMPL) {
		;
            /* not a fatal error */
        }
    }

#if APR_TCP_NODELAY_INHERITED
    zevent_sock_disable_nagle(s);
#endif

    if ((stat = apr_socket_bind(s, server->bind_addr)) != APR_SUCCESS) {
        apr_socket_close(s);
        return stat;
    }

    if ((stat = apr_socket_listen(s, zevent_listenbacklog)) != APR_SUCCESS) {
        apr_socket_close(s);
        return stat;
    }

#ifdef WIN32
    /* I seriously doubt that this would work on Unix; I have doubts that
     * it entirely solves the problem on Win32.  However, since setting
     * reuseaddr on the listener -prior- to binding the socket has allowed
     * us to attach to the same port as an already running instance of
     * Apache, or even another web server, we cannot identify that this
     * port was exclusively granted to this instance of Apache.
     *
     * So set reuseaddr, but do not attempt to do so until we have the
     * parent listeners successfully bound.
     */
    stat = apr_socket_opt_set(s, APR_SO_REUSEADDR, one);
    if (stat != APR_SUCCESS && stat != APR_ENOTIMPL) {
        zevent_log_perror(APLOG_MARK, APLOG_CRIT, stat, p,
                    "make_sock: for address %pI, apr_socket_opt_set: (SO_REUSEADDR)",
                     server->bind_addr);
        apr_socket_close(s);
        return stat;
    }
#endif

    server->sd = s;
    server->active = 1;

    server->accept_func = unixd_accept;

    return APR_SUCCESS;
}


static const char *alloc_listener(apr_pool_t *p, char *addr,
                                  apr_port_t port, const char* proto)
{
    zevent_listen_rec **walk, *last;
    apr_status_t status;
    apr_sockaddr_t *sa;
    int found_listener = 0;

    /* see if we've got an old listener for this address:port */
    for (walk = &old_listeners; *walk;) {
        sa = (*walk)->bind_addr;
        /* Some listeners are not real so they will not have a bind_addr. */
        if (sa) {
            zevent_listen_rec *new;
            apr_port_t oldport;

            oldport = sa->port;
            /* If both ports are equivalent, then if their names are equivalent,
             * then we will re-use the existing record.
             */
            if (port == oldport &&
                ((!addr && !sa->hostname) ||
                 ((addr && sa->hostname) && !strcmp(sa->hostname, addr)))) {
                new = *walk;
                *walk = new->next;
                new->next = zevent_listeners;
                zevent_listeners = new;
                found_listener = 1;
                continue;
            }
        }

        walk = &(*walk)->next;
    }

    if (found_listener) {
        return NULL;
    }

    if ((status = apr_sockaddr_info_get(&sa, addr, APR_UNSPEC, port, 0,
                                        p))
        != APR_SUCCESS) {
	    ;
    }

    /* Initialize to our last configured zevent_listener. */
    last = zevent_listeners;
    while (last && last->next) {
        last = last->next;
    }

    while (sa) {

        zevent_listen_rec *new;

        /* this has to survive restarts */
        new = apr_palloc(p, sizeof(zevent_listen_rec));
        new->active = 0;
        new->next = 0;
        new->bind_addr = sa;
        new->protocol = apr_pstrdup(p, proto);

        /* Go to the next sockaddr. */
        sa = sa->next;

        status = apr_socket_create(&new->sd, new->bind_addr->family,
                                    SOCK_STREAM, 0, p);

        if (status != APR_SUCCESS) {
		;
        }

        /* We need to preserve the order returned by getaddrinfo() */
        if (last == NULL) {
            zevent_listeners = last = new;
        } else {
            last->next = new;
            last = new;
        }
    }

    return NULL;
}

ZEVENT_DECLARE_NONSTD(void) zevent_close_listeners(void)
{
    zevent_listen_rec *lr;

    for (lr = zevent_listeners; lr; lr = lr->next) {
        apr_socket_close(lr->sd);
        lr->active = 0;
    }
}


static apr_status_t close_listeners_on_exec(void *v)
{
    zevent_close_listeners();
    return APR_SUCCESS;
}

/* Evaluates to true if the (apr_sockaddr_t *) addr argument is the
 * IPv4 match-any-address, 0.0.0.0. */
#define IS_INADDR_ANY(addr) ((addr)->family == APR_INET \
                             && (addr)->sa.sin.sin_addr.s_addr == INADDR_ANY)

/* Evaluates to true if the (apr_sockaddr_t *) addr argument is the
 * IPv6 match-any-address, [::]. */
#define IS_IN6ADDR_ANY(addr) ((addr)->family == APR_INET6 \
                              && IN6_IS_ADDR_UNSPECIFIED(&(addr)->sa.sin6.sin6_addr))

/**
 * Create, open, listen, and bind all sockets.
 * @param process The process record for the currently running server
 * @return The number of open sockets
 */
ZEVENT_DECLARE(int) zevent_open_listeners(apr_pool_t *pool)
{
    zevent_listen_rec *lr;
    zevent_listen_rec *next;
    zevent_listen_rec *previous;
    int num_open;
    const char *userdata_key = "zevent_open_listeners";
    void *data;
    /* Don't allocate a default listener.  If we need to listen to a
     * port, then the user needs to have a Listen directive in their
     * config file.
     */
    num_open = 0;
    previous = NULL;
    for (lr = zevent_listeners; lr; previous = lr, lr = lr->next) {
        if (lr->active) {
            ++num_open;
        }
        else {
           if (make_sock(pool, lr) == APR_SUCCESS) {
                ++num_open;
                lr->active = 1;
            }
            else {
                return -1;
            }
        }
    }

    /* close the old listeners */
    for (lr = old_listeners; lr; lr = next) {
        apr_socket_close(lr->sd);
        lr->active = 0;
        next = lr->next;
    }
    old_listeners = NULL;

    /* we come through here on both passes of the open logs phase
     * only register the cleanup once... otherwise we try to close
     * listening sockets twice when cleaning up prior to exec
     */
    apr_pool_userdata_get(&data, userdata_key, pool);
    if (!data) {
        apr_pool_userdata_set((const void *)1, userdata_key,
                              apr_pool_cleanup_null, pool);
        apr_pool_cleanup_register(pool, NULL, apr_pool_cleanup_null,
                                  close_listeners_on_exec);
    }

    return num_open ? 0 : -1;
}



ZEVENT_DECLARE(void) zevent_listen_pre_config(void)
{
    old_listeners = zevent_listeners;
    zevent_listeners = NULL;
    zevent_listenbacklog = DEFAULT_LISTENBACKLOG;
}

ZEVENT_DECLARE(void) zevent_str_tolower(char *str)
{
    while (*str) {
        *str = apr_tolower(*str);
        ++str;
    }
}

ZEVENT_DECLARE_NONSTD(const char *) zevent_set_listener(apr_pool_t *p,int argc,const char *argv[])
{
    char *host, *scope_id, *proto;
    apr_port_t port;
    apr_status_t rv;

    rv = apr_parse_addr_port(&host, &scope_id, &port, argv[0], p);
    if (rv != APR_SUCCESS) {
        return "Invalid address or port";
    }

    if (host && !strcmp(host, "*")) {
        host = NULL;
    }

    if (scope_id) {
        /* XXX scope id support is useful with link-local IPv6 addresses */
        return "Scope id is not supported";
    }

    if (!port) {
        return "Port must be specified";
    }

    if (argc != 2) {
        proto = "http";
    }
    else {
        proto = apr_pstrdup(p, argv[1]);
        zevent_str_tolower(proto);
    }

    return alloc_listener(p, host, port, proto);
}

ZEVENT_DECLARE_NONSTD(const char *) zevent_set_listenbacklog(const char *arg)
{
    int b;

    b = atoi(arg);
    if (b < 1) {
        return "ListenBacklog must be > 0";
    }

    zevent_listenbacklog = b;
    return NULL;
}

ZEVENT_DECLARE_NONSTD(const char *) zevent_set_send_buffer_size(const char *arg)
{
    int s = atoi(arg);


    if (s < 512 && s != 0) {
        return "SendBufferSize must be >= 512 bytes, or 0 for system default.";
    }

    send_buffer_size = s;
    return NULL;
}

ZEVENT_DECLARE_NONSTD(const char *) zevent_set_receive_buffer_size(const char *arg)
{
    int s = atoi(arg);

    if (s < 512 && s != 0) {
        return "ReceiveBufferSize must be >= 512 bytes, or 0 for system default.";
    }

    receive_buffer_size = s;
    return NULL;
}
ZEVENT_DECLARE(apr_status_t) unixd_accept(void **accepted, zevent_listen_rec *lr,
                                        apr_pool_t *ptrans)
{
    apr_socket_t *csd;
    apr_status_t status;
#ifdef _OSD_POSIX
    int sockdes;
#endif

    *accepted = NULL;
    status = apr_socket_accept(&csd, lr->sd, ptrans);
    if (status == APR_SUCCESS) {
        *accepted = csd;
#ifdef _OSD_POSIX
        apr_os_sock_get(&sockdes, csd);
        if (sockdes >= FD_SETSIZE) {
            apr_socket_close(csd);
            return APR_EINTR;
        }
#endif
        return APR_SUCCESS;
    }

    if (APR_STATUS_IS_EINTR(status)) {
        return status;
    }
    /* Our old behaviour here was to continue after accept()
     * errors.  But this leads us into lots of troubles
     * because most of the errors are quite fatal.  For
     * example, EMFILE can be caused by slow descriptor
     * leaks (say in a 3rd party module, or libc).  It's
     * foolish for us to continue after an EMFILE.  We also
     * seem to tickle kernel bugs on some platforms which
     * lead to never-ending loops here.  So it seems best
     * to just exit in most cases.
     */
    switch (status) {
#if defined(HPUX11) && defined(ENOBUFS)
        /* On HPUX 11.x, the 'ENOBUFS, No buffer space available'
         * error occurs because the accept() cannot complete.
         * You will not see ENOBUFS with 10.20 because the kernel
         * hides any occurrence from being returned to user space.
         * ENOBUFS with 11.x's TCP/IP stack is possible, and could
         * occur intermittently. As a work-around, we are going to
         * ignore ENOBUFS.
         */
        case ENOBUFS:
#endif

#ifdef EPROTO
        /* EPROTO on certain older kernels really means
         * ECONNABORTED, so we need to ignore it for them.
         * See discussion in new-httpd archives nh.9701
         * search for EPROTO.
         *
         * Also see nh.9603, search for EPROTO:
         * There is potentially a bug in Solaris 2.x x<6,
         * and other boxes that implement tcp sockets in
         * userland (i.e. on top of STREAMS).  On these
         * systems, EPROTO can actually result in a fatal
         * loop.  See PR#981 for example.  It's hard to
         * handle both uses of EPROTO.
         */
        case EPROTO:
#endif
#ifdef ECONNABORTED
        case ECONNABORTED:
#endif
        /* Linux generates the rest of these, other tcp
         * stacks (i.e. bsd) tend to hide them behind
         * getsockopt() interfaces.  They occur when
         * the net goes sour or the client disconnects
         * after the three-way handshake has been done
         * in the kernel but before userland has picked
         * up the socket.
         */
#ifdef ECONNRESET
        case ECONNRESET:
#endif
#ifdef ETIMEDOUT
        case ETIMEDOUT:
#endif
#ifdef EHOSTUNREACH
        case EHOSTUNREACH:
#endif
#ifdef ENETUNREACH
        case ENETUNREACH:
#endif
        /* EAGAIN/EWOULDBLOCK can be returned on BSD-derived
         * TCP stacks when the connection is aborted before
         * we call connect, but only because our listener
         * sockets are non-blocking (ZEVENT_NONBLOCK_WHEN_MULTI_LISTEN)
         */
#ifdef EAGAIN
        case EAGAIN:
#endif
#ifdef EWOULDBLOCK
#if !defined(EAGAIN) || EAGAIN != EWOULDBLOCK
        case EWOULDBLOCK:
#endif
#endif
            break;
#ifdef ENETDOWN
        case ENETDOWN:
            /*
             * When the network layer has been shut down, there
             * is not much use in simply exiting: the parent
             * would simply re-create us (and we'd fail again).
             * Use the CHILDFATAL code to tear the server down.
             * @@@ Martin's idea for possible improvement:
             * A different approach would be to define
             * a new APEXIT_NETDOWN exit code, the reception
             * of which would make the parent shutdown all
             * children, then idle-loop until it detected that
             * the network is up again, and restart the children.
             * Ben Hyde noted that temporary ENETDOWN situations
             * occur in mobile IP.
             */
            return APR_EGENERAL;
#endif /*ENETDOWN*/

#ifdef TPF
        case EINACT:
            return APR_EGENERAL;
            break;
        default:
            return APR_EGENERAL;
#else
        default:
            return APR_EGENERAL;
#endif
    }
    return status;
}

/*
 * More machine-dependent networking gooo... on some systems,
 * you've got to be *really* sure that all the packets are acknowledged
 * before closing the connection, since the client will not be able
 * to see the last response if their TCP buffer is flushed by a RST
 * packet from us, which is what the server's TCP stack will send
 * if it receives any request data after closing the connection.
 *
 * In an ideal world, this function would be accomplished by simply
 * setting the socket option SO_LINGER and handling it within the
 * server's TCP stack while the process continues on to the next request.
 * Unfortunately, it seems that most (if not all) operating systems
 * block the server process on close() when SO_LINGER is used.
 * For those that don't, see USE_SO_LINGER below.  For the rest,
 * we have created a home-brew lingering_close.
 *
 * Many operating systems tend to block, puke, or otherwise mishandle
 * calls to shutdown only half of the connection.  You should define
 * NO_LINGCLOSE in zevent_config.h if such is the case for your system.
 */
#ifndef MAX_SECS_TO_LINGER
#define MAX_SECS_TO_LINGER 30
#endif

/* we now proceed to read from the client until we get EOF, or until
 * MAX_SECS_TO_LINGER has passed.  the reasons for doing this are
 * documented in a draft:
 *
 * http://www.ics.uci.edu/pub/ietf/http/draft-ietf-http-connection-00.txt
 *
 * in a nutshell -- if we don't make this effort we risk causing
 * TCP RST packets to be sent which can tear down a connection before
 * all the response data has been sent to the client.
 */
#define SECONDS_TO_LINGER  2
ZEVENT_DECLARE(void) zevent_lingering_close(apr_socket_t *csd)
{
    char dummybuf[512];
    apr_size_t nbytes;
    apr_time_t timeup = 0;

    if (!csd) {
        return;
    }

    /* Close the connection, being careful to send out whatever is still
     * in our buffers.  If possible, try to avoid a hard close until the
     * client has ACKed our FIN and/or has stopped sending us data.
     */

    /* Send any leftover data to the client, but never try to again */

    /* Shut down the socket for write, which will send a FIN
     * to the peer.
     */
    if (apr_socket_shutdown(csd, APR_SHUTDOWN_WRITE) != APR_SUCCESS
        ) {
        apr_socket_close(csd);
        return;
    }

    /* Read available data from the client whilst it continues sending
     * it, for a maximum time of MAX_SECS_TO_LINGER.  If the client
     * does not send any data within 2 seconds (a value pulled from
     * Apache 1.3 which seems to work well), give up.
     */
    apr_socket_timeout_set(csd, apr_time_from_sec(SECONDS_TO_LINGER));
    apr_socket_opt_set(csd, APR_INCOMPLETE_READ, 1);

    /* The common path here is that the initial apr_socket_recv() call
     * will return 0 bytes read; so that case must avoid the expensive
     * apr_time_now() call and time arithmetic. */

    do {
        nbytes = sizeof(dummybuf);
        if (apr_socket_recv(csd, dummybuf, &nbytes) || nbytes == 0)
            break;

        if (timeup == 0) {
            /* First time through; calculate now + 30 seconds. */
            timeup = apr_time_now() + apr_time_from_sec(MAX_SECS_TO_LINGER);
            continue;
        }
    } while (apr_time_now() < timeup);

    apr_socket_close(csd);
    return;
}

