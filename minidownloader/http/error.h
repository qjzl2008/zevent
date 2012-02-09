#ifndef _ZNet_ERROR_H_
#define _ZNet_ERROR_H_

/* error/status codes defined here */

/* no error */
#define OK 0
#define ZNet_ERROR                  -1

/* generic errors */
#define BAD_MALLOC                  -499
#define BAD_PARAMS                  -498

/* socket errors */
#define SOCKET_CONNECT_FAILED       -999
#define SOCKET_SEND_TIMEOUT         -998
#define SOCKET_SEND_FAILED          -997
#define SOCKET_RECV_TIMEOUT         -996
#define SOCKET_RECV_FAILED          -995
#define SOCKET_WSASTARTUP_FAILED    -994
#define SOCKET_INVALID_LIB          -993
#define SOCKET_GETHOSTBYNAME_FAILED -992
#define SOCKET_INVALID              -991
#define SOCKET_IOCTL_FAILURE        -990
#define SOCKET_GETFL_FAILURE        -989
#define SOCKET_SETFL_FAILURE        -988
#define SOCKET_GETSOCKNAME_FAILED   -987

/* http errors */
#define HTTP_RECV_FAILED            -899
#define HTTP_RECV_OVER_MAXSIZE      -898
#define HTTP_PORT_OUT_OF_RANGE      -897
#define HTTP_NO_CONTENT_LENGTH      -896
#define HTTP_HEADER_NOT_OK          -895
#define HTTP_INVALID_URL            -894

/* call this to print an error to stderr. */
void ZNet_Print_Internal_Error(int error);


#endif 
