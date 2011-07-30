#include "punch_common.h"
#include "p2p_punch.h"
#include <assert.h>
#include <sys/stat.h>

#if defined(DEBUG)
#define SOCKET_TIMEOUT_INTERVAL_SECS    5
#define REGISTER_SUPER_INTERVAL_DFL     20 /* sec */
#else  /* #if defined(DEBUG) */
#define SOCKET_TIMEOUT_INTERVAL_SECS    10
#define REGISTER_SUPER_INTERVAL_DFL     60 /* sec */
#endif /* #if defined(DEBUG) */

#define REGISTER_SUPER_INTERVAL_MIN     20   /* sec */
#define REGISTER_SUPER_INTERVAL_MAX     3600 /* sec */

#define P2P_EDGE_MGMT_PORT              5644


/** Return the IP address of the current supernode in the ring. */
static const char * supernode_ip( const p2p_edge_t * eee )
{
    return (eee->sn_ip_array)[eee->sn_idx];
}


static void supernode2addr(p2p_sock_t * sn, const p2p_sn_name_t addr);

static void initWin32() {
	WSADATA wsaData;
	int err;

	err = WSAStartup(MAKEWORD(2,2), &wsaData);
	if(err != 0) {
		printf("FATAL ERROR: unable to initialize Winsock 2.x.");
		exit(-1);
	}
}


/* ************************************** */


/** Initialise an edge to defaults.
 *
 *  This also initialises the NULL transform operation opstruct.
 */
static int edge_init(p2p_edge_t * eee)
{
#ifdef WIN32
    initWin32();
#endif
    memset(eee, 0, sizeof(p2p_edge_t));
    eee->start_time = time(NULL);

    eee->daemon = 1;    /* By default run in daemon mode. */
    eee->udp_sock       = -1;
    eee->udp_mgmt_sock  = -1;
    eee->known_peers    = NULL;
    eee->pending_peers  = NULL;
    eee->last_register_req = 0;
    eee->register_lifetime = REGISTER_SUPER_INTERVAL_DFL;
    eee->last_p2p = 0;
    eee->last_sup = 0;
    eee->sup_attempts = P2P_EDGE_SUP_ATTEMPTS;
	eee->run_punching = 1;

    return(0);
}


/** Deinitialise the edge and deallocate any owned memory. */
static void edge_deinit(p2p_edge_t * eee)
{
    if ( eee->udp_sock >=0 )
    {
        closesocket( eee->udp_sock );
    }

    if ( eee->udp_mgmt_sock >= 0 )
    {
        closesocket(eee->udp_mgmt_sock);
    }

    clear_peer_list( &(eee->pending_peers) );
    clear_peer_list( &(eee->known_peers) );
}

static void readFromIPSocket( p2p_edge_t * eee );

static void readFromMgmtSocket( p2p_edge_t * eee, int * keep_running );

/** Send a datagram to a socket defined by a p2p_sock_t */
static ssize_t sendto_sock( int fd, const void * buf, size_t len, const p2p_sock_t * dest )
{
	p2p_sock_str_t sockbuf;
    struct sockaddr_in peer_addr;
    ssize_t sent;

    fill_sockaddr( (struct sockaddr *) &peer_addr,
                   sizeof(peer_addr),
                   dest );

    sent = sendto( fd, buf, len, 0/*flags*/,
                   (struct sockaddr *)&peer_addr, sizeof(struct sockaddr_in) );
    if ( sent < 0 )
    {
        char * c = strerror(errno);
        traceEvent( TRACE_ERROR, "sendto failed (%d) %s", errno, c );
    }
    else
    {
        traceEvent( TRACE_DEBUG, "sendto sent=%d to %s ", (signed int)sent,sock_to_cstr(sockbuf,dest));
    }

    return sent;
}


/** Send a REGISTER packet to another edge. */
static void send_register( p2p_edge_t * eee,
						  uint8_t flags,
                           const p2p_sock_t * remote_peer,
						   p2p_sock_t *dst)
{
    uint8_t pktbuf[P2P_PKT_BUF_SIZE];
    size_t idx;
    ssize_t sent;
    p2p_common_t cmn;
    p2p_REGISTER_t reg;
    p2p_sock_str_t sockbuf;

    memset(&cmn, 0, sizeof(cmn) );
    memset(&reg, 0, sizeof(reg) );
    cmn.ttl=P2P_DEFAULT_TTL;
    cmn.pc = p2p_register;
	cmn.flags = flags;
    memcpy( cmn.community, eee->community_name, P2P_COMMUNITY_SIZE );

    idx=0;
    encode_uint32( reg.cookie, &idx, 123456789 );
    idx=0;
	reg.dest_sock = *dst;
	reg.orig_sender.family = AF_INET;

    idx=0;
    encode_REGISTER( pktbuf, &idx, &cmn, &reg );

    traceEvent( TRACE_INFO, "send REGISTER %s",
                sock_to_cstr( sockbuf, remote_peer ) );


    sent = sendto_sock( eee->udp_sock, pktbuf, idx, remote_peer );

}


