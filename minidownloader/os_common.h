#include "os.h"

#if OS_UNIX
  /* defines for unix */
  #include <fcntl.h>
  #include <time.h>
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <sys/select.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <stdlib.h>
  #include <unistd.h>
  
  #include <sys/time.h>
  #include <string.h>
  
  /* this is the unix version of the OsSocket struct */
  struct OsSocket {
    int sock;
  };
#endif
#if OS_WIN
  /* defines for win32 */
  #include <windows.h>
  #include <winsock.h>
  
  /* this is the win32 version of the OsSocket struct */
  struct OsSocket {
    SOCKET sock;
  };
#endif


int ZNet_Common_Socket_Udp_Send(OsSocket * s, const char * host_addr, short int port, 
                                char * buf, int amt, int * amt_sent);
                                
int ZNet_Common_Socket_Udp_Recv(OsSocket * s, const char * host_addr, short int port,
                                char * buf, int amt, int * amt_recv, int timeout_sec);
                                
int ZNet_Common_Socket_Send(OsSocket * s, char * buf, int amt, int * amt_sent);

int ZNet_Common_Socket_Recv(OsSocket * s, char * buf, int amt, 
                            int * amt_recv, int timeout_sec);


struct sockaddr_in;
struct hostent;

int Common_Initialize_Sockaddr_in(struct sockaddr_in* server,
                                  struct hostent** hp,
                                  const char * host_addr,
                                  short int port);

int Select_Till_Readyread(OsSocket * s, int timeout_sec);

int Select_Till_Readywrite(OsSocket * s, int timeout_sec);

int ZNet_Common_Get_Local_Ip(OsSocket * s, char ** local_ip);


