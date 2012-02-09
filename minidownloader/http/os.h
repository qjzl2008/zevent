#ifndef _HTTP_OS_H_
#define _HTTP_OS_H_

#if !defined(OS_UNIX)
# ifndef OS_WIN
#   if defined(_WIN32) || defined(WIN32) || defined(__CYGWIN__) || defined(__MINGW32__) || defined(__BORLANDC__)
#     define OS_WIN 1
#     define OS_UNIX 0
#   else
#     define OS_WIN 0
#     define OS_UNIX 1
#  endif
# else
#  define OS_UNIX 0
# endif
#else
# ifndef OS_WIN
#  define OS_WIN 0
# endif
#endif

#if OS_UNIX
#define ZNet_Os_Socket_Udp_Setup          ZNet_Unix_Socket_Udp_Setup
#define ZNet_Os_Socket_Udp_Close          ZNet_Unix_Socket_Udp_Close
#define ZNet_Os_Socket_Udp_Send           ZNet_Unix_Socket_Udp_Send
#define ZNet_Os_Socket_Udp_Recv           ZNet_Unix_Socket_Udp_Recv

#define ZNet_Os_Socket_Connect            ZNet_Unix_Socket_Connect
#define ZNet_Os_Socket_Close              ZNet_Unix_Socket_Close
#define ZNet_Os_Socket_Send               ZNet_Unix_Socket_Send
#define ZNet_Os_Socket_Recv               ZNet_Unix_Socket_Recv

#define ZNet_Os_Get_Local_Ip              ZNet_Unix_Get_Local_Ip
#endif

#if OS_WIN
#define ZNet_Os_Socket_Udp_Setup          ZNet_Win_Socket_Udp_Setup
#define ZNet_Os_Socket_Udp_Close          ZNet_Win_Socket_Udp_Close
#define ZNet_Os_Socket_Udp_Send           ZNet_Win_Socket_Udp_Send
#define ZNet_Os_Socket_Udp_Recv           ZNet_Win_Socket_Udp_Recv

#define ZNet_Os_Socket_Connect            ZNet_Win_Socket_Connect
#define ZNet_Os_Socket_Close              ZNet_Win_Socket_Close
#define ZNet_Os_Socket_Send               ZNet_Win_Socket_Send
#define ZNet_Os_Socket_Recv               ZNet_Win_Socket_Recv

#define ZNet_Os_Get_Local_Ip              ZNet_Win_Get_Local_Ip
#endif


typedef struct OsSocket OsSocket;


int ZNet_Os_Socket_Udp_Setup(const char *local_ip,OsSocket ** s);
int ZNet_Os_Socket_Udp_Close(OsSocket ** s);
int ZNet_Os_Socket_Udp_Send(OsSocket * s, const char * host_addr, short int port, 
                            char * buf, int amt, int * amt_sent);
int ZNet_Os_Socket_Udp_Recv(OsSocket * s, const char * host_addr, short int port,
                            char * buf, int amt, int * amt_recv, int timeout_sec);

int ZNet_Os_Socket_Connect(OsSocket ** s, const char * host_addr, 
                           short int port, int timeout_sec);
int ZNet_Os_Socket_Close(OsSocket **);
int ZNet_Os_Socket_Send(OsSocket *, char * buf, int amt, int * amt_sent);
int ZNet_Os_Socket_Recv(OsSocket *, char * buf, int amt, 
                        int * amt_recv, int timeout_sec);


int ZNet_Os_Get_Local_Ip(OsSocket *, char ** local_ip);


#endif