/** Send a REGISTER_SUPER packet to the current supernode. */
static void send_register_super( p2p_edge_t * eee,
                                 const p2p_sock_t * supernode)
{
    uint8_t pktbuf[P2P_PKT_BUF_SIZE];
    size_t idx;
    ssize_t sent;
    p2p_common_t cmn;
    p2p_REGISTER_SUPER_t reg;
    p2p_sock_str_t sockbuf;

    memset(&cmn, 0, sizeof(cmn) );
    memset(&reg, 0, sizeof(reg) );
    cmn.ttl=P2P_DEFAULT_TTL;
    cmn.pc = p2p_register_super;
    cmn.flags = 0;
    memcpy( cmn.community, eee->community_name, P2P_COMMUNITY_SIZE );

    for( idx=0; idx < P2P_COOKIE_SIZE; ++idx )
    {
        eee->last_cookie[idx] = rand() % 0xff;
    }

    memcpy( reg.cookie, eee->last_cookie, P2P_COOKIE_SIZE );
    reg.auth.scheme=0; /* No auth yet */

    idx=0;
    encode_REGISTER_SUPER( pktbuf, &idx, &cmn, &reg );

    traceEvent( TRACE_INFO, "send REGISTER_SUPER to %s",
                sock_to_cstr( sockbuf, supernode ) );


    sent = sendto_sock( eee->udp_sock, pktbuf, idx, supernode );

}


/** Send a REGISTER_ACK packet to a peer edge. */
static void send_register_ack( p2p_edge_t * eee,
                               const p2p_sock_t * remote_peer,
                               const p2p_REGISTER_t * reg )
{
    uint8_t pktbuf[P2P_PKT_BUF_SIZE];
    size_t idx;
    ssize_t sent;
    p2p_common_t cmn;
    p2p_REGISTER_ACK_t ack;
    p2p_sock_str_t sockbuf;

    memset(&cmn, 0, sizeof(cmn) );
    memset(&ack, 0, sizeof(reg) );
    cmn.ttl=P2P_DEFAULT_TTL;
    cmn.pc = p2p_register_ack;
    cmn.flags = 0;
    memcpy( cmn.community, eee->community_name, P2P_COMMUNITY_SIZE );

    memset( &ack, 0, sizeof(ack) );
    memcpy( ack.cookie, reg->cookie, P2P_COOKIE_SIZE );

    idx=0;
    encode_REGISTER_ACK( pktbuf, &idx, &cmn, &ack );

    traceEvent( TRACE_INFO, "send REGISTER_ACK %s",
                sock_to_cstr( sockbuf, remote_peer ) );


    sent = sendto_sock( eee->udp_sock, pktbuf, idx, remote_peer );
}


/** NOT IMPLEMENTED
 *
 *  This would send a DEREGISTER packet to a peer edge or supernode to indicate
 *  the edge is going away.
 */
static void send_deregister(p2p_edge_t * eee,
                            p2p_sock_t * remote_peer)
{
    /* Marshall and send message */
}


static void update_peer_address(p2p_edge_t * eee,
                                uint8_t from_supernode,
                                const p2p_sock_t *dst,
                                const p2p_sock_t * peer,
                                time_t when);
void check_peer( p2p_edge_t * eee,
                 uint8_t from_supernode,
                 const p2p_sock_t *dst,
                 const p2p_sock_t * peer );
void try_send_register( p2p_edge_t * eee,
                        uint8_t from_supernode,
                        const p2p_sock_t *dst,
                        const p2p_sock_t * peer );
void set_peer_operational( p2p_edge_t * eee,
                           const p2p_sock_t * peer );



/** Start the registration process.
 *
 *  If the peer is already in pending_peers, ignore the request.
 *  If not in pending_peers, add it and send a REGISTER.
 *
 *  If hdr is for a direct peer-to-peer packet, try to register back to sender
 *  even if the MAC is in pending_peers. This is because an incident direct
 *  packet indicates that peer-to-peer exchange should work so more aggressive
 *  registration can be permitted (once per incoming packet) as this should only
 *  last for a small number of packets..
 *
 *  Called from the main loop when Rx a packet for our device mac.
 */
