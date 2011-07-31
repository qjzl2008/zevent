#include "punch_common.h"

#include <assert.h>

#if defined(DEBUG)
#   define PURGE_REGISTRATION_FREQUENCY   60
#   define REGISTRATION_TIMEOUT          120
#else /* #if defined(DEBUG) */
#   define PURGE_REGISTRATION_FREQUENCY   60
#   define REGISTRATION_TIMEOUT           (60*5)
#endif /* #if defined(DEBUG) */

/* ************************************** */

SOCKET open_socket(int local_port, int bind_any) {
  SOCKET sock_fd;
  struct sockaddr_in local_address;
  int sockopt = 1;

  if((sock_fd = socket(PF_INET, SOCK_DGRAM, 0))  < 0) {
    traceEvent(TRACE_ERROR, "Unable to create socket [%s][%d]\n",
	       strerror(errno), sock_fd);
    return(-1);
  }

#ifndef WIN32
  /* fcntl(sock_fd, F_SETFL, O_NONBLOCK); */
#endif

  setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&sockopt, sizeof(sockopt));

  memset(&local_address, 0, sizeof(local_address));
  local_address.sin_family = AF_INET;
  local_address.sin_port = htons(local_port);
  local_address.sin_addr.s_addr = htonl(bind_any?INADDR_ANY:INADDR_LOOPBACK);
  if(bind(sock_fd, (struct sockaddr*) &local_address, sizeof(local_address)) == -1) {
    traceEvent(TRACE_ERROR, "Bind error [%s]\n", strerror(errno));
    return(-1);
  }

  return(sock_fd);
}

int traceLevel = 6 /* NORMAL */;
int useSyslog = 0, syslog_opened = 0;

#define P2P_TRACE_DATESIZE 32
void traceEvent(int eventTraceLevel, char* file, int line, char * format, ...) {
  va_list va_ap;

  if(eventTraceLevel <= traceLevel) {
    char buf[2048];
    char out_buf[640];
    char theDate[P2P_TRACE_DATESIZE];
    char *extra_msg = "";
    time_t theTime = time(NULL);
#ifdef WIN32
	int i;
#endif

    /* We have two paths - one if we're logging, one if we aren't
     *   Note that the no-log case is those systems which don't support it (WIN32),
     *                                those without the headers !defined(USE_SYSLOG)
     *                                those where it's parametrically off...
     */

    memset(buf, 0, sizeof(buf));
    strftime(theDate, P2P_TRACE_DATESIZE, "%d/%b/%Y %H:%M:%S", localtime(&theTime));

    va_start (va_ap, format);
    vsnprintf(buf, sizeof(buf)-1, format, va_ap);
    va_end(va_ap);

    if(eventTraceLevel == 0 /* TRACE_ERROR */)
      extra_msg = "ERROR: ";
    else if(eventTraceLevel == 1 /* TRACE_WARNING */)
      extra_msg = "WARNING: ";

    while(buf[strlen(buf)-1] == '\n') buf[strlen(buf)-1] = '\0';

#ifndef WIN32
    if(useSyslog) {
      if(!syslog_opened) {
        openlog("p2p", LOG_PID, LOG_DAEMON);
        syslog_opened = 1;
      }

      snprintf(out_buf, sizeof(out_buf), "%s%s", extra_msg, buf);
      syslog(LOG_INFO, "%s", out_buf);
    } else {
      snprintf(out_buf, sizeof(out_buf), "%s [%11s:%4d] %s%s", theDate, file, line, extra_msg, buf);
      printf("%s\n", out_buf);
      fflush(stdout);
    }
#else
    /* this is the WIN32 code */
	for(i=strlen(file)-1; i>0; i--) if(file[i] == '\\') { i++; break; };
    snprintf(out_buf, sizeof(out_buf), "%s [%11s:%4d] %s%s", theDate, &file[i], line, extra_msg, buf);
    printf("%s\n", out_buf);
    fflush(stdout);
#endif
  }

}

/* *********************************************** */

/* addr should be in network order. Things are so much simpler that way. */
char* intoa(uint32_t /* host order */ addr, char* buf, uint16_t buf_len) {
  char *cp, *retStr;
  uint8_t byteval;
  int n;

  cp = &buf[buf_len];
  *--cp = '\0';

  n = 4;
  do {
    byteval = addr & 0xff;
    *--cp = byteval % 10 + '0';
    byteval /= 10;
    if (byteval > 0) {
      *--cp = byteval % 10 + '0';
      byteval /= 10;
      if (byteval > 0)
        *--cp = byteval + '0';
    }
    *--cp = '.';
    addr >>= 8;
  } while (--n > 0);

  /* Convert the string to lowercase */
  retStr = (char*)(cp+1);

  return(retStr);
}


/* *********************************************** */

void hexdump(const uint8_t * buf, size_t len)
{
    size_t i;

    if ( 0 == len ) { return; }

    for(i=0; i<len; i++)
    {
        if((i > 0) && ((i % 16) == 0)) { printf("\n"); }
        printf("%02X ", buf[i] & 0xFF);
    }

    printf("\n");
}

