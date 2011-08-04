#if !defined( P2P_WIRE_H_ )
#define P2P_WIRE_H_

#ifdef __cplusplus
extern "C"
#endif

#include <stdlib.h>

#if defined(WIN32)
#include "win32/p2p_win32.h"

#else /* #if defined(WIN32) */
#include <stdint.h>
#include <netinet/in.h>
#include <sys/socket.h> /* AF_INET and AF_INET6 */
#endif /* #if defined(WIN32) */

#define P2P_PKT_VERSION                 2
#define P2P_DEFAULT_TTL                 2       /* can be forwarded twice at most */
#define P2P_COMMUNITY_SIZE              16
#define P2P_COOKIE_SIZE                 4
#define P2P_PKT_BUF_SIZE                2048
#define P2P_SOCKBUF_SIZE                64      /* string representation of INET or INET6 sockets */

typedef uint8_t p2p_community_t[P2P_COMMUNITY_SIZE];
typedef uint8_t p2p_cookie_t[P2P_COOKIE_SIZE];

typedef char    p2p_sock_str_t[P2P_SOCKBUF_SIZE];       /* tracing string buffer */

enum p2p_pc
{
    p2p_ping=0,                 /* Not used */
    p2p_register=1,             /* Register edge to edge */
    p2p_deregister=2,           /* Deregister this edge */
    p2p_packet=3,               /* PACKET data content */
    p2p_register_ack=4,         /* ACK of a registration from edge to edge */
    p2p_register_super=5,       /* Register edge to supernode */
    p2p_register_super_ack=6,   /* ACK from supernode to edge */
    p2p_register_super_nak=7,   /* NAK from supernode to edge - registration refused */
	p2p_heartbeat=8
};

typedef enum p2p_pc p2p_pc_t;

#define P2P_FLAGS_OPTIONS               0x0080
#define P2P_FLAGS_SOCKET                0x0040
#define P2P_FLAGS_FROM_SUPERNODE        0x0020

/* The bits in flag that are the packet type */
#define P2P_FLAGS_TYPE_MASK             0x001f  /* 0 - 31 */
#define P2P_FLAGS_BITS_MASK             0xffe0

#define IPV4_SIZE                       4
#define IPV6_SIZE                       16


#define P2P_AUTH_TOKEN_SIZE             32      /* bytes */


#define P2P_EUNKNOWN                    -1
#define P2P_ENOTIMPL                    -2
#define P2P_EINVAL                      -3
#define P2P_ENOSPACE                    -4


typedef uint16_t p2p_flags_t;
typedef uint16_t p2p_transform_t;       /* Encryption, compression type. */
typedef uint32_t p2p_sa_t;              /* security association number */

struct p2p_sock 
{
    uint8_t     family;         /* AF_INET or AF_INET6; or 0 if invalid */
    uint16_t    port;           /* host order */
    union 
    {
    uint8_t     v6[IPV6_SIZE];  /* byte sequence */
    uint8_t     v4[IPV4_SIZE];  /* byte sequence */
    } addr;
};

typedef struct p2p_sock p2p_sock_t;

struct p2p_auth
{
    uint16_t    scheme;                         /* What kind of auth */
    uint16_t    toksize;                        /* Size of auth token */
    uint8_t     token[P2P_AUTH_TOKEN_SIZE];     /* Auth data interpreted based on scheme */
};

typedef struct p2p_auth p2p_auth_t;


struct p2p_common
{
    /* int                 version; */
    uint8_t             ttl;
    p2p_pc_t            pc;
    p2p_flags_t         flags;
    p2p_community_t     community;
};

typedef struct p2p_common p2p_common_t;

struct p2p_REGISTER
{
    p2p_cookie_t        cookie;         /* Link REGISTER and REGISTER_ACK */
    p2p_sock_t          orig_sender;         /* src */
    p2p_sock_t          dest_sock;           /* REVISIT: unused? */
};

typedef struct p2p_REGISTER p2p_REGISTER_t;

struct p2p_REGISTER_ACK
{
    p2p_cookie_t        cookie;         /* Return cookie from REGISTER */
    p2p_sock_t          sock;           /* Supernode's view of edge socket (IP Addr, port) */
};

typedef struct p2p_REGISTER_ACK p2p_REGISTER_ACK_t;

struct p2p_PACKET
{
    p2p_sock_t          sock;
};

typedef struct p2p_PACKET p2p_PACKET_t;


/* Linked with p2p_register_super in p2p_pc_t. Only from edge to supernode. */
struct p2p_REGISTER_SUPER
{
    p2p_cookie_t        cookie;         /* Link REGISTER_SUPER and REGISTER_SUPER_ACK */
    p2p_auth_t          auth;           /* Authentication scheme and tokens */
};

typedef struct p2p_REGISTER_SUPER p2p_REGISTER_SUPER_t;