void try_send_register( p2p_edge_t * eee,
                        uint8_t from_supernode,
                        const p2p_sock_t *dst,
                        const p2p_sock_t * peer )
{
    /* REVISIT: purge of pending_peers not yet done. */
    struct peer_info * scan = find_peer_by_addr( eee->pending_peers, dst );
    p2p_sock_str_t sockbuf;

    if ( NULL == scan )
    {
        scan = calloc( 1, sizeof( struct peer_info ) );

        memcpy(&scan->sock, dst, sizeof(p2p_sock_t));
        scan->sock = *dst;
        scan->last_seen = time(NULL); /* Don't change this it marks the pending peer for removal. */

        peer_list_add( &(eee->pending_peers), scan );

        traceEvent( TRACE_DEBUG, "=== new pending  %s",
                    sock_to_cstr( sockbuf, &(scan->sock) ) );

        traceEvent( TRACE_INFO, "Pending peers list size=%u",
                    (unsigned int)peer_list_size( eee->pending_peers ) );

        /* trace Sending REGISTER */

        send_register(eee, from_supernode,peer,dst);

        /* pending_peers now owns scan. */
    }
    else
    {
    }
}


/** Update the last_seen time for this peer, or get registered. */
void check_peer( p2p_edge_t * eee,
                 uint8_t from_supernode,
                 const p2p_sock_t *dst,
                 const p2p_sock_t * peer )
{
    struct peer_info * scan = find_peer_by_addr( eee->known_peers, dst );

    if ( NULL == scan )
    {
        /* Not in known_peers - start the REGISTER process. */
        try_send_register( eee, from_supernode, dst, peer );
    }
    else
    {
        /* Already in known_peers. */
        update_peer_address( eee, from_supernode, dst, peer, time(NULL) );
    }
}


/* Move the peer from the pending_peers list to the known_peers lists.
 *
 * peer must be a pointer to an element of the pending_peers list.
 *
 * Called by main loop when Rx a REGISTER_ACK.
 */
void set_peer_operational( p2p_edge_t * eee,
                        const p2p_sock_t * peer )
{
    struct peer_info * prev = NULL;
    struct peer_info * scan;
    p2p_sock_str_t sockbuf;

    traceEvent( TRACE_INFO, "set_peer_operational: %s",
                sock_to_cstr( sockbuf, peer ) );

    scan=find_peer_by_addr(eee->pending_peers,peer);

    if ( scan )
    {


        /* Remove scan from pending_peers. */
        if ( prev )
        {
            prev->next = scan->next;
        }
        else
        {
            eee->pending_peers = scan->next;
        }

        /* Add scan to known_peers. */
        scan->next = eee->known_peers;
        eee->known_peers = scan;

        scan->sock = *peer;

        traceEvent( TRACE_DEBUG, "=== new peer %s",
                    sock_to_cstr( sockbuf, &(scan->sock) ) );

        traceEvent( TRACE_INFO, "Pending peers list size=%u",
                    (unsigned int)peer_list_size( eee->pending_peers ) );

        traceEvent( TRACE_INFO, "Operational peers list size=%u",
                    (unsigned int)peer_list_size( eee->known_peers ) );


        scan->last_seen = time(NULL);
    }
    else
    {
        traceEvent( TRACE_DEBUG, "Failed to find sender in pending_peers." );
    }
}


static int is_empty_ip_address( const p2p_sock_t * sock )
{
    const uint8_t * ptr=NULL;
    size_t len=0;
    size_t i;

    if ( AF_INET6 == sock->family )
    {
        ptr = sock->addr.v6;
        len = 16;
    }
    else
    {
        ptr = sock->addr.v4;
        len = 4;
    }

    for (i=0; i<len; ++i)
    {
        if ( 0 != ptr[i] )
        {
            /* found a non-zero byte in address */
            return 0;
        }
    }

    return 1;
}


/** Keep the known_peers list straight.
 *
 *  Ignore broadcast L2 packets, and packets with invalid public_ip.
 *  If the dst_mac is in known_peers make sure the entry is correct:
 *  - if the public_ip socket has changed, erase the entry
 *  - if the same, update its last_seen = when
 */
