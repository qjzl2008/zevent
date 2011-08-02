#include "punch_common.h"


#define P2P_SN_LPORT_DEFAULT 7654
#define P2P_SN_PKTBUF_SIZE   2048

#define P2P_SN_MGMT_PORT                5645


struct sn_stats
{
    size_t errors;              /* Number of errors encountered. */
    size_t reg_super;           /* Number of REGISTER_SUPER requests received. */
    size_t reg_super_nak;       /* Number of REGISTER_SUPER requests declined. */
    size_t fwd;                 /* Number of messages forwarded. */
    size_t broadcast;           /* Number of messages broadcast to a community. */
    time_t last_fwd;            /* Time when last message was forwarded. */
    time_t last_reg_super;      /* Time when last REGISTER_SUPER was received. */
};

typedef struct sn_stats sn_stats_t;

struct p2p_sn
{
    time_t              start_time;     /* Used to measure uptime. */
    sn_stats_t          stats;
    int                 daemon;         /* If non-zero then daemonise. */
    uint16_t            lport;          /* Local UDP port to bind to. */
    int                 sock;           /* Main socket for UDP traffic with edges. */
    int                 mgmt_sock;      /* management socket. */
    struct peer_info *  edges;          /* Link list of registered edges. */
};

typedef struct p2p_sn p2p_sn_t;


static int try_forward( p2p_sn_t * sss, 
                        const p2p_common_t * cmn,
                        const p2p_sock_t *dst,
                        const uint8_t * pktbuf,
                        size_t pktsize );
static void initWin32() {
	WSADATA wsaData;
	int err;

	err = WSAStartup(MAKEWORD(2,2),&wsaData);
	if(err!=0)
	{
		printf("FATAL ERROR:unable to initialize Winsock 2.x.");
		exit(-1);
	}
}


/** Initialise the supernode structure */
static int init_sn( p2p_sn_t * sss )
{
#ifdef WIN32
    initWin32();
#endif
    memset( sss, 0, sizeof(p2p_sn_t) );

    sss->daemon = 1; /* By defult run as a daemon. */
    sss->lport = P2P_SN_LPORT_DEFAULT;
    sss->sock = -1;
    sss->mgmt_sock = -1;
    sss->edges = NULL;

    return 0; /* OK */
}

/** Deinitialise the supernode structure and deallocate any memory owned by
 *  it. */
static void deinit_sn( p2p_sn_t * sss )
{
    if (sss->sock >= 0)
    {
        closesocket(sss->sock);
    }
    sss->sock=-1;

    if ( sss->mgmt_sock >= 0 )
    {
        closesocket(sss->mgmt_sock);
    }
    sss->mgmt_sock=-1;

    purge_peer_list( &(sss->edges), 0xffffffff );
}


/** Determine the appropriate lifetime for new registrations.
 *
 *  If the supernode has been put into a pre-shutdown phase then this lifetime
 *  should not allow registrations to continue beyond the shutdown point.
 */
static uint16_t reg_lifetime( p2p_sn_t * sss )
{
    return 120;
}


/** Update the edge table with the details of the edge which contacted the
 *  supernode. */
static int update_edge( p2p_sn_t * sss, 
                        const p2p_community_t community,
                        const p2p_sock_t * sender_sock,
                        time_t now)
{
    p2p_sock_str_t      sockbuf;
    struct peer_info *  scan;

    traceEvent( TRACE_DEBUG, "update_edge for [%s]",
                sock_to_cstr( sockbuf, sender_sock ) );

    scan = find_peer_by_addr( sss->edges, sender_sock );

    if ( NULL == scan )
    {
        /* Not known */

        scan = (struct peer_info*)calloc(1, sizeof(struct peer_info)); /* deallocated in purge_expired_registrations */

        memcpy(scan->community_name, community, sizeof(p2p_community_t) );
        memcpy(&(scan->sock), sender_sock, sizeof(p2p_sock_t));

        /* insert this guy at the head of the edges list */
        scan->next = sss->edges;     /* first in list */
        sss->edges = scan;           /* head of list points to new scan */

        traceEvent( TRACE_INFO, "update_edge created   %s",
                    sock_to_cstr( sockbuf, sender_sock ) );
    }
    else
    {
        /* Known */
        if ( (0 != memcmp(community, scan->community_name, sizeof(p2p_community_t))) ||
             (0 != sock_equal(sender_sock, &(scan->sock) )) )
        {
            memcpy(scan->community_name, community, sizeof(p2p_community_t) );
            memcpy(&(scan->sock), sender_sock, sizeof(p2p_sock_t));

            traceEvent( TRACE_INFO, "update_edge updated   %s",
                        sock_to_cstr( sockbuf, sender_sock ) );
        }
        else
        {
            traceEvent( TRACE_DEBUG, "update_edge unchanged %s",
                        sock_to_cstr( sockbuf, sender_sock ) );
        }

    }

    scan->last_seen = now;
    return 0;
}


