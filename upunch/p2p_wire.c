/* (c) 2009 Richard Andrews <andrews@ntop.org> */

/** Routines for encoding and decoding p2p packets on the wire.
 *
 *  encode_X(base,idx,v) prototypes are inspired by the erlang internal
 *  encoding model. Passing the start of a buffer in base and a pointer to an
 *  integer (initially set to zero). Each encode routine increases idx by the
 *  amount written and returns the amount written. In this way complex sequences
 *  of encodings can be represented cleanly. See encode_register() for an
 *  example.
 */

#include "p2p_wire.h"
#include <string.h>

int encode_uint8( uint8_t * base, 
                  size_t * idx,
                  const uint8_t v )
{
    *(base + (*idx)) = (v & 0xff);
    ++(*idx);
    return 1;
}

int decode_uint8( uint8_t * out,
                  const uint8_t * base,
                  size_t * rem,
                  size_t * idx )
{
    if (*rem < 1 ) { return 0; }

    *out = ( base[*idx] & 0xff );
    ++(*idx);
    --(*rem);
    return 1;
}

int encode_uint16( uint8_t * base, 
                   size_t * idx,
                   const uint16_t v )
{
    *(base + (*idx))     = ( v >> 8) & 0xff;
    *(base + (1 + *idx)) = ( v & 0xff );
    *idx += 2;
    return 2;
}

int decode_uint16( uint16_t * out,
                   const uint8_t * base,
                   size_t * rem,
                   size_t * idx )
{
    if (*rem < 2 ) { return 0; }

    *out  = ( base[*idx] & 0xff ) << 8;
    *out |= ( base[1 + *idx] & 0xff );
    *idx += 2;
    *rem -= 2;
    return 2;
}

int encode_uint32( uint8_t * base, 
                   size_t * idx,
                   const uint32_t v )
{
    *(base + (0 + *idx)) = ( v >> 24) & 0xff;
    *(base + (1 + *idx)) = ( v >> 16) & 0xff;
    *(base + (2 + *idx)) = ( v >> 8) & 0xff;
    *(base + (3 + *idx)) = ( v & 0xff );
    *idx += 4;
    return 4;
}

int decode_uint32( uint32_t * out,
                   const uint8_t * base,
                   size_t * rem,
                   size_t * idx )
{
    if (*rem < 4 ) { return 0; }

    *out  = ( base[0 + *idx] & 0xff ) << 24;
    *out |= ( base[1 + *idx] & 0xff ) << 16;
    *out |= ( base[2 + *idx] & 0xff ) << 8;
    *out |= ( base[3 + *idx] & 0xff );
    *idx += 4;
    *rem -= 4;
    return 4;
}

int encode_buf( uint8_t * base, 
                size_t * idx,
                const void * p, 
                size_t s)
{
    memcpy( (base + (*idx)), p, s );
    *idx += s;
    return s;
}

/* Copy from base to out of size bufsize */
int decode_buf( uint8_t * out,
                size_t bufsize,
                const uint8_t * base,
                size_t * rem,
                size_t * idx )
{
    if (*rem < bufsize ) { return 0; }

    memcpy( out, (base + *idx), bufsize );
    *idx += bufsize;
    *rem -= bufsize;
    return bufsize;
}

int encode_common( uint8_t * base, 
                   size_t * idx,
                   const p2p_common_t * common )
{
    uint16_t flags=0;
    encode_uint8( base, idx, P2P_PKT_VERSION );
    encode_uint8( base, idx, common->ttl );

    flags  = common->pc & P2P_FLAGS_TYPE_MASK;
    flags |= common->flags & P2P_FLAGS_BITS_MASK;

    encode_uint16( base, idx, flags );
    encode_buf( base, idx, common->community, P2P_COMMUNITY_SIZE );
    
    return -1;
}

int decode_common( p2p_common_t * out,
                   const uint8_t * base,
                   size_t * rem,
                   size_t * idx )
{
    size_t idx0=*idx;
    uint8_t dummy=0;
    decode_uint8( &dummy, base, rem, idx );

    if ( P2P_PKT_VERSION != dummy )
    {
        return -1;
    }
    
    decode_uint8( &(out->ttl), base, rem, idx );
    decode_uint16( &(out->flags), base, rem, idx );
    out->pc = ( out->flags & P2P_FLAGS_TYPE_MASK );
    out->flags &= P2P_FLAGS_BITS_MASK;

    decode_buf( out->community, P2P_COMMUNITY_SIZE, base, rem, idx );

    return (*idx - idx0);
}