static void update_peer_address(p2p_edge_t * eee,
                                uint8_t from_supernode,
                                const p2p_sock_t *dst,
                                const p2p_sock_t * peer,
                                time_t when)
{
    struct peer_info *scan = eee->known_peers;
    struct peer_info *prev = NULL; /* use to remove bad registrations. */
    p2p_sock_str_t sockbuf1;
    p2p_sock_str_t sockbuf2; /* don't clobber sockbuf1 if writing two addresses to trace */

    if ( is_empty_ip_address( peer ) )
    {
        /* Not to be registered. */
        return;
    }

    while(scan != NULL)
    {
        if(memcmp(dst, &scan->sock, sizeof(p2p_sock_t)) == 0)
        {
            break;
        }

        prev = scan;
        scan = scan->next;
    }

    if ( NULL == scan )
    {
        /* Not in known_peers. */
        return;
    }

    if ( 0 != sock_equal( &(scan->sock), peer))
    {
        if ( 0 == from_supernode )
        {
            traceEvent( TRACE_NORMAL, "Peer changed %s -> %s",
                        sock_to_cstr(sockbuf1, &(scan->sock)),
                        sock_to_cstr(sockbuf2, peer) );

            /* The peer has changed public socket. It can no longer be assumed to be reachable. */
            /* Remove the peer. */
            if ( NULL == prev )
            {
                /* scan was head of list */
                eee->known_peers = scan->next;
            }
            else
            {
                prev->next = scan->next;
            }
            free(scan);

            try_send_register( eee, from_supernode, dst, peer );
        }
        else
        {
            /* Don't worry about what the supernode reports, it could be seeing a different socket. */
        }
    }
    else
    {
        /* Found and unchanged. */
        scan->last_seen = when;
    }
}


/** @brief Check to see if we should re-register with the supernode.
 *
 *  This is frequently called by the main loop.
 */
static void update_supernode_reg( p2p_edge_t * eee, time_t nowTime )
{
    if ( eee->sn_wait && ( nowTime > (eee->last_register_req + (eee->register_lifetime/10) ) ) )
    {
        /* fall through */
        traceEvent( TRACE_DEBUG, "update_supernode_reg: doing fast retry." );
    }
    else if ( nowTime < (eee->last_register_req + eee->register_lifetime))
    {
        return; /* Too early */
    }

    if ( 0 == eee->sup_attempts )
    {
        /* Give up on that supernode and try the next one. */
        ++(eee->sn_idx);

        if (eee->sn_idx >= eee->sn_num)
        {
            /* Got to end of list, go back to the start. Also works for list of one entry. */
            eee->sn_idx=0;
        }

        traceEvent(TRACE_WARNING, "Supernode not responding - moving to %u of %u", 
                   (unsigned int)eee->sn_idx, (unsigned int)eee->sn_num );

        eee->sup_attempts = P2P_EDGE_SUP_ATTEMPTS;
    }
    else
    {
        --(eee->sup_attempts);
    }


    traceEvent(TRACE_DEBUG, "Registering with supernode (%s) (attempts left %u)",
               supernode_ip(eee), (unsigned int)eee->sup_attempts);

    send_register_super( eee, &(eee->supernode) );

    eee->sn_wait=1;

    /* REVISIT: turn-on gratuitous ARP with config option. */
    /* send_grat_arps(sock_fd, is_udp_sock); */

    eee->last_register_req = nowTime;
}



/** Read a datagram from the management UDP socket and take appropriate
 *  action. */
