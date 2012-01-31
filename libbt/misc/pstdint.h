#ifdef _MSC_VER

typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef int ssize_t;

#else
#include <stdint.h>
#endif