int encode_sock( uint8_t * base, 
                 size_t * idx,
                 const p2p_sock_t * sock )
{
    int retval=0;
    uint16_t f;

    switch (sock->family) 
    {
    case AF_INET:
    {
        f = 0;
        retval += encode_uint16(base,idx,f);
        retval += encode_uint16(base,idx,sock->port);
        retval += encode_buf(base,idx,sock->addr.v4,IPV4_SIZE);
        break;
    }
    case AF_INET6:
    {
        f = 0x8000;
        retval += encode_uint16(base,idx,f);
        retval += encode_uint16(base,idx,sock->port);
        retval += encode_buf(base,idx,sock->addr.v6,IPV6_SIZE);
        break;
    }
    default:
        retval=-1;
    }

    return retval;
}


int decode_sock( p2p_sock_t * sock,
                 const uint8_t * base,
                 size_t * rem,
                 size_t * idx )
{
    size_t * idx0=idx;
    uint16_t f;
    
    decode_uint16( &f, base, rem, idx );

    if( f & 0x8000 )
    {
        /* IPv6 */
        sock->family = AF_INET6;
        decode_uint16( &(sock->port), base, rem, idx );
        decode_buf( sock->addr.v6, IPV6_SIZE, base, rem, idx );
    }
    else
    {
        /* IPv4 */
        sock->family = AF_INET;
        decode_uint16( &(sock->port), base, rem, idx );
        memset( sock->addr.v6, 0, IPV6_SIZE ); /* so memcmp() works for equality. */
        decode_buf( sock->addr.v4, IPV4_SIZE, base, rem, idx );
    }

    return (idx-idx0);
}

int encode_REGISTER( uint8_t * base, 
                     size_t * idx,
                     const p2p_common_t * common, 
                     const p2p_REGISTER_t * reg )
{
    int retval=0;
    retval += encode_common( base, idx, common );
    retval += encode_buf( base, idx, reg->cookie, P2P_COOKIE_SIZE );
	retval += encode_sock( base, idx, &(reg->orig_sender) );
	retval += encode_sock( base, idx, &(reg->dest_sock) );

    return retval;
}

int decode_REGISTER( p2p_REGISTER_t * reg,
                     const p2p_common_t * cmn, /* info on how to interpret it */
                     const uint8_t * base,
                     size_t * rem,
                     size_t * idx )
{
    size_t retval=0;
    memset( reg, 0, sizeof(p2p_REGISTER_t) );
    retval += decode_buf( reg->cookie, P2P_COOKIE_SIZE, base, rem, idx );
    retval += decode_sock( &(reg->orig_sender), base, rem, idx );
    retval += decode_sock( &(reg->dest_sock), base, rem, idx );

    return retval;
}

int encode_REGISTER_SUPER( uint8_t * base, 
                           size_t * idx,
                           const p2p_common_t * common, 
                           const p2p_REGISTER_SUPER_t * reg )
{
    int retval=0;
    retval += encode_common( base, idx, common );
    retval += encode_buf( base, idx, reg->cookie, P2P_COOKIE_SIZE );
    retval += encode_uint16( base, idx, 0 ); /* NULL auth scheme */
    retval += encode_uint16( base, idx, 0 ); /* No auth data */

    return retval;
}

int decode_REGISTER_SUPER( p2p_REGISTER_SUPER_t * reg,
                           const p2p_common_t * cmn, /* info on how to interpret it */
                           const uint8_t * base,
                           size_t * rem,
                           size_t * idx )
{
    size_t retval=0;
    memset( reg, 0, sizeof(p2p_REGISTER_SUPER_t) );
    retval += decode_buf( reg->cookie, P2P_COOKIE_SIZE, base, rem, idx );
    retval += decode_uint16( &(reg->auth.scheme), base, rem, idx );
    retval += decode_uint16( &(reg->auth.toksize), base, rem, idx );
    retval += decode_buf( reg->auth.token, reg->auth.toksize, base, rem, idx );
    return retval;
}