static void readFromMgmtSocket( p2p_edge_t * eee, int * keep_running )
{
    uint8_t             udp_buf[P2P_PKT_BUF_SIZE];      /* Compete UDP packet */
    ssize_t             recvlen;
    ssize_t             sendlen;
    struct sockaddr_in  sender_sock;
    socklen_t           i;
    size_t              msg_len;
    time_t              now;

    now = time(NULL);
    i = sizeof(sender_sock);
    recvlen=recvfrom(eee->udp_mgmt_sock, udp_buf, P2P_PKT_BUF_SIZE, 0/*flags*/,
		     (struct sockaddr *)&sender_sock, (socklen_t*)&i);

    if ( recvlen < 0 )
    {
        traceEvent(TRACE_ERROR, "mgmt recvfrom failed with %s", strerror(errno) );

        return; /* failed to receive data from UDP */
    }

    if ( recvlen >= 4 )
    {
        if ( 0 == memcmp( udp_buf, "stop", 4 ) )
        {
            traceEvent( TRACE_ERROR, "stop command received." );
            *keep_running = 0;
            return;
        }

        if ( 0 == memcmp( udp_buf, "help", 4 ) )
        {
            msg_len=0;
            ++traceLevel;

            msg_len += snprintf( (char *)(udp_buf+msg_len), (P2P_PKT_BUF_SIZE-msg_len),
                                 "Help for edge management console:\n"
                                 "  stop    Gracefully exit edge\n"
                                 "  help    This help message\n"
                                 "  +verb   Increase verbosity of logging\n"
                                 "  -verb   Decrease verbosity of logging\n"
                                 "  <enter> Display statistics\n\n");

            sendto( eee->udp_mgmt_sock, udp_buf, msg_len, 0/*flags*/,
                    (struct sockaddr *)&sender_sock, sizeof(struct sockaddr_in) );

            return;
        }

    }

    if ( recvlen >= 5 )
    {
        if ( 0 == memcmp( udp_buf, "+verb", 5 ) )
        {
            msg_len=0;
            ++traceLevel;

            traceEvent( TRACE_ERROR, "+verb traceLevel=%u", (unsigned int)traceLevel );
            msg_len += snprintf( (char *)(udp_buf+msg_len), (P2P_PKT_BUF_SIZE-msg_len),
                                     "> +OK traceLevel=%u\n", (unsigned int)traceLevel );

            sendto( eee->udp_mgmt_sock, udp_buf, msg_len, 0/*flags*/,
                    (struct sockaddr *)&sender_sock, sizeof(struct sockaddr_in) );

            return;
        }

        if ( 0 == memcmp( udp_buf, "-verb", 5 ) )
        {
            msg_len=0;

            if ( traceLevel > 0 )
            {
                --traceLevel;
                msg_len += snprintf( (char *)(udp_buf+msg_len), (P2P_PKT_BUF_SIZE-msg_len),
                                     "> -OK traceLevel=%u\n", traceLevel );
            }
            else
            {
                msg_len += snprintf( (char *)(udp_buf+msg_len), (P2P_PKT_BUF_SIZE-msg_len),
                                     "> -NOK traceLevel=%u\n", traceLevel );
            }

            traceEvent( TRACE_ERROR, "-verb traceLevel=%u", (unsigned int)traceLevel );

            sendto( eee->udp_mgmt_sock, udp_buf, msg_len, 0/*flags*/,
                    (struct sockaddr *)&sender_sock, sizeof(struct sockaddr_in) );
            return;
        }
    }


    traceEvent(TRACE_DEBUG, "mgmt status rq" );

    msg_len=0;
    msg_len += snprintf( (char *)(udp_buf+msg_len), (P2P_PKT_BUF_SIZE-msg_len),
                         "Statistics for edge\n" );

    msg_len += snprintf( (char *)(udp_buf+msg_len), (P2P_PKT_BUF_SIZE-msg_len),
                         "uptime %lu\n",
                         time(NULL) - eee->start_time );

    msg_len += snprintf( (char *)(udp_buf+msg_len), (P2P_PKT_BUF_SIZE-msg_len),
                         "paths  super:%u,%u p2p:%u,%u\n",
                         (unsigned int)eee->tx_sup,
			 (unsigned int)eee->rx_sup,
			 (unsigned int)eee->tx_p2p,
			 (unsigned int)eee->rx_p2p );


    msg_len += snprintf( (char *)(udp_buf+msg_len), (P2P_PKT_BUF_SIZE-msg_len),
                         "peers  pend:%u full:%u\n",
                         (unsigned int)peer_list_size( eee->pending_peers ), 
			 (unsigned int)peer_list_size( eee->known_peers ) );

    msg_len += snprintf( (char *)(udp_buf+msg_len), (P2P_PKT_BUF_SIZE-msg_len),
                         "last   super:%lu(%ld sec ago) p2p:%lu(%ld sec ago)\n",
                         eee->last_sup, (now-eee->last_sup), eee->last_p2p, (now-eee->last_p2p) );

    traceEvent(TRACE_DEBUG, "mgmt status sending: %s", udp_buf );


    sendlen = sendto( eee->udp_mgmt_sock, udp_buf, msg_len, 0/*flags*/,
                      (struct sockaddr *)&sender_sock, sizeof(struct sockaddr_in) );

}