/* Linked with p2p_register_super_ack in p2p_pc_t. Only from supernode to edge. */
struct p2p_REGISTER_SUPER_ACK
{
    p2p_cookie_t        cookie;         /* Return cookie from REGISTER_SUPER */
    uint16_t            lifetime;       /* How long the registration will live */
    p2p_sock_t          sock;           /* Sending sockets associated with edgeMac */

    /* The packet format provides additional supernode definitions here. 
     * uint8_t count, then for each count there is one
     * p2p_sock_t.
     */
    uint8_t             num_sn;         /* Number of supernodes that were send
                                         * even if we cannot store them all. If
                                         * non-zero then sn_bak is valid. */
    p2p_sock_t          sn_bak;         /* Socket of the first backup supernode */

};

typedef struct p2p_REGISTER_SUPER_ACK p2p_REGISTER_SUPER_ACK_t;


/* Linked with p2p_register_super_ack in p2p_pc_t. Only from supernode to edge. */
struct p2p_REGISTER_SUPER_NAK
{
    p2p_cookie_t        cookie;         /* Return cookie from REGISTER_SUPER */
};

typedef struct p2p_REGISTER_SUPER_NAK p2p_REGISTER_SUPER_NAK_t;



struct p2p_buf
{
    uint8_t *   data;
    size_t      size;
};

typedef struct p2p_buf p2p_buf_t;

int encode_uint8( uint8_t * base, 
                  size_t * idx,
                  const uint8_t v );

int decode_uint8( uint8_t * out,
                  const uint8_t * base,
                  size_t * rem,
                  size_t * idx );

int encode_uint16( uint8_t * base, 
                   size_t * idx,
                   const uint16_t v );

int decode_uint16( uint16_t * out,
                   const uint8_t * base,
                   size_t * rem,
                   size_t * idx );

int encode_uint32( uint8_t * base, 
                   size_t * idx,
                   const uint32_t v );

int decode_uint32( uint32_t * out,
                   const uint8_t * base,
                   size_t * rem,
                   size_t * idx );

int encode_buf( uint8_t * base, 
                size_t * idx,
                const void * p, 
                size_t s);

int decode_buf( uint8_t * out,
                size_t bufsize,
                const uint8_t * base,
                size_t * rem,
                size_t * idx );

int encode_common( uint8_t * base, 
                   size_t * idx,
                   const p2p_common_t * common );

int decode_common( p2p_common_t * out,
                   const uint8_t * base,
                   size_t * rem,
                   size_t * idx );

int encode_sock( uint8_t * base, 
                 size_t * idx,
                 const p2p_sock_t * sock );

int decode_sock( p2p_sock_t * sock,
                 const uint8_t * base,
                 size_t * rem,
                 size_t * idx );

int encode_REGISTER( uint8_t * base, 
                     size_t * idx,
                     const p2p_common_t * common, 
                     const p2p_REGISTER_t * reg );

int decode_REGISTER( p2p_REGISTER_t * pkt,
                     const p2p_common_t * cmn, /* info on how to interpret it */
                     const uint8_t * base,
                     size_t * rem,
                     size_t * idx );

int encode_REGISTER_SUPER( uint8_t * base, 
                           size_t * idx,
                           const p2p_common_t * common, 
                           const p2p_REGISTER_SUPER_t * reg );

int decode_REGISTER_SUPER( p2p_REGISTER_SUPER_t * pkt,
                           const p2p_common_t * cmn, /* info on how to interpret it */
                           const uint8_t * base,
                           size_t * rem,
                           size_t * idx );

int encode_REGISTER_ACK( uint8_t * base, 
                         size_t * idx,
                         const p2p_common_t * common, 
                         const p2p_REGISTER_ACK_t * reg );

int decode_REGISTER_ACK( p2p_REGISTER_ACK_t * pkt,
                         const p2p_common_t * cmn, /* info on how to interpret it */
                         const uint8_t * base,
                         size_t * rem,
                         size_t * idx );

int encode_REGISTER_SUPER_ACK( uint8_t * base,
                               size_t * idx,
                               const p2p_common_t * cmn,
                               const p2p_REGISTER_SUPER_ACK_t * reg );

int decode_REGISTER_SUPER_ACK( p2p_REGISTER_SUPER_ACK_t * reg,
                               const p2p_common_t * cmn, /* info on how to interpret it */
                               const uint8_t * base,
                               size_t * rem,
                               size_t * idx );

int fill_sockaddr( struct sockaddr * addr, 
                   size_t addrlen, 
                   const p2p_sock_t * sock );

int encode_PACKET( uint8_t * base, 
                   size_t * idx,
                   const p2p_common_t * common, 
                   const p2p_PACKET_t * pkt );

int decode_PACKET( p2p_PACKET_t * pkt,
                   const p2p_common_t * cmn, /* info on how to interpret it */
                   const uint8_t * base,
                   size_t * rem,
                   size_t * idx );
#ifdef __cplusplus
}
#endif

#endif /* #if !defined( P2P_WIRE_H_ ) */
