#ifndef _P2P_WIN32_H_
#define _P2P_WIN32_H_

#ifdef __cplusplus
extern "C" {
#endif

#undef UNICODE
#undef _UNICODE

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <winsock2.h>
#include <windows.h>
#include <winioctl.h>

#include <WS2tcpip.h>
#include <process.h>
#ifdef _MSC_VER
#include "getopt.h"

/* Other Win environments are expected to support stdint.h */

/* stdint.h typedefs (C99) (not present in Visual Studio) */
typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;

/* sys/types.h typedefs (not present in Visual Studio) */
typedef unsigned int u_int32_t;
typedef unsigned short u_int16_t;
typedef unsigned char u_int8_t;

typedef int ssize_t;
#endif /* #ifdef _MSC_VER */

typedef unsigned long in_addr_t;


#define MAX(a,b) (a > b ? a : b)
#define MIN(a,b) (a < b ? a : b)

#define snprintf _snprintf
#define strdup _strdup

#define socklen_t int

#define index(a,b) strchr(a,b)

#ifdef __cplusplus
}
#endif

#endif