/** Send a datagram to the destination embodied in a p2p_sock_t.
 *
 *  @return -1 on error otherwise number of bytes sent
 */
static ssize_t sendto_sock(p2p_sn_t * sss, 
                           const p2p_sock_t * sock, 
                           const uint8_t * pktbuf, 
                           size_t pktsize)
{
    p2p_sock_str_t      sockbuf;

    if ( AF_INET == sock->family )
    {
        struct sockaddr_in udpsock;

        udpsock.sin_family = AF_INET;
        udpsock.sin_port = htons( sock->port );
        memcpy( &(udpsock.sin_addr.s_addr), &(sock->addr.v4), IPV4_SIZE );

        traceEvent( TRACE_DEBUG, "sendto_sock %lu to [%s]",
                    pktsize,
                    sock_to_cstr( sockbuf, sock ) );

        return sendto( sss->sock, pktbuf, pktsize, 0, 
                       (const struct sockaddr *)&udpsock, sizeof(struct sockaddr_in) );
    }
    else
    {
        /* AF_INET6 not implemented */
        return -1;
    }
}



/** Try to forward a message to a unicast MAC. If the MAC is unknown then
 *  broadcast to all edges in the destination community.
 */
static int try_forward( p2p_sn_t * sss, 
                        const p2p_common_t * cmn,
                        const p2p_sock_t *dst,
                        const uint8_t * pktbuf,
                        size_t pktsize )
{
    struct peer_info *  scan;
    p2p_sock_str_t      sockbuf;

    scan = find_peer_by_addr( sss->edges, dst );

    if ( NULL != scan )
    {
        int data_sent_len;
        data_sent_len = sendto_sock( sss, &(scan->sock), pktbuf, pktsize );

        if ( data_sent_len == pktsize )
        {
            ++(sss->stats.fwd);
            traceEvent(TRACE_DEBUG, "unicast %lu to [%s]",
                       pktsize,
                       sock_to_cstr( sockbuf, &(scan->sock) ));
        }
        else
        {
            ++(sss->stats.errors);
            traceEvent(TRACE_ERROR, "unicast %lu to [%s] FAILED (%d: %s)",
                       pktsize,
                       sock_to_cstr( sockbuf, &(scan->sock) ),
                       errno, strerror(errno) );
        }
    }
    else
    {
        traceEvent( TRACE_DEBUG, "try_forward unknown peer" );
    }
    
    return 0;
}