/** Read a datagram from the main UDP socket to the internet. */
static void readFromIPSocket( p2p_edge_t * eee )
{
    p2p_common_t        cmn; /* common fields in the packet header */

    p2p_sock_str_t      sockbuf1;
    p2p_sock_str_t      sockbuf2; /* don't clobber sockbuf1 if writing two addresses to trace */

    uint8_t             udp_buf[P2P_PKT_BUF_SIZE];      /* Compete UDP packet */
    ssize_t             recvlen;
    size_t              rem;
    size_t              idx;
    size_t              msg_type;
    uint8_t             from_supernode;
    struct sockaddr_in  sender_sock;
    p2p_sock_t          sender;
    p2p_sock_t *        orig_sender=NULL;
    time_t              now=0;

    size_t              i;

    i = sizeof(sender_sock);
    recvlen=recvfrom(eee->udp_sock, udp_buf, P2P_PKT_BUF_SIZE, 0/*flags*/,
                     (struct sockaddr *)&sender_sock, (socklen_t*)&i);

    if ( recvlen < 0 )
    {
        traceEvent(TRACE_ERROR, "recvfrom failed with %s", strerror(errno) );

        return; /* failed to receive data from UDP */
    }

    /* REVISIT: when UDP/IPv6 is supported we will need a flag to indicate which
     * IP transport version the packet arrived on. May need to UDP sockets. */
    sender.family = AF_INET; /* udp_sock was opened PF_INET v4 */
    sender.port = ntohs(sender_sock.sin_port);
    memcpy( &(sender.addr.v4), &(sender_sock.sin_addr.s_addr), IPV4_SIZE );

    /* The packet may not have an orig_sender socket spec. So default to last
     * hop as sender. */
    orig_sender=&sender;

    traceEvent(TRACE_INFO, "### Rx P2P UDP (%d) from %s", 
               (signed int)recvlen, sock_to_cstr(sockbuf1, &sender) );

    /* hexdump( udp_buf, recvlen ); */

    rem = recvlen; /* Counts down bytes of packet to protect against buffer overruns. */
    idx = 0; /* marches through packet header as parts are decoded. */
    if ( decode_common(&cmn, udp_buf, &rem, &idx) < 0 )
    {
        traceEvent( TRACE_ERROR, "Failed to decode common section in P2P_UDP" );
        return; /* failed to decode packet */
    }

    now = time(NULL);

    msg_type = cmn.pc; /* packet code */
    from_supernode= cmn.flags & P2P_FLAGS_FROM_SUPERNODE;

    if( 0 == memcmp(cmn.community, eee->community_name, P2P_COMMUNITY_SIZE) )
    {
        if( msg_type == MSG_TYPE_PACKET)
        {
            traceEvent(TRACE_DEBUG, "Rx REGISTER_ACK (NOT IMPLEMENTED)");
        }
        else if(msg_type == MSG_TYPE_REGISTER)
        {
            /* Another edge is registering with us */
            p2p_REGISTER_t reg;

            decode_REGISTER( &reg, &cmn, udp_buf, &rem, &idx );

            if ( cmn.flags == P2P_FLAGS_FROM_SUPERNODE)
            {
                orig_sender = &(reg.orig_sender);
				cmn.flags = 0;
				from_supernode = 0;
            }

            traceEvent(TRACE_INFO, "Rx REGISTER from peer %s (%s)",
                       sock_to_cstr(sockbuf1, &sender),
                       sock_to_cstr(sockbuf2, orig_sender) );

			check_peer( eee, from_supernode, orig_sender, orig_sender );

            send_register_ack(eee, orig_sender, &reg);
        }
        else if(msg_type == MSG_TYPE_REGISTER_ACK)
        {
            /* Peer edge is acknowledging our register request */
            p2p_REGISTER_ACK_t ra;

            decode_REGISTER_ACK( &ra, &cmn, udp_buf, &rem, &idx );

            if ( ra.sock.family )
            {
                orig_sender = &(ra.sock);
            }

            traceEvent(TRACE_INFO, "Rx REGISTER_ACK from peer %s (%s)",
                       sock_to_cstr(sockbuf1, &sender),
                       sock_to_cstr(sockbuf2, orig_sender) );

            /* Move from pending_peers to known_peers; ignore if not in pending. */
            set_peer_operational( eee, &sender );
        }
        else if(msg_type == MSG_TYPE_REGISTER_SUPER_ACK)
        {
            p2p_REGISTER_SUPER_ACK_t ra;

            if ( eee->sn_wait )
            {
                decode_REGISTER_SUPER_ACK( &ra, &cmn, udp_buf, &rem, &idx );

                if ( ra.sock.family )
                {
                    orig_sender = &(ra.sock);
                }

                traceEvent(TRACE_NORMAL, "Rx REGISTER_SUPER_ACK %s (external %s). Attempts %u",
                           sock_to_cstr(sockbuf1, &sender),
                           sock_to_cstr(sockbuf2, orig_sender), 
                           (unsigned int)eee->sup_attempts );

                if ( 0 == memcmp( ra.cookie, eee->last_cookie, P2P_COOKIE_SIZE ) )
                {
                    if ( ra.num_sn > 0 )
                    {
                        traceEvent(TRACE_NORMAL, "Rx REGISTER_SUPER_ACK backup supernode at %s",
                                   sock_to_cstr(sockbuf1, &(ra.sn_bak) ) );
                    }

                    eee->last_sup = now;
                    eee->sn_wait=0;
                    eee->sup_attempts = P2P_EDGE_SUP_ATTEMPTS; /* refresh because we got a response */

                    /* REVISIT: store sn_back */
                    eee->register_lifetime = ra.lifetime;
                    eee->register_lifetime = MAX( eee->register_lifetime, REGISTER_SUPER_INTERVAL_MIN );
                    eee->register_lifetime = MIN( eee->register_lifetime, REGISTER_SUPER_INTERVAL_MAX );
                }
                else
                {
                    traceEvent( TRACE_WARNING, "Rx REGISTER_SUPER_ACK with wrong or old cookie." );
                }
            }
            else
            {
                traceEvent( TRACE_WARNING, "Rx REGISTER_SUPER_ACK with no outstanding REGISTER_SUPER." );
            }
        }
        else
        {
            /* Not a known message type */
            traceEvent(TRACE_WARNING, "Unable to handle packet type %d: ignored", (signed int)msg_type);
            return;
        }
    } /* if (community match) */
    else
    {
        traceEvent(TRACE_WARNING, "Received packet with invalid community");
    }

}

