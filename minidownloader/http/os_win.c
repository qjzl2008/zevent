#include "error.h"
#include "os_common.h"


static int Initialize_Winsock_Library();
static int Is_Valid_Library(WSADATA * wsaData);
static int Initialize_Sockaddr_in(struct sockaddr_in* server, struct hostent** hp, 
                                  const char * host, short int port);



int ZNet_Win_Socket_Udp_Setup(const char *local_ip,OsSocket ** s)
{
  int ret;
  int blockMode = 1;              
    struct sockaddr_in sa;
  *s = (OsSocket *)malloc(sizeof(OsSocket));
  if(*s == NULL) {
    return BAD_MALLOC;
  }
  
  if((ret = Initialize_Winsock_Library()) != OK) {
    free(*s);
    return ret;
  }

  if(((*s)->sock = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
    WSACleanup();   
    free(*s);
    return SOCKET_INVALID;
  }
  
  if(ioctlsocket((*s)->sock, FIONBIO, (u_long FAR*)&blockMode)) {
    WSACleanup();  
    return SOCKET_IOCTL_FAILURE;
  }
  
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = inet_addr(local_ip);
  sa.sin_port = 0;

  ret = bind((*s)->sock, (SOCKADDR*)&sa, sizeof(sa));
  return OK;
}

int ZNet_Win_Socket_Udp_Close(OsSocket ** s)
{
  return ZNet_Win_Socket_Close(s);
}

int ZNet_Win_Socket_Udp_Send(OsSocket * s, const char * host_addr, short int port, 
                            char * buf, int amt, int * amt_sent)
{
  return ZNet_Common_Socket_Udp_Send(s, host_addr, port, buf, amt, amt_sent);
}


int ZNet_Win_Socket_Udp_Recv(OsSocket * s, const char * host_addr, short int port,
                            char * buf, int amt, int * amt_recv, int timeout_sec)
{
  return ZNet_Common_Socket_Udp_Recv(s, host_addr, port, buf, amt, 
                                     amt_recv, timeout_sec);
}


int ZNet_Win_Socket_Connect(OsSocket ** s, const char * host_addr, short int port,
                            int timeout_sec)
{
  int blockMode = 1;              
  struct sockaddr_in server;      
  struct hostent * hp;           
  int ret;
  
  *s = (OsSocket *)malloc(sizeof(OsSocket));
  if(*s == NULL) {
    return BAD_MALLOC;
  }
 
  if((ret = Initialize_Winsock_Library()) != OK) {
    free(*s);
    return ret;
  }
  
  if(((*s)->sock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
    WSACleanup();   
    free(*s);
    return SOCKET_INVALID;
  }
  
  if(ioctlsocket((*s)->sock, FIONBIO, (u_long FAR*)&blockMode)) {
    WSACleanup();   
    return SOCKET_IOCTL_FAILURE;
  }
  
  if((ret = Initialize_Sockaddr_in(&server, &hp, host_addr, port)) != OK) {
    WSACleanup();
    free(*s);
    return ret;
  }
  
  connect((*s)->sock, (struct sockaddr *)&server, sizeof(server));
  if(Select_Till_Readywrite(*s, timeout_sec) != OK) {
    WSACleanup(); 
    free(*s);
    return SOCKET_CONNECT_FAILED;
  }
  
  return OK;
}

int ZNet_Win_Socket_Close(OsSocket ** s)
{
  (void)closesocket((*s)->sock);
  WSACleanup();
  free(*s);
  return OK;
}

int ZNet_Win_Socket_Send(OsSocket * s, char * buf, int amt, int * amt_sent)
{
  return ZNet_Common_Socket_Send(s, buf, amt, amt_sent);
}

int ZNet_Win_Socket_Recv(OsSocket * s, char * buf, int amt, 
                         int * amt_recv, int timeout_sec)
{
  return ZNet_Common_Socket_Recv(s, buf, amt, amt_recv, timeout_sec);
}


int ZNet_Win_Get_Local_Ip(OsSocket * s, char ** local_ip)
{
  return ZNet_Common_Get_Local_Ip(s, local_ip);
}


static int Initialize_Winsock_Library()
{
  WORD wVersionRequested;    
  WSADATA wsaData;           

  wVersionRequested = MAKEWORD(2, 2);
  if(WSAStartup(wVersionRequested, &wsaData) != 0) {
    WSACleanup(); 
    return SOCKET_WSASTARTUP_FAILED;
  }
  
  if(Is_Valid_Library(&wsaData) != OK) {
    WSACleanup();
    return SOCKET_INVALID_LIB;
  }

  return OK;
}


static Is_Valid_Library(WSADATA * wsaData)
{
  float socklib_ver;
  socklib_ver = HIBYTE(wsaData->wVersion)/10.0F;
  socklib_ver += LOBYTE(wsaData->wVersion);
  if(socklib_ver < 2.2) {
    return SOCKET_INVALID_LIB;
  }
  return OK;
}


static int Initialize_Sockaddr_in(struct sockaddr_in* server,
                                  struct hostent** hp,
                                  const char * host_addr,
                                  short int port)
{
  return Common_Initialize_Sockaddr_in(server, hp, host_addr, port);
}