static int process_mgmt( p2p_sn_t * sss, 
                         const struct sockaddr_in * sender_sock,
                         const uint8_t * mgmt_buf, 
                         size_t mgmt_size,
                         time_t now)
{
    char resbuf[P2P_SN_PKTBUF_SIZE];
    size_t ressize=0;
    ssize_t r;

    traceEvent( TRACE_DEBUG, "process_mgmt" );

    ressize += snprintf( resbuf+ressize, P2P_SN_PKTBUF_SIZE-ressize, 
                         "----------------\n" );

    ressize += snprintf( resbuf+ressize, P2P_SN_PKTBUF_SIZE-ressize, 
                         "uptime    %lu\n", (now - sss->start_time) );

    ressize += snprintf( resbuf+ressize, P2P_SN_PKTBUF_SIZE-ressize, 
                         "edges     %u\n", 
			 (unsigned int)peer_list_size( sss->edges ) );

    ressize += snprintf( resbuf+ressize, P2P_SN_PKTBUF_SIZE-ressize, 
                         "errors    %u\n", 
			 (unsigned int)sss->stats.errors );

    ressize += snprintf( resbuf+ressize, P2P_SN_PKTBUF_SIZE-ressize, 
                         "reg_sup   %u\n", 
			 (unsigned int)sss->stats.reg_super );

    ressize += snprintf( resbuf+ressize, P2P_SN_PKTBUF_SIZE-ressize, 
                         "reg_nak   %u\n", 
			 (unsigned int)sss->stats.reg_super_nak );

    ressize += snprintf( resbuf+ressize, P2P_SN_PKTBUF_SIZE-ressize, 
                         "fwd       %u\n",
			 (unsigned int) sss->stats.fwd );

    ressize += snprintf( resbuf+ressize, P2P_SN_PKTBUF_SIZE-ressize, 
                         "broadcast %u\n",
			 (unsigned int) sss->stats.broadcast );

    ressize += snprintf( resbuf+ressize, P2P_SN_PKTBUF_SIZE-ressize, 
                         "last fwd  %lu sec ago\n", 
			 (long unsigned int)(now - sss->stats.last_fwd) );

    ressize += snprintf( resbuf+ressize, P2P_SN_PKTBUF_SIZE-ressize, 
                         "last reg  %lu sec ago\n",
			 (long unsigned int) (now - sss->stats.last_reg_super) );


    r = sendto( sss->mgmt_sock, resbuf, ressize, 0/*flags*/, 
                (struct sockaddr *)sender_sock, sizeof(struct sockaddr_in) );

    if ( r <= 0 )
    {
        ++(sss->stats.errors);
        traceEvent( TRACE_ERROR, "process_mgmt : sendto failed. %s", strerror(errno) );
    }

    return 0;
}


/** Examine a datagram and determine what to do with it.
 *
 */
static int process_udp( p2p_sn_t * sss, 
                        const struct sockaddr_in * sender_sock,
                        const uint8_t * udp_buf, 
                        size_t udp_size,
                        time_t now)
{
    p2p_common_t        cmn; /* common fields in the packet header */
    size_t              rem;
    size_t              idx;
    size_t              msg_type;
    uint8_t             from_supernode;
    p2p_sock_str_t      sockbuf;


    traceEvent( TRACE_DEBUG, "process_udp(%lu)", udp_size );

    /* Use decode_common() to determine the kind of packet then process it:
     *
     * REGISTER_SUPER adds an edge and generate a return REGISTER_SUPER_ACK
     *
     * REGISTER, REGISTER_ACK and PACKET messages are forwarded to their
     * destination edge. If the destination is not known then PACKETs are
     * broadcast.
     */

    rem = udp_size; /* Counts down bytes of packet to protect against buffer overruns. */
    idx = 0; /* marches through packet header as parts are decoded. */
    if ( decode_common(&cmn, udp_buf, &rem, &idx) < 0 )
    {
        traceEvent( TRACE_ERROR, "Failed to decode common section" );
        return -1; /* failed to decode packet */
    }

    msg_type = cmn.pc; /* packet code */
    from_supernode= cmn.flags & P2P_FLAGS_FROM_SUPERNODE;

    if ( cmn.ttl < 1 )
    {
        traceEvent( TRACE_WARNING, "Expired TTL" );
        return 0; /* Don't process further */
    }

    --(cmn.ttl); /* The value copied into all forwarded packets. */

    if ( msg_type == MSG_TYPE_PACKET )
    {
        /* PACKET from one edge to another edge via supernode. */
		traceEvent(TRACE_DEBUG, "Rx REGISTER_ACK (NOT IMPLEMENTED) Should not be via supernode");

    }/* MSG_TYPE_PACKET */
    else if ( msg_type == MSG_TYPE_REGISTER )
	{
		/* Forwarding a REGISTER from one edge to the next */

		p2p_REGISTER_t                  reg;
		p2p_common_t                    cmn2;
		uint8_t                         encbuf[P2P_SN_PKTBUF_SIZE];
		size_t                          encx=0;
		const uint8_t *                 rec_buf; /* either udp_buf or encbuf */

		sss->stats.last_fwd=now;
		decode_REGISTER( &reg, &cmn, udp_buf, &rem, &idx );

		memcpy( &cmn2, &cmn, sizeof( p2p_common_t ) );

		/* We are going to add socket even if it was not there before */
		cmn2.flags = P2P_FLAGS_FROM_SUPERNODE;

		reg.orig_sender.family = AF_INET;
		reg.orig_sender.port = ntohs(sender_sock->sin_port);
		memcpy( reg.orig_sender.addr.v4, &(sender_sock->sin_addr.s_addr), IPV4_SIZE );

		rec_buf = encbuf;

		/* Re-encode the header. */
		encode_REGISTER( encbuf, &encx, &cmn2, &reg );

		/* Copy the original payload unchanged */
		encode_buf( encbuf, &encx, (udp_buf + idx), (udp_size - idx ) );

		try_forward( sss, &cmn, &reg.dest_sock, rec_buf, encx ); /* unicast only */
	}
    else if ( msg_type == MSG_TYPE_REGISTER_ACK )
    {
        traceEvent( TRACE_DEBUG, "Rx REGISTER_ACK (NOT IMPLEMENTED) SHould not be via supernode" );
    }
    else if ( msg_type == MSG_TYPE_REGISTER_SUPER )
    {
        p2p_REGISTER_SUPER_t            reg;
        p2p_REGISTER_SUPER_ACK_t        ack;
        p2p_common_t                    cmn2;
        uint8_t                         ackbuf[P2P_SN_PKTBUF_SIZE];
        size_t                          encx=0;

        /* Edge requesting registration with us.  */
        
        sss->stats.last_reg_super=now;
        ++(sss->stats.reg_super);
        decode_REGISTER_SUPER( &reg, &cmn, udp_buf, &rem, &idx );

        cmn2.ttl = P2P_DEFAULT_TTL;
        cmn2.pc = p2p_register_super_ack;
        cmn2.flags = P2P_FLAGS_SOCKET | P2P_FLAGS_FROM_SUPERNODE;
        memcpy( cmn2.community, cmn.community, sizeof(p2p_community_t) );

        memcpy( &(ack.cookie), &(reg.cookie), sizeof(p2p_cookie_t) );
        ack.lifetime = reg_lifetime( sss );

        ack.sock.family = AF_INET;
        ack.sock.port = ntohs(sender_sock->sin_port);
        memcpy( ack.sock.addr.v4, &(sender_sock->sin_addr.s_addr), IPV4_SIZE );

        ack.num_sn=0; /* No backup */
        memset( &(ack.sn_bak), 0, sizeof(p2p_sock_t) );

        traceEvent( TRACE_DEBUG, "Rx REGISTER_SUPER for [%s]",
                    sock_to_cstr( sockbuf, &(ack.sock) ) );

        update_edge( sss, cmn.community, &(ack.sock), now );

        encode_REGISTER_SUPER_ACK( ackbuf, &encx, &cmn2, &ack );

        sendto( sss->sock, ackbuf, encx, 0, 
                (struct sockaddr *)sender_sock, sizeof(struct sockaddr_in) );

        traceEvent( TRACE_DEBUG, "Tx REGISTER_SUPER_ACK for [%s]",
                    sock_to_cstr( sockbuf, &(ack.sock) ) );

    }


    return 0;
}