/* ***************************************************** */

/** Resolve the supernode IP address.
 *
 *  REVISIT: This is a really bad idea. The edge will block completely while the
 *           hostname resolution is performed. This could take 15 seconds.
 */
static void supernode2addr(p2p_sock_t * sn, const p2p_sn_name_t addrIn)
{
    p2p_sn_name_t addr;
	const char *supernode_host;

    memcpy( addr, addrIn, P2P_EDGE_SN_HOST_SIZE );

    supernode_host = strtok(addr, ":");

    if(supernode_host)
    {
        in_addr_t sn_addr;
        char *supernode_port = strtok(NULL, ":");
        const struct addrinfo aihints = {0, PF_INET, 0, 0, 0, NULL, NULL, NULL};
        struct addrinfo * ainfo = NULL;
        int nameerr;

        if ( supernode_port )
            sn->port = atoi(supernode_port);
        else
            traceEvent(TRACE_WARNING, "Bad supernode parameter (-l <host:port>) %s %s:%s",
                       addr, supernode_host, supernode_port);

        nameerr = getaddrinfo( supernode_host, NULL, &aihints, &ainfo );

        if( 0 == nameerr )
        {
            struct sockaddr_in * saddr;

            /* ainfo s the head of a linked list if non-NULL. */
            if ( ainfo && (PF_INET == ainfo->ai_family) )
            {
                /* It is definitely and IPv4 address -> sockaddr_in */
                saddr = (struct sockaddr_in *)ainfo->ai_addr;

                memcpy( sn->addr.v4, &(saddr->sin_addr.s_addr), IPV4_SIZE );
                sn->family=AF_INET;
            }
            else
            {
                /* Should only return IPv4 addresses due to aihints. */
                traceEvent(TRACE_WARNING, "Failed to resolve supernode IPv4 address for %s", supernode_host);
            }

            freeaddrinfo(ainfo); /* free everything allocated by getaddrinfo(). */
            ainfo = NULL;
        } else {
            traceEvent(TRACE_WARNING, "Failed to resolve supernode host %s, assuming numeric", supernode_host);
            sn_addr = inet_addr(supernode_host); /* uint32_t */
            memcpy( sn->addr.v4, &(sn_addr), IPV4_SIZE );
            sn->family=AF_INET;
        }

    } else
        traceEvent(TRACE_WARNING, "Wrong supernode parameter (-l <host:port>)");
}

/* ***************************************************** */
P2P_DECLARE(int) punching_hole(p2p_edge_t *node, const char *peer_ip, int port)
{
	p2p_sock_t dest,snode;
	unsigned long naddr;

	dest.family = AF_INET;
	dest.port = port;
	naddr = inet_addr(peer_ip);
	memcpy(&(dest.addr.v4), &(naddr), IPV4_SIZE);

	supernode2addr( &snode, node->sn_ip_array[node->sn_idx]);

	try_send_register(node, 0, &dest,&snode);
	return 0;
}

/* ***************************************************** */
static void *punching_worker(void *opaque);
static int run_loop(p2p_edge_t *eee);

P2P_DECLARE(int) stop_punching_daemon(p2p_edge_t *node)
{
	node->run_punching = 0;
	WaitForSingleObject(node->hPhunching,10000);
	return 0;
}

