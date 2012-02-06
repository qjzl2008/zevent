#define _CRT_SECURE_NO_DEPRECATE 1

#include <errno.h>
#include "os_common.h"
#include "error.h"


int ZNet_Common_Socket_Udp_Send(OsSocket * s, const char * host_addr, 
                                short int port, char * buf, int amt, 
                                int * amt_sent)
{
  int ret;
  int send_ret = 1; 
  int sent_sofar = 0;
  struct sockaddr_in server;
  struct hostent* hp;

  if((ret = Common_Initialize_Sockaddr_in(&server, &hp, host_addr, port)) != OK) {
    return ret;
  }

  while(amt && !(send_ret <= 0)) {
    send_ret = sendto(s->sock, &(buf[sent_sofar]), amt, 0,
                      (struct sockaddr*)&server,
                      sizeof(struct sockaddr_in));
    if(send_ret > 0) {
      amt -= send_ret;
      sent_sofar += send_ret;
    }
  }
  *amt_sent = sent_sofar; 

  if(send_ret <= 0) {
    return SOCKET_SEND_FAILED;
  }

  return OK;
}
                                
int ZNet_Common_Socket_Udp_Recv(OsSocket * s, const char * host_addr, 
                                short int port, char * buf, int amt, 
                                int * amt_recv, int timeout_sec)
{
  int ret;
  int recv_ret = 1; 
  int recv_sofar = 0;
  struct sockaddr_in server;
  struct hostent* hp;
  unsigned int sender_addr_sz = sizeof(server);

  if((ret = Common_Initialize_Sockaddr_in(&server, &hp, host_addr, port)) != OK) {
    return ret;
  }
  
  if(Select_Till_Readyread(s, timeout_sec) == OK) {
    recv_ret = recvfrom(s->sock, &(buf[recv_sofar]), amt, 0,
                    (struct sockaddr*)&server,
                    &sender_addr_sz);
    recv_sofar += recv_ret;
  } else {
    recv_ret = 0;
  }

  if(recv_ret < 0) {
    *amt_recv = 0;
    return SOCKET_RECV_FAILED;
  }

  *amt_recv = recv_sofar;
  return OK;
}
                                
int ZNet_Common_Socket_Send(OsSocket * s, char * buf, int amt, int * amt_sent)
{
  int send_ret = 1; 
  int sent_sofar = 0;
  
  while(amt && !(send_ret <= 0)) {
    send_ret = send(s->sock, &(buf[sent_sofar]), amt, 0);
    if(send_ret > 0) {
      amt -= send_ret;
      sent_sofar += send_ret;
    }
  }
  *amt_sent = sent_sofar;

  if(send_ret <= 0) {
    return SOCKET_SEND_FAILED;
  }

  return OK;
}

int ZNet_Common_Socket_Recv(OsSocket * s, char * buf, int amt, 
                            int * amt_recv, int timeout_sec)
{
  int recv_ret = 1;   
  int recv_sofar = 0;
  
  while(amt && !(recv_ret <= 0)) {
    if(Select_Till_Readyread(s, timeout_sec) == OK) {
      recv_ret = recv(s->sock, &(buf[recv_sofar]), amt, 0);
      amt -= recv_ret;
      recv_sofar += recv_ret;
    } else {
      recv_ret = 0;
    }
  }
  if(recv_ret < 0) {
    *amt_recv = 0;
    return SOCKET_RECV_FAILED;
  }
  
  *amt_recv = recv_sofar;
  return OK;
}


int Common_Initialize_Sockaddr_in(struct sockaddr_in* server,
                                  struct hostent** hp,
                                  const char * host_addr,
                                  short int port)
{
  memset(server, 0, sizeof(struct sockaddr));
  server->sin_family = AF_INET;
  if((*hp = gethostbyname(host_addr)) == NULL) {
    return SOCKET_GETHOSTBYNAME_FAILED;
  }
  memcpy(&server->sin_addr,
        (*hp)->h_addr_list[0],
        (*hp)->h_length);
  server->sin_port = htons(port);

  return OK;
}

int Select_Till_Readyread(OsSocket * s,
                          int timeout_sec)
{
  int rv;
  fd_set read_fds;
  fd_set error_fds;
  struct timeval tv;

  do {
    tv.tv_sec = timeout_sec;
    tv.tv_usec = 0;
    FD_ZERO(&read_fds);
    FD_ZERO(&error_fds);
    FD_SET(s->sock, &read_fds);
    FD_SET(s->sock, &error_fds);

    rv = select((int)(s->sock+1), &read_fds, NULL, &error_fds, &tv);
  } while(rv < 0 && errno == EINTR);

  if(FD_ISSET(s->sock, &read_fds)) {
    return OK;
  } else {
    return SOCKET_RECV_TIMEOUT;
  }
}


int Select_Till_Readywrite(OsSocket * s,
                           int timeout_sec)
{
  int rv;
  fd_set write_fds;
  fd_set error_fds;
  struct timeval tv;

  do {
    tv.tv_sec = timeout_sec;
    tv.tv_usec = 0;
    FD_ZERO(&write_fds);
    FD_ZERO(&error_fds);
    FD_SET(s->sock, &write_fds);
    FD_SET(s->sock, &error_fds);

    rv = select((int)(s->sock+1), NULL, &write_fds, &error_fds, &tv);
  } while(rv < 0 && errno == EINTR);

  if(FD_ISSET(s->sock, &write_fds)) {
    return OK;
  } else {
    return SOCKET_SEND_TIMEOUT;
  }
}


int ZNet_Common_Get_Local_Ip(OsSocket * s, char ** local_ip)
{
  struct sockaddr_in local;
  size_t saSize = sizeof(struct sockaddr);
  if(getsockname(s->sock,(struct sockaddr *)&local,&saSize)) {
    return SOCKET_GETSOCKNAME_FAILED;
  }
  *local_ip = (char *)malloc(strlen(inet_ntoa(local.sin_addr))+1);
  if(NULL == *local_ip) {
    return BAD_MALLOC;
  }
  strcpy(*local_ip, inet_ntoa(local.sin_addr));

  return OK;
}