/** Help message to print if the command line arguments are not valid. */
static void exit_help(int argc, char * const argv[])
{
    fprintf( stderr, "%s usage\n", argv[0] );
    fprintf( stderr, "-l <lport>\tSet UDP main listen port to <lport>\n" );

#if defined(P2P_HAVE_DAEMON)
    fprintf( stderr, "-f        \tRun in foreground.\n" );
#endif /* #if defined(P2P_HAVE_DAEMON) */
    fprintf( stderr, "-v        \tIncrease verbosity. Can be used multiple times.\n" );
    fprintf( stderr, "-h        \tThis help message.\n" );
    fprintf( stderr, "\n" );
    exit(1);
}

static int run_loop( p2p_sn_t * sss );

/* *********************************************** */

static const struct option long_options[] = {
  { "foreground",      no_argument,       NULL, 'f' },
  { "local-port",      required_argument, NULL, 'l' },
  { "help"   ,         no_argument,       NULL, 'h' },
  { "verbose",         no_argument,       NULL, 'v' },
  { NULL,              0,                 NULL,  0  }
};

/** Main program entry point from kernel. */
int main( int argc, char * const argv[] )
{
    p2p_sn_t sss;

    init_sn( &sss );

    {
        int opt;

        while((opt = getopt_long(argc, argv, "fl:vh", long_options, NULL)) != -1) 
        {
            switch (opt) 
            {
            case 'l': /* local-port */
                sss.lport = atoi(optarg);
                break;
            case 'f': /* foreground */
                sss.daemon = 0;
                break;
            case 'h': /* help */
                exit_help(argc, argv);
                break;
            case 'v': /* verbose */
                ++traceLevel;
                break;
            }
        }
        
    }

#if defined(P2P_HAVE_DAEMON)
    if (sss.daemon)
    {
        useSyslog=1; /* traceEvent output now goes to syslog. */
        if ( -1 == daemon( 0, 0 ) )
        {
            traceEvent( TRACE_ERROR, "Failed to become daemon." );
            exit(-5);
        }
    }
#endif /* #if defined(P2P_HAVE_DAEMON) */

    traceEvent( TRACE_DEBUG, "traceLevel is %d", traceLevel);

    sss.sock = open_socket(sss.lport, 1 /*bind ANY*/ );
    if ( -1 == sss.sock )
    {
        traceEvent( TRACE_ERROR, "Failed to open main socket. %s", strerror(errno) );
        exit(-2);
    }
    else
    {
        traceEvent( TRACE_NORMAL, "supernode is listening on UDP %u (main)", sss.lport );
    }

    sss.mgmt_sock = open_socket(P2P_SN_MGMT_PORT, 1 /* bind ANY */ );
    if ( -1 == sss.mgmt_sock )
    {
        traceEvent( TRACE_ERROR, "Failed to open management socket. %s", strerror(errno) );
        exit(-2);
    }
    else
    {
        traceEvent( TRACE_NORMAL, "supernode is listening on UDP %u (management)", P2P_SN_MGMT_PORT );
    }

    traceEvent(TRACE_NORMAL, "supernode started");

    return run_loop(&sss);
}