/* ***************************************************** */
P2P_DECLARE(int) start_punching_daemon(punch_arg_t *args, p2p_edge_t *node)
{
	int i;

	traceLevel = args->traceLevel;
	if( -1 == edge_init(node))
	{
		traceEvent(TRACE_ERROR, "Failed in edge_init");
		exit(1);
	}

	memset(node->community_name, 0, P2P_COMMUNITY_SIZE);
	strncpy((char *)node->community_name, args->community_name, P2P_COMMUNITY_SIZE);

	memset(&(node->supernode), 0, sizeof(node->supernode));
	node->supernode.family = AF_INET;

	strncpy((node->sn_ip_array[node->sn_num]), args->sn, P2P_EDGE_SN_HOST_SIZE);
	traceEvent(TRACE_DEBUG, "Adding supernode[%u] = %s\n",(unsigned int)node->sn_num, (node->sn_ip_array[node->sn_num]));
	++node->sn_num;

	traceEvent(TRACE_NORMAL, "Starting p2p edge %s %s",p2p_sw_version,p2p_sw_buildDate);

	for(i=0; i< P2P_EDGE_NUM_SUPERNODES; ++i)
	{
		traceEvent(TRACE_NORMAL, "supernode %u => %s\n",i,(node->sn_ip_array[i]));
	}

	supernode2addr(&(node->supernode), node->sn_ip_array[node->sn_idx]);

	if(!(
		(node->community_name[0] != 0)
		))
	{

	}

	if(args->local_port > 0)
		traceEvent(TRACE_NORMAL, "Binding to local port %d", (signed int)args->local_port);

	node->udp_sock = open_socket(args->local_port,1/*bind ANY*/);
	if(node->udp_sock < 0)
	{
		traceEvent(TRACE_ERROR, "Failed to bind main UDP port %u", (signed int)args->local_port);
		return (-1);
	}

	node->udp_mgmt_sock = open_socket(P2P_EDGE_MGMT_PORT,1/*bind ANY*/);
	if(node->udp_mgmt_sock < 0)
	{
		traceEvent(TRACE_ERROR, "Failed to bind management UDP port %u", (unsigned int)P2P_EDGE_MGMT_PORT);
		return (-1);
	}

	traceEvent(TRACE_NORMAL,"edge started");
	update_supernode_reg(node,time(NULL));

	if((node->hPhunching = (HANDLE)_beginthreadex(NULL,
		(DWORD)0,
		(unsigned int (__stdcall *)(void *))punching_worker,
		(void *)node,0,NULL)) == 0) {
			return -1;
	}
	Sleep(100);
	return 0;
}

static void *punching_worker(void *opaque)
{
	p2p_edge_t *eee = (p2p_edge_t *)opaque;
	run_loop(eee);
	return 0;
}

static int run_loop(p2p_edge_t * eee )
{
    size_t numPurged;
    time_t lastIfaceCheck=0;

    /* Main loop
     *
     * select() is used to wait for input on either the TAP fd or the UDP/TCP
     * socket. When input is present the data is read and processed by either
     * readFromIPSocket() or readFromTAPSocket()
     */

    while(eee->run_punching)
    {
        int rc, max_sock = 0;
        fd_set socket_mask;
        struct timeval wait_time;
        time_t nowTime;

        FD_ZERO(&socket_mask);
        FD_SET(eee->udp_sock, &socket_mask);
        FD_SET(eee->udp_mgmt_sock, &socket_mask);
        max_sock = max( eee->udp_sock, eee->udp_mgmt_sock );

        wait_time.tv_sec = SOCKET_TIMEOUT_INTERVAL_SECS; wait_time.tv_usec = 0;

        rc = select(max_sock+1, &socket_mask, NULL, NULL, &wait_time);
        nowTime=time(NULL);

        /* Make sure ciphers are updated before the packet is treated. */

        if(rc > 0)
        {
            /* Any or all of the FDs could have input; check them all. */

            if(FD_ISSET(eee->udp_sock, &socket_mask))
            {
                /* Read a cooked socket from the internet socket. Writes on the TAP
                 * socket. */
                readFromIPSocket(eee);
            }

            if(FD_ISSET(eee->udp_mgmt_sock, &socket_mask))
            {
                /* Read a cooked socket from the internet socket. Writes on the TAP
                 * socket. */
                readFromMgmtSocket(eee, &eee->run_punching);
            }

        }

        /* Finished processing select data. */


        update_supernode_reg(eee, nowTime);

        numPurged =  purge_expired_registrations( &(eee->known_peers) );
        numPurged += purge_expired_registrations( &(eee->pending_peers) );
        if ( numPurged > 0 )
        {
            traceEvent( TRACE_NORMAL, "Peer removed: pending=%u, operational=%u",
                        (unsigned int)peer_list_size( eee->pending_peers ), 
                        (unsigned int)peer_list_size( eee->known_peers ) );
        }

    } /* while */

    send_deregister( eee, &(eee->supernode));

    closesocket(eee->udp_sock);

    edge_deinit( eee );

    return(0);
}