/* *********************************************** */

void print_p2p_version() {
  printf("Welcome to p2p v.%s for %s\n"
         "Built on %s\n"
	 "Copyright 2007-09 - http://www.zhoubug.org\n\n",
         p2p_sw_version, p2p_sw_osName, p2p_sw_buildDate);
}




/** Find the peer entry in list with mac_addr equal to mac.
 *
 *  Does not modify the list.
 *
 *  @return NULL if not found; otherwise pointer to peer entry.
 */
struct peer_info * find_peer_by_addr( struct peer_info * list, const p2p_sock_t *addr )
{
	while(list != NULL)
	{
		if(addr->port == list->sock.port && (memcmp(addr->addr.v4,list->sock.addr.v4,IPV4_SIZE) == 0))
		{
			return list;
		}
		list = list->next;
	}

	return NULL;
}


/** Return the number of elements in the list.
 *
 */
size_t peer_list_size( const struct peer_info * list )
{
  size_t retval=0;

  while ( list )
    {
      ++retval;
      list = list->next;
    }

  return retval;
}

/** Add new to the head of list. If list is NULL; create it.
 *
 *  The item new is added to the head of the list. New is modified during
 *  insertion. list takes ownership of new.
 */
void peer_list_add( struct peer_info * * list,
                    struct peer_info * new )
{
  new->next = *list;
  new->last_seen = time(NULL);
  *list = new;
}


size_t purge_expired_registrations( struct peer_info ** peer_list ) {
  static time_t last_purge = 0;
  time_t now = time(NULL);
  size_t num_reg = 0;

  if((now - last_purge) < PURGE_REGISTRATION_FREQUENCY) return 0;

  traceEvent(TRACE_INFO, "Purging old registrations");

  num_reg = purge_peer_list( peer_list, now-REGISTRATION_TIMEOUT );

  last_purge = now;
  traceEvent(TRACE_INFO, "Remove %ld registrations", num_reg);

  return num_reg;
}

/** Purge old items from the peer_list and return the number of items that were removed. */
size_t purge_peer_list( struct peer_info ** peer_list,
                        time_t purge_before )
{
  struct peer_info *scan;
  struct peer_info *prev;
  size_t retval=0;

  scan = *peer_list;
  prev = NULL;
  while(scan != NULL)
  {
	  if(scan->last_seen < purge_before)
	  {
		  struct peer_info *next = scan->next;

		  if(prev == NULL)
		  {
			  *peer_list = next;
		  }
		  else
		  {
			  prev->next = next;
		  }

		  ++retval;
		  free(scan);
		  scan = next;
	  }
	  else
	  {
		  prev = scan;
		  scan = scan->next;
	  }
  }

  return retval;
}

/** Purge all items from the peer_list and return the number of items that were removed. */
size_t clear_peer_list( struct peer_info ** peer_list )
{
    struct peer_info *scan;
    struct peer_info *prev;
    size_t retval=0;

    scan = *peer_list;
    prev = NULL;
    while(scan != NULL)
    {
        struct peer_info *next = scan->next;

        if(prev == NULL)
        {
            *peer_list = next;
        }
        else
        {
            prev->next = next;
        }

        ++retval;
        free(scan);
        scan = next;
    }

    return retval;
}

static uint8_t hex2byte( const char * s )
{
  char tmp[3];
  tmp[0]=s[0];
  tmp[1]=s[1];
  tmp[2]=0; /* NULL term */

  return((uint8_t)strtol( s, NULL, 16 ));
}

extern char * sock_to_cstr( p2p_sock_str_t out,
                            const p2p_sock_t * sock )
{
    int r;

    if ( NULL == out ) { return NULL; }
    memset(out, 0, P2P_SOCKBUF_SIZE);

    if ( AF_INET6 == sock->family )
    {
        /* INET6 not written yet */
        r = snprintf( out, P2P_SOCKBUF_SIZE, "XXXX:%hu", sock->port );
        return out;
    }
    else
    {
        const uint8_t * a = sock->addr.v4;
        r = snprintf( out, P2P_SOCKBUF_SIZE, "%hu.%hu.%hu.%hu:%hu", 
                      (a[0] & 0xff), (a[1] & 0xff), (a[2] & 0xff), (a[3] & 0xff), sock->port );
        return out;
    }
}

/* @return zero if the two sockets are equivalent. */
int sock_equal( const p2p_sock_t * a,
                const p2p_sock_t * b )
{
    if ( a->port != b->port ) { return 1; }
    if ( a->family != b->family ) { return 1; }
    switch (a->family) /* they are the same */
    {
    case AF_INET:
        if ( 0 != memcmp( a->addr.v4, b->addr.v4, IPV4_SIZE ) ) { return 1;};
        break;
    default:
        if ( 0 != memcmp( a->addr.v6, b->addr.v6, IPV6_SIZE ) ) { return 1;};
        break;
    }

    return 0;
}