int encode_REGISTER_ACK( uint8_t * base, 
                         size_t * idx,
                         const p2p_common_t * common, 
                         const p2p_REGISTER_ACK_t * reg )
{
    int retval=0;
    retval += encode_common( base, idx, common );
    retval += encode_buf( base, idx, reg->cookie, P2P_COOKIE_SIZE );

    /* The socket in REGISTER_ACK is the socket from which the REGISTER
     * arrived. This is sent back to the sender so it knows what its public
     * socket is. */
    if ( 0 != reg->sock.family )
    {
        retval += encode_sock( base, idx, &(reg->sock) );
    }

    return retval;
}

int decode_REGISTER_ACK( p2p_REGISTER_ACK_t * reg,
                         const p2p_common_t * cmn, /* info on how to interpret it */
                         const uint8_t * base,
                         size_t * rem,
                         size_t * idx )
{
    size_t retval=0;
    memset( reg, 0, sizeof(p2p_REGISTER_ACK_t) );
    retval += decode_buf( reg->cookie, P2P_COOKIE_SIZE, base, rem, idx );

    /* The socket in REGISTER_ACK is the socket from which the REGISTER
     * arrived. This is sent back to the sender so it knows what its public
     * socket is. */
    if ( cmn->flags & P2P_FLAGS_SOCKET )
    {
        retval += decode_sock( &(reg->sock), base, rem, idx );
    }

    return retval;
}

int encode_REGISTER_SUPER_ACK( uint8_t * base,
                               size_t * idx,
                               const p2p_common_t * common,
                               const p2p_REGISTER_SUPER_ACK_t * reg )
{
    int retval=0;
    retval += encode_common( base, idx, common );
    retval += encode_buf( base, idx, reg->cookie, P2P_COOKIE_SIZE );
    retval += encode_uint16( base, idx, reg->lifetime );
    retval += encode_sock( base, idx, &(reg->sock) );
    retval += encode_uint8( base, idx, reg->num_sn );
    if ( reg->num_sn > 0 )
    {
        /* We only support 0 or 1 at this stage */
        retval += encode_sock( base, idx, &(reg->sn_bak) );
    }

    return retval;
}

int decode_REGISTER_SUPER_ACK( p2p_REGISTER_SUPER_ACK_t * reg,
                               const p2p_common_t * cmn, /* info on how to interpret it */
                               const uint8_t * base,
                               size_t * rem,
                               size_t * idx )
{
    size_t retval=0;

    memset( reg, 0, sizeof(p2p_REGISTER_SUPER_ACK_t) );
    retval += decode_buf( reg->cookie, P2P_COOKIE_SIZE, base, rem, idx );
    retval += decode_uint16( &(reg->lifetime), base, rem, idx );

    /* Socket is mandatory in this message type */
    retval += decode_sock( &(reg->sock), base, rem, idx );

    /* Following the edge socket are an array of backup supernodes. */
    retval += decode_uint8( &(reg->num_sn), base, rem, idx );
    if ( reg->num_sn > 0 )
    {
        /* We only support 0 or 1 at this stage */
        retval += decode_sock( &(reg->sn_bak), base, rem, idx );
    }

    return retval;
}

int fill_sockaddr( struct sockaddr * addr, 
                   size_t addrlen, 
                   const p2p_sock_t * sock )
{
    int retval=-1;

    if ( AF_INET == sock->family )
    {
        if ( addrlen >= sizeof(struct sockaddr_in) )
        {
            struct sockaddr_in * si = (struct sockaddr_in *)addr;
            si->sin_family = sock->family;
            si->sin_port = htons( sock->port );
            memcpy( &(si->sin_addr.s_addr), sock->addr.v4, IPV4_SIZE );
            retval=0;
        }
    }

    return retval;
}


int encode_PACKET( uint8_t * base, 
                   size_t * idx,
                   const p2p_common_t * common, 
                   const p2p_PACKET_t * pkt )
{
    int retval=0;
    retval += encode_common( base, idx, common );
    if ( 0 != pkt->sock.family )
    {
        retval += encode_sock( base, idx, &(pkt->sock) );
    }

    return retval;
}


int decode_PACKET( p2p_PACKET_t * pkt,
                   const p2p_common_t * cmn, /* info on how to interpret it */
                   const uint8_t * base,
                   size_t * rem,
                   size_t * idx )
{
    size_t retval=0;
    memset( pkt, 0, sizeof(p2p_PACKET_t) );

    if ( cmn->flags & P2P_FLAGS_SOCKET )
    {
        retval += decode_sock( &(pkt->sock), base, rem, idx );
    }

    return retval;
}