/** Long lived processing entry point. Split out from main to simply
 *  daemonisation on some platforms. */
static int run_loop( p2p_sn_t * sss )
{
    uint8_t pktbuf[P2P_SN_PKTBUF_SIZE];
    int keep_running=1;

    sss->start_time = time(NULL);

    while(keep_running) 
    {
        int rc;
        ssize_t bread;
        int max_sock;
        fd_set socket_mask;
        struct timeval wait_time;
        time_t now=0;

        FD_ZERO(&socket_mask);
        max_sock = MAX(sss->sock, sss->mgmt_sock);

        FD_SET(sss->sock, &socket_mask);
        FD_SET(sss->mgmt_sock, &socket_mask);

        wait_time.tv_sec = 10; wait_time.tv_usec = 0;
        rc = select(max_sock+1, &socket_mask, NULL, NULL, &wait_time);

        now = time(NULL);

        if(rc > 0) 
        {
            if (FD_ISSET(sss->sock, &socket_mask)) 
			{
				struct sockaddr_in  sender_sock;
				socklen_t           i;

				i = sizeof(sender_sock);
				bread = recvfrom( sss->sock, pktbuf, P2P_SN_PKTBUF_SIZE, 0/*flags*/,
					(struct sockaddr *)&sender_sock, (socklen_t*)&i);

				if ( bread == SOCKET_ERROR ) /* For UDP bread of zero just means no data (unlike TCP). */
				{
					/* The fd is no good now. Maybe we lost our interface. */
					traceEvent( TRACE_ERROR, "recvfrom() failed %d errno %d (%s)", bread, errno, strerror(errno) );
					keep_running=0;
					break;
				}
				if(bread == 0)
				{
					continue;
				}

				/* We have a datagram to process */
				process_udp( sss, &sender_sock, pktbuf, bread, now );
			}

            if (FD_ISSET(sss->mgmt_sock, &socket_mask)) 
            {
                struct sockaddr_in  sender_sock;
                size_t              i;

                i = sizeof(sender_sock);
                bread = recvfrom( sss->mgmt_sock, pktbuf, P2P_SN_PKTBUF_SIZE, 0/*flags*/,
				  (struct sockaddr *)&sender_sock, (socklen_t*)&i);

                if ( bread == SOCKET_ERROR )
                {
                    traceEvent( TRACE_ERROR, "recvfrom() errno %d", WSAGetLastError() );
                    keep_running=0;
                    break;
                }
				if(bread == 0)
					continue;

                /* We have a datagram to process */
                process_mgmt( sss, &sender_sock, pktbuf, bread, now );
            }
        }
        else
        {
            traceEvent( TRACE_DEBUG, "timeout" );
        }

        purge_expired_registrations( &(sss->edges) );

    } /* while */

    deinit_sn( sss );

    return 0;
}

